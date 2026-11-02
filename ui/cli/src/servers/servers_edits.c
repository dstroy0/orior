// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_edits.c: a server's workspace edit written as each file's edits, in a cell counted to the edit's files and
// edits before they are read. An edit of a file given its server as a slice is an edit of the file its lines came from,
// and one on a bracket the slice closes is none
#include "../command_line/cells.h"
#include "servers_internal.h"

#include <stdlib.h>

// One edit of a workspace edit: the file it changes, its span, the node of its text, and its place among the edits
typedef struct
{
    unsigned int file;
    CliTextPlace from;
    CliTextPlace to;
    unsigned int text;
    unsigned int order;
} CliServersEdit;
// The edits of one workspace edit and the paths of its files, `edit_room` and `file_room` of them in one cell
typedef struct
{
    CliServersEdit *edit;
    unsigned int edits;
    unsigned int edit_room;
    char (*edited)[CLI_PATH_MAX_LEN];
    unsigned int files;
    unsigned int file_room;
} CliServersEditing;
// the most files one workspace edit changes
#define CLI_SERVERS_EDITED 1024u

typedef struct
{
    const CliJson *json;
    unsigned int edits;
    unsigned int before;
    CliServersEditing *editing;
    const char *path;
    const char *uri;
    unsigned long long length;
    CliTextPlace one;
    CliTextPlace other;
    unsigned int edit;
} ServersEditsCtx;
// the edits of an array, CLI_TEXT_EDITS at the most counted with those before
EMBED_INLINE unsigned int edits_on_count(const ServersEditsCtx *args)
{
    const CliJson *json = args->json;
    unsigned int count = args->before;
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = args->edits, .kind = CFG_JSON_ARRAY)
                                ? json->node[args->edits].first
                                : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (count < CLI_TEXT_EDITS); one = json->node[one].next)
    {
        count += 1u;
    }
    return count;
}

// the place of the file at `path` among those edited, put after them where it is not among them: `file_room` where no
// room for it is left
EMBED_INLINE unsigned int edits_file(const ServersEditsCtx *args)
{
    unsigned int file = 0u;
    while ((file < args->editing->files) &&
           (!cellul.eq(
               &(CatenaFinitaCfg){.src = args->editing->edited[file], .other = args->path, .cap = CLI_CELLS_POOL})))
    {
        file += 1u;
    }
    if (file == args->editing->files)
    {
        if (args->editing->files == args->editing->file_room)
        {
            return args->editing->file_room;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = args->editing->edited[file],
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
        args->editing->files += 1u;
    }
    return file;
}

// the edits of `edits` for the file of `uri`, `length` bytes, put after those stored
static void edits_add(const ServersEditsCtx *args)
{
    const CliJson *json = args->json;
    const char *uri = args->uri;
    unsigned long long length = args->length;
    unsigned int edits = args->edits;
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri, .length = length, .path_out = path))
    {
        return;
    }
    const unsigned int file = EMBED_CALL(edits_file, ServersEditsCtx, .editing = args->editing, .path = path);
    if (file == args->editing->file_room)
    {
        return;
    }
    for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                       .node = edits, .kind = CFG_JSON_ARRAY)
                                ? json->node[edits].first
                                : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (args->editing->edits < args->editing->edit_room); one = json->node[one].next)
    {
        const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = one, .name = "range");
        CliTextPlace from =
            EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = range, .name = "start"));
        CliTextPlace to =
            EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = range, .name = "end"));
        // an edit of a slice is an edit of the file its lines came from, a span's lines standing together in both
        char moved[CLI_PATH_MAX_LEN];
        const unsigned int rows = (to.line > from.line) ? (to.line - from.line) : 0u;
        const int where = EMBED_CALL(cli_servers_path_line_from_internal.slice_out, CliServersPathLineFromCtx,
                                     .path = path, .line_pointer = &from.line, .from_room = moved);
        to.line = from.line + rows;
        const unsigned int target =
            (where == 2) ? EMBED_CALL(edits_file, ServersEditsCtx, .editing = args->editing, .path = moved) : file;
        if ((where == 0) || (target == args->editing->file_room))
        {
            continue;
        }
        args->editing->edit[args->editing->edits] =
            (CliServersEdit){target, from, to,
                             EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = one, .name = "newText"),
                             args->editing->edits};
        args->editing->edits += 1u;
    }
}

// two places in the order they stand in a text
EMBED_INLINE int edits_place_order(const ServersEditsCtx *args)
{
    if (args->one.line != args->other.line)
    {
        return (args->one.line < args->other.line) ? -1 : 1;
    }
    return (args->one.col < args->other.col) ? -1 : (args->one.col > args->other.col) ? 1 : 0;
}

// two edits by their file, then their span, then the order they were given in
static int edits_order(const void *one, const void *other)
{
    const CliServersEdit *const left = one;
    const CliServersEdit *const right = other;
    if (left->file != right->file)
    {
        return (left->file < right->file) ? -1 : 1;
    }
    const int from = EMBED_CALL(edits_place_order, ServersEditsCtx, .one = left->from, .other = right->from);
    if (from != 0)
    {
        return from;
    }
    const int to = EMBED_CALL(edits_place_order, ServersEditsCtx, .one = left->to, .other = right->to);
    if (to != 0)
    {
        return to;
    }
    return (left->order < right->order) ? -1 : 1;
}

// the room the edits of `edits` for the file of `uri`, `length` bytes, take counted with that before: one file, and in
// a file given its server as a slice a file more for each edit, as each may stand on a line of another file
EMBED_INLINE void edits_room_count(const ServersEditsCtx *args)
{
    const char *uri = args->uri;
    unsigned long long length = args->length;
    const unsigned int before = args->editing->edit_room;
    args->editing->edit_room = EMBED_CALL(edits_on_count, ServersEditsCtx, .json = args->json, .edits = args->edits,
                                          .before = args->editing->edit_room);
    char path[CLI_PATH_MAX_LEN];
    const int sliced =
        EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri, .length = length, .path_out = path) &&
        EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path);
    const unsigned int files = args->editing->file_room + 1u + (sliced ? (args->editing->edit_room - before) : 0u);
    args->editing->file_room = (files < CLI_SERVERS_EDITED) ? files : CLI_SERVERS_EDITED;
}

// each file's edits of a workspace edit put, from its document changes or its changes, into `editing`; where
// `editing` has no room, they are only counted, its rooms the counts after
static void edits_walk(const ServersEditsCtx *args)
{
    const CliJson *json = args->json;
    const int counting = (args->editing->edit == NULL);
    const unsigned int document_changes =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = args->edit,
                   .name = "documentChanges");
    const unsigned int changes = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = args->edit, .name = "changes");
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = document_changes,
                   .kind = CFG_JSON_ARRAY))
    {
        for (unsigned int change = json->node[document_changes].first; change != CLI_JSON_NONE;
             change = json->node[change].next)
        {
            unsigned long long length = 0ull;
            const char *const uri = EMBED_CALL(
                cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                   .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                        .json_read = json, .object = change, .name = "textDocument"),
                                   .name = "uri"),
                .length_pointer = &length);
            const unsigned int edits = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = json, .object = change, .name = "edits");
            if ((uri != NULL) && counting)
            {
                EMBED_CALL(edits_room_count, ServersEditsCtx, .editing = args->editing, .json = json, .uri = uri,
                           .length = length, .edits = edits);
            }
            else if (uri != NULL)
            {
                EMBED_CALL(edits_add, ServersEditsCtx, .editing = args->editing, .json = json, .uri = uri,
                           .length = length, .edits = edits);
            }
        }
    }
    else if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = changes,
                        .kind = CFG_JSON_OBJECT))
    {
        for (unsigned int change = json->node[changes].first; change != CLI_JSON_NONE; change = json->node[change].next)
        {
            const char *const uri = json->text + json->node[change].key;
            if (counting)
            {
                EMBED_CALL(edits_room_count, ServersEditsCtx, .editing = args->editing, .json = json, .uri = uri,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}),
                           .edits = change);
                continue;
            }
            EMBED_CALL(edits_add, ServersEditsCtx, .editing = args->editing, .json = json, .uri = uri,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), .edits = change);
        }
    }
}

void cli_servers_workspace_edit_get(const CliServersJsonTreeOutCtx *args)
{
    unsigned int edit = args->edit;
    CliServersEditing editing = {NULL, 0u, 0u, NULL, 0u, 0u};
    EMBED_CALL(edits_walk, ServersEditsCtx, .editing = &editing, .json = args->json, .edit = edit);
    // the edits, on a boundary of their own, and the paths after them
    const unsigned long long edit_bytes = (unsigned long long)editing.edit_room * sizeof(CliServersEdit);
    char *const cell = g_cli_cells->temporary_buf_alloc(
        edit_bytes + ((unsigned long long)editing.file_room * CLI_PATH_MAX_LEN) + 1ull, &g_cli_servers_edits_cells);
    if (cell != NULL)
    {
        editing.edit = (CliServersEdit *)(void *)cell;
        editing.edited = (char (*)[CLI_PATH_MAX_LEN])(void *)(cell + edit_bytes);
        EMBED_CALL(edits_walk, ServersEditsCtx, .editing = &editing, .json = args->json, .edit = edit);
        qsort(editing.edit, editing.edits, sizeof(editing.edit[0]), edits_order);
    }
    mmgr_line_ch(args->out, '[');
    unsigned int at = 0u;
    for (unsigned int file = 0u; file < editing.files; file += 1u)
    {
        mmgr_line_put(args->out, (file > 0u) ? ",{\"path\":" : "{\"path\":");
        EMBED_CALL(cli_servers_path_err_msg_internal.path_write, CliServersPathErrMsgCtx, .tree = args->tree,
                   .path = editing.edited[file], .out = args->out);
        mmgr_line_put(args->out, ",\"edits\":[");
        for (unsigned int first = at; (at < editing.edits) && (editing.edit[at].file == file); at += 1u)
        {
            unsigned long long size = 0ull;
            const char *const text =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = editing.edit[at].text, .length_pointer = &size);
            const CliTextEdit one = {editing.edit[at].from, editing.edit[at].to, (text != NULL) ? text : "", size};
            mmgr_line_put(args->out, (at > first) ? "," : "");
            EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &one, .out = args->out);
        }
        mmgr_line_put(args->out, "]}");
    }
    mmgr_line_ch(args->out, ']');
    g_cli_cells->persistent_buf_release(cell, &g_cli_servers_edits_cells);
}
