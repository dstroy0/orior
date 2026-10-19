// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// completions.c: completion items, each its texts kept in its items' room, made unique by what they show and written as
// the page reads them
#include "../text/edits.h"
#include "../text/json_tree.h"

void cli_completions_begun(CliCompletions *items)
{
    items->count = 0u;
    items->used = 0u;
    items->full = 0;
}

// `size` bytes of `bytes` kept in the items' room: where they start, or CLI_COMPLETIONS_TEXT where they do not fit
static unsigned int completions_kept(CliCompletions *items, const char *bytes, unsigned long long size)
{
    if (size + 1u > CLI_COMPLETIONS_TEXT - items->used)
    {
        return CLI_COMPLETIONS_TEXT;
    }
    const unsigned int at = items->used;
    memor.cpy(&(MemoriaCfg){.dst = items->text + at, .src = bytes, .bytes = (size_t)size});
    items->text[at + size] = '\0';
    items->used += (unsigned int)size + 1u;
    return at;
}

int cli_completion_put(CliCompletions *items, const char *label, unsigned long long label_size, const char *kind,
                       const char *detail, unsigned long long detail_size, const char *insert,
                       unsigned long long insert_size, int snippet)
{
    if (items->count == CLI_COMPLETIONS_MOST)
    {
        items->full = 1;
        return 0;
    }
    const unsigned int used = items->used;
    const unsigned int label_at = completions_kept(items, label, label_size);
    const unsigned int detail_at = completions_kept(items, detail, detail_size);
    const unsigned int insert_at = completions_kept(items, insert, insert_size);
    if ((label_at == CLI_COMPLETIONS_TEXT) || (detail_at == CLI_COMPLETIONS_TEXT) ||
        (insert_at == CLI_COMPLETIONS_TEXT))
    {
        items->used = used;
        items->full = 1;
        return 0;
    }
    items->item[items->count] =
        (CliCompletion){label_at,  (unsigned int)label_size,  kind,   detail_at, (unsigned int)detail_size,
                        insert_at, (unsigned int)insert_size, snippet};
    items->count += 1u;
    return 1;
}

void cli_completions_unique(CliCompletions *items)
{
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const one = &items->item[at];
        int seen = 0;
        for (unsigned int before = 0u; !seen && (before < kept); before += 1u)
        {
            const CliCompletion *const other = &items->item[before];
            seen =
                (other->label_size == one->label_size) && (memor.cmp(&(MemoriaCfg){.src = items->text + other->label_at,
                                                                                   .other = items->text + one->label_at,
                                                                                   .bytes = one->label_size}) == 0);
        }
        if (!seen)
        {
            items->item[kept] = *one;
            kept += 1u;
        }
    }
    items->count = kept;
}

void cli_completion_written(const CliCompletions *items, unsigned int at, CliLine *out)
{
    const CliCompletion *const one = &items->item[at];
    cli_line_text(out, "{\"label\":");
    cli_json_quoted(out, items->text + one->label_at, one->label_size);
    cli_line_text(out, ",\"kind\":");
    cli_json_quoted(out, one->kind, cellul.len(&(CatenaFinitaCfg){.src = one->kind, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"detail\":");
    cli_json_quoted(out, items->text + one->detail_at, one->detail_size);
    cli_line_text(out, ",\"insert\":");
    cli_json_quoted(out, items->text + one->insert_at, one->insert_size);
    cli_line_text(out, one->snippet ? ",\"snippet\":true}" : "}");
}
