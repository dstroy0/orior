// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_lex.c: JavaScript's tokens: strings, templates with the code of their `${}` read as tokens before them,
// regular expressions where a value is expected, names, numbers and punctuation, the longest that matches; and the
// reading of them: the bracket each bracket matches, what each brace opens, and the brace each token stands directly
// inside
#include "inspect_js.h"

unsigned int *g_cli_js_pair;
unsigned char *g_cli_js_brace;
unsigned int *g_cli_js_within;

// the open brackets of the reading, a list of the reading's cell
static unsigned int *s_stack;

void cli_inspect_js_lex_layout(const CliInspectPartitionCtx *args)
{
    const unsigned long long tokens = CLI_INSPECT_TOKENS;
    g_cli_js_pair = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                               .partition = args->partition, .bytes = tokens * sizeof(unsigned int));
    g_cli_js_brace = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                                .partition = args->partition, .bytes = tokens);
    g_cli_js_within = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                                 .partition = args->partition, .bytes = tokens * sizeof(unsigned int));
    s_stack = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                         .bytes = tokens * sizeof(unsigned int));
}

const char *const g_cli_js_keywords[35] = {"break",   "case",     "catch", "class",  "const",  "continue",   "debugger",
                                           "default", "delete",   "do",    "else",   "export", "extends",    "finally",
                                           "for",     "function", "if",    "import", "in",     "instanceof", "let",
                                           "new",     "return",   "super", "switch", "this",   "throw",      "try",
                                           "typeof",  "var",      "void",  "while",  "with",   "yield",      "await"};

const char *const g_cli_js_control[7] = {"if", "for", "while", "switch", "catch", "with", "return"};

// the punctuation, the longest that matches first
static const char *const s_puncts[52] = {">>>=", "...", "===", "!==", "**=", "<<=", ">>=", ">>>", "&&=", "||=", "?\?=",
                                         "=>",   "==",  "!=",  "<=",  ">=",  "&&",  "||",  "??",  "?.",  "++",  "--",
                                         "+=",   "-=",  "*=",  "/=",  "%=",  "&=",  "|=",  "^=",  "**",  "<<",  ">>",
                                         "+",    "-",   "*",   "/",   "%",   "&",   "|",   "^",   "!",   "~",   "<",
                                         ">",    "=",   "?",   ":",   ";",   ",",   ".",   "@"};

// the words after which a slash starts a regular expression
static const char *const s_before_value[14] = {"return", "typeof", "instanceof", "in", "of",   "new",   "delete",
                                               "void",   "throw",  "case",       "do", "else", "yield", "await"};

typedef struct
{
    unsigned int letter;
    const char *src;
    CliInspectScan *scan;
    int inside;
    unsigned int at;
} InspectJsLexCtx;
EMBED_INLINE int lex_name_start(const InspectJsLexCtx *args)
{
    unsigned int letter = args->letter;
    return (letter == '_') || (letter == '$') ||
           EMBED_CALL(cli_inspect_internal.alphabetic, CliInspectCtx, .letter = letter);
}

EMBED_INLINE int lex_name(const InspectJsLexCtx *args)
{
    unsigned int letter = args->letter;
    return (letter == '_') || (letter == '$') ||
           EMBED_CALL(cli_inspect_internal.alphanumeric, CliInspectCtx, .letter = letter);
}

int cli_js_is(const CliInspectJsAtWordCfg *args)
{
    const char *src = args->src;
    if (args->at == CLI_JS_NO)
    {
        return 0;
    }
    const CliInspectTok *const tok = &g_cli_inspect_tok[args->at];
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES});
    return (tok->end - tok->start == length) &&
           (memor.cmp(&(MemoriaCfg){.src = src + tok->start, .other = args->word_text, .bytes = length}) == 0);
}

int cli_js_one_get(const CliInspectJsAtCfg *args)
{
    for (unsigned int index = 0u; index < args->count; index += 1u)
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = args->at,
                       .word_text = args->words[index]))
        {
            return 1;
        }
    }
    return 0;
}

// whether a value is expected after the tokens so far, and a slash starts a regular expression
EMBED_INLINE int lex_value_expected(const InspectJsLexCtx *args)
{
    const char *src = args->src;
    if (g_cli_inspect_toks == 0u)
    {
        return 1;
    }
    const unsigned int last = g_cli_inspect_toks - 1u;
    const unsigned char type = g_cli_inspect_tok[last].type;
    if (type == CLI_JS_PUNCT)
    {
        return !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = last,
                           .word_text = ")") &&
               !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = last,
                           .word_text = "]") &&
               !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = last,
                           .word_text = "}");
    }
    return (type == CLI_JS_NAME) && EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = last,
                                               .words = s_before_value, .count = 14u);
}

// a block comment from where the scan stands
EMBED_INLINE void lex_block_comment(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    const CliInspectPosition from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan);
    (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    for (unsigned int letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
         letter != CLI_INSPECT_END;
         letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
    {
        if ((letter == '*') && (EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx,
                                           .scan = scan, .ahead = 1u) == '/'))
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
            break;
        }
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    }
    EMBED_CALL(cli_inspect_from_to_internal.comment_append, CliInspectFromToCtx, .from = from,
               .to = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan));
}

// a regular expression from its slash, its flags with it
EMBED_INLINE void lex_regex(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    const unsigned long long start = scan->pos;
    const CliInspectPosition from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan);
    (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    int in_class = 0;
    for (unsigned int letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
         letter != CLI_INSPECT_END;
         letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        if (letter == '\\')
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        }
        else if (letter == '[')
        {
            in_class = 1;
        }
        else if (letter == ']')
        {
            in_class = 0;
        }
        else if (((letter == '/') && !in_class) || (letter == '\n'))
        {
            break;
        }
    }
    while (EMBED_CALL(lex_name, InspectJsLexCtx,
                      .letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan)))
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    }
    EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_JS_REGEX,
               .start = start, .from = from, .field = 0);
}

// punctuation, the longest that matches, else the one letter
EMBED_INLINE void lex_punct(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    const unsigned long long start = scan->pos;
    const CliInspectPosition from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan);
    embed_word length = 1u;
    for (unsigned int at = 0u; at < 52u; at += 1u)
    {
        const embed_word size = cellul.len(&(CatenaFinitaCfg){.src = s_puncts[at], .cap = CLI_POOL_BYTES});
        if ((scan->size - start >= size) &&
            (memor.cmp(&(MemoriaCfg){.src = scan->src + start, .other = s_puncts[at], .bytes = size}) == 0))
        {
            length = size;
            break;
        }
    }
    for (embed_word at = 0u; at < length; at += 1u)
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    }
    EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_JS_PUNCT,
               .start = start, .from = from, .field = 0);
}

// tokens read until the text ends or, `inside` a template's `${`, until its `}`
static void lex(const InspectJsLexCtx *args);

// the scan moved past a string that its quote `letter` opens, a backslash taking the letter after it, to its closing
// quote or the line's end
EMBED_INLINE void lex_string(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    for (unsigned int one = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
         one != CLI_INSPECT_END;
         one = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        if (one == '\\')
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        }
        else if ((one == args->letter) || (one == '\n'))
        {
            return;
        }
    }
}

// the scan moved past a template that its backquote opens, a backslash taking the letter after it, and each `${`
// lexed as code to the brace that closes it
EMBED_INLINE void lex_template(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    for (unsigned int one = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
         one != CLI_INSPECT_END;
         one = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        if (one == '`')
        {
            return;
        }
        if (one == '\\')
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        }
        else if ((one == '$') &&
                 (EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan) == '{'))
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
            EMBED_CALL(lex, InspectJsLexCtx, .scan = scan, .inside = 1);
        }
    }
}

// the scan moved past a name's letters after its first
EMBED_INLINE void lex_word(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    while (EMBED_CALL(lex_name, InspectJsLexCtx,
                      .letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan)))
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    }
}

// the scan moved past a number's letters, digits, letters, `_` and `.`
EMBED_INLINE void lex_number(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    for (unsigned int one = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
         (one != CLI_INSPECT_END) &&
         (EMBED_CALL(cli_inspect_internal.alphanumeric, CliInspectCtx, .letter = one) || (one == '_') || (one == '.'));
         one = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
    {
        (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
    }
}

// whether a letter is a bracket, round, square or curly, opening or closing
EMBED_INLINE int lex_bracket(const InspectJsLexCtx *args)
{
    const unsigned int letter = args->letter;
    return (letter == '(') || (letter == '[') || (letter == '{') || (letter == ')') || (letter == ']') ||
           (letter == '}');
}

static void lex(const InspectJsLexCtx *args)
{
    CliInspectScan *scan = args->scan;
    int depth = 0;
    for (unsigned int letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan);
         letter != CLI_INSPECT_END;
         letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = scan))
    {
        const unsigned long long start = scan->pos;
        const CliInspectPosition from = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = scan);
        const unsigned int next =
            EMBED_CALL(cli_inspect_scan_ahead_internal.peek_at, CliInspectScanAheadCtx, .scan = scan, .ahead = 1u);
        if ((letter == ' ') || (letter == '\t') || (letter == '\r') || (letter == '\n') || (letter == 0xfeffu) ||
            (letter == 0xa0u))
        {
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
        }
        else if ((letter == '/') && (next == '/'))
        {
            EMBED_CALL(cli_inspect_size_scan_internal.skip_line, CliInspectSizeScanCtx, .scan_out = scan);
        }
        else if ((letter == '/') && (next == '*'))
        {
            EMBED_CALL(lex_block_comment, InspectJsLexCtx, .scan = scan);
        }
        else if ((letter == '\'') || (letter == '"'))
        {
            EMBED_CALL(lex_string, InspectJsLexCtx, .scan = scan, .letter = letter);
            EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_JS_STR,
                       .start = start, .from = from, .field = 0);
        }
        else if (letter == '`')
        {
            EMBED_CALL(lex_template, InspectJsLexCtx, .scan = scan);
            EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_JS_STR,
                       .start = start, .from = from, .field = 0);
        }
        else if ((letter == '/') && EMBED_CALL(lex_value_expected, InspectJsLexCtx, .src = scan->src))
        {
            EMBED_CALL(lex_regex, InspectJsLexCtx, .scan = scan);
        }
        else if (EMBED_CALL(lex_name_start, InspectJsLexCtx, .letter = letter) || (letter == '#'))
        {
            EMBED_CALL(lex_word, InspectJsLexCtx, .scan = scan);
            EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_JS_NAME,
                       .start = start, .from = from, .field = 0);
        }
        else if (((letter >= '0') && (letter <= '9')) || ((letter == '.') && (next >= '0') && (next <= '9')))
        {
            EMBED_CALL(lex_number, InspectJsLexCtx, .scan = scan);
            EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_JS_NUMBER,
                       .start = start, .from = from, .field = 0);
        }
        else if (EMBED_CALL(lex_bracket, InspectJsLexCtx, .letter = letter))
        {
            if (args->inside && (letter == '}') && (depth == 0))
            {
                (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
                return;
            }
            depth += ((letter == '(') || (letter == '[') || (letter == '{')) ? 1 : -1;
            (void)EMBED_CALL(cli_inspect_size_scan_internal.bump, CliInspectSizeScanCtx, .scan_out = scan);
            EMBED_CALL(cli_inspect_scan_internal.tok_append, CliInspectScanCtx, .scan = scan, .type = CLI_JS_PUNCT,
                       .start = start, .from = from, .field = 0);
        }
        else
        {
            EMBED_CALL(lex_punct, InspectJsLexCtx, .scan = scan);
        }
    }
}

// what the brace at `at` opens
EMBED_INLINE unsigned char read_brace(const InspectJsLexCtx *args)
{
    const char *src = args->src;
    unsigned int at = args->at;
    if (at == 0u)
    {
        return CLI_JS_BLOCK;
    }
    const unsigned int before = at - 1u;
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word_text = "=>"))
    {
        return CLI_JS_FUNCTION;
    }
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = before, .word_text = ")") &&
        (g_cli_js_pair[before] != CLI_JS_NO))
    {
        const unsigned int open = g_cli_js_pair[before];
        const unsigned int name = (open > 0u) ? (open - 1u) : CLI_JS_NO;
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = name,
                       .word_text = "function"))
        {
            return CLI_JS_FUNCTION;
        }
        const int named = (name != CLI_JS_NO) && (g_cli_inspect_tok[name].type == CLI_JS_NAME) &&
                          !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = name,
                                      .words = g_cli_js_control, .count = 7u);
        return named ? CLI_JS_FUNCTION : CLI_JS_BLOCK;
    }
    // `class Name {` and `class Name extends Base {`
    for (unsigned int back = at; back > ((at > 8u) ? (at - 8u) : 0u); back -= 1u)
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = back - 1u,
                       .word_text = "class") &&
            (g_cli_inspect_tok[back - 1u].type == CLI_JS_NAME))
        {
            return CLI_JS_CLASS;
        }
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = back - 1u,
                       .word_text = ";") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = back - 1u,
                       .word_text = "{") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = back - 1u,
                       .word_text = "}"))
        {
            break;
        }
    }
    static const char *const s_closers[4] = {")", "]", "}", ";"};
    static const char *const s_values[7] = {"return", "yield", "await", "case", "default", "in", "of"};
    const int punct = (g_cli_inspect_tok[before].type == CLI_JS_PUNCT) &&
                      !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = before,
                                  .words = s_closers, .count = 4u);
    return (punct || EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = before,
                                .words = s_values, .count = 7u))
               ? CLI_JS_OBJECT
               : CLI_JS_BLOCK;
}

void cli_js_read(const CliInspectJsCfg *args)
{
    const char *src = args->src;
    unsigned long long size = args->size;
    CliInspectScan scan;
    EMBED_CALL(cli_inspect_size_scan_internal.scan_begin, CliInspectSizeScanCtx, .scan_out = &scan, .src = src,
               .size = size);
    g_cli_inspect_toks = 0u;
    g_cli_inspect_comments = 0u;
    g_cli_inspect_over = 0;
    EMBED_CALL(lex, InspectJsLexCtx, .scan = &scan, .inside = 0);
    unsigned int stack = 0u;
    unsigned int brace = CLI_JS_NO;
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        g_cli_js_pair[at] = CLI_JS_NO;
        g_cli_js_brace[at] = CLI_JS_NONE;
        brace = CLI_JS_NO;
        for (unsigned int open = stack; open > 0u; open -= 1u)
        {
            if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = s_stack[open - 1u],
                           .word_text = "{"))
            {
                brace = s_stack[open - 1u];
                break;
            }
        }
        g_cli_js_within[at] = brace;
        if (g_cli_inspect_tok[at].type != CLI_JS_PUNCT)
        {
            continue;
        }
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word_text = "(") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word_text = "[") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word_text = "{"))
        {
            s_stack[stack] = at;
            stack += 1u;
        }
        else if ((EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at,
                             .word_text = ")") ||
                  EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at,
                             .word_text = "]") ||
                  EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at,
                             .word_text = "}")) &&
                 (stack > 0u))
        {
            stack -= 1u;
            g_cli_js_pair[s_stack[stack]] = at;
            g_cli_js_pair[at] = s_stack[stack];
        }
    }
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word_text = "{") &&
            (g_cli_inspect_tok[at].type == CLI_JS_PUNCT))
        {
            g_cli_js_brace[at] = EMBED_CALL(read_brace, InspectJsLexCtx, .src = src, .at = at);
        }
    }
}

int cli_js_is_read(const CliInspectJsAtWordCfg *args)
{
    const CliInspectTok *const tok = &g_cli_inspect_tok[args->at];
    if ((tok->type != CLI_JS_NAME) ||
        EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = args->src, .at = args->at,
                   .words = g_cli_js_keywords, .count = 35u) ||
        (args->src[tok->start] == '#'))
    {
        return 0;
    }
    const unsigned int before = (args->at > 0u) ? (args->at - 1u) : CLI_JS_NO;
    if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before,
                   .word_text = ".") ||
        EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before,
                   .word_text = "?."))
    {
        return 0;
    }
    const unsigned int after = (args->at + 1u < g_cli_inspect_toks) ? (args->at + 1u) : CLI_JS_NO;
    const unsigned int open = g_cli_js_within[args->at];
    const unsigned char holder = (open != CLI_JS_NO) ? g_cli_js_brace[open] : CLI_JS_NONE;
    if ((holder == CLI_JS_OBJECT) &&
        (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before,
                    .word_text = "{") ||
         EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = before,
                    .word_text = ",")) &&
        (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = after,
                    .word_text = ":") ||
         EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = after,
                    .word_text = "(")))
    {
        return 0;
    }
    static const char *const s_members[8] = {"{", "}", ";", "static", "async", "get", "set", "*"};
    return !((holder == CLI_JS_CLASS) &&
             EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = after,
                        .word_text = "(") &&
             EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = args->src, .at = before,
                        .words = s_members, .count = 8u));
}

int cli_js_pair_depth(const CliInspectJsFromCfg *args)
{
    int depth = 0;
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        depth += (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                             .word_text = "(") ||
                  EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                             .word_text = "[") ||
                  EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                             .word_text = "{"))
                     ? 1
                     : 0;
        depth -= (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                             .word_text = ")") ||
                  EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                             .word_text = "]") ||
                  EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                             .word_text = "}"))
                     ? 1
                     : 0;
    }
    return depth;
}
