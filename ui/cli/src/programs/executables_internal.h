// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_internal.h: what the executables sources share: the kinds of build script, a program one makes, the
// readers of each kind's text (executables_scan.c), the tree's scripts read (executables_read.c), and the facts of this
// machine a job is made from (executables_jobs.c)
#ifndef EXECUTABLES_INTERNAL_H
#define EXECUTABLES_INTERNAL_H

#include "executables.h"

// the kinds of build script read here, and none
enum
{
    CLI_KIND_CARGO,
    CLI_KIND_CMAKE,
    CLI_KIND_MESON,
    CLI_KIND_GO,
    CLI_KIND_DOTNET,
    CLI_KIND_ZIG,
    CLI_KIND_PYINSTALLER,
    CLI_KIND_NPM,
    CLI_KIND_NONE
};

// the most programs the tree's scripts make, the most names one script gives, the most bytes of a script read, and how
// deep in the tree's folders a build script is looked for
#define CLI_EXECUTABLES_MOST 1024u

#define CLI_EXECUTABLES_NAMES 256u

#define CLI_EXECUTABLES_BYTES (1u << 22u)

#define CLI_EXECUTABLES_DEEP 5u

// One program a build script makes: its kind, its name, the script's path, and what its kind needs beyond the name: a
// Go package's folder as `go build` takes it, the file a spec freezes by its path under the spec's folder, or a bin's
// script by its path in the tree, in `detail`; whether a build.zig takes the optimize option; and a bin's builders, the
// one taken unless told otherwise first, and whether its script is CommonJS
typedef struct
{
    unsigned int kind;
    char name[CLI_NAME_LONGEST];
    char script[CLI_PATH_LONGEST];
    char detail[CLI_PATH_LONGEST];
    int optimize;
    const char *with[3];
    unsigned int withs;
    int commonjs;
} CliExecutable;

// The programs found, and their places in the order of their scripts and then their names
typedef struct
{
    CliExecutable item[CLI_EXECUTABLES_MOST];
    unsigned int count;
    unsigned int order[CLI_EXECUTABLES_MOST];
} CliExecutables;

// Names a script gives, each with a second text where its kind has one: a Go main's package, a bin's script
typedef struct
{
    char name[CLI_EXECUTABLES_NAMES][CLI_NAME_LONGEST];
    char value[CLI_EXECUTABLES_NAMES][CLI_PATH_LONGEST];
    unsigned int count;
} CliNames;

// the kind of build script a file's name is: CLI_KIND_NONE where it is none
unsigned int cli_executables_kind_of(const char *name);
// a name put after the names, with its second text, where the names hold it not already
void cli_names_put(CliNames *names, const char *name, unsigned long long length, const char *value);

// whether a letter is ASCII's whitespace; `*length` bytes from `*text` trimmed of whitespace at either end; and
// `length` bytes of `text` into `out`, which holds `room` (executables_scan.c)
int cli_scan_white(char letter);
void cli_scan_trim(const char **text, unsigned long long *length);
void cli_scan_copied(const char *text, unsigned long long length, char *out, unsigned long long room);

// A call found in a script's code: the text inside its parentheses
typedef struct
{
    const char *text;
    unsigned long long length;
} CliCall;

// the readers of a script's text (executables_scan.c): a TOML value as text into `out`, which holds `room`; the
// binaries of the package a Cargo.toml at `manifest` describes; a script's text with its comments taken out into
// `out`, which holds `room`; the text inside each call of `command` in `code`, into `calls`, which holds `room`: their
// count; the names CMake's add_executable calls give and the name its project() gives, into `out`, which holds
// CLI_NAME_LONGEST: 1, or 0 where it gives none; the same of Meson; the package a Go file's clause names; the program a
// .NET project builds; the string a call's `key` is set to; the names a build.zig's addExecutable calls give; the
// program a PyInstaller spec freezes and its path under the spec's folder; and the bins of a package.json
void cli_toml_value(const char *value, unsigned long long length, char *out, unsigned long long room);
void cli_cargo_bins(const char *manifest, const char *text, CliNames *names);
void cli_uncommented(const char *text, char quote, char *out, unsigned long long room);
unsigned int cli_calls(const char *code, const char *command, CliCall *calls, unsigned int room);
void cli_cmake_targets(const char *text, const char *project, CliNames *names);
int cli_cmake_project(const char *text, char *out);
void cli_meson_targets(const char *text, CliNames *names);
int cli_meson_project(const char *text, char *out);
int cli_go_package(const char *text, char *out);
int cli_dotnet_program(const char *project, const char *text, char *out);
int cli_keyword(const char *call, unsigned long long length, const char *key, char *out);
void cli_zig_exes(const char *text, CliNames *names);
int cli_spec_made(const char *text, char *exe, char *path);
void cli_npm_bins(const char *text, CliNames *names, int *module);

// the most build scripts read, and the most folders still to look in
#define CLI_EXECUTABLES_SCRIPTS 4096u

#define CLI_EXECUTABLES_PENDING 4096u

// A walk of the tree's folders: those still to look in and their depths, the folder looked in, and what is found
typedef struct
{
    char pending[CLI_EXECUTABLES_PENDING][CLI_PATH_LONGEST];
    unsigned int depth[CLI_EXECUTABLES_PENDING];
    unsigned int pending_count;
    const char *folder;
    unsigned int at_depth;
    char found[CLI_EXECUTABLES_SCRIPTS][CLI_PATH_LONGEST];
    unsigned int found_count;
    int main;
} CliWalk;

// `name` joined to `folder` into `out`, which holds CLI_PATH_LONGEST: 1, or 0 where it does not fit; whether a folder
// is one a walk passes; the folders a walk has pending looked through in turn from `top`, each entry handed to
// `each`, a folder whose entries set `main` put after those found; and every program the tree's build scripts make,
// by script and then by name (executables_read.c)
int cli_executables_joined(const char *folder, const char *name, char *out);
int cli_executables_passed(const char *name);
void cli_executables_walked(CliWalk *walk, const char *top,
                            void (*each)(void *context, const char *name, int is_folder));
void cli_executables_read(const char *root, CliExecutables *found);

// the main packages of the Go module whose go.mod is `module` and whose text is `text`, each named with its folder as
// `go build` takes it, in order; and the programs that can build a bin whose script is `script`, in the folder of its
// package, into `one` (executables_go.c)
void cli_go_mains(const char *module, const char *text, CliNames *mains);
void cli_npm_builders(const char *folder, const char *script, CliExecutable *one);

// the program's file name on this system into `out`, which holds CLI_NAME_LONGEST; the script's folder in the tree,
// "." for the tree's own, into `out`, which holds CLI_PATH_LONGEST; the folder a CMake or Meson project's build of each
// configuration goes in, before the configuration's name; this machine's system and processor by Go's names; and the
// first value of `key`, or `fallback` where it has none (executables_jobs.c)
void cli_program_file(const char *name, char *out);
void cli_folder_of(const char *root, const char *script, char *out);
void cli_build_folder(const char *root, const char *script, const char *tool, char *out);
void cli_go_host(const char **system, const char **processor);
const char *cli_first(const CliValues *values, const char *key, const char *fallback);

// the job that builds `exe` begun in the catalog, titled `title`, with what it says of itself: 1, or 0 where the
// catalog is full; and its steps put on it, each with SOURCE_DATE_EPOCH set, then the program it made run as the job's
// `then` asks (executables_steps.c)
int cli_executables_job_begun(const char *root, const CliExecutable *exe, const char *title, CliCatalog *catalog);
void cli_executables_steps(const char *root, const CliExecutable *exe, CliCatalog *catalog);

// what the program `name` of the toolchain `tool` writes for `args`, `count` of them, run in `folder` where it is
// given, into `ran`: 1 where it ends well, or 0 with `name`, its first argument and what went wrong said
// (executables.c)
int cli_executables_output(const char *tool, const char *name, const char *const *args, unsigned int count,
                           const char *folder, CliRan *ran, CliLine *said);

// the program .NET's build made, from the last line it printed for the project `name`, into `out`, which holds
// CLI_PATH_LONGEST: 1, or 0 with why said (executables.c)
int cli_dotnet_artifact(const char *name, const CliLines *lines, char *out, CliLine *said);

// the file a CMake build in `build` made for target `name` in configuration `config`, as the reply of CMake's file
// API names it, and the file Meson's build in `build` made for executable `name`, as `meson introspect` names it, each
// into `out`, which holds CLI_PATH_LONGEST: 1, or 0 with why said (executables_cmake.c)
int cli_cmake_artifact(const char *build, const char *name, const char *config, char *out, CliLine *said);
int cli_meson_artifact(const char *build, const char *name, char *out, CliLine *said);

// the warnings file a PyInstaller build of the spec at `spec` wrote, read into `bytes`, which holds `room`, empty where
// there is none, with the spec's folder into `folder`, which holds CLI_PATH_LONGEST; and each module that file says
// PyInstaller did not find handed to `each` with the text of the modules that import it, `A (top-level), B (delayed)`
// (executables_advice.c)
void cli_pyinstaller_warn(const char *spec, char *folder, char *bytes, unsigned long long room);
void cli_pyinstaller_missing(const char *text,
                             void (*each)(void *context, const char *module, unsigned long long module_length,
                                          const char *importers, unsigned long long importers_length),
                             void *context);

// the text of the file at `path` read whole into `bytes`, which holds `room`, ended by a 0: 1, or 0 where it could not
// be read
int cli_text_read(const char *path, char *bytes, unsigned long long room);

#endif
