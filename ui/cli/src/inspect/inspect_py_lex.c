// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_lex.c: Python's text read as logical lines of tokens: a line runs on inside brackets and past a backslash
// at its end, its indent the width of its first physical line's, a tab reaching the next multiple of eight; a string,
// with its prefix, is one token, and the names an f-string's or a t-string's fields read are tokens after it
#include "inspect_py.h"

// the most names one string's fields read that are recorded
#define CLI_PY_FIELD_NAMES 4096u

CliPyLine *g_cli_py_line;
unsigned int g_cli_py_lines;

void cli_inspect_py_lex_layout(const CliInspectPartitionCtx *args)
{
    g_cli_py_line =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)CLI_INSPECT_LINES * sizeof(CliPyLine));
}

const char *const g_cli_py_keywords[35] = {"False",  "None",   "True",    "and",      "as",       "assert", "async",
                                           "await",  "break",  "class",   "continue", "def",      "del",    "elif",
                                           "else",   "except", "finally", "for",      "from",     "global", "if",
                                           "import", "in",     "is",      "lambda",   "nonlocal", "not",    "or",
                                           "pass",   "raise",  "return",  "try",      "while",    "with",   "yield"};

// the operators, the longest that matches first
static const char *const s_ops[47] = {"**=", "//=", ">>=", "<<=", "...", "->", ":=", "==", "!=", "<=", ">=", "+=",
                                      "-=",  "*=",  "/=",  "%=",  "&=",  "|=", "^=", "@=", "**", "//", "<<", ">>",
                                      "+",   "-",   "*",   "/",   "%",   "@",  "&",  "|",  "^",  "~",  "<",  ">",
                                      "(",   ")",   "[",   "]",   "{",   "}",  ",",  ":",  ".",  ";",  "="};

// the prefixes a string may have, in lower case
static const char *const s_prefixes[13] = {"r", "u", "b", "f", "t", "br", "rb", "fr", "rf", "tr", "rt", "bt", "tb"};

// the names a string's fields read, recorded until the string's own token is put
static CliInspectTok s_field_names[CLI_PY_FIELD_NAMES];

typedef struct
{
    unsigned int letter;
    const char *src;
    unsigned long long start;
    unsigned long long end;
    CliInspectScan *scan;
    CliInspectPosition from;
    unsigned long long prefix_size;
    unsigned int indent;
    unsigned int *first;
    int *depth;
    unsigned int *indent_pointer;
    unsigned int count;
    unsigned int quote;
    int triple;
    int fields;
    unsigned int *braces;
    unsigned int *names;
} InspectPyLexCtx;
EMBED_INLINE int lex_name_start(const InspectPyLexCtx *args)
{
    unsigned int letter = args->letter;
    return (letter == '_') || EMBED_CALL(cli_inspect_internal.alphabetic, CliInspectCtx, .letter = letter);
}

EMBED_INLINE int lex_name(const InspectPyLexCtx *args)
{
    unsigned int letter = args->letter;
    return (letter == '_') || EMBED_CALL(cli_inspect_internal.alphanumeric, CliInspectCtx, .letter = letter);
}

// whether the bytes from `start` to `end` of `src` are a prefix a string may have, case aside
EMBED_INLINE int lex_prefix(const InspectPyLexCtx *args)
{
    const char *src = args->src;
    char lower[4];
    if (args->end - args->start > 2u)
    {
        return 0;
    }
    for (unsigned long long at = args->start; at < args->end; at += 1u)
    {
        lower[at - args->start] = ((src[at] >= 'A') && (src[at] <= 'Z')) ? (char)(src[at] - 'A' + 'a') : src[at];
    }
    lower[args->end - args->start] = '\0';
    for (unsigned int at = 0u; at < 13u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = s_prefixes[at], .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// `count` letters passed over from where the scan stands
EMBED_INLINE void lex_passed(const InspectPyLexCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = args->scan);
    }
}

// whether `letter`, where the scan stands, closes a string opened by `quote`: the quote alone, or for a `triple`
// string the quote three times
EMBED_INLINE int lex_quote_closes(const InspectPyLexCtx *args)
{
    return (args->letter == args->quote) &&
           (!args->triple || ((EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx,
                                          .scan = args->scan, .ahead = 1u) == args->quote) &&
                              (EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx,
                                          .scan = args->scan, .ahead = 2u) == args->quote)));
}

// the name a field reads, where the scan stands, read to its end, and recorded after the `names` recorded before it
// where no dot or digit stands before it and they number fewer than CLI_PY_FIELD_NAMES
EMBED_INLINE void lex_field_name(const InspectPyLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    const int after_dot = (scan->pos > 0u) && (scan->src[scan->pos - 1u] == '.');
    const int after_digit =
        (scan->pos > 0u) && (scan->src[scan->pos - 1u] >= '0') && (scan->src[scan->pos - 1u] <= '9');
    const unsigned long long name_start = scan->pos;
    const CliInspectPosition name_from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan);
    while (EMBED_CALL(lex_name, InspectPyLexCtx,
                      .letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan)))
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    }
    if (!after_dot && !after_digit && (*args->names < CLI_PY_FIELD_NAMES))
    {
        // a text read is at most 4 GiB, which a token's bytes count
        s_field_names[*args->names] =
            (CliInspectTok){CLI_PY_NAME,
                            1u,
                            (unsigned int)name_start,
                            (unsigned int)scan->pos,
                            name_from,
                            EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan)};
        *args->names += 1u;
    }
}

// one step of a string's body from `letter`, where the scan stands, in a string opened by `quote`, `triple` or not,
// whose `fields` are read or not: an escape passed over, the closing quote passed over, a doubled brace passed over,
// a field's brace counted into `braces`, a name a field reads recorded in `names`, else the letter passed over; 0
// where the string ends there, else 1
EMBED_INLINE int lex_string_step(const InspectPyLexCtx *args)
{
    const unsigned int letter = args->letter;
    if (letter == '\\')
    {
        EMBED_CALL(lex_passed, InspectPyLexCtx, .scan = args->scan, .count = 2u);
        return 1;
    }
    if (EMBED_CALL(lex_quote_closes, InspectPyLexCtx, .scan = args->scan, .letter = letter, .quote = args->quote,
                   .triple = args->triple))
    {
        EMBED_CALL(lex_passed, InspectPyLexCtx, .scan = args->scan, .count = args->triple ? 3u : 1u);
        return 0;
    }
    if ((letter == '\n') && !args->triple)
    {
        return 0;
    }
    if (args->fields && (letter == '{') && (*args->braces == 0u) &&
        (EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx, .scan = args->scan, .ahead = 1u) ==
         '{'))
    {
        EMBED_CALL(lex_passed, InspectPyLexCtx, .scan = args->scan, .count = 2u);
        return 1;
    }
    if (args->fields && (letter == '{'))
    {
        *args->braces += 1u;
    }
    else if (args->fields && (letter == '}') && (*args->braces > 0u))
    {
        *args->braces -= 1u;
    }
    else if ((*args->braces > 0u) && EMBED_CALL(lex_name_start, InspectPyLexCtx, .letter = letter))
    {
        EMBED_CALL(lex_field_name, InspectPyLexCtx, .scan = args->scan, .names = args->names);
        return 1;
    }
    (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = args->scan);
    return 1;
}

// a string from its prefix at `start`, `prefix_size` bytes before the quote, to its closing quote, and the names its
// fields read where it is an f-string or a t-string
static void lex_string(const InspectPyLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    unsigned long long start = args->start;
    CliInspectPosition from = args->from;
    const unsigned int peeked = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
    const unsigned int quote = (peeked == CLI_INSPECT_END) ? '"' : peeked;
    const int triple = (EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx, .scan = scan,
                                   .ahead = 1u) == quote) &&
                       (EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx, .scan = scan,
                                   .ahead = 2u) == quote);
    int fields = 0;
    for (unsigned long long at = start; at < start + args->prefix_size; at += 1u)
    {
        fields = fields || (scan->src[at] == 'f') || (scan->src[at] == 'F') || (scan->src[at] == 't') ||
                 (scan->src[at] == 'T');
    }
    EMBED_CALL(lex_passed, InspectPyLexCtx, .scan = scan, .count = triple ? 3u : 1u);
    unsigned int names = 0u;
    unsigned int braces = 0u;
    unsigned int letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
    int going = 1;
    while (going && (letter != CLI_INSPECT_END))
    {
        going = EMBED_CALL(lex_string_step, InspectPyLexCtx, .scan = scan, .letter = letter, .quote = quote,
                           .triple = triple, .fields = fields, .braces = &braces, .names = &names);
        letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
    }
    EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_PY_STR,
               .start = start, .from = from, .field = 0);
    for (unsigned int at = 0u; at < names; at += 1u)
    {
        if (g_cli_inspect_toks == CLI_INSPECT_TOKENS)
        {
            g_cli_inspect_over = 1;
            break;
        }
        g_cli_inspect_tok[g_cli_inspect_toks] = s_field_names[at];
        g_cli_inspect_toks += 1u;
    }
}

// the logical line of the tokens from `first` put on the lines where it contains any
EMBED_INLINE void lex_line_append(const InspectPyLexCtx *args)
{
    unsigned int *first = args->first;
    if (g_cli_inspect_toks > *first)
    {
        if (g_cli_py_lines == CLI_INSPECT_LINES)
        {
            g_cli_inspect_over = 1;
        }
        else
        {
            g_cli_py_line[g_cli_py_lines] = (CliPyLine){args->indent, *first, g_cli_inspect_toks - *first};
            g_cli_py_lines += 1u;
        }
    }
    *first = g_cli_inspect_toks;
}

// a number from where the scan stands
EMBED_INLINE void lex_number(const InspectPyLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    const unsigned long long start = scan->pos;
    const CliInspectPosition from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan);
    for (unsigned int letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
         (letter != CLI_INSPECT_END) &&
         (EMBED_CALL(cli_inspect_internal.alphanumeric, CliInspectCtx, .letter = letter) || (letter == '_') ||
          (letter == '.'));
         letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
    {
        const int hex = (scan->pos - start >= 2u) && (scan->src[start] == '0') &&
                        ((scan->src[start + 1u] == 'x') || (scan->src[start + 1u] == 'X'));
        const int exponent = ((letter == 'e') || (letter == 'E')) && !hex;
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        if (exponent &&
            ((EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan) == '+') ||
             (EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan) == '-')))
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        }
    }
    EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_PY_NUMBER,
               .start = start, .from = from, .field = 0);
}

// an operator, the longest that matches, else the one letter, from where the scan stands, the brackets' depth tracked
EMBED_INLINE void lex_operator(const InspectPyLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    int *depth = args->depth;
    const unsigned long long start = scan->pos;
    const CliInspectPosition from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan);
    const char *op = NULL;
    {
        unsigned int at = 0u;
        while ((op == NULL) && (at < 47u))
        {
            const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = s_ops[at], .cap = CLI_POOL_BYTES});
            op = ((scan->size - start >= length) &&
                  (memor.cmp(&(MemoriaCfg){.src = scan->src + start, .other = s_ops[at], .bytes = length}) == 0))
                     ? s_ops[at]
                     : NULL;
            at += 1u;
        }
    }
    if (op == NULL)
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    }
    else
    {
        for (embed_word at = 0u; at < cellul.len(&(CatenaFinitaCfg){.src = op, .cap = CLI_POOL_BYTES}); at += 1u)
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        }
        *depth += ((op[0] == '(') || (op[0] == '[') || (op[0] == '{')) ? 1 : 0;
        if (((op[0] == ')') || (op[0] == ']') || (op[0] == '}')) && (*depth > 0))
        {
            *depth -= 1;
        }
    }
    EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_PY_OP, .start = start,
               .from = from, .field = 0);
}

// the indent of a line read at its start, its blank and comment lines passed over: 1, or 0 at the text's end
EMBED_INLINE int lex_line_start(const InspectPyLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    unsigned int *indent = args->indent_pointer;
    for (;;)
    {
        unsigned int width = 0u;
        for (unsigned int letter =
                 EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
             ; letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
        {
            if (letter == ' ')
            {
                width += 1u;
            }
            else if (letter == '\t')
            {
                width = ((width / 8u) + 1u) * 8u;
            }
            else if ((letter != 0x0cu) && (letter != '\r'))
            {
                break;
            }
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        }
        const unsigned int letter =
            EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
        if (letter == CLI_INSPECT_END)
        {
            return 0;
        }
        if (letter == '\n')
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
            continue;
        }
        if (letter == '#')
        {
            EMBED_CALL(cli_inspect_size_scan_internal.skip_line, CliInspectSizeScanCtx, .scan_out = scan);
            continue;
        }
        *indent = width;
        return 1;
    }
}

void cli_py_read(const CliInspectPySrcSizeCfg *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliInspectScan scan;
    EMBED_CALL(cli_inspect_size_scan_internal.scan_begin, CliInspectSizeScanCtx, .scan_out = &scan, .src = src,
               .size = size);
    g_cli_inspect_toks = 0u;
    g_cli_inspect_comments = 0u;
    g_cli_inspect_over = 0;
    g_cli_py_lines = 0u;
    unsigned int first = 0u;
    unsigned int indent = 0u;
    int depth = 0;
    int at_start = 1;
    int going = 1;
    while (going)
    {
        if (at_start)
        {
            if (!EMBED_CALL(lex_line_start, InspectPyLexCtx, .scan = &scan, .indent_pointer = &indent))
            {
                going = 0;
                continue;
            }
            at_start = 0;
        }
        const unsigned int letter =
            EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = &scan);
        if (letter == CLI_INSPECT_END)
        {
            going = 0;
            continue;
        }
        const unsigned long long start = scan.pos;
        const CliInspectPosition from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = &scan);
        if (letter == '\n')
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = &scan);
            if (depth == 0)
            {
                EMBED_CALL(lex_line_append, InspectPyLexCtx, .indent = indent, .first = &first);
                at_start = 1;
            }
        }
        else if ((letter == ' ') || (letter == '\t') || (letter == '\r') || (letter == 0x0cu))
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = &scan);
        }
        else if (letter == '#')
        {
            EMBED_CALL(cli_inspect_size_scan_internal.skip_line, CliInspectSizeScanCtx, .scan_out = &scan);
        }
        else if (letter == '\\')
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = &scan);
            if (EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = &scan) == '\r')
            {
                (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = &scan);
            }
            if (EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = &scan) == '\n')
            {
                (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = &scan);
            }
        }
        else if ((letter == '\'') || (letter == '"'))
        {
            EMBED_CALL(lex_string, InspectPyLexCtx, .scan = &scan, .start = start, .from = from, .prefix_size = 0ull);
        }
        else if (EMBED_CALL(lex_name_start, InspectPyLexCtx, .letter = letter))
        {
            while (EMBED_CALL(
                lex_name, InspectPyLexCtx,
                .letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = &scan)))
            {
                (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = &scan);
            }
            const unsigned int next =
                EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = &scan);
            if (((next == '\'') || (next == '"')) &&
                EMBED_CALL(lex_prefix, InspectPyLexCtx, .src = src, .start = start, .end = scan.pos))
            {
                EMBED_CALL(lex_string, InspectPyLexCtx, .scan = &scan, .start = start, .from = from,
                           .prefix_size = scan.pos - start);
            }
            else
            {
                EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = &scan, .type = CLI_PY_NAME,
                           .start = start, .from = from, .field = 0);
            }
        }
        else if (((letter >= '0') && (letter <= '9')) ||
                 ((letter == '.') &&
                  (EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx, .scan = &scan,
                              .ahead = 1u) >= '0') &&
                  (EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx, .scan = &scan,
                              .ahead = 1u) <= '9')))
        {
            EMBED_CALL(lex_number, InspectPyLexCtx, .scan = &scan);
        }
        else
        {
            EMBED_CALL(lex_operator, InspectPyLexCtx, .scan = &scan, .depth = &depth);
        }
    }
    EMBED_CALL(lex_line_append, InspectPyLexCtx, .indent = indent, .first = &first);
}

const CliInspectTok *cli_py_tok(const CliInspectPyCfg *args)
{
    return &g_cli_inspect_tok[args->line->first + args->at];
}

int cli_py_is(const CliInspectPySrcTokCfg *args)
{
    const char *src = args->src;
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES});
    return (args->tok->end - args->tok->start == length) &&
           (memor.cmp(&(MemoriaCfg){.src = src + args->tok->start, .other = args->word_text, .bytes = length}) == 0);
}

int cli_py_keyword(const CliInspectPySrcTokCfg *args)
{
    for (unsigned int at = 0u; at < 35u; at += 1u)
    {
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src, .tok = args->tok,
                       .word_text = g_cli_py_keywords[at]))
        {
            return 1;
        }
    }
    return 0;
}

int cli_py_opens(const CliInspectPySrcLineCfg *args)
{
    const CliInspectTok *const last =
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->line, .at = args->line->count - 1u);
    return (last->type == CLI_PY_OP) && EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg,
                                                   .src = args->src, .tok = last, .word_text = ":");
}

unsigned int cli_py_block_end(const CliInspectPyCfg *args)
{
    const unsigned int indent = g_cli_py_line[args->at].indent;
    unsigned int end = args->at + 1u;
    while ((end < g_cli_py_lines) && (g_cli_py_line[end].indent > indent))
    {
        end += 1u;
    }
    return end;
}

int cli_py_head_is(const CliInspectPySrcLineCfg *args)
{
    const CliInspectTok *const first = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->line, .at = 0u);
    const CliInspectTok *const head =
        (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src, .tok = first,
                    .word_text = "async") &&
         (args->line->count > 1u))
            ? EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->line, .at = 1u)
            : first;
    return EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src, .tok = head,
                      .word_text = args->word_text);
}

int cli_py_contains(const CliInspectPySrcWordCfg *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = args->src, .tok = &args->toks[at],
                       .word_text = args->word_text))
        {
            return 1;
        }
    }
    return 0;
}
