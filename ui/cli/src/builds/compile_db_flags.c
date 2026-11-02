// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_flags.c: the defines and the include folders and files the tree's compilation database gives, read again
// each time its file changes. Each entry's `-D` and `/D` words are defines, the entry's file a node of the tree: a
// define every entry under a folder gives with the same value is given to that folder, the highest such, and an
// entry's file saves those its folder does not give it; a branch inherits the top's defines, then each folder's down to
// its own, each handed on in that order so a lower one stands in for one it inherits. Each entry's `-iquote`, `-I`,
// `/I`, `-isystem` and `-idirafter` words are its include folders, a folder named from the entry's own folder made
// whole, and its `-include` and `/FI` words the files the compiler includes before its first line; a file no entry
// names takes those of the entry nearest it in the tree
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// A file or folder read: its path from the tree's top, a place among the texts, its defines, a run among the ids, and
// for a file its include folders and files, a run among the includes saved
typedef struct
{
    unsigned int path;
    unsigned int first;
    unsigned int count;
    unsigned int include_first;
    unsigned int includes;
} CliDefineNode;
// What a read works with and gives back as it ends: the folders read and the ids of the files' and folders' defines;
// the folder of the entry being read; what the last word awaits, and of an include, its kind; and whether the cells
// refused it
typedef struct
{
    const char *root;
    CliDefineNode *folder;
    unsigned int folders;
    unsigned int folder_capacity;
    unsigned int *id;
    unsigned int ids;
    unsigned int id_capacity;
    const char *directory;
    unsigned int awaiting;
    unsigned int awaited_kind;
    int refused;
} CliFlagsReadState;
// the most files, distinct defines, folders, defines of the files and folders together, defines given, and include
// folders and files read; and the bytes the texts, and the nodes and bytes the database's tree, are taken with
#define CLI_FLAGS_FILES 65536u
#define CLI_FLAGS_DISTINCT 65536u
#define CLI_FLAGS_FOLDERS 65536u
#define CLI_FLAGS_IDS (1u << 22u)
#define CLI_FLAGS_INPUT (1u << 20u)
#define CLI_FLAGS_INCLUDES (1u << 20u)
#define CLI_FLAGS_TEXT_MORE (64ull << 10u)
// the bytes of a compilation database a flag takes at the least, and the entries over those a list takes
#define CLI_FLAGS_ENTRY_BYTES 32ull
#define CLI_FLAGS_ENTRIES_MORE 256ull

// the entries a list of the database read takes at the most, by the database's bytes
static unsigned long long s_flags_entries;

typedef struct
{
    unsigned int most;
    unsigned int at;
    const char *text;
    unsigned long long length;
    const char *name;
    unsigned long long name_length;
    const char *value;
    unsigned long long value_length;
    CliFlagsReadState *read_state;
    unsigned int first;
    unsigned int id;
    const char *define;
    const char *named;
    unsigned int kind;
    const CliJson *json;
    unsigned int entry;
    const char *root;
    char *path_out;
    const CliFlagsReadState *read_state_read;
    const CliDefineNode *node;
    const char *path;
    const CliDefineNode *file;
    unsigned int entries;
    const char *node_text;
    CliDefineEach each;
    void *context;
    const char *one;
    const char *other;
} CompileDbFlagsCtx;
EMBED_INLINE unsigned int flags_max(const CompileDbFlagsCtx *args)
{
    // below `most`, which an unsigned int stores
    return (s_flags_entries < args->most) ? (unsigned int)s_flags_entries : args->most;
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

// A define given to a node: the node and the define, a place among the texts and a place among the distinct
typedef struct
{
    unsigned int node;
    unsigned int define;
} CliDefineNodeInput;

// An include folder or file of a file: its whole path, a place among the texts, and its kind, a CLI_INCLUDE_ one
typedef struct
{
    unsigned int path;
    unsigned int kind;
} CliIncludeWord;

// What a read saves, under the lock: the tree and the database file it read with that file's time, the texts, the
// distinct defines, those given, the files read with their include runs, and the include folders and files, all in one
// cell
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    CliStamped database;
    VerbaLinea text;
    CliDefineWord *word;
    unsigned int words;
    unsigned int word_capacity;
    CliDefineNodeInput *supplied;
    unsigned int givens;
    unsigned int input_capacity;
    CliDefineNode *file;
    unsigned int files;
    unsigned int file_capacity;
    CliIncludeWord *include;
    unsigned int includes;
    unsigned int include_capacity;
} CliFlagsSaved;

static CliFlagsSaved s_saved;

// the text at `at` among the saved texts
EMBED_INLINE const char *cli_flags_text(const CompileDbFlagsCtx *args)
{
    return s_saved.text.out + args->at;
}

// `length` bytes of `text` put after the saved texts with a zero after them: their place, or CLI_JSON_NONE where the
// line is full
EMBED_INLINE unsigned int cli_flags_text_append(const CompileDbFlagsCtx *args)
{
    // the texts read from one database are far under 4 GiB
    const unsigned int at = (unsigned int)s_saved.text.at;
    mmgr_line_put_n(&s_saved.text, args->text, args->length);
    mmgr_line_ch(&s_saved.text, '\0');
    return mmgr_line_ok(&s_saved.text) ? at : CLI_JSON_NONE;
}

// the place among the distinct of the define of `name`, `name_length` bytes, and `value`, `value_length`, put where it
// is new; CLI_JSON_NONE where the cells cannot meet it
EMBED_INLINE unsigned int cli_flags_word(const CompileDbFlagsCtx *args)
{
    for (unsigned int at = 0u; at < s_saved.words; at += 1u)
    {
        const char *const saved_name = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.word[at].name);
        const char *const saved_value = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.word[at].value);
        if ((cellul.len(&(CatenaFinitaCfg){.src = saved_name, .cap = CLI_CELLS_POOL}) == args->name_length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = saved_name, .other_pointer = args->name,
                       .size = args->name_length) &&
            (cellul.len(&(CatenaFinitaCfg){.src = saved_value, .cap = CLI_CELLS_POOL}) == args->value_length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = saved_value, .other_pointer = args->value,
                       .size = args->value_length))
        {
            return at;
        }
    }
    const unsigned int named =
        (s_saved.words < s_saved.word_capacity)
            ? EMBED_CALL(cli_flags_text_append, CompileDbFlagsCtx, .text = args->name, .length = args->name_length)
            : CLI_JSON_NONE;
    const unsigned int valued = (named != CLI_JSON_NONE) ? EMBED_CALL(cli_flags_text_append, CompileDbFlagsCtx,
                                                                      .text = args->value, .length = args->value_length)
                                                         : CLI_JSON_NONE;
    if (valued == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    s_saved.word[s_saved.words] = (CliDefineWord){named, valued};
    s_saved.words += 1u;
    return s_saved.words - 1u;
}

// the define `id` put after the ids of the run that begins at `first`, standing in for a define of the same name the
// run holds
EMBED_INLINE void cli_flags_id_append(const CompileDbFlagsCtx *args)
{
    const char *const name = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.word[args->id].name);
    for (unsigned int at = args->first; at < args->read_state->ids; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.word[args->read_state->id[at]].name),
                .other = name,
                .cap = CLI_CELLS_POOL}))
        {
            args->read_state->id[at] = args->id;
            return;
        }
    }
    if (args->read_state->ids >= args->read_state->id_capacity)
    {
        args->read_state->refused = 1;
        return;
    }
    args->read_state->id[args->read_state->ids] = args->id;
    args->read_state->ids += 1u;
}

// a define, `NAME`, `NAME=VALUE` or `NAME(PARAMS)=BODY`, `length` bytes, put among the entry's ids, its value `1`
// where the word gives none
EMBED_INLINE void cli_flags_define(const CompileDbFlagsCtx *args)
{
    unsigned long long name_length = 0ull;
    name_length = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->define,
                             .at = name_length, .size = args->length, .set = "=");
    const int valued = name_length < args->length;
    const unsigned int id =
        (name_length != 0ull)
            ? EMBED_CALL(cli_flags_word, CompileDbFlagsCtx, .name = args->define, .name_length = name_length,
                         .value = valued ? (args->define + name_length + 1u) : "1",
                         .value_length = valued ? (args->length - name_length - 1ull) : 1ull)
            : CLI_JSON_NONE;
    if (id != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_flags_id_append, CompileDbFlagsCtx, .read_state = args->read_state,
                   .first = s_saved.file[s_saved.files].first, .id = id);
    }
}

// an include folder or file of the entry, `length` bytes, of `kind`, made whole from the entry's own folder where it
// is named from it, its slashes made `/`, put after the entry's others
EMBED_INLINE void cli_flags_include(const CompileDbFlagsCtx *args)
{
    if ((args->length == 0ull) || (args->length >= CLI_PATH_MAX_LEN))
    {
        return;
    }
    char supplied[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = supplied, .src = args->named, .bytes = (size_t)args->length});
    supplied[args->length] = '\0';
    char whole[CLI_PATH_MAX_LEN];
    const int is_absolute =
        (supplied[0] == '/') || (supplied[0] == '\\') || ((args->length > 1ull) && (supplied[1] == ':'));
    if (is_absolute || (args->read_state->directory == NULL))
    {
        memor.cpy(&(MemoriaCfg){.dst = whole, .src = supplied, .bytes = (size_t)args->length + 1u});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->read_state->directory, .relative = supplied,
                   .joined = whole);
    }
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = whole,
               .size = cellul.len(&(CatenaFinitaCfg){.src = whole, .cap = CLI_CELLS_POOL}), .from = '\\', .to = '/');
    const unsigned int put =
        (s_saved.includes < s_saved.include_capacity)
            ? EMBED_CALL(cli_flags_text_append, CompileDbFlagsCtx, .text = whole,
                         .length = cellul.len(&(CatenaFinitaCfg){.src = whole, .cap = CLI_CELLS_POOL}))
            : CLI_JSON_NONE;
    if (put == CLI_JSON_NONE)
    {
        args->read_state->refused = 1;
        return;
    }
    s_saved.include[s_saved.includes] = (CliIncludeWord){put, args->kind};
    s_saved.includes += 1u;
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
static void cli_flags_word_read(const CliEachWordSizeCfg *args)
{
    unsigned long long size = args->size;
    CliFlagsReadState *const read_state = (CliFlagsReadState *)args->context;
    if (read_state->awaiting != CLI_FLAGS_AWAITS_NONE)
    {
        const unsigned int awaited = read_state->awaiting;
        read_state->awaiting = CLI_FLAGS_AWAITS_NONE;
        if (awaited == CLI_FLAGS_AWAITS_DEFINE)
        {
            EMBED_CALL(cli_flags_define, CompileDbFlagsCtx, .read_state = read_state, .define = args->word,
                       .length = size);
        }
        else
        {
            EMBED_CALL(cli_flags_include, CompileDbFlagsCtx, .read_state = read_state, .named = args->word,
                       .length = size, .kind = read_state->awaited_kind);
        }
        return;
    }
    for (unsigned int at = 0u; at < (sizeof(s_flags) / sizeof(s_flags[0])); at += 1u)
    {
        const CliFlagWord *const flag = &s_flags[at];
        if ((size < flag->length) || !EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->word,
                                                 .other_pointer = flag->flag, .size = flag->length))
        {
            continue;
        }
        if (size == flag->length)
        {
            read_state->awaiting = (flag->kind == CLI_JSON_NONE) ? CLI_FLAGS_AWAITS_DEFINE : CLI_FLAGS_AWAITS_INCLUDE;
            read_state->awaited_kind = flag->kind;
        }
        else if (flag->kind == CLI_JSON_NONE)
        {
            EMBED_CALL(cli_flags_define, CompileDbFlagsCtx, .read_state = read_state,
                       .define = args->word + flag->length, .length = size - flag->length);
        }
        else
        {
            EMBED_CALL(cli_flags_include, CompileDbFlagsCtx, .read_state = read_state,
                       .named = args->word + flag->length, .length = size - flag->length, .kind = flag->kind);
        }
        return;
    }
}

// the path of the entry's file from the tree's top into `path`, its slashes made `/`, sized for CLI_PATH_MAX_LEN: 1,
// or 0 where the entry names no file inside the tree
EMBED_INLINE int cli_flags_file_get(const CompileDbFlagsCtx *args)
{
    const CliJson *json = args->json;
    const char *root = args->root;
    unsigned long long size = 0ull;
    const char *const file =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->entry, .name = "file"),
                   .length_pointer = &size);
    const char *const directory =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->entry, .name = "directory"),
                   .length_pointer = NULL);
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
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = directory, .relative = named, .joined = joined);
    }
    char top[CLI_PATH_MAX_LEN];
    const unsigned long long top_length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){.dst = top, .src = root, .bytes = (size_t)top_length + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = joined,
               .size = cellul.len(&(CatenaFinitaCfg){.src = joined, .cap = CLI_CELLS_POOL}), .from = '\\', .to = '/');
    for (unsigned long long at = 0ull; at < top_length; at += 1ull)
    {
        top[at] = (top[at] == '\\') ? '/' : top[at];
    }
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = joined, .other_pointer = top,
                    .size = top_length) ||
        (joined[top_length] != '/'))
    {
        return 0;
    }
    const unsigned long long length =
        cellul.len(&(CatenaFinitaCfg){.src = joined + top_length + 1u, .cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){.dst = args->path_out, .src = joined + top_length + 1u, .bytes = (size_t)length + 1u});
    return 1;
}

// whether the run of `node` holds the define `id`
EMBED_INLINE int cli_flags_holds(const CompileDbFlagsCtx *args)
{
    for (unsigned int at = args->node->first; at < (args->node->first + args->node->count); at += 1u)
    {
        if (args->read_state_read->id[at] == args->id)
        {
            return 1;
        }
    }
    return 0;
}

// the folder `path`, `length` bytes, made to hold the defines of `file` with those it had: each it had that `file`
// has saved, and where it is new, a copy of `file`'s; its place, or CLI_JSON_NONE where the cells cannot meet it
EMBED_INLINE unsigned int cli_flags_folder_intersect(const CompileDbFlagsCtx *args)
{
    for (unsigned int at = 0u; at < args->read_state->folders; at += 1u)
    {
        const char *const saved =
            EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = args->read_state->folder[at].path);
        if ((cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_CELLS_POOL}) == args->length) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = saved, .other_pointer = args->path,
                       .size = args->length))
        {
            CliDefineNode *const folder = &args->read_state->folder[at];
            unsigned int saved = 0u;
            for (unsigned int each = 0u; each < folder->count; each += 1u)
            {
                const unsigned int id = args->read_state->id[folder->first + each];
                if (EMBED_CALL(cli_flags_holds, CompileDbFlagsCtx, .read_state_read = args->read_state,
                               .node = args->file, .id = id))
                {
                    args->read_state->id[folder->first + saved] = id;
                    saved += 1u;
                }
            }
            folder->count = saved;
            return at;
        }
    }
    const unsigned int named =
        (args->read_state->folders < args->read_state->folder_capacity)
            ? EMBED_CALL(cli_flags_text_append, CompileDbFlagsCtx, .text = args->path, .length = args->length)
            : CLI_JSON_NONE;
    if (named == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    const unsigned int first = args->read_state->ids;
    for (unsigned int each = 0u; each < args->file->count; each += 1u)
    {
        EMBED_CALL(cli_flags_id_append, CompileDbFlagsCtx, .read_state = args->read_state, .first = first,
                   .id = args->read_state->id[args->file->first + each]);
    }
    args->read_state->folder[args->read_state->folders] =
        (CliDefineNode){named, first, args->read_state->ids - first, 0u, 0u};
    args->read_state->folders += 1u;
    return args->read_state->folders - 1u;
}

// the folder that holds the file or folder `path`, its place among those read, CLI_JSON_NONE for the top's own
EMBED_INLINE unsigned int cli_flags_parent(const CompileDbFlagsCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    if (length == 0ull)
    {
        return CLI_JSON_NONE;
    }
    unsigned long long cut = length;
    cut = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path, .at = cut,
                     .set = "/");
    const unsigned long long parent = (cut > 0ull) ? (cut - 1ull) : 0ull;
    for (unsigned int at = 0u; at < args->read_state_read->folders; at += 1u)
    {
        const char *const saved =
            EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = args->read_state_read->folder[at].path);
        if ((cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_CELLS_POOL}) == parent) &&
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = saved, .other_pointer = args->path,
                       .size = parent))
        {
            return at;
        }
    }
    return CLI_JSON_NONE;
}

// the defines of `node` its folder does not give it, given to it
EMBED_INLINE void cli_flags_input_append(const CompileDbFlagsCtx *args)
{
    const unsigned int parent =
        EMBED_CALL(cli_flags_parent, CompileDbFlagsCtx, .read_state_read = args->read_state,
                   .path = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = args->node->path));
    for (unsigned int each = 0u; each < args->node->count; each += 1u)
    {
        const unsigned int id = args->read_state->id[args->node->first + each];
        if ((parent != CLI_JSON_NONE) &&
            EMBED_CALL(cli_flags_holds, CompileDbFlagsCtx, .read_state_read = args->read_state,
                       .node = &args->read_state->folder[parent], .id = id))
        {
            continue;
        }
        if (s_saved.givens >= s_saved.input_capacity)
        {
            args->read_state->refused = 1;
            return;
        }
        s_saved.supplied[s_saved.givens] = (CliDefineNodeInput){args->node->path, id};
        s_saved.givens += 1u;
    }
}

// the entries of the database read into the saved files, each with its defines and include folders and files, under
// the lock; an entry of a file read before passed over
EMBED_INLINE void cli_flags_entries(const CompileDbFlagsCtx *args)
{
    const CliJson *json = args->json;
    for (unsigned int item = 0u;
         !args->read_state->refused &&
         (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                     .array = args->entries, .index = item) != CLI_JSON_NONE);
         item += 1u)
    {
        const unsigned int entry = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = args->entries, .index = item);
        char path[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_flags_file_get, CompileDbFlagsCtx, .json = json, .entry = entry,
                        .root = args->read_state->root, .path_out = path))
        {
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        int seen = 0;
        for (unsigned int at = 0u; !seen && (at < s_saved.files); at += 1u)
        {
            seen = cellul.eq(
                &(CatenaFinitaCfg){.src = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.file[at].path),
                                   .other = path,
                                   .cap = CLI_CELLS_POOL});
        }
        const unsigned int named =
            (!seen && (s_saved.files < s_saved.file_capacity))
                ? EMBED_CALL(cli_flags_text_append, CompileDbFlagsCtx, .text = path, .length = length)
                : CLI_JSON_NONE;
        if (named == CLI_JSON_NONE)
        {
            continue;
        }
        s_saved.file[s_saved.files] = (CliDefineNode){named, args->read_state->ids, 0u, s_saved.includes, 0u};
        args->read_state->awaiting = CLI_FLAGS_AWAITS_NONE;
        args->read_state->directory =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = entry, .name = "directory"),
                       .length_pointer = NULL);
        const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = json, .object = entry, .name = "arguments");
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = arguments,
                       .kind = CFG_JSON_ARRAY))
        {
            for (unsigned int word = 0u;
                 EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                            .array = arguments, .index = word) != CLI_JSON_NONE;
                 word += 1u)
            {
                unsigned long long size = 0ull;
                const char *const text =
                    EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                               .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                  .json_read = json, .array = arguments, .index = word),
                               .length_pointer = &size);
                if (text != NULL)
                {
                    EMBED_CALL(cli_flags_word_read, CliEachWordSizeCfg, .context = args->read_state, .word = text,
                               .size = size);
                }
            }
        }
        else
        {
            unsigned long long size = 0ull;
            const char *const command =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = entry, .name = "command"),
                           .length_pointer = &size);
            if (command != NULL)
            {
                EMBED_CALL(cli_compile_db_text_internal.compile_words_each, CliCompileDbTextCtx, .text = command,
                           .length = size, .each = cli_flags_word_read, .context = args->read_state);
            }
        }
        s_saved.file[s_saved.files].count = args->read_state->ids - s_saved.file[s_saved.files].first;
        s_saved.file[s_saved.files].includes = s_saved.includes - s_saved.file[s_saved.files].include_first;
        s_saved.files += 1u;
    }
}

// each file's folders, from the one that holds it up to the top's own, made to hold the defines every file under
// each holds, and then the defines given: each folder's its folder does not give it, and each file's the same
EMBED_INLINE void cli_flags_lift(const CompileDbFlagsCtx *args)
{
    for (unsigned int at = 0u; !args->read_state->refused && (at < s_saved.files); at += 1u)
    {
        const CliDefineNode file = s_saved.file[at];
        // the file's path copied out of the saved texts, which move as the folders' paths are put after them
        char path[CLI_PATH_MAX_LEN];
        const char *const saved = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = file.path);
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = saved, .cap = CLI_CELLS_POOL});
        memor.cpy(&(MemoriaCfg){.dst = path, .src = saved, .bytes = (size_t)size + 1u});
        unsigned long long cut = size;
        int done = 0;
        while (!done)
        {
            cut = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = path, .at = cut,
                             .set = "/");
            const unsigned long long length = (cut > 0ull) ? (cut - 1ull) : 0ull;
            args->read_state->refused |=
                EMBED_CALL(cli_flags_folder_intersect, CompileDbFlagsCtx, .read_state = args->read_state, .path = path,
                           .length = length, .file = &file) == CLI_JSON_NONE;
            done = (cut == 0ull) || args->read_state->refused;
            cut = length;
        }
    }
    for (unsigned int at = 0u; !args->read_state->refused && (at < args->read_state->folders); at += 1u)
    {
        EMBED_CALL(cli_flags_input_append, CompileDbFlagsCtx, .read_state = args->read_state,
                   .node = &args->read_state->folder[at]);
    }
    for (unsigned int at = 0u; !args->read_state->refused && (at < s_saved.files); at += 1u)
    {
        EMBED_CALL(cli_flags_input_append, CompileDbFlagsCtx, .read_state = args->read_state,
                   .node = &s_saved.file[at]);
    }
}

// the saved flags let go, their cells given back, under the lock
static void cli_flags_let_go(void)
{
    g_cli_cells->persistent_buf_release(s_saved.word, &g_cli_compile_cells);
    s_saved.text = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    s_saved.word = NULL;
    s_saved.words = 0u;
    s_saved.word_capacity = 0u;
    s_saved.supplied = NULL;
    s_saved.givens = 0u;
    s_saved.input_capacity = 0u;
    s_saved.file = NULL;
    s_saved.files = 0u;
    s_saved.file_capacity = 0u;
    s_saved.include = NULL;
    s_saved.includes = 0u;
    s_saved.include_capacity = 0u;
}

// the database of the tree at `root` read into the saved flags where it or the tree is not the one read last, under
// the lock: the tree's own database, or the one gathered from its builds
static void cli_flags_read(const CompileDbFlagsCtx *args)
{
    const char *root = args->root;
    CliDatabase database;
    (void)EMBED_CALL(cli_compile_db_root_database.refresh, CliCompileDbRootDatabaseCfg, .root = root,
                     .database = &database);
    CliInputs inputs = {0, NULL, 0u, 0u};
    EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs, CliCompileDbInputsCtx, .root = root, .inputs = &inputs);
    char path[CLI_PATH_MAX_LEN];
    if (inputs.own)
    {
        memor.cpy(&(MemoriaCfg){.dst = path, .src = inputs.file[0].path, .bytes = CLI_PATH_MAX_LEN});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = CLI_COMPILE_WRITTEN, .joined = path);
    }
    EMBED_CALL(cli_compile_db_inputs_internal.compile_inputs_free, CliCompileDbInputsCtx, .inputs = &inputs);
    CliStamped stamped = {"", 0ull};
    const int there = EMBED_CALL(cli_compile_db_path_stamped_internal.compile_stamp, CliCompileDbPathStampedCtx,
                                 .path = path, .stamped = &stamped);
    const int same =
        cellul.eq(&(CatenaFinitaCfg){.src = s_saved.root, .other = root, .cap = CLI_CELLS_POOL}) &&
        cellul.eq(&(CatenaFinitaCfg){.src = s_saved.database.path, .other = stamped.path, .cap = CLI_CELLS_POOL}) &&
        (s_saved.database.time == stamped.time) && (s_saved.text.out != NULL);
    if (same)
    {
        return;
    }
    cli_flags_let_go();
    memor.cpy(&(MemoriaCfg){.dst = s_saved.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_saved.database = stamped;
    unsigned long long database_size = 0ull;
    if (!there || !EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = stamped.path,
                              .size_pointer = &database_size))
    {
        return;
    }
    // the flags' texts stand among the database's own bytes
    s_flags_entries = (database_size / CLI_FLAGS_ENTRY_BYTES) + CLI_FLAGS_ENTRIES_MORE;
    const unsigned long long text_capacity = database_size + CLI_FLAGS_TEXT_MORE;
    const unsigned int word_capacity = EMBED_CALL(flags_max, CompileDbFlagsCtx, .most = CLI_FLAGS_DISTINCT);
    const unsigned int input_capacity = EMBED_CALL(flags_max, CompileDbFlagsCtx, .most = CLI_FLAGS_INPUT);
    const unsigned int file_capacity = EMBED_CALL(flags_max, CompileDbFlagsCtx, .most = CLI_FLAGS_FILES);
    const unsigned int include_capacity = EMBED_CALL(flags_max, CompileDbFlagsCtx, .most = CLI_FLAGS_INCLUDES);
    const unsigned long long word_bytes = (unsigned long long)word_capacity * sizeof(CliDefineWord);
    const unsigned long long input_bytes = (unsigned long long)input_capacity * sizeof(CliDefineNodeInput);
    const unsigned long long file_bytes = (unsigned long long)file_capacity * sizeof(CliDefineNode);
    const unsigned long long include_bytes = (unsigned long long)include_capacity * sizeof(CliIncludeWord);
    char *const saved = g_cli_cells->persistent_buf_alloc(
        word_bytes + input_bytes + file_bytes + include_bytes + text_capacity, &g_cli_compile_cells);
    if (saved == NULL)
    {
        return;
    }
    s_saved.word = (CliDefineWord *)(void *)saved;
    s_saved.word_capacity = word_capacity;
    s_saved.supplied = (CliDefineNodeInput *)(void *)(saved + word_bytes);
    s_saved.input_capacity = input_capacity;
    s_saved.file = (CliDefineNode *)(void *)(saved + word_bytes + input_bytes);
    s_saved.file_capacity = file_capacity;
    s_saved.include = (CliIncludeWord *)(void *)(saved + word_bytes + input_bytes + file_bytes);
    s_saved.include_capacity = include_capacity;
    s_saved.text = (VerbaLinea){saved + word_bytes + input_bytes + file_bytes + include_bytes, text_capacity, 0ull};
    CliJson json;
    char unread_text[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {unread_text, sizeof(unread_text), 0ull};
    const unsigned int entries =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &json, .path = stamped.path,
                   .nodes_more = 0u, .text_more = 0ull, .err_msg = &unread, .owner = &g_cli_compile_cells);
    const unsigned int folder_capacity = EMBED_CALL(flags_max, CompileDbFlagsCtx, .most = CLI_FLAGS_FOLDERS);
    const unsigned int id_capacity = EMBED_CALL(flags_max, CompileDbFlagsCtx, .most = CLI_FLAGS_IDS);
    const unsigned long long folder_bytes = (unsigned long long)folder_capacity * sizeof(CliDefineNode);
    char *const work = g_cli_cells->temporary_buf_alloc(
        folder_bytes + ((unsigned long long)id_capacity * sizeof(unsigned int)), &g_cli_compile_cells);
    CliFlagsReadState read_state = {root, NULL, 0u, 0u, NULL, 0u, 0u, NULL, CLI_FLAGS_AWAITS_NONE, 0u, 0};
    if (work != NULL)
    {
        read_state.folder = (CliDefineNode *)(void *)work;
        read_state.folder_capacity = folder_capacity;
        read_state.id = (unsigned int *)(void *)(work + folder_bytes);
        read_state.id_capacity = id_capacity;
    }
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = entries,
                   .kind = CFG_JSON_ARRAY))
    {
        EMBED_CALL(cli_flags_entries, CompileDbFlagsCtx, .read_state = &read_state, .json = &json, .entries = entries);
        EMBED_CALL(cli_flags_lift, CompileDbFlagsCtx, .read_state = &read_state);
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    g_cli_cells->persistent_buf_release(work, &g_cli_compile_cells);
}

// the define of the `-D` word's `name`, `NAME` or `NAME(PARAMS)`, and `value`, given to `node`, handed to `each`: a
// name with brackets one called with arguments, its parameters the names between commas within them, white space
// about each left off, and a `...` among them taking the arguments past them
EMBED_INLINE void cli_flags_define_dispatch(const CompileDbFlagsCtx *args)
{
    char named[CLI_NAME_MAX_LEN];
    char params[CLI_NAME_MAX_LEN];
    CliDefineInput define = {named, 0, {NULL}, 0u, 0, args->value, args->node_text};
    unsigned long long at = 0ull;
    at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->name, .at = 0ull,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}), .set = "(");
    at = (at < sizeof(named)) ? at : (sizeof(named) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = named, .src = args->name, .bytes = (size_t)at});
    named[at] = '\0';
    define.called = args->name[at] == '(';
    unsigned long long put = 0ull;
    const char *from = define.called ? (args->name + at + 1u) : "";
    while ((*from != '\0') && (*from != ')') && (define.params < CLI_DEFINE_PARAMS))
    {
        from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), .set = " \t");
        const char *to = from;
        to += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = to, .at = 0ull,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = to, .cap = CLI_CELLS_POOL}), .set = ",) \t");
        // a parameter's name lies between its separators, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        const int rest = (length == 3ull) && EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = from,
                                                        .other_pointer = "...", .size = 3ull);
        if ((length != 0ull) && !rest && ((put + length + 1u) < sizeof(params)))
        {
            memor.cpy(&(MemoriaCfg){.dst = params + put, .src = from, .bytes = (size_t)length});
            params[put + length] = '\0';
            define.param[define.params] = params + put;
            define.params += 1u;
            put += length + 1u;
        }
        define.rest |= rest;
        to += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = to, .at = 0ull,
                         .size = cellul.len(&(CatenaFinitaCfg){.src = to, .cap = CLI_CELLS_POOL}), .set = " \t");
        from = (*to == ',') ? (to + 1) : to;
    }
    EMBED_CALL(args->each, CliDefineEachCfg, .context = args->context, .define = &define);
}

void cli_compile_db_defines_get(const CliCompileDbRootCfg *args)
{
    EMBED_CALL(cli_flags_read, CompileDbFlagsCtx, .root = args->root);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    // the top's own, then each folder's down to the branch's own, each node a cut of the path at a slash or its end
    unsigned long long cut = 0ull;
    for (int done = 0; !done && (s_saved.text.out != NULL);)
    {
        for (unsigned int at = 0u; at < s_saved.givens; at += 1u)
        {
            const char *const node = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.supplied[at].node);
            if ((cellul.len(&(CatenaFinitaCfg){.src = node, .cap = CLI_CELLS_POOL}) == cut) &&
                EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = node, .other_pointer = args->path,
                           .size = cut))
            {
                const CliDefineWord *const word = &s_saved.word[s_saved.supplied[at].define];
                EMBED_CALL(cli_flags_define_dispatch, CompileDbFlagsCtx,
                           .name = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = word->name),
                           .value = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = word->value), .node_text = node,
                           .each = args->each_define_each, .context = args->context);
            }
        }
        done = cut >= length;
        cut += (cut == 0ull) ? 0ull : 1ull;
        cut = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->path, .at = cut,
                         .size = length, .set = "/");
    }
}

void cli_compile_db_includes_all(const CliCompileDbRootPathCfg *args)
{
    EMBED_CALL(cli_flags_read, CompileDbFlagsCtx, .root = args->root);
    for (unsigned int at = 0u; (s_saved.text.out != NULL) && (at < s_saved.includes); at += 1u)
    {
        const char *const path = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.include[at].path);
        int before = 0;
        for (unsigned int earlier = 0u; !before && (earlier < at); earlier += 1u)
        {
            before = cellul.eq(&(CatenaFinitaCfg){
                .src = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.include[earlier].path),
                .other = path,
                .cap = CLI_CELLS_POOL});
        }
        if (!before)
        {
            EMBED_CALL(args->each, CliEachPathKindCfg, .context = args->context, .path = path,
                       .kind = s_saved.include[at].kind);
        }
    }
}

// how many bytes of folders the paths `one` and `other` share, to the last slash both have at the same place
EMBED_INLINE unsigned long long cli_flags_share(const CompileDbFlagsCtx *args)
{
    const char *other = args->other;
    unsigned long long shared = 0ull;
    shared = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->one,
                        .at = cellul.diff(&(CatenaFinitaCfg){.src = args->one, .other = other, .cap = CLI_CELLS_POOL}),
                        .set = "/");
    return shared;
}

void cli_compile_db_includes_get(const CliCompileDbRootPathCfg *args)
{
    static const unsigned int s_order[5] = {CLI_INCLUDE_QUOTE, CLI_INCLUDE_FOLDER, CLI_INCLUDE_SYSTEM,
                                            CLI_INCLUDE_AFTER, CLI_INCLUDE_FORCED};
    EMBED_CALL(cli_flags_read, CompileDbFlagsCtx, .root = args->root);
    // the file's own entry, else the entry sharing the most of its folders, the first of those that share as many
    unsigned int nearest = CLI_JSON_NONE;
    unsigned long long most = 0ull;
    for (unsigned int at = 0u; (s_saved.text.out != NULL) && (at < s_saved.files); at += 1u)
    {
        const char *const file = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.file[at].path);
        if (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = args->path, .cap = CLI_CELLS_POOL}))
        {
            nearest = at;
            break;
        }
        const unsigned long long shared =
            EMBED_CALL(cli_flags_share, CompileDbFlagsCtx, .one = file, .other = args->path);
        if ((nearest == CLI_JSON_NONE) || (shared > most))
        {
            nearest = at;
            most = shared;
        }
    }
    for (unsigned int kind = 0u; (nearest != CLI_JSON_NONE) && (kind < 5u); kind += 1u)
    {
        const CliDefineNode *const file = &s_saved.file[nearest];
        for (unsigned int at = file->include_first; at < (file->include_first + file->includes); at += 1u)
        {
            if (s_saved.include[at].kind == s_order[kind])
            {
                EMBED_CALL(args->each, CliEachPathKindCfg, .context = args->context,
                           .path = EMBED_CALL(cli_flags_text, CompileDbFlagsCtx, .at = s_saved.include[at].path),
                           .kind = s_order[kind]);
            }
        }
    }
}
