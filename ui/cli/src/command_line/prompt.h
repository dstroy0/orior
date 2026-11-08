// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// prompt.h: the one ask the foot tools share, standing in a foot pane's rows in place of the page's sheets: a sentence,
// the statement it asks about in the code face, a field and a row of buttons. The ask's texts stand in one cell sized
// to them, the rows are numbers over it, and the field stands in a cell of its own sized to the letters typed; a
// password field answers the count of its letters and never its text. A button pressed hands the field's text and the
// button's place to the continuation the ask opened with, and the ask closes (prompt.c)
#ifndef PROMPT_H
#define PROMPT_H
#include "../text/typed.h"
#include "cli_internal.h"
#include "embed_dispatch_layout.h"
#include "pool.h"

typedef struct CliPromptCfg CliPromptCfg;
typedef struct CliPromptRowsCfg CliPromptRowsCfg;
typedef struct CliPromptInputCfg CliPromptInputCfg;

// the asks: a sentence over a field, a sentence over a statement with buttons, and a sentence over a password's field
// with buttons
#define CLI_PROMPT_ASK_LINE 0u
#define CLI_PROMPT_CONFIRM 1u
#define CLI_PROMPT_PASSWORD 2u

// the rows' types: the sentence, the statement, the field and the buttons
#define CLI_PROMPT_ROW_SENTENCE 0u
#define CLI_PROMPT_ROW_STATEMENT 1u
#define CLI_PROMPT_ROW_FIELD 2u
#define CLI_PROMPT_ROW_BUTTONS 3u

// the most buttons and the most bytes of a button's label, the most bytes of the statement shown and of the field, and
// the place answered by an ask closed with no button
#define CLI_PROMPT_CHOICES 4u
#define CLI_PROMPT_LABEL_MAX 256ULL
#define CLI_PROMPT_STATEMENT_MAX 600ULL
#define CLI_PROMPT_FIELD_MAX 4096ULL
#define CLI_PROMPT_NONE 0xFFFFFFFFu

// A row of the ask: its type; its text and bytes, NULL for a password's field; whether the field is a password's and
// how many letters it carries; and a buttons row's labels with their bytes, how many
typedef struct
{
    unsigned int type;
    const char *text;
    unsigned long long length;
    int secret;
    unsigned long long letters;
    const char *choice[CLI_PROMPT_CHOICES];
    unsigned long long choice_length[CLI_PROMPT_CHOICES];
    unsigned int choices;
} CliPromptRow;

// the ask opened of `mode` with `sentence`, `statement` cut to CLI_PROMPT_STATEMENT_MAX bytes where it is not NULL, the
// field begun with `preset` where it is not NULL, and `count` buttons `choices`, `then` called with `context` as a
// button is pressed, an ask open before closed with no answer: 1, or 0 where the cells cannot meet it; the ask closed
// with no answer, its cells released; the button `at` pressed: `then` handed `at`, the field's `text` and its `size`
// bytes and `context`, then the ask closed: 1, or 0 where no ask stands or `at` is past its buttons; and whether an
// ask stands, its continuation's `context` where `context` is not NULL
int cli_prompt_open(const CliPromptCfg *args);
void cli_prompt_close(const CliPromptCfg *args);
int cli_prompt_answer(const CliPromptCfg *args);
int cli_prompt_asked(const CliPromptCfg *args);
struct CliPromptCfg
{
    unsigned int mode;
    const char *sentence;
    const char *statement;
    unsigned long long statement_size;
    const char *preset;
    const char *const *choices;
    unsigned int count;
    void (*then)(const CliPromptCfg *args);
    void *context;
    unsigned int at;
    const char *text;
    unsigned long long size;
};

typedef struct
{
    int (*open)(const CliPromptCfg *args);
    void (*close)(const CliPromptCfg *args);
    int (*answer)(const CliPromptCfg *args);
    int (*asked)(const CliPromptCfg *args);
} CliPromptNs;
EMBED_TABLE_LAYOUT(CliPromptNs, open, close, answer, asked);
EMBED_TABLE_STORAGE CliPromptNs cli_prompt EMBED_UNUSED = {
    .open = cli_prompt_open,
    .close = cli_prompt_close,
    .answer = cli_prompt_answer,
    .asked = cli_prompt_asked,
};

// how many rows the ask has, none where no ask stands, a number counted as the ask opens; and the row `at` into
// `row`: 1, or 0 past the last
unsigned int cli_prompt_row_count(const CliPromptRowsCfg *args);
int cli_prompt_row_read(const CliPromptRowsCfg *args);
struct CliPromptRowsCfg
{
    unsigned int at;
    CliPromptRow *row;
};

typedef struct
{
    unsigned int (*rows)(const CliPromptRowsCfg *args);
    int (*row)(const CliPromptRowsCfg *args);
} CliPromptRowsNs;
EMBED_TABLE_LAYOUT(CliPromptRowsNs, rows, row);
EMBED_TABLE_STORAGE CliPromptRowsNs cli_prompt_rows EMBED_UNUSED = {
    .rows = cli_prompt_row_count,
    .row = cli_prompt_row_read,
};

// `size` bytes of `text` typed at the field's end, a UTF-8 run, while an ask stands; and the key `name` pressed:
// Backspace erases the field's last letter, Enter presses the first button, Escape closes the ask with no answer, each
// 1, any other key 0
void cli_prompt_type(const CliPromptInputCfg *args);
int cli_prompt_key(const CliPromptInputCfg *args);
struct CliPromptInputCfg
{
    const char *text;
    unsigned long long size;
    const char *name;
};

typedef struct
{
    void (*type)(const CliPromptInputCfg *args);
    int (*key)(const CliPromptInputCfg *args);
} CliPromptInputNs;
EMBED_TABLE_LAYOUT(CliPromptInputNs, type, key);
EMBED_TABLE_STORAGE CliPromptInputNs cli_prompt_input EMBED_UNUSED = {
    .type = cli_prompt_type,
    .key = cli_prompt_key,
};

#endif
