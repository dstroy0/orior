// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// busy.c: the busy circle. Its pictures are made in integers from the turn's table as it comes, a whole swirl in one
// cell, and nothing is worked out for a frame: a stage is read by the clock's bits and positioned by the raster's one blend,
// over the pixels saved from under the circle
#include "busy.h"

#include "memoria_operor/memoria_operor.h"

#include "block.h"


// What a pixel of a picture has gathered: its channels by what they have of it, and what of 255 it leaves the ground
typedef struct
{
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int rest;
} BusyPixel;
// the stages of the turn the trail reaches back round the ring; how many pulses a swirl holds and how far they fall,
// the burn's level running from its least to all of 255; and the shares the ash and the hottest of the trail are at,
// the trail's least under the burn's whole share for the burn alone to be as the fuse's own color
#define BUSY_TRAIL 48u
#define BUSY_PULSES 4u
#define BUSY_LEVEL_MIN 96u
#define BUSY_LEVEL_STEP 5u
#define BUSY_SHARE_ASH 153u
#define BUSY_SHARE_TRAIL 92u

// the sine of each of a quarter's stages of the turn with sixteen bits of fraction
static const int s_quarter[(GUI_BUSY_STAGES / 4u) + 1u] = {
    0, 6424, 12785, 19024, 25080, 30894, 36410, 41575, 46341, 50660, 54491, 57798, 60547, 62714, 64277, 65220, 65536};

_Static_assert((GUI_BUSY_STAGES & (GUI_BUSY_STAGES - 1u)) == 0u, "GUI_BUSY_STAGES is a power of two");
_Static_assert(BUSY_SHARE_ASH + BUSY_SHARE_TRAIL < 255u, "the trail's share stays under the burn's whole share");

typedef struct
{
    unsigned int turn;
    unsigned int phase;
    GuiBusy *busy;
    const GuiBusy *busy_read;
    int scale;
    GuiColor from;
    GuiColor to;
    unsigned int share;
    BusyPixel *pixel;
    GuiColor color;
    unsigned int outer;
    unsigned int inner;
    unsigned int across;
    unsigned int down;
    unsigned int half;
    unsigned int reach;
    int sine;
    const GuiPalette *palette;
    unsigned int stage;
    unsigned int cell;
    const unsigned char *ring;
    const unsigned char *glow;
    const unsigned char *sector;
    GuiRaster *raster;
    unsigned int at;
    int head_x;
    int head_y;
    unsigned int level;
    unsigned long long now_ms;
} BusyCtx;
// the sine of the stage `turn` round the ring, read from the quarter's table
EMBED_INLINE int busy_sine(const BusyCtx *args)
{
    const unsigned int on = args->turn & (GUI_BUSY_STAGES - 1u);
    const unsigned int within = on & ((GUI_BUSY_STAGES / 2u) - 1u);
    const int size =
        (within <= (GUI_BUSY_STAGES / 4u)) ? s_quarter[within] : s_quarter[(GUI_BUSY_STAGES / 2u) - within];
    return (on < (GUI_BUSY_STAGES / 2u)) ? size : -size;
}

// a triangle from -16 to 16 at `phase` of a swirl's stages, every whole multiple of a stage coming round with the swirl
EMBED_INLINE int busy_wave(const BusyCtx *args)
{
    // a phase within a swirl is far inside what an int represents
    const int on = (int)(args->phase & (GUI_BUSY_STAGES - 1u));
    return (on < 32) ? on - 16 : 48 - on;
}

void gui_busy_init(const GuiBusyStillCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->busy, .val = 0, .bytes = sizeof(*args->busy)});
}

void gui_busy_work(const GuiBusyRunningCfg *args)
{
    const unsigned long long now_ms = args->now_ms;
    if (!args->running)
    {
        args->busy->work_start_ms = 0ULL;
    }
    else if (args->busy->work_start_ms == 0ULL)
    {
        args->busy->work_start_ms = now_ms + 1ULL;
    }
}

int gui_busy_waiting(const GuiBusySinceCfg *args)
{
    if (args->busy_read->work_start_ms == 0ULL)
    {
        return 0;
    }
    *args->since = args->busy_read->work_start_ms - 1ULL;
    return 1;
}

// whether an ask has was past the wait
EMBED_INLINE int busy_wait_over(const BusyCtx *args)
{
    const unsigned long long now_ms = args->now_ms;
    const GuiBusy *busy = args->busy_read;
    unsigned long long since = 0ULL;
    return EMBED_CALL(gui_busy_since.waiting, GuiBusySinceCfg, .busy_read = busy, .since = &since) &&
           (now_ms >= since + GUI_BUSY_SHOW_DELAY_MS);
}

int gui_busy_tick(const GuiBusyRunningCfg *args)
{
    const unsigned long long now_ms = args->now_ms;
    if (!args->busy->displayed)
    {
        if (!EMBED_CALL(busy_wait_over, BusyCtx, .busy_read = args->busy, .now_ms = args->now_ms))
        {
            return 0;
        }
        args->busy->displayed = 1;
        args->busy->visible_start_ms = now_ms;
        return 1;
    }
    if (EMBED_CALL(busy_wait_over, BusyCtx, .busy_read = args->busy, .now_ms = args->now_ms) ||
        (now_ms < args->busy->visible_start_ms + GUI_BUSY_MIN_VISIBLE_MS))
    {
        return 0;
    }
    args->busy->displayed = 0;
    args->busy->cell = NULL;
    args->busy->cell_bytes = 0ULL;
    args->busy->under_saved = 0;
    return 1;
}

unsigned long long gui_busy_next_deadline_ms(const GuiBusyCfg *args)
{
    const unsigned long long now_ms = args->now_ms;
    unsigned long long since = 0ULL;
    const int waiting =
        EMBED_CALL(gui_busy_since.waiting, GuiBusySinceCfg, .busy_read = args->busy_read, .since = &since);
    const unsigned long long show_deadline_ms = since + GUI_BUSY_SHOW_DELAY_MS;
    if (!args->busy_read->displayed)
    {
        return waiting ? show_deadline_ms : 0ULL;
    }
    if (!args->busy_read->still)
    {
        return now_ms + GUI_BUSY_STAGE_MS - ((now_ms - args->busy_read->visible_start_ms) & (GUI_BUSY_STAGE_MS - 1ULL));
    }
    // stationary it wants a tick only to go: at its minimum visible time, and then as soon as no request has was
    // past the show delay; while one has, the answer wakes the window
    const unsigned long long min_visible_end_ms = args->busy_read->visible_start_ms + GUI_BUSY_MIN_VISIBLE_MS;
    if (now_ms < min_visible_end_ms)
    {
        return min_visible_end_ms;
    }
    if (!waiting)
    {
        return now_ms + 1ULL;
    }
    return (now_ms < show_deadline_ms) ? show_deadline_ms : 0ULL;
}

unsigned int gui_busy_stage_at(const GuiBusyCfg *args)
{
    const unsigned long long now_ms = args->now_ms;
    if (args->busy_read->still)
    {
        return 0u;
    }
    // a stage's index within a swirl is far inside what an unsigned int represents
    return (unsigned int)((now_ms - args->busy_read->visible_start_ms) >> GUI_BUSY_STAGE_SHIFT) &
           (GUI_BUSY_STAGES - 1u);
}

void gui_busy_still(const GuiBusyStillCfg *args)
{
    int still = args->still;
    if (args->busy->still != still)
    {
        args->busy->cell = NULL;
        args->busy->cell_bytes = 0ULL;
        args->busy->under_saved = 0;
    }
    args->busy->still = still;
}

// the circle's side at `scale`, an even count of pixels, its center between four of them
EMBED_INLINE unsigned int busy_side(const BusyCtx *args)
{
    int scale = args->scale;
    // a size in pixels is not negative
    return (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BUSY_SIDE,
                                    .scale = scale) &
           ~1u;
}

GuiRect gui_busy_rect(const GuiBusyFrameCfg *args)
{
    // the side is far inside what an int represents
    const int side = (int)EMBED_CALL(busy_side, BusyCtx, .scale = args->frame->scale);
    const int edge =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BUSY_EDGE, .scale = args->frame->scale);
    const GuiRect place = args->frame->editor;
    return (GuiRect){place.x + place.width - side - edge, place.y + place.height - side - edge, side, side};
}

// a color between `from` and `to` by `share` of 255
EMBED_INLINE GuiColor busy_between(const BusyCtx *args)
{
    const unsigned int left = 255u - args->share;
    return GUI_RGB(((GUI_RED_OF(args->from) * left) + (GUI_RED_OF(args->to) * args->share) + 127u) / 255u,
                   ((GUI_GREEN_OF(args->from) * left) + (GUI_GREEN_OF(args->to) * args->share) + 127u) / 255u,
                   ((GUI_BLUE_OF(args->from) * left) + (GUI_BLUE_OF(args->to) * args->share) + 127u) / 255u);
}

// `color` positioned over what the pixel has gathered by `share` of 255
EMBED_INLINE void busy_layout_over(const BusyCtx *args)
{
    const unsigned int left = 255u - args->share;
    args->pixel->red = ((args->pixel->red * left) + (GUI_RED_OF(args->color) * args->share) + 127u) / 255u;
    args->pixel->green = ((args->pixel->green * left) + (GUI_GREEN_OF(args->color) * args->share) + 127u) / 255u;
    args->pixel->blue = ((args->pixel->blue * left) + (GUI_BLUE_OF(args->color) * args->share) + 127u) / 255u;
    args->pixel->rest = ((args->pixel->rest * left) + 127u) / 255u;
}

// the share of 255 of a pixel between the arcs of `inner` and `outer`
EMBED_INLINE unsigned int busy_between_arcs(const BusyCtx *args)
{
    unsigned int across = args->across;
    unsigned int down = args->down;
    const unsigned int whole =
        EMBED_CALL(gui_raster_reach.arc_cover, GuiRasterReachCfg, .reach = args->outer, .across = across, .down = down);
    const unsigned int hollow =
        EMBED_CALL(gui_raster_reach.arc_cover, GuiRasterReachCfg, .reach = args->inner, .across = across, .down = down);
    return (whole > hollow) ? whole - hollow : 0u;
}

// how far along one side of the circle the sine of a stage is at `reach`: where a pixel's middle falls, the
// circle's center lying between two pixels and either way from it the same
EMBED_INLINE unsigned int busy_along(const BusyCtx *args)
{
    // a reach by a sine with sixteen bits of fraction, either way from the center, is far inside what an int represents
    const unsigned int exists = ((unsigned int)((args->sine < 0) ? -args->sine : args->sine) * args->reach) >> 16u;
    return (args->sine < 0) ? args->half - 1u - exists : args->half + exists;
}

// the trail at the pixel `at` positioned on `pixel`, `share` of 255 its heat there: the burn's color glowing about the ring
// by the pixel's share of the glow, and the ring's color, from the ash through the burn to the fire as the heat rises,
// by its share of the ring
EMBED_INLINE void busy_trail_layout(const BusyCtx *args)
{
    const unsigned int heat = args->share;
    const unsigned int at = args->at;
    if (args->glow[at] != 0u)
    {
        EMBED_CALL(busy_layout_over, BusyCtx, .pixel = args->pixel, .color = args->palette->fuse_burn,
                   .share = (args->glow[at] * heat) / (255u * 4u));
    }
    if (args->ring[at] != 0u)
    {
        const GuiColor color = (heat < 128u) ? EMBED_CALL(busy_between, BusyCtx, .from = args->palette->fuse_ash,
                                                          .to = args->palette->fuse_burn, .share = heat * 2u)
                                             : EMBED_CALL(busy_between, BusyCtx, .from = args->palette->fuse_burn,
                                                          .to = args->palette->fuse_fire, .share = (heat - 128u) * 2u);
        EMBED_CALL(busy_layout_over, BusyCtx, .pixel = args->pixel, .color = color,
                   .share = (args->ring[at] * (BUSY_SHARE_ASH + ((BUSY_SHARE_TRAIL * heat) / 255u))) / 255u);
    }
}

// the burn positioned on `pixel`, `across` pixels from the picture's left and `down` from its top, its middle cell at
// `head_x` and `head_y` and each cell `cell` pixels wide: the cell at its middle and one to each side of it, two lines
// of three crossing. The `level` against the ordered threshold lights the pixel or lets it are dim
EMBED_INLINE void busy_burn_layout(const BusyCtx *args)
{
    // a cell's place and a pixel's inside a picture are far inside what an int represents
    const int reach_cell = (int)args->cell;
    const int column = (int)args->across;
    const int row = (int)args->down;
    const int from_x = column - args->head_x;
    const int from_y = row - args->head_y;
    const int cell_x = (from_x >= 0) ? from_x / reach_cell : -1 - ((-1 - from_x) / reach_cell);
    const int cell_y = (from_y >= 0) ? from_y / reach_cell : -1 - ((-1 - from_y) / reach_cell);
    const int on_line =
        ((cell_x == 0) && (cell_y >= -1) && (cell_y <= 1)) || ((cell_y == 0) && (cell_x >= -1) && (cell_x <= 1));
    if (!on_line)
    {
        return;
    }
    const int lit =
        args->level > ((EMBED_CALL(gui_raster_rect.order, GuiRasterRectCfg, .x = column, .y = row) * 16u) + 8u);
    if ((cell_x == 0) && (cell_y == 0))
    {
        EMBED_CALL(busy_layout_over, BusyCtx, .pixel = args->pixel,
                   .color = lit ? args->palette->fuse_core : args->palette->fuse_fire, .share = 255u);
    }
    else if (lit)
    {
        EMBED_CALL(busy_layout_over, BusyCtx, .pixel = args->pixel, .color = args->palette->fuse_fire, .share = 255u);
    }
    else
    {
        EMBED_CALL(busy_layout_over, BusyCtx, .pixel = args->pixel, .color = args->palette->fuse_burn, .share = 128u);
    }
}

// the picture of `stage`, its pixels gathered from the ring, the trail's glow about it and the burn at its head; the
// ring's reaches and each pixel's stage of the turn read from what the cell holds for the picture's making
EMBED_INLINE void busy_stage_init(const BusyCtx *args)
{
    const unsigned int side = args->busy->side;
    const unsigned int half = side / 2u;
    const unsigned int pixels = side * side;
    unsigned int *const red_blue = args->busy->red_blue + (args->stage * pixels);
    unsigned int *const green = args->busy->green + (args->stage * pixels);
    unsigned int *const rest = args->busy->rest + (args->stage * pixels);
    // the burn's level through the pulses, and where its middle cell lies with its twitch, a pixel off the ring where
    // two waves that never line up are together at their heights
    const unsigned int level_wave =
        (unsigned int)(EMBED_CALL(busy_wave, BusyCtx, .phase = args->stage * BUSY_PULSES) + 16);
    const unsigned int level_raw = BUSY_LEVEL_MIN + (level_wave * BUSY_LEVEL_STEP);
    const unsigned int level = (level_raw > 255u) ? 255u : level_raw;
    const int twitch_x = (EMBED_CALL(busy_wave, BusyCtx, .phase = args->stage * 5u) *
                          EMBED_CALL(busy_wave, BusyCtx, .phase = (args->stage * 3u) + 9u)) /
                         200;
    const int twitch_y = (EMBED_CALL(busy_wave, BusyCtx, .phase = (args->stage * 7u) + 21u) *
                          EMBED_CALL(busy_wave, BusyCtx, .phase = args->stage * 2u)) /
                         200;
    // a cell's place inside a picture is far inside what an int represents
    const int reach_cell = (int)args->cell;
    const int head_x = (int)EMBED_CALL(busy_along, BusyCtx, .half = half, .reach = args->reach,
                                       .sine = EMBED_CALL(busy_sine, BusyCtx, .turn = args->stage)) +
                       (twitch_x * reach_cell) - (reach_cell / 2);
    const int head_y =
        (int)EMBED_CALL(busy_along, BusyCtx, .half = half, .reach = args->reach,
                        .sine = -EMBED_CALL(busy_sine, BusyCtx, .turn = args->stage + (GUI_BUSY_STAGES / 4u))) +
        (twitch_y * reach_cell) - (reach_cell / 2);
    for (unsigned int row = 0u; row < side; row += 1u)
    {
        for (unsigned int column = 0u; column < side; column += 1u)
        {
            const unsigned int at = (row * side) + column;
            BusyPixel pixel = {0u, 0u, 0u, 255u};
            const unsigned int behind = (args->stage - args->sector[at]) & (GUI_BUSY_STAGES - 1u);
            const unsigned int left = (behind < BUSY_TRAIL) ? BUSY_TRAIL - behind : 0u;
            const unsigned int heat = (left * left * 255u) / (BUSY_TRAIL * BUSY_TRAIL);
            EMBED_CALL(busy_trail_layout, BusyCtx, .pixel = &pixel, .at = at, .share = heat, .glow = args->glow,
                       .ring = args->ring, .palette = args->palette);
            EMBED_CALL(busy_burn_layout, BusyCtx, .pixel = &pixel, .across = column, .down = row, .head_x = head_x,
                       .head_y = head_y, .cell = args->cell, .level = level, .palette = args->palette);
            // rounding in the gathering is not to take the pixel past the whole of 255 with what the ground adds
            const unsigned int max = 255u - pixel.rest;
            const unsigned int red = (pixel.red > max) ? max : pixel.red;
            const unsigned int gathered_green = (pixel.green > max) ? max : pixel.green;
            const unsigned int blue = (pixel.blue > max) ? max : pixel.blue;
            red_blue[at] = (((red * 255u) << 16u) | (blue * 255u)) + 0x00800080u;
            green[at] = (gathered_green * 255u) + 0x80u;
            rest[at] = pixel.rest;
        }
    }
}

// the pictures made in one cell for the circle's side at `scale`: the pixels saved from under it, each stage's lanes,
// then what the pictures are made from: 1, or 0 with none made
EMBED_INLINE int busy_pictures_init(const BusyCtx *args)
{
    int scale = args->scale;
    args->busy->cell = NULL;
    args->busy->cell_bytes = 0ULL;
    args->busy->under_saved = 0;
    const unsigned int side = EMBED_CALL(busy_side, BusyCtx, .scale = scale);
    const unsigned int pixels = side * side;
    const unsigned int stages = args->busy->still ? 1u : GUI_BUSY_STAGES;
    const unsigned long long under_bytes = (unsigned long long)pixels * sizeof(GuiColor);
    const unsigned long long lane_bytes = (unsigned long long)stages * pixels * sizeof(unsigned int);
    const unsigned long long bytes = under_bytes + (3ULL * lane_bytes) + (3ULL * pixels);
    // the cell lies in the busy part of the block, positioned anew at each size, across the part's segments
    unsigned long long at = 0ULL;
    unsigned char *const cell =
        EMBED_CALL(gui_block_part.take, GuiBlockPartCfg, .part = GUI_BLOCK_BUSY, .cursor = &at, .bytes = bytes);
    if (cell == NULL)
    {
        return 0;
    }
    args->busy->cell = cell;
    args->busy->cell_bytes = bytes;
    args->busy->side = side;
    args->busy->stages = stages;
    args->busy->under = (GuiColor *)cell;
    args->busy->red_blue = (unsigned int *)(cell + under_bytes);
    args->busy->green = args->busy->red_blue + ((unsigned long long)stages * pixels);
    args->busy->rest = args->busy->green + ((unsigned long long)stages * pixels);
    unsigned char *const ring = cell + under_bytes + (3ULL * lane_bytes);
    unsigned char *const glow = ring + pixels;
    unsigned char *const sector = glow + pixels;
    // sizes in pixels are not negative
    const unsigned int reach =
        (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_BUSY_REACH, .scale = scale);
    const int scaled_cell = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = scale);
    const unsigned int burn_cell = (scaled_cell < 1) ? 1u : (unsigned int)scaled_cell;
    const unsigned int half = side / 2u;
    for (unsigned int row = 0u; row < side; row += 1u)
    {
        for (unsigned int column = 0u; column < side; column += 1u)
        {
            const unsigned int at = (row * side) + column;
            const unsigned int across = (column >= half) ? column - half : half - 1u - column;
            const unsigned int down = (row >= half) ? row - half : half - 1u - row;
            const unsigned int on_ring = EMBED_CALL(busy_between_arcs, BusyCtx, .outer = reach + burn_cell,
                                                    .inner = reach - burn_cell, .across = across, .down = down);
            const unsigned int about = EMBED_CALL(busy_between_arcs, BusyCtx, .outer = reach + (2u * burn_cell),
                                                  .inner = reach - (2u * burn_cell), .across = across, .down = down);
            // shares of 255 are bytes
            ring[at] = (unsigned char)on_ring;
            glow[at] = (unsigned char)((about > on_ring) ? about - on_ring : 0u);
            // the pixel's stage of the turn: the one whose way round from the top it lies along max, twice its place
            // from the center saving the middle of a pixel whole. A place inside a picture is far inside what an int
            // represents
            const int from_x = (2 * (int)column) + 1 - (int)side;
            const int from_y = (2 * (int)row) + 1 - (int)side;
            long long best = 0LL;
            unsigned int best_stage = 0u;
            for (unsigned int turn = 0u; turn < GUI_BUSY_STAGES; turn += 1u)
            {
                const long long along =
                    ((long long)from_x * EMBED_CALL(busy_sine, BusyCtx, .turn = turn)) -
                    ((long long)from_y * EMBED_CALL(busy_sine, BusyCtx, .turn = turn + (GUI_BUSY_STAGES / 4u)));
                const int further = (turn == 0u) || (along > best);
                best = further ? along : best;
                best_stage = further ? turn : best_stage;
            }
            // a stage of the turn is a byte
            sector[at] = (unsigned char)best_stage;
        }
    }
    for (unsigned int stage = 0u; stage < stages; stage += 1u)
    {
        EMBED_CALL(busy_stage_init, BusyCtx, .busy = args->busy, .palette = args->palette, .stage = stage,
                   .reach = reach, .cell = burn_cell, .ring = ring, .glow = glow, .sector = sector);
    }
    return 1;
}

// the picture of the stage positioned in the circle's rectangle, no further than what was saved
EMBED_INLINE void busy_stage_layout(const BusyCtx *args)
{
    GuiBusy *busy = args->busy;
    GuiRaster *raster = args->raster;
    const unsigned int stage = EMBED_CALL(gui_busy.stage_at, GuiBusyCfg, .busy_read = busy, .now_ms = args->now_ms);
    const unsigned int pixels = busy->side * busy->side;
    busy->stage_drawn = stage;
    EMBED_CALL(gui_raster_width_x.lanes_layout, GuiRasterWidthXCfg, .raster = raster,
               .red_blue = busy->red_blue + (stage * pixels), .green = busy->green + (stage * pixels),
               .rest = busy->rest + (stage * pixels), .width = busy->side, .height = busy->side, .x = busy->positioned_rect.x,
               .y = busy->positioned_rect.y, .clip = busy->under_rect);
}

void gui_busy_draw(const GuiBusyFrameCfg *args)
{
    args->busy->under_saved = 0;
    if (!args->busy->displayed)
    {
        return;
    }
    if (((args->busy->cell == NULL) ||
         (args->busy->side != EMBED_CALL(busy_side, BusyCtx, .scale = args->frame->scale))) &&
        !EMBED_CALL(busy_pictures_init, BusyCtx, .busy = args->busy, .palette = args->palette,
                    .scale = args->frame->scale))
    {
        return;
    }
    args->busy->positioned_rect = gui_busy_frame.rect(args);
    // the raster's sides are far inside what an int represents
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->busy->positioned_rect,
                                  .other = (GuiRect){0, 0, (int)args->raster->width, (int)args->raster->height});
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = on))
    {
        return;
    }
    // the rectangle's sides and place are not negative, it lying on the raster and not being empty
    for (unsigned int row = 0u; row < (unsigned int)on.height; row += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = &args->busy->under[row * (unsigned int)on.width],
            .src = &args->raster->pixels[(((unsigned int)on.y + row) * args->raster->width) + (unsigned int)on.x],
            .bytes = (unsigned long long)on.width * sizeof(GuiColor)});
    }
    args->busy->under_rect = on;
    args->busy->under_saved = 1;
    EMBED_CALL(busy_stage_layout, BusyCtx, .busy = args->busy, .raster = args->raster, .now_ms = args->now_ms);
}

GuiRect gui_busy_redrawn(const GuiBusyFrameCfg *args)
{
    if (!args->busy->under_saved)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect on = args->busy->under_rect;
    // the rectangle's sides and place are not negative, it being saved from the raster
    for (unsigned int row = 0u; row < (unsigned int)on.height; row += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = &args->raster->pixels[(((unsigned int)on.y + row) * args->raster->width) + (unsigned int)on.x],
            .src = &args->busy->under[row * (unsigned int)on.width],
            .bytes = (unsigned long long)on.width * sizeof(GuiColor)});
    }
    EMBED_CALL(busy_stage_layout, BusyCtx, .busy = args->busy, .raster = args->raster, .now_ms = args->now_ms);
    return on;
}
