// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_layers.c: the settings in force from the reader's chain and the tree's, each part of a file standing over
// the parts before it, each `${name}` read, and every problem reported once
#include "../command_line/callback_cfg.h"
#include "settings_internal.h"
#include "state.h"

#include <stddef.h>

// A setting as every part sets it, the last standing: its key, and the chain and the node of its value
typedef struct
{
    const char *key;
    const CliChain *chain;
    unsigned int value;
} CliRaw;

// What a `${name}` reads: the settings in force by key, the tree's folder, the reader's home folder and the system,
// and the names nothing gives
typedef struct
{
    CliRaw raw[CLI_LAYER_RAW];
    unsigned int raws;
    const char *root;
    char home[CLI_PATH_MAX_LEN];
    const char *system;
    unsigned int depth;
    char missing[CLI_SETTINGS_PROBLEMS][CLI_NAME_MAX_LEN];
    unsigned int missings;
} CliNames;

static CliNames s_names;
static CliPart s_parts[CLI_LAYER_PARTS];
static CliPartValue s_values[CLI_LAYER_RAW];
static CliKindSet s_kinds[CLI_LAYER_PARTS];
static char s_value[CLI_LAYER_VALUE];

// the bytes the layers' texts are first taken with

void cli_settings_layers_free(const CliSettingsRootCfg *args)
{
    g_cli_cells->persistent_buf_release(args->layers->text, &g_cli_settings_cells);
    args->layers->text = NULL;
    args->layers->used = 0ull;
    args->layers->room = 0ull;
}

unsigned int cli_settings_text_store(const CliSettingsTextLengthCtx *args)
{
    const char *text = args->text;
    const unsigned long long most = args->layers->used + args->length + 1ull;
    if (most > CLI_SETTINGS_TEXT)
    {
        return 0u;
    }
    if (args->layers->room == 0ull)
    {
        args->layers->text = g_cli_cells->persistent_buf_alloc(CLI_SETTINGS_TEXT, &g_cli_settings_cells);
        if (args->layers->text == NULL)
        {
            return 0u;
        }
        args->layers->room = CLI_SETTINGS_TEXT;
    }
    // a place is below CLI_SETTINGS_TEXT, which an unsigned int stores
    const unsigned int at = (unsigned int)args->layers->used;
    memor.cpy(&(MemoriaCfg){.dst = args->layers->text + at, .src = text, .bytes = args->length});
    args->layers->text[at + args->length] = '\0';
    args->layers->used += args->length + 1ull;
    return at;
}

void cli_settings_problem(const CliSettingsLayersProblemCtx *args)
{
    for (unsigned int at = 0u; at < args->layers->problems; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->layers->text + args->layers->problem[at], .other = args->problem, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (args->layers->problems < (CLI_SETTINGS_PROBLEMS * 2u))
    {
        args->layers->problem[args->layers->problems] =
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = args->layers,
                       .text = args->problem,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->problem, .cap = CLI_CELLS_POOL}));
        args->layers->problems += 1u;
    }
}

typedef struct
{
    const CliChain *chain;
    unsigned int value;
    VerbaLinea *out;
    CliLayers *layers;
    const char *file;
    const char *key;
    CliLayerText *texts;
    unsigned int *count;
    unsigned int text;
    const CliLayers *layers_read;
    const char *part;
} SettingsLayersCtx;
EMBED_INLINE void cli_value_write(const SettingsLayersCtx *args);

// what `${name}` reads into `out`: 1, or 0 where nothing gives it, the name recorded among those missing
static int cli_look(const CliEachNameLengthOutCfg *args)
{
    const char *name = args->name;
    VerbaLinea *out = args->out;
    CliNames *const names = (CliNames *)args->context;
    int found = 0;
    char variable[CLI_NAME_MAX_LEN];
    char value[CLI_PATH_MAX_LEN];
    if ((args->length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "home", .bytes = 4u}) == 0))
    {
        mmgr_line_put(out, names->home);
        found = names->home[0] != '\0';
    }
    else if ((args->length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "tree", .bytes = 4u}) == 0))
    {
        mmgr_line_put(out, names->root);
        found = names->root[0] != '\0';
    }
    else if ((args->length == 6ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "system", .bytes = 6u}) == 0))
    {
        mmgr_line_put(out, names->system);
        found = 1;
    }
    else if ((args->length > 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "env:", .bytes = 4u}) == 0))
    {
        VerbaLinea line = {variable, sizeof(variable), 0ull};
        mmgr_line_put_n(&line, name + 4, args->length - 4ull);
        mmgr_line_finish(&line);
        found = EMBED_CALL(query_name_out_internal.environment_write, QueryNameOutCtx, .name = variable, .out = out);
    }
    else if ((args->length > 8ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "setting:", .bytes = 8u}) == 0) &&
             (names->depth < CLI_SETTINGS_NAMES_DEEP))
    {
        for (unsigned int at = names->raws; !found && (at > 0u); at -= 1u)
        {
            const CliRaw *const raw = &names->raw[at - 1u];
            if ((cellul.len(&(CatenaFinitaCfg){.src = raw->key, .cap = CLI_CELLS_POOL}) == (args->length - 8ull)) &&
                (memor.cmp(&(MemoriaCfg){.src = raw->key, .other = name + 8, .bytes = args->length - 8ull}) == 0))
            {
                names->depth += 1u;
                EMBED_CALL(cli_value_write, SettingsLayersCtx, .chain = raw->chain, .value = raw->value, .out = out);
                names->depth -= 1u;
                found = 1;
            }
        }
    }
    if (!found && (names->missings < CLI_SETTINGS_PROBLEMS))
    {
        VerbaLinea line = {names->missing[names->missings], CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put_n(&line, name, args->length);
        mmgr_line_finish(&line);
        names->missings += 1u;
    }
    return found;
}

// a value with each `${name}` in its texts read, as the page reads it back: a string's text, else compact JSON
static void cli_json_expand(const SettingsLayersCtx *args);

EMBED_INLINE void cli_value_write(const SettingsLayersCtx *args)
{
    unsigned long long length = 0ull;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &args->chain->json,
                   .node = args->value, .length_pointer = &length);
    if (text != NULL)
    {
        EMBED_CALL(cli_settings_text_internal.expand, CliSettingsTextCtx, .text = text, .length = length,
                   .look = cli_look, .context = &s_names, .out_verba_linea_pointer = args->out);
        return;
    }
    cli_json_expand(args);
}

static void cli_json_expand(const SettingsLayersCtx *args)
{
    VerbaLinea *out = args->out;
    const CliJsonNode *const node = &args->chain->json.node[args->value];
    if (node->kind == CFG_JSON_STRING)
    {
        static char s_text[CLI_SETTINGS_NAMES_DEEP + 1u][CLI_LAYER_VALUE];
        char *const room = s_text[s_names.depth];
        VerbaLinea line = {room, CLI_LAYER_VALUE, 0ull};
        unsigned long long length = 0ull;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &args->chain->json,
                       .node = args->value, .length_pointer = &length);
        EMBED_CALL(cli_settings_text_internal.expand, CliSettingsTextCtx, .text = text, .length = length,
                   .look = cli_look, .context = &s_names, .out_verba_linea_pointer = &line);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = room,
                   .length = mmgr_line_ok(&line) ? line.at : 0ull);
        return;
    }
    if ((node->kind != CFG_JSON_ARRAY) && (node->kind != CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &args->chain->json,
                   .node = args->value, .out = out, .indent = 0u, .sorted = 0);
        return;
    }
    const int object = node->kind == CFG_JSON_OBJECT;
    mmgr_line_ch(out, object ? '{' : '[');
    for (unsigned int child = node->first; child != CLI_JSON_NONE; child = args->chain->json.node[child].next)
    {
        mmgr_line_put(out, (child != node->first) ? "," : "");
        if (object)
        {
            const char *const key = args->chain->json.text + args->chain->json.node[child].key;
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            mmgr_line_ch(out, ':');
        }
        EMBED_CALL(cli_json_expand, SettingsLayersCtx, .chain = args->chain, .value = child, .out = out);
    }
    mmgr_line_ch(out, object ? '}' : ']');
}

// each name `missing` that the setting `key` of `file` names and nothing gives, reported as a problem
EMBED_INLINE void cli_missing_report(const SettingsLayersCtx *args)
{
    CliLayers *layers = args->layers;
    for (unsigned int at = 0u; at < s_names.missings; at += 1u)
    {
        char problem[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea line = {problem, sizeof(problem), 0ull};
        mmgr_line_put(&line, args->file);
        mmgr_line_put(&line, ": ");
        mmgr_line_put(&line, args->key);
        mmgr_line_put(&line, " names ${");
        mmgr_line_put(&line, s_names.missing[at]);
        mmgr_line_put(&line, "}, which nothing gives");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx, .layers = layers,
                   .problem = problem);
    }
    s_names.missings = 0u;
}

// a text put among the layers' over or under, in the place of one it had for the same key
EMBED_INLINE void cli_text_set(const SettingsLayersCtx *args)
{
    CliLayers *layers = args->layers;
    unsigned int *count = args->count;
    unsigned int text = args->text;
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = layers->text + args->texts[at].key, .other = args->key, .cap = CLI_CELLS_POOL}))
        {
            args->texts[at].text = text;
            return;
        }
    }
    if (*count < CLI_LAYERS_KEYS)
    {
        args->texts[*count] = (CliLayerText){
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                       .text = args->key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL})),
            text};
        *count += 1u;
    }
}

// whether `from` contains a part that sets `key` already
EMBED_INLINE int cli_set_before(const SettingsLayersCtx *args)
{
    for (unsigned int at = 0u; at < args->layers_read->froms; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->layers_read->text + args->layers_read->from[at].key,
                                         .other = args->key,
                                         .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// a part that sets `key` put after `from`
EMBED_INLINE void cli_source_put(const SettingsLayersCtx *args)
{
    CliLayers *layers = args->layers;
    unsigned int text = args->text;
    if (layers->froms < CLI_LAYERS_SOURCES)
    {
        layers->from[layers->froms] = (CliLayerSource){
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                       .text = args->key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL})),
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                       .text = args->file,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL})),
            (args->part != NULL)
                ? EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                             .text = args->part,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = args->part, .cap = CLI_CELLS_POOL}))
                : 0u,
            args->part != NULL, text};
        layers->froms += 1u;
    }
}

// a value read with each `${name}` in it read, stored in the layers' texts, each name nothing gives reported as a
// problem of the file and the key: its place
EMBED_INLINE unsigned int cli_read_store(const SettingsLayersCtx *args)
{
    CliLayers *layers = args->layers;
    VerbaLinea line = {s_value, sizeof(s_value), 0ull};
    s_names.depth = 0u;
    EMBED_CALL(cli_value_write, SettingsLayersCtx, .chain = args->chain, .value = args->value, .out = &line);
    EMBED_CALL(cli_missing_report, SettingsLayersCtx, .layers = layers, .file = args->file, .key = args->key);
    return EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                      .text = s_value, .length = mmgr_line_ok(&line) ? line.at : 0ull);
}

void cli_settings_layers(const CliSettingsRootCfg *args)
{
    const char *root = args->root;
    // every count and every key's place made 0, the texts begun anew with the empty text at their first place
    memor.set(&(MemoriaCfg){.dst = args->layers, .val = 0, .bytes = sizeof(*args->layers)});
    (void)EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = args->layers,
                     .text = "", .length = 0ull);
    args->layers->system = cli_settings_system();
    if (args->reader->files != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->layers->own_file,
                                .bytes = CLI_PATH_MAX_LEN,
                                .src = args->reader->file[args->reader->files - 1u]});
        args->layers->has_own_file = 1;
    }
    for (unsigned int at = 0u; at < args->reader->problems; at += 1u)
    {
        EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx, .layers = args->layers,
                   .problem = args->reader->problem[at]);
    }
    for (unsigned int at = 0u; at < args->tree->problems; at += 1u)
    {
        EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx, .layers = args->layers,
                   .problem = args->tree->problem[at]);
    }
    EMBED_CALL(cli_settings_parts_internal.parts_store, CliSettingsPartsCtx, .parts = s_parts, .values = s_values,
               .kinds = s_kinds);
    unsigned int parts = 0u;
    unsigned int values = 0u;
    unsigned int kinds = 0u;
    const unsigned int last = (args->reader->files != 0u) ? (args->reader->files - 1u) : 0u;
    for (unsigned int at = 0u; at < (args->reader->files + args->tree->files); at += 1u)
    {
        const int of_tree = at >= args->reader->files;
        const CliChain *const chain = of_tree ? args->tree : args->reader;
        const unsigned int file = of_tree ? (at - args->reader->files) : at;
        if (args->layers->files < (CLI_SETTINGS_DEEPEST * 2u))
        {
            args->layers->file[args->layers->files] =
                EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx,
                           .layers = args->layers, .text = chain->file[file],
                           .length = cellul.len(&(CatenaFinitaCfg){.src = chain->file[file], .cap = CLI_CELLS_POOL}));
            args->layers->files += 1u;
        }
        parts = EMBED_CALL(cli_settings_layers_internal.file_parts, CliSettingsLayersCtx, .layers = args->layers,
                           .chain = chain, .file = file, .of_tree = of_tree, .own = !of_tree && (file == last),
                           .parts_u32 = parts, .values_u32s = &values, .kinds_u32s = &kinds);
    }
    // every setting as the parts set it, the last standing, for `${setting:name}`
    s_names.raws = 0u;
    s_names.root = root;
    s_names.system = args->layers->system;
    s_names.missings = 0u;
    EMBED_CALL(cli_settings_internal.home, CliSettingsCtx, .out = s_names.home);
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        for (unsigned int each = 0u; (each < s_parts[at].count) && (s_names.raws < CLI_LAYER_RAW); each += 1u)
        {
            const CliPartValue *const value = &s_values[s_parts[at].first + each];
            s_names.raw[s_names.raws] = (CliRaw){value->key, s_parts[at].chain, value->value};
            s_names.raws += 1u;
        }
    }
    for (unsigned int at = parts; at > 0u; at -= 1u)
    {
        const CliPart *const part = &s_parts[at - 1u];
        const char *const file = part->chain->file[part->file];
        for (unsigned int each = 0u; each < part->count; each += 1u)
        {
            const CliPartValue *const value = &s_values[part->first + each];
            const unsigned int text =
                EMBED_CALL(cli_read_store, SettingsLayersCtx, .layers = args->layers, .chain = part->chain,
                           .value = value->value, .file = file, .key = value->key);
            if (!EMBED_CALL(cli_set_before, SettingsLayersCtx, .layers_read = args->layers, .key = value->key))
            {
                // a value of the reader's own that names another stands over the files it takes
                char raw[CLI_LAYER_VALUE];
                VerbaLinea back = {raw, sizeof(raw), 0ull};
                EMBED_CALL(cli_state_json.read_back, CliStateJsonCfg, .json = &part->chain->json, .value = value->value,
                           .out = &back);
                mmgr_line_finish(&back);
                const int named =
                    cellul.find(&(CatenaFinitaCfg){
                        .src = raw, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL}) != NULL;
                if ((part->rank == CLI_RANK_OVER) || ((part->rank == CLI_RANK_OWN) && named))
                {
                    EMBED_CALL(cli_text_set, SettingsLayersCtx, .layers = args->layers, .texts = args->layers->over,
                               .count = &args->layers->overs, .key = value->key, .text = text);
                }
                if ((part->rank == CLI_RANK_OWN) && named && (args->layers->owns < CLI_LAYERS_KEYS))
                {
                    args->layers->own[args->layers->owns] =
                        EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx,
                                   .layers = args->layers, .text = value->key,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = value->key, .cap = CLI_CELLS_POOL}));
                    args->layers->owns += 1u;
                }
                if (part->rank == CLI_RANK_UNDER)
                {
                    EMBED_CALL(cli_text_set, SettingsLayersCtx, .layers = args->layers, .texts = args->layers->under,
                               .count = &args->layers->unders, .key = value->key, .text = text);
                }
            }
            EMBED_CALL(cli_source_put, SettingsLayersCtx, .layers = args->layers, .key = value->key, .file = file,
                       .part = part->part, .text = text);
        }
    }
    for (unsigned int at = kinds; at > 0u; at -= 1u)
    {
        const CliKindSet *const set = &s_kinds[at - 1u];
        const CliJson *const json = &set->chain->json;
        const char *const file = set->chain->file[set->file];
        const char *kind = NULL;
        unsigned long long kind_length = 0ull;
        EMBED_CALL(cli_settings_key_internal.kind_get, CliSettingsKeyCtx, .key = set->key, .kind = &kind,
                   .length_pointer = &kind_length);
        for (unsigned int member = json->node[set->object].first; member != CLI_JSON_NONE;
             member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            if ((!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "theme.dark", .cap = CLI_CELLS_POOL})) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "theme.light", .cap = CLI_CELLS_POOL})))
            {
                char problem[CLI_PATH_MAX_LEN * 2u];
                VerbaLinea line = {problem, sizeof(problem), 0ull};
                mmgr_line_put(&line, file);
                mmgr_line_put(&line, ": ");
                mmgr_line_put(&line, set->key);
                mmgr_line_put(&line, " sets ");
                mmgr_line_put(&line, key);
                mmgr_line_put(&line, ", and a kind of file sets its theme alone: theme.dark and theme.light");
                mmgr_line_finish(&line);
                EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx,
                           .layers = args->layers, .problem = problem);
                continue;
            }
            const unsigned int text = EMBED_CALL(cli_read_store, SettingsLayersCtx, .layers = args->layers,
                                                 .chain = set->chain, .value = member, .file = file, .key = key);
            const unsigned int kind_text =
                EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx,
                           .layers = args->layers, .text = kind, .length = kind_length);
            int known = 0;
            for (unsigned int each = 0u; !known && (each < args->layers->kinds); each += 1u)
            {
                known = (cellul.eq(&(CatenaFinitaCfg){.src = args->layers->text + args->layers->kind[each].kind,
                                                      .other = args->layers->text + kind_text,
                                                      .cap = CLI_CELLS_POOL})) &&
                        (cellul.eq(&(CatenaFinitaCfg){.src = args->layers->text + args->layers->kind[each].key,
                                                      .other = key,
                                                      .cap = CLI_CELLS_POOL}));
            }
            if (!known && (args->layers->kinds < CLI_LAYERS_KEYS))
            {
                args->layers->kind[args->layers->kinds] = (CliKindText){
                    kind_text,
                    EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx,
                               .layers = args->layers, .text = key,
                               .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL})),
                    text};
                args->layers->kinds += 1u;
            }
            char from[CLI_NAME_MAX_LEN * 2u];
            VerbaLinea named = {from, sizeof(from), 0ull};
            mmgr_line_put(&named, set->key);
            mmgr_line_ch(&named, ' ');
            mmgr_line_put(&named, key);
            mmgr_line_finish(&named);
            EMBED_CALL(cli_source_put, SettingsLayersCtx, .layers = args->layers, .key = from, .file = file,
                       .part = NULL, .text = text);
        }
    }
}
