// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_undo.c: the takings kept, read back and written, and a taking undone
#include "carry_internal.h"

void cli_carry_taking_written(const CliTaking *taking, int pretty, CliLine *out)
{
    cli_line_text(out, pretty ? "{\n  \"id\": " : "{\"id\":");
    cli_json_quoted(out, taking->id, cellul.len(&(CatenaFinitaCfg){.src = taking->id, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, pretty ? ",\n  \"at\": " : ",\"at\":");
    cli_line_decimal(out, taking->at, 0u);
    cli_line_text(out, pretty ? ",\n  \"from\": " : ",\"from\":");
    cli_json_quoted(out, taking->from, cellul.len(&(CatenaFinitaCfg){.src = taking->from, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, pretty ? ",\n  \"files\": [" : ",\"files\":[");
    for (unsigned int at = 0u; at < taking->files; at += 1u)
    {
        if (pretty)
        {
            cli_line_text(out, (at != 0u) ? ",\n    [\n      " : "\n    [\n      ");
        }
        else
        {
            cli_line_text(out, (at != 0u) ? ",[" : "[");
        }
        cli_json_quoted(out, taking->place[at],
                        cellul.len(&(CatenaFinitaCfg){.src = taking->place[at], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, pretty ? ",\n      " : ",");
        cli_line_text(out, taking->stood[at] ? "true" : "false");
        cli_line_text(out, pretty ? "\n    ]" : "]");
    }
    if (pretty)
    {
        cli_line_text(out, (taking->files != 0u) ? "\n  ]\n}" : "]\n}");
    }
    else
    {
        cli_line_text(out, "]}");
    }
}

// The takings kept: each one's folder name and when it was taken, and the room one is read into
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
static CliLock s_lock = CLI_LOCK_INIT;

// the lock taken and the room begun empty under it
static void undo_held(void)
{
    cli_lock_take(&s_lock);
    cli_carry_room_begun(&s_takings.room);
}

// the room's tree given back, and the lock
static void undo_released(void)
{
    cli_carry_room_given(&s_takings.room);
    cli_lock_give(&s_lock);
}

// the taking kept at `file` read into `taking`: 1, or 0 where it is not there or not one
static int cli_taking_read(const char *file, CliTaking *taking)
{
    CliJson *const json = &s_takings.room.json;
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(file, &size);
    if (bytes == NULL)
    {
        return 0;
    }
    cli_json_given(json);
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(json, (const char *)bytes, size, 0u, 0ull, &unread);
    cli_file_let_go(bytes, size);
    const char *const id = cli_json_string(json, cli_json_member(json, root, "id"), NULL);
    const char *const from = cli_json_string(json, cli_json_member(json, root, "from"), NULL);
    const unsigned int at = cli_json_member(json, root, "at");
    const unsigned int files = cli_json_member(json, root, "files");
    if ((id == NULL) || (from == NULL) || !cli_json_is(json, at, CFG_JSON_NUMBER) ||
        !cli_json_is(json, files, CFG_JSON_ARRAY))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = taking->id, .src = id, .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = taking->from,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
    taking->at = 0ull;
    const CliJsonNode *const number = &json->node[at];
    for (unsigned long long digit = 0ull; digit < number->length; digit += 1ull)
    {
        const char letter = json->text[number->text + digit];
        if ((letter < '0') || (letter > '9'))
        {
            return 0;
        }
        taking->at = (taking->at * 10ull) + (unsigned long long)(letter - '0');
    }
    taking->files = 0u;
    for (unsigned int one = 0u; cli_json_item(json, files, one) != CLI_JSON_NONE; one += 1u)
    {
        const unsigned int pair = cli_json_item(json, files, one);
        const char *const place = cli_json_string(json, cli_json_item(json, pair, 0u), NULL);
        const unsigned int stood = cli_json_item(json, pair, 1u);
        const int truth = cli_json_is(json, stood, CFG_JSON_TRUE);
        if ((place == NULL) || (!truth && !cli_json_is(json, stood, CFG_JSON_FALSE)))
        {
            return 0;
        }
        if (taking->files < CLI_TAKING_FILES)
        {
            memor.cpy(&(MemoriaCfg){.dst = taking->place[taking->files],
                                    .src = place,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}) + 1u});
            taking->stood[taking->files] = truth ? 1u : 0u;
            taking->files += 1u;
        }
    }
    return 1;
}

static void cli_taking_each(void *context, const char *name, int is_folder)
{
    (void)context;
    char folder[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    cli_full(s_takings.carried, name, folder);
    cli_full(folder, "taking.json", file);
    if (!is_folder || (s_takings.count >= CLI_TAKINGS) ||
        (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) >= 64u) ||
        !cli_taking_read(file, &s_takings.taking))
    {
        return;
    }
    // the takings kept by when they were taken, the last first, those taken at once in the order they were read
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
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    s_takings.at[place] = s_takings.taking.at;
    s_takings.count += 1u;
}

// every taking kept under `home`, by its folder, the last first
static void cli_takings_read(const char *home, char *carried)
{
    cli_full(home, "carried", carried);
    s_takings.carried = carried;
    s_takings.count = 0u;
    query_folder_each(carried, cli_taking_each, NULL);
}

// the taking kept in the folder `name` read into the room's taking: 1, or 0
static int cli_taking_of(const char *carried, const char *name)
{
    char folder[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    cli_full(carried, name, folder);
    cli_full(folder, "taking.json", file);
    return cli_taking_read(file, &s_takings.taking);
}

void cli_carry_takings(const char *home, CliLine *out)
{
    undo_held();
    char carried[CLI_PATH_LONGEST];
    cli_takings_read(home, carried);
    cli_line_character(out, '[');
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_takings.count; at += 1u)
    {
        if (cli_taking_of(carried, s_takings.name[at]))
        {
            cli_line_text(out, (written != 0u) ? "," : "");
            cli_carry_taking_written(&s_takings.taking, 0, out);
            written += 1u;
        }
    }
    cli_line_character(out, ']');
    undo_released();
}

int cli_carry_undo(const char *home, const char *id, CliLine *out, CliLine *said)
{
    undo_held();
    char carried[CLI_PATH_LONGEST];
    cli_takings_read(home, carried);
    int found = 0;
    for (unsigned int at = 0u; !found && (at < s_takings.count); at += 1u)
    {
        found = cli_taking_of(carried, s_takings.name[at]) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = s_takings.taking.id, .other = id, .cap = CLI_CELLS_POOL}));
    }
    if (!found)
    {
        cli_line_text(said, "no taking ");
        cli_line_text(said, id);
        cli_line_text(said, " is kept");
        undo_released();
        return 0;
    }
    const CliTaking *const taking = &s_takings.taking;
    char kept[CLI_PATH_LONGEST];
    cli_full(carried, taking->id, kept);
    for (unsigned int at = 0u; at < taking->files; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        char saved[CLI_PATH_LONGEST];
        cli_full(home, taking->place[at], path);
        cli_full(kept, taking->place[at], saved);
        if (taking->stood[at])
        {
            unsigned long long size = 0ull;
            const unsigned char *const bytes = cli_file_held(saved, &size);
            const int back = (bytes != NULL) && query_file_written(path, bytes, size);
            if (bytes != NULL)
            {
                cli_file_let_go(bytes, size);
            }
            if (!back)
            {
                cli_line_text(said, (bytes == NULL) ? saved : path);
                cli_line_text(said, (bytes == NULL) ? ": it could not be read" : ": it could not be written");
                undo_released();
                return 0;
            }
        }
        else if (query_is_file(path) && !query_removed(path))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be taken out");
            undo_released();
            return 0;
        }
    }
    if (!query_removed(kept))
    {
        cli_line_text(said, kept);
        cli_line_text(said, ": it could not be taken out");
        undo_released();
        return 0;
    }
    cli_carry_taking_written(taking, 0, out);
    undo_released();
    return 1;
}
