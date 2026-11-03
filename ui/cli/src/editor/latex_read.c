// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_read.c: what a make leaves read: the names the engine writes read as the tree's files, the errors its log
// names, and the pages pdftocairo draws, every id in them made the preview's own
#include "latex_internal.h"

#include "../text/text.h"

typedef struct
{
    const char *text;
    unsigned long long length;
    int plain;
    char *out_capacity;
    const char *name;
    VerbaLinea *out;
    unsigned long long at;
    const char *needle;
    const char *tag;
    const char *svg;
    unsigned long long size;
    double width;
    double height;
    int first;
} LatexReadCtx;

// `length` bytes of a path, trimmed, written with forward slashes and `/./` as `/` into `out`, sized for
// CLI_PATH_MAX_LEN; and, where `plain`, with no `./` before it and in lower case on Windows, where case does not
// distinguish two files apart
static void latex_slash(const LatexReadCtx *args)
{
    const char *text = args->text;
    unsigned long long start = 0ULL;
    unsigned long long end = args->length;
    while ((start < end) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = text[start]))
    {
        start += 1ULL;
    }
    while ((end > start) && EMBED_CALL(cli_latex_internal.tex_white, CliLatexCtx, .letter = text[end - 1ULL]))
    {
        end -= 1ULL;
    }
    VerbaLinea line = {args->out_capacity, CLI_PATH_MAX_LEN, 0ULL};
    unsigned long long at = start;
    while (at < end)
    {
        const int slash = (text[at] == '/') || (text[at] == '\\');
        if (slash && (at + 2ULL < end) && (text[at + 1ULL] == '.') &&
            ((text[at + 2ULL] == '/') || (text[at + 2ULL] == '\\')))
        {
            mmgr_line_ch(&line, '/');
            at += 3ULL;
            continue;
        }
        mmgr_line_ch(&line, slash ? '/' : text[at]);
        at += 1ULL;
    }
    mmgr_line_finish(&line);
    args->out_capacity[CLI_PATH_MAX_LEN - 1u] = '\0';
    if (!args->plain)
    {
        return;
    }
    if ((args->out_capacity[0] == '.') && (args->out_capacity[1] == '/'))
    {
        memor.move_down(&(MemoriaCfg){
            .dst = args->out_capacity,
            .src = args->out_capacity + 2,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->out_capacity + 2, .cap = CLI_CELLS_POOL}) + 1u});
    }
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->out_capacity,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->out_capacity, .cap = CLI_CELLS_POOL}));
#endif
}

void cli_tex_names_init(const CliLatexNamesRootCtx *args)
{
    const char *path = args->path;
    char named[CLI_PATH_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ULL};
    mmgr_line_put(&line, args->job);
    mmgr_line_put(&line, ".tex");
    mmgr_line_finish(&line);
    EMBED_CALL(latex_slash, LatexReadCtx, .text = named,
               .length = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}), .plain = 1,
               .out_capacity = args->names_out->job);
    char made[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->dir, .relative = named, .joined = made);
    EMBED_CALL(latex_slash, LatexReadCtx, .text = made,
               .length = cellul.len(&(CatenaFinitaCfg){.src = made, .cap = CLI_CELLS_POOL}), .plain = 1,
               .out_capacity = args->names_out->made);
    EMBED_CALL(latex_slash, LatexReadCtx, .text = args->root,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}), .plain = 1,
               .out_capacity = args->names_out->root);
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->names_out->root, .cap = CLI_CELLS_POOL});
    if (((size == 0u) || (args->names_out->root[size - 1u] != '/')) && (size + 1u < CLI_PATH_MAX_LEN))
    {
        args->names_out->root[size] = '/';
        args->names_out->root[size + 1u] = '\0';
    }
    args->names_out->path = path;
}

int cli_tex_names_get(const CliLatexLengthCtx *args)
{
    char supplied[CLI_PATH_MAX_LEN];
    EMBED_CALL(latex_slash, LatexReadCtx, .text = args->name, .length = args->length, .plain = 1,
               .out_capacity = supplied);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = supplied, .other = args->names->job, .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = supplied, .other = args->names->made, .cap = CLI_CELLS_POOL})))
    {
        VerbaLinea line = {args->file, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, args->names->path);
        mmgr_line_finish(&line);
        return 1;
    }
    const size_t root = cellul.len(&(CatenaFinitaCfg){.src = args->names->root, .cap = CLI_CELLS_POOL});
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = supplied, .other_pointer = args->names->root,
                    .size = root))
    {
        return 0;
    }
    char start[CLI_PATH_MAX_LEN];
    EMBED_CALL(latex_slash, LatexReadCtx, .text = args->name, .length = args->length, .plain = 0,
               .out_capacity = start);
    // the rest is none where the root's length falls past the name or inside one of its letters
    if ((cellul.len(&(CatenaFinitaCfg){.src = start, .cap = CLI_CELLS_POOL}) < root) ||
        (((unsigned char)start[root] & 0xC0u) == 0x80u))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->file,
                            .src = start + root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = start + root, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}
