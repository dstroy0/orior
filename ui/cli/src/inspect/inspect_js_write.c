// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_js_write.c: a JSDoc comment drawn up for the JavaScript function at or around a line, its parameters, what
// it returns where it returns a value, and what it throws; and the class around a line with its methods sorted by
// name
#include "../text/json_tree.h"
#include "../text/text.h"
#include "inspect_js.h"

// the most parameters and thrown names a JSDoc comment names, and the most members a class sorted contains
#define CLI_JS_NAMED 256u
#define CLI_JS_MEMBERS (CLI_INSPECT_TOKENS / 4u)

static CliInspectSet s_names;
static unsigned int s_params[CLI_JS_NAMED];
static const char *s_thrown[CLI_JS_NAMED];
static unsigned int s_thrown_size[CLI_JS_NAMED];
static unsigned long long *s_starts;
static unsigned long long *s_ends;
static unsigned int *s_firsts;
static unsigned int *s_lasts;
static const char **s_member_names;
static unsigned int *s_member_sizes;

void cli_inspect_js_write_layout(const CliInspectPartitionCtx *args)
{
    EMBED_CALL(cli_inspect_partition_set_internal.set_layout, CliInspectPartitionSetCtx, .set = &s_names,
               .partition = args->partition);
    const unsigned long long lines = CLI_INSPECT_LINES;
    const unsigned long long members = CLI_JS_MEMBERS;
    s_starts = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                          .partition = args->partition, .bytes = lines * sizeof(unsigned long long));
    s_ends = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                        .bytes = lines * sizeof(unsigned long long));
    s_firsts = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                          .partition = args->partition, .bytes = members * sizeof(unsigned int));
    s_lasts = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                         .bytes = members * sizeof(unsigned int));
    s_member_names = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                                .partition = args->partition, .bytes = members * sizeof(const char *));
    s_member_sizes = EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx,
                                .partition = args->partition, .bytes = members * sizeof(unsigned int));
}

typedef struct
{
    const char *src;
    unsigned int at;
    unsigned int *size;
    unsigned long long size_u64;
    unsigned int line;
    unsigned long long *start;
    unsigned long long *end;
    unsigned int start_u32;
    unsigned int close;
    const char **name;
    unsigned int *name_size;
    unsigned int count;
    unsigned long long from;
    unsigned long long to;
    unsigned int floor;
} InspectJsWriteCtx;
// the text of the token at `at`, `size` bytes of it
EMBED_INLINE const char *write_word(const InspectJsWriteCtx *args)
{
    unsigned int *size = args->size;
    *size = g_cli_inspect_tok[args->at].end - g_cli_inspect_tok[args->at].start;
    return args->src + g_cli_inspect_tok[args->at].start;
}

// the white letters a physical line starts with, its start and their end, into `start` and `end`
EMBED_INLINE void write_indent(const InspectJsWriteCtx *args)
{
    unsigned long long size = args->size_u64;
    unsigned long long *start = args->start;
    unsigned long long *end = args->end;
    unsigned long long at = 0ULL;
    for (unsigned int index = 0u; (index < args->line) && (at < size); index += 1u)
    {
        at = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->src, .at = at, .size = size).next;
    }
    *start = at;
    while (at < size)
    {
        unsigned int letter = 0u;
        const unsigned long long reserved =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)args->src, .length = size, .at = at, .letter = &letter);
        if ((letter == '\n') || !EMBED_CALL(cli_inspect_internal.white, CliInspectCtx, .letter = letter))
        {
            break;
        }
        at += reserved;
    }
    *end = at;
}

// the function at or around `line`: its body's opening brace, CLI_JS_NO where none is
EMBED_INLINE unsigned int write_function_at(const InspectJsWriteCtx *args)
{
    const char *src = args->src;
    unsigned int found = CLI_JS_NO;
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        if ((g_cli_js_brace[at] != CLI_JS_FUNCTION) ||
            (EMBED_CALL(cli_inspect_js_open.js_function_name, CliInspectJsOpenCfg, .src = src, .open = at) ==
             CLI_JS_NO))
        {
            continue;
        }
        const unsigned int close = g_cli_js_pair[at];
        const int encloses = (g_cli_inspect_tok[at].from.line <= args->line) && (close != CLI_JS_NO) &&
                             (g_cli_inspect_tok[close].to.line >= args->line);
        const int named_on = (g_cli_inspect_tok[EMBED_CALL(cli_inspect_js_open.js_function_name, CliInspectJsOpenCfg,
                                                           .src = src, .open = at)]
                                  .from.line == args->line);
        found = (encloses || named_on) ? at : found;
    }
    return found;
}

// the first token of the statement on the line of the token `start_u32`: the token after the `;`, `{` or `}` before
// it on that line, or the line's first
EMBED_INLINE unsigned int write_lead(const InspectJsWriteCtx *args)
{
    const char *const src = args->src;
    const unsigned int start = args->start_u32;
    unsigned int lead = start;
    while ((lead > 0u) &&
           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = lead - 1u,
                       .word_text = ";") &&
           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = lead - 1u,
                       .word_text = "{") &&
           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = lead - 1u,
                       .word_text = "}") &&
           (g_cli_inspect_tok[lead - 1u].from.line == g_cli_inspect_tok[start].from.line))
    {
        lead -= 1u;
    }
    return lead;
}

// whether a comment ends on the line before `line` and that line ends in `*/`
EMBED_INLINE int write_commented(const InspectJsWriteCtx *args)
{
    const char *const src = args->src;
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        const unsigned int to_line = g_cli_inspect_comment[at].to.line;
        unsigned long long line_start = 0ULL;
        unsigned long long line_end = 0ULL;
        EMBED_CALL(write_indent, InspectJsWriteCtx, .src = src, .size_u64 = args->size_u64, .line = to_line,
                   .start = &line_start, .end = &line_end);
        unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = src,
                                            .at = line_start, .size = args->size_u64, .set = "\n");
        while ((end > line_start) && (cellul.chr(&(CatenaFinitaCfg){
                                          .src = " \t\r\v\f", .cap = CLI_POOL_BYTES, .byte = src[end - 1u]}) != NULL))
        {
            end -= 1u;
        }
        const int closed = (end - line_start >= 2u) &&
                           (memor.cmp(&(MemoriaCfg){.src = src + end - 2u, .other = "*/", .bytes = 2u}) == 0);
        if ((to_line + 1u == args->line) && closed)
        {
            return 1;
        }
    }
    return 0;
}

// the names a function binds from the token `start_u32` to its body's opening brace `close`, each once, into
// s_params: their count
EMBED_INLINE unsigned int write_params_found(const InspectJsWriteCtx *args)
{
    const char *const src = args->src;
    unsigned int params = 0u;
    for (unsigned int at = args->start_u32; (at < args->close) && (params < CLI_JS_NAMED); at += 1u)
    {
        unsigned int length = 0u;
        const char *const word_text = EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = at, .size = &length);
        int known = !EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_names, .at = word_text,
                                .size_u32 = length) ||
                    (g_cli_inspect_tok[at].type != CLI_JS_NAME);
        unsigned int index = 0u;
        while (!known && (index < params))
        {
            unsigned int other = 0u;
            const char *const before =
                EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = s_params[index], .size = &other);
            known = (other == length) &&
                    (memor.cmp(&(MemoriaCfg){.src = before, .other = word_text, .bytes = length}) == 0);
            index += 1u;
        }
        if (!known)
        {
            s_params[params] = at;
            params += 1u;
        }
    }
    return params;
}

// the token after `at` in a body: past the closing brace of a function's body `at` opens
EMBED_INLINE unsigned int write_body_next(const InspectJsWriteCtx *args)
{
    const unsigned int pair = g_cli_js_pair[args->at];
    return ((g_cli_js_brace[args->at] == CLI_JS_FUNCTION) && (pair != CLI_JS_NO)) ? (pair + 1u) : (args->at + 1u);
}

// whether the body between the braces `start_u32` and `close` returns a value, the functions inside it passed over
EMBED_INLINE int write_returns(const InspectJsWriteCtx *args)
{
    const char *const src = args->src;
    for (unsigned int at = args->start_u32 + 1u; at < args->close;
         at = EMBED_CALL(write_body_next, InspectJsWriteCtx, .at = at))
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at,
                       .word_text = "return") &&
            !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u,
                        .word_text = ";") &&
            !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u,
                        .word_text = "}"))
        {
            return 1;
        }
    }
    return 0;
}

// the name the `throw` at `at` throws, the class after its `new` or else Error, put at `count` of s_thrown where none
// of the `count` before it is the same: 1 where it is put, else 0
EMBED_INLINE unsigned int write_throw_put(const InspectJsWriteCtx *args)
{
    const unsigned int at = args->at;
    const int made = EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at + 1u,
                                .word_text = "new") &&
                     (at + 2u < g_cli_inspect_toks);
    unsigned int length = 5u;
    const char *const name =
        made ? EMBED_CALL(write_word, InspectJsWriteCtx, .src = args->src, .at = at + 2u, .size = &length) : "Error";
    for (unsigned int index = 0u; index < args->count; index += 1u)
    {
        if ((s_thrown_size[index] == length) &&
            (memor.cmp(&(MemoriaCfg){.src = s_thrown[index], .other = name, .bytes = length}) == 0))
        {
            return 0u;
        }
    }
    s_thrown[args->count] = name;
    s_thrown_size[args->count] = length;
    return 1u;
}

// the names thrown in the body between the braces `start_u32` and `close`, the functions inside it passed over, each
// once, into s_thrown and s_thrown_size: their count
EMBED_INLINE unsigned int write_throws_found(const InspectJsWriteCtx *args)
{
    unsigned int thrown = 0u;
    for (unsigned int at = args->start_u32 + 1u; at < args->close;
         at = EMBED_CALL(write_body_next, InspectJsWriteCtx, .at = at))
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                       .word_text = "throw") &&
            (thrown < CLI_JS_NAMED))
        {
            thrown += EMBED_CALL(write_throw_put, InspectJsWriteCtx, .src = args->src, .at = at, .count = thrown);
        }
    }
    return thrown;
}

// the UTF-16 units the letters from `from` to `to` of `src` take, a letter past U+FFFF taking two
EMBED_INLINE unsigned int write_units(const InspectJsWriteCtx *args)
{
    unsigned int units = 0u;
    for (unsigned long long letter_at = args->from; letter_at < args->to;)
    {
        unsigned int letter = 0u;
        letter_at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                .text_bytes = (const unsigned char *)args->src, .length = args->to, .at = letter_at,
                                .letter = &letter);
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    return units;
}

int cli_inspect_js_docstring(const CliInspectSizeSrcCtx *args)
{
    const char *src = args->src;
    CliInspectEdit *edit = args->edit;
    CliInspectPosition *summary = args->summary;
    EMBED_CALL(cli_inspect_js.js_read, CliInspectJsCfg, .src = src, .size = args->size);
    const unsigned int open = EMBED_CALL(write_function_at, InspectJsWriteCtx, .src = src, .line = args->line);
    if (open == CLI_JS_NO)
    {
        mmgr_line_put(args->err_msg, "Write Docstring writes one for the function the cursor is in.");
        return 0;
    }
    const unsigned int close = (g_cli_js_pair[open] != CLI_JS_NO) ? g_cli_js_pair[open] : open;
    EMBED_CALL(cli_inspect_partition_set_internal.set_clear, CliInspectPartitionSetCtx, .set = &s_names);
    const unsigned int first =
        EMBED_CALL(cli_inspect_js_open.js_parameters, CliInspectJsOpenCfg, .src = src, .open = open, .names = &s_names);
    const unsigned int named =
        EMBED_CALL(cli_inspect_js_open.js_function_name, CliInspectJsOpenCfg, .src = src, .open = open);
    const unsigned int name_at = (named != CLI_JS_NO) ? named : first;
    const unsigned int start = (name_at < first) ? name_at : first;
    const unsigned int first_line =
        g_cli_inspect_tok[EMBED_CALL(write_lead, InspectJsWriteCtx, .src = src, .start_u32 = start)].from.line;
    if (EMBED_CALL(write_commented, InspectJsWriteCtx, .src = src, .size_u64 = args->size, .line = first_line))
    {
        mmgr_line_put(args->err_msg, "The function has a comment over it.");
        return 0;
    }
    const unsigned int params =
        EMBED_CALL(write_params_found, InspectJsWriteCtx, .src = src, .start_u32 = first, .close = open);
    const int releases = EMBED_CALL(write_returns, InspectJsWriteCtx, .src = src, .start_u32 = open, .close = close);
    const unsigned int thrown =
        EMBED_CALL(write_throws_found, InspectJsWriteCtx, .src = src, .start_u32 = open, .close = close);
    unsigned long long indent_start = 0ULL;
    unsigned long long indent_end = 0ULL;
    EMBED_CALL(write_indent, InspectJsWriteCtx, .src = src, .size_u64 = args->size, .line = first_line,
               .start = &indent_start, .end = &indent_end);
    const char *const indent = src + indent_start;
    const unsigned long long indent_size = indent_end - indent_start;
    mmgr_line_put_n(args->text, indent, indent_size);
    mmgr_line_put(args->text, "/**\n");
    mmgr_line_put_n(args->text, indent, indent_size);
    mmgr_line_put(args->text, " * \n");
    if ((params > 0u) || releases || (thrown > 0u))
    {
        mmgr_line_put_n(args->text, indent, indent_size);
        mmgr_line_put(args->text, " *\n");
    }
    for (unsigned int index = 0u; index < params; index += 1u)
    {
        unsigned int length = 0u;
        const char *const word_text =
            EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = s_params[index], .size = &length);
        mmgr_line_put_n(args->text, indent, indent_size);
        mmgr_line_put(args->text, " * @param ");
        mmgr_line_put_n(args->text, word_text, length);
        mmgr_line_ch(args->text, '\n');
    }
    if (releases)
    {
        mmgr_line_put_n(args->text, indent, indent_size);
        mmgr_line_put(args->text, " * @returns\n");
    }
    for (unsigned int index = 0u; index < thrown; index += 1u)
    {
        mmgr_line_put_n(args->text, indent, indent_size);
        mmgr_line_put(args->text, " * @throws {");
        mmgr_line_put_n(args->text, s_thrown[index], s_thrown_size[index]);
        mmgr_line_put(args->text, "}\n");
    }
    mmgr_line_put_n(args->text, indent, indent_size);
    mmgr_line_put(args->text, " */\n");
    const unsigned int units =
        EMBED_CALL(write_units, InspectJsWriteCtx, .src = src, .from = indent_start, .to = indent_end);
    const CliInspectPosition place = {first_line, 0u};
    *edit = (CliInspectEdit){place, place, {0u, 0u}};
    *summary = (CliInspectPosition){first_line + 1u, units + 3u};
    return 1;
}

// the end of the class member that starts at `start` before `close`, the token it ends on, and its name where it is a
// method, into `name` and `name_size`
EMBED_INLINE unsigned int write_member_end(const InspectJsWriteCtx *args)
{
    const char *src = args->src;
    const char **name = args->name;
    unsigned int *name_size = args->name_size;
    static const char *const s_runs_on[4] = {"(", "{", "=", "."};
    unsigned int end = args->start_u32;
    *name = NULL;
    *name_size = 0u;
    int going = 1;
    while (going && (end < args->close))
    {
        if (g_cli_js_brace[end] == CLI_JS_FUNCTION)
        {
            const unsigned int named =
                EMBED_CALL(cli_inspect_js_open.js_function_name, CliInspectJsOpenCfg, .src = src, .open = end);
            if (named != CLI_JS_NO)
            {
                *name = EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = named, .size = name_size);
            }
            end = (g_cli_js_pair[end] != CLI_JS_NO) ? g_cli_js_pair[end] : end;
            going = 0;
            continue;
        }
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word_text = ";"))
        {
            going = 0;
            continue;
        }
        const unsigned int pair = g_cli_js_pair[end];
        const int opens =
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word_text = "(") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word_text = "[");
        end = ((pair != CLI_JS_NO) && (pair > end) && opens) ? (pair + 1u) : (end + 1u);
        end = (end < args->close) ? end : args->close;
        const int broken = (end < args->close) &&
                           (g_cli_inspect_tok[end].from.line > g_cli_inspect_tok[end - 1u].to.line) &&
                           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end - 1u,
                                       .word_text = ",") &&
                           !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = end,
                                       .words = s_runs_on, .count = 4u) &&
                           (g_cli_js_brace[end] != CLI_JS_FUNCTION);
        if (broken)
        {
            end -= 1u;
            going = 0;
            continue;
        }
    }
    return (end < args->close - 1u) ? end : (args->close - 1u);
}

// the first line of a member starting on `line`, the comments standing on the lines right above it taken in where one
// starts on `floor` or after it
EMBED_INLINE unsigned int write_member_first(const InspectJsWriteCtx *args)
{
    const unsigned int floor = args->floor;
    unsigned int first = args->line;
    for (int moved = 1; moved && (first > floor);)
    {
        moved = 0;
        unsigned int min = first - 1u;
        int any = 0;
        for (unsigned int comment = 0u; comment < g_cli_inspect_comments; comment += 1u)
        {
            const CliInspectSpan span = g_cli_inspect_comment[comment];
            if (span.to.line != first - 1u)
            {
                continue;
            }
            moved = moved || (span.from.line >= floor);
            min = (!any || (span.from.line < min)) ? span.from.line : min;
            any = 1;
        }
        first = moved ? min : first;
    }
    return first;
}

int cli_inspect_js_sort_methods(const CliInspectSizeSrcCtx *args)
{
    CliInspectEdit *edit = args->edit;
    EMBED_CALL(cli_inspect_js.js_read, CliInspectJsCfg, .src = args->src, .size = args->size);
    unsigned int open = CLI_JS_NO;
    for (unsigned int at = 0u; at < g_cli_inspect_toks; at += 1u)
    {
        const unsigned int close = g_cli_js_pair[at];
        const int around = (g_cli_js_brace[at] == CLI_JS_CLASS) && (g_cli_inspect_tok[at].from.line <= args->line) &&
                           (close != CLI_JS_NO) && (g_cli_inspect_tok[close].to.line >= args->line);
        open = around ? at : open;
    }
    if (open == CLI_JS_NO)
    {
        mmgr_line_put(args->err_msg, "Sort Methods by Name sorts the class the cursor is in.");
        return 0;
    }
    const unsigned int close = g_cli_js_pair[open];
    const unsigned int physical =
        EMBED_CALL(cli_inspect_size_src_starts_internal.physical, CliInspectSizeSrcStartsCtx, .src = args->src,
                   .size = args->size, .starts = s_starts, .ends = s_ends, .most = CLI_INSPECT_LINES);
    unsigned int members = 0u;
    unsigned int at = open + 1u;
    unsigned int floor = g_cli_inspect_tok[open].to.line + 1u;
    while ((at < close) && (members < CLI_JS_MEMBERS))
    {
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at,
                       .word_text = ";"))
        {
            at += 1u;
            continue;
        }
        const unsigned int start = at;
        const unsigned int end =
            EMBED_CALL(write_member_end, InspectJsWriteCtx, .src = args->src, .start_u32 = start, .close = close,
                       .name = &s_member_names[members], .name_size = &s_member_sizes[members]);
        s_firsts[members] = EMBED_CALL(write_member_first, InspectJsWriteCtx,
                                       .line = g_cli_inspect_tok[start].from.line, .floor = floor);
        s_lasts[members] = g_cli_inspect_tok[end].to.line;
        floor = s_lasts[members] + 1u;
        members += 1u;
        at = end + 1u;
    }
    if (members == 0u)
    {
        mmgr_line_put(args->err_msg, "The class has no members.");
        return 0;
    }
    const unsigned int first = s_firsts[0];
    const unsigned int last = s_lasts[members - 1u];
    if ((last >= physical) ||
        !EMBED_CALL(cli_inspect_size_internal.sorted_body, CliInspectSizeCtx, .src = args->src, .size = args->size,
                    .line_starts = s_starts, .line_ends = s_ends, .firsts = s_firsts, .lasts = s_lasts,
                    .names = s_member_names, .name_sizes = s_member_sizes, .members = members, .out = args->text))
    {
        mmgr_line_put(args->err_msg, "The class's methods stand in the order of their names.");
        return 0;
    }
    const unsigned int end_col =
        EMBED_CALL(write_units, InspectJsWriteCtx, .src = args->src, .from = s_starts[last], .to = s_ends[last]);
    *edit = (CliInspectEdit){{first, 0u}, {last, end_col}, {0u, 0u}};
    return 1;
}
