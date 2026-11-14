// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_outline.c: the outline of one text, its declarations as rows. The text is read whole from its document and
// passed to the declaration reader in its path's language; each declaration's name and type are copied beside the rows
// as they come, and the depths are read off the lines' indents in one walk of the text once every row is
#include "symbols.h"

#include "../command_line/pool.h"
#include "../editor/document.h"

// the bytes each row's name and type are copied into
#define OUTLINE_TEXT_BYTES (256u * 1024u)

// A row as stored: its name's and its type's places in the text copied, its name's bytes, its line, its line's indent
// in columns and its depth
typedef struct
{
    unsigned int name;
    unsigned long long length;
    unsigned int type;
    unsigned long long line;
    unsigned int indent;
    unsigned int depth;
} OutlineRow;

// The outline: its rows and how many, and the text each row's name and type are copied into and how much of it is used,
// in one cell taken as the first outline is read
typedef struct
{
    OutlineRow row[CLI_OUTLINE_ROWS];
    unsigned int count;
    unsigned int used;
    char text[OUTLINE_TEXT_BYTES];
} Outline;

// the outline under build, a cell of the temporary tier for the one read; and the outline located: its rows then its
// text from `text_at` in a movable cell sized to them, and how many rows
static Outline *s_outline;
static const CarcerMovableEntry *s_outline_cell;
static unsigned int s_outline_count;
static unsigned long long s_outline_text_at;

// the outline located let go, none located
static void outline_released(void)
{
    (void)g_cli_guard->movable_buf_release(s_outline_cell, g_cli_code_owner);
    s_outline_cell = NULL;
    s_outline_count = 0u;
    s_outline_text_at = 0ULL;
}

// the outline under build published as the outline located, in a movable cell of its rows and the text they use
static void outline_published(void)
{
    outline_released();
    const embed_word rows_bytes = (embed_word)s_outline->count * sizeof(OutlineRow);
    if (s_outline->count == 0u)
    {
        return;
    }
    s_outline_cell = g_cli_guard->movable_buf_alloc(rows_bytes + s_outline->used, g_cli_code_owner);
    if (s_outline_cell == NULL)
    {
        return;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = s_outline_cell, .src = s_outline->row, .bytes = rows_bytes});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = s_outline_cell, .offset = rows_bytes, .src = s_outline->text, .bytes = s_outline->used});
    s_outline_count = s_outline->count;
    s_outline_text_at = rows_bytes;
}

typedef struct
{
    const char *text;
    unsigned long long length;
} OutlineCtx;

// `length` bytes of `text` copied after the texts copied so far with a NUL after them: where they start, or
// OUTLINE_TEXT_BYTES where they do not fit
EMBED_INLINE unsigned int outline_text_append(const OutlineCtx *args)
{
    Outline *const outline = s_outline;
    if (args->length + 1ULL > (unsigned long long)(OUTLINE_TEXT_BYTES - outline->used))
    {
        return OUTLINE_TEXT_BYTES;
    }
    const unsigned int at = outline->used;
    memor.cpy(&(MemoriaCfg){.dst = outline->text + at, .src = args->text, .bytes = args->length});
    outline->text[at + args->length] = '\0';
    // the text copied fits the bytes, which an unsigned int counts
    outline->used = at + (unsigned int)args->length + 1u;
    return at;
}

// a declaration read: a row of it, its name and type copied, where the rows and the text fit it
static void outline_declared(const CliDeclaredCfg *args)
{
    Outline *const outline = s_outline;
    if (outline->count >= CLI_OUTLINE_ROWS)
    {
        return;
    }
    const unsigned int name = EMBED_CALL(outline_text_append, OutlineCtx, .text = args->name, .length = args->length);
    const unsigned int type =
        EMBED_CALL(outline_text_append, OutlineCtx, .text = args->type,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->type, .cap = CLI_NAME_MAX_LEN}));
    if ((name == OUTLINE_TEXT_BYTES) || (type == OUTLINE_TEXT_BYTES))
    {
        return;
    }
    outline->row[outline->count] = (OutlineRow){name, args->length, type, args->line, 0u, 0u};
    outline->count += 1u;
}

// each row's indent read off its line in one walk of `length` bytes of `text`, the rows located in the order of their
// lines; then each row's depth, its indent in steps of the smallest indent any row has, no deeper than
// CLI_OUTLINE_DEEPEST, 0 where no row is indented
EMBED_INLINE void outline_depths_read(const OutlineCtx *args)
{
    Outline *const outline = s_outline;
    unsigned long long line = 0ULL;
    unsigned long long at = 0ULL;
    unsigned int row = 0u;
    while ((row < outline->count) && (at <= args->length))
    {
        // the indent of the line starting at `at`: its spaces and tabs
        unsigned long long indent = 0ULL;
        while ((at + indent < args->length) && ((args->text[at + indent] == ' ') || (args->text[at + indent] == '\t')))
        {
            indent += 1ULL;
        }
        while ((row < outline->count) && (outline->row[row].line <= line))
        {
            // an indent is far shorter than an unsigned int counts
            outline->row[row].indent = (outline->row[row].line == line) ? (unsigned int)indent : 0u;
            row += 1u;
        }
        while ((at < args->length) && (args->text[at] != '\n'))
        {
            at += 1ULL;
        }
        at += 1ULL;
        line += 1ULL;
    }
    unsigned int step = 0u;
    for (unsigned int each = 0u; each < outline->count; each += 1u)
    {
        const unsigned int indent = outline->row[each].indent;
        step = ((indent != 0u) && ((step == 0u) || (indent < step))) ? indent : step;
    }
    for (unsigned int each = 0u; each < outline->count; each += 1u)
    {
        const unsigned int deep = (step != 0u) ? (outline->row[each].indent + (step / 2u)) / step : 0u;
        outline->row[each].depth = (deep < CLI_OUTLINE_DEEPEST) ? deep : CLI_OUTLINE_DEEPEST;
    }
}

void cli_outline_read(const CliOutlineCfg *args)
{
    outline_released();
    if ((args->key == NULL) || (args->path == NULL))
    {
        return;
    }
    CliShape shape = {0ULL, 0ULL};
    if (!EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = args->key, .shape = &shape) ||
        (shape.lines > CLI_OUTLINE_LINES_MAX))
    {
        return;
    }
    // the outline built in a cell of this read, then published sized to its rows
    s_outline = g_cli_guard->temporary_buf_alloc(sizeof(Outline), g_cli_code_owner);
    if (s_outline == NULL)
    {
        return;
    }
    s_outline->count = 0u;
    s_outline->used = 0u;
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->key,
                                  .owner = g_cli_code_owner, .size_pointer = &size);
    if (text != NULL)
    {
        EMBED_CALL(cli_symbols_path_text_size.declare, CliSymbolsPathTextSizeCfg, .path = args->path, .text = text,
                   .size = size, .each_declared = outline_declared);
        EMBED_CALL(outline_depths_read, OutlineCtx, .text = text, .length = size);
        g_cli_guard->persistent_buf_release(text, g_cli_code_owner);
        outline_published();
    }
    g_cli_guard->persistent_buf_release(s_outline, g_cli_code_owner);
    s_outline = NULL;
}

unsigned int cli_outline_rows(const CliOutlineCfg *args)
{
    (void)args;
    return s_outline_count;
}

int cli_outline_row(const CliOutlineCfg *args)
{
    if (args->at >= s_outline_count)
    {
        return 0;
    }
    OutlineRow row;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = s_outline_cell, .offset = (embed_word)args->at * sizeof(row), .dst = &row, .bytes = sizeof(row)});
    {
        const unsigned long long text_read =
            (row.length < ((sizeof(args->row->name)) - 1ULL)) ? row.length : ((sizeof(args->row->name)) - 1ULL);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_outline_cell,
                                                                .offset = (embed_word)(s_outline_text_at + row.name),
                                                                .dst = args->row->name,
                                                                .bytes = (embed_word)text_read});
        args->row->name[text_read] = '\0';
    }
    // the type is ended in the text: its bytes are those before the end
    const embed_word type_length = g_cli_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = s_outline_cell, .offset = (embed_word)(s_outline_text_at + row.type)});
    {
        const unsigned long long text_read =
            (type_length < ((sizeof(args->row->type)) - 1ULL)) ? type_length : ((sizeof(args->row->type)) - 1ULL);
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_outline_cell,
                                                                .offset = (embed_word)(s_outline_text_at + row.type),
                                                                .dst = args->row->type,
                                                                .bytes = (embed_word)text_read});
        args->row->type[text_read] = '\0';
    }
    args->row->length = row.length;
    args->row->line = row.line;
    args->row->depth = row.depth;
    return 1;
}
