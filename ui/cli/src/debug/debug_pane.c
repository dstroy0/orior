// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_pane.c: the Debug pane's sink and step. The sink stores each event, its name and body as texts of one store,
// under a lock taken by exchange and wakes the view's thread; the step takes the store whole under the lock and reads
// each event on the view's thread: output into the console, a stop into the frames and variables with the status
// line, a continue, an exit and an end into the rows and the status line. Here too are the stores' cells, the status
// line's, and the view shown
#include "debug_internal.h"
#include "debug_pane_internal.h"

// the opening sentence, the status line once the session followed ends, and the bytes of a scratch a session's answer
// is written into
#define PANE_OPENING "Debug File (Shift+F9) runs the file in the editor under its debugger."
#define PANE_ENDED "Ended"
#define PANE_ANSWER (1ULL << 20u)

CliDebugPaneState g_cli_debug_pane;

typedef struct
{
    const CliJson *json;
    unsigned int node;
    const char *name;
    unsigned long long *length_out;
    long long *value;
    unsigned long long session;
    const char *event;
    const char *body;
    unsigned long long size;
    const char *root;
} DebugPaneCtx;

// the views' labels, in the order of CLI_DEBUG_VIEW_FRAMES and the rest
static const char *const s_labels[CLI_DEBUG_VIEWS] = {"Frames", "Variables", "Console"};

// the sink's lock taken, a thread spinning while another has it, and let go
EMBED_INLINE void pane_locked(const DebugPaneCtx *args)
{
    (void)args;
    while (EMBED_ATOMIC_EXCHANGE_32(&g_cli_debug_pane.lock, 1u) != 0u)
    {
    }
}

EMBED_INLINE void pane_unlocked(const DebugPaneCtx *args)
{
    (void)args;
    EMBED_ATOMIC_STORE(&g_cli_debug_pane.lock, 0u);
}

unsigned long long cli_debug_pane_text_put(const CliDebugPaneCtx *args)
{
    CliDebugPaneStore *const store = args->store;
    const unsigned long long needed = store->used + args->length + 1ULL;
    if (needed > store->size)
    {
        const unsigned long long size = (needed * 2ULL > 256ULL) ? needed * 2ULL : 256ULL;
        char *const grown = g_cli_guard->persistent_buf_alloc(size, g_cli_debug_pane_owner);
        if (grown == NULL)
        {
            return CLI_DEBUG_PANE_NONE;
        }
        if (store->used != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = store->text, .bytes = store->used});
        }
        g_cli_guard->persistent_buf_release(store->text, g_cli_debug_pane_owner);
        store->text = grown;
        store->size = size;
    }
    const unsigned long long at = store->used;
    if (args->length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = store->text + at, .src = args->text, .bytes = args->length});
    }
    store->text[at + args->length] = '\0';
    store->used = needed;
    return at;
}

CliDebugPaneEntry *cli_debug_pane_entry_put(const CliDebugPaneCtx *args)
{
    CliDebugPaneStore *const store = args->store;
    if (store->count == store->capacity)
    {
        const unsigned int capacity = (store->capacity != 0u) ? store->capacity * 2u : 16u;
        CliDebugPaneEntry *const grown = g_cli_guard->persistent_buf_alloc(
            (unsigned long long)capacity * sizeof(CliDebugPaneEntry), g_cli_debug_pane_owner);
        if (grown == NULL)
        {
            return NULL;
        }
        if (store->count != 0u)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown,
                                    .src = store->entry,
                                    .bytes = (unsigned long long)store->count * sizeof(CliDebugPaneEntry)});
        }
        g_cli_guard->persistent_buf_release(store->entry, g_cli_debug_pane_owner);
        store->entry = grown;
        store->capacity = capacity;
    }
    CliDebugPaneEntry *const entry = &store->entry[store->count];
    *entry = (CliDebugPaneEntry){0};
    store->count += 1u;
    return entry;
}

void cli_debug_pane_store_empty(const CliDebugPaneCtx *args)
{
    CliDebugPaneStore *const store = args->store;
    g_cli_guard->persistent_buf_release(store->entry, g_cli_debug_pane_owner);
    g_cli_guard->persistent_buf_release(store->text, g_cli_debug_pane_owner);
    *store = (CliDebugPaneStore){0};
}

void cli_debug_pane_status_set(const CliDebugPaneCtx *args)
{
    char *const status = g_cli_guard->persistent_buf_alloc(args->length + 1ULL, g_cli_debug_pane_owner);
    if (status == NULL)
    {
        return;
    }
    if (args->length != 0ULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = status, .src = args->text, .bytes = args->length});
    }
    status[args->length] = '\0';
    g_cli_guard->persistent_buf_release(g_cli_debug_pane.status, g_cli_debug_pane_owner);
    g_cli_debug_pane.status = status;
    g_cli_debug_pane.status_length = args->length;
    g_cli_debug_pane.status_failed = args->failed;
}

// the string member `name` of `node`, its bytes into `length_out`: NULL where it has none
EMBED_INLINE const char *pane_member(const DebugPaneCtx *args)
{
    *args->length_out = 0ULL;
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->node, .name = args->name),
                      .length_pointer = args->length_out);
}

// the number member `name` of `node` into `value`: 1, or 0 where it has none
EMBED_INLINE int pane_number(const DebugPaneCtx *args)
{
    return EMBED_CALL(cli_debug_json_node_internal.debug_sign, CliDebugJsonNodeCtx, .json = args->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->node, .name = args->name),
                      .value = args->value);
}

void cli_debug_pane_running_said(const CliDebugPaneCtx *args)
{
    (void)args;
    char sentence[(2u * CLI_PATH_MAX_LEN) + 32u];
    VerbaLinea line = {sentence, sizeof(sentence), 0ULL};
    const char *const texts = (g_cli_debug_pane.session_texts != NULL) ? g_cli_debug_pane.session_texts : "";
    mmgr_line_put(&line, "Running ");
    mmgr_line_put_n(&line, texts, g_cli_debug_pane.name_length);
    mmgr_line_put(&line, " under ");
    mmgr_line_put_n(&line, texts + g_cli_debug_pane.tool_at, g_cli_debug_pane.tool_length);
    EMBED_CALL(cli_debug_pane_internal.status_set, CliDebugPaneCtx, .text = sentence,
               .length = mmgr_line_ok(&line) ? line.at : 0ULL);
}

// the session `session` followed: its name, tool and language and whether it steps back read from the sessions
// running into a cell sized to them, none where it is not listed
EMBED_INLINE void pane_followed(const DebugPaneCtx *args)
{
    g_cli_debug_pane.session = args->session;
    g_cli_debug_pane.has_session = 1;
    g_cli_debug_pane.paused = 0;
    g_cli_debug_pane.has_thread = 0;
    g_cli_debug_pane.step_back = 0;
    g_cli_guard->persistent_buf_release(g_cli_debug_pane.session_texts, g_cli_debug_pane_owner);
    g_cli_debug_pane.session_texts = NULL;
    g_cli_debug_pane.name_length = 0ULL;
    g_cli_debug_pane.tool_at = 0ULL;
    g_cli_debug_pane.tool_length = 0ULL;
    g_cli_debug_pane.language_at = 0ULL;
    g_cli_debug_pane.language_length = 0ULL;
    char *const scratch = g_cli_guard->temporary_buf_alloc(PANE_ANSWER, g_cli_debug_pane_owner);
    if (scratch == NULL)
    {
        return;
    }
    VerbaLinea out = {scratch, PANE_ANSWER, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    g_cli_debug_pane.requests->sessions(&(CliDebugOutErrMsgIdStepCfg){.out = &out, .err_msg = &err_msg});
    CliJson json = {0};
    const unsigned int root = mmgr_line_ok(&out)
                                  ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary,
                                               CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = scratch,
                                               .size = out.at, .err_msg = &err_msg, .owner = g_cli_debug_pane_owner)
                                  : CLI_JSON_NONE;
    for (unsigned int at = 0u; root != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &json, .array = root, .index = at);
        long long id = 0LL;
        // a session's number is far inside what a signed number counts
        if ((one == CLI_JSON_NONE) ||
            (EMBED_CALL(pane_number, DebugPaneCtx, .json = &json, .node = one, .name = "id", .value = &id) &&
             (id == (long long)args->session)))
        {
            unsigned long long name_length = 0ULL;
            unsigned long long tool_length = 0ULL;
            unsigned long long language_length = 0ULL;
            const char *const name = (one != CLI_JSON_NONE)
                                         ? EMBED_CALL(pane_member, DebugPaneCtx, .json = &json, .node = one,
                                                      .name = "name", .length_out = &name_length)
                                         : NULL;
            const char *const tool = (one != CLI_JSON_NONE)
                                         ? EMBED_CALL(pane_member, DebugPaneCtx, .json = &json, .node = one,
                                                      .name = "tool", .length_out = &tool_length)
                                         : NULL;
            const char *const language = (one != CLI_JSON_NONE)
                                             ? EMBED_CALL(pane_member, DebugPaneCtx, .json = &json, .node = one,
                                                          .name = "language", .length_out = &language_length)
                                             : NULL;
            g_cli_debug_pane.step_back =
                (one != CLI_JSON_NONE) &&
                EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx, .json = &json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = one, .name = "step_back"));
            // the three texts one after another, each with its end
            const unsigned long long bytes = name_length + tool_length + language_length + 3ULL;
            char *const texts =
                (one != CLI_JSON_NONE) ? g_cli_guard->persistent_buf_alloc(bytes, g_cli_debug_pane_owner) : NULL;
            if (texts != NULL)
            {
                memor.cpy(&(MemoriaCfg){.dst = texts, .src = (name != NULL) ? name : "", .bytes = name_length});
                texts[name_length] = '\0';
                memor.cpy(&(MemoriaCfg){
                    .dst = texts + name_length + 1ULL, .src = (tool != NULL) ? tool : "", .bytes = tool_length});
                texts[name_length + 1ULL + tool_length] = '\0';
                memor.cpy(&(MemoriaCfg){.dst = texts + name_length + tool_length + 2ULL,
                                        .src = (language != NULL) ? language : "",
                                        .bytes = language_length});
                texts[bytes - 1ULL] = '\0';
                g_cli_debug_pane.session_texts = texts;
                g_cli_debug_pane.name_length = name_length;
                g_cli_debug_pane.tool_at = name_length + 1ULL;
                g_cli_debug_pane.tool_length = tool_length;
                g_cli_debug_pane.language_at = name_length + tool_length + 2ULL;
                g_cli_debug_pane.language_length = language_length;
            }
            break;
        }
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
    g_cli_guard->persistent_buf_release(scratch, g_cli_debug_pane_owner);
}

// the words a stop's reason reads as in the status line, the reason itself where it is none of the adapters' own
EMBED_INLINE const char *pane_why(const DebugPaneCtx *args)
{
    static const char *const s_reasons[6][2] = {
        {"breakpoint", "at a breakpoint"}, {"step", "after a step"},  {"pause", "paused"},
        {"exception", "on an exception"},  {"entry", "at the start"}, {"data breakpoint", "where its data changed"},
    };
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_reasons[at][0], .cap = 32ULL}))
        {
            return s_reasons[at][1];
        }
    }
    return args->name;
}

// a stop of the session followed: its thread, the thread's frames, the first chosen, the status line, Stopped, why
// and where, the chosen frame's variables, and an exception's text written to the console
EMBED_INLINE void pane_stopped(const DebugPaneCtx *args)
{
    const CliJson *const json = args->json;
    long long thread = 0LL;
    g_cli_debug_pane.has_thread =
        EMBED_CALL(pane_number, DebugPaneCtx, .json = json, .node = args->node, .name = "threadId", .value = &thread);
    g_cli_debug_pane.thread = thread;
    g_cli_debug_pane.paused = 1;
    g_cli_debug_pane.frame_at = 0u;
    EMBED_CALL(cli_debug_pane_internal.frames_read, CliDebugPaneCtx, .root = args->root);
    unsigned long long reason_length = 0ULL;
    const char *const reason = EMBED_CALL(pane_member, DebugPaneCtx, .json = json, .node = args->node, .name = "reason",
                                          .length_out = &reason_length);
    const char *const why = EMBED_CALL(pane_why, DebugPaneCtx, .name = (reason != NULL) ? reason : "pause");
    char sentence[CLI_PATH_MAX_LEN + 96u];
    VerbaLinea line = {sentence, sizeof(sentence), 0ULL};
    mmgr_line_put(&line, "Stopped ");
    mmgr_line_put(&line, why);
    const CliDebugPaneStore *const frames = &g_cli_debug_pane.frames;
    if (frames->count != 0u)
    {
        const CliDebugPaneEntry *const top = &frames->entry[0];
        mmgr_line_put(&line, ", ");
        // a frame with no file is placed by its function
        if (top->second_length != 0ULL)
        {
            mmgr_line_put_n(&line, frames->text + top->second, top->second_length);
        }
        else
        {
            mmgr_line_put_n(&line, frames->text + top->text, top->text_length);
        }
    }
    EMBED_CALL(cli_debug_pane_internal.status_set, CliDebugPaneCtx, .text = sentence,
               .length = mmgr_line_ok(&line) ? line.at : 0ULL);
    unsigned long long raised_length = 0ULL;
    const char *const raised = EMBED_CALL(pane_member, DebugPaneCtx, .json = json, .node = args->node, .name = "text",
                                          .length_out = &raised_length);
    if ((reason != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "exception", .cap = 16ULL}) &&
        (raised != NULL))
    {
        EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = raised, .length = raised_length,
                   .kind = CLI_DEBUG_LINE_STDERR);
        EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = "\n", .length = 1ULL,
                   .kind = CLI_DEBUG_LINE_STDERR);
    }
    EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
}

// an output event's text written to the console, an error's as an error line and the debugger's own as its; the
// adapter's telemetry is not the program's and is passed over
EMBED_INLINE void pane_output(const DebugPaneCtx *args)
{
    unsigned long long category_length = 0ULL;
    unsigned long long output_length = 0ULL;
    const char *const category = EMBED_CALL(pane_member, DebugPaneCtx, .json = args->json, .node = args->node,
                                            .name = "category", .length_out = &category_length);
    const char *const output = EMBED_CALL(pane_member, DebugPaneCtx, .json = args->json, .node = args->node,
                                          .name = "output", .length_out = &output_length);
    const char *const kind = (category != NULL) ? category : "stdout";
    if ((output == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "telemetry", .cap = 16ULL}))
    {
        return;
    }
    unsigned int line_kind = CLI_DEBUG_LINE_STDOUT;
    if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "stderr", .cap = 16ULL}))
    {
        line_kind = CLI_DEBUG_LINE_STDERR;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "console", .cap = 16ULL}) ||
             cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "important", .cap = 16ULL}))
    {
        line_kind = CLI_DEBUG_LINE_CONSOLE;
    }
    EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = output, .length = output_length,
               .kind = line_kind);
}

// an exit written to the console as the debugger's words: the session's name, ended with exit code, the code
EMBED_INLINE void pane_exited(const DebugPaneCtx *args)
{
    long long code = 0LL;
    const int coded = EMBED_CALL(pane_number, DebugPaneCtx, .json = args->json, .node = args->node, .name = "exitCode",
                                 .value = &code);
    char sentence[CLI_PATH_MAX_LEN + 64u];
    VerbaLinea line = {sentence, sizeof(sentence), 0ULL};
    if ((g_cli_debug_pane.session_texts != NULL) && (g_cli_debug_pane.name_length != 0ULL))
    {
        mmgr_line_put_n(&line, g_cli_debug_pane.session_texts, g_cli_debug_pane.name_length);
    }
    else
    {
        mmgr_line_put(&line, "The program");
    }
    mmgr_line_put(&line, " ended with exit code ");
    if (coded)
    {
        mmgr_line_decimal_signed(&line, code);
    }
    else
    {
        mmgr_line_put(&line, "unknown");
    }
    mmgr_line_put(&line, ".\n");
    EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = sentence,
               .length = mmgr_line_ok(&line) ? line.at : 0ULL, .kind = CLI_DEBUG_LINE_CONSOLE);
}

// the event `event` of session `session` with `size` bytes of `body` read on the view's thread for the tree at
// `root`: 1 where a row, the bar or the status line changed
EMBED_INLINE int pane_event(const DebugPaneCtx *args)
{
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int body = (args->size != 0ULL)
                                  ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary,
                                               CliJsonTreeTextSizeErrMsgCfg, .json = &json, .text = args->body,
                                               .size = args->size, .err_msg = &err_msg, .owner = g_cli_debug_pane_owner)
                                  : CLI_JSON_NONE;
    const char *const event = args->event;
    // a session's first event follows it while none is followed, and a stop in any session follows it
    const int stop = cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "stopped", .cap = 32ULL});
    const int followed = g_cli_debug_pane.has_session && (g_cli_debug_pane.session == args->session);
    const int follows = !followed && (stop || !g_cli_debug_pane.has_session);
    if (follows)
    {
        EMBED_CALL(pane_followed, DebugPaneCtx, .session = args->session);
        EMBED_CALL(cli_debug_pane_internal.running_said, CliDebugPaneCtx, .root = args->root);
    }
    int changed = follows;
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "output", .cap = 32ULL}))
    {
        EMBED_CALL(pane_output, DebugPaneCtx, .json = &json, .node = body);
        changed = 1;
    }
    else if (stop)
    {
        EMBED_CALL(pane_stopped, DebugPaneCtx, .json = &json, .node = body, .root = args->root);
        changed = 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "continued", .cap = 32ULL}) && (follows || followed))
    {
        g_cli_debug_pane.paused = 0;
        EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = &g_cli_debug_pane.frames);
        EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
        EMBED_CALL(cli_debug_pane_internal.running_said, CliDebugPaneCtx, .root = args->root);
        changed = 1;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "exited", .cap = 32ULL}))
    {
        EMBED_CALL(pane_exited, DebugPaneCtx, .json = &json, .node = body);
        changed = 1;
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "terminated", .cap = 32ULL}) ||
              cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "adapterStopped", .cap = 32ULL})) &&
             (follows || followed))
    {
        EMBED_CALL(cli_debug_pane_internal.ended, CliDebugPaneCtx, .text = PANE_ENDED,
                   .length = sizeof(PANE_ENDED) - 1ULL);
        changed = 1;
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
    EMBED_CALL(cli_debug_pane_internal.bar_settle, CliDebugPaneCtx, .root = args->root);
    return changed;
}

// an event a session's adapter reports, stored under the lock from the adapter's thread, the view's thread woken
static void pane_emitted(const CliDebugEventCfg *args)
{
    EMBED_CALL(pane_locked, DebugPaneCtx, .session = args->session);
    CliDebugPaneStore *const events = &g_cli_debug_pane.events;
    CliDebugPaneEntry *const entry = EMBED_CALL(cli_debug_pane_internal.entry_put, CliDebugPaneCtx, .store = events);
    const char *const event = (args->event != NULL) ? args->event : "";
    const unsigned long long name =
        (entry != NULL) ? EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = events, .text = event,
                                     .length = cellul.len(&(CatenaFinitaCfg){.src = event, .cap = 64ULL}))
                        : CLI_DEBUG_PANE_NONE;
    const unsigned long long body = (name != CLI_DEBUG_PANE_NONE)
                                        ? EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = events,
                                                     .text = (args->body != NULL) ? args->body : "",
                                                     .length = (args->body != NULL) ? args->size : 0ULL)
                                        : CLI_DEBUG_PANE_NONE;
    if (body == CLI_DEBUG_PANE_NONE)
    {
        // an event the cells cannot meet is passed over whole
        events->count -= (entry != NULL) ? 1u : 0u;
    }
    else
    {
        // the session's number is far inside what a signed reference counts
        entry->reference = (long long)args->session;
        entry->text = name;
        entry->second = body;
        entry->second_length = (args->body != NULL) ? args->size : 0ULL;
    }
    EMBED_ATOMIC_STORE(&g_cli_debug_pane.waiting, 1u);
    EMBED_CALL(pane_unlocked, DebugPaneCtx, .session = args->session);
    if (g_cli_debug_pane.woken != NULL)
    {
        g_cli_debug_pane.woken();
    }
}

void cli_debug_pane_begin(const CliDebugPaneViewCfg *args)
{
    CliDebugPaneStore *const stores[6] = {&g_cli_debug_pane.frames,  &g_cli_debug_pane.variables,
                                          &g_cli_debug_pane.console, &g_cli_debug_pane.watches,
                                          &g_cli_debug_pane.flipped, &g_cli_debug_pane.events};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = stores[at]);
    }
    g_cli_guard->persistent_buf_release(g_cli_debug_pane.watch_typed.bytes, g_cli_debug_pane_owner);
    g_cli_guard->persistent_buf_release(g_cli_debug_pane.console_typed.bytes, g_cli_debug_pane_owner);
    g_cli_guard->persistent_buf_release(g_cli_debug_pane.session_texts, g_cli_debug_pane_owner);
    g_cli_debug_pane.watch_typed = (CliTyped){NULL, 0ULL};
    g_cli_debug_pane.console_typed = (CliTyped){NULL, 0ULL};
    g_cli_debug_pane.session_texts = NULL;
    g_cli_debug_pane.view = CLI_DEBUG_VIEW_FRAMES;
    g_cli_debug_pane.has_session = 0;
    g_cli_debug_pane.paused = 0;
    g_cli_debug_pane.has_thread = 0;
    g_cli_debug_pane.step_back = 0;
    g_cli_debug_pane.frame_at = 0u;
    g_cli_debug_pane.woken = args->woken;
    g_cli_debug_pane.requests = (args->requests != NULL) ? args->requests : &cli_debug_pane_requests;
    EMBED_ATOMIC_STORE(&g_cli_debug_pane.waiting, 0u);
    EMBED_ATOMIC_STORE(&g_cli_debug_pane.lock, 0u);
    EMBED_CALL(cli_debug_pane_internal.status_set, CliDebugPaneCtx, .text = PANE_OPENING,
               .length = sizeof(PANE_OPENING) - 1ULL);
    EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
    EMBED_CALL(cli_debug_pane_internal.bar_settle, CliDebugPaneCtx, .root = args->root);
}

CliDebugSink cli_debug_pane_sink(const CliDebugPaneViewCfg *args)
{
    (void)args;
    return (CliDebugSink){pane_emitted, &g_cli_debug_pane};
}

int cli_debug_pane_step(const CliDebugPaneViewCfg *args)
{
    if (EMBED_ATOMIC_LOAD(&g_cli_debug_pane.waiting) == 0u)
    {
        return 0;
    }
    // the events stored taken whole under the lock, the sink storing anew into an empty store
    EMBED_CALL(pane_locked, DebugPaneCtx, .root = args->root);
    const CliDebugPaneStore events = g_cli_debug_pane.events;
    g_cli_debug_pane.events = (CliDebugPaneStore){0};
    EMBED_ATOMIC_STORE(&g_cli_debug_pane.waiting, 0u);
    EMBED_CALL(pane_unlocked, DebugPaneCtx, .root = args->root);
    int changed = 0;
    for (unsigned int at = 0u; at < events.count; at += 1u)
    {
        const CliDebugPaneEntry *const entry = &events.entry[at];
        // a session's number stored is not negative
        changed = EMBED_CALL(pane_event, DebugPaneCtx, .session = (unsigned long long)entry->reference,
                             .event = events.text + entry->text, .body = events.text + entry->second,
                             .size = entry->second_length, .root = args->root) ||
                  changed;
    }
    g_cli_guard->persistent_buf_release(events.entry, g_cli_debug_pane_owner);
    g_cli_guard->persistent_buf_release(events.text, g_cli_debug_pane_owner);
    return changed;
}

unsigned long long cli_debug_pane_deadline_ms(const CliDebugPaneViewCfg *args)
{
    return (EMBED_ATOMIC_LOAD(&g_cli_debug_pane.waiting) != 0u) ? args->now_ms : 0ULL;
}

unsigned int cli_debug_pane_shown(const CliDebugPaneViewCfg *args)
{
    (void)args;
    return g_cli_debug_pane.view;
}

void cli_debug_pane_view_set(const CliDebugPaneViewCfg *args)
{
    if (args->view < CLI_DEBUG_VIEWS)
    {
        g_cli_debug_pane.view = args->view;
    }
}

const char *cli_debug_pane_label(const CliDebugPaneViewCfg *args)
{
    const char *const label = (args->view < CLI_DEBUG_VIEWS) ? s_labels[args->view] : "";
    *args->length_out = cellul.len(&(CatenaFinitaCfg){.src = label, .cap = 16ULL});
    return label;
}
