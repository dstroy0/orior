// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_edits.c: a server's workspace edit written as each file's edits, in a cell counted to the edit's files and
// edits before they are read
#include "../command_line/cells.h"
#include "servers_internal.h"

#include <stdlib.h>

// the most files one workspace edit changes
#define CLI_SERVERS_EDITED 1024u

CliCellsTally g_cli_servers_edits_cells = CLI_CELLS_TALLY("cli_servers_edits");

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
    char (*edited)[CLI_PATH_LONGEST];
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

// the edits of `edits` for the file of `uri`, `length` bytes, put after those kept
static void edits_added(CliServersEditing *editing, const CliJson *json, const char *uri, unsigned long long length,
                        unsigned int edits)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_lsp_path_of(uri, length, path))
    {
        return;
    }
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
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = editing->edited[file],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        editing->files += 1u;
    }
    for (unsigned int one = cli_json_is(json, edits, CFG_JSON_ARRAY) ? json->node[edits].first : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (editing->edits < editing->edit_room); one = json->node[one].next)
    {
        const unsigned int range = cli_json_member(json, one, "range");
        editing->edit[editing->edits] =
            (CliServersEdit){file, cli_servers_place(json, cli_json_member(json, range, "start")),
                             cli_servers_place(json, cli_json_member(json, range, "end")),
                             cli_json_member(json, one, "newText"), editing->edits};
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

// each file's edits of a workspace edit put, from its document changes or its changes, into `editing`; where
// `editing` holds no room, they are only counted, its rooms the counts after
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
                editing->edit_room = edits_counted(json, edits, editing->edit_room);
                editing->file_room += (editing->file_room < CLI_SERVERS_EDITED) ? 1u : 0u;
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
                editing->edit_room = edits_counted(json, change, editing->edit_room);
                editing->file_room += (editing->file_room < CLI_SERVERS_EDITED) ? 1u : 0u;
                continue;
            }
            edits_added(editing, json, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), change);
        }
    }
}

void cli_servers_workspace_edit_of(const CliJson *json, unsigned int edit, const char *tree, CliLine *out)
{
    CliServersEditing editing = {NULL, 0u, 0u, NULL, 0u, 0u};
    edits_walked(&editing, json, edit);
    // the edits, on a boundary of their own, and the paths after them
    const unsigned long long edit_bytes = (unsigned long long)editing.edit_room * sizeof(CliServersEdit);
    char *const cell = cli_cell_taken_by(&g_cli_servers_edits_cells,
                                         edit_bytes + ((unsigned long long)editing.file_room * CLI_PATH_LONGEST) + 1ull,
                                         "a workspace edit's edits and files");
    if (cell != NULL)
    {
        editing.edit = (CliServersEdit *)(void *)cell;
        editing.edited = (char (*)[CLI_PATH_LONGEST])(void *)(cell + edit_bytes);
        edits_walked(&editing, json, edit);
        qsort(editing.edit, editing.edits, sizeof(editing.edit[0]), edits_order);
    }
    cli_line_character(out, '[');
    unsigned int at = 0u;
    for (unsigned int file = 0u; file < editing.files; file += 1u)
    {
        cli_line_text(out, (file > 0u) ? ",{\"path\":" : "{\"path\":");
        cli_servers_path_written(tree, editing.edited[file], out);
        cli_line_text(out, ",\"edits\":[");
        for (unsigned int first = at; (at < editing.edits) && (editing.edit[at].file == file); at += 1u)
        {
            unsigned long long size = 0ull;
            const char *const text = cli_json_string(json, editing.edit[at].text, &size);
            const CliTextEdit one = {editing.edit[at].from, editing.edit[at].to, (text != NULL) ? text : "", size};
            cli_line_text(out, (at > first) ? "," : "");
            cli_text_edit_written(&one, out);
        }
        cli_line_text(out, "]}");
    }
    cli_line_character(out, ']');
    cli_cell_given_by(&g_cli_servers_edits_cells, cell);
}
