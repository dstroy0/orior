// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_edits.c: a server's workspace edit written as each file's edits, in a cell counted to the edit's files and
// edits before they are read. An edit of a file given its server as a slice is an edit of the file its lines came from,
// and one on a bracket the slice closes is none
#include "../command_line/cells.h"
#include "servers_internal.h"

#include <stdlib.h>

// the most files one workspace edit changes
#define CLI_SERVERS_EDITED 1024u

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

// the edits of an array, CLI_TEXT_EDITS at the most counted with those before
static unsigned int edits_counted(const CliJson *json, unsigned int edits, unsigned int before)
{
    unsigned int count = before;
    for (unsigned int one = cli_json_is(json, edits, CFG_JSON_ARRAY) ? json->node[edits].first : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (count < CLI_TEXT_EDITS); one = json->node[one].next)
    {
        count += 1u;
    }
    return count;
}

// the place of the file at `path` among those edited, put after them where it is not among them: `file_room` where no
// room for it is left
static unsigned int edits_file(CliServersEditing *editing, const char *path)
{
    unsigned int file = 0u;
    while ((file < editing->files) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = editing->edited[file], .other = path, .cap = CLI_CELLS_POOL})))
    {
        file += 1u;
    }
    if (file == editing->files)
    {
        if (editing->files == editing->file_room)
        {
            return editing->file_room;
        }
        memor.cpy(&(MemoriaCfg){.dst = editing->edited[file],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        editing->files += 1u;
    }
    return file;
}

// the edits of `edits` for the file of `uri`, `length` bytes, put after those stored
static void edits_added(CliServersEditing *editing, const CliJson *json, const char *uri, unsigned long long length,
                        unsigned int edits)
{
    char path[CLI_PATH_MAX_LEN];
    if (!cli_lsp_path_of(uri, length, path))
    {
        return;
    }
    const unsigned int file = edits_file(editing, path);
    if (file == editing->file_room)
    {
        return;
    }
    for (unsigned int one = cli_json_is(json, edits, CFG_JSON_ARRAY) ? json->node[edits].first : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (editing->edits < editing->edit_room); one = json->node[one].next)
    {
        const unsigned int range = cli_json_member(json, one, "range");
        CliTextPlace from = cli_servers_place(json, cli_json_member(json, range, "start"));
        CliTextPlace to = cli_servers_place(json, cli_json_member(json, range, "end"));
        // an edit of a slice is an edit of the file its lines came from, a span's lines standing together in both
        char moved[CLI_PATH_MAX_LEN];
        const unsigned int rows = (to.line > from.line) ? (to.line - from.line) : 0u;
        const int where = cli_servers_slice_out(path, &from.line, moved);
        to.line = from.line + rows;
        const unsigned int target = (where == 2) ? edits_file(editing, moved) : file;
        if ((where == 0) || (target == editing->file_room))
        {
            continue;
        }
        editing->edit[editing->edits] =
            (CliServersEdit){target, from, to, cli_json_member(json, one, "newText"), editing->edits};
        editing->edits += 1u;
    }
}

// two places in the order they stand in a text
static int edits_place_order(CliTextPlace one, CliTextPlace other)
{
    if (one.line != other.line)
    {
        return (one.line < other.line) ? -1 : 1;
    }
    return (one.col < other.col) ? -1 : (one.col > other.col) ? 1 : 0;
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
    const int from = edits_place_order(left->from, right->from);
    if (from != 0)
    {
        return from;
    }
    const int to = edits_place_order(left->to, right->to);
    if (to != 0)
    {
        return to;
    }
    return (left->order < right->order) ? -1 : 1;
}

// the room the edits of `edits` for the file of `uri`, `length` bytes, take counted with that before: one file, and in
// a file given its server as a slice a file more for each edit, as each may stand on a line of another file
static void edits_room_counted(CliServersEditing *editing, const CliJson *json, const char *uri,
                               unsigned long long length, unsigned int edits)
{
    const unsigned int before = editing->edit_room;
    editing->edit_room = edits_counted(json, edits, editing->edit_room);
    char path[CLI_PATH_MAX_LEN];
    const int sliced = cli_lsp_path_of(uri, length, path) && cli_servers_sliced(path);
    const unsigned int files = editing->file_room + 1u + (sliced ? (editing->edit_room - before) : 0u);
    editing->file_room = (files < CLI_SERVERS_EDITED) ? files : CLI_SERVERS_EDITED;
}

// each file's edits of a workspace edit put, from its document changes or its changes, into `editing`; where
// `editing` has no room, they are only counted, its rooms the counts after
static void edits_walked(CliServersEditing *editing, const CliJson *json, unsigned int edit)
{
    const int counting = (editing->edit == NULL);
    const unsigned int document_changes = cli_json_member(json, edit, "documentChanges");
    const unsigned int changes = cli_json_member(json, edit, "changes");
    if (cli_json_is(json, document_changes, CFG_JSON_ARRAY))
    {
        for (unsigned int change = json->node[document_changes].first; change != CLI_JSON_NONE;
             change = json->node[change].next)
        {
            unsigned long long length = 0ull;
            const char *const uri = cli_json_string(
                json, cli_json_member(json, cli_json_member(json, change, "textDocument"), "uri"), &length);
            const unsigned int edits = cli_json_member(json, change, "edits");
            if ((uri != NULL) && counting)
            {
                edits_room_counted(editing, json, uri, length, edits);
            }
            else if (uri != NULL)
            {
                edits_added(editing, json, uri, length, edits);
            }
        }
    }
    else if (cli_json_is(json, changes, CFG_JSON_OBJECT))
    {
        for (unsigned int change = json->node[changes].first; change != CLI_JSON_NONE; change = json->node[change].next)
        {
            const char *const uri = json->text + json->node[change].key;
            if (counting)
            {
                edits_room_counted(editing, json, uri,
                                   cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), change);
                continue;
            }
            edits_added(editing, json, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), change);
        }
    }
}

void cli_servers_workspace_edit_of(const CliJson *json, unsigned int edit, const char *tree, VerbaLinea *out)
{
    CliServersEditing editing = {NULL, 0u, 0u, NULL, 0u, 0u};
    edits_walked(&editing, json, edit);
    // the edits, on a boundary of their own, and the paths after them
    const unsigned long long edit_bytes = (unsigned long long)editing.edit_room * sizeof(CliServersEdit);
    char *const cell = g_cli_cells->temporary_buf_alloc(
        edit_bytes + ((unsigned long long)editing.file_room * CLI_PATH_MAX_LEN) + 1ull, &g_cli_servers_edits_cells);
    if (cell != NULL)
    {
        editing.edit = (CliServersEdit *)(void *)cell;
        editing.edited = (char (*)[CLI_PATH_MAX_LEN])(void *)(cell + edit_bytes);
        edits_walked(&editing, json, edit);
        qsort(editing.edit, editing.edits, sizeof(editing.edit[0]), edits_order);
    }
    mmgr_line_ch(out, '[');
    unsigned int at = 0u;
    for (unsigned int file = 0u; file < editing.files; file += 1u)
    {
        mmgr_line_put(out, (file > 0u) ? ",{\"path\":" : "{\"path\":");
        cli_servers_path_written(tree, editing.edited[file], out);
        mmgr_line_put(out, ",\"edits\":[");
        for (unsigned int first = at; (at < editing.edits) && (editing.edit[at].file == file); at += 1u)
        {
            unsigned long long size = 0ull;
            const char *const text = cli_json_string(json, editing.edit[at].text, &size);
            const CliTextEdit one = {editing.edit[at].from, editing.edit[at].to, (text != NULL) ? text : "", size};
            mmgr_line_put(out, (at > first) ? "," : "");
            cli_text_edit_written(&one, out);
        }
        mmgr_line_put(out, "]}");
    }
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(cell, &g_cli_servers_edits_cells);
}
