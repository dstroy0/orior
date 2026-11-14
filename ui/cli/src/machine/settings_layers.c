// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_layers.c: the settings in force from the reader's chain and the tree's, each part of a file located over
// the parts before it, each `${name}` read, and every problem reported once
#include "../command_line/callback_cfg.h"
#include "settings_internal.h"
#include "state.h"

// A setting as every part sets it, the last located: its key, and the chain and the node of its value
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
static CliTypeSet s_types[CLI_LAYER_PARTS];
static char s_value[CLI_LAYER_VALUE];

// the bytes the layers' texts are first taken with

unsigned int cli_settings_text_store(const CliSettingsTextLengthCtx *args)
{
    const char *text = args->text;
    const unsigned long long max = args->layers->used + args->length + 1ULL;
    if (max > CLI_SETTINGS_TEXT)
    {
        return 0u;
    }
    // the texts in a grown line of the settings owner, taken small and grown to what they hold up to CLI_SETTINGS_TEXT
    if (args->layers->capacity == 0ULL)
    {
        cli_grown_begin(&args->layers->grown, CLI_SETTINGS_TEXT, g_cli_settings_owner);
        if (args->layers->grown.cell == NULL)
        {
            return 0u;
        }
        args->layers->capacity = CLI_SETTINGS_TEXT;
    }
    // a place is below CLI_SETTINGS_TEXT, which an unsigned int stores
    const unsigned int at = (unsigned int)args->layers->used;
    mmgr_line_put_n(&args->layers->grown.line, text, args->length);
    mmgr_line_ch(&args->layers->grown.line, '\0');
    if (!mmgr_line_ok(&args->layers->grown.line))
    {
        return 0u;
    }
    args->layers->text = args->layers->grown.cell;
    args->layers->used += args->length + 1ULL;
    return at;
}

void cli_settings_problem(const CliSettingsLayersProblemCtx *args)
{
    for (unsigned int at = 0u; at < args->layers->problems; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->layers->text + args->layers->problem[at], .other = args->problem, .cap = CLI_POOL_BYTES}))
        {
            return;
        }
    }
    if (args->layers->problems < (CLI_SETTINGS_PROBLEMS * 2u))
    {
        args->layers->problem[args->layers->problems] =
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = args->layers,
                       .text = args->problem,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->problem, .cap = CLI_POOL_BYTES}));
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
    const CliChain *reader;
    const CliChain *tree;
    unsigned int parts;
    const CliPart *part_at;
    const CliTypeSet *set;
    const char *type;
    unsigned long long type_length;
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
    if ((args->length == 4ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "home", .bytes = 4u}) == 0))
    {
        mmgr_line_put(out, names->home);
        found = names->home[0] != '\0';
    }
    else if ((args->length == 4ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "tree", .bytes = 4u}) == 0))
    {
        mmgr_line_put(out, names->root);
        found = names->root[0] != '\0';
    }
    else if ((args->length == 6ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "system", .bytes = 6u}) == 0))
    {
        mmgr_line_put(out, names->system);
        found = 1;
    }
    else if ((args->length > 4ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "env:", .bytes = 4u}) == 0))
    {
        VerbaLinea line = {variable, sizeof(variable), 0ULL};
        mmgr_line_put_n(&line, name + 4, args->length - 4ULL);
        mmgr_line_finish(&line);
        found = EMBED_CALL(query_name_out_internal.environment_write, QueryNameOutCtx, .name = variable, .out = out);
    }
    else if ((args->length > 8ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "setting:", .bytes = 8u}) == 0) &&
             (names->depth < CLI_SETTINGS_NAMES_DEEP))
    {
        unsigned int at = names->raws;
        while (!found && (at > 0u))
        {
            const CliRaw *const raw = &names->raw[at - 1u];
            if ((cellul.len(&(CatenaFinitaCfg){.src = raw->key, .cap = CLI_POOL_BYTES}) == (args->length - 8ULL)) &&
                (memor.cmp(&(MemoriaCfg){.src = raw->key, .other = name + 8, .bytes = args->length - 8ULL}) == 0))
            {
                names->depth += 1u;
                EMBED_CALL(cli_value_write, SettingsLayersCtx, .chain = raw->chain, .value = raw->value, .out = out);
                names->depth -= 1u;
                found = 1;
            }
            at -= 1u;
        }
    }
    if (!found && (names->missings < CLI_SETTINGS_PROBLEMS))
    {
        VerbaLinea line = {names->missing[names->missings], CLI_NAME_MAX_LEN, 0ULL};
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
    unsigned long long length = 0ULL;
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
    if (node->type == CFG_JSON_STRING)
    {
        static char s_text[CLI_SETTINGS_NAMES_DEEP + 1u][CLI_LAYER_VALUE];
        char *const capacity = s_text[s_names.depth];
        VerbaLinea line = {capacity, CLI_LAYER_VALUE, 0ULL};
        unsigned long long length = 0ULL;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &args->chain->json,
                       .node = args->value, .length_pointer = &length);
        EMBED_CALL(cli_settings_text_internal.expand, CliSettingsTextCtx, .text = text, .length = length,
                   .look = cli_look, .context = &s_names, .out_verba_linea_pointer = &line);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = capacity,
                   .length = mmgr_line_ok(&line) ? line.at : 0ULL);
        return;
    }
    if ((node->type != CFG_JSON_ARRAY) && (node->type != CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &args->chain->json,
                   .node = args->value, .out = out, .indent = 0u, .sorted = 0);
        return;
    }
    const int object = node->type == CFG_JSON_OBJECT;
    mmgr_line_ch(out, object ? '{' : '[');
    for (unsigned int child = node->first; child != CLI_JSON_NONE; child = args->chain->json.node[child].next)
    {
        mmgr_line_put(out, (child != node->first) ? "," : "");
        if (object)
        {
            const char *const key = args->chain->json.text + args->chain->json.node[child].key;
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}));
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
        VerbaLinea line = {problem, sizeof(problem), 0ULL};
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
                .src = layers->text + args->texts[at].key, .other = args->key, .cap = CLI_POOL_BYTES}))
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
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES})),
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
                                         .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// a part that sets `key` put after `from`
EMBED_INLINE void cli_source_append(const SettingsLayersCtx *args)
{
    CliLayers *layers = args->layers;
    unsigned int text = args->text;
    if (layers->froms < CLI_LAYERS_SOURCES)
    {
        layers->from[layers->froms] = (CliLayerSource){
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                       .text = args->key,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES})),
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                       .text = args->file,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES})),
            (args->part != NULL)
                ? EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                             .text = args->part,
                             .length = cellul.len(&(CatenaFinitaCfg){.src = args->part, .cap = CLI_POOL_BYTES}))
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
    VerbaLinea line = {s_value, sizeof(s_value), 0ULL};
    s_names.depth = 0u;
    EMBED_CALL(cli_value_write, SettingsLayersCtx, .chain = args->chain, .value = args->value, .out = &line);
    EMBED_CALL(cli_missing_report, SettingsLayersCtx, .layers = layers, .file = args->file, .key = args->key);
    return EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                      .text = s_value, .length = mmgr_line_ok(&line) ? line.at : 0ULL);
}

// the layers begun: every count and every key's place 0, the texts begun anew with the empty text at their first
// place, the system named, the last file of the `reader` its own, and the problems the `reader` and the `tree` found
EMBED_INLINE void cli_layers_begun(const SettingsLayersCtx *args)
{
    CliLayers *const layers = args->layers;
    memor.set(&(MemoriaCfg){.dst = layers, .val = 0, .bytes = sizeof(*layers)});
    (void)EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                     .text = "", .length = 0ULL);
    layers->system = cli_settings_system();
    if (args->reader->files != 0u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = layers->own_file, .bytes = CLI_PATH_MAX_LEN, .src = args->reader->file[args->reader->files - 1u]});
        layers->has_own_file = 1;
    }
    for (unsigned int at = 0u; at < args->reader->problems; at += 1u)
    {
        EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx, .layers = layers,
                   .problem = args->reader->problem[at]);
    }
    for (unsigned int at = 0u; at < args->tree->problems; at += 1u)
    {
        EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx, .layers = layers,
                   .problem = args->tree->problem[at]);
    }
}

// every setting the first `parts` of the parts set, each as its part sets it, the last located, for
// `${setting:name}`
EMBED_INLINE void cli_raws_put(const SettingsLayersCtx *args)
{
    for (unsigned int at = 0u; at < args->parts; at += 1u)
    {
        for (unsigned int each = 0u; (each < s_parts[at].count) && (s_names.raws < CLI_LAYER_RAW); each += 1u)
        {
            const CliPartValue *const value = &s_values[s_parts[at].first + each];
            s_names.raw[s_names.raws] = (CliRaw){value->key, s_parts[at].chain, value->value};
            s_names.raws += 1u;
        }
    }
}

// the value `value` of `part_at`, which sets `key` to the stored text `text`, put among the layers' over or under by
// the part's rank: over where it ranks over, or where it is the reader's own and names another value, a key of the
// reader's own then; under where it ranks under
EMBED_INLINE void cli_value_ranked(const SettingsLayersCtx *args)
{
    CliLayers *const layers = args->layers;
    const CliPart *const part = args->part_at;
    // a value of the reader's own that names another is over the files it takes
    char raw[CLI_LAYER_VALUE];
    VerbaLinea back = {raw, sizeof(raw), 0ULL};
    EMBED_CALL(cli_state_json.read_back, CliStateJsonCfg, .json = &part->chain->json, .value = args->value,
               .out = &back);
    mmgr_line_finish(&back);
    const int named = cellul.find(&(CatenaFinitaCfg){
                          .src = raw, .cap = CLI_POOL_BYTES, .other = "${", .other_cap = CLI_POOL_BYTES}) != NULL;
    if ((part->rank == CLI_RANK_OVER) || ((part->rank == CLI_RANK_OWN) && named))
    {
        EMBED_CALL(cli_text_set, SettingsLayersCtx, .layers = layers, .texts = layers->over, .count = &layers->overs,
                   .key = args->key, .text = args->text);
    }
    if ((part->rank == CLI_RANK_OWN) && named && (layers->owner < CLI_LAYERS_KEYS))
    {
        layers->own[layers->owner] = EMBED_CALL(
            cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers, .text = args->key,
            .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES}));
        layers->owner += 1u;
    }
    if (part->rank == CLI_RANK_UNDER)
    {
        EMBED_CALL(cli_text_set, SettingsLayersCtx, .layers = layers, .texts = layers->under, .count = &layers->unders,
                   .key = args->key, .text = args->text);
    }
}

// each value `part_at` sets read and stored, ranked where no part read before it sets its key, and its source put
// after the layers' sources
EMBED_INLINE void cli_part_read(const SettingsLayersCtx *args)
{
    const CliPart *const part = args->part_at;
    const char *const file = part->chain->file[part->file];
    for (unsigned int each = 0u; each < part->count; each += 1u)
    {
        const CliPartValue *const value = &s_values[part->first + each];
        const unsigned int text =
            EMBED_CALL(cli_read_store, SettingsLayersCtx, .layers = args->layers, .chain = part->chain,
                       .value = value->value, .file = file, .key = value->key);
        if (!EMBED_CALL(cli_set_before, SettingsLayersCtx, .layers_read = args->layers, .key = value->key))
        {
            EMBED_CALL(cli_value_ranked, SettingsLayersCtx, .layers = args->layers, .part_at = part,
                       .value = value->value, .key = value->key, .text = text);
        }
        EMBED_CALL(cli_source_append, SettingsLayersCtx, .layers = args->layers, .key = value->key, .file = file,
                   .part = part->part, .text = text);
    }
}

// the member `value` of the settings object a type of file's key sets in `set`, of the type `type`, read: a problem
// where it is not theme.dark or theme.light, else stored, put among the layers' types where the type and the key are
// not there yet, and its source, the set's key and its own, put after the layers' sources
EMBED_INLINE void cli_type_member_read(const SettingsLayersCtx *args)
{
    CliLayers *const layers = args->layers;
    const CliTypeSet *const set = args->set;
    const CliJson *const json = &set->chain->json;
    const char *const file = set->chain->file[set->file];
    const char *const key = json->text + json->node[args->value].key;
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "theme.dark", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "theme.light", .cap = CLI_POOL_BYTES})))
    {
        char problem[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea line = {problem, sizeof(problem), 0ULL};
        mmgr_line_put(&line, file);
        mmgr_line_put(&line, ": ");
        mmgr_line_put(&line, set->key);
        mmgr_line_put(&line, " sets ");
        mmgr_line_put(&line, key);
        mmgr_line_put(&line, ", and a type of file sets its theme alone: theme.dark and theme.light");
        mmgr_line_finish(&line);
        EMBED_CALL(cli_settings_layers_problem_internal.problem, CliSettingsLayersProblemCtx, .layers = layers,
                   .problem = problem);
        return;
    }
    const unsigned int text = EMBED_CALL(cli_read_store, SettingsLayersCtx, .layers = layers, .chain = set->chain,
                                         .value = args->value, .file = file, .key = key);
    const unsigned int type_text = EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx,
                                              .layers = layers, .text = args->type, .length = args->type_length);
    int known = 0;
    for (unsigned int each = 0u; !known && (each < layers->types); each += 1u)
    {
        known = (cellul.eq(&(CatenaFinitaCfg){.src = layers->text + layers->type[each].type,
                                              .other = layers->text + type_text,
                                              .cap = CLI_POOL_BYTES})) &&
                (cellul.eq(&(CatenaFinitaCfg){
                    .src = layers->text + layers->type[each].key, .other = key, .cap = CLI_POOL_BYTES}));
    }
    if (!known && (layers->types < CLI_LAYERS_KEYS))
    {
        layers->type[layers->types] = (CliTypeText){
            type_text,
            EMBED_CALL(cli_settings_text_length_internal.text_store, CliSettingsTextLengthCtx, .layers = layers,
                       .text = key, .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES})),
            text};
        layers->types += 1u;
    }
    char from[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea named = {from, sizeof(from), 0ULL};
    mmgr_line_put(&named, set->key);
    mmgr_line_ch(&named, ' ');
    mmgr_line_put(&named, key);
    mmgr_line_finish(&named);
    EMBED_CALL(cli_source_append, SettingsLayersCtx, .layers = layers, .key = from, .file = file, .part = NULL,
               .text = text);
}

// each member of the settings object a type of file's key sets in `set` read
EMBED_INLINE void cli_type_set_read(const SettingsLayersCtx *args)
{
    const CliTypeSet *const set = args->set;
    const CliJson *const json = &set->chain->json;
    const char *type = NULL;
    unsigned long long type_length = 0ULL;
    EMBED_CALL(cli_settings_key_internal.type_get, CliSettingsKeyCtx, .key = set->key, .type = &type,
               .length_pointer = &type_length);
    for (unsigned int member = json->node[set->object].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        EMBED_CALL(cli_type_member_read, SettingsLayersCtx, .layers = args->layers, .set = set, .value = member,
                   .type = type, .type_length = type_length);
    }
}

void cli_settings_layers(const CliSettingsRootCfg *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_layers_begun, SettingsLayersCtx, .layers = args->layers, .reader = args->reader, .tree = args->tree);
    EMBED_CALL(cli_settings_parts_internal.parts_store, CliSettingsPartsCtx, .parts = s_parts, .values = s_values,
               .types = s_types);
    unsigned int parts = 0u;
    unsigned int values = 0u;
    unsigned int types = 0u;
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
                           .length = cellul.len(&(CatenaFinitaCfg){.src = chain->file[file], .cap = CLI_POOL_BYTES}));
            args->layers->files += 1u;
        }
        parts = EMBED_CALL(cli_settings_layers_internal.file_parts, CliSettingsLayersCtx, .layers = args->layers,
                           .chain = chain, .file = file, .of_tree = of_tree, .own = !of_tree && (file == last),
                           .parts_u32 = parts, .values_u32s = &values, .types_u32s = &types);
    }
    // every setting as the parts set it, the last located, for `${setting:name}`
    s_names.raws = 0u;
    s_names.root = root;
    s_names.system = args->layers->system;
    s_names.missings = 0u;
    EMBED_CALL(cli_settings_internal.home, CliSettingsCtx, .out = s_names.home);
    EMBED_CALL(cli_raws_put, SettingsLayersCtx, .parts = parts);
    for (unsigned int at = parts; at > 0u; at -= 1u)
    {
        EMBED_CALL(cli_part_read, SettingsLayersCtx, .layers = args->layers, .part_at = &s_parts[at - 1u]);
    }
    for (unsigned int at = types; at > 0u; at -= 1u)
    {
        EMBED_CALL(cli_type_set_read, SettingsLayersCtx, .layers = args->layers, .set = &s_types[at - 1u]);
    }
}
