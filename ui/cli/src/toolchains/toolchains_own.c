// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_own.c: orior's own folder and whether it is on the PATH; a folder added to the reader's own PATH and
// taken off it again; and a line of the reader's shell profiles put in and taken out
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
    const char *line;
    unsigned long long length;
    int remove;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *start;
    const char *end;
} ToolchainsOwnCtx;

// the reader's shell profiles: ~/.profile and, on macOS, ~/.zprofile, each made where it is not there; and
// ~/.bash_profile and ~/.bash_login where they are there
#define TOOLCHAINS_PROFILES 4u
static const char *const s_profile_names[TOOLCHAINS_PROFILES] = {"/.profile", "/.zprofile", "/.bash_profile",
                                                                 "/.bash_login"};

// whether the trimmed line from `start` to `end` is the line of `length` bytes at `line`
EMBED_INLINE int profile_line_same(const ToolchainsOwnCtx *args)
{
    const char *start = args->start;
    const char *end = args->end;
    start += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = start, .at = 0ULL,
                        .size = (unsigned long long)(end - start), .set = CLI_TEXT_WHITE);
    end = start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = start,
                             .at = (unsigned long long)(end - start), .set = CLI_TEXT_WHITE);
    return ((unsigned long long)(end - start) == args->length) &&
           (memor.cmp(&(MemoriaCfg){.src = start, .other = args->line, .bytes = args->length}) == 0);
}

// The line `line`, `length` bytes, put at the end of each of the reader's shell profiles, or taken out of each where
// `remove`. A line already there is left as it is; a profile without the line is left as it is on removal
EMBED_INLINE int profile_line_each(const ToolchainsOwnCtx *args)
{
    static char s_file[1u << 20u];
    static char s_kept[1u << 20u];
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = home,
                    .capacity = sizeof(home)))
    {
        mmgr_line_put(args->err_msg, "HOME is not set");
        return 0;
    }
    for (unsigned int at = 0u; at < TOOLCHAINS_PROFILES; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        VerbaLinea named = {path, sizeof(path), 0ULL};
        mmgr_line_put(&named, home);
        mmgr_line_put(&named, s_profile_names[at]);
        mmgr_line_finish(&named);
#if !defined(__APPLE__)
        if (at == 1u)
        {
            continue;
        }
#endif
        const int there_file = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
        if ((at >= 2u) && !there_file)
        {
            continue;
        }
        if (args->remove && !there_file)
        {
            continue;
        }
        unsigned long long size =
            qry_member_file_read(path, (unsigned char *)s_file, sizeof(s_file) - args->length - 4u);
        size = (size == QRY_MEMBER_UNREAD) ? 0ULL : size;
        // every line of the file read, the one wanted found, and on removal every other line kept
        VerbaLinea kept = {s_kept, sizeof(s_kept), 0ULL};
        int there = 0;
        const char *from = s_file;
        const char *place = s_file;
        while (place <= (s_file + size))
        {
            if ((place == (s_file + size)) || (place[0] == '\n'))
            {
                const int same = EMBED_CALL(profile_line_same, ToolchainsOwnCtx, .line = args->line,
                                            .length = args->length, .start = from, .end = place);
                there = there || same;
                if (!same && (place > from))
                {
                    mmgr_line_put_n(&kept, from, (unsigned long long)(place - from));
                    mmgr_line_ch(&kept, '\n');
                }
                from = place + 1;
            }
            place += 1;
        }
        if (args->remove ? !there : there)
        {
            continue;
        }
        const char *text = s_kept;
        unsigned long long written = mmgr_line_finish(&kept);
        if (!args->remove)
        {
            VerbaLinea whole = {s_file, sizeof(s_file), size};
            if ((size != 0ULL) && (s_file[size - 1u] != '\n'))
            {
                mmgr_line_ch(&whole, '\n');
            }
            mmgr_line_put_n(&whole, args->line, args->length);
            mmgr_line_ch(&whole, '\n');
            if (!mmgr_line_ok(&whole))
            {
                mmgr_line_put(args->err_msg, path);
                mmgr_line_put(args->err_msg, ": it is too large to add a line to");
                return 0;
            }
            text = s_file;
            written = whole.at;
        }
        if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                        .size = written))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

// the profile line that puts `folder` on the PATH, into `out`: its length
EMBED_INLINE unsigned long long path_line_made(const ToolchainsOwnCtx *args)
{
    mmgr_line_put(args->out, "export PATH=\"$PATH:");
    EMBED_CALL(cli_text_size_set.append_mark, CliTextSizeSetCfg, .out = args->out, .text = args->folder, .set = "\"",
               .mark = '\\');
    mmgr_line_ch(args->out, '"');
    return mmgr_line_finish(args->out);
}

#if defined(_WIN32)
// `folder` taken out of the reader's Path in the registry, each entry compared by its key, and every open program
// notified it changed; a Path without the folder is left as it is
EMBED_INLINE int registry_path_folder_cut(const ToolchainsOwnCtx *args)
{
    static char s_now[CLI_PATH_TEXT];
    static char s_next[CLI_PATH_TEXT];
    if (!EMBED_CALL(cli_out_capacity_internal.registry_user_value, CliOutCapacityCtx, .name = "Path", .out = s_now,
                    .capacity = sizeof(s_now)))
    {
        return 1;
    }
    char wanted[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = args->folder, .key = wanted);
    VerbaLinea next = {s_next, sizeof(s_next), 0ULL};
    int cut = 0;
    const char *from = s_now;
    for (const char *place = s_now;; place += 1)
    {
        if ((place[0] != ';') && (place[0] != '\0'))
        {
            continue;
        }
        char entry[CLI_PATH_MAX_LEN];
        const unsigned long long length = (unsigned long long)(place - from);
        if ((length > 0ULL) && (length < sizeof(entry)))
        {
            memor.cpy(&(MemoriaCfg){.dst = entry, .src = from, .bytes = length});
            entry[length] = '\0';
            char key[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_toolchains_dir_key.key_get, CliToolchainsDirKeyCfg, .dir = entry, .key = key);
            if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = wanted, .cap = CLI_POOL_BYTES}))
            {
                cut = 1;
            }
            else
            {
                if (next.at != 0ULL)
                {
                    mmgr_line_ch(&next, ';');
                }
                mmgr_line_put_n(&next, from, length);
            }
        }
        from = place + 1;
        if (place[0] == '\0')
        {
            break;
        }
    }
    (void)mmgr_line_finish(&next);
    if (cut && !EMBED_CALL(cli_err_msg_name_internal.registry_set_user_value, CliErrMsgNameCtx, .name = "Path",
                           .value = s_next, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (cut)
    {
        cli_registry_announce();
    }
    return 1;
}
#endif

// A folder added to the reader's own PATH: on Windows the reader's Path in the registry, stored as text whose variables
// expand, and every open program notified it changed; elsewhere a line at the end of the reader's shell profiles
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
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea made = {line, sizeof(line), 0ULL};
    const unsigned long long length = EMBED_CALL(path_line_made, ToolchainsOwnCtx, .folder = folder, .out = &made);
    return EMBED_CALL(profile_line_each, ToolchainsOwnCtx, .line = line, .length = length, .remove = 0,
                      .err_msg = err_msg);
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
            mmgr_line_put(args->err_msg, " is not found: install it or give impero its folder first");
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

int cli_toolchains_path_folder_add(const CliToolchainsErrMsgFolderCfg *args)
{
    return EMBED_CALL(cli_add_folder, ToolchainsOwnCtx, .folder = args->folder, .err_msg = args->err_msg);
}

int cli_toolchains_path_folder_remove(const CliToolchainsErrMsgFolderCfg *args)
{
#if defined(_WIN32)
    return EMBED_CALL(registry_path_folder_cut, ToolchainsOwnCtx, .folder = args->folder, .err_msg = args->err_msg);
#else
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea made = {line, sizeof(line), 0ULL};
    const unsigned long long length =
        EMBED_CALL(path_line_made, ToolchainsOwnCtx, .folder = args->folder, .out = &made);
    return EMBED_CALL(profile_line_each, ToolchainsOwnCtx, .line = line, .length = length, .remove = 1,
                      .err_msg = args->err_msg);
#endif
}

int cli_toolchains_profile_line_add(const CliToolchainsErrMsgFolderCfg *args)
{
    return EMBED_CALL(profile_line_each, ToolchainsOwnCtx, .line = args->what,
                      .length = cellul.len(&(CatenaFinitaCfg){.src = args->what, .cap = CLI_POOL_BYTES}), .remove = 0,
                      .err_msg = args->err_msg);
}

int cli_toolchains_profile_line_remove(const CliToolchainsErrMsgFolderCfg *args)
{
    return EMBED_CALL(profile_line_each, ToolchainsOwnCtx, .line = args->what,
                      .length = cellul.len(&(CatenaFinitaCfg){.src = args->what, .cap = CLI_POOL_BYTES}), .remove = 1,
                      .err_msg = args->err_msg);
}
