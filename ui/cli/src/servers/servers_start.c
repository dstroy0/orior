// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_start.c: a server found and started: the toolchain whose `server` serves a language; the server of a tree
// started where it is not running, at the tree's top, every server stopped where another tree is served; clangd started
// again, given back its files, where the build's database changed; one that ended started again and given back its
// files, unless it ended RESTARTS times of late, when it is left stopped and why is reported; and one that cannot be
// found or does not start not tried again until the tree changes. A server that checks its projects as a whole is given
// each project the tree contains; the options of a tree whose own code is disabled, or whose code runs, are added to
// its own
#include "../builds/compile_db.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

#include "../text/text.h"

CliServersState g_cli_servers = {CLI_LOCK_INIT};

// the files a server that ended or is started again had open, to be given back to the one started after it
static CliKeyed s_reopen;

// the manifest read, under its own lock

// a string member of a manifest object copied into `out`, sized for `room`
static void start_string(const CliJson *json, unsigned int object, const char *name, char *out, unsigned long long room)
{
    unsigned long long size = 0ull;
    const char *const text = cli_json_string(json, cli_json_member(json, object, name), &size);
    const unsigned long long copied_len = ((text != NULL) && (size < room)) ? size : 0ull;
    memor.cpy(&(MemoriaCfg){.dst = out, .src = (text != NULL) ? text : "", .bytes = (size_t)copied_len});
    out[copied_len] = '\0';
}

// a JSON member of a manifest object written into `out`, sized for `room`, empty where it has none
static void start_json(const CliJson *json, unsigned int object, const char *name, char *out, unsigned long long room)
{
    const unsigned int member = cli_json_member(json, object, name);
    VerbaLinea line = {out, room, 0ull};
    if (member != CLI_JSON_NONE)
    {
        cli_json_write(json, member, &line, 0u, 0);
    }
    if (!mmgr_line_ok(&line))
    {
        line.at = 0ull;
    }
    (void)mmgr_line_finish(&line);
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
        if ((text != NULL) && (size < CLI_NAME_MAX_LEN))
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
        if ((text != NULL) && (size < CLI_NAME_MAX_LEN) &&
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
    start_json(json, server, "blocked", spec->blocked_args, sizeof(spec->blocked_args));
    start_json(json, server, "trusted", spec->trusted, sizeof(spec->trusted));
    spec->slices = cli_json_is(json, cli_json_member(json, server, "slices"), CFG_JSON_TRUE);
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
    if (named)
    {
        (void)cli_text_lowered_into(extension, sizeof(extension), dot + 1);
    }
    VerbaLinea line = {id, CLI_NAME_MAX_LEN, 0ull};
    for (unsigned int at = 0u; at < spec->ids; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = spec->id_extension[at], .other = extension, .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line, spec->id_name[at]);
            (void)mmgr_line_finish(&line);
            return;
        }
    }
    mmgr_line_put(&line, language);
    (void)mmgr_line_finish(&line);
}

CliLsp *cli_servers_owner_of(const char *path)
{
    cli_lock_take(&g_cli_servers.lock);
    CliLsp *found = NULL;
    for (unsigned int at = 0u; (found == NULL) && (at < CLI_SERVERS_MOST); at += 1u)
    {
        CliServerRunning *const one = &g_cli_servers.running[at];
        if (one->used && cli_lsp_has(one->lsp, path))
        {
            found = one->lsp;
            cli_lsp_ref(found);
        }
    }
    cli_lock_give(&g_cli_servers.lock);
    return found;
}

int cli_servers_slicing(const CliLsp *server)
{
    cli_lock_take(&g_cli_servers.lock);
    int slicing = 0;
    for (unsigned int at = 0u; at < CLI_SERVERS_MOST; at += 1u)
    {
        const CliServerRunning *const one = &g_cli_servers.running[at];
        slicing |= one->used && (one->lsp == server) && one->spec.slices;
    }
    cli_lock_give(&g_cli_servers.lock);
    return slicing;
}

// a file a server had open retained to be given back to the one started after it
static void start_reopen_stored(void *context, const char *path, const char *text, unsigned long long length)
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
            return g_cli_servers.failed_msg[at];
        }
    }
    return NULL;
}

// why the toolchain `tool` failed recorded
static void start_failure(const char *tool, const char *err_msg)
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
    VerbaLinea line = {g_cli_servers.failed_tool[at], CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, tool);
    (void)mmgr_line_finish(&line);
    VerbaLinea why = {g_cli_servers.failed_msg[at], sizeof(g_cli_servers.failed_msg[at]), 0ull};
    mmgr_line_put(&why, err_msg);
    (void)mmgr_line_finish(&why);
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
        VerbaLinea line = {g_cli_servers.restart_tool[at], CLI_NAME_MAX_LEN, 0ull};
        mmgr_line_put(&line, tool);
        (void)mmgr_line_finish(&line);
        cli_restarts_made(&g_cli_servers.restarts[at], CLI_SERVERS_RESTARTS, CLI_SERVERS_RESTARTS_WITHIN);
        g_cli_servers.restarted += 1u;
    }
    return cli_restarts_died(&g_cli_servers.restarts[at], cli_clock_milliseconds());
}

CliLsp *cli_servers_server(const char *root, const char *language, const CliServersSink *sink, CliServerSpec *spec,
                           VerbaLinea *err_msg)
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
        VerbaLinea line = {g_cli_servers.root, CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, root);
        (void)mmgr_line_finish(&line);
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
        cli_lsp_open_each(old, start_reopen_stored, NULL);
        cli_servers_stop_later(old);
    }
    else if (at < CLI_SERVERS_MOST)
    {
        CliLsp *const server = g_cli_servers.running[at].lsp;
        if (cli_lsp_alive(server))
        {
            cli_lsp_ref(server);
            cli_lock_give(&g_cli_servers.lock);
            return server;
        }
        g_cli_servers.running[at].used = 0;
        if (!start_again(spec->tool))
        {
            char why[CLI_LSP_COMPLAINT];
            VerbaLinea complaint = {why, sizeof(why), 0ull};
            cli_lsp_complaint(server, &complaint);
            mmgr_line_put(err_msg, spec->program);
            mmgr_line_put(err_msg, " ended ");
            mmgr_line_decimal(err_msg, CLI_SERVERS_RESTARTS + 1u, 1u);
            mmgr_line_put(err_msg, " times within ");
            mmgr_line_decimal(err_msg, CLI_SERVERS_RESTARTS_WITHIN / 1000u, 1u);
            mmgr_line_put(err_msg, " s and is left stopped");
            if (complaint.at > 0u)
            {
                mmgr_line_put(err_msg, ": ");
                mmgr_line_put_n(err_msg, why, complaint.at);
            }
            (void)mmgr_line_finish(err_msg);
            start_failure(spec->tool, err_msg->out);
            cli_lsp_unref(server);
            cli_lock_give(&g_cli_servers.lock);
            return NULL;
        }
        cli_lsp_open_each(server, start_reopen_stored, NULL);
        cli_lsp_unref(server);
    }
    const char *const failed = start_failed(spec->tool);
    if (failed != NULL)
    {
        mmgr_line_put(err_msg, failed);
        cli_lock_give(&g_cli_servers.lock);
        return NULL;
    }
    CliLsp *const server = cli_servers_launch(root, spec, sink, err_msg);
    if (server == NULL)
    {
        (void)mmgr_line_finish(err_msg);
        start_failure(spec->tool, err_msg->out);
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
        char id[CLI_NAME_MAX_LEN];
        cli_servers_id_of(spec, path, language, id);
        char ignored[256];
        VerbaLinea quiet = {ignored, sizeof(ignored), 0ull};
        (void)cli_lsp_opened(server, path, id, text, size, &quiet);
    }
    for (unsigned int index = 0u; index < CLI_SERVERS_MOST; index += 1u)
    {
        if (!g_cli_servers.running[index].used)
        {
            g_cli_servers.running[index].used = 1;
            g_cli_servers.running[index].lsp = server;
            g_cli_servers.running[index].spec = *spec;
            g_cli_servers.running[index].waits = 0u;
            g_cli_servers.running[index].asks = 0ull;
            g_cli_servers.running[index].quiet_from = cli_clock_milliseconds();
            break;
        }
    }
    cli_lsp_ref(server);
    cli_lock_give(&g_cli_servers.lock);
    return server;
}
