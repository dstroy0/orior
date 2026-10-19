// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_launch.c: a session started as its plan says: the adapter started, connected to or run in orior, told who asks,
// the program launched or attached to, the breakpoints and the exceptions to stop on set once the adapter says it is
// ready for them, and the session listed
#include "../toolchains/toolchains.h"
#include "debug_start.h"
#include "recording.h"

// the most words an adapter is started with, and the folders its spec puts on its PATH
#define CLI_DEBUG_ADAPTER_WORDS 64u
#define CLI_DEBUG_ADAPTER_FOLDERS 64u

// a place given back whose session never had an adapter
static void launch_place_freed(CliDebugSession *place)
{
    cli_lock_take(&g_cli_debug.lock);
    place->taken = 0;
    cli_lock_give(&g_cli_debug.lock);
}

// the adapter started as the plan says: run in orior for a recorded run, connected to where a debugger already
// listens, else its program started with the spec's words and PATH: the adapter, or NULL with why said
static CliDap *launch_adapter(CliDebugSession *place, const char *root, const CliDebugPlan *plan, CliLine *said)
{
    const CliJson *const json = &place->start;
    if (plan->recording)
    {
        void *const replay = cli_recording_begun(root);
        if (replay == NULL)
        {
            cli_line_text(said, "a recording's replay could not be taken");
            return NULL;
        }
        return cli_dap_within(cli_recording_serve, replay, cli_debug_heard, place, said);
    }
    if (plan->listening)
    {
        return cli_dap_connected(plan->host, plan->port, plan->ends_program, cli_debug_heard, place, said);
    }
    static char s_folders[CLI_DEBUG_SESSIONS][CLI_DEBUG_ADAPTER_FOLDERS][CLI_PATH_LONGEST];
    static char s_path[CLI_DEBUG_SESSIONS][CLI_PATH_TEXT];
    const unsigned int slot = (unsigned int)(place - g_cli_debug.sessions);
    const unsigned int folders = cli_toolchains_folders_for(json, cli_json_member(json, plan->spec, "path"),
                                                            s_folders[slot], 0u, CLI_DEBUG_ADAPTER_FOLDERS);
    cli_toolchains_run_path_with((const char (*)[CLI_PATH_LONGEST])s_folders[slot], folders, s_path[slot],
                                 sizeof(s_path[slot]));
    const char *words[CLI_DEBUG_ADAPTER_WORDS];
    unsigned int count = 0u;
    const unsigned int args = cli_json_member(json, plan->spec, "args");
    for (unsigned int at = 0u; (cli_json_item(json, args, at) != CLI_JSON_NONE) && (count < CLI_DEBUG_ADAPTER_WORDS);
         at += 1u)
    {
        words[count] = cli_debug_text(json, cli_json_item(json, args, at), "");
        count += 1u;
    }
    return cli_dap_started(
        plan->program, words, count, root, s_path[slot],
        cellul.eq(&(CatenaFinitaCfg){.src = plan->request, .other = "launch", .cap = CLI_CELLS_POOL}), cli_debug_heard,
        place, said);
}

// a start that failed after its adapter started: the adapter's last complaint and the line that installs it said after
// why, the adapter stopped, and the session's place let go
static int launch_failed(CliDebugSession *place, const CliDebugPlan *plan, CliLine *said)
{
    char complaint[CLI_SAID_LONGEST];
    CliLine heard = {complaint, sizeof(complaint), 0ull};
    cli_dap_complaint(place->dap, &heard);
    cli_dap_stopped(place->dap);
    if (heard.at > 0ull)
    {
        cli_line_text(said, ": ");
        cli_line_counted(said, heard.out, heard.at);
    }
    const char *const setup = cli_json_string(&place->start, cli_json_member(&place->start, plan->spec, "setup"), NULL);
    if (setup != NULL)
    {
        cli_line_character(said, '\n');
        cli_line_text(said, setup);
        cli_line_text(said, " installs ");
        cli_line_text(said, cli_debug_text(&place->start, cli_json_member(&place->start, plan->spec, "program"), ""));
    }
    cli_debug_let_go(place);
    return 0;
}

// what the session is, as the adapter's capabilities say what it can do, read into `info`
static void launch_info(const CliDebugSession *place, const CliDebugPlan *plan, const CliDebugAnswer *answer,
                        const CliJson *json, unsigned int options, CliDebugInfo *info)
{
    const CliJson *const said = &answer->json;
    const unsigned int capabilities = answer->root;
    const unsigned int exceptions = cli_json_member(json, options, "exceptions");
    const unsigned int filters = cli_json_member(said, capabilities, "exceptionBreakpointFilters");
    memor.set(&(MemoriaCfg){.dst = info, .val = 0, .bytes = sizeof(*info)});
    info->id = place->info.id;
    memor.cpy(&(MemoriaCfg){.dst = info->name,
                            .src = plan->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = plan->name, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = info->tool,
                            .src = plan->tool,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = plan->tool, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(
        &(MemoriaCfg){.dst = info->language,
                      .src = plan->language,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = plan->language, .cap = CLI_CELLS_POOL}) + 1u});
    info->has_parent = plan->has_parent;
    info->parent = plan->parent;
    for (unsigned int at = 0u;
         (cli_json_item(said, filters, at) != CLI_JSON_NONE) && (info->filter_count < CLI_DEBUG_FILTERS); at += 1u)
    {
        const unsigned int one = cli_json_item(said, filters, at);
        const char *const filter = cli_json_string(said, cli_json_member(said, one, "filter"), NULL);
        if (filter == NULL)
        {
            continue;
        }
        CliDebugFilter *const kept = &info->filters[info->filter_count];
        memor.cpy(&(MemoriaCfg){.dst = kept->filter,
                                .src = filter,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = filter, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = kept->label,
                                .src = cli_debug_text(said, cli_json_member(said, one, "label"), filter),
                                .bytes = cellul.len(&(CatenaFinitaCfg){
                                             .src = cli_debug_text(said, cli_json_member(said, one, "label"), filter),
                                             .cap = CLI_CELLS_POOL}) +
                                         1u});
        kept->on = 0;
        for (unsigned int on = 0u; cli_json_item(json, exceptions, on) != CLI_JSON_NONE; on += 1u)
        {
            const char *const named = cli_json_string(json, cli_json_item(json, exceptions, on), NULL);
            kept->on =
                kept->on || ((named != NULL) &&
                             (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = filter, .cap = CLI_CELLS_POOL})));
        }
        info->filter_count += 1u;
    }
    info->conditions = cli_debug_true(said, cli_json_member(said, capabilities, "supportsConditionalBreakpoints"));
    info->hits = cli_debug_true(said, cli_json_member(said, capabilities, "supportsHitConditionalBreakpoints"));
    info->logs = cli_debug_true(said, cli_json_member(said, capabilities, "supportsLogPoints"));
    info->memory = cli_debug_true(said, cli_json_member(said, capabilities, "supportsReadMemoryRequest"));
    info->disassembly = cli_debug_true(said, cli_json_member(said, capabilities, "supportsDisassembleRequest"));
    info->data = cli_debug_true(said, cli_json_member(said, capabilities, "supportsDataBreakpoints"));
    info->exception_info = cli_debug_true(said, cli_json_member(said, capabilities, "supportsExceptionInfoRequest"));
    info->step_back = cli_debug_true(said, cli_json_member(said, capabilities, "supportsStepBack"));
    info->recorded = plan->recording;
    info->has_process = plan->has_pid;
    info->process = plan->pid;
}

// each file's breakpoints set in the session where it debugs the file's language, and the exceptions it stops on
static void launch_configured(CliDebugSession *place, const char *root, const CliJson *json, unsigned int options,
                              const CliDebugInfo *info)
{
    static char s_placed[CLI_DEBUG_SESSIONS][1u << 16u];
    const unsigned int slot = (unsigned int)(place - g_cli_debug.sessions);
    const unsigned int breakpoints = cli_json_member(json, options, "breakpoints");
    const unsigned int first =
        cli_json_is(json, breakpoints, CFG_JSON_OBJECT) ? json->node[breakpoints].first : CLI_JSON_NONE;
    for (unsigned int file = first; file != CLI_JSON_NONE; file = json->node[file].next)
    {
        const char *const path = json->text + json->node[file].key;
        if (!cli_debug_takes(info->language, path))
        {
            continue;
        }
        char full[CLI_PATH_LONGEST];
        char why[CLI_SAID_LONGEST];
        CliLine placed = {s_placed[slot], sizeof(s_placed[slot]), 0ull};
        CliLine said = {why, sizeof(why), 0ull};
        cli_full(root, path, full);
        (void)cli_debug_set(place, full, json, file, &placed, &said);
    }
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    cli_line_text(&answer->asked, "{\"filters\":[");
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < info->filter_count; at += 1u)
    {
        if (info->filters[at].on)
        {
            cli_line_text(&answer->asked, (written > 0u) ? "," : "");
            cli_json_quoted_lossy(
                &answer->asked, (const unsigned char *)info->filters[at].filter,
                cellul.len(&(CatenaFinitaCfg){.src = info->filters[at].filter, .cap = CLI_CELLS_POOL}));
            written += 1u;
        }
    }
    cli_line_text(&answer->asked, "]}");
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    (void)cli_debug_asked(place, "setExceptionBreakpoints", answer, CLI_DEBUG_ASKING, &said);
    cli_debug_answer_given(answer);
}

// whether the adapter said it is initialized within `milliseconds`
static int launch_initialized_within(CliDebugSession *place, unsigned long long milliseconds)
{
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    cli_lock_take(&g_cli_debug.lock);
    while (!place->initialized && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&g_cli_debug.wake, &g_cli_debug.lock, until - cli_clock_milliseconds());
    }
    const int initialized = place->initialized;
    cli_lock_give(&g_cli_debug.lock);
    return initialized;
}

int cli_debug_start(const char *root, const CliJson *json, unsigned int start, unsigned int options, CliDebugEmit emit,
                    CliLine *out, CliLine *said)
{
    CliDebugSession *const place = cli_debug_place();
    if (place == NULL)
    {
        cli_line_text(said, "four debug sessions run already; end one to start another");
        return 0;
    }
    CliDebugPlan plan;
    if (!cli_debug_planned(place, root, json, start, options, &plan, said))
    {
        launch_place_freed(place);
        return 0;
    }
    cli_lock_take(&g_cli_debug.lock);
    g_cli_debug.next += 1ull;
    place->info.id = g_cli_debug.next;
    place->emit = emit;
    cli_lock_give(&g_cli_debug.lock);
    const int reached = plan.listening;
    place->dap = launch_adapter(place, root, &plan, said);
    if (place->dap == NULL)
    {
        launch_place_freed(place);
        return 0;
    }
    CliDebugAnswer *const answer = cli_debug_answer_taken();
    cli_line_text(&answer->asked, "{\"clientID\":\"orior\",\"clientName\":\"orior\",\"adapterID\":");
    const char *const adapter_id =
        cli_debug_text(&place->start, cli_json_member(&place->start, plan.spec, "program"), "");
    cli_json_quoted_lossy(&answer->asked, (const unsigned char *)adapter_id,
                          cellul.len(&(CatenaFinitaCfg){.src = adapter_id, .cap = CLI_CELLS_POOL}));
    cli_line_text(&answer->asked, ",\"locale\":\"en\",\"linesStartAt1\":true,\"columnsStartAt1\":true,"
                                  "\"pathFormat\":\"path\",\"supportsVariableType\":true,"
                                  "\"supportsRunInTerminalRequest\":false,\"supportsMemoryReferences\":true,"
                                  "\"supportsStartDebuggingRequest\":false}");
    const unsigned long long before = said->at;
    if (!cli_debug_asked(place, "initialize", answer, reached ? CLI_DEBUG_ASKING : CLI_DEBUG_STARTING, said))
    {
        cli_debug_answer_given(answer);
        if (reached)
        {
            // an adapter already listening answers at once; one that took the connection and is silent is serving
            // another client
            said->at = before;
            cli_line_text(said, plan.name);
            cli_line_text(said, " took the connection and did not answer; it may be serving another debugger");
        }
        return launch_failed(place, &plan, said);
    }
    CliDebugInfo info;
    launch_info(place, &plan, answer, json, options, &info);
    cli_debug_answer_given(answer);
    // an adapter that says it is ready before the launch, as gdb does, runs the program as the launch comes: its
    // breakpoints are set first, each bound as the program loads; the others say it once the launch is under way
    const int early = launch_initialized_within(place, CLI_DEBUG_EARLY);
    if (early)
    {
        launch_configured(place, root, json, options, &info);
    }
    // the launch's arguments written through pieces into a cell of their size, given back once sent
    char piece[1024];
    CliPieces pieces;
    CliLine arguments;
    cli_pieces_begun(&pieces, &arguments, piece, sizeof(piece));
    cli_json_write(&place->start, plan.arguments, &arguments, 0u, 0);
    unsigned long long arguments_size = 0ull;
    char *const written = cli_pieces_joined(&pieces, &arguments, "a launch's arguments", &arguments_size);
    unsigned long long seq = 0ull;
    const int sent = (written != NULL) && cli_dap_sent(place->dap, plan.request, written, arguments_size, &seq, said);
    cli_cell_given(written);
    if (!sent)
    {
        return launch_failed(place, &plan, said);
    }
    if (!early)
    {
        if (!launch_initialized_within(place, CLI_DEBUG_STARTING))
        {
            if (cli_dap_answered(place->dap, seq, said) != 0)
            {
                cli_line_text(said, adapter_id);
                cli_line_text(said, " did not say it was ready");
            }
            return launch_failed(place, &plan, said);
        }
        launch_configured(place, root, json, options, &info);
    }
    CliDebugAnswer *const done = cli_debug_answer_taken();
    const int configured = cli_debug_asked(place, "configurationDone", done, CLI_DEBUG_ASKING, said);
    cli_debug_answer_given(done);
    if (!configured)
    {
        return launch_failed(place, &plan, said);
    }
    char body[1024];
    CliLine launched = {body, sizeof(body), 0ull};
    if (!cli_dap_waited(place->dap, plan.request, seq, CLI_DEBUG_STARTING, &launched, said))
    {
        return launch_failed(place, &plan, said);
    }
    cli_lock_take(&g_cli_debug.lock);
    if (!info.has_process && (place->pid != 0ull))
    {
        info.has_process = 1;
        // a process's number keeps its low 32 bits, as the systems' numbers are
        info.process = (unsigned int)(place->pid & 0xffffffffull);
    }
    place->info = info;
    memor.cpy(&(MemoriaCfg){.dst = place->program,
                            .src = adapter_id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = adapter_id, .cap = CLI_CELLS_POOL}) + 1u});
    place->listed = 1;
    cli_lock_give(&g_cli_debug.lock);
    cli_debug_info_written(&info, out);
    return 1;
}
