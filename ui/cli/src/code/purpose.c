// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose.c: what a file declares it is for, and the files of the tree that use it, read on the cores the work may take
#include "purpose.h"
#include "../command_line/pool.h"
#include "../editor/files_internal.h"
#include "../machine/cores.h"
#include "../text/json_tree.h"
#include "../text/text.h"

#define CLI_PURPOSE_TEXT ((CLI_PURPOSE_READ * 3u) + 1u)
// One reader of the tree's files: the files it reads, from `from` to `to`, and the capacity a file's text is read into
typedef struct
{
    unsigned int from;
    unsigned int to;
    char text[CLI_PURPOSE_TEXT];
    CliThread thread;
} CliPurposePart;
// the most threads the tree's files are read on, and the bytes a file's text takes once read as UTF-8
#define CLI_PURPOSE_THREADS 16u

typedef struct
{
    unsigned int letter;
    const char *within;
    unsigned long long size;
    unsigned long long at;
    unsigned long long length;
    const char *name;
    const unsigned char *bytes;
    char *text;
    CliPurposePart *part;
    VerbaLinea *out;
    const char *root;
    const char *path;
    VerbaLinea *err_msg;
} PurposeCtx;
// whether a letter joins a word: a letter, a number or `_`
EMBED_INLINE int cli_joins(const PurposeCtx *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_files_letter_internal.hit_wordy, CliFilesLetterCtx, .letter = letter);
}

// whether the span of `length` bytes at `at` of `size` bytes of `within` is a whole word: no letter, number, `_` or
// `-` before it, and no letter, number or `_` after it
EMBED_INLINE int cli_word(const PurposeCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long length = args->length;
    if (at > 0ULL)
    {
        unsigned long long start = at - 1ULL;
        while ((start > 0ULL) && ((((unsigned char)args->within[start]) & 0xc0u) == 0x80u))
        {
            start -= 1ULL;
        }
        unsigned int before = 0u;
        EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                   .text_bytes = (const unsigned char *)args->within, .length = at, .at = start, .letter = &before);
        if (EMBED_CALL(cli_joins, PurposeCtx, .letter = before) || (before == '-'))
        {
            return 0;
        }
    }
    if ((at + length) < args->size)
    {
        unsigned int after = 0u;
        EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                   .text_bytes = (const unsigned char *)args->within, .length = args->size, .at = at + length,
                   .letter = &after);
        if (EMBED_CALL(cli_joins, PurposeCtx, .letter = after))
        {
            return 0;
        }
    }
    return 1;
}

// whether `name` is a whole word in `size` bytes of `within`, its matches read without overlapping
EMBED_INLINE int cli_name_in(const PurposeCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    if (length == 0ULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; (at + length) <= args->size;)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->within + at, .other = args->name, .bytes = length}) != 0)
        {
            at += 1ULL;
            continue;
        }
        if (EMBED_CALL(cli_word, PurposeCtx, .within = args->within, .size = args->size, .at = at, .length = length))
        {
            return 1;
        }
        at += length;
    }
    return 0;
}

int cli_purpose_uses(const CliPurposeTextCfg *args)
{
    static const char *const s_imports[12] = {"import ", "from ",    "use ",    "mod ",      "pub use ", "pub mod ",
                                              "require", "#include", "\\input", "\\include", "source ",  "load("};
    if (EMBED_CALL(cli_name_in, PurposeCtx, .within = args->text, .size = args->size, .name = args->name))
    {
        return 1;
    }
    if (cellul.len(&(CatenaFinitaCfg){.src = args->stem, .cap = CLI_POOL_BYTES}) < 2ULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->text, .at = at, .size = args->size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        const char *const line = args->text + at;
        at = end + 1ULL;
        unsigned long long lead = 0ULL;
        while (lead < length)
        {
            unsigned int letter = 0u;
            const unsigned long long step =
                EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                           .text_bytes = (const unsigned char *)line, .length = length, .at = lead, .letter = &letter);
            if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
            {
                break;
            }
            lead += step;
        }
        int imports = 0;
        unsigned int keyword = 0u;
        while (!imports && (keyword < 12u))
        {
            const unsigned long long size_of =
                cellul.len(&(CatenaFinitaCfg){.src = s_imports[keyword], .cap = CLI_POOL_BYTES});
            imports =
                ((length - lead) >= size_of) &&
                (memor.cmp(&(MemoriaCfg){.src = line + lead, .other = s_imports[keyword], .bytes = size_of}) == 0);
            keyword += 1u;
        }
        if (imports && EMBED_CALL(cli_name_in, PurposeCtx, .within = line, .size = length, .name = args->stem))
        {
            return 1;
        }
    }
    return 0;
}
