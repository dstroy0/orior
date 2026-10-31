// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lattice.c: the lattice's computing, built into the window and, with LATTICE_WASM set, to wasm32 for the page
// (src-js/lattice/wasm.js): each lattice's points laid out in a slot of its own, seeded and the same every time; the
// loop of order the lattice moves through and its stage at a moment; each point moved by the disorder where it
// stands, then shaded by it as a dot drawn where it stands, or grouped by the level of its pen for a page that draws
// on its 2D canvas; whether the loop moved enough between two moments to be seen, and the soonest moment it next
// will. Built to wasm32 it is freestanding: the page hands in its own cos, sin and remainder, so every number is the
// one the page's own arithmetic gives, and its memory is the module's own; built into the window its arithmetic is
// the C library's and its memory the cellblock's
#include "lattice.h"

#if LATTICE_WASM
// the page's Math.cos, Math.sin and %
LATTICE_IMPORT(cos) double lattice_cos(double angle);
LATTICE_IMPORT(sin) double lattice_sin(double angle);
LATTICE_IMPORT(remainder) double lattice_remainder(double value, double by);
#else
#include "../../cli/src/command_line/cli_internal.h"

#include <math.h>

static double lattice_cos(double angle)
{
    return cos(angle);
}

static double lattice_sin(double angle)
{
    return sin(angle);
}

static double lattice_remainder(double value, double by)
{
    return fmod(value, by);
}
#endif

// the probes across and down at which the loop's change is read, and the least move of a point that shows
#define LATTICE_PROBES 24u
#define LATTICE_PROBE_COUNT ((LATTICE_PROBES + 1u) * (LATTICE_PROBES + 1u))
#define LATTICE_STEP_SHOWN 0.25

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
static uint32_t s_lattice_probes_laid;

LatticeSlot *lattice_slot(uint32_t slot)
{
    return &s_lattice_slots[slot];
}

#if LATTICE_WASM
// the end of what the module has taken of its memory, from the linker's heap base up
extern unsigned char __heap_base;
static uintptr_t s_lattice_taken;

// `bytes` of memory, 16-byte aligned, the memory grown where it must be, in place of `before`, which the module
// keeps: 0 where it cannot grow
static void *lattice_taken(void *before, uintptr_t bytes)
{
    (void)before;
    if (s_lattice_taken == 0u)
    {
        s_lattice_taken = (uintptr_t)&__heap_base;
    }
    const uintptr_t start = (s_lattice_taken + 15u) & ~(uintptr_t)15u;
    const uintptr_t end = start + bytes;
    const uintptr_t have = (uintptr_t)__builtin_wasm_memory_size(0) * 65536u;
    if (end > have)
    {
        const uintptr_t pages = (end - have + 65535u) / 65536u;
        if (__builtin_wasm_memory_grow(0, pages) == (uintptr_t)-1)
        {
            return 0;
        }
    }
    s_lattice_taken = end;
    return (void *)start;
}
#else
// `bytes` of a cell in place of `before`, which is given back once the cell is taken: 0 where none can be, `before`
// kept
static void *lattice_taken(void *before, uintptr_t bytes)
{
    void *const taken = g_cli_cells->persistent_buf_alloc(bytes, &g_gui_lattice_cells);
    if (taken == NULL)
    {
        return 0;
    }
    g_cli_cells->persistent_buf_release(before, &g_gui_lattice_cells);
    return taken;
}

void lattice_given(uint32_t slot)
{
    if (slot >= LATTICE_SLOTS)
    {
        return;
    }
    LatticeSlot *const held = &s_lattice_slots[slot];
    g_cli_cells->persistent_buf_release(held->offsets, &g_gui_lattice_cells);
    g_cli_cells->persistent_buf_release(held->dots, &g_gui_lattice_cells);
    g_cli_cells->persistent_buf_release(held->placed, &g_gui_lattice_cells);
    *held = (LatticeSlot){0};
}
#endif

// the seeded draw the page's lattice has always used, one step of it from `state`
static double lattice_draw(uint32_t *state)
{
    *state += 0x6d2b79f5u;
    uint32_t mixed = *state;
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

// the spacing of the points, which lays eight times the points of the docs site's lattice in the same room
static double lattice_step(void)
{
    return 22.0 / __builtin_sqrt(3.0) / __builtin_sqrt(8.0);
}

// a share from -1 to 1 of a circle's radius as the signed byte that keeps it
static int8_t lattice_offset(double share)
{
    // a share from -1 to 1 rounds to a whole count within a signed byte's 127 either side of 0
    return (int8_t)__builtin_floor(share * LATTICE_OFFSET_MOST + 0.5);
}

// the seed of the point at `row` and `column`, the same wherever the box it is laid out over ends
static uint32_t lattice_seed_at(uint32_t row, uint32_t column)
{
    // the row and the column mixed into the lattice's seed, each by an odd constant, which keeps two points near
    // each other from starting their draws alike
    return LATTICE_SEED ^ (row * 0x9e3779b1u) ^ (column * 0x85ebca77u);
}

LATTICE_EXPORT(lattice_lay) uint32_t lattice_lay(uint32_t slot, double width, double height)
{
    if (slot >= LATTICE_SLOTS)
    {
        return 0u;
    }
    const double step = lattice_step();
    // the points that stand inside the box each way are a whole count, at least one
    const uint32_t columns = (uint32_t)lattice_at_least_one(__builtin_floor((width - step / 2.0) / step) + 1.0);
    const uint32_t rows = (uint32_t)lattice_at_least_one(__builtin_floor((height - step / 2.0) / step) + 1.0);
    const uint32_t count = rows * columns;
    LatticeSlot *const held = &s_lattice_slots[slot];
    if (count > held->capacity)
    {
        const uint32_t room = count + count / 4u;
        int8_t *const offsets = lattice_taken(held->offsets, (uintptr_t)room * 2u);
        if (offsets == 0)
        {
            return 0u;
        }
        held->offsets = offsets;
        held->capacity = room;
        held->dots_room = 0u;
        held->placed_room = 0u;
    }
    held->columns = columns;
    held->rows = rows;
    held->half = step / 2.0;
    held->step = step;
    for (uint32_t row = 0u; row < rows; row += 1u)
    {
        for (uint32_t column = 0u; column < columns; column += 1u)
        {
            uint32_t state = lattice_seed_at(row, column);
            const double turn = lattice_draw(&state) * LATTICE_PI * 2.0;
            const double reach = lattice_draw(&state);
            const uint32_t point = row * columns + column;
            held->offsets[point * 2u] = lattice_offset(lattice_cos(turn) * reach);
            held->offsets[point * 2u + 1u] = lattice_offset(lattice_sin(turn) * reach);
        }
    }
    held->count = count;
    return count;
}

LATTICE_EXPORT(lattice_offsets) int8_t *lattice_offsets(uint32_t slot)
{
    return slot < LATTICE_SLOTS ? s_lattice_slots[slot].offsets : 0;
}

LATTICE_EXPORT(lattice_placed) double *lattice_placed(uint32_t slot)
{
    return slot < LATTICE_SLOTS ? s_lattice_slots[slot].placed : 0;
}

LATTICE_EXPORT(lattice_counts) uint32_t *lattice_counts(uint32_t slot)
{
    return slot < LATTICE_SLOTS ? s_lattice_slots[slot].counts : 0;
}

LATTICE_EXPORT(lattice_dots) LatticeDot *lattice_dots(uint32_t slot)
{
    return slot < LATTICE_SLOTS ? s_lattice_slots[slot].dots : 0;
}

LatticeStageAt *lattice_stage(double seconds)
{
    double left = lattice_remainder(seconds, LATTICE_LOOP);
    double turns = 0.0;
    LatticeStageAt *const at = &s_lattice_stage_at;
    for (uint32_t stage = 0u; stage < LATTICE_STAGE_COUNT; stage += 1u)
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
        if (one->name == LATTICE_SPREADS)
        {
            *at = (LatticeStageAt){LATTICE_SPREAD, 0.0, LATTICE_ORDERED + (1.0 - LATTICE_ORDERED) * part, part};
        }
        else if (one->name == LATTICE_INVERT)
        {
            *at = (LatticeStageAt){LATTICE_WHOLE, 0.0, 0.0, part};
        }
        else if (one->name == LATTICE_GATHERS)
        {
            *at = (LatticeStageAt){LATTICE_GATHER, LATTICE_PI / 2.0, 0.0, part};
        }
        else if (one->name == LATTICE_TURN)
        {
            *at = (LatticeStageAt){LATTICE_SIDED, (LATTICE_PI / 2.0) * (1.0 + turns + part), 0.0, part};
        }
        return at;
    }
    *at = (LatticeStageAt){LATTICE_SIDED, 0.0, 0.0, 0.0};
    return at;
}

// a channel from 0 to 1 as the byte it puts down, a share outside it taken as its end
static uint32_t lattice_byte(double share)
{
    const double within = share < 0.0 ? 0.0 : (share > 1.0 ? 1.0 : share);
    // a share from 0 to 1 is a byte from 0 to 255
    return (uint32_t)(within * 255.0 + 0.5);
}

void lattice_shaded(const LatticePens *pens, LatticeShades *shades)
{
    for (uint32_t shade = 0u; shade < LATTICE_SHADES; shade += 1u)
    {
        const double level = (double)shade / (LATTICE_SHADES - 1u) * (LATTICE_LEVELS - 1);
        // the lower pen is a whole level from 0 to LATTICE_LEVELS less two
        const uint32_t low = (uint32_t)(level < LATTICE_LEVELS - 2 ? level : LATTICE_LEVELS - 2);
        const double between = level - low;
        uint32_t color = 0u;
        for (uint32_t channel = 0u; channel < 4u; channel += 1u)
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
// its slope across and down and its value at the top left, kept between `least` and 1
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
    if (at->kind == LATTICE_SPREAD)
    {
        return (LatticePlane){1.0 / LATTICE_RAMP, 0.0, -at->front / LATTICE_RAMP, 0.0};
    }
    if (at->kind == LATTICE_WHOLE)
    {
        return (LatticePlane){0.0, 0.0, at->part, at->part};
    }
    const double dx = lattice_cos(at->angle);
    const double dy = lattice_sin(at->angle);
    const double along = 1.0 / ((lattice_absolute(dx) + lattice_absolute(dy)) * LATTICE_RAMP);
    const double gathered = at->kind == LATTICE_GATHER ? 1.0 - at->part : 0.0;
    return (LatticePlane){dx * along, dy * along, (0.5 - LATTICE_ORDERED) / LATTICE_RAMP - 0.5 * (dx + dy) * along,
                          gathered};
}

// the disorder of `plane` at `u`, `v`, each from 0 to 1
static double lattice_loose(const LatticePlane *plane, double u, double v)
{
    const double loose = plane->across * u + plane->down * v + plane->start;
    return loose < plane->least ? plane->least : (loose > 1.0 ? 1.0 : loose);
}

// places every point of `slot`'s lattice, `width` by `height`, with the loop at `seconds`: moved by its share of the
// reach and grouped by the nearest of `levels` pens, in the order the points stand within each level; none where the
// memory cannot hold the places
LATTICE_EXPORT(lattice_place) void lattice_place(uint32_t slot, double width, double height, double seconds, uint32_t levels)
{
    if (slot >= LATTICE_SLOTS || levels < 2u || levels > LATTICE_LEVELS_MOST)
    {
        return;
    }
    LatticeSlot *const held = &s_lattice_slots[slot];
    for (uint32_t level = 0u; level < levels; level += 1u)
    {
        held->counts[level] = 0u;
    }
    if (held->placed_room < held->capacity)
    {
        double *const placed = lattice_taken(held->placed, (uintptr_t)held->capacity * 2u * sizeof(double));
        if (placed == 0)
        {
            return;
        }
        held->placed = placed;
        held->placed_room = held->capacity;
    }
    const LatticePlane plane = lattice_plane(lattice_stage(seconds));
    for (uint32_t row = 0u; row < held->rows; row += 1u)
    {
        for (uint32_t column = 0u; column < held->columns; column += 1u)
        {
            const double x = held->half + column * held->step;
            const double y = held->half + row * held->step;
            const double loose = lattice_loose(&plane, x / width, y / height);
            // the level is a whole count from 0 to levels less one
            held->counts[(uint32_t)__builtin_floor(loose * (levels - 1u) + 0.5)] += 1u;
        }
    }
    uint32_t starts[LATTICE_LEVELS_MOST];
    uint32_t start = 0u;
    for (uint32_t level = 0u; level < levels; level += 1u)
    {
        starts[level] = start;
        start += held->counts[level];
    }
    const double reach = LATTICE_REACH / LATTICE_OFFSET_MOST;
    uint32_t point = 0u;
    for (uint32_t row = 0u; row < held->rows; row += 1u)
    {
        for (uint32_t column = 0u; column < held->columns; column += 1u)
        {
            const double x = held->half + column * held->step;
            const double y = held->half + row * held->step;
            const double loose = lattice_loose(&plane, x / width, y / height);
            const double shift = loose * loose * reach;
            const uint32_t level = (uint32_t)__builtin_floor(loose * (levels - 1u) + 0.5);
            double *const placed = &held->placed[starts[level] * 2u];
            placed[0] = x + held->offsets[point * 2u] * shift;
            placed[1] = y + held->offsets[point * 2u + 1u] * shift;
            starts[level] += 1u;
            point += 1u;
        }
    }
}

uint32_t lattice_dots_moved(LatticeSlot *held, double width, double height, double seconds, const LatticeShades *shades)
{
    if (held->dots_room < held->capacity)
    {
        LatticeDot *const dots = lattice_taken(held->dots, (uintptr_t)held->capacity * sizeof(LatticeDot));
        if (dots == 0)
        {
            return 0u;
        }
        held->dots = dots;
        held->dots_room = held->capacity;
    }
    const LatticePlane plane = lattice_plane(lattice_stage(seconds));
    // the plane's slopes a pixel across and down, read once in the floats each point is moved in, and a byte of offset
    // as a share of the reach; a slope, a share and a place on the page are well inside what a float holds
    const float across = (float)(plane.across / width);
    const float down = (float)(plane.down / height);
    const float start = (float)plane.start;
    const float least = (float)plane.least;
    const float reach = (float)(LATTICE_REACH / LATTICE_OFFSET_MOST);
    const float steps = (float)(LATTICE_SHADES - 1u);
    uint32_t point = 0u;
    for (uint32_t row = 0u; row < held->rows; row += 1u)
    {
        const float y = (float)(held->half + row * held->step);
        for (uint32_t column = 0u; column < held->columns; column += 1u)
        {
            const float x = (float)(held->half + column * held->step);
            const float plain = across * x + down * y + start;
            const float loose = plain < least ? least : (plain > 1.0f ? 1.0f : plain);
            const float shift = loose * loose * reach;
            // the shade is a whole step from 0 to LATTICE_SHADES less one
            const uint32_t shade = (uint32_t)(loose * steps + 0.5f);
            LatticeDot *const dot = &held->dots[point];
            dot->across = x + held->offsets[point * 2u] * shift;
            dot->down = y + held->offsets[point * 2u + 1u] * shift;
            dot->radius = shades->radius[shade];
            dot->color = shades->color[shade];
            point += 1u;
        }
    }
    return 1u;
}

// the disorder at every probe at `seconds` into `out`, read as each dot's is
static void lattice_probe(double seconds, float *out)
{
    if (s_lattice_probes_laid == 0u)
    {
        uint32_t at = 0u;
        for (uint32_t across = 0u; across <= LATTICE_PROBES; across += 1u)
        {
            for (uint32_t down = 0u; down <= LATTICE_PROBES; down += 1u)
            {
                s_lattice_probe_u[at] = (float)((double)across / LATTICE_PROBES);
                s_lattice_probe_v[at] = (float)((double)down / LATTICE_PROBES);
                at += 1u;
            }
        }
        s_lattice_probes_laid = 1u;
    }
    const LatticePlane plane = lattice_plane(lattice_stage(seconds));
    for (uint32_t probe = 0u; probe < LATTICE_PROBE_COUNT; probe += 1u)
    {
        // a share from 0 to 1, which a float holds
        out[probe] = (float)lattice_loose(&plane, s_lattice_probe_u[probe], s_lattice_probe_v[probe]);
    }
}

// 1 where the disorder probed at `now` moves some point from where the disorder probed at `then` put it by a quarter
// pixel, which the soft-edged points show as a shift of shade; 0 where no point moved that far
static uint32_t lattice_probes_moved(double now)
{
    lattice_probe(now, s_lattice_loose_now);
    for (uint32_t probe = 0u; probe < LATTICE_PROBE_COUNT; probe += 1u)
    {
        const double was = s_lattice_loose_then[probe];
        const double is = s_lattice_loose_now[probe];
        if (lattice_absolute(is * is - was * was) * LATTICE_REACH >= LATTICE_STEP_SHOWN)
        {
            return 1u;
        }
    }
    return 0u;
}

LATTICE_EXPORT(lattice_shows) uint32_t lattice_shows(double then, double now)
{
    lattice_probe(then, s_lattice_loose_then);
    return lattice_probes_moved(now);
}

// looked for LATTICE_LOOK apart and then halved to within LATTICE_LOOK_CLOSE of the first that shows
LATTICE_EXPORT(lattice_shown_after) double lattice_shown_after(double then, double from)
{
    lattice_probe(then, s_lattice_loose_then);
    // the looks are whole steps through one loop, the most a change is ever looked for
    const uint32_t looks = (uint32_t)(LATTICE_LOOP / LATTICE_LOOK);
    double before = from;
    for (uint32_t look = 0u; look <= looks; look += 1u)
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
