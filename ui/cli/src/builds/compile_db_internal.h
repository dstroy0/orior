// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_internal.h: what the compilation database sources share: the files a look reads, the folders a search
// below a build finds, a command line's words, and a JSON file read into a tree
#ifndef COMPILE_DB_INTERNAL_H
#define COMPILE_DB_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "../text/reply.h"
#include "compile_db.h"
#include "embed_dispatch_layout.h"

typedef struct CliCompileDbFoundCtx CliCompileDbFoundCtx;
typedef struct CliCompileDbTextCtx CliCompileDbTextCtx;
typedef struct CliCompileDbFoundTopCtx CliCompileDbFoundTopCtx;
typedef struct CliCompileDbTextLengthCtx CliCompileDbTextLengthCtx;
typedef struct CliCompileDbRootCtx CliCompileDbRootCtx;
typedef struct CliCompileDbIndexCtx CliCompileDbIndexCtx;
typedef struct CliCompileDbInputsCtx CliCompileDbInputsCtx;
typedef struct CliCompileDbPathCtx CliCompileDbPathCtx;
typedef struct CliCompileDbReplyCtx CliCompileDbReplyCtx;
typedef struct CliCompileDbPathStampedCtx CliCompileDbPathStampedCtx;
typedef struct CliCompileDbCtx CliCompileDbCtx;

// how deep below build/cmake and build/meson a build is looked for; the most files a look reads; the most folders a
// search below a build finds; and the files and folders their capacities are taken with
#define CLI_COMPILE_DEPTH 6u
#define CLI_COMPILE_INPUTS 1024u
#define CLI_COMPILE_FOUND 1024u

// the cells the files a look reads, the folders a search finds and the folders a walk has still to look in are
// allocated in, counted (compile_db.c)
extern CellOwner g_cli_compile_cells;

// A file a look reads, and when it last changed, in milliseconds since 1970
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long time;
} CliStamped;

// What a look reads: the tree's own database, where it has one, or each build orior_cli configured, in a cell at their
// most, given back with cli_compile_inputs_free
typedef struct
{
    int own;
    CliStamped *file;
    unsigned int count;
    unsigned int capacity;
} CliInputs;

// Folders a search below a build finds, in a cell at their most, given back with cli_compile_found_free
typedef struct
{
    char (*path)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliFoundFolders;

// the cells of what a look read, and of the folders a search found, given back, each left empty; and the folders' cell
// taken at their most, the list with no capacity where it is refused (compile_db.c)
void cli_compile_inputs_free(const CliCompileDbInputsCtx *args);
void cli_compile_found_free(const CliCompileDbFoundTopCtx *args);
void cli_compile_found_alloc(const CliCompileDbFoundTopCtx *args);

// the file at `path` with when it last changed into `stamped`: 1, or 0 where it is not there; each folder below `top`,
// at most CLI_COMPILE_DEPTH down, that ends in the folders `tail`, `count` of them, into `found`; each file named
// `name` below `top`, the same; and the newest file of a CMake reply folder whose name starts `index-` into `index`:
// 1, or 0 where it contains none (compile_db.c)
int cli_compile_stamp(const CliCompileDbPathStampedCtx *args);
void cli_compile_folders_by_name(const CliCompileDbFoundCtx *args);
void cli_compile_files_by_name(const CliCompileDbFoundTopCtx *args);
int cli_compile_newest_index(const CliCompileDbReplyCtx *args);

// whether a letter is whitespace as a command line is read; the words of a command line, a word in double quotes
// retained whole, the first into `first`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it has none; and each of them
// handed to `each` (compile_db.c)
int cli_compile_white(const CliCompileDbCtx *args);
int cli_compile_first_word(const CliCompileDbTextLengthCtx *args);
void cli_compile_words_each(const CliCompileDbTextCtx *args);

// the JSON file at `path` read into `json`, a tree, its tokens a cell of their count given back as the read ends: its
// root, or CLI_JSON_NONE where it is not there or is no JSON (compile_db.c)
unsigned int cli_compile_json_read(const CliCompileDbPathCtx *args);
// what a look at the tree at `root` reads into `inputs` (compile_db_look.c)
void cli_compile_inputs(const CliCompileDbInputsCtx *args);

// the nodes and bytes the entries gathered are taken with, the most projects, languages and flags of one compile group,
// and the bytes the database and a compile group's flags are written in
#define CLI_GATHER_NODES_CAPACITY (1u << 18u)
#define CLI_GATHER_TEXT_CAPACITY (16ull << 20u)
#define CLI_GATHER_PROJECTS 256u
#define CLI_GATHER_LANGUAGES 16u
#define CLI_GATHER_FLAGS 4096u
#define CLI_GATHER_FLAGS_CAPACITY (1ull << 20u)

// A project's entries: its folder, when its configuration was written, and its entries in the gathered tree
typedef struct
{
    char project[CLI_PATH_MAX_LEN];
    unsigned long long time;
    unsigned int entries;
} CliProject;

// A language's compiler as a CMake build gives it: its language, its path and its id
typedef struct
{
    char language[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
} CliCompilerOf;

// The gathering: the entries tree, in a cell of its own, the projects, a compile group's flags, their text a line, each
// one after another with its zero after it, and a fragment of a compile command decoded, a line, taken for one
// gathering and given back as it ends
typedef struct
{
    CliJson json;
    CliProject project[CLI_GATHER_PROJECTS];
    unsigned int projects;
    CliCompilerOf compiler[CLI_GATHER_LANGUAGES];
    unsigned int compilers;
    VerbaLinea flags_text;
    unsigned int flag[CLI_GATHER_FLAGS];
    unsigned int flags;
    VerbaLinea fragment_text;
} CliGathering;

extern CliGathering g_cli_gather;

// the entries of the CMake build whose file API reply `index` is, made in the gathered tree, and the project's source
// folder into `source`: the entries, or CLI_JSON_NONE where the reply does not read (compile_db_cmake.c)
unsigned int cli_compile_cmake_entries(const CliCompileDbIndexCtx *args);

// the database `inputs` make into `database`: the tree's own, read for its compilers, or the builds' written together,
// a project configured more than once taking its configuration configured last (compile_db_gather.c)
void cli_compile_gather(const CliCompileDbRootCtx *args);

struct CliCompileDbFoundCtx
{
    CliFoundFolders *found;
    const char *top;
    const char *const *tail;
    unsigned int count;
};

struct CliCompileDbTextCtx
{
    const char *text;
    unsigned long long length;
    void (*each)(const CliEachWordSizeCfg *args);
    void *context;
};

struct CliCompileDbFoundTopCtx
{
    CliFoundFolders *found;
    const char *top;
    const char *name;
};

struct CliCompileDbTextLengthCtx
{
    const char *text;
    unsigned long long length;
    char *first;
};

struct CliCompileDbRootCtx
{
    const char *root;
    const CliInputs *inputs_read;
    CliDatabase *database;
};

struct CliCompileDbIndexCtx
{
    const char *index_read;
    char *source;
};

struct CliCompileDbInputsCtx
{
    CliInputs *inputs;
    const char *root;
};

struct CliCompileDbPathCtx
{
    const char *path;
    CliJson *json;
};

struct CliCompileDbReplyCtx
{
    const char *reply;
    char *index;
};

struct CliCompileDbPathStampedCtx
{
    const char *path;
    CliStamped *stamped;
};

struct CliCompileDbCtx
{
    char letter;
};

typedef struct
{
    void (*compile_inputs_free)(const CliCompileDbInputsCtx *args);
    void (*compile_inputs)(const CliCompileDbInputsCtx *args);
} CliCompileDbInputsNs;
EMBED_TABLE_LAYOUT(CliCompileDbInputsNs, compile_inputs_free, compile_inputs);
EMBED_TABLE_STORAGE CliCompileDbInputsNs cli_compile_db_inputs_internal EMBED_UNUSED = {
    .compile_inputs_free = cli_compile_inputs_free,
    .compile_inputs = cli_compile_inputs,
};

typedef struct
{
    void (*compile_found_free)(const CliCompileDbFoundTopCtx *args);
    void (*compile_found_alloc)(const CliCompileDbFoundTopCtx *args);
    void (*compile_files_by_name)(const CliCompileDbFoundTopCtx *args);
} CliCompileDbFoundTopNs;
EMBED_TABLE_LAYOUT(CliCompileDbFoundTopNs, compile_found_free, compile_found_alloc, compile_files_by_name);
EMBED_TABLE_STORAGE CliCompileDbFoundTopNs cli_compile_db_found_top_internal EMBED_UNUSED = {
    .compile_found_free = cli_compile_found_free,
    .compile_found_alloc = cli_compile_found_alloc,
    .compile_files_by_name = cli_compile_files_by_name,
};

typedef struct
{
    int (*compile_stamp)(const CliCompileDbPathStampedCtx *args);
} CliCompileDbPathStampedNs;
EMBED_TABLE_LAYOUT(CliCompileDbPathStampedNs, compile_stamp);
EMBED_TABLE_STORAGE CliCompileDbPathStampedNs cli_compile_db_path_stamped_internal EMBED_UNUSED = {
    .compile_stamp = cli_compile_stamp,
};

typedef struct
{
    void (*compile_folders_by_name)(const CliCompileDbFoundCtx *args);
} CliCompileDbFoundNs;
EMBED_TABLE_LAYOUT(CliCompileDbFoundNs, compile_folders_by_name);
EMBED_TABLE_STORAGE CliCompileDbFoundNs cli_compile_db_found_internal EMBED_UNUSED = {
    .compile_folders_by_name = cli_compile_folders_by_name,
};

typedef struct
{
    int (*compile_newest_index)(const CliCompileDbReplyCtx *args);
} CliCompileDbReplyNs;
EMBED_TABLE_LAYOUT(CliCompileDbReplyNs, compile_newest_index);
EMBED_TABLE_STORAGE CliCompileDbReplyNs cli_compile_db_reply_internal EMBED_UNUSED = {
    .compile_newest_index = cli_compile_newest_index,
};

typedef struct
{
    int (*compile_white)(const CliCompileDbCtx *args);
} CliCompileDbNsTable;
EMBED_TABLE_LAYOUT(CliCompileDbNsTable, compile_white);
EMBED_TABLE_STORAGE CliCompileDbNsTable cli_compile_db_internal EMBED_UNUSED = {
    .compile_white = cli_compile_white,
};

typedef struct
{
    int (*compile_first_word)(const CliCompileDbTextLengthCtx *args);
} CliCompileDbTextLengthNs;
EMBED_TABLE_LAYOUT(CliCompileDbTextLengthNs, compile_first_word);
EMBED_TABLE_STORAGE CliCompileDbTextLengthNs cli_compile_db_text_length_internal EMBED_UNUSED = {
    .compile_first_word = cli_compile_first_word,
};

typedef struct
{
    void (*compile_words_each)(const CliCompileDbTextCtx *args);
} CliCompileDbTextNs;
EMBED_TABLE_LAYOUT(CliCompileDbTextNs, compile_words_each);
EMBED_TABLE_STORAGE CliCompileDbTextNs cli_compile_db_text_internal EMBED_UNUSED = {
    .compile_words_each = cli_compile_words_each,
};

typedef struct
{
    unsigned int (*compile_json_read)(const CliCompileDbPathCtx *args);
} CliCompileDbPathNs;
EMBED_TABLE_LAYOUT(CliCompileDbPathNs, compile_json_read);
EMBED_TABLE_STORAGE CliCompileDbPathNs cli_compile_db_path_internal EMBED_UNUSED = {
    .compile_json_read = cli_compile_json_read,
};

typedef struct
{
    unsigned int (*compile_cmake_entries)(const CliCompileDbIndexCtx *args);
} CliCompileDbIndexNs;
EMBED_TABLE_LAYOUT(CliCompileDbIndexNs, compile_cmake_entries);
EMBED_TABLE_STORAGE CliCompileDbIndexNs cli_compile_db_index_internal EMBED_UNUSED = {
    .compile_cmake_entries = cli_compile_cmake_entries,
};

typedef struct
{
    void (*compile_gather)(const CliCompileDbRootCtx *args);
} CliCompileDbRootNsTable;
EMBED_TABLE_LAYOUT(CliCompileDbRootNsTable, compile_gather);
EMBED_TABLE_STORAGE CliCompileDbRootNsTable cli_compile_db_root_internal EMBED_UNUSED = {
    .compile_gather = cli_compile_gather,
};

#endif
