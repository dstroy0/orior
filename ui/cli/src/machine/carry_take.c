// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_take.c: what is carried here compared with orior's own folder, taken in as the reader chooses with each file it
// writes over kept, and a taking undone
#include "../text/text.h"
#include "carry_internal.h"

// the most keys a settings comparison reads, and the most bytes of the settings that differ or are written
#define CLI_CARRY_KEYS 4096u
#define CLI_CARRY_SETTINGS (1u << 24u)

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
static void cli_value_written(CliLine *out, unsigned int value)
{
    static char s_value[1u << 20u];
    if (value == CLI_JSON_NONE)
    {
        cli_line_text(out, "null");
        return;
    }
    CliLine line = {s_value, sizeof(s_value), 0ull};
    cli_json_write(&s_take.room.json, value, &line, 0u, 0);
    cli_json_quoted(out, s_value, line.at);
}

// the settings that differ between two settings texts written, each with its value here and there: 1 where any do
static int cli_settings_differ(const CliCarried *there, unsigned int mine_at, unsigned int theirs_at, CliLine *out)
{
    CliJson *const json = &s_take.room.json;
    const unsigned int mine = cli_carry_settings_of(
        &s_take.room, (mine_at < s_take.here.count) ? cli_carried_value(&s_take.here, mine_at) : "",
        (mine_at < s_take.here.count) ? s_take.here.size[mine_at] : 0ull, 1);
    const unsigned int theirs =
        cli_carry_settings_of(&s_take.room, (theirs_at < there->count) ? cli_carried_value(there, theirs_at) : "",
                              (theirs_at < there->count) ? there->size[theirs_at] : 0ull, 0);
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
        cli_line_text(out, (written != 0u) ? ",{\"key\":" : "{\"key\":");
        cli_json_quoted(out, s_take.key[at],
                        cellul.len(&(CatenaFinitaCfg){.src = s_take.key[at], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"here\":");
        cli_value_written(out, here);
        cli_line_text(out, ",\"there\":");
        cli_value_written(out, away);
        cli_line_character(out, '}');
        written += 1u;
    }
    return written != 0u;
}

void cli_carry_differences(const char *home, const CliCarried *there, CliLine *out)
{
    cli_lock_take(&s_lock);
    char *const settings_text = cli_cell_taken(CLI_CARRY_SETTINGS, "the settings that differ");
    if ((settings_text == NULL) || !cli_carry_room_taken(&s_take.room))
    {
        cli_cell_given(settings_text);
        cli_line_text(out, "[]");
        cli_lock_give(&s_lock);
        return;
    }
    char why[CLI_NAME_LONGEST];
    CliLine unpacked = {why, sizeof(why), 0ull};
    if (!cli_carry_pack(home, &s_take.here, &unpacked))
    {
        cli_carried_clear(&s_take.here);
    }
    cli_line_character(out, '[');
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
        CliLine settings = {settings_text, CLI_CARRY_SETTINGS, 0ull};
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
        cli_line_text(out, (written != 0u) ? ",{\"place\":" : "{\"place\":");
        cli_json_quoted(out, place, cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"how\":\"");
        cli_line_text(out, how);
        cli_line_text(out, "\",\"settings\":[");
        cli_line_counted(out, settings_text, settings.at);
        cli_line_text(out, "]}");
        written += 1u;
    }
    cli_line_character(out, ']');
    cli_carried_clear(&s_take.here);
    cli_carry_room_given(&s_take.room);
    cli_cell_given(settings_text);
    cli_lock_give(&s_lock);
}

// the text of the file at `path` written whole at `to`, the folder above it made: 1, or 0 with why said
static int cli_written_to(const char *to, const char *text, unsigned long long size, CliLine *said)
{
    char parent[CLI_PATH_LONGEST];
    if ((cli_path_parent(to, parent) && !query_folder_made(parent)) || !query_file_written(to, text, size))
    {
        cli_line_text(said, to);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

// the text of the file at `path` where it is there and UTF-8, mapped: its bytes, or NULL
static const unsigned char *cli_text_at(const char *path, unsigned long long *size)
{
    const unsigned char *const bytes = qry_member_file_mapped(path, size);
    for (unsigned long long at = 0ull; (bytes != NULL) && (at < *size);)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, *size, at, &letter);
        if ((step == 1ull) && (bytes[at] >= 0x80u))
        {
            qry_member_file_unmapped(bytes, *size);
            return NULL;
        }
        at += step;
    }
    return bytes;
}

// the settings of this machine kept and of `keys` taken from there, written into `out`
static void cli_settings_taken(const char *before, unsigned long long before_size, const CliCarried *there,
                               const char *const *keys, unsigned int key_count, CliLine *out)
{
    CliJson *const json = &s_take.room.json;
    const unsigned int theirs_at = cli_carried_find(there, "settings.json");
    const unsigned int mine = cli_carry_settings_of(&s_take.room, before, before_size, 1);
    const unsigned int theirs =
        cli_carry_settings_of(&s_take.room, (theirs_at < there->count) ? cli_carried_value(there, theirs_at) : "",
                              (theirs_at < there->count) ? there->size[theirs_at] : 0ull, 0);
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
}

// one place taken in: its file kept in `kept` where one stood, and written as there: 1, or 0 with why said
static int cli_place_taken(const char *home, const char *kept, const char *place, const CliCarried *there,
                           const char *const *keys, unsigned int key_count, CliLine *said)
{
    char *const settings_text = cli_cell_taken(CLI_CARRY_SETTINGS, "the settings written");
    if (settings_text == NULL)
    {
        cli_line_text(said, "the settings written could not be given a cell");
        return 0;
    }
    char path[CLI_PATH_LONGEST];
    cli_full(home, place, path);
    unsigned long long before_size = 0ull;
    const unsigned char *const before = cli_text_at(path, &before_size);
    const char *text = NULL;
    unsigned long long size = 0ull;
    CliLine settings = {settings_text, CLI_CARRY_SETTINGS, 0ull};
    if (cellul.eq(&(CatenaFinitaCfg){.src = place, .other = "settings.json", .cap = CLI_CELLS_POOL}))
    {
        cli_settings_taken((before != NULL) ? (const char *)before : "", before_size, there, keys, key_count,
                           &settings);
        text = settings_text;
        size = settings.at;
    }
    else
    {
        const unsigned int at = cli_carried_find(there, place);
        text = (at < there->count) ? cli_carried_value(there, at) : NULL;
        size = (at < there->count) ? there->size[at] : 0ull;
    }
    int taken = 1;
    if ((text != NULL) && (before != NULL))
    {
        char saved[CLI_PATH_LONGEST];
        cli_full(kept, place, saved);
        taken = cli_written_to(saved, (const char *)before, before_size, said);
    }
    // the file is let go of before it is written over: a file still mapped is not truncated on Windows, and reading a
    // view of one that was is a fault elsewhere; what the writing needs of it is in the settings written and the saved
    // copy already
    const unsigned int stood = (before != NULL) ? 1u : 0u;
    if (before != NULL)
    {
        qry_member_file_unmapped(before, before_size);
    }
    taken = taken && ((text == NULL) || cli_written_to(path, text, size, said));
    if (taken && (text != NULL) && (s_take.taking.files < CLI_TAKING_FILES))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_take.taking.place[s_take.taking.files], .bytes = CLI_NAME_LONGEST, .src = place});
        s_take.taking.stood[s_take.taking.files] = stood;
        s_take.taking.files += 1u;
    }
    cli_cell_given(settings_text);
    return taken;
}

int cli_carry_take(const char *home, const char *from, const CliCarried *there, const char *const *places,
                   unsigned int count, const char *const *keys, unsigned int key_count, CliLine *out, CliLine *said)
{
    static char s_written[1u << 20u];
    cli_lock_take(&s_lock);
    if (!cli_carry_room_taken(&s_take.room))
    {
        cli_line_text(said, "the settings' cells could not be taken");
        cli_lock_give(&s_lock);
        return 0;
    }
    CliTaking *const taking = &s_take.taking;
    taking->at = cli_wall_milliseconds();
    CliLine id = {taking->id, sizeof(taking->id), 0ull};
    cli_line_decimal(&id, taking->at, 0u);
    cli_line_closed(&id);
    memor.cpy(&(MemoriaCfg){.dst = taking->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    taking->files = 0u;
    char carried[CLI_PATH_LONGEST];
    char kept[CLI_PATH_LONGEST];
    cli_full(home, "carried", carried);
    cli_full(carried, taking->id, kept);
    int taken = 1;
    for (unsigned int at = 0u; taken && (at < count); at += 1u)
    {
        if (cli_carry_allowed(places[at]))
        {
            taken = cli_place_taken(home, kept, places[at], there, keys, key_count, said);
        }
    }
    char file[CLI_PATH_LONGEST];
    cli_full(kept, "taking.json", file);
    CliLine written = {s_written, sizeof(s_written), 0ull};
    cli_carry_taking_written(taking, 1, &written);
    taken = taken && query_folder_made(kept) && cli_written_to(file, s_written, written.at, said);
    if (taken)
    {
        cli_carry_taking_written(taking, 0, out);
    }
    cli_carry_room_given(&s_take.room);
    cli_lock_give(&s_lock);
    return taken;
}
