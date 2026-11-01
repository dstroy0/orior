// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bridge.c: Lstar.klq, the .klm maps and the rulesets read line by line into the bridge
#include "bridge.h"

#include "../command_line/cells.h"
#include "../text/text.h"

// the most nodes and bytes of texts the bridge's tree makes of a line of one of its files, and of a file besides its
// lines: a pair under a key not seen yet is the most a line makes, and a verdict the most text, its line's bytes twice
#define CLI_BRIDGE_LINE_NODES 16u
#define CLI_BRIDGE_LINE_TEXT 128u
#define CLI_BRIDGE_FILE_NODES 8u
#define CLI_BRIDGE_FILE_TEXT 64u

// One file of the bridge's folder: its name, and its bytes mapped
typedef struct
{
    char name[CLI_NAME_MAX_LEN];
    const unsigned char *bytes;
    unsigned long long size;
} CliBridgeFile;

// The bridge folder's files listed: those retained, counted where `file` is NULL, and the most retained
typedef struct
{
    CliBridgeFile *file;
    unsigned int count;
    unsigned int most;
} CliBridgeFiles;

static int cli_is_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

// One line that contains something: its number from 0, its first word and the rest, trimmed
typedef struct
{
    unsigned int line;
    const char *word;
    unsigned long long word_length;
    const char *rest;
    unsigned long long rest_length;
} CliBridgeLine;

// the next line from `*at` before `end` that contains something, `*number` counting lines as they pass: 1, or 0 at the
// end
static int cli_next_line(const char **at, const char *end, unsigned int *number, CliBridgeLine *parsed)
{
    while (*at < end)
    {
        const char *from = *at;
        const char *stop = from;
        stop += cli_text_to_any(stop, 0ull, (unsigned long long)(end - stop), "\n");
        *at = (stop < end) ? (stop + 1) : stop;
        const unsigned int line = *number;
        *number += 1u;
        while ((from < stop) && cli_is_space(from[0]))
        {
            from += 1;
        }
        while ((stop > from) && cli_is_space(stop[-1]))
        {
            stop -= 1;
        }
        if ((from == stop) || (from[0] == '#'))
        {
            continue;
        }
        const char *word_end = from;
        while ((word_end < stop) && !cli_is_space(word_end[0]))
        {
            word_end += 1;
        }
        const char *rest = word_end;
        while ((rest < stop) && cli_is_space(rest[0]))
        {
            rest += 1;
        }
        const CliBridgeLine found = {line, from, (unsigned long long)(word_end - from), rest,
                                     (unsigned long long)(stop - rest)};
        *parsed = found;
        return 1;
    }
    return 0;
}

static int cli_word_is(const CliBridgeLine *parsed, const char *word)
{
    return (parsed->word_length == cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = parsed->word, .other = word, .bytes = parsed->word_length}) == 0);
}

// the `index`th word of a text, its start into `word`: its length, 0 where it has none
static unsigned long long cli_nth(const char *text, unsigned long long length, unsigned int index, const char **word)
{
    const char *at = text;
    const char *const end = text + length;
    for (unsigned int each = 0u; at < end; each += 1u)
    {
        while ((at < end) && cli_is_space(at[0]))
        {
            at += 1;
        }
        const char *stop = at;
        while ((stop < end) && !cli_is_space(stop[0]))
        {
            stop += 1;
        }
        if (each == index)
        {
            *word = at;
            return (unsigned long long)(stop - at);
        }
        at = stop;
    }
    *word = end;
    return 0ull;
}

static unsigned int cli_number(CliJson *json, unsigned long long value)
{
    char digits[24];
    VerbaLinea line = {digits, sizeof(digits), 0ull};
    mmgr_line_decimal(&line, value, 1u);
    mmgr_line_finish(&line);
    return cli_json_number_made(json, digits);
}

// A note: a line's first word, the rest, and its number
static unsigned int cli_note(CliJson *json, const char *word, unsigned long long word_length, const char *rest,
                             unsigned long long rest_length, unsigned int line)
{
    const unsigned int note = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, note, "word", cli_json_string_made(json, word, word_length));
    cli_json_set(json, note, "rest", cli_json_string_made(json, rest, rest_length));
    cli_json_set(json, note, "line", cli_number(json, line));
    return note;
}

// the member of `object` named by `length` bytes of `name`, made by `made` where it has none
static unsigned int cli_entry(CliJson *json, unsigned int object, const char *name, unsigned long long length,
                              unsigned int (*made)(CliJson *json, unsigned int line), unsigned int line)
{
    char key[CLI_PATH_MAX_LEN];
    VerbaLinea named = {key, sizeof(key), 0ull};
    mmgr_line_put_n(&named, name, length);
    mmgr_line_finish(&named);
    const unsigned int had = cli_json_member(json, object, key);
    if (had != CLI_JSON_NONE)
    {
        return had;
    }
    const unsigned int entry = made(json, line);
    cli_json_set(json, object, key, entry);
    return entry;
}

static unsigned int cli_key_made(CliJson *json, unsigned int line)
{
    const unsigned int key = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, key, "line", cli_number(json, line));
    cli_json_set(json, key, "pairs", cli_json_made(json, CFG_JSON_ARRAY));
    return key;
}

// a key with no pairs, line 0, where a pair names one not seen yet
static unsigned int cli_key_default(CliJson *json, unsigned int line)
{
    (void)line;
    return cli_key_made(json, 0u);
}

unsigned int cli_bridge_klq(CliJson *json, const char *path, const char *text, unsigned long long size)
{
    const unsigned int keys = cli_json_made(json, CFG_JSON_OBJECT);
    const unsigned int identities = cli_json_made(json, CFG_JSON_ARRAY);
    // the key the lines stand under, as its name in the text
    const char *key = NULL;
    unsigned long long key_length = 0ull;
    // where the lines under a pair go: the pair's own place in its key, or an identity block
    int under_pair = 0;
    int under_identity = 0;
    const char *at = text;
    unsigned int number = 0u;
    CliBridgeLine parsed;
    while (cli_next_line(&at, text + size, &number, &parsed))
    {
        if (cli_word_is(&parsed, "klq"))
        {
            continue;
        }
        if (cli_word_is(&parsed, "key"))
        {
            const char *name = NULL;
            const unsigned long long length = cli_nth(parsed.rest, parsed.rest_length, 0u, &name);
            const unsigned int entry = cli_entry(json, keys, name, length, cli_key_made, parsed.line);
            cli_json_set(json, entry, "line", cli_number(json, parsed.line));
            key = name;
            key_length = length;
            under_pair = 0;
            under_identity = 0;
            continue;
        }
        if (cli_word_is(&parsed, "pair"))
        {
            const char *first = NULL;
            const char *second = NULL;
            const unsigned long long first_length = cli_nth(parsed.rest, parsed.rest_length, 0u, &first);
            const unsigned long long second_length = cli_nth(parsed.rest, parsed.rest_length, 1u, &second);
            const unsigned int owner = (key != NULL) ? cli_entry(json, keys, key, key_length, cli_key_default, 0u)
                                                     : cli_entry(json, keys, first, first_length, cli_key_default, 0u);
            const unsigned int pair = cli_json_made(json, CFG_JSON_OBJECT);
            cli_json_set(json, pair, "first", cli_json_string_made(json, first, first_length));
            cli_json_set(json, pair, "second", cli_json_string_made(json, second, second_length));
            cli_json_set(json, pair, "line", cli_number(json, parsed.line));
            cli_json_set(json, pair, "verdict", cli_json_made(json, CFG_JSON_NULL));
            cli_json_set(json, pair, "notes", cli_json_made(json, CFG_JSON_ARRAY));
            cli_json_append(json, cli_json_member(json, owner, "pairs"), pair);
            under_pair = 1;
            under_identity = 0;
            continue;
        }
        if (cli_word_is(&parsed, "text_identity"))
        {
            cli_json_append(
                json, identities,
                cli_note(json, parsed.word, parsed.word_length, parsed.rest, parsed.rest_length, parsed.line));
            under_pair = 0;
            under_identity = 1;
            continue;
        }
        const int verdict = cli_word_is(&parsed, "open") || cli_word_is(&parsed, "closed");
        if (under_identity && verdict)
        {
            // the verdict after an identity put after its text, two spaces between
            const unsigned int last = json->node[identities].last;
            if (last != CLI_JSON_NONE)
            {
                unsigned long long length = 0ull;
                const unsigned int rest = cli_json_member(json, last, "rest");
                const char *const had = cli_json_string(json, rest, &length);
                const unsigned long long start = json->used;
                if ((json->used + length + parsed.word_length + parsed.rest_length + 4u) <= json->text_most)
                {
                    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = had, .bytes = length});
                    json->used += length;
                    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = "  ", .bytes = 2u});
                    json->used += 2u;
                    memor.cpy(
                        &(MemoriaCfg){.dst = json->text + json->used, .src = parsed.word, .bytes = parsed.word_length});
                    json->used += parsed.word_length;
                    json->text[json->used] = ' ';
                    json->used += 1u;
                    memor.cpy(
                        &(MemoriaCfg){.dst = json->text + json->used, .src = parsed.rest, .bytes = parsed.rest_length});
                    json->used += parsed.rest_length;
                    const unsigned int joined = cli_json_string_made(json, json->text + start, json->used - start);
                    cli_json_set(json, last, "rest", joined);
                }
                else
                {
                    json->full = 1;
                }
            }
            under_identity = 0;
            continue;
        }
        if (!under_pair || (key == NULL))
        {
            continue;
        }
        char name[CLI_PATH_MAX_LEN];
        VerbaLinea named = {name, sizeof(name), 0ull};
        mmgr_line_put_n(&named, key, key_length);
        mmgr_line_finish(&named);
        const unsigned int pairs = cli_json_member(json, cli_json_member(json, keys, name), "pairs");
        const unsigned int pair = cli_json_is(json, pairs, CFG_JSON_ARRAY) ? json->node[pairs].last : CLI_JSON_NONE;
        if (pair == CLI_JSON_NONE)
        {
            continue;
        }
        const unsigned int note =
            cli_note(json, parsed.word, parsed.word_length, parsed.rest, parsed.rest_length, parsed.line);
        if (verdict && cli_json_is(json, cli_json_member(json, pair, "verdict"), CFG_JSON_NULL))
        {
            cli_json_set(json, pair, "verdict", note);
        }
        else
        {
            cli_json_append(json, cli_json_member(json, pair, "notes"), note);
        }
    }
    cli_json_sorted(json, keys);
    const unsigned int klq = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, klq, "path",
                 cli_json_string_made(json, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL})));
    cli_json_set(json, klq, "keys", keys);
    cli_json_set(json, klq, "identities", identities);
    return klq;
}

static unsigned int cli_name_made(CliJson *json, unsigned int line)
{
    const unsigned int name = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, name, "key", cli_json_string_made(json, "", 0u));
    cli_json_set(json, name, "line", cli_number(json, line));
    cli_json_set(json, name, "kind", cli_json_made(json, CFG_JSON_NULL));
    cli_json_set(json, name, "breaks", cli_json_made(json, CFG_JSON_ARRAY));
    return name;
}

unsigned int cli_bridge_klm(CliJson *json, const char *path, const char *text, unsigned long long size)
{
    const unsigned int names = cli_json_made(json, CFG_JSON_OBJECT);
    unsigned int language = cli_json_string_made(json, "", 0u);
    const char *at = text;
    unsigned int number = 0u;
    CliBridgeLine parsed;
    while (cli_next_line(&at, text + size, &number, &parsed))
    {
        const char *name = NULL;
        const unsigned long long name_length = cli_nth(parsed.rest, parsed.rest_length, 0u, &name);
        // the words after the name, one space between each
        const unsigned long long start = json->used;
        const char *word = NULL;
        for (unsigned int index = 1u; cli_nth(parsed.rest, parsed.rest_length, index, &word) != 0ull; index += 1u)
        {
            const unsigned long long length = cli_nth(parsed.rest, parsed.rest_length, index, &word);
            if ((json->used + length + 2u) > json->text_most)
            {
                json->full = 1;
                break;
            }
            if (index != 1u)
            {
                json->text[json->used] = ' ';
                json->used += 1u;
            }
            memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = word, .bytes = length});
            json->used += length;
        }
        const unsigned int value = cli_json_string_made(json, json->text + start, json->used - start);
        if (cli_word_is(&parsed, "klm"))
        {
            language = cli_json_string_made(json, name, name_length);
        }
        else if (cli_word_is(&parsed, "key"))
        {
            const unsigned int entry = cli_entry(json, names, name, name_length, cli_name_made, parsed.line);
            cli_json_set(json, entry, "key", value);
            cli_json_set(json, entry, "line", cli_number(json, parsed.line));
        }
        else if (cli_word_is(&parsed, "kind") || cli_word_is(&parsed, "breaks"))
        {
            const unsigned int entry = cli_entry(json, names, name, name_length, cli_name_made, parsed.line);
            unsigned long long length = 0ull;
            const char *const joined = cli_json_string(json, value, &length);
            const unsigned int note = cli_note(json, parsed.word, parsed.word_length, joined, length, parsed.line);
            if (cli_word_is(&parsed, "kind"))
            {
                cli_json_set(json, entry, "kind", note);
            }
            else
            {
                cli_json_append(json, cli_json_member(json, entry, "breaks"), note);
            }
        }
    }
    cli_json_sorted(json, names);
    const unsigned int klm = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, klm, "path",
                 cli_json_string_made(json, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL})));
    cli_json_set(json, klm, "language", language);
    cli_json_set(json, klm, "names", names);
    return klm;
}

// whether a word is a kind in lower case, and whether a word is a name
static int cli_is_kind(const char *word, unsigned long long length)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if (!(((word[at] >= 'a') && (word[at] <= 'z')) || (word[at] == '_')))
        {
            return 0;
        }
    }
    return length != 0ull;
}

static int cli_is_name(const char *word, unsigned long long length)
{
    if ((length == 0ull) || !(((word[0] | 0x20) >= 'a') && ((word[0] | 0x20) <= 'z')) && (word[0] != '_'))
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = word[at];
        if (!(((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
              ((letter >= '0') && (letter <= '9')) || (letter == '_') || (letter == '.')))
        {
            return 0;
        }
    }
    return 1;
}

static unsigned int cli_list_made(CliJson *json, unsigned int line)
{
    (void)line;
    return cli_json_made(json, CFG_JSON_ARRAY);
}

// A ruleset's entries by name: every line whose first word is a kind in lower case and whose second is a name
unsigned int cli_bridge_ruleset(CliJson *json, const char *path, const char *text, unsigned long long size)
{
    const unsigned int entries = cli_json_made(json, CFG_JSON_OBJECT);
    const char *at = text;
    unsigned int number = 0u;
    CliBridgeLine parsed;
    while (cli_next_line(&at, text + size, &number, &parsed))
    {
        const char *name = NULL;
        const unsigned long long name_length = cli_nth(parsed.rest, parsed.rest_length, 0u, &name);
        if (cli_is_kind(parsed.word, parsed.word_length) && cli_is_name(name, name_length))
        {
            const unsigned int list = cli_entry(json, entries, name, name_length, cli_list_made, parsed.line);
            const unsigned int entry = cli_json_made(json, CFG_JSON_OBJECT);
            cli_json_set(json, entry, "kind", cli_json_string_made(json, parsed.word, parsed.word_length));
            cli_json_set(json, entry, "line", cli_number(json, parsed.line));
            cli_json_append(json, list, entry);
        }
    }
    cli_json_sorted(json, entries);
    const unsigned int ruleset = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, ruleset, "path",
                 cli_json_string_made(json, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL})));
    cli_json_set(json, ruleset, "entries", entries);
    return ruleset;
}

// a file of the bridge's folder counted, or retained where the list has room for it
static void cli_file_each(void *context, const char *name, int is_folder)
{
    CliBridgeFiles *const files = (CliBridgeFiles *)context;
    if (is_folder || (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_MAX_LEN))
    {
        return;
    }
    if ((files->file != NULL) && (files->count < files->most))
    {
        memor.cpy(&(MemoriaCfg){.dst = files->file[files->count].name,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    }
    files->count += ((files->file == NULL) || (files->count < files->most)) ? 1u : 0u;
}

// whether a file's name ends in an extension the bridge reads
static int cli_bridge_named(const char *name)
{
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    const char *const ext = (dot != NULL) ? (dot + 1) : "";
    return (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klq", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klm", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "krs", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "kdm", .cap = CLI_CELLS_POOL}));
}

unsigned int cli_bridge_read(CliJson *json, const char *root)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char relative[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&line, root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, CLI_BRIDGE_DIR);
    mmgr_line_finish(&line);
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    CliBridgeFiles files = {NULL, 0u, 0u};
    query_folder_each(folder, cli_file_each, &files);
    files = (CliBridgeFiles){
        g_cli_cells->persistent_buf_alloc((files.count + 1ull) * sizeof(CliBridgeFile), &g_cli_editor_cells), 0u,
        files.count};
    if (files.file == NULL)
    {
        return CLI_JSON_NONE;
    }
    query_folder_each(folder, cli_file_each, &files);
    for (unsigned int at = 1u; at < files.count; at += 1u)
    {
        const CliBridgeFile swap = files.file[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(files.file[place - 1u].name, swap.name) > 0))
        {
            files.file[place] = files.file[place - 1u];
            place -= 1u;
        }
        files.file[place] = swap;
    }
    // each file read mapped, and the tree's cell taken once for the most its files' lines make
    unsigned long long nodes = CLI_BRIDGE_FILE_NODES;
    unsigned long long text = CLI_BRIDGE_FILE_TEXT;
    for (unsigned int at = 0u; at < files.count; at += 1u)
    {
        CliBridgeFile *const one = &files.file[at];
        VerbaLinea file = {path, sizeof(path), 0ull};
        mmgr_line_put(&file, folder);
        mmgr_line_ch(&file, '/');
        mmgr_line_put(&file, one->name);
        one->bytes = (cli_bridge_named(one->name) && (mmgr_line_finish(&file) != 0ull))
                         ? cli_file_taken(path, &one->size, &g_cli_editor_cells)
                         : NULL;
        if (one->bytes != NULL)
        {
            const unsigned long long lines = 1ull + cli_text_counted((const char *)one->bytes, one->size, '\n');
            nodes += (CLI_BRIDGE_LINE_NODES * lines) + CLI_BRIDGE_FILE_NODES;
            text += (2ull * one->size) + (CLI_BRIDGE_LINE_TEXT * lines) + CLI_PATH_MAX_LEN;
        }
    }
    // a tree's node count is an unsigned int's, and a bridge past it reads as full
    const int swap = (nodes < CLI_JSON_NONE) &&
                     cli_json_reserved(json, (unsigned int)nodes, text, "the bridge read", &g_cli_editor_cells);
    const unsigned int bridge = swap ? cli_json_made(json, CFG_JSON_OBJECT) : CLI_JSON_NONE;
    cli_json_set(json, bridge, "dir",
                 cli_json_string_made(json, CLI_BRIDGE_DIR,
                                      cellul.len(&(CatenaFinitaCfg){.src = CLI_BRIDGE_DIR, .cap = CLI_CELLS_POOL})));
    cli_json_set(json, bridge, "klq", cli_json_made(json, CFG_JSON_NULL));
    const unsigned int maps = cli_json_made(json, CFG_JSON_ARRAY);
    const unsigned int rulesets = cli_json_made(json, CFG_JSON_ARRAY);
    for (unsigned int at = 0u; at < files.count; at += 1u)
    {
        const CliBridgeFile *const one = &files.file[at];
        if (one->bytes == NULL)
        {
            continue;
        }
        const char *const ext =
            cli_text_last(one->name, cellul.len(&(CatenaFinitaCfg){.src = one->name, .cap = CLI_CELLS_POOL}), '.') + 1;
        const char *const text_of = (const char *)one->bytes;
        VerbaLinea file = {path, sizeof(path), 0ull};
        mmgr_line_put(&file, folder);
        mmgr_line_ch(&file, '/');
        mmgr_line_put(&file, one->name);
        mmgr_line_finish(&file);
        cli_relative(root, path, relative);
        if (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klq", .cap = CLI_CELLS_POOL}))
        {
            cli_json_set(json, bridge, "klq", cli_bridge_klq(json, relative, text_of, one->size));
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = ext, .other = "klm", .cap = CLI_CELLS_POOL}))
        {
            cli_json_append(json, maps, cli_bridge_klm(json, relative, text_of, one->size));
        }
        else
        {
            cli_json_append(json, rulesets, cli_bridge_ruleset(json, relative, text_of, one->size));
        }
        cli_file_given(one->bytes, &g_cli_editor_cells);
    }
    g_cli_cells->persistent_buf_release(files.file, &g_cli_editor_cells);
    cli_json_set(json, bridge, "maps", maps);
    cli_json_set(json, bridge, "rulesets", rulesets);
    return json->full ? CLI_JSON_NONE : bridge;
}
