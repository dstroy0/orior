// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_build.c: the calls of the build files orior_cli reads itself: a Maven POM's settings read and set, the
// edits that set one going by the POM's path in the tree, and the line that updates a build's snapshot dependencies
#include "../builds/builds.h"
#include "../builds/maven.h"
#include "../command_line/cells.h"
#include "serve_internal.h"

// A POM's text as the calls read it: the argument `text` where the editor has it, else the file mapped, NULL where
// it is not there, let go with tool_pom_let_go
typedef struct
{
    const char *text;
    unsigned long long size;
    const unsigned char *mapped;
} CliServePom;
// the bytes a setting's edits are taken with
#define CLI_SERVE_SET_ROOM (1ull << 20u)

typedef struct
{
    const CliServePom *read;
    const CliServeArgs *request;
    char *root;
    char *pom;
    CliServePom *read_out;
    VerbaLinea *err_msg;
} ServeCallBuildCtx;
EMBED_INLINE void tool_pom_let_go(const ServeCallBuildCtx *args)
{
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = args->read->mapped,
               .owner = &g_cli_window_cells);
}

// the POM of the argument `path` of the tree, `pom.xml` at its top where none is given, into `pom`, and its text into
// `read`: 1, or 0 with why given
EMBED_INLINE int tool_pom_get(const ServeCallBuildCtx *args)
{
    const CliServeArgs *arguments = args->request;
    char *root = args->root;
    CliServePom *read = args->read_out;
    VerbaLinea *err_msg = args->err_msg;
    *read = (CliServePom){"", 0ull, NULL};
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .otherwise = NULL);
    if ((path != NULL) &&
        (EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = path) == CLI_BUILD_MAVEN))
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = args->pom);
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "pom.xml", .joined = args->pom);
    }
    const unsigned int given =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text");
    unsigned long long size = 0ull;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = arguments->json, .node = given, .length_pointer = &size);
    if (text != NULL)
    {
        *read = (CliServePom){text, size, NULL};
        return 1;
    }
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->pom,
                                                   .size_pointer = &size, .owner = &g_cli_window_cells);
    // the file's bytes are read as its text; a POM not there is read as empty
    *read = (mapped != NULL) ? (CliServePom){(const char *)mapped, size, mapped} : (CliServePom){"", 0ull, NULL};
    return 1;
}

static int maven_settings(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char pom[CLI_PATH_MAX_LEN];
    CliServePom read;
    const int done = EMBED_CALL(tool_pom_get, ServeCallBuildCtx, .request = arguments, .root = root, .pom = pom,
                                .read_out = &read, .err_msg = err_msg);
    if (done)
    {
        EMBED_CALL(cli_maven.settings, CliMavenCfg, .root = root, .pom = pom, .text = read.text, .size = read.size,
                   .out = out);
    }
    EMBED_CALL(tool_pom_let_go, ServeCallBuildCtx, .read = &read);
    return done;
}

static int maven_set(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char pom[CLI_PATH_MAX_LEN];
    CliServePom read;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const value = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "value", .err_msg = err_msg)
                                            : NULL;
    if (value == NULL)
    {
        return 0;
    }
    static char s_head[CLI_PATH_MAX_LEN * 2u];
    char *const set_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_SET_ROOM, &g_cli_window_cells);
    VerbaLinea set = mmgr_line_over(set_cell, CLI_SERVE_SET_ROOM);
    const int begun = set.out != NULL;
    const int done = begun &&
                     EMBED_CALL(tool_pom_get, ServeCallBuildCtx, .request = arguments, .root = root, .pom = pom,
                                .read_out = &read, .err_msg = err_msg) &&
                     EMBED_CALL(cli_maven_key.set, CliMavenKeyCfg, .pom = pom, .text = read.text, .size = read.size,
                                .key = key, .value = value, .out = &set, .err_msg = err_msg);
    if (done)
    {
        // the POM's edits go by its path in the tree
        VerbaLinea head = {s_head, sizeof(s_head), 0ull};
        mmgr_line_put(&head, "[{\"path\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &head,
                   .text_bytes = (const unsigned char *)pom,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = pom, .cap = CLI_CELLS_POOL}));
        const int named = mmgr_line_ok(&head) && (set.at >= head.at) &&
                          (memor.cmp(&(MemoriaCfg){.src = set.out, .other = s_head, .bytes = head.at}) == 0);
        if (named)
        {
            char shown[CLI_PATH_MAX_LEN];
            VerbaLinea where = {shown, sizeof(shown), 0ull};
            EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = root, .path = pom,
                       .out = &where);
            mmgr_line_put(out, "[{\"path\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                       .text_bytes = (const unsigned char *)shown, .length = mmgr_line_ok(&where) ? where.at : 0ull);
            mmgr_line_put_n(out, set.out + head.at, set.at - head.at);
        }
        else
        {
            mmgr_line_put_n(out, set.out, set.at);
        }
    }
    if (begun)
    {
        EMBED_CALL(tool_pom_let_go, ServeCallBuildCtx, .read = &read);
    }
    g_cli_cells->persistent_buf_release(set.out, &g_cli_window_cells);
    return done;
}

static int snapshots_line(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char line[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .otherwise = NULL);
    if (path != NULL)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
    }
    if (!EMBED_CALL(cli_builds_path_root.build_snapshots_line, CliBuildsPathRootCfg, .root = root,
                    .path = (path != NULL) ? full : NULL, .folder_out = folder, .line_room = line, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_ch(out, '[');
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)folder,
               .length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, ',');
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)line,
               .length = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, ']');
    return 1;
}

const CliServeCall g_cli_serve_build_calls[] = {
    {"maven_settings", maven_settings},
    {"maven_set", maven_set},
    {"snapshots_line", snapshots_line},
};
const unsigned int g_cli_serve_build_count = sizeof(g_cli_serve_build_calls) / sizeof(g_cli_serve_build_calls[0]);
