// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_changes.c: the changes as the window reads them. Each read lists the tree's changed files through the
// repositories' status into the list read from git, whose texts stand in the persistent tier until the next read, and
// every row points into it
#include "git.h"

#include "../command_line/commands.h"

// the list, standing between the window's frames until the next read
static CliChangedList s_changed;

void cli_changes_read(const CliGitChangesCfg *args)
{
    g_cli_cells->persistent_buf_release(s_changed.text.bytes, &g_cli_git_cells);
    s_changed.text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
    s_changed.count = 0u;
    if (args->root == NULL)
    {
        return;
    }
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = args->root, .named = 0,
               .found_changed_list_pointer = &s_changed);
}

unsigned int cli_changes_rows(const CliGitChangesCfg *args)
{
    (void)args;
    return s_changed.count;
}

int cli_changes_row(const CliGitChangesCfg *args)
{
    if (args->at >= s_changed.count)
    {
        return 0;
    }
    const CliChanged *const changed = &s_changed.item[args->at];
    const char *const path =
        EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_changed.text, .at = changed->path);
    *args->row =
        (CliChangedRow){path, path + changed->name, (changed->name > 0u) ? changed->name - 1u : 0u, changed->state};
    return 1;
}

int cli_changes_menu(const CliGitChangesCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &read))
    {
        return 0;
    }
    // the rows run with the file's path, which stands in the list while the menu is open
    cli_menus_own_begin();
    cli_menus_own_row(
        &(CliItem){"Open File", "file-open", "", read.path, (read.state == 'D') ? "never" : "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Copy Path", "file-copy-path", "", read.path, "", "", 0, 0, 0, 0, 0u});
    return 1;
}
