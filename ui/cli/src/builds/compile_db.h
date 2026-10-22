// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db.h: the compilation database clangd reads: the build's own. The code help reads the compiler the build
// uses, with its include folders and its defines, and no file of settings is written by hand.
//
// A tree that holds its own database keeps it: a compile_commands.json at its top or in its build/ folder, or the
// folder a .clangd at its top names. Otherwise each CMake project orior_cli configured gives its targets' sources from
// CMake's file API, which answers for every generator, Visual Studio's among them, from the configuration configured
// last; and each Meson build gives the compile_commands.json Meson writes. They are written together to
// build/orior/compile_commands.json.
//
// clangd asks each compiler the database names, but MSVC's cl, for its own headers and target (`--query-driver`);
// where one is cl, clangd runs in Visual Studio's environment, as cl does. The database is looked for again at most
// every two seconds, and a clangd running when it changes is started again on it (compile_db.c, compile_db_look.c,
// compile_db_gather.c, compile_db_program.c)
#ifndef COMPILE_DB_H
#define COMPILE_DB_H

#include "../command_line/cli_internal.h"

// where the database gathered from the tree's builds is written, from the tree's top; and the most compilers one
// database names
#define CLI_COMPILE_WRITTEN "build/orior/compile_commands.json"
#define CLI_COMPILERS 64u

// The database clangd reads: the folder to give it where it is not one clangd finds itself, the compilers it names,
// in order, each once, and whether one of them is MSVC's cl
typedef struct
{
    char folder[CLI_PATH_LONGEST];
    int has_folder;
    char compiler[CLI_COMPILERS][CLI_PATH_LONGEST];
    unsigned int compilers;
    int msvc;
} CliDatabase;

// A program a CMake or Meson build orior_cli configured makes from a source: the build tool, cmake or meson, the
// build's folder, the target's name, the configuration, the program's path, and the source's compiler as GNU, Clang or
// MSVC
typedef struct
{
    char tool[16];
    char build[CLI_PATH_LONGEST];
    char target[CLI_NAME_LONGEST];
    char config[CLI_NAME_LONGEST];
    char program[CLI_PATH_LONGEST];
    char compiler[CLI_NAME_LONGEST];
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
// the compilers it may ask for their headers and target, every one but cl
int cli_compile_db_refresh(const char *root, CliDatabase *database);
int cli_compile_db_is_cl(const char *compiler);
void cli_compile_db_words(const CliDatabase *database, CliWords *words);

// the program a CMake or Meson build orior_cli configured makes from `file`, where one does, into `program`: from a
// configuration with debugging information, Debug or RelWithDebInfo, or Meson's debug or debugoptimized, where one is
// configured, and from the one configured last where not. 1, or 0 where none does (compile_db_program.c)
int cli_compile_db_program_of(const char *root, const char *file, CliProgram *program);

// Visual Studio's environment for a program that runs as cl does, read once, from the newest Visual Studio with its
// C++ tools that vswhere finds: NULL off Windows or where none is found
const CliVsEnvironment *cli_compile_db_vs_environment(void);

#endif
