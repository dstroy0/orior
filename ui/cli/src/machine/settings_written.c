// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_written.c: the settings in force written as the window reads them, each map in the order of its keys, and
// a setting's text in force read from them
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "settings_internal.h"

typedef struct
{
    const CliLayers *layers;
    unsigned int text;
    VerbaLinea *out;
    const void *items;
    unsigned int (*keys)(const CliItemsAtCfg *args);
    unsigned int count;
    unsigned int *order;
    const CliLayerText *texts;
    const unsigned int *texts_u32s;
} SettingsWrittenCtx;

// a text of the layers written as a JSON string
EMBED_INLINE void cli_quote(const SettingsWrittenCtx *args)
{
    unsigned int text = args->text;
    VerbaLinea *out = args->out;
    const char *const at = args->layers->text + text;
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = at,
               .length = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}));
}

// the places `count` keys stand in when sorted by their bytes, into `order`, `keys` giving the key at a place
EMBED_INLINE void cli_order(const SettingsWrittenCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) &&
               (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                           .one = args->layers->text + EMBED_CALL(args->keys, CliItemsAtCfg, .items = args->items,
                                                                  .at = args->order[place - 1u]),
                           .other_pointer = args->layers->text +
                                            EMBED_CALL(args->keys, CliItemsAtCfg, .items = args->items, .at = at)) > 0))
        {
            args->order[place] = args->order[place - 1u];
            place -= 1u;
        }
        args->order[place] = at;
    }
}

// the key of a text, and of a part that sets one
static unsigned int cli_text_key(const CliItemsAtCfg *args)
{
    return ((const CliLayerText *)args->items)[args->at].key;
}

static unsigned int cli_source_key(const CliItemsAtCfg *args)
{
    return ((const CliLayerSource *)args->items)[args->at].key;
}

// a map of keys and texts written in the order of its keys
EMBED_INLINE void cli_map_write(const SettingsWrittenCtx *args)
{
    static unsigned int s_order[CLI_LAYERS_KEYS];
    EMBED_CALL(cli_order, SettingsWrittenCtx, .layers = args->layers, .items = args->texts, .keys = cli_text_key,
               .count = args->count, .order = s_order);
    mmgr_line_ch(args->out, '{');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const CliLayerText *const one = &args->texts[s_order[at]];
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers, .text = one->key, .out = args->out);
        mmgr_line_ch(args->out, ':');
        EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers, .text = one->text, .out = args->out);
    }
    mmgr_line_ch(args->out, '}');
}

// a list of texts written as a JSON array
EMBED_INLINE void cli_list_write(const SettingsWrittenCtx *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers, .text = args->texts_u32s[at],
                   .out = args->out);
    }
    mmgr_line_ch(args->out, ']');
}

void cli_settings_layers_write(const CliSettingsOutCfg *args)
{
    static unsigned int s_order[CLI_LAYERS_SOURCES];
    mmgr_line_put(args->out, "{\"own_file\":");
    if (args->layers_read->has_own_file)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->layers_read->own_file,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->layers_read->own_file, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_put(args->out, ",\"over\":");
    EMBED_CALL(cli_map_write, SettingsWrittenCtx, .layers = args->layers_read, .texts = args->layers_read->over,
               .count = args->layers_read->overs, .out = args->out);
    mmgr_line_put(args->out, ",\"under\":");
    EMBED_CALL(cli_map_write, SettingsWrittenCtx, .layers = args->layers_read, .texts = args->layers_read->under,
               .count = args->layers_read->unders, .out = args->out);
    mmgr_line_put(args->out, ",\"own\":");
    EMBED_CALL(cli_list_write, SettingsWrittenCtx, .layers = args->layers_read, .texts_u32s = args->layers_read->own,
               .count = args->layers_read->owns, .out = args->out);
    // each key's parts, in the order of the keys, each key's in the order they were put
    mmgr_line_put(args->out, ",\"from\":{");
    EMBED_CALL(cli_order, SettingsWrittenCtx, .layers = args->layers_read, .items = args->layers_read->from,
               .keys = cli_source_key, .count = args->layers_read->froms, .order = s_order);
    for (unsigned int at = 0u; at < args->layers_read->froms; at += 1u)
    {
        const CliLayerSource *const one = &args->layers_read->from[s_order[at]];
        const int first =
            (at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = args->layers_read->text +
                                                                args->layers_read->from[s_order[at - 1u]].key,
                                                         .other = args->layers_read->text + one->key,
                                                         .cap = CLI_CELLS_POOL}));
        if (first)
        {
            mmgr_line_put(args->out, (at != 0u) ? "]," : "");
            EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers_read, .text = one->key, .out = args->out);
            mmgr_line_put(args->out, ":[");
        }
        else
        {
            mmgr_line_ch(args->out, ',');
        }
        mmgr_line_put(args->out, "{\"file\":");
        EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers_read, .text = one->file, .out = args->out);
        mmgr_line_put(args->out, ",\"part\":");
        if (one->has_part)
        {
            EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers_read, .text = one->part, .out = args->out);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_put(args->out, ",\"text\":");
        EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers_read, .text = one->text, .out = args->out);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_put(args->out, (args->layers_read->froms != 0u) ? "]}" : "}");
    // each kind's settings, the kinds in order and each kind's keys in order
    mmgr_line_put(args->out, ",\"kinds\":{");
    for (unsigned int at = 0u; at < args->layers_read->kinds; at += 1u)
    {
        unsigned int place = at;
        while (place > 0u)
        {
            const CliKindText *const before = &args->layers_read->kind[s_order[place - 1u]];
            const CliKindText *const one = &args->layers_read->kind[at];
            const int order =
                EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->layers_read->text + before->kind,
                           .other_pointer = args->layers_read->text + one->kind);
            if ((order < 0) || ((order == 0) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                                            .one = args->layers_read->text + before->key,
                                                            .other_pointer = args->layers_read->text + one->key) <= 0)))
            {
                break;
            }
            s_order[place] = s_order[place - 1u];
            place -= 1u;
        }
        s_order[place] = at;
    }
    for (unsigned int at = 0u; at < args->layers_read->kinds; at += 1u)
    {
        const CliKindText *const one = &args->layers_read->kind[s_order[at]];
        const int first =
            (at == 0u) || (!cellul.eq(&(CatenaFinitaCfg){.src = args->layers_read->text +
                                                                args->layers_read->kind[s_order[at - 1u]].kind,
                                                         .other = args->layers_read->text + one->kind,
                                                         .cap = CLI_CELLS_POOL}));
        if (first)
        {
            mmgr_line_put(args->out, (at != 0u) ? "}," : "");
            EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers_read, .text = one->kind, .out = args->out);
            mmgr_line_put(args->out, ":{");
        }
        else
        {
            mmgr_line_ch(args->out, ',');
        }
        EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers_read, .text = one->key, .out = args->out);
        mmgr_line_ch(args->out, ':');
        EMBED_CALL(cli_quote, SettingsWrittenCtx, .layers = args->layers_read, .text = one->text, .out = args->out);
    }
    mmgr_line_put(args->out, (args->layers_read->kinds != 0u) ? "}}" : "}");
    mmgr_line_put(args->out, ",\"files\":");
    EMBED_CALL(cli_list_write, SettingsWrittenCtx, .layers = args->layers_read, .texts_u32s = args->layers_read->file,
               .count = args->layers_read->files, .out = args->out);
    mmgr_line_put(args->out, ",\"problems\":");
    EMBED_CALL(cli_list_write, SettingsWrittenCtx, .layers = args->layers_read,
               .texts_u32s = args->layers_read->problem, .count = args->layers_read->problems, .out = args->out);
    mmgr_line_put(args->out, ",\"system\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->layers_read->system,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->layers_read->system, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(args->out, '}');
}

const char *cli_settings_layer(const CliSettingsKeyCfg *args)
{
    const CliLayerText *const texts = args->over ? args->layers_read->over : args->layers_read->under;
    const unsigned int count = args->over ? args->layers_read->overs : args->layers_read->unders;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->layers_read->text + texts[at].key, .other = args->key, .cap = CLI_CELLS_POOL}))
        {
            return args->layers_read->text + texts[at].text;
        }
    }
    return NULL;
}
