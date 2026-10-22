// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_tool.c: the tool plugins, those that come with orior and the reader's, each read from its plugin.json as
// serde reads a Tool: a plugin of kind "tool" that is not one is passed over, and of two of one id the later stands
#include "plugins.h"
#include "validate_internal.h"

#include "../text/text.h"

// the most tool plugins looked over
#define CLI_VALIDATE_TOOLS (CLI_USER_PLUGINS + 256u)

int cli_validate_holds(const CliJson *json, unsigned int array, const char *text, unsigned long long length)
{
    for (unsigned int index = 0u; cli_json_item(json, array, index) != CLI_JSON_NONE; index += 1u)
    {
        unsigned long long size = 0ull;
        const char *const one = cli_json_string(json, cli_json_item(json, array, index), &size);
        if ((one != NULL) && (size == length) &&
            (memor.cmp(&(MemoriaCfg){.src = one, .other = text, .bytes = length}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

int cli_validate_contains(const char *text, unsigned long long length, const char *part)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + size <= length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = text + at, .other = part, .bytes = size}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// the string member `name` of `object` into `value`: 1, or 0 where it is no string, or absent and `needed`; "" where
// it is absent and not needed
static int tool_string(const CliJson *json, unsigned int object, const char *name, int needed, const char **value)
{
    const unsigned int member = cli_json_member(json, object, name);
    *value = (member == CLI_JSON_NONE) ? "" : cli_json_string(json, member, NULL);
    return (member == CLI_JSON_NONE) ? !needed : (*value != NULL);
}

// the array of strings `name` of `object` into `array`: 1, or 0 where it is no array of strings, or absent and
// `needed`; CLI_JSON_NONE, an array with none, where it is absent and not needed
static int tool_strings(const CliJson *json, unsigned int object, const char *name, int needed, unsigned int *array)
{
    *array = cli_json_member(json, object, name);
    if (*array == CLI_JSON_NONE)
    {
        return !needed;
    }
    if (!cli_json_is(json, *array, CFG_JSON_ARRAY))
    {
        return 0;
    }
    for (unsigned int index = 0u; cli_json_item(json, *array, index) != CLI_JSON_NONE; index += 1u)
    {
        if (!cli_json_is(json, cli_json_item(json, *array, index), CFG_JSON_STRING))
        {
            return 0;
        }
    }
    return 1;
}

// the whole number `name` of `object`, no greater than `most`, into `value`: 1, or 0 where it is none, as serde reads
// an unsigned integer, or absent and no `fallback` is given; `*fallback` where it is absent
static int tool_count(const CliJson *json, unsigned int object, const char *name, const unsigned long long *fallback,
                      unsigned long long most, unsigned long long *value)
{
    const unsigned int member = cli_json_member(json, object, name);
    if (member == CLI_JSON_NONE)
    {
        *value = (fallback != NULL) ? *fallback : 0ull;
        return fallback != NULL;
    }
    if (!cli_json_is(json, member, CFG_JSON_NUMBER))
    {
        return 0;
    }
    const char *const text = json->text + json->node[member].text;
    const unsigned long long length = json->node[member].length;
    unsigned long long sum = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return 0;
        }
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        if (sum > (most - digit) / 10ull)
        {
            return 0;
        }
        sum = (sum * 10ull) + digit;
    }
    *value = sum;
    return length > 0ull;
}

// the barriers of a tool read: 1, or 0 where one is not a Barrier
static int tool_barriers(const CliJson *json, unsigned int array, CliTool *tool)
{
    tool->barrier_count = 0u;
    for (unsigned int index = 0u; cli_json_item(json, array, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int one = cli_json_item(json, array, index);
        if ((tool->barrier_count == CLI_VALIDATE_BARRIERS) || !cli_json_is(json, one, CFG_JSON_OBJECT))
        {
            return 0;
        }
        CliBarrier *const barrier = &tool->barriers[tool->barrier_count];
        if (!tool_string(json, one, "name", 1, &barrier->name) ||
            !tool_string(json, one, "about", 0, &barrier->about) ||
            !tool_strings(json, one, "kinds", 0, &barrier->kinds) ||
            !tool_strings(json, one, "says", 0, &barrier->says) ||
            !tool_strings(json, one, "not", 0, &barrier->unless) ||
            !tool_string(json, one, "lift", 1, &barrier->lift) ||
            !tool_count(json, one, "default", NULL, ~0ull, &barrier->start) ||
            !tool_count(json, one, "grow", NULL, ~0ull, &barrier->grow) ||
            !tool_count(json, one, "ceiling", NULL, ~0ull, &barrier->ceiling))
        {
            return 0;
        }
        tool->barrier_count += 1u;
    }
    return 1;
}

// the flags of a tool read: 1, or 0 where one is not a Flag
static int tool_flags(const CliJson *json, unsigned int array, CliTool *tool)
{
    tool->flag_count = 0u;
    for (unsigned int index = 0u; cli_json_item(json, array, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int one = cli_json_item(json, array, index);
        CliFlag *const flag = &tool->flags[tool->flag_count];
        unsigned long long severity = 0ull;
        if ((tool->flag_count == CLI_VALIDATE_FLAGS) || !cli_json_is(json, one, CFG_JSON_OBJECT) ||
            !tool_strings(json, one, "words", 1, &flag->words) ||
            !tool_count(json, one, "severity", NULL, 255ull, &severity) ||
            !tool_string(json, one, "says", 1, &flag->says))
        {
            return 0;
        }
        flag->severity = (unsigned int)severity;
        tool->flag_count += 1u;
    }
    return 1;
}

// a tool's audit read, none where it is absent or null: 1, or 0 where it is not an Audit
static int tool_audit(const CliJson *json, unsigned int audit, CliTool *tool)
{
    tool->audited = (audit != CLI_JSON_NONE) && !cli_json_is(json, audit, CFG_JSON_NULL);
    tool->declarations = CLI_JSON_NONE;
    tool->trusted = CLI_JSON_NONE;
    tool->gaps = CLI_JSON_NONE;
    tool->explains = CLI_JSON_NONE;
    tool->repeats = CLI_JSON_NONE;
    tool->print = "";
    if (!tool->audited)
    {
        return 1;
    }
    if (!cli_json_is(json, audit, CFG_JSON_OBJECT) ||
        !tool_strings(json, audit, "declarations", 1, &tool->declarations) ||
        !tool_string(json, audit, "print", 1, &tool->print) ||
        !tool_strings(json, audit, "trusted", 1, &tool->trusted) ||
        !tool_strings(json, audit, "gaps", 0, &tool->gaps) || !tool_strings(json, audit, "repeats", 0, &tool->repeats))
    {
        return 0;
    }
    tool->explains = cli_json_member(json, audit, "explains");
    if (tool->explains == CLI_JSON_NONE)
    {
        return 1;
    }
    if (!cli_json_is(json, tool->explains, CFG_JSON_OBJECT))
    {
        return 0;
    }
    for (unsigned int member = json->node[tool->explains].first; member != CLI_JSON_NONE;
         member = json->node[member].next)
    {
        if (!cli_json_is(json, member, CFG_JSON_STRING))
        {
            return 0;
        }
    }
    // an explanation is found by its key in the order a BTreeMap keeps them
    cli_json_sorted((CliJson *)json, tool->explains);
    return 1;
}

// the tool `root` says read into `tool`: 1, or 0 where it is no tool plugin, or not a Tool as serde reads one
static int tool_read(const CliJson *json, unsigned int root, CliTool *tool)
{
    const char *kind = NULL;
    if (!cli_json_is(json, root, CFG_JSON_OBJECT) || !tool_string(json, root, "kind", 1, &kind) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "tool", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    const unsigned int run = cli_json_member(json, root, "run");
    const unsigned int barriers = cli_json_member(json, root, "barriers");
    const unsigned int flags = cli_json_member(json, root, "flags");
    static const unsigned long long s_patience = 600ull;
    return tool_string(json, root, "id", 1, &tool->id) && tool_string(json, root, "name", 1, &tool->name) &&
           tool_strings(json, root, "languages", 0, &tool->languages) &&
           tool_string(json, root, "toolchain", 1, &tool->toolchain) && cli_json_is(json, run, CFG_JSON_OBJECT) &&
           tool_string(json, run, "program", 1, &tool->program) && tool_strings(json, run, "args", 1, &tool->args) &&
           tool_strings(json, run, "project", 0, &tool->project) &&
           tool_strings(json, run, "inProject", 0, &tool->in_project) &&
           tool_count(json, run, "patience", &s_patience, ~0ull, &tool->patience) &&
           ((barriers == CLI_JSON_NONE) || cli_json_is(json, barriers, CFG_JSON_ARRAY)) &&
           tool_barriers(json, barriers, tool) &&
           ((flags == CLI_JSON_NONE) || cli_json_is(json, flags, CFG_JSON_ARRAY)) && tool_flags(json, flags, tool) &&
           tool_audit(json, cli_json_member(json, root, "audit"), tool);
}

// What a look over the tool plugins finds: each tool by its id, the last plugin of an id standing for it, with its
// place among the plugins and whether it checks the language sought; and, as one is read again, the place sought and
// whether it was read
typedef struct
{
    const char *language;
    char ids[CLI_VALIDATE_TOOLS][CLI_NAME_LONGEST];
    unsigned int places[CLI_VALIDATE_TOOLS];
    int checks[CLI_VALIDATE_TOOLS];
    unsigned int count;
    unsigned int seen;
    int loading;
    unsigned int sought;
    int loaded;
} CliToolLook;

static CliToolLook s_look;
static CliLock s_look_lock = CLI_LOCK_INIT;

// a plugin looked at: a tool kept by its id, or, as one is read again, the tool at the place sought read into the room
static void tool_looked(void *context, const CliPlugin *plugin)
{
    CliToolLook *const look = (CliToolLook *)context;
    const unsigned int place = look->seen;
    look->seen += 1u;
    if (look->loading && (place != look->sought))
    {
        return;
    }
    // the plugin's tree in a cell of its own sized to its text, the one before given back, held until the room is
    CliJson *const json = &g_cli_validate->json;
    cli_json_given(json);
    char why[CLI_SAID_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_utf8_whole(plugin->text, plugin->size)
                                  ? cli_json_taken(json, plugin->text, plugin->size, 0u, 0ull, &unread)
                                  : CLI_JSON_NONE;
    CliTool *const tool = &g_cli_validate->tool;
    if ((root == CLI_JSON_NONE) || !tool_read(json, root, tool))
    {
        return;
    }
    if (look->loading)
    {
        look->loaded = 1;
        return;
    }
    unsigned int at = 0u;
    while ((at < look->count) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = look->ids[at], .other = tool->id, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if ((at == look->count) &&
        ((look->count == CLI_VALIDATE_TOOLS) ||
         (cellul.len(&(CatenaFinitaCfg){.src = tool->id, .cap = CLI_CELLS_POOL}) >= CLI_NAME_LONGEST)))
    {
        return;
    }
    if (at == look->count)
    {
        memor.cpy(&(MemoriaCfg){.dst = look->ids[at],
                                .src = tool->id,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = tool->id, .cap = CLI_CELLS_POOL}) + 1u});
        look->count += 1u;
    }
    look->places[at] = place;
    look->checks[at] = (look->language != NULL) &&
                       cli_validate_holds(json, tool->languages, look->language,
                                          cellul.len(&(CatenaFinitaCfg){.src = look->language, .cap = CLI_CELLS_POOL}));
}

// every tool plugin looked over, each id's last kept, under the look's lock, which the caller gives back
static void tool_looked_over(const char *language)
{
    cli_lock_take(&s_look_lock);
    s_look.language = language;
    s_look.count = 0u;
    s_look.seen = 0u;
    s_look.loading = 0;
    cli_plugins_each(tool_looked, &s_look);
}

int cli_validate_tool_for(const char *language, char *id)
{
    // each plugin is read into the validation's room, held for the look as a validation holds it
    if (!cli_validate_held())
    {
        return 0;
    }
    tool_looked_over(language);
    const char *least = NULL;
    for (unsigned int at = 0u; at < s_look.count; at += 1u)
    {
        if (s_look.checks[at] && ((least == NULL) || (cli_text_order(s_look.ids[at], least) < 0)))
        {
            least = s_look.ids[at];
        }
    }
    if (least != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = id,
                                .src = least,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = least, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_lock_give(&s_look_lock);
    cli_validate_released();
    return least != NULL;
}

int cli_validate_tool_read(const char *id)
{
    tool_looked_over(NULL);
    unsigned int at = 0u;
    while ((at < s_look.count) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_look.ids[at], .other = id, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    s_look.loaded = 0;
    if (at < s_look.count)
    {
        s_look.seen = 0u;
        s_look.loading = 1;
        s_look.sought = s_look.places[at];
        cli_plugins_each(tool_looked, &s_look);
    }
    const int loaded = s_look.loaded;
    cli_lock_give(&s_look_lock);
    return loaded;
}
