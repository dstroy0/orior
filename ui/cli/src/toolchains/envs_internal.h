// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs_internal.h: what the environments' modules share: the names a folder requires, read and written (envs.c)
#ifndef ENVS_INTERNAL_H
#define ENVS_INTERNAL_H

#include "envs.h"

// the most names one folder requires and the bytes of them, and the names and the bytes their rooms are taken with
#define CLI_ENVS_NAMES 1024u
#define CLI_ENVS_TEXT (1u << 20u)

// A name required: where it starts in the text, and its bytes
typedef struct
{
    unsigned int start;
    unsigned int length;
} CliRequiredName;

// The names a folder requires and their texts, each in a cell at their most, given back with cli_required_given
typedef struct
{
    CliRequiredName *name;
    unsigned int count;
    unsigned int room;
    char *text;
    unsigned int used;
    unsigned int text_room;
} CliRequired;

// the names a folder's files require read into `required`, those it contained before let go; the names written as a
// JSON array; and their cells given back, the names left empty
void cli_required_taken(CliRequired *required);
void cli_required_in(const char *folder, CliRequired *required);
void cli_required_written(const CliRequired *required, VerbaLinea *out);
void cli_required_given(CliRequired *required);

#endif
