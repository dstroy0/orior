// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_start.c: a server found and started: the toolchain whose `server` serves a language; the server of a tree
// started where it is not running, at the tree's top, every server stopped where another tree is served; clangd started
// again, given back its files, where the build's database changed; one that ended started again and given back its
// files, unless it ended RESTARTS times of late, when it is left stopped and why is said; and one that cannot be found
// or does not start not tried again until the tree changes. A server that checks its projects as a whole is given each
// project the tree holds; the options of a tree whose own code is held back, or whose code runs, are added to its own
#include "../builds/compile_db.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

#include "../text/text.h"

CliServersState g_cli_servers = {CLI_LOCK_INIT};

// the files a server that ended or is started again held, to be given back to the one started after it
static CliKeyed s_reopen;

// the manifest read, under its own lock

// a string member of a manifest object copied into `out`, which holds `room`
static void start_string(const CliJson *json, unsigned int object, const char *name, char *out, unsigned long long room)
{
    unsigned long long size = 0ull;
    const char *const text = cli_json_string(json, cli_json_member(json, object, name), &size);
    const unsigned long long kept = ((text != NULL) && (size < room)) ? size : 0ull;
    memor.cpy(&(MemoriaCfg){.dst = out, .src = (text != NULL) ? text : "", .bytes = (size_t)kept});
    out[kept] = '\0';
}

// a JSON member of a manifest object written into `out`, which holds `room`, empty where it has none
static void start_json(const CliJson *json, unsigned int object, const char *name, char *out, unsigned long long room)
{
    const unsigned int member = cli_json_member(json, object, name);
    CliLine line = {out, room, 0ull};
    if (member != CLI_JSON_NONE)
    {
        cli_json_write(json, member, &line, 0u, 0);
    }
    if (!cli_line_fits(&line))
    {
        line.at = 0ull;
    }
    (void)cli_line_closed(&line);
}

// a server's spec read from the manifest's tool at `tool`
static void start_spec(const CliJson *json, unsigned int tool, unsigned int server, CliServerSpec *spec)
{
    memor.set(&(MemoriaCfg){.dst = spec, .val = 0, .bytes = sizeof(*spec)});
    start_string(json, tool, "id", spec->tool, sizeof(spec->tool));
    start_string(json, tool, "name", spec->name, sizeof(spec->name));
    start_string(json, server, "program", spec->program, sizeof(spec->program));
    const unsigned int args = cli_json_member(json, server, "args");
    for (unsigned int at = cli_json_is(json, args, CFG_JSON_ARRAY) ? json->node[args].first : CLI_JSON_NONE;
         (at != CLI_JSON_NONE) && (spec->args < CLI_SERVERS_ARGS); at = json->node[at].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, at, &size);
        if ((text != NULL) && (size < sizeof(spec->arg[0])))
        {
            memor.cpy(&(MemoriaCfg){.dst = spec->arg[spec->args], .src = text, .bytes = (size_t)size});
            spec->arg[spec->args][size] = '\0';
            spec->args += 1u;
        }
    }
    const unsigned int languages = cli_json_member(json, server, "languages");
    for (unsigned int at = cli_json_is(json, languages, CFG_JSON_ARRAY) ? json->node[languages].first : CLI_JSON_NONE;
         (at != CLI_JSON_NONE) && (spec->languages < CLI_SERVERS_LANGUAGES); at = json->node[at].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, at, &size);
        if ((text != NULL) && (size < CLI_NAME_LONGEST))
        {
            memor.cpy(&(MemoriaCfg){.dst = spec->language[spec->languages], .src = text, .bytes = (size_t)size});
            spec->language[spec->languages][size] = '\0';
            spec->languages += 1u;
        }
    }
    const unsigned int ids = cli_json_member(json, server, "ids");
    for (unsigned int at = cli_json_is(json, ids, CFG_JSON_OBJECT) ? json->node[ids].first : CLI_JSON_NONE;
         (at != CLI_JSON_NONE) && (spec->ids < CLI_SERVERS_LANGUAGES); at = json->node[at].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, at, &size);
        const char *const key = json->text + json->node[at].key;
        if ((text != NULL) && (size < CLI_NAME_LONGEST) &&
            (cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) < sizeof(spec->id_extension[0])))
        {
            memor.cpy(&(MemoriaCfg){.dst = spec->id_extension[spec->ids],
                                    .src = key,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){.dst = spec->id_name[spec->ids], .src = text, .bytes = (size_t)size});
            spec->id_name[spec->ids][size] = '\0';
            spec->ids += 1u;
        }
    }
    start_string(json, server, "setup", spec->setup, sizeof(spec->setup));
    start_string(json, server, "projects", spec->projects, sizeof(spec->projects));
    start_string(json, server, "checks", spec->checks, sizeof(spec->checks));
    start_json(json, server, "held", spec->held, sizeof(spec->held));
    start_json(json, server, "trusted", spec->trusted, sizeof(spec->trusted));
}

int cli_servers_spec_for(const char *language, CliServerSpec *spec)
{
    CliJson manifest_json;
    const unsigned int manifest = cli_toolchains_manifest(&manifest_json, 0u, 0ull);
    int found = 0;
    for (unsigned int tool = (manifest != CLI_JSON_NONE) ? manifest_json.node[manifest].first : CLI_JSON_NONE;
         !found && (tool != CLI_JSON_NONE); tool = manifest_json.node[tool].next)
    {
        const unsigned int server = cli_json_member(&manifest_json, tool, "server");
        const unsigned int languages = cli_json_member(&manifest_json, server, "languages");
        for (unsigned int at = cli_json_is(&manifest_json, languages, CFG_JSON_ARRAY)
                                   ? manifest_json.node[languages].first
                                   : CLI_JSON_NONE;
             !found && (at != CLI_JSON_NONE); at = manifest_json.node[at].next)
        {
            unsigned long long size = 0ull;
            const char *const text = cli_json_string(&manifest_json, at, &size);
            found = (text != NULL) &&
                    (cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_CELLS_POOL}) == size) &&
                    (memor.cmp(&(MemoriaCfg){.src = text, .other = language, .bytes = (size_t)size}) == 0);
        }
        if (found)
        {
            start_spec(&manifest_json, tool, server, spec);
        }
    }
    cli_json_given(&manifest_json);
    return found;
}

void cli_servers_id_of(const CliServerSpec *spec, const char *path, const char *language, char *id)
{
    const char *const dot =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '.');
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    const char *const back =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '\\');
    const int named = (dot != NULL) && ((slash == NULL) || (dot > slash)) && ((back == NULL) || (dot > back));
    char extension[32] = {0};
    for (unsigned int at = 0u; named && (dot[1 + at] != '\0') && (at + 1u < sizeof(extension)); at += 1u)
    {
        const char one = dot[1 + at];
        extension[at] = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
    }
    CliLine line = {id, CLI_NAME_LONGEST, 0ull};
    for (unsigned int at = 0u; at < spec->ids; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = spec->id_extension[at], .other = extension, .cap = CLI_CELLS_POOL}))
        {
            cli_line_text(&line, spec->id_name[at]);
            (void)cli_line_closed(&line);
            return;
        }
    }
    cli_line_text(&line, language);
    (void)cli_line_closed(&line);
}

CliLsp *cli_servers_holding(const char *path)
{
    cli_lock_take(&g_cli_servers.lock);
    CliLsp *found = NULL;
    for (unsigned int at = 0u; (found == NULL) && (at < CLI_SERVERS_MOST); at += 1u)
    {
        CliServerRunning *const one = &g_cli_servers.running[at];
        if (one->used && cli_lsp_has(one->lsp, path))
        {
            found = one->lsp;
            cli_lsp_held(found);
        }
    }
    cli_lock_give(&g_cli_servers.lock);
    return found;
}

// a file a server held kept to be given back to the one started after it
static void start_reopen_kept(void *context, const char *path, const char *text, unsigned long long length)
{
    (void)context;
    (void)cli_keyed_put(&s_reopen, path, "", text, length);
}

// the running server of `tool`: its place, CLI_SERVERS_MOST where none runs
static unsigned int start_running(const char *tool)
{
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        if (g_cli_servers.running[at].used &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = g_cli_servers.running[at].spec.tool, .other = tool, .cap = CLI_CELLS_POOL})))
        {
            return at;
        }
    }
    return CLI_SERVERS_MOST;
}

// why the toolchain `tool` failed, NULL where it did not
static const char *start_failed(const char *tool)
{
    for (unsigned int at = 0u; at < g_cli_servers.failed; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_servers.failed_tool[at], .other = tool, .cap = CLI_CELLS_POOL}))
        {
            return g_cli_servers.failed_said[at];
        }
    }
    return NULL;
}

// why the toolchain `tool` failed kept
static void start_failure(const char *tool, const char *said)
{
    unsigned int at = 0u;
    while ((at < g_cli_servers.failed) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_servers.failed_tool[at], .other = tool, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if (at == CLI_SERVERS_FAILED)
    {
        return;
    }
    g_cli_servers.failed += (at == g_cli_servers.failed) ? 1u : 0u;
    CliLine line = {g_cli_servers.failed_tool[at], CLI_NAME_LONGEST, 0ull};
    cli_line_text(&line, tool);
    (void)cli_line_closed(&line);
    CliLine why = {g_cli_servers.failed_said[at], sizeof(g_cli_servers.failed_said[at]), 0ull};
    cli_line_text(&why, said);
    (void)cli_line_closed(&why);
}

// whether the toolchain `tool`'s server, which ended, may be started again, its end counted
static int start_again(const char *tool)
{
    unsigned int at = 0u;
    while (
        (at < g_cli_servers.restarted) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_servers.restart_tool[at], .other = tool, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if (at == CLI_SERVERS_FAILED)
    {
        return 1;
    }
    if (at == g_cli_servers.restarted)
    {
        CliLine line = {g_cli_servers.restart_tool[at], CLI_NAME_LONGEST, 0ull};
        cli_line_text(&line, tool);
        (void)cli_line_closed(&line);
        cli_restarts_made(&g_cli_servers.restarts[at], CLI_SERVERS_RESTARTS, CLI_SERVERS_RESTARTS_WITHIN);
        g_cli_servers.restarted += 1u;
    }
    return cli_restarts_died(&g_cli_servers.restarts[at], cli_clock_milliseconds());
}

CliLsp *cli_servers_server(const char *root, const char *language, const CliServersEmit *emit, CliServerSpec *spec,
                           CliLine *said)
{
    if (!cli_servers_spec_for(language, spec))
    {
        return NULL;
    }
    cli_lock_take(&g_cli_servers.lock);
    if (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_servers.root, .other = root, .cap = CLI_CELLS_POOL}))
    {
        cli_lock_give(&g_cli_servers.lock);
        cli_servers_stop_all();
        cli_lock_take(&g_cli_servers.lock);
        CliLine line = {g_cli_servers.root, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, root);
        (void)cli_line_closed(&line);
    }
    cli_keyed_clear(&s_reopen);
    cli_keyed_begun(&s_reopen, "files to open again", 256u);
    const unsigned int at = start_running(spec->tool);
    static CliDatabase s_database;
    if ((at < CLI_SERVERS_MOST) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = spec->program, .other = "clangd", .cap = CLI_CELLS_POOL})) &&
        cli_lsp_alive(g_cli_servers.running[at].lsp) && cli_compile_db_refresh(root, &s_database))
    {
        CliLsp *const old = g_cli_servers.running[at].lsp;
        g_cli_servers.running[at].used = 0;
        cli_lsp_held_each(old, start_reopen_kept, NULL);
        cli_servers_stop_later(old);
    }
    else if (at < CLI_SERVERS_MOST)
    {
        CliLsp *const server = g_cli_servers.running[at].lsp;
        if (cli_lsp_alive(server))
        {
            cli_lsp_held(server);
            cli_lock_give(&g_cli_servers.lock);
            return server;
        }
        g_cli_servers.running[at].used = 0;
        if (!start_again(spec->tool))
        {
            char why[CLI_LSP_COMPLAINT];
            CliLine complaint = {why, sizeof(why), 0ull};
            cli_lsp_complaint(server, &complaint);
            cli_line_text(said, spec->program);
            cli_line_text(said, " ended ");
            cli_line_decimal(said, CLI_SERVERS_RESTARTS + 1u, 1u);
            cli_line_text(said, " times within ");
            cli_line_decimal(said, CLI_SERVERS_RESTARTS_WITHIN / 1000u, 1u);
            cli_line_text(said, " s and is left stopped");
            if (complaint.at > 0u)
            {
                cli_line_text(said, ": ");
                cli_line_counted(said, why, complaint.at);
            }
            (void)cli_line_closed(said);
            start_failure(spec->tool, said->out);
            cli_lsp_given(server);
            cli_lock_give(&g_cli_servers.lock);
            return NULL;
        }
        cli_lsp_held_each(server, start_reopen_kept, NULL);
        cli_lsp_given(server);
    }
    const char *const failed = start_failed(spec->tool);
    if (failed != NULL)
    {
        cli_line_text(said, failed);
        cli_lock_give(&g_cli_servers.lock);
        return NULL;
    }
    CliLsp *const server = cli_servers_launch(root, spec, emit, said);
    if (server == NULL)
    {
        (void)cli_line_closed(said);
        start_failure(spec->tool, said->out);
        cli_lock_give(&g_cli_servers.lock);
        return NULL;
    }
    for (unsigned int one = 0u; one < s_reopen.entries; one += 1u)
    {
        if (!cli_keyed_entry(&s_reopen, one)->used)
        {
            continue;
        }
        const char *const path = cli_keyed_entry(&s_reopen, one)->cell;
        const char *beside = NULL;
        unsigned long long size = 0ull;
        const char *const text = cli_keyed_get(&s_reopen, path, &beside, &size);
        char id[CLI_NAME_LONGEST];
        cli_servers_id_of(spec, path, language, id);
        char ignored[256];
        CliLine quiet = {ignored, sizeof(ignored), 0ull};
        (void)cli_lsp_opened(server, path, id, text, size, &quiet);
    }
    for (unsigned int slot = 0u; slot < CLI_SERVERS_MOST; slot += 1u)
    {
        if (!g_cli_servers.running[slot].used)
        {
            g_cli_servers.running[slot].used = 1;
            g_cli_servers.running[slot].lsp = server;
            g_cli_servers.running[slot].spec = *spec;
            break;
        }
    }
    cli_lsp_held(server);
    cli_lock_give(&g_cli_servers.lock);
    return server;
}
