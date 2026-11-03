// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook.c: a notebook read from its JSON or its percent script, and written back as it was read
#include "../text/text.h"
#include "notebook_internal.h"

typedef struct
{
    const char *path;
    char *out;
    char letter;
    const char *from;
    const char *to;
    const char *mark;
    CliJson *json;
    const char *text;
    unsigned long long length;
    unsigned long long start;
    unsigned int cell;
    unsigned int cells;
    const char *comment;
    unsigned int kind;
    unsigned int title;
    const char *rest;
    const char *end;
    unsigned int *kind_pointer;
    unsigned int *title_pointer;
    unsigned long long size;
} NotebookCtx;

// the extension of `path`, after the last `.` of its last part, in lower case, into `out`, sized for 16: empty
// where it has none or one longer
EMBED_INLINE void cli_extension(const NotebookCtx *args)
{
    char *out = args->out;
    const char *name = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    name = args->path + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                                   .at = (unsigned long long)(name - args->path), .set = "/\\");
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    out[0] = '\0';
    if ((dot == NULL) || (cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL}) >= 16u))
    {
        return;
    }
    (void)EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .out_capacity = out, .capacity = 16u,
                     .text = dot + 1);
}

// The comment a script's cells are marked with in the language of `path`, by its extension
const char *cli_notebook_comment(const CliNotebookCtx *args)
{
    static const char *const s_slashes[] = {"js",   "ts", "mjs",   "cs", "cpp", "c",
                                            "java", "kt", "scala", "go", "rs",  "swift"};
    static const char *const s_dashes[] = {"lua", "sql", "hs"};
    char extension[16];
    EMBED_CALL(cli_extension, NotebookCtx, .path = args->path, .out = extension);
    for (unsigned int at = 0u; at < (sizeof(s_slashes) / sizeof(s_slashes[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = s_slashes[at], .cap = CLI_CELLS_POOL}))
        {
            return "//";
        }
    }
    for (unsigned int at = 0u; at < (sizeof(s_dashes) / sizeof(s_dashes[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = s_dashes[at], .cap = CLI_CELLS_POOL}))
        {
            return "--";
        }
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "m", .cap = CLI_CELLS_POOL})) ? "%" : "#";
}

EMBED_INLINE int cli_is_space(const NotebookCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

// the line of `text` that starts at `at` before `end`: where it ends, its `\r` before `\n` left off, into `stop`;
// where the next starts
const char *cli_notebook_line(const CliNotebookAtCtx *args)
{
    const char **stop = args->stop;
    const char *next = args->at;
    next += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = next, .at = 0ULL,
                       .size = (unsigned long long)(args->end - next), .set = "\n");
    *stop = ((next > args->at) && (next[-1] == '\r')) ? (next - 1) : next;
    return (next < args->end) ? (next + 1) : next;
}

// whether the line from `from` to `to`, its spaces before it left off, starts with `mark`: where after it, else NULL
EMBED_INLINE const char *cli_on_mark(const NotebookCtx *args)
{
    const char *from = args->from;
    while ((from < args->to) && EMBED_CALL(cli_is_space, NotebookCtx, .letter = from[0]))
    {
        from += 1;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->mark, .cap = CLI_CELLS_POOL});
    return (((unsigned long long)(args->to - from) >= length) &&
            (memor.cmp(&(MemoriaCfg){.src = from, .other = args->mark, .bytes = length}) == 0))
               ? (from + length)
               : NULL;
}

int cli_notebook_is(const CliNotebookPathCfg *args)
{
    char extension[16];
    EMBED_CALL(cli_extension, NotebookCtx, .path = args->path, .out = extension);
    if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "ipynb", .cap = CLI_CELLS_POOL}))
    {
        return 1;
    }
    char mark[8];
    VerbaLinea line = {mark, sizeof(mark), 0ULL};
    mmgr_line_put(&line, EMBED_CALL(cli_notebook_internal.comment, CliNotebookCtx, .path = args->path));
    mmgr_line_put(&line, " %%");
    mmgr_line_finish(&line);
    const char *const end = args->text + args->size;
    const char *at = args->text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next =
            EMBED_CALL(cli_notebook_at_internal.line, CliNotebookAtCtx, .at = at, .end = end, .stop = &stop);
        if (EMBED_CALL(cli_on_mark, NotebookCtx, .from = at, .to = stop, .mark = mark) != NULL)
        {
            return 1;
        }
        at = next;
    }
    return 0;
}

int cli_notebook_ends(const CliNotebookTextCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->ending, .cap = CLI_CELLS_POOL});
    return (args->length >= size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text + args->length - size, .other = args->ending, .bytes = size}) ==
            0);
}

// `length` bytes of `text` put after the text being made at the end of the tree's texts
EMBED_INLINE void cli_piece(const NotebookCtx *args)
{
    if ((args->json->used + args->length + 1ULL) > args->json->text_max)
    {
        args->json->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->json->text + args->json->used, .src = args->text, .bytes = args->length});
    args->json->used += args->length;
}

// the text made from `start` to the end of the tree's texts made a string node
EMBED_INLINE unsigned int cli_pieces_init(const NotebookCtx *args)
{
    CliJson *json = args->json;
    const unsigned int made = json->full ? CLI_JSON_NONE
                                         : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json,
                                                      .kind = CFG_JSON_STRING);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    json->text[json->used] = '\0';
    json->node[made].text = args->start;
    json->node[made].length = json->used - args->start;
    json->used += 1ULL;
    return made;
}

unsigned int cli_notebook_join(const CliNotebookJsonValueCtx *args)
{
    const unsigned long long start = args->json->used;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                   .node = args->value, .kind = CFG_JSON_STRING))
    {
        EMBED_CALL(cli_piece, NotebookCtx, .json = args->json,
                   .text = args->json->text + args->json->node[args->value].text,
                   .length = args->json->node[args->value].length);
    }
    for (unsigned int line = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = args->value, .kind = CFG_JSON_ARRAY)
                                 ? args->json->node[args->value].first
                                 : CLI_JSON_NONE;
         line != CLI_JSON_NONE; line = args->json->node[line].next)
    {
        if (args->json->node[line].kind == CFG_JSON_STRING)
        {
            EMBED_CALL(cli_piece, NotebookCtx, .json = args->json,
                       .text = args->json->text + args->json->node[line].text, .length = args->json->node[line].length);
        }
    }
    return EMBED_CALL(cli_pieces_init, NotebookCtx, .json = args->json, .start = start);
}

const char *cli_notebook_source(const CliNotebookJsonCellCtx *args)
{
    unsigned long long *length = args->length_pointer;
    const char *const source =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json_read,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = args->json_read, .object = args->cell, .name = "source"),
                   .length_pointer = length);
    if (source == NULL)
    {
        *length = 0ULL;
        return "";
    }
    return source;
}

// A text as Jupyter stores it: its lines, each with its line end but the last
unsigned int cli_notebook_lines_get(const CliNotebookJsonCtx *args)
{
    const unsigned int lines =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json, .kind = CFG_JSON_ARRAY);
    unsigned long long from = 0ULL;
    while (from < args->length)
    {
        unsigned long long to = from;
        to = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = to,
                        .size = args->length, .set = "\n");
        to += (to < args->length) ? 1ULL : 0ULL;
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json, .array = lines,
                   .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                       .json = args->json, .text = args->text + from, .length = to - from));
        from = to;
    }
    return lines;
}

// each output of a cell read: its text one text, and each of its data that is a list and no JSON one text
EMBED_INLINE void cli_outputs_join(const NotebookCtx *args)
{
    CliJson *json = args->json;
    const unsigned int outputs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->cell, .name = "outputs");
    for (unsigned int output = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = outputs, .kind = CFG_JSON_ARRAY)
                                   ? json->node[outputs].first
                                   : CLI_JSON_NONE;
         output != CLI_JSON_NONE; output = json->node[output].next)
    {
        const unsigned int text = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = output, .name = "text");
        if (text != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = output,
                       .name = "text",
                       .value = EMBED_CALL(cli_notebook_json_value_internal.join, CliNotebookJsonValueCtx, .json = json,
                                           .value = text));
        }
        const unsigned int data = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = output, .name = "data");
        for (unsigned int kind = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = data, .kind = CFG_JSON_OBJECT)
                                     ? json->node[data].first
                                     : CLI_JSON_NONE;
             kind != CLI_JSON_NONE; kind = json->node[kind].next)
        {
            const char *const name = json->text + json->node[kind].key;
            if ((json->node[kind].kind == CFG_JSON_ARRAY) &&
                !EMBED_CALL(cli_notebook_text_internal.ends, CliNotebookTextCtx, .text = name,
                            .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}),
                            .ending = "json"))
            {
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = data,
                           .name = name,
                           .value = EMBED_CALL(cli_notebook_json_value_internal.join, CliNotebookJsonValueCtx,
                                               .json = json, .value = kind));
            }
        }
    }
}

// A cell of a script ended: its lines from `from` to `to`, its blank lines at the end left off, its source the lines
// joined, a markdown cell's each with its comment taken off
static void cli_cell_finish(const NotebookCtx *args)
{
    CliJson *json = args->json;
    unsigned int kind = args->kind;
    const int markdown = cellul.eq(
        &(CatenaFinitaCfg){.src = json->text + json->node[kind].text, .other = "markdown", .cap = CLI_CELLS_POOL});
    const unsigned long long start = json->used;
    const unsigned long long comment_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->comment, .cap = CLI_CELLS_POOL});
    int first = 1;
    const char *at = args->from;
    while (at < args->to)
    {
        const char *stop = NULL;
        const char *const next =
            EMBED_CALL(cli_notebook_at_internal.line, CliNotebookAtCtx, .at = at, .end = args->to, .stop = &stop);
        const char *line = at;
        if (markdown && ((unsigned long long)(stop - line) >= comment_length) &&
            (memor.cmp(&(MemoriaCfg){.src = line, .other = args->comment, .bytes = comment_length}) == 0))
        {
            line += comment_length;
            line += ((line < stop) && (line[0] == ' ')) ? 1 : 0;
        }
        if (!first)
        {
            EMBED_CALL(cli_piece, NotebookCtx, .json = json, .text = "\n", .length = 1ULL);
        }
        first = 0;
        EMBED_CALL(cli_piece, NotebookCtx, .json = json, .text = line, .length = (unsigned long long)(stop - line));
        at = next;
    }
    const unsigned int source = EMBED_CALL(cli_pieces_init, NotebookCtx, .json = json, .start = start);
    const unsigned int cell =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    const unsigned int metadata =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = cell,
               .name = "cell_type", .value = kind);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = cell,
               .name = "source", .value = source);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = metadata,
               .name = "title", .value = args->title);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = cell,
               .name = "metadata", .value = metadata);
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = cell, .name = "outputs",
        .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = cell,
               .name = "execution_count",
               .value =
                   EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_NULL));
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = json, .array = args->cells, .value = cell);
}

// a cell's mark read after `%%`, `rest` to `end`: its kind, and its title, the kind's word taken out, both trimmed
EMBED_INLINE void cli_mark_read(const NotebookCtx *args)
{
    CliJson *json = args->json;
    const char *rest = args->rest;
    const char *end = args->end;
    unsigned int *kind = args->kind_pointer;
    unsigned int *title = args->title_pointer;
    while ((rest < end) && EMBED_CALL(cli_is_space, NotebookCtx, .letter = rest[0]))
    {
        rest += 1;
    }
    while ((end > rest) && EMBED_CALL(cli_is_space, NotebookCtx, .letter = end[-1]))
    {
        end -= 1;
    }
    const char *word_text = NULL;
    const char *after = end;
    const char *found = NULL;
    {
        const char *at = rest;
        while ((found == NULL) && (at < end))
        {
            const unsigned long long left = (unsigned long long)(end - at);
            if (((left >= 10u) && (memor.cmp(&(MemoriaCfg){.src = at, .other = "[markdown]", .bytes = 10u}) == 0)) ||
                ((left >= 4u) && (memor.cmp(&(MemoriaCfg){.src = at, .other = "[md]", .bytes = 4u}) == 0)))
            {
                found = at;
                word_text = "markdown";
                after = at;
                after += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = after, .at = 0ULL,
                                    .size = (unsigned long long)(end - after), .set = "]");
                after += (after < end) ? 1 : 0;
            }
            at += 1;
        }
    }
    {
        const char *at = rest;
        while ((found == NULL) && (at + 5 <= end))
        {
            if (memor.cmp(&(MemoriaCfg){.src = at, .other = "[raw]", .bytes = 5u}) == 0)
            {
                found = at;
                word_text = "raw";
                after = at + 5;
            }
            at += 1;
        }
    }
    const char *const named = (word_text != NULL) ? word_text : "code";
    *kind = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = json,
                       .text = named, .length = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}));
    const unsigned long long start = json->used;
    EMBED_CALL(cli_piece, NotebookCtx, .json = json, .text = rest,
               .length = (unsigned long long)(((found != NULL) ? found : end) - rest));
    EMBED_CALL(cli_piece, NotebookCtx, .json = json, .text = after, .length = (unsigned long long)(end - after));
    // the title trimmed where it is made, before anything else is put after it
    unsigned long long from = start;
    unsigned long long to = json->used;
    while ((from < to) && EMBED_CALL(cli_is_space, NotebookCtx, .letter = json->text[from]))
    {
        from += 1ULL;
    }
    while ((to > from) && EMBED_CALL(cli_is_space, NotebookCtx, .letter = json->text[to - 1u]))
    {
        to -= 1ULL;
    }
    json->used = to;
    *title = EMBED_CALL(cli_pieces_init, NotebookCtx, .json = json, .start = from);
}

// a script of `%%` cells read: the lines before the first its header, each cell from its mark to the next
EMBED_INLINE unsigned int cli_script_read(const NotebookCtx *args)
{
    CliJson *json = args->json;
    const char *path = args->path;
    const char *text = args->text;
    const char *const comment = EMBED_CALL(cli_notebook_internal.comment, CliNotebookCtx, .path = path);
    char mark[8];
    VerbaLinea line = {mark, sizeof(mark), 0ULL};
    mmgr_line_put(&line, comment);
    mmgr_line_put(&line, " %%");
    mmgr_line_finish(&line);
    const char *const end = text + args->size;
    const unsigned int cells =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_ARRAY);
    const unsigned int book =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    unsigned int kind = CLI_JSON_NONE;
    unsigned int title = CLI_JSON_NONE;
    const char *header_end = end;
    const char *body = NULL;
    const char *body_end = NULL;
    const char *at = text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next =
            EMBED_CALL(cli_notebook_at_internal.line, CliNotebookAtCtx, .at = at, .end = end, .stop = &stop);
        const char *const rest = EMBED_CALL(cli_on_mark, NotebookCtx, .from = at, .to = stop, .mark = mark);
        if (rest != NULL)
        {
            if (kind != CLI_JSON_NONE)
            {
                EMBED_CALL(cli_cell_finish, NotebookCtx, .json = json, .cells = cells, .comment = comment, .kind = kind,
                           .title = title, .from = body, .to = body_end);
            }
            header_end = (kind == CLI_JSON_NONE) ? at : header_end;
            EMBED_CALL(cli_mark_read, NotebookCtx, .json = json, .rest = rest, .end = stop, .kind_pointer = &kind,
                       .title_pointer = &title);
            body = next;
            body_end = next;
        }
        else if (kind != CLI_JSON_NONE)
        {
            const char *from = at;
            while ((from < stop) && EMBED_CALL(cli_is_space, NotebookCtx, .letter = from[0]))
            {
                from += 1;
            }
            body_end = (from < stop) ? next : body_end;
        }
        at = next;
    }
    if (kind != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_cell_finish, NotebookCtx, .json = json, .cells = cells, .comment = comment, .kind = kind,
                   .title = title, .from = body, .to = body_end);
    }
    // the header: each line before the first cell, a line end after each
    const unsigned long long start = json->used;
    at = text;
    while (at < header_end)
    {
        const char *stop = NULL;
        const char *const next =
            EMBED_CALL(cli_notebook_at_internal.line, CliNotebookAtCtx, .at = at, .end = header_end, .stop = &stop);
        EMBED_CALL(cli_piece, NotebookCtx, .json = json, .text = at, .length = (unsigned long long)(stop - at));
        EMBED_CALL(cli_piece, NotebookCtx, .json = json, .text = "\n", .length = 1ULL);
        at = next;
    }
    const unsigned int header = EMBED_CALL(cli_pieces_init, NotebookCtx, .json = json, .start = start);
    const char *language;
    if (cellul.eq(&(CatenaFinitaCfg){.src = comment, .other = "//", .cap = CLI_CELLS_POOL}))
    {
        language = "javascript";
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = comment, .other = "%", .cap = CLI_CELLS_POOL}))
    {
        language = "matlab";
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = comment, .other = "--", .cap = CLI_CELLS_POOL}))
    {
        language = "lua";
    }
    else
    {
        language = "python";
    }
    const unsigned int metadata =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = metadata,
               .name = "language",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = language,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_CELLS_POOL})));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = book,
               .name = "format",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = json, .text = "percent", .length = 7u));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = book,
               .name = "header", .value = header);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = book,
               .name = "cells", .value = cells);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = book,
               .name = "metadata", .value = metadata);
    return json->full ? CLI_JSON_NONE : book;
}

unsigned int cli_notebook_read(const CliNotebookPathCfg *args)
{
    char extension[16];
    EMBED_CALL(cli_extension, NotebookCtx, .path = args->path, .out = extension);
    if (!cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "ipynb", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int book = EMBED_CALL(cli_script_read, NotebookCtx, .json = args->json, .path = args->path,
                                             .text = args->text, .size = args->size);
        if (book == CLI_JSON_NONE)
        {
            mmgr_line_put(args->err_msg, args->path);
            mmgr_line_put(args->err_msg, ": it contains more than a notebook is read into");
        }
        return book;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ULL};
    const unsigned int book = EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                                         .json = args->json, .text = args->text, .size = args->size,
                                         .tokens = args->tokens, .tokens_max = args->tokens_max, .err_msg = &reason);
    mmgr_line_finish(&reason);
    if (book == CLI_JSON_NONE)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put(args->err_msg, why);
        return book;
    }
    const unsigned int cells = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = book, .name = "cells");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = cells,
                    .kind = CFG_JSON_ARRAY))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, " contains no cells");
        return CLI_JSON_NONE;
    }
    for (unsigned int cell = args->json->node[cells].first; cell != CLI_JSON_NONE; cell = args->json->node[cell].next)
    {
        if (args->json->node[cell].kind == CFG_JSON_OBJECT)
        {
            EMBED_CALL(
                cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = cell,
                .name = "source",
                .value = EMBED_CALL(cli_notebook_json_value_internal.join, CliNotebookJsonValueCtx, .json = args->json,
                                    .value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                        .json_read = args->json, .object = cell, .name = "source")));
            EMBED_CALL(cli_outputs_join, NotebookCtx, .json = args->json, .cell = cell);
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = book,
               .name = "format",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = args->json, .text = "ipynb", .length = 5u));
    return book;
}
