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
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->part, .cap = CLI_POOL_BYTES});
    for (unsigned long long at = 0ULL; at + size <= args->length; at += 1ULL)
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
    while ((size > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = args->line[size - 1ULL]}) != NULL))
    {
        size -= 1ULL;
    }
    for (unsigned int word_text = 0u; word_text < 6u; word_text += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = s_words[word_text], .cap = CLI_POOL_BYTES});
        unsigned long long at = 0ULL;
        while ((at + length <= size) &&
               (memor.cmp(&(MemoriaCfg){.src = args->line + at, .other = s_words[word_text], .bytes = length}) != 0))
        {
            at += 1ULL;
        }
        if (at + length > size)
        {
            continue;
        }
        unsigned long long start = 0ULL;
        unsigned long long end = at;
        while ((start < end) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_POOL_BYTES, .byte = args->line[start]}) != NULL))
        {
            start += 1ULL;
        }
        while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                     .src = " \t", .cap = CLI_POOL_BYTES, .byte = args->line[end - 1ULL]}) != NULL))
        {
            end -= 1ULL;
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
            *outcome = s_outcomes[word_text];
            return 1;
        }
    }
    return 0;
}

int cli_test_unittest_line(const CliTestRunsSizeLineCtx *args)
{
    const char **outcome = args->outcome_pointer;
    unsigned long long split = 0ULL;
    while ((split + 5ULL <= args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->line + split, .other = " ... ", .bytes = 5u}) != 0))
    {
        split += 1ULL;
    }
    if (split + 5ULL > args->size)
    {
        return 0;
    }
    unsigned long long open = 0ULL;
    open = EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = args->line, .at = 0ULL,
                      .size = split, .first = ' ', .second = '(');
    if (open + 2ULL > split)
    {
        return 0;
    }
    const unsigned long long dotted_size =
        EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->line + open + 2ULL,
                   .at = split - open - 2ULL, .set = ")");
    unsigned long long tail = split + 5ULL;
    unsigned long long tail_size = args->size - tail;
    while ((tail_size > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_POOL_BYTES, .byte = args->line[tail]}) != NULL))
    {
        tail += 1ULL;
        tail_size -= 1ULL;
    }
    while ((tail_size > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n", .cap = CLI_POOL_BYTES, .byte = args->line[tail + tail_size - 1ULL]}) != NULL))
    {
        tail_size -= 1ULL;
    }
    const int ok =
        (tail_size == 2ULL) && (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "ok", .bytes = 2u}) == 0);
    const int failed = ((tail_size == 4ULL) &&
                        (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "FAIL", .bytes = 4u}) == 0)) ||
                       ((tail_size == 5ULL) &&
                        (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "ERROR", .bytes = 5u}) == 0));
    const int skipped =
        ((tail_size >= 7ULL) &&
         (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "skipped", .bytes = 7u}) == 0)) ||
        ((tail_size >= 16ULL) &&
         (memor.cmp(&(MemoriaCfg){.src = args->line + tail, .other = "expected failure", .bytes = 16u}) == 0));
    if (!ok && !failed && !skipped)
    {
        return 0;
    }
    if (ok)
    {
        *outcome = "passed";
    }
    else if (failed)
    {
        *outcome = "failed";
    }
    else
    {
        *outcome = "skipped";
    }
    return EMBED_CALL(cli_test_runs_length_internal.test_unittest_id, CliTestRunsLengthCtx,
                      .dotted = args->line + open + 2ULL, .length = dotted_size, .id_out = args->id_out,
                      .capacity = args->capacity);
}

void cli_test_dot(const CliTestRunsCtx *args)
{
    char dotted[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {dotted, sizeof(dotted), 0ULL};
    const char *const path = cellul.find(
        &(CatenaFinitaCfg){.src = args->id, .cap = CLI_POOL_BYTES, .other = "::", .other_cap = CLI_POOL_BYTES});
    unsigned long long module = (path != NULL) ? (unsigned long long)(path - args->id)
                                               : cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_POOL_BYTES});
    while ((module >= 3ULL) &&
           (memor.cmp(&(MemoriaCfg){.src = args->id + module - 3ULL, .other = ".py", .bytes = 3u}) == 0))
    {
        module -= 3ULL;
    }
    for (unsigned long long at = 0ULL; at < module; at += 1ULL)
    {
        mmgr_line_ch(&line, ((args->id[at] == '/') || (args->id[at] == '\\')) ? '.' : args->id[at]);
    }
    for (const char *rest = path; rest != NULL;)
    {
        rest += 2;
        const char *const next = cellul.find(
            &(CatenaFinitaCfg){.src = rest, .cap = CLI_POOL_BYTES, .other = "::", .other_cap = CLI_POOL_BYTES});
        mmgr_line_ch(&line, '.');
        mmgr_line_put_n(&line, rest,
                        (next != NULL) ? (unsigned long long)(next - rest)
                                       : cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_POOL_BYTES}));
        rest = next;
    }
    mmgr_line_finish(&line);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = dotted);
}

int cli_test_unittest_id(const CliTestRunsLengthCtx *args)
{
    // the last part that starts with a capital is the class: the parts before it the module, those after it the name
    unsigned long long class_start = args->length;
    unsigned long long class_end = args->length;
    unsigned long long part = 0ULL;
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
        part = end + 1ULL;
    }
    if (!has_class || (class_part == 0u) || (class_part + 1u >= parts))
    {
        return 0;
    }
    VerbaLinea line = {args->id_out, args->capacity, 0ULL};
    for (unsigned long long at = 0ULL; at + 1ULL < class_start; at += 1ULL)
    {
        mmgr_line_ch(&line, (args->dotted[at] == '.') ? '/' : args->dotted[at]);
    }
    mmgr_line_put(&line, ".py::");
    mmgr_line_put_n(&line, args->dotted + class_start, class_end - class_start);
    mmgr_line_put(&line, "::");
    mmgr_line_put_n(&line, args->dotted + class_end + 1ULL, args->length - class_end - 1ULL);
    return mmgr_line_finish(&line) != 0ULL;
}
