// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_internal.h: a text stored as document_store.c stores it and document_branch.c colors it
#ifndef DOCUMENT_INTERNAL_H
#define DOCUMENT_INTERNAL_H
#include "document.h"
#include "embed_atomic.h"
#include "embed_dispatch_layout.h"

typedef struct CliDocumentCtx CliDocumentCtx;
typedef struct CliDocumentKeyCtx CliDocumentKeyCtx;

// the line past the last of a text read whole whose runs are handed on: every line's
#define CLI_DOC_CACHED_ALL 0xffffffffu

// the most colorings of one text whose lines are claimed at once; a coloring past them claims none, its lines colored
// again by any that comes to them
#define CLI_DOC_CLAIMS 16u

// a line's regions as the walk records them, read off its leaves by the roles its grammar's names give them: the
// regions it opens and leaves open, in the low bits; above them those opened above it that it closes; above those how
// many of them it closes before any leaf stands on it, each count no more than CLI_DOC_REGION_COUNT; whether it holds
// a leaf, a run its grammar names of one letter or more; whether every leaf on it closes a pair, ends a statement or
// parts one; and whether a mark of no letters closes the first region it closes
#define CLI_DOC_REGION_COUNT 0x1ffu
#define CLI_DOC_REGION_CLOSES_AT 9u
#define CLI_DOC_REGION_UNLED_AT 18u
#define CLI_DOC_REGION_FILLED 0x08000000u
#define CLI_DOC_REGION_CLOSERS 0x10000000u
#define CLI_DOC_REGION_MARK 0x20000000u

// A text stored, a cell of its own taken as it is first stored and given back as it is let go, as many stored as the
// cells meet: the next stored, its key, its lines in a capacity of their own, whether the last commit's file is known
// and whether the commit stores one, and that file's lines in a capacity of their own; and as it is colored, the key of
// the grammar it is colored by, empty for none, and how many names that grammar's tokens take, the states its lines are
// colored from and end in, each written once and ended by a NUL in a cell at their most, a line's state the place its
// text starts at, each state found again by its hash in an index at their most, each place its state's place plus 1, 0
// for none, with how many states there are and the place found last, the line the walk works out from, whether the
// cells refused its colors, the changes written to it since it was stored, which its colors are handed on with, and
// whether the names its grammar's tokens take were handed on since it was colored by the grammar; and the lines from
// `cached_from` to before `cached_to`, those the page holds about its view, the only lines whose runs are handed on,
// every line until the page names them; and the letters of each line whose runs are handed on, as far as the view
// draws, 0 for every letter, each line colored whole and the next entered in the state it ends in; the roles of the
// names its grammar's tokens take, each by its name's place, CLI_DOC_ROLES_KNOWN with the roles once read and 0 before,
// and above them from CLI_DOC_RANK_AT one more than how many names before it count the depth of a pair, 0 before it is
// read, in a cell at their most; whether its lines' regions moved since its folds were handed on; and the runs the walk
// found on its lines, saved as it colors them, each line's how many, then their columns, then their names as the walk
// names them, a line's place among them in its `runs`, in a cell taken again at the runs its lines save and a share
// past them where a line's runs do not fit, and how much of the capacity is written. A paged text saves the runs of the
// lines the page holds alone. Its lines are colored with the lock given back: a coloring claims lines under the lock,
// colors them with no lock taken and puts what it found in under the lock again. For that the text saves the serial its
// lines and states stand at, a number no text took before, taken anew each time they are stored again, edited or
// colored by another grammar, so a coloring that finds it moved puts nothing in; how many colorings, walks and saves
// read it on threads of their own, counted atomically, whether it was let go while one did, its cells freed on the
// controller's thread once the last ends, whether its pages go with it as it is freed, a paged text released with its
// tab, and the next text let go while read; whether a
// walker walks it, one at a time, and whether the walk found nothing to color on it since it last moved; whether every
// name of its grammar has its roles read; and the lines each coloring under way claims, from `claim_from` to before
// `claim_to`, a place free where the two are the same, which the walk leaves to it. A paged text is a branch the walk
// reads through the pages, the text stored there under the same key: its capacity holds each line's two states, its
// regions and its runs' place and no units, and its lines are as many as the pages' index has read and whole once it is
typedef struct CliStoredText
{
    struct CliStoredText *next;
    char key[CLI_NAME_MAX_LEN];
    CliLines lines;
    int head_known;
    int head_has;
    CliLines head;
    char grammar[CLI_NAME_MAX_LEN];
    unsigned int names;
    char *state;
    unsigned int state_used;
    unsigned int state_capacity;
    unsigned int *state_index;
    unsigned int state_indexed;
    unsigned int state_count;
    unsigned int state_last;
    unsigned int origin;
    int refused;
    unsigned long long changes;
    int named;
    int paged;
    int whole;
    unsigned int cached_from;
    unsigned int cached_to;
    unsigned long long columns;
    unsigned int *roles;
    unsigned int roles_capacity;
    int regions_moved;
    unsigned int *run;
    unsigned int run_used;
    unsigned int run_capacity;
    unsigned long long serial;
    embed_atomic_u32 readers;
    int let_go;
    int pages_release;
    struct CliStoredText *released_next;
    int walked;
    int resting;
    int roles_complete;
    unsigned int claim_from[CLI_DOC_CLAIMS];
    unsigned int claim_to[CLI_DOC_CLAIMS];
} CliStoredText;

// a name's roles read and saved among a text's roles; where its rank among the names that count the depth of a pair is
// saved above them, and the most ranks saved
#define CLI_DOC_ROLES_KNOWN 0x8000u
#define CLI_DOC_RANK_AT 16u
#define CLI_DOC_RANKS 0xfffeu

// the lock the texts are read and written under taken and given back; the first of the texts stored; the text stored
// under `key`, or NULL; and the lines of a paged text counted again as far as the pages' index has read, each line it
// had not counted not colored: 1, or 0 where the cells cannot meet their states. Each under the lock (document_store.c)
CliStoredText *cli_document_texts(void);
CliStoredText *cli_document_on_store(const CliDocumentKeyCtx *args);
int cli_document_paged_count(const CliDocumentCtx *args);
// the serial of `one` taken anew, a number no text took before; `one` saved across the lock given back, and let go of,
// its cells given back where it was let go while saved and this was the last to save it. Each under the lock
// (document_store.c)
void cli_document_serial_move(const CliDocumentCtx *args);
void cli_document_holder_add(const CliDocumentCtx *args);
void cli_document_holder_end(const CliDocumentCtx *args);
// the walk of the texts' colors woken for `one`, which moved: a walker started on a thread of its own where the walk
// runs on threads, another text than those walked has lines to color, and fewer walk than the machine has processors
// to spare; under the lock (document_branch.c)
void cli_document_wake(const CliDocumentCtx *args);

struct CliDocumentCtx
{
    CliStoredText *one;
};

struct CliDocumentKeyCtx
{
    const char *key;
};

typedef struct
{
    CliStoredText *(*store)(const CliDocumentKeyCtx *args);
} CliDocumentKeyNsTable;
EMBED_TABLE_LAYOUT(CliDocumentKeyNsTable, store);
EMBED_TABLE_STORAGE CliDocumentKeyNsTable cli_document_key_internal EMBED_UNUSED = {
    .store = cli_document_on_store,
};

typedef struct
{
    int (*paged_count)(const CliDocumentCtx *args);
    void (*serial_move)(const CliDocumentCtx *args);
    void (*holder_add)(const CliDocumentCtx *args);
    void (*holder_end)(const CliDocumentCtx *args);
    void (*wake)(const CliDocumentCtx *args);
} CliDocumentNsTable;
EMBED_TABLE_LAYOUT(CliDocumentNsTable, paged_count, serial_move, holder_add, holder_end, wake);
EMBED_TABLE_STORAGE CliDocumentNsTable cli_document_internal EMBED_UNUSED = {
    .paged_count = cli_document_paged_count,
    .serial_move = cli_document_serial_move,
    .holder_add = cli_document_holder_add,
    .holder_end = cli_document_holder_end,
    .wake = cli_document_wake,
};

#endif
