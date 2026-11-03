// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// completions.c: completion items, each its texts stored in its items' capacity, made unique by what they show and
// written as the page reads them
#include "../text/edits.h"
#include "../text/json_tree.h"

void cli_completions_begin(const CliEditsItemsCfg *args)
{
    args->items->count = 0u;
    args->items->used = 0u;
    args->items->full = 0;
}

typedef struct
{
    CliCompletions *items;
    const char *bytes;
    unsigned long long size;
} CompletionsCtx;
// `size` bytes of `bytes` stored in the items' capacity: where they start, or CLI_COMPLETIONS_TEXT where they do not
// fit
EMBED_INLINE unsigned int completions_text_store(const CompletionsCtx *args)
{
    const char *bytes = args->bytes;
    if (args->size + 1u > CLI_COMPLETIONS_TEXT - args->items->used)
    {
        return CLI_COMPLETIONS_TEXT;
    }
    const unsigned int at = args->items->used;
    memor.cpy(&(MemoriaCfg){.dst = args->items->text + at, .src = bytes, .bytes = (size_t)args->size});
    args->items->text[at + args->size] = '\0';
    args->items->used += (unsigned int)args->size + 1u;
    return at;
}

int cli_completion_append(const CliEditsItemsCfg *args)
{
    if (args->items->count == CLI_COMPLETIONS_MAX)
    {
        args->items->full = 1;
        return 0;
    }
    const unsigned int used = args->items->used;
    const unsigned int label_at = EMBED_CALL(completions_text_store, CompletionsCtx, .items = args->items,
                                             .bytes = args->label, .size = args->label_size);
    const unsigned int detail_at = EMBED_CALL(completions_text_store, CompletionsCtx, .items = args->items,
                                              .bytes = args->detail, .size = args->detail_size);
    const unsigned int insert_at = EMBED_CALL(completions_text_store, CompletionsCtx, .items = args->items,
                                              .bytes = args->insert, .size = args->insert_size);
    if ((label_at == CLI_COMPLETIONS_TEXT) || (detail_at == CLI_COMPLETIONS_TEXT) ||
        (insert_at == CLI_COMPLETIONS_TEXT))
    {
        args->items->used = used;
        args->items->full = 1;
        return 0;
    }
    args->items->item[args->items->count] = (CliCompletion){
        label_at,  (unsigned int)args->label_size,  args->kind,   detail_at, (unsigned int)args->detail_size,
        insert_at, (unsigned int)args->insert_size, args->snippet};
    args->items->count += 1u;
    return 1;
}

void cli_completions_unique(const CliEditsItemsCfg *args)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < args->items->count; at += 1u)
    {
        const CliCompletion *const one = &args->items->item[at];
        int seen = 0;
        unsigned int before = 0u;
        while (!seen && (before < count))
        {
            const CliCompletion *const other = &args->items->item[before];
            seen = (other->label_size == one->label_size) &&
                   (memor.cmp(&(MemoriaCfg){.src = args->items->text + other->label_at,
                                            .other = args->items->text + one->label_at,
                                            .bytes = one->label_size}) == 0);
            before += 1u;
        }
        if (!seen)
        {
            args->items->item[count] = *one;
            count += 1u;
        }
    }
    args->items->count = count;
}
