// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_added.c: the toolchains and groups the reader added, kept in user_toolchains.json in orior's own folder,
// and the files of orior's own folder these are kept in read and written
#include "toolchains_internal.h"

CliLock g_cli_toolchains_lock = CLI_LOCK_INIT;

// the file `name` in orior's own folder into `path`: 1, or 0 where orior has no folder of its own
int cli_toolchains_file(const char *name, char *path)
{
    if (!cli_home_folder(path))
    {
        path[0] = '\0';
        return 0;
    }
    CliLine line = {path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}),
                    CLI_PATH_LONGEST - cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), 0ull};
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    return cli_line_closed(&line) != 0ull;
}

unsigned int cli_toolchains_file_read(CliJson *json, const char *path, unsigned int nodes_more,
                                      unsigned long long text_more)
{
    // the empty object made where the file is none takes a node of the room over
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int read = cli_json_file_taken(json, path, nodes_more + 1u, text_more, &said);
    if (json->node == NULL)
    {
        (void)cli_json_held(json, nodes_more + 1u, text_more, "the reader's toolchains");
    }
    return cli_json_is(json, read, CFG_JSON_OBJECT) ? read : cli_json_made(json, CFG_JSON_OBJECT);
}

int cli_toolchains_file_kept(const CliJson *json, unsigned int node, const char *path, int sorted, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (cli_home_folder(folder))
    {
        query_folder_made(folder);
    }
    // the text handed to the file as it is written, a piece at a time
    CliFileWriting file;
    char piece[4096];
    const int begun = cli_file_begun(&file, path);
    CliLine out = {piece, sizeof(piece), 0ull, cli_file_put, &file, 0ull};
    if (begun)
    {
        cli_json_write(json, node, &out, 2u, sorted);
        cli_line_character(&out, '\n');
    }
    const int written = begun && cli_line_flushed(&out);
    const int ended = begun && cli_file_ended(&file, written);
    if (!written || !ended)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

// The reader's added groups and tools: the `groups` and `tools` arrays of their file, each made where it is not there
static void cli_added_lists(CliJson *json, unsigned int added, unsigned int *groups, unsigned int *tools)
{
    *groups = cli_json_member(json, added, "groups");
    if (!cli_json_is(json, *groups, CFG_JSON_ARRAY))
    {
        *groups = cli_json_made(json, CFG_JSON_ARRAY);
        cli_json_set(json, added, "groups", *groups);
    }
    *tools = cli_json_member(json, added, "tools");
    if (!cli_json_is(json, *tools, CFG_JSON_ARRAY))
    {
        *tools = cli_json_made(json, CFG_JSON_ARRAY);
        cli_json_set(json, added, "tools", *tools);
    }
}

// whether a list holds a node whose `key`, or itself where `key` is NULL, is the text `text`
static int cli_holds(const CliJson *json, unsigned int list, const char *key, const char *text)
{
    for (unsigned int item = cli_json_is(json, list, CFG_JSON_ARRAY) ? json->node[list].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const held = cli_json_string(json, (key != NULL) ? cli_json_member(json, item, key) : item, NULL);
        if ((held != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = held, .other = text, .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
    }
    return 0;
}

// orior's own tools, read whole into a tree sized to them, which cli_json_given gives back: the array of them
static unsigned int cli_bundled(CliJson *json)
{
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    // the program's own bytes are toolchains.json's text
    const unsigned int read =
        cli_json_taken(json, (const char *)g_cli_toolchains_json, g_cli_toolchains_json_size, 0u, 0ull, &said);
    return cli_json_member(json, read, "tools");
}

// each group handed on once, in order: those of orior's tools, of the reader's tools, then the reader's own
static void cli_groups_of(const CliJson *bundled, unsigned int tools, const CliJson *json, unsigned int added_tools,
                          unsigned int added_groups, void (*each)(void *context, const char *group), void *context)
{
    static char s_seen[512][CLI_NAME_LONGEST];
    unsigned int seen = 0u;
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        const CliJson *const in = (list == 0u) ? bundled : json;
        const unsigned int array = (list == 0u) ? tools : (list == 1u) ? added_tools : added_groups;
        for (unsigned int item = cli_json_is(in, array, CFG_JSON_ARRAY) ? in->node[array].first : CLI_JSON_NONE;
             item != CLI_JSON_NONE; item = in->node[item].next)
        {
            const char *const group =
                cli_json_string(in, (list == 2u) ? item : cli_json_member(in, item, "group"), NULL);
            int known = group == NULL;
            for (unsigned int at = 0u; !known && (at < seen); at += 1u)
            {
                known = cellul.eq(&(CatenaFinitaCfg){.src = s_seen[at], .other = group, .cap = CLI_CELLS_POOL});
            }
            if (!known && (seen < 512u))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = s_seen[seen],
                                  .src = group,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_CELLS_POOL}) + 1u});
                seen += 1u;
                each(context, group);
            }
        }
    }
}

void cli_toolchains_groups(void (*each)(void *context, const char *group), void *context)
{
    CliJson bundled;
    CliJson json;
    char path[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_toolchains_lock);
    const unsigned int tools = cli_bundled(&bundled);
    // the two lists made where the file has none take two nodes and their keys
    const unsigned int added =
        cli_toolchains_file_read(&json, cli_toolchains_file("user_toolchains.json", path) ? path : "", 2u, 16ull);
    unsigned int groups = CLI_JSON_NONE;
    unsigned int added_tools = CLI_JSON_NONE;
    cli_added_lists(&json, added, &groups, &added_tools);
    cli_groups_of(&bundled, tools, &json, added_tools, groups, each, context);
    cli_json_given(&json);
    cli_json_given(&bundled);
    cli_lock_give(&g_cli_toolchains_lock);
}

// whether a group is among those listed: kept while each is handed
typedef struct
{
    const char *group;
    int found;
} CliGroupFound;

static void cli_group_found(void *context, const char *group)
{
    CliGroupFound *const found = (CliGroupFound *)context;
    found->found =
        found->found || (cellul.eq(&(CatenaFinitaCfg){.src = found->group, .other = group, .cap = CLI_CELLS_POOL}));
}

// a text trimmed of ASCII's spaces at each end into `out`, which holds `room`
void cli_toolchains_trimmed(const char *text, char *out, unsigned long long room)
{
    const char *from = (text != NULL) ? text : "";
    const char *to = from + cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
    {
        to -= 1;
    }
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, from, (unsigned long long)(to - from));
    cli_line_closed(&line);
}

// An id made from a name: its letters and digits in lower case, each run of anything else a dash
static void cli_id_of(const char *name, char *id)
{
    unsigned long long length = 0ull;
    for (const char *at = name; (at[0] != '\0') && ((length + 1ull) < CLI_NAME_LONGEST); at += 1)
    {
        const char letter = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
        const int kept = ((letter >= 'a') && (letter <= 'z')) || ((letter >= '0') && (letter <= '9'));
        if (kept)
        {
            id[length] = letter;
            length += 1ull;
        }
        else if ((length != 0ull) && (id[length - 1u] != '-'))
        {
            id[length] = '-';
            length += 1ull;
        }
    }
    while ((length != 0ull) && (id[length - 1u] == '-'))
    {
        length -= 1ull;
    }
    id[length] = '\0';
}

// The keys of a tool in the order its struct writes them, each written where it is given and not empty
static const char *const s_tool_keys[21] = {"id",      "name",    "group",   "for",    "programs", "version", "env",
                                            "notIn",   "formats", "runs",    "server", "debugger", "builds",  "format",
                                            "checker", "only",    "install", "setup",  "places",   "holds",   "needed"};

// whether a member's value is written: one that is no empty list, empty object, null or false
static int cli_written(const CliJson *json, unsigned int value)
{
    if (value == CLI_JSON_NONE)
    {
        return 0;
    }
    const CfgJsonKind kind = json->node[value].kind;
    if ((kind == CFG_JSON_ARRAY) || (kind == CFG_JSON_OBJECT))
    {
        return json->node[value].count != 0u;
    }
    return (kind != CFG_JSON_NULL) && (kind != CFG_JSON_FALSE);
}

int cli_toolchains_add(const CliJson *given, unsigned int tool, char *id, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_toolchains_file("user_toolchains.json", path))
    {
        cli_line_text(said, "orior has no folder of its own to keep toolchains in");
        return 0;
    }
    char name[CLI_NAME_LONGEST];
    char group[CLI_NAME_LONGEST];
    char uses[CLI_PATH_LONGEST];
    cli_toolchains_trimmed(cli_json_string(given, cli_json_member(given, tool, "name"), NULL), name, sizeof(name));
    cli_toolchains_trimmed(cli_json_string(given, cli_json_member(given, tool, "group"), NULL), group, sizeof(group));
    cli_toolchains_trimmed(cli_json_string(given, cli_json_member(given, tool, "for"), NULL), uses, sizeof(uses));
    cli_toolchains_trimmed(cli_json_string(given, cli_json_member(given, tool, "id"), NULL), id, CLI_NAME_LONGEST);
    if (id[0] == '\0')
    {
        cli_id_of(name, id);
    }
    if ((name[0] == '\0') || (id[0] == '\0'))
    {
        cli_line_text(said, "a toolchain needs a name");
        return 0;
    }
    if (group[0] == '\0')
    {
        cli_line_text(said, name);
        cli_line_text(said, " needs a group");
        return 0;
    }
    cli_lock_take(&g_cli_toolchains_lock);
    CliJson bundled;
    CliJson json;
    const unsigned int tools = cli_bundled(&bundled);
    // the tool made takes what the given one holds, a node and its key for each of its members, its programs, and
    // the two lists and the group made where the file has none
    unsigned int nodes_more = 0u;
    unsigned long long text_more = 0ull;
    cli_json_extent(given, tool, &nodes_more, &text_more);
    const unsigned int added =
        cli_toolchains_file_read(&json, path, nodes_more + 32u, text_more + (4ull * CLI_PATH_LONGEST) + 512ull);
    unsigned int groups = CLI_JSON_NONE;
    unsigned int added_tools = CLI_JSON_NONE;
    cli_added_lists(&json, added, &groups, &added_tools);
    const unsigned int made = cli_json_made(&json, CFG_JSON_OBJECT);
    const unsigned int programs = cli_json_made(&json, CFG_JSON_ARRAY);
    const unsigned int listed = cli_json_member(given, tool, "programs");
    for (unsigned int item = cli_json_is(given, listed, CFG_JSON_ARRAY) ? given->node[listed].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = given->node[item].next)
    {
        char program[CLI_NAME_LONGEST];
        cli_toolchains_trimmed(cli_json_string(given, item, NULL), program, sizeof(program));
        if (program[0] != '\0')
        {
            cli_json_append(&json, programs,
                            cli_json_string_made(
                                &json, program, cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL})));
        }
    }
    const char *refused = NULL;
    if (json.node[programs].count == 0u)
    {
        refused = " needs the name of at least one program to look for";
    }
    else if (cli_holds(&bundled, tools, "id", id) || cli_holds(&json, added_tools, "id", id))
    {
        refused = "";
    }
    if (refused != NULL)
    {
        cli_json_given(&json);
        cli_json_given(&bundled);
        cli_lock_give(&g_cli_toolchains_lock);
        cli_line_text(said, (refused[0] == '\0') ? "there is a toolchain " : name);
        cli_line_text(said, (refused[0] == '\0') ? id : refused);
        cli_line_text(said, (refused[0] == '\0') ? " already" : "");
        return 0;
    }
    // the tool written as its struct is, the given members past the five made here copied across
    for (unsigned int key = 0u; key < 21u; key += 1u)
    {
        const char *const named = s_tool_keys[key];
        unsigned int value = CLI_JSON_NONE;
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "id", .cap = CLI_CELLS_POOL}))
        {
            value = cli_json_string_made(&json, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "name", .cap = CLI_CELLS_POOL})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "group", .cap = CLI_CELLS_POOL})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "for", .cap = CLI_CELLS_POOL})))
        {
            const char *const text = (named[0] == 'n') ? name : (named[0] == 'g') ? group : uses;
            value =
                cli_json_string_made(&json, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "programs", .cap = CLI_CELLS_POOL}))
        {
            value = programs;
        }
        else
        {
            const unsigned int member = cli_json_member(given, tool, named);
            value = cli_written(given, member) ? cli_json_copy_from(&json, given, member) : CLI_JSON_NONE;
        }
        if ((value != CLI_JSON_NONE) || (key < 5u))
        {
            cli_json_set(&json, made, named, (value != CLI_JSON_NONE) ? value : cli_json_string_made(&json, "", 0u));
        }
    }
    CliGroupFound found = {group, 0};
    cli_groups_of(&bundled, tools, &json, added_tools, groups, cli_group_found, &found);
    if (!found.found)
    {
        cli_json_append(
            &json, groups,
            cli_json_string_made(&json, group, cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_CELLS_POOL})));
    }
    cli_json_append(&json, added_tools, made);
    const int kept = cli_toolchains_file_kept(&json, added, path, 0, said);
    cli_json_given(&json);
    cli_json_given(&bundled);
    cli_lock_give(&g_cli_toolchains_lock);
    return kept;
}

// The reader's file read with orior's own tools beside it, under the lock: 1, or 0 with why said where orior has no
// folder of its own
typedef struct
{
    char path[CLI_PATH_LONGEST];
    CliJson bundled;
    CliJson json;
    unsigned int tools;
    unsigned int added;
    unsigned int groups;
    unsigned int added_tools;
} CliAdded;

static int cli_added_read(CliAdded *read, CliLine *said)
{
    cli_json_begun(&read->json, NULL, 0u, NULL, 0ull);
    cli_json_begun(&read->bundled, NULL, 0u, NULL, 0ull);
    if (!cli_toolchains_file("user_toolchains.json", read->path))
    {
        cli_line_text(said, "orior has no folder of its own to keep toolchains in");
        return 0;
    }
    // the two lists made where the file has none, and a group put, take three nodes and their texts
    read->tools = cli_bundled(&read->bundled);
    read->added = cli_toolchains_file_read(&read->json, read->path, 3u, CLI_NAME_LONGEST + 16ull);
    cli_added_lists(&read->json, read->added, &read->groups, &read->added_tools);
    return 1;
}

// the reader's file's tree and orior's own given back, and the lock with them
static void cli_added_released(CliAdded *read)
{
    cli_json_given(&read->json);
    cli_json_given(&read->bundled);
    cli_lock_give(&g_cli_toolchains_lock);
}

// each item of a list that is `text`, or whose `key` is, taken out: how many were
static unsigned int cli_taken_out(CliJson *json, unsigned int list, const char *key, const char *text)
{
    unsigned int taken = 0u;
    unsigned int before = CLI_JSON_NONE;
    for (unsigned int item = json->node[list].first; item != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[item].next;
        const char *const held = cli_json_string(json, (key != NULL) ? cli_json_member(json, item, key) : item, NULL);
        if ((held != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = held, .other = text, .cap = CLI_CELLS_POOL})))
        {
            if (before == CLI_JSON_NONE)
            {
                json->node[list].first = next;
            }
            else
            {
                json->node[before].next = next;
            }
            json->node[list].last = (json->node[list].last == item) ? before : json->node[list].last;
            json->node[list].count -= 1u;
            taken += 1u;
        }
        else
        {
            before = item;
        }
        item = next;
    }
    return taken;
}

int cli_toolchains_add_group(const char *name, CliLine *said)
{
    static CliAdded s_read;
    char group[CLI_NAME_LONGEST];
    cli_toolchains_trimmed(name, group, sizeof(group));
    if (group[0] == '\0')
    {
        cli_line_text(said, "a group needs a name");
        return 0;
    }
    cli_lock_take(&g_cli_toolchains_lock);
    int done = cli_added_read(&s_read, said);
    CliGroupFound found = {group, 0};
    if (done)
    {
        cli_groups_of(&s_read.bundled, s_read.tools, &s_read.json, s_read.added_tools, s_read.groups, cli_group_found,
                      &found);
    }
    if (done && found.found)
    {
        cli_line_text(said, "there is a group ");
        cli_line_text(said, group);
        cli_line_text(said, " already");
        done = 0;
    }
    if (done)
    {
        cli_json_append(&s_read.json, s_read.groups,
                        cli_json_string_made(&s_read.json, group,
                                             cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_CELLS_POOL})));
        done = cli_toolchains_file_kept(&s_read.json, s_read.added, s_read.path, 0, said);
    }
    cli_added_released(&s_read);
    return done;
}

int cli_toolchains_remove(const char *id, CliLine *said)
{
    static CliAdded s_read;
    cli_lock_take(&g_cli_toolchains_lock);
    int done = cli_added_read(&s_read, said);
    if (done && (cli_taken_out(&s_read.json, s_read.added_tools, "id", id) == 0u))
    {
        const int own = cli_holds(&s_read.bundled, s_read.tools, "id", id);
        cli_line_text(said, own ? id : "there is no toolchain ");
        cli_line_text(said, own ? " comes with orior: only a toolchain you added can be taken out" : id);
        done = 0;
    }
    if (done)
    {
        done = cli_toolchains_file_kept(&s_read.json, s_read.added, s_read.path, 0, said);
    }
    cli_added_released(&s_read);
    return done && cli_toolchains_forget(id, said);
}

int cli_toolchains_remove_group(const char *name, CliLine *said)
{
    static CliAdded s_read;
    cli_lock_take(&g_cli_toolchains_lock);
    int done = cli_added_read(&s_read, said);
    if (done && (cli_holds(&s_read.bundled, s_read.tools, "group", name) ||
                 cli_holds(&s_read.json, s_read.added_tools, "group", name)))
    {
        cli_line_text(said, name);
        cli_line_text(said, " holds toolchains: take them out first, or it is one of orior's own");
        done = 0;
    }
    if (done && (cli_taken_out(&s_read.json, s_read.groups, NULL, name) == 0u))
    {
        cli_line_text(said, "there is no group ");
        cli_line_text(said, name);
        done = 0;
    }
    if (done)
    {
        done = cli_toolchains_file_kept(&s_read.json, s_read.added, s_read.path, 0, said);
    }
    cli_added_released(&s_read);
    return done;
}

int cli_toolchains_added(const char *id)
{
    static CliAdded s_read;
    char unsaid[CLI_PATH_LONGEST];
    CliLine said = {unsaid, sizeof(unsaid), 0ull};
    cli_lock_take(&g_cli_toolchains_lock);
    const int added = cli_added_read(&s_read, &said) && cli_holds(&s_read.json, s_read.added_tools, "id", id);
    cli_added_released(&s_read);
    return added;
}
