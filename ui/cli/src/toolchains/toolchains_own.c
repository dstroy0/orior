// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_own.c: orior's own folder and whether it is on the PATH, and a folder added to the reader's own PATH
#include "../text/text.h"
#include "toolchains_internal.h"

int cli_toolchains_own(const CliToolchainsErrMsgFolderOnPathCfg *args)
{
    int *on_path = args->on_path;
    char program[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_capacity_internal.program_own, CliPathCapacityCtx, .path_out = program,
                    .capacity_bytes = sizeof(program)))
    {
        mmgr_line_put(args->err_msg, "orior's program could not be found");
        return 0;
    }
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = program, .full = full,
                    .capacity = sizeof(full)))
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = program,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_POOL_BYTES}) + 1u});
    }
    char *cut = full + cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_POOL_BYTES});
    cut = full + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = full,
                            .at = (unsigned long long)(cut - full), .set = "/\\");
    if (cut == full)
    {
        mmgr_line_put(args->err_msg, "orior's program has no folder");
        return 0;
    }
    cut[-1] = '\0';
    memor.cpy(&(MemoriaCfg){.dst = args->folder,
                            .src = full,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_POOL_BYTES}) + 1u});
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = args->folder, .key = key);
    const unsigned int count = cli_toolchains_find_path();
    *on_path = 0;
    for (unsigned int at = 0u; !*on_path && (at < count); at += 1u)
    {
        char saved_path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = g_cli_toolchains_path[at],
                   .key = saved_path);
        *on_path = cellul.eq(&(CatenaFinitaCfg){.src = saved_path, .other = key, .cap = CLI_POOL_BYTES});
    }
    EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = NULL);
    return 1;
}

typedef struct
{
    const char *folder;
    VerbaLinea *err_msg;
} ToolchainsOwnCtx;
// A folder added to the reader's own PATH: on Windows the reader's Path in the registry, stored as text whose variables
// expand, and every open program notified it changed; elsewhere a line at the end of ~/.profile, and of ~/.zprofile on
// macOS, and of ~/.bash_profile and ~/.bash_login where they are there
EMBED_INLINE int cli_add_folder(const ToolchainsOwnCtx *args)
{
    const char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
#if defined(_WIN32)
    static char s_now[CLI_PATH_TEXT];
    static char s_next[CLI_PATH_TEXT];
    if (!EMBED_CALL(cli_out_capacity_internal.registry_user_value, CliOutCapacityCtx, .name = "Path", .out = s_now,
                    .capacity = sizeof(s_now)))
    {
        s_now[0] = '\0';
    }
    if (EMBED_CALL(cli_toolchains_folder.join, CliToolchainsFolderCfg, .path_read = s_now, .folder_read = folder,
                   .out_capacity = s_next, .size = sizeof(s_next)))
    {
        if (!EMBED_CALL(cli_err_msg_name_internal.registry_set_user_value, CliErrMsgNameCtx, .name = "Path",
                        .value = s_next, .err_msg = err_msg))
        {
            return 0;
        }
        cli_registry_announce();
    }
    return 1;
#else
    static char s_file[1u << 20u];
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = home,
                    .capacity = sizeof(home)))
    {
        mmgr_line_put(err_msg, "HOME is not set");
        return 0;
    }
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea made = {line, sizeof(line), 0ULL};
    mmgr_line_put(&made, "export PATH=\"$PATH:");
    EMBED_CALL(cli_text_size_set.append_mark, CliTextSizeSetCfg, .out = &made, .text = folder, .set = "\"",
               .mark = '\\');
    mmgr_line_ch(&made, '"');
    const unsigned long long line_length = mmgr_line_finish(&made);
    static const char *const s_names[4] = {"/.profile", "/.zprofile", "/.bash_profile", "/.bash_login"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea named = {path, sizeof(path), 0ULL};
        mmgr_line_put(&named, home);
        mmgr_line_put(&named, s_names[at]);
        mmgr_line_finish(&named);
#if !defined(__APPLE__)
        if (at == 1u)
        {
            continue;
        }
#endif
        if ((at >= 2u) && !EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
        {
            continue;
        }
        unsigned long long size =
            qry_member_file_read(path, (unsigned char *)s_file, sizeof(s_file) - line_length - 4u);
        size = (size == QRY_MEMBER_UNREAD) ? 0ULL : size;
        // a line already there, trimmed, is retained as it is
        int there = 0;
        const char *from = s_file;
        const char *place = s_file;
        while (!there && (place <= (s_file + size)))
        {
            if ((place == (s_file + size)) || (place[0] == '\n'))
            {
                const char *start = from;
                const char *end = place;
                start += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = start,
                                    .at = 0ULL, .size = (unsigned long long)(end - start), .set = CLI_TEXT_WHITE);
                end = start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = start,
                                         .at = (unsigned long long)(end - start), .set = CLI_TEXT_WHITE);
                there = ((unsigned long long)(end - start) == line_length) &&
                        (memor.cmp(&(MemoriaCfg){.src = start, .other = line, .bytes = line_length}) == 0);
                from = place + 1;
            }
            place += 1;
        }
        if (there)
        {
            continue;
        }
        VerbaLinea text = {s_file, sizeof(s_file), size};
        if ((size != 0ULL) && (s_file[size - 1u] != '\n'))
        {
            mmgr_line_ch(&text, '\n');
        }
        mmgr_line_put_n(&text, line, line_length);
        mmgr_line_ch(&text, '\n');
        if (!mmgr_line_ok(&text) || !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                                .path = path, .bytes = s_file, .size = text.at))
        {
            mmgr_line_put(err_msg, path);
            mmgr_line_put(err_msg, ": it could not be written");
            return 0;
        }
    }
    return 1;
#endif
}

int cli_toolchains_add_to_path(const CliToolchainsErrMsgFolderCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->what, .other = "orior", .cap = CLI_POOL_BYTES}))
    {
        int on_path = 0;
        if (!EMBED_CALL(cli_toolchains_err_msg_folder_on_path.own, CliToolchainsErrMsgFolderOnPathCfg,
                        .folder = args->folder, .on_path = &on_path, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    else
    {
        CliJson json;
        CliFound found;
        const unsigned int tool =
            EMBED_CALL(cli_toolchains_json_manifest.tool, CliToolchainsJsonManifestCfg, .json_out = &json,
                       .manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                              .json_out = &json, .nodes_more = 0u, .text_more = 0ULL),
                       .id = args->what, .err_msg = args->err_msg);
        if (tool == CLI_JSON_NONE)
        {
            EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = &json);
            return 0;
        }
        const unsigned int count = cli_toolchains_find_path();
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = tool,
                   .path_callback = (const char (*)[CLI_PATH_MAX_LEN])g_cli_toolchains_path, .count = count,
                   .found = &found);
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = tool, .name = "name"),
                       .length_pointer = NULL);
        if (!found.has_program)
        {
            mmgr_line_put(args->err_msg, (name != NULL) ? name : args->what);
            mmgr_line_put(args->err_msg, " is not found: install it or give orior_cli its folder first");
            EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = &json);
            return 0;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = args->folder,
                          .src = found.folder,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = found.folder, .cap = CLI_POOL_BYTES}) + 1u});
        EMBED_CALL(cli_toolchains_json_internal.find_release, CliToolchainsJsonCtx, .json = &json);
    }
    return EMBED_CALL(cli_add_folder, ToolchainsOwnCtx, .folder = args->folder, .err_msg = args->err_msg);
}
