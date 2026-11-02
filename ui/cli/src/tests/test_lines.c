// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_lines.c: the lines pytest and unittest print in their verbose reports as each test ends, read, and a test's
// name as unittest and as pytest give it
#include "../text/text.h"
#include "test_runs_internal.h"

typedef struct
{
    const char *text;
    unsigned long long length;
    const char *part;
} TestLinesCtx;

// whether `length` bytes from `start` of `text` contain `part`
EMBED_INLINE int python_contains(const TestLinesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->part, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + size <= args->length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = args->part, .bytes = size}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int cli_test_pytest_line(const CliTestRunsSizeCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long *id = args->id_u64s;
    unsigned long long *id_size = args->id_size;
    const char **outcome = args->outcome_pointer;
    static const char *const s_words[6] = {" PASSED", " FAILED", " ERROR", " SKIPPED", " XFAIL", " XPASS"};
    static const char *const s_outcomes[6] = {"passed", "failed", "failed", "skipped", "skipped", "passed"};
    while ((size > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->line[size - 1ull]}) != NULL))
    {
        size -= 1ull;
    }
    for (unsigned int word = 0u; word < 6u; word += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_words[word], .cap = CLI_CELLS_POOL});
        unsigned long long at = 0ull;
        while ((at + length <= size) &&
               (memor.cmp(&(MemoriaCfg){.src = args->line + at, .other = s_words[word], .bytes = length}) != 0))
        {
            at += 1ull;
        }
        if (at + length > size)
        {
            continue;
        }
        unsigned long long start = 0ull;
        unsigned long long end = at;
        while ((start < end) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = args->line[start]}) != NULL))
        {
            start += 1ull;
        }
        while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t", .cap = CLI_CELLS_POOL, .byte = args->line[end - 1ull]}) != NULL))
        {
            end -= 1ull;
        }
        const unsigned long long after = at + length;
        const int ends = (after == size) || (args->line[after] == ' ') || (args->line[after] == '[');
        if (EMBED_CALL(python_contains, TestLinesCtx, .text = args->line + start, .length = end - start,
                       .part = "::") &&
            !EMBED_CALL(python_contains, TestLinesCtx, .text = args->line + start, .length = end - start,
                        .part = " ") &&
            ends)
        {
            *id = start;
            *id_size = end - start;
            *outcome = s_outcomes[word];
            return 1;
        }
    }
    return 0;
}

int cli_test_unittest_line(const CliTestRunsSizeLineCtx *args)
{
    const char **outcome = args->outcome_pointer;
    unsigned long long split = 0ull;
    while ((split + 5ull <= args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->line + split, .other = " ... ", .bytes = 5u}) != 0))
    {
        split += 1ull;
    }
    if (split + 5ull > args->size)
    {
        return 0;
    }
    unsigned long long open = 0ull;
    open = EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = args->line, .at = 0ull,
                      .size = split, .first = ' ', .second = '(');
    if (open + 2ull > split)
    {
        return 0;
    }
    const unsigned long long dotted_size =
        EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = args->line + open + 2ull,
                   .at = split - open - 2ull, .set = ")");
    unsigned long long tail = split + 5ull;
    unsigned long long tail_size = args->size - tail;
    while ((tail_size > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->line[tail]}) != NULL))
    {
        tail += 1ull;
        tail_size -= 1ull;
    }
    while ((tail_size > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = args->line[tail + tail_size - 1ull]}) != NULL))
    {
        tail_size -= 1ull;
    }
    const int ok =
        (tail_size == 2ull) && (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "ok", .bytes = 2u}) == 0);
    const int failed = ((tail_size == 4ull) &&
                        (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "FAIL", .bytes = 4u}) == 0)) ||
                       ((tail_size == 5ull) &&
                        (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "ERROR", .bytes = 5u}) == 0));
    const int skipped =
        ((tail_size >= 7ull) &&
         (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "skipped", .bytes = 7u}) == 0)) ||
        ((tail_size >= 16ull) &&
         (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "expected failure", .bytes = 16u}) == 0));
    if (!ok && !failed && !skipped)
    {
        return 0;
    }
    *outcome = ok ? "passed" : failed ? "failed" : "skipped";
    return EMBED_CALL(cli_test_runs_length_internal.test_unittest_id, CliTestRunsLengthCtx,
                      .dotted = args->line + open + 2ull, .length = dotted_size, .id_out = args->id_out,
                      .room = args->room);
}

void cli_test_dot(const CliTestRunsCtx *args)
{
    char dotted[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {dotted, sizeof(dotted), 0ull};
    const char *const path = cellul.find(
        &(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    unsigned long long module = (path != NULL) ? (unsigned long long)(path - args->id)
                                               : cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL});
    while ((module >= 3ull) &&
           (memor.cmp(&(MemoriaCfg){.src = args->id + module - 3ull, .other = ".py", .bytes = 3u}) == 0))
    {
        module -= 3ull;
    }
    for (unsigned long long at = 0ull; at < module; at += 1ull)
    {
        mmgr_line_ch(&line, ((args->id[at] == '/') || (args->id[at] == '\\')) ? '.' : args->id[at]);
    }
    for (const char *rest = path; rest != NULL;)
    {
        rest += 2;
        const char *const next = cellul.find(
            &(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
        mmgr_line_ch(&line, '.');
        mmgr_line_put_n(&line, rest,
                        (next != NULL) ? (unsigned long long)(next - rest)
                                       : cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}));
        rest = next;
    }
    mmgr_line_finish(&line);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word = dotted);
}

int cli_test_unittest_id(const CliTestRunsLengthCtx *args)
{
    // the last part that starts with a capital is the class: the parts before it the module, those after it the name
    unsigned long long class_start = args->length;
    unsigned long long class_end = args->length;
    unsigned long long part = 0ull;
    unsigned int parts = 0u;
    unsigned int class_part = 0u;
    int has_class = 0;
    while (part <= args->length)
    {
        const char *const dot =
            memor.chr(&(MemoriaCfg){.src = args->dotted + part, .val = '.', .bytes = args->length - part});
        const unsigned long long end = (dot != NULL) ? (unsigned long long)(dot - args->dotted) : args->length;
        if ((end > part) && (args->dotted[part] >= 'A') && (args->dotted[part] <= 'Z'))
        {
            class_start = part;
            class_end = end;
            class_part = parts;
            has_class = 1;
        }
        parts += 1u;
        part = end + 1ull;
    }
    if (!has_class || (class_part == 0u) || (class_part + 1u >= parts))
    {
        return 0;
    }
    VerbaLinea line = {args->id_out, args->room, 0ull};
    for (unsigned long long at = 0ull; at + 1ull < class_start; at += 1ull)
    {
        mmgr_line_ch(&line, (args->dotted[at] == '.') ? '/' : args->dotted[at]);
    }
    mmgr_line_put(&line, ".py::");
    mmgr_line_put_n(&line, args->dotted + class_start, class_end - class_start);
    mmgr_line_put(&line, "::");
    mmgr_line_put_n(&line, args->dotted + class_end + 1ull, args->length - class_end - 1ull);
    return mmgr_line_finish(&line) != 0ull;
}
