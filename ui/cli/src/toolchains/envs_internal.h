// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_internal.h: what the environments' modules share: the names a folder requires, read and written (envs.c)
#ifndef ENVS_INTERNAL_H
#define ENVS_INTERNAL_H
typedef struct CliEnvsCtx CliEnvsCtx;
typedef struct CliEnvsOutCtx CliEnvsOutCtx;

#include "embed_dispatch_layout.h"
#include "envs.h"

// the most names one folder requires and the bytes of them, and the names and the bytes their capacities are taken with
#define CLI_ENVS_NAMES 1024u
#define CLI_ENVS_TEXT (1u << 20u)

// A name required: where it starts in the text, and its bytes
typedef struct
{
    unsigned int start;
    unsigned int length;
} CliRequiredName;

// The names a folder requires and their texts, each in a cell at their most, given back with cli_required_free
typedef struct
{
    CliRequiredName *name;
    unsigned int count;
    unsigned int capacity;
    char *text;
    unsigned int used;
    unsigned int text_capacity;
} CliRequired;

// the names a folder's files require read into `required`, those it contained before let go; the names written as a
// JSON array; and their cells given back, the names left empty
void cli_required_alloc(const CliEnvsCtx *args);
void cli_required_in(const CliEnvsCtx *args);
void cli_required_free(const CliEnvsCtx *args);

struct CliEnvsCtx
{
    CliRequired *required;
    const char *folder;
};

struct CliEnvsOutCtx
{
    const CliRequired *required_read;
    VerbaLinea *out;
};

typedef struct
{
    void (*required_alloc)(const CliEnvsCtx *args);
    void (*required_in)(const CliEnvsCtx *args);
    void (*required_free)(const CliEnvsCtx *args);
} CliEnvsNsTable;
EMBED_TABLE_LAYOUT(CliEnvsNsTable, required_alloc, required_in, required_free);
EMBED_TABLE_STORAGE CliEnvsNsTable cli_envs_internal EMBED_UNUSED = {
    .required_alloc = cli_required_alloc,
    .required_in = cli_required_in,
    .required_free = cli_required_free,
};

#endif
