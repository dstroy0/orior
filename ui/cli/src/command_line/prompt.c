// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// prompt.c: the one ask. Its sentence, its statement and its buttons' labels are in the ask's own bytes with their
// ends, positioned as the ask opens, the sentence and the statement each cut to CLI_PROMPT_STATEMENT_MAX bytes; its field
// is in a cell of its own sized to the characters typed, with the count of its characters following each change; and its
// rows are types positioned as the ask opens, a number a row. An answer passes the field's text to the continuation with the
// ask already cleared, so that the continuation may open another
#include "prompt.h"

// the bytes the sentence, the statement and the labels take at max, each with its end
#define PROMPT_TEXTS                                                                                                   \
    ((2ULL * (CLI_PROMPT_STATEMENT_MAX + 1ULL)) +                                                                      \
     ((unsigned long long)CLI_PROMPT_CHOICES * (CLI_PROMPT_LABEL_MAX + 1ULL)))

// The ask: its texts, the sentence's and the statement's bytes and places in them, the buttons' labels in them with
// their bytes and how many, its mode, its field and the characters the field carries, its continuation with its context,
// whether it is, and its rows' types and how many
typedef struct
{
    char texts[PROMPT_TEXTS];
    const char *sentence;
    unsigned long long sentence_length;
    const char *statement;
    unsigned long long statement_length;
    const char *choice[CLI_PROMPT_CHOICES];
    unsigned long long choice_length[CLI_PROMPT_CHOICES];
    unsigned int choices;
    unsigned int mode;
    CliTyped field;
    char text[CLI_PROMPT_FIELD_MAX];
    unsigned long long characters;
    void (*then)(const CliPromptCfg *args);
    void *context;
    int asked;
    unsigned int row_type[4];
    unsigned int rows;
} Prompt;

static Prompt s_prompt;

typedef struct
{
    const char *text;
    unsigned long long size;
} PromptCtx;

// the characters of `size` bytes of `text`, each a lead byte of UTF-8
EMBED_INLINE unsigned long long characters_counted(const PromptCtx *args)
{
    unsigned long long characters = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        // a byte read as its bits
        const unsigned char byte = (unsigned char)args->text[at];
        characters += ((byte & 0xC0u) != 0x80u) ? 1ULL : 0ULL;
    }
    return characters;
}

void cli_prompt_close(const CliPromptCfg *args)
{
    (void)args;
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &s_prompt.field, .owner = g_cli_command_line_owner);
    memor.set(&(MemoriaCfg){.dst = &s_prompt, .val = 0, .bytes = sizeof(s_prompt)});
}

// the field's text copied out of the typed text's cell into the prompt's own buffer, which the rows read, and its
// characters counted
EMBED_INLINE void field_refreshed(const PromptCtx *args)
{
    (void)args;
    const unsigned long long length = EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &s_prompt.field,
                                                 .dst = s_prompt.text, .capacity = sizeof(s_prompt.text));
    s_prompt.characters = EMBED_CALL(characters_counted, PromptCtx, .text = s_prompt.text, .size = length);
}

int cli_prompt_open(const CliPromptCfg *args)
{
    EMBED_CALL(cli_prompt.close, CliPromptCfg, .at = 0u);
    const unsigned int count = (args->count < CLI_PROMPT_CHOICES) ? args->count : CLI_PROMPT_CHOICES;
    const unsigned long long sentence_length =
        (args->sentence != NULL)
            ? cellul.len(&(CatenaFinitaCfg){.src = args->sentence, .cap = CLI_PROMPT_STATEMENT_MAX})
            : 0ULL;
    const unsigned long long statement_whole =
        (args->statement != NULL)
            ? ((args->statement_size != 0ULL)
                   ? args->statement_size
                   : cellul.len(&(CatenaFinitaCfg){.src = args->statement, .cap = CLI_PROMPT_STATEMENT_MAX}))
            : 0ULL;
    const unsigned long long statement_length =
        (statement_whole < CLI_PROMPT_STATEMENT_MAX) ? statement_whole : CLI_PROMPT_STATEMENT_MAX;
    char *const cell = s_prompt.texts;
    unsigned long long used = 0ULL;
    s_prompt.sentence = cell;
    s_prompt.sentence_length = sentence_length;
    if (sentence_length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->sentence, .bytes = sentence_length});
    }
    cell[sentence_length] = '\0';
    used = sentence_length + 1ULL;
    s_prompt.statement = (args->statement != NULL) ? cell + used : NULL;
    s_prompt.statement_length = statement_length;
    if (statement_length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = cell + used, .src = args->statement, .bytes = statement_length});
    }
    cell[used + statement_length] = '\0';
    used += statement_length + 1ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = args->choices[at], .cap = CLI_PROMPT_LABEL_MAX});
        memor.cpy(&(MemoriaCfg){.dst = cell + used, .src = args->choices[at], .bytes = length + 1ULL});
        s_prompt.choice[at] = cell + used;
        s_prompt.choice_length[at] = length;
        used += length + 1ULL;
    }
    s_prompt.choices = count;
    s_prompt.mode = args->mode;
    s_prompt.then = args->then;
    s_prompt.context = args->context;
    s_prompt.asked = 1;
    if (args->preset != NULL)
    {
        EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &s_prompt.field, .text = args->preset,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->preset, .cap = CLI_PROMPT_FIELD_MAX}),
                   .max = CLI_PROMPT_FIELD_MAX, .owner = g_cli_command_line_owner);
    }
    EMBED_CALL(field_refreshed, PromptCtx, .size = 0ULL);
    // the rows as the mode positions them: the sentence, the statement where one is asked about, the field where the mode
    // takes one, and the buttons where there are any
    unsigned int rows = 0u;
    s_prompt.row_type[rows] = CLI_PROMPT_ROW_SENTENCE;
    rows += 1u;
    if (s_prompt.statement != NULL)
    {
        s_prompt.row_type[rows] = CLI_PROMPT_ROW_STATEMENT;
        rows += 1u;
    }
    if (args->mode != CLI_PROMPT_CONFIRM)
    {
        s_prompt.row_type[rows] = CLI_PROMPT_ROW_FIELD;
        rows += 1u;
    }
    if (count != 0u)
    {
        s_prompt.row_type[rows] = CLI_PROMPT_ROW_BUTTONS;
        rows += 1u;
    }
    s_prompt.rows = rows;
    return 1;
}

int cli_prompt_answer(const CliPromptCfg *args)
{
    if (!s_prompt.asked || ((args->at >= s_prompt.choices) && (args->at != CLI_PROMPT_NONE)))
    {
        return 0;
    }
    // the ask cleared before its continuation runs, which may open the next; its cells released after
    static Prompt s_answered;
    s_answered = s_prompt;
    memor.set(&(MemoriaCfg){.dst = &s_prompt, .val = 0, .bytes = sizeof(s_prompt)});
    if (s_answered.then != NULL)
    {
        EMBED_CALL(s_answered.then, CliPromptCfg, .at = args->at, .text = s_answered.text,
                   .size = s_answered.field.length, .context = s_answered.context);
    }
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = &s_answered.field, .owner = g_cli_command_line_owner);
    return 1;
}

int cli_prompt_asked(const CliPromptCfg *args)
{
    return s_prompt.asked && ((args->context == NULL) || (args->context == s_prompt.context));
}

unsigned int cli_prompt_row_count(const CliPromptRowsCfg *args)
{
    (void)args;
    return s_prompt.rows;
}

int cli_prompt_row_read(const CliPromptRowsCfg *args)
{
    if (args->at >= s_prompt.rows)
    {
        return 0;
    }
    CliPromptRow *const row = args->row;
    *row = (CliPromptRow){0};
    row->type = s_prompt.row_type[args->at];
    switch (row->type)
    {
    case CLI_PROMPT_ROW_SENTENCE:
        row->text = s_prompt.sentence;
        row->length = s_prompt.sentence_length;
        return 1;
    case CLI_PROMPT_ROW_STATEMENT:
        row->text = s_prompt.statement;
        row->length = s_prompt.statement_length;
        return 1;
    case CLI_PROMPT_ROW_FIELD:
        row->secret = s_prompt.mode == CLI_PROMPT_PASSWORD;
        row->characters = s_prompt.characters;
        row->text = row->secret ? NULL : s_prompt.text;
        row->length = row->secret ? 0ULL : s_prompt.field.length;
        return 1;
    default:
        for (unsigned int at = 0u; at < s_prompt.choices; at += 1u)
        {
            row->choice[at] = s_prompt.choice[at];
            row->choice_length[at] = s_prompt.choice_length[at];
        }
        row->choices = s_prompt.choices;
        return 1;
    }
}

void cli_prompt_type(const CliPromptInputCfg *args)
{
    if (!s_prompt.asked || (s_prompt.mode == CLI_PROMPT_CONFIRM))
    {
        return;
    }
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &s_prompt.field, .text = args->text, .size = args->size,
               .max = CLI_PROMPT_FIELD_MAX, .owner = g_cli_command_line_owner);
    EMBED_CALL(field_refreshed, PromptCtx, .size = 0ULL);
}

int cli_prompt_key(const CliPromptInputCfg *args)
{
    if (!s_prompt.asked)
    {
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &s_prompt.field, .owner = g_cli_command_line_owner);
        EMBED_CALL(field_refreshed, PromptCtx, .size = 0ULL);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Enter", .cap = 16ULL}))
    {
        (void)EMBED_CALL(cli_prompt.answer, CliPromptCfg, .at = (s_prompt.choices != 0u) ? 0u : CLI_PROMPT_NONE);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        EMBED_CALL(cli_prompt.close, CliPromptCfg, .at = 0u);
        return 1;
    }
    return 0;
}
