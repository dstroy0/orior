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
    char name[CLI_NAME_LONGEST];
    const unsigned char *bytes;
    unsigned long long size;
} CliBridgeFile;

// The bridge folder's files listed: those kept, counted where `file` is NULL, and the most kept
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

// One line that says something: its number from 0, its first word and the rest, trimmed
typedef struct
{
    unsigned int line;
    const char *word;
    unsigned long long word_length;
    const char *rest;
    unsigned long long rest_length;
} CliSaid;

// the next line from `*at` before `end` that says something, `*number` counting lines as they pass: 1, or 0 at the end
static int cli_said(const char **at, const char *end, unsigned int *number, CliSaid *said)
{
    while (*at < end)
    {
        const char *from = *at;
        const char *stop = from;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
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
        const CliSaid found = {line, from, (unsigned long long)(word_end - from), rest,
                               (unsigned long long)(stop - rest)};
        *said = found;
        return 1;
    }
    return 0;
}

static int cli_word_is(const CliSaid *said, const char *word)
{
    return (said->word_length == cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = said->word, .other = word, .bytes = said->word_length}) == 0);
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
    CliLine line = {digits, sizeof(digits), 0ull};
    cli_line_decimal(&line, value, 1u);
    cli_line_closed(&line);
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
    char key[CLI_PATH_LONGEST];
    CliLine named = {key, sizeof(key), 0ull};
    cli_line_counted(&named, name, length);
    cli_line_closed(&named);
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
    CliSaid said;
    while (cli_said(&at, text + size, &number, &said))
    {
        if (cli_word_is(&said, "klq"))
        {
            continue;
        }
        if (cli_word_is(&said, "key"))
        {
            const char *name = NULL;
            const unsigned long long length = cli_nth(said.rest, said.rest_length, 0u, &name);
            const unsigned int entry = cli_entry(json, keys, name, length, cli_key_made, said.line);
            cli_json_set(json, entry, "line", cli_number(json, said.line));
            key = name;
            key_length = length;
            under_pair = 0;
            under_identity = 0;
            continue;
        }
        if (cli_word_is(&said, "pair"))
        {
            const char *first = NULL;
            const char *second = NULL;
            const unsigned long long first_length = cli_nth(said.rest, said.rest_length, 0u, &first);
            const unsigned long long second_length = cli_nth(said.rest, said.rest_length, 1u, &second);
            const unsigned int owner = (key != NULL) ? cli_entry(json, keys, key, key_length, cli_key_default, 0u)
                                                     : cli_entry(json, keys, first, first_length, cli_key_default, 0u);
            const unsigned int pair = cli_json_made(json, CFG_JSON_OBJECT);
            cli_json_set(json, pair, "first", cli_json_string_made(json, first, first_length));
            cli_json_set(json, pair, "second", cli_json_string_made(json, second, second_length));
            cli_json_set(json, pair, "line", cli_number(json, said.line));
            cli_json_set(json, pair, "verdict", cli_json_made(json, CFG_JSON_NULL));
            cli_json_set(json, pair, "notes", cli_json_made(json, CFG_JSON_ARRAY));
            cli_json_append(json, cli_json_member(json, owner, "pairs"), pair);
            under_pair = 1;
            under_identity = 0;
            continue;
        }
        if (cli_word_is(&said, "text_identity"))
        {
            cli_json_append(json, identities,
                            cli_note(json, said.word, said.word_length, said.rest, said.rest_length, said.line));
            under_pair = 0;
            under_identity = 1;
            continue;
        }
        const int verdict = cli_word_is(&said, "open") || cli_word_is(&said, "closed");
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
                if ((json->used + length + said.word_length + said.rest_length + 4u) <= json->text_most)
                {
                    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = had, .bytes = length});
                    json->used += length;
                    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = "  ", .bytes = 2u});
                    json->used += 2u;
                    memor.cpy(
                        &(MemoriaCfg){.dst = json->text + json->used, .src = said.word, .bytes = said.word_length});
                    json->used += said.word_length;
                    json->text[json->used] = ' ';
                    json->used += 1u;
                    memor.cpy(
                        &(MemoriaCfg){.dst = json->text + json->used, .src = said.rest, .bytes = said.rest_length});
                    json->used += said.rest_length;
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
        char name[CLI_PATH_LONGEST];
        CliLine named = {name, sizeof(name), 0ull};
        cli_line_counted(&named, key, key_length);
        cli_line_closed(&named);
        const unsigned int pairs = cli_json_member(json, cli_json_member(json, keys, name), "pairs");
        const unsigned int pair = cli_json_is(json, pairs, CFG_JSON_ARRAY) ? json->node[pairs].last : CLI_JSON_NONE;
        if (pair == CLI_JSON_NONE)
        {
            continue;
        }
        const unsigned int note = cli_note(json, said.word, said.word_length, said.rest, said.rest_length, said.line);
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
    CliSaid said;
    while (cli_said(&at, text + size, &number, &said))
    {
        const char *name = NULL;
        const unsigned long long name_length = cli_nth(said.rest, said.rest_length, 0u, &name);
        // the words after the name, one space between each
        const unsigned long long start = json->used;
        const char *word = NULL;
        for (unsigned int index = 1u; cli_nth(said.rest, said.rest_length, index, &word) != 0ull; index += 1u)
        {
            const unsigned long long length = cli_nth(said.rest, said.rest_length, index, &word);
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
        if (cli_word_is(&said, "klm"))
        {
            language = cli_json_string_made(json, name, name_length);
        }
        else if (cli_word_is(&said, "key"))
        {
            const unsigned int entry = cli_entry(json, names, name, name_length, cli_name_made, said.line);
            cli_json_set(json, entry, "key", value);
            cli_json_set(json, entry, "line", cli_number(json, said.line));
        }
        else if (cli_word_is(&said, "kind") || cli_word_is(&said, "breaks"))
        {
            const unsigned int entry = cli_entry(json, names, name, name_length, cli_name_made, said.line);
            unsigned long long length = 0ull;
            const char *const joined = cli_json_string(json, value, &length);
            const unsigned int note = cli_note(json, said.word, said.word_length, joined, length, said.line);
            if (cli_word_is(&said, "kind"))
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
    CliSaid said;
    while (cli_said(&at, text + size, &number, &said))
    {
        const char *name = NULL;
        const unsigned long long name_length = cli_nth(said.rest, said.rest_length, 0u, &name);
        if (cli_is_kind(said.word, said.word_length) && cli_is_name(name, name_length))
        {
            const unsigned int list = cli_entry(json, entries, name, name_length, cli_list_made, said.line);
            const unsigned int entry = cli_json_made(json, CFG_JSON_OBJECT);
            cli_json_set(json, entry, "kind", cli_json_string_made(json, said.word, said.word_length));
            cli_json_set(json, entry, "line", cli_number(json, said.line));
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

// a file of the bridge's folder counted, or kept where the list holds room for it
static void cli_file_each(void *context, const char *name, int is_folder)
{
    CliBridgeFiles *const files = (CliBridgeFiles *)context;
    if (is_folder || (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_LONGEST))
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
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char relative[CLI_PATH_LONGEST];
    CliLine line = {folder, sizeof(folder), 0ull};
    cli_line_text(&line, root);
    cli_line_character(&line, '/');
    cli_line_text(&line, CLI_BRIDGE_DIR);
    cli_line_closed(&line);
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    CliBridgeFiles files = {NULL, 0u, 0u};
    query_folder_each(folder, cli_file_each, &files);
    files = (CliBridgeFiles){cli_cell_taken((files.count + 1ull) * sizeof(CliBridgeFile), "the bridge's files"), 0u,
                             files.count};
    if (files.file == NULL)
    {
        return CLI_JSON_NONE;
    }
    query_folder_each(folder, cli_file_each, &files);
    for (unsigned int at = 1u; at < files.count; at += 1u)
    {
        const CliBridgeFile held = files.file[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(files.file[place - 1u].name, held.name) > 0))
        {
            files.file[place] = files.file[place - 1u];
            place -= 1u;
        }
        files.file[place] = held;
    }
    // each file read mapped, and the tree's cell taken once for the most its files' lines make
    unsigned long long nodes = CLI_BRIDGE_FILE_NODES;
    unsigned long long text = CLI_BRIDGE_FILE_TEXT;
    for (unsigned int at = 0u; at < files.count; at += 1u)
    {
        CliBridgeFile *const one = &files.file[at];
        CliLine file = {path, sizeof(path), 0ull};
        cli_line_text(&file, folder);
        cli_line_character(&file, '/');
        cli_line_text(&file, one->name);
        one->bytes = (cli_bridge_named(one->name) && (cli_line_closed(&file) != 0ull))
                         ? qry_member_file_mapped(path, &one->size)
                         : NULL;
        unsigned long long lines = 1ull;
        for (unsigned long long letter = 0ull; (one->bytes != NULL) && (letter < one->size); letter += 1ull)
        {
            lines += (one->bytes[letter] == '\n') ? 1ull : 0ull;
        }
        nodes += (one->bytes != NULL) ? ((CLI_BRIDGE_LINE_NODES * lines) + CLI_BRIDGE_FILE_NODES) : 0ull;
        text += (one->bytes != NULL) ? ((2ull * one->size) + (CLI_BRIDGE_LINE_TEXT * lines) + CLI_PATH_LONGEST) : 0ull;
    }
    // a tree's node count is an unsigned int's, so a bridge past it reads as full
    const int held = (nodes < CLI_JSON_NONE) && cli_json_held(json, (unsigned int)nodes, text, "the bridge read");
    const unsigned int bridge = held ? cli_json_made(json, CFG_JSON_OBJECT) : CLI_JSON_NONE;
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
        CliLine file = {path, sizeof(path), 0ull};
        cli_line_text(&file, folder);
        cli_line_character(&file, '/');
        cli_line_text(&file, one->name);
        cli_line_closed(&file);
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
        qry_member_file_unmapped(one->bytes, one->size);
    }
    cli_cell_given(files.file);
    cli_json_set(json, bridge, "maps", maps);
    cli_json_set(json, bridge, "rulesets", rulesets);
    return json->full ? CLI_JSON_NONE : bridge;
}
