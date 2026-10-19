// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings.h: settings by file, each part of a file standing over the parts before it: the files the reader's
// settings.json takes, settings.json itself, the files a tree's `.orior/settings.json` takes, and that file
// (settings.c, settings_layers.c). A settings file is JSON, with comments and trailing commas as VS Code's files hold
// them, each key a setting as settings.json keeps it (kept.c), and these keys of its own:
//
// - `takes`: the path of another settings file, from the file's own folder, whose settings this one takes, its own
//   standing over them;
// - `windows`, `linux` and `macos`: settings for that system alone, standing over the file's others;
// - `machine`, in the reader's own settings.json: settings for this machine alone, standing over the file's others and
//   its system's, and never carried to another machine;
// - `[kind]`, as `[markdown]`: the theme of each scheme, `theme.dark` and `theme.light`, the editor shows the files of
//   the language `kind` in.
//
// Text in a value names another setting's value with `${setting:name}`, a variable of the system's environment with
// `${env:NAME}`, the reader's home folder with `${home}`, the tree's folder with `${tree}`, and the system with
// `${system}`. A tree's file does not set the settings the reader's alone are
#ifndef SETTINGS_H
#define SETTINGS_H

#include "../text/json_tree.h"

// the tree's settings file, from its top folder; the most files one chain of `takes` reads; and the most settings one
// `${setting:name}` reads through
#define CLI_SETTINGS_TREE ".orior/settings.json"

#define CLI_SETTINGS_DEEPEST 16u

#define CLI_SETTINGS_NAMES_DEEP 8u

// the most nodes and bytes of one chain's files, and the most problems a chain or the layers tell
#define CLI_SETTINGS_NODES (1u << 16u)

#define CLI_SETTINGS_TEXT (1u << 21u)

#define CLI_SETTINGS_PROBLEMS 64u

// The files of one chain, the first taken by the second and so on to the last, each the path it is shown by and the
// object of every key it holds, and what could not be read
typedef struct
{
    CliJson json;
    CliJsonNode node[CLI_SETTINGS_NODES];
    char text[CLI_SETTINGS_TEXT];
    char file[CLI_SETTINGS_DEEPEST][CLI_PATH_LONGEST];
    unsigned int values[CLI_SETTINGS_DEEPEST];
    unsigned int files;
    char problem[CLI_SETTINGS_PROBLEMS][CLI_PATH_LONGEST * 2u];
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

// A setting of a kind of file: the kind, its key and its text
typedef struct
{
    unsigned int kind;
    unsigned int key;
    unsigned int text;
} CliKindText;

// The settings in force, as the page reads them over its own entries, each by its key without `orior.`: `over`, the
// text of each a file stands over the reader's own settings.json with, and of each settings.json sets through a
// variable, those in `own`; `under`, the text of each only the files settings.json takes set; `from`, each part of a
// file that sets a setting, the one in force first, a kind's settings under `[kind] key`; `kinds`, the settings of each
// kind of file; `own_file`, the reader's own settings.json as `from` names it, where it is there; and the files read,
// the problems met and the system
typedef struct
{
    char own_file[CLI_PATH_LONGEST];
    int has_own_file;
    CliLayerText over[CLI_LAYERS_KEYS];
    unsigned int overs;
    CliLayerText under[CLI_LAYERS_KEYS];
    unsigned int unders;
    unsigned int own[CLI_LAYERS_KEYS];
    unsigned int owns;
    CliLayerSource from[CLI_LAYERS_SOURCES];
    unsigned int froms;
    CliKindText kind[CLI_LAYERS_KEYS];
    unsigned int kinds;
    unsigned int file[CLI_SETTINGS_DEEPEST * 2u];
    unsigned int files;
    unsigned int problem[CLI_SETTINGS_PROBLEMS * 2u];
    unsigned int problems;
    const char *system;
    char text[CLI_SETTINGS_TEXT];
    unsigned long long used;
} CliLayers;

// the system orior runs on, as a settings file names it; and whether `key` is one of a settings file's own keys and no
// setting: `takes`, a system's, `machine` or a kind's
const char *cli_settings_system(void);
int cli_settings_is_file_key(const char *key);
// the file at `path` and each it takes, the one taken first, each shown by its path from `root` where it is under it,
// else as it is; a file not there gives none; a file taken that is not there, or that a chain takes twice, is a
// problem. The tree's settings, its file and each it takes, shown from the tree's top folder; and the reader's,
// settings.json in orior's own folder and each file it takes. Each into `chain` (settings.c)
void cli_settings_chain(const char *path, const char *root, CliChain *chain);
void cli_settings_tree(const char *root, CliChain *chain);
void cli_settings_reader(CliChain *chain);
// a chain written as the window reads it, `{"files": [{"file", "values"}], "problems"}`, after `out`; and a chain the
// window hands back, so written, read into `chain`: 1, or 0 with why said (settings.c)
void cli_settings_chain_written(const CliChain *chain, CliLine *out);
int cli_settings_chain_read(const char *text, unsigned long long length, CliChain *chain, CliLine *said);
// the settings in force from the reader's chain and the tree's, for the tree at `root`, into `layers`; the layers
// written as the window reads them after `out`; and the text of `key` the layers set over the reader's own, or under
// it, NULL where they set none (settings_layers.c)
void cli_settings_layers(const CliChain *reader, const CliChain *tree, const char *root, CliLayers *layers);
void cli_settings_layers_written(const CliLayers *layers, CliLine *out);
const char *cli_settings_layer(const CliLayers *layers, const char *key, int over);

#endif
