// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths.c: a Python script read line by line for what it puts on `sys.path`
// name in the tree counted, the most used first
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "python_paths_internal.h"

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

// the span with the whitespace at either end left off
static CliSpan cli_trimmed(CliSpan span)
{
    const unsigned char *const bytes = (const unsigned char *)span.at;
    unsigned long long start = span.length;
    unsigned long long end = 0ull;
    unsigned long long at = 0ull;
    while (at < span.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, span.length, at, &letter);
        if (!cli_fuzzy_white(letter))
        {
            start = (start == span.length) ? at : start;
            end = at + step;
        }
        at += step;
    }
    return (start < end) ? (CliSpan){span.at + start, end - start} : (CliSpan){span.at, 0ull};
}

// the span with every `letter` at its end left off
static CliSpan cli_trimmed_of(CliSpan span, char letter)
{
    while ((span.length > 0ull) && (span.at[span.length - 1ull] == letter))
    {
        span.length -= 1ull;
    }
    return span;
}

// the first place `text` stands in the span, or the span's length where it stands nowhere; and the last
static unsigned long long cli_found(CliSpan span, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; (at + length) <= span.length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = span.at + at, .other = text, .bytes = length}) == 0)
        {
            return at;
        }
    }
    return span.length;
}

static unsigned long long cli_found_last(CliSpan span, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = span.length + 1ull; at > length; at -= 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = span.at + (at - 1ull - length), .other = text, .bytes = length}) == 0)
        {
            return at - 1ull - length;
        }
    }
    return span.length;
}

// whether the span starts with `text`, and whether each of its letters joins a word
static int cli_starts(CliSpan span, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return (span.length >= length) && (memor.cmp(&(MemoriaCfg){.src = span.at, .other = text, .bytes = length}) == 0);
}

static int cli_wordy_all(CliSpan span)
{
    unsigned long long at = 0ull;
    while (at < span.length)
    {
        unsigned int letter = 0u;
        at += cli_lossy_letter((const unsigned char *)span.at, span.length, at, &letter);
        if (!cli_hit_wordy(letter))
        {
            return 0;
        }
    }
    return 1;
}

// The logical lines of a script: a line whose brackets stay open goes on with the next, in the reading's cell, the
// text no longer than the script with a space joining each line, and no more lines than its bytes and one
typedef struct
{
    char *text;
    unsigned int used;
    unsigned int text_most;
    unsigned int *start;
    unsigned int *length;
    unsigned int count;
    unsigned int lines_most;
} CliPyLines;

static CliPyLines s_lines;

void cli_py_lines_placed(char *text, unsigned int text_most, unsigned int *start, unsigned int *length,
                         unsigned int lines_most)
{
    s_lines = (CliPyLines){text, 0u, text_most, start, length, 0u, lines_most};
}

// the line being made ended, as one of the logical lines
static void cli_line_ended(unsigned int *line_start)
{
    if (s_lines.count < s_lines.lines_most)
    {
        s_lines.start[s_lines.count] = *line_start;
        s_lines.length[s_lines.count] = s_lines.used - *line_start;
        s_lines.count += 1u;
    }
    *line_start = s_lines.used;
}

// `size` bytes of a script read as its logical lines
static void cli_logical_lines(const char *text, unsigned long long size)
{
    s_lines.used = 0u;
    s_lines.count = 0u;
    unsigned int line_start = 0u;
    long long open = 0ll;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        CliSpan raw = {text + at, end - at};
        if ((newline != NULL) && (raw.length > 0ull) && (raw.at[raw.length - 1ull] == '\r'))
        {
            raw.length -= 1ull;
        }
        at = end + 1ull;
        const CliSpan code = {raw.at, cli_found(raw, " #")};
        const CliSpan kept = cli_trimmed_of(code, '\\');
        if ((s_lines.used + kept.length + 1ull) > s_lines.text_most)
        {
            return;
        }
        if (s_lines.used != line_start)
        {
            s_lines.text[s_lines.used] = ' ';
            s_lines.used += 1u;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_lines.text + s_lines.used, .src = kept.at, .bytes = kept.length});
        // a line is below the lines' room, which an unsigned int holds
        s_lines.used += (unsigned int)kept.length;
        for (unsigned long long place = 0ull; place < code.length; place += 1ull)
        {
            const char letter = code.at[place];
            open += ((letter == '(') || (letter == '[') || (letter == '{')) ? 1ll : 0ll;
            open -= ((letter == ')') || (letter == ']') || (letter == '}')) ? 1ll : 0ll;
        }
        const CliSpan trimmed = cli_trimmed(code);
        const int joined = (trimmed.length > 0ull) && (trimmed.at[trimmed.length - 1ull] == '\\');
        if ((open <= 0ll) && !joined)
        {
            cli_line_ended(&line_start);
            open = 0ll;
        }
    }
    if (s_lines.used != line_start)
    {
        cli_line_ended(&line_start);
    }
}

// the expression the span says: its place, or CLI_PY_NONE
static unsigned int cli_span_parsed(CliSpan span)
{
    return cli_py_parse(span.at, span.length);
}

// `values` set as what the name of `length` bytes of `name` may be
static void cli_name_set(const char *name, unsigned long long length, CliValues values)
{
    for (unsigned int at = 0u; at < g_cli_py.names; at += 1u)
    {
        const char *const held = cli_py_text(g_cli_py.name[at].name);
        if ((cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = held, .other = name, .bytes = length}) == 0))
        {
            g_cli_py.name[at].values = values;
            return;
        }
    }
    const unsigned int text = cli_py_text_put(name, length);
    if ((text != CLI_PY_NONE) && (g_cli_py.names < CLI_PY_NAMES))
    {
        g_cli_py.name[g_cli_py.names] = (CliPyName){text, values};
        g_cli_py.names += 1u;
    }
}

// what a `for` takes its name from may be: each folder in a folder `os.scandir`, `os.listdir` or `iterdir` lists, or
// what its expression may be
static CliValues cli_looped(unsigned int expr)
{
    const CliExpr *const one = &g_cli_py.expr[expr];
    if (one->kind != CLI_EXPR_CALL)
    {
        return cli_py_eval(expr);
    }
    char dotted[CLI_NAME_LONGEST];
    CliLine line = {dotted, sizeof(dotted), 0ull};
    const int named = cli_py_dotted(one->left, &line);
    cli_line_closed(&line);
    const CliExpr *const callee = &g_cli_py.expr[one->left];
    const int scans =
        named && ((cellul.eq(&(CatenaFinitaCfg){.src = dotted, .other = "os.scandir", .cap = CLI_CELLS_POOL})) ||
                  (cellul.eq(&(CatenaFinitaCfg){.src = dotted, .other = "os.listdir", .cap = CLI_CELLS_POOL})));
    const int iterates =
        (callee->kind == CLI_EXPR_ATTR) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = cli_py_text(callee->text), .other = "iterdir", .cap = CLI_CELLS_POOL}));
    if (!scans && !iterates)
    {
        return cli_py_eval(expr);
    }
    CliValues folders = {g_cli_py.values, 0u};
    if (scans && (one->count > 0u))
    {
        folders = cli_py_eval(g_cli_py.item[one->first]);
    }
    else if (iterates)
    {
        folders = cli_py_eval(callee->left);
    }
    const unsigned int mark = g_cli_py.values;
    for (unsigned int at = 0u; at < folders.count; at += 1u)
    {
        cli_py_folders_in(cli_py_text(g_cli_py.value[folders.first + at]));
    }
    return cli_py_values_from(mark);
}

// a `for` line read: the name it sets and what it may be
static void cli_for_read(CliSpan rest)
{
    const unsigned long long in = cli_found(rest, " in ");
    if (in == rest.length)
    {
        return;
    }
    const CliSpan name = cli_trimmed((CliSpan){rest.at, in});
    const CliSpan iterable =
        cli_trimmed(cli_trimmed_of(cli_trimmed((CliSpan){rest.at + in + 4u, rest.length - in - 4ull}), ':'));
    const unsigned int expr = cli_span_parsed(iterable);
    if (expr == CLI_PY_NONE)
    {
        return;
    }
    const CliValues values = cli_looped(expr);
    if (cli_wordy_all(name))
    {
        cli_name_set(name.at, name.length, values);
    }
}

// what an expression may be put among those found
static void cli_found_put(CliSpan span)
{
    const unsigned int expr = cli_span_parsed(span);
    if (expr == CLI_PY_NONE)
    {
        return;
    }
    const CliValues values = cli_py_eval(expr);
    for (unsigned int at = 0u; (at < values.count) && (g_cli_py.founds < CLI_PY_VALUES); at += 1u)
    {
        g_cli_py.found[g_cli_py.founds] = g_cli_py.value[values.first + at];
        g_cli_py.founds += 1u;
    }
}

// a line's calls of `sys.path.insert`, `append` and `extend` read
static void cli_calls_read(CliSpan code)
{
    static const char *const s_leads[3] = {"sys.path.insert(", "sys.path.append(", "sys.path.extend("};
    for (unsigned int lead = 0u; lead < 3u; lead += 1u)
    {
        const unsigned long long at = cli_found(code, s_leads[lead]);
        if (at == code.length)
        {
            continue;
        }
        const unsigned long long skip =
            at + cellul.len(&(CatenaFinitaCfg){.src = s_leads[lead], .cap = CLI_CELLS_POOL});
        const CliSpan inner = {code.at + skip, code.length - skip};
        const unsigned long long end = cli_found_last(inner, ")");
        if (end == inner.length)
        {
            continue;
        }
        CliSpan given = {inner.at, end};
        if (lead == 0u)
        {
            const unsigned long long comma = cli_found(given, ",");
            given = (comma < given.length) ? (CliSpan){given.at + comma + 1u, given.length - comma - 1ull}
                                           : (CliSpan){given.at, 0ull};
        }
        cli_found_put(cli_trimmed(cli_trimmed_of(cli_trimmed(given), ',')));
    }
}

// whether `target` stands as a word in `value`
static int cli_reads_itself(CliSpan value, CliSpan target)
{
    unsigned long long start = 0ull;
    unsigned long long at = 0ull;
    while (at <= value.length)
    {
        unsigned int letter = ' ';
        const unsigned long long step =
            (at < value.length) ? cli_lossy_letter((const unsigned char *)value.at, value.length, at, &letter) : 1ull;
        if (!cli_hit_wordy(letter))
        {
            if (((at - start) == target.length) &&
                (memor.cmp(&(MemoriaCfg){.src = value.at + start, .other = target.at, .bytes = target.length}) == 0))
            {
                return 1;
            }
            start = at + step;
        }
        at += step;
    }
    return 0;
}

// a line that sets a name read: what the name may be, each folder above a name's values where it is set from itself
static void cli_set_read(CliSpan code)
{
    const unsigned long long equals = cli_found(code, "=");
    if (equals == code.length)
    {
        return;
    }
    const CliSpan whole = {code.at, equals};
    const CliSpan target = cli_trimmed((CliSpan){whole.at, cli_found(whole, ":")});
    const CliSpan value = {code.at + equals + 1u, code.length - equals - 1ull};
    if (cli_starts(value, "=") || (target.length == 0ull) || !cli_wordy_all(target))
    {
        return;
    }
    const unsigned int expr = cli_span_parsed(cli_trimmed(value));
    if (expr == CLI_PY_NONE)
    {
        return;
    }
    CliValues values = {g_cli_py.values, 0u};
    if (cli_reads_itself(value, target))
    {
        for (unsigned int at = 0u; at < g_cli_py.names; at += 1u)
        {
            const char *const held = cli_py_text(g_cli_py.name[at].name);
            if ((cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) == target.length) &&
                (memor.cmp(&(MemoriaCfg){.src = held, .other = target.at, .bytes = target.length}) == 0))
            {
                const CliValues was = g_cli_py.name[at].values;
                const unsigned int mark = g_cli_py.values;
                for (unsigned int one = 0u; one < was.count; one += 1u)
                {
                    cli_py_upward(cli_py_text(g_cli_py.value[was.first + one]));
                }
                values = cli_py_values_from(mark);
            }
        }
    }
    else
    {
        values = cli_py_eval(expr);
    }
    if (values.count != 0u)
    {
        cli_name_set(target.at, target.length, values);
    }
}

// the folders the script at `file`, `size` bytes of `text`, puts on `sys.path`, as it may write them, into the found
void cli_py_paths_in(const char *file, const char *text, unsigned long long size)
{
    if (!cli_py_reading_taken(size))
    {
        return;
    }
    // `__file__` is written with the system's separator, as Python gives it
    memor.cpy(&(MemoriaCfg){.dst = g_cli_py.file,
                            .src = file,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
#if defined(_WIN32)
    for (char *at = g_cli_py.file; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '/') ? '\\' : at[0];
    }
#endif
    cli_logical_lines(text, size);
    for (unsigned int index = 0u; index < s_lines.count; index += 1u)
    {
        const CliSpan code = cli_trimmed((CliSpan){s_lines.text + s_lines.start[index], s_lines.length[index]});
        if (cli_starts(code, "for "))
        {
            cli_for_read((CliSpan){code.at + 4u, code.length - 4ull});
            continue;
        }
        cli_calls_read(code);
        if (cli_starts(code, "sys.path["))
        {
            const CliSpan rest = {code.at + 9u, code.length - 9ull};
            const unsigned long long set = cli_found(rest, "] =");
            if (set < rest.length)
            {
                cli_found_put(cli_trimmed((CliSpan){rest.at + set + 3u, rest.length - set - 3ull}));
            }
        }
        cli_set_read(code);
    }
}
