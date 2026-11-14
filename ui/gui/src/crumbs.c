// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// crumbs.c: the crumbs positioned along the status bar's left, each crumb's text from the CLI's rows, the mark between them,
// as far as the right edge leaves whole crumbs; the rectangles positioned are for the press that follows
#include "crumbs.h"

#include "../../../includes/window/frame.h"

// the crumbs' mark between a crumb and the one it holds
static const char s_crumb_mark[] = "\xe2\x80\xba";

// The crumbs as last positioned: each one's rectangle, in the order of x, and how many
typedef struct
{
    GuiRect rect[CLI_CRUMBS_MAX];
    unsigned int count;
} CrumbsPlaces;

static CrumbsPlaces s_places;

typedef struct
{
    const GuiCrumbsCfg *crumbs;
    float x;
    const char *text;
    unsigned long long length;
    GuiColor color;
} CrumbsCtx;

// `length` bytes of `text` drawn from x on the baseline in `color` where there is a raster, else measured: the x after
EMBED_INLINE float crumbs_text(const CrumbsCtx *args)
{
    const GuiCrumbsCfg *const crumbs = args->crumbs;
    if (crumbs->raster == NULL)
    {
        return args->x + EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = crumbs->glyphs,
                                    .face = GUI_FACE_HEAD, .size = crumbs->size, .text = args->text,
                                    .length = args->length);
    }
    return EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = crumbs->raster,
                      .glyphs = crumbs->glyphs, .face = GUI_FACE_HEAD, .size = crumbs->size, .x_f32 = args->x,
                      .y = crumbs->baseline, .text = args->text, .length = args->length, .color = args->color,
                      .clip = crumbs->clip);
}

float gui_crumbs_draw(const GuiCrumbsCfg *args)
{
    s_places.count = 0u;
    const GuiPalette *const palette = args->palette;
    const float gap =
        (float)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_CRUMBS_GAP, .scale = args->scale);
    const float mark =
        EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs, .face = GUI_FACE_HEAD,
                   .size = args->size, .text = s_crumb_mark, .length = sizeof(s_crumb_mark) - 1u);
    const float right = (float)(args->clip.x + args->clip.width);
    const unsigned int rows = EMBED_CALL(cli_crumbs.rows, CliCrumbsCfg, .at = 0u);
    float x = args->x_f32;
    for (unsigned int at = 0u; (at < rows) && (at < CLI_CRUMBS_MAX); at += 1u)
    {
        CliCrumbRow row;
        if (!EMBED_CALL(cli_crumbs.row, CliCrumbsCfg, .at = at, .row = &row))
        {
            break;
        }
        const float before = (at != 0u) ? gap + mark + gap : 0.0f;
        const float width =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = args->glyphs,
                       .face = GUI_FACE_HEAD, .size = args->size, .text = row.text, .length = row.length);
        // a crumb that does not fit whole is left out, with every one after it
        if (x + before + width > right)
        {
            break;
        }
        if (at != 0u)
        {
            (void)EMBED_CALL(crumbs_text, CrumbsCtx, .crumbs = args, .x = x + gap, .text = s_crumb_mark,
                             .length = sizeof(s_crumb_mark) - 1u, .color = palette->top_menu_path);
        }
        const float start = x + before;
        GuiColor color = palette->status_text;
        color = (row.type == CLI_CRUMB_FILE) ? palette->top_menu_bright : color;
        color = (row.type == CLI_CRUMB_SYMBOL) ? palette->link : color;
        x = EMBED_CALL(crumbs_text, CrumbsCtx, .crumbs = args, .x = start, .text = row.text, .length = row.length,
                       .color = color);
        // a crumb is inside the bar, its pixels far inside an int
        s_places.rect[s_places.count] = (GuiRect){(int)start, args->clip.y, (int)(x - start + 0.5f), args->clip.height};
        s_places.count += 1u;
    }
    return x;
}

unsigned int gui_crumbs_count(const GuiCrumbsCfg *args)
{
    (void)args;
    return s_places.count;
}

GuiRect gui_crumbs_place(const GuiCrumbsCfg *args)
{
    return (args->index < s_places.count) ? s_places.rect[args->index] : (GuiRect){0, 0, 0, 0};
}

int gui_crumbs_hit(const GuiCrumbsCfg *args)
{
    // the last crumb whose left is at max x, by halving over the lefts, which are in order
    unsigned int low = 0u;
    unsigned int high = s_places.count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (s_places.rect[middle].x <= args->x)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    if ((low == 0u) || !EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = s_places.rect[low - 1u],
                                   .x = args->x, .y = args->y))
    {
        return 0;
    }
    *args->index_out = low - 1u;
    return 1;
}
