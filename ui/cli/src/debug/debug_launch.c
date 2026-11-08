// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_launch.c: a session started as its plan specifies: the adapter started, connected to or run in orior, informed
// of who requests, the program launched or attached to, the breakpoints and the exceptions to stop on set once the
// adapter reports it is ready for them, and the session listed
#include "../toolchains/toolchains.h"
#include "debug_start.h"
#include "recording.h"

// the most words an adapter is started with, and the folders its spec puts on its PATH
#define CLI_DEBUG_ADAPTER_WORDS 64u
#define CLI_DEBUG_ADAPTER_FOLDERS 64u

typedef struct
{
    CliDebugSession *place;
    const char *root;
    const CliDebugPlan *plan;
    VerbaLinea *err_msg;
    const CliDebugSession *position_read;
    const CliDebugAnswer *answer;
    const CliJson *json;
    unsigned int options;
    CliDebugInfo *info;
    const CliDebugInfo *info_read;
    unsigned long long milliseconds;
} DebugLaunchCtx;

// the adapter started as the plan specifies: run in orior_cli for a recorded run, connected to where a debugger already
// listens, else its program started with the spec's words and PATH: the adapter, or NULL with why given
EMBED_INLINE CliDap *launch_adapter(const DebugLaunchCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    const CliJson *const json = &args->place->start;
    if (args->plan->recording)
    {
        void *const replay = EMBED_CALL(cli_recording.begin, CliRecordingCfg, .root = root);
        if (replay == NULL)
        {
            mmgr_line_put(err_msg, "a recording's replay could not be taken");
            return NULL;
        }
        return EMBED_CALL(cli_dap_emit.within, CliDapEmitCfg, .serve = cli_recording_serve, .serve_context = replay,
                          .emit = cli_debug_event, .context = args->place, .err_msg = err_msg);
    }
    if (args->plan->listening)
    {
        return EMBED_CALL(cli_dap_ends_program.connect, CliDapEndsProgramCfg, .host = args->plan->host,
                          .port = args->plan->port, .ends_program = args->plan->ends_program, .emit = cli_debug_event,
                          .context = args->place, .err_msg = err_msg);
    }
    static char s_folders[CLI_DEBUG_SESSIONS][CLI_DEBUG_ADAPTER_FOLDERS][CLI_PATH_MAX_LEN];
    static char s_path[CLI_DEBUG_SESSIONS][CLI_PATH_TEXT];
    const unsigned int session_at = (unsigned int)(args->place - g_cli_debug.sessions);
    const unsigned int folders =
        EMBED_CALL(cli_toolchains_json_count.folders_for, CliToolchainsJsonCountCfg, .json = json,
                   .by_system = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->plan->spec, .name = "path"),
                   .folders = s_folders[session_at], .count = 0u, .capacity = CLI_DEBUG_ADAPTER_FOLDERS);
    EMBED_CALL(cli_toolchains_count_path.run_path_with, CliToolchainsCountPathCfg,
               .ahead = (const char (*)[CLI_PATH_MAX_LEN])s_folders[session_at], .count = folders,
               .path = s_path[session_at], .size = sizeof(s_path[session_at]));
    const char *words[CLI_DEBUG_ADAPTER_WORDS];
    unsigned int count = 0u;
    const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = args->plan->spec, .name = "args");
    for (unsigned int at = 0u; (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = json, .array = arguments, .index = at) != CLI_JSON_NONE) &&
                               (count < CLI_DEBUG_ADAPTER_WORDS);
         at += 1u)
    {
        words[count] = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = json,
                                  .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                     .json_read = json, .array = arguments, .index = at),
                                  .fallback = "");
        count += 1u;
    }
    return EMBED_CALL(cli_dap_program.start, CliDapProgramCfg, .program = args->plan->program, .request = words,
                      .count = count, .folder = root, .path = s_path[session_at],
                      .ends_program = cellul.eq(
                          &(CatenaFinitaCfg){.src = args->plan->request, .other = "launch", .cap = CLI_POOL_BYTES}),
                      .emit = cli_debug_event, .context = args->place, .err_msg = err_msg);
}

// a start that failed after its adapter started: the adapter's last complaint and the line that installs it given after
// why, the adapter stopped, and the session's place let go
EMBED_INLINE int launch_fail(const DebugLaunchCtx *args)
{
    char complaint[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea complaint_line = {complaint, sizeof(complaint), 0ULL};
    EMBED_CALL(cli_dap.complaint, CliDapCfg, .dap = args->place->dap, .out = &complaint_line);
    EMBED_CALL(cli_dap.stop, CliDapCfg, .dap = args->place->dap);
    if (complaint_line.at > 0ULL)
    {
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put_n(args->err_msg, complaint_line.out, complaint_line.at);
    }
    const char *const setup =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &args->place->start,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &args->place->start, .object = args->plan->spec, .name = "setup"),
                   .length_pointer = NULL);
    if (setup != NULL)
    {
        mmgr_line_ch(args->err_msg, '\n');
        mmgr_line_put(args->err_msg, setup);
        mmgr_line_put(args->err_msg, " installs ");
        mmgr_line_put(args->err_msg,
                      EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                 .json = &args->place->start,
                                 .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = &args->place->start, .object = args->plan->spec,
                                                    .name = "program"),
                                 .fallback = ""));
    }
    EMBED_CALL(cli_debug_session_err_msg_internal.let_go, CliDebugSessionErrMsgCtx, .session = args->place);
    return 0;
}

// what the session is, as the adapter's capabilities report what it can do, read into `info`
EMBED_INLINE void launch_info(const DebugLaunchCtx *args)
{
    const CliJson *json = args->json;
    const CliJson *const reply = &args->answer->json;
    const unsigned int capabilities = args->answer->root;
    const unsigned int exceptions = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = args->options, .name = "exceptions");
    const unsigned int filters = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                            .object = capabilities, .name = "exceptionBreakpointFilters");
    memor.set(&(MemoriaCfg){.dst = args->info, .val = 0, .bytes = sizeof(*args->info)});
    args->info->id = args->position_read->info.id;
    memor.cpy(
        &(MemoriaCfg){.dst = args->info->name,
                      .src = args->plan->name,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->plan->name, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(
        &(MemoriaCfg){.dst = args->info->tool,
                      .src = args->plan->tool,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->plan->tool, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = args->info->language,
        .src = args->plan->language,
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->plan->language, .cap = CLI_POOL_BYTES}) + 1u});
    args->info->has_parent = args->plan->has_parent;
    args->info->parent = args->plan->parent;
    for (unsigned int at = 0u; (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = reply, .array = filters, .index = at) != CLI_JSON_NONE) &&
                               (args->info->filter_count < CLI_DEBUG_FILTERS);
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = reply, .array = filters, .index = at);
        const char *const filter =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = reply,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                          .object = one, .name = "filter"),
                       .length_pointer = NULL);
        if (filter == NULL)
        {
            continue;
        }
        CliDebugFilter *const session_at = &args->info->filters[args->info->filter_count];
        memor.cpy(&(MemoriaCfg){.dst = session_at->filter,
                                .src = filter,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = filter, .cap = CLI_POOL_BYTES}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = session_at->label,
            .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = reply,
                              .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = reply, .object = one, .name = "label"),
                              .fallback = filter),
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                           .json = reply,
                                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = reply, .object = one, .name = "label"),
                                           .fallback = filter),
                         .cap = CLI_POOL_BYTES}) +
                     1u});
        session_at->on = 0;
        for (unsigned int on = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = exceptions, .index = on) != CLI_JSON_NONE;
             on += 1u)
        {
            const char *const named =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = exceptions, .index = on),
                           .length_pointer = NULL);
            session_at->on = session_at->on ||
                             ((named != NULL) &&
                              (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = filter, .cap = CLI_POOL_BYTES})));
        }
        args->info->filter_count += 1u;
    }
    args->info->conditions =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsConditionalBreakpoints"));
    args->info->hits =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsHitConditionalBreakpoints"));
    args->info->logs =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsLogPoints"));
    args->info->memory =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsReadMemoryRequest"));
    args->info->disassembly =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsDisassembleRequest"));
    args->info->data =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsDataBreakpoints"));
    args->info->exception_info =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsExceptionInfoRequest"));
    args->info->step_back =
        EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = reply,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = reply,
                                      .object = capabilities, .name = "supportsStepBack"));
    args->info->recorded = args->plan->recording;
    args->info->has_process = args->plan->has_pid;
    args->info->process = args->plan->pid;
}

// each file's breakpoints set in the session where it debugs the file's language, and the exceptions it stops on
static void launch_configure(const DebugLaunchCtx *args)
{
    const char *root = args->root;
    const CliJson *json = args->json;
    static char s_positioned[CLI_DEBUG_SESSIONS][1u << 16u];
    const unsigned int session_at = (unsigned int)(args->place - g_cli_debug.sessions);
    const unsigned int breakpoints = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = args->options, .name = "breakpoints");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                          .node = breakpoints, .type = CFG_JSON_OBJECT)
                                   ? json->node[breakpoints].first
                                   : CLI_JSON_NONE;
    for (unsigned int file = first; file != CLI_JSON_NONE; file = json->node[file].next)
    {
        const char *const path = json->text + json->node[file].key;
        if (!EMBED_CALL(cli_debug_file_internal.allocs, CliDebugFileCtx, .language = args->info_read->language,
                        .file = path))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea positioned = {s_positioned[session_at], sizeof(s_positioned[session_at]), 0ULL};
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
        (void)EMBED_CALL(cli_debug_session_internal.set, CliDebugSessionCtx, .session = args->place, .file = full,
                         .json = json, .breakpoints = file, .out = &positioned, .err_msg = &err_msg);
    }
    CliDebugAnswer *const answer = cli_debug_answer_alloc();
    mmgr_line_put(&answer->args, "{\"filters\":[");
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < args->info_read->filter_count; at += 1u)
    {
        if (args->info_read->filters[at].on)
        {
            mmgr_line_put(&answer->args, (written > 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args,
                       .text_bytes = (const unsigned char *)args->info_read->filters[at].filter,
                       .length = cellul.len(
                           &(CatenaFinitaCfg){.src = args->info_read->filters[at].filter, .cap = CLI_POOL_BYTES}));
            written += 1u;
        }
    }
    mmgr_line_put(&answer->args, "]}");
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = args->place,
                     .command = "setExceptionBreakpoints", .answer = answer, .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS,
                     .err_msg = &err_msg);
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
}

// whether the adapter reported it is initialized within `milliseconds`
EMBED_INLINE int launch_initialized_within(const DebugLaunchCtx *args)
{
    unsigned long long milliseconds = args->milliseconds;
    const unsigned long long until = cli_clock_milliseconds() + milliseconds;
    while (!args->place->initialized && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &g_cli_debug.wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int initialized = args->place->initialized;
    return initialized;
}

int cli_debug_start(const CliDebugRootCfg *args)
{
    CliDebugSink sink = args->sink;
    VerbaLinea *err_msg = args->err_msg;
    CliDebugSession *const place = cli_debug_position();
    if (place == NULL)
    {
        mmgr_line_put(err_msg, "four debug sessions run already; end one to start another");
        return 0;
    }
    CliDebugPlan plan;
    if (!EMBED_CALL(cli_debug_start_table.debug_plan, CliDebugStartCfg, .place = place, .root = args->root,
                    .json = args->json, .start = args->start, .options = args->options, .plan = &plan,
                    .err_msg = err_msg))
    {
        g_cli_guard->persistent_buf_release(place->positioned, g_cli_debug_owner);
        place->data = NULL;
        place->data_count = 0u;
        place->data_capacity = 0u;
        place->positioned = NULL;
        place->positioned_count = 0u;
        place->positioned_capacity = 0u;
        g_cli_guard->persistent_buf_release(place->start.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), place->start.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&place->start),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        place->reserved = 0;
        return 0;
    }
    g_cli_debug.next += 1ULL;
    place->info.id = g_cli_debug.next;
    place->sink = sink;
    const int reached = plan.listening;
    place->dap = EMBED_CALL(launch_adapter, DebugLaunchCtx, .place = place, .root = args->root, .plan = &plan,
                            .err_msg = err_msg);
    if (place->dap == NULL)
    {
        g_cli_guard->persistent_buf_release(place->positioned, g_cli_debug_owner);
        place->data = NULL;
        place->data_count = 0u;
        place->data_capacity = 0u;
        place->positioned = NULL;
        place->positioned_count = 0u;
        place->positioned_capacity = 0u;
        g_cli_guard->persistent_buf_release(place->start.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), place->start.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&place->start),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        place->reserved = 0;
        return 0;
    }
    CliDebugAnswer *const answer = cli_debug_answer_alloc();
    mmgr_line_put(&answer->args, "{\"clientID\":\"orior\",\"clientName\":\"orior\",\"adapterID\":");
    const char *const adapter_id =
        EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = &place->start,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &place->start, .object = plan.spec, .name = "program"),
                   .fallback = "");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &answer->args,
               .text_bytes = (const unsigned char *)adapter_id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = adapter_id, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&answer->args, ",\"locale\":\"en\",\"linesStartAt1\":true,\"columnsStartAt1\":true,"
                                 "\"pathFormat\":\"path\",\"supportsVariableType\":true,"
                                 "\"supportsRunInTerminalRequest\":false,\"supportsMemoryReferences\":true,"
                                 "\"supportsStartDebuggingRequest\":false}");
    const unsigned long long before = err_msg->at;
    if (!EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx, .session = place,
                    .command = "initialize", .answer = answer,
                    .wait = reached ? CLI_DEBUG_REQUEST_TIMEOUT_MS : CLI_DEBUG_STARTING, .err_msg = err_msg))
    {
        EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
        if (reached)
        {
            // an adapter already listening answers at once; one that took the connection and is silent is serving
            // another client
            err_msg->at = before;
            mmgr_line_put(err_msg, plan.name);
            mmgr_line_put(err_msg, " took the connection and did not answer; it may be serving another debugger");
        }
        return EMBED_CALL(launch_fail, DebugLaunchCtx, .place = place, .plan = &plan, .err_msg = err_msg);
    }
    CliDebugInfo info;
    EMBED_CALL(launch_info, DebugLaunchCtx, .position_read = place, .plan = &plan, .answer = answer, .json = args->json,
               .options = args->options, .info = &info);
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = answer);
    // an adapter that reports it is ready before the launch, as gdb does, runs the program as the launch comes: its
    // breakpoints are set first, each bound as the program loads; the others report it once the launch is under way
    const int early =
        EMBED_CALL(launch_initialized_within, DebugLaunchCtx, .place = place, .milliseconds = CLI_DEBUG_EARLY);
    if (early)
    {
        EMBED_CALL(launch_configure, DebugLaunchCtx, .place = place, .root = args->root, .json = args->json,
                   .options = args->options, .info_read = &info);
    }
    // the launch's arguments in a cell of the most they hold, given back once sent
    char *const written = g_cli_guard->temporary_buf_alloc(CLI_DEBUG_ARGS, g_cli_debug_owner);
    VerbaLinea arguments = mmgr_line_over(written, CLI_DEBUG_ARGS);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &place->start,
               .node = plan.arguments, .out = &arguments, .indent = 0u, .sorted = 0);
    unsigned long long seq = 0ULL;
    const int sent =
        mmgr_line_ok(&arguments) &&
        EMBED_CALL(cli_dap_err_msg_command.send, CliDapErrMsgCommandCfg, .dap = place->dap, .command = plan.request,
                   .arguments = written, .length = arguments.at, .seq_pointer = &seq, .err_msg = err_msg);
    g_cli_guard->persistent_buf_release(written, g_cli_debug_owner);
    if (!sent)
    {
        return EMBED_CALL(launch_fail, DebugLaunchCtx, .place = place, .plan = &plan, .err_msg = err_msg);
    }
    if (!early)
    {
        if (!EMBED_CALL(launch_initialized_within, DebugLaunchCtx, .place = place, .milliseconds = CLI_DEBUG_STARTING))
        {
            if (EMBED_CALL(cli_dap_err_msg_command_seq.answer, CliDapErrMsgCommandSeqCfg, .dap = place->dap, .seq = seq,
                           .err_msg = err_msg) != 0)
            {
                mmgr_line_put(err_msg, adapter_id);
                mmgr_line_put(err_msg, " did not report it was ready");
            }
            return EMBED_CALL(launch_fail, DebugLaunchCtx, .place = place, .plan = &plan, .err_msg = err_msg);
        }
        EMBED_CALL(launch_configure, DebugLaunchCtx, .place = place, .root = args->root, .json = args->json,
                   .options = args->options, .info_read = &info);
    }
    CliDebugAnswer *const done = cli_debug_answer_alloc();
    const int configured = EMBED_CALL(cli_debug_session_command_internal.request, CliDebugSessionCommandCtx,
                                      .session = place, .command = "configurationDone", .answer = done,
                                      .wait = CLI_DEBUG_REQUEST_TIMEOUT_MS, .err_msg = err_msg);
    EMBED_CALL(cli_debug_session_command_internal.answer_free, CliDebugSessionCommandCtx, .answer = done);
    if (!configured)
    {
        return EMBED_CALL(launch_fail, DebugLaunchCtx, .place = place, .plan = &plan, .err_msg = err_msg);
    }
    CliLspAnswer launched;
    if (!EMBED_CALL(cli_dap_err_msg_command_seq.wait, CliDapErrMsgCommandSeqCfg, .dap = place->dap,
                    .command = plan.request, .seq = seq, .wait = CLI_DEBUG_STARTING, .answer = &launched,
                    .err_msg = err_msg))
    {
        return EMBED_CALL(launch_fail, DebugLaunchCtx, .place = place, .plan = &plan, .err_msg = err_msg);
    }
    g_cli_guard->persistent_buf_release(launched.message, g_cli_debug_owner);
    if (!info.has_process && (place->pid != 0ULL))
    {
        info.has_process = 1;
        // a process's number retains its low 32 bits, as the systems' numbers are
        info.process = (unsigned int)(place->pid & 0xffffffffULL);
    }
    place->info = info;
    memor.cpy(&(MemoriaCfg){.dst = place->program,
                            .src = adapter_id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = adapter_id, .cap = CLI_POOL_BYTES}) + 1u});
    place->list_entries = 1;
    EMBED_CALL(cli_debug_info_internal.info_write, CliDebugInfoCtx, .info = &info, .out = args->out);
    return 1;
}
