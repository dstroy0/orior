// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_send.c: what travels written to one file, or laid out in a folder, which, where it is a repository, is
// committed and pushed
#include "../code/git.h"
#include "carry_internal.h"

// The room a sending takes: what travels and the files the folder contains that travel, their cells given back as the
// sending ends
typedef struct
{
    CliCarried packed;
    CliCarried there;
} CliSending;

static CliSending s_send;

typedef struct
{
    VerbaLinea *out;
    unsigned int count;
    const char *how;
    const char *to;
    const char *after;
    VerbaLinea *err_msg;
    const char *folder;
    const char *const *request;
} CarrySendCtx;
// that `count` files went to `to`, reported, with how
EMBED_INLINE void cli_send(const CarrySendCtx *args)
{
    mmgr_line_decimal(args->out, args->count, 0u);
    mmgr_line_put(args->out, args->how);
    mmgr_line_put(args->out, args->to);
    mmgr_line_put(args->out, args->after);
}

// what travels written as one file at `to`: 1, or 0 with why given
EMBED_INLINE int cli_file_send(const CarrySendCtx *args)
{
    // the file written in a cell sized to what travels
    const unsigned long long file_room =
        EMBED_CALL(cli_carry_carried_internal.carried_sent_room, CliCarryCarriedCtx, .carried = &s_send.packed);
    char *const file_cell = g_cli_cells->temporary_buf_alloc(file_room, &g_cli_machine_cells);
    VerbaLinea file = mmgr_line_over(file_cell, file_room);
    if (file.out == NULL)
    {
        mmgr_line_put(args->err_msg, args->to);
        mmgr_line_put(args->err_msg, ": no cell could be taken to write it in");
        return 0;
    }
    mmgr_line_put(&file, "{\n  \"orior\": \"" CLI_CARRY_MARK "\",\n  \"files\": {");
    for (unsigned int at = 0u; at < s_send.packed.count; at += 1u)
    {
        const char *const place = EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                             .carried = &s_send.packed, .at = at);
        mmgr_line_put(&file, (at != 0u) ? ",\n    " : "\n    ");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &file, .text = place,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(&file, ": ");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &file,
                   .text = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                      .carried = &s_send.packed, .at = at),
                   .length = s_send.packed.size[at]);
    }
    mmgr_line_put(&file, (s_send.packed.count != 0u) ? "\n  }\n}\n" : "}\n}\n");
    const int written =
        mmgr_line_ok(&file) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                          .path = args->to, .bytes = file.out, .size = file.at);
    g_cli_cells->persistent_buf_release(file.out, &g_cli_machine_cells);
    if (!written)
    {
        mmgr_line_put(args->err_msg, args->to);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// git run in the folder with `args`: 1 where it succeeded, its words into `out`
EMBED_INLINE int cli_git_in(const CarrySendCtx *args)
{
    const char *const *arguments = args->request;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    char words[CLI_NAME_MAX_LEN * 4u];
    VerbaLinea out = {words, sizeof(words), 0ull};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder,
                      .request = arguments, .count = count, .out = &out, .err_msg = err_msg);
}

// whether the repository at `folder` has a remote
EMBED_INLINE int cli_has_remote(const CarrySendCtx *args)
{
    static const char *const s_remote[1] = {"remote"};
    char remotes[CLI_NAME_MAX_LEN * 4u];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea out = {remotes, sizeof(remotes), 0ull};
    VerbaLinea unrun = {why, sizeof(why), 0ull};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder,
                      .request = s_remote, .count = 1u, .out = &out, .err_msg = &unrun) &&
           (out.at > 0ull);
}

// the folder `to` made to contain exactly what travels: 1, or 0 with why given
EMBED_INLINE int cli_folder_layout(const CarrySendCtx *args)
{
    EMBED_CALL(cli_carry_folder_internal.files, CliCarryFolderCtx, .folder = args->to, .found = &s_send.there);
    for (unsigned int at = 0u; at < s_send.there.count; at += 1u)
    {
        const char *const path = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                            .carried = &s_send.there, .at = at);
        if ((EMBED_CALL(cli_carry_carried_internal.carried_find, CliCarryCarriedCtx, .carried = &s_send.packed,
                        .place = EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                            .carried = &s_send.there, .at = at)) == s_send.packed.count) &&
            !EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, ": it could not be taken out");
            return 0;
        }
    }
    for (unsigned int at = 0u; at < s_send.packed.count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        char parent[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->to,
                   .relative = EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                          .carried = &s_send.packed, .at = at),
                   .joined = path);
        const int made =
            !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = parent) ||
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent);
        if (!made || !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                                 .bytes = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                                     .carried = &s_send.packed, .at = at),
                                 .size = s_send.packed.size[at]))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

// the folder `to`, a repository, committed, and pushed where it has a remote: 1 with what it did reported, or 0
EMBED_INLINE int cli_commit(const CarrySendCtx *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_add[4] = {"add", "-A", "--", "."};
    static const char *const s_diff[3] = {"diff", "--cached", "--quiet"};
    if (!EMBED_CALL(cli_git_in, CarrySendCtx, .folder = args->to, .request = s_add, .count = 4u, .err_msg = err_msg))
    {
        return 0;
    }
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea quiet = {why, sizeof(why), 0ull};
    if (EMBED_CALL(cli_git_in, CarrySendCtx, .folder = args->to, .request = s_diff, .count = 3u, .err_msg = &quiet))
    {
        mmgr_line_put(out, args->to);
        mmgr_line_put(out, " contains these settings already");
        return 1;
    }
    char name[CLI_NAME_MAX_LEN];
#if defined(_WIN32)
    const int named = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "COMPUTERNAME",
                                 .value_out = name, .room = sizeof(name));
#else
    const int named = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOSTNAME",
                                 .value_out = name, .room = sizeof(name));
#endif
    char message[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea line = {message, sizeof(message), 0ull};
    mmgr_line_put(&line, "orior's settings from ");
    mmgr_line_put(&line, named ? name : "a machine");
    mmgr_line_finish(&line);
    const char *const commit[3] = {"commit", "-m", message};
    if (!EMBED_CALL(cli_git_in, CarrySendCtx, .folder = args->to, .request = commit, .count = 3u, .err_msg = err_msg))
    {
        return 0;
    }
    if (EMBED_CALL(cli_has_remote, CarrySendCtx, .folder = args->to))
    {
        char words[CLI_NAME_MAX_LEN * 4u];
        VerbaLinea pushed = {words, sizeof(words), 0ull};
        if (!EMBED_CALL(cli_git_root_out_count_err_msg.push, CliGitRootOutCountErrMsgCfg, .root = args->to,
                        .out = &pushed, .err_msg = err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_send, CarrySendCtx, .out = out, .count = s_send.packed.count, .how = " files committed to ",
                   .to = args->to, .after = " and pushed");
        return 1;
    }
    EMBED_CALL(cli_send, CarrySendCtx, .out = out, .count = s_send.packed.count, .how = " files committed to ",
               .to = args->to, .after = "");
    return 1;
}

int cli_carry_send(const CliCarryHomeToCfg *args)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    int sent =
        EMBED_CALL(cli_carry.pack, CliCarryCfg, .home = args->home, .packed = &s_send.packed, .err_msg = args->err_msg);
    if (sent && !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->to))
    {
        sent = EMBED_CALL(cli_file_send, CarrySendCtx, .to = args->to, .err_msg = args->err_msg);
        if (sent)
        {
            EMBED_CALL(cli_send, CarrySendCtx, .out = args->out, .count = s_send.packed.count,
                       .how = " files written to ", .to = args->to, .after = "");
        }
        EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx,
                   .carried_out = &s_send.packed);
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return sent;
    }
    char git[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->to, .relative = ".git", .joined = git);
    const int repository = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = git) ||
                           EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = git);
    if (sent && repository && EMBED_CALL(cli_has_remote, CarrySendCtx, .folder = args->to))
    {
        char words[CLI_NAME_MAX_LEN * 4u];
        VerbaLinea pulled = {words, sizeof(words), 0ull};
        sent = EMBED_CALL(cli_git_root_out_count_err_msg.pull, CliGitRootOutCountErrMsgCfg, .root = args->to,
                          .out = &pulled, .err_msg = args->err_msg);
    }
    sent = sent && EMBED_CALL(cli_folder_layout, CarrySendCtx, .to = args->to, .err_msg = args->err_msg);
    if (sent && !repository)
    {
        EMBED_CALL(cli_send, CarrySendCtx, .out = args->out, .count = s_send.packed.count, .how = " files written to ",
                   .to = args->to, .after = "");
    }
    else if (sent)
    {
        sent = EMBED_CALL(cli_commit, CarrySendCtx, .to = args->to, .out = args->out, .err_msg = args->err_msg);
    }
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = &s_send.there);
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = &s_send.packed);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return sent;
}
