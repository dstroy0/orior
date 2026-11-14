// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// install.c: orior installed from the folder its programs are in, and taken out again: the license displayed and
// accepted, the programs and the license texts copied, imp and tab put on the PATH where neither name is taken, the
// defaults written, and the record retained that the uninstall reads
#if !defined(_WIN32)
// symlink is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#endif
#include "install.h"
#include "../toolchains/toolchains.h"
#include "../toolchains/toolchains_internal.h"
#include "state.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

const char *const g_cli_install_names[CLI_INSTALL_PROGRAMS] = {"imp", "tab"};

// the max bytes of a license text displayed, of the record read, and of a text the install builds to say
#define INSTALL_LICENSE_BYTES (1u << 18u)
#define INSTALL_RECORD_BYTES (1u << 16u)
#define INSTALL_LINE_BYTES (CLI_PATH_MAX_LEN * 3u)

// the license the programs come under, said where no LICENSE is beside them
#define INSTALL_LICENSE_LINE "AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational"

// the state file the window reads its settings from, whose absence says the install is the first
#define INSTALL_STATE_FILE "state.json"

typedef struct
{
    const CliInstallCfg *cfg;
    const char *text;
    const char *other;
    const char *folder;
    const char *into;
    const char *name;
    const char *path;
    VerbaLinea *line;
    VerbaLinea *record;
    unsigned int at;
    int present[CLI_INSTALL_PROGRAMS];
    int placed;
} InstallCtx;

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
#if defined(_WIN32)
    mmgr_line_put(args->line, ".exe");
#endif
    return mmgr_line_finish(args->line);
}

int cli_install_options(const CliInstallCfg *args)
{
    CliInstallOptions *const options = args->options;
    memor.set(&(MemoriaCfg){.dst = options, .val = 0, .bytes = sizeof(*options)});
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const word = args->words[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--accept", .cap = CLI_POOL_BYTES}))
        {
            options->accepted = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--yes", .cap = CLI_POOL_BYTES}))
        {
            options->yes = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--aliases", .cap = CLI_POOL_BYTES}))
        {
            options->aliases = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--no-imp", .cap = CLI_POOL_BYTES}))
        {
            options->skipped[0] = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--no-tab", .cap = CLI_POOL_BYTES}))
        {
            options->skipped[1] = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--files", .cap = CLI_POOL_BYTES}))
        {
            options->files = 1;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--debug-port", .cap = CLI_POOL_BYTES}))
        {
            const char *const value = (at + 1u < args->count) ? args->words[at + 1u] : NULL;
            int digits = (value != NULL) && (value[0] != '\0');
            for (const char *character = (value != NULL) ? value : ""; digits && (character[0] != '\0'); character += 1)
            {
                digits = (character[0] >= '0') && (character[0] <= '9');
            }
            const int off =
                (value != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = value, .other = "off", .cap = CLI_POOL_BYTES});
            if (!digits && !off)
            {
                mmgr_line_put(args->err_msg, "--debug-port takes a port number or off");
                return 0;
            }
            VerbaLinea port = {options->port, sizeof(options->port), 0ULL};
            mmgr_line_put(&port, value);
            if (mmgr_line_finish(&port) == 0ULL)
            {
                mmgr_line_put(args->err_msg, "--debug-port takes a port number or off");
                return 0;
            }
            at += 1u;
        }
        else if ((word[0] == '-') || (options->folder[0] != '\0'))
        {
            mmgr_line_put(args->err_msg, word);
            mmgr_line_put(args->err_msg, " is not a word install takes: [folder] [--accept] [--yes] [--no-imp] "
                                         "[--no-tab] [--aliases] [--debug-port <port|off>] [--files]");
            return 0;
        }
        else
        {
            VerbaLinea folder = {options->folder, sizeof(options->folder), 0ULL};
            mmgr_line_put(&folder, word);
            if (mmgr_line_finish(&folder) == 0ULL)
            {
                mmgr_line_put(args->err_msg, "the folder's path is too long");
                return 0;
            }
        }
    }
    return 1;
}

int cli_install_default_folder(const CliInstallCfg *args)
{
    VerbaLinea folder = {args->folder_out, CLI_PATH_MAX_LEN, 0ULL};
#if defined(_WIN32)
    const char *const base = args->local_app_data;
    const char *const under = "/Programs/orior";
#else
    const char *const base = args->home;
    const char *const under = "/.local/orior";
#endif
    if ((base == NULL) || (base[0] == '\0'))
    {
        args->folder_out[0] = '\0';
        return 0;
    }
    mmgr_line_put(&folder, base);
    mmgr_line_put(&folder, under);
    return mmgr_line_finish(&folder) != 0ULL;
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
    if ((size != QRY_MEMBER_UNREAD) && (size != 0ULL))
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
#if !defined(_WIN32)
        // the copy carries the bytes alone: the program's mode set so that it runs for the reader and reads for all
        if (!same && (chmod(to, 0755) != 0))
        {
            mmgr_line_put(args->cfg->err_msg, to);
            mmgr_line_put(args->cfg->err_msg, " could not be made to run");
            return 0;
        }
#endif
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

// a line of the record: its type, a tab and its value
EMBED_INLINE void install_recorded(const InstallCtx *args)
{
    mmgr_line_put(args->record, args->text);
    mmgr_line_ch(args->record, '\t');
    mmgr_line_put(args->record, args->other);
    mmgr_line_ch(args->record, '\n');
}

#if defined(_WIN32)
// the shim `into`/bin/`name`.cmd, which runs the program beside its folder with the words it was given
EMBED_INLINE int install_shim_written(InstallCtx *args)
{
    char shims[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char text[CLI_PATH_MAX_LEN];
    VerbaLinea shims_line = {shims, sizeof(shims), 0ULL};
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    VerbaLinea text_line = {text, sizeof(text), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &shims_line, .folder = args->into, .name = CLI_INSTALL_SHIMS);
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
    EMBED_CALL(install_recorded, InstallCtx, .record = args->record, .text = "shim", .other = path);
    EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = args->name, .other = " answers through ",
               .path = path);
    args->placed = 1;
    return 1;
}
#else
// the link ~/.local/bin/`name` to the program, or with --aliases the alias line in the reader's shell profiles
EMBED_INLINE int install_link_made(InstallCtx *args)
{
    char target[CLI_PATH_MAX_LEN];
    VerbaLinea target_line = {target, sizeof(target), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &target_line, .folder = args->into, .name = args->name);
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
        EMBED_CALL(install_recorded, InstallCtx, .record = args->record, .text = "profile", .other = alias);
        EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = args->name,
                   .other = " is an alias in your shell profiles: ", .path = target);
        return 1;
    }
    char links[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea links_line = {links, sizeof(links), 0ULL};
    VerbaLinea path_line = {path, sizeof(path), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &links_line, .folder = args->folder, .name = CLI_INSTALL_LINKS);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = links))
    {
        mmgr_line_put(args->cfg->err_msg, links);
        mmgr_line_put(args->cfg->err_msg, " could not be made");
        return 0;
    }
    EMBED_CALL(install_joined, InstallCtx, .line = &path_line, .folder = links, .name = args->name);
    (void)unlink(path);
    if (symlink(target, path) != 0)
    {
        EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = path,
                   .other = " could not be made: run the program from ", .path = args->into);
        return 1;
    }
    EMBED_CALL(install_recorded, InstallCtx, .record = args->record, .text = "link", .other = path);
    EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = args->name, .other = " answers through ",
               .path = path);
    args->placed = 1;
    return 1;
}
#endif

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
#if defined(_WIN32)
        whole = install_shim_written(args);
#else
        whole = install_link_made(args);
#endif
    }
    if (whole && args->placed)
    {
        char answers[CLI_PATH_MAX_LEN];
        VerbaLinea answers_line = {answers, sizeof(answers), 0ULL};
#if defined(_WIN32)
        EMBED_CALL(install_joined, InstallCtx, .line = &answers_line, .folder = args->into, .name = CLI_INSTALL_SHIMS);
#else
        EMBED_CALL(install_joined, InstallCtx, .line = &answers_line, .folder = args->folder,
                   .name = CLI_INSTALL_LINKS);
#endif
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

// the reader's orior folder made, and the debugging port's setting written: as given, or off on a first install
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
    char state[CLI_PATH_MAX_LEN];
    VerbaLinea state_line = {state, sizeof(state), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &state_line, .folder = home, .name = INSTALL_STATE_FILE);
    const int first = !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = state);
    const char *const port = (args->cfg->options->port[0] != '\0') ? args->cfg->options->port : NULL;
    if ((port == NULL) && !first)
    {
        return 1;
    }
    if (!cli_state_value_save(&(CliStateValueCfg){.folder = home,
                                                  .key = CLI_INSTALL_PORT_KEY,
                                                  .value = (port != NULL) ? port : CLI_INSTALL_PORT_DEFAULT,
                                                  .err_msg = args->cfg->err_msg}))
    {
        return 0;
    }
    EMBED_CALL(install_said, InstallCtx, .cfg = args->cfg, .text = "your files are in ", .other = home,
               .path = (port != NULL) ? "; the debugging port is set" : "; the debugging port is off");
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
    else
    {
        char base[CLI_PATH_MAX_LEN];
        base[0] = '\0';
#if defined(_WIN32)
        (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "LOCALAPPDATA",
                         .value_out = base, .capacity = sizeof(base));
        const int found =
            EMBED_CALL(cli_install_default_folder, CliInstallCfg, .local_app_data = base, .folder_out = into);
#else
        (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME",
                         .value_out = base, .capacity = sizeof(base));
        const int found = EMBED_CALL(cli_install_default_folder, CliInstallCfg, .home = base, .folder_out = into);
#endif
        if (!found)
        {
            mmgr_line_put(args->err_msg, "no default install folder: give the folder to install into");
            return 0;
        }
    }
    char home[CLI_PATH_MAX_LEN];
    home[0] = '\0';
#if !defined(_WIN32)
    (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = home,
                     .capacity = sizeof(home));
#endif
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
    if ((size == QRY_MEMBER_UNREAD) || (size == 0ULL))
    {
        return 0ULL;
    }
    s_record[size] = '\0';
    return size;
}

#if defined(_WIN32)
// The install folder removed after this program has ended, where the program runs from that folder and cannot remove
// itself: a script in the temporary folder waits two seconds, removes the folder and removes itself, run by a shell
// started with a console of its own and no job, so that it outlives this process
EMBED_INLINE int install_folder_removed_after(const InstallCtx *args)
{
    char temporary[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "TEMP",
                    .value_out = temporary, .capacity = sizeof(temporary)))
    {
        memor.cpy(&(MemoriaCfg){.dst = temporary, .src = args->folder, .bytes = sizeof(temporary)});
    }
    char script[CLI_PATH_MAX_LEN];
    VerbaLinea script_line = {script, sizeof(script), 0ULL};
    EMBED_CALL(install_joined, InstallCtx, .line = &script_line, .folder = temporary, .name = "orior-uninstall.cmd");
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
        return 0;
    }
    const char *const words[4] = {"cmd.exe", "/c", script, NULL};
    unsigned int id = 0u;
    if (!EMBED_CALL(cli_words_id_internal.started_outside, CliWordsIdCtx, .words = words, .id = &id))
    {
        mmgr_line_put(args->cfg->err_msg, "the shell that removes the folder could not be started");
        return 0;
    }
    return 1;
}
#endif

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
        char base[CLI_PATH_MAX_LEN];
        base[0] = '\0';
#if defined(_WIN32)
        (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "LOCALAPPDATA",
                         .value_out = base, .capacity = sizeof(base));
        (void)EMBED_CALL(cli_install_default_folder, CliInstallCfg, .local_app_data = base, .folder_out = into);
#else
        (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME",
                         .value_out = base, .capacity = sizeof(base));
        (void)EMBED_CALL(cli_install_default_folder, CliInstallCfg, .home = base, .folder_out = into);
#endif
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
    char *from = s_record;
    for (char *place = s_record; place <= s_record + size; place += 1)
    {
        if ((place[0] != '\n') && (place[0] != '\0'))
        {
            continue;
        }
        place[0] = '\0';
        char *value = from;
        while ((value[0] != '\t') && (value[0] != '\0'))
        {
            value += 1;
        }
        if (value[0] == '\t')
        {
            value[0] = '\0';
            value += 1;
            if (cellul.eq(&(CatenaFinitaCfg){.src = from, .other = "into", .cap = CLI_POOL_BYTES}))
            {
                memor.cpy(&(MemoriaCfg){.dst = into, .src = value, .bytes = sizeof(into)});
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
                memor.cpy(&(MemoriaCfg){.dst = folder, .src = value, .bytes = sizeof(folder)});
                (void)EMBED_CALL(cli_toolchains_err_msg_folder.path_folder_remove, CliToolchainsErrMsgFolderCfg,
                                 .folder = folder, .err_msg = args->err_msg);
                EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = value, .other = " is off your PATH");
            }
        }
        from = place + 1;
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
        char home[CLI_PATH_MAX_LEN];
        home[0] = '\0';
#if defined(_WIN32)
        (void)EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "USERPROFILE",
                         .value_out = home, .capacity = sizeof(home));
        if (EMBED_CALL(install_same_folder, InstallCtx, .folder = own, .other = into))
        {
            if (!EMBED_CALL(install_folder_removed_after, InstallCtx, .cfg = args, .into = into,
                            .folder = (home[0] != '\0') ? home : "."))
            {
                return 0;
            }
            EMBED_CALL(install_said, InstallCtx, .cfg = args, .text = into,
                       .other = " is removed a moment after this program ends");
        }
        else
#endif
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
