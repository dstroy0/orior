// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains.h: the toolchains orior and the tree use, as toolchains.json lists them: what each is for, the programs
// that name it, the page to install it from on each system, and the folders it is usually installed in. orior installs
// none of them; it finds them, opens their install pages, and puts their folders on the reader's PATH when asked
// (toolchains.c reads the manifest and its folders, toolchains_added.c keeps what the reader added and chose,
// toolchains_path.c makes the PATH, toolchains_find.c finds each tool).
//
// A tool is found in this order: the program its variable names, such as ORIOR_PYTHON; a folder the reader gave orior
// for it; the PATH; then the folders it is usually installed in. On Windows the PATH read is what the registry holds
// now, the system's and the reader's, ahead of the PATH orior started with: a folder added since shows without
// starting orior again. A program in a folder `notIn` names is passed over there: the bash in System32 is WSL's, and
// the python in WindowsApps is the Store's stand-in.
//
// The folders the reader gave are kept in toolchains.json in orior's own folder, by tool, and every job and terminal
// orior starts has them at the front of its PATH.
//
// The reader adds toolchains and groups of their own, kept in user_toolchains.json in orior's own folder as
// { "groups": [...], "tools": [...] }, each tool written as toolchains.json writes one. They come after orior's own in
// every list, each group after the groups orior's tools are in, and a group may stand empty until a tool is added to
// it. Only what the reader added can be taken out.
//
// The manifest is read into a JSON tree its caller hands, each tool an object of toolchains.json's own keys, which the
// modules that start a tool's server, debugger, checker or formatter read by name
#ifndef TOOLCHAINS_H
#define TOOLCHAINS_H

#include "../text/json_tree.h"

// the bytes of toolchains.json as built into the program (plugins_bundled.c, written as the program is built)
extern const unsigned char g_cli_toolchains_json[];
extern const unsigned long long g_cli_toolchains_json_size;

// the most folders a PATH holds as orior reads it, and the most bytes of a PATH
#define CLI_PATH_FOLDERS 512u

#define CLI_PATH_TEXT QUERY_ENVIRONMENT_LONGEST

// the most folders a tree's environment puts first on the PATH
#define CLI_ENVIRONMENT_BINS 16u

// The tree's own environment, as envs finds one: the folders it puts first on the PATH, its Python where it has one,
// and its folder in the tree
typedef struct
{
    char bin[CLI_ENVIRONMENT_BINS][CLI_PATH_LONGEST];
    unsigned int bins;
    char python[CLI_PATH_LONGEST];
    int has_python;
    char place[CLI_PATH_LONGEST];
} CliTreeEnvironment;

// A tool as orior finds it: `state` is "env", "tree", "chosen", "path", "found" or "missing": named by its variable,
// the tree's environment's Python, in the folder the reader gave, on the PATH, installed in one of its usual folders
// and not on the PATH, or not found. The program found and its folder, where it was
typedef struct
{
    const char *state;
    char program[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    int has_program;
} CliFound;

// "windows", "macos" or "linux": the key toolchains.json gives each system's pages and folders under
const char *cli_toolchains_system(void);
// orior's own toolchains, then the reader's, those for another system left out, read into `json` in a cell of its own
// sized to both texts, with `nodes_more` nodes and `text_more` bytes over for what the caller makes after, which
// cli_json_given gives back: the array of them, CLI_JSON_NONE where the tree is full; and the one of `id`,
// CLI_JSON_NONE with why said where orior knows none
unsigned int cli_toolchains_manifest(CliJson *json, unsigned int nodes_more, unsigned long long text_more);
unsigned int cli_toolchains_tool(CliJson *json, unsigned int manifest, const char *id, CliLine *said);
// the member of a map this system's key names, else its "any", CLI_JSON_NONE where it has neither
unsigned int cli_toolchains_for_system(const CliJson *json, unsigned int map);
// the first of the programs `names`, `count` of them, in the folder `dir`, as a file there, into `path`, which holds
// CLI_PATH_LONGEST: 1, or 0 where it holds none; the same of a tool's `programs`
int cli_toolchains_program_in(const char *dir, const char *const *names, unsigned int count, char *path);
int cli_toolchains_tool_program_in(const CliJson *json, unsigned int tool, const char *dir, char *path);
// the folders a pattern of toolchains.json names, newest first where a * stands for a version, each that is a folder
// into `folders` from `count`, which holds `room`: the count after
unsigned int cli_toolchains_folders_of(const char *pattern, char (*folders)[CLI_PATH_LONGEST], unsigned int count,
                                       unsigned int room);
// a folder as PATH entries are compared into `key`, which holds CLI_PATH_LONGEST: without a trailing separator, and
// case aside on Windows
void cli_toolchains_key_of(const char *dir, char *key);

// every group, in the order the sheet and the command line list them, those of orior's tools as they first come, then
// those the reader added, each handed to `each` (toolchains_added.c)
void cli_toolchains_groups(void (*each)(void *context, const char *group), void *context);
// the reader's toolchain made of the JSON object at `tool` added, its id made from its name where it has none, and its
// group with it where the group is new, its id into `id`, which holds CLI_NAME_LONGEST; an empty group of the reader's
// added; a toolchain the reader added taken out, and the folder they gave for it; and an empty group of theirs taken
// out. Each 1, or 0 with why said
int cli_toolchains_add(const CliJson *json, unsigned int tool, char *id, CliLine *said);
int cli_toolchains_add_group(const char *name, CliLine *said);
int cli_toolchains_remove(const char *id, CliLine *said);
int cli_toolchains_remove_group(const char *name, CliLine *said);
// the folder the reader gave orior for tool `id` into `folder`, which holds CLI_PATH_LONGEST: 1, or 0 where they gave
// none; orior made to use `folder` for tool `id`, which must hold one of the tool's programs, the program found there
// into `program`; and the folder forgotten. Each 1, or 0 with why said
int cli_toolchains_chosen(const char *id, char *folder);
int cli_toolchains_choose(const char *id, const char *folder, char *program, CliLine *said);
int cli_toolchains_forget(const char *id, CliLine *said);
// each folder the reader gave, by tool, handed to `each`
void cli_toolchains_chosen_each(void (*each)(void *context, const char *id, const char *folder), void *context);
// whether the reader added the tool `id`
int cli_toolchains_added(const char *id);
// the program orior runs for tool `id` where the reader gave a folder for it, into `path`: 1, or 0 where none
int cli_toolchains_chosen_program(const char *id, char *path);

// `root` made the tree whose own tools come first on the PATH; the tree's own tools' folders that are there,
// node_modules/.bin for npm's and vendor/bin for Composer's, into `folders`, which holds `room`: their count; the
// tree's environment set, or none where `environment` is NULL; and the tree's environment into `environment`: 1, or 0
// where none is set (toolchains_path.c)
void cli_toolchains_set_tree(const char *root);
unsigned int cli_toolchains_tree_tools(char (*folders)[CLI_PATH_LONGEST], unsigned int room);
void cli_toolchains_set_environment(const CliTreeEnvironment *environment);
int cli_toolchains_environment(CliTreeEnvironment *environment);
// the PATH's folders, in order and each once, into `folders`, which holds `room`: their count. The tree's own tools'
// folders, then on Windows the system's and the reader's as the registry holds them now, then the PATH orior started
// with, then the folders the login shell's PATH adds, once it has been read
unsigned int cli_toolchains_path_folders(char (*folders)[CLI_PATH_LONGEST], unsigned int room);
// the PATH's folders as cli_toolchains_path_folders reads them, in a cell sized to the most their texts name, which
// cli_cell_given gives back: the folders, their count into `count`, or NULL with 0 where the cell cannot be taken
char (*cli_toolchains_path_taken(unsigned int *count))[CLI_PATH_LONGEST];
// the first of `count` program `names` found in the PATH's folders, in their order, into `program`, which holds
// CLI_PATH_LONGEST: 1, or 0 where none holds one
int cli_toolchains_on_path(const char *const *names, unsigned int count, char *program);
// the PATH a program gets when it is opened on its own and not from orior, as its folders: on Windows the system's
// and the reader's as the registry holds them, elsewhere the PATH orior started with, without the folders orior puts
// ahead of it for its jobs
unsigned int cli_toolchains_system_path(char (*folders)[CLI_PATH_LONGEST], unsigned int room);
// the PATH every step runs with into `path`, which holds `size`: the tree's environment's folders, its own tools'
// folders, the folders the reader gave for toolchains, then the PATH's; and the same with `ahead`, `count` folders, in
// front of it
void cli_toolchains_run_path(char *path, unsigned long long size);
void cli_toolchains_run_path_with(const char (*ahead)[CLI_PATH_LONGEST], unsigned int count, char *path,
                                  unsigned long long size);
// the folders that exist of those written by system as `places` writes them, each pattern's in order, put into
// `folders` from `count`: the count after
unsigned int cli_toolchains_folders_for(const CliJson *json, unsigned int by_system, char (*folders)[CLI_PATH_LONGEST],
                                        unsigned int count, unsigned int room);
// `path` with `folder` added at its end into `out`, which holds `size`: 1, or 0 where it is there already
int cli_toolchains_joined(const char *path, const char *folder, char *out, unsigned long long size);
// orior's own folder into `folder`, which holds CLI_PATH_LONGEST, and whether it is on the PATH: 1, or 0 with why
// said; and a folder added to the reader's own PATH, for every terminal and program started after: "orior" adds orior's
// own folder, and a tool's id the folder its program was found in, the folder into `folder`: 1, or 0 with why said
int cli_toolchains_own(char *folder, int *on_path, CliLine *said);
int cli_toolchains_add_to_path(const char *what, char *folder, CliLine *said);

// a tool as the module's opening says, and Python as the tree's environment has it first, where the tree has one, its
// folders found on `path`, `count` of them, into `found` (toolchains_find.c)
void cli_toolchains_find(const CliJson *json, unsigned int tool, const char (*path)[CLI_PATH_LONGEST],
                         unsigned int count, CliFound *found);
// every toolchain, as orior finds it now, written as the window reads it, each with its version where it is asked
// one and `versions` is set; what tool `id` says its version is into `out`, the first line it writes with a dotted
// number in it, such as 13.3, which a copyright's years are not; the shell line that installs tool `id` on this system;
// and the install page of tool `id` for this system opened in the browser, its address into `out`. Each 1, or 0 with
// why said
void cli_toolchains_check(int versions, CliLine *out);
int cli_toolchains_version(const char *id, CliLine *out, CliLine *said);
int cli_toolchains_setup_line(const char *id, CliLine *out, CliLine *said);
int cli_toolchains_open_install(const char *id, CliLine *out, CliLine *said);

#endif
