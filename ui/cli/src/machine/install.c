// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// install.c: orior installed from the folder its programs are in, and taken out again: the license displayed and
// accepted, the programs and the license texts copied, imp and tab put on the PATH where neither name is taken, the
// defaults written, and the record retained that the uninstall reads. Each system's part is in its platform file,
// windows_install.c and posix_install.c
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "../toolchains/toolchains_internal.h"
#include "install_internal.h"
#include "state.h"

const char *const g_cli_install_names[CLI_INSTALL_PROGRAMS] = {"imp", "tab"};

// the max bytes of a license text displayed, of the record read, and of a text the install builds to say
#define INSTALL_LICENSE_BYTES (1u << 18u)
#define INSTALL_RECORD_BYTES (1u << 16u)
#define INSTALL_LINE_BYTES (CLI_PATH_MAX_LEN * 3u)

// the license the programs come under, said where no LICENSE is beside them
#define INSTALL_LICENSE_LINE "AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational"

static char s_license[INSTALL_LICENSE_BYTES];
static char s_record[INSTALL_RECORD_BYTES];
static char s_said[INSTALL_LINE_BYTES];

// a line of the account passed to the caller, made of up to three texts
EMBED_INLINE void install_said(const InstallCtx *args)
{
    VerbaLinea line = {s_said, sizeof(s_said), 0ULL};
    mmgr_line_put(&line, args->text);
    if (args->other != NULL)
    {
        mmgr_line_put(&line, args->other);
    }
    if (args->path != NULL)
    {
        mmgr_line_put(&line, args->path);
    }
    (void)mmgr_line_finish(&line);
    args->cfg->say(args->cfg->context, s_said);
}

// `folder`/`name` into `line`
EMBED_INLINE unsigned long long install_joined(const InstallCtx *args)
{
    mmgr_line_put(args->line, args->folder);
    mmgr_line_ch(args->line, '/');
    mmgr_line_put(args->line, args->name);
    return mmgr_line_finish(args->line);
}

// whether `folder` and `other` name one folder, each compared by its key
EMBED_INLINE int install_same_folder(const InstallCtx *args)
{
    char one[CLI_PATH_MAX_LEN];
    char two[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = args->folder, .key = one);
    EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = args->other, .key = two);
    return cellul.eq(&(CatenaFinitaCfg){.src = one, .other = two, .cap = CLI_POOL_BYTES});
}

// the file name of the program `at` on this system
EMBED_INLINE unsigned long long install_program_file(const InstallCtx *args)
{
    mmgr_line_put(args->line, g_cli_install_names[args->at]);
    mmgr_line_put(args->line, g_cli_system_facts.program_end);
    return mmgr_line_finish(args->line);
}

// a line of the record: its type, a tab and its value
EMBED_INLINE void install_recorded(const InstallCtx *args)
{
    mmgr_line_put(args->record, args->text);
    mmgr_line_ch(args->record, '\t');
    mmgr_line_put(args->record, args->other);
    mmgr_line_ch(args->record, '\n');
}

void cli_install_said(const InstallCtx *args)
{
    install_said(args);
}

unsigned long long cli_install_joined(const InstallCtx *args)
{
    return install_joined(args);
}

void cli_install_recorded(const InstallCtx *args)
{
    install_recorded(args);
}

int cli_install_same_folder(const InstallCtx *args)
{
    return install_same_folder(args);
}

int cli_install_options(const CliInstallCfg *args)
{
    CliInstallOptions *const options = args->options;
    memor.set(&(MemoriaCfg){.dst = options, .val = 0, .bytes = sizeof(*options)});
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const given = args->words[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "--accept", .cap = CLI_POOL_BYTES}))
        {
            options->accepted = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "--yes", .cap = CLI_POOL_BYTES}))
        {
            options->yes = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "--aliases", .cap = CLI_POOL_BYTES}))
        {
            options->aliases = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "--no-imp", .cap = CLI_POOL_BYTES}))
        {
            options->skipped[0] = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "--no-tab", .cap = CLI_POOL_BYTES}))
        {
            options->skipped[1] = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "--files", .cap = CLI_POOL_BYTES}))
        {
            options->files = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = given, .other = "--checks-host", .cap = CLI_POOL_BYTES}))
        {
            // the Linux host the window's checks run on takes the next word, as user@host
            VerbaLinea host = {options->checks_host, sizeof(options->checks_host), 0ULL};
            if (at + 1u < args->count)
            {
                at += 1u;
                mmgr_line_put(&host, args->words[at]);
            }
            if (mmgr_line_finish(&host) == 0ULL)
            {
                mmgr_line_put(args->err_msg, "--checks-host takes the host after it, as user@host");
                return 0;
            }
        }
        else if ((given[0] == '-') || (options->folder[0] != '\0'))
        {
            mmgr_line_put(args->err_msg, given);
            mmgr_line_put(args->err_msg, " is not a word install takes: [folder] [--accept] [--yes] [--no-imp] "
                                         "[--no-tab] [--aliases] [--checks-host user@host] [--files]");
            return 0;
        }
        else
        {
            VerbaLinea folder = {options->folder, sizeof(options->folder), 0ULL};
            mmgr_line_put(&folder, given);
            if (mmgr_line_finish(&folder) == 0ULL)
            {
                mmgr_line_put(args->err_msg, "the folder's path is too long");
                return 0;
            }
        }
    }
    return 1;
}

// the file names a program `name` answers to in a PATH folder on this system
static const char *const s_program_ends[] = {"", ".exe", ".cmd"};

unsigned int cli_install_collision(const CliInstallCfg *args)
{
    for (unsigned int at = 0u; at < args->folder_count; at += 1u)
    {
        const char *const folder = args->folders[at];
        if ((folder[0] == '\0') ||
            ((args->own != NULL) &&
             EMBED_CALL(install_same_folder, InstallCtx, .folder = folder, .other = args->own)) ||
            ((args->into != NULL) &&
             EMBED_CALL(install_same_folder, InstallCtx, .folder = folder, .other = args->into)))
        {
            continue;
        }
        for (unsigned int end = 0u; end < sizeof(s_program_ends) / sizeof(s_program_ends[0]); end += 1u)
        {
            char path[CLI_PATH_MAX_LEN];
            VerbaLinea line = {path, sizeof(path), 0ULL};
            mmgr_line_put(&line, folder);
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, args->name);
            mmgr_line_put(&line, s_program_ends[end]);
            if ((mmgr_line_finish(&line) != 0ULL) &&
                EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
            {
                return at + 1u;
            }
        }
    }
    return 0u;
}

// the folder the running program is in, into `folder_out`
EMBED_INLINE int install_own_folder(const InstallCtx *args)
{
    int on_path = 0;
    return EMBED_CALL(cli_toolchains_err_msg_folder_on_path.own, CliToolchainsErrMsgFolderOnPathCfg,
                      .folder = args->cfg->folder_out, .on_path = &on_path, .err_msg = args->cfg->err_msg);
}

// the license displayed from LICENSE beside the program, else its one line, and the reader's acceptance taken unless
// the words accepted it: 1, or 0 where it was not accepted
EMBED_INLINE int install_license(const InstallCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &line, .folder = args->folder, .name = "LICENSE");
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_license, sizeof(s_license) - 1u);
    if ((size != QRY_MEMBER_UNREAD) && (size != 0ULL) && (size < sizeof(s_license)))
    {
        s_license[size] = '\0';
        args->cfg->say(args->cfg->context, s_license);
    }
    else
    {
        EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = "orior comes under ",
                   .other = INSTALL_LICENSE_LINE, .path = "; the texts are in LICENSES beside the sources");
    }
    if (args->cfg->options->accepted)
    {
        return 1;
    }
    if (!args->cfg->asked(args->cfg->context, "Do you accept the license? [y/N]"))
    {
        mmgr_line_put(args->cfg->err_msg, "the license was not accepted; --accept accepts it on the command line");
        return 0;
    }
    return 1;
}

// each entry of the LICENSES folder copied into the install folder's LICENSES
static void install_license_copied(const CliEachNameIsFolderCfg *args)
{
    const InstallCtx *const ctx = (const InstallCtx *)args->context;
    if (args->is_folder)
    {
        return;
    }
    char from[CLI_PATH_MAX_LEN];
    char to[CLI_PATH_MAX_LEN];
    VerbaLinea from_line = {from, sizeof(from), 0ULL};
    VerbaLinea to_line = {to, sizeof(to), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &from_line, .folder = ctx->folder, .name = args->name);
    EMBED_CALL(install_joined, InstallCtx, .line = &to_line, .folder = ctx->into, .name = args->name);
    (void)EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = from, .to = to);
}

// the programs and the license texts copied from `folder` into `into`, `present` marked for each program found; a
// missing impero refuses, a missing tabvla installs the command line alone
EMBED_INLINE int install_copied(InstallCtx *args)
{
    const int same = EMBED_CALL(install_same_folder, InstallCtx, .folder = args->folder, .other = args->into);
    if (!same && !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->into))
    {
        mmgr_line_put(args->cfg->err_msg, args->into);
        mmgr_line_put(args->cfg->err_msg, " could not be made");
        return 0;
    }
    for (unsigned int at = 0u; at < CLI_INSTALL_PROGRAMS; at += 1u)
    {
        char file[64];
        char from[CLI_PATH_MAX_LEN];
        char to[CLI_PATH_MAX_LEN];
        VerbaLinea file_line = {file, sizeof(file), 0ULL};
        VerbaLinea from_line = {from, sizeof(from), 0ULL};
        VerbaLinea to_line = {to, sizeof(to), 0ULL};
        EMBED_CALL(install_program_file, InstallCtx, .line = &file_line, .at = at);
        EMBED_CALL(install_joined, InstallCtx, .line = &from_line, .folder = args->folder, .name = file);
        EMBED_CALL(install_joined, InstallCtx, .line = &to_line, .folder = args->into, .name = file);
        args->present[at] = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = from);
        if (!args->present[at])
        {
            if (at == 0u)
            {
                mmgr_line_put(args->cfg->err_msg, from);
                mmgr_line_put(args->cfg->err_msg, " is not there: the install runs from the folder impero is in");
                return 0;
            }
            EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg,
                       .text = "tabvla is not beside impero: the command line alone is installed");
            continue;
        }
        if (!same && !EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = from, .to = to))
        {
            mmgr_line_put(args->cfg->err_msg, to);
            mmgr_line_put(args->cfg->err_msg, " could not be written");
            return 0;
        }
        // the copy carries the bytes alone: the system makes the program runnable by its owner
        if (!same && !EMBED_CALL(cli_install_system.runnable, InstallCtx, .path = to))
        {
            mmgr_line_put(args->cfg->err_msg, to);
            mmgr_line_put(args->cfg->err_msg, " could not be made to run");
            return 0;
        }
    }
    if (same)
    {
        return 1;
    }
    char from[CLI_PATH_MAX_LEN];
    char to[CLI_PATH_MAX_LEN];
    VerbaLinea from_line = {from, sizeof(from), 0ULL};
    VerbaLinea to_line = {to, sizeof(to), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &from_line, .folder = args->folder, .name = "LICENSE");
    EMBED_CALL(install_joined, InstallCtx, .line = &to_line, .folder = args->into, .name = "LICENSE");
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = from))
    {
        (void)EMBED_CALL(query_from_internal.file_copy, QueryFromCtx, .from = from, .to = to);
    }
    VerbaLinea texts_from = {from, sizeof(from), 0ULL};
    VerbaLinea texts_to = {to, sizeof(to), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &texts_from, .folder = args->folder, .name = "LICENSES");
    EMBED_CALL(install_joined, InstallCtx, .line = &texts_to, .folder = args->into, .name = "LICENSES");
    if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = from) &&
        EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = to))
    {
        InstallCtx texts = {.cfg = args->cfg, .folder = from, .into = to};
        (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = from,
                         .each = install_license_copied, .context_bytess = &texts);
    }
    return 1;
}

// imp and tab put on the PATH, each name that is wanted, present and not taken by another program's folder; then the
// folder they answer from put on the PATH where it is not on it
EMBED_INLINE int install_placed(InstallCtx *args)
{
    const unsigned int count = cli_toolchains_find_path();
    int whole = 1;
    for (unsigned int at = 0u; whole && (at < CLI_INSTALL_PROGRAMS); at += 1u)
    {
        const char *const name = g_cli_install_names[at];
        if (args->cfg->options->skipped[at] || !args->present[at])
        {
            continue;
        }
        const unsigned int taken = EMBED_CALL(cli_install_collision, CliInstallCfg, .name = name,
                                              .folders = (const char (*)[CLI_PATH_MAX_LEN])g_cli_toolchains_path,
                                              .folder_count = count, .own = args->folder, .into = args->into);
        if (taken != 0u)
        {
            EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = name,
                       .other = " already answers to a program in ", .path = g_cli_toolchains_path[taken - 1u]);
            EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = "  it is left to that program; --no-",
                       .other = name, .path = " leaves it out without this note");
            continue;
        }
        args->name = name;
        whole = cli_install_system.place(args);
    }
    if (whole && args->placed)
    {
        char answers[CLI_PATH_MAX_LEN];
        VerbaLinea answers_line = {answers, sizeof(answers), 0ULL};
        EMBED_CALL(cli_install_system.answers, InstallCtx, .line = &answers_line, .folder = args->folder,
                   .into = args->into);
        int on_path = 0;
        for (unsigned int at = 0u; !on_path && (at < count); at += 1u)
        {
            on_path =
                EMBED_CALL(install_same_folder, InstallCtx, .folder = g_cli_toolchains_path[at], .other = answers);
        }
        if (!on_path)
        {
            char folder[CLI_PATH_MAX_LEN];
            memor.cpy(&(MemoriaCfg){.dst = folder, .src = answers, .bytes = sizeof(folder)});
            whole = EMBED_CALL(cli_toolchains_err_msg_folder.path_folder_add, CliToolchainsErrMsgFolderCfg,
                               .folder = folder, .err_msg = args->cfg->err_msg);
            if (whole)
            {
                EMBED_CALL(install_recorded, InstallCtx, .record = args->record, .text = "path", .other = answers);
                EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = answers,
                           .other = " is on your PATH for every terminal started from now");
            }
        }
    }
    EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = NULL);
    return whole;
}

// the reader's orior folder made, where the settings, themes and terminal logs go; and the Linux host the window's
// checks run on saved as the setting checks.host where the install named one
EMBED_INLINE int install_defaults(const InstallCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        mmgr_line_put(args->cfg->err_msg, "orior's folder could not be found");
        return 0;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = home))
    {
        mmgr_line_put(args->cfg->err_msg, home);
        mmgr_line_put(args->cfg->err_msg, " could not be made");
        return 0;
    }
    if (args->cfg->options->checks_host[0] != '\0')
    {
        if (!cli_state_value_save(&(CliStateValueCfg){.folder = home,
                                                      .key = CLI_INSTALL_CHECKS_HOST_KEY,
                                                      .value = args->cfg->options->checks_host,
                                                      .err_msg = args->cfg->err_msg}))
        {
            return 0;
        }
        EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = "the window's checks run on ",
                   .other = args->cfg->options->checks_host, .path = "");
    }
    EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = "your files are in ", .other = home, .path = "");
    return 1;
}

int cli_install_run(const CliInstallCfg *args)
{
    char own[CLI_PATH_MAX_LEN];
    char into[CLI_PATH_MAX_LEN];
    InstallCtx ctx = {.cfg = args, .folder = own, .into = into};
    CliInstallCfg own_cfg = *args;
    own_cfg.folder_out = own;
    if (!EMBED_CALL(install_own_folder, InstallCtx, .cfg = &own_cfg))
    {
        return 0;
    }
    if (!install_license(&ctx))
    {
        return 0;
    }
    if (args->options->folder[0] != '\0')
    {
        memor.cpy(&(MemoriaCfg){.dst = into, .src = args->options->folder, .bytes = sizeof(into)});
    }
    else if (!EMBED_CALL(cli_install_system.default_found, CliInstallCfg, .folder_out = into))
    {
        mmgr_line_put(args->err_msg, "no default install folder: give the folder to install into");
        return 0;
    }
    char home[CLI_PATH_MAX_LEN];
    VerbaLinea home_line = {home, sizeof(home), 0ULL};
    EMBED_CALL(cli_install_system.home, InstallCtx, .line = &home_line);
    EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = "orior is installed into ", .other = into);
    if (!install_copied(&ctx))
    {
        return 0;
    }
    VerbaLinea record = {s_record, sizeof(s_record), 0ULL};
    EMBED_CALL(install_recorded, InstallCtx, .record = &record, .text = "into", .other = into);
    ctx.record = &record;
    // the links are under the reader's home; the shims under the install folder, which `into` names
    InstallCtx placed = ctx;
    placed.folder = (home[0] != '\0') ? home : own;
    if (!install_placed(&placed))
    {
        return 0;
    }
    if (!install_defaults(&ctx))
    {
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &path_line, .folder = into, .name = CLI_INSTALL_RECORD);
    const unsigned long long size = mmgr_line_finish(&record);
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_record,
                    .size = size))
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, " could not be written");
        return 0;
    }
    EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = "done: imp file uninstall takes it out again");
    return 1;
}

// the record read from `path` into s_record: its size, or 0 where there is none
EMBED_INLINE unsigned long long install_record_read(const InstallCtx *args)
{
    const unsigned long long size = qry_member_file_read(args->path, (unsigned char *)s_record, sizeof(s_record) - 1u);
    if ((size == QRY_MEMBER_UNREAD) || (size == 0ULL) || (size >= sizeof(s_record)))
    {
        return 0ULL;
    }
    s_record[size] = '\0';
    return size;
}

int cli_uninstall_run(const CliInstallCfg *args)
{
    char own[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char into[CLI_PATH_MAX_LEN];
    CliInstallCfg own_cfg = *args;
    own_cfg.folder_out = own;
    if (!EMBED_CALL(install_own_folder, InstallCtx, .cfg = &own_cfg))
    {
        return 0;
    }
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &path_line, .folder = own, .name = CLI_INSTALL_RECORD);
    unsigned long long size = EMBED_CALL(install_record_read, InstallCtx, .path = path);
    if (size == 0ULL)
    {
        (void)EMBED_CALL(cli_install_system.default_found, CliInstallCfg, .folder_out = into);
        VerbaLinea again = {path, sizeof(path), 0ULL};
        EMBED_CALL(install_joined, InstallCtx, .line = &again, .folder = into, .name = CLI_INSTALL_RECORD);
        size = EMBED_CALL(install_record_read, InstallCtx, .path = path);
    }
    if (size == 0ULL)
    {
        mmgr_line_put(args->err_msg, "no install record is found beside the program or in the default folder");
        return 0;
    }
    into[0] = '\0';
    // each line of the record undone: the links, the shims and the profile lines removed, the PATH folder taken off
    for (unsigned long long at = 0ULL; at <= size;)
    {
        // the line from `at` to its newline, or to the record's end, and its type before its tab
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = s_record, .at = at, .size = size, .set = "\n");
        s_record[end] = '\0';
        const char *const from = s_record + at;
        const unsigned long long tab = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = s_record, .at = at, .size = end, .set = "\t");
        at = end + 1ULL;
        if (tab < end)
        {
            s_record[tab] = '\0';
            const char *const value = s_record + tab + 1ULL;
            if (cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "into", .cap = CLI_POOL_BYTES}))
            {
                VerbaLinea into_line = {into, sizeof(into), 0ULL};
                mmgr_line_put(&into_line, value);
                (void)mmgr_line_finish(&into_line);
            }
            else if (cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "link", .cap = CLI_POOL_BYTES}) ||
                     cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "shim", .cap = CLI_POOL_BYTES}))
            {
                (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = value);
                EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = value, .other = " is removed");
            }
            else if (cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "profile", .cap = CLI_POOL_BYTES}))
            {
                (void)EMBED_CALL(cli_toolchains_err_msg_folder.profile_line_remove, CliToolchainsErrMsgFolderCfg,
                                 .what = value, .err_msg = args->err_msg);
                EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = "the alias ", .other = value,
                           .path = " is out of your shell profiles");
            }
            else if (cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "path", .cap = CLI_POOL_BYTES}))
            {
                char folder[CLI_PATH_MAX_LEN];
                VerbaLinea folder_line = {folder, sizeof(folder), 0ULL};
                mmgr_line_put(&folder_line, value);
                (void)mmgr_line_finish(&folder_line);
                (void)EMBED_CALL(cli_toolchains_err_msg_folder.path_folder_remove, CliToolchainsErrMsgFolderCfg,
                                 .folder = folder, .err_msg = args->err_msg);
                EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = value, .other = " is off your PATH");
            }
        }
    }
    if (into[0] == '\0')
    {
        mmgr_line_put(args->err_msg, "the record names no install folder");
        return 0;
    }
    char question[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea question_line = {question, sizeof(question), 0ULL};
    mmgr_line_put(&question_line, "Remove ");
    mmgr_line_put(&question_line, into);
    mmgr_line_put(&question_line, " and everything in it? [y/N]");
    (void)mmgr_line_finish(&question_line);
    if (args->options->yes || args->asked(args->context, question))
    {
        // a system can remove the folder of the running program only after the program ends
        const int later =
            EMBED_CALL(cli_install_system.removed_later, InstallCtx, .cfg = args, .into = into, .folder = own);
        if (later < 0)
        {
            return 0;
        }
        if (later == 0)
        {
            if (!EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = into))
            {
                mmgr_line_put(args->err_msg, into);
                mmgr_line_put(args->err_msg, " could not be removed whole");
                return 0;
            }
            EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = into, .other = " is removed");
        }
    }
    if (!args->options->files)
    {
        return 1;
    }
    char files[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = files))
    {
        return 1;
    }
    VerbaLinea files_line = {question, sizeof(question), 0ULL};
    mmgr_line_put(&files_line, "Remove your orior folder ");
    mmgr_line_put(&files_line, files);
    mmgr_line_put(&files_line, " with your settings, themes and terminal logs? [y/N]");
    (void)mmgr_line_finish(&files_line);
    if (args->options->yes || args->asked(args->context, question))
    {
        if (!EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = files))
        {
            mmgr_line_put(args->err_msg, files);
            mmgr_line_put(args->err_msg, " could not be removed whole");
            return 0;
        }
        EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = files, .other = " is removed");
    }
    return 1;
}
