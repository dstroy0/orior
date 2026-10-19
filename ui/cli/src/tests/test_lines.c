// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_lines.c: the lines pytest and unittest print in their verbose reports as each test ends, read, and a test's
// name as unittest and as pytest give it
#include "test_runs_internal.h"

// whether `length` bytes from `start` of `text` hold `part`
static int python_holds(const char *text, unsigned long long length, const char *part)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + size <= length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = text + at, .other = part, .bytes = size}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int cli_test_pytest_line(const char *line, unsigned long long size, unsigned long long *id, unsigned long long *id_size,
                         const char **outcome)
{
    static const char *const s_words[6] = {" PASSED", " FAILED", " ERROR", " SKIPPED", " XFAIL", " XPASS"};
    static const char *const s_outcomes[6] = {"passed", "failed", "failed", "skipped", "skipped", "passed"};
    while ((size > 0ull) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = line[size - 1ull]}) != NULL))
    {
        size -= 1ull;
    }
    for (unsigned int word = 0u; word < 6u; word += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_words[word], .cap = CLI_CELLS_POOL});
        unsigned long long at = 0ull;
        while ((at + length <= size) &&
               (memor.cmp(&(MemoriaCfg){.src = line + at, .other = s_words[word], .bytes = length}) != 0))
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
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = line[start]}) != NULL))
        {
            start += 1ull;
        }
        while ((end > start) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = line[end - 1ull]}) != NULL))
        {
            end -= 1ull;
        }
        const unsigned long long after = at + length;
        const int ends = (after == size) || (line[after] == ' ') || (line[after] == '[');
        if (python_holds(line + start, end - start, "::") && !python_holds(line + start, end - start, " ") && ends)
        {
            *id = start;
            *id_size = end - start;
            *outcome = s_outcomes[word];
            return 1;
        }
    }
    return 0;
}

int cli_test_unittest_line(const char *line, unsigned long long size, char *id, unsigned long long room,
                           const char **outcome)
{
    unsigned long long split = 0ull;
    while ((split + 5ull <= size) &&
           (memor.cmp(&(MemoriaCfg){.src = line + split, .other = " ... ", .bytes = 5u}) != 0))
    {
        split += 1ull;
    }
    if (split + 5ull > size)
    {
        return 0;
    }
    unsigned long long open = 0ull;
    while ((open + 2ull <= split) && (memor.cmp(&(MemoriaCfg){.src = line + open, .other = " (", .bytes = 2u}) != 0))
    {
        open += 1ull;
    }
    if (open + 2ull > split)
    {
        return 0;
    }
    unsigned long long dotted_size = split - open - 2ull;
    while ((dotted_size > 0ull) && (line[open + 2ull + dotted_size - 1ull] == ')'))
    {
        dotted_size -= 1ull;
    }
    unsigned long long tail = split + 5ull;
    unsigned long long tail_size = size - tail;
    while ((tail_size > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = line[tail]}) != NULL))
    {
        tail += 1ull;
        tail_size -= 1ull;
    }
    while ((tail_size > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = line[tail + tail_size - 1ull]}) != NULL))
    {
        tail_size -= 1ull;
    }
    const int ok =
        (tail_size == 2ull) && (memor.cmp(&(MemoriaCfg){.src = line + tail, .other = "ok", .bytes = 2u}) == 0);
    const int failed =
        ((tail_size == 4ull) && (memor.cmp(&(MemoriaCfg){.src = line + tail, .other = "FAIL", .bytes = 4u}) == 0)) ||
        ((tail_size == 5ull) && (memor.cmp(&(MemoriaCfg){.src = line + tail, .other = "ERROR", .bytes = 5u}) == 0));
    const int skipped =
        ((tail_size >= 7ull) && (memor.cmp(&(MemoriaCfg){.src = line + tail, .other = "skipped", .bytes = 7u}) == 0)) ||
        ((tail_size >= 16ull) &&
         (memor.cmp(&(MemoriaCfg){.src = line + tail, .other = "expected failure", .bytes = 16u}) == 0));
    if (!ok && !failed && !skipped)
    {
        return 0;
    }
    *outcome = ok ? "passed" : failed ? "failed" : "skipped";
    return cli_test_unittest_id(line + open + 2ull, dotted_size, id, room);
}

void cli_test_dotted(CliWords *words, const char *id)
{
    char dotted[CLI_PATH_LONGEST * 2u];
    CliLine line = {dotted, sizeof(dotted), 0ull};
    const char *const path =
        cellul.find(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
    unsigned long long module = (path != NULL) ? (unsigned long long)(path - id)
                                               : cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
    while ((module >= 3ull) && (memor.cmp(&(MemoriaCfg){.src = id + module - 3ull, .other = ".py", .bytes = 3u}) == 0))
    {
        module -= 3ull;
    }
    for (unsigned long long at = 0ull; at < module; at += 1ull)
    {
        cli_line_character(&line, ((id[at] == '/') || (id[at] == '\\')) ? '.' : id[at]);
    }
    for (const char *rest = path; rest != NULL;)
    {
        rest += 2;
        const char *const next = cellul.find(
            &(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .other = "::", .other_cap = CLI_CELLS_POOL});
        cli_line_character(&line, '.');
        cli_line_counted(&line, rest,
                         (next != NULL) ? (unsigned long long)(next - rest)
                                        : cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}));
        rest = next;
    }
    cli_line_closed(&line);
    cli_words_put(words, dotted);
}

int cli_test_unittest_id(const char *dotted, unsigned long long length, char *id, unsigned long long room)
{
    // the last part that starts with a capital is the class: the parts before it the module, those after it the name
    unsigned long long class_start = length;
    unsigned long long class_end = length;
    unsigned long long part = 0ull;
    unsigned int parts = 0u;
    unsigned int class_part = 0u;
    int has_class = 0;
    while (part <= length)
    {
        const char *const dot = memor.chr(&(MemoriaCfg){.src = dotted + part, .val = '.', .bytes = length - part});
        const unsigned long long end = (dot != NULL) ? (unsigned long long)(dot - dotted) : length;
        if ((end > part) && (dotted[part] >= 'A') && (dotted[part] <= 'Z'))
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
    CliLine line = {id, room, 0ull};
    for (unsigned long long at = 0ull; at + 1ull < class_start; at += 1ull)
    {
        cli_line_character(&line, (dotted[at] == '.') ? '/' : dotted[at]);
    }
    cli_line_text(&line, ".py::");
    cli_line_counted(&line, dotted + class_start, class_end - class_start);
    cli_line_text(&line, "::");
    cli_line_counted(&line, dotted + class_end + 1ull, length - class_end - 1ull);
    return cli_line_closed(&line) != 0ull;
}
