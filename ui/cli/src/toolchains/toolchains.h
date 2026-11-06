// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains.h: the toolchains orior_cli and the tree use, as toolchains.json lists them: what each is for, the
// programs that name it, the page to install it from on each system, and the folders it is usually installed in.
// orior_cli installs none of them; it finds them, opens their install pages, and puts their folders on the reader's
// PATH when requested (toolchains.c reads the manifest and its folders, toolchains_added.c stores what the reader added
// and chose, toolchains_path.c makes the PATH, toolchains_find.c finds each tool).
//
// A tool is found in this order: the program its variable names, such as ORIOR_PYTHON; a folder the reader gave orior
// for it; the PATH; then the folders it is usually installed in. On Windows the PATH read is what the registry stores
// now, the system's and the reader's, ahead of the PATH orior_cli started with: a folder added since shows without
// starting orior_cli again. A program in a folder `notIn` names is passed over there: the bash in System32 is WSL's,
// and the python in WindowsApps is the Store's stand-in.
//
// A tree uses a tool its jobs need, one the reader added, and one whose `uses` names a file's ending, such as
// `*.lean`, or a file's name, such as `CMakeLists.txt`, that the tree's view saw as it was read; the window requests
// only those their versions.
//
// The folders the reader gave are stored in toolchains.json in orior's own folder, by tool, and every job and terminal
// orior_cli starts has them at the front of its PATH.
//
// The reader adds toolchains and groups of their own, stored in user_toolchains.json in orior's own folder as
// { "groups": [...], "tools": [...] }, each tool written as toolchains.json writes one. They come after orior's own in
// every list, each group after the groups orior's tools are in, and a group may stand empty until a tool is added to
// it. Only what the reader added can be taken out.
//
// The manifest is read into a JSON tree its caller hands, each tool an object of toolchains.json's own keys, which the
// modules that start a tool's server, debugger, checker or formatter read by name
#ifndef TOOLCHAINS_H
#define TOOLCHAINS_H
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliToolchainsJsonCfg CliToolchainsJsonCfg;
typedef struct CliToolchainsJsonCountCfg CliToolchainsJsonCountCfg;
typedef struct CliToolchainsErrMsgCfg CliToolchainsErrMsgCfg;
typedef struct CliToolchainsVersionsCfg CliToolchainsVersionsCfg;
typedef struct CliToolchainsIdCfg CliToolchainsIdCfg;
typedef struct CliToolchainsCountCfg CliToolchainsCountCfg;
typedef struct CliToolchainsFolderCfg CliToolchainsFolderCfg;
typedef struct CliToolchainsDirCfg CliToolchainsDirCfg;
typedef struct CliToolchainsCountPathCfg CliToolchainsCountPathCfg;
typedef struct CliToolchainsJsonManifestCfg CliToolchainsJsonManifestCfg;
typedef struct CliToolchainsJsonDirCfg CliToolchainsJsonDirCfg;
typedef struct CliToolchainsErrMsgFolderCfg CliToolchainsErrMsgFolderCfg;
typedef struct CliToolchainsJsonNodesMoreCfg CliToolchainsJsonNodesMoreCfg;
typedef struct CliToolchainsNamesCfg CliToolchainsNamesCfg;
typedef struct CliToolchainsIdErrMsgCfg CliToolchainsIdErrMsgCfg;
typedef struct CliToolchainsErrMsgFolderOnPathCfg CliToolchainsErrMsgFolderOnPathCfg;
typedef struct CliToolchainsErrMsgNameCfg CliToolchainsErrMsgNameCfg;
typedef struct CliToolchainsIdFolderCfg CliToolchainsIdFolderCfg;
typedef struct CliToolchainsContextCfg CliToolchainsContextCfg;
typedef struct CliToolchainsIdPathCfg CliToolchainsIdPathCfg;
typedef struct CliToolchainsJsonMapCfg CliToolchainsJsonMapCfg;
typedef struct CliToolchainsEachCfg CliToolchainsEachCfg;
typedef struct CliToolchainsDirKeyCfg CliToolchainsDirKeyCfg;
typedef struct CliToolchainsCfg CliToolchainsCfg;
typedef struct CliToolchainsEnvironmentCfg CliToolchainsEnvironmentCfg;
typedef struct CliToolchainsRootCfg CliToolchainsRootCfg;

// the bytes of toolchains.json as built into the program (plugins_bundled.c, written as the program is built)
extern const unsigned char g_cli_toolchains_json[];
extern const unsigned long long g_cli_toolchains_json_size;

// the most folders a PATH contains as orior_cli reads it, and the most bytes of a PATH
#define CLI_PATH_FOLDERS 512u

#define CLI_PATH_TEXT QUERY_ENVIRONMENT_MAX_LEN

// the most folders a tree's environment puts first on the PATH
#define CLI_ENVIRONMENT_BINS 16u

// The tree's own environment, as envs finds one: the folders it puts first on the PATH, its Python where it has one,
// and its folder in the tree
typedef struct
{
    char bin[CLI_ENVIRONMENT_BINS][CLI_PATH_MAX_LEN];
    unsigned int bins;
    char python[CLI_PATH_MAX_LEN];
    int has_python;
    char place[CLI_PATH_MAX_LEN];
} CliTreeEnvironment;

// A tool as orior_cli finds it: `state` is "env", "tree", "chosen", "path", "found" or "missing": named by its
// variable, the tree's environment's Python, in the folder the reader gave, on the PATH, installed in one of its usual
// folders and not on the PATH, or not found. The program found and its folder, where it was
typedef struct
{
    const char *state;
    char program[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    int has_program;
} CliFound;

// "windows", "macos" or "linux": the key toolchains.json gives each system's pages and folders under
const char *cli_toolchains_system(void);
// orior's own toolchains, then the reader's, those for another system left out, read into `json` in a cell of its own
// sized to both texts, with `nodes_more` nodes and `text_more` bytes over for what the caller makes after, which
// persistent_buf_release releases: the array of them, CLI_JSON_NONE where the tree is full; and the one of `id`,
// CLI_JSON_NONE with why given where orior_cli knows none
unsigned int cli_toolchains_manifest(const CliToolchainsJsonNodesMoreCfg *args);
unsigned int cli_toolchains_tool(const CliToolchainsJsonManifestCfg *args);
// the member of a map this system's key names, else its "any", CLI_JSON_NONE where it has neither
unsigned int cli_toolchains_for_system(const CliToolchainsJsonMapCfg *args);
// the first of the programs `names`, `count` of them, in the folder `dir`, as a file there, into `path`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 where it contains none; the same of a tool's `programs`
int cli_toolchains_program_in(const CliToolchainsDirCfg *args);
int cli_toolchains_tool_program_in(const CliToolchainsJsonDirCfg *args);
// the folders a pattern of toolchains.json names, newest first where a * stands for a version, each that is a folder
// into `folders` from `count`, sized for `capacity`: the count after
unsigned int cli_toolchains_folders_get(const CliToolchainsCountCfg *args);
// a folder as PATH entries are compared into `key`, sized for CLI_PATH_MAX_LEN: without a trailing separator, and
// case aside on Windows
void cli_toolchains_key_get(const CliToolchainsDirKeyCfg *args);

// every group, in the order the sheet and the command line list them, those of orior's tools as they first come, then
// those the reader added, each handed to `each` (toolchains_added.c)
void cli_toolchains_groups(const CliToolchainsEachCfg *args);
// the reader's toolchain made of the JSON object at `tool` added, its id made from its name where it has none, and its
// group with it where the group is new, its id into `id`, sized for CLI_NAME_MAX_LEN; an empty group of the reader's
// added; a toolchain the reader added taken out, and the folder they gave for it; and an empty group of theirs taken
// out. Each 1, or 0 with why given
int cli_toolchains_add(const CliToolchainsErrMsgCfg *args);
int cli_toolchains_add_group(const CliToolchainsErrMsgNameCfg *args);
int cli_toolchains_remove(const CliToolchainsIdErrMsgCfg *args);
int cli_toolchains_remove_group(const CliToolchainsErrMsgNameCfg *args);
// the folder the reader gave orior_cli for tool `id` into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0 where they
// gave none; orior_cli made to use `folder` for tool `id`, which must contain one of the tool's programs, the program
// found there into `program`; and the folder forgotten. Each 1, or 0 with why given
int cli_toolchains_on_choose(const CliToolchainsIdFolderCfg *args);
int cli_toolchains_choose(const CliToolchainsIdCfg *args);
int cli_toolchains_forget(const CliToolchainsIdErrMsgCfg *args);
// each folder the reader gave, by tool, handed to `each`
void cli_toolchains_chosen_each(const CliToolchainsContextCfg *args);
// whether the reader added the tool `id`
int cli_toolchains_on_add(const CliToolchainsIdFolderCfg *args);
// the program orior_cli runs for tool `id` where the reader gave a folder for it, into `path`: 1, or 0 where none
int cli_toolchains_chosen_program(const CliToolchainsIdPathCfg *args);

// the most folders of the tree's own tools
#define CLI_TREE_TOOLS 2u

// `root` made the tree whose own tools come first on the PATH; the tree's own tools' folders that are there,
// node_modules/.bin for npm's and vendor/bin for Composer's, into `folders`, sized for `capacity`: their count; the
// tree's environment set, or none where `environment` is NULL; and the tree's environment into `environment`: 1, or 0
// where none is set (toolchains_path.c)
void cli_toolchains_set_tree(const CliToolchainsRootCfg *args);
unsigned int cli_toolchains_tree_tools(const CliToolchainsCountCfg *args);
void cli_toolchains_set_environment(const CliToolchainsEnvironmentCfg *args);
int cli_toolchains_environment(const CliToolchainsCfg *args);
// the PATH's folders, in order and each once, into `folders`, sized for `capacity`: their count. The tree's own tools'
// folders, then on Windows the system's and the reader's as the registry stores them now, then the PATH orior_cli
// started with, then the folders the login shell's PATH adds, once it has been read
unsigned int cli_toolchains_path_folders(const CliToolchainsCountCfg *args);
// the PATH's folders as cli_toolchains_path_folders reads them, in a cell counted to `owner` sized to the most their
// texts name, which persistent_buf_release gives back: the folders, their count into `count`, or NULL with 0 where the
// cell cannot be taken
char (*cli_toolchains_path_allocated(unsigned int *count, const CellOwner *owner))[CLI_PATH_MAX_LEN];
// the first of `count` program `names` found in the PATH's folders, in their order, into `program`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 where none contains one
int cli_toolchains_on_path(const CliToolchainsNamesCfg *args);
// the PATH a program gets when it is opened on its own and not from orior, as its folders: on Windows the system's
// and the reader's as the registry stores them, elsewhere the PATH orior_cli started with, without the folders
// orior_cli puts ahead of it for its jobs
unsigned int cli_toolchains_system_path(const CliToolchainsCountCfg *args);
// the PATH every step runs with into `path`, sized for `size`: the tree's environment's folders, its own tools'
// folders, the folders the reader gave for toolchains, then the PATH's; and the same with `ahead`, `count` folders, in
// front of it
void cli_toolchains_run_path(const CliToolchainsCountPathCfg *args);
void cli_toolchains_run_path_with(const CliToolchainsCountPathCfg *args);
// the folders that exist of those written by system as `positions` writes them, each pattern's in order, put into
// `folders` from `count`: the count after
unsigned int cli_toolchains_folders_for(const CliToolchainsJsonCountCfg *args);
// `path` with `folder` added at its end into `out`, sized for `size`: 1, or 0 where it is there already
int cli_toolchains_join(const CliToolchainsFolderCfg *args);
// orior's own folder into `folder`, sized for CLI_PATH_MAX_LEN, and whether it is on the PATH: 1, or 0 with why
// given; and a folder added to the reader's own PATH, for every terminal and program started after: "orior" adds
// orior's own folder, and a tool's id the folder its program was found in, the folder into `folder`: 1, or 0 with why
// given
int cli_toolchains_own(const CliToolchainsErrMsgFolderOnPathCfg *args);
int cli_toolchains_add_to_path(const CliToolchainsErrMsgFolderCfg *args);

// a tool as the module's opening declares, and Python as the tree's environment has it first, where the tree has one,
// its folders found on `path`, `count` of them, into `found` (toolchains_find.c)
void cli_toolchains_find(const CliToolchainsJsonCfg *args);
// every toolchain, as orior_cli finds it now, written as the window reads it, each with its version where it is
// requested one and `versions` is set, and whether the tree uses it, as `seen` reports of a name or an ending its view
// saw, given `context`, none where `seen` is NULL; what tool `id` reports its version is into `out`, the first line it
// writes with a dotted number in it, such as 13.3, which a copyright's years are not; the shell line that installs tool
// `id` on this system; and the install page of tool `id` for this system opened in the browser, its address into `out`.
// Each 1, or 0 with why given
void cli_toolchains_check(const CliToolchainsVersionsCfg *args);
int cli_toolchains_version(const CliToolchainsIdErrMsgCfg *args);
int cli_toolchains_setup_line(const CliToolchainsIdErrMsgCfg *args);
int cli_toolchains_open_install(const CliToolchainsIdErrMsgCfg *args);

struct CliToolchainsJsonCfg
{
    const CliJson *json;
    unsigned int count;
    unsigned int tool;
    const char (*path_callback)[CLI_PATH_MAX_LEN];
    CliFound *found;
};

struct CliToolchainsJsonCountCfg
{
    const CliJson *json;
    unsigned int count;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int capacity;
    unsigned int by_system;
};

struct CliToolchainsErrMsgCfg
{
    VerbaLinea *err_msg;
    unsigned int tool;
    const CliJson *supplied;
    char *id_out;
};

struct CliToolchainsVersionsCfg
{
    int versions;
    int (*seen)(const CliEachKindCfg *args);
    const void *context_read;
    VerbaLinea *out;
};

struct CliToolchainsIdCfg
{
    const char *id;
    VerbaLinea *err_msg;
    const char *folder_read;
    char *program;
};

struct CliToolchainsCountCfg
{
    unsigned int count;
    const char *pattern;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int capacity;
};

struct CliToolchainsFolderCfg
{
    const char *folder_read;
    unsigned long long size;
    const char *path_read;
    char *out_capacity;
};

struct CliToolchainsDirCfg
{
    const char *dir;
    const char *const *names;
    unsigned int count;
    char *path;
};

struct CliToolchainsCountPathCfg
{
    unsigned int count;
    char *path;
    unsigned long long size;
    const char (*ahead)[CLI_PATH_MAX_LEN];
};

struct CliToolchainsJsonManifestCfg
{
    CliJson *json_out;
    unsigned int manifest;
    const char *id;
    VerbaLinea *err_msg;
};

struct CliToolchainsJsonDirCfg
{
    const CliJson *json;
    const char *dir;
    char *path;
    unsigned int tool;
};

struct CliToolchainsErrMsgFolderCfg
{
    VerbaLinea *err_msg;
    char *folder;
    const char *what;
};

struct CliToolchainsJsonNodesMoreCfg
{
    CliJson *json_out;
    unsigned int nodes_more;
    unsigned long long text_more;
};

struct CliToolchainsNamesCfg
{
    const char *const *names;
    unsigned int count;
    char *program;
};

struct CliToolchainsIdErrMsgCfg
{
    const char *id;
    VerbaLinea *err_msg;
    VerbaLinea *out;
};

struct CliToolchainsErrMsgFolderOnPathCfg
{
    VerbaLinea *err_msg;
    char *folder;
    int *on_path;
};

struct CliToolchainsErrMsgNameCfg
{
    VerbaLinea *err_msg;
    const char *name;
};

struct CliToolchainsIdFolderCfg
{
    const char *id;
    char *folder;
};

struct CliToolchainsContextCfg
{
    void *context;
    void (*each_callback)(const CliEachIdFolderCfg *args);
};

struct CliToolchainsIdPathCfg
{
    const char *id;
    char *path;
};

struct CliToolchainsJsonMapCfg
{
    const CliJson *json;
    unsigned int map;
};

struct CliToolchainsEachCfg
{
    void (*each)(const CliEachGroupCfg *args);
    void *context;
};

struct CliToolchainsDirKeyCfg
{
    const char *dir;
    char *key;
};

struct CliToolchainsCfg
{
    CliTreeEnvironment *environment_out;
};

struct CliToolchainsEnvironmentCfg
{
    const CliTreeEnvironment *environment;
};

struct CliToolchainsRootCfg
{
    const char *root;
};

typedef struct
{
    unsigned int (*manifest)(const CliToolchainsJsonNodesMoreCfg *args);
} CliToolchainsJsonNodesMoreNs;
EMBED_TABLE_LAYOUT(CliToolchainsJsonNodesMoreNs, manifest);
EMBED_TABLE_STORAGE CliToolchainsJsonNodesMoreNs cli_toolchains_json_nodes_more EMBED_UNUSED = {
    .manifest = cli_toolchains_manifest,
};

typedef struct
{
    unsigned int (*tool)(const CliToolchainsJsonManifestCfg *args);
} CliToolchainsJsonManifestNs;
EMBED_TABLE_LAYOUT(CliToolchainsJsonManifestNs, tool);
EMBED_TABLE_STORAGE CliToolchainsJsonManifestNs cli_toolchains_json_manifest EMBED_UNUSED = {
    .tool = cli_toolchains_tool,
};

typedef struct
{
    unsigned int (*for_system)(const CliToolchainsJsonMapCfg *args);
} CliToolchainsJsonMapNs;
EMBED_TABLE_LAYOUT(CliToolchainsJsonMapNs, for_system);
EMBED_TABLE_STORAGE CliToolchainsJsonMapNs cli_toolchains_json_map EMBED_UNUSED = {
    .for_system = cli_toolchains_for_system,
};

typedef struct
{
    int (*program_in)(const CliToolchainsDirCfg *args);
} CliToolchainsDirNs;
EMBED_TABLE_LAYOUT(CliToolchainsDirNs, program_in);
EMBED_TABLE_STORAGE CliToolchainsDirNs cli_toolchains_dir EMBED_UNUSED = {
    .program_in = cli_toolchains_program_in,
};

typedef struct
{
    int (*tool_program_in)(const CliToolchainsJsonDirCfg *args);
} CliToolchainsJsonDirNs;
EMBED_TABLE_LAYOUT(CliToolchainsJsonDirNs, tool_program_in);
EMBED_TABLE_STORAGE CliToolchainsJsonDirNs cli_toolchains_json_dir EMBED_UNUSED = {
    .tool_program_in = cli_toolchains_tool_program_in,
};

typedef struct
{
    unsigned int (*folders_get)(const CliToolchainsCountCfg *args);
    unsigned int (*tree_tools)(const CliToolchainsCountCfg *args);
    unsigned int (*path_folders)(const CliToolchainsCountCfg *args);
    unsigned int (*system_path)(const CliToolchainsCountCfg *args);
} CliToolchainsCountNs;
EMBED_TABLE_LAYOUT(CliToolchainsCountNs, folders_get, tree_tools, path_folders, system_path);
EMBED_TABLE_STORAGE CliToolchainsCountNs cli_toolchains_count EMBED_UNUSED = {
    .folders_get = cli_toolchains_folders_get,
    .tree_tools = cli_toolchains_tree_tools,
    .path_folders = cli_toolchains_path_folders,
    .system_path = cli_toolchains_system_path,
};

typedef struct
{
    void (*key_get)(const CliToolchainsDirKeyCfg *args);
} CliToolchainsDirKeyNs;
EMBED_TABLE_LAYOUT(CliToolchainsDirKeyNs, key_get);
EMBED_TABLE_STORAGE CliToolchainsDirKeyNs cli_toolchains_dir_key EMBED_UNUSED = {
    .key_get = cli_toolchains_key_get,
};

typedef struct
{
    void (*groups)(const CliToolchainsEachCfg *args);
} CliToolchainsEachNs;
EMBED_TABLE_LAYOUT(CliToolchainsEachNs, groups);
EMBED_TABLE_STORAGE CliToolchainsEachNs cli_toolchains_each EMBED_UNUSED = {
    .groups = cli_toolchains_groups,
};

typedef struct
{
    int (*add)(const CliToolchainsErrMsgCfg *args);
} CliToolchainsErrMsgNs;
EMBED_TABLE_LAYOUT(CliToolchainsErrMsgNs, add);
EMBED_TABLE_STORAGE CliToolchainsErrMsgNs cli_toolchains_err_msg EMBED_UNUSED = {
    .add = cli_toolchains_add,
};

typedef struct
{
    int (*add_group)(const CliToolchainsErrMsgNameCfg *args);
    int (*remove_group)(const CliToolchainsErrMsgNameCfg *args);
} CliToolchainsErrMsgNameNs;
EMBED_TABLE_LAYOUT(CliToolchainsErrMsgNameNs, add_group, remove_group);
EMBED_TABLE_STORAGE CliToolchainsErrMsgNameNs cli_toolchains_err_msg_name EMBED_UNUSED = {
    .add_group = cli_toolchains_add_group,
    .remove_group = cli_toolchains_remove_group,
};

typedef struct
{
    int (*remove)(const CliToolchainsIdErrMsgCfg *args);
    int (*forget)(const CliToolchainsIdErrMsgCfg *args);
    int (*version)(const CliToolchainsIdErrMsgCfg *args);
    int (*setup_line)(const CliToolchainsIdErrMsgCfg *args);
    int (*open_install)(const CliToolchainsIdErrMsgCfg *args);
} CliToolchainsIdErrMsgNs;
EMBED_TABLE_LAYOUT(CliToolchainsIdErrMsgNs, remove, forget, version, setup_line, open_install);
EMBED_TABLE_STORAGE CliToolchainsIdErrMsgNs cli_toolchains_id_err_msg EMBED_UNUSED = {
    .remove = cli_toolchains_remove,
    .forget = cli_toolchains_forget,
    .version = cli_toolchains_version,
    .setup_line = cli_toolchains_setup_line,
    .open_install = cli_toolchains_open_install,
};

typedef struct
{
    int (*choose)(const CliToolchainsIdFolderCfg *args);
    int (*add)(const CliToolchainsIdFolderCfg *args);
} CliToolchainsIdFolderNs;
EMBED_TABLE_LAYOUT(CliToolchainsIdFolderNs, choose, add);
EMBED_TABLE_STORAGE CliToolchainsIdFolderNs cli_toolchains_id_folder EMBED_UNUSED = {
    .choose = cli_toolchains_on_choose,
    .add = cli_toolchains_on_add,
};

typedef struct
{
    int (*choose)(const CliToolchainsIdCfg *args);
} CliToolchainsIdNs;
EMBED_TABLE_LAYOUT(CliToolchainsIdNs, choose);
EMBED_TABLE_STORAGE CliToolchainsIdNs cli_toolchains_id EMBED_UNUSED = {
    .choose = cli_toolchains_choose,
};

typedef struct
{
    void (*chosen_each)(const CliToolchainsContextCfg *args);
} CliToolchainsContextNs;
EMBED_TABLE_LAYOUT(CliToolchainsContextNs, chosen_each);
EMBED_TABLE_STORAGE CliToolchainsContextNs cli_toolchains_context EMBED_UNUSED = {
    .chosen_each = cli_toolchains_chosen_each,
};

typedef struct
{
    int (*chosen_program)(const CliToolchainsIdPathCfg *args);
} CliToolchainsIdPathNs;
EMBED_TABLE_LAYOUT(CliToolchainsIdPathNs, chosen_program);
EMBED_TABLE_STORAGE CliToolchainsIdPathNs cli_toolchains_id_path EMBED_UNUSED = {
    .chosen_program = cli_toolchains_chosen_program,
};

typedef struct
{
    void (*set_tree)(const CliToolchainsRootCfg *args);
} CliToolchainsRootNs;
EMBED_TABLE_LAYOUT(CliToolchainsRootNs, set_tree);
EMBED_TABLE_STORAGE CliToolchainsRootNs cli_toolchains_root EMBED_UNUSED = {
    .set_tree = cli_toolchains_set_tree,
};

typedef struct
{
    void (*set_environment)(const CliToolchainsEnvironmentCfg *args);
} CliToolchainsEnvironmentNs;
EMBED_TABLE_LAYOUT(CliToolchainsEnvironmentNs, set_environment);
EMBED_TABLE_STORAGE CliToolchainsEnvironmentNs cli_toolchains_environment_table EMBED_UNUSED = {
    .set_environment = cli_toolchains_set_environment,
};

typedef struct
{
    int (*environment)(const CliToolchainsCfg *args);
} CliToolchainsNs;
EMBED_TABLE_LAYOUT(CliToolchainsNs, environment);
EMBED_TABLE_STORAGE CliToolchainsNs cli_toolchains_table EMBED_UNUSED = {
    .environment = cli_toolchains_environment,
};

typedef struct
{
    int (*on_path)(const CliToolchainsNamesCfg *args);
} CliToolchainsNamesNs;
EMBED_TABLE_LAYOUT(CliToolchainsNamesNs, on_path);
EMBED_TABLE_STORAGE CliToolchainsNamesNs cli_toolchains_names EMBED_UNUSED = {
    .on_path = cli_toolchains_on_path,
};

typedef struct
{
    void (*run_path)(const CliToolchainsCountPathCfg *args);
    void (*run_path_with)(const CliToolchainsCountPathCfg *args);
} CliToolchainsCountPathNs;
EMBED_TABLE_LAYOUT(CliToolchainsCountPathNs, run_path, run_path_with);
EMBED_TABLE_STORAGE CliToolchainsCountPathNs cli_toolchains_count_path EMBED_UNUSED = {
    .run_path = cli_toolchains_run_path,
    .run_path_with = cli_toolchains_run_path_with,
};

typedef struct
{
    unsigned int (*folders_for)(const CliToolchainsJsonCountCfg *args);
} CliToolchainsJsonCountNs;
EMBED_TABLE_LAYOUT(CliToolchainsJsonCountNs, folders_for);
EMBED_TABLE_STORAGE CliToolchainsJsonCountNs cli_toolchains_json_count EMBED_UNUSED = {
    .folders_for = cli_toolchains_folders_for,
};

typedef struct
{
    int (*join)(const CliToolchainsFolderCfg *args);
} CliToolchainsFolderNs;
EMBED_TABLE_LAYOUT(CliToolchainsFolderNs, join);
EMBED_TABLE_STORAGE CliToolchainsFolderNs cli_toolchains_folder EMBED_UNUSED = {
    .join = cli_toolchains_join,
};

typedef struct
{
    int (*own)(const CliToolchainsErrMsgFolderOnPathCfg *args);
} CliToolchainsErrMsgFolderOnPathNs;
EMBED_TABLE_LAYOUT(CliToolchainsErrMsgFolderOnPathNs, own);
EMBED_TABLE_STORAGE CliToolchainsErrMsgFolderOnPathNs cli_toolchains_err_msg_folder_on_path EMBED_UNUSED = {
    .own = cli_toolchains_own,
};

typedef struct
{
    int (*add_to_path)(const CliToolchainsErrMsgFolderCfg *args);
} CliToolchainsErrMsgFolderNs;
EMBED_TABLE_LAYOUT(CliToolchainsErrMsgFolderNs, add_to_path);
EMBED_TABLE_STORAGE CliToolchainsErrMsgFolderNs cli_toolchains_err_msg_folder EMBED_UNUSED = {
    .add_to_path = cli_toolchains_add_to_path,
};

typedef struct
{
    void (*find)(const CliToolchainsJsonCfg *args);
} CliToolchainsJsonNs;
EMBED_TABLE_LAYOUT(CliToolchainsJsonNs, find);
EMBED_TABLE_STORAGE CliToolchainsJsonNs cli_toolchains_json EMBED_UNUSED = {
    .find = cli_toolchains_find,
};

typedef struct
{
    void (*check)(const CliToolchainsVersionsCfg *args);
} CliToolchainsVersionsNs;
EMBED_TABLE_LAYOUT(CliToolchainsVersionsNs, check);
EMBED_TABLE_STORAGE CliToolchainsVersionsNs cli_toolchains_versions EMBED_UNUSED = {
    .check = cli_toolchains_check,
};

#endif
