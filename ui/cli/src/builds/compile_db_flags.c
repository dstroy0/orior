// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_flags.c: the defines and the include folders and files the tree's compilation database gives, read again
// each time its file changes. Each entry's `-D` and `/D` words are defines, the entry's file a node of the tree: a
// define every entry under a folder gives with the same value is given to that folder, the highest such, and an
// entry's file keeps those its folder does not give it; a branch inherits the top's defines, then each folder's down to
// its own, each handed on in that order so a lower one stands in for one it inherits. Each entry's `-iquote`, `-I`,
// `/I`, `-isystem` and `-idirafter` words are its include folders, a folder named from the entry's own folder made
// whole, and its `-include` and `/FI` words the files the compiler includes before its first line; a file no entry
// names takes those of the entry nearest it in the tree
#include "../command_line/cells.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// the most files, distinct defines, folders, defines of the files and folders together, defines given, and include
// folders and files read; and the bytes the texts, and the nodes and bytes the database's tree, are taken with
#define CLI_FLAGS_FILES 65536u
#define CLI_FLAGS_DISTINCT 65536u
#define CLI_FLAGS_FOLDERS 65536u
#define CLI_FLAGS_IDS (1u << 22u)
#define CLI_FLAGS_GIVEN (1u << 20u)
#define CLI_FLAGS_INCLUDES (1u << 20u)
#define CLI_FLAGS_TEXT_MORE (64ull << 10u)
// the bytes of a compilation database a flag takes at the least, and the entries over those a list takes
#define CLI_FLAGS_ENTRY_BYTES 32ull
#define CLI_FLAGS_ENTRIES_MORE 256ull

// the entries a list of the database read takes at the most, by the database's bytes
static unsigned long long s_flags_entries;

static unsigned int flags_most(unsigned int most)
{
    // below `most`, which an unsigned int stores
    return (s_flags_entries < most) ? (unsigned int)s_flags_entries : most;
}

// what a word awaits the word after it to be: nothing, a define, or an include folder or file of a kind
#define CLI_FLAGS_AWAITS_NONE 0u
#define CLI_FLAGS_AWAITS_DEFINE 1u
#define CLI_FLAGS_AWAITS_INCLUDE 2u

// A distinct define: its name and its value, each a place among the texts
typedef struct
{
    unsigned int name;
    unsigned int value;
} CliDefineWord;

// A file or folder read: its path from the tree's top, a place among the texts, its defines, a run among the ids, and
// for a file its include folders and files, a run among the includes kept
typedef struct
{
    unsigned int path;
    unsigned int first;
    unsigned int count;
    unsigned int include_first;
    unsigned int includes;
} CliDefineNode;

// A define given to a node: the node and the define, a place among the texts and a place among the distinct
typedef struct
{
    unsigned int node;
    unsigned int define;
} CliDefineNodeGiven;

// An include folder or file of a file: its whole path, a place among the texts, and its kind, a CLI_INCLUDE_ one
typedef struct
{
    unsigned int path;
    unsigned int kind;
} CliIncludeWord;

// What a read keeps, under the lock: the tree and the database file it read with that file's time, the texts, the
// distinct defines, those given, the files read with their include runs, and the include folders and files, all in one
// cell
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    CliStamped database;
    VerbaLinea text;
    CliDefineWord *word;
    unsigned int words;
    unsigned int word_room;
    CliDefineNodeGiven *given;
    unsigned int givens;
    unsigned int given_room;
    CliDefineNode *file;
    unsigned int files;
    unsigned int file_room;
    CliIncludeWord *include;
    unsigned int includes;
    unsigned int include_room;
} CliFlagsKept;

// What a read works with and gives back as it ends: the folders read and the ids of the files' and folders' defines;
// the folder of the entry being read; what the last word awaits, and of an include, its kind; and whether the cells
// refused it
typedef struct
{
    const char *root;
    CliDefineNode *folder;
    unsigned int folders;
    unsigned int folder_room;
    unsigned int *id;
    unsigned int ids;
    unsigned int id_room;
    const char *directory;
    unsigned int awaiting;
    unsigned int awaited_kind;
    int refused;
} CliFlagsReading;

static CliFlagsKept s_kept;
static CliLock s_kept_lock = CLI_LOCK_INIT;

// the text at `at` among the kept texts
static const char *cli_flags_text(unsigned int at)
{
    return s_kept.text.out + at;
}

// `length` bytes of `text` put after the kept texts with a zero after them: their place, or CLI_JSON_NONE where the
// line is full
static unsigned int cli_flags_text_put(const char *text, unsigned long long length)
{
    // the texts read from one database are far under 4 GiB
    const unsigned int at = (unsigned int)s_kept.text.at;
    mmgr_line_put_n(&s_kept.text, text, length);
    mmgr_line_ch(&s_kept.text, '\0');
    return mmgr_line_ok(&s_kept.text) ? at : CLI_JSON_NONE;
}

// the place among the distinct of the define of `name`, `name_length` bytes, and `value`, `value_length`, put where it
// is new; CLI_JSON_NONE where the cells cannot meet it
static unsigned int cli_flags_word(const char *name, unsigned long long name_length, const char *value,
                                   unsigned long long value_length)
{
    for (unsigned int at = 0u; at < s_kept.words; at += 1u)
    {
        const char *const kept_name = cli_flags_text(s_kept.word[at].name);
        const char *const kept_value = cli_flags_text(s_kept.word[at].value);
        if ((cellul.len(&(CatenaFinitaCfg){.src = kept_name, .cap = CLI_CELLS_POOL}) == name_length) &&
            cli_text_same(kept_name, name, name_length) &&
            (cellul.len(&(CatenaFinitaCfg){.src = kept_value, .cap = CLI_CELLS_POOL}) == value_length) &&
            cli_text_same(kept_value, value, value_length))
        {
            return at;
        }
    }
    const unsigned int named =
        (s_kept.words < s_kept.word_room) ? cli_flags_text_put(name, name_length) : CLI_JSON_NONE;
    const unsigned int valued = (named != CLI_JSON_NONE) ? cli_flags_text_put(value, value_length) : CLI_JSON_NONE;
    if (valued == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    s_kept.word[s_kept.words] = (CliDefineWord){named, valued};
    s_kept.words += 1u;
    return s_kept.words - 1u;
}

// the define `id` put after the ids of the run that begins at `first`, standing in for a define of the same name the
// run holds
static void cli_flags_id_put(CliFlagsReading *reading, unsigned int first, unsigned int id)
{
    const char *const name = cli_flags_text(s_kept.word[id].name);
    for (unsigned int at = first; at < reading->ids; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_flags_text(s_kept.word[reading->id[at]].name), .other = name, .cap = CLI_CELLS_POOL}))
        {
            reading->id[at] = id;
            return;
        }
    }
    if (reading->ids >= reading->id_room)
    {
        reading->refused = 1;
        return;
    }
    reading->id[reading->ids] = id;
    reading->ids += 1u;
}

// a define, `NAME`, `NAME=VALUE` or `NAME(PARAMS)=BODY`, `length` bytes, put among the entry's ids, its value `1`
// where the word gives none
static void cli_flags_define(CliFlagsReading *reading, const char *define, unsigned long long length)
{
    unsigned long long name_length = 0ull;
    name_length = cli_text_to_any(define, name_length, length, "=");
    const int valued = name_length < length;
    const unsigned int id = (name_length != 0ull)
                                ? cli_flags_word(define, name_length, valued ? (define + name_length + 1u) : "1",
                                                 valued ? (length - name_length - 1ull) : 1ull)
                                : CLI_JSON_NONE;
    if (id != CLI_JSON_NONE)
    {
        cli_flags_id_put(reading, s_kept.file[s_kept.files].first, id);
    }
}

// an include folder or file of the entry, `length` bytes, of `kind`, made whole from the entry's own folder where it
// is named from it, its slashes made `/`, put after the entry's others
static void cli_flags_include(CliFlagsReading *reading, const char *named, unsigned long long length, unsigned int kind)
{
    if ((length == 0ull) || (length >= CLI_PATH_MAX_LEN))
    {
        return;
    }
    char given[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = given, .src = named, .bytes = (size_t)length});
    given[length] = '\0';
    char whole[CLI_PATH_MAX_LEN];
    const int is_whole = (given[0] == '/') || (given[0] == '\\') || ((length > 1ull) && (given[1] == ':'));
    if (is_whole || (reading->directory == NULL))
    {
        memor.cpy(&(MemoriaCfg){.dst = whole, .src = given, .bytes = (size_t)length + 1u});
    }
    else
    {
        cli_full(reading->directory, given, whole);
    }
    cli_text_swapped(whole, cellul.len(&(CatenaFinitaCfg){.src = whole, .cap = CLI_CELLS_POOL}), '\\', '/');
    const unsigned int put =
        (s_kept.includes < s_kept.include_room)
            ? cli_flags_text_put(whole, cellul.len(&(CatenaFinitaCfg){.src = whole, .cap = CLI_CELLS_POOL}))
            : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        reading->refused = 1;
        return;
    }
    s_kept.include[s_kept.includes] = (CliIncludeWord){put, kind};
    s_kept.includes += 1u;
}

// One flag of a command line: the flag's text, its length, and the kind of include it gives, CLI_JSON_NONE for a
// define
typedef struct
{
    const char *flag;
    unsigned long long length;
    unsigned int kind;
} CliFlagWord;

// the flags read, longest first where one begins another
static const CliFlagWord s_flags[] = {
    {"-idirafter", 10ull, CLI_INCLUDE_AFTER},
    {"-isystem", 8ull, CLI_INCLUDE_SYSTEM},
    {"-include", 8ull, CLI_INCLUDE_FORCED},
    {"-iquote", 7ull, CLI_INCLUDE_QUOTE},
    {"/FI", 3ull, CLI_INCLUDE_FORCED},
    {"-I", 2ull, CLI_INCLUDE_FOLDER},
    {"/I", 2ull, CLI_INCLUDE_FOLDER},
    {"-D", 2ull, CLI_JSON_NONE},
    {"/D", 2ull, CLI_JSON_NONE},
};

// One word of an entry's command line read: a flag alone awaits what it gives in the word after it, and a flag joined
// to what it gives, or the word a flag awaited, is a define or an include folder or file of the entry
static void cli_flags_word_read(void *context, const char *word, unsigned long long size)
{
    CliFlagsReading *const reading = (CliFlagsReading *)context;
    if (reading->awaiting != CLI_FLAGS_AWAITS_NONE)
    {
        const unsigned int awaited = reading->awaiting;
        reading->awaiting = CLI_FLAGS_AWAITS_NONE;
        if (awaited == CLI_FLAGS_AWAITS_DEFINE)
        {
            cli_flags_define(reading, word, size);
        }
        else
        {
            cli_flags_include(reading, word, size, reading->awaited_kind);
        }
        return;
    }
    for (unsigned int at = 0u; at < (sizeof(s_flags) / sizeof(s_flags[0])); at += 1u)
    {
        const CliFlagWord *const flag = &s_flags[at];
        if ((size < flag->length) || !cli_text_same(word, flag->flag, flag->length))
        {
            continue;
        }
        if (size == flag->length)
        {
            reading->awaiting = (flag->kind == CLI_JSON_NONE) ? CLI_FLAGS_AWAITS_DEFINE : CLI_FLAGS_AWAITS_INCLUDE;
            reading->awaited_kind = flag->kind;
        }
        else if (flag->kind == CLI_JSON_NONE)
        {
            cli_flags_define(reading, word + flag->length, size - flag->length);
        }
        else
        {
            cli_flags_include(reading, word + flag->length, size - flag->length, flag->kind);
        }
        return;
    }
}

// the path of the entry's file from the tree's top into `path`, its slashes made `/`, sized for CLI_PATH_MAX_LEN: 1,
// or 0 where the entry names no file inside the tree
static int cli_flags_file_of(const CliJson *json, unsigned int entry, const char *root, char *path)
{
    unsigned long long size = 0ull;
    const char *const file = cli_json_string(json, cli_json_member(json, entry, "file"), &size);
    const char *const directory = cli_json_string(json, cli_json_member(json, entry, "directory"), NULL);
    if ((file == NULL) || (size >= CLI_PATH_MAX_LEN))
    {
        return 0;
    }
    char named[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = named, .src = file, .bytes = (size_t)size});
    named[size] = '\0';
    char joined[CLI_PATH_MAX_LEN];
    const int whole = (named[0] == '/') || (named[0] == '\\') || ((size > 1ull) && (named[1] == ':'));
    if (whole || (directory == NULL))
    {
        memor.cpy(&(MemoriaCfg){.dst = joined, .src = named, .bytes = (size_t)size + 1u});
    }
    else
    {
        cli_full(directory, named, joined);
    }
    char top[CLI_PATH_MAX_LEN];
    const unsigned long long top_length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){.dst = top, .src = root, .bytes = (size_t)top_length + 1u});
    cli_text_swapped(joined, cellul.len(&(CatenaFinitaCfg){.src = joined, .cap = CLI_CELLS_POOL}), '\\', '/');
    for (unsigned long long at = 0ull; at < top_length; at += 1ull)
    {
        top[at] = (top[at] == '\\') ? '/' : top[at];
    }
    if (!cli_text_same(joined, top, top_length) || (joined[top_length] != '/'))
    {
        return 0;
    }
    const unsigned long long length =
        cellul.len(&(CatenaFinitaCfg){.src = joined + top_length + 1u, .cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){.dst = path, .src = joined + top_length + 1u, .bytes = (size_t)length + 1u});
    return 1;
}

// whether the run of `node` holds the define `id`
static int cli_flags_holds(const CliFlagsReading *reading, const CliDefineNode *node, unsigned int id)
{
    for (unsigned int at = node->first; at < (node->first + node->count); at += 1u)
    {
        if (reading->id[at] == id)
        {
            return 1;
        }
    }
    return 0;
}

// the folder `path`, `length` bytes, made to hold the defines of `file` with those it had: each it had that `file`
// has kept, and where it is new, a copy of `file`'s; its place, or CLI_JSON_NONE where the cells cannot meet it
static unsigned int cli_flags_folder_met(CliFlagsReading *reading, const char *path, unsigned long long length,
                                         const CliDefineNode *file)
{
    for (unsigned int at = 0u; at < reading->folders; at += 1u)
    {
        const char *const kept = cli_flags_text(reading->folder[at].path);
        if ((cellul.len(&(CatenaFinitaCfg){.src = kept, .cap = CLI_CELLS_POOL}) == length) &&
            cli_text_same(kept, path, length))
        {
            CliDefineNode *const folder = &reading->folder[at];
            unsigned int kept = 0u;
            for (unsigned int each = 0u; each < folder->count; each += 1u)
            {
                const unsigned int id = reading->id[folder->first + each];
                if (cli_flags_holds(reading, file, id))
                {
                    reading->id[folder->first + kept] = id;
                    kept += 1u;
                }
            }
            folder->count = kept;
            return at;
        }
    }
    const unsigned int named =
        (reading->folders < reading->folder_room) ? cli_flags_text_put(path, length) : CLI_JSON_NONE;
    if (named == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    const unsigned int first = reading->ids;
    for (unsigned int each = 0u; each < file->count; each += 1u)
    {
        cli_flags_id_put(reading, first, reading->id[file->first + each]);
    }
    reading->folder[reading->folders] = (CliDefineNode){named, first, reading->ids - first, 0u, 0u};
    reading->folders += 1u;
    return reading->folders - 1u;
}

// the folder that holds the file or folder `path`, its place among those read, CLI_JSON_NONE for the top's own
static unsigned int cli_flags_parent(const CliFlagsReading *reading, const char *path)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    if (length == 0ull)
    {
        return CLI_JSON_NONE;
    }
    unsigned long long cut = length;
    cut = cli_text_back_any(path, cut, "/");
    const unsigned long long parent = (cut > 0ull) ? (cut - 1ull) : 0ull;
    for (unsigned int at = 0u; at < reading->folders; at += 1u)
    {
        const char *const kept = cli_flags_text(reading->folder[at].path);
        if ((cellul.len(&(CatenaFinitaCfg){.src = kept, .cap = CLI_CELLS_POOL}) == parent) &&
            cli_text_same(kept, path, parent))
        {
            return at;
        }
    }
    return CLI_JSON_NONE;
}

// the defines of `node` its folder does not give it, given to it
static void cli_flags_given_put(CliFlagsReading *reading, const CliDefineNode *node)
{
    const unsigned int parent = cli_flags_parent(reading, cli_flags_text(node->path));
    for (unsigned int each = 0u; each < node->count; each += 1u)
    {
        const unsigned int id = reading->id[node->first + each];
        if ((parent != CLI_JSON_NONE) && cli_flags_holds(reading, &reading->folder[parent], id))
        {
            continue;
        }
        if (s_kept.givens >= s_kept.given_room)
        {
            reading->refused = 1;
            return;
        }
        s_kept.given[s_kept.givens] = (CliDefineNodeGiven){node->path, id};
        s_kept.givens += 1u;
    }
}

// the entries of the database read into the kept files, each with its defines and include folders and files, under
// the lock; an entry of a file read before passed over
static void cli_flags_entries(CliFlagsReading *reading, const CliJson *json, unsigned int entries)
{
    for (unsigned int item = 0u; !reading->refused && (cli_json_item(json, entries, item) != CLI_JSON_NONE); item += 1u)
    {
        const unsigned int entry = cli_json_item(json, entries, item);
        char path[CLI_PATH_MAX_LEN];
        if (!cli_flags_file_of(json, entry, reading->root, path))
        {
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        int seen = 0;
        for (unsigned int at = 0u; !seen && (at < s_kept.files); at += 1u)
        {
            seen = cellul.eq(
                &(CatenaFinitaCfg){.src = cli_flags_text(s_kept.file[at].path), .other = path, .cap = CLI_CELLS_POOL});
        }
        const unsigned int named =
            (!seen && (s_kept.files < s_kept.file_room)) ? cli_flags_text_put(path, length) : CLI_JSON_NONE;
        if (named == CLI_JSON_NONE)
        {
            continue;
        }
        s_kept.file[s_kept.files] = (CliDefineNode){named, reading->ids, 0u, s_kept.includes, 0u};
        reading->awaiting = CLI_FLAGS_AWAITS_NONE;
        reading->directory = cli_json_string(json, cli_json_member(json, entry, "directory"), NULL);
        const unsigned int arguments = cli_json_member(json, entry, "arguments");
        if (cli_json_is(json, arguments, CFG_JSON_ARRAY))
        {
            for (unsigned int word = 0u; cli_json_item(json, arguments, word) != CLI_JSON_NONE; word += 1u)
            {
                unsigned long long size = 0ull;
                const char *const text = cli_json_string(json, cli_json_item(json, arguments, word), &size);
                if (text != NULL)
                {
                    cli_flags_word_read(reading, text, size);
                }
            }
        }
        else
        {
            unsigned long long size = 0ull;
            const char *const command = cli_json_string(json, cli_json_member(json, entry, "command"), &size);
            if (command != NULL)
            {
                cli_compile_words_each(command, size, cli_flags_word_read, reading);
            }
        }
        s_kept.file[s_kept.files].count = reading->ids - s_kept.file[s_kept.files].first;
        s_kept.file[s_kept.files].includes = s_kept.includes - s_kept.file[s_kept.files].include_first;
        s_kept.files += 1u;
    }
}

// each file's folders, from the one that holds it up to the top's own, made to hold the defines every file under
// each holds, and then the defines given: each folder's its folder does not give it, and each file's the same
static void cli_flags_lifted(CliFlagsReading *reading)
{
    for (unsigned int at = 0u; !reading->refused && (at < s_kept.files); at += 1u)
    {
        const CliDefineNode file = s_kept.file[at];
        // the file's path copied out of the kept texts, which move as the folders' paths are put after them
        char path[CLI_PATH_MAX_LEN];
        const char *const kept = cli_flags_text(file.path);
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = kept, .cap = CLI_CELLS_POOL});
        memor.cpy(&(MemoriaCfg){.dst = path, .src = kept, .bytes = (size_t)size + 1u});
        unsigned long long cut = size;
        int done = 0;
        while (!done)
        {
            cut = cli_text_back_any(path, cut, "/");
            const unsigned long long length = (cut > 0ull) ? (cut - 1ull) : 0ull;
            reading->refused |= cli_flags_folder_met(reading, path, length, &file) == CLI_JSON_NONE;
            done = (cut == 0ull) || reading->refused;
            cut = length;
        }
    }
    for (unsigned int at = 0u; !reading->refused && (at < reading->folders); at += 1u)
    {
        cli_flags_given_put(reading, &reading->folder[at]);
    }
    for (unsigned int at = 0u; !reading->refused && (at < s_kept.files); at += 1u)
    {
        cli_flags_given_put(reading, &s_kept.file[at]);
    }
}

// the kept flags let go, their cells given back, under the lock
static void cli_flags_let_go(void)
{
    g_cli_cells->persistent_buf_release(s_kept.word, &g_cli_compile_cells);
    s_kept.text = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    s_kept.word = NULL;
    s_kept.words = 0u;
    s_kept.word_room = 0u;
    s_kept.given = NULL;
    s_kept.givens = 0u;
    s_kept.given_room = 0u;
    s_kept.file = NULL;
    s_kept.files = 0u;
    s_kept.file_room = 0u;
    s_kept.include = NULL;
    s_kept.includes = 0u;
    s_kept.include_room = 0u;
}

// the database of the tree at `root` read into the kept flags where it or the tree is not the one read last, under
// the lock: the tree's own database, or the one gathered from its builds
static void cli_flags_read(const char *root)
{
    CliDatabase database;
    (void)cli_compile_db_refresh(root, &database);
    CliInputs inputs = {0, NULL, 0u, 0u};
    cli_compile_inputs(root, &inputs);
    char path[CLI_PATH_MAX_LEN];
    if (inputs.own)
    {
        memor.cpy(&(MemoriaCfg){.dst = path, .src = inputs.file[0].path, .bytes = CLI_PATH_MAX_LEN});
    }
    else
    {
        cli_full(root, CLI_COMPILE_WRITTEN, path);
    }
    cli_compile_inputs_given(&inputs);
    CliStamped stamped = {"", 0ull};
    const int there = cli_compile_stamped(path, &stamped);
    const int same =
        cellul.eq(&(CatenaFinitaCfg){.src = s_kept.root, .other = root, .cap = CLI_CELLS_POOL}) &&
        cellul.eq(&(CatenaFinitaCfg){.src = s_kept.database.path, .other = stamped.path, .cap = CLI_CELLS_POOL}) &&
        (s_kept.database.time == stamped.time) && (s_kept.text.out != NULL);
    if (same)
    {
        return;
    }
    cli_flags_let_go();
    memor.cpy(&(MemoriaCfg){.dst = s_kept.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_kept.database = stamped;
    unsigned long long database_size = 0ull;
    if (!there || !cli_file_size(stamped.path, &database_size))
    {
        return;
    }
    // the flags' texts stand among the database's own bytes
    s_flags_entries = (database_size / CLI_FLAGS_ENTRY_BYTES) + CLI_FLAGS_ENTRIES_MORE;
    const unsigned long long text_room = database_size + CLI_FLAGS_TEXT_MORE;
    const unsigned int word_room = flags_most(CLI_FLAGS_DISTINCT);
    const unsigned int given_room = flags_most(CLI_FLAGS_GIVEN);
    const unsigned int file_room = flags_most(CLI_FLAGS_FILES);
    const unsigned int include_room = flags_most(CLI_FLAGS_INCLUDES);
    const unsigned long long word_bytes = (unsigned long long)word_room * sizeof(CliDefineWord);
    const unsigned long long given_bytes = (unsigned long long)given_room * sizeof(CliDefineNodeGiven);
    const unsigned long long file_bytes = (unsigned long long)file_room * sizeof(CliDefineNode);
    const unsigned long long include_bytes = (unsigned long long)include_room * sizeof(CliIncludeWord);
    char *const kept = g_cli_cells->persistent_buf_alloc(
        word_bytes + given_bytes + file_bytes + include_bytes + text_room, &g_cli_compile_cells);
    if (kept == NULL)
    {
        return;
    }
    s_kept.word = (CliDefineWord *)(void *)kept;
    s_kept.word_room = word_room;
    s_kept.given = (CliDefineNodeGiven *)(void *)(kept + word_bytes);
    s_kept.given_room = given_room;
    s_kept.file = (CliDefineNode *)(void *)(kept + word_bytes + given_bytes);
    s_kept.file_room = file_room;
    s_kept.include = (CliIncludeWord *)(void *)(kept + word_bytes + given_bytes + file_bytes);
    s_kept.include_room = include_room;
    s_kept.text = (VerbaLinea){kept + word_bytes + given_bytes + file_bytes + include_bytes, text_room, 0ull};
    CliJson json;
    char unread_text[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {unread_text, sizeof(unread_text), 0ull};
    const unsigned int entries = cli_json_file_taken(&json, stamped.path, 0u, 0ull, &unread, &g_cli_compile_cells);
    const unsigned int folder_room = flags_most(CLI_FLAGS_FOLDERS);
    const unsigned int id_room = flags_most(CLI_FLAGS_IDS);
    const unsigned long long folder_bytes = (unsigned long long)folder_room * sizeof(CliDefineNode);
    char *const work = g_cli_cells->temporary_buf_alloc(
        folder_bytes + ((unsigned long long)id_room * sizeof(unsigned int)), &g_cli_compile_cells);
    CliFlagsReading reading = {root, NULL, 0u, 0u, NULL, 0u, 0u, NULL, CLI_FLAGS_AWAITS_NONE, 0u, 0};
    if (work != NULL)
    {
        reading.folder = (CliDefineNode *)(void *)work;
        reading.folder_room = folder_room;
        reading.id = (unsigned int *)(void *)(work + folder_bytes);
        reading.id_room = id_room;
    }
    if (cli_json_is(&json, entries, CFG_JSON_ARRAY))
    {
        cli_flags_entries(&reading, &json, entries);
        cli_flags_lifted(&reading);
    }
    cli_json_given(&json);
    g_cli_cells->persistent_buf_release(work, &g_cli_compile_cells);
}

// the define of the `-D` word's `name`, `NAME` or `NAME(PARAMS)`, and `value`, given to `node`, handed to `each`: a
// name with brackets one called with arguments, its parameters the names between commas within them, white space
// about each left off, and a `...` among them taking the arguments past them
static void cli_flags_define_handed(const char *name, const char *value, const char *node, CliDefineEach each,
                                    void *context)
{
    char named[CLI_NAME_MAX_LEN];
    char params[CLI_NAME_MAX_LEN];
    CliDefineGiven define = {named, 0, {NULL}, 0u, 0, value, node};
    unsigned long long at = 0ull;
    at = cli_text_to_any(name, 0ull, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), "(");
    at = (at < sizeof(named)) ? at : (sizeof(named) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = named, .src = name, .bytes = (size_t)at});
    named[at] = '\0';
    define.called = name[at] == '(';
    unsigned long long put = 0ull;
    const char *from = define.called ? (name + at + 1u) : "";
    while ((*from != '\0') && (*from != ')') && (define.params < CLI_DEFINE_PARAMS))
    {
        from +=
            cli_text_past_any(from, 0ull, cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), " \t");
        const char *to = from;
        to += cli_text_to_any(to, 0ull, cellul.len(&(CatenaFinitaCfg){.src = to, .cap = CLI_CELLS_POOL}), ",) \t");
        // a parameter's name lies between its separators, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        const int rest = (length == 3ull) && cli_text_same(from, "...", 3ull);
        if ((length != 0ull) && !rest && ((put + length + 1u) < sizeof(params)))
        {
            memor.cpy(&(MemoriaCfg){.dst = params + put, .src = from, .bytes = (size_t)length});
            params[put + length] = '\0';
            define.param[define.params] = params + put;
            define.params += 1u;
            put += length + 1u;
        }
        define.rest |= rest;
        to += cli_text_past_any(to, 0ull, cellul.len(&(CatenaFinitaCfg){.src = to, .cap = CLI_CELLS_POOL}), " \t");
        from = (*to == ',') ? (to + 1) : to;
    }
    each(context, &define);
}

void cli_compile_db_defines_of(const char *root, const char *path, CliDefineEach each, void *context)
{
    cli_lock_take(&s_kept_lock);
    cli_flags_read(root);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    // the top's own, then each folder's down to the branch's own, each node a cut of the path at a slash or its end
    unsigned long long cut = 0ull;
    for (int done = 0; !done && (s_kept.text.out != NULL);)
    {
        for (unsigned int at = 0u; at < s_kept.givens; at += 1u)
        {
            const char *const node = cli_flags_text(s_kept.given[at].node);
            if ((cellul.len(&(CatenaFinitaCfg){.src = node, .cap = CLI_CELLS_POOL}) == cut) &&
                cli_text_same(node, path, cut))
            {
                const CliDefineWord *const word = &s_kept.word[s_kept.given[at].define];
                cli_flags_define_handed(cli_flags_text(word->name), cli_flags_text(word->value), node, each, context);
            }
        }
        done = cut >= length;
        cut += (cut == 0ull) ? 0ull : 1ull;
        cut = cli_text_to_any(path, cut, length, "/");
    }
    cli_lock_give(&s_kept_lock);
}

void cli_compile_db_includes_all(const char *root, CliIncludeEach each, void *context)
{
    cli_lock_take(&s_kept_lock);
    cli_flags_read(root);
    for (unsigned int at = 0u; (s_kept.text.out != NULL) && (at < s_kept.includes); at += 1u)
    {
        const char *const path = cli_flags_text(s_kept.include[at].path);
        int before = 0;
        for (unsigned int earlier = 0u; !before && (earlier < at); earlier += 1u)
        {
            before = cellul.eq(&(CatenaFinitaCfg){
                .src = cli_flags_text(s_kept.include[earlier].path), .other = path, .cap = CLI_CELLS_POOL});
        }
        if (!before)
        {
            each(context, path, s_kept.include[at].kind);
        }
    }
    cli_lock_give(&s_kept_lock);
}

// how many bytes of folders the paths `one` and `other` share, to the last slash both have at the same place
static unsigned long long cli_flags_shared(const char *one, const char *other)
{
    unsigned long long shared = 0ull;
    shared =
        cli_text_back_any(one, cellul.diff(&(CatenaFinitaCfg){.src = one, .other = other, .cap = CLI_CELLS_POOL}), "/");
    return shared;
}

void cli_compile_db_includes_of(const char *root, const char *path, CliIncludeEach each, void *context)
{
    static const unsigned int s_order[5] = {CLI_INCLUDE_QUOTE, CLI_INCLUDE_FOLDER, CLI_INCLUDE_SYSTEM,
                                            CLI_INCLUDE_AFTER, CLI_INCLUDE_FORCED};
    cli_lock_take(&s_kept_lock);
    cli_flags_read(root);
    // the file's own entry, else the entry sharing the most of its folders, the first of those that share as many
    unsigned int nearest = CLI_JSON_NONE;
    unsigned long long most = 0ull;
    for (unsigned int at = 0u; (s_kept.text.out != NULL) && (at < s_kept.files); at += 1u)
    {
        const char *const file = cli_flags_text(s_kept.file[at].path);
        if (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = path, .cap = CLI_CELLS_POOL}))
        {
            nearest = at;
            break;
        }
        const unsigned long long shared = cli_flags_shared(file, path);
        if ((nearest == CLI_JSON_NONE) || (shared > most))
        {
            nearest = at;
            most = shared;
        }
    }
    for (unsigned int kind = 0u; (nearest != CLI_JSON_NONE) && (kind < 5u); kind += 1u)
    {
        const CliDefineNode *const file = &s_kept.file[nearest];
        for (unsigned int at = file->include_first; at < (file->include_first + file->includes); at += 1u)
        {
            if (s_kept.include[at].kind == s_order[kind])
            {
                each(context, cli_flags_text(s_kept.include[at].path), s_order[kind]);
            }
        }
    }
    cli_lock_give(&s_kept_lock);
}
