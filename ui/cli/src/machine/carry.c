// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry.c: the files of orior's own folder that travel, packed and sent to a file or a folder, and read back from one
#include "../code/git.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "carry_internal.h"

// the most a file of the reader's that travels carries
#define CLI_CARRY_LARGEST (16ull << 20u)

// the files that travel, each a file or a folder of orior's own folder, by its place there; and the settings that
// name this machine's own and stay on it, each a key or the start of keys that ends in `.`: the part of settings.json
// for this machine alone, how programs are signed here, the environments of the trees here, the memory the window may
// take here, and the file or folder the settings are carried through
static const char *const s_carried[7] = {"settings.json", "menus.json",       "user.css",  "themes/",
                                         "plugins/",      "plugin_settings/", "languages/"};
static const char *const s_machine[6] = {"machine",       "signing.",       "environment.",
                                         "memory-budget", "cores-reserved", "carry.place"};

void cli_carried_clear(const CliCarryCarriedPlaceCtx *args)
{
    for (unsigned int at = 0u; at < args->carried_out->count; at += 1u)
    {
        g_cli_cells->persistent_buf_release(args->carried_out->value[at], &g_cli_machine_cells);
        g_cli_cells->persistent_buf_release(args->carried_out->place[at], &g_cli_machine_cells);
        args->carried_out->value[at] = NULL;
        args->carried_out->place[at] = NULL;
    }
    args->carried_out->count = 0u;
}

unsigned int cli_carried_find(const CliCarryCarriedCtx *args)
{
    for (unsigned int at = 0u; at < args->carried->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->carried->place[at], .other = args->place, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    return args->carried->count;
}

const char *cli_carried_place(const CliCarryCarriedAtCtx *args)
{
    return args->carried->place[args->at];
}

const char *cli_carried_value(const CliCarryCarriedAtCtx *args)
{
    return args->carried->value[args->at];
}

void cli_carry_room_free(const CliCarryRoomObjectCtx *args)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->room->json);
}

void cli_carry_room_begin(const CliCarryRoomObjectCtx *args)
{
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->room->json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
}

typedef struct
{
    CliCarried *paths;
    CliCarryRoom *room;
    const char *text;
    unsigned long long size;
    const char *what;
    int fresh;
    unsigned int nodes_more;
    unsigned long long text_more;
    const char *folder;
} CarryCtx;
// the cells a carry's work took given back, its paths and its room, and its lock
EMBED_INLINE void carry_release(const CarryCtx *args)
{
    CliCarryRoom *room = args->room;
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = args->paths);
    EMBED_CALL(cli_carry_room_object_internal.room_free, CliCarryRoomObjectCtx, .room = room);
}

// `size` bytes of `text` in a cell of their own, a 0 after them: the cell, or NULL where none could be taken
EMBED_INLINE char *cli_carried_cell(const CarryCtx *args)
{
    char *const cell = (args->size < CLI_CARRIED_TEXT)
                           ? g_cli_cells->persistent_buf_alloc(args->size + 1ull, &g_cli_machine_cells)
                           : NULL;
    if (cell != NULL)
    {
        // a size below CLI_CARRIED_TEXT fits a size_t
        memor.cpy(&(MemoriaCfg){.dst = cell, .src = args->text, .bytes = (size_t)args->size});
        cell[args->size] = '\0';
    }
    return cell;
}

unsigned long long cli_carried_sent_room(const CliCarryCarriedCtx *args)
{
    unsigned long long room = 128ull;
    for (unsigned int at = 0u; at < args->carried->count; at += 1u)
    {
        room += (args->carried->size[at] * 6ull) + (CLI_NAME_MAX_LEN * 6ull) + 16ull;
    }
    return room;
}

int cli_carried_put(const CliCarryCarriedPlaceCtx *args)
{
    unsigned int at = 0u;
    while ((at < args->carried_out->count) &&
           (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->carried_out->place[at],
                       .other_pointer = args->place) < 0))
    {
        at += 1u;
    }
    const int there = (at < args->carried_out->count) &&
                      (cellul.eq(&(CatenaFinitaCfg){
                          .src = args->carried_out->place[at], .other = args->place, .cap = CLI_CELLS_POOL}));
    if (!there && (args->carried_out->count >= CLI_CARRIED_MOST))
    {
        return 0;
    }
    char *const pooled_value = EMBED_CALL(cli_carried_cell, CarryCtx, .text = args->value, .size = args->size,
                                          .what = "a carried file's text");
    char *const pooled_place =
        there ? args->carried_out->place[at]
              : EMBED_CALL(cli_carried_cell, CarryCtx, .text = args->place,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = args->place, .cap = CLI_CELLS_POOL}),
                           .what = "a carried place");
    if ((pooled_place == NULL) || (pooled_value == NULL))
    {
        g_cli_cells->persistent_buf_release(pooled_value, &g_cli_machine_cells);
        if (!there)
        {
            g_cli_cells->persistent_buf_release(pooled_place, &g_cli_machine_cells);
        }
        return 0;
    }
    if (there)
    {
        g_cli_cells->persistent_buf_release(args->carried_out->value[at], &g_cli_machine_cells);
    }
    else
    {
        for (unsigned int move = args->carried_out->count; move > at; move -= 1u)
        {
            args->carried_out->place[move] = args->carried_out->place[move - 1u];
            args->carried_out->value[move] = args->carried_out->value[move - 1u];
            args->carried_out->size[move] = args->carried_out->size[move - 1u];
        }
        args->carried_out->count += 1u;
    }
    args->carried_out->place[at] = pooled_place;
    args->carried_out->value[at] = pooled_value;
    args->carried_out->size[at] = args->size;
    return 1;
}

int cli_carry_machine(const CliCarryCtx *args)
{
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_machine[at], .cap = CLI_CELLS_POOL});
        const int start = (s_machine[at][length - 1ull] == '.') &&
                          EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->key,
                                     .other_pointer = s_machine[at], .size = length);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = s_machine[at], .cap = CLI_CELLS_POOL})) || start)
        {
            return 1;
        }
    }
    return 0;
}

// `size` bytes of settings text made plain, which is never longer than the text, and read into the room's tree, a
// fresh one in a cell sized to it with `nodes_more` nodes and `text_more` bytes over where `fresh` is 1, else beside
// what it contains: the object read, an empty one where it is no object, CLI_JSON_NONE where the tree has no cell
static unsigned int carry_plain_read(const CarryCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int nodes_more = args->nodes_more;
    unsigned long long text_more = args->text_more;
    char *const plain_text = g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_machine_cells);
    VerbaLinea plain = mmgr_line_over(plain_text, size + 1ull);
    EMBED_CALL(cli_devcontainer_text.plain_json, CliDevcontainerTextCfg, .text = text, .size = size, .plain = &plain);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    unsigned int root = CLI_JSON_NONE;
    if (args->fresh)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->room->json);
        root =
            (plain_text != NULL)
                ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                             .json = &args->room->json, .text = plain_text, .size = plain.at, .nodes_more = nodes_more,
                             .text_more = text_more, .err_msg = &unread, .owner = &g_cli_machine_cells)
                : CLI_JSON_NONE;
        if (args->room->json.node == NULL)
        {
            (void)EMBED_CALL(cli_json_tree_nodes_most.json_reserve, CliJsonTreeNodesMostCfg, .json = &args->room->json,
                             .nodes_most = nodes_more, .text_most = text_more, .what = "a settings tree",
                             .owner = &g_cli_machine_cells);
        }
    }
    else if (plain_text != NULL)
    {
        const unsigned int tokens_most =
            EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = plain_text, .size = plain.at) +
            1u;
        CfgJsonToken *const tokens =
            g_cli_cells->temporary_buf_alloc((unsigned long long)tokens_most * sizeof(*tokens), &g_cli_machine_cells);
        root = (tokens != NULL) ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                                             .json = &args->room->json, .text = plain_text, .size = plain.at,
                                             .tokens = tokens, .tokens_most = tokens_most, .err_msg = &unread)
                                : CLI_JSON_NONE;
        g_cli_cells->persistent_buf_release(tokens, &g_cli_machine_cells);
    }
    g_cli_cells->persistent_buf_release(plain_text, &g_cli_machine_cells);
    if (args->room->json.node == NULL)
    {
        return CLI_JSON_NONE;
    }
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &args->room->json,
                      .node = root, .kind = CFG_JSON_OBJECT)
               ? root
               : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &args->room->json,
                            .kind = CFG_JSON_OBJECT);
}

unsigned int cli_carry_settings_get(const CliCarryRoomCtx *args)
{
    // a text read beside makes at most as many nodes as its bytes, and at most twice them in text with a 0 after each,
    // and a copy of what it read takes as much again; the four over are the empty objects either side may be made
    return EMBED_CALL(carry_plain_read, CarryCtx, .room = args->room, .text = args->text, .size = args->size,
                      .fresh = 1, .nodes_more = (unsigned int)((2ull * args->beside) + 4ull),
                      .text_more = (4ull * args->beside) + 64ull);
}

unsigned int cli_carry_settings_beside(const CliCarryRoomCtx *args)
{
    return EMBED_CALL(carry_plain_read, CarryCtx, .room = args->room, .text = args->text, .size = args->size,
                      .fresh = 0, .nodes_more = 0u, .text_more = 0ull);
}

void cli_carry_settings_write(const CliCarryRoomObjectCtx *args)
{
    EMBED_CALL(cli_json_tree_object_name.json_sort, CliJsonTreeObjectNameCfg, .json = &args->room->json,
               .object = args->object);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &args->room->json,
               .node = args->object, .out = args->out, .indent = 2u, .sorted = 0);
    mmgr_line_ch(args->out, '\n');
}

int cli_carry_allow(const CliCarryCarriedCtx *args)
{
    const char *part = args->place;
    for (const char *at = args->place;; at += 1)
    {
        if ((at[0] == '/') || (at[0] == '\0'))
        {
            // the part lies before `at`, its end at or past its start
            const unsigned long long size = (unsigned long long)(at - part);
            if ((size == 0ull) || ((size == 1ull) && (part[0] == '.')) ||
                ((size == 2ull) && (part[0] == '.') && (part[1] == '.')))
            {
                return 0;
            }
            if (at[0] == '\0')
            {
                break;
            }
            part = at + 1;
        }
    }
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_carried[at], .cap = CLI_CELLS_POOL});
        const int under = (s_carried[at][length - 1ull] == '/') &&
                          EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->place,
                                     .other_pointer = s_carried[at], .size = length);
        if ((cellul.eq(&(CatenaFinitaCfg){.src = args->place, .other = s_carried[at], .cap = CLI_CELLS_POOL})) || under)
        {
            return 1;
        }
    }
    return 0;
}

// A walk of a folder whose files travel: the folder, the place its files start with, and the files found
typedef struct
{
    const char *folder;
    const char *under;
    CliCarried *found;
} CliUnder;

static void cli_under_each(const CliEachNameIsFolderCfg *args)
{
    void *context = args->context;
    const CliUnder *const walk = (const CliUnder *)context;
    if (args->name[0] == '.')
    {
        return;
    }
    char path[CLI_PATH_MAX_LEN];
    char place[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = walk->folder, .relative = args->name, .joined = path);
    VerbaLinea line = {place, sizeof(place), 0ull};
    mmgr_line_put(&line, walk->under);
    mmgr_line_put(&line, args->name);
    if (args->is_folder)
    {
        mmgr_line_ch(&line, '/');
    }
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    if (args->is_folder)
    {
        const CliUnder below = {path, place, walk->found};
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path, .each = cli_under_each,
                   .context_bytess = (void *)&below);
        return;
    }
    EMBED_CALL(cli_carry_carried_place_internal.carried_put, CliCarryCarriedPlaceCtx, .carried_out = walk->found,
               .place = place, .value = path,
               .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
}

void cli_carry_files(const CliCarryFolderCtx *args)
{
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = args->found);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_carried[at], .cap = CLI_CELLS_POOL});
        if (s_carried[at][length - 1ull] == '/')
        {
            char dir[CLI_NAME_MAX_LEN];
            memor.cpy(&(MemoriaCfg){
                .dst = dir,
                .src = s_carried[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_carried[at], .cap = CLI_CELLS_POOL}) + 1u});
            dir[length - 1ull] = '\0';
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = dir, .joined = path);
            const CliUnder walk = {path, s_carried[at], args->found};
            EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path,
                       .each = cli_under_each, .context_bytess = (void *)&walk);
            continue;
        }
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->folder, .relative = s_carried[at], .joined = path);
        if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path))
        {
            EMBED_CALL(cli_carry_carried_place_internal.carried_put, CliCarryCarriedPlaceCtx,
                       .carried_out = args->found, .place = s_carried[at], .value = path,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        }
    }
}

int cli_carry_pack(const CliCarryCfg *args)
{
    static CliCarried s_paths;
    static CliCarryRoom s_room;
    EMBED_CALL(cli_carry_room_object_internal.room_begin, CliCarryRoomObjectCtx, .room = &s_room);
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = args->packed);
    EMBED_CALL(cli_carry_folder_internal.files, CliCarryFolderCtx, .folder = args->home, .found = &s_paths);
    for (unsigned int at = 0u; at < s_paths.count; at += 1u)
    {
        const char *const place = EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                             .carried = &s_paths, .at = at);
        const char *const path = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                            .carried = &s_paths, .at = at);
        unsigned long long size = 0ull;
        if (EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size) &&
            (size > CLI_CARRY_LARGEST))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, " is over 16 MB, the most a file that travels carries");
            EMBED_CALL(carry_release, CarryCtx, .paths = &s_paths, .room = &s_room);
            return 0;
        }
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take_utf8, CliPathCtx, .path = path,
                                                      .size_pointer = &size, .owner = &g_cli_machine_cells);
        if (bytes == NULL)
        {
            continue;
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL}))
        {
            const unsigned int object =
                EMBED_CALL(cli_carry_room_internal.settings_get, CliCarryRoomCtx, .room = &s_room,
                           .text = (const char *)bytes, .size = size, .beside = 0ull);
            for (unsigned int member = (object != CLI_JSON_NONE) ? s_room.json.node[object].first : CLI_JSON_NONE;
                 member != CLI_JSON_NONE;)
            {
                const unsigned int next = s_room.json.node[member].next;
                const char *const key = s_room.json.text + s_room.json.node[member].key;
                if (EMBED_CALL(cli_carry_internal.machine, CliCarryCtx, .key = key))
                {
                    EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = &s_room.json,
                               .object = object, .name = key);
                }
                member = next;
            }
            const unsigned long long written_room =
                EMBED_CALL(cli_json_tree_node_out.json_pretty_room, CliJsonTreeNodeOutCfg, .json_read = &s_room.json,
                           .indent = 2u);
            char *const written_cell =
                (object != CLI_JSON_NONE) ? g_cli_cells->temporary_buf_alloc(written_room, &g_cli_machine_cells) : NULL;
            VerbaLinea written = mmgr_line_over(written_cell, written_room);
            EMBED_CALL(cli_carry_room_object_internal.settings_write, CliCarryRoomObjectCtx, .room = &s_room,
                       .object = object, .out = &written);
            if (mmgr_line_ok(&written))
            {
                EMBED_CALL(cli_carry_carried_place_internal.carried_put, CliCarryCarriedPlaceCtx,
                           .carried_out = args->packed, .place = place, .value = written.out, .size = written.at);
            }
            g_cli_cells->persistent_buf_release(written.out, &g_cli_machine_cells);
        }
        else
        {
            EMBED_CALL(cli_carry_carried_place_internal.carried_put, CliCarryCarriedPlaceCtx,
                       .carried_out = args->packed, .place = place, .value = (const char *)bytes, .size = size);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                   .owner = &g_cli_machine_cells);
    }
    EMBED_CALL(carry_release, CarryCtx, .paths = &s_paths, .room = &s_room);
    return 1;
}

// whether the repository at `folder` has a remote
EMBED_INLINE int cli_has_remote(const CarryCtx *args)
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

int cli_carry_read(const CliCarryFromCfg *args)
{
    static CliCarried s_paths;
    static CliCarryRoom s_room;
    EMBED_CALL(cli_carry_room_object_internal.room_begin, CliCarryRoomObjectCtx, .room = &s_room);
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = args->there_out);
    if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->from))
    {
        char git[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->from, .relative = ".git", .joined = git);
        char words[CLI_NAME_MAX_LEN * 4u];
        VerbaLinea pulled = {words, sizeof(words), 0ull};
        if ((EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = git) ||
             EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = git)) &&
            EMBED_CALL(cli_has_remote, CarryCtx, .folder = args->from) &&
            !EMBED_CALL(cli_git_root_out_count_err_msg.pull, CliGitRootOutCountErrMsgCfg, .root = args->from,
                        .out = &pulled, .err_msg = args->err_msg))
        {
            EMBED_CALL(carry_release, CarryCtx, .paths = &s_paths, .room = &s_room);
            return 0;
        }
        EMBED_CALL(cli_carry_folder_internal.files, CliCarryFolderCtx, .folder = args->from, .found = &s_paths);
        for (unsigned int at = 0u; at < s_paths.count; at += 1u)
        {
            unsigned long long size = 0ull;
            const unsigned char *const bytes =
                EMBED_CALL(cli_path_internal.file_take_utf8, CliPathCtx,
                           .path = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                              .carried = &s_paths, .at = at),
                           .size_pointer = &size, .owner = &g_cli_machine_cells);
            if (bytes != NULL)
            {
                EMBED_CALL(cli_carry_carried_place_internal.carried_put, CliCarryCarriedPlaceCtx,
                           .carried_out = args->there_out,
                           .place = EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                               .carried = &s_paths, .at = at),
                           .value = (const char *)bytes, .size = size);
                EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                           .owner = &g_cli_machine_cells);
            }
        }
        EMBED_CALL(carry_release, CarryCtx, .paths = &s_paths, .room = &s_room);
        return 1;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take_utf8, CliPathCtx, .path = args->from,
                                                  .size_pointer = &size, .owner = &g_cli_machine_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg, ": it could not be read");
        EMBED_CALL(carry_release, CarryCtx, .paths = &s_paths, .room = &s_room);
        return 0;
    }
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &s_room.json,
                   .text = (const char *)bytes, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &unread,
                   .owner = &g_cli_machine_cells);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_machine_cells);
    const char *const mark =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_room.json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_room.json,
                                      .object = root, .name = "orior"),
                   .length_pointer = NULL);
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_room.json, .object = root, .name = "files");
    int read = (root != CLI_JSON_NONE) && (mark != NULL) &&
               (cellul.eq(&(CatenaFinitaCfg){.src = mark, .other = CLI_CARRY_MARK, .cap = CLI_CELLS_POOL})) &&
               EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_room.json,
                          .node = files, .kind = CFG_JSON_OBJECT);
    for (unsigned int member = read ? s_room.json.node[files].first : CLI_JSON_NONE; member != CLI_JSON_NONE;
         member = s_room.json.node[member].next)
    {
        unsigned long long length = 0ull;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &s_room.json, .node = member, .length_pointer = &length);
        const char *const place = s_room.json.text + s_room.json.node[member].key;
        read = read && (text != NULL);
        if ((text != NULL) && EMBED_CALL(cli_carry_carried_internal.allow, CliCarryCarriedCtx, .place = place))
        {
            EMBED_CALL(cli_carry_carried_place_internal.carried_put, CliCarryCarriedPlaceCtx,
                       .carried_out = args->there_out, .place = place, .value = text, .size = length);
        }
    }
    if (!read)
    {
        mmgr_line_put(args->err_msg, args->from);
        mmgr_line_put(args->err_msg,
                      (root == CLI_JSON_NONE) ? ": it is no JSON" : " contains no settings orior_cli carried");
    }
    EMBED_CALL(carry_release, CarryCtx, .paths = &s_paths, .room = &s_room);
    return read;
}
