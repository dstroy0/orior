// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// identity_settings.h: the settings the query crate gives the identity, each a record of the run appended before the
// identity starts, the latest of each read (identity_settings.oracle.tsv)
#ifndef IDENTITY_SETTINGS_H
#define IDENTITY_SETTINGS_H

// the seed the order of a magnitude's links is drawn from, one word
#define IDENTITY_SEED "identity.seed"

// the path of the ruleset the identity writes forms in, from the top of the tree, its bytes without an end
#define IDENTITY_RULESET "identity.ruleset"

// the vendor's writer the identity hands texts to, an IdentityWriter
#define IDENTITY_WRITER "identity.writer"

// the longest path a setting names, and the longest part
#define IDENTITY_PATH_LONGEST 1024u
#define IDENTITY_PART_LONGEST 64u

// the vendor's writer: its program, the part it writes for, and the part's layout and mnemonics files, each ended by a
// NUL within its room
typedef struct
{
    char program[IDENTITY_PATH_LONGEST];
    char part[IDENTITY_PART_LONGEST];
    char layout[IDENTITY_PATH_LONGEST];
    char mnemonics[IDENTITY_PATH_LONGEST];
} IdentityWriter;

#ifdef __cplusplus
static_assert(sizeof(IdentityWriter) == 3136u, "IdentityWriter must be the 3136 bytes of its record");
#else
_Static_assert(sizeof(IdentityWriter) == 3136u, "IdentityWriter must be the 3136 bytes of its record");
#endif

#endif
