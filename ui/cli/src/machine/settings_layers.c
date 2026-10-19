// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_layers.c: the settings in force from the reader's chain and the tree's, each part of a file standing over
// the parts before it, each `${name}` read, and every problem told once
#include "kept.h"
#include "settings_internal.h"

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
    char home[CLI_PATH_LONGEST];
    const char *system;
    unsigned int depth;
    char missing[CLI_SETTINGS_PROBLEMS][CLI_NAME_LONGEST];
    unsigned int missings;
} CliNames;

static CliNames s_names;
static CliPart s_parts[CLI_LAYER_PARTS];
static CliPartValue s_values[CLI_LAYER_RAW];
static CliKindSet s_kinds[CLI_LAYER_PARTS];
static char s_value[CLI_LAYER_VALUE];
static CliLock s_lock = CLI_LOCK_INIT;

unsigned int cli_settings_kept_text(CliLayers *layers, const char *text, unsigned long long length)
{
    if ((layers->used + length + 1ull) > CLI_SETTINGS_TEXT)
    {
        return 0u;
    }
    // a place is below CLI_SETTINGS_TEXT, which an unsigned int holds
    const unsigned int at = (unsigned int)layers->used;
    memor.cpy(&(MemoriaCfg){.dst = layers->text + at, .src = text, .bytes = length});
    layers->text[at + length] = '\0';
    layers->used += length + 1ull;
    return at;
}

void cli_settings_problem(CliLayers *layers, const char *problem)
{
    for (unsigned int at = 0u; at < layers->problems; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = layers->text + layers->problem[at], .other = problem, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (layers->problems < (CLI_SETTINGS_PROBLEMS * 2u))
    {
        layers->problem[layers->problems] = cli_settings_kept_text(
            layers, problem, cellul.len(&(CatenaFinitaCfg){.src = problem, .cap = CLI_CELLS_POOL}));
        layers->problems += 1u;
    }
}

static void cli_value_written(const CliChain *chain, unsigned int value, CliLine *out);

// what `${name}` reads into `out`: 1, or 0 where nothing gives it, the name kept among those missing
static int cli_look(void *context, const char *name, unsigned long long length, CliLine *out)
{
    CliNames *const names = (CliNames *)context;
    int found = 0;
    char variable[CLI_NAME_LONGEST];
    char value[CLI_PATH_LONGEST];
    if ((length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "home", .bytes = 4u}) == 0))
    {
        cli_line_text(out, names->home);
        found = names->home[0] != '\0';
    }
    else if ((length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "tree", .bytes = 4u}) == 0))
    {
        cli_line_text(out, names->root);
        found = names->root[0] != '\0';
    }
    else if ((length == 6ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "system", .bytes = 6u}) == 0))
    {
        cli_line_text(out, names->system);
        found = 1;
    }
    else if ((length > 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "env:", .bytes = 4u}) == 0))
    {
        CliLine line = {variable, sizeof(variable), 0ull};
        cli_line_counted(&line, name + 4, length - 4ull);
        cli_line_closed(&line);
        found = query_environment_said(variable, out);
    }
    else if ((length > 8ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "setting:", .bytes = 8u}) == 0) &&
             (names->depth < CLI_SETTINGS_NAMES_DEEP))
    {
        for (unsigned int at = names->raws; !found && (at > 0u); at -= 1u)
        {
            const CliRaw *const raw = &names->raw[at - 1u];
            if ((cellul.len(&(CatenaFinitaCfg){.src = raw->key, .cap = CLI_CELLS_POOL}) == (length - 8ull)) &&
                (memor.cmp(&(MemoriaCfg){.src = raw->key, .other = name + 8, .bytes = length - 8ull}) == 0))
            {
                names->depth += 1u;
                cli_value_written(raw->chain, raw->value, out);
                names->depth -= 1u;
                found = 1;
            }
        }
    }
    if (!found && (names->missings < CLI_SETTINGS_PROBLEMS))
    {
        CliLine line = {names->missing[names->missings], CLI_NAME_LONGEST, 0ull};
        cli_line_counted(&line, name, length);
        cli_line_closed(&line);
        names->missings += 1u;
    }
    return found;
}

// a value with each `${name}` in its texts read, as the page reads it back: a string's text, else compact JSON
static void cli_json_expanded(const CliChain *chain, unsigned int value, CliLine *out);

static void cli_value_written(const CliChain *chain, unsigned int value, CliLine *out)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(&chain->json, value, &length);
    if (text != NULL)
    {
        cli_settings_expand(text, length, cli_look, &s_names, out);
        return;
    }
    cli_json_expanded(chain, value, out);
}

static void cli_json_expanded(const CliChain *chain, unsigned int value, CliLine *out)
{
    const CliJsonNode *const node = &chain->json.node[value];
    if (node->kind == CFG_JSON_STRING)
    {
        static char s_text[CLI_SETTINGS_NAMES_DEEP + 1u][CLI_LAYER_VALUE];
        char *const room = s_text[s_names.depth];
        CliLine line = {room, CLI_LAYER_VALUE, 0ull};
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(&chain->json, value, &length);
        cli_settings_expand(text, length, cli_look, &s_names, &line);
        cli_json_quoted(out, room, cli_line_fits(&line) ? line.at : 0ull);
        return;
    }
    if ((node->kind != CFG_JSON_ARRAY) && (node->kind != CFG_JSON_OBJECT))
    {
        cli_json_write(&chain->json, value, out, 0u, 0);
        return;
    }
    const int object = node->kind == CFG_JSON_OBJECT;
    cli_line_character(out, object ? '{' : '[');
    for (unsigned int child = node->first; child != CLI_JSON_NONE; child = chain->json.node[child].next)
    {
        cli_line_text(out, (child != node->first) ? "," : "");
        if (object)
        {
            const char *const key = chain->json.text + chain->json.node[child].key;
            cli_json_quoted(out, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            cli_line_character(out, ':');
        }
        cli_json_expanded(chain, child, out);
    }
    cli_line_character(out, object ? '}' : ']');
}

// each name `missing` that the setting `key` of `file` names and nothing gives, said as a problem
static void cli_said(CliLayers *layers, const char *file, const char *key)
{
    for (unsigned int at = 0u; at < s_names.missings; at += 1u)
    {
        char problem[CLI_PATH_LONGEST * 2u];
        CliLine line = {problem, sizeof(problem), 0ull};
        cli_line_text(&line, file);
        cli_line_text(&line, ": ");
        cli_line_text(&line, key);
        cli_line_text(&line, " names ${");
        cli_line_text(&line, s_names.missing[at]);
        cli_line_text(&line, "}, which nothing gives");
        cli_line_closed(&line);
        cli_settings_problem(layers, problem);
    }
    s_names.missings = 0u;
}

// a text put among the layers' over or under, in the place of one it had for the same key
static void cli_text_set(CliLayers *layers, CliLayerText *texts, unsigned int *count, const char *key,
                         unsigned int text)
{
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = layers->text + texts[at].key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            texts[at].text = text;
            return;
        }
    }
    if (*count < CLI_LAYERS_KEYS)
    {
        texts[*count] = (CliLayerText){
            cli_settings_kept_text(layers, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL})),
            text};
        *count += 1u;
    }
}

// whether `from` holds a part that sets `key` already
static int cli_set_before(const CliLayers *layers, const char *key)
{
    for (unsigned int at = 0u; at < layers->froms; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = layers->text + layers->from[at].key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// a part that sets `key` put after `from`
static void cli_source_put(CliLayers *layers, const char *key, const char *file, const char *part, unsigned int text)
{
    if (layers->froms < CLI_LAYERS_SOURCES)
    {
        layers->from[layers->froms] = (CliLayerSource){
            cli_settings_kept_text(layers, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL})),
            cli_settings_kept_text(layers, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL})),
            (part != NULL) ? cli_settings_kept_text(layers, part,
                                                    cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL}))
                           : 0u,
            part != NULL, text};
        layers->froms += 1u;
    }
}

// a value read with each `${name}` in it read, kept in the layers' texts, each name nothing gives said as a problem of
// the file and the key: its place
static unsigned int cli_read_kept(CliLayers *layers, const CliChain *chain, unsigned int value, const char *file,
                                  const char *key)
{
    CliLine line = {s_value, sizeof(s_value), 0ull};
    s_names.depth = 0u;
    cli_value_written(chain, value, &line);
    cli_said(layers, file, key);
    return cli_settings_kept_text(layers, s_value, cli_line_fits(&line) ? line.at : 0ull);
}

void cli_settings_layers(const CliChain *reader, const CliChain *tree, const char *root, CliLayers *layers)
{
    // every count and every key's place made 0, the texts begun anew
    memor.set(&(MemoriaCfg){.dst = layers, .val = 0, .bytes = offsetof(CliLayers, text)});
    layers->text[0] = '\0';
    layers->used = 1ull;
    layers->system = cli_settings_system();
    if (reader->files != 0u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = layers->own_file, .bytes = CLI_PATH_LONGEST, .src = reader->file[reader->files - 1u]});
        layers->has_own_file = 1;
    }
    for (unsigned int at = 0u; at < reader->problems; at += 1u)
    {
        cli_settings_problem(layers, reader->problem[at]);
    }
    for (unsigned int at = 0u; at < tree->problems; at += 1u)
    {
        cli_settings_problem(layers, tree->problem[at]);
    }
    cli_lock_take(&s_lock);
    cli_settings_parts_kept(s_parts, s_values, s_kinds);
    unsigned int parts = 0u;
    unsigned int values = 0u;
    unsigned int kinds = 0u;
    const unsigned int last = (reader->files != 0u) ? (reader->files - 1u) : 0u;
    for (unsigned int at = 0u; at < (reader->files + tree->files); at += 1u)
    {
        const int of_tree = at >= reader->files;
        const CliChain *const chain = of_tree ? tree : reader;
        const unsigned int file = of_tree ? (at - reader->files) : at;
        if (layers->files < (CLI_SETTINGS_DEEPEST * 2u))
        {
            layers->file[layers->files] =
                cli_settings_kept_text(layers, chain->file[file],
                                       cellul.len(&(CatenaFinitaCfg){.src = chain->file[file], .cap = CLI_CELLS_POOL}));
            layers->files += 1u;
        }
        parts =
            cli_settings_file_parts(layers, chain, file, of_tree, !of_tree && (file == last), parts, &values, &kinds);
    }
    // every setting as the parts set it, the last standing, for `${setting:name}`
    s_names.raws = 0u;
    s_names.root = root;
    s_names.system = layers->system;
    s_names.missings = 0u;
    cli_settings_home(s_names.home);
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
            const unsigned int text = cli_read_kept(layers, part->chain, value->value, file, value->key);
            if (!cli_set_before(layers, value->key))
            {
                // a value of the reader's own that names another stands over the files it takes
                char raw[CLI_LAYER_VALUE];
                CliLine back = {raw, sizeof(raw), 0ull};
                cli_kept_read_back(&part->chain->json, value->value, &back);
                cli_line_closed(&back);
                const int named =
                    cellul.find(&(CatenaFinitaCfg){
                        .src = raw, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL}) != NULL;
                if ((part->rank == CLI_RANK_OVER) || ((part->rank == CLI_RANK_OWN) && named))
                {
                    cli_text_set(layers, layers->over, &layers->overs, value->key, text);
                }
                if ((part->rank == CLI_RANK_OWN) && named && (layers->owns < CLI_LAYERS_KEYS))
                {
                    layers->own[layers->owns] = cli_settings_kept_text(
                        layers, value->key, cellul.len(&(CatenaFinitaCfg){.src = value->key, .cap = CLI_CELLS_POOL}));
                    layers->owns += 1u;
                }
                if (part->rank == CLI_RANK_UNDER)
                {
                    cli_text_set(layers, layers->under, &layers->unders, value->key, text);
                }
            }
            cli_source_put(layers, value->key, file, part->part, text);
        }
    }
    for (unsigned int at = kinds; at > 0u; at -= 1u)
    {
        const CliKindSet *const set = &s_kinds[at - 1u];
        const CliJson *const json = &set->chain->json;
        const char *const file = set->chain->file[set->file];
        const char *kind = NULL;
        unsigned long long kind_length = 0ull;
        cli_settings_kind_of(set->key, &kind, &kind_length);
        for (unsigned int member = json->node[set->object].first; member != CLI_JSON_NONE;
             member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            if ((!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "theme.dark", .cap = CLI_CELLS_POOL})) &&
                (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "theme.light", .cap = CLI_CELLS_POOL})))
            {
                char problem[CLI_PATH_LONGEST * 2u];
                CliLine line = {problem, sizeof(problem), 0ull};
                cli_line_text(&line, file);
                cli_line_text(&line, ": ");
                cli_line_text(&line, set->key);
                cli_line_text(&line, " sets ");
                cli_line_text(&line, key);
                cli_line_text(&line, ", and a kind of file sets its theme alone: theme.dark and theme.light");
                cli_line_closed(&line);
                cli_settings_problem(layers, problem);
                continue;
            }
            const unsigned int text = cli_read_kept(layers, set->chain, member, file, key);
            const unsigned int kind_text = cli_settings_kept_text(layers, kind, kind_length);
            int known = 0;
            for (unsigned int each = 0u; !known && (each < layers->kinds); each += 1u)
            {
                known = (cellul.eq(&(CatenaFinitaCfg){.src = layers->text + layers->kind[each].kind,
                                                      .other = layers->text + kind_text,
                                                      .cap = CLI_CELLS_POOL})) &&
                        (cellul.eq(&(CatenaFinitaCfg){
                            .src = layers->text + layers->kind[each].key, .other = key, .cap = CLI_CELLS_POOL}));
            }
            if (!known && (layers->kinds < CLI_LAYERS_KEYS))
            {
                layers->kind[layers->kinds] =
                    (CliKindText){kind_text,
                                  cli_settings_kept_text(
                                      layers, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL})),
                                  text};
                layers->kinds += 1u;
            }
            char from[CLI_NAME_LONGEST * 2u];
            CliLine named = {from, sizeof(from), 0ull};
            cli_line_text(&named, set->key);
            cli_line_character(&named, ' ');
            cli_line_text(&named, key);
            cli_line_closed(&named);
            cli_source_put(layers, from, file, NULL, text);
        }
    }
    cli_lock_give(&s_lock);
}
