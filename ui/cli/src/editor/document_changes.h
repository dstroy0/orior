// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document_changes.h: the changes of a text against its file as the last commit has it, answered off the store's diff
// as they are asked for, nothing retained: the change a line is in, the change `at` among them, the one `by` changes
// on from a line or a change, the marks of a run of lines, and the edit that puts a change's older lines back. A change
// is four numbers: the newer text's lines from `now_from` to before `now_to`, in place of the older's from `then_from`
// to before `then_to`. The gutter marks a change on its newer lines, and a change that takes lines out alone on the
// line above them (document_changes.c)
#ifndef DOCUMENT_CHANGES_H
#define DOCUMENT_CHANGES_H
#include "document.h"
#include "embed_dispatch_layout.h"

typedef struct CliChangeCfg CliChangeCfg;

// no change; and a line's mark: none, lines put in, lines changed, and lines taken out after it
#define CLI_CHANGE_NONE 0xffffffffu
#define CLI_CHANGE_MARK_NONE 0u
#define CLI_CHANGE_MARK_ADDED 1u
#define CLI_CHANGE_MARK_CHANGED 2u
#define CLI_CHANGE_MARK_REMOVED 3u

typedef struct
{
    unsigned int now_from;
    unsigned int now_to;
    unsigned int then_from;
    unsigned int then_to;
} CliChange;

// the change `at` among the changes of the text stored under `key`, or where `at` is CLI_CHANGE_NONE the change the
// line `line` is in, into `change_out`, its place into `at_out` and how many changes are into `count_out`, each
// where not NULL: 1, or 0 where none is there, the count still given
int cli_document_change_find(const CliChangeCfg *args);
// the change `by` changes on from the change `at`, wrapping at the ends, or where `at` is CLI_CHANGE_NONE the first
// change that starts below the line `line` going down and the last that starts above it going up, the ends wrapping:
// 1 with it into `change_out` and its place into `at_out`, or 0 where no change is
int cli_document_change_step(const CliChangeCfg *args);
// the marks of the `count` lines from `line`, one byte a line into `marks_out`, CLI_CHANGE_MARK_NONE where a line
// is in no change
void cli_document_change_marks(const CliChangeCfg *args);
// the edit that puts the older lines of `change` back into the text stored under `key`, its text in a temporary cell
// the caller gives back with cli_document_change_revert_end once the edit is applied: 1 with the edit into `edit_out`,
// or 0 with why written into `err_msg` where the cells cannot meet it
int cli_document_change_revert(const CliChangeCfg *args);
void cli_document_change_revert_end(const CliChangeCfg *args);
// the mark of `change`, and the first line the gutter marks it on
unsigned int cli_document_change_mark(const CliChangeCfg *args);
unsigned int cli_document_change_marked_from(const CliChangeCfg *args);
struct CliChangeCfg
{
    const char *key;
    unsigned long long line;
    unsigned int at;
    int by;
    unsigned int count;
    const CliChange *change;
    CliChange *change_out;
    unsigned int *at_out;
    unsigned int *count_out;
    unsigned char *marks_out;
    CliEdit *edit_out;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*find)(const CliChangeCfg *args);
    int (*step)(const CliChangeCfg *args);
    void (*marks)(const CliChangeCfg *args);
    int (*revert)(const CliChangeCfg *args);
    void (*revert_end)(const CliChangeCfg *args);
    unsigned int (*mark)(const CliChangeCfg *args);
    unsigned int (*marked_from)(const CliChangeCfg *args);
} CliDocumentChangeNs;
EMBED_TABLE_LAYOUT(CliDocumentChangeNs, find, step, marks, revert, revert_end, mark, marked_from);
EMBED_TABLE_STORAGE CliDocumentChangeNs cli_document_change EMBED_UNUSED = {
    .find = cli_document_change_find,
    .step = cli_document_change_step,
    .marks = cli_document_change_marks,
    .revert = cli_document_change_revert,
    .revert_end = cli_document_change_revert_end,
    .mark = cli_document_change_mark,
    .marked_from = cli_document_change_marked_from,
};

#endif
