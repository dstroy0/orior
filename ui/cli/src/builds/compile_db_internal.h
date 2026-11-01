// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_internal.h: what the compilation database sources share: the files a look reads, the folders a search
// below a build finds, a command line's words, and a JSON file read into a tree
#ifndef COMPILE_DB_INTERNAL_H
#define COMPILE_DB_INTERNAL_H

#include "../text/json_tree.h"
#include "../text/reply.h"
#include "compile_db.h"

// how deep below build/cmake and build/meson a build is looked for; the most files a look reads; the most folders a
// search below a build finds; and the files and folders their rooms are taken with
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
// most, given back with cli_compile_inputs_given
typedef struct
{
    int own;
    CliStamped *file;
    unsigned int count;
    unsigned int room;
} CliInputs;

// Folders a search below a build finds, in a cell at their most, given back with cli_compile_found_given
typedef struct
{
    char (*path)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int room;
} CliFoundFolders;

// the cells of what a look read, and of the folders a search found, given back, each left empty; and the folders' cell
// taken at their most, the list with no room where it is refused (compile_db.c)
void cli_compile_inputs_given(CliInputs *inputs);
void cli_compile_found_given(CliFoundFolders *found);
void cli_compile_found_taken(CliFoundFolders *found);

// the file at `path` with when it last changed into `stamped`: 1, or 0 where it is not there; each folder below `top`,
// at most CLI_COMPILE_DEPTH down, that ends in the folders `tail`, `count` of them, into `found`; each file named
// `name` below `top`, the same; and the newest file of a CMake reply folder whose name starts `index-` into `index`:
// 1, or 0 where it contains none (compile_db.c)
int cli_compile_stamped(const char *path, CliStamped *stamped);
void cli_compile_folders_named(const char *top, const char *const *tail, unsigned int count, CliFoundFolders *found);
void cli_compile_files_named(const char *top, const char *name, CliFoundFolders *found);
int cli_compile_newest_index(const char *reply, char *index);

// whether a letter is whitespace as a command line is read; the words of a command line, a word in double quotes
// retained whole, the first into `first`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it has none; and each of them
// handed to `each` (compile_db.c)
int cli_compile_white(char letter);
int cli_compile_first_word(const char *text, unsigned long long length, char *first);
void cli_compile_words_each(const char *text, unsigned long long length,
                            void (*each)(void *context, const char *word, unsigned long long size), void *context);

// the JSON file at `path` read into `json`, a tree, its tokens a cell of their count given back as the read ends: its
// root, or CLI_JSON_NONE where it is not there or is no JSON (compile_db.c)
unsigned int cli_compile_json_read(CliJson *json, const char *path);
// what a look at the tree at `root` reads into `inputs` (compile_db_look.c)
void cli_compile_inputs(const char *root, CliInputs *inputs);

// the nodes and bytes the entries gathered are taken with, the most projects, languages and flags of one compile group,
// and the bytes the database and a compile group's flags are written in
#define CLI_GATHER_NODES_ROOM (1u << 18u)
#define CLI_GATHER_TEXT_ROOM (16ull << 20u)
#define CLI_GATHER_PROJECTS 256u
#define CLI_GATHER_LANGUAGES 16u
#define CLI_GATHER_FLAGS 4096u
#define CLI_GATHER_FLAGS_ROOM (1ull << 20u)

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
unsigned int cli_compile_cmake_entries(const char *index, char *source);

// the database `inputs` make into `database`: the tree's own, read for its compilers, or the builds' written together,
// a project configured more than once taking its configuration configured last (compile_db_gather.c)
void cli_compile_gather(const char *root, const CliInputs *inputs, CliDatabase *database);

#endif
