// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings.h: settings by file, each part of a file standing over the parts before it: the files the reader's
// settings.json takes, settings.json itself, the files a tree's `.orior/settings.json` takes, and that file
// (settings.c, settings_layers.c). A settings file is JSON, with comments and trailing commas as VS Code's files write
// them, each key a setting as settings.json stores it (state.c), and these keys of its own:
//
// - `allocs`: the path of another settings file, from the file's own folder, whose settings this one takes, its own
//   standing over them;
// - `windows`, `linux` and `macos`: settings for that system alone, standing over the file's others;
// - `machine`, in the reader's own settings.json: settings for this machine alone, standing over the file's others and
//   its system's, and never carried to another machine;
// - `[type]`, as `[markdown]`: the theme of each scheme, `theme.dark` and `theme.light`, the editor shows the files of
//   the language `type` in.
//
// Text in a value names another setting's value with `${setting:name}`, a variable of the system's environment with
// `${env:NAME}`, the reader's home folder with `${home}`, the tree's folder with `${tree}`, and the system with
// `${system}`. A tree's file does not set the settings the reader's alone are
#ifndef SETTINGS_H
#define SETTINGS_H
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliSettingsChainCfg CliSettingsChainCfg;
typedef struct CliSettingsRootCfg CliSettingsRootCfg;
typedef struct CliSettingsPathCfg CliSettingsPathCfg;
typedef struct CliSettingsKeyCfg CliSettingsKeyCfg;
typedef struct CliSettingsCfg CliSettingsCfg;
typedef struct CliSettingsOutCfg CliSettingsOutCfg;

// the tree's settings file, from its top folder; the most files one chain of `allocs` reads; and the most settings one
// `${setting:name}` reads through
#define CLI_SETTINGS_TREE ".orior/settings.json"

#define CLI_SETTINGS_DEEPEST 16u

#define CLI_SETTINGS_NAMES_DEEP 8u

// the most bytes of the layers' texts, and the most problems a chain or the layers report
#define CLI_SETTINGS_TEXT (1u << 21u)

#define CLI_SETTINGS_PROBLEMS 64u

// The files of one chain, the first taken by the second and so on to the last, read into one tree in a cell of their
// tokens and bytes, each the path it is shown by and the object of every key it contains, and what could not be read
typedef struct
{
    CliJson json;
    char file[CLI_SETTINGS_DEEPEST][CLI_PATH_MAX_LEN];
    unsigned int values[CLI_SETTINGS_DEEPEST];
    unsigned int files;
    char problem[CLI_SETTINGS_PROBLEMS][CLI_PATH_MAX_LEN * 2u];
    unsigned int problems;
} CliChain;

// the most keys the layers set, and the most parts of files that set them
#define CLI_LAYERS_KEYS 4096u

#define CLI_LAYERS_SOURCES 8192u

// A key's text in force, both places in the layers' texts
typedef struct
{
    unsigned int key;
    unsigned int text;
} CliLayerText;

// A part of a file that sets a setting: the key it sets, the file, the system the part is for where it is one, and the
// text it sets the setting to
typedef struct
{
    unsigned int key;
    unsigned int file;
    unsigned int part;
    int has_part;
    unsigned int text;
} CliLayerSource;

// A setting of a type of file: the type, its key and its text
typedef struct
{
    unsigned int type;
    unsigned int key;
    unsigned int text;
} CliTypeText;

// The settings in force, as the page reads them over its own entries, each by its key without `orior.`: `over`, the
// text of each a file stands over the reader's own settings.json with, and of each settings.json sets through a
// variable, those in `own`; `under`, the text of each only the files settings.json takes set; `from`, each part of a
// file that sets a setting, the one in force first, a type's settings under `[type] key`; `types`, the settings of each
// type of file; `own_file`, the reader's own settings.json as `from` names it, where it is there; and the files read,
// the problems met and the system
typedef struct
{
    char own_file[CLI_PATH_MAX_LEN];
    int has_own_file;
    CliLayerText over[CLI_LAYERS_KEYS];
    unsigned int overs;
    CliLayerText under[CLI_LAYERS_KEYS];
    unsigned int unders;
    unsigned int own[CLI_LAYERS_KEYS];
    unsigned int owner;
    CliLayerSource from[CLI_LAYERS_SOURCES];
    unsigned int froms;
    CliTypeText type[CLI_LAYERS_KEYS];
    unsigned int types;
    unsigned int file[CLI_SETTINGS_DEEPEST * 2u];
    unsigned int files;
    unsigned int problem[CLI_SETTINGS_PROBLEMS * 2u];
    unsigned int problems;
    const char *system;
    char *text;
    unsigned long long used;
    unsigned long long capacity;
    CliGrownLine grown;
} CliLayers;

// the system impero runs on, as a settings file names it; and whether `key` is one of a settings file's own keys and
// no setting: `allocs`, a system's, `machine` or a type's
const char *cli_settings_system(void);
int cli_settings_is_file_key(const CliSettingsKeyCfg *args);
// the file at `path` and each it takes, the one taken first, each shown by its path from `root` where it is under it,
// else as it is; a file not there gives none; a file taken that is not there, or that a chain takes twice, is a
// problem. The tree's settings, its file and each it takes, shown from the tree's top folder; and the reader's,
// settings.json in orior's own folder and each file it takes. Each into `chain`, which contains nothing before, its
// tree released through persistent_buf_release (settings.c)
void cli_settings_chain(const CliSettingsPathCfg *args);
void cli_settings_tree(const CliSettingsPathCfg *args);
void cli_settings_reader(const CliSettingsPathCfg *args);
// a chain written as the window reads it, `{"files": [{"file", "values"}], "problems"}`, after `out`; a chain the
// window hands back, so written, read into `chain`, which contains nothing before: 1, or 0 with why given; and a
// chain's tree given back, the chain left containing nothing (settings_chain_text.c)
int cli_settings_chain_read(const CliSettingsChainCfg *args);
// the settings in force from the reader's chain and the tree's, for the tree at `root`, into `layers`, which contain
// nothing before, their texts in a cell at their most; the layers' texts given back, the layers left containing none;
// the layers written as the window reads them after `out`; and the text of `key` the layers set over the reader's own,
// or under it, NULL where they set none (settings_layers.c)
void cli_settings_layers(const CliSettingsRootCfg *args);
const char *cli_settings_layer(const CliSettingsKeyCfg *args);

struct CliSettingsChainCfg
{
    CliChain *chain;
    const char *text;
    unsigned long long length;
    VerbaLinea *err_msg;
};

struct CliSettingsRootCfg
{
    const char *root;
    const CliChain *reader;
    const CliChain *tree;
    CliLayers *layers;
};

struct CliSettingsPathCfg
{
    const char *path;
    const char *root;
    CliChain *chain;
};

struct CliSettingsKeyCfg
{
    const char *key;
    const CliLayers *layers_read;
    int over;
};

struct CliSettingsCfg
{
    const CliChain *chain_read;
    VerbaLinea *out;
};

struct CliSettingsOutCfg
{
    VerbaLinea *out;
    const CliLayers *layers_read;
};

typedef struct
{
    int (*is_file_key)(const CliSettingsKeyCfg *args);
    const char *(*layer)(const CliSettingsKeyCfg *args);
} CliSettingsKeyNs;
EMBED_TABLE_LAYOUT(CliSettingsKeyNs, is_file_key, layer);
EMBED_TABLE_STORAGE CliSettingsKeyNs cli_settings_key EMBED_UNUSED = {
    .is_file_key = cli_settings_is_file_key,
    .layer = cli_settings_layer,
};

typedef struct
{
    void (*chain)(const CliSettingsPathCfg *args);
    void (*tree)(const CliSettingsPathCfg *args);
    void (*reader)(const CliSettingsPathCfg *args);
} CliSettingsPathNs;
EMBED_TABLE_LAYOUT(CliSettingsPathNs, chain, tree, reader);
EMBED_TABLE_STORAGE CliSettingsPathNs cli_settings_path_table EMBED_UNUSED = {
    .chain = cli_settings_chain,
    .tree = cli_settings_tree,
    .reader = cli_settings_reader,
};

typedef struct
{
    int (*chain_read)(const CliSettingsChainCfg *args);
} CliSettingsChainNs;
EMBED_TABLE_LAYOUT(CliSettingsChainNs, chain_read);
EMBED_TABLE_STORAGE CliSettingsChainNs cli_settings_chain_table EMBED_UNUSED = {
    .chain_read = cli_settings_chain_read,
};

typedef struct
{
    void (*layers)(const CliSettingsRootCfg *args);
} CliSettingsRootNs;
EMBED_TABLE_LAYOUT(CliSettingsRootNs, layers);
EMBED_TABLE_STORAGE CliSettingsRootNs cli_settings_root EMBED_UNUSED = {
    .layers = cli_settings_layers,
};

#endif
