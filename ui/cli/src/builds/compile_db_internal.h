// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_internal.h: what the compilation database sources share: the files a look reads, the folders a search
// below a build finds, a command line's words, and a JSON file read into a tree
#ifndef COMPILE_DB_INTERNAL_H
#define COMPILE_DB_INTERNAL_H

#include "../text/json_tree.h"
#include "compile_db.h"

// how deep below build/cmake and build/meson a build is looked for; the most files a look reads; and the most folders
// a search below a build finds
#define CLI_COMPILE_DEPTH 6u
#define CLI_COMPILE_INPUTS 1024u
#define CLI_COMPILE_FOUND 1024u

// A file a look reads, and when it last changed, in milliseconds since 1970
typedef struct
{
    char path[CLI_PATH_LONGEST];
    unsigned long long time;
} CliStamped;

// What a look reads: the tree's own database, where it has one, or each build orior configured
typedef struct
{
    int own;
    CliStamped file[CLI_COMPILE_INPUTS];
    unsigned int count;
} CliInputs;

// Folders a search below a build finds
typedef struct
{
    char path[CLI_COMPILE_FOUND][CLI_PATH_LONGEST];
    unsigned int count;
} CliFoundFolders;

// the file at `path` with when it last changed into `stamped`: 1, or 0 where it is not there; each folder below `top`,
// at most CLI_COMPILE_DEPTH down, that ends in the folders `tail`, `count` of them, into `found`; each file named
// `name` below `top`, the same; and the newest file of a CMake reply folder whose name starts `index-` into `index`:
// 1, or 0 where it holds none (compile_db.c)
int cli_compile_stamped(const char *path, CliStamped *stamped);
void cli_compile_folders_named(const char *top, const char *const *tail, unsigned int count, CliFoundFolders *found);
void cli_compile_files_named(const char *top, const char *name, CliFoundFolders *found);
int cli_compile_newest_index(const char *reply, char *index);

// the words of a command line, a word in double quotes kept whole, the first into `first`, which holds
// CLI_PATH_LONGEST: 1, or 0 where it has none; and each of them handed to `each` (compile_db.c)
int cli_compile_first_word(const char *text, unsigned long long length, char *first);
void cli_compile_words_each(const char *text, unsigned long long length,
                            void (*each)(void *context, const char *word, unsigned long long size), void *context);

// the JSON file at `path` read into `json`, `tokens` holding `tokens_most`: its root, or CLI_JSON_NONE where it is not
// there or is no JSON (compile_db.c)
unsigned int cli_compile_json_read(CliJson *json, const char *path, CfgJsonToken *tokens, unsigned int tokens_most);
// what a look at the tree at `root` reads into `inputs` (compile_db.c)
void cli_compile_inputs(const char *root, CliInputs *inputs);

// the most nodes and bytes of the entries gathered and of a CMake reply read, the most projects, languages and flags
// of one compile group, and the most bytes the database is written in
#define CLI_GATHER_NODES (1u << 20u)
#define CLI_GATHER_TEXT (1u << 25u)
#define CLI_REPLY_NODES (1u << 19u)
#define CLI_REPLY_TEXT (1u << 24u)
#define CLI_GATHER_PROJECTS 256u
#define CLI_GATHER_LANGUAGES 16u
#define CLI_GATHER_FLAGS 4096u
#define CLI_GATHER_WRITTEN (1u << 26u)

// A project's entries: its folder, when its configuration was written, and its entries in the gathered tree
typedef struct
{
    char project[CLI_PATH_LONGEST];
    unsigned long long time;
    unsigned int entries;
} CliProject;

// A language's compiler as a CMake build gives it: its path and its id
typedef struct
{
    const char *language;
    const char *path;
    const char *id;
} CliCompilerOf;

// The gathering: the entries tree and the reply tree with their tokens, the projects, a compile group's flags, and the
// database written; the trees' nodes, texts and tokens and the database written are cells of CLI_GATHER_NODES,
// CLI_GATHER_TEXT, CLI_REPLY_NODES, CLI_REPLY_TEXT and CLI_GATHER_WRITTEN, taken for one gathering and given back
// as it ends
typedef struct
{
    CliJsonNode *nodes;
    char *text;
    CliJson json;
    CfgJsonToken *tokens;
    CliJsonNode *reply_nodes;
    char *reply_text;
    CliJson reply;
    CliProject project[CLI_GATHER_PROJECTS];
    unsigned int projects;
    CliCompilerOf compiler[CLI_GATHER_LANGUAGES];
    unsigned int compilers;
    char flags_text[1u << 20u];
    unsigned int flags_used;
    unsigned int flag[CLI_GATHER_FLAGS];
    unsigned int flags;
    char *written;
} CliGathering;

extern CliGathering g_cli_gather;

// the entries of the CMake build whose file API reply `index` is, made in the gathered tree, and the project's source
// folder into `source`: the entries, or CLI_JSON_NONE where the reply does not read (compile_db_cmake.c)
unsigned int cli_compile_cmake_entries(const char *index, char *source);

// the database `inputs` make into `database`: the tree's own, read for its compilers, or the builds' written together,
// a project configured more than once taking its configuration configured last (compile_db_gather.c)
void cli_compile_gather(const char *root, const CliInputs *inputs, CliDatabase *database);

#endif
