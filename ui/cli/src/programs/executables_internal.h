// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_internal.h: what the executables sources share: the types of build script, a program one makes, the
// readers of each type's text (executables_scan.c), the tree's scripts read (executables_read.c), and the facts of this
// machine a job is made from (executables_jobs.c)
#ifndef EXECUTABLES_INTERNAL_H
#define EXECUTABLES_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"
#include "executables.h"

typedef struct CliExecutablesNameCtx CliExecutablesNameCtx;
typedef struct CliExecutablesNameOutCtx CliExecutablesNameOutCtx;
typedef struct CliExecutablesOutCtx CliExecutablesOutCtx;
typedef struct CliExecutablesCodeCtx CliExecutablesCodeCtx;
typedef struct CliExecutablesNameOutErrMsgCtx CliExecutablesNameOutErrMsgCtx;
typedef struct CliExecutablesRootCtx CliExecutablesRootCtx;
typedef struct CliExecutablesLengthCtx CliExecutablesLengthCtx;
typedef struct CliExecutablesNameNamesCtx CliExecutablesNameNamesCtx;
typedef struct CliExecutablesLengthTextCtx CliExecutablesLengthTextCtx;
typedef struct CliExecutablesLengthValueCtx CliExecutablesLengthValueCtx;
typedef struct CliExecutablesTextCtx CliExecutablesTextCtx;
typedef struct CliExecutablesCodeCommandCtx CliExecutablesCodeCommandCtx;
typedef struct CliExecutablesNamesCtx CliExecutablesNamesCtx;
typedef struct CliExecutablesNamesTextCtx CliExecutablesNamesTextCtx;
typedef struct CliExecutablesTextOutCtx CliExecutablesTextOutCtx;
typedef struct CliExecutablesNameOutFolderCtx CliExecutablesNameOutFolderCtx;
typedef struct CliExecutablesWalkCtx CliExecutablesWalkCtx;
typedef struct CliExecutablesWalkTopCtx CliExecutablesWalkTopCtx;
typedef struct CliExecutablesKeyCtx CliExecutablesKeyCtx;
typedef struct CliExecutablesTextModuleCtx CliExecutablesTextModuleCtx;
typedef struct CliExecutablesNamesTextModuleCtx CliExecutablesNamesTextModuleCtx;
typedef struct CliExecutablesFolderCtx CliExecutablesFolderCtx;
typedef struct CliExecutablesTextEachCtx CliExecutablesTextEachCtx;
typedef struct CliExecutablesTextExeCtx CliExecutablesTextExeCtx;
typedef struct CliExecutablesCapacityCtx CliExecutablesCapacityCtx;
typedef struct CliExecutablesFoundCtx CliExecutablesFoundCtx;
typedef struct CliExecutablesSystemCtx CliExecutablesSystemCtx;
typedef struct CliExecutablesSpecCtx CliExecutablesSpecCtx;
typedef struct CliExecutablesTextLengthCtx CliExecutablesTextLengthCtx;
typedef struct CliExecutablesCtx CliExecutablesCtx;

// the types of build script read here, and none
enum
{
    CLI_TYPE_CARGO,
    CLI_TYPE_CMAKE,
    CLI_TYPE_MESON,
    CLI_TYPE_GO,
    CLI_TYPE_DOTNET,
    CLI_TYPE_ZIG,
    CLI_TYPE_PYINSTALLER,
    CLI_TYPE_NPM,
    CLI_TYPE_NONE
};

// the most programs the tree's scripts make, the most names one script gives, the most bytes of a script read, and how
// deep in the tree's folders a build script is looked for
#define CLI_EXECUTABLES_MAX 1024u

#define CLI_EXECUTABLES_NAMES 256u

#define CLI_EXECUTABLES_BYTES (1u << 22u)

#define CLI_EXECUTABLES_DEEP 5u

// One program a build script makes: its type, its name, the script's path, and what its type needs beyond the name: a
// Go package's folder as `go build` takes it, the file a spec freezes by its path under the spec's folder, or a bin's
// script by its path in the tree, in `detail`; whether a build.zig takes the optimize option; and a bin's builders, the
// one taken unless instructed otherwise first, and whether its script is CommonJS
typedef struct
{
    unsigned int type;
    char name[CLI_NAME_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char detail[CLI_PATH_MAX_LEN];
    int optimize;
    const char *with[3];
    unsigned int withs;
    int commonjs;
} CliExecutable;

// The programs found, `count` of them in a cell of `capacity` at their most, and their places in the order of their
// scripts and then their names, a cell of their count; both released through persistent_buf_release
typedef struct
{
    CliExecutable *item;
    unsigned int count;
    unsigned int capacity;
    unsigned int *order;
} CliExecutables;

// the programs' places put in the order of their scripts and then their names, the programs left where they are, none
// where the cellblock cannot meet them; and the programs' cells given back, the list containing none
// (executables_found.c)
void cli_executables_order(const CliExecutablesFoundCtx *args);

// Names a script gives, each with a second text where its type has one: a Go main's package, a bin's script
typedef struct
{
    char name[CLI_EXECUTABLES_NAMES][CLI_NAME_MAX_LEN];
    char value[CLI_EXECUTABLES_NAMES][CLI_PATH_MAX_LEN];
    unsigned int count;
} CliNames;

// the type of build script a file's name is: CLI_KIND_NONE where it is none
unsigned int cli_executables_type_get(const CliExecutablesNameOutFolderCtx *args);
// a name put after the names, with its second text, where the names contain it not already
void cli_names_append(const CliExecutablesNameNamesCtx *args);

// whether a letter is ASCII's whitespace; `*length` bytes from `*text` trimmed of whitespace at either end; and
// `length` bytes of `text` into `out`, sized for `capacity` (executables_scan.c)
int cli_scan_white(const CliExecutablesCtx *args);
void cli_scan_trim(const CliExecutablesTextLengthCtx *args);
void cli_scan_copy(const CliExecutablesLengthTextCtx *args);

// A call found in a script's code: the text inside its parentheses
typedef struct
{
    const char *text;
    unsigned long long length;
} CliCall;

// the readers of a script's text (executables_scan.c): a TOML value as text into `out`, sized for `capacity`; the
// binaries of the package a Cargo.toml at `manifest` describes; a script's text with its comments taken out into
// `out`, sized for `capacity`; the text inside each call of `command` in `code`, into `calls`, sized for `capacity`:
// their count, or, where `calls` is NULL, the count of them all; the names CMake's add_executable calls give and the
// name its project() gives, into `out`, sized for CLI_NAME_MAX_LEN: 1, or 0 where it gives none; the same of Meson; the
// package a Go file's clause names; the program a .NET project builds; the string a call's `key` is set to; the names a
// build.zig's addExecutable calls give; the program a PyInstaller spec freezes and its path under the spec's folder;
// and the bins of a package.json
void cli_toml_value(const CliExecutablesLengthValueCtx *args);
void cli_cargo_bins(const CliExecutablesNamesCtx *args);
void cli_uncommented(const CliExecutablesTextCtx *args);
unsigned int cli_calls(const CliExecutablesCodeCtx *args);
// a script's text with its comments taken out, in a cell no longer than the text, NULL where the cell is refused; and
// the calls of `command` in `code` in a cell of their count into `*calls`: their count, 0 and NULL where `code` is
// NULL, it contains none or the cell is refused. Each cell is given back to g_cli_executables_owner
char *cli_uncommented_alloc(const CliExecutablesTextCtx *args);
unsigned int cli_calls_alloc(const CliExecutablesCodeCommandCtx *args);
void cli_cmake_targets(const CliExecutablesNamesTextCtx *args);
int cli_cmake_project(const CliExecutablesTextOutCtx *args);
void cli_meson_targets(const CliExecutablesNamesCtx *args);
int cli_meson_project(const CliExecutablesTextOutCtx *args);
int cli_go_package(const CliExecutablesTextOutCtx *args);
int cli_dotnet_program(const CliExecutablesTextOutCtx *args);
int cli_keyword(const CliExecutablesLengthCtx *args);
void cli_zig_exes(const CliExecutablesNamesCtx *args);
int cli_spec_init(const CliExecutablesTextExeCtx *args);
void cli_npm_bins(const CliExecutablesNamesTextModuleCtx *args);

// the most build scripts read, and the most folders still to look in
#define CLI_EXECUTABLES_SCRIPTS 4096u
#define CLI_EXECUTABLES_PENDING 4096u

// A walk of the tree's folders: those still to look in and their depths, the folder looked in, and what is found, all
// in one cell taken with cli_executables_walk_alloc and released through persistent_buf_release
typedef struct
{
    char (*pending)[CLI_PATH_MAX_LEN];
    unsigned int *depth;
    unsigned int pending_count;
    unsigned int pending_capacity;
    const char *folder;
    unsigned int at_depth;
    char (*found)[CLI_PATH_MAX_LEN];
    unsigned int found_count;
    unsigned int found_capacity;
    int main;
} CliWalk;

// `name` joined to `folder` into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it does not fit; whether a folder
// is one a walk passes; the folders a walk has pending looked through in turn from `top`, each entry handed to
// `each`, a folder whose entries set `main` put after those found; a folder put after those pending at `depth`, and a
// path put after those found, each up to its most: 1, or 0 where it is past the most; a walk's cell taken, its
// capacities none where it is refused; a walk's cell given back; and every program the tree's build scripts make, by
// script and then by name (executables_read.c)
int cli_executables_join(const CliExecutablesNameOutFolderCtx *args);
int cli_executables_pass(const CliExecutablesNameOutFolderCtx *args);
void cli_executables_walk(const CliExecutablesWalkTopCtx *args);
int cli_executables_pending_append(const CliExecutablesWalkCtx *args);
int cli_executables_found_append(const CliExecutablesWalkCtx *args);
void cli_executables_walk_alloc(const CliExecutablesWalkCtx *args);
void cli_executables_read(const CliExecutablesFoundCtx *args);

// the main packages of the Go module whose go.mod is `module` and whose text is `text`, each named with its folder as
// `go build` takes it, in order; and the programs that can build a bin whose script is `script`, in the folder of its
// package, into `one` (executables_go.c)
void cli_go_mains(const CliExecutablesTextModuleCtx *args);
void cli_npm_builders(const CliExecutablesFolderCtx *args);

// the program's file name on this system into `out`, sized for CLI_NAME_MAX_LEN; the script's folder in the tree,
// "." for the tree's own, into `out`, sized for CLI_PATH_MAX_LEN; the folder a CMake or Meson project's build of each
// configuration goes in, before the configuration's name; this machine's system and processor by Go's names; and the
// first value of `key`, or `fallback` where it has none (executables_jobs.c)
void cli_program_file(const CliExecutablesNameOutFolderCtx *args);
void cli_folder_get(const CliExecutablesOutCtx *args);
void cli_build_folder(const CliExecutablesOutCtx *args);
void cli_go_host(const CliExecutablesSystemCtx *args);
const char *cli_first(const CliExecutablesKeyCtx *args);

// the job that builds `exe` written in the catalog, titled `title`, with what it declares of itself: 1, or 0 where the
// catalog is full; and its steps put on it, each with SOURCE_DATE_EPOCH set, then the program it made run as the job's
// `then` requests (executables_steps.c)
int cli_executables_job_begin(const CliExecutablesRootCtx *args);
void cli_executables_steps(const CliExecutablesRootCtx *args);

// what the program `name` of the toolchain `tool` writes for `args`, `count` of them, run in `folder` where it is
// given, into `run_result`: 1 where it ends well, or 0 with `name`, its first argument and what went wrong reported
// (executables.c)
int cli_executables_output(const CliExecutablesNameCtx *args);

// the program .NET's build made, from the last line it printed for the project `name`, into `out`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 with why given (executables.c)
int cli_dotnet_artifact(const CliExecutablesNameOutErrMsgCtx *args);

// the file a CMake build in `build` made for target `name` in configuration `config`, as the reply of CMake's file
// API names it, and the file Meson's build in `build` made for executable `name`, as `meson introspect` names it, each
// into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 with why given (executables_cmake.c)
int cli_cmake_artifact(const CliExecutablesNameOutCtx *args);
int cli_meson_artifact(const CliExecutablesNameOutCtx *args);

// the warnings file a PyInstaller build of the spec at `spec` wrote, read into a cell of its size given back to
// g_cli_executables_owner, NULL where there is none or the cell is refused, with the spec's folder into `folder`,
// which is sized for CLI_PATH_MAX_LEN; and each module that file imports
// PyInstaller did not find handed to `each` with the text of the modules that import it, `A (top-level), B (delayed)`
// (executables_advice.c)
char *cli_pyinstaller_warn(const CliExecutablesSpecCtx *args);
void cli_pyinstaller_missing(const CliExecutablesTextEachCtx *args);

// the text of the file at `path` read whole into `bytes`, sized for `capacity`, ended by a 0: 1, or 0 where it could
// not be read; and read whole into a cell of its size and a 0 after it, given back to g_cli_executables_owner: the
// cell, or NULL where it could not be read or the cell is refused
int cli_text_read(const CliExecutablesCapacityCtx *args);
char *cli_text_alloc(const CliExecutablesWalkCtx *args);

struct CliExecutablesNameCtx
{
    const char *name;
    const char *folder;
    const char *tool;
    const char *const *request;
    unsigned int count;
    CliRunResult *run_result;
    VerbaLinea *err_msg;
};

struct CliExecutablesNameOutCtx
{
    const char *name;
    char *out;
    VerbaLinea *err_msg;
    const char *build;
    const char *config;
};

struct CliExecutablesOutCtx
{
    char *out;
    const char *root;
    const char *script;
    const char *tool;
};

struct CliExecutablesCodeCtx
{
    const char *code;
    const char *command;
    CliCall *calls;
    unsigned int capacity_u32;
};

struct CliExecutablesNameOutErrMsgCtx
{
    const char *name;
    char *out;
    VerbaLinea *err_msg;
    const CliLines *lines;
};

struct CliExecutablesRootCtx
{
    const char *root;
    const CliExecutable *exe;
    const char *title;
    CliCatalog *catalog;
};

struct CliExecutablesLengthCtx
{
    unsigned long long length;
    char *out;
    const char *call;
    const char *key;
};

struct CliExecutablesNameNamesCtx
{
    const char *name;
    CliNames *names;
    unsigned long long length;
    const char *value;
};

struct CliExecutablesLengthTextCtx
{
    unsigned long long length;
    const char *text;
    char *out;
    unsigned long long capacity;
};

struct CliExecutablesLengthValueCtx
{
    unsigned long long length;
    const char *value;
    char *out;
    unsigned long long capacity;
};

struct CliExecutablesTextCtx
{
    const char *text;
    char *out;
    unsigned long long capacity;
    char quote;
};

struct CliExecutablesCodeCommandCtx
{
    const char *code;
    const char *command;
    CliCall **calls_pointer;
};

struct CliExecutablesNamesCtx
{
    CliNames *names;
    const char *text;
    const char *manifest;
};

struct CliExecutablesNamesTextCtx
{
    CliNames *names;
    const char *text;
    const char *project;
};

struct CliExecutablesTextOutCtx
{
    const char *text;
    char *out;
    const char *project;
};

struct CliExecutablesNameOutFolderCtx
{
    const char *name;
    char *out;
    const char *folder;
};

struct CliExecutablesWalkCtx
{
    CliWalk *walk;
    const char *path;
    unsigned int depth;
};

struct CliExecutablesWalkTopCtx
{
    CliWalk *walk;
    const char *top;
    void (*each)(const CliEachNameIsFolderCfg *args);
};

struct CliExecutablesKeyCtx
{
    const char *key;
    const CliValues *values;
    const char *fallback;
};

struct CliExecutablesTextModuleCtx
{
    const char *text;
    const char *module_text;
    CliNames *mains;
};

struct CliExecutablesNamesTextModuleCtx
{
    CliNames *names;
    const char *text;
    int *module;
};

struct CliExecutablesFolderCtx
{
    const char *folder;
    const char *script;
    CliExecutable *one;
};

struct CliExecutablesTextEachCtx
{
    const char *text;
    void (*each_callback)(const CliEachModuleImportersCfg *args);
    void *context;
};

struct CliExecutablesTextExeCtx
{
    const char *text;
    char *exe_capacity;
    char *path_out;
};

struct CliExecutablesCapacityCtx
{
    unsigned long long capacity;
    const char *path;
    char *bytes;
};

struct CliExecutablesFoundCtx
{
    CliExecutables *found;
    const char *root;
};

struct CliExecutablesSystemCtx
{
    const char **system;
    const char **processor;
};

struct CliExecutablesSpecCtx
{
    const char *spec;
    char *folder_out;
};

struct CliExecutablesTextLengthCtx
{
    const char **text_pointer;
    unsigned long long *length_pointer;
};

struct CliExecutablesCtx
{
    char letter;
};

typedef struct
{
    void (*order)(const CliExecutablesFoundCtx *args);
    void (*read)(const CliExecutablesFoundCtx *args);
} CliExecutablesFoundNs;
EMBED_TABLE_LAYOUT(CliExecutablesFoundNs, order, read);
EMBED_TABLE_STORAGE CliExecutablesFoundNs cli_executables_found_internal EMBED_UNUSED = {
    .order = cli_executables_order,
    .read = cli_executables_read,
};

typedef struct
{
    unsigned int (*type_get)(const CliExecutablesNameOutFolderCtx *args);
    int (*join)(const CliExecutablesNameOutFolderCtx *args);
    int (*pass)(const CliExecutablesNameOutFolderCtx *args);
    void (*program_file)(const CliExecutablesNameOutFolderCtx *args);
} CliExecutablesNameOutFolderNs;
EMBED_TABLE_LAYOUT(CliExecutablesNameOutFolderNs, type_get, join, pass, program_file);
EMBED_TABLE_STORAGE CliExecutablesNameOutFolderNs cli_executables_name_out_folder_internal EMBED_UNUSED = {
    .type_get = cli_executables_type_get,
    .join = cli_executables_join,
    .pass = cli_executables_pass,
    .program_file = cli_program_file,
};

typedef struct
{
    void (*names_append)(const CliExecutablesNameNamesCtx *args);
} CliExecutablesNameNamesNs;
EMBED_TABLE_LAYOUT(CliExecutablesNameNamesNs, names_append);
EMBED_TABLE_STORAGE CliExecutablesNameNamesNs cli_executables_name_names_internal EMBED_UNUSED = {
    .names_append = cli_names_append,
};

typedef struct
{
    int (*scan_white)(const CliExecutablesCtx *args);
} CliExecutablesNsTable;
EMBED_TABLE_LAYOUT(CliExecutablesNsTable, scan_white);
EMBED_TABLE_STORAGE CliExecutablesNsTable cli_executables_internal EMBED_UNUSED = {
    .scan_white = cli_scan_white,
};

typedef struct
{
    void (*scan_trim)(const CliExecutablesTextLengthCtx *args);
} CliExecutablesTextLengthNs;
EMBED_TABLE_LAYOUT(CliExecutablesTextLengthNs, scan_trim);
EMBED_TABLE_STORAGE CliExecutablesTextLengthNs cli_executables_text_length_internal EMBED_UNUSED = {
    .scan_trim = cli_scan_trim,
};

typedef struct
{
    void (*scan_copy)(const CliExecutablesLengthTextCtx *args);
} CliExecutablesLengthTextNs;
EMBED_TABLE_LAYOUT(CliExecutablesLengthTextNs, scan_copy);
EMBED_TABLE_STORAGE CliExecutablesLengthTextNs cli_executables_length_text_internal EMBED_UNUSED = {
    .scan_copy = cli_scan_copy,
};

typedef struct
{
    void (*toml_value)(const CliExecutablesLengthValueCtx *args);
} CliExecutablesLengthValueNs;
EMBED_TABLE_LAYOUT(CliExecutablesLengthValueNs, toml_value);
EMBED_TABLE_STORAGE CliExecutablesLengthValueNs cli_executables_length_value_internal EMBED_UNUSED = {
    .toml_value = cli_toml_value,
};

typedef struct
{
    void (*cargo_bins)(const CliExecutablesNamesCtx *args);
    void (*meson_targets)(const CliExecutablesNamesCtx *args);
    void (*zig_exes)(const CliExecutablesNamesCtx *args);
} CliExecutablesNamesNs;
EMBED_TABLE_LAYOUT(CliExecutablesNamesNs, cargo_bins, meson_targets, zig_exes);
EMBED_TABLE_STORAGE CliExecutablesNamesNs cli_executables_names_internal EMBED_UNUSED = {
    .cargo_bins = cli_cargo_bins,
    .meson_targets = cli_meson_targets,
    .zig_exes = cli_zig_exes,
};

typedef struct
{
    void (*uncommented)(const CliExecutablesTextCtx *args);
    char *(*uncommented_alloc)(const CliExecutablesTextCtx *args);
} CliExecutablesTextNs;
EMBED_TABLE_LAYOUT(CliExecutablesTextNs, uncommented, uncommented_alloc);
EMBED_TABLE_STORAGE CliExecutablesTextNs cli_executables_text_internal EMBED_UNUSED = {
    .uncommented = cli_uncommented,
    .uncommented_alloc = cli_uncommented_alloc,
};

typedef struct
{
    unsigned int (*calls)(const CliExecutablesCodeCtx *args);
} CliExecutablesCodeNs;
EMBED_TABLE_LAYOUT(CliExecutablesCodeNs, calls);
EMBED_TABLE_STORAGE CliExecutablesCodeNs cli_executables_code_internal EMBED_UNUSED = {
    .calls = cli_calls,
};

typedef struct
{
    unsigned int (*calls_alloc)(const CliExecutablesCodeCommandCtx *args);
} CliExecutablesCodeCommandNs;
EMBED_TABLE_LAYOUT(CliExecutablesCodeCommandNs, calls_alloc);
EMBED_TABLE_STORAGE CliExecutablesCodeCommandNs cli_executables_code_command_internal EMBED_UNUSED = {
    .calls_alloc = cli_calls_alloc,
};

typedef struct
{
    void (*cmake_targets)(const CliExecutablesNamesTextCtx *args);
} CliExecutablesNamesTextNs;
EMBED_TABLE_LAYOUT(CliExecutablesNamesTextNs, cmake_targets);
EMBED_TABLE_STORAGE CliExecutablesNamesTextNs cli_executables_names_text_internal EMBED_UNUSED = {
    .cmake_targets = cli_cmake_targets,
};

typedef struct
{
    int (*cmake_project)(const CliExecutablesTextOutCtx *args);
    int (*meson_project)(const CliExecutablesTextOutCtx *args);
    int (*go_package)(const CliExecutablesTextOutCtx *args);
    int (*dotnet_program)(const CliExecutablesTextOutCtx *args);
} CliExecutablesTextOutNs;
EMBED_TABLE_LAYOUT(CliExecutablesTextOutNs, cmake_project, meson_project, go_package, dotnet_program);
EMBED_TABLE_STORAGE CliExecutablesTextOutNs cli_executables_text_out_internal EMBED_UNUSED = {
    .cmake_project = cli_cmake_project,
    .meson_project = cli_meson_project,
    .go_package = cli_go_package,
    .dotnet_program = cli_dotnet_program,
};

typedef struct
{
    int (*keyword)(const CliExecutablesLengthCtx *args);
} CliExecutablesLengthNs;
EMBED_TABLE_LAYOUT(CliExecutablesLengthNs, keyword);
EMBED_TABLE_STORAGE CliExecutablesLengthNs cli_executables_length_internal EMBED_UNUSED = {
    .keyword = cli_keyword,
};

typedef struct
{
    int (*spec_init)(const CliExecutablesTextExeCtx *args);
} CliExecutablesTextExeNs;
EMBED_TABLE_LAYOUT(CliExecutablesTextExeNs, spec_init);
EMBED_TABLE_STORAGE CliExecutablesTextExeNs cli_executables_text_exe_internal EMBED_UNUSED = {
    .spec_init = cli_spec_init,
};

typedef struct
{
    void (*npm_bins)(const CliExecutablesNamesTextModuleCtx *args);
} CliExecutablesNamesTextModuleNs;
EMBED_TABLE_LAYOUT(CliExecutablesNamesTextModuleNs, npm_bins);
EMBED_TABLE_STORAGE CliExecutablesNamesTextModuleNs cli_executables_names_text_module_internal EMBED_UNUSED = {
    .npm_bins = cli_npm_bins,
};

typedef struct
{
    void (*walk)(const CliExecutablesWalkTopCtx *args);
} CliExecutablesWalkTopNs;
EMBED_TABLE_LAYOUT(CliExecutablesWalkTopNs, walk);
EMBED_TABLE_STORAGE CliExecutablesWalkTopNs cli_executables_walk_top_internal EMBED_UNUSED = {
    .walk = cli_executables_walk,
};

typedef struct
{
    int (*pending_append)(const CliExecutablesWalkCtx *args);
    int (*found_append)(const CliExecutablesWalkCtx *args);
    void (*walk_alloc)(const CliExecutablesWalkCtx *args);
    char *(*text_alloc)(const CliExecutablesWalkCtx *args);
} CliExecutablesWalkNs;
EMBED_TABLE_LAYOUT(CliExecutablesWalkNs, pending_append, found_append, walk_alloc, text_alloc);
EMBED_TABLE_STORAGE CliExecutablesWalkNs cli_executables_walk_internal EMBED_UNUSED = {
    .pending_append = cli_executables_pending_append,
    .found_append = cli_executables_found_append,
    .walk_alloc = cli_executables_walk_alloc,
    .text_alloc = cli_text_alloc,
};

typedef struct
{
    void (*go_mains)(const CliExecutablesTextModuleCtx *args);
} CliExecutablesTextModuleNs;
EMBED_TABLE_LAYOUT(CliExecutablesTextModuleNs, go_mains);
EMBED_TABLE_STORAGE CliExecutablesTextModuleNs cli_executables_text_module_internal EMBED_UNUSED = {
    .go_mains = cli_go_mains,
};

typedef struct
{
    void (*npm_builders)(const CliExecutablesFolderCtx *args);
} CliExecutablesFolderNs;
EMBED_TABLE_LAYOUT(CliExecutablesFolderNs, npm_builders);
EMBED_TABLE_STORAGE CliExecutablesFolderNs cli_executables_folder_internal EMBED_UNUSED = {
    .npm_builders = cli_npm_builders,
};

typedef struct
{
    void (*folder_get)(const CliExecutablesOutCtx *args);
    void (*build_folder)(const CliExecutablesOutCtx *args);
} CliExecutablesOutNs;
EMBED_TABLE_LAYOUT(CliExecutablesOutNs, folder_get, build_folder);
EMBED_TABLE_STORAGE CliExecutablesOutNs cli_executables_out_internal EMBED_UNUSED = {
    .folder_get = cli_folder_get,
    .build_folder = cli_build_folder,
};

typedef struct
{
    void (*go_host)(const CliExecutablesSystemCtx *args);
} CliExecutablesSystemNs;
EMBED_TABLE_LAYOUT(CliExecutablesSystemNs, go_host);
EMBED_TABLE_STORAGE CliExecutablesSystemNs cli_executables_system_internal EMBED_UNUSED = {
    .go_host = cli_go_host,
};

typedef struct
{
    const char *(*first)(const CliExecutablesKeyCtx *args);
} CliExecutablesKeyNs;
EMBED_TABLE_LAYOUT(CliExecutablesKeyNs, first);
EMBED_TABLE_STORAGE CliExecutablesKeyNs cli_executables_key_internal EMBED_UNUSED = {
    .first = cli_first,
};

typedef struct
{
    int (*job_begin)(const CliExecutablesRootCtx *args);
    void (*steps)(const CliExecutablesRootCtx *args);
} CliExecutablesRootNsTable;
EMBED_TABLE_LAYOUT(CliExecutablesRootNsTable, job_begin, steps);
EMBED_TABLE_STORAGE CliExecutablesRootNsTable cli_executables_root_internal EMBED_UNUSED = {
    .job_begin = cli_executables_job_begin,
    .steps = cli_executables_steps,
};

typedef struct
{
    int (*output)(const CliExecutablesNameCtx *args);
} CliExecutablesNameNs;
EMBED_TABLE_LAYOUT(CliExecutablesNameNs, output);
EMBED_TABLE_STORAGE CliExecutablesNameNs cli_executables_name_internal EMBED_UNUSED = {
    .output = cli_executables_output,
};

typedef struct
{
    int (*dotnet_artifact)(const CliExecutablesNameOutErrMsgCtx *args);
} CliExecutablesNameOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliExecutablesNameOutErrMsgNs, dotnet_artifact);
EMBED_TABLE_STORAGE CliExecutablesNameOutErrMsgNs cli_executables_name_out_err_msg_internal EMBED_UNUSED = {
    .dotnet_artifact = cli_dotnet_artifact,
};

typedef struct
{
    int (*cmake_artifact)(const CliExecutablesNameOutCtx *args);
    int (*meson_artifact)(const CliExecutablesNameOutCtx *args);
} CliExecutablesNameOutNs;
EMBED_TABLE_LAYOUT(CliExecutablesNameOutNs, cmake_artifact, meson_artifact);
EMBED_TABLE_STORAGE CliExecutablesNameOutNs cli_executables_name_out_internal EMBED_UNUSED = {
    .cmake_artifact = cli_cmake_artifact,
    .meson_artifact = cli_meson_artifact,
};

typedef struct
{
    char *(*pyinstaller_warn)(const CliExecutablesSpecCtx *args);
} CliExecutablesSpecNs;
EMBED_TABLE_LAYOUT(CliExecutablesSpecNs, pyinstaller_warn);
EMBED_TABLE_STORAGE CliExecutablesSpecNs cli_executables_spec_internal EMBED_UNUSED = {
    .pyinstaller_warn = cli_pyinstaller_warn,
};

typedef struct
{
    void (*pyinstaller_missing)(const CliExecutablesTextEachCtx *args);
} CliExecutablesTextEachNs;
EMBED_TABLE_LAYOUT(CliExecutablesTextEachNs, pyinstaller_missing);
EMBED_TABLE_STORAGE CliExecutablesTextEachNs cli_executables_text_each_internal EMBED_UNUSED = {
    .pyinstaller_missing = cli_pyinstaller_missing,
};

typedef struct
{
    int (*text_read)(const CliExecutablesCapacityCtx *args);
} CliExecutablesCapacityNs;
EMBED_TABLE_LAYOUT(CliExecutablesCapacityNs, text_read);
EMBED_TABLE_STORAGE CliExecutablesCapacityNs cli_executables_capacity_internal EMBED_UNUSED = {
    .text_read = cli_text_read,
};

#endif
