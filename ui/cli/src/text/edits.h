// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// edits.h: a span of a text and what is written over it, and the completion items, as the language servers, the build
// files' and the schemas' readers and the fills give them: a place's line from 0 and its column in UTF-16 units from 0,
// as the protocol counts them. Edits are written over a text each placed against the text as it stood before any, the
// first in the text first; a line ends at its "\n", and a "\r" before that stays the line's end (edits.c,
// completions.c)
#ifndef EDITS_H
#define EDITS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliEditsItemsCfg CliEditsItemsCfg;
typedef struct CliEditsOutCfg CliEditsOutCfg;
typedef struct CliEditsOutItemsCfg CliEditsOutItemsCfg;
typedef struct CliEditsLineCfg CliEditsLineCfg;
typedef struct CliEditsCfg CliEditsCfg;
typedef struct CliEditsPositionCfg CliEditsPositionCfg;

// the most edits written over a text at once
#define CLI_TEXT_EDITS 65536u

// A place in a text: its line and its column in UTF-16 units, each from 0
typedef struct
{
    unsigned int line;
    unsigned int col;
} CliTextPosition;

// One span of a text and what is written over it, `size` bytes
typedef struct
{
    CliTextPosition from;
    CliTextPosition to;
    const char *text;
    unsigned long long size;
} CliTextEdit;

// a place written as `{"line", "col"}`, and an edit as `{"from", "to", "text"}`
void cli_text_position_write(const CliEditsPositionCfg *args);
void cli_text_edit_write(const CliEditsCfg *args);

// the byte where the UTF-16 column `col` of `size` bytes of a line falls, the line's end where it is past it
unsigned long long cli_text_byte_get(const CliEditsLineCfg *args);

// `size` bytes of `text` with `edits`, `count` of them, written over it into `out`: 1, or 0 where there are more than
// CLI_TEXT_EDITS of them or the cellblock cannot meet the places they are found at
int cli_text_apply(const CliEditsOutCfg *args);

// One parameter of a signature's label: its first byte and the byte after it
typedef struct
{
    unsigned int from;
    unsigned int to;
} CliSignatureParameter;

// A signature in one cell: the label and how long; its parameters; the parameter the cursor's argument answers, -1 for
// none; the parameters the arguments differ from, each a place among the parameters; and the documentation as Markdown,
// empty for none
typedef struct
{
    const char *label;
    unsigned long long length;
    unsigned int parameters;
    CliSignatureParameter *parameter;
    int active;
    unsigned int differing;
    unsigned int *differs;
    const char *doc;
    unsigned long long doc_length;
} CliSignaturePanel;

// the most completion items one request gives, and the bytes their texts take
#define CLI_COMPLETIONS_MAX 4096u
#define CLI_COMPLETIONS_TEXT (1u << 20u)

// A completion item: what it shows, its type, what it states of itself and what it inserts, each a text of its items',
// and whether what it inserts is a snippet, its stops written `$1` and `${1:name}`
typedef struct
{
    unsigned int label_at;
    unsigned int label_size;
    const char *type;
    unsigned int detail_at;
    unsigned int detail_size;
    unsigned int insert_at;
    unsigned int insert_size;
    int snippet;
} CliCompletion;

// Completion items and the texts they take, with 1 in `full` where one did not fit
typedef struct
{
    CliCompletion item[CLI_COMPLETIONS_MAX];
    unsigned int count;
    char text[CLI_COMPLETIONS_TEXT];
    unsigned int used;
    int full;
} CliCompletions;

// items made empty; an item put after the others: 1, or 0 where it does not fit; the items made unique by what they
// show, the first of each retained; and an item written as the page reads one, `{"label", "type", "detail", "insert"}`
// and `"snippet": true` where it is one (completions.c)
void cli_completions_begin(const CliEditsItemsCfg *args);
int cli_completion_append(const CliEditsItemsCfg *args);
void cli_completions_unique(const CliEditsItemsCfg *args);

struct CliEditsItemsCfg
{
    CliCompletions *items;
    const char *label;
    unsigned long long label_size;
    const char *type;
    const char *detail;
    unsigned long long detail_size;
    const char *insert;
    unsigned long long insert_size;
    int snippet;
};

struct CliEditsOutCfg
{
    VerbaLinea *out;
    unsigned long long size;
    const char *text;
    const CliTextEdit *edits;
    unsigned int count;
};

struct CliEditsOutItemsCfg
{
    VerbaLinea *out;
    const CliCompletions *items_read;
    unsigned int at;
};

struct CliEditsLineCfg
{
    const char *line;
    unsigned long long size;
    unsigned int col;
};

struct CliEditsCfg
{
    VerbaLinea *out;
    const CliTextEdit *edit;
};

struct CliEditsPositionCfg
{
    CliTextPosition place;
    VerbaLinea *out;
};

typedef struct
{
    void (*text_position_write)(const CliEditsPositionCfg *args);
} CliEditsPositionNs;
EMBED_TABLE_LAYOUT(CliEditsPositionNs, text_position_write);
EMBED_TABLE_STORAGE CliEditsPositionNs cli_edits_position EMBED_UNUSED = {
    .text_position_write = cli_text_position_write,
};

typedef struct
{
    void (*text_edit_write)(const CliEditsCfg *args);
} CliEditsNs;
EMBED_TABLE_LAYOUT(CliEditsNs, text_edit_write);
EMBED_TABLE_STORAGE CliEditsNs cli_edits EMBED_UNUSED = {
    .text_edit_write = cli_text_edit_write,
};

typedef struct
{
    unsigned long long (*text_byte_get)(const CliEditsLineCfg *args);
} CliEditsLineNs;
EMBED_TABLE_LAYOUT(CliEditsLineNs, text_byte_get);
EMBED_TABLE_STORAGE CliEditsLineNs cli_edits_line EMBED_UNUSED = {
    .text_byte_get = cli_text_byte_get,
};

typedef struct
{
    int (*text_apply)(const CliEditsOutCfg *args);
} CliEditsOutNs;
EMBED_TABLE_LAYOUT(CliEditsOutNs, text_apply);
EMBED_TABLE_STORAGE CliEditsOutNs cli_edits_out EMBED_UNUSED = {
    .text_apply = cli_text_apply,
};

typedef struct
{
    void (*completions_begin)(const CliEditsItemsCfg *args);
    int (*completion_append)(const CliEditsItemsCfg *args);
    void (*completions_unique)(const CliEditsItemsCfg *args);
} CliEditsItemsNs;
EMBED_TABLE_LAYOUT(CliEditsItemsNs, completions_begin, completion_append, completions_unique);
EMBED_TABLE_STORAGE CliEditsItemsNs cli_edits_items EMBED_UNUSED = {
    .completions_begin = cli_completions_begin,
    .completion_append = cli_completion_append,
    .completions_unique = cli_completions_unique,
};

#endif
