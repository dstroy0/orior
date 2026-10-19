// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_internal.h: what settings.c and settings_layers.c share
#ifndef SETTINGS_INTERNAL_H
#define SETTINGS_INTERNAL_H

#include "settings.h"

// the kind a `[kind]` key names, its letters from `*kind`, `*length` of them: 1, or 0 where the key names none;
// whether a tree's file does not set `key`; the reader's home folder into `out`, which holds CLI_PATH_LONGEST, empty
// where there is none; and `length` bytes of `text` with each `${name}` that `look` reads put in its place, one it does
// not read staying as it is, after `out`
int cli_settings_kind_of(const char *key, const char **kind, unsigned long long *length);
int cli_settings_held(const char *key);
void cli_settings_home(char *out);
void cli_settings_expand(const char *text, unsigned long long length,
                         int (*look)(void *context, const char *name, unsigned long long length, CliLine *out),
                         void *context, CliLine *out);

// the most parts of files read, the most settings they hold together, and the most bytes of a setting's text
#define CLI_LAYER_PARTS 128u

#define CLI_LAYER_RAW 8192u

#define CLI_LAYER_VALUE (1u << 16u)

// Where a part stands: under the reader's own settings.json, as settings.json itself, or over it
enum
{
    CLI_RANK_UNDER,
    CLI_RANK_OWN,
    CLI_RANK_OVER
};

// A part of a file: the chain and the file it is of, the system it is for where it is one, the settings it sets,
// `count` of the values from `first`, and where it stands
typedef struct
{
    const CliChain *chain;
    unsigned int file;
    const char *part;
    unsigned int first;
    unsigned int count;
    unsigned int rank;
} CliPart;

// A setting a part sets: its key and the node of its value
typedef struct
{
    const char *key;
    unsigned int value;
} CliPartValue;

// A kind of file's settings: the chain and the file that sets them, the kind's key, and the object of them
typedef struct
{
    const CliChain *chain;
    unsigned int file;
    const char *key;
    unsigned int object;
} CliKindSet;

// a text put in the layers' texts: its place; and a problem put after the layers', told once (settings_layers.c)
unsigned int cli_settings_kept_text(CliLayers *layers, const char *text, unsigned long long length);
void cli_settings_problem(CliLayers *layers, const char *problem);
// where the parts, their values and the kinds' settings are kept; and the parts of the file `file` of `chain` put
// after the parts, its values after `*values` and its kinds' settings after `*kinds`: the parts' count after
// (settings_parts.c)
void cli_settings_parts_kept(CliPart *parts, CliPartValue *values, CliKindSet *kinds);
unsigned int cli_settings_file_parts(CliLayers *layers, const CliChain *chain, unsigned int file, int of_tree, int own,
                                     unsigned int parts, unsigned int *values, unsigned int *kinds);

#endif
