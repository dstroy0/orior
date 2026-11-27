// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_install.c: orior's install on Windows. The default folder is under the reader's local application data. A
// name reaches the PATH as a .cmd shim in the install folder's bin. The folder of the running program goes after the
// program ends
#include "../machine/install_internal.h"

#if defined(_WIN32)

int cli_install_default_folder(const CliInstallCfg *args)
{
    VerbaLinea folder = {args->folder_out, CLI_PATH_MAX_LEN, 0ULL};
    const char *const base = args->local_app_data;
    if ((base == NULL) || (base[0] == '\0'))
    {
        args->folder_out[0] = '\0';
        return 0;
    }
    mmgr_line_put(&folder, base);
    mmgr_line_put(&folder, "/Programs/orior");
    return mmgr_line_finish(&folder) != 0ULL;
}

int cli_install_default_found(const CliInstallCfg *args)
{
    char base[CLI_PATH_MAX_LEN];
    base[0] = '\0';
    (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "LOCALAPPDATA",
                     .value_out = base, .capacity = sizeof(base));
    return EMBED_CALL(cli_install_default_folder, CliInstallCfg, .local_app_data = base,
                      .folder_out = args->folder_out);
}

int cli_install_runnable(const InstallCtx *args)
{
    // a copy of a program on Windows runs as it is
    (void)args;
    return 1;
}

// the shim `into`/bin/`name`.cmd, which runs the program beside its folder with the words that the shim gets
int cli_install_place(InstallCtx *args)
{
    char shims[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char text[CLI_PATH_MAX_LEN];
    VerbaLinea shims_line = {shims, sizeof(shims), 0ULL};
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    VerbaLinea text_line = {text, sizeof(text), 0ULL};
    EMBED_CALL(cli_install_shared.joined, InstallCtx, .line = &shims_line, .folder = args->into,
               .name = CLI_INSTALL_SHIMS);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = shims))
    {
        mmgr_line_put(args->cfg->err_msg, shims);
        mmgr_line_put(args->cfg->err_msg, " could not be made");
        return 0;
    }
    mmgr_line_put(&path_line, shims);
    mmgr_line_ch(&path_line, '/');
    mmgr_line_put(&path_line, args->name);
    mmgr_line_put(&path_line, ".cmd");
    (void)mmgr_line_finish(&path_line);
    mmgr_line_put(&text_line, "@\"%~dp0..\\");
    mmgr_line_put(&text_line, args->name);
    mmgr_line_put(&text_line, ".exe\" %*\r\n");
    const unsigned long long size = mmgr_line_finish(&text_line);
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                    .size = size))
    {
        mmgr_line_put(args->cfg->err_msg, path);
        mmgr_line_put(args->cfg->err_msg, " could not be written");
        return 0;
    }
    EMBED_CALL(cli_install_shared.recorded, InstallCtx, .record = args->record, .text = "shim", .other = path);
    EMBED_CALL(cli_install_shared.said, InstallCtx, .cfg = args->cfg, .text = args->name, .other = " answers through ",
               .path = path);
    args->placed = 1;
    return 1;
}

void cli_install_answers(const InstallCtx *args)
{
    EMBED_CALL(cli_install_shared.joined, InstallCtx, .line = args->line, .folder = args->into,
               .name = CLI_INSTALL_SHIMS);
}

void cli_install_home(const InstallCtx *args)
{
    // the shims are under the install folder, and no link goes under the reader's home
    (void)mmgr_line_finish(args->line);
}

// The install folder removed after this program ends, where the program runs from that folder and cannot remove
// itself. A script in the temporary folder waits two seconds, removes the folder and removes itself. A shell runs it,
// started with a console of its own and no job, and the shell outlives this process
int cli_install_removed_later(const InstallCtx *args)
{
    if (!EMBED_CALL(cli_install_shared.same_folder, InstallCtx, .folder = args->folder, .other = args->into))
    {
        return 0;
    }
    char home[CLI_PATH_MAX_LEN];
    home[0] = '\0';
    (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "USERPROFILE",
                     .value_out = home, .capacity = sizeof(home));
    char temporary[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "TEMP",
                    .value_out = temporary, .capacity = sizeof(temporary)))
    {
        VerbaLinea temporary_line = {temporary, sizeof(temporary), 0ULL};
        mmgr_line_put(&temporary_line, (home[0] != '\0') ? home : ".");
        (void)mmgr_line_finish(&temporary_line);
    }
    char script[CLI_PATH_MAX_LEN];
    VerbaLinea script_line = {script, sizeof(script), 0ULL};
    EMBED_CALL(cli_install_shared.joined, InstallCtx, .line = &script_line, .folder = temporary,
               .name = "orior-uninstall.cmd");
    char text[CLI_PATH_MAX_LEN + 128u];
    VerbaLinea text_line = {text, sizeof(text), 0ULL};
    mmgr_line_put(&text_line, "@echo off\r\nping -n 3 127.0.0.1 >nul\r\nrmdir /s /q \"");
    mmgr_line_put(&text_line, args->into);
    mmgr_line_put(&text_line, "\"\r\ndel \"%~f0\"\r\n");
    const unsigned long long size = mmgr_line_finish(&text_line);
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = script, .bytes = text,
                    .size = size))
    {
        mmgr_line_put(args->cfg->err_msg, script);
        mmgr_line_put(args->cfg->err_msg, " could not be written");
        return -1;
    }
    const char *const words[4] = {"cmd.exe", "/c", script, NULL};
    unsigned int id = 0u;
    if (!EMBED_CALL(cli_words_id_internal.started_outside, CliWordsIdCtx, .words = words, .id = &id))
    {
        mmgr_line_put(args->cfg->err_msg, "the shell that removes the folder could not be started");
        return -1;
    }
    EMBED_CALL(cli_install_shared.said, InstallCtx, .cfg = args->cfg, .text = args->into,
               .other = " is removed a moment after this program ends");
    return 1;
}
#endif
