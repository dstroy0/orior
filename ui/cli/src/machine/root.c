// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// root.c: the orior tree the app works on, and the rule that confines every path inside it. A window can contain
// folders from other places, or single files from them, beside its tree: each is mounted under a name of its own, and a
// path that starts `@name/` is under that folder, or `@name` is that file. The rule that confines a path to the tree
// confines one under a mount inside the mount
#include "../command_line/cli_internal.h"
#include "../text/text.h"

// the file whose presence marks the top of the engine's tree, which the command line finds by walking up where no
// tree is named
#define CLI_TREE_MARKER "src/cu/engine/engine_config.h"

// the folders and files mounted beside the tree, each under its name, in the order they were mounted
static char s_mount_name[CLI_MOUNTS_MAX][CLI_NAME_MAX_LEN];
static char s_mount_path[CLI_MOUNTS_MAX][CLI_PATH_MAX_LEN];
static unsigned int s_mounts;

int cli_is_tree_root(const CliWordsErrMsgCtx *args)
{
    char marker[CLI_PATH_MAX_LEN];
    VerbaLinea line = {marker, sizeof(marker), 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, CLI_TREE_MARKER);
    return (mmgr_line_finish(&line) != 0ULL) &&
           EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = marker);
}

int cli_tree_at(const CliPathErrMsgFullCtx *args)
{
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = args->path,
                    .full = args->full, .capacity = CLI_PATH_MAX_LEN))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be made a full path");
        return 0;
    }
    if (!EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->full))
    {
        mmgr_line_put(args->err_msg, args->full);
        mmgr_line_put(args->err_msg, " is no folder");
        return 0;
    }
    return 1;
}

typedef struct
{
    const char *start;
    char *tree;
    const char *path;
    const char *base;
} RootCtx;
// the first folder from `start` up containing the tree into `tree`: 1, or 0 where none does
EMBED_INLINE int cli_tree_above(const RootCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = args->start, .full = folder,
                    .capacity = sizeof(folder)))
    {
        return 0;
    }
    for (;;)
    {
        if (EMBED_CALL(cli_words_err_msg_internal.is_tree_root, CliWordsErrMsgCtx, .folder = folder))
        {
            return EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = folder,
                              .full = args->tree, .capacity = CLI_PATH_MAX_LEN);
        }
        char *slash = NULL;
        const unsigned long long slash_end =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = folder,
                       .at = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}), .set = "/\\");
        slash = (slash_end != 0ULL) ? (folder + (slash_end - 1ULL)) : slash;
        // a root, `C:\` or `/`, stands where no separator is left past its first
        if ((slash == NULL) || (slash == folder) || ((slash == (folder + 2)) && (folder[1] == ':')))
        {
            if ((slash != NULL) && (slash[1] != '\0'))
            {
                slash[1] = '\0';
                continue;
            }
            return 0;
        }
        *slash = '\0';
    }
}

int cli_tree_on_find(const CliTreeCtx *args)
{
    char named[CLI_PATH_MAX_LEN];
    char err_buf[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_ROOT",
                   .value_out = named, .capacity = sizeof(named)) &&
        EMBED_CALL(cli_path_err_msg_full_internal.tree_at, CliPathErrMsgFullCtx, .path = named, .full = args->tree,
                   .err_msg = &err_msg))
    {
        return 1;
    }
    char here[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_capacity_internal.folder_current, CliPathCapacityCtx, .path_out = here,
                   .capacity_bytes = sizeof(here)) &&
        EMBED_CALL(cli_tree_above, RootCtx, .start = here, .tree = args->tree))
    {
        return 1;
    }
    char program[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_capacity_internal.program_own, CliPathCapacityCtx, .path_out = program,
                   .capacity_bytes = sizeof(program)))
    {
        char *slash = NULL;
        const unsigned long long slash_end =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = program,
                       .at = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_POOL_BYTES}), .set = "/\\");
        slash = (slash_end != 0ULL) ? (program + (slash_end - 1ULL)) : slash;
        if (slash != NULL)
        {
            *slash = '\0';
            return EMBED_CALL(cli_tree_above, RootCtx, .start = program, .tree = args->tree);
        }
    }
    return 0;
}

int cli_mount(const CliPathErrMsgNameCtx *args)
{
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = args->path, .full = full,
                    .capacity = sizeof(full)) ||
        (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full) &&
         !EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = full)))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it is not there");
        return 0;
    }
    for (unsigned int at = 0u; at < s_mounts; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_mount_path[at], .other = full, .cap = CLI_POOL_BYTES}))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->name_out,
                .src = s_mount_name[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_mount_name[at], .cap = CLI_POOL_BYTES}) + 1u});
            return args->name_out[0] != '\0';
        }
    }
    if (s_mounts >= CLI_MOUNTS_MAX)
    {
        mmgr_line_put(args->err_msg, "the window has as many mounts as it can");
        return 0;
    }
    // the last part of the path, each separator in it made `_`
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_POOL_BYTES});
    const unsigned long long saved =
        ((whole != 0ULL) && ((full[whole - 1ULL] == '/') || (full[whole - 1ULL] == '\\'))) ? (whole - 1ULL) : whole;
    const char *const base = full + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = full,
                                               .at = saved, .set = "/\\");
    char stem[CLI_NAME_MAX_LEN];
    VerbaLinea stem_line = {stem, sizeof(stem), 0ULL};
    EMBED_CALL(cli_text_set_table.append_replace, CliTextSetCfg, .out = &stem_line, .text = base, .set = "/\\",
               .with = "_");
    if ((mmgr_line_finish(&stem_line) == 0ULL) || (stem[0] == '\0'))
    {
        memor.cpy(&(MemoriaCfg){.dst = stem,
                                .src = "folder",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "folder", .cap = CLI_POOL_BYTES}) + 1u});
    }
    VerbaLinea name_line = {args->name_out, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&name_line, stem);
    mmgr_line_finish(&name_line);
    for (unsigned long long count = 2ULL;; count += 1ULL)
    {
        int reserved = 0;
        {
            unsigned int at = 0u;
            while (!reserved && (at < s_mounts))
            {
                reserved = cellul.eq(
                    &(CatenaFinitaCfg){.src = s_mount_name[at], .other = args->name_out, .cap = CLI_POOL_BYTES});
                at += 1u;
            }
        }
        if (!reserved)
        {
            break;
        }
        name_line.at = 0ULL;
        mmgr_line_put(&name_line, stem);
        mmgr_line_ch(&name_line, '-');
        mmgr_line_decimal(&name_line, count, 0u);
        mmgr_line_finish(&name_line);
    }
    (void)cellul.copy(
        &(CatenaFinitaCfg){.dst = s_mount_name[s_mounts], .src = args->name_out, .cap = CLI_NAME_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = s_mount_path[s_mounts], .src = full, .bytes = CLI_PATH_MAX_LEN});
    s_mounts += 1u;
    return 1;
}

void cli_unmount(const CliErrMsgNameCtx *args)
{
    unsigned int remaining = 0u;
    for (unsigned int at = 0u; at < s_mounts; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = s_mount_name[at], .other = args->name, .cap = CLI_POOL_BYTES}))
        {
            memor.move_down(
                &(MemoriaCfg){.dst = s_mount_name[remaining], .src = s_mount_name[at], .bytes = CLI_NAME_MAX_LEN});
            memor.move_down(
                &(MemoriaCfg){.dst = s_mount_path[remaining], .src = s_mount_path[at], .bytes = CLI_PATH_MAX_LEN});
            remaining += 1u;
        }
    }
    s_mounts = remaining;
}

void cli_unmounted_all(void)
{
    s_mounts = 0u;
}

unsigned int cli_mounts(const CliCapacityNamesCtx *args)
{
    unsigned int at = 0u;
    for (; (at < s_mounts) && (at < args->capacity_u32); at += 1u)
    {
        args->names[at] = s_mount_name[at];
        args->paths_read[at] = s_mount_path[at];
    }
    return at;
}

int cli_inside(const CliRootCtx *args)
{
    VerbaLinea line = {args->joined, CLI_PATH_MAX_LEN, 0ULL};
    const char *rest = args->relative;
    if (args->relative[0] == '@')
    {
        const char *const slash =
            cellul.chr(&(CatenaFinitaCfg){.src = args->relative + 1, .cap = CLI_POOL_BYTES, .byte = '/'});
        const size_t length = (slash != NULL)
                                  ? (size_t)(slash - args->relative - 1)
                                  : cellul.len(&(CatenaFinitaCfg){.src = args->relative + 1, .cap = CLI_POOL_BYTES});
        int found = 0;
        unsigned int at = 0u;
        while (!found && (at < s_mounts))
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = s_mount_name[at], .cap = CLI_POOL_BYTES}) == length) &&
                EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = s_mount_name[at],
                           .other_pointer = args->relative + 1, .size = length))
            {
                mmgr_line_put(&line, s_mount_path[at]);
                found = 1;
            }
            at += 1u;
        }
        if (!found)
        {
            mmgr_line_put(args->err_msg, args->relative);
            mmgr_line_put(args->err_msg, " is in no folder of this window");
            return 0;
        }
        rest = (slash != NULL) ? (slash + 1) : "";
    }
    else
    {
        mmgr_line_put(&line, args->root);
    }
    // each part a name or `.`: one that climbs out, or a root of its own, is refused before anything touches the disk
    const int rooted = (rest[0] == '/') || (rest[0] == '\\') || ((rest[0] != '\0') && (rest[1] == ':'));
    const char *part = rest;
    while (!rooted && (*part != '\0'))
    {
        const size_t length = EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = part, .set = "/\\");
        if ((length == 2u) && (part[0] == '.') && (part[1] == '.'))
        {
            break;
        }
        if ((length != 0u) && !((length == 1u) && (part[0] == '.')))
        {
            mmgr_line_ch(&line, '/');
            mmgr_line_put_n(&line, part, length);
        }
        part += length;
        part += (*part != '\0') ? 1 : 0;
    }
    if (rooted || (*part != '\0'))
    {
        mmgr_line_put(args->err_msg, args->relative);
        mmgr_line_put(args->err_msg, " is outside the tree");
        return 0;
    }
    return mmgr_line_finish(&line) != 0ULL;
}

void cli_full(const CliRootCtx *args)
{
    char err_buf[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->relative,
                    .joined = args->joined, .err_msg = &err_msg))
    {
        // a whole path is the path it names, and any other is written under the tree
        const int whole = (args->relative[0] == '/') || (args->relative[0] == '\\') ||
                          ((args->relative[0] != '\0') && (args->relative[1] == ':'));
        VerbaLinea line = {args->joined, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, whole ? "" : args->root);
        mmgr_line_put(&line, whole ? "" : "/");
        mmgr_line_put(&line, args->relative);
        mmgr_line_finish(&line);
    }
}

// 1 where `path` is `base` or stands under it
EMBED_INLINE int cli_starts_with(const RootCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_POOL_BYTES});
    return EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->path, .other_pointer = args->base,
                      .size = length) &&
           ((args->path[length] == '\0') || (args->path[length] == '/') || (args->path[length] == '\\'));
}

void cli_relative(const CliPathRootCtx *args)
{
    VerbaLinea line = {args->relative_out, CLI_PATH_MAX_LEN, 0ULL};
    const char *rest = args->path;
    if (!EMBED_CALL(cli_starts_with, RootCtx, .path = args->path, .base = args->root))
    {
        // the mount whose folder is the longest that contains the path
        unsigned int found = s_mounts;
        size_t longest = 0u;
        for (unsigned int at = 0u; at < s_mounts; at += 1u)
        {
            const size_t length = cellul.len(&(CatenaFinitaCfg){.src = s_mount_path[at], .cap = CLI_POOL_BYTES});
            if (EMBED_CALL(cli_starts_with, RootCtx, .path = args->path, .base = s_mount_path[at]) &&
                (length >= longest))
            {
                found = at;
                longest = length;
            }
        }
        if (found < s_mounts)
        {
            mmgr_line_ch(&line, '@');
            mmgr_line_put(&line, s_mount_name[found]);
            rest = args->path + longest;
            if (*rest != '\0')
            {
                mmgr_line_ch(&line, '/');
                rest += 1;
            }
            (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = rest, .from = '\\',
                             .to = '/');
            mmgr_line_finish(&line);
            return;
        }
    }
    else
    {
        rest = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES});
        rest += ((*rest == '/') || (*rest == '\\')) ? 1 : 0;
    }
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = rest, .from = '\\', .to = '/');
    mmgr_line_finish(&line);
}

int cli_path_parent(const CliPathOutTypeCtx *args)
{
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    const unsigned long long root = ((length >= 2ULL) && (args->path[1] == ':')) ? 3ULL : 1ULL;
    length = (root + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->path + root,
                                .at = length - root, .set = "/\\"));
    unsigned long long slash = length;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        slash = ((args->path[at] == '/') || (args->path[at] == '\\')) ? at : slash;
    }
    if ((slash == length) || ((slash + 1ULL) == length))
    {
        return 0;
    }
    const unsigned long long cut = (slash < root) ? root : slash;
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, args->path, (cut > length) ? length : cut);
    return mmgr_line_finish(&line) != 0ULL;
}
