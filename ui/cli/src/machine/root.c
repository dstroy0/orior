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

// the folders and files mounted beside the tree, each under its name, in the order they were mounted: records in a
// movable cell the process holds, grown as mounts fill it, its token handed to readers
#define CLI_MOUNTS_FIRST (4ULL * sizeof(CliMount))

static const CarcerMovableEntry *s_mounts_cell;
static unsigned long long s_mounts_bytes;
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
    unsigned int at;
    CliMount *mount;
    unsigned long long needed;
} RootCtx;

// the mounts' cell grown to `needed` bytes where it is less, doubled from its size, the mounts copied over, the cell
// before given back: 1, or 0 where the cells cannot meet it
EMBED_INLINE int cli_mounts_grown(const RootCtx *args)
{
    if (args->needed <= s_mounts_bytes)
    {
        return 1;
    }
    const unsigned long long doubled = (s_mounts_bytes != 0ULL) ? (s_mounts_bytes * 2ULL) : CLI_MOUNTS_FIRST;
    const unsigned long long bytes = (doubled < args->needed) ? args->needed : doubled;
    const CarcerMovableEntry *const cell = g_cli_guard->movable_buf_alloc(bytes, g_cli_machine_owner);
    if (cell == NULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < s_mounts_bytes; at += sizeof(CliMount))
    {
        CliMount mount;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = s_mounts_cell, .offset = (embed_word)at, .dst = &mount, .bytes = sizeof(mount)});
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = (embed_word)at, .src = &mount, .bytes = sizeof(mount)});
    }
    if (s_mounts_cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(s_mounts_cell, g_cli_machine_owner);
    }
    s_mounts_cell = cell;
    s_mounts_bytes = bytes;
    return 1;
}
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
        // a root, `C:\` or `/`, is where no separator is left past its first
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
    CliMount mount;
    for (unsigned int at = 0u; at < s_mounts; at += 1u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_mounts_cell,
                                                                .offset = (embed_word)(at) * sizeof(CliMount),
                                                                .dst = &mount,
                                                                .bytes = sizeof(CliMount)});
        if (cellul.eq(&(CatenaFinitaCfg){.src = mount.path, .other = full, .cap = CLI_PATH_MAX_LEN}))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->name_out,
                .src = mount.name,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = mount.name, .cap = CLI_NAME_MAX_LEN}) + 1u});
            return args->name_out[0] != '\0';
        }
    }
    if (!EMBED_CALL(cli_mounts_grown, RootCtx, .needed = ((unsigned long long)s_mounts + 1ULL) * sizeof(CliMount)))
    {
        mmgr_line_put(args->err_msg, "the window has no room for another mount");
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
        for (unsigned int at = 0u; !reserved && (at < s_mounts); at += 1u)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_mounts_cell,
                                                                    .offset = (embed_word)(at) * sizeof(CliMount),
                                                                    .dst = &mount,
                                                                    .bytes = sizeof(CliMount)});
            reserved =
                cellul.eq(&(CatenaFinitaCfg){.src = mount.name, .other = args->name_out, .cap = CLI_NAME_MAX_LEN});
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
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = mount.name, .src = args->name_out, .cap = CLI_NAME_MAX_LEN});
    memor.cpy(&(MemoriaCfg){.dst = mount.path, .src = full, .bytes = CLI_PATH_MAX_LEN});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = s_mounts_cell,
                                                             .offset = (embed_word)(s_mounts) * sizeof(CliMount),
                                                             .src = &mount,
                                                             .bytes = sizeof(CliMount)});
    s_mounts += 1u;
    return 1;
}

void cli_unmount(const CliErrMsgNameCtx *args)
{
    // the mounts kept close up over the one taken out
    unsigned int remaining = 0u;
    for (unsigned int at = 0u; at < s_mounts; at += 1u)
    {
        CliMount mount;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_mounts_cell,
                                                                .offset = (embed_word)(at) * sizeof(CliMount),
                                                                .dst = &mount,
                                                                .bytes = sizeof(CliMount)});
        if (!cellul.eq(&(CatenaFinitaCfg){.src = mount.name, .other = args->name, .cap = CLI_NAME_MAX_LEN}))
        {
            if (remaining != at)
            {
                (void)g_cli_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = s_mounts_cell,
                                        .offset = (embed_word)(remaining) * sizeof(CliMount),
                                        .src = &mount,
                                        .bytes = sizeof(CliMount)});
            }
            remaining += 1u;
        }
    }
    s_mounts = remaining;
}

void cli_unmounted_all(void)
{
    s_mounts = 0u;
}

CliMounts cli_mounts(void)
{
    const CliMounts mounts = {s_mounts_cell, s_mounts};
    return mounts;
}

int cli_inside(const CliRootCtx *args)
{
    VerbaLinea line = {args->joined, CLI_PATH_MAX_LEN, 0ULL};
    const char *rest = args->relative;
    if (args->relative[0] == '@')
    {
        const char *const slash =
            cellul.chr(&(CatenaFinitaCfg){.src = args->relative + 1, .cap = CLI_POOL_BYTES, .byte = '/'});
        const embed_word length =
            (slash != NULL) ? (embed_word)(slash - args->relative - 1)
                            : cellul.len(&(CatenaFinitaCfg){.src = args->relative + 1, .cap = CLI_POOL_BYTES});
        int found = 0;
        for (unsigned int at = 0u; !found && (at < s_mounts); at += 1u)
        {
            CliMount mount;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_mounts_cell,
                                                                    .offset = (embed_word)(at) * sizeof(CliMount),
                                                                    .dst = &mount,
                                                                    .bytes = sizeof(CliMount)});
            if ((cellul.len(&(CatenaFinitaCfg){.src = mount.name, .cap = CLI_NAME_MAX_LEN}) == length) &&
                EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = mount.name,
                           .other_pointer = args->relative + 1, .size = length))
            {
                mmgr_line_put(&line, mount.path);
                found = 1;
            }
        }
        if (!found)
        {
            mmgr_line_put(args->err_msg, args->relative);
            mmgr_line_put(args->err_msg, " is in no folder of this window");
            return 0;
        }
        rest = (slash != NULL) ? (slash + 1) : "";
    }
    else if (args->root != NULL)
    {
        mmgr_line_put(&line, args->root);
    }
    else
    {
        // no folder is open, so a path in the tree has no place
        mmgr_line_put(args->err_msg, "no folder is open in this window");
        return 0;
    }
    // each part a name or `.`: one that climbs out, or a root of its own, is refused before anything touches the disk
    const int rooted = (rest[0] == '/') || (rest[0] == '\\') || ((rest[0] != '\0') && (rest[1] == ':'));
    const char *part = rest;
    while (!rooted && (*part != '\0'))
    {
        const embed_word length = EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = part, .set = "/\\");
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

// 1 where `path` is `base` or is under it
EMBED_INLINE int cli_starts_with(const RootCtx *args)
{
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_POOL_BYTES});
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
        embed_word longest = 0u;
        CliMount mount;
        for (unsigned int at = 0u; at < s_mounts; at += 1u)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_mounts_cell,
                                                                    .offset = (embed_word)(at) * sizeof(CliMount),
                                                                    .dst = &mount,
                                                                    .bytes = sizeof(CliMount)});
            const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = mount.path, .cap = CLI_PATH_MAX_LEN});
            if (EMBED_CALL(cli_starts_with, RootCtx, .path = args->path, .base = mount.path) && (length >= longest))
            {
                found = at;
                longest = length;
            }
        }
        if (found < s_mounts)
        {
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = s_mounts_cell,
                                                                    .offset = (embed_word)(found) * sizeof(CliMount),
                                                                    .dst = &mount,
                                                                    .bytes = sizeof(CliMount)});
            mmgr_line_ch(&line, '@');
            mmgr_line_put(&line, mount.name);
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
