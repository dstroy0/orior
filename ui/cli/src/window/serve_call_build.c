// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_build.c: the calls of the build files orior_cli reads itself: a Maven POM's settings read and set, the
// edits that set one going by the POM's path in the tree, and the line that updates a build's snapshot dependencies
#include "../builds/builds.h"
#include "../builds/maven.h"
#include "serve_internal.h"

// the bytes a setting's edits are begun with, the line growing past them
#define CLI_SERVE_SET_FIRST 4096ull

// A POM's text as the calls read it: the argument `text` where the editor holds it, else the file mapped, NULL where
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
// `read`: 1, or 0 with why said
static int tool_pom_of(const CliServeArgs *args, char *root, char *pom, CliServePom *read, CliLine *said)
{
    *read = (CliServePom){"", 0ull, NULL};
    if (!cli_serve_tree(root, said))
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
    const unsigned char *const mapped = cli_file_held(pom, &size);
    // the file's bytes are read as its text; a POM not there is read as empty
    *read = (mapped != NULL) ? (CliServePom){(const char *)mapped, size, mapped} : (CliServePom){"", 0ull, NULL};
    return 1;
}

static int maven_settings(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_LONGEST];
    char pom[CLI_PATH_LONGEST];
    CliServePom read;
    cli_lock_take(&s_lock);
    const int done = tool_pom_of(args, root, pom, &read, said);
    if (done)
    {
        cli_maven_settings(root, pom, read.text, read.size, out);
    }
    tool_pom_let_go(&read);
    cli_lock_give(&s_lock);
    return done;
}

static int maven_set(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_LONGEST];
    char pom[CLI_PATH_LONGEST];
    CliServePom read;
    const char *const key = cli_serve_string(args, "key", said);
    const char *const value = (key != NULL) ? cli_serve_string(args, "value", said) : NULL;
    if (value == NULL)
    {
        return 0;
    }
    static char s_head[CLI_PATH_LONGEST * 2u];
    cli_lock_take(&s_lock);
    CliLine set;
    const int begun = cli_line_growing(&set, CLI_SERVE_SET_FIRST, "a setting's edits");
    const int done = begun && tool_pom_of(args, root, pom, &read, said) &&
                     cli_maven_set(pom, read.text, read.size, key, value, &set, said);
    if (done)
    {
        // the POM's edits go by its path in the tree
        CliLine head = {s_head, sizeof(s_head), 0ull};
        cli_line_text(&head, "[{\"path\":");
        cli_json_quoted_lossy(&head, (const unsigned char *)pom,
                              cellul.len(&(CatenaFinitaCfg){.src = pom, .cap = CLI_CELLS_POOL}));
        const int named = cli_line_fits(&head) && (set.at >= head.at) &&
                          (memor.cmp(&(MemoriaCfg){.src = set.out, .other = s_head, .bytes = head.at}) == 0);
        if (named)
        {
            char shown[CLI_PATH_LONGEST];
            CliLine where = {shown, sizeof(shown), 0ull};
            cli_servers_tree_path(root, pom, &where);
            cli_line_text(out, "[{\"path\":");
            cli_json_quoted_lossy(out, (const unsigned char *)shown, cli_line_fits(&where) ? where.at : 0ull);
            cli_line_counted(out, set.out + head.at, set.at - head.at);
        }
        else
        {
            cli_line_counted(out, set.out, set.at);
        }
    }
    if (begun)
    {
        tool_pom_let_go(&read);
    }
    cli_cell_given(set.out);
    cli_lock_give(&s_lock);
    return done;
}

static int snapshots_line(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char line[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    const char *const path = cli_serve_string_or(args, "path", NULL);
    if (path != NULL)
    {
        cli_full(root, path, full);
    }
    if (!cli_build_snapshots_line(root, (path != NULL) ? full : NULL, folder, line, said))
    {
        return 0;
    }
    cli_line_character(out, '[');
    cli_json_quoted_lossy(out, (const unsigned char *)folder,
                          cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    cli_line_character(out, ',');
    cli_json_quoted_lossy(out, (const unsigned char *)line,
                          cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
    cli_line_character(out, ']');
    return 1;
}

const CliServeCall g_cli_serve_build_calls[] = {
    {"maven_settings", maven_settings},
    {"maven_set", maven_set},
    {"snapshots_line", snapshots_line},
};
const unsigned int g_cli_serve_build_count = sizeof(g_cli_serve_build_calls) / sizeof(g_cli_serve_build_calls[0]);
