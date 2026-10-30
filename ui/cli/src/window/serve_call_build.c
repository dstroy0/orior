// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_build.c: the calls of the build files orior_cli reads itself: a Maven POM's settings read and set, the
// edits that set one going by the POM's path in the tree, and the line that updates a build's snapshot dependencies
#include "../builds/builds.h"
#include "../builds/maven.h"
#include "serve_internal.h"

// the bytes a setting's edits are begun with, the line growing past them
#define CLI_SERVE_SET_FIRST 4096ull

// A POM's text as the calls read it: the argument `text` where the editor has it, else the file mapped, NULL where
// it is not there, let go with tool_pom_let_go
typedef struct
{
    const char *text;
    unsigned long long size;
    const unsigned char *mapped;
} CliServePom;

static void tool_pom_let_go(const CliServePom *read)
{
    if (read->mapped != NULL)
    {
        cli_file_let_go(read->mapped, read->size);
    }
}

// the POM of the argument `path` of the tree, `pom.xml` at its top where none is given, into `pom`, and its text into
// `read`: 1, or 0 with why given
static int tool_pom_of(const CliServeArgs *args, char *root, char *pom, CliServePom *read, VerbaLinea *err_msg)
{
    *read = (CliServePom){"", 0ull, NULL};
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    const char *const path = cli_serve_string_or(args, "path", NULL);
    if ((path != NULL) && (cli_build_kind_of(path) == CLI_BUILD_MAVEN))
    {
        cli_full(root, path, pom);
    }
    else
    {
        cli_full(root, "pom.xml", pom);
    }
    const unsigned int given = cli_serve_arg(args, "text");
    unsigned long long size = 0ull;
    const char *const text = cli_json_string(args->json, given, &size);
    if (text != NULL)
    {
        *read = (CliServePom){text, size, NULL};
        return 1;
    }
    const unsigned char *const mapped = cli_file_read(pom, &size);
    // the file's bytes are read as its text; a POM not there is read as empty
    *read = (mapped != NULL) ? (CliServePom){(const char *)mapped, size, mapped} : (CliServePom){"", 0ull, NULL};
    return 1;
}

static int maven_settings(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_MAX_LEN];
    char pom[CLI_PATH_MAX_LEN];
    CliServePom read;
    cli_lock_take(&s_lock);
    const int done = tool_pom_of(args, root, pom, &read, err_msg);
    if (done)
    {
        cli_maven_settings(root, pom, read.text, read.size, out);
    }
    tool_pom_let_go(&read);
    cli_lock_give(&s_lock);
    return done;
}

static int maven_set(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_MAX_LEN];
    char pom[CLI_PATH_MAX_LEN];
    CliServePom read;
    const char *const key = cli_serve_string(args, "key", err_msg);
    const char *const value = (key != NULL) ? cli_serve_string(args, "value", err_msg) : NULL;
    if (value == NULL)
    {
        return 0;
    }
    static char s_head[CLI_PATH_MAX_LEN * 2u];
    cli_lock_take(&s_lock);
    VerbaLinea set;
    const int begun = cli_growing_line_made(&set, CLI_SERVE_SET_FIRST, &g_cli_window_cells);
    const int done = begun && tool_pom_of(args, root, pom, &read, err_msg) &&
                     cli_maven_set(pom, read.text, read.size, key, value, &set, err_msg);
    if (done)
    {
        // the POM's edits go by its path in the tree
        VerbaLinea head = {s_head, sizeof(s_head), 0ull};
        mmgr_line_put(&head, "[{\"path\":");
        cli_json_quoted_lossy(&head, (const unsigned char *)pom,
                              cellul.len(&(CatenaFinitaCfg){.src = pom, .cap = CLI_CELLS_POOL}));
        const int named = mmgr_line_ok(&head) && (set.at >= head.at) &&
                          (memor.cmp(&(MemoriaCfg){.src = set.out, .other = s_head, .bytes = head.at}) == 0);
        if (named)
        {
            char shown[CLI_PATH_MAX_LEN];
            VerbaLinea where = {shown, sizeof(shown), 0ull};
            cli_servers_tree_path(root, pom, &where);
            mmgr_line_put(out, "[{\"path\":");
            cli_json_quoted_lossy(out, (const unsigned char *)shown, mmgr_line_ok(&where) ? where.at : 0ull);
            mmgr_line_put_n(out, set.out + head.at, set.at - head.at);
        }
        else
        {
            mmgr_line_put_n(out, set.out, set.at);
        }
    }
    if (begun)
    {
        tool_pom_let_go(&read);
    }
    g_cli_cells->persistent_buf_release(set.out, &g_cli_window_cells);
    cli_lock_give(&s_lock);
    return done;
}

static int snapshots_line(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char line[CLI_PATH_MAX_LEN];
    if (!cli_serve_tree(root, err_msg))
    {
        return 0;
    }
    const char *const path = cli_serve_string_or(args, "path", NULL);
    if (path != NULL)
    {
        cli_full(root, path, full);
    }
    if (!cli_build_snapshots_line(root, (path != NULL) ? full : NULL, folder, line, err_msg))
    {
        return 0;
    }
    mmgr_line_ch(out, '[');
    cli_json_quoted_lossy(out, (const unsigned char *)folder,
                          cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, ',');
    cli_json_quoted_lossy(out, (const unsigned char *)line,
                          cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(out, ']');
    return 1;
}

const CliServeCall g_cli_serve_build_calls[] = {
    {"maven_settings", maven_settings},
    {"maven_set", maven_set},
    {"snapshots_line", snapshots_line},
};
const unsigned int g_cli_serve_build_count = sizeof(g_cli_serve_build_calls) / sizeof(g_cli_serve_build_calls[0]);
