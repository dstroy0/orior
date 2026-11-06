// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_py_write.c: a docstring drawn up for the Python function at or around a line, in Google's, NumPy's or
// reStructuredText's form or a summary line alone, from what the function declares of itself: its parameters with their
// annotations, what it gives back, whether it yields, and what it raises
#include "../text/json_tree.h"
#include "../text/text.h"
#include "inspect_py.h"

// the most parameters and raised names a function's docstring names
#define CLI_PY_PARAMS 256u

// What a function declares of itself for its docstring: its parameters, each its name with its stars and its
// annotation where it has one, what it gives back where its header names it, whether it gives anything, whether it
// yields, and what it raises, each a part of the source
typedef struct
{
    char param[CLI_PY_PARAMS][CLI_NAME_MAX_LEN];
    unsigned long long annotation_start[CLI_PY_PARAMS];
    unsigned long long annotation_end[CLI_PY_PARAMS];
    int annotated[CLI_PY_PARAMS];
    unsigned int params;
    unsigned long long returns_start;
    unsigned long long returns_end;
    int has_returns;
    int releases;
    int yields;
    char raised[CLI_PY_PARAMS][CLI_NAME_MAX_LEN];
    unsigned int raises;
} PySignature;

static PySignature s_sign;

typedef struct
{
    const char *src;
    unsigned int line;
    const char *head;
    const CliPyLine *header;
    unsigned int first;
    unsigned int end;
    int index;
    unsigned int at;
    const char *text;
    const char *form;
    const char *title;
    unsigned long long start;
    unsigned long long end_u64;
} InspectPyWriteCtx;
// the function whose header or block contains `line`, the innermost: its line's index, or g_cli_py_lines for none
EMBED_INLINE unsigned int write_function_around(const InspectPyWriteCtx *args)
{
    const char *src = args->src;
    unsigned int line = args->line;
    unsigned int found = g_cli_py_lines;
    for (unsigned int at = 0u; at < g_cli_py_lines; at += 1u)
    {
        const CliPyLine *const one = &g_cli_py_line[at];
        if (!EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src, .line = one,
                        .word_text = args->head) ||
            !EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = src, .line = one) ||
            (EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = one, .at = 0u)->from.line > line))
        {
            continue;
        }
        const CliPyLine *const last =
            &g_cli_py_line[EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at) - 1u];
        if (line <= EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = last, .at = last->count - 1u)->to.line)
        {
            found = at;
        }
    }
    return found;
}

// one parameter of a header, its tokens from `first` to `end` among the line's, recorded in the signature
static void write_parameter(const InspectPyWriteCtx *args)
{
    const char *src = args->src;
    char stars[8];
    VerbaLinea starred = {stars, sizeof(stars), 0ULL};
    unsigned int at = args->first;
    while ((at < args->end) &&
           (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = at),
                       .word_text = "*") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = at),
                       .word_text = "**")))
    {
        const CliInspectTok *const star =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = at);
        mmgr_line_put_n(&starred, src + star->start, star->end - star->start);
        at += 1u;
    }
    unsigned int name_at = args->end;
    for (unsigned int part = args->first; (name_at == args->end) && (part < args->end); part += 1u)
    {
        name_at =
            (EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = part)->kind == CLI_PY_NAME)
                ? part
                : name_at;
    }
    if ((name_at == args->end) || (s_sign.params == CLI_PY_PARAMS))
    {
        return;
    }
    const CliInspectTok *const name =
        EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = name_at);
    if ((args->index == 0) && (starred.at == 0u) &&
        (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = name,
                    .word_text = "self") ||
         EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = name, .word_text = "cls")))
    {
        return;
    }
    VerbaLinea param = {s_sign.param[s_sign.params], CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put_n(&param, stars, starred.at);
    mmgr_line_put_n(&param, src + name->start, name->end - name->start);
    mmgr_line_finish(&param);
    unsigned int colon = args->end;
    unsigned int equal = args->end;
    for (unsigned int part = args->first; part < args->end; part += 1u)
    {
        colon = ((colon == args->end) &&
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                            .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = part),
                            .word_text = ":"))
                    ? part
                    : colon;
        equal = ((equal == args->end) &&
                 EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                            .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = part),
                            .word_text = "="))
                    ? part
                    : equal;
    }
    const unsigned int stop = (equal < args->end) ? equal : args->end;
    s_sign.annotated[s_sign.params] = (colon < args->end) && (colon + 1u < stop);
    if (s_sign.annotated[s_sign.params])
    {
        s_sign.annotation_start[s_sign.params] =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = colon + 1u)->start;
        s_sign.annotation_end[s_sign.params] =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = stop - 1u)->end;
    }
    s_sign.params += 1u;
}

// the parameters and what a function's header declares it gives back
EMBED_INLINE void write_header(const InspectPyWriteCtx *args)
{
    const char *src = args->src;
    unsigned int open = 0u;
    while ((open < args->header->count) &&
           !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = open),
                       .word_text = "("))
    {
        open += 1u;
    }
    open = (open == args->header->count) ? 0u : open;
    int depth = 0;
    unsigned int close = open;
    unsigned int part_first = open + 1u;
    int index = 0;
    for (unsigned int offset = open; offset < args->header->count; offset += 1u)
    {
        const CliInspectTok *const tok =
            EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = offset);
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "(") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "[") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "{"))
        {
            depth += 1;
            part_first = (depth == 1) ? (offset + 1u) : part_first;
            continue;
        }
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = ")") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "]") ||
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = "}"))
        {
            depth -= 1;
            if (depth != 0)
            {
                continue;
            }
            close = offset;
            if (offset > part_first)
            {
                EMBED_CALL(write_parameter, InspectPyWriteCtx, .src = src, .header = args->header, .first = part_first,
                           .end = offset, .index = index);
            }
            break;
        }
        if ((depth == 1) &&
            EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = tok, .word_text = ","))
        {
            if (offset > part_first)
            {
                EMBED_CALL(write_parameter, InspectPyWriteCtx, .src = src, .header = args->header, .first = part_first,
                           .end = offset, .index = index);
                index += 1;
            }
            part_first = offset + 1u;
        }
    }
    for (unsigned int at = close; at < args->header->count; at += 1u)
    {
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                       .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = at),
                       .word_text = "->"))
        {
            const unsigned int end = args->header->count - 1u;
            if (at + 1u < end)
            {
                s_sign.has_returns = 1;
                s_sign.returns_start =
                    EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = at + 1u)->start;
                s_sign.returns_end =
                    EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = args->header, .at = end - 1u)->end;
            }
            break;
        }
    }
}

// what the function whose header is the line at `at` declares of itself, into s_sign
EMBED_INLINE void write_signature(const InspectPyWriteCtx *args)
{
    const char *src = args->src;
    unsigned int at = args->at;
    memor.set(&(MemoriaCfg){.dst = &s_sign, .val = 0, .bytes = sizeof(s_sign)});
    EMBED_CALL(write_header, InspectPyWriteCtx, .src = src, .header = &g_cli_py_line[at]);
    const unsigned int end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at);
    unsigned int nested_end = 0u;
    for (unsigned int index = at + 1u; index < end; index += 1u)
    {
        const CliPyLine *const line = &g_cli_py_line[index];
        if (index < nested_end)
        {
            continue;
        }
        if ((EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src, .line = line,
                        .word_text = "def") ||
             EMBED_CALL(cli_inspect_py_src_line.py_head_is, CliInspectPySrcLineCfg, .src = src, .line = line,
                        .word_text = "class")) &&
            EMBED_CALL(cli_inspect_py_src_line.py_opens, CliInspectPySrcLineCfg, .src = src, .line = line))
        {
            nested_end = EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = index);
            continue;
        }
        const CliInspectTok *const first = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 0u);
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = first,
                       .word_text = "return") &&
            (line->count > 1u) &&
            !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                        .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = 1u),
                        .word_text = "None"))
        {
            s_sign.releases = 1;
        }
        s_sign.yields = s_sign.yields ||
                        EMBED_CALL(cli_inspect_py_src_word.py_contains, CliInspectPySrcWordCfg, .src = src,
                                   .toks = &g_cli_inspect_tok[line->first], .count = line->count, .word_text = "yield");
        if (EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src, .tok = first,
                       .word_text = "raise") &&
            (line->count > 1u) && (s_sign.raises < CLI_PY_PARAMS))
        {
            VerbaLinea raised = {s_sign.raised[s_sign.raises], CLI_NAME_MAX_LEN, 0ULL};
            unsigned int part = 1u;
            while ((part < line->count) &&
                   !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                               .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = part),
                               .word_text = "(") &&
                   !EMBED_CALL(cli_inspect_py_src_tok.py_is, CliInspectPySrcTokCfg, .src = src,
                               .tok = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = part),
                               .word_text = "from"))
            {
                const CliInspectTok *const word_text =
                    EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = line, .at = part);
                mmgr_line_put_n(&raised, src + word_text->start, word_text->end - word_text->start);
                part += 1u;
            }
            int known = (mmgr_line_finish(&raised) == 0ULL) || (raised.at == 0u);
            for (unsigned int one = 0u; !known && (one < s_sign.raises); one += 1u)
            {
                known = (cellul.eq(&(CatenaFinitaCfg){
                    .src = s_sign.raised[one], .other = s_sign.raised[s_sign.raises], .cap = CLI_POOL_BYTES}));
            }
            s_sign.raises += known ? 0u : 1u;
        }
    }
    const int none = s_sign.has_returns && (s_sign.returns_end - s_sign.returns_start == 4u) &&
                     (memor.cmp(&(MemoriaCfg){.src = src + s_sign.returns_start, .other = "None", .bytes = 4u}) == 0);
    s_sign.releases = s_sign.releases || (s_sign.has_returns && !none);
}

// the lines of the docstring being drawn up, each its start and length in their pool, and how many there are
static char s_pool[1u << 18u];
static VerbaLinea s_pool_line;
static unsigned long long s_line_start[CLI_PY_PARAMS * 4u];
static unsigned long long s_line_size[CLI_PY_PARAMS * 4u];
static unsigned int s_lines;

// the docstring's next line written in the pool
static VerbaLinea *write_line(void)
{
    if (s_lines < CLI_PY_PARAMS * 4u)
    {
        s_line_start[s_lines] = s_pool_line.at;
        s_line_size[s_lines] = 0ULL;
        s_lines += 1u;
    }
    return &s_pool_line;
}

// the docstring's last line ended where the pool stands
static void write_line_finish(void)
{
    s_line_size[s_lines - 1u] = s_pool_line.at - s_line_start[s_lines - 1u];
}

// a line of `text` put on the docstring
EMBED_INLINE void write_text_line(const InspectPyWriteCtx *args)
{
    mmgr_line_put(write_line(), args->text);
    write_line_finish();
}

// a section begun: a blank line and its title, underlined in NumPy's form and with a colon after it in Google's
EMBED_INLINE void write_section(const InspectPyWriteCtx *args)
{
    EMBED_CALL(write_text_line, InspectPyWriteCtx, .text = "");
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->form, .other = "numpy", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(write_text_line, InspectPyWriteCtx, .text = args->title);
        VerbaLinea *const line = write_line();
        for (size_t at = 0u; at < cellul.len(&(CatenaFinitaCfg){.src = args->title, .cap = CLI_POOL_BYTES}); at += 1u)
        {
            mmgr_line_ch(line, '-');
        }
        write_line_finish();
        return;
    }
    VerbaLinea *const line = write_line();
    mmgr_line_put(line, args->title);
    mmgr_line_ch(line, ':');
    write_line_finish();
}

// a part of the source put on the docstring's line
EMBED_INLINE void write_part(const InspectPyWriteCtx *args)
{
    mmgr_line_put_n(&s_pool_line, args->src + args->start, args->end_u64 - args->start);
}

// the fields of reStructuredText's form put on the docstring
EMBED_INLINE void write_remainder(const InspectPyWriteCtx *args)
{
    if ((s_sign.params > 0u) || s_sign.releases || s_sign.yields || (s_sign.raises > 0u))
    {
        EMBED_CALL(write_text_line, InspectPyWriteCtx, .text = "");
    }
    for (unsigned int at = 0u; at < s_sign.params; at += 1u)
    {
        mmgr_line_put(write_line(), ":param ");
        mmgr_line_put(&s_pool_line, s_sign.param[at]);
        mmgr_line_ch(&s_pool_line, ':');
        write_line_finish();
        if (s_sign.annotated[at])
        {
            mmgr_line_put(write_line(), ":type ");
            mmgr_line_put(&s_pool_line, s_sign.param[at]);
            mmgr_line_put(&s_pool_line, ": ");
            EMBED_CALL(write_part, InspectPyWriteCtx, .src = args->src, .start = s_sign.annotation_start[at],
                       .end_u64 = s_sign.annotation_end[at]);
            write_line_finish();
        }
    }
    if (s_sign.releases || s_sign.yields)
    {
        EMBED_CALL(write_text_line, InspectPyWriteCtx, .text = s_sign.yields ? ":yield:" : ":return:");
        if (s_sign.has_returns)
        {
            mmgr_line_put(write_line(), ":rtype: ");
            EMBED_CALL(write_part, InspectPyWriteCtx, .src = args->src, .start = s_sign.returns_start,
                       .end_u64 = s_sign.returns_end);
            write_line_finish();
        }
    }
    for (unsigned int at = 0u; at < s_sign.raises; at += 1u)
    {
        mmgr_line_put(write_line(), ":raises ");
        mmgr_line_put(&s_pool_line, s_sign.raised[at]);
        mmgr_line_ch(&s_pool_line, ':');
        write_line_finish();
    }
}

// the parameters' section of Google's or NumPy's `form` put on the docstring where the function has parameters: each
// parameter's name, with its annotation where it has one
EMBED_INLINE void write_params_section(const InspectPyWriteCtx *args)
{
    if (s_sign.params == 0u)
    {
        return;
    }
    const int numpy = cellul.eq(&(CatenaFinitaCfg){.src = args->form, .other = "numpy", .cap = CLI_POOL_BYTES});
    EMBED_CALL(write_section, InspectPyWriteCtx, .form = args->form, .title = numpy ? "Parameters" : "Args");
    for (unsigned int at = 0u; at < s_sign.params; at += 1u)
    {
        VerbaLinea *const line = write_line();
        mmgr_line_put(line, numpy ? "" : "    ");
        mmgr_line_put(line, s_sign.param[at]);
        if (s_sign.annotated[at])
        {
            mmgr_line_put(line, numpy ? " : " : " (");
            EMBED_CALL(write_part, InspectPyWriteCtx, .src = args->src, .start = s_sign.annotation_start[at],
                       .end_u64 = s_sign.annotation_end[at]);
            mmgr_line_put(line, numpy ? "" : ")");
        }
        mmgr_line_put(line, numpy ? "" : ":");
        write_line_finish();
    }
}

// the returns' or yields' section of Google's or NumPy's `form` put on the docstring where the function gives or
// yields anything: what its header names it gives, or an empty line taken off again where it names none
EMBED_INLINE void write_returns_section(const InspectPyWriteCtx *args)
{
    if (!s_sign.releases && !s_sign.yields)
    {
        return;
    }
    const int numpy = cellul.eq(&(CatenaFinitaCfg){.src = args->form, .other = "numpy", .cap = CLI_POOL_BYTES});
    EMBED_CALL(write_section, InspectPyWriteCtx, .form = args->form, .title = s_sign.yields ? "Yields" : "Returns");
    VerbaLinea *const line = write_line();
    if (s_sign.has_returns)
    {
        mmgr_line_put(line, numpy ? "" : "    ");
        EMBED_CALL(write_part, InspectPyWriteCtx, .src = args->src, .start = s_sign.returns_start,
                   .end_u64 = s_sign.returns_end);
        mmgr_line_put(line, numpy ? "" : ":");
    }
    write_line_finish();
    if (s_line_size[s_lines - 1u] == 0u)
    {
        s_lines -= 1u;
    }
}

// the raises' section of Google's or NumPy's `form` put on the docstring where the function raises anything: each
// name it raises
EMBED_INLINE void write_raises_section(const InspectPyWriteCtx *args)
{
    if (s_sign.raises == 0u)
    {
        return;
    }
    const int numpy = cellul.eq(&(CatenaFinitaCfg){.src = args->form, .other = "numpy", .cap = CLI_POOL_BYTES});
    EMBED_CALL(write_section, InspectPyWriteCtx, .form = args->form, .title = "Raises");
    for (unsigned int at = 0u; at < s_sign.raises; at += 1u)
    {
        VerbaLinea *const line = write_line();
        mmgr_line_put(line, numpy ? "" : "    ");
        mmgr_line_put(line, s_sign.raised[at]);
        mmgr_line_put(line, numpy ? "" : ":");
        write_line_finish();
    }
}

// the sections of Google's or NumPy's form put on the docstring
EMBED_INLINE void write_sections(const InspectPyWriteCtx *args)
{
    EMBED_CALL(write_params_section, InspectPyWriteCtx, .src = args->src, .form = args->form);
    EMBED_CALL(write_returns_section, InspectPyWriteCtx, .src = args->src, .form = args->form);
    EMBED_CALL(write_raises_section, InspectPyWriteCtx, .src = args->src, .form = args->form);
}

unsigned int cli_py_around(const CliInspectPySrcLineHeadCfg *args)
{
    return EMBED_CALL(write_function_around, InspectPyWriteCtx, .src = args->src, .line = args->line_u32,
                      .head = args->head);
}

int cli_inspect_py_docstring(const CliInspectSizeSrcCtx *args)
{
    const char *src = args->src;
    CliInspectEdit *edit = args->edit;
    CliInspectPosition *summary = args->summary;
    EMBED_CALL(cli_inspect_py_src_size.py_read, CliInspectPySrcSizeCfg, .src = src, .size = args->size);
    const unsigned int at =
        EMBED_CALL(write_function_around, InspectPyWriteCtx, .src = src, .line = args->line, .head = "def");
    if (at == g_cli_py_lines)
    {
        mmgr_line_put(args->err_msg, "Write Docstring writes one for the function the cursor is in.");
        return 0;
    }
    if (EMBED_CALL(cli_inspect_py.py_block_end, CliInspectPyCfg, .at = at) <= at + 1u)
    {
        mmgr_line_put(args->err_msg, "The function's body stands on its header's line.");
        return 0;
    }
    const CliPyLine *const first = &g_cli_py_line[at + 1u];
    int documented = (EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = first, .at = 0u)->kind == CLI_PY_STR);
    for (unsigned int part = 1u; documented && (part < first->count); part += 1u)
    {
        documented = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = first, .at = part)->field;
    }
    if (documented)
    {
        mmgr_line_put(args->err_msg, "The function has a docstring.");
        return 0;
    }
    EMBED_CALL(write_signature, InspectPyWriteCtx, .src = src, .at = at);
    const unsigned int header_end = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = &g_cli_py_line[at],
                                               .at = g_cli_py_line[at].count - 1u)
                                        ->to.line;
    unsigned long long body = EMBED_CALL(cli_inspect_py.py_tok, CliInspectPyCfg, .line = first, .at = 0u)->start;
    body = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = src, .at = body, .set = "\n");
    unsigned long long indent = body;
    unsigned int units = 0u;
    while (indent < args->size)
    {
        unsigned int letter = 0u;
        const unsigned long long reserved =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)src, .length = args->size, .at = indent, .letter = &letter);
        if ((letter == '\n') || !EMBED_CALL(cli_inspect_internal.white, CliInspectCtx, .letter = letter))
        {
            break;
        }
        indent += reserved;
        units += (letter >= 0x10000u) ? 2u : 1u;
    }
    s_pool_line = (VerbaLinea){s_pool, sizeof(s_pool), 0ULL};
    s_lines = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->form, .other = "rest", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(write_remainder, InspectPyWriteCtx, .src = src);
    }
    else if (!cellul.eq(&(CatenaFinitaCfg){.src = args->form, .other = "plain", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(write_sections, InspectPyWriteCtx, .src = src, .form = args->form);
    }
    mmgr_line_put_n(args->text, src + body, indent - body);
    mmgr_line_put(args->text, "\"\"\"");
    if (s_lines == 0u)
    {
        mmgr_line_put(args->text, "\"\"\"\n");
    }
    else
    {
        mmgr_line_ch(args->text, '\n');
        for (unsigned int index = 0u; index < s_lines; index += 1u)
        {
            if (s_line_size[index] > 0u)
            {
                mmgr_line_put_n(args->text, src + body, indent - body);
                mmgr_line_put_n(args->text, s_pool + s_line_start[index], s_line_size[index]);
            }
            mmgr_line_ch(args->text, '\n');
        }
        mmgr_line_put_n(args->text, src + body, indent - body);
        mmgr_line_put(args->text, "\"\"\"\n");
    }
    const CliInspectPosition place = {header_end + 1u, 0u};
    *edit = (CliInspectEdit){place, place, {0u, 0u}};
    *summary = (CliInspectPosition){header_end + 1u, units + 3u};
    return 1;
}
