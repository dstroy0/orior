// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_take.c: what is carried here compared with orior's own folder, taken in as the reader chooses with each file it
// writes over saved, and a taking undone
#include "../command_line/cells.h"
#include "../text/text.h"
#include "carry_internal.h"

// the most keys a settings comparison reads
#define CLI_CARRY_KEYS 4096u

// The room taking needs: what travels here, the settings read, the keys of two settings, and a taking
typedef struct
{
    CliCarried here;
    CliCarryRoom room;
    const char *key[CLI_CARRY_KEYS];
    unsigned int keys;
    CliTaking taking;
} CliTakeRoom;

static CliTakeRoom s_take;
static CliLock s_lock = CLI_LOCK_INIT;

typedef struct
{
    unsigned int object;
    unsigned int mine;
    unsigned int theirs;
    VerbaLinea *out;
    unsigned int value;
    const CliCarried *there;
    unsigned int mine_at;
    unsigned int theirs_at;
    const char *to;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
    const char *before;
    unsigned long long before_size;
    const char *const *keys;
    unsigned int key_count;
    const char *home;
    const char *backup_dir;
    const char *place;
} CarryTakeCtx;
// the members of a settings object
EMBED_INLINE unsigned int cli_member_first(const CarryTakeCtx *args)
{
    return s_take.room.json.node[args->object].first;
}

// the keys of two settings objects, in order, each once, less the settings of this machine alone
EMBED_INLINE void cli_keys_get(const CarryTakeCtx *args)
{
    CliJson *const json = &s_take.room.json;
    s_take.keys = 0u;
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        for (unsigned int member =
                 EMBED_CALL(cli_member_first, CarryTakeCtx, .object = (side == 0u) ? args->mine : args->theirs);
             member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            unsigned int at = 0u;
            while ((at < s_take.keys) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_take.key[at],
                                                     .other_pointer = key) < 0))
            {
                at += 1u;
            }
            if (((at < s_take.keys) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_take.key[at], .other = key, .cap = CLI_CELLS_POOL}))) ||
                EMBED_CALL(cli_carry_internal.machine, CliCarryCtx, .key = key) || (s_take.keys >= CLI_CARRY_KEYS))
            {
                continue;
            }
            for (unsigned int move = s_take.keys; move > at; move -= 1u)
            {
                s_take.key[move] = s_take.key[move - 1u];
            }
            s_take.key[at] = key;
            s_take.keys += 1u;
        }
    }
}

// a setting's value written as JSON in a string, or null where it is not set
EMBED_INLINE void cli_value_write(const CarryTakeCtx *args)
{
    VerbaLinea *out = args->out;
    const unsigned long long line_room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = &s_take.room.json);
    char *const line_cell =
        (args->value != CLI_JSON_NONE) ? g_cli_cells->temporary_buf_alloc(line_room, &g_cli_machine_cells) : NULL;
    VerbaLinea line = mmgr_line_over(line_cell, line_room);
    if (line_cell == NULL)
    {
        mmgr_line_put(out, "null");
        return;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_take.room.json,
               .node = args->value, .out = &line, .indent = 0u, .sorted = 0);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = line.out,
               .length = mmgr_line_ok(&line) ? line.at : 0ull);
    g_cli_cells->persistent_buf_release(line.out, &g_cli_machine_cells);
}

// the settings that differ between two settings texts written, each with its value here and there: 1 where any do
EMBED_INLINE int cli_settings_differ(const CarryTakeCtx *args)
{
    VerbaLinea *out = args->out;
    CliJson *const json = &s_take.room.json;
    const unsigned long long theirs_size =
        (args->theirs_at < args->there->count) ? args->there->size[args->theirs_at] : 0ull;
    const unsigned int mine = EMBED_CALL(
        cli_carry_room_internal.settings_get, CliCarryRoomCtx, .room = &s_take.room,
        .text = (args->mine_at < s_take.here.count)
                    ? EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                 .carried = &s_take.here, .at = args->mine_at)
                    : "",
        .size = (args->mine_at < s_take.here.count) ? s_take.here.size[args->mine_at] : 0ull, .beside = theirs_size);
    const unsigned int theirs =
        (mine != CLI_JSON_NONE)
            ? EMBED_CALL(cli_carry_room_internal.settings_beside, CliCarryRoomCtx, .room = &s_take.room,
                         .text = (args->theirs_at < args->there->count)
                                     ? EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                                  .carried = args->there, .at = args->theirs_at)
                                     : "",
                         .size = theirs_size)
            : CLI_JSON_NONE;
    if (theirs == CLI_JSON_NONE)
    {
        return 0;
    }
    EMBED_CALL(cli_keys_get, CarryTakeCtx, .mine = mine, .theirs = theirs);
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_take.keys; at += 1u)
    {
        const unsigned int here = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = mine, .name = s_take.key[at]);
        const unsigned int away = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = theirs, .name = s_take.key[at]);
        const int same = (here != CLI_JSON_NONE) && (away != CLI_JSON_NONE) &&
                         EMBED_CALL(cli_json_tree_one.json_same, CliJsonTreeOneCfg, .json_read = json, .one = here,
                                    .other_json = json, .other = away);
        if (same || ((here == CLI_JSON_NONE) && (away == CLI_JSON_NONE)))
        {
            continue;
        }
        mmgr_line_put(out, (written != 0u) ? ",{\"key\":" : "{\"key\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = s_take.key[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = s_take.key[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"here\":");
        EMBED_CALL(cli_value_write, CarryTakeCtx, .out = out, .value = here);
        mmgr_line_put(out, ",\"there\":");
        EMBED_CALL(cli_value_write, CarryTakeCtx, .out = out, .value = away);
        mmgr_line_ch(out, '}');
        written += 1u;
    }
    return written != 0u;
}

void cli_carry_differences(const CliCarryHomeCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    char *const settings_cell = g_cli_cells->temporary_buf_alloc(CLI_CARRY_WRITTEN_ROOM, &g_cli_machine_cells);
    VerbaLinea settings = mmgr_line_over(settings_cell, CLI_CARRY_WRITTEN_ROOM);
    if (settings.out == NULL)
    {
        mmgr_line_put(args->out, "[]");
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        return;
    }
    EMBED_CALL(cli_carry_room_object_internal.room_begin, CliCarryRoomObjectCtx, .room = &s_take.room);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unpacked = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_carry.pack, CliCarryCfg, .home = args->home, .packed = &s_take.here, .err_msg = &unpacked))
    {
        EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx,
                   .carried_out = &s_take.here);
    }
    mmgr_line_ch(args->out, '[');
    unsigned int written = 0u;
    unsigned int mine_at = 0u;
    unsigned int theirs_at = 0u;
    // the places of both sides, in order
    while ((mine_at < s_take.here.count) || (theirs_at < args->there->count))
    {
        const int has_mine = mine_at < s_take.here.count;
        const int has_theirs = theirs_at < args->there->count;
        const int order =
            !has_mine     ? 1
            : !has_theirs ? -1
                          : EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                       .one = EMBED_CALL(cli_carry_carried_at_internal.carried_place,
                                                         CliCarryCarriedAtCtx, .carried = &s_take.here, .at = mine_at),
                                       .other_pointer =
                                           EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                                      .carried = args->there, .at = theirs_at));
        const unsigned int mine = (order <= 0) ? mine_at : s_take.here.count;
        const unsigned int theirs = (order >= 0) ? theirs_at : args->there->count;
        const char *const place = (order <= 0)
                                      ? EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                                   .carried = &s_take.here, .at = mine_at)
                                      : EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                                   .carried = args->there, .at = theirs_at);
        mine_at += (order <= 0) ? 1u : 0u;
        theirs_at += (order >= 0) ? 1u : 0u;
        const char *how = NULL;
        settings.at = 0ull;
        if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL}))
        {
            how = EMBED_CALL(cli_settings_differ, CarryTakeCtx, .there = args->there, .mine_at = mine,
                             .theirs_at = theirs, .out = &settings)
                      ? "changed"
                      : NULL;
        }
        else if ((mine < s_take.here.count) && (theirs < args->there->count))
        {
            const int same = (s_take.here.size[mine] == args->there->size[theirs]) &&
                             (memor.cmp(&(MemoriaCfg){
                                  .src = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                                    .carried = &s_take.here, .at = mine),
                                  .other = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                                      .carried = args->there, .at = theirs),
                                  .bytes = args->there->size[theirs]}) == 0);
            how = same ? NULL : "changed";
        }
        else
        {
            how = (mine < s_take.here.count) ? "here" : "there";
        }
        if (how == NULL)
        {
            continue;
        }
        mmgr_line_put(args->out, (written != 0u) ? ",{\"place\":" : "{\"place\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = place,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, ",\"how\":\"");
        mmgr_line_put(args->out, how);
        mmgr_line_put(args->out, "\",\"settings\":[");
        mmgr_line_put_n(args->out, settings.out, mmgr_line_ok(&settings) ? settings.at : 0ull);
        mmgr_line_put(args->out, "]}");
        written += 1u;
    }
    mmgr_line_ch(args->out, ']');
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = &s_take.here);
    EMBED_CALL(cli_carry_room_object_internal.room_free, CliCarryRoomObjectCtx, .room = &s_take.room);
    g_cli_cells->persistent_buf_release(settings.out, &g_cli_machine_cells);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// the text of the file at `path` written whole at `to`, the folder above it made: 1, or 0 with why given
EMBED_INLINE int cli_write_on_to(const CarryTakeCtx *args)
{
    unsigned long long size = args->size;
    char parent[CLI_PATH_MAX_LEN];
    if ((EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->to, .out = parent) &&
         !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent)) ||
        !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = args->to,
                    .bytes = args->text, .size = size))
    {
        mmgr_line_put(args->err_msg, args->to);
        mmgr_line_put(args->err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// the settings of this machine retained and of `keys` taken from there, written into `out`: 1, or 0 where the settings'
// tree is refused its cell or they do not fit
EMBED_INLINE int cli_settings_take(const CarryTakeCtx *args)
{
    VerbaLinea *out = args->out;
    CliJson *const json = &s_take.room.json;
    const unsigned int theirs_at = EMBED_CALL(cli_carry_carried_internal.carried_find, CliCarryCarriedCtx,
                                              .carried = args->there, .place = "settings.json");
    const unsigned long long theirs_size = (theirs_at < args->there->count) ? args->there->size[theirs_at] : 0ull;
    const unsigned int mine = EMBED_CALL(cli_carry_room_internal.settings_get, CliCarryRoomCtx, .room = &s_take.room,
                                         .text = args->before, .size = args->before_size, .beside = theirs_size);
    const unsigned int theirs =
        (mine != CLI_JSON_NONE)
            ? EMBED_CALL(cli_carry_room_internal.settings_beside, CliCarryRoomCtx, .room = &s_take.room,
                         .text = (theirs_at < args->there->count)
                                     ? EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                                  .carried = args->there, .at = theirs_at)
                                     : "",
                         .size = theirs_size)
            : CLI_JSON_NONE;
    if (theirs == CLI_JSON_NONE)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->key_count; at += 1u)
    {
        if (EMBED_CALL(cli_carry_internal.machine, CliCarryCtx, .key = args->keys[at]))
        {
            continue;
        }
        const unsigned int value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = theirs, .name = args->keys[at]);
        if (value != CLI_JSON_NONE)
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = json, .object = mine,
                       .name = args->keys[at],
                       .value = EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = json, .node = value));
        }
        else
        {
            EMBED_CALL(cli_json_tree_object_name.json_remove, CliJsonTreeObjectNameCfg, .json = json, .object = mine,
                       .name = args->keys[at]);
        }
    }
    EMBED_CALL(cli_carry_room_object_internal.settings_write, CliCarryRoomObjectCtx, .room = &s_take.room,
               .object = mine, .out = out);
    return mmgr_line_ok(out);
}

// one place taken in: its file saved in `backup_dir` where one stood, and written as there: 1, or 0 with why given
EMBED_INLINE int cli_place_take(const CarryTakeCtx *args)
{
    const char *place = args->place;
    char *const settings_cell = g_cli_cells->temporary_buf_alloc(CLI_CARRY_WRITTEN_ROOM, &g_cli_machine_cells);
    VerbaLinea settings = mmgr_line_over(settings_cell, CLI_CARRY_WRITTEN_ROOM);
    if (settings.out == NULL)
    {
        mmgr_line_put(args->err_msg, "the settings written could not be given a cell");
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = place, .joined = path);
    unsigned long long before_size = 0ull;
    const unsigned char *const before = EMBED_CALL(cli_path_internal.file_take_utf8, CliPathCtx, .path = path,
                                                   .size_pointer = &before_size, .owner = &g_cli_machine_cells);
    const char *text = NULL;
    unsigned long long size = 0ull;
    int taken = 1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL}))
    {
        taken = EMBED_CALL(cli_settings_take, CarryTakeCtx, .before = (before != NULL) ? (const char *)before : "",
                           .before_size = before_size, .there = args->there, .keys = args->keys,
                           .key_count = args->key_count, .out = &settings);
        if (!taken)
        {
            mmgr_line_put(args->err_msg, "the settings taken could not be given cells");
        }
        text = settings.out;
        size = settings.at;
    }
    else
    {
        const unsigned int at = EMBED_CALL(cli_carry_carried_internal.carried_find, CliCarryCarriedCtx,
                                           .carried = args->there, .place = place);
        text = (at < args->there->count) ? EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                                      .carried = args->there, .at = at)
                                         : NULL;
        size = (at < args->there->count) ? args->there->size[at] : 0ull;
    }
    if (taken && (text != NULL) && (before != NULL))
    {
        char saved[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->backup_dir, .relative = place, .joined = saved);
        taken = EMBED_CALL(cli_write_on_to, CarryTakeCtx, .to = saved, .text = (const char *)before,
                           .size = before_size, .err_msg = args->err_msg);
    }
    // the file is let go of before it is written over: a file still mapped is not truncated on Windows, and reading a
    // view of one that was is a fault elsewhere; what the writing needs of it is in the settings written and the saved
    // copy already
    const unsigned int stood = (before != NULL) ? 1u : 0u;
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = before, .owner = &g_cli_machine_cells);
    if (taken && (text != NULL))
    {
        taken =
            EMBED_CALL(cli_write_on_to, CarryTakeCtx, .to = path, .text = text, .size = size, .err_msg = args->err_msg);
    }
    if (taken && (text != NULL) && (s_take.taking.files < CLI_TAKING_FILES))
    {
        (void)cellul.copy(
            &(CatenaFinitaCfg){.dst = s_take.taking.place[s_take.taking.files], .src = place, .cap = CLI_NAME_MAX_LEN});
        s_take.taking.stood[s_take.taking.files] = stood;
        s_take.taking.files += 1u;
    }
    g_cli_cells->persistent_buf_release(settings.out, &g_cli_machine_cells);
    return taken;
}

int cli_carry_take(const CliCarryHomeCfg *args)
{
    char *const written_cell = g_cli_cells->temporary_buf_alloc(CLI_CARRY_WRITTEN_ROOM, &g_cli_machine_cells);
    VerbaLinea written = mmgr_line_over(written_cell, CLI_CARRY_WRITTEN_ROOM);
    if (written.out == NULL)
    {
        mmgr_line_put(args->err_msg, "a taking written could not be given a cell");
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    EMBED_CALL(cli_carry_room_object_internal.room_begin, CliCarryRoomObjectCtx, .room = &s_take.room);
    CliTaking *const taking = &s_take.taking;
    taking->at = cli_wall_milliseconds();
    VerbaLinea id = {taking->id, sizeof(taking->id), 0ull};
    mmgr_line_decimal(&id, taking->at, 0u);
    mmgr_line_finish(&id);
    memor.cpy(&(MemoriaCfg){.dst = taking->from,
                            .src = args->from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->from, .cap = CLI_CELLS_POOL}) + 1u});
    taking->files = 0u;
    char carried[CLI_PATH_MAX_LEN];
    char backup_dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = "carried", .joined = carried);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = carried, .relative = taking->id, .joined = backup_dir);
    int taken = 1;
    for (unsigned int at = 0u; taken && (at < args->count); at += 1u)
    {
        if (EMBED_CALL(cli_carry_carried_internal.allow, CliCarryCarriedCtx, .place = args->places[at]))
        {
            taken = EMBED_CALL(cli_place_take, CarryTakeCtx, .home = args->home, .backup_dir = backup_dir,
                               .place = args->places[at], .there = args->there, .keys = args->keys,
                               .key_count = args->key_count, .err_msg = args->err_msg);
        }
    }
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = backup_dir, .relative = "taking.json", .joined = file);
    EMBED_CALL(cli_carry_out_internal.taking_write, CliCarryOutCtx, .taking = taking, .pretty = 1, .out = &written);
    taken = taken && mmgr_line_ok(&written) &&
            EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = backup_dir) &&
            EMBED_CALL(cli_write_on_to, CarryTakeCtx, .to = file, .text = written.out, .size = written.at,
                       .err_msg = args->err_msg);
    if (taken)
    {
        EMBED_CALL(cli_carry_out_internal.taking_write, CliCarryOutCtx, .taking = taking, .pretty = 0,
                   .out = args->out);
    }
    EMBED_CALL(cli_carry_room_object_internal.room_free, CliCarryRoomObjectCtx, .room = &s_take.room);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    g_cli_cells->persistent_buf_release(written.out, &g_cli_machine_cells);
    return taken;
}
