// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_page.c: the calls of the page's own kinds of file and of the machine's containers: a file too large for
// the page held here as the file and the lines written over it, opened, shaped, read, edited, changed, found in, saved
// and let go; a file of HTTP requests parsed, its environments read, a request sent, its answers kept, written as curl
// and read from curl, and its private environment made; a notebook read, written, written for a diff, merged from a
// merge's stages and written without its outputs; and the containers and pods, their logs, and a container acted on
// or a line run in it
#include "../code/git.h"
#include "../command_line/cells.h"
#include "../deploy/containers.h"
#include "../editor/document.h"
#include "../editor/pages.h"
#include "../notebooks/notebook.h"
#include "../requests/requests.h"
#include "serve_internal.h"

// the most changes and edits one edit of a held text makes, and the most bytes of a notebook
#define CLI_SERVE_CHANGES 4096u
#define CLI_SERVE_PAGE_EDITS 65536u
#define CLI_SERVE_NOTEBOOK (32ull << 20u)

// the edits of the changes given, each change's first edit and its count
static CliEdit s_edit[CLI_SERVE_PAGE_EDITS];
static const CliEdit *s_change[CLI_SERVE_CHANGES];
static unsigned int s_edits[CLI_SERVE_CHANGES];
static CliLock s_edit_lock = CLI_LOCK_INIT;

// a place `{line, col}` of the page's read into `place`: 1, or 0 where it is none
static int page_place(const CliJson *json, unsigned int node, CliPlace *place)
{
    const unsigned long long none = ~0ull;
    place->line = cli_servers_whole(json, cli_json_member(json, node, "line"), none);
    place->col = cli_servers_whole(json, cli_json_member(json, node, "col"), none);
    return (place->line != none) && (place->col != none);
}

static int pages_open(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const key = cli_serve_string(args, "key", said);
    return (key != NULL) && cli_serve_at(args, root, full, said) && cli_pages_open(key, full, out, said);
}

static int pages_shape(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    return (key != NULL) && cli_pages_shape(key, out, said);
}

static int pages_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    const char *const key = cli_serve_string(args, "key", said);
    return (key != NULL) && cli_serve_whole(args, "from", &from, said) && cli_serve_whole(args, "to", &to, said) &&
           cli_pages_read(key, from, to, out, said);
}

static int pages_edit(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    if (key == NULL)
    {
        return 0;
    }
    const CliJson *const json = args->json;
    const unsigned int changes = cli_serve_arg(args, "changes");
    if (!cli_json_is(json, changes, CFG_JSON_ARRAY))
    {
        cli_line_text(said, "changes: invalid type: null, expected a sequence");
        return 0;
    }
    cli_lock_take(&s_edit_lock);
    unsigned int count = 0u;
    unsigned int used = 0u;
    const char *missing = NULL;
    for (unsigned int change = json->node[changes].first;
         (missing == NULL) && (change != CLI_JSON_NONE) && (count < CLI_SERVE_CHANGES);
         change = json->node[change].next)
    {
        s_change[count] = &s_edit[used];
        s_edits[count] = 0u;
        for (unsigned int edit = cli_json_is(json, change, CFG_JSON_ARRAY) ? json->node[change].first : CLI_JSON_NONE;
             (missing == NULL) && (edit != CLI_JSON_NONE) && (used < CLI_SERVE_PAGE_EDITS);
             edit = json->node[edit].next)
        {
            CliEdit *const one = &s_edit[used];
            one->text = cli_json_string(json, cli_json_member(json, edit, "text"), &one->length);
            missing = !page_place(json, cli_json_member(json, edit, "from"), &one->from) ? "from"
                      : !page_place(json, cli_json_member(json, edit, "to"), &one->to)   ? "to"
                      : (one->text == NULL)                                              ? "text"
                                                                                         : NULL;
            used += 1u;
            s_edits[count] += 1u;
        }
        count += 1u;
    }
    const int whole = (missing == NULL);
    if (!whole)
    {
        cli_line_text(said, "changes: missing field `");
        cli_line_text(said, missing);
        cli_line_character(said, '`');
    }
    const int done = whole && cli_pages_edit(key, s_change, s_edits, count, out, said);
    cli_lock_give(&s_edit_lock);
    return done;
}

static int pages_changed(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    return (key != NULL) && cli_pages_changed(key, out, said);
}

static int pages_find(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long most = 0ull;
    const char *const key = cli_serve_string(args, "key", said);
    const unsigned int node = cli_serve_arg(args, "sought");
    const CliServeArgs sought_args = {args->json, node};
    int regex = 0;
    int match_case = 0;
    int word = 0;
    int unicode = 0;
    const char *const query =
        cli_json_is(args->json, node, CFG_JSON_OBJECT) ? cli_serve_string(&sought_args, "query", said) : NULL;
    if ((key == NULL) || (query == NULL) || !cli_serve_bool(&sought_args, "regex", &regex, said) ||
        !cli_serve_bool(&sought_args, "case", &match_case, said) ||
        !cli_serve_bool(&sought_args, "word", &word, said) ||
        !cli_serve_bool(&sought_args, "unicode", &unicode, said) || !cli_serve_whole(args, "most", &most, said))
    {
        if ((key != NULL) && !cli_json_is(args->json, node, CFG_JSON_OBJECT))
        {
            cli_line_text(said, "sought: invalid type: null, expected struct Sought");
        }
        return 0;
    }
    const CliDocSought sought = {query, regex, match_case, word, unicode};
    return cli_pages_find(key, &sought, most, out, said);
}

static int pages_save(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    return (key != NULL) && cli_pages_save(key, out, said);
}

static int pages_release(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    if (key == NULL)
    {
        return 0;
    }
    cli_pages_release(key);
    cli_line_text(out, "null");
    return 1;
}

static int requests_parse(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long size = 0ull;
    const char *const text = cli_serve_string(args, "text", said);
    if (text == NULL)
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &size);
    return cli_requests_parsed(text, size, out, said);
}

static int requests_environments(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    return (path != NULL) && cli_serve_tree(root, said) && cli_requests_environments(root, path, out, said);
}

// the arguments `path`, `text` and `line` of a request, and the tree: 1, or 0 with why said
static int requests_at(const CliServeArgs *args, char *root, const char **path, const char **text,
                       unsigned long long *size, unsigned long long *line, CliLine *said)
{
    *path = cli_serve_string(args, "path", said);
    *text = (*path != NULL) ? cli_serve_string(args, "text", said) : NULL;
    if ((*text == NULL) || !cli_serve_whole(args, "line", line, said) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), size);
    return 1;
}

static int requests_send(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *path = NULL;
    const char *text = NULL;
    unsigned long long size = 0ull;
    unsigned long long line = 0ull;
    return requests_at(args, root, &path, &text, &size, &line, said) &&
           cli_requests_sent_at(root, path, text, size, line, cli_serve_string_or(args, "env", NULL), out, said);
}

static int requests_answers(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *path = NULL;
    const char *text = NULL;
    unsigned long long size = 0ull;
    unsigned long long line = 0ull;
    return requests_at(args, root, &path, &text, &size, &line, said) &&
           cli_requests_answers(root, path, text, size, line, out, said);
}

static int requests_curl(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *path = NULL;
    const char *text = NULL;
    unsigned long long size = 0ull;
    unsigned long long line = 0ull;
    return requests_at(args, root, &path, &text, &size, &line, said) &&
           cli_requests_curl(root, path, text, size, line, cli_serve_string_or(args, "env", NULL), out, said);
}

static int requests_from_curl(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long size = 0ull;
    const char *const text = cli_serve_string(args, "text", said);
    if (text == NULL)
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &size);
    return cli_requests_from_curl(text, size, out, said);
}

static int requests_private(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    return (path != NULL) && cli_serve_tree(root, said) && cli_requests_private_made(root, path, out, said);
}

// the most nodes and tokens a notebook's tree holds
#define CLI_SERVE_BOOK_NODES (1u << 19u)

// a notebook's text, a merge's three stages, and the tree it is read into, cells taken with their lock and given back
// before it is
static char *s_book;
static char *s_stage[3];
static CliJsonNode *s_node;
static char *s_tree_text;
static CfgJsonToken *s_token;
static CliJson s_json;
static CliLock s_book_lock = CLI_LOCK_INIT;

// the notebook's cells given back, and its lock
static void book_given(void)
{
    cli_cell_given(s_token);
    cli_cell_given(s_tree_text);
    cli_cell_given(s_node);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_cell_given(s_stage[at]);
        s_stage[at] = NULL;
    }
    cli_cell_given(s_book);
    s_token = NULL;
    s_tree_text = NULL;
    s_node = NULL;
    s_book = NULL;
    cli_lock_give(&s_book_lock);
}

// the notebook's lock taken and its cells with it: 1, or 0 with why said and the lock given back where the cells
// cannot meet them
static int book_taken(CliLine *said)
{
    cli_lock_take(&s_book_lock);
    s_book = cli_cell_taken(CLI_SERVE_NOTEBOOK, "a notebook's text");
    int taken = s_book != NULL;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        s_stage[at] = cli_cell_taken(CLI_SERVE_NOTEBOOK, "a merge's stage");
        taken = taken && (s_stage[at] != NULL);
    }
    s_node = cli_cell_taken(CLI_SERVE_BOOK_NODES * sizeof(CliJsonNode), "a notebook's nodes");
    s_tree_text = cli_cell_taken(CLI_SERVE_NOTEBOOK, "a notebook's tree texts");
    s_token = cli_cell_taken(CLI_SERVE_BOOK_NODES * sizeof(CfgJsonToken), "a notebook's tokens");
    taken = taken && (s_node != NULL) && (s_tree_text != NULL) && (s_token != NULL);
    if (!taken)
    {
        book_given();
        cli_line_text(said, "the notebook's cells could not be taken");
    }
    return taken;
}

// the notebook tree made empty
static void notebook_begun(void)
{
    cli_json_begun(&s_json, s_node, CLI_SERVE_BOOK_NODES, s_tree_text, CLI_SERVE_NOTEBOOK);
}

// the file of the argument `path` read into the notebook's text: its size, 1, or 0 with why said
static int notebook_file(const CliServeArgs *args, char *full, unsigned long long *size, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_at(args, root, full, said))
    {
        return 0;
    }
    *size = qry_member_file_read(full, (unsigned char *)s_book, CLI_SERVE_NOTEBOOK);
    if ((*size == QRY_MEMBER_UNREAD) || !cli_utf8_whole(s_book, *size))
    {
        cli_line_text(said, full);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    return 1;
}

static int notebook_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char full[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    if (!book_taken(said))
    {
        return 0;
    }
    notebook_begun();
    const unsigned int book = notebook_file(args, full, &size, said)
                                  ? cli_notebook_read(&s_json, full, s_book, size, s_token, CLI_SERVE_BOOK_NODES, said)
                                  : CLI_JSON_NONE;
    if (book != CLI_JSON_NONE)
    {
        cli_json_write(&s_json, book, out, 0u, 0);
    }
    book_given();
    return book != CLI_JSON_NONE;
}

static int notebook_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_at(args, root, full, said))
    {
        return 0;
    }
    if (!book_taken(said))
    {
        return 0;
    }
    CliLine text = {s_book, CLI_SERVE_NOTEBOOK, 0ull};
    // the arguments' tree is the call's own room, which the writer may add nodes to
    cli_notebook_write((CliJson *)args->json, full, cli_serve_arg(args, "book"), &text);
    const int done = cli_line_fits(&text) && cli_serve_file_write(path, s_book, text.at, said);
    book_given();
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int notebook_diff_text(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long size = 0ull;
    const char *const text = cli_serve_string(args, "text", said);
    if (text == NULL)
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &size);
    if (!book_taken(said))
    {
        return 0;
    }
    notebook_begun();
    const int done = cli_notebook_for_diff(&s_json, text, size, s_token, CLI_SERVE_BOOK_NODES, out, said);
    book_given();
    return done;
}

static int notebook_without_outputs(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char full[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    if (!book_taken(said))
    {
        return 0;
    }
    notebook_begun();
    const int done = notebook_file(args, full, &size, said) &&
                     cli_notebook_without_outputs(&s_json, s_book, size, s_token, CLI_SERVE_BOOK_NODES, out, said);
    book_given();
    return done;
}

// stage `stage` of a merge of the notebook `given` read into the notebook tree: its node, or CLI_JSON_NONE with why
// said
static unsigned int notebook_side(const char *root, const char *given, unsigned int stage, CliLine *said)
{
    CliLine text = {s_stage[stage - 1u], CLI_SERVE_NOTEBOOK, 0ull};
    if (!cli_git_stage_text(root, given, stage, &text) || !cli_line_fits(&text))
    {
        cli_line_text(said, given);
        cli_line_text(said, " holds no stage ");
        cli_line_decimal(said, stage, 1u);
        cli_line_text(said, " of a merge");
        return CLI_JSON_NONE;
    }
    return cli_notebook_read(&s_json, "x.ipynb", s_stage[stage - 1u], text.at, s_token, CLI_SERVE_BOOK_NODES, said);
}

static int notebook_merge(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const given = cli_serve_string(args, "path", said);
    if ((given == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    if (!book_taken(said))
    {
        return 0;
    }
    notebook_begun();
    char quiet[1024];
    CliLine ignored = {quiet, sizeof(quiet), 0ull};
    unsigned int base = notebook_side(root, given, 1u, &ignored);
    if (base == CLI_JSON_NONE)
    {
        base = cli_json_made(&s_json, CFG_JSON_OBJECT);
        cli_json_set(&s_json, base, "cells", cli_json_made(&s_json, CFG_JSON_ARRAY));
    }
    const unsigned int ours = notebook_side(root, given, 2u, said);
    const unsigned int theirs = (ours != CLI_JSON_NONE) ? notebook_side(root, given, 3u, said) : CLI_JSON_NONE;
    if (theirs != CLI_JSON_NONE)
    {
        cli_json_write(&s_json, cli_notebook_merge(&s_json, base, ours, theirs), out, 0u, 0);
    }
    book_given();
    return theirs != CLI_JSON_NONE;
}

static int containers_list(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_containers_list(out);
    return 1;
}

static int container_act(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const name = cli_serve_string(args, "name", said);
    const char *const act = (name != NULL) ? cli_serve_string(args, "act", said) : NULL;
    if ((act == NULL) || !cli_containers_act(name, act, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int container_logs(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const name = cli_serve_string(args, "name", said);
    return (name != NULL) && cli_containers_logs(name, out, said);
}

static int container_run(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const name = cli_serve_string(args, "name", said);
    const char *const line = (name != NULL) ? cli_serve_string(args, "line", said) : NULL;
    return (line != NULL) && cli_containers_run_in(name, line, out, said);
}

static int pods_list(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_containers_pods(out);
    return 1;
}

static int pod_logs(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const space = cli_serve_string(args, "namespace", said);
    const char *const name = (space != NULL) ? cli_serve_string(args, "name", said) : NULL;
    return (name != NULL) &&
           cli_containers_pod_logs(space, name, cli_serve_string_or(args, "container", NULL), out, said);
}

const CliServeCall g_cli_serve_page_calls[] = {
    {"pages_open", pages_open},
    {"pages_shape", pages_shape},
    {"pages_read", pages_read},
    {"pages_edit", pages_edit},
    {"pages_changed", pages_changed},
    {"pages_find", pages_find},
    {"pages_save", pages_save},
    {"pages_release", pages_release},
    {"requests_parse", requests_parse},
    {"requests_environments", requests_environments},
    {"requests_send", requests_send},
    {"requests_answers", requests_answers},
    {"requests_curl", requests_curl},
    {"requests_from_curl", requests_from_curl},
    {"requests_private", requests_private},
    {"notebook_read", notebook_read},
    {"notebook_write", notebook_write},
    {"notebook_diff_text", notebook_diff_text},
    {"notebook_merge", notebook_merge},
    {"notebook_without_outputs", notebook_without_outputs},
    {"containers_list", containers_list},
    {"container_act", container_act},
    {"container_logs", container_logs},
    {"container_run", container_run},
    {"pods_list", pods_list},
    {"pod_logs", pod_logs},
};
const unsigned int g_cli_serve_page_count = sizeof(g_cli_serve_page_calls) / sizeof(g_cli_serve_page_calls[0]);
