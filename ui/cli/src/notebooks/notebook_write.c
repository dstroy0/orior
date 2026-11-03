// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_write.c: a notebook written back as the file it was read from, and a notebook's JSON written without its
// outputs
#include "notebook_internal.h"

typedef struct
{
    const CliJson *json;
    const char *path;
    unsigned int book;
    VerbaLinea *out;
    CliJson *json_out;
    unsigned int value;
} NotebookWriteCtx;

// a percent script's text: its header, then each cell's mark and its lines, a blank line before each mark
EMBED_INLINE void cli_script_write(const NotebookWriteCtx *args)
{
    const CliJson *json = args->json;
    const char *path = args->path;
    const char *const comment = EMBED_CALL(cli_notebook_internal.comment, CliNotebookCtx, .path = path);
    const unsigned long long begun = args->out->at;
    const char *const header =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->book, .name = "header"),
                   .length_pointer = NULL);
    mmgr_line_put(args->out, (header != NULL) ? header : "");
    const unsigned int cells = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->book, .name = "cells");
    for (unsigned int cell = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = cells, .kind = CFG_JSON_ARRAY)
                                 ? json->node[cells].first
                                 : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = json->node[cell].next)
    {
        const unsigned long long written = mmgr_line_ok(args->out) ? (args->out->at - begun) : 0ULL;
        if ((written != 0ULL) && !EMBED_CALL(cli_notebook_text_internal.ends, CliNotebookTextCtx,
                                             .text = args->out->out + begun, .length = written, .ending = "\n\n"))
        {
            mmgr_line_ch(args->out, '\n');
        }
        const unsigned int metadata = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = cell, .name = "metadata");
        const char *const title =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = metadata, .name = "title"),
                       .length_pointer = NULL);
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = cell, .name = "cell_type"),
                       .length_pointer = NULL);
        const char *const kind = (named != NULL) ? named : "code";
        const int markdown = cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "markdown", .cap = CLI_CELLS_POOL});
        mmgr_line_put(args->out, comment);
        mmgr_line_put(args->out, " %%");
        if ((title != NULL) && (title[0] != '\0'))
        {
            mmgr_line_ch(args->out, ' ');
            mmgr_line_put(args->out, title);
        }
        if (markdown || (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "raw", .cap = CLI_CELLS_POOL})))
        {
            mmgr_line_put(args->out, " [");
            mmgr_line_put(args->out, kind);
            mmgr_line_ch(args->out, ']');
        }
        mmgr_line_ch(args->out, '\n');
        unsigned long long length = 0ULL;
        const char *const source = EMBED_CALL(cli_notebook_json_cell_internal.source, CliNotebookJsonCellCtx,
                                              .json_read = json, .cell = cell, .length_pointer = &length);
        const char *at = source;
        while (at < (source + length))
        {
            const char *stop = NULL;
            const char *const next = EMBED_CALL(cli_notebook_at_internal.line, CliNotebookAtCtx, .at = at,
                                                .end = source + length, .stop = &stop);
            if (markdown)
            {
                mmgr_line_put(args->out, comment);
                if (stop > at)
                {
                    mmgr_line_ch(args->out, ' ');
                }
            }
            mmgr_line_put_n(args->out, at, (unsigned long long)(stop - at));
            mmgr_line_ch(args->out, '\n');
            at = next;
        }
    }
}

// the list of lines a text node stores, where it is one
EMBED_INLINE unsigned int cli_lines_again(const NotebookWriteCtx *args)
{
    CliJson *json = args->json_out;
    unsigned int value = args->value;
    const unsigned int joined =
        EMBED_CALL(cli_notebook_json_value_internal.join, CliNotebookJsonValueCtx, .json = json, .value = value);
    unsigned long long length = 0ULL;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = json, .node = joined, .length_pointer = &length);
    return (text != NULL) ? EMBED_CALL(cli_notebook_json_internal.lines_get, CliNotebookJsonCtx, .json = json,
                                       .text = text, .length = length)
                          : CLI_JSON_NONE;
}

void cli_notebook_write(const CliNotebookPathBookCfg *args)
{
    const char *const format =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->book, .name = "format"),
                   .length_pointer = NULL);
    if ((format != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = format, .other = "percent", .cap = CLI_CELLS_POOL})))
    {
        EMBED_CALL(cli_script_write, NotebookWriteCtx, .json = args->json, .path = args->path, .book = args->book,
                   .out = args->out);
        return;
    }
    const unsigned int copy =
        EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = args->json, .node = args->book);
    EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = args->json, .object = copy,
               .name = "format");
    const unsigned int cells = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = copy, .name = "cells");
    for (unsigned int cell = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = cells, .kind = CFG_JSON_ARRAY)
                                 ? args->json->node[cells].first
                                 : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = args->json->node[cell].next)
    {
        if (args->json->node[cell].kind != CFG_JSON_OBJECT)
        {
            continue;
        }
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = cell,
                   .name = "source",
                   .value = EMBED_CALL(cli_lines_again, NotebookWriteCtx, .json_out = args->json,
                                       .value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                           .json_read = args->json, .object = cell, .name = "source")));
        const unsigned int outputs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = args->json, .object = cell, .name = "outputs");
        for (unsigned int output = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = args->json, .node = outputs, .kind = CFG_JSON_ARRAY)
                                       ? args->json->node[outputs].first
                                       : CLI_JSON_NONE;
             output != CLI_JSON_NONE; output = args->json->node[output].next)
        {
            const unsigned int text = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = args->json, .object = output, .name = "text");
            if (text != CLI_JSON_NONE)
            {
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                           .object = output, .name = "text",
                           .value =
                               EMBED_CALL(cli_lines_again, NotebookWriteCtx, .json_out = args->json, .value = text));
            }
            const unsigned int data = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = args->json, .object = output, .name = "data");
            for (unsigned int kind = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = args->json, .node = data, .kind = CFG_JSON_OBJECT)
                                         ? args->json->node[data].first
                                         : CLI_JSON_NONE;
                 kind != CLI_JSON_NONE; kind = args->json->node[kind].next)
            {
                const char *const name = args->json->text + args->json->node[kind].key;
                if ((args->json->node[kind].kind == CFG_JSON_STRING) &&
                    !EMBED_CALL(cli_notebook_text_internal.ends, CliNotebookTextCtx, .text = name,
                                .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}),
                                .ending = "json") &&
                    (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "text/", .cap = CLI_CELLS_POOL})))
                {
                    EMBED_CALL(
                        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json,
                        .object = data, .name = name,
                        .value = EMBED_CALL(cli_lines_again, NotebookWriteCtx, .json_out = args->json, .value = kind));
                }
            }
        }
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = args->json, .node = copy,
               .out = args->out, .indent = 1u, .sorted = 1);
    mmgr_line_ch(args->out, '\n');
}

int cli_notebook_without_outputs(const CliNotebookTextCfg *args)
{
    const unsigned int book = EMBED_CALL(
        cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = args->json, .text = args->text,
        .size = args->size, .tokens = args->tokens, .tokens_max = args->tokens_max, .err_msg = args->err_msg);
    if (book == CLI_JSON_NONE)
    {
        return 0;
    }
    const unsigned int cells = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = book, .name = "cells");
    for (unsigned int cell = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = cells, .kind = CFG_JSON_ARRAY)
                                 ? args->json->node[cells].first
                                 : CLI_JSON_NONE;
         cell != CLI_JSON_NONE; cell = args->json->node[cell].next)
    {
        const char *const kind =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = cell, .name = "cell_type"),
                       .length_pointer = NULL);
        if ((kind != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "code", .cap = CLI_CELLS_POOL})))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = cell,
                       .name = "outputs",
                       .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                           .kind = CFG_JSON_ARRAY));
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = cell,
                       .name = "execution_count",
                       .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json,
                                           .kind = CFG_JSON_NULL));
        }
        if (args->json->node[cell].kind == CFG_JSON_OBJECT)
        {
            EMBED_CALL(
                cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = cell,
                .name = "source",
                .value = EMBED_CALL(cli_notebook_json_value_internal.join, CliNotebookJsonValueCtx, .json = args->json,
                                    .value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                        .json_read = args->json, .object = cell, .name = "source")));
        }
    }
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->json, .object = book,
               .name = "format",
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = args->json, .text = "ipynb", .length = 5u));
    EMBED_CALL(cli_notebook_path_book.write, CliNotebookPathBookCfg, .json = args->json, .path = "x.ipynb",
               .book = book, .out = args->out);
    if (args->json->full)
    {
        mmgr_line_put(args->err_msg, "the notebook contains more than the tree it is read into fits");
        return 0;
    }
    return 1;
}
