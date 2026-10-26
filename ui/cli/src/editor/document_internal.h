// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_internal.h: a text stored as document_store.c stores it and document_branch.c colors it
#ifndef DOCUMENT_INTERNAL_H
#define DOCUMENT_INTERNAL_H

#include "document.h"

// the line past the last of a text read whole whose runs are handed on: every line's
#define CLI_DOC_HELD_ALL 0xffffffffu

// A text stored, a cell of its own taken as it is first stored and given back as it is let go, as many stored as the
// cells meet: the next stored, its key, its lines in a room of their own, whether the last commit's file is known and
// whether the commit stores one, and that file's lines in a room of their own; and as it is colored, the key of the
// grammar it is colored by, empty for none, the states its lines are
// colored from and end in, each written once and ended by a NUL in a cell whose room doubles as they come, a line's
// state the place its text starts at, the line the walk works out from, whether the cells refused its colors, the
// changes written to it since it was stored, which its colors are handed on with, and whether the names its grammar's
// tokens take were handed on since it was colored by the grammar; and the lines from `held_from` to before `held_to`,
// those the page holds about its view, the only lines whose runs are handed on, every line until the page names them.
// A paged text is a branch the walk reads through the pages, the text stored there under the same key: its room holds
// each line's two states and no units, and its lines are as many as the pages' index has read and whole once it is
typedef struct CliStoredText
{
    struct CliStoredText *next;
    char key[CLI_NAME_MAX_LEN];
    CliLines lines;
    int head_known;
    int head_has;
    CliLines head;
    char grammar[CLI_NAME_MAX_LEN];
    char *state;
    unsigned int state_used;
    unsigned int state_room;
    unsigned int origin;
    int refused;
    unsigned long long changes;
    int named;
    int paged;
    int whole;
    unsigned int held_from;
    unsigned int held_to;
} CliStoredText;

// the lock the texts are read and written under taken and given back; the first of the texts stored; the text stored
// under `key`, or NULL; and the lines of a paged text counted again as far as the pages' index has read, each line it
// had not counted not colored: 1, or 0 where the cells cannot meet their states. Each under the lock (document_store.c)
void cli_document_lock_take(void);
void cli_document_lock_give(void);
CliStoredText *cli_document_texts(void);
CliStoredText *cli_document_stored(const char *key);
int cli_document_paged_counted(CliStoredText *one);
// the walk of the texts' colors woken, on a thread of its own where it walks on one and none is walking; under the lock
// (document_branch.c)
void cli_document_woken(void);

#endif
