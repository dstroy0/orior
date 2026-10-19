// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_text.c: the window's calls on texts: a file of the tree held for the page's slow work on it, whole or
// as its slices give it, or a text the page gives a part at a time; its changes written as the page makes them; its
// gutter's marks against the last commit; find's matches in it; two texts compared by structure; a grammar kept and
// lines colored with it; and a file anywhere on the machine read or written as the reader chose it
#include "../code/structure.h"
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../editor/document.h"
#include "../editor/highlight.h"
#include "window_internal.h"

// the most changes and edits one writing holds, and the most lines colored at once
#define CLI_WINDOW_CHANGES 4096u
#define CLI_WINDOW_EDITS 65536u
#define CLI_WINDOW_LINES 65536u

// A call's room, a cell taken with the lock and given back with it: the tree's answer and the tree it is read into,
// one writing's edits and changes, the lines colored, and a workspace file made plain
typedef struct
{
    char answer[16u << 20u];
    CliJsonNode node[1u << 16u];
    char tree_text[16u << 20u];
    CfgJsonToken token[1u << 16u];
    CliJson json;
    CliEdit edit[CLI_WINDOW_EDITS];
    const CliEdit *change[CLI_WINDOW_CHANGES];
    unsigned int edits[CLI_WINDOW_CHANGES];
    const char *lines[CLI_WINDOW_LINES];
    char plain[16u << 20u];
} CliTextRoom;

static CliLock s_lock = CLI_LOCK_INIT;
static CliTextRoom *s_room;

// the lock taken and the room a cell under it: 1, or 0 with why said and the lock given back
static int text_held(CliLine *said)
{
    cli_lock_take(&s_lock);
    s_room = cli_cell_taken(sizeof(*s_room), "a call on a text");
    if (s_room == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a call on a text could not be taken");
    }
    return s_room != NULL;
}

// the room given back, and the lock with it
static void text_released(void)
{
    cli_cell_given(s_room);
    s_room = NULL;
    cli_lock_give(&s_lock);
}

// a text's shape written as the page reads it
static void text_shape_written(const CliShape *shape, CliLine *out)
{
    cli_line_text(out, "{\"lines\":");
    cli_line_decimal(out, shape->lines, 1u);
    cli_line_text(out, ",\"units\":");
    cli_line_decimal(out, shape->units, 1u);
    cli_line_character(out, '}');
}

// the tree's call `name` with `{"path": path}` and `more` after it, its answer read into the tree, under the lock: its
// node, or CLI_JSON_NONE with why said
static unsigned int text_tree_called(const char *name, const char *path, const char *more, CliLine *said)
{
    char args[CLI_PATH_LONGEST * 2u];
    CliLine line = {args, sizeof(args), 0ull};
    cli_line_text(&line, "{\"path\":");
    cli_window_quoted(&line, path);
    cli_line_text(&line, more);
    cli_line_character(&line, '}');
    CliLine answer = {s_room->answer, sizeof(s_room->answer), 0ull};
    if (!cli_line_fits(&line) || !cli_window_tree_call(name, args, line.at, &answer, said))
    {
        return CLI_JSON_NONE;
    }
    cli_json_begun(&s_room->json, s_room->node, sizeof(s_room->node) / sizeof(s_room->node[0]), s_room->tree_text,
                   sizeof(s_room->tree_text));
    return cli_json_read(&s_room->json, s_room->answer, cli_line_fits(&answer) ? answer.at : 0ull, s_room->token,
                         sizeof(s_room->token) / sizeof(s_room->token[0]), said);
}

// the file at `path` read from the tree as the page read it, whole or by its slices, held under `key`: its shape, or
// null where the file is not text
static int text_document_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", said) : NULL;
    if ((path == NULL) || !text_held(said))
    {
        return 0;
    }
    unsigned int opened = text_tree_called("file_read", path, "", said);
    int done = (opened != CLI_JSON_NONE);
    unsigned long long length = 0ull;
    const char *text =
        done ? cli_json_string(&s_room->json, cli_json_member(&s_room->json, opened, "text"), &length) : NULL;
    if (done && (text == NULL) &&
        cli_json_is(&s_room->json, cli_json_member(&s_room->json, opened, "windowed"), CFG_JSON_TRUE))
    {
        const unsigned int size_node = cli_json_member(&s_room->json, opened, "size");
        char more[128];
        CliLine slice = {more, sizeof(more), 0ull};
        cli_line_text(&slice, ",\"start\":0,\"end\":");
        if (cli_json_is(&s_room->json, size_node, CFG_JSON_NUMBER))
        {
            cli_line_counted(&slice, s_room->json.text + s_room->json.node[size_node].text,
                             s_room->json.node[size_node].length);
        }
        else
        {
            cli_line_character(&slice, '0');
        }
        (void)cli_line_closed(&slice);
        opened = text_tree_called("file_slice", path, more, said);
        done = (opened != CLI_JSON_NONE);
        text = done ? cli_json_string(&s_room->json, cli_json_member(&s_room->json, opened, "text"), &length) : NULL;
    }
    CliShape shape;
    if (done && (text != NULL) && cli_document_hold(key, text, length, &shape))
    {
        text_shape_written(&shape, out);
    }
    else if (done)
    {
        cli_line_text(out, "null");
    }
    text_released();
    return done;
}

// a text the page gives a part at a time held under `key`: its shape once the last comes, else null
static int text_document_part(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    unsigned long long length = 0ull;
    const char *const part = (key != NULL) ? cli_json_string(args->json, cli_serve_arg(args, "part"), &length) : NULL;
    int first = 0;
    int last = 0;
    if ((part == NULL) && (key != NULL))
    {
        (void)cli_serve_string(args, "part", said);
    }
    if ((part == NULL) || !cli_serve_bool(args, "first", &first, said) || !cli_serve_bool(args, "done", &last, said))
    {
        return 0;
    }
    CliShape shape;
    if (cli_document_hold_part(key, part, length, first, last, &shape))
    {
        text_shape_written(&shape, out);
    }
    else
    {
        cli_line_text(out, "null");
    }
    return 1;
}

// a place argument `{line, col}` of an edit read
static CliPlace text_place(const CliJson *json, unsigned int edit, const char *name)
{
    const unsigned int place = cli_json_member(json, edit, name);
    CliPlace read = {0ull, 0ull};
    const char *const keys[2] = {"line", "col"};
    unsigned long long *const into[2] = {&read.line, &read.col};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const unsigned int number = cli_json_member(json, place, keys[at]);
        if (cli_json_is(json, number, CFG_JSON_NUMBER))
        {
            for (unsigned long long digit = 0ull; digit < json->node[number].length; digit += 1ull)
            {
                const char letter = json->text[json->node[number].text + digit];
                if ((letter < '0') || (letter > '9'))
                {
                    break;
                }
                *into[at] = (*into[at] * 10ull) + (unsigned long long)(letter - '0');
            }
        }
    }
    return read;
}

// the changes the page made to the text held under `key` written, in order: its shape
static int text_document_edit(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    const unsigned int changes = cli_serve_arg(args, "changes");
    if (key == NULL)
    {
        return 0;
    }
    if (!cli_json_is(args->json, changes, CFG_JSON_ARRAY))
    {
        cli_serve_refused(args, changes, "changes", "a sequence", said);
        return 0;
    }
    if (!text_held(said))
    {
        return 0;
    }
    unsigned int count = 0u;
    unsigned int used = 0u;
    for (unsigned int change = args->json->node[changes].first;
         (change != CLI_JSON_NONE) && (count < CLI_WINDOW_CHANGES); change = args->json->node[change].next)
    {
        s_room->change[count] = &s_room->edit[used];
        s_room->edits[count] = 0u;
        const unsigned int first =
            cli_json_is(args->json, change, CFG_JSON_ARRAY) ? args->json->node[change].first : CLI_JSON_NONE;
        for (unsigned int edit = first; (edit != CLI_JSON_NONE) && (used < CLI_WINDOW_EDITS);
             edit = args->json->node[edit].next)
        {
            unsigned long long length = 0ull;
            const char *const text = cli_json_string(args->json, cli_json_member(args->json, edit, "text"), &length);
            s_room->edit[used].from = text_place(args->json, edit, "from");
            s_room->edit[used].to = text_place(args->json, edit, "to");
            s_room->edit[used].text = (text != NULL) ? text : "";
            s_room->edit[used].length = (text != NULL) ? length : 0ull;
            used += 1u;
            s_room->edits[count] += 1u;
        }
        count += 1u;
    }
    CliShape shape;
    const int edited = cli_document_edit(key, s_room->change, s_room->edits, count, &shape, said);
    if (edited)
    {
        text_shape_written(&shape, out);
    }
    text_released();
    return edited;
}

// the text held under `key` let go
static int text_document_release(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char *const key = cli_serve_string(args, "key", said);
    if (key == NULL)
    {
        return 0;
    }
    cli_document_release(key);
    return 1;
}

// the gutter's marks for the text held under `key` against the file at `path` as the last commit holds it, read from
// the tree where it is not kept beside the text yet or `fresh` asks; null where no text is held under `key`
static int text_document_changes(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    const char *const path = (key != NULL) ? cli_serve_string(args, "path", said) : NULL;
    int fresh = 0;
    if ((path == NULL) || !cli_serve_bool(args, "fresh", &fresh, said))
    {
        return 0;
    }
    const unsigned long long start = out->at;
    if (!fresh && cli_document_has_head(key))
    {
        if (!cli_document_marks(key, 0, NULL, 0ull, out))
        {
            out->at = start;
            cli_line_text(out, "null");
        }
        return 1;
    }
    if (!text_held(said))
    {
        return 0;
    }
    const unsigned int head = text_tree_called("file_head", path, "", said);
    unsigned long long length = 0ull;
    const char *const text = (head != CLI_JSON_NONE) ? cli_json_string(&s_room->json, head, &length) : NULL;
    const int read = (head != CLI_JSON_NONE);
    if (read && !cli_document_marks(key, 1, text, length, out))
    {
        out->at = start;
        cli_line_text(out, "null");
    }
    text_released();
    return read;
}

// find's matches in the text held under `key`, each [line, from col, to col], at most `most`
static int text_document_find(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    const unsigned int sought = cli_serve_arg(args, "sought");
    unsigned long long most = 0ull;
    if ((key == NULL) || !cli_serve_whole(args, "most", &most, said))
    {
        return 0;
    }
    const char *const query = cli_json_string(args->json, cli_json_member(args->json, sought, "query"), NULL);
    if (query == NULL)
    {
        cli_serve_refused(args, sought, "sought", "struct Sought", said);
        return 0;
    }
    const CliDocSought read = {query,
                               cli_json_is(args->json, cli_json_member(args->json, sought, "regex"), CFG_JSON_TRUE),
                               cli_json_is(args->json, cli_json_member(args->json, sought, "case"), CFG_JSON_TRUE),
                               cli_json_is(args->json, cli_json_member(args->json, sought, "word"), CFG_JSON_TRUE),
                               cli_json_is(args->json, cli_json_member(args->json, sought, "unicode"), CFG_JSON_TRUE)};
    return cli_document_find(key, &read, most, out, said);
}

// two texts compared by structure, the older first, or null where they are too far apart
static int text_structure_compare(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const then = cli_serve_string_or(args, "then", "");
    const char *const now = cli_serve_string(args, "now", said);
    if (now == NULL)
    {
        return 0;
    }
    cli_structure_compare(then, cellul.len(&(CatenaFinitaCfg){.src = then, .cap = CLI_CELLS_POOL}), now,
                          cellul.len(&(CatenaFinitaCfg){.src = now, .cap = CLI_CELLS_POOL}), out);
    return 1;
}

// a grammar of the page's kept under `key`
static int text_highlight_grammar(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char *const key = cli_serve_string(args, "key", said);
    return (key != NULL) && cli_highlight_keep(key, args->json, cli_serve_arg(args, "def"), said);
}

// lines colored with the grammar kept under `key`, the first starting in `state`
static int text_highlight_lines(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned int count = 0u;
    const char *const key = cli_serve_string(args, "key", said);
    const char *const state = (key != NULL) ? cli_serve_string(args, "state", said) : NULL;
    if ((state == NULL) || !text_held(said))
    {
        return 0;
    }
    const int colored = cli_serve_strings(args, "lines", s_room->lines, CLI_WINDOW_LINES, &count, said) &&
                        cli_highlight_color(key, state, s_room->lines, count, out, said);
    text_released();
    return colored;
}

// the text of the file at `path`, anywhere on the machine, as the reader chose it
static int text_file_read_any(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !text_held(said))
    {
        return 0;
    }
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_room->answer, sizeof(s_room->answer));
    const int read = (size != QRY_MEMBER_UNREAD) && cli_utf8_whole(s_room->answer, size);
    if (read)
    {
        cli_json_quoted(out, s_room->answer, size);
    }
    text_released();
    if (!read)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
    }
    return read;
}

// `text` written to the file at `path`, anywhere on the machine, the folders it stands in made where they are not
static int text_file_write_any(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char *const path = cli_serve_string(args, "path", said);
    unsigned long long length = 0ull;
    const char *const text = (path != NULL) ? cli_json_string(args->json, cli_serve_arg(args, "text"), &length) : NULL;
    if ((text == NULL) && (path != NULL))
    {
        (void)cli_serve_string(args, "text", said);
    }
    if (text == NULL)
    {
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    if (cli_path_parent(path, folder) && (folder[0] != '\0') && !query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    if (!query_file_written(path, text, length))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

// the folders a .code-workspace file names, each made whole against the file's own folder, as JSON with comments and
// trailing commas reads them
static int text_workspace_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !text_held(said))
    {
        return 0;
    }
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_room->answer, sizeof(s_room->answer));
    if (size == QRY_MEMBER_UNREAD)
    {
        text_released();
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    CliLine plain = {s_room->plain, sizeof(s_room->plain), 0ull};
    cli_plain_json(s_room->answer, size, &plain);
    cli_json_begun(&s_room->json, s_room->node, sizeof(s_room->node) / sizeof(s_room->node[0]), s_room->tree_text,
                   sizeof(s_room->tree_text));
    char why[1024];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int value = cli_json_read(&s_room->json, s_room->plain, cli_line_fits(&plain) ? plain.at : 0ull,
                                             s_room->token, sizeof(s_room->token) / sizeof(s_room->token[0]), &unread);
    if (value == CLI_JSON_NONE)
    {
        text_released();
        cli_line_text(said, path);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, cli_line_fits(&unread) ? unread.at : 0ull);
        return 0;
    }
    char base[CLI_PATH_LONGEST] = "";
    (void)cli_path_parent(path, base);
    const unsigned int folders = cli_json_member(&s_room->json, value, "folders");
    const unsigned int first =
        cli_json_is(&s_room->json, folders, CFG_JSON_ARRAY) ? s_room->json.node[folders].first : CLI_JSON_NONE;
    cli_line_character(out, '[');
    int any = 0;
    for (unsigned int folder = first; folder != CLI_JSON_NONE; folder = s_room->json.node[folder].next)
    {
        const char *const named = cli_json_string(&s_room->json, cli_json_member(&s_room->json, folder, "path"), NULL);
        if (named == NULL)
        {
            continue;
        }
        const int whole = (named[0] == '/') || (named[0] == '\\') ||
                          ((named[0] != '\0') && (named[1] == ':') && ((named[2] == '/') || (named[2] == '\\')));
        char joined[CLI_PATH_LONGEST];
        char full[CLI_PATH_LONGEST];
        if (whole)
        {
            memor.cpy(&(MemoriaCfg){.dst = joined,
                                    .src = named,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
        }
        else
        {
            cli_full(base, named, joined);
        }
        cli_line_text(out, any ? "," : "");
        cli_window_quoted(out, query_path_full(joined, full, sizeof(full)) ? full : joined);
        any = 1;
    }
    cli_line_character(out, ']');
    text_released();
    return 1;
}

const CliServeCall g_cli_window_text_calls[] = {
    {"document_read", text_document_read},         {"document_part", text_document_part},
    {"document_edit", text_document_edit},         {"document_release", text_document_release},
    {"document_changes", text_document_changes},   {"document_find", text_document_find},
    {"structure_compare", text_structure_compare}, {"highlight_grammar", text_highlight_grammar},
    {"highlight_lines", text_highlight_lines},     {"file_read_any", text_file_read_any},
    {"file_write_any", text_file_write_any},       {"workspace_read", text_workspace_read},
};
const unsigned int g_cli_window_text_count = sizeof(g_cli_window_text_calls) / sizeof(g_cli_window_text_calls[0]);
