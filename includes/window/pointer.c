// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pointer.c: the pointer's sprites, their cell tables and the shapes made of them. The shapes are baked once into a
// sheet, and a draw puts the baked picture over the pixels saved from under it
#include "pointer.h"

#include "memoria_operor/memoria_operor.h"

// the arrow, its hot point the tip at the top left
static const char s_arrow[19][12] = {
    "#          ", "##         ", "#o#        ", "#oo#       ", "#ooo#      ", "#oooo#     ", "#ooooo#    ",
    "#oooooo#   ", "#ooooooo#  ", "#oooooooo# ", "#ooooo#####", "#oo#oo#    ", "#o# #oo#   ", "##  #oo#   ",
    "#    #oo#  ", "     #oo#  ", "      #oo# ", "      #oo# ", "       ##  ",
};

// the text beam, its hot point at its middle
static const char s_beam[16][8] = {
    "### ###", "#oo#oo#", "###o###", "  #o#  ", "  #o#  ", "  #o#  ", "  #o#  ", "  #o#  ",
    "  #o#  ", "  #o#  ", "  #o#  ", "  #o#  ", "  #o#  ", "###o###", "#oo#oo#", "### ###",
};

// the hand, its hot point the finger's tip
static const char s_hand[19][17] = {
    "     ##         ", "    #oo#        ", "    #oo#        ", "    #oo#        ", "    #oo#        ",
    "    #oo###      ", "    #oo#oo###   ", "    #oo#oo#oo## ", " ## #oo#oo#oo#o#", "#oo##oooooooo#o#",
    "#ooo#oooooooooo#", " #oo#oooooooooo#", "  #oooooooooooo#", "  #ooooooooooo# ", "   #oooooooooo# ",
    "   #ooooooooo#  ", "    #oooooooo#  ", "    #oooooooo#  ", "    ##########  ",
};

// the arrow both ways, left and right; turned on its diagonal, up and down
static const char s_both_ways[7][18] = {
    "   #         #   ", "  #o#########o#  ", " #ooooooooooooo# ", "#ooooooooooooooo#",
    " #ooooooooooooo# ", "  #o#########o#  ", "   #         #   ",
};

// the arrow both ways from the top left to the bottom right; mirrored, from the top right to the bottom left
static const char s_diagonal[15][16] = {
    "######         ", "#oooo#         ", "#ooo#          ", "#oooo#         ", "#o#ooo#        ",
    "## #ooo#       ", "    #ooo#      ", "     #ooo#     ", "      #ooo#    ", "       #ooo# ##",
    "        #ooo#o#", "         #oooo#", "          #ooo#", "         #oooo#", "         ######",
};

// the hourglass beside the arrow while the window works
static const char s_glass[8][8] = {
    "#######", "#ooooo#", " #ooo# ", "  #o#  ", "  #o#  ", " #ooo# ", "#ooooo#", "#######",
};

// a layer of the table `table_`: its rows' width is a row's bytes less the end of its text, and its height its rows
#define POINTER_TABLE(table_, turned_, mirrored_, x_, y_)                                                              \
    {&(table_)[0][0], (unsigned int)sizeof((table_)[0]) - 1u, (unsigned int)(sizeof(table_) / sizeof((table_)[0])),    \
     (turned_), (mirrored_), (x_), (y_)}

// each shape's sprite: its size in cells, its hot point and its layers, in the order of the shapes' numbers
static const GuiPointerSprite s_sprites[GUI_POINTER_SHAPES] = {
    {11u, 19u, 0u, 0u, 1u, {POINTER_TABLE(s_arrow, 0, 0, 0u, 0u)}},
    {7u, 16u, 3u, 8u, 1u, {POINTER_TABLE(s_beam, 0, 0, 0u, 0u)}},
    {16u, 19u, 5u, 0u, 1u, {POINTER_TABLE(s_hand, 0, 0, 0u, 0u)}},
    {7u, 17u, 3u, 8u, 1u, {POINTER_TABLE(s_both_ways, 1, 0, 0u, 0u)}},
    {17u, 7u, 8u, 3u, 1u, {POINTER_TABLE(s_both_ways, 0, 0, 0u, 0u)}},
    {15u, 15u, 7u, 7u, 1u, {POINTER_TABLE(s_diagonal, 0, 0, 0u, 0u)}},
    {15u, 15u, 7u, 7u, 1u, {POINTER_TABLE(s_diagonal, 0, 1, 0u, 0u)}},
    {17u, 17u, 8u, 8u, 2u, {POINTER_TABLE(s_both_ways, 0, 0, 0u, 5u), POINTER_TABLE(s_both_ways, 1, 0, 5u, 0u)}},
    {18u, 19u, 0u, 0u, 2u, {POINTER_TABLE(s_arrow, 0, 0, 0u, 0u), POINTER_TABLE(s_glass, 0, 0, 11u, 11u)}},
};

// the passes a sprite draws in: its shadow, its line and its fill
#define POINTER_PASS_SHADOW 0u
#define POINTER_PASS_LINE 1u
#define POINTER_PASS_FILL 2u

typedef struct
{
    const GuiPointer *pointer_read;
    GuiPointer *pointer;
    GuiColor *picture;
    unsigned int stride;
    GuiPointerMark *mark;
    const GuiPointerLayer *layer;
    GuiRaster *raster;
    const GuiPalette *palette;
    GuiRect rect;
    GuiColor color;
    unsigned int pass;
    int restoring;
    unsigned int cell;
    unsigned int column;
    unsigned int line;
    int left;
    int top;
} PointerCtx;

void gui_pointer_init(const GuiPointerCfg *args)
{
    args->pointer->x = 0;
    args->pointer->y = 0;
    args->pointer->shape = GUI_POINTER_ARROW;
    args->pointer->shown = 0;
    for (unsigned int at = 0u; at < GUI_POINTER_RASTERS; at += 1u)
    {
        args->pointer->mark[at].pixels = NULL;
        args->pointer->mark[at].rect = (GuiRect){0, 0, 0, 0};
        args->pointer->mark[at].saved = 0;
    }
    args->pointer->sheet.made = 0;
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

unsigned int gui_pointer_cell(const GuiPointerReadCfg *args)
{
    const int rounded = (args->scale + (GUI_SCALE_ONE / 2)) / GUI_SCALE_ONE;
    if (rounded < 1)
    {
        return 1u;
    }
    // a rounded scale below the most is a small positive count
    return ((unsigned int)rounded > GUI_POINTER_SCALE_MAX) ? GUI_POINTER_SCALE_MAX : (unsigned int)rounded;
}

GuiRect gui_pointer_rect(const GuiPointerReadCfg *args)
{
    const GuiPointer *const pointer = args->pointer_read;
    if (!pointer->shown)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiPointerSprite *const sprite = &s_sprites[pointer->shape];
    // a sprite's cells and the pixels of one are a few dozen at the most, far inside an int
    const int cell = (int)gui_pointer_cell(args);
    return (GuiRect){pointer->x - ((int)sprite->hot_x * cell), pointer->y - ((int)sprite->hot_y * cell),
                     ((int)sprite->width + 1) * cell, ((int)sprite->height + 1) * cell};
}

// the mark of the raster `raster`'s pixels: the one that came from them, else one that holds nothing, else the first
EMBED_INLINE GuiPointerMark *pointer_mark(const PointerCtx *args)
{
    GuiPointerMark *free_mark = NULL;
    for (unsigned int at = 0u; at < GUI_POINTER_RASTERS; at += 1u)
    {
        GuiPointerMark *const mark = &args->pointer->mark[at];
        if (mark->pixels == args->raster->pixels)
        {
            return mark;
        }
        if ((free_mark == NULL) && !mark->saved)
        {
            free_mark = mark;
        }
    }
    return (free_mark != NULL) ? free_mark : &args->pointer->mark[0];
}

// `rect` cut to the raster's pixels
EMBED_INLINE GuiRect pointer_on_raster(const PointerCtx *args)
{
    // a raster's sides are far inside what an int represents
    return EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg, .one = args->rect,
                      .other = (GuiRect){0, 0, (int)args->raster->width, (int)args->raster->height});
}

// the pixels of `mark`'s rectangle on the raster saved into it, or put back from it where `restoring`
EMBED_INLINE void pointer_under_copy(const PointerCtx *args)
{
    const GuiRect rect = args->mark->rect;
    for (int row = 0; row < rect.height; row += 1)
    {
        // a rect cut to the raster lies on its pixels: its rows and columns are not negative
        GuiColor *const pixel = args->raster->pixels +
                                ((unsigned long long)(rect.y + row) * args->raster->width) + (unsigned long long)rect.x;
        GuiColor *const saved = args->mark->under + ((unsigned long long)row * (unsigned long long)rect.width);
        memor.cpy(&(MemoriaCfg){.dst = args->restoring ? (void *)pixel : (void *)saved,
                                .src = args->restoring ? (const void *)saved : (const void *)pixel,
                                .bytes = (unsigned long long)rect.width * sizeof(GuiColor)});
    }
}

// the byte at `shift` of the word `word`
#define POINTER_BYTE(word_, shift_) (((word_) >> (shift_)) & 0xffu)
// `value` times `share` of 255, rounded
#define POINTER_SHARE(value_, share_) ((((value_) * (share_)) + 127u) / 255u)

// `color` put over the baked pixel at `left`, `top` of `picture`, a square of `cell` pixels. A baked pixel keeps its
// color multiplied by its alpha. Each pass over it keeps what the pass's alpha leaves of it and adds the pass's color
// times that alpha. The alpha is the share of the ground the passes cover
EMBED_INLINE void pointer_square_bake(const PointerCtx *args)
{
    const unsigned int alpha = GUI_ALPHA_OF(args->color);
    const unsigned int keep = 255u - alpha;
    // a sprite's cells and the pixels of one are a few dozen at the most: a place in the picture is not negative
    for (unsigned int row = 0u; row < args->cell; row += 1u)
    {
        GuiColor *const line = args->picture + ((((unsigned int)args->top + row) * args->stride) + (unsigned int)args->left);
        for (unsigned int column = 0u; column < args->cell; column += 1u)
        {
            const GuiColor baked = line[column];
            GuiColor made = (255u - POINTER_SHARE(255u - POINTER_BYTE(baked, 24u), keep)) << 24u;
            for (unsigned int shift = 0u; shift < 24u; shift += 8u)
            {
                const unsigned int channel = POINTER_SHARE(POINTER_BYTE(baked, shift), keep) +
                                             POINTER_SHARE(POINTER_BYTE(args->color, shift), alpha);
                made |= ((channel > 255u) ? 255u : channel) << shift;
            }
            line[column] = made;
        }
    }
}

// the cell at `column`, `line` of the layer as it is turned and mirrored: ' ', '#' or 'o'
EMBED_INLINE char pointer_layer_cell(const PointerCtx *args)
{
    const GuiPointerLayer *const layer = args->layer;
    const unsigned int across = layer->turned ? layer->height : layer->width;
    const unsigned int column = layer->mirrored ? (across - 1u - args->column) : args->column;
    // a row is its cells and the end of its text after them
    const unsigned int row = layer->turned ? column : args->line;
    const unsigned int place = layer->turned ? args->line : column;
    return layer->cells[(row * (layer->width + 1u)) + place];
}

// one pass of the layer baked into `picture`: each of its cells that the pass draws, a square of `cell` pixels
EMBED_INLINE void pointer_layer_pass(const PointerCtx *args)
{
    const GuiPointerLayer *const layer = args->layer;
    const unsigned int across = layer->turned ? layer->height : layer->width;
    const unsigned int down = layer->turned ? layer->width : layer->height;
    // the shadow lies a cell down and right of the sprite
    const int shift = (args->pass == POINTER_PASS_SHADOW) ? (int)args->cell : 0;
    for (unsigned int line = 0u; line < down; line += 1u)
    {
        for (unsigned int column = 0u; column < across; column += 1u)
        {
            const char symbol = EMBED_CALL(pointer_layer_cell, PointerCtx, .layer = layer, .column = column, .line = line);
            const int drawn = (args->pass == POINTER_PASS_SHADOW)
                                  ? ((symbol == '#') || (symbol == 'o'))
                                  : (symbol == ((args->pass == POINTER_PASS_LINE) ? '#' : 'o'));
            if (drawn)
            {
                // a sprite's cells are a few dozen at the most, far inside an int
                EMBED_CALL(pointer_square_bake, PointerCtx, .picture = args->picture, .stride = args->stride,
                           .color = args->color, .cell = args->cell,
                           .left = ((int)(layer->x + column) * (int)args->cell) + shift,
                           .top = ((int)(layer->y + line) * (int)args->cell) + shift);
            }
        }
    }
}

// The sheet baked at `cell` in the palette's pointer colors: every shape's picture made again where the cell or a
// color is not the sheet's, else nothing done
EMBED_INLINE void pointer_sheet_bake(const PointerCtx *args)
{
    GuiPointerSheet *const sheet = &args->pointer->sheet;
    const GuiColor colors[3] = {args->palette->pointer_shadow, args->palette->pointer_line,
                                args->palette->pointer_fill};
    if (sheet->made && (sheet->cell == args->cell) && (sheet->shadow == colors[POINTER_PASS_SHADOW]) &&
        (sheet->line == colors[POINTER_PASS_LINE]) && (sheet->fill == colors[POINTER_PASS_FILL]))
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = sheet->pixels, .val = 0, .bytes = sizeof(sheet->pixels)});
    for (unsigned int shape = 0u; shape < GUI_POINTER_SHAPES; shape += 1u)
    {
        const GuiPointerSprite *const sprite = &s_sprites[shape];
        for (unsigned int pass = POINTER_PASS_SHADOW; pass <= POINTER_PASS_FILL; pass += 1u)
        {
            for (unsigned int at = 0u; at < sprite->layers; at += 1u)
            {
                EMBED_CALL(pointer_layer_pass, PointerCtx, .layer = &sprite->layer[at], .picture = sheet->pixels[shape],
                           .stride = (sprite->width + 1u) * args->cell, .color = colors[pass], .pass = pass,
                           .cell = args->cell);
            }
        }
    }
    sheet->made = 1;
    sheet->cell = args->cell;
    sheet->shadow = colors[POINTER_PASS_SHADOW];
    sheet->line = colors[POINTER_PASS_LINE];
    sheet->fill = colors[POINTER_PASS_FILL];
}

// The shape's baked picture put over the raster at `rect`, the sprite's whole rectangle, cut to the raster and to its
// clip where it has one. A pixel the picture covers whole is copied, and one it does not cover is left as it is.
// Any other pixel keeps the share of the ground the picture leaves, and the picture's color is added to it
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

GuiRect gui_pointer_draw(const GuiPointerRasterCfg *args)
{
    GuiPointer *const pointer = args->pointer;
    if (args->raster->pixels == NULL)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    GuiPointerMark *const mark = EMBED_CALL(pointer_mark, PointerCtx, .pointer = pointer, .raster = args->raster);
    if (!pointer->shown)
    {
        // the caller drew the frame over the place the pixels were saved at: they are the frame's again
        if (mark->pixels == args->raster->pixels)
        {
            mark->saved = 0;
        }
        return (GuiRect){0, 0, 0, 0};
    }
    const GuiRect rect = gui_pointer_rect(&(GuiPointerReadCfg){.pointer_read = pointer, .scale = args->scale});
    mark->pixels = args->raster->pixels;
    mark->rect = EMBED_CALL(pointer_on_raster, PointerCtx, .raster = args->raster, .rect = rect);
    mark->saved = (mark->rect.width > 0) && (mark->rect.height > 0);
    if (mark->saved)
    {
        EMBED_CALL(pointer_under_copy, PointerCtx, .raster = args->raster, .mark = mark, .restoring = 0);
    }
    const unsigned int cell = gui_pointer_cell(&(GuiPointerReadCfg){.pointer_read = pointer, .scale = args->scale});
    EMBED_CALL(pointer_sheet_bake, PointerCtx, .pointer = pointer, .palette = args->palette, .cell = cell);
    EMBED_CALL(pointer_sheet_blit, PointerCtx, .pointer_read = pointer, .raster = args->raster, .rect = rect);
    return mark->rect;
}

GuiRect gui_pointer_restore(const GuiPointerRasterCfg *args)
{
    GuiPointerMark *const mark =
        EMBED_CALL(pointer_mark, PointerCtx, .pointer = args->pointer, .raster = args->raster);
    if ((args->raster->pixels == NULL) || (mark->pixels != args->raster->pixels) || !mark->saved)
    {
        return (GuiRect){0, 0, 0, 0};
    }
    EMBED_CALL(pointer_under_copy, PointerCtx, .raster = args->raster, .mark = mark, .restoring = 1);
    mark->saved = 0;
    return mark->rect;
}

GuiRect gui_pointer_saved(const GuiPointerRasterCfg *args)
{
    for (unsigned int at = 0u; at < GUI_POINTER_RASTERS; at += 1u)
    {
        const GuiPointerMark *const mark = &args->pointer->mark[at];
        if ((args->raster->pixels != NULL) && (mark->pixels == args->raster->pixels) && mark->saved)
        {
            return mark->rect;
        }
    }
    return (GuiRect){0, 0, 0, 0};
}

GuiRect gui_pointer_redrawn(const GuiPointerRasterCfg *args)
{
    const GuiRect restored = gui_pointer_restore(args);
    const GuiRect drawn = gui_pointer_draw(args);
    if ((restored.width <= 0) || (restored.height <= 0))
    {
        return drawn;
    }
    if ((drawn.width <= 0) || (drawn.height <= 0))
    {
        return restored;
    }
    const int left = (restored.x < drawn.x) ? restored.x : drawn.x;
    const int top = (restored.y < drawn.y) ? restored.y : drawn.y;
    const int right = ((restored.x + restored.width) > (drawn.x + drawn.width)) ? (restored.x + restored.width)
                                                                                : (drawn.x + drawn.width);
    const int foot = ((restored.y + restored.height) > (drawn.y + drawn.height)) ? (restored.y + restored.height)
                                                                                 : (drawn.y + drawn.height);
    return (GuiRect){left, top, right - left, foot - top};
}
