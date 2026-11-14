// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lattice.c: the lattice's computing for the window: each lattice's points arranged in a slot of its own, seeded and
// the same every time; the loop of order the lattice moves through and its stage at a moment; each point moved by the
// disorder where it is, then shaded by it as a dot drawn where it is; whether the loop moved enough between two
// moments to be seen, and the soonest moment it next will. Its arithmetic is the C library's and its memory the
// cellblock's
#include "lattice.h"

#include "block.h"
#include "../arith/arith.h"


static double lattice_cos(double angle)
{
    return arith_cos(angle);
}

static double lattice_sin(double angle)
{
    return arith_sin(angle);
}

static double lattice_remainder(double value, double by)
{
    return arith_remainder(value, by);
}

// the probes across and down at which the loop's change is read, and the least move of a point that shows
#define LATTICE_PROBES 24u
#define LATTICE_PROBE_COUNT ((LATTICE_PROBES + 1u) * (LATTICE_PROBES + 1u))
#define LATTICE_STEP_DISPLAYED 0.25

// how far apart the loop is looked at for its next change, a display's frame at 60 a second, and how close the
// moment found is to the first that shows, in seconds
#define LATTICE_LOOK 0.016
#define LATTICE_LOOK_CLOSE 0.001

// the loop's stages, each its length in seconds
enum LatticeStageName
{
    LATTICE_REST,
    LATTICE_SPREADS,
    LATTICE_INVERT,
    LATTICE_GATHERS,
    LATTICE_TURN,
};

typedef struct
{
    enum LatticeStageName name;
    double seconds;
} LatticeStage;

static const LatticeStage s_lattice_stages[] = {
    {LATTICE_REST, 16.0}, {LATTICE_SPREADS, 40.0}, {LATTICE_INVERT, 20.0}, {LATTICE_GATHERS, 28.0},
    {LATTICE_TURN, 32.0}, {LATTICE_TURN, 32.0},    {LATTICE_TURN, 32.0},
};
#define LATTICE_STAGE_COUNT (sizeof(s_lattice_stages) / sizeof(s_lattice_stages[0]))
#define LATTICE_LOOP 200.0

static LatticeSlot s_lattice_slots[LATTICE_SLOTS];
static LatticeStageAt s_lattice_stage_at;
static float s_lattice_probe_u[LATTICE_PROBE_COUNT];
static float s_lattice_probe_v[LATTICE_PROBE_COUNT];
static float s_lattice_loose_then[LATTICE_PROBE_COUNT];
static float s_lattice_loose_now[LATTICE_PROBE_COUNT];
static embed_u32 s_lattice_probes_positioned;

LatticeSlot *lattice_slot(embed_u32 slot)
{
    return &s_lattice_slots[slot];
}

// where a slot's dots start in its segment of the block: after its offsets, on a boundary
static embed_word lattice_dots_at(embed_u32 capacity)
{
    return ((embed_word)capacity * 2u + GUI_BLOCK_POSITION - 1u) & ~(embed_word)(GUI_BLOCK_POSITION - 1u);
}

// `bytes` from the start of the slot's segment of the block's lattice part: the span, or 0 where the segment has no room
static void *lattice_taken(const LatticeSlot *lattice, embed_word bytes)
{
    // the slot's place among the slots is its segment
    const unsigned int segment = (unsigned int)(lattice - s_lattice_slots);
    return EMBED_CALL(gui_block_part.span, GuiBlockPartCfg, .part = GUI_BLOCK_LATTICE, .segment = segment,
                      .bytes = bytes);
}

void lattice_given(embed_u32 slot)
{
    if (slot >= LATTICE_SLOTS)
    {
        return;
    }
    LatticeSlot *const lattice = &s_lattice_slots[slot];
    *lattice = (LatticeSlot){0};
}

// the seeded draw of the lattice, one step of it from `state`
static double lattice_draw(embed_u32 *state)
{
    *state += 0x6d2b79f5u;
    embed_u32 mixed = *state;
    mixed = (mixed ^ (mixed >> 15)) * (mixed | 1u);
    mixed ^= mixed + (mixed ^ (mixed >> 7)) * (mixed | 61u);
    return (double)(mixed ^ (mixed >> 14)) / 4294967296.0;
}

static double lattice_ease(double part)
{
    return part * part * (3.0 - 2.0 * part);
}

static double lattice_absolute(double value)
{
    return value < 0.0 ? -value : value;
}

static double lattice_at_least_one(double value)
{
    return value > 1.0 ? value : 1.0;
}

// the spacing of the points, which positions eight times the points of the docs site's lattice in the same capacity
static double lattice_step(void)
{
    return 22.0 / __builtin_sqrt(3.0) / __builtin_sqrt(8.0);
}

// a share from -1 to 1 of a circle's radius as the signed byte that retains it
static embed_i8 lattice_offset(double share)
{
    // a share from -1 to 1 rounds to a whole count within a signed byte's 127 either side of 0
    return (embed_i8)arith_floor(share * LATTICE_OFFSET_MAX + 0.5);
}

// the seed of the point at `row` and `column`, the same wherever the box it is arranged over ends
static embed_u32 lattice_seed_at(embed_u32 row, embed_u32 column)
{
    // the row and the column mixed into the lattice's seed, each by an odd constant, which retains two points near
    // each other from starting their draws alike
    return LATTICE_SEED ^ (row * 0x9e3779b1u) ^ (column * 0x85ebca77u);
}

embed_u32 lattice_position(embed_u32 slot, double width, double height)
{
    if (slot >= LATTICE_SLOTS)
    {
        return 0u;
    }
    const double step = lattice_step();
    // the points that are inside the box each way are a whole count, at least one
    const embed_u32 columns = (embed_u32)lattice_at_least_one(arith_floor((width - step / 2.0) / step) + 1.0);
    const embed_u32 rows = (embed_u32)lattice_at_least_one(arith_floor((height - step / 2.0) / step) + 1.0);
    const embed_u32 count = rows * columns;
    LatticeSlot *const lattice = &s_lattice_slots[slot];
    if (count > lattice->capacity)
    {
        const embed_u32 capacity = count + count / 4u;
        embed_i8 *const offsets = lattice_taken(lattice, (embed_word)capacity * 2u);
        if (offsets == 0)
        {
            return 0u;
        }
        lattice->offsets = offsets;
        lattice->capacity = capacity;
        lattice->dots_capacity = 0u;
    }
    lattice->columns = columns;
    lattice->rows = rows;
    lattice->half = step / 2.0;
    lattice->step = step;
    for (embed_u32 row = 0u; row < rows; row += 1u)
    {
        for (embed_u32 column = 0u; column < columns; column += 1u)
        {
            embed_u32 state = lattice_seed_at(row, column);
            const double turn = lattice_draw(&state) * LATTICE_PI * 2.0;
            const double reach = lattice_draw(&state);
            const embed_u32 point = row * columns + column;
            lattice->offsets[point * 2u] = lattice_offset(lattice_cos(turn) * reach);
            lattice->offsets[point * 2u + 1u] = lattice_offset(lattice_sin(turn) * reach);
        }
    }
    lattice->count = count;
    return count;
}

LatticeStageAt *lattice_stage(double seconds)
{
    double left = lattice_remainder(seconds, LATTICE_LOOP);
    double turns = 0.0;
    LatticeStageAt *const at = &s_lattice_stage_at;
    for (embed_u32 stage = 0u; stage < LATTICE_STAGE_COUNT; stage += 1u)
    {
        const LatticeStage *const one = &s_lattice_stages[stage];
        if (left >= one->seconds)
        {
            left -= one->seconds;
            turns += one->name == LATTICE_TURN ? 1.0 : 0.0;
            continue;
        }
        const double part = lattice_ease(left / one->seconds);
        *at = (LatticeStageAt){LATTICE_SIDED, 0.0, 0.0, part};
        switch (one->name)
        {
        case LATTICE_SPREADS: {
            *at = (LatticeStageAt){LATTICE_SPREAD, 0.0, LATTICE_ORDERED + (1.0 - LATTICE_ORDERED) * part, part};
            break;
        }
        case LATTICE_INVERT: {
            *at = (LatticeStageAt){LATTICE_WHOLE, 0.0, 0.0, part};
            break;
        }
        case LATTICE_GATHERS: {
            *at = (LatticeStageAt){LATTICE_GATHER, LATTICE_PI / 2.0, 0.0, part};
            break;
        }
        case LATTICE_TURN: {
            *at = (LatticeStageAt){LATTICE_SIDED, (LATTICE_PI / 2.0) * (1.0 + turns + part), 0.0, part};
            break;
        }
        default: {
            break;
        }
        }
        return at;
    }
    *at = (LatticeStageAt){LATTICE_SIDED, 0.0, 0.0, 0.0};
    return at;
}

// a channel from 0 to 1 as the byte it puts down, a share outside it taken as its end
static embed_u32 lattice_byte(double share)
{
    double within;
    if (share < 0.0)
    {
        within = 0.0;
    }
    else if (share > 1.0)
    {
        within = 1.0;
    }
    else
    {
        within = share;
    }
    // a share from 0 to 1 is a byte from 0 to 255
    return (embed_u32)(within * 255.0 + 0.5);
}

void lattice_shaded(const LatticePens *pens, LatticeShades *shades)
{
    for (embed_u32 shade = 0u; shade < LATTICE_SHADES; shade += 1u)
    {
        const double level = (double)shade / (LATTICE_SHADES - 1u) * (LATTICE_LEVELS - 1);
        // the lower pen is a whole level from 0 to LATTICE_LEVELS less two
        const embed_u32 low = (embed_u32)(level < LATTICE_LEVELS - 2 ? level : LATTICE_LEVELS - 2);
        const double between = level - low;
        embed_u32 color = 0u;
        for (embed_u32 channel = 0u; channel < 4u; channel += 1u)
        {
            const double from = pens->color[low * 4u + channel];
            const double to = pens->color[(low + 1u) * 4u + channel];
            color |= lattice_byte(from + (to - from) * between) << (channel * 8u);
        }
        const double from = pens->radius[low];
        const double to = pens->radius[low + 1u];
        shades->color[shade] = color;
        // a radius of a pixel or so, which a float holds whole
        shades->radius[shade] = (float)((from + (to - from) * between) * LATTICE_FITS);
    }
}

// The disorder over the lattice with the loop at one moment: a plane over the place across and down, each from 0 to 1,
// its slope across and down and its value at the top left, retained between `least` and 1
typedef struct
{
    double across;
    double down;
    double start;
    double least;
} LatticePlane;

// the plane of the disorder with the loop at `at`: a spread's front ramping across; the whole lattice at one share;
// and a side ordered away from the angle, the ordered share before the ramp, which a gathering holds above what is
// still to gather
static LatticePlane lattice_plane(const LatticeStageAt *at)
{
    if (at->type == LATTICE_SPREAD)
    {
        return (LatticePlane){1.0 / LATTICE_RAMP, 0.0, -at->front / LATTICE_RAMP, 0.0};
    }
    if (at->type == LATTICE_WHOLE)
    {
        return (LatticePlane){0.0, 0.0, at->part, at->part};
    }
    const double dx = lattice_cos(at->angle);
    const double dy = lattice_sin(at->angle);
    const double along = 1.0 / ((lattice_absolute(dx) + lattice_absolute(dy)) * LATTICE_RAMP);
    const double gathered = at->type == LATTICE_GATHER ? 1.0 - at->part : 0.0;
    return (LatticePlane){dx * along, dy * along, (0.5 - LATTICE_ORDERED) / LATTICE_RAMP - 0.5 * (dx + dy) * along,
                          gathered};
}

// the disorder of `plane` at `u`, `v`, each from 0 to 1
static double lattice_loose(const LatticePlane *plane, double u, double v)
{
    const double loose = plane->across * u + plane->down * v + plane->start;
    if (loose < plane->least)
    {
        return plane->least;
    }
    if (loose > 1.0)
    {
        return 1.0;
    }
    return loose;
}

// the disorder at x, y of the plane read in the floats the points move in: its slopes `across` and `down`, its value
// `start` at the top left, retained between `least` and 1
static float lattice_loose_at(float across, float down, float start, float least, float x, float y)
{
    const float plain = across * x + down * y + start;
    if (plain < least)
    {
        return least;
    }
    return (plain > 1.0f) ? 1.0f : plain;
}

// the share of its reach a point at x, y has gone from its origin: its disorder squared, as its shift is
static float lattice_gone_at(float across, float down, float start, float least, float x, float y)
{
    const float loose = lattice_loose_at(across, down, start, least, x, y);
    return loose * loose;
}

embed_u32 lattice_dots_moved(LatticeSlot *lattice, double width, double height, double seconds,
                             const LatticeShades *shades)
{
    if (lattice->dots_capacity < lattice->capacity)
    {
        const embed_word dots_at = lattice_dots_at(lattice->capacity);
        embed_u8 *const span = lattice_taken(lattice, dots_at + ((embed_word)lattice->capacity * sizeof(LatticeDot)));
        if (span == 0)
        {
            return 0u;
        }
        lattice->dots = (LatticeDot *)(void *)(span + dots_at);
        lattice->dots_capacity = lattice->capacity;
    }
    const LatticePlane plane = lattice_plane(lattice_stage(seconds));
    // the plane's slopes a pixel across and down, read once in the floats each point is moved in, and a byte of offset
    // as a share of the reach; a slope, a share and a place on the page are well inside what a float holds
    const float across = (float)(plane.across / width);
    const float down = (float)(plane.down / height);
    const float start = (float)plane.start;
    const float least = (float)plane.least;
    const float reach = (float)(LATTICE_REACH / LATTICE_OFFSET_MAX);
    const float steps = (float)(LATTICE_SHADES - 1u);
    const float step = (float)lattice->step;
    embed_u32 point = 0u;
    for (embed_u32 row = 0u; row < lattice->rows; row += 1u)
    {
        const float y = (float)(lattice->half + row * lattice->step);
        for (embed_u32 column = 0u; column < lattice->columns; column += 1u)
        {
            const float x = (float)(lattice->half + column * lattice->step);
            const float loose = lattice_loose_at(across, down, start, least, x, y);
            const float shift = loose * loose * reach;
            // the shade is a whole step from 0 to LATTICE_SHADES less one
            const embed_u32 shade = (embed_u32)(loose * steps + 0.5f);
            // the ink fades with the alignment about the point: its own share of its reach gone and its four nearest
            // neighbors', a fifth each, against the fade's span
            const float gone = (loose * loose + lattice_gone_at(across, down, start, least, x - step, y) +
                                lattice_gone_at(across, down, start, least, x + step, y) +
                                lattice_gone_at(across, down, start, least, x, y - step) +
                                lattice_gone_at(across, down, start, least, x, y + step)) /
                               (5.0f * LATTICE_FADE_SPAN);
            const float inked = (gone > 1.0f) ? 1.0f : gone;
            LatticeDot *const dot = &lattice->dots[point];
            dot->across = x + lattice->offsets[point * 2u] * shift;
            dot->down = y + lattice->offsets[point * 2u + 1u] * shift;
            dot->radius = shades->radius[shade];
            // the ink byte by the share, rounded, is a byte
            dot->color = (shades->color[shade] & 0x00ffffffu) |
                         ((embed_u32)((float)(shades->color[shade] >> 24u) * inked + 0.5f) << 24u);
            point += 1u;
        }
    }
    return 1u;
}

// the disorder at every probe at `seconds` into `out`, read as each dot's is
static void lattice_probe(double seconds, float *out)
{
    if (s_lattice_probes_positioned == 0u)
    {
        embed_u32 at = 0u;
        for (embed_u32 across = 0u; across <= LATTICE_PROBES; across += 1u)
        {
            for (embed_u32 down = 0u; down <= LATTICE_PROBES; down += 1u)
            {
                s_lattice_probe_u[at] = (float)((double)across / LATTICE_PROBES);
                s_lattice_probe_v[at] = (float)((double)down / LATTICE_PROBES);
                at += 1u;
            }
        }
        s_lattice_probes_positioned = 1u;
    }
    const LatticePlane plane = lattice_plane(lattice_stage(seconds));
    for (embed_u32 probe = 0u; probe < LATTICE_PROBE_COUNT; probe += 1u)
    {
        // a share from 0 to 1, which a float holds
        out[probe] = (float)lattice_loose(&plane, s_lattice_probe_u[probe], s_lattice_probe_v[probe]);
    }
}

// 1 where the disorder probed at `now` moves some point from where the disorder probed at `then` put it by a quarter
// pixel, which the soft-edged points show as a shift of shade; 0 where no point moved that far
static embed_u32 lattice_probes_moved(double now)
{
    lattice_probe(now, s_lattice_loose_now);
    for (embed_u32 probe = 0u; probe < LATTICE_PROBE_COUNT; probe += 1u)
    {
        const double was = s_lattice_loose_then[probe];
        const double is = s_lattice_loose_now[probe];
        if (lattice_absolute(is * is - was * was) * LATTICE_REACH >= LATTICE_STEP_DISPLAYED)
        {
            return 1u;
        }
    }
    return 0u;
}

embed_u32 lattice_shows(double then, double now)
{
    lattice_probe(then, s_lattice_loose_then);
    return lattice_probes_moved(now);
}

// looked for LATTICE_LOOK apart and then halved to within LATTICE_LOOK_CLOSE of the first that shows
double lattice_displayed_after(double then, double from)
{
    lattice_probe(then, s_lattice_loose_then);
    // the looks are whole steps through one loop, the max a change is ever looked for
    const embed_u32 looks = (embed_u32)(LATTICE_LOOP / LATTICE_LOOK);
    double before = from;
    for (embed_u32 look = 0u; look <= looks; look += 1u)
    {
        double after = from + look * LATTICE_LOOK;
        if (!lattice_probes_moved(after))
        {
            before = after;
            continue;
        }
        while ((look > 0u) && (after - before > LATTICE_LOOK_CLOSE))
        {
            const double middle = (before + after) / 2.0;
            if (lattice_probes_moved(middle))
            {
                after = middle;
            }
            else
            {
                before = middle;
            }
        }
        return after;
    }
    return from + LATTICE_LOOP;
}
