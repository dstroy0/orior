// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_install.c: orior's install on Linux and macOS. The default folder is .local/orior under the reader's home, and
// a copied program's mode lets its owner alone run it. A name reaches the PATH as a link in ~/.local/bin, or as an
// alias in the reader's shell profiles
#if !defined(_WIN32)
// symlink and chmod are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#include "../../../ui/cli/src/machine/install_internal.h"
#include "../../../ui/cli/src/toolchains/toolchains.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>

int cli_install_default_folder(const CliInstallCfg *args)
{
    VerbaLinea folder = {args->folder_out, CLI_PATH_MAX_LEN, 0ULL};
    const char *const base = args->home;
    if ((base == NULL) || (base[0] == '\0'))
    {
        args->folder_out[0] = '\0';
        return 0;
    }
    mmgr_line_put(&folder, base);
    mmgr_line_put(&folder, "/.local/orior");
    return mmgr_line_finish(&folder) != 0ULL;
}

int cli_install_default_found(const CliInstallCfg *args)
{
    char base[CLI_PATH_MAX_LEN];
    base[0] = '\0';
    (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = base,
                     .capacity = sizeof(base));
    return EMBED_CALL(cli_install_default_folder, CliInstallCfg, .home = base, .folder_out = args->folder_out);
}

int cli_install_runnable(const InstallCtx *args)
{
    // the program's owner alone reads, writes and runs it
    return chmod(args->path, 0700) == 0;
}

// the link ~/.local/bin/`name` to the program, or with --aliases the alias line in the reader's shell profiles
int cli_install_place(InstallCtx *args)
{
    char target[CLI_PATH_MAX_LEN];
    VerbaLinea target_line = {target, sizeof(target), 0ULL};
    EMBED_CALL(cli_install_shared.joined, InstallCtx, .line = &target_line, .folder = args->into, .name = args->name);
    if (args->cfg->options->aliases)
    {
        char alias[CLI_PATH_MAX_LEN + 64u];
        VerbaLinea alias_line = {alias, sizeof(alias), 0ULL};
        mmgr_line_put(&alias_line, "alias ");
        mmgr_line_put(&alias_line, args->name);
        mmgr_line_put(&alias_line, "='");
        mmgr_line_put(&alias_line, target);
        mmgr_line_put(&alias_line, "'");
        (void)mmgr_line_finish(&alias_line);
        if (!EMBED_CALL(cli_toolchains_err_msg_folder.profile_line_add, CliToolchainsErrMsgFolderCfg, .what = alias,
                        .err_msg = args->cfg->err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_install_shared.recorded, InstallCtx, .record = args->record, .text = "profile", .other = alias);
        EMBED_CALL(cli_install_shared.said, InstallCtx, .cfg = args->cfg, .text = args->name,
                   .other = " is an alias in your shell profiles: ", .path = target);
        return 1;
    }
    char links[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea links_line = {links, sizeof(links), 0ULL};
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    EMBED_CALL(cli_install_shared.joined, InstallCtx, .line = &links_line, .folder = args->folder,
               .name = CLI_INSTALL_LINKS);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = links))
    {
        mmgr_line_put(args->cfg->err_msg, links);
        mmgr_line_put(args->cfg->err_msg, " could not be made");
        return 0;
    }
    EMBED_CALL(cli_install_shared.joined, InstallCtx, .line = &path_line, .folder = links, .name = args->name);
    (void)unlink(path);
    if (symlink(target, path) != 0)
    {
        EMBED_CALL(cli_install_shared.said, InstallCtx, .cfg = args->cfg, .text = path,
                   .other = " could not be made: run the program from ", .path = args->into);
        return 1;
    }
    EMBED_CALL(cli_install_shared.recorded, InstallCtx, .record = args->record, .text = "link", .other = path);
    EMBED_CALL(cli_install_shared.said, InstallCtx, .cfg = args->cfg, .text = args->name, .other = " answers through ",
               .path = path);
    args->placed = 1;
    return 1;
}

void cli_install_answers(const InstallCtx *args)
{
    EMBED_CALL(cli_install_shared.joined, InstallCtx, .line = args->line, .folder = args->folder,
               .name = CLI_INSTALL_LINKS);
}

void cli_install_home(const InstallCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    home[0] = '\0';
    (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = home,
                     .capacity = sizeof(home));
    mmgr_line_put(args->line, home);
    (void)mmgr_line_finish(args->line);
}

int cli_install_removed_later(const InstallCtx *args)
{
    // a POSIX system removes the folder of a running program at once
    (void)args;
    return 0;
}
#endif
