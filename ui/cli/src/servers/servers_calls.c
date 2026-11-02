// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_calls.c: the call hierarchy: the function at a place as its top, as its server finds it where the server
// answers for calls, as the tree's leaves give it where the file's grammar names its functions and regions
// (symbols_calls.c), and as the inspections' tree finds it elsewhere; and the functions that call a function, or that
// it calls, each with the places of its calls, from where the hierarchy's top came from. A function is written with its
// name and what the server reports of it, its file and the span of its name, the file its calls stand in and where
// each starts, and the server's or the tree's own name for it, which is given back to read the level below it
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../inspect/inspect_tree.h"
#include "servers_internal.h"

#include "../text/text.h"

// What a level of the tree's hierarchy is written with: the tree and the tree's root, the file the hierarchy is
// at, whether its calls come in, where it is written and whether one is
typedef struct
{
    const char *tree;
    const char *root;
    const char *file;
    int incoming;
    VerbaLinea *out;
    int written;
} CliServersCalls;

// what the server answers, under the reading's lock
static CliServersReading s_reading = {0};
static CliServersReading s_item = {0};

typedef struct
{
    const char *tree;
    const char *path;
    VerbaLinea *out;
    const CliTextPlace *sites;
    unsigned int count;
    const CliJson *json;
    unsigned int item;
    const char *site;
    const CliServersCalls *calls;
    const char *file;
    const CliFacts *facts;
    unsigned int function;
    const CliInspectSpan *sites_inspect_span_pointer;
    char *root;
    char *file_out;
    VerbaLinea *err_msg;
} ServersCallsCtx;
// a path written as a JSON string as it stands in `tree`
EMBED_INLINE void calls_path_write(const ServersCallsCtx *args)
{
    const char *tree = args->tree;
    const char *path = args->path;
    VerbaLinea *out = args->out;
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea where = {shown, sizeof(shown), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = tree, .path = path,
               .out = &where);
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)shown, .length = mmgr_line_ok(&where) ? where.at : 0ull);
}

// the places of calls written as an array
EMBED_INLINE void calls_sites_write(const ServersCallsCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = args->sites[at], .out = out);
    }
    mmgr_line_ch(out, ']');
}

// a function of a server's item written, its calls in `site`, the item's own file where it is NULL, at `sites`: 1, or
// 0 where the item names no file
static int calls_get(const ServersCallsCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    unsigned long long length = 0ull;
    const char *const uri =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->item, .name = "uri"),
                   .length_pointer = &length);
    char path[CLI_PATH_MAX_LEN];
    if ((uri == NULL) ||
        !EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri, .length = length, .path_out = path))
    {
        return 0;
    }
    const unsigned int selection = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->item, .name = "selectionRange");
    const unsigned int span = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                         .node = selection, .kind = CFG_JSON_OBJECT)
                                  ? selection
                                  : EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = args->item, .name = "range");
    const CliTextPlace from =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = span, .name = "start"));
    const CliTextPlace to =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = span, .name = "end"));
    const char *const calls_in = (args->site != NULL) ? args->site : path;
    unsigned long long size = 0ull;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->item, .name = "name"),
                   .length_pointer = &size);
    mmgr_line_put(out, "{\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = (name != NULL) ? name : "", .length = size);
    size = 0ull;
    const char *const detail =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->item, .name = "detail"),
                   .length_pointer = &size);
    mmgr_line_put(out, ",\"detail\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = (detail != NULL) ? detail : "", .length = size);
    mmgr_line_put(out, ",\"path\":");
    EMBED_CALL(calls_path_write, ServersCallsCtx, .tree = args->tree, .path = path, .out = out);
    mmgr_line_put(out, ",\"from\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = from, .out = out);
    mmgr_line_put(out, ",\"to\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = to, .out = out);
    mmgr_line_put(out, ",\"site\":");
    EMBED_CALL(calls_path_write, ServersCallsCtx, .tree = args->tree, .path = calls_in, .out = out);
    mmgr_line_put(out, ",\"at\":");
    EMBED_CALL(calls_sites_write, ServersCallsCtx, .sites = args->sites, .count = args->count, .out = out);
    mmgr_line_put(out, ",\"item\":");
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = args->item,
               .out = out, .indent = 0u, .sorted = 0);
    mmgr_line_ch(out, '}');
    return 1;
}

// a function of the tree written, its calls in `site` at `sites`
EMBED_INLINE void calls_tree_write(const ServersCallsCtx *args)
{
    const CliInspectFunction *const one = &args->facts->function[args->function];
    VerbaLinea *const out = args->calls->out;
    char path[CLI_PATH_MAX_LEN];
    mmgr_line_put(out, "{\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = EMBED_CALL(cli_inspect_pool.text, CliInspectPoolCfg, .pool = args->facts->text,
                                  .text_inspect_text = one->name),
               .length = one->name.size);
    mmgr_line_put(out, ",\"detail\":\"\",\"path\":");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->calls->root, .relative = args->file, .joined = path);
    EMBED_CALL(calls_path_write, ServersCallsCtx, .tree = args->calls->tree, .path = path, .out = out);
    mmgr_line_put(out, ",\"from\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg,
               .place = (CliTextPlace){one->from.line, one->from.col}, .out = out);
    mmgr_line_put(out, ",\"to\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){one->to.line, one->to.col},
               .out = out);
    mmgr_line_put(out, ",\"site\":");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->calls->root, .relative = args->site, .joined = path);
    EMBED_CALL(calls_path_write, ServersCallsCtx, .tree = args->calls->tree, .path = path, .out = out);
    mmgr_line_put(out, ",\"at\":[");
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg,
                   .place = (CliTextPlace){args->sites_inspect_span_pointer[at].from.line,
                                           args->sites_inspect_span_pointer[at].from.col},
                   .out = out);
    }
    mmgr_line_put(out, "],\"item\":{\"orior\":{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->file,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, one->from.line, 1u);
    mmgr_line_put(out, "}}}");
}

// a function the tree reaches written after those written
static void calls_reach(const CliInspectReachedCfg *args)
{
    const CliFacts *facts = args->facts;
    unsigned int function = args->function;
    const CliInspectSpan *sites = args->sites;
    unsigned int count = args->count;
    CliServersCalls *const calls = args->context;
    mmgr_line_put(calls->out, calls->written ? "," : "");
    EMBED_CALL(calls_tree_write, ServersCallsCtx, .calls = calls, .file = args->path, .facts = facts,
               .function = function, .site = calls->incoming ? args->path : calls->file,
               .sites_inspect_span_pointer = sites, .count = count);
    calls->written = 1;
}

// The tree's call hierarchy being written: the tree's top, the tree's file the calls stand in where they are the
// function's asked about, NULL where each stands in its own, where it is written, and whether one is
typedef struct
{
    const char *tree;
    const char *site;
    VerbaLinea *out;
    int written;
} CliServersTreeCalls;

// the text the editor holds of the tree's file at `path`, in a cell the caller gives back: NULL where it holds none
static char *calls_open_text(const CliOpenTextCfg *args)
{
    const char *path = args->path;
    unsigned long long *size = args->size;
    const CliServersTreeCalls *const calls = (const CliServersTreeCalls *)args->context;
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = calls->tree, .relative = path, .joined = full);
    return EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg, .path = full,
                      .size_pointer = size);
}

// a function the tree's hierarchy reaches written after those written, its calls at `sites`, `count` of them, in its
// own file or the one the hierarchy was asked about, the tree's own item for it naming its file and line
static void calls_symbols_write(const CliCallReachedCfg *args)
{
    const char *path = args->path;
    unsigned long long length = args->length;
    CliServersTreeCalls *const calls = (CliServersTreeCalls *)args->context;
    VerbaLinea *const out = calls->out;
    char full[CLI_PATH_MAX_LEN];
    mmgr_line_put(out, calls->written ? ",{\"name\":" : "{\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->name,
               .length = length);
    mmgr_line_put(out, ",\"detail\":\"\",\"path\":");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = calls->tree, .relative = path, .joined = full);
    EMBED_CALL(calls_path_write, ServersCallsCtx, .tree = calls->tree, .path = full, .out = out);
    mmgr_line_put(out, ",\"from\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){args->line, args->from},
               .out = out);
    mmgr_line_put(out, ",\"to\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){args->line, args->to},
               .out = out);
    mmgr_line_put(out, ",\"site\":");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = calls->tree,
               .relative = (calls->site != NULL) ? calls->site : path, .joined = full);
    EMBED_CALL(calls_path_write, ServersCallsCtx, .tree = calls->tree, .path = full, .out = out);
    mmgr_line_put(out, ",\"at\":[");
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg,
                   .place = (CliTextPlace){args->sites[at].line, args->sites[at].col}, .out = out);
    }
    mmgr_line_put(out, "],\"item\":{\"tree\":{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, args->line, 1u);
    mmgr_line_put(out, "}}}");
    calls->written = 1;
}

// the root of the tree and the path of `path` in it into `root` and `file`, each sized for CLI_PATH_MAX_LEN:
// 1, or 0 with why given
EMBED_INLINE int calls_tree_file(const ServersCallsCtx *args)
{
    char *root = args->root;
    if (!EMBED_CALL(cli_servers_root_kind_internal.tree_read, CliServersRootKindCtx, .root_out = root))
    {
        mmgr_line_put(args->err_msg, "no language server answers calls here, and orior's own tree is still being read");
        return 0;
    }
    const size_t top = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->path, .other_pointer = root, .size = top) ||
        ((args->path[top] != '/') && (args->path[top] != '\\') && (args->path[top] != '\0')))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, " is outside the tree");
        return 0;
    }
    const char *rest = args->path + top;
    rest += ((*rest == '/') || (*rest == '\\')) ? 1 : 0;
    VerbaLinea line = {args->file_out, CLI_PATH_MAX_LEN, 0ull};
    rest += EMBED_CALL(cli_text_from.put_swap, CliTextFromCfg, .out = &line, .text = rest, .from = '\\', .to = '/');
    (void)mmgr_line_finish(&line);
    return 1;
}

int cli_servers_call_root(const CliServersPathOutCfg *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = path);
    if ((server != NULL) && EMBED_CALL(cli_servers_server_internal.capable, CliServersServerCtx, .server = server,
                                       .name = "callHierarchyProvider"))
    {
        EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_reading);
        CliLspAnswer answer;
        const int requested = EMBED_CALL(cli_servers_path_internal.request_at, CliServersPathCtx, .server = server,
                                         .method = "textDocument/prepareCallHierarchy", .path = path, .line = line,
                                         .col = args->col, .added = NULL, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                                         .answer = &answer, .err_msg = args->err_msg);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        if (requested)
        {
            const CliJson *const json = &s_reading.json;
            const unsigned int first =
                EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                           .array = EMBED_CALL(cli_servers_reading_answer_internal.answer_read,
                                               CliServersReadingAnswerCtx, .reading = &s_reading, .answer = &answer),
                           .index = 0u);
            if ((first == CLI_JSON_NONE) ||
                !EMBED_CALL(calls_get, ServersCallsCtx, .json = json, .item = first, .site = NULL, .sites = NULL,
                            .count = 0u, .tree = args->tree, .out = args->out))
            {
                mmgr_line_put(args->out, "null");
            }
        }
        EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx,
                   .reading = &s_reading);
        return requested;
    }
    if (server != NULL)
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    }
    // the function the tree's leaves give at the place, where the file's grammar names its functions and regions
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->tree, .path = path,
               .out = &own_line);
    if (mmgr_line_finish(&own_line) != 0ull)
    {
        CliServersTreeCalls symbols = {args->tree, NULL, args->out, 0};
        const int found =
            EMBED_CALL(cli_symbols_path_context_line.function_at, CliSymbolsPathContextLineCfg, .path = own,
                       .line = line, .open_text = calls_open_text, .open_text_context = &symbols,
                       .each = calls_symbols_write, .context = &symbols, .err_msg = args->err_msg);
        if (found != 0)
        {
            return found > 0;
        }
    }
    char root[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(calls_tree_file, ServersCallsCtx, .path = path, .root = root, .file_out = file,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    unsigned int at = 0u;
    CliServersCalls calls = {args->tree, root, file, 0, args->out, 0};
    const int found = (EMBED_CALL(cli_inspect_tree_path_line.function_at, CliInspectTreePathLineCfg, .path = file,
                                  .line = line, .at_pointer = &at) ||
                       EMBED_CALL(cli_inspect_tree_path_line.function_on, CliInspectTreePathLineCfg, .path = file,
                                  .line = line, .at_pointer = &at)) &&
                      EMBED_CALL(cli_inspect_tree_path.function, CliInspectTreePathCfg, .path = file, .at = at,
                                 .each = calls_reach, .context = &calls);
    if (!found)
    {
        mmgr_line_put(args->out, "null");
    }
    return 1;
}

int cli_servers_calls(const CliServersPathCfg *args)
{
    const char *path = args->path;
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_item);
    const CliJson *const json = &s_item.json;
    const unsigned int given = EMBED_CALL(cli_servers_reading_internal.read, CliServersReadingCtx, .reading = &s_item,
                                          .text = args->item, .size = args->size);
    const unsigned int leaves = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = given, .name = "tree");
    if (leaves != CLI_JSON_NONE)
    {
        char file[CLI_PATH_MAX_LEN];
        unsigned long long length = 0ull;
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = leaves, .name = "path"),
                       .length_pointer = &length);
        const unsigned long long copied_len = ((named != NULL) && (length < sizeof(file))) ? length : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = file, .src = (named != NULL) ? named : "", .bytes = (size_t)copied_len});
        file[copied_len] = '\0';
        // the line is cut to its low word, as the tree counts lines
        const unsigned int line = (unsigned int)EMBED_CALL(
            cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                               .object = leaves, .name = "line"),
            .otherwise = 0ull);
        CliServersTreeCalls symbols = {args->tree, args->incoming ? NULL : file, args->out, 0};
        mmgr_line_ch(args->out, '[');
        const int found =
            args->incoming
                ? EMBED_CALL(cli_symbols_path_context_line.callers, CliSymbolsPathContextLineCfg, .path = file,
                             .line = line, .open_text = calls_open_text, .open_text_context = &symbols,
                             .each = calls_symbols_write, .context = &symbols, .err_msg = args->err_msg)
                : EMBED_CALL(cli_symbols_path_context_line.callees, CliSymbolsPathContextLineCfg, .path = file,
                             .line = line, .open_text = calls_open_text, .open_text_context = &symbols,
                             .each = calls_symbols_write, .context = &symbols, .err_msg = args->err_msg);
        mmgr_line_ch(args->out, ']');
        EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_item);
        return found >= 0;
    }
    const unsigned int own = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = given, .name = "orior");
    int done = 1;
    if (own != CLI_JSON_NONE)
    {
        char root[CLI_PATH_MAX_LEN];
        char ignored[CLI_PATH_MAX_LEN];
        done = EMBED_CALL(calls_tree_file, ServersCallsCtx, .path = path, .root = root, .file_out = ignored,
                          .err_msg = args->err_msg);
        if (done)
        {
            char file[CLI_PATH_MAX_LEN];
            unsigned long long length = 0ull;
            const char *const named =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = own, .name = "path"),
                           .length_pointer = &length);
            const unsigned long long copied_len = ((named != NULL) && (length < sizeof(file))) ? length : 0ull;
            memor.cpy(&(MemoriaCfg){.dst = file, .src = (named != NULL) ? named : "", .bytes = (size_t)copied_len});
            file[copied_len] = '\0';
            // the line is cut to its low word, as the tree counts lines
            const unsigned int line = (unsigned int)EMBED_CALL(
                cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                   .object = own, .name = "line"),
                .otherwise = 0ull);
            CliServersCalls calls = {args->tree, root, file, args->incoming, args->out, 0};
            unsigned int at = 0u;
            mmgr_line_ch(args->out, '[');
            if (EMBED_CALL(cli_inspect_tree_path_line.function_on, CliInspectTreePathLineCfg, .path = file,
                           .line = line, .at_pointer = &at))
            {
                if (args->incoming)
                {
                    EMBED_CALL(cli_inspect_tree_path.callers, CliInspectTreePathCfg, .path = file, .at = at,
                               .each = calls_reach, .context = &calls);
                }
                else
                {
                    EMBED_CALL(cli_inspect_tree_path.callees, CliInspectTreePathCfg, .path = file, .at = at,
                               .each = calls_reach, .context = &calls);
                }
            }
            mmgr_line_ch(args->out, ']');
        }
        EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_item);
        return done;
    }
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = path);
    if (server == NULL)
    {
        mmgr_line_put(args->err_msg, "no language server has this file open");
        EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_item);
        return 0;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_reading);
    // the item written back, a string's escapes free to differ from how it was given, inside its member
    const unsigned long long room = (args->size * 2ull) + 16ull;
    char *const params_text = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
    VerbaLinea params = mmgr_line_over(params_text, room);
    mmgr_line_put(&params, "{\"item\":");
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = given,
               .out = &params, .indent = 0u, .sorted = 0);
    mmgr_line_ch(&params, '}');
    CliLspAnswer reply;
    done = EMBED_CALL(cli_servers_err_msg_internal.request, CliServersErrMsgCtx, .server = server,
                      .method = args->incoming ? "callHierarchy/incomingCalls" : "callHierarchy/outgoingCalls",
                      .params = &params, .wait = CLI_SERVERS_SEARCHING, .answer = &reply, .err_msg = args->err_msg);
    g_cli_cells->persistent_buf_release(params_text, &g_cli_servers_cells);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    if (done)
    {
        unsigned long long length = 0ull;
        const char *const uri =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = given, .name = "uri"),
                       .length_pointer = &length);
        char parent[CLI_PATH_MAX_LEN];
        const int has_parent = (uri != NULL) && EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri,
                                                           .length = length, .path_out = parent);
        const CliJson *const answer = &s_reading.json;
        const unsigned int all = EMBED_CALL(cli_servers_reading_answer_internal.answer_read, CliServersReadingAnswerCtx,
                                            .reading = &s_reading, .answer = &reply);
        mmgr_line_ch(args->out, '[');
        int written = 0;
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = answer,
                                           .node = all, .kind = CFG_JSON_ARRAY)
                                    ? answer->node[all].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = answer->node[one].next)
        {
            // the call's sites in a cell of one place for each range it gives, none where it cannot be taken
            const unsigned int ranges = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                   .json_read = answer, .object = one, .name = "fromRanges");
            const unsigned int given_ranges = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                         .json_read = answer, .node = ranges, .kind = CFG_JSON_ARRAY)
                                                  ? answer->node[ranges].count
                                                  : 0u;
            CliTextPlace *const site =
                (given_ranges > 0u) ? g_cli_cells->temporary_buf_alloc(
                                          (unsigned long long)given_ranges * sizeof(CliTextPlace), &g_cli_servers_cells)
                                    : NULL;
            unsigned int count = 0u;
            for (unsigned int range = (site != NULL) ? answer->node[ranges].first : CLI_JSON_NONE;
                 range != CLI_JSON_NONE; range = answer->node[range].next)
            {
                site[count] = EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx,
                                         .json = answer,
                                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                            .json_read = answer, .object = range, .name = "start"));
                count += 1u;
            }
            const unsigned long long before = args->out->at;
            mmgr_line_put(args->out, written ? "," : "");
            const int wrote =
                args->incoming
                    ? EMBED_CALL(calls_get, ServersCallsCtx, .json = answer,
                                 .item = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = answer, .object = one, .name = "from"),
                                 .site = NULL, .sites = site, .count = count, .tree = args->tree, .out = args->out)
                    : EMBED_CALL(calls_get, ServersCallsCtx, .json = answer,
                                 .item = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = answer, .object = one, .name = "to"),
                                 .site = has_parent ? parent : NULL, .sites = site, .count = count, .tree = args->tree,
                                 .out = args->out);
            g_cli_cells->persistent_buf_release(site, &g_cli_servers_cells);
            if (!wrote)
            {
                args->out->at = before;
                continue;
            }
            written = 1;
        }
        mmgr_line_ch(args->out, ']');
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_reading);
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_item);
    return done;
}
