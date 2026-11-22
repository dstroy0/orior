// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scripts.c: the hosts of the script plugins (scripts.h). A host is a language server to the LSP client: its process is
// Node, the program host.js, written into orior's own folder and read from there. Node starts with its permission
// model, and the one file that it reads is host.js. The plugin's code, its id and the state that it keeps go to the
// host in `initialize`. The host reports to the overseer as a plugin, under the plugin's id, held to its resources. A
// watcher thread stops each host after a quiet, and the state that the plugin keeps goes into plugin_state/<id>.json
// first
#include "scripts.h"
#include "../command_line/pool.h"
#include "../machine/overseer.h"
#include "../plugins/plugins.h"
#include "../programs/executables.h"
#include "../servers/lsp.h"
#include "../text/json_tree.h"

// the most bytes of a plugin's code, the most bytes of its stored state, and how often the watcher looks, in
// milliseconds
#define CLI_SCRIPTS_CODE_MAX (4ULL << 20u)
#define CLI_SCRIPTS_STATE_MAX (1ULL << 20u)
#define CLI_SCRIPTS_LOOK_MS 1000u

// One host: whether it holds its place, its plugin's id, the server it runs as, the asks under way, and when the last
// ask ended
typedef struct
{
    int used;
    char id[CLI_NAME_MAX_LEN];
    CliLsp *lsp;
    unsigned int asks;
    unsigned long long quiet_from;
} ScriptsHost;

// The resources of a plugin: its working set in bytes, its share of one processor in percent, and the milliseconds of
// one ask
typedef struct
{
    unsigned long long memory_bytes;
    unsigned int cpu_percent;
    unsigned long long ask_ms;
} ScriptsResources;

// the hosts under their gate; the gate that one start holds; and the watcher, whether it runs
static ScriptsHost s_hosts[CLI_SCRIPTS_HOSTS];
static CliGate s_gate = CLI_GATE_INIT;
static CliGate s_starting = CLI_GATE_INIT;
static CliThread s_watcher;
static int s_watching;

typedef struct
{
    const char *id;
    const char *leaf;
    const char *name;
    char *path_out;
    VerbaLinea *err_msg;
    ScriptsResources *resources;
    const CliJson *json;
    unsigned int node;
    const char *key;
    unsigned long long fallback;
    unsigned int at;
    CliLsp *lsp;
    VerbaLinea *out;
} ScriptsCtx;

// the place of the host of the plugin `id`, CLI_SCRIPTS_HOSTS where none runs. Under the gate
EMBED_INLINE unsigned int scripts_position(const ScriptsCtx *args)
{
    unsigned int at = 0u;
    while ((at < CLI_SCRIPTS_HOSTS) &&
           !(s_hosts[at].used &&
             cellul.eq(&(CatenaFinitaCfg){.src = s_hosts[at].id, .other = args->id, .cap = CLI_NAME_MAX_LEN})))
    {
        at += 1u;
    }
    return at;
}

// the path of `name` in the folder `leaf` of orior's own folder into `path_out`, sized for CLI_PATH_MAX_LEN. The call
// makes the folder where it is not there: 1, or 0 with why given
EMBED_INLINE int scripts_path(const ScriptsCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        mmgr_line_put(args->err_msg, "orior's own folder could not be found");
        return 0;
    }
    VerbaLinea folder = {args->path_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&folder, home);
    mmgr_line_put(&folder, "/");
    mmgr_line_put(&folder, args->leaf);
    if ((mmgr_line_finish(&folder) == 0ULL) ||
        !EMBED_CALL(cli_words_err_msg_internal.private_folder_init, CliWordsErrMsgCtx, .folder = args->path_out))
    {
        mmgr_line_put(args->err_msg, "the folder ");
        mmgr_line_put(args->err_msg, args->leaf);
        mmgr_line_put(args->err_msg, " could not be made in orior's own folder");
        return 0;
    }
    mmgr_line_put(&folder, "/");
    mmgr_line_put(&folder, args->name);
    return mmgr_line_finish(&folder) != 0ULL;
}

// the whole number that the dotted `key` reaches in a plugin's settings, `fallback` where it reaches none or no number
EMBED_INLINE unsigned long long scripts_number(const ScriptsCtx *args)
{
    const unsigned int node = EMBED_CALL(cli_plugins_setting_table.setting, CliPluginsSettingCfg, .json = args->json,
                                         .node = args->node, .key = args->key);
    if ((node == CLI_JSON_NONE) || (args->json->node[node].type != CFG_JSON_NUMBER))
    {
        return args->fallback;
    }
    const char *const text = args->json->text + args->json->node[node].text;
    unsigned long long value = 0ULL;
    unsigned long long at = 0ULL;
    while ((at < args->json->node[node].length) && (text[at] >= '0') && (text[at] <= '9'))
    {
        value = (value * 10ULL) + (unsigned long long)(text[at] - '0');
        at += 1ULL;
    }
    return ((at == 0ULL) || (value == 0ULL)) ? args->fallback : value;
}

// the resources of one plugin read from its kept settings, as cli_plugins_settings_of passes it
static void scripts_resources_read(const CliEachPluginCfg *args)
{
    ScriptsResources *const resources = (ScriptsResources *)args->context;
    const CliJson *const json = args->plugin->json;
    const unsigned int root = args->plugin->root;
    resources->memory_bytes = EMBED_CALL(scripts_number, ScriptsCtx, .json = json, .node = root,
                                         .key = "resources.memoryMiB", .fallback = CLI_SCRIPTS_MEMORY_MIB) *
                              (1ULL << 20u);
    // a share of one processor past what an unsigned int counts names no processor share a machine has
    resources->cpu_percent =
        (unsigned int)EMBED_CALL(scripts_number, ScriptsCtx, .json = json, .node = root, .key = "resources.cpuPercent",
                                 .fallback = CLI_SCRIPTS_CPU_PERCENT);
    resources->ask_ms = EMBED_CALL(scripts_number, ScriptsCtx, .json = json, .node = root,
                                   .key = "resources.askSeconds", .fallback = CLI_SCRIPTS_ASK_SECONDS) *
                        1000ULL;
}

// what the host sends unprompted. The window refuses a call of `orior.*` here until its family has a call in C; a line
// that the plugin logs goes nowhere
static void scripts_recv(const CliLspRecvCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->method, .other = "orior/call", .cap = 16u}))
    {
        mmgr_line_put(args->answer, "{\"oriorError\":\"the window answers no call of this plugin\"}");
    }
}

// the host of the overseer's record ended at once, past one of its plugin's limits
static void scripts_overseen_end(void *context)
{
    ScriptsHost *const host = (ScriptsHost *)context;
    cli_gate_shut(&s_gate);
    CliLsp *const lsp = host->used ? host->lsp : NULL;
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = lsp);
    }
    cli_gate_opened(&s_gate);
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.kill, CliLspContextCfg, .lsp = lsp);
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
    }
}

// the options of `initialize` for the plugin `id` into `out`: its id, its code and the state it stored, as JSON. 1, or
// 0 with why given
EMBED_INLINE int scripts_options(const ScriptsCtx *args)
{
    VerbaLinea *const out = args->out;
    char *const code_text = g_cli_guard->temporary_buf_alloc(CLI_SCRIPTS_CODE_MAX, g_cli_scripts_owner);
    if (code_text == NULL)
    {
        mmgr_line_put(args->err_msg, "no room for the plugin's code");
        return 0;
    }
    VerbaLinea code = mmgr_line_over(code_text, CLI_SCRIPTS_CODE_MAX);
    const int read =
        EMBED_CALL(cli_plugins_id.code_get, CliPluginsIdCfg, .id = args->id, .err_msg = args->err_msg, .out = &code);
    char state_path[CLI_PATH_MAX_LEN];
    char state_name[CLI_NAME_MAX_LEN + 8u];
    VerbaLinea named = {state_name, sizeof(state_name), 0ULL};
    mmgr_line_put(&named, args->id);
    mmgr_line_put(&named, ".json");
    unsigned long long state_size = 0ULL;
    const unsigned char *const state = (read && (mmgr_line_finish(&named) != 0ULL) &&
                                        EMBED_CALL(scripts_path, ScriptsCtx, .leaf = "plugin_state", .name = state_name,
                                                   .path_out = state_path, .err_msg = args->err_msg))
                                           ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = state_path,
                                                        .size_pointer = &state_size, .owner = g_cli_scripts_owner)
                                           : NULL;
    if (read)
    {
        mmgr_line_put(out, "{\"id\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->id,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_NAME_MAX_LEN}));
        mmgr_line_put(out, ",\"code\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)code_text, .length = code.at);
        mmgr_line_put(out, ",\"state\":");
        mmgr_line_put_n(out, ((state != NULL) && (state_size > 0ULL)) ? (const char *)state : "null",
                        ((state != NULL) && (state_size > 0ULL)) ? state_size : 4ULL);
        mmgr_line_put(out, "}");
    }
    g_cli_guard->persistent_buf_release((void *)(embed_word)state, g_cli_scripts_owner);
    g_cli_guard->persistent_buf_release(code_text, g_cli_scripts_owner);
    if (read && !mmgr_line_ok(out))
    {
        mmgr_line_put(args->err_msg, "the plugin's code and state do not fit the host's options");
    }
    return read && mmgr_line_ok(out);
}

// the watcher: each host quiet for CLI_SCRIPTS_QUIET_MS stopped, once a CLI_SCRIPTS_LOOK_MS
static void scripts_watch(void *context)
{
    (void)context;
    for (;;)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = CLI_SCRIPTS_LOOK_MS);
        EMBED_CALL(cli_scripts.quiet, CliScriptsCfg, .now_ms = cli_clock_milliseconds());
    }
}

// the host of the plugin `id` started into the place `at`, which the caller took: 1, or 0 with why given and the place
// given back. Under the start's gate
EMBED_INLINE int scripts_start(const ScriptsCtx *args)
{
    char host_path[CLI_PATH_MAX_LEN];
    char node[CLI_PATH_MAX_LEN];
    int ready = EMBED_CALL(scripts_path, ScriptsCtx, .leaf = "plugin_host", .name = "host.js", .path_out = host_path,
                           .err_msg = args->err_msg) &&
                EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "node", .name = "node",
                           .path = node, .err_msg = args->err_msg);
    // the host's folder is its root and where it starts: host.js's path up to its last separator
    char host_folder[CLI_PATH_MAX_LEN];
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = host_folder, .src = host_path, .cap = CLI_PATH_MAX_LEN});
    const unsigned long long name_at = cellul.len(&(CatenaFinitaCfg){.src = host_folder, .cap = CLI_PATH_MAX_LEN});
    host_folder[(ready && (name_at > 8ULL)) ? (name_at - 8ULL) : 0ULL] = '\0';
    if (ready && !EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = host_path,
                             .bytes = g_cli_scripts_host, .size = g_cli_scripts_host_size, .fresh = 1))
    {
        mmgr_line_put(args->err_msg, "the host could not be written into orior's own folder");
        ready = 0;
    }
    ScriptsResources resources = {CLI_SCRIPTS_MEMORY_MIB << 20u, CLI_SCRIPTS_CPU_PERCENT,
                                  CLI_SCRIPTS_ASK_SECONDS * 1000ULL};
    (void)EMBED_CALL(cli_plugins_settings_of_table.of, CliPluginsSettingsOfCfg, .id = args->id,
                     .each = scripts_resources_read, .context = &resources);
    const unsigned long long options_capacity = (CLI_SCRIPTS_CODE_MAX * 6ULL) + CLI_SCRIPTS_STATE_MAX + 512ULL;
    char *const options_text = ready ? g_cli_guard->temporary_buf_alloc(options_capacity, g_cli_scripts_owner) : NULL;
    VerbaLinea options = mmgr_line_over(options_text, (options_text != NULL) ? options_capacity : 0ULL);
    ready = ready && (options_text != NULL) &&
            EMBED_CALL(scripts_options, ScriptsCtx, .id = args->id, .err_msg = args->err_msg, .out = &options);
    char allow[CLI_PATH_MAX_LEN + 32u];
    char heap[48];
    VerbaLinea allowed = {allow, sizeof(allow), 0ULL};
    mmgr_line_put(&allowed, "--allow-fs-read=");
    mmgr_line_put(&allowed, host_path);
    VerbaLinea heaped = {heap, sizeof(heap), 0ULL};
    mmgr_line_put(&heaped, "--max-old-space-size=");
    mmgr_line_decimal(&heaped, resources.memory_bytes >> 20u, 1u);
    ready = ready && (mmgr_line_finish(&allowed) != 0ULL) && (mmgr_line_finish(&heaped) != 0ULL);
    const char *const words[] = {"--permission", allow, "--disallow-code-generation-from-strings", heap, host_path};
    CliLsp *const lsp =
        ready ? EMBED_CALL(cli_lsp_err_msg.start, CliLspErrMsgCfg, .err_msg = args->err_msg, .program = node,
                           .request = words, .count = 5u, .root = host_folder, .options = options.out,
                           .length = options.at, .recv = scripts_recv, .context = &s_hosts[args->at],
                           .overseen = args->id, .overseen_role = CLI_OVERSEER_PLUGIN)
              : NULL;
    g_cli_guard->persistent_buf_release(options_text, g_cli_scripts_owner);
    cli_gate_shut(&s_gate);
    s_hosts[args->at].lsp = lsp;
    s_hosts[args->at].used = (lsp != NULL);
    s_hosts[args->at].quiet_from = cli_clock_milliseconds();
    cli_gate_opened(&s_gate);
    if (lsp == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_overseer.limits, CliOverseerCfg, .name = args->id, .memory_bytes = resources.memory_bytes,
               .cpu_percent = resources.cpu_percent, .ask_ms = resources.ask_ms, .end = scripts_overseen_end,
               .context = &s_hosts[args->at]);
    if (!s_watching)
    {
        s_watcher = (CliThread){scripts_watch, NULL, 0u};
        s_watching = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_watcher);
    }
    return 1;
}

// the state that the host's plugin keeps asked for and stored, then the host stopped and its place given back. The
// caller takes the place `at` out of the table under the gate before
EMBED_INLINE void scripts_kept_stop(const ScriptsCtx *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliLspAnswer answer = {NULL, NULL, 0ULL};
    if (EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = args->lsp, .method = "orior/keep",
                   .params = "null", .length = 4ULL, .wait = CLI_SCRIPTS_ASK_SECONDS * 1000ULL, .answer = &answer,
                   .err_msg = &err_msg) &&
        (answer.size > 0ULL) && !cellul.eq(&(CatenaFinitaCfg){.src = answer.text, .other = "null", .cap = answer.size}))
    {
        char state_path[CLI_PATH_MAX_LEN];
        char state_name[CLI_NAME_MAX_LEN + 8u];
        VerbaLinea named = {state_name, sizeof(state_name), 0ULL};
        mmgr_line_put(&named, args->id);
        mmgr_line_put(&named, ".json");
        if ((mmgr_line_finish(&named) != 0ULL) &&
            EMBED_CALL(scripts_path, ScriptsCtx, .leaf = "plugin_state", .name = state_name, .path_out = state_path,
                       .err_msg = &err_msg))
        {
            (void)EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = state_path,
                             .bytes = answer.text, .size = answer.size, .fresh = 1);
        }
    }
    g_cli_guard->persistent_buf_release(answer.message, g_cli_servers_owner);
    EMBED_CALL(cli_lsp_context.stop, CliLspContextCfg, .lsp = args->lsp);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = args->lsp);
}

int cli_scripts_ask(const CliScriptsCfg *args)
{
    cli_gate_shut(&s_starting);
    cli_gate_shut(&s_gate);
    unsigned int at = EMBED_CALL(scripts_position, ScriptsCtx, .id = args->id);
    int started = (at < CLI_SCRIPTS_HOSTS);
    // a host whose process ended, past a limit or of itself, gives its place back and starts again in it
    CliLsp *const ended = (started && !EMBED_CALL(cli_lsp_context.alive, CliLspContextCfg, .lsp = s_hosts[at].lsp))
                              ? s_hosts[at].lsp
                              : NULL;
    if (ended != NULL)
    {
        s_hosts[at].used = 0;
        s_hosts[at].lsp = NULL;
        started = 0;
    }
    if (!started && (ended == NULL))
    {
        at = 0u;
        while ((at < CLI_SCRIPTS_HOSTS) && s_hosts[at].used)
        {
            at += 1u;
        }
    }
    if (!started && (at < CLI_SCRIPTS_HOSTS))
    {
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = s_hosts[at].id, .src = args->id, .cap = CLI_NAME_MAX_LEN});
        s_hosts[at].asks = 0u;
    }
    cli_gate_opened(&s_gate);
    if (ended != NULL)
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = ended);
    }
    if (at >= CLI_SCRIPTS_HOSTS)
    {
        cli_gate_opened(&s_starting);
        mmgr_line_put(args->err_msg, "no more script plugins run at once");
        return 0;
    }
    started = started || EMBED_CALL(scripts_start, ScriptsCtx, .id = args->id, .at = at, .err_msg = args->err_msg);
    cli_gate_shut(&s_gate);
    CliLsp *const lsp = (started && s_hosts[at].used) ? s_hosts[at].lsp : NULL;
    if (lsp != NULL)
    {
        EMBED_CALL(cli_lsp_context.ref, CliLspContextCfg, .lsp = lsp);
        s_hosts[at].asks += 1u;
    }
    cli_gate_opened(&s_gate);
    cli_gate_opened(&s_starting);
    if (lsp == NULL)
    {
        return 0;
    }
    static const char *const s_sources[3] = {"command", "event", "server"};
    const unsigned long long params_capacity = args->length + (CLI_PATH_MAX_LEN * 2ULL);
    char *const asked_text = g_cli_guard->temporary_buf_alloc(params_capacity, g_cli_scripts_owner);
    VerbaLinea params = mmgr_line_over(asked_text, (asked_text != NULL) ? params_capacity : 0ULL);
    mmgr_line_put(&params, "{\"source\":\"");
    mmgr_line_put(&params, s_sources[(args->source < 3u) ? args->source : 0u]);
    mmgr_line_put(&params, "\",\"name\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &params,
               .text_bytes = (const unsigned char *)args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_PATH_MAX_LEN}));
    mmgr_line_put(&params, ",\"value\":");
    mmgr_line_put_n(&params, (args->length > 0ULL) ? args->value : "null", (args->length > 0ULL) ? args->length : 4ULL);
    mmgr_line_put(&params, "}");
    ScriptsResources resources = {CLI_SCRIPTS_MEMORY_MIB << 20u, CLI_SCRIPTS_CPU_PERCENT,
                                  CLI_SCRIPTS_ASK_SECONDS * 1000ULL};
    (void)EMBED_CALL(cli_plugins_settings_of_table.of, CliPluginsSettingsOfCfg, .id = args->id,
                     .each = scripts_resources_read, .context = &resources);
    CliLspAnswer answer = {NULL, NULL, 0ULL};
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = args->id, .role = CLI_OVERSEER_PLUGIN,
               .event = CLI_OVERSEER_ASKED);
    const int answered =
        mmgr_line_ok(&params) && EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = lsp,
                                            .method = "orior/ask", .params = params.out, .length = params.at,
                                            .wait = resources.ask_ms, .answer = &answer, .err_msg = args->err_msg);
    EMBED_CALL(cli_overseer.report, CliOverseerCfg, .name = args->id, .role = CLI_OVERSEER_PLUGIN,
               .event = CLI_OVERSEER_ANSWERED);
    if (answered && (args->answer != NULL))
    {
        mmgr_line_put_n(args->answer, answer.text, answer.size);
    }
    g_cli_guard->persistent_buf_release(answer.message, g_cli_servers_owner);
    g_cli_guard->persistent_buf_release(asked_text, g_cli_scripts_owner);
    cli_gate_shut(&s_gate);
    s_hosts[at].asks -= (s_hosts[at].asks > 0u) ? 1u : 0u;
    s_hosts[at].quiet_from = cli_clock_milliseconds();
    cli_gate_opened(&s_gate);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = lsp);
    return answered;
}

int cli_scripts_running(const CliScriptsCfg *args)
{
    cli_gate_shut(&s_gate);
    const int running = EMBED_CALL(scripts_position, ScriptsCtx, .id = args->id) < CLI_SCRIPTS_HOSTS;
    cli_gate_opened(&s_gate);
    return running;
}

void cli_scripts_stop(const CliScriptsCfg *args)
{
    cli_gate_shut(&s_gate);
    const unsigned int at = EMBED_CALL(scripts_position, ScriptsCtx, .id = args->id);
    CliLsp *const lsp = (at < CLI_SCRIPTS_HOSTS) ? s_hosts[at].lsp : NULL;
    if (at < CLI_SCRIPTS_HOSTS)
    {
        s_hosts[at].used = 0;
        s_hosts[at].lsp = NULL;
    }
    cli_gate_opened(&s_gate);
    if (lsp != NULL)
    {
        EMBED_CALL(scripts_kept_stop, ScriptsCtx, .id = args->id, .lsp = lsp);
    }
}

void cli_scripts_quiet(const CliScriptsCfg *args)
{
    char going[CLI_SCRIPTS_HOSTS][CLI_NAME_MAX_LEN];
    unsigned int goes = 0u;
    cli_gate_shut(&s_gate);
    for (unsigned int at = 0u; at < CLI_SCRIPTS_HOSTS; at += 1u)
    {
        if (s_hosts[at].used && (s_hosts[at].asks == 0u) && (args->now_ms >= s_hosts[at].quiet_from) &&
            ((args->now_ms - s_hosts[at].quiet_from) >= CLI_SCRIPTS_QUIET_MS))
        {
            (void)cellul.copy(&(CatenaFinitaCfg){.dst = going[goes], .src = s_hosts[at].id, .cap = CLI_NAME_MAX_LEN});
            goes += 1u;
        }
    }
    cli_gate_opened(&s_gate);
    for (unsigned int at = 0u; at < goes; at += 1u)
    {
        EMBED_CALL(cli_scripts.stop, CliScriptsCfg, .id = going[at]);
    }
}

void cli_scripts_end(void)
{
    for (unsigned int at = 0u; at < CLI_SCRIPTS_HOSTS; at += 1u)
    {
        char id[CLI_NAME_MAX_LEN];
        cli_gate_shut(&s_gate);
        const int used = s_hosts[at].used;
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = id, .src = s_hosts[at].id, .cap = CLI_NAME_MAX_LEN});
        cli_gate_opened(&s_gate);
        if (used)
        {
            EMBED_CALL(cli_scripts.stop, CliScriptsCfg, .id = id);
        }
    }
}
