// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_page.c: the calls of the page's own kinds of file and of the machine's containers: a file too large for
// the page held here as the file and the lines written over it, opened, shaped, read, edited, changed, found in, saved
// and let go; a file of HTTP requests parsed, its environments read, a request sent, its answers kept, written as curl
// and read from curl, and its private environment made; and the containers and pods, their logs, and a container
// acted on or a line run in it
#include "../command_line/cells.h"
#include "../deploy/containers.h"
#include "../editor/document.h"
#include "../editor/pages.h"
#include "../requests/requests.h"
#include "serve_internal.h"

// the most changes and edits one edit of a held text makes
#define CLI_SERVE_CHANGES 4096u
#define CLI_SERVE_PAGE_EDITS 65536u

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
    // the changes and their edits counted, CLI_SERVE_CHANGES and CLI_SERVE_PAGE_EDITS at the most, and one cell taken
    // for the edits, each change's first and each change's count
    unsigned int changes_most = 0u;
    unsigned int edits_most = 0u;
    for (unsigned int change = json->node[changes].first;
         (change != CLI_JSON_NONE) && (changes_most < CLI_SERVE_CHANGES); change = json->node[change].next)
    {
        changes_most += 1u;
        for (unsigned int edit = cli_json_is(json, change, CFG_JSON_ARRAY) ? json->node[change].first : CLI_JSON_NONE;
             (edit != CLI_JSON_NONE) && (edits_most < CLI_SERVE_PAGE_EDITS); edit = json->node[edit].next)
        {
            edits_most += 1u;
        }
    }
    const unsigned long long edit_bytes = (edits_most + 1ull) * sizeof(CliEdit);
    const unsigned long long change_bytes = (changes_most + 1ull) * sizeof(const CliEdit *);
    char *const cell = cli_serve_cell(edit_bytes + change_bytes + ((changes_most + 1ull) * sizeof(unsigned int)),
                                      "the edits of a held text", said);
    if (cell == NULL)
    {
        return 0;
    }
    // the edits on the cell's boundary, the changes' pointers then their counts after them
    CliEdit *const edit_list = (CliEdit *)(void *)cell;
    const CliEdit **const change_list = (const CliEdit **)(void *)(cell + edit_bytes);
    unsigned int *const edit_counts = (unsigned int *)(void *)(cell + edit_bytes + change_bytes);
    unsigned int count = 0u;
    unsigned int used = 0u;
    const char *missing = NULL;
    for (unsigned int change = json->node[changes].first;
         (missing == NULL) && (change != CLI_JSON_NONE) && (count < changes_most); change = json->node[change].next)
    {
        change_list[count] = &edit_list[used];
        edit_counts[count] = 0u;
        for (unsigned int edit = cli_json_is(json, change, CFG_JSON_ARRAY) ? json->node[change].first : CLI_JSON_NONE;
             (missing == NULL) && (edit != CLI_JSON_NONE) && (used < edits_most); edit = json->node[edit].next)
        {
            CliEdit *const one = &edit_list[used];
            one->text = cli_json_string(json, cli_json_member(json, edit, "text"), &one->length);
            missing = !page_place(json, cli_json_member(json, edit, "from"), &one->from) ? "from"
                      : !page_place(json, cli_json_member(json, edit, "to"), &one->to)   ? "to"
                      : (one->text == NULL)                                              ? "text"
                                                                                         : NULL;
            used += 1u;
            edit_counts[count] += 1u;
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
    const int done = whole && cli_pages_edit(key, change_list, edit_counts, count, out, said);
    cli_cell_given(cell);
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
    if (key == NULL)
    {
        return 0;
    }
    // a paged file is written as every file of the tree is, one write at a time
    cli_lock_take(&g_cli_serve_write_lock);
    const int saved = cli_pages_save(key, out, said);
    cli_lock_give(&g_cli_serve_write_lock);
    return saved;
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
    {"containers_list", containers_list},
    {"container_act", container_act},
    {"container_logs", container_logs},
    {"container_run", container_run},
    {"pods_list", pods_list},
    {"pod_logs", pod_logs},
};
const unsigned int g_cli_serve_page_count = sizeof(g_cli_serve_page_calls) / sizeof(g_cli_serve_page_calls[0]);
