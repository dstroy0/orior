// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_hints.c: what a server can do, as its capabilities name it; the spans Expand Selection steps through, the
// tree's, read off the editor's text by the roles its grammar names (symbols_spans.c); and the hints a server writes in
// the text and not into it, each its place, its text, its kind, 1 a type, 2 a parameter's name and 0 any other, and
// whether a space stands before it and after it. A file given its server as slices is asked of the tree alone: its
// hints are the names of the parameters its calls' arguments are given to (symbols_hints.c)
#include "../code/symbols.h"
#include "servers_internal.h"

// a server's capabilities read into a tree taken to their size, under their lock
static CliJson s_capable;
static CliLock s_capable_lock = CLI_LOCK_INIT;

// what the server answers read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

// the capability `name` of the server read, with the capabilities' lock taken: its node
static unsigned int hints_capability(CliLsp *server, const char *name)
{
    unsigned long long length = 0ull;
    const char *const capabilities = cli_lsp_capabilities(server, &length);
    cli_lock_take(&s_capable_lock);
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    const unsigned int root = cli_json_taken(&s_capable, capabilities, length, 0u, 0ull, &err_msg);
    return cli_json_member(&s_capable, root, name);
}

// the capabilities' tree given back with their lock
static void hints_capability_given(void)
{
    cli_json_given(&s_capable);
    cli_lock_give(&s_capable_lock);
}

int cli_servers_capable(CliLsp *server, const char *name)
{
    const unsigned int member = hints_capability(server, name);
    const int capable = (member != CLI_JSON_NONE) && !cli_json_is(&s_capable, member, CFG_JSON_NULL) &&
                        !cli_json_is(&s_capable, member, CFG_JSON_FALSE);
    hints_capability_given();
    return capable;
}

int cli_servers_capable_true(CliLsp *server, const char *name, const char *inner)
{
    const unsigned int member = hints_capability(server, name);
    const int capable = cli_json_is(&s_capable, cli_json_member(&s_capable, member, inner), CFG_JSON_TRUE);
    hints_capability_given();
    return capable;
}

int cli_servers_capable_command(CliLsp *server, const char *command)
{
    const unsigned int member = hints_capability(server, "executeCommandProvider");
    const unsigned int commands = cli_json_member(&s_capable, member, "commands");
    int listed = 0;
    for (unsigned int one = cli_json_is(&s_capable, commands, CFG_JSON_ARRAY) ? s_capable.node[commands].first
                                                                              : CLI_JSON_NONE;
         !listed && (one != CLI_JSON_NONE); one = s_capable.node[one].next)
    {
        const char *const name = cli_json_string(&s_capable, one, NULL);
        listed = (name != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = command, .cap = CLI_CELLS_POOL});
    }
    hints_capability_given();
    return listed;
}

// the params of a request about the file `uri` begun, its object left open
static void hints_params_begun(const char *uri, VerbaLinea *params)
{
    mmgr_line_put(params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(params, '}');
}

// The tree's spans being written: where, and whether one was
typedef struct
{
    VerbaLinea *out;
    int written;
} CliServersSpanned;

// a span the tree gives written as `[from, to]`, after the others
static void hints_span_written(void *context, CliSymbolPlace from, CliSymbolPlace to)
{
    CliServersSpanned *const spanned = (CliServersSpanned *)context;
    mmgr_line_put(spanned->out, spanned->written ? ",[" : "[");
    cli_text_place_written((CliTextPlace){from.line, from.col}, spanned->out);
    mmgr_line_ch(spanned->out, ',');
    cli_text_place_written((CliTextPlace){to.line, to.col}, spanned->out);
    mmgr_line_ch(spanned->out, ']');
    spanned->written = 1;
}

int cli_servers_selection(const char *path, CliTextPlace from, CliTextPlace to, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long size = 0ull;
    char *const text = cli_servers_text_taken(path, &size);
    CliServersSpanned spanned = {out, 0};
    mmgr_line_ch(out, '[');
    const int done =
        (text == NULL) || cli_symbols_spans(path, text, size, (CliSymbolPlace){from.line, from.col},
                                            (CliSymbolPlace){to.line, to.col}, hints_span_written, &spanned, err_msg);
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(text, NULL);
    return done;
}

// a hint's label written as a JSON string, trimmed: its text, or the values of its parts joined; 0 where it is empty
static int hints_label_written(const CliJson *json, unsigned int label, VerbaLinea *out)
{
    static char s_label[1u << 16u];
    VerbaLinea joined = {s_label, sizeof(s_label), 0ull};
    unsigned long long size = 0ull;
    const char *const text = cli_json_string(json, label, &size);
    if (text != NULL)
    {
        mmgr_line_put_n(&joined, text, size);
    }
    for (unsigned int part = cli_json_is(json, label, CFG_JSON_ARRAY) ? json->node[label].first : CLI_JSON_NONE;
         part != CLI_JSON_NONE; part = json->node[part].next)
    {
        const char *const value = cli_json_string(json, cli_json_member(json, part, "value"), &size);
        if (value != NULL)
        {
            mmgr_line_put_n(&joined, value, size);
        }
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(s_label, mmgr_line_ok(&joined) ? joined.at : 0ull, &from, &to);
    if (to == from)
    {
        return 0;
    }
    cli_json_quoted(out, s_label + from, to - from);
    return 1;
}

// The tree's hints being written: where, and whether one was
typedef struct
{
    VerbaLinea *out;
    int written;
} CliServersHinted;

// a parameter's name the tree gives an argument written as a hint of a parameter's name, a space after it
static void hints_tree_written(void *context, unsigned int line, unsigned int col, const char *name,
                               unsigned long long length)
{
    CliServersHinted *const hinted = (CliServersHinted *)context;
    mmgr_line_put(hinted->out, hinted->written ? ",{\"line\":" : "{\"line\":");
    mmgr_line_decimal(hinted->out, line, 1u);
    mmgr_line_put(hinted->out, ",\"col\":");
    mmgr_line_decimal(hinted->out, col, 1u);
    mmgr_line_put(hinted->out, ",\"label\":\"");
    // a parameter's name is a name, plain letters a JSON string holds as they are
    mmgr_line_put_n(hinted->out, name, (size_t)length);
    mmgr_line_put(hinted->out, ":\",\"kind\":2,\"left\":false,\"right\":true}");
    hinted->written = 1;
}

// the hints on the lines `from` to `to` of a file given its server as a slice, from the tree: 1, or 0 with why given
static int hints_of_tree(const char *path, unsigned int from, unsigned int to, VerbaLinea *out, VerbaLinea *err_msg)
{
    char tree[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_servers.lock);
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = g_cli_servers.checked_root, .bytes = sizeof(tree)});
    cli_lock_give(&g_cli_servers.lock);
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    cli_servers_tree_path(tree, path, &own_line);
    unsigned long long size = 0ull;
    char *const text = (mmgr_line_finish(&own_line) != 0ull) ? cli_servers_text_taken(path, &size) : NULL;
    CliServersHinted hinted = {out, 0};
    mmgr_line_ch(out, '[');
    const int done =
        (text == NULL) || cli_symbols_hints(own, text, size, from, to, hints_tree_written, &hinted, err_msg);
    mmgr_line_ch(out, ']');
    g_cli_cells->persistent_buf_release(text, NULL);
    return done;
}

int cli_servers_hints(const char *path, unsigned int from, unsigned int to, VerbaLinea *out, VerbaLinea *err_msg)
{
    if (cli_servers_sliced(path))
    {
        return hints_of_tree(path, from, to, out, err_msg);
    }
    CliLsp *const server = cli_servers_owner_answering(path);
    if ((server == NULL) || !cli_servers_capable(server, "inlayHintProvider"))
    {
        if (server != NULL)
        {
            cli_lsp_unref(server);
        }
        mmgr_line_put(out, "[]");
        return 1;
    }
    cli_servers_reading_locked(&s_reading);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea hints_line;
    cli_pieces_begun(&pieces, &hints_line, piece, sizeof(piece));
    const int has = cli_lsp_text(server, path, &hints_line);
    unsigned long long joined_size = 0ull;
    char *const text = cli_pieces_joined(&pieces, &hints_line, "the text a server has", &joined_size);
    const unsigned long long size = (has && (text != NULL)) ? joined_size : 0ull;
    unsigned long long lines = 1ull;
    unsigned long long last = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        if (text[at] == '\n')
        {
            lines += 1ull;
            last = at + 1ull;
        }
    }
    // the range ends at the start of the line after `to`, or at the end of the text where `to` is its last line
    char uri[CLI_PATH_MAX_LEN];
    cli_servers_uri_of(path, uri);
    VerbaLinea params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    hints_params_begun(uri, &params);
    mmgr_line_put(&params, ",\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(&params, from, 1u);
    mmgr_line_put(&params, ",\"character\":0},\"end\":{\"line\":");
    const int inside = ((unsigned long long)to + 1ull) < lines;
    mmgr_line_decimal(&params, inside ? (unsigned long long)to + 1ull : lines - 1ull, 1u);
    mmgr_line_put(&params, ",\"character\":");
    mmgr_line_decimal(&params, inside ? 0ull : cli_text_units((size > 0ull) ? text + last : "", size - last), 1u);
    mmgr_line_put(&params, "}}}");
    g_cli_cells->persistent_buf_release(text, NULL);
    VerbaLinea result;
    cli_servers_answer_begun(&s_reading, &result);
    const int requested = cli_servers_request(server, "textDocument/inlayHint", &pieces, &params,
                                              CLI_SERVERS_REQUEST_TIMEOUT_MS, &result, err_msg);
    cli_lsp_unref(server);
    if (requested)
    {
        const CliJson *const json = &s_reading.json;
        const unsigned int all = cli_servers_answer_read(&s_reading, &result);
        mmgr_line_ch(out, '[');
        int written = 0;
        for (unsigned int one = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            const unsigned long long before = out->at;
            const CliTextPlace at = cli_servers_place(json, cli_json_member(json, one, "position"));
            mmgr_line_put(out, written ? ",{\"line\":" : "{\"line\":");
            mmgr_line_decimal(out, at.line, 1u);
            mmgr_line_put(out, ",\"col\":");
            mmgr_line_decimal(out, at.col, 1u);
            mmgr_line_put(out, ",\"label\":");
            if (!hints_label_written(json, cli_json_member(json, one, "label"), out))
            {
                out->at = before;
                continue;
            }
            mmgr_line_put(out, ",\"kind\":");
            // the kind is cut to its low byte, as the protocol's 1 and 2 always fit
            mmgr_line_decimal(out, cli_servers_whole(json, cli_json_member(json, one, "kind"), 0ull) & 0xffull, 1u);
            const int left = cli_json_is(json, cli_json_member(json, one, "paddingLeft"), CFG_JSON_TRUE);
            const int right = cli_json_is(json, cli_json_member(json, one, "paddingRight"), CFG_JSON_TRUE);
            mmgr_line_put(out, left ? ",\"left\":true" : ",\"left\":false");
            mmgr_line_put(out, right ? ",\"right\":true}" : ",\"right\":false}");
            written = 1;
        }
        mmgr_line_ch(out, ']');
    }
    cli_servers_reading_released(&s_reading);
    return requested;
}
