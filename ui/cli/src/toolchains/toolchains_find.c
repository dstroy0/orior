// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains_find.c: each tool found, every tool written as the window reads it, a tool's version asked, and its
// install line and page
#include "../machine/report.h"
#include "../text/json_tree.h"
#include "toolchains_internal.h"

// how long a program may take to say its version before it is stopped, in milliseconds
#define CLI_PATIENCE 6000ull

// the PATH's folders for one call, and the folders a tool's places name, under the lock
char (*g_cli_toolchains_path)[CLI_PATH_LONGEST];
static char s_places[64][CLI_PATH_LONGEST];
CliLock g_cli_toolchains_find_lock = CLI_LOCK_INIT;

unsigned int cli_toolchains_find_path(void)
{
    unsigned int count = 0u;
    cli_cell_given(g_cli_toolchains_path);
    g_cli_toolchains_path = cli_toolchains_path_taken(&count);
    return count;
}

void cli_toolchains_find_released(CliJson *json)
{
    if (json != NULL)
    {
        cli_json_given(json);
    }
    cli_cell_given(g_cli_toolchains_path);
    g_cli_toolchains_path = NULL;
    cli_lock_give(&g_cli_toolchains_find_lock);
}

// whether the folder is passed over for the tool: a part of it is a name `notIn` lists, case aside
static int cli_passed_over(const CliJson *json, unsigned int tool, const char *dir)
{
    const unsigned int not_in = cli_json_member(json, tool, "notIn");
    for (unsigned int item = cli_json_is(json, not_in, CFG_JSON_ARRAY) ? json->node[not_in].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const name = cli_json_string(json, item, NULL);
        const unsigned long long length =
            (name != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) : 0ull;
        const char *part = dir;
        while ((name != NULL) && (part[0] != '\0'))
        {
            const char *end = part;
            while ((end[0] != '\0') && (end[0] != '/') && (end[0] != '\\'))
            {
                end += 1;
            }
            int same = (unsigned long long)(end - part) == length;
            for (unsigned long long at = 0ull; same && (at < length); at += 1ull)
            {
                const char left = ((part[at] >= 'A') && (part[at] <= 'Z')) ? (char)(part[at] + 32) : part[at];
                const char right = ((name[at] >= 'A') && (name[at] <= 'Z')) ? (char)(name[at] + 32) : name[at];
                same = left == right;
            }
            if (same)
            {
                return 1;
            }
            part = (end[0] != '\0') ? (end + 1) : end;
        }
    }
    return 0;
}

// whether the folder holds what the tool's `holds` names, where it names one
static int cli_holds(const CliJson *json, unsigned int tool, const char *dir)
{
    const char *const inner = cli_json_string(json, cli_json_member(json, tool, "holds"), NULL);
    if (inner == NULL)
    {
        return 1;
    }
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, dir);
    cli_line_character(&line, '/');
    cli_line_text(&line, inner);
    return (cli_line_closed(&line) != 0ull) && query_is_folder(path);
}

// the program found set with its state, and its folder
static void cli_set(CliFound *found, const char *state, const char *program)
{
    found->state = state;
    found->has_program = 1;
    memor.cpy(&(MemoriaCfg){.dst = found->program,
                            .src = program,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = found->folder,
                            .src = program,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}) + 1u});
    char *cut = found->folder + cellul.len(&(CatenaFinitaCfg){.src = found->folder, .cap = CLI_CELLS_POOL});
    while ((cut > found->folder) && (cut[-1] != '/') && (cut[-1] != '\\'))
    {
        cut -= 1;
    }
    cut -= (cut > found->folder) ? 1 : 0;
    cut[0] = '\0';
}

void cli_toolchains_find(const CliJson *json, unsigned int tool, const char (*path)[CLI_PATH_LONGEST],
                         unsigned int count, CliFound *found)
{
    found->state = "missing";
    found->has_program = 0;
    found->program[0] = '\0';
    found->folder[0] = '\0';
    char program[CLI_PATH_LONGEST];
    const char *const env = cli_json_string(json, cli_json_member(json, tool, "env"), NULL);
    if ((env != NULL) && query_environment_read(env, program, sizeof(program)))
    {
        cli_set(found, "env", program);
        return;
    }
    const char *const id = cli_json_string(json, cli_json_member(json, tool, "id"), NULL);
    CliTreeEnvironment environment;
    if ((id != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = "python", .cap = CLI_CELLS_POOL})) &&
        cli_toolchains_environment(&environment) && environment.has_python)
    {
        cli_set(found, "tree", environment.python);
        return;
    }
    char chosen[CLI_PATH_LONGEST];
    if ((id != NULL) && cli_toolchains_chosen(id, chosen) &&
        cli_toolchains_tool_program_in(json, tool, chosen, program))
    {
        cli_set(found, "chosen", program);
        return;
    }
    const unsigned int programs = cli_json_member(json, tool, "programs");
    for (unsigned int item = cli_json_is(json, programs, CFG_JSON_ARRAY) ? json->node[programs].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const name = cli_json_string(json, item, NULL);
        for (unsigned int at = 0u; (name != NULL) && (at < count); at += 1u)
        {
            if (!cli_passed_over(json, tool, path[at]) && cli_holds(json, tool, path[at]) &&
                cli_toolchains_program_in(path[at], &name, 1u, program))
            {
                cli_set(found, "path", program);
                return;
            }
        }
    }
    const unsigned int places =
        cli_toolchains_folders_for(json, cli_json_member(json, tool, "places"), s_places, 0u, 64u);
    for (unsigned int at = 0u; at < places; at += 1u)
    {
        if (!cli_passed_over(json, tool, s_places[at]) && cli_holds(json, tool, s_places[at]) &&
            cli_toolchains_tool_program_in(json, tool, s_places[at], program))
        {
            cli_set(found, "found", program);
            return;
        }
    }
}

// a text written as a JSON string, or null where it is none
static void cli_quoted_or_null(CliLine *out, const char *text)
{
    if (text == NULL)
    {
        cli_line_text(out, "null");
        return;
    }
    cli_json_quoted(out, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// What tool `tool` says its version is into `out`: the first line it writes with a dotted number, else its first
// line, at most 120 letters. 1, or 0 with why said
static int cli_version_of(const CliJson *json, unsigned int tool, const CliFound *found, CliLine *out, CliLine *said)
{
    static char s_said[1u << 16u];
    const char *const name = cli_json_string(json, cli_json_member(json, tool, "name"), NULL);
    const unsigned int version = cli_json_member(json, tool, "version");
    if (!cli_json_is(json, version, CFG_JSON_ARRAY))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not asked its version: running it opens a window or takes long");
        return 0;
    }
    if (!found->has_program)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is not found");
        return 0;
    }
    const char *words[32];
    unsigned int count = 0u;
    words[count] = found->program;
    count += 1u;
    for (unsigned int item = json->node[version].first; (item != CLI_JSON_NONE) && (count < 31u);
         item = json->node[item].next)
    {
        const char *const word = cli_json_string(json, item, NULL);
        if (word != NULL)
        {
            words[count] = word;
            count += 1u;
        }
    }
    words[count] = NULL;
    CliSpawned spawned;
    if (!cli_spawn(&spawned, words, NULL, NULL, 0u, 1, said))
    {
        return 0;
    }
    // the output read first, then the errors, each until it ends or the patience runs out
    const unsigned long long began = cli_clock_milliseconds();
    long long code = 0ll;
    while (!cli_spawn_poll(&spawned, &code))
    {
        if ((cli_clock_milliseconds() - began) >= CLI_PATIENCE)
        {
            cli_spawn_tree_end(spawned.id);
            cli_spawn_wait(&spawned);
            cli_spawn_closed(&spawned);
            cli_line_text(said, name);
            cli_line_text(said, " did not say its version in 6 s");
            return 0;
        }
        query_paused(40u);
    }
    unsigned long long read = 0ull;
    for (unsigned int stream = 0u; stream < 2u; stream += 1u)
    {
        const unsigned long long pipe = (stream == 0u) ? spawned.out : spawned.err;
        for (;;)
        {
            const unsigned long long got = cli_spawn_read(pipe, s_said + read, sizeof(s_said) - read - 2ull);
            if (got == 0ull)
            {
                break;
            }
            read += got;
        }
        s_said[read] = '\n';
        read += (read + 1ull < sizeof(s_said)) ? 1ull : 0ull;
    }
    cli_spawn_closed(&spawned);
    s_said[read] = '\0';
    const char *first = NULL;
    const char *numbered = NULL;
    unsigned long long first_length = 0ull;
    unsigned long long numbered_length = 0ull;
    const char *at = s_said;
    while ((at[0] != '\0') && (numbered == NULL))
    {
        const char *end = at;
        while ((end[0] != '\0') && (end[0] != '\n'))
        {
            end += 1;
        }
        const char *from = at;
        const char *to = end;
        while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
        {
            from += 1;
        }
        while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
        {
            to -= 1;
        }
        if ((to > from) && (first == NULL))
        {
            first = from;
            first_length = (unsigned long long)(to - from);
        }
        for (const char *three = from; (numbered == NULL) && ((three + 3) <= to); three += 1)
        {
            if ((three[0] >= '0') && (three[0] <= '9') && (three[1] == '.') && (three[2] >= '0') && (three[2] <= '9'))
            {
                numbered = from;
                numbered_length = (unsigned long long)(to - from);
            }
        }
        at = (end[0] != '\0') ? (end + 1) : end;
    }
    const char *const line = (numbered != NULL) ? numbered : first;
    const unsigned long long length = (numbered != NULL) ? numbered_length : first_length;
    if (line == NULL)
    {
        cli_line_text(said, name);
        cli_line_text(said, " said nothing");
        return 0;
    }
    // at most 120 letters, a letter whole
    unsigned long long cut = 0ull;
    unsigned int letters = 0u;
    while ((cut < length) && (letters < 120u))
    {
        cut += 1ull;
        while ((cut < length) && ((((unsigned char)line[cut]) & 0xc0u) == 0x80u))
        {
            cut += 1ull;
        }
        letters += 1u;
    }
    cli_line_counted(out, line, cut);
    return 1;
}

int cli_toolchains_version(const char *id, CliLine *out, CliLine *said)
{
    CliJson json;
    cli_lock_take(&g_cli_toolchains_find_lock);
    const unsigned int tool = cli_toolchains_tool(&json, cli_toolchains_manifest(&json, 0u, 0ull), id, said);
    int done = 0;
    if (tool != CLI_JSON_NONE)
    {
        CliFound found;
        const unsigned int count = cli_toolchains_find_path();
        cli_toolchains_find(&json, tool, (const char (*)[CLI_PATH_LONGEST])g_cli_toolchains_path, count, &found);
        done = cli_version_of(&json, tool, &found, out, said);
    }
    cli_toolchains_find_released(&json);
    return done;
}

// a member of a tool written: its text, or its list of texts, or null
static void cli_member_written(const CliJson *json, unsigned int tool, const char *key, CliLine *out)
{
    const unsigned int value = cli_json_member(json, tool, key);
    if (cli_json_is(json, value, CFG_JSON_ARRAY))
    {
        cli_json_write(json, value, out, 0u, 0);
        return;
    }
    cli_quoted_or_null(out, cli_json_string(json, value, NULL));
}

void cli_toolchains_check(int versions, CliLine *out)
{
    CliJson json;
    cli_lock_take(&g_cli_toolchains_find_lock);
    const unsigned int manifest = cli_toolchains_manifest(&json, 0u, 0ull);
    const unsigned int count = cli_toolchains_find_path();
    cli_line_character(out, '[');
    int first = 1;
    for (unsigned int tool = cli_json_is(&json, manifest, CFG_JSON_ARRAY) ? json.node[manifest].first : CLI_JSON_NONE;
         tool != CLI_JSON_NONE; tool = json.node[tool].next)
    {
        CliFound found;
        cli_toolchains_find(&json, tool, (const char (*)[CLI_PATH_LONGEST])g_cli_toolchains_path, count, &found);
        const char *const id = cli_json_string(&json, cli_json_member(&json, tool, "id"), NULL);
        char chosen[CLI_PATH_LONGEST];
        const int has_chosen = (id != NULL) && cli_toolchains_chosen(id, chosen);
        const unsigned int install = cli_toolchains_for_system(&json, cli_json_member(&json, tool, "install"));
        cli_line_text(out, first ? "{" : ",{");
        first = 0;
        static const char *const s_keys[4] = {"id", "name", "group", "for"};
        for (unsigned int key = 0u; key < 4u; key += 1u)
        {
            cli_line_text(out, (key != 0u) ? ",\"" : "\"");
            cli_line_text(out, (key == 3u) ? "for" : s_keys[key]);
            cli_line_text(out, "\":");
            const char *const text = cli_json_string(&json, cli_json_member(&json, tool, s_keys[key]), NULL);
            cli_quoted_or_null(out, (text != NULL) ? text : "");
        }
        cli_line_text(out, ",\"programs\":");
        cli_member_written(&json, tool, "programs", out);
        cli_line_text(out, ",\"formats\":");
        const unsigned int formats = cli_json_member(&json, tool, "formats");
        if (cli_json_is(&json, formats, CFG_JSON_ARRAY))
        {
            cli_json_write(&json, formats, out, 0u, 0);
        }
        else
        {
            cli_line_text(out, "[]");
        }
        cli_line_text(out, ",\"state\":");
        cli_quoted_or_null(out, found.state);
        cli_line_text(out, ",\"program\":");
        cli_quoted_or_null(out, found.has_program ? found.program : NULL);
        cli_line_text(out, ",\"folder\":");
        cli_quoted_or_null(out, found.has_program ? found.folder : NULL);
        cli_line_text(out, ",\"chosen\":");
        cli_quoted_or_null(out, has_chosen ? chosen : NULL);
        cli_line_text(out, ",\"install\":");
        cli_quoted_or_null(out, cli_json_string(&json, install, NULL));
        cli_line_text(out, (cli_toolchains_for_system(&json, cli_json_member(&json, tool, "setup")) != CLI_JSON_NONE)
                               ? ",\"setup\":true"
                               : ",\"setup\":false");
        const int versioned = cli_json_is(&json, cli_json_member(&json, tool, "version"), CFG_JSON_ARRAY);
        cli_line_text(out, versioned ? ",\"versioned\":true" : ",\"versioned\":false");
        cli_line_text(out, cli_json_is(&json, cli_json_member(&json, tool, "needed"), CFG_JSON_TRUE)
                               ? ",\"needed\":true"
                               : ",\"needed\":false");
        cli_line_text(out, ((id != NULL) && cli_toolchains_added(id)) ? ",\"added\":true" : ",\"added\":false");
        if (versions && versioned && found.has_program)
        {
            char line[512];
            char unsaid[CLI_PATH_LONGEST];
            CliLine version_line = {line, sizeof(line), 0ull};
            CliLine said = {unsaid, sizeof(unsaid), 0ull};
            if (cli_version_of(&json, tool, &found, &version_line, &said))
            {
                cli_line_closed(&version_line);
                cli_line_text(out, ",\"version\":");
                cli_quoted_or_null(out, line);
            }
        }
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    cli_toolchains_find_released(&json);
}

int cli_toolchains_setup_line(const char *id, CliLine *out, CliLine *said)
{
    CliJson json;
    cli_lock_take(&g_cli_toolchains_find_lock);
    const unsigned int tool = cli_toolchains_tool(&json, cli_toolchains_manifest(&json, 0u, 0ull), id, said);
    const char *const line =
        (tool != CLI_JSON_NONE)
            ? cli_json_string(&json, cli_toolchains_for_system(&json, cli_json_member(&json, tool, "setup")), NULL)
            : NULL;
    if ((tool != CLI_JSON_NONE) && (line == NULL))
    {
        const char *const name = cli_json_string(&json, cli_json_member(&json, tool, "name"), NULL);
        cli_line_text(said, "orior has no way to install ");
        cli_line_text(said, (name != NULL) ? name : id);
        cli_line_text(said, " on ");
        cli_line_text(said, cli_toolchains_system());
        cli_line_text(said, ": its install page has the steps");
    }
    if (line != NULL)
    {
        cli_line_text(out, line);
    }
    cli_toolchains_find_released(&json);
    return line != NULL;
}

int cli_toolchains_open_install(const char *id, CliLine *out, CliLine *said)
{
    CliJson json;
    char url[CLI_PATH_LONGEST];
    cli_lock_take(&g_cli_toolchains_find_lock);
    const unsigned int tool = cli_toolchains_tool(&json, cli_toolchains_manifest(&json, 0u, 0ull), id, said);
    const char *const page =
        (tool != CLI_JSON_NONE)
            ? cli_json_string(&json, cli_toolchains_for_system(&json, cli_json_member(&json, tool, "install")), NULL)
            : NULL;
    if ((tool != CLI_JSON_NONE) && (page == NULL))
    {
        const char *const name = cli_json_string(&json, cli_json_member(&json, tool, "name"), NULL);
        cli_line_text(said, (name != NULL) ? name : id);
        cli_line_text(said, " has no install page for ");
        cli_line_text(said, cli_toolchains_system());
    }
    memor.cpy(&(MemoriaCfg){
        .dst = url,
        .src = (page != NULL) ? page : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (page != NULL) ? page : "", .cap = CLI_CELLS_POOL}) + 1u});
    cli_toolchains_find_released(&json);
    if (page == NULL)
    {
        return 0;
    }
    if (!cellul.starts(&(CatenaFinitaCfg){.src = url, .other = "https://", .cap = CLI_CELLS_POOL}))
    {
        cli_line_text(said, url);
        cli_line_text(said, " is not an https page");
        return 0;
    }
    if (!cli_report_open_url(url, said))
    {
        return 0;
    }
    cli_line_text(out, url);
    return 1;
}
