// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// signature.c: the parameter info panel. The label is drawn piece by piece, each parameter's span its own run, which
// lets the active and differing parameters take their colors and lines without a second layout
#include "signature.h"

#include "../../../cli/src/command_line/cli_internal.h"
#include "../../../cli/src/text/text.h"
#include "../answer.h"
#include "../frame.h"

void gui_signature_made(GuiSignature *signature)
{
    memor.set(&(MemoriaCfg){.dst = signature, .val = 0, .bytes = sizeof(*signature)});
    signature->active = -1;
}

void gui_signature_given(GuiSignature *signature)
{
    g_cli_cells->persistent_buf_release(signature->label, NULL);
    g_cli_cells->persistent_buf_release(signature->doc, NULL);
    signature->label = NULL;
    signature->doc = NULL;
    signature->length = 0ull;
    signature->doc_length = 0ull;
    signature->params = 0u;
    signature->differing = 0u;
    signature->active = -1;
    signature->shown = 0;
}

void gui_signature_hidden(GuiSignature *signature)
{
    gui_signature_given(signature);
    signature->typed_ms = 0ull;
    signature->rect = (GuiRect){0, 0, 0, 0};
}

int gui_signature_typed(GuiSignature *signature, const char *text, unsigned long long size, unsigned long long now)
{
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if ((text[at] == '(') || (text[at] == ','))
        {
            signature->typed_ms = now;
            return 1;
        }
    }
    return 0;
}

unsigned long long gui_signature_due(const GuiSignature *signature, unsigned long long now)
{
    if (signature->typed_ms == 0ull)
    {
        return 0ull;
    }
    const unsigned long long passed = now - signature->typed_ms;
    return (passed < GUI_SIGNATURE_WAIT) ? GUI_SIGNATURE_WAIT - passed : 1ull;
}

int gui_signature_ticked(GuiSignature *signature, unsigned long long now)
{
    if ((signature->typed_ms == 0ull) || (now - signature->typed_ms < GUI_SIGNATURE_WAIT))
    {
        return 0;
    }
    signature->typed_ms = 0ull;
    return 1;
}

// `length` bytes of `text` copied into a cell of their own: the cell, or NULL where it could not be taken
static char *signature_copied(const char *text, unsigned long long length)
{
    char *const cell = g_cli_cells->persistent_buf_alloc((length > 0ull) ? length : 1ull, NULL);
    if (cell != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = text, .bytes = length});
    }
    return cell;
}

// the spans of the array `params` of `json`, each `[from, to]`, read into the signature, no more than it holds
static void signature_params_read(GuiSignature *signature, const CliJson *json, unsigned int params)
{
    signature->params = 0u;
    const unsigned int count = cli_json_is(json, params, CFG_JSON_ARRAY) ? json->node[params].count : 0u;
    for (unsigned int at = 0u; (at < count) && (signature->params < GUI_SIGNATURE_PARAMS); at += 1u)
    {
        const unsigned int span = cli_json_item(json, params, at);
        signature->span[signature->params][0] = gui_answer_whole(json, cli_json_item(json, span, 0u), 0ull);
        signature->span[signature->params][1] = gui_answer_whole(json, cli_json_item(json, span, 1u), 0ull);
        signature->params += 1u;
    }
}

int gui_signature_put(GuiSignature *signature, const CliJson *json, unsigned int root)
{
    const unsigned int server = cli_json_member(json, root, "server");
    const unsigned int own = cli_json_member(json, root, "own");
    const int served = cli_json_is(json, server, CFG_JSON_OBJECT);
    const unsigned int declarations = cli_json_member(json, own, "declarations");
    const unsigned int first = cli_json_item(json, declarations, 0u);
    if (!served && !cli_json_is(json, first, CFG_JSON_OBJECT))
    {
        gui_signature_hidden(signature);
        return 1;
    }
    const unsigned int from = served ? server : first;
    unsigned long long length = 0ull;
    const char *const label = cli_json_string(json, cli_json_member(json, from, "label"), &length);
    if (label == NULL)
    {
        gui_signature_hidden(signature);
        return 1;
    }
    char *const copied = signature_copied(label, length);
    if (copied == NULL)
    {
        return 0;
    }
    gui_signature_given(signature);
    signature->label = copied;
    signature->length = length;
    signature_params_read(signature, json, cli_json_member(json, from, "params"));
    const unsigned long long none = ~0ull;
    const unsigned long long active = gui_answer_member_whole(json, served ? server : own, "active", none);
    // a parameter's index is far inside what an int represents
    signature->active = ((active != none) && (active < (unsigned long long)signature->params)) ? (int)active : -1;
    signature->count = (unsigned int)gui_answer_member_whole(json, server, "count", served ? 1ull : 0ull);
    const unsigned int differs = cli_json_member(json, first, "differs");
    const unsigned int differ_count = cli_json_is(json, differs, CFG_JSON_ARRAY) ? json->node[differs].count : 0u;
    for (unsigned int at = 0u; (at < differ_count) && (signature->differing < GUI_SIGNATURE_PARAMS); at += 1u)
    {
        signature->differs[signature->differing] = (unsigned int)gui_answer_whole(json, cli_json_item(json, differs, at), 0ull);
        signature->differing += 1u;
    }
    unsigned long long doc_length = 0ull;
    const char *const doc = cli_json_string(json, cli_json_member(json, server, "doc"), &doc_length);
    if ((doc != NULL) && (doc_length > 0ull))
    {
        signature->doc = signature_copied(doc, doc_length);
        signature->doc_length = (signature->doc != NULL) ? doc_length : 0ull;
    }
    signature->shown = 1;
    signature->rect = (GuiRect){0, 0, 0, 0};
    return 1;
}

int gui_signature_holds(const GuiSignature *signature, int x, int y)
{
    return signature->shown && gui_rect_holds(signature->rect, x, y);
}

// whether the parameter `param` is among those the arguments differ from
static int signature_differs(const GuiSignature *signature, unsigned int param)
{
    for (unsigned int at = 0u; at < signature->differing; at += 1u)
    {
        if (signature->differs[at] == param)
        {
            return 1;
        }
    }
    return 0;
}

// the parameter whose span starts at byte `at` of the label, or -1
static int signature_param_at(const GuiSignature *signature, unsigned long long at)
{
    for (unsigned int param = 0u; param < signature->params; param += 1u)
    {
        if ((signature->span[param][0] == at) && (signature->span[param][1] > at))
        {
            // the parameters are fewer than an int counts
            return (int)param;
        }
    }
    return -1;
}

// the label laid from x, y `wide` pixels wide, wrapped by letters, each parameter's span in its color with its line,
// drawn where `raster` is given: the height taken
static int signature_label_laid(const GuiSignature *signature, GuiRaster *raster, GuiGlyphs *glyphs,
                                const GuiPalette *palette, int x, int y, int wide, float scale, GuiRect clip)
{
    const unsigned int size = (unsigned int)(((float)GUI_PANEL_CODE_SIZE * scale) + 0.5f);
    const int line_tall = gui_scaled(GUI_PANEL_CODE_LINE, scale);
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_CODE, size);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_CODE, size);
    const int baseline = (int)((((float)line_tall - ascent - descent) / 2.0f) + ascent + 0.5f);
    const GuiColor type = gui_palette_token(palette, "type", 4ull);
    float pen = (float)x;
    int top = y;
    int param = -1;
    unsigned long long param_end = 0ull;
    unsigned long long at = 0ull;
    while (at < signature->length)
    {
        if (at >= param_end)
        {
            param = signature_param_at(signature, at);
            param_end = (param >= 0) ? signature->span[param][1] : 0ull;
        }
        unsigned long long took = 0ull;
        (void)cli_text_letter(signature->label, signature->length, at, &took);
        took = (took != 0ull) ? took : 1ull;
        const float letter_wide = gui_text_width(glyphs, GUI_FACE_CODE, size, signature->label + at, took);
        if ((pen + letter_wide > (float)(x + wide)) && (pen > (float)x))
        {
            top += line_tall;
            pen = (float)x;
        }
        const int active = (param >= 0) && (param == signature->active);
        const int differs = (param >= 0) && signature_differs(signature, (unsigned int)param);
        if (raster != NULL)
        {
            const GuiColor color = active ? type : palette->editor_text;
            const float after = gui_text_drawn(raster, glyphs, active ? GUI_FACE_CODE_BOLD : GUI_FACE_CODE, size, pen, top + baseline,
                                               signature->label + at, took, color, clip);
            if (active || differs)
            {
                const int under = top + baseline + gui_scaled(3, scale);
                gui_raster_filled(raster, gui_rect_met((GuiRect){(int)pen, under, (int)(after - pen + 0.5f), gui_scaled(1, scale)}, clip),
                                  differs ? palette->fail : type);
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

void gui_signature_drawn(GuiSignature *signature, GuiRaster *raster, GuiGlyphs *glyphs, const GuiPalette *palette,
                         GuiRect anchor, GuiRect bounds, float scale)
{
    if (!signature->shown || (signature->label == NULL))
    {
        return;
    }
    const int pad_x = gui_scaled(GUI_SIGNATURE_PAD_X, scale);
    const int pad_y = gui_scaled(GUI_SIGNATURE_PAD_Y, scale);
    const int gap = gui_scaled(GUI_SIGNATURE_GAP, scale);
    int wide = gui_scaled(GUI_SIGNATURE_WIDE, scale);
    wide = (wide > bounds.width) ? bounds.width : wide;
    const int inner = wide - (2 * pad_x);
    const int label_tall = signature_label_laid(signature, NULL, glyphs, palette, 0, 0, inner, scale, bounds);
    const int doc_tall = (signature->doc != NULL)
                             ? gap + gui_panel_markdown(NULL, glyphs, palette, signature->doc, signature->doc_length, 0, 0, inner, scale, bounds)
                             : 0;
    int tall = label_tall + doc_tall + (2 * pad_y);
    const int most = gui_scaled(GUI_SIGNATURE_TALL, scale);
    tall = (tall > most) ? most : tall;
    signature->rect = gui_panel_placed(anchor, wide, tall, 0, bounds, 1, scale);
    gui_panel_box_drawn(raster, palette, signature->rect, scale);
    const GuiRect clip = gui_rect_met((GuiRect){signature->rect.x + pad_x, signature->rect.y + pad_y, inner, tall - (2 * pad_y)}, bounds);
    const int laid = signature_label_laid(signature, raster, glyphs, palette, signature->rect.x + pad_x, signature->rect.y + pad_y, inner,
                                          scale, clip);
    if (signature->doc != NULL)
    {
        (void)gui_panel_markdown(raster, glyphs, palette, signature->doc, signature->doc_length, signature->rect.x + pad_x,
                                 signature->rect.y + pad_y + laid + gap, inner, scale, clip);
    }
}
