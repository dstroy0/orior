// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db.h: the compilation database clangd reads: the build's own. The code help reads the compiler the build
// uses, with its include folders and its defines, and no file of settings is written by hand.
//
// A tree that has its own database uses it: a compile_commands.json at its top or in its build/ folder, or the
// folder a .clangd at its top names. Otherwise each CMake project orior_cli configured gives its targets' sources from
// CMake's file API, which answers for every generator, Visual Studio's among them, from the configuration configured
// last; and each Meson build gives the compile_commands.json Meson writes. They are written together to
// build/orior/compile_commands.json.
//
// clangd queries each compiler the database names, but MSVC's cl, for its own headers and target (`--query-driver`);
// where one is cl, clangd runs in Visual Studio's environment, as cl does. The database is looked for again at most
// every two seconds, and a clangd running when it changes is started again on it (compile_db.c, compile_db_look.c,
// compile_db_gather.c, compile_db_program.c)
#ifndef COMPILE_DB_H
#define COMPILE_DB_H
typedef struct CliCompileDbRootCfg CliCompileDbRootCfg;
typedef struct CliCompileDbRootPathCfg CliCompileDbRootPathCfg;
typedef struct CliCompileDbRootFileCfg CliCompileDbRootFileCfg;
typedef struct CliCompileDbRootDatabaseCfg CliCompileDbRootDatabaseCfg;
typedef struct CliCompileDbDatabaseCfg CliCompileDbDatabaseCfg;
typedef struct CliCompileDbCfg CliCompileDbCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// where the database gathered from the tree's builds is written, from the tree's top; and the most compilers one
// database names
#define CLI_COMPILE_WRITTEN "build/orior/compile_commands.json"
#define CLI_COMPILERS 64u

// The database clangd reads: the folder to give it where it is not one clangd finds itself, the compilers it names,
// in order, each once, and whether one of them is MSVC's cl
typedef struct
{
    char folder[CLI_PATH_MAX_LEN];
    int has_folder;
    char compiler[CLI_COMPILERS][CLI_PATH_MAX_LEN];
    unsigned int compilers;
    int msvc;
} CliDatabase;

// A program a CMake or Meson build orior_cli configured makes from a source: the build tool, cmake or meson, the
// build's folder, the target's name, the configuration, the program's path, and the source's compiler as GNU, Clang or
// MSVC
typedef struct
{
    char tool[16];
    char build[CLI_PATH_MAX_LEN];
    char target[CLI_NAME_MAX_LEN];
    char config[CLI_NAME_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    char compiler[CLI_NAME_MAX_LEN];
} CliProgram;

// Visual Studio's environment, as its vcvars64.bat sets it: each name and its value
typedef struct
{
    const char *name[1024];
    const char *value[1024];
    unsigned int count;
    char text[1u << 18u];
} CliVsEnvironment;

// the database of the tree at `root` into `database`, gathered and written where the tree's builds changed since the
// last look, at most every two seconds: 1 where it changed since the last look at this tree, else 0; whether a
// compiler is MSVC's cl; the words clangd starts with for a database put after `words`: the folder it is given and
// the compilers it may query for their headers and target, every one but cl
int cli_compile_db_refresh(const CliCompileDbRootDatabaseCfg *args);
int cli_compile_db_is_cl(const CliCompileDbCfg *args);
void cli_compile_db_words(const CliCompileDbDatabaseCfg *args);

// the program a CMake or Meson build orior_cli configured makes from `file`, where one does, into `program`: from a
// configuration with debugging information, Debug or RelWithDebInfo, or Meson's debug or debugoptimized, where one is
// configured, and from the one configured last where not. 1, or 0 where none does (compile_db_program.c)
int cli_compile_db_program_get(const CliCompileDbRootFileCfg *args);

// Visual Studio's environment for a program that runs as cl does, read once, from the newest Visual Studio with its
// C++ tools that vswhere finds: NULL off Windows or where none is found
const CliVsEnvironment *cli_compile_db_vs_environment(void);

// the most parameters of a define read
#define CLI_DEFINE_PARAMS 32u

// One define a branch inherits, as its `-D` word gives it: its name; whether it is called with arguments, and if so
// its parameters' names, how many, and whether it takes the arguments past them, a `-D` word's `...`; its value; and
// the node it is given to, its path from the tree's top, empty for the top's own; each text one that stands while it
// is handed on
typedef struct
{
    const char *name;
    int called;
    const char *param[CLI_DEFINE_PARAMS];
    unsigned int params;
    int rest;
    const char *value;
    const char *node;
} CliDefineGiven;

typedef struct
{
    void *context;
    const CliDefineGiven *define;
} CliDefineEachCfg;

// what each define a branch inherits is handed to
typedef void (*CliDefineEach)(const CliDefineEachCfg *args);
// each define the file or folder at `path`, from the top of the tree at `root`, inherits from the tree's compilation
// database handed to `each`: the top's, then each folder's down to its own, so a later one stands in for an earlier
// of the same name. The database's entries are read again each time its file changes: each entry's `-D` and `/D`
// words, a define every entry under a folder gives with the same value given to that folder, the highest such, and an
// entry's file saving those its folder does not give it (compile_db_flags.c)
void cli_compile_db_defines_get(const CliCompileDbRootCfg *args);

// the kinds of an include folder or file, in the order a compiler searches the folders: a folder searched for an
// include in quotes alone (`-iquote`), a folder (`-I`, `/I`), a system folder (`-isystem`), a folder searched last
// (`-idirafter`), and a file included before the first line (`-include`, `/FI`)
#define CLI_INCLUDE_QUOTE 0u
#define CLI_INCLUDE_FOLDER 1u
#define CLI_INCLUDE_SYSTEM 2u
#define CLI_INCLUDE_AFTER 3u
#define CLI_INCLUDE_FORCED 4u
// what each include folder or file is handed to: its whole path, its slashes `/`, a text that stands while `each`
// runs, and its kind
typedef void (*CliIncludeEach)(const CliEachPathKindCfg *args);
// each include folder and file of the file at `path`, from the top of the tree at `root`, from the tree's compilation
// database handed to `each`, the folders in the order a compiler searches them and then the files included before its
// first line: its own entry's, or a file no entry names takes those of the entry sharing the most of its folders
// (compile_db_flags.c)
void cli_compile_db_includes_get(const CliCompileDbRootPathCfg *args);
// every include folder and file the entries of the tree's compilation database name handed to `each`, each once
// (compile_db_flags.c)
void cli_compile_db_includes_all(const CliCompileDbRootPathCfg *args);

struct CliCompileDbRootCfg
{
    const char *root;
    const char *path;
    CliDefineEach each_define_each;
    void *context;
};

struct CliCompileDbRootPathCfg
{
    const char *root;
    const char *path;
    void *context;
    CliIncludeEach each;
};

struct CliCompileDbRootFileCfg
{
    const char *root;
    const char *file;
    CliProgram *program;
};

struct CliCompileDbRootDatabaseCfg
{
    const char *root;
    CliDatabase *database;
};

struct CliCompileDbDatabaseCfg
{
    const CliDatabase *database_read;
    CliWords *words;
};

struct CliCompileDbCfg
{
    const char *compiler;
};

typedef struct
{
    int (*refresh)(const CliCompileDbRootDatabaseCfg *args);
} CliCompileDbRootDatabaseNs;
EMBED_TABLE_LAYOUT(CliCompileDbRootDatabaseNs, refresh);
EMBED_TABLE_STORAGE CliCompileDbRootDatabaseNs cli_compile_db_root_database EMBED_UNUSED = {
    .refresh = cli_compile_db_refresh,
};

typedef struct
{
    int (*is_cl)(const CliCompileDbCfg *args);
} CliCompileDbNs;
EMBED_TABLE_LAYOUT(CliCompileDbNs, is_cl);
EMBED_TABLE_STORAGE CliCompileDbNs cli_compile_db_table EMBED_UNUSED = {
    .is_cl = cli_compile_db_is_cl,
};

typedef struct
{
    void (*words)(const CliCompileDbDatabaseCfg *args);
} CliCompileDbDatabaseNs;
EMBED_TABLE_LAYOUT(CliCompileDbDatabaseNs, words);
EMBED_TABLE_STORAGE CliCompileDbDatabaseNs cli_compile_db_database EMBED_UNUSED = {
    .words = cli_compile_db_words,
};

typedef struct
{
    int (*program_get)(const CliCompileDbRootFileCfg *args);
} CliCompileDbRootFileNs;
EMBED_TABLE_LAYOUT(CliCompileDbRootFileNs, program_get);
EMBED_TABLE_STORAGE CliCompileDbRootFileNs cli_compile_db_root_file EMBED_UNUSED = {
    .program_get = cli_compile_db_program_get,
};

typedef struct
{
    void (*defines_get)(const CliCompileDbRootCfg *args);
} CliCompileDbRootNs;
EMBED_TABLE_LAYOUT(CliCompileDbRootNs, defines_get);
EMBED_TABLE_STORAGE CliCompileDbRootNs cli_compile_db_root EMBED_UNUSED = {
    .defines_get = cli_compile_db_defines_get,
};

typedef struct
{
    void (*includes_get)(const CliCompileDbRootPathCfg *args);
    void (*includes_all)(const CliCompileDbRootPathCfg *args);
} CliCompileDbRootPathNs;
EMBED_TABLE_LAYOUT(CliCompileDbRootPathNs, includes_get, includes_all);
EMBED_TABLE_STORAGE CliCompileDbRootPathNs cli_compile_db_root_path EMBED_UNUSED = {
    .includes_get = cli_compile_db_includes_get,
    .includes_all = cli_compile_db_includes_all,
};

#endif
