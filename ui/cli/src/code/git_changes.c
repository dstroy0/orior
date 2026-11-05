// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_changes.c: the changes as the window reads them. Each read lists the tree's changed files through the
// repositories' status and copies each path beside the rows, in a cell of their own taken as the first read comes, so
// the rows stand as long as the window draws them while the list's own text goes back at once
#include "git.h"

#include "../command_line/cells.h"
#include "../command_line/commands.h"

// the bytes the rows' paths are copied into
#define CHANGES_TEXT_BYTES (512u * 1024u)

// A row as stored: its path's place in the text copied, the bytes of its folder and its state letter
typedef struct
{
    unsigned int path;
    unsigned long long folder_length;
    char state;
} ChangesRow;

// The changes: the rows and how many, and the text their paths are copied into and how much of it is used
typedef struct
{
    ChangesRow row[CLI_CHANGES_ROWS];
    unsigned int count;
    unsigned int used;
    char text[CHANGES_TEXT_BYTES];
} Changes;

static Changes *s_changes;
static CliChangedList s_changed;

typedef struct
{
    const char *path;
} ChangesCtx;

// `path` copied after the paths copied so far with a NUL after it, a row of it with its folder's bytes, where the rows
// and the text fit it
EMBED_INLINE void changes_row_append(const ChangesCtx *args)
{
    Changes *const changes = s_changes;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    if ((changes->count >= CLI_CHANGES_ROWS) ||
        (length + 1ULL > (unsigned long long)(CHANGES_TEXT_BYTES - changes->used)))
    {
        return;
    }
    const unsigned int at = changes->used;
    memor.cpy(&(MemoriaCfg){.dst = changes->text + at, .src = args->path, .bytes = length + 1ULL});
    // a path fits the bytes, which an unsigned int counts
    changes->used = at + (unsigned int)length + 1u;
    // the folder is what stands before the last slash, none where the path has no slash
    unsigned long long cut = 0ULL;
    for (unsigned long long each = 0ULL; each < length; each += 1ULL)
    {
        cut = (args->path[each] == '/') ? each : cut;
    }
    changes->row[changes->count] = (ChangesRow){at, cut, '\0'};
    changes->count += 1u;
}

void cli_changes_read(const CliGitChangesCfg *args)
{
    if (s_changes == NULL)
    {
        s_changes = g_cli_cells->persistent_buf_alloc(sizeof(Changes), &g_cli_git_cells);
        if (s_changes == NULL)
        {
            return;
        }
    }
    s_changes->count = 0u;
    s_changes->used = 0u;
    if (args->root == NULL)
    {
        return;
    }
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = args->root, .named = 0,
               .found_changed_list_pointer = &s_changed);
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        const unsigned int before = s_changes->count;
        EMBED_CALL(changes_row_append, ChangesCtx,
                   .path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_changed.text,
                                      .at = s_changed.item[at].path));
        if (s_changes->count != before)
        {
            s_changes->row[before].state = s_changed.item[at].state;
        }
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_changed.text);
}

unsigned int cli_changes_rows(const CliGitChangesCfg *args)
{
    (void)args;
    return (s_changes != NULL) ? s_changes->count : 0u;
}

int cli_changes_row(const CliGitChangesCfg *args)
{
    if ((s_changes == NULL) || (args->at >= s_changes->count))
    {
        return 0;
    }
    const ChangesRow *const row = &s_changes->row[args->at];
    const char *const path = s_changes->text + row->path;
    *args->row = (CliChangedRow){path, (row->folder_length != 0ULL) ? path + row->folder_length + 1ULL : path,
                                 row->folder_length, row->state};
    return 1;
}

int cli_changes_menu(const CliGitChangesCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &read))
    {
        return 0;
    }
    char words[CLI_PATH_MAX_LEN + 2u];
    VerbaLinea line = {words, sizeof(words), 0ULL};
    mmgr_line_put(&line, read.path);
    mmgr_line_ch(&line, '\n');
    (void)mmgr_line_finish(&line);
    cli_menus_own_begin();
    cli_menus_own_row(
        &(CliItem){"Open File", "file-open", "", words, (read.state == 'D') ? "never" : "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Copy Path", "file-copy-path", "", words, "", "", 0, 0, 0, 0, 0u});
    return 1;
}
