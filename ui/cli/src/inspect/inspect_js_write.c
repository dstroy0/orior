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
    unsigned long long at = 0ull;
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
    unsigned int lead = start;
    while ((lead > 0u) &&
           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = lead - 1u, .word = ";") &&
           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = lead - 1u, .word = "{") &&
           !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = lead - 1u, .word = "}") &&
           (g_cli_inspect_tok[lead - 1u].from.line == g_cli_inspect_tok[start].from.line))
    {
        lead -= 1u;
    }
    const unsigned int first_line = g_cli_inspect_tok[lead].from.line;
    for (unsigned int at = 0u; at < g_cli_inspect_comments; at += 1u)
    {
        const unsigned int to_line = g_cli_inspect_comment[at].to.line;
        unsigned long long line_start = 0ull;
        unsigned long long line_end = 0ull;
        EMBED_CALL(write_indent, InspectJsWriteCtx, .src = src, .size_u64 = args->size, .line = to_line,
                   .start = &line_start, .end = &line_end);
        unsigned long long end = line_start;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = src, .at = end,
                         .size = args->size, .set = "\n");
        while ((end > line_start) && (cellul.chr(&(CatenaFinitaCfg){
                                          .src = " \t\r\v\f", .cap = CLI_CELLS_POOL, .byte = src[end - 1u]}) != NULL))
        {
            end -= 1u;
        }
        const int closed = (end - line_start >= 2u) &&
                           (memor.cmp(&(MemoriaCfg){.src = src + end - 2u, .other = "*/", .bytes = 2u}) == 0);
        if ((to_line + 1u == first_line) && closed)
        {
            mmgr_line_put(args->err_msg, "The function has a comment over it.");
            return 0;
        }
    }
    unsigned int params = 0u;
    for (unsigned int at = first; (at < open) && (params < CLI_JS_NAMED); at += 1u)
    {
        unsigned int length = 0u;
        const char *const word = EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = at, .size = &length);
        int known = !EMBED_CALL(cli_inspect_at_internal.set_has, CliInspectAtCtx, .set_read = &s_names, .at = word,
                                .size_u32 = length) ||
                    (g_cli_inspect_tok[at].kind != CLI_JS_NAME);
        for (unsigned int index = 0u; !known && (index < params); index += 1u)
        {
            unsigned int other = 0u;
            const char *const before =
                EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = s_params[index], .size = &other);
            known = (other == length) && (memor.cmp(&(MemoriaCfg){.src = before, .other = word, .bytes = length}) == 0);
        }
        if (!known)
        {
            s_params[params] = at;
            params += 1u;
        }
    }
    int releases = 0;
    unsigned int thrown = 0u;
    unsigned int at = open + 1u;
    while (at < close)
    {
        if (g_cli_js_brace[at] == CLI_JS_FUNCTION)
        {
            at = (g_cli_js_pair[at] != CLI_JS_NO) ? (g_cli_js_pair[at] + 1u) : (at + 1u);
            continue;
        }
        releases =
            releases ||
            (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "return") &&
             !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u, .word = ";") &&
             !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u, .word = "}"));
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at, .word = "throw") &&
            (thrown < CLI_JS_NAMED))
        {
            const int made = EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = at + 1u,
                                        .word = "new") &&
                             (at + 2u < g_cli_inspect_toks);
            unsigned int length = 5u;
            const char *const name =
                made ? EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = at + 2u, .size = &length) : "Error";
            int known = 0;
            for (unsigned int index = 0u; !known && (index < thrown); index += 1u)
            {
                known = (s_thrown_size[index] == length) &&
                        (memor.cmp(&(MemoriaCfg){.src = s_thrown[index], .other = name, .bytes = length}) == 0);
            }
            if (!known)
            {
                s_thrown[thrown] = name;
                s_thrown_size[thrown] = length;
                thrown += 1u;
            }
        }
        at += 1u;
    }
    unsigned long long indent_start = 0ull;
    unsigned long long indent_end = 0ull;
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
        const char *const word =
            EMBED_CALL(write_word, InspectJsWriteCtx, .src = src, .at = s_params[index], .size = &length);
        mmgr_line_put_n(args->text, indent, indent_size);
        mmgr_line_put(args->text, " * @param ");
        mmgr_line_put_n(args->text, word, length);
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
    unsigned int units = 0u;
    for (unsigned long long letter_at = indent_start; letter_at < indent_end;)
    {
        unsigned int letter = 0u;
        letter_at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                .text_bytes = (const unsigned char *)src, .length = indent_end, .at = letter_at,
                                .letter = &letter);
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
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
    while (end < args->close)
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
            break;
        }
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word = ";"))
        {
            break;
        }
        const unsigned int pair = g_cli_js_pair[end];
        const int opens =
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word = "(") ||
            EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end, .word = "[");
        end = ((pair != CLI_JS_NO) && (pair > end) && opens) ? (pair + 1u) : (end + 1u);
        end = (end < args->close) ? end : args->close;
        const int broken =
            (end < args->close) && (g_cli_inspect_tok[end].from.line > g_cli_inspect_tok[end - 1u].to.line) &&
            !EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = src, .at = end - 1u, .word = ",") &&
            !EMBED_CALL(cli_inspect_js_at.js_one_get, CliInspectJsAtCfg, .src = src, .at = end, .words = s_runs_on,
                        .count = 4u) &&
            (g_cli_js_brace[end] != CLI_JS_FUNCTION);
        if (broken)
        {
            end -= 1u;
            break;
        }
    }
    return (end < args->close - 1u) ? end : (args->close - 1u);
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
        if (EMBED_CALL(cli_inspect_js_at_word.js_is, CliInspectJsAtWordCfg, .src = args->src, .at = at, .word = ";"))
        {
            at += 1u;
            continue;
        }
        const unsigned int start = at;
        const unsigned int end =
            EMBED_CALL(write_member_end, InspectJsWriteCtx, .src = args->src, .start_u32 = start, .close = close,
                       .name = &s_member_names[members], .name_size = &s_member_sizes[members]);
        unsigned int first = g_cli_inspect_tok[start].from.line;
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
        s_firsts[members] = first;
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
    unsigned int end_col = 0u;
    for (unsigned long long letter_at = s_starts[last]; letter_at < s_ends[last];)
    {
        unsigned int letter = 0u;
        letter_at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                .text_bytes = (const unsigned char *)args->src, .length = s_ends[last], .at = letter_at,
                                .letter = &letter);
        end_col += (letter >= 0x10000u) ? 2u : 1u;
    }
    *edit = (CliInspectEdit){{first, 0u}, {last, end_col}, {0u, 0u}};
    return 1;
}
