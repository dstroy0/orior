// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_pane_bar.c: the Debug pane's bar and field. The bar's steps, each with the state it acts in: Continue and the
// steps forward while the session followed is stopped, Pause while it runs, the steps backward while it is stopped and
// steps back, and shown only for a session that steps back, Restart once a start is remembered, Stop while a session
// is followed; each run on the stopped thread, a Python session's Step Into through the step across. The field takes
// a watch's expression in Variables and an expression to evaluate in the chosen frame in Console
#include "debug_internal.h"
#include "debug_pane_internal.h"

// what a step needs to act: the session stopped, running, stopped and stepping back, a start remembered, or followed
#define BAR_PAUSED 0u
#define BAR_RUNNING 1u
#define BAR_BACK 2u
#define BAR_LAST 3u
#define BAR_SESSION 4u

// the bytes of a scratch an evaluation's answer is written into, the sentence of an evaluation with no session, and
// the placeholders of the fields
#define BAR_ANSWER (1ULL << 20u)
#define BAR_NONE "Nothing is being debugged.\n"
#define BAR_WATCH "Add a watch: an expression, then Enter"
#define BAR_EVALUATE "Evaluate in the chosen frame, then Enter"
#define BAR_ENDED "Ended"

// A step: its command as the catalog lists it, its label, its keys, what it needs to act, and the request it sends,
// NULL for Restart and Stop
typedef struct
{
    const char *command;
    const char *label;
    const char *keys;
    unsigned int need;
    const char *how;
} BarStep;

static const BarStep s_steps[CLI_DEBUG_STEPS] = {
    {"continue", "Continue", "F5", BAR_PAUSED, "continue"},
    {"pause", "Pause", "F6", BAR_RUNNING, "pause"},
    {"step-over", "Step Over", "F10", BAR_PAUSED, "next"},
    {"step-into", "Step Into", "F11, F7", BAR_PAUSED, "stepIn"},
    {"step-out", "Step Out", "Shift+F11", BAR_PAUSED, "stepOut"},
    {"step-back", "Step Back", "", BAR_BACK, "stepBack"},
    {"reverse-continue", "Run Backward", "", BAR_BACK, "reverseContinue"},
    {"restart-debug", "Restart", "Ctrl+Shift+F5", BAR_LAST, NULL},
    {"stop-debug", "Stop", "Shift+F5, Ctrl+F2", BAR_SESSION, NULL},
};

typedef struct
{
    const char *text;
    unsigned long long length;
    const char *root;
} DebugPaneBarCtx;

void cli_debug_pane_bar_settle(const CliDebugPaneCtx *args)
{
    (void)args;
    const int session = g_cli_debug_pane.has_session;
    const int paused = session && g_cli_debug_pane.paused;
    const int back = paused && g_cli_debug_pane.step_back;
    for (unsigned int at = 0u; at < CLI_DEBUG_STEPS; at += 1u)
    {
        int enabled = 0;
        switch (s_steps[at].need)
        {
        case BAR_PAUSED:
            enabled = paused;
            break;
        case BAR_RUNNING:
            enabled = session && !paused;
            break;
        case BAR_BACK:
            enabled = back;
            break;
        case BAR_SESSION:
            enabled = session;
            break;
        default:
            enabled = 0;
            break;
        }
        g_cli_debug_pane.enabled[at] = enabled ? 1u : 0u;
        // the steps backward are shown only for a session that takes them
        g_cli_debug_pane.shown[at] =
            ((s_steps[at].need != BAR_BACK) || (session && g_cli_debug_pane.step_back)) ? 1u : 0u;
    }
}

int cli_debug_pane_item(const CliDebugPaneBarCfg *args)
{
    if (args->at >= CLI_DEBUG_STEPS)
    {
        return 0;
    }
    const BarStep *const step = &s_steps[args->at];
    *args->item = (CliDebugBarItem){step->command,
                                    step->label,
                                    cellul.len(&(CatenaFinitaCfg){.src = step->label, .cap = 32ULL}),
                                    step->command,
                                    step->keys,
                                    g_cli_debug_pane.enabled[args->at],
                                    g_cli_debug_pane.shown[args->at]};
    return 1;
}

// the request's refusal, `length` bytes of `text`, written to the console as an error line
EMBED_INLINE void bar_refused(const DebugPaneBarCtx *args)
{
    EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = args->text, .length = args->length,
               .type = CLI_DEBUG_LINE_STDERR);
    EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = "\n", .length = 1ULL,
               .type = CLI_DEBUG_LINE_STDERR);
}

// whether the session followed is a Python one, whose Step Into goes through the step across
EMBED_INLINE int bar_python(const DebugPaneBarCtx *args)
{
    (void)args;
    return (g_cli_debug_pane.session_texts != NULL) && (g_cli_debug_pane.language_length == 6ULL) &&
           (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = g_cli_debug_pane.session_texts,
                                                                 .offset = g_cli_debug_pane.language_at,
                                                                 .src = "python",
                                                                 .bytes = 7ULL}) == 0);
}

int cli_debug_pane_run(const CliDebugPaneBarCfg *args)
{
    if ((args->at >= CLI_DEBUG_STEPS) || !g_cli_debug_pane.enabled[args->at])
    {
        return 0;
    }
    const BarStep *const step = &s_steps[args->at];
    if (args->at == CLI_DEBUG_STEP_STOP)
    {
        g_cli_debug_pane.requests->stop(&(CliDebugCfg){.id = g_cli_debug_pane.session});
        EMBED_CALL(cli_debug_pane_internal.ended, CliDebugPaneCtx, .text = BAR_ENDED,
                   .length = sizeof(BAR_ENDED) - 1ULL, .root = args->root);
        EMBED_CALL(cli_debug_pane_internal.bar_settle, CliDebugPaneCtx, .root = args->root);
        return 1;
    }
    if (step->how == NULL)
    {
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    char answer[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea out = {answer, sizeof(answer), 0ULL};
    if (!g_cli_debug_pane.has_thread && (args->at == CLI_DEBUG_STEP_PAUSE))
    {
        // a program that has not stopped yet is paused on the first thread the adapter lists
        char *const scratch = g_cli_guard->temporary_buf_alloc(BAR_ANSWER, g_cli_debug_pane_owner);
        VerbaLinea threads_out = {scratch, (scratch != NULL) ? BAR_ANSWER : 0ULL, 0ULL};
        CliJson json = {0};
        const unsigned int threads =
            ((scratch != NULL) &&
             g_cli_debug_pane.requests->threads(&(CliDebugOutErrMsgIdStepCfg){
                 .out = &threads_out, .err_msg = &err_msg, .id = g_cli_debug_pane.session}) &&
             mmgr_line_ok(&threads_out))
                ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                             .text = scratch, .size = threads_out.at, .err_msg = &err_msg,
                             .owner = g_cli_debug_pane_owner)
                : CLI_JSON_NONE;
        long long thread = 0LL;
        g_cli_debug_pane.has_thread =
            (threads != CLI_JSON_NONE) &&
            EMBED_CALL(
                cli_debug_json_node_internal.debug_sign, CliDebugJsonNodeCtx, .json = &json,
                .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                   .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                        .json_read = &json, .array = threads, .index = 0u),
                                   .name = "id"),
                .value = &thread);
        g_cli_debug_pane.thread = thread;
        g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
        g_cli_guard->persistent_buf_release(scratch, g_cli_debug_pane_owner);
        if (!g_cli_debug_pane.has_thread)
        {
            return 0;
        }
    }
    const int across = (args->at == CLI_DEBUG_STEP_INTO) && EMBED_CALL(bar_python, DebugPaneBarCtx, .root = args->root);
    const int stepped =
        across ? g_cli_debug_pane.requests->step_across(&(CliDebugRootOutErrMsgCfg){.root = args->root,
                                                                                    .out = &out,
                                                                                    .err_msg = &err_msg,
                                                                                    .id = g_cli_debug_pane.session,
                                                                                    .thread = g_cli_debug_pane.thread})
               : g_cli_debug_pane.requests->step(&(CliDebugOutErrMsgIdThreadHowCfg){.out = &out,
                                                                                    .err_msg = &err_msg,
                                                                                    .id = g_cli_debug_pane.session,
                                                                                    .thread = g_cli_debug_pane.thread,
                                                                                    .how = step->how});
    if (!stepped)
    {
        EMBED_CALL(bar_refused, DebugPaneBarCtx, .text = why, .length = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return 0;
    }
    if (args->at != CLI_DEBUG_STEP_PAUSE)
    {
        // the program runs on: its frames go until it stops again
        g_cli_debug_pane.paused = 0;
        EMBED_CALL(cli_debug_pane_internal.store_empty, CliDebugPaneCtx, .store = &g_cli_debug_pane.frames);
        EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
        EMBED_CALL(cli_debug_pane_internal.running_said, CliDebugPaneCtx, .root = args->root);
    }
    EMBED_CALL(cli_debug_pane_internal.bar_settle, CliDebugPaneCtx, .root = args->root);
    return 1;
}

void cli_debug_pane_status(const CliDebugPaneBarCfg *args)
{
    *args->status_out =
        (CliDebugPaneStatus){g_cli_debug_pane.status, g_cli_debug_pane.status_length, g_cli_debug_pane.status_failed};
}

// the field's text of the view shown, NULL where the view has no field
EMBED_INLINE CliTyped *bar_typed(const DebugPaneBarCtx *args)
{
    (void)args;
    switch (g_cli_debug_pane.view)
    {
    case CLI_DEBUG_VIEW_VARIABLES:
        return &g_cli_debug_pane.watch_typed;
    case CLI_DEBUG_VIEW_CONSOLE:
        return &g_cli_debug_pane.console_typed;
    default:
        return NULL;
    }
}

void cli_debug_pane_field(const CliDebugPaneInputCfg *args)
{
    const CliTyped *const typed = EMBED_CALL(bar_typed, DebugPaneBarCtx, .root = args->root);
    const char *const placeholder = (g_cli_debug_pane.view == CLI_DEBUG_VIEW_VARIABLES) ? BAR_WATCH
                                    : (typed != NULL)                                   ? BAR_EVALUATE
                                                                                        : "";
    // the field's text copied out of the typed text's cell into the bar's own buffer, which the caller reads before the
    // next field read
    static char s_field_text[CLI_DEBUG_FIELD_MAX];
    const unsigned long long length = (typed != NULL)
                                          ? EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed_read = typed,
                                                       .dst = s_field_text, .capacity = sizeof(s_field_text))
                                          : 0ULL;
    s_field_text[length] = '\0';
    *args->field_out =
        (CliDebugPaneField){s_field_text, length, placeholder,
                            cellul.len(&(CatenaFinitaCfg){.src = placeholder, .cap = 64ULL}), typed != NULL};
}

void cli_debug_pane_type(const CliDebugPaneInputCfg *args)
{
    CliTyped *const typed = EMBED_CALL(bar_typed, DebugPaneBarCtx, .root = args->root);
    if (typed == NULL)
    {
        return;
    }
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = typed, .text = args->text, .size = args->size,
               .most = CLI_DEBUG_FIELD_MAX, .owner = g_cli_debug_pane_owner);
}

// an expression evaluated in the chosen frame, or with no frame where the session runs, written to the console: the
// expression as an input line, then its value as a result line or the refusal as an error line, the variables read
// again where the session is stopped
EMBED_INLINE void bar_evaluated(const DebugPaneBarCtx *args)
{
    EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = "> ", .length = 2ULL,
               .type = CLI_DEBUG_LINE_INPUT);
    EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = args->text, .length = args->length,
               .type = CLI_DEBUG_LINE_INPUT);
    EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = "\n", .length = 1ULL,
               .type = CLI_DEBUG_LINE_INPUT);
    if (!g_cli_debug_pane.has_session)
    {
        EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = BAR_NONE,
                   .length = sizeof(BAR_NONE) - 1ULL, .type = CLI_DEBUG_LINE_STDERR);
        return;
    }
    CliDebugPaneStore *const frames = &g_cli_debug_pane.frames;
    const int framed = g_cli_debug_pane.paused && (g_cli_debug_pane.frame_at < frames->count);
    char *const scratch = g_cli_guard->temporary_buf_alloc(BAR_ANSWER, g_cli_debug_pane_owner);
    if (scratch == NULL)
    {
        return;
    }
    VerbaLinea out = {scratch, BAR_ANSWER, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliDebugPaneEntry chosen = {0};
    if (framed)
    {
        EMBED_CALL(cli_debug_pane_internal.entry_read, CliDebugPaneCtx, .store = frames,
                   .at = g_cli_debug_pane.frame_at, .entry_out = &chosen);
    }
    const int evaluated =
        g_cli_debug_pane.requests->evaluate(&(CliDebugOutErrMsgCfg){.out = &out,
                                                                    .err_msg = &err_msg,
                                                                    .id = g_cli_debug_pane.session,
                                                                    .frame = framed ? chosen.reference : 0LL,
                                                                    .expression = args->text,
                                                                    .has_frame = framed,
                                                                    .context_text = "repl"});
    CliJson json = {0};
    const unsigned int root =
        (evaluated && mmgr_line_ok(&out))
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                         .text = scratch, .size = out.at, .err_msg = &err_msg, .owner = g_cli_debug_pane_owner)
            : CLI_JSON_NONE;
    unsigned long long value_length = 0ULL;
    const char *const value =
        (root != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "value"),
                         .length_pointer = &value_length)
            : NULL;
    if (value != NULL)
    {
        EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = value, .length = value_length,
                   .type = CLI_DEBUG_LINE_RESULT);
        EMBED_CALL(cli_debug_pane_internal.line_write, CliDebugPaneCtx, .text = "\n", .length = 1ULL,
                   .type = CLI_DEBUG_LINE_RESULT);
    }
    else
    {
        EMBED_CALL(bar_refused, DebugPaneBarCtx, .text = why, .length = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
    }
    g_cli_guard->persistent_buf_release(json.node, g_cli_debug_pane_owner);
    g_cli_guard->persistent_buf_release(scratch, g_cli_debug_pane_owner);
    if (g_cli_debug_pane.paused)
    {
        EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
    }
}

// the field's text, its blanks at either end let go, put as a watch after the watches, the variables read again
EMBED_INLINE void bar_watched(const DebugPaneBarCtx *args)
{
    CliDebugPaneStore *const watches = &g_cli_debug_pane.watches;
    const unsigned int at = EMBED_CALL(cli_debug_pane_internal.entry_put, CliDebugPaneCtx, .store = watches);
    if (at == CLI_DEBUG_PANE_NO_ENTRY)
    {
        return;
    }
    const unsigned long long text = EMBED_CALL(cli_debug_pane_internal.text_put, CliDebugPaneCtx, .store = watches,
                                               .text = args->text, .length = args->length);
    if (text == CLI_DEBUG_PANE_NONE)
    {
        watches->count -= 1u;
        return;
    }
    CliDebugPaneEntry entry = {0};
    entry.type = CLI_DEBUG_ROW_WATCH;
    entry.text = text;
    entry.text_length = args->length;
    EMBED_CALL(cli_debug_pane_internal.entry_written, CliDebugPaneCtx, .store = watches, .at = at, .entry = &entry);
    EMBED_CALL(cli_debug_pane_internal.variables_read, CliDebugPaneCtx, .root = args->root);
}

int cli_debug_pane_key(const CliDebugPaneInputCfg *args)
{
    CliTyped *const typed = EMBED_CALL(bar_typed, DebugPaneBarCtx, .root = args->root);
    if (typed == NULL)
    {
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = typed, .owner = g_cli_debug_pane_owner);
        return 1;
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Enter", .cap = 16ULL}))
    {
        return 0;
    }
    // the text copied out of the typed text's cell, with its blanks at either end let go, and its end after it
    char bytes[CLI_DEBUG_FIELD_MAX];
    unsigned long long first = 0ULL;
    unsigned long long last =
        EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = typed, .dst = bytes, .capacity = sizeof(bytes));
    while ((first < last) && ((bytes[first] == ' ') || (bytes[first] == '\t')))
    {
        first += 1ULL;
    }
    while ((last > first) && ((bytes[last - 1ULL] == ' ') || (bytes[last - 1ULL] == '\t')))
    {
        last -= 1ULL;
    }
    if (last == first)
    {
        return 1;
    }
    char expression[CLI_DEBUG_FIELD_MAX];
    const unsigned long long length = last - first;
    memor.cpy(&(MemoriaCfg){.dst = expression, .src = bytes + first, .bytes = length});
    expression[length] = '\0';
    EMBED_CALL(cli_typed.release, CliTypedCfg, .typed = typed, .owner = g_cli_debug_pane_owner);
    if (g_cli_debug_pane.view == CLI_DEBUG_VIEW_VARIABLES)
    {
        EMBED_CALL(bar_watched, DebugPaneBarCtx, .text = expression, .length = length, .root = args->root);
    }
    else
    {
        EMBED_CALL(bar_evaluated, DebugPaneBarCtx, .text = expression, .length = length, .root = args->root);
    }
    return 1;
}
