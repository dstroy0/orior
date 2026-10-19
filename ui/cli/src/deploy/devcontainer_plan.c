// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_plan.c: a description's JSON read, its strings, members and lists, its variables read in, and the
// plan's texts it is kept in
#include "../../../../src/cu/includes/formats/cfg_json/cfg_json.h"
#include "../text/text.h"
#include "devcontainer_internal.h"

// the most bytes a description holds and the most tokens it reads into
#define CLI_DESCRIPTION_BYTES (1u << 20u)

#define CLI_DESCRIPTION_TOKENS 65536u

static char s_raw[CLI_DESCRIPTION_BYTES];
static char s_plain[CLI_DESCRIPTION_BYTES];
static CfgJsonToken s_tokens[CLI_DESCRIPTION_TOKENS];

// 1 where a text of the description did not fit what a plan keeps
static int s_full;

int cli_plan_description_read(const char *file, CliLine *said)
{
    s_full = 0;
    const unsigned long long size = qry_member_file_read(file, (unsigned char *)s_raw, sizeof(s_raw));
    if (size == QRY_MEMBER_UNREAD)
    {
        cli_line_text(said, file);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    CliLine plain = {s_plain, sizeof(s_plain), 0ull};
    cli_plain_json(s_raw, size, &plain);
    CfgJsonParse parse = {0u, 0u, 0u, NULL};
    if (cli_line_fits(&plain) && cfg_json_parse_metadata(s_plain, plain.at, s_tokens, CLI_DESCRIPTION_TOKENS, &parse))
    {
        return 1;
    }
    cli_line_text(said, file);
    cli_line_text(said, ": ");
    if (!cli_line_fits(&plain))
    {
        cli_line_text(said, "it is longer than a description is read");
        return 0;
    }
    cli_line_text(said, (parse.reason != NULL) ? parse.reason : "it is not JSON");
    cli_line_text(said, " at line ");
    cli_line_decimal(said, parse.line, 1u);
    cli_line_text(said, ", column ");
    cli_line_decimal(said, parse.column, 1u);
    return 0;
}

int cli_plan_overflowed(void)
{
    return s_full;
}

// A string too long to decode is kept as none and the plan refused
int cli_plan_string(unsigned int value, char *out)
{
    if ((value == 0u) || (s_tokens[value].kind != CFG_JSON_STRING))
    {
        return 0;
    }
    if (!cfg_json_string(s_plain, &s_tokens[value], out, CLI_STRING_LONGEST))
    {
        s_full = 1;
        return 0;
    }
    return 1;
}

unsigned int cli_plan_member(const char *name)
{
    return cfg_json_member(s_plain, s_tokens, 0u, name);
}

unsigned int cli_plan_member_of(unsigned int object, const char *name)
{
    return (object == 0u) ? 0u : cfg_json_member(s_plain, s_tokens, object, name);
}

unsigned int cli_plan_strings(unsigned int value, unsigned int *tokens, unsigned int room)
{
    if ((value != 0u) && (s_tokens[value].kind == CFG_JSON_STRING) && (room != 0u))
    {
        tokens[0] = value;
        return 1u;
    }
    unsigned int count = 0u;
    unsigned int at = value + 1u;
    for (unsigned int each = 0u;
         (value != 0u) && (s_tokens[value].kind == CFG_JSON_ARRAY) && (each < s_tokens[value].count) && (count < room);
         each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_STRING)
        {
            tokens[count] = at;
            count += 1u;
        }
        at = s_tokens[at].next;
    }
    return count;
}

// whether two keys are written the same
static int cli_same_key(unsigned int one, unsigned int other)
{
    const size_t size = s_tokens[one].end - s_tokens[one].start;
    return ((s_tokens[other].end - s_tokens[other].start) == size) &&
           (memor.cmp(&(MemoriaCfg){
                .src = s_plain + s_tokens[one].start, .other = s_plain + s_tokens[other].start, .bytes = size}) == 0);
}

// each member of the object at token `object` once, as a JSON object holds it: where its key first stands, with the
// value of its last; the count, each key and value token into `keys` and `values`
static unsigned int cli_members(unsigned int object, unsigned int *keys, unsigned int *values, unsigned int room)
{
    unsigned int count = 0u;
    unsigned int key = object + 1u;
    for (unsigned int member = 0u;
         (object != 0u) && (s_tokens[object].kind == CFG_JSON_OBJECT) && (member < s_tokens[object].count);
         member += 1u)
    {
        unsigned int seen = 0u;
        while ((seen < count) && !cli_same_key(keys[seen], key))
        {
            seen += 1u;
        }
        if (seen < count)
        {
            values[seen] = key + 1u;
        }
        else if (count < room)
        {
            keys[count] = key;
            values[count] = key + 1u;
            count += 1u;
        }
        else
        {
            s_full = 1;
        }
        key = s_tokens[key + 1u].next;
    }
    return count;
}

// The empty text at 0 where `text` does not fit
unsigned int cli_plan_put(CliPlan *plan, const char *text)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1ull;
    if ((plan->used + size) > CLI_PLAN_TEXT)
    {
        s_full = 1;
        return 0u;
    }
    const unsigned int at = plan->used;
    memor.cpy(&(MemoriaCfg){.dst = plan->text + at, .src = text, .bytes = size});
    // the size is below CLI_PLAN_TEXT, checked above
    plan->used += (unsigned int)size;
    return at;
}

void cli_plan_listed(CliPlan *plan, CliPlanList *list, const char *text)
{
    if (list->count >= CLI_PLAN_LIST_MOST)
    {
        s_full = 1;
        return;
    }
    list->at[list->count] = cli_plan_put(plan, text);
    list->count += 1u;
}

// the last part of `path`, after its last separator
static const char *cli_last_part(const char *path)
{
    const char *last = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((last > path) && (last[-1] != '/') && (last[-1] != '\\'))
    {
        last -= 1;
    }
    return last;
}

// A variable the description does not know, and `${` with no `}` after it, stand as they are
void cli_plan_expanded(const char *text, const CliPlanPlaces *places, char *out)
{
    CliLine line = {out, CLI_STRING_LONGEST, 0ull};
    const char *rest = text;
    const char *open =
        cellul.find(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL});
    while (open != NULL)
    {
        cli_line_counted(&line, rest, (unsigned long long)(open - rest));
        const char *const close = cellul.chr(&(CatenaFinitaCfg){.src = open, .cap = CLI_CELLS_POOL, .byte = '}'});
        if (close == NULL)
        {
            rest = open;
            break;
        }
        char named[CLI_STRING_LONGEST];
        CliLine name_line = {named, sizeof(named), 0ull};
        cli_line_counted(&name_line, open + 2, (unsigned long long)(close - open - 2));
        cli_line_closed(&name_line);
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "localWorkspaceFolder", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, places->root);
        }
        else if (cellul.eq(
                     &(CatenaFinitaCfg){.src = named, .other = "localWorkspaceFolderBasename", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, cli_last_part(places->root));
        }
        else if (cellul.eq(
                     &(CatenaFinitaCfg){.src = named, .other = "containerWorkspaceFolder", .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, places->folder);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){
                     .src = named, .other = "containerWorkspaceFolderBasename", .cap = CLI_CELLS_POOL}))
        {
            const char *const slash = cli_text_last(
                places->folder, cellul.len(&(CatenaFinitaCfg){.src = places->folder, .cap = CLI_CELLS_POOL}), '/');
            cli_line_text(&line, (slash != NULL) ? (slash + 1) : places->folder);
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = named, .other = "localEnv:", .cap = CLI_CELLS_POOL}))
        {
            char *const name = named + 9;
            char *const colon = cellul.chr(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL, .byte = ':'});
            const char *const fallback = (colon != NULL) ? (colon + 1) : "";
            if (colon != NULL)
            {
                colon[0] = '\0';
            }
            if (!query_environment_said(name, &line))
            {
                cli_line_text(&line, fallback);
            }
        }
        else
        {
            cli_line_counted(&line, open, (unsigned long long)(close - open) + 1ull);
        }
        rest = close + 1;
        open = cellul.find(
            &(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL});
    }
    cli_line_text(&line, rest);
    s_full = s_full || !cli_line_fits(&line);
    cli_line_closed(&line);
}

// `path` alone where it is whole, else after `base` and the separator `base` writes
void cli_plan_joined(const char *base, const char *path, char *out)
{
    const int whole = (path[0] == '/') || (path[0] == '\\') || ((path[0] != '\0') && (path[1] == ':'));
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    if (!whole)
    {
        const char *const forward =
            cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '/');
        const char *const backward =
            cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '\\');
        cli_line_text(&line, base);
        cli_line_character(&line, ((backward != NULL) && ((forward == NULL) || (backward > forward))) ? '\\' : '/');
    }
    cli_line_text(&line, path);
    s_full = s_full || !cli_line_fits(&line);
    cli_line_closed(&line);
}

// a command the description runs, as the value at token `value` writes it: a string a shell's line, an array a
// program's words, an object each of its values in turn
static void cli_commands_of(CliPlan *plan, unsigned int value, CliPlanCommand *commands, unsigned int *count)
{
    static char s_word[CLI_STRING_LONGEST];
    const CfgJsonKind kind = s_tokens[value].kind;
    if ((kind == CFG_JSON_STRING) || (kind == CFG_JSON_ARRAY))
    {
        if (*count >= CLI_PLAN_COMMANDS_MOST)
        {
            s_full = 1;
            return;
        }
        unsigned int tokens[CLI_PLAN_LIST_MOST];
        const unsigned int words = cli_plan_strings(value, tokens, CLI_PLAN_LIST_MOST);
        const CliPlanCommand command = {kind == CFG_JSON_STRING, plan->words.count, words};
        for (unsigned int at = 0u; at < words; at += 1u)
        {
            if (cli_plan_string(tokens[at], s_word))
            {
                cli_plan_listed(plan, &plan->words, s_word);
            }
        }
        commands[*count] = command;
        *count += 1u;
    }
    else if (kind == CFG_JSON_OBJECT)
    {
        unsigned int keys[CLI_PLAN_LIST_MOST];
        unsigned int values[CLI_PLAN_LIST_MOST];
        const unsigned int members = cli_members(value, keys, values, CLI_PLAN_LIST_MOST);
        for (unsigned int at = 0u; at < members; at += 1u)
        {
            cli_commands_of(plan, values[at], commands, count);
        }
    }
}

void cli_plan_commands(CliPlan *plan, const char *key, CliPlanCommand *commands, unsigned int *count)
{
    const unsigned int value = cli_plan_member(key);
    if ((value != 0u) && (s_tokens[value].kind != CFG_JSON_NULL))
    {
        cli_commands_of(plan, value, commands, count);
    }
}

void cli_plan_pairs(CliPlan *plan, unsigned int object, const CliPlanPlaces *places, CliPlanList *list)
{
    static char s_key[CLI_STRING_LONGEST];
    static char s_value[CLI_STRING_LONGEST];
    static char s_read[CLI_STRING_LONGEST];
    unsigned int keys[CLI_PLAN_LIST_MOST];
    unsigned int values[CLI_PLAN_LIST_MOST];
    const unsigned int members = cli_members(object, keys, values, CLI_PLAN_LIST_MOST);
    for (unsigned int at = 0u; at < members; at += 1u)
    {
        if (cfg_json_string(s_plain, &s_tokens[keys[at]], s_key, sizeof(s_key)) && cli_plan_string(values[at], s_value))
        {
            cli_plan_expanded(s_value, places, s_read);
            cli_plan_listed(plan, list, s_key);
            cli_plan_listed(plan, list, s_read);
        }
    }
}

// a mount as `mounts` names it, read: a string as docker takes it, or an object of its parts joined `key=value,...`
static void cli_mount_of(CliPlan *plan, unsigned int value, const CliPlanPlaces *places)
{
    static char s_mount[CLI_STRING_LONGEST];
    static char s_read[CLI_STRING_LONGEST];
    static char s_part[CLI_STRING_LONGEST];
    static char s_key[CLI_STRING_LONGEST];
    if (cli_plan_string(value, s_mount))
    {
        cli_plan_expanded(s_mount, places, s_read);
        cli_plan_listed(plan, &plan->mounts, s_read);
        return;
    }
    if (s_tokens[value].kind != CFG_JSON_OBJECT)
    {
        return;
    }
    CliLine line = {s_mount, sizeof(s_mount), 0ull};
    unsigned int keys[CLI_PLAN_LIST_MOST];
    unsigned int values[CLI_PLAN_LIST_MOST];
    const unsigned int members = cli_members(value, keys, values, CLI_PLAN_LIST_MOST);
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < members; at += 1u)
    {
        if (!cli_plan_string(values[at], s_part))
        {
            continue;
        }
        if (written != 0u)
        {
            cli_line_character(&line, ',');
        }
        s_full = s_full || !cfg_json_string(s_plain, &s_tokens[keys[at]], s_key, sizeof(s_key));
        cli_line_text(&line, s_key);
        cli_line_character(&line, '=');
        cli_line_text(&line, s_part);
        written += 1u;
    }
    s_full = s_full || !cli_line_fits(&line);
    cli_line_closed(&line);
    cli_plan_expanded(s_mount, places, s_read);
    cli_plan_listed(plan, &plan->mounts, s_read);
}

void cli_plan_mounts(CliPlan *plan, const CliPlanPlaces *places)
{
    const unsigned int mounts = cli_plan_member("mounts");
    unsigned int at = mounts + 1u;
    for (unsigned int each = 0u;
         (mounts != 0u) && (s_tokens[mounts].kind == CFG_JSON_ARRAY) && (each < s_tokens[mounts].count); each += 1u)
    {
        cli_mount_of(plan, at, places);
        at = s_tokens[at].next;
    }
}
