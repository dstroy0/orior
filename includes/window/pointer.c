// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pointer.c: the pointer's sprites, each a set of figures with rounded corners, and the glass they are baked in. The
// shapes are baked once into a sheet; the picture goes to the pointer's own surface, or a draw puts it over a raster
#include "pointer.h"

// a figure of `count_` points with corners rounded by `corner_`, the points after
#define POINTER_FIGURE(corner_, count_, ...)                                                                           \
    {                                                                                                                  \
        (count_), (corner_),                                                                                           \
        {                                                                                                              \
            __VA_ARGS__                                                                                                \
        }                                                                                                              \
    }

// the arrow, its tip at the top left; the double arrow from left to right; the arrow's figure is the busy arrow's too
#define POINTER_ARROW_FIGURE POINTER_FIGURE(1.0, 4u, {1.5, 1.5}, {19.0, 11.0}, {10.0, 12.6}, {7.4, 20.8})
#define POINTER_BOTH_WAYS_FIGURE                                                                                       \
    POINTER_FIGURE(0.7, 10u, {1.0, 5.5}, {6.2, 1.0}, {6.2, 4.0}, {15.8, 4.0}, {15.8, 1.0}, {21.0, 5.5}, {15.8, 10.0},  \
                   {15.8, 7.0}, {6.2, 7.0}, {6.2, 10.0})

// The compass of the glide: an orb at its middle, then an arrow each way round it, north first and on round to the
// east. The arrows' figures follow the orb in the order of the glide's shapes
#define POINTER_COMPASS_FIGURES                                                                                        \
    {POINTER_FIGURE(3.4, 1u, {12.0, 12.0}),                                                                            \
     POINTER_FIGURE(0.5, 3u, {12.0, 1.2}, {14.4, 5.6}, {9.6, 5.6}),                                                    \
     POINTER_FIGURE(0.5, 3u, {19.637, 4.363}, {18.222, 9.172}, {14.828, 5.778}),                                       \
     POINTER_FIGURE(0.5, 3u, {22.8, 12.0}, {18.4, 14.4}, {18.4, 9.6}),                                                 \
     POINTER_FIGURE(0.5, 3u, {19.637, 19.637}, {14.828, 18.222}, {18.222, 14.828}),                                    \
     POINTER_FIGURE(0.5, 3u, {12.0, 22.8}, {9.6, 18.4}, {14.4, 18.4}),                                                 \
     POINTER_FIGURE(0.5, 3u, {4.363, 19.637}, {5.778, 14.828}, {9.172, 18.222}),                                       \
     POINTER_FIGURE(0.5, 3u, {1.2, 12.0}, {5.6, 9.6}, {5.6, 14.4}),                                                    \
     POINTER_FIGURE(0.5, 3u, {4.363, 4.363}, {9.172, 5.778}, {5.778, 9.172})}
// the compass with the figure `lit_` lit, 0 for none: the orb and the lit arrow drawn, every other arrow not
#define POINTER_COMPASS(lit_)                                                                                          \
    {24.0, 24.0, 12.0, 12.0, 0u, 12.0, 12.0, 9u, POINTER_COMPASS_FIGURES, (lit_), 0x1feu & ~(1u << (lit_))}

// the sprite of each shape, in the order of the numbers of the shapes. It holds its size and hot point, and the eighths
// of a turn that its figures turn by. It holds the place in the figures that its center shows, and its figures
static const GuiPointerSprite s_sprites[GUI_POINTER_SHAPES] = {
    {20.0, 22.0, 1.5, 1.5, 0u, 10.0, 11.0, 1u, {POINTER_ARROW_FIGURE}},
    {9.0,
     20.0,
     4.5,
     10.0,
     0u,
     4.5,
     10.0,
     3u,
     {POINTER_FIGURE(0.5, 4u, {3.7, 2.2}, {5.3, 2.2}, {5.3, 17.8}, {3.7, 17.8}),
      POINTER_FIGURE(0.5, 4u, {1.3, 1.0}, {7.7, 1.0}, {7.7, 2.4}, {1.3, 2.4}),
      POINTER_FIGURE(0.5, 4u, {1.3, 17.6}, {7.7, 17.6}, {7.7, 19.0}, {1.3, 19.0})}},
    {18.0,
     22.0,
     6.5,
     1.0,
     0u,
     9.0,
     11.0,
     6u,
     {POINTER_FIGURE(1.3, 4u, {5.2, 1.8}, {7.8, 1.8}, {7.8, 11.5}, {5.2, 11.5}),
      POINTER_FIGURE(1.2, 4u, {8.8, 7.4}, {11.2, 7.4}, {11.2, 11.5}, {8.8, 11.5}),
      POINTER_FIGURE(1.1, 4u, {12.2, 8.4}, {14.4, 8.4}, {14.4, 11.5}, {12.2, 11.5}),
      POINTER_FIGURE(1.0, 4u, {15.2, 10.0}, {16.6, 10.0}, {16.6, 12.5}, {15.2, 12.5}),
      POINTER_FIGURE(2.0, 4u, {4.6, 10.2}, {16.6, 10.2}, {15.8, 19.4}, {6.0, 19.4}),
      POINTER_FIGURE(1.2, 4u, {1.6, 11.6}, {3.2, 10.8}, {6.6, 14.2}, {5.4, 16.4})}},
    {11.0, 22.0, 5.5, 11.0, 2u, 11.0, 5.5, 1u, {POINTER_BOTH_WAYS_FIGURE}},
    {22.0, 11.0, 11.0, 5.5, 0u, 11.0, 5.5, 1u, {POINTER_BOTH_WAYS_FIGURE}},
    {19.0, 19.0, 9.5, 9.5, 1u, 11.0, 5.5, 1u, {POINTER_BOTH_WAYS_FIGURE}},
    {19.0, 19.0, 9.5, 9.5, 3u, 11.0, 5.5, 1u, {POINTER_BOTH_WAYS_FIGURE}},
    POINTER_COMPASS(0u),
    {24.0,
     24.0,
     1.5,
     1.5,
     0u,
     12.0,
     12.0,
     2u,
     {POINTER_ARROW_FIGURE,
      POINTER_FIGURE(0.6, 6u, {15.0, 13.5}, {22.0, 13.5}, {19.2, 18.5}, {22.0, 23.0}, {15.0, 23.0}, {17.8, 18.5})}},
    POINTER_COMPASS(1u),
    POINTER_COMPASS(2u),
    POINTER_COMPASS(3u),
    POINTER_COMPASS(4u),
    POINTER_COMPASS(5u),
    POINTER_COMPASS(6u),
    POINTER_COMPASS(7u),
    POINTER_COMPASS(8u),
};

// The glass, in pixels at the scale of 100: the rim's width; the glint's place inside the edge and its half width. The
// shadow's offset down and right, its softness, and the share of the rim's color the body takes
#define POINTER_RIM 1.4
#define POINTER_GLINT_AT 0.7
#define POINTER_GLINT_HALF 0.6
#define POINTER_SHADOW_X 1.0
#define POINTER_SHADOW_Y 1.6
#define POINTER_SHADOW_SOFT 2.4
#define POINTER_BODY_TINT 0.3

// the cosine and sine of each eighth of a turn
static const double s_turns[8][2] = {
    {1.0, 0.0},  {0.70710678118654752, 0.70710678118654752},   {0.0, 1.0},  {-0.70710678118654752, 0.70710678118654752},
    {-1.0, 0.0}, {-0.70710678118654752, -0.70710678118654752}, {0.0, -1.0}, {0.70710678118654752, -0.70710678118654752},
};

typedef struct
{
    const GuiPointer *pointer_read;
    GuiPointer *pointer;
    const GuiPointerSprite *sprite;
    const GuiPointerFigure *figure;
    GuiRaster *raster;
    const GuiPalette *palette;
    GuiColor *picture;
    GuiRect rect;
    unsigned int *nearest;
    int scale;
    double x;
    double y;
    double low;
    double high;
    double value;
} PointerCtx;

// One color of the glass as four shares of one: its red, green, blue and alpha
typedef struct
{
    double red;
    double green;
    double blue;
    double alpha;
} PointerShares;

void gui_pointer_init(const GuiPointerCfg *args)
{
    args->pointer->x = 0;
    args->pointer->y = 0;
    args->pointer->shape = GUI_POINTER_ARROW;
    args->pointer->shown = 0;
    args->pointer->sheet.made = 0;
    args->pointer->sheet.bakes = 0u;
}

int gui_pointer_place(const GuiPointerPlaceCfg *args)
{
    GuiPointer *const pointer = args->pointer;
    const unsigned int shape = (args->shape < GUI_POINTER_SHAPES) ? args->shape : GUI_POINTER_ARROW;
    const int shown = args->shown != 0;
    const int changed =
        (pointer->x != args->x) || (pointer->y != args->y) || (pointer->shape != shape) || (pointer->shown != shown);
    pointer->x = args->x;
    pointer->y = args->y;
    pointer->shape = shape;
    pointer->shown = shown;
    return changed;
}

const GuiPointerSprite *gui_pointer_sprite(const GuiPointerShapeCfg *args)
{
    return &s_sprites[(args->shape < GUI_POINTER_SHAPES) ? args->shape : GUI_POINTER_ARROW];
}

int gui_pointer_scale(const GuiPointerReadCfg *args)
{
    if (args->scale < GUI_POINTER_SCALE_LEAST)
    {
        return GUI_POINTER_SCALE_LEAST;
    }
    return (args->scale > GUI_POINTER_SCALE_MOST) ? GUI_POINTER_SCALE_MOST : args->scale;
}

// a side of a sprite in design units in the pixels of the baked picture at `scale`, its margin on both sides with it
EMBED_INLINE int pointer_pixels(const PointerCtx *args)
{
    // a sprite's side at the most scale is a few dozen pixels, far inside an int
    return (int)__builtin_ceil((args->value * (double)args->scale) / (double)GUI_SCALE_ONE) +
           (2 * (int)GUI_POINTER_MARGIN);
}

// a place of a sprite's hot point in design units in the pixels of the baked picture at `scale`, its margin with it
EMBED_INLINE int pointer_hot_pixel(const PointerCtx *args)
{
    // a hot point at the most scale is a few dozen pixels, far inside an int
    return (int)__builtin_floor(((args->value * (double)args->scale) / (double)GUI_SCALE_ONE) + 0.5) +
           (int)GUI_POINTER_MARGIN;
}

GuiRect gui_pointer_rect(const GuiPointerReadCfg *args)
{
    const GuiPointer *const pointer = args->pointer_read;
    if (!pointer->shown)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiPointerSprite *const sprite = &s_sprites[pointer->shape];
    const int scale = gui_pointer_scale(args);
    return (GuiRect){pointer->x - EMBED_CALL(pointer_hot_pixel, PointerCtx, .value = sprite->hot_x, .scale = scale),
                     pointer->y - EMBED_CALL(pointer_hot_pixel, PointerCtx, .value = sprite->hot_y, .scale = scale),
                     EMBED_CALL(pointer_pixels, PointerCtx, .value = sprite->width, .scale = scale),
                     EMBED_CALL(pointer_pixels, PointerCtx, .value = sprite->height, .scale = scale)};
}

// `rect` cut to the raster's pixels
EMBED_INLINE GuiRect pointer_on_raster(const PointerCtx *args)
{
    // a raster's sides are far inside what an int represents
    return EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                      .other = (GuiRect){0, 0, (int)args->raster->width, (int)args->raster->height});
}

// the byte at `shift` of the word `word`
#define POINTER_BYTE(word_, shift_) (((word_) >> (shift_)) & 0xffu)
// `value` times `share` of 255, rounded
#define POINTER_SHARE(value_, share_) ((((value_) * (share_)) + 127u) / 255u)

// The distance in design units from x, y to the edge of `figure`, its corners rounded: below 0 inside the figure.
// For a disc it is the distance to its center less its radius. Else the code first finds the distance to the nearest
// side. A ray to the right that crosses the sides an odd number of times puts the place inside
EMBED_INLINE double pointer_figure_distance(const PointerCtx *args)
{
    const GuiPointerFigure *const figure = args->figure;
    if (figure->points == 1u)
    {
        const double across = args->x - figure->point[0][0];
        const double down = args->y - figure->point[0][1];
        return __builtin_sqrt((across * across) + (down * down)) - figure->corner;
    }
    double nearest = 1.0e9;
    int inside = 0;
    for (unsigned int at = 0u, before = figure->points - 1u; at < figure->points; before = at, at += 1u)
    {
        const double from_x = figure->point[at][0];
        const double from_y = figure->point[at][1];
        const double side_x = figure->point[before][0] - from_x;
        const double side_y = figure->point[before][1] - from_y;
        const double to_x = args->x - from_x;
        const double to_y = args->y - from_y;
        double along = ((to_x * side_x) + (to_y * side_y)) / ((side_x * side_x) + (side_y * side_y));
        along = (along < 0.0) ? 0.0 : ((along > 1.0) ? 1.0 : along);
        const double off_x = to_x - (side_x * along);
        const double off_y = to_y - (side_y * along);
        const double squared = (off_x * off_x) + (off_y * off_y);
        nearest = (squared < nearest) ? squared : nearest;
        const int above = args->y >= from_y;
        const int below = args->y < figure->point[before][1];
        const int left = (side_x * to_y) > (side_y * to_x);
        if ((above && below && left) || (!above && !below && !left))
        {
            inside = !inside;
        }
    }
    const double distance = __builtin_sqrt(nearest);
    return (inside ? -distance : distance) - figure->corner;
}

// The distance in design units from x, y of the sprite to the edge of its figures: the least of the distances of the
// figures. The figure of that distance goes into `nearest` where the caller gives it. The code first turns the place
// back by the turn of the sprite round its center, into the units of the figures
EMBED_INLINE double pointer_sprite_distance(const PointerCtx *args)
{
    const GuiPointerSprite *const sprite = args->sprite;
    const double cosine = s_turns[sprite->eighths & 7u][0];
    const double sine = s_turns[sprite->eighths & 7u][1];
    const double across = args->x - (sprite->width * 0.5);
    const double down = args->y - (sprite->height * 0.5);
    const double figure_x = (across * cosine) + (down * sine) + sprite->center_x;
    const double figure_y = (down * cosine) - (across * sine) + sprite->center_y;
    double least = 1.0e9;
    for (unsigned int at = 0u; at < sprite->figures; at += 1u)
    {
        if ((sprite->hidden >> at) & 1u)
        {
            continue;
        }
        const double distance = EMBED_CALL(pointer_figure_distance, PointerCtx, .figure = &sprite->figure[at],
                                           .x = figure_x, .y = figure_y);
        if (distance < least)
        {
            least = distance;
            if (args->nearest != NULL)
            {
                *args->nearest = at;
            }
        }
    }
    return least;
}

// 0 at `low` and below, 1 at `high` and above, and a smooth step between
EMBED_INLINE double pointer_step(const PointerCtx *args)
{
    double share = (args->value - args->low) / (args->high - args->low);
    share = (share < 0.0) ? 0.0 : ((share > 1.0) ? 1.0 : share);
    return share * share * (3.0 - (2.0 * share));
}

// the color `color` as four shares of one
static PointerShares pointer_shares(GuiColor color)
{
    return (PointerShares){(double)POINTER_BYTE(color, 16u) / 255.0, (double)POINTER_BYTE(color, 8u) / 255.0,
                           (double)POINTER_BYTE(color, 0u) / 255.0, (double)POINTER_BYTE(color, 24u) / 255.0};
}

// `layer` put over `under`, both multiplied by their alphas, `layer` at `alpha`. What is under keeps the share that the
// alpha leaves, and the code adds the color of the layer times the alpha
static PointerShares pointer_over(PointerShares under, PointerShares layer, double alpha)
{
    const double keep = 1.0 - alpha;
    return (PointerShares){(under.red * keep) + (layer.red * alpha), (under.green * keep) + (layer.green * alpha),
                           (under.blue * keep) + (layer.blue * alpha), (under.alpha * keep) + alpha};
}

// a share of one as a byte, rounded and clamped to 0 to 255
static GuiColor pointer_byte(double share)
{
    const double clamped = (share < 0.0) ? 0.0 : ((share > 1.0) ? 1.0 : share);
    // a share clamped to 0 to 1 is a byte once scaled
    return (GuiColor)((clamped * 255.0) + 0.5);
}

// The sprite baked into `picture` at `scale` in the palette's pointer colors. The code samples each pixel at its center. Its
// distance to the figures' edge gives how much of it the glass covers, the rim, the glint and the shadow. The rim's
// color goes from the border's to the border's end across the sprite from its top left to its bottom right. The
// glint lies on the edges that face the top left. A lit figure's body is the rim's color whole, and the glint washes
// over it, most at the top left
EMBED_INLINE void pointer_sprite_bake(const PointerCtx *args)
{
    const GuiPointerSprite *const sprite = args->sprite;
    const GuiPalette *const palette = args->palette;
    const double scale = (double)args->scale / (double)GUI_SCALE_ONE;
    const int width = EMBED_CALL(pointer_pixels, PointerCtx, .value = sprite->width, .scale = args->scale);
    const int height = EMBED_CALL(pointer_pixels, PointerCtx, .value = sprite->height, .scale = args->scale);
    const PointerShares fill = pointer_shares(palette->pointer_fill);
    const PointerShares line = pointer_shares(palette->pointer_line);
    const PointerShares line_end = pointer_shares(palette->pointer_line_end);
    const PointerShares glint = pointer_shares(palette->pointer_glint);
    const PointerShares shadow = pointer_shares(palette->pointer_shadow);
    const double rim = POINTER_RIM * scale;
    const double half = 0.5 / scale;
    for (int row = 0; row < height; row += 1)
    {
        for (int column = 0; column < width; column += 1)
        {
            const double x = (((double)column + 0.5) - (double)GUI_POINTER_MARGIN) / scale;
            const double y = (((double)row + 0.5) - (double)GUI_POINTER_MARGIN) / scale;
            unsigned int nearest = 0u;
            const double edge =
                EMBED_CALL(pointer_sprite_distance, PointerCtx, .sprite = sprite, .x = x, .y = y, .nearest = &nearest) *
                scale;
            const int lit = (sprite->lit != 0u) && (nearest == sprite->lit);
            const double cover = (edge > 0.5) ? 0.0 : ((edge < -0.5) ? 1.0 : (0.5 - edge));
            const double shadowed = EMBED_CALL(pointer_sprite_distance, PointerCtx, .sprite = sprite,
                                               .x = x - POINTER_SHADOW_X, .y = y - POINTER_SHADOW_Y) *
                                    scale;
            PointerShares made = {0.0, 0.0, 0.0, 0.0};
            made = pointer_over(made, shadow,
                                shadow.alpha * (1.0 - cover) *
                                    (1.0 - EMBED_CALL(pointer_step, PointerCtx, .low = -1.0,
                                                      .high = POINTER_SHADOW_SOFT * scale, .value = shadowed)));
            if (cover > 0.0)
            {
                double across = ((x / sprite->width) + (y / sprite->height)) * 0.5;
                across = (across < 0.0) ? 0.0 : ((across > 1.0) ? 1.0 : across);
                const PointerShares rim_color = {line.red + ((line_end.red - line.red) * across),
                                                 line.green + ((line_end.green - line.green) * across),
                                                 line.blue + ((line_end.blue - line.blue) * across),
                                                 line.alpha + ((line_end.alpha - line.alpha) * across)};
                const double tint = lit ? 1.0 : POINTER_BODY_TINT;
                const PointerShares body = {fill.red + ((rim_color.red - fill.red) * tint),
                                            fill.green + ((rim_color.green - fill.green) * tint),
                                            fill.blue + ((rim_color.blue - fill.blue) * tint),
                                            lit ? rim_color.alpha : fill.alpha};
                made = pointer_over(made, body, body.alpha * cover);
                const double depth = -edge;
                const double rim_share =
                    1.0 - EMBED_CALL(pointer_step, PointerCtx, .low = rim - 0.6, .high = rim + 0.6, .value = depth);
                made = pointer_over(made, rim_color, rim_color.alpha * cover * rim_share);
                const double toward_x =
                    EMBED_CALL(pointer_sprite_distance, PointerCtx, .sprite = sprite, .x = x + half, .y = y) -
                    EMBED_CALL(pointer_sprite_distance, PointerCtx, .sprite = sprite, .x = x - half, .y = y);
                const double toward_y =
                    EMBED_CALL(pointer_sprite_distance, PointerCtx, .sprite = sprite, .x = x, .y = y + half) -
                    EMBED_CALL(pointer_sprite_distance, PointerCtx, .sprite = sprite, .x = x, .y = y - half);
                const double length = __builtin_sqrt((toward_x * toward_x) + (toward_y * toward_y));
                double facing = (length > 0.0) ? (-(toward_x + toward_y) * 0.70710678118654752 / length) : 0.0;
                facing = (facing < 0.0) ? 0.0 : facing;
                double band = 1.0 - (__builtin_fabs(depth - (POINTER_GLINT_AT * scale)) / (POINTER_GLINT_HALF * scale));
                band = (band < 0.0) ? 0.0 : band;
                const double glinted = lit ? (0.35 * (1.0 - across)) : (band * facing * facing);
                made = pointer_over(made, glint, glint.alpha * cover * glinted);
            }
            args->picture[(row * width) + column] = (pointer_byte(made.alpha) << 24u) |
                                                    (pointer_byte(made.red) << 16u) | (pointer_byte(made.green) << 8u) |
                                                    pointer_byte(made.blue);
        }
    }
}

// The sheet, baked at `scale` in the pointer colors of the palette. The code makes the picture of each shape again
// where the scale or a color is not that of the sheet. Else it does nothing
EMBED_INLINE void pointer_sheet_bake(const PointerCtx *args)
{
    GuiPointerSheet *const sheet = &args->pointer->sheet;
    const GuiPalette *const palette = args->palette;
    if (sheet->made && (sheet->scale == args->scale) && (sheet->shadow == palette->pointer_shadow) &&
        (sheet->line == palette->pointer_line) && (sheet->line_end == palette->pointer_line_end) &&
        (sheet->glint == palette->pointer_glint) && (sheet->fill == palette->pointer_fill))
    {
        return;
    }
    for (unsigned int shape = 0u; shape < GUI_POINTER_SHAPES; shape += 1u)
    {
        EMBED_CALL(pointer_sprite_bake, PointerCtx, .sprite = &s_sprites[shape], .palette = palette,
                   .picture = sheet->pixels[shape], .scale = args->scale);
    }
    sheet->made = 1;
    sheet->bakes += 1u;
    sheet->scale = args->scale;
    sheet->shadow = palette->pointer_shadow;
    sheet->line = palette->pointer_line;
    sheet->line_end = palette->pointer_line_end;
    sheet->glint = palette->pointer_glint;
    sheet->fill = palette->pointer_fill;
}

// The shape's baked picture put over the raster at `rect`, the sprite's whole rectangle, cut to the raster and to its
// clip where it has one. The code copies a pixel that the picture covers whole. A pixel that it does not cover
// stays as it is. Any other pixel keeps the share of the ground that the picture leaves, plus the color of the picture
EMBED_INLINE void pointer_sheet_blit(const PointerCtx *args)
{
    GuiRaster *const raster = args->raster;
    const GuiRect rect = args->rect;
    GuiRect cut = EMBED_CALL(pointer_on_raster, PointerCtx, .raster = raster, .rect = rect);
    if ((raster->clip.width > 0) && (raster->clip.height > 0))
    {
        cut = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = cut, .other = raster->clip);
    }
    const GuiColor *const picture = args->pointer_read->sheet.pixels[args->pointer_read->shape];
    for (int row = cut.y; row < cut.y + cut.height; row += 1)
    {
        // a rect cut to the raster lies on its pixels and inside the sprite's rectangle: no place here is negative
        GuiColor *const pixel = raster->pixels + ((unsigned long long)row * raster->width);
        const GuiColor *const baked = picture + ((unsigned long long)(row - rect.y) * (unsigned long long)rect.width);
        for (int column = cut.x; column < cut.x + cut.width; column += 1)
        {
            const GuiColor source = baked[column - rect.x];
            const unsigned int alpha = POINTER_BYTE(source, 24u);
            if (alpha == 255u)
            {
                pixel[column] = source;
            }
            else if (alpha != 0u)
            {
                const GuiColor ground = pixel[column];
                GuiColor made = 0xff000000u;
                for (unsigned int shift = 0u; shift < 24u; shift += 8u)
                {
                    const unsigned int channel =
                        POINTER_SHARE(POINTER_BYTE(ground, shift), 255u - alpha) + POINTER_BYTE(source, shift);
                    made |= ((channel > 255u) ? 255u : channel) << shift;
                }
                pixel[column] = made;
            }
        }
    }
}

const GuiColor *gui_pointer_picture(const GuiPointerRasterCfg *args)
{
    GuiPointer *const pointer = args->pointer;
    EMBED_CALL(pointer_sheet_bake, PointerCtx, .pointer = pointer, .palette = args->palette,
               .scale = gui_pointer_scale(&(GuiPointerReadCfg){.pointer_read = pointer, .scale = args->scale}));
    return pointer->sheet.pixels[pointer->shape];
}

void gui_pointer_hand_bake(const GuiPointerHandCfg *args)
{
    // the palette as given, the fill the hand's mark
    GuiPalette marked = *args->palette;
    marked.pointer_fill = args->fill;
    const unsigned int shape = (args->shape < GUI_POINTER_SHAPES) ? args->shape : GUI_POINTER_ARROW;
    EMBED_CALL(pointer_sprite_bake, PointerCtx, .sprite = &s_sprites[shape], .palette = &marked,
               .picture = args->picture,
               .scale = gui_pointer_scale(&(GuiPointerReadCfg){.pointer_read = NULL, .scale = args->scale}));
}

GuiRect gui_pointer_hand_rect(const GuiPointerHandCfg *args)
{
    const unsigned int shape = (args->shape < GUI_POINTER_SHAPES) ? args->shape : GUI_POINTER_ARROW;
    const GuiPointerSprite *const sprite = &s_sprites[shape];
    const int scale = gui_pointer_scale(&(GuiPointerReadCfg){.pointer_read = NULL, .scale = args->scale});
    return (GuiRect){args->x - EMBED_CALL(pointer_hot_pixel, PointerCtx, .value = sprite->hot_x, .scale = scale),
                     args->y - EMBED_CALL(pointer_hot_pixel, PointerCtx, .value = sprite->hot_y, .scale = scale),
                     EMBED_CALL(pointer_pixels, PointerCtx, .value = sprite->width, .scale = scale),
                     EMBED_CALL(pointer_pixels, PointerCtx, .value = sprite->height, .scale = scale)};
}

GuiRect gui_pointer_draw(const GuiPointerRasterCfg *args)
{
    GuiPointer *const pointer = args->pointer;
    if ((args->raster->pixels == NULL) || !pointer->shown)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect rect = gui_pointer_rect(&(GuiPointerReadCfg){.pointer_read = pointer, .scale = args->scale});
    (void)gui_pointer_picture(args);
    EMBED_CALL(pointer_sheet_blit, PointerCtx, .pointer_read = pointer, .raster = args->raster, .rect = rect);
    return EMBED_CALL(pointer_on_raster, PointerCtx, .raster = args->raster, .rect = rect);
}
