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

// the members of a settings object
static unsigned int cli_member_first(unsigned int object)
{
    return s_take.room.json.node[object].first;
}

// the keys of two settings objects, in order, each once, less the settings of this machine alone
static void cli_keys_of(unsigned int mine, unsigned int theirs)
{
    CliJson *const json = &s_take.room.json;
    s_take.keys = 0u;
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        for (unsigned int member = cli_member_first((side == 0u) ? mine : theirs); member != CLI_JSON_NONE;
             member = json->node[member].next)
        {
            const char *const key = json->text + json->node[member].key;
            unsigned int at = 0u;
            while ((at < s_take.keys) && (cli_text_order(s_take.key[at], key) < 0))
            {
                at += 1u;
            }
            if (((at < s_take.keys) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_take.key[at], .other = key, .cap = CLI_CELLS_POOL}))) ||
                cli_carry_machine(key) || (s_take.keys >= CLI_CARRY_KEYS))
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
static void cli_value_written(VerbaLinea *out, unsigned int value)
{
    const unsigned long long line_room = cli_json_written_room(&s_take.room.json);
    char *const line_cell =
        (value != CLI_JSON_NONE) ? g_cli_cells->temporary_buf_alloc(line_room, &g_cli_machine_cells) : NULL;
    VerbaLinea line = mmgr_line_over(line_cell, line_room);
    if (line_cell == NULL)
    {
        mmgr_line_put(out, "null");
        return;
    }
    cli_json_write(&s_take.room.json, value, &line, 0u, 0);
    cli_json_quoted(out, line.out, mmgr_line_ok(&line) ? line.at : 0ull);
    g_cli_cells->persistent_buf_release(line.out, &g_cli_machine_cells);
}

// the settings that differ between two settings texts written, each with its value here and there: 1 where any do
static int cli_settings_differ(const CliCarried *there, unsigned int mine_at, unsigned int theirs_at, VerbaLinea *out)
{
    CliJson *const json = &s_take.room.json;
    const unsigned long long theirs_size = (theirs_at < there->count) ? there->size[theirs_at] : 0ull;
    const unsigned int mine = cli_carry_settings_of(
        &s_take.room, (mine_at < s_take.here.count) ? cli_carried_value(&s_take.here, mine_at) : "",
        (mine_at < s_take.here.count) ? s_take.here.size[mine_at] : 0ull, theirs_size);
    const unsigned int theirs =
        (mine != CLI_JSON_NONE)
            ? cli_carry_settings_beside(
                  &s_take.room, (theirs_at < there->count) ? cli_carried_value(there, theirs_at) : "", theirs_size)
            : CLI_JSON_NONE;
    if (theirs == CLI_JSON_NONE)
    {
        return 0;
    }
    cli_keys_of(mine, theirs);
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_take.keys; at += 1u)
    {
        const unsigned int here = cli_json_member(json, mine, s_take.key[at]);
        const unsigned int away = cli_json_member(json, theirs, s_take.key[at]);
        const int same = (here != CLI_JSON_NONE) && (away != CLI_JSON_NONE) && cli_json_same(json, here, json, away);
        if (same || ((here == CLI_JSON_NONE) && (away == CLI_JSON_NONE)))
        {
            continue;
        }
        mmgr_line_put(out, (written != 0u) ? ",{\"key\":" : "{\"key\":");
        cli_json_quoted(out, s_take.key[at],
                        cellul.len(&(CatenaFinitaCfg){.src = s_take.key[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"here\":");
        cli_value_written(out, here);
        mmgr_line_put(out, ",\"there\":");
        cli_value_written(out, away);
        mmgr_line_ch(out, '}');
        written += 1u;
    }
    return written != 0u;
}

void cli_carry_differences(const char *home, const CliCarried *there, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    char *const settings_cell = g_cli_cells->temporary_buf_alloc(CLI_CARRY_WRITTEN_ROOM, &g_cli_machine_cells);
    VerbaLinea settings = mmgr_line_over(settings_cell, CLI_CARRY_WRITTEN_ROOM);
    if (settings.out == NULL)
    {
        mmgr_line_put(out, "[]");
        cli_lock_give(&s_lock);
        return;
    }
    cli_carry_room_begun(&s_take.room);
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unpacked = {why, sizeof(why), 0ull};
    if (!cli_carry_pack(home, &s_take.here, &unpacked))
    {
        cli_carried_clear(&s_take.here);
    }
    mmgr_line_ch(out, '[');
    unsigned int written = 0u;
    unsigned int mine_at = 0u;
    unsigned int theirs_at = 0u;
    // the places of both sides, in order
    while ((mine_at < s_take.here.count) || (theirs_at < there->count))
    {
        const int has_mine = mine_at < s_take.here.count;
        const int has_theirs = theirs_at < there->count;
        const int order = !has_mine     ? 1
                          : !has_theirs ? -1
                                        : cli_text_order(cli_carried_place(&s_take.here, mine_at),
                                                         cli_carried_place(there, theirs_at));
        const unsigned int mine = (order <= 0) ? mine_at : s_take.here.count;
        const unsigned int theirs = (order >= 0) ? theirs_at : there->count;
        const char *const place =
            (order <= 0) ? cli_carried_place(&s_take.here, mine_at) : cli_carried_place(there, theirs_at);
        mine_at += (order <= 0) ? 1u : 0u;
        theirs_at += (order >= 0) ? 1u : 0u;
        const char *how = NULL;
        settings.at = 0ull;
        if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL}))
        {
            how = cli_settings_differ(there, mine, theirs, &settings) ? "changed" : NULL;
        }
        else if ((mine < s_take.here.count) && (theirs < there->count))
        {
            const int same = (s_take.here.size[mine] == there->size[theirs]) &&
                             (memor.cmp(&(MemoriaCfg){.src = cli_carried_value(&s_take.here, mine),
                                                      .other = cli_carried_value(there, theirs),
                                                      .bytes = there->size[theirs]}) == 0);
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
        mmgr_line_put(out, (written != 0u) ? ",{\"place\":" : "{\"place\":");
        cli_json_quoted(out, place, cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"how\":\"");
        mmgr_line_put(out, how);
        mmgr_line_put(out, "\",\"settings\":[");
        mmgr_line_put_n(out, settings.out, mmgr_line_ok(&settings) ? settings.at : 0ull);
        mmgr_line_put(out, "]}");
        written += 1u;
    }
    mmgr_line_ch(out, ']');
    cli_carried_clear(&s_take.here);
    cli_carry_room_given(&s_take.room);
    g_cli_cells->persistent_buf_release(settings.out, &g_cli_machine_cells);
    cli_lock_give(&s_lock);
}

// the text of the file at `path` written whole at `to`, the folder above it made: 1, or 0 with why given
static int cli_written_to(const char *to, const char *text, unsigned long long size, VerbaLinea *err_msg)
{
    char parent[CLI_PATH_MAX_LEN];
    if ((cli_path_parent(to, parent) && !query_folder_made(parent)) || !query_file_written(to, text, size))
    {
        mmgr_line_put(err_msg, to);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// the settings of this machine retained and of `keys` taken from there, written into `out`: 1, or 0 where the settings'
// tree is refused its cell or they do not fit
static int cli_settings_taken(const char *before, unsigned long long before_size, const CliCarried *there,
                              const char *const *keys, unsigned int key_count, VerbaLinea *out)
{
    CliJson *const json = &s_take.room.json;
    const unsigned int theirs_at = cli_carried_find(there, "settings.json");
    const unsigned long long theirs_size = (theirs_at < there->count) ? there->size[theirs_at] : 0ull;
    const unsigned int mine = cli_carry_settings_of(&s_take.room, before, before_size, theirs_size);
    const unsigned int theirs =
        (mine != CLI_JSON_NONE)
            ? cli_carry_settings_beside(
                  &s_take.room, (theirs_at < there->count) ? cli_carried_value(there, theirs_at) : "", theirs_size)
            : CLI_JSON_NONE;
    if (theirs == CLI_JSON_NONE)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < key_count; at += 1u)
    {
        if (cli_carry_machine(keys[at]))
        {
            continue;
        }
        const unsigned int value = cli_json_member(json, theirs, keys[at]);
        if (value != CLI_JSON_NONE)
        {
            cli_json_set(json, mine, keys[at], cli_json_copy(json, value));
        }
        else
        {
            cli_json_remove(json, mine, keys[at]);
        }
    }
    cli_carry_settings_written(&s_take.room, mine, out);
    return mmgr_line_ok(out);
}

// one place taken in: its file saved in `backup_dir` where one stood, and written as there: 1, or 0 with why given
static int cli_place_taken(const char *home, const char *backup_dir, const char *place, const CliCarried *there,
                           const char *const *keys, unsigned int key_count, VerbaLinea *err_msg)
{
    char *const settings_cell = g_cli_cells->temporary_buf_alloc(CLI_CARRY_WRITTEN_ROOM, &g_cli_machine_cells);
    VerbaLinea settings = mmgr_line_over(settings_cell, CLI_CARRY_WRITTEN_ROOM);
    if (settings.out == NULL)
    {
        mmgr_line_put(err_msg, "the settings written could not be given a cell");
        return 0;
    }
    char path[CLI_PATH_MAX_LEN];
    cli_full(home, place, path);
    unsigned long long before_size = 0ull;
    const unsigned char *const before = cli_file_taken_utf8(path, &before_size, &g_cli_machine_cells);
    const char *text = NULL;
    unsigned long long size = 0ull;
    int taken = 1;
    if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL}))
    {
        taken = cli_settings_taken((before != NULL) ? (const char *)before : "", before_size, there, keys, key_count,
                                   &settings);
        if (!taken)
        {
            mmgr_line_put(err_msg, "the settings taken could not be given cells");
        }
        text = settings.out;
        size = settings.at;
    }
    else
    {
        const unsigned int at = cli_carried_find(there, place);
        text = (at < there->count) ? cli_carried_value(there, at) : NULL;
        size = (at < there->count) ? there->size[at] : 0ull;
    }
    if (taken && (text != NULL) && (before != NULL))
    {
        char saved[CLI_PATH_MAX_LEN];
        cli_full(backup_dir, place, saved);
        taken = cli_written_to(saved, (const char *)before, before_size, err_msg);
    }
    // the file is let go of before it is written over: a file still mapped is not truncated on Windows, and reading a
    // view of one that was is a fault elsewhere; what the writing needs of it is in the settings written and the saved
    // copy already
    const unsigned int stood = (before != NULL) ? 1u : 0u;
    cli_file_given(before, &g_cli_machine_cells);
    if (taken && (text != NULL))
    {
        taken = cli_written_to(path, text, size, err_msg);
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

int cli_carry_take(const char *home, const char *from, const CliCarried *there, const char *const *places,
                   unsigned int count, const char *const *keys, unsigned int key_count, VerbaLinea *out,
                   VerbaLinea *err_msg)
{
    char *const written_cell = g_cli_cells->temporary_buf_alloc(CLI_CARRY_WRITTEN_ROOM, &g_cli_machine_cells);
    VerbaLinea written = mmgr_line_over(written_cell, CLI_CARRY_WRITTEN_ROOM);
    if (written.out == NULL)
    {
        mmgr_line_put(err_msg, "a taking written could not be given a cell");
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_carry_room_begun(&s_take.room);
    CliTaking *const taking = &s_take.taking;
    taking->at = cli_wall_milliseconds();
    VerbaLinea id = {taking->id, sizeof(taking->id), 0ull};
    mmgr_line_decimal(&id, taking->at, 0u);
    mmgr_line_finish(&id);
    memor.cpy(&(MemoriaCfg){.dst = taking->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    taking->files = 0u;
    char carried[CLI_PATH_MAX_LEN];
    char backup_dir[CLI_PATH_MAX_LEN];
    cli_full(home, "carried", carried);
    cli_full(carried, taking->id, backup_dir);
    int taken = 1;
    for (unsigned int at = 0u; taken && (at < count); at += 1u)
    {
        if (cli_carry_allowed(places[at]))
        {
            taken = cli_place_taken(home, backup_dir, places[at], there, keys, key_count, err_msg);
        }
    }
    char file[CLI_PATH_MAX_LEN];
    cli_full(backup_dir, "taking.json", file);
    cli_carry_taking_written(taking, 1, &written);
    taken = taken && mmgr_line_ok(&written) && query_folder_made(backup_dir) &&
            cli_written_to(file, written.out, written.at, err_msg);
    if (taken)
    {
        cli_carry_taking_written(taking, 0, out);
    }
    cli_carry_room_given(&s_take.room);
    cli_lock_give(&s_lock);
    g_cli_cells->persistent_buf_release(written.out, &g_cli_machine_cells);
    return taken;
}
