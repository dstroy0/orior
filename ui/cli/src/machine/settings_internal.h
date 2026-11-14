// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_internal.h: what settings.c and settings_layers.c share
#ifndef SETTINGS_INTERNAL_H
#define SETTINGS_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "embed_dispatch_layout.h"
#include "settings.h"

typedef struct CliSettingsLayersCtx CliSettingsLayersCtx;
typedef struct CliSettingsTextCtx CliSettingsTextCtx;
typedef struct CliSettingsKeyCtx CliSettingsKeyCtx;
typedef struct CliSettingsPartsCtx CliSettingsPartsCtx;
typedef struct CliSettingsTextLengthCtx CliSettingsTextLengthCtx;
typedef struct CliSettingsLayersProblemCtx CliSettingsLayersProblemCtx;
typedef struct CliSettingsCtx CliSettingsCtx;

// the type a `[type]` key names, its characters from `*type`, `*length` of them: 1, or 0 where the key names none;
// whether a tree's file does not set `key`; the reader's home folder into `out`, sized for CLI_PATH_MAX_LEN, empty
// where there is none; and `length` bytes of `text` with each `${name}` that `look` reads put in its place, one it does
// not read staying as it is, after `out`
int cli_settings_type_get(const CliSettingsKeyCtx *args);
int cli_settings_tree_scope(const CliSettingsKeyCtx *args);
void cli_settings_home(const CliSettingsCtx *args);
void cli_settings_expand(const CliSettingsTextCtx *args);

// the max parts of files read, the max settings they contain together, and the max bytes of a setting's text
#define CLI_LAYER_PARTS 128u

#define CLI_LAYER_RAW 8192u

#define CLI_LAYER_VALUE (1u << 16u)

// Where a part is: under the reader's own settings.json, as settings.json itself, or over it
enum
{
    CLI_RANK_UNDER,
    CLI_RANK_OWN,
    CLI_RANK_OVER
};

// A part of a file: the chain and the file it is of, the system it is for where it is one, the settings it sets,
// `count` of the values from `first`, and where it is
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

// A type of file's settings: the chain and the file that sets them, the type's key, and the object of them
typedef struct
{
    const CliChain *chain;
    unsigned int file;
    const char *key;
    unsigned int object;
} CliTypeSet;

// a text put in the layers' texts: its place; and a problem put after the layers', reported once (settings_layers.c)
unsigned int cli_settings_text_store(const CliSettingsTextLengthCtx *args);
void cli_settings_problem(const CliSettingsLayersProblemCtx *args);
// where the parts, their values and the types' settings are stored; and the parts of the file `file` of `chain` put
// after the parts, its values after `*values` and its types' settings after `*types`: the parts' count after
// (settings_parts.c)
void cli_settings_parts_store(const CliSettingsPartsCtx *args);
unsigned int cli_settings_file_parts(const CliSettingsLayersCtx *args);

struct CliSettingsLayersCtx
{
    CliLayers *layers;
    const CliChain *chain;
    unsigned int file;
    int of_tree;
    int own;
    unsigned int parts_u32;
    unsigned int *values_u32s;
    unsigned int *types_u32s;
};

struct CliSettingsTextCtx
{
    const char *text;
    unsigned long long length;
    int (*look)(const CliEachNameLengthOutCfg *args);
    void *context;
    VerbaLinea *out_verba_linea_pointer;
};

struct CliSettingsKeyCtx
{
    const char *key;
    const char **type;
    unsigned long long *length_pointer;
};

struct CliSettingsPartsCtx
{
    CliPart *parts;
    CliPartValue *values;
    CliTypeSet *types;
};

struct CliSettingsTextLengthCtx
{
    const char *text;
    unsigned long long length;
    CliLayers *layers;
};

struct CliSettingsLayersProblemCtx
{
    CliLayers *layers;
    const char *problem;
};

struct CliSettingsCtx
{
    char *out;
};

typedef struct
{
    int (*type_get)(const CliSettingsKeyCtx *args);
    int (*tree_scope)(const CliSettingsKeyCtx *args);
} CliSettingsKeyNsTable;
EMBED_TABLE_LAYOUT(CliSettingsKeyNsTable, type_get, tree_scope);
EMBED_TABLE_STORAGE CliSettingsKeyNsTable cli_settings_key_internal EMBED_UNUSED = {
    .type_get = cli_settings_type_get,
    .tree_scope = cli_settings_tree_scope,
};

typedef struct
{
    void (*home)(const CliSettingsCtx *args);
} CliSettingsNsTable;
EMBED_TABLE_LAYOUT(CliSettingsNsTable, home);
EMBED_TABLE_STORAGE CliSettingsNsTable cli_settings_internal EMBED_UNUSED = {
    .home = cli_settings_home,
};

typedef struct
{
    void (*expand)(const CliSettingsTextCtx *args);
} CliSettingsTextNs;
EMBED_TABLE_LAYOUT(CliSettingsTextNs, expand);
EMBED_TABLE_STORAGE CliSettingsTextNs cli_settings_text_internal EMBED_UNUSED = {
    .expand = cli_settings_expand,
};

typedef struct
{
    unsigned int (*text_store)(const CliSettingsTextLengthCtx *args);
} CliSettingsTextLengthNs;
EMBED_TABLE_LAYOUT(CliSettingsTextLengthNs, text_store);
EMBED_TABLE_STORAGE CliSettingsTextLengthNs cli_settings_text_length_internal EMBED_UNUSED = {
    .text_store = cli_settings_text_store,
};

typedef struct
{
    void (*problem)(const CliSettingsLayersProblemCtx *args);
} CliSettingsLayersProblemNs;
EMBED_TABLE_LAYOUT(CliSettingsLayersProblemNs, problem);
EMBED_TABLE_STORAGE CliSettingsLayersProblemNs cli_settings_layers_problem_internal EMBED_UNUSED = {
    .problem = cli_settings_problem,
};

typedef struct
{
    void (*parts_store)(const CliSettingsPartsCtx *args);
} CliSettingsPartsNs;
EMBED_TABLE_LAYOUT(CliSettingsPartsNs, parts_store);
EMBED_TABLE_STORAGE CliSettingsPartsNs cli_settings_parts_internal EMBED_UNUSED = {
    .parts_store = cli_settings_parts_store,
};

typedef struct
{
    unsigned int (*file_parts)(const CliSettingsLayersCtx *args);
} CliSettingsLayersNs;
EMBED_TABLE_LAYOUT(CliSettingsLayersNs, file_parts);
EMBED_TABLE_STORAGE CliSettingsLayersNs cli_settings_layers_internal EMBED_UNUSED = {
    .file_parts = cli_settings_file_parts,
};

#endif
