// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// signature.c: the parameter info panel. The label is drawn piece by piece, each parameter's span its own run, which
// lets the active and differing parameters take their colors and lines without a second layout
#include "signature.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "../../../cli/src/text/text.h"
#include "../answer.h"
#include "../frame.h"

void gui_signature_init(const GuiSignatureJsonCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->signature, .val = 0, .bytes = sizeof(*args->signature)});
    args->signature->active = -1;
}

void gui_signature_free(const GuiSignatureJsonCfg *args)
{
    g_cli_cells->persistent_buf_release(args->signature->label, &g_gui_cells);
    g_cli_cells->persistent_buf_release(args->signature->doc, &g_gui_cells);
    args->signature->label = NULL;
    args->signature->doc = NULL;
    args->signature->length = 0ull;
    args->signature->doc_length = 0ull;
    args->signature->params = 0u;
    args->signature->differing = 0u;
    args->signature->active = -1;
    args->signature->shown = 0;
}

void gui_signature_hide(const GuiSignatureJsonCfg *args)
{
    gui_signature_json.free(args);
    args->signature->request_deadline_ms = 0ull;
    args->signature->rect = (GuiRect){0, 0, 0, 0};
}

int gui_signature_type(const GuiSignatureTextCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        if ((args->text[at] == '(') || (args->text[at] == ','))
        {
            args->signature->request_deadline_ms = now_ms;
            return 1;
        }
    }
    return 0;
}

unsigned long long gui_signature_next_deadline_ms(const GuiSignatureCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (args->signature_read->request_deadline_ms == 0ull)
    {
        return 0ull;
    }
    const unsigned long long passed = now_ms - args->signature_read->request_deadline_ms;
    return (passed < GUI_SIGNATURE_DEBOUNCE_MS) ? GUI_SIGNATURE_DEBOUNCE_MS - passed : 1ull;
}

int gui_signature_tick(const GuiSignatureTextCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if ((args->signature->request_deadline_ms == 0ull) || (now_ms - args->signature->request_deadline_ms < GUI_SIGNATURE_DEBOUNCE_MS))
    {
        return 0;
    }
    args->signature->request_deadline_ms = 0ull;
    return 1;
}

typedef struct
{
    const char *text;
    unsigned long long length;
    GuiSignature *signature_out;
    const CliJson *json;
    unsigned int params;
    const GuiSignature *signature;
    unsigned int param;
    unsigned long long at;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    int x;
    int y;
    int wide;
    int scale;
    GuiRect clip;
} SignatureCtx;
// `length` bytes of `text` copied into a cell of their own: the cell, or NULL where it could not be taken
EMBED_INLINE char *signature_copy(const SignatureCtx *args)
{
    char *const cell = g_cli_cells->persistent_buf_alloc((args->length > 0ull) ? args->length : 1ull, &g_gui_cells);
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->text, .bytes = args->length});
    }
    return cell;
}

// the spans of the array `params` of `json`, each `[from, to]`, read into the signature, no more than it holds
EMBED_INLINE void signature_params_read(const SignatureCtx *args)
{
    const CliJson *json = args->json;
    unsigned int params = args->params;
    args->signature_out->params = 0u;
    const unsigned int count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = params, .kind = CFG_JSON_ARRAY)
                                   ? json->node[params].count
                                   : 0u;
    for (unsigned int at = 0u; (at < count) && (args->signature_out->params < GUI_SIGNATURE_PARAMS); at += 1u)
    {
        const unsigned int span = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = params, .index = at);
        args->signature_out->span[args->signature_out->params][0] =
            EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = span, .index = 0u),
                       .otherwise = 0ull);
        args->signature_out->span[args->signature_out->params][1] =
            EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = span, .index = 1u),
                       .otherwise = 0ull);
        args->signature_out->params += 1u;
    }
}

int gui_signature_put(const GuiSignatureJsonCfg *args)
{
    const CliJson *json = args->json;
    const unsigned int server = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->root, .name = "server");
    const unsigned int own = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = args->root, .name = "own");
    const int served = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                  .node = server, .kind = CFG_JSON_OBJECT);
    const unsigned int declarations = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = own, .name = "declarations");
    const unsigned int first = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = declarations, .index = 0u);
    if (!served && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                               .node = first, .kind = CFG_JSON_OBJECT))
    {
        gui_signature_json.hide(args);
        return 1;
    }
    const unsigned int from = served ? server : first;
    unsigned long long length = 0ull;
    const char *const label =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = from, .name = "label"),
                   .length_pointer = &length);
    if (label == NULL)
    {
        gui_signature_json.hide(args);
        return 1;
    }
    char *const copied = EMBED_CALL(signature_copy, SignatureCtx, .text = label, .length = length);
    if (copied == NULL)
    {
        return 0;
    }
    gui_signature_json.free(args);
    args->signature->label = copied;
    args->signature->length = length;
    EMBED_CALL(signature_params_read, SignatureCtx, .signature_out = args->signature, .json = json,
               .params = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                    .object = from, .name = "params"));
    const unsigned long long none = ~0ull;
    const unsigned long long active = EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json,
                                                 .object = served ? server : own, .name = "active", .otherwise = none);
    // a parameter's index is far inside what an int represents
    args->signature->active =
        ((active != none) && (active < (unsigned long long)args->signature->params)) ? (int)active : -1;
    args->signature->count =
        (unsigned int)EMBED_CALL(gui_answer_object.member_whole, GuiAnswerObjectCfg, .json = json, .object = server,
                                 .name = "count", .otherwise = served ? 1ull : 0ull);
    const unsigned int differs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = first, .name = "differs");
    const unsigned int differ_count = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                 .json_read = json, .node = differs, .kind = CFG_JSON_ARRAY)
                                          ? json->node[differs].count
                                          : 0u;
    for (unsigned int at = 0u; (at < differ_count) && (args->signature->differing < GUI_SIGNATURE_PARAMS); at += 1u)
    {
        args->signature->differs[args->signature->differing] =
            (unsigned int)EMBED_CALL(gui_answer_table.whole, GuiAnswerCfg, .json = json,
                                     .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                        .json_read = json, .array = differs, .index = at),
                                     .otherwise = 0ull);
        args->signature->differing += 1u;
    }
    unsigned long long doc_length = 0ull;
    const char *const doc =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = server, .name = "doc"),
                   .length_pointer = &doc_length);
    if ((doc != NULL) && (doc_length > 0ull))
    {
        args->signature->doc = EMBED_CALL(signature_copy, SignatureCtx, .text = doc, .length = doc_length);
        args->signature->doc_length = (args->signature->doc != NULL) ? doc_length : 0ull;
    }
    args->signature->shown = 1;
    args->signature->rect = (GuiRect){0, 0, 0, 0};
    return 1;
}

int gui_signature_holds(const GuiSignatureXCfg *args)
{
    return args->signature_read->shown && EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg,
                                                     .rect = args->signature_read->rect, .x = args->x, .y = args->y);
}

// whether the parameter `param` is among those the arguments differ from
EMBED_INLINE int signature_differs(const SignatureCtx *args)
{
    for (unsigned int at = 0u; at < args->signature->differing; at += 1u)
    {
        if (args->signature->differs[at] == args->param)
        {
            return 1;
        }
    }
    return 0;
}

// the parameter whose span starts at byte `at` of the label, or -1
EMBED_INLINE int signature_param_at(const SignatureCtx *args)
{
    for (unsigned int param = 0u; param < args->signature->params; param += 1u)
    {
        if ((args->signature->span[param][0] == args->at) && (args->signature->span[param][1] > args->at))
        {
            // the parameters are fewer than an int counts
            return (int)param;
        }
    }
    return -1;
}

// the label laid from x, y `wide` pixels wide, wrapped by letters, each parameter's span in its color with its line,
// drawn where `raster` is given: the height taken
static int signature_label_layout(const SignatureCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    int x = args->x;
    int y = args->y;
    int scale = args->scale;
    GuiRect clip = args->clip;
    const unsigned int size = (unsigned int)EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                       .pixels = GUI_PANEL_CODE_SIZE, .scale = scale);
    const int line_tall =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_PANEL_CODE_LINE, .scale = scale);
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_CODE, .size = size);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_CODE, .size = size);
    const int baseline = (int)((((float)line_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    const GuiColor type = EMBED_CALL(gui_palette_length.token, GuiPaletteLengthCfg, .palette = palette,
                                     .name_read = "type", .length = 4ull);
    float pen = (float)x;
    int top = y;
    int param = -1;
    unsigned long long param_end = 0ull;
    unsigned long long at = 0ull;
    while (at < args->signature->length)
    {
        if (at >= param_end)
        {
            param = EMBED_CALL(signature_param_at, SignatureCtx, .signature = args->signature, .at = at);
            param_end = (param >= 0) ? args->signature->span[param][1] : 0ull;
        }
        unsigned long long took = 0ull;
        (void)EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->signature->label,
                         .size = args->signature->length, .at = at, .length = &took);
        took = (took != 0ull) ? took : 1ull;
        const float letter_wide =
            EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = glyphs, .face = GUI_FACE_CODE,
                       .size = size, .text = args->signature->label + at, .length = took);
        if ((pen + letter_wide > (float)(x + args->wide)) && (pen > (float)x))
        {
            top += line_tall;
            pen = (float)x;
        }
        const int active = (param >= 0) && (param == args->signature->active);
        const int differs = (param >= 0) && EMBED_CALL(signature_differs, SignatureCtx, .signature = args->signature,
                                                       .param = (unsigned int)param);
        if (raster != NULL)
        {
            const GuiColor color = active ? type : palette->editor_text;
            const float after = EMBED_CALL(
                gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = raster, .glyphs = glyphs,
                .face = active ? GUI_FACE_CODE_BOLD : GUI_FACE_CODE, .size = size, .x_f32 = pen, .y = top + baseline,
                .text = args->signature->label + at, .length = took, .color = color, .clip = clip);
            if (active || differs)
            {
                const int under =
                    top + baseline + EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 3, .scale = scale);
                EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = raster,
                           .rect = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                              .one = (GuiRect){(int)pen, under, (int)(after - pen + 0.5f),
                                                               EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg,
                                                                          .pixels = 1, .scale = scale)},
                                              .other = clip),
                           .color = differs ? palette->fail : type);
            }
            pen = after;
        }
        else
        {
            pen += letter_wide;
        }
        at += took;
    }
    return (top - y) + line_tall;
}

void gui_signature_draw(const GuiSignatureRasterCfg *args)
{
    GuiRaster *raster = args->raster;
    GuiGlyphs *glyphs = args->glyphs;
    const GuiPalette *palette = args->palette;
    GuiRect anchor = args->anchor;
    GuiRect bounds = args->bounds;
    int scale = args->scale;
    if (!args->signature->shown || (args->signature->label == NULL))
    {
        return;
    }
    const int pad_x =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_PAD_X, .scale = scale);
    const int pad_y =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_PAD_Y, .scale = scale);
    const int gap = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_GAP, .scale = scale);
    int wide = EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_WIDE, .scale = scale);
    wide = (wide > bounds.width) ? bounds.width : wide;
    const int inner = wide - (2 * pad_x);
    const int label_tall =
        EMBED_CALL(signature_label_layout, SignatureCtx, .signature = args->signature, .raster = NULL, .glyphs = glyphs,
                   .palette = palette, .x = 0, .y = 0, .wide = inner, .scale = scale, .clip = bounds);
    const int doc_tall =
        (args->signature->doc != NULL)
            ? gap + EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = NULL, .glyphs = glyphs,
                               .palette = palette, .text = args->signature->doc, .length = args->signature->doc_length,
                               .x = 0, .y = 0, .wide = inner, .scale = scale, .clip = bounds)
            : 0;
    int tall = label_tall + doc_tall + (2 * pad_y);
    const int most =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = GUI_SIGNATURE_TALL, .scale = scale);
    tall = (tall > most) ? most : tall;
    args->signature->rect = EMBED_CALL(gui_panel_wide.place, GuiPanelWideCfg, .anchor = anchor, .wide = wide,
                                       .tall = tall, .back = 0, .bounds = bounds, .above = 1, .scale = scale);
    EMBED_CALL(gui_panel.box_draw, GuiPanelCfg, .raster = raster, .palette = palette, .rect = args->signature->rect,
               .scale = scale);
    const GuiRect clip = EMBED_CALL(
        gui_raster_one.rect_intersect, GuiRasterOneCfg,
        .one = (GuiRect){args->signature->rect.x + pad_x, args->signature->rect.y + pad_y, inner, tall - (2 * pad_y)},
        .other = bounds);
    const int laid = EMBED_CALL(signature_label_layout, SignatureCtx, .signature = args->signature, .raster = raster,
                                .glyphs = glyphs, .palette = palette, .x = args->signature->rect.x + pad_x,
                                .y = args->signature->rect.y + pad_y, .wide = inner, .scale = scale, .clip = clip);
    if (args->signature->doc != NULL)
    {
        (void)EMBED_CALL(gui_panel_raster.markdown, GuiPanelRasterCfg, .raster = raster, .glyphs = glyphs,
                         .palette = palette, .text = args->signature->doc, .length = args->signature->doc_length,
                         .x = args->signature->rect.x + pad_x, .y = args->signature->rect.y + pad_y + laid + gap,
                         .wide = inner, .scale = scale, .clip = clip);
    }
}
