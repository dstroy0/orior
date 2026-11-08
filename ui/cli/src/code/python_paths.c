// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths.c: a Python script read line by line for what it puts on `sys.path`
// name in the tree counted, the most used first
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "python_paths_internal.h"

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

typedef struct
{
    CliSpan span;
    char letter;
    const char *text;
    unsigned int *line_start;
    unsigned long long size;
    const char *name;
    unsigned long long length;
    CliValues values;
    unsigned int expr;
    CliSpan rest;
    CliSpan code;
    CliSpan value;
    CliSpan target;
} PythonPathsCtx;
// the span with the whitespace at either end left off
EMBED_INLINE CliSpan cli_trim(const PythonPathsCtx *args)
{
    const unsigned char *const bytes = (const unsigned char *)args->span.at;
    unsigned long long start = args->span.length;
    unsigned long long end = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->span.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = bytes,
                       .length = args->span.length, .at = at, .letter = &letter);
        if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
        {
            start = (start == args->span.length) ? at : start;
            end = at + step;
        }
        at += step;
    }
    return (start < end) ? (CliSpan){args->span.at + start, end - start} : (CliSpan){args->span.at, 0ULL};
}

// the span with every `letter` at its end left off
EMBED_INLINE CliSpan cli_trimmed_get(const PythonPathsCtx *args)
{
    CliSpan span = args->span;
    while ((span.length > 0ULL) && (span.at[span.length - 1ULL] == args->letter))
    {
        span.length -= 1ULL;
    }
    return span;
}

// the first place `text` stands in the span, or the span's length where it stands nowhere; and the last
EMBED_INLINE unsigned long long cli_on_find(const PythonPathsCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    for (unsigned long long at = 0ULL; (at + length) <= args->span.length; at += 1ULL)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->span.at + at, .other = args->text, .bytes = length}) == 0)
        {
            return at;
        }
    }
    return args->span.length;
}

EMBED_INLINE unsigned long long cli_found_last(const PythonPathsCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    for (unsigned long long at = args->span.length + 1ULL; at > length; at -= 1ULL)
    {
        if (memor.cmp(
                &(MemoriaCfg){.src = args->span.at + (at - 1ULL - length), .other = args->text, .bytes = length}) == 0)
        {
            return at - 1ULL - length;
        }
    }
    return args->span.length;
}

// whether the span starts with `text`, and whether each of its letters joins a word
EMBED_INLINE int cli_starts(const PythonPathsCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    return (args->span.length >= length) &&
           (memor.cmp(&(MemoriaCfg){.src = args->span.at, .other = args->text, .bytes = length}) == 0);
}

EMBED_INLINE int cli_wordy_all(const PythonPathsCtx *args)
{
    unsigned long long at = 0ULL;
    while (at < args->span.length)
    {
        unsigned int letter = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                         .text_bytes = (const unsigned char *)args->span.at, .length = args->span.length, .at = at,
                         .letter = &letter);
        if (!EMBED_CALL(cli_files_letter_internal.hit_wordy, CliFilesLetterCtx, .letter = letter))
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
    unsigned int text_max;
    unsigned int *start;
    unsigned int *length;
    unsigned int count;
    unsigned int lines_max;
} CliPyLines;

static CliPyLines s_lines;

void cli_py_lines_position(const CliPythonPathsTextTextMaxCtx *args)
{
    s_lines = (CliPyLines){args->text_out, 0u, args->text_max, args->start, args->length_u32s, 0u, args->lines_max};
}

// the line being made ended, as one of the logical lines
EMBED_INLINE void cli_line_end(const PythonPathsCtx *args)
{
    unsigned int *line_start = args->line_start;
    if (s_lines.count < s_lines.lines_max)
    {
        s_lines.start[s_lines.count] = *line_start;
        s_lines.length[s_lines.count] = s_lines.used - *line_start;
        s_lines.count += 1u;
    }
    *line_start = s_lines.used;
}

// `size` bytes of a script read as its logical lines
EMBED_INLINE void cli_logical_lines(const PythonPathsCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    s_lines.used = 0u;
    s_lines.count = 0u;
    unsigned int line_start = 0u;
    long long open = 0LL;
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size);
        const unsigned long long end = bounds.stop;
        CliSpan raw = {text + at, end - at};
        raw.length -= bounds.stop - bounds.cut;
        at = end + 1ULL;
        const CliSpan code = {raw.at, EMBED_CALL(cli_on_find, PythonPathsCtx, .span = raw, .text = " #")};
        const CliSpan trimmed_line = EMBED_CALL(cli_trimmed_get, PythonPathsCtx, .span = code, .letter = '\\');
        if ((s_lines.used + trimmed_line.length + 1ULL) > s_lines.text_max)
        {
            return;
        }
        if (s_lines.used != line_start)
        {
            s_lines.text[s_lines.used] = ' ';
            s_lines.used += 1u;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = s_lines.text + s_lines.used, .src = trimmed_line.at, .bytes = trimmed_line.length});
        // a line is below the lines' capacity, which an unsigned int stores
        s_lines.used += (unsigned int)trimmed_line.length;
        for (unsigned long long place = 0ULL; place < code.length; place += 1ULL)
        {
            const char letter = code.at[place];
            open += ((letter == '(') || (letter == '[') || (letter == '{')) ? 1LL : 0LL;
            open -= ((letter == ')') || (letter == ']') || (letter == '}')) ? 1LL : 0LL;
        }
        const CliSpan trimmed = EMBED_CALL(cli_trim, PythonPathsCtx, .span = code);
        const int joined = (trimmed.length > 0ULL) && (trimmed.at[trimmed.length - 1ULL] == '\\');
        if ((open <= 0LL) && !joined)
        {
            EMBED_CALL(cli_line_end, PythonPathsCtx, .line_start = &line_start);
            open = 0LL;
        }
    }
    if (s_lines.used != line_start)
    {
        EMBED_CALL(cli_line_end, PythonPathsCtx, .line_start = &line_start);
    }
}

// the expression the span denotes: its place, or CLI_PY_NONE
EMBED_INLINE unsigned int cli_span_parse(const PythonPathsCtx *args)
{
    return EMBED_CALL(cli_python_paths_text_internal.py_parse, CliPythonPathsTextCtx, .text = args->span.at,
                      .length = args->span.length);
}

// `values` set as what the name of `length` bytes of `name` may be
EMBED_INLINE void cli_name_set(const PythonPathsCtx *args)
{
    unsigned long long length = args->length;
    CliValues values = args->values;
    for (unsigned int at = 0u; at < g_cli_py.names; at += 1u)
    {
        const char *const name_at =
            EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = g_cli_py.name[at].name);
        if ((cellul.len(&(CatenaFinitaCfg){.src = name_at, .cap = CLI_POOL_BYTES}) == length) &&
            (memor.cmp(&(MemoriaCfg){.src = name_at, .other = args->name, .bytes = length}) == 0))
        {
            g_cli_py.name[at].values = values;
            return;
        }
    }
    const unsigned int text = EMBED_CALL(cli_python_paths_text_internal.py_text_append, CliPythonPathsTextCtx,
                                         .text = args->name, .length = length);
    if ((text != CLI_PY_NONE) && (g_cli_py.names < CLI_PY_NAMES))
    {
        g_cli_py.name[g_cli_py.names] = (CliPyName){text, values};
        g_cli_py.names += 1u;
    }
}

// what a `for` takes its name from may be: each folder in a folder `os.scandir`, `os.listdir` or `iterdir` lists, or
// what its expression may be
EMBED_INLINE CliValues cli_loop(const PythonPathsCtx *args)
{
    unsigned int expr = args->expr;
    const CliExpr *const one = &g_cli_py.expr[expr];
    if (one->type != CLI_EXPR_CALL)
    {
        return EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = expr);
    }
    char dotted[CLI_NAME_MAX_LEN];
    VerbaLinea line = {dotted, sizeof(dotted), 0ULL};
    const int named =
        EMBED_CALL(cli_python_paths_expr_internal.py_dot, CliPythonPathsExprCtx, .expr = one->left, .out = &line);
    mmgr_line_finish(&line);
    const CliExpr *const callee = &g_cli_py.expr[one->left];
    const int scans =
        named && ((cellul.eq(&(CatenaFinitaCfg){.src = dotted, .other = "os.scandir", .cap = CLI_POOL_BYTES})) ||
                  (cellul.eq(&(CatenaFinitaCfg){.src = dotted, .other = "os.listdir", .cap = CLI_POOL_BYTES})));
    const int iterates = (callee->type == CLI_EXPR_ATTR) &&
                         (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_python_paths_at_internal.py_text,
                                                                         CliPythonPathsAtCtx, .at = callee->text),
                                                       .other = "iterdir",
                                                       .cap = CLI_POOL_BYTES}));
    if (!scans && !iterates)
    {
        return EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = expr);
    }
    CliValues folders = {g_cli_py.values, 0u};
    if (scans && (one->count > 0u))
    {
        folders = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx,
                             .expr = g_cli_py.item[one->first]);
    }
    else if (iterates)
    {
        folders = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = callee->left);
    }
    const unsigned int mark = g_cli_py.values;
    for (unsigned int at = 0u; at < folders.count; at += 1u)
    {
        EMBED_CALL(cli_python_paths_internal.py_folders_in, CliPythonPathsCtx,
                   .folder = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx,
                                        .at = g_cli_py.value[folders.first + at]));
    }
    return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
}

// a `for` line read: the name it sets and what it may be
EMBED_INLINE void cli_for_read(const PythonPathsCtx *args)
{
    const unsigned long long in = EMBED_CALL(cli_on_find, PythonPathsCtx, .span = args->rest, .text = " in ");
    if (in == args->rest.length)
    {
        return;
    }
    const CliSpan name = EMBED_CALL(cli_trim, PythonPathsCtx, .span = (CliSpan){args->rest.at, in});
    const CliSpan iterable =
        EMBED_CALL(cli_trim, PythonPathsCtx,
                   .span = EMBED_CALL(
                       cli_trimmed_get, PythonPathsCtx,
                       .span = EMBED_CALL(cli_trim, PythonPathsCtx,
                                          .span = (CliSpan){args->rest.at + in + 4u, args->rest.length - in - 4ULL}),
                       .letter = ':'));
    const unsigned int expr = EMBED_CALL(cli_span_parse, PythonPathsCtx, .span = iterable);
    if (expr == CLI_PY_NONE)
    {
        return;
    }
    const CliValues values = EMBED_CALL(cli_loop, PythonPathsCtx, .expr = expr);
    if (EMBED_CALL(cli_wordy_all, PythonPathsCtx, .span = name))
    {
        EMBED_CALL(cli_name_set, PythonPathsCtx, .name = name.at, .length = name.length, .values = values);
    }
}

// what an expression may be put among those found
EMBED_INLINE void cli_found_append(const PythonPathsCtx *args)
{
    const unsigned int expr = cli_span_parse(args);
    if (expr == CLI_PY_NONE)
    {
        return;
    }
    const CliValues values = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = expr);
    for (unsigned int at = 0u; (at < values.count) && (g_cli_py.founds < CLI_PY_VALUES); at += 1u)
    {
        g_cli_py.found[g_cli_py.founds] = g_cli_py.value[values.first + at];
        g_cli_py.founds += 1u;
    }
}

// a line's calls of `sys.path.insert`, `append` and `extend` read
EMBED_INLINE void cli_calls_read(const PythonPathsCtx *args)
{
    static const char *const s_leads[3] = {"sys.path.insert(", "sys.path.append(", "sys.path.extend("};
    for (unsigned int lead = 0u; lead < 3u; lead += 1u)
    {
        const unsigned long long at =
            EMBED_CALL(cli_on_find, PythonPathsCtx, .span = args->code, .text = s_leads[lead]);
        if (at == args->code.length)
        {
            continue;
        }
        const unsigned long long skip =
            at + cellul.len(&(CatenaFinitaCfg){.src = s_leads[lead], .cap = CLI_POOL_BYTES});
        const CliSpan inner = {args->code.at + skip, args->code.length - skip};
        const unsigned long long end = EMBED_CALL(cli_found_last, PythonPathsCtx, .span = inner, .text = ")");
        if (end == inner.length)
        {
            continue;
        }
        CliSpan supplied = {inner.at, end};
        if (lead == 0u)
        {
            const unsigned long long comma = EMBED_CALL(cli_on_find, PythonPathsCtx, .span = supplied, .text = ",");
            supplied = (comma < supplied.length) ? (CliSpan){supplied.at + comma + 1u, supplied.length - comma - 1ULL}
                                                 : (CliSpan){supplied.at, 0ULL};
        }
        EMBED_CALL(cli_found_append, PythonPathsCtx,
                   .span = EMBED_CALL(cli_trim, PythonPathsCtx,
                                      .span = EMBED_CALL(cli_trimmed_get, PythonPathsCtx,
                                                         .span = EMBED_CALL(cli_trim, PythonPathsCtx, .span = supplied),
                                                         .letter = ',')));
    }
}

// whether `target` stands as a word in `value`
EMBED_INLINE int cli_reads_itself(const PythonPathsCtx *args)
{
    unsigned long long start = 0ULL;
    unsigned long long at = 0ULL;
    while (at <= args->value.length)
    {
        unsigned int letter = ' ';
        const unsigned long long step = (at < args->value.length)
                                            ? EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                         .text_bytes = (const unsigned char *)args->value.at,
                                                         .length = args->value.length, .at = at, .letter = &letter)
                                            : 1ULL;
        if (!EMBED_CALL(cli_files_letter_internal.hit_wordy, CliFilesLetterCtx, .letter = letter))
        {
            if (((at - start) == args->target.length) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = args->value.at + start, .other = args->target.at, .bytes = args->target.length}) == 0))
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
EMBED_INLINE void cli_set_read(const PythonPathsCtx *args)
{
    const unsigned long long equals = EMBED_CALL(cli_on_find, PythonPathsCtx, .span = args->code, .text = "=");
    if (equals == args->code.length)
    {
        return;
    }
    const CliSpan whole = {args->code.at, equals};
    const CliSpan target =
        EMBED_CALL(cli_trim, PythonPathsCtx,
                   .span = (CliSpan){whole.at, EMBED_CALL(cli_on_find, PythonPathsCtx, .span = whole, .text = ":")});
    const CliSpan value = {args->code.at + equals + 1u, args->code.length - equals - 1ULL};
    if (EMBED_CALL(cli_starts, PythonPathsCtx, .span = value, .text = "=") || (target.length == 0ULL) ||
        !EMBED_CALL(cli_wordy_all, PythonPathsCtx, .span = target))
    {
        return;
    }
    const unsigned int expr =
        EMBED_CALL(cli_span_parse, PythonPathsCtx, .span = EMBED_CALL(cli_trim, PythonPathsCtx, .span = value));
    if (expr == CLI_PY_NONE)
    {
        return;
    }
    CliValues values = {g_cli_py.values, 0u};
    if (EMBED_CALL(cli_reads_itself, PythonPathsCtx, .value = value, .target = target))
    {
        for (unsigned int at = 0u; at < g_cli_py.names; at += 1u)
        {
            const char *const name_at =
                EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = g_cli_py.name[at].name);
            if ((cellul.len(&(CatenaFinitaCfg){.src = name_at, .cap = CLI_POOL_BYTES}) != target.length) ||
                (memor.cmp(&(MemoriaCfg){.src = name_at, .other = target.at, .bytes = target.length}) != 0))
            {
                continue;
            }
            const CliValues was = g_cli_py.name[at].values;
            const unsigned int mark = g_cli_py.values;
            for (unsigned int one = 0u; one < was.count; one += 1u)
            {
                EMBED_CALL(cli_python_paths_out_internal.py_upward, CliPythonPathsOutCtx,
                           .path = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx,
                                              .at = g_cli_py.value[was.first + one]));
            }
            values = EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
        }
    }
    else
    {
        values = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = expr);
    }
    if (values.count != 0u)
    {
        EMBED_CALL(cli_name_set, PythonPathsCtx, .name = target.at, .length = target.length, .values = values);
    }
}

// the folders the script at `file`, `size` bytes of `text`, puts on `sys.path`, as it may write them, into the found
void cli_py_paths_in(const CliPythonPathsSizeCtx *args)
{
    if (!cli_python_paths_size_internal.py_read_state_alloc(args))
    {
        return;
    }
    // `__file__` is written with the system's separator, as Python gives it
    memor.cpy(&(MemoriaCfg){.dst = g_cli_py.file,
                            .src = args->file,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES}) + 1u});
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = g_cli_py.file,
               .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_py.file, .cap = CLI_POOL_BYTES}), .from = '/',
               .to = '\\');
#endif
    EMBED_CALL(cli_logical_lines, PythonPathsCtx, .text = args->text, .size = args->size);
    for (unsigned int index = 0u; index < s_lines.count; index += 1u)
    {
        const CliSpan code = EMBED_CALL(cli_trim, PythonPathsCtx,
                                        .span = (CliSpan){s_lines.text + s_lines.start[index], s_lines.length[index]});
        if (EMBED_CALL(cli_starts, PythonPathsCtx, .span = code, .text = "for "))
        {
            EMBED_CALL(cli_for_read, PythonPathsCtx, .rest = (CliSpan){code.at + 4u, code.length - 4ULL});
            continue;
        }
        EMBED_CALL(cli_calls_read, PythonPathsCtx, .code = code);
        if (EMBED_CALL(cli_starts, PythonPathsCtx, .span = code, .text = "sys.path["))
        {
            const CliSpan rest = {code.at + 9u, code.length - 9ULL};
            const unsigned long long set = EMBED_CALL(cli_on_find, PythonPathsCtx, .span = rest, .text = "] =");
            if (set < rest.length)
            {
                EMBED_CALL(cli_found_append, PythonPathsCtx,
                           .span = EMBED_CALL(cli_trim, PythonPathsCtx,
                                              .span = (CliSpan){rest.at + set + 3u, rest.length - set - 3ULL}));
            }
        }
        EMBED_CALL(cli_set_read, PythonPathsCtx, .code = code);
    }
}
