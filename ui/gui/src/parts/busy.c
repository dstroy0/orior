// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// busy.c: the busy circle. Its pictures are made in integers from the turn's table as it comes, a whole swirl in one cell,
// and nothing is worked out for a frame: a stage is read by the clock's bits and laid by the raster's one blend, over the
// pixels kept from under the circle
#include "busy.h"

#include "../../../cli/src/command_line/cli_internal.h"

// the stages of the turn the trail reaches back round the ring; how many pulses a swirl holds and how far they fall, the
// burn's level running from its least to all of 255; and the shares the ash and the hottest of the trail stand at, the
// trail's least under the burn's whole share for the burn alone to stand as the fuse's own color
#define BUSY_TRAIL 48u
#define BUSY_PULSES 4u
#define BUSY_LEVEL_LEAST 96u
#define BUSY_LEVEL_STEP 5u
#define BUSY_SHARE_ASH 153u
#define BUSY_SHARE_TRAIL 92u

// the sine of each of a quarter's stages of the turn with sixteen bits of fraction
static const int s_quarter[(GUI_BUSY_STAGES / 4u) + 1u] = {0,     6424,  12785, 19024, 25080, 30894, 36410, 41575, 46341,
                                                           50660, 54491, 57798, 60547, 62714, 64277, 65220, 65536};

_Static_assert((GUI_BUSY_STAGES & (GUI_BUSY_STAGES - 1u)) == 0u, "GUI_BUSY_STAGES is a power of two");
_Static_assert((GUI_BUSY_ASKS & (GUI_BUSY_ASKS - 1u)) == 0u, "GUI_BUSY_ASKS is a power of two");
_Static_assert(BUSY_SHARE_ASH + BUSY_SHARE_TRAIL < 255u, "the trail's share stays under the burn's whole share");

// the sine of the stage `turn` round the ring, read from the quarter's table
static int busy_sine(unsigned int turn)
{
    const unsigned int on = turn & (GUI_BUSY_STAGES - 1u);
    const unsigned int within = on & ((GUI_BUSY_STAGES / 2u) - 1u);
    const int size = (within <= (GUI_BUSY_STAGES / 4u)) ? s_quarter[within] : s_quarter[(GUI_BUSY_STAGES / 2u) - within];
    return (on < (GUI_BUSY_STAGES / 2u)) ? size : -size;
}

// a triangle from -16 to 16 at `phase` of a swirl's stages, every whole multiple of a stage coming round with the swirl
static int busy_wave(unsigned int phase)
{
    // a phase within a swirl is far inside what an int represents
    const int on = (int)(phase & (GUI_BUSY_STAGES - 1u));
    return (on < 32) ? on - 16 : 48 - on;
}

void gui_busy_made(GuiBusy *busy)
{
    memor.set(&(MemoriaCfg){.dst = busy, .val = 0, .bytes = sizeof(*busy)});
    busy->oldest = 1ull;
}

// the pictures' cell given back, nothing left kept
static void busy_cell_given(GuiBusy *busy)
{
    g_cli_cells->persistent_buf_release(busy->cell, &g_gui_cells);
    busy->cell = NULL;
    busy->cell_bytes = 0ull;
    busy->under_kept = 0;
}

void gui_busy_given(GuiBusy *busy)
{
    busy_cell_given(busy);
    busy->shown = 0;
}

// the oldest ticket walked on past those answered, each ticket walked once: it stands on an ask unanswered or past the newest
static void busy_oldest_walked(GuiBusy *busy)
{
    while ((busy->oldest <= busy->newest) && (busy->asked_ms[busy->oldest & (GUI_BUSY_ASKS - 1u)] == 0ull))
    {
        busy->oldest += 1ull;
    }
}

void gui_busy_asked(GuiBusy *busy, unsigned long long ticket, unsigned long long now)
{
    busy->asked_ms[ticket & (GUI_BUSY_ASKS - 1u)] = now + 1ull;
    busy->newest = (ticket > busy->newest) ? ticket : busy->newest;
    busy_oldest_walked(busy);
}

void gui_busy_answered(GuiBusy *busy, unsigned long long ticket)
{
    busy->asked_ms[ticket & (GUI_BUSY_ASKS - 1u)] = 0ull;
    busy_oldest_walked(busy);
}

void gui_busy_work(GuiBusy *busy, int running, unsigned long long now)
{
    if (!running)
    {
        busy->work_ms = 0ull;
    }
    else if (busy->work_ms == 0ull)
    {
        busy->work_ms = now + 1ull;
    }
}

int gui_busy_waiting(const GuiBusy *busy, unsigned long long *since)
{
    const unsigned long long asked = (busy->oldest <= busy->newest) ? busy->asked_ms[busy->oldest & (GUI_BUSY_ASKS - 1u)] : 0ull;
    if ((asked == 0ull) && (busy->work_ms == 0ull))
    {
        return 0;
    }
    // each one past its millisecond, 0 for none: the lesser of the two that stand
    const unsigned long long oldest = ((asked != 0ull) && ((busy->work_ms == 0ull) || (asked < busy->work_ms))) ? asked : busy->work_ms;
    *since = oldest - 1ull;
    return 1;
}

// whether an ask has stood past the wait at `now`
static int busy_wait_over(const GuiBusy *busy, unsigned long long now)
{
    unsigned long long since = 0ull;
    return gui_busy_waiting(busy, &since) && (now >= since + GUI_BUSY_WAITS);
}

int gui_busy_ticked(GuiBusy *busy, unsigned long long now)
{
    if (!busy->shown)
    {
        if (!busy_wait_over(busy, now))
        {
            return 0;
        }
        busy->shown = 1;
        busy->shown_ms = now;
        return 1;
    }
    if (busy_wait_over(busy, now) || (now < busy->shown_ms + GUI_BUSY_STANDS))
    {
        return 0;
    }
    busy->shown = 0;
    busy_cell_given(busy);
    return 1;
}

unsigned long long gui_busy_due(const GuiBusy *busy, unsigned long long now)
{
    unsigned long long since = 0ull;
    const int waiting = gui_busy_waiting(busy, &since);
    if (!busy->shown)
    {
        if (!waiting)
        {
            return 0ull;
        }
        return (now < since + GUI_BUSY_WAITS) ? (since + GUI_BUSY_WAITS) - now : 1ull;
    }
    if (!busy->still)
    {
        return GUI_BUSY_STAGE_MS - ((now - busy->shown_ms) & (GUI_BUSY_STAGE_MS - 1ull));
    }
    // standing still it wants a tick only to go: as its least ends, and then as soon as no ask has stood past the wait; while one
    // has, the answer wakes the window
    if (now < busy->shown_ms + GUI_BUSY_STANDS)
    {
        return (busy->shown_ms + GUI_BUSY_STANDS) - now;
    }
    if (!waiting)
    {
        return 1ull;
    }
    return (now < since + GUI_BUSY_WAITS) ? (since + GUI_BUSY_WAITS) - now : 0ull;
}

unsigned int gui_busy_stage_at(const GuiBusy *busy, unsigned long long now)
{
    if (busy->still)
    {
        return 0u;
    }
    // a stage's index within a swirl is far inside what an unsigned int represents
    return (unsigned int)((now - busy->shown_ms) >> GUI_BUSY_STAGE_SHIFT) & (GUI_BUSY_STAGES - 1u);
}

void gui_busy_still(GuiBusy *busy, int still)
{
    if (busy->still != still)
    {
        busy_cell_given(busy);
    }
    busy->still = still;
}

// the circle's side at `scale`, an even count of pixels, its center between four of them
static unsigned int busy_side(float scale)
{
    // a size in pixels is not negative
    return (unsigned int)gui_scaled(GUI_BUSY_SIDE, scale) & ~1u;
}

GuiRect gui_busy_rect(const GuiFrame *frame)
{
    // the side is far inside what an int represents
    const int side = (int)busy_side(frame->scale);
    const int edge = gui_scaled(GUI_BUSY_EDGE, frame->scale);
    const GuiRect place = frame->editor;
    return (GuiRect){place.x + place.width - side - edge, place.y + place.height - side - edge, side, side};
}

// a color between `from` and `to` by `share` of 255
static GuiColor busy_between(GuiColor from, GuiColor to, unsigned int share)
{
    const unsigned int left = 255u - share;
    return GUI_RGB(((GUI_RED_OF(from) * left) + (GUI_RED_OF(to) * share) + 127u) / 255u,
                   ((GUI_GREEN_OF(from) * left) + (GUI_GREEN_OF(to) * share) + 127u) / 255u,
                   ((GUI_BLUE_OF(from) * left) + (GUI_BLUE_OF(to) * share) + 127u) / 255u);
}

// What a pixel of a picture has gathered: its channels by what they have of it, and what of 255 it leaves the ground
typedef struct
{
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int rest;
} BusyPixel;

// `color` laid over what the pixel has gathered by `share` of 255
static void busy_laid_over(BusyPixel *pixel, GuiColor color, unsigned int share)
{
    const unsigned int left = 255u - share;
    pixel->red = ((pixel->red * left) + (GUI_RED_OF(color) * share) + 127u) / 255u;
    pixel->green = ((pixel->green * left) + (GUI_GREEN_OF(color) * share) + 127u) / 255u;
    pixel->blue = ((pixel->blue * left) + (GUI_BLUE_OF(color) * share) + 127u) / 255u;
    pixel->rest = ((pixel->rest * left) + 127u) / 255u;
}

// the share of 255 of a pixel between the arcs of `inner` and `outer`
static unsigned int busy_between_arcs(unsigned int outer, unsigned int inner, unsigned int across, unsigned int down)
{
    const unsigned int whole = gui_raster_arc_cover(outer, across, down);
    const unsigned int hollow = gui_raster_arc_cover(inner, across, down);
    return (whole > hollow) ? whole - hollow : 0u;
}

// how far along one side of the circle the sine of a stage stands at `reach`: where a pixel's middle falls, the circle's center
// lying between two pixels and either way from it the same
static unsigned int busy_along(unsigned int half, unsigned int reach, int sine)
{
    // a reach by a sine with sixteen bits of fraction, either way from the center, is far inside what an int represents
    const unsigned int stands = ((unsigned int)((sine < 0) ? -sine : sine) * reach) >> 16u;
    return (sine < 0) ? half - 1u - stands : half + stands;
}

// the picture of `stage`, its pixels gathered from the ring, the trail's glow about it and the burn at its head; the ring's
// reaches and each pixel's stage of the turn read from what the cell holds for the picture's making
static void busy_stage_made(GuiBusy *busy, const GuiPalette *palette, unsigned int stage, unsigned int reach, unsigned int cell,
                            const unsigned char *ring, const unsigned char *glow, const unsigned char *sector)
{
    const unsigned int side = busy->side;
    const unsigned int half = side / 2u;
    const unsigned int pixels = side * side;
    unsigned int *const red_blue = busy->red_blue + (stage * pixels);
    unsigned int *const green = busy->green + (stage * pixels);
    unsigned int *const rest = busy->rest + (stage * pixels);
    // the burn's level through the pulses, and where its middle cell lies with its twitch, a pixel off the ring where two waves
    // that never line up stand together at their heights
    const unsigned int level_wave = (unsigned int)(busy_wave(stage * BUSY_PULSES) + 16);
    const unsigned int level_raw = BUSY_LEVEL_LEAST + (level_wave * BUSY_LEVEL_STEP);
    const unsigned int level = (level_raw > 255u) ? 255u : level_raw;
    const int twitch_x = (busy_wave(stage * 5u) * busy_wave((stage * 3u) + 9u)) / 200;
    const int twitch_y = (busy_wave((stage * 7u) + 21u) * busy_wave(stage * 2u)) / 200;
    // a cell's place inside a picture is far inside what an int represents
    const int reach_cell = (int)cell;
    const int head_x = (int)busy_along(half, reach, busy_sine(stage)) + (twitch_x * reach_cell) - (reach_cell / 2);
    const int head_y = (int)busy_along(half, reach, -busy_sine(stage + (GUI_BUSY_STAGES / 4u))) + (twitch_y * reach_cell) - (reach_cell / 2);
    for (unsigned int row = 0u; row < side; row += 1u)
    {
        for (unsigned int column = 0u; column < side; column += 1u)
        {
            const unsigned int at = (row * side) + column;
            BusyPixel pixel = {0u, 0u, 0u, 255u};
            const unsigned int behind = (stage - sector[at]) & (GUI_BUSY_STAGES - 1u);
            const unsigned int left = (behind < BUSY_TRAIL) ? BUSY_TRAIL - behind : 0u;
            const unsigned int heat = (left * left * 255u) / (BUSY_TRAIL * BUSY_TRAIL);
            if (glow[at] != 0u)
            {
                busy_laid_over(&pixel, palette->fuse_burn, (glow[at] * heat) / (255u * 4u));
            }
            if (ring[at] != 0u)
            {
                const GuiColor color = (heat < 128u) ? busy_between(palette->fuse_ash, palette->fuse_burn, heat * 2u)
                                                     : busy_between(palette->fuse_burn, palette->fuse_fire, (heat - 128u) * 2u);
                busy_laid_over(&pixel, color, (ring[at] * (BUSY_SHARE_ASH + ((BUSY_SHARE_TRAIL * heat) / 255u))) / 255u);
            }
            // the burn: the cell at its middle and one to each side of it, two lines of three crossing; the level against the
            // ordered threshold lights the pixel or lets it stand dim
            // a pixel's place inside a picture is far inside what an int represents
            const int from_x = (int)column - head_x;
            const int from_y = (int)row - head_y;
            const int cell_x = (from_x >= 0) ? from_x / reach_cell : -1 - ((-1 - from_x) / reach_cell);
            const int cell_y = (from_y >= 0) ? from_y / reach_cell : -1 - ((-1 - from_y) / reach_cell);
            const int on_line = ((cell_x == 0) && (cell_y >= -1) && (cell_y <= 1)) || ((cell_y == 0) && (cell_x >= -1) && (cell_x <= 1));
            if (on_line)
            {
                const int lit = level > ((gui_raster_ordered((int)column, (int)row) * 16u) + 8u);
                if ((cell_x == 0) && (cell_y == 0))
                {
                    busy_laid_over(&pixel, lit ? palette->fuse_core : palette->fuse_fire, 255u);
                }
                else if (lit)
                {
                    busy_laid_over(&pixel, palette->fuse_fire, 255u);
                }
                else
                {
                    busy_laid_over(&pixel, palette->fuse_burn, 128u);
                }
            }
            // rounding in the gathering is not to take the pixel past the whole of 255 with what the ground adds
            const unsigned int most = 255u - pixel.rest;
            const unsigned int red = (pixel.red > most) ? most : pixel.red;
            const unsigned int gathered_green = (pixel.green > most) ? most : pixel.green;
            const unsigned int blue = (pixel.blue > most) ? most : pixel.blue;
            red_blue[at] = (((red * 255u) << 16u) | (blue * 255u)) + 0x00800080u;
            green[at] = (gathered_green * 255u) + 0x80u;
            rest[at] = pixel.rest;
        }
    }
}

// the pictures made in one cell for the circle's side at `scale`: the pixels kept from under it, each stage's lanes, then what the
// pictures are made from: 1, or 0 with none made
static int busy_pictures_made(GuiBusy *busy, const GuiPalette *palette, float scale)
{
    busy_cell_given(busy);
    const unsigned int side = busy_side(scale);
    const unsigned int pixels = side * side;
    const unsigned int stages = busy->still ? 1u : GUI_BUSY_STAGES;
    const unsigned long long under_bytes = (unsigned long long)pixels * sizeof(GuiColor);
    const unsigned long long lane_bytes = (unsigned long long)stages * pixels * sizeof(unsigned int);
    const unsigned long long bytes = under_bytes + (3ull * lane_bytes) + (3ull * pixels);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_gui_cells);
    if (cell == NULL)
    {
        return 0;
    }
    busy->cell = cell;
    busy->cell_bytes = bytes;
    busy->side = side;
    busy->stages = stages;
    busy->under = (GuiColor *)cell;
    busy->red_blue = (unsigned int *)(cell + under_bytes);
    busy->green = busy->red_blue + ((unsigned long long)stages * pixels);
    busy->rest = busy->green + ((unsigned long long)stages * pixels);
    unsigned char *const ring = cell + under_bytes + (3ull * lane_bytes);
    unsigned char *const glow = ring + pixels;
    unsigned char *const sector = glow + pixels;
    // sizes in pixels are not negative
    const unsigned int reach = (unsigned int)gui_scaled(GUI_BUSY_REACH, scale);
    const int scaled_cell = gui_scaled(1, scale);
    const unsigned int burn_cell = (scaled_cell < 1) ? 1u : (unsigned int)scaled_cell;
    const unsigned int half = side / 2u;
    for (unsigned int row = 0u; row < side; row += 1u)
    {
        for (unsigned int column = 0u; column < side; column += 1u)
        {
            const unsigned int at = (row * side) + column;
            const unsigned int across = (column >= half) ? column - half : half - 1u - column;
            const unsigned int down = (row >= half) ? row - half : half - 1u - row;
            const unsigned int on_ring = busy_between_arcs(reach + burn_cell, reach - burn_cell, across, down);
            const unsigned int about = busy_between_arcs(reach + (2u * burn_cell), reach - (2u * burn_cell), across, down);
            // shares of 255 are bytes
            ring[at] = (unsigned char)on_ring;
            glow[at] = (unsigned char)((about > on_ring) ? about - on_ring : 0u);
            // the pixel's stage of the turn: the one whose way round from the top it lies along most, twice its place from the
            // center keeping the middle of a pixel whole
            // a place inside a picture is far inside what an int represents
            const int from_x = (2 * (int)column) + 1 - (int)side;
            const int from_y = (2 * (int)row) + 1 - (int)side;
            long long best = 0ll;
            unsigned int best_stage = 0u;
            for (unsigned int turn = 0u; turn < GUI_BUSY_STAGES; turn += 1u)
            {
                const long long along = ((long long)from_x * busy_sine(turn)) - ((long long)from_y * busy_sine(turn + (GUI_BUSY_STAGES / 4u)));
                if ((turn == 0u) || (along > best))
                {
                    best = along;
                    best_stage = turn;
                }
            }
            // a stage of the turn is a byte
            sector[at] = (unsigned char)best_stage;
        }
    }
    for (unsigned int stage = 0u; stage < stages; stage += 1u)
    {
        busy_stage_made(busy, palette, stage, reach, burn_cell, ring, glow, sector);
    }
    return 1;
}

// the picture of the stage at `now` laid in the circle's rectangle, no further than what was kept
static void busy_stage_laid(GuiBusy *busy, GuiRaster *raster, unsigned long long now)
{
    const unsigned int stage = gui_busy_stage_at(busy, now);
    const unsigned int pixels = busy->side * busy->side;
    busy->stage_drawn = stage;
    gui_raster_lanes_laid(raster, busy->red_blue + (stage * pixels), busy->green + (stage * pixels), busy->rest + (stage * pixels), busy->side,
                          busy->side, busy->laid_rect.x, busy->laid_rect.y, busy->under_rect);
}

void gui_busy_drawn(GuiBusy *busy, GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame, unsigned long long now)
{
    busy->under_kept = 0;
    if (!busy->shown)
    {
        return;
    }
    if (((busy->cell == NULL) || (busy->side != busy_side(frame->scale))) && !busy_pictures_made(busy, palette, frame->scale))
    {
        return;
    }
    busy->laid_rect = gui_busy_rect(frame);
    // the raster's sides are far inside what an int represents
    const GuiRect on = gui_rect_met(busy->laid_rect, (GuiRect){0, 0, (int)raster->width, (int)raster->height});
    if (gui_rect_empty(on))
    {
        return;
    }
    // the rectangle's sides and place are not negative, it lying on the raster and not being empty
    for (unsigned int row = 0u; row < (unsigned int)on.height; row += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = &busy->under[row * (unsigned int)on.width],
                                .src = &raster->pixels[(((unsigned int)on.y + row) * raster->width) + (unsigned int)on.x],
                                .bytes = (unsigned long long)on.width * sizeof(GuiColor)});
    }
    busy->under_rect = on;
    busy->under_kept = 1;
    busy_stage_laid(busy, raster, now);
}

GuiRect gui_busy_redrawn(GuiBusy *busy, GuiRaster *raster, unsigned long long now)
{
    if (!busy->under_kept)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect on = busy->under_rect;
    // the rectangle's sides and place are not negative, it being kept from the raster
    for (unsigned int row = 0u; row < (unsigned int)on.height; row += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = &raster->pixels[(((unsigned int)on.y + row) * raster->width) + (unsigned int)on.x],
                                .src = &busy->under[row * (unsigned int)on.width],
                                .bytes = (unsigned long long)on.width * sizeof(GuiColor)});
    }
    busy_stage_laid(busy, raster, now);
    return on;
}
