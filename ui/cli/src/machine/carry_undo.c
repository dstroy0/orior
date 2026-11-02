// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_undo.c: the takings saved, read back and written, and a taking undone
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "carry_internal.h"

void cli_carry_taking_write(const CliCarryOutCtx *args)
{
    mmgr_line_put(args->out, args->pretty ? "{\n  \"id\": " : "{\"id\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->taking->id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->taking->id, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, args->pretty ? ",\n  \"at\": " : ",\"at\":");
    mmgr_line_decimal(args->out, args->taking->at, 0u);
    mmgr_line_put(args->out, args->pretty ? ",\n  \"from\": " : ",\"from\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->taking->from,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->taking->from, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, args->pretty ? ",\n  \"files\": [" : ",\"files\":[");
    for (unsigned int at = 0u; at < args->taking->files; at += 1u)
    {
        if (args->pretty)
        {
            mmgr_line_put(args->out, (at != 0u) ? ",\n    [\n      " : "\n    [\n      ");
        }
        else
        {
            mmgr_line_put(args->out, (at != 0u) ? ",[" : "[");
        }
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->taking->place[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->taking->place[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->out, args->pretty ? ",\n      " : ",");
        mmgr_line_put(args->out, args->taking->stood[at] ? "true" : "false");
        mmgr_line_put(args->out, args->pretty ? "\n    ]" : "]");
    }
    if (args->pretty)
    {
        mmgr_line_put(args->out, (args->taking->files != 0u) ? "\n  ]\n}" : "]\n}");
    }
    else
    {
        mmgr_line_put(args->out, "]}");
    }
}

// The takings saved: each one's folder name and when it was taken, and the room one is read into
typedef struct
{
    char name[CLI_TAKINGS][64];
    unsigned long long at[CLI_TAKINGS];
    unsigned int count;
    const char *carried;
    CliTaking taking;
    CliCarryRoom room;
} CliTakings;

static CliTakings s_takings;

// the lock taken and the room begun empty under it
static void undo_lock(void)
{
    EMBED_CALL(cli_carry_room_object_internal.room_begin, CliCarryRoomObjectCtx, .room = &s_takings.room);
}

// the room's tree given back, and the lock
static void undo_release(void)
{
    EMBED_CALL(cli_carry_room_object_internal.room_free, CliCarryRoomObjectCtx, .room = &s_takings.room);
}

typedef struct
{
    const char *file;
    CliTaking *taking;
    const char *home;
    char *carried;
    const char *carried_read;
    const char *name;
} CarryUndoCtx;
// the taking saved at `file` read into `taking`: 1, or 0 where it is not there or not one
static int cli_taking_read(const CarryUndoCtx *args)
{
    CliJson *const json = &s_takings.room.json;
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->file,
                                                  .size_pointer = &size, .owner = &g_cli_machine_cells);
    if (bytes == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = json, .text = (const char *)bytes, .size = size, .nodes_more = 0u,
                                         .text_more = 0ull, .err_msg = &unread, .owner = &g_cli_machine_cells);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_machine_cells);
    const char *const id =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = root, .name = "id"),
                   .length_pointer = NULL);
    const char *const from =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = root, .name = "from"),
                   .length_pointer = NULL);
    const unsigned int at = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                       .object = root, .name = "at");
    const unsigned int files = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = root, .name = "files");
    if ((id == NULL) || (from == NULL) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = at,
                    .kind = CFG_JSON_NUMBER) ||
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = files,
                    .kind = CFG_JSON_ARRAY))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->taking->id,
                            .src = id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->taking->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    args->taking->at = 0ull;
    const CliJsonNode *const number = &json->node[at];
    for (unsigned long long digit = 0ull; digit < number->length; digit += 1ull)
    {
        const char letter = json->text[number->text + digit];
        if ((letter < '0') || (letter > '9'))
        {
            return 0;
        }
        args->taking->at = (args->taking->at * 10ull) + (unsigned long long)(letter - '0');
    }
    args->taking->files = 0u;
    for (unsigned int one = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = files, .index = one) != CLI_JSON_NONE;
         one += 1u)
    {
        const unsigned int pair = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = files, .index = one);
        const char *const place =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = pair, .index = 0u),
                       .length_pointer = NULL);
        const unsigned int stood = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = pair, .index = 1u);
        const int truth = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                     .node = stood, .kind = CFG_JSON_TRUE);
        if ((place == NULL) || (!truth && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                      .json_read = json, .node = stood, .kind = CFG_JSON_FALSE)))
        {
            return 0;
        }
        if (args->taking->files < CLI_TAKING_FILES)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->taking->place[args->taking->files],
                                    .src = place,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}) + 1u});
            args->taking->stood[args->taking->files] = truth ? 1u : 0u;
            args->taking->files += 1u;
        }
    }
    return 1;
}

static void cli_taking_each(const CliEachNameIsFolderCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_takings.carried, .relative = args->name, .joined = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "taking.json", .joined = file);
    if (!args->is_folder || (s_takings.count >= CLI_TAKINGS) ||
        (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) >= 64u) ||
        !EMBED_CALL(cli_taking_read, CarryUndoCtx, .file = file, .taking = &s_takings.taking))
    {
        return;
    }
    // the takings saved by when they were taken, the last first, those taken at once in the order they were read
    unsigned int place = s_takings.count;
    while ((place > 0u) && (s_takings.at[place - 1u] < s_takings.taking.at))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_takings.name[place],
            .src = s_takings.name[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_takings.name[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        s_takings.at[place] = s_takings.at[place - 1u];
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_takings.name[place],
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
    s_takings.at[place] = s_takings.taking.at;
    s_takings.count += 1u;
}

// every taking saved under `home`, by its folder, the last first
EMBED_INLINE void cli_takings_read(const CarryUndoCtx *args)
{
    char *carried = args->carried;
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = "carried", .joined = carried);
    s_takings.carried = carried;
    s_takings.count = 0u;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = carried, .each = cli_taking_each,
               .context_bytess = NULL);
}

// the taking saved in the folder `name` read into the room's taking: 1, or 0
EMBED_INLINE int cli_taking_get(const CarryUndoCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->carried_read, .relative = args->name,
               .joined = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "taking.json", .joined = file);
    return EMBED_CALL(cli_taking_read, CarryUndoCtx, .file = file, .taking = &s_takings.taking);
}

void cli_carry_takings(const CliCarryHomeToCfg *args)
{
    undo_lock();
    char carried[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_takings_read, CarryUndoCtx, .home = args->home, .carried = carried);
    mmgr_line_ch(args->out, '[');
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_takings.count; at += 1u)
    {
        if (EMBED_CALL(cli_taking_get, CarryUndoCtx, .carried_read = carried, .name = s_takings.name[at]))
        {
            mmgr_line_put(args->out, (written != 0u) ? "," : "");
            EMBED_CALL(cli_carry_out_internal.taking_write, CliCarryOutCtx, .taking = &s_takings.taking, .pretty = 0,
                       .out = args->out);
            written += 1u;
        }
    }
    mmgr_line_ch(args->out, ']');
    undo_release();
}

int cli_carry_undo(const CliCarryHomeOutCfg *args)
{
    undo_lock();
    char carried[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_takings_read, CarryUndoCtx, .home = args->home, .carried = carried);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < s_takings.count); at += 1u)
    {
        found = EMBED_CALL(cli_taking_get, CarryUndoCtx, .carried_read = carried, .name = s_takings.name[at]) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = s_takings.taking.id, .other = args->id, .cap = CLI_CELLS_POOL}));
    }
    if (!found)
    {
        mmgr_line_put(args->err_msg, "no taking ");
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is saved");
        undo_release();
        return 0;
    }
    const CliTaking *const taking = &s_takings.taking;
    char backup_dir[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = carried, .relative = taking->id, .joined = backup_dir);
    for (unsigned int at = 0u; at < taking->files; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        char saved[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->home, .relative = taking->place[at],
                   .joined = path);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = backup_dir, .relative = taking->place[at],
                   .joined = saved);
        if (taking->stood[at])
        {
            unsigned long long size = 0ull;
            const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = saved,
                                                          .size_pointer = &size, .owner = &g_cli_machine_cells);
            const int back =
                (bytes != NULL) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                              .path = path, .bytes = bytes, .size = size);
            EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                       .owner = &g_cli_machine_cells);
            if (!back)
            {
                mmgr_line_put(args->err_msg, (bytes == NULL) ? saved : path);
                mmgr_line_put(args->err_msg, (bytes == NULL) ? ": it could not be read" : ": it could not be written");
                undo_release();
                return 0;
            }
        }
        else if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path) &&
                 !EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path))
        {
            mmgr_line_put(args->err_msg, path);
            mmgr_line_put(args->err_msg, ": it could not be taken out");
            undo_release();
            return 0;
        }
    }
    if (!EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = backup_dir))
    {
        mmgr_line_put(args->err_msg, backup_dir);
        mmgr_line_put(args->err_msg, ": it could not be taken out");
        undo_release();
        return 0;
    }
    EMBED_CALL(cli_carry_out_internal.taking_write, CliCarryOutCtx, .taking = taking, .pretty = 0, .out = args->out);
    undo_release();
    return 1;
}
