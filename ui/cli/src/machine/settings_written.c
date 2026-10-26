// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_written.c: the settings in force written as the window reads them, each map in the order of its keys, and
// a setting's text in force read from them
#include "../text/text.h"
#include "settings_internal.h"

// a text of the layers written as a JSON string
static void cli_quoted(const CliLayers *layers, unsigned int text, VerbaLinea *out)
{
    const char *const at = layers->text + text;
    cli_json_quoted(out, at, cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}));
}

// the places `count` keys stand in when sorted by their bytes, into `order`, `keys` giving the key at a place
static void cli_order(const CliLayers *layers, const void *items,
                      unsigned int (*keys)(const void *items, unsigned int at), unsigned int count, unsigned int *order)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) &&
               (cli_text_order(layers->text + keys(items, order[place - 1u]), layers->text + keys(items, at)) > 0))
        {
            order[place] = order[place - 1u];
            place -= 1u;
        }
        order[place] = at;
    }
}

// the key of a text, and of a part that sets one
static unsigned int cli_text_key(const void *items, unsigned int at)
{
    return ((const CliLayerText *)items)[at].key;
}

static unsigned int cli_source_key(const void *items, unsigned int at)
{
    return ((const CliLayerSource *)items)[at].key;
}

// a map of keys and texts written in the order of its keys
static void cli_map_written(const CliLayers *layers, const CliLayerText *texts, unsigned int count, VerbaLinea *out)
{
    static unsigned int s_order[CLI_LAYERS_KEYS];
    cli_order(layers, texts, cli_text_key, count, s_order);
    mmgr_line_ch(out, '{');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliLayerText *const one = &texts[s_order[at]];
        mmgr_line_put(out, (at != 0u) ? "," : "");
        cli_quoted(layers, one->key, out);
        mmgr_line_ch(out, ':');
        cli_quoted(layers, one->text, out);
    }
    mmgr_line_ch(out, '}');
}

// a list of texts written as a JSON array
static void cli_list_written(const CliLayers *layers, const unsigned int *texts, unsigned int count, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at != 0u) ? "," : "");
        cli_quoted(layers, texts[at], out);
    }
    mmgr_line_ch(out, ']');
}

void cli_settings_layers_written(const CliLayers *layers, VerbaLinea *out)
{
    static unsigned int s_order[CLI_LAYERS_SOURCES];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    mmgr_line_put(out, "{\"own_file\":");
    if (layers->has_own_file)
    {
        cli_json_quoted(out, layers->own_file,
                        cellul.len(&(CatenaFinitaCfg){.src = layers->own_file, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"over\":");
    cli_map_written(layers, layers->over, layers->overs, out);
    mmgr_line_put(out, ",\"under\":");
    cli_map_written(layers, layers->under, layers->unders, out);
    mmgr_line_put(out, ",\"own\":");
    cli_list_written(layers, layers->own, layers->owns, out);
    // each key's parts, in the order of the keys, each key's in the order they were put
    mmgr_line_put(out, ",\"from\":{");
    cli_order(layers, layers->from, cli_source_key, layers->froms, s_order);
    for (unsigned int at = 0u; at < layers->froms; at += 1u)
    {
        const CliLayerSource *const one = &layers->from[s_order[at]];
        const int first =
            (at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = layers->text + layers->from[s_order[at - 1u]].key,
                                                         .other = layers->text + one->key,
                                                         .cap = CLI_CELLS_POOL}));
        if (first)
        {
            mmgr_line_put(out, (at != 0u) ? "]," : "");
            cli_quoted(layers, one->key, out);
            mmgr_line_put(out, ":[");
        }
        else
        {
            mmgr_line_ch(out, ',');
        }
        mmgr_line_put(out, "{\"file\":");
        cli_quoted(layers, one->file, out);
        mmgr_line_put(out, ",\"part\":");
        if (one->has_part)
        {
            cli_quoted(layers, one->part, out);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_put(out, ",\"text\":");
        cli_quoted(layers, one->text, out);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, (layers->froms != 0u) ? "]}" : "}");
    // each kind's settings, the kinds in order and each kind's keys in order
    mmgr_line_put(out, ",\"kinds\":{");
    for (unsigned int at = 0u; at < layers->kinds; at += 1u)
    {
        unsigned int place = at;
        while (place > 0u)
        {
            const CliKindText *const before = &layers->kind[s_order[place - 1u]];
            const CliKindText *const one = &layers->kind[at];
            const int order = cli_text_order(layers->text + before->kind, layers->text + one->kind);
            if ((order < 0) ||
                ((order == 0) && (cli_text_order(layers->text + before->key, layers->text + one->key) <= 0)))
            {
                break;
            }
            s_order[place] = s_order[place - 1u];
            place -= 1u;
        }
        s_order[place] = at;
    }
    for (unsigned int at = 0u; at < layers->kinds; at += 1u)
    {
        const CliKindText *const one = &layers->kind[s_order[at]];
        const int first =
            (at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = layers->text + layers->kind[s_order[at - 1u]].kind,
                                                         .other = layers->text + one->kind,
                                                         .cap = CLI_CELLS_POOL}));
        if (first)
        {
            mmgr_line_put(out, (at != 0u) ? "}," : "");
            cli_quoted(layers, one->kind, out);
            mmgr_line_put(out, ":{");
        }
        else
        {
            mmgr_line_ch(out, ',');
        }
        cli_quoted(layers, one->key, out);
        mmgr_line_ch(out, ':');
        cli_quoted(layers, one->text, out);
    }
    mmgr_line_put(out, (layers->kinds != 0u) ? "}}" : "}");
    mmgr_line_put(out, ",\"files\":");
    cli_list_written(layers, layers->file, layers->files, out);
    mmgr_line_put(out, ",\"problems\":");
    cli_list_written(layers, layers->problem, layers->problems, out);
    mmgr_line_put(out, ",\"system\":");
    cli_json_quoted(out, layers->system, cellul.len(&(CatenaFinitaCfg){.src = layers->system, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, '}');
    cli_lock_give(&s_lock);
}

const char *cli_settings_layer(const CliLayers *layers, const char *key, int over)
{
    const CliLayerText *const texts = over ? layers->over : layers->under;
    const unsigned int count = over ? layers->overs : layers->unders;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = layers->text + texts[at].key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            return layers->text + texts[at].text;
        }
    }
    return NULL;
}
