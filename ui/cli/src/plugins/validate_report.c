// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_report.c: what the checks found, reported: each barrier the first check stopped at and what lifting it came
// to, the mathematics the last check found wrong, the flags, the axioms each theorem rests on past those trusted, and
// the verdict
#include "validate_internal.h"

#include "../text/text.h"

static CliFinding s_sorting[CLI_VALIDATE_FINDINGS];

void cli_validate_finding_append(const CliValidateLengthCtx *args)
{
    const char *message = args->message_text;
    const char *lifted = args->lifted;
    const unsigned long long lifted_size =
        (lifted != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = lifted, .cap = CLI_POOL_BYTES}) + 1ULL : 0ULL;
    if ((g_cli_validate->finding_count == CLI_VALIDATE_FINDINGS) ||
        (g_cli_validate->finding_used + args->length + 1ULL + lifted_size > CLI_VALIDATE_FINDING_TEXT))
    {
        return;
    }
    CliFinding *const put = &g_cli_validate->findings[g_cli_validate->finding_count];
    *put = *args->finding;
    put->message = g_cli_validate->finding_used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_validate->finding_text + put->message, .src = message, .bytes = args->length});
    g_cli_validate->finding_text[put->message + args->length] = '\0';
    g_cli_validate->finding_used += args->length + 1ULL;
    put->lifted = CLI_VALIDATE_NONE;
    if (lifted != NULL)
    {
        put->lifted = g_cli_validate->finding_used;
        memor.cpy(
            &(MemoriaCfg){.dst = g_cli_validate->finding_text + put->lifted, .src = lifted, .bytes = lifted_size});
        g_cli_validate->finding_used += lifted_size;
    }
    g_cli_validate->finding_count += 1u;
}

typedef struct
{
    VerbaLinea *out;
    unsigned int key;
    unsigned int from;
    unsigned int to;
    unsigned long long count;
    const char *one;
    const char *many;
    const char *after;
    int passed;
    unsigned long long math;
    unsigned long long blocked;
    const VerbaLinea *untrusted;
    const CliFinding *finding;
} ValidateReportCtx;
// a limit written as `<name> <value>`, nothing where it was not grown
EMBED_INLINE void report_setting(const ValidateReportCtx *args)
{
    if (g_cli_validate->has_grown[args->key])
    {
        mmgr_line_put(args->out, g_cli_validate->tool.barriers[args->key].name);
        mmgr_line_ch(args->out, ' ');
        mmgr_line_decimal(args->out, g_cli_validate->grown[args->key], 1u);
    }
}

// the barriers the first check stopped at, each a finding with what lifting it came to
static void report_barriers(void)
{
    const CliProverMessages *const first = &g_cli_validate->first;
    const CliProverMessages *const last = &g_cli_validate->last;
    const CliTool *const tool = &g_cli_validate->tool;
    for (unsigned int at = 0u; at < first->count; at += 1u)
    {
        const CliProverMessage *const message = &first->message[at];
        const int barrier = EMBED_CALL(cli_validate_messages_internal.barrier_get, CliValidateMessagesCtx,
                                       .messages_read = first, .message = message);
        if ((message->severity != 1u) || (message->line >= g_cli_validate->lines) || (barrier < 0))
        {
            continue;
        }
        const CliBarrier *const stopping = &tool->barriers[barrier];
        const unsigned int key =
            EMBED_CALL(cli_validate_at_internal.key_get, CliValidateAtCtx, .at = (unsigned int)barrier);
        int still = 0;
        for (unsigned int now = 0u; !still && (now < last->count); now += 1u)
        {
            const CliProverMessage *const again = &last->message[now];
            const int again_barrier = EMBED_CALL(cli_validate_messages_internal.barrier_get, CliValidateMessagesCtx,
                                                 .messages_read = last, .message = again);
            still = (again->severity == 1u) && (again->line == message->line) && (again_barrier >= 0) &&
                    (cellul.eq(&(CatenaFinitaCfg){
                        .src = tool->barriers[again_barrier].name, .other = stopping->name, .cap = CLI_POOL_BYTES}));
        }
        char lifted[1024];
        VerbaLinea lifted_line = {lifted, sizeof(lifted), 0ULL};
        if (g_cli_validate->stuck[key])
        {
            mmgr_line_put(&lifted_line, "still stops the check at its ceiling, ");
            mmgr_line_decimal(&lifted_line, g_cli_validate->stuck_at[key], 1u);
        }
        else if (still)
        {
            mmgr_line_put(&lifted_line, "still stops the check once the other limits are lifted");
        }
        else
        {
            g_cli_validate->lifted[key] = 1;
            mmgr_line_put(&lifted_line, "goes through at ");
            EMBED_CALL(report_setting, ValidateReportCtx, .out = &lifted_line, .key = key);
        }
        mmgr_line_finish(&lifted_line);
        const char *const text = first->text + message->text;
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = 0ULL, .size = message->text_size);
        unsigned long long first_line = bounds.stop;
        first_line -= bounds.stop - bounds.cut;
        char msg_buf[8192];
        VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ULL};
        mmgr_line_put(&msg, stopping->name);
        mmgr_line_put(&msg, ": ");
        mmgr_line_put(&msg, stopping->about);
        mmgr_line_put(&msg, " stopped the check here: ");
        mmgr_line_put_n(&msg, text, first_line);
        const CliFinding finding = {
            message->line, message->col, message->end_line, message->end_col, still ? 1u : 3u, "barrier", barrier,
            0ULL,          0ULL};
        EMBED_CALL(cli_validate_length_internal.finding_append, CliValidateLengthCtx, .finding = &finding,
                   .message_text = msg_buf, .length = msg.at, .lifted = lifted);
    }
}

// the mathematics a message of the types the audit specifies better goes taken out, the audit naming the gap with what
// fills it
static void report_repeats(void)
{
    const CliProverMessages *const last = &g_cli_validate->last;
    const CliJson *const json = &g_cli_validate->json;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        const CliFinding *const finding = &g_cli_validate->findings[at];
        int repeated = 0;
        for (unsigned int now = 0u;
             !repeated &&
             (cellul.eq(&(CatenaFinitaCfg){.src = finding->type, .other = "math", .cap = CLI_POOL_BYTES})) &&
             (now < last->count);
             now += 1u)
        {
            const CliProverMessage *const message = &last->message[now];
            repeated = (message->line == finding->line) && (message->col == finding->col) &&
                       EMBED_CALL(cli_validate_json_internal.in_array, CliValidateJsonCtx, .json = json,
                                  .array = g_cli_validate->tool.repeats, .text = last->text + message->type,
                                  .length = message->type_size);
        }
        if (!repeated)
        {
            g_cli_validate->findings[count] = *finding;
            count += 1u;
        }
    }
    g_cli_validate->finding_count = count;
}

// the findings in the order of their places, as a stable sort orders them, from `from` to `to`
static void report_sort(const ValidateReportCtx *args)
{
    if (args->to - args->from < 2u)
    {
        return;
    }
    const unsigned int middle = args->from + ((args->to - args->from) / 2u);
    EMBED_CALL(report_sort, ValidateReportCtx, .from = args->from, .to = middle);
    EMBED_CALL(report_sort, ValidateReportCtx, .from = middle, .to = args->to);
    CliFinding *const found = g_cli_validate->findings;
    unsigned int left = args->from;
    unsigned int right = middle;
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        const int take_right =
            (right < args->to) && ((left == middle) || (found[right].line < found[left].line) ||
                                   ((found[right].line == found[left].line) && (found[right].col < found[left].col)));
        s_sorting[at] = take_right ? found[right] : found[left];
        right += take_right ? 1u : 0u;
        left += take_right ? 0u : 1u;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = found + args->from, .src = s_sorting + args->from, .bytes = (args->to - args->from) * sizeof(found[0])});
}

// a count and its noun, `one` where it is 1 and `many` otherwise, a "; " before it where the verdict reports something
EMBED_INLINE void report_on_count(const ValidateReportCtx *args)
{
    mmgr_line_put(args->out, (args->out->at > 0ULL) ? "; " : "");
    mmgr_line_decimal(args->out, args->count, 1u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_put(args->out, (args->count == 1ULL) ? args->one : args->many);
    mmgr_line_put(args->out, args->after);
}

// the verdict: whether the check ran out of time or passes, the errors, the places a limit stops, the limits lifted,
// the file's own settings changed and the theorems that rest on axioms past those trusted
EMBED_INLINE void report_verdict(const ValidateReportCtx *args)
{
    const CliTool *const tool = &g_cli_validate->tool;
    const CliJson *const json = &g_cli_validate->json;
    if (g_cli_validate->late)
    {
        mmgr_line_put(args->out, "the check ran past ");
        mmgr_line_decimal(args->out, tool->patience, 1u);
        mmgr_line_put(args->out, " seconds and was stopped");
    }
    else if (args->passed && (g_cli_validate->theorem_count == 0u))
    {
        mmgr_line_put(args->out, "it checks");
    }
    else if (args->passed)
    {
        mmgr_line_decimal(args->out, g_cli_validate->theorem_count, 1u);
        mmgr_line_put(args->out, " theorems check");
    }
    if (args->math > 0ULL)
    {
        EMBED_CALL(report_on_count, ValidateReportCtx, .out = args->out, .count = args->math, .one = "error",
                   .many = "errors", .after = " in the mathematics");
    }
    if (args->blocked > 0ULL)
    {
        EMBED_CALL(report_on_count, ValidateReportCtx, .out = args->out, .count = args->blocked, .one = "place",
                   .many = "places", .after = " a limit stops past its ceiling");
    }
    unsigned int keys[CLI_VALIDATE_BARRIERS];
    unsigned int lifted = 0u;
    for (unsigned int key = 0u; key < tool->barrier_count; key += 1u)
    {
        if (!g_cli_validate->lifted[key])
        {
            continue;
        }
        unsigned int place = lifted;
        while ((place > 0u) &&
               (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = tool->barriers[keys[place - 1u]].name,
                           .other_pointer = tool->barriers[key].name) > 0))
        {
            keys[place] = keys[place - 1u];
            place -= 1u;
        }
        keys[place] = key;
        lifted += 1u;
    }
    if (lifted > 0u)
    {
        mmgr_line_put(args->out, (args->out->at > 0ULL) ? "; once lifted to " : "once lifted to ");
        int printed = 0;
        for (unsigned int at = 0u; at < lifted; at += 1u)
        {
            if (g_cli_validate->has_grown[keys[at]])
            {
                mmgr_line_put(args->out, printed ? ", " : "");
                EMBED_CALL(report_setting, ValidateReportCtx, .out = args->out, .key = keys[at]);
                printed = 1;
            }
        }
    }
    if (g_cli_validate->own_size > 0ULL)
    {
        mmgr_line_put(args->out, (args->out->at > 0ULL) ? "; the file's own " : "the file's own ");
        mmgr_line_put_n(args->out, g_cli_validate->own, g_cli_validate->own_size);
    }
    if (args->untrusted->at > 0ULL)
    {
        mmgr_line_put(args->out, (args->out->at > 0ULL) ? "; " : "");
        mmgr_line_put_n(args->out, args->untrusted->out, args->untrusted->at);
        mmgr_line_put(args->out, " rest on axioms past ");
        for (unsigned int index = 0u;
             EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                        .array = tool->trusted, .index = index) != CLI_JSON_NONE;
             index += 1u)
        {
            const char *const axiom =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = tool->trusted, .index = index),
                           .length_pointer = NULL);
            mmgr_line_put(args->out, (index > 0u) ? ", " : "");
            mmgr_line_put(args->out, axiom);
        }
    }
}

// a finding written as `{"line", "col", "end_line", "end_col", "severity", "type", "barrier", "message", "lifted"}`
EMBED_INLINE void report_finding_write(const ValidateReportCtx *args)
{
    VerbaLinea *out = args->out;
    static const char *const keys[4] = {"{\"line\":", ",\"col\":", ",\"end_line\":", ",\"end_col\":"};
    const unsigned int positions[4] = {args->finding->line, args->finding->col, args->finding->end_line,
                                       args->finding->end_col};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        mmgr_line_put(out, keys[at]);
        mmgr_line_decimal(out, positions[at], 1u);
    }
    mmgr_line_put(out, ",\"severity\":");
    mmgr_line_decimal(out, args->finding->severity, 1u);
    mmgr_line_put(out, ",\"kind\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->finding->type,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->finding->type, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(out, ",\"barrier\":");
    if (args->finding->barrier < 0)
    {
        mmgr_line_put(out, "null");
    }
    else
    {
        const char *const name = g_cli_validate->tool.barriers[args->finding->barrier].name;
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    }
    const char *const message = g_cli_validate->finding_text + args->finding->message;
    mmgr_line_put(out, ",\"message\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = message,
               .length = cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(out, ",\"lifted\":");
    if (args->finding->lifted == CLI_VALIDATE_NONE)
    {
        mmgr_line_put(out, "null");
    }
    else
    {
        const char *const lifted = g_cli_validate->finding_text + args->finding->lifted;
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = lifted,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = lifted, .cap = CLI_POOL_BYTES}));
    }
    mmgr_line_ch(out, '}');
}

void cli_validate_report(const CliValidateProjectCtx *args)
{
    static char s_untrusted[1u << 16u];
    VerbaLinea untrusted = {s_untrusted, sizeof(s_untrusted), 0ULL};
    report_barriers();
    const unsigned int answers = cli_validate_mathematics();
    cli_validate_flags_in();
    if (g_cli_validate->tool.audited)
    {
        report_repeats();
        EMBED_CALL(cli_validate_answers_internal.axioms_report, CliValidateAnswersCtx, .answers = answers,
                   .untrusted = &untrusted);
    }
    EMBED_CALL(report_sort, ValidateReportCtx, .from = 0u, .to = g_cli_validate->finding_count);
    unsigned long long math = 0ULL;
    unsigned long long blocked = 0ULL;
    unsigned long long gaps = 0ULL;
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        const CliFinding *const finding = &g_cli_validate->findings[at];
        const int error = (finding->severity == 1u);
        math += (error && (cellul.eq(&(CatenaFinitaCfg){.src = finding->type, .other = "math", .cap = CLI_POOL_BYTES})))
                    ? 1ULL
                    : 0ULL;
        blocked +=
            (error && (cellul.eq(&(CatenaFinitaCfg){.src = finding->type, .other = "barrier", .cap = CLI_POOL_BYTES})))
                ? 1ULL
                : 0ULL;
        gaps +=
            (error && ((cellul.eq(&(CatenaFinitaCfg){.src = finding->type, .other = "flag", .cap = CLI_POOL_BYTES})) ||
                       (cellul.eq(&(CatenaFinitaCfg){.src = finding->type, .other = "axiom", .cap = CLI_POOL_BYTES}))))
                ? 1ULL
                : 0ULL;
    }
    const int passed = !g_cli_validate->late && (math == 0ULL) && (blocked == 0ULL) && (gaps == 0ULL);
    static char s_verdict[1u << 18u];
    VerbaLinea verdict = {s_verdict, sizeof(s_verdict), 0ULL};
    EMBED_CALL(report_verdict, ValidateReportCtx, .out = &verdict, .passed = passed, .math = math, .blocked = blocked,
               .untrusted = &untrusted);
    const char *const name = g_cli_validate->tool.name;
    mmgr_line_put(args->report, "{\"tool\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->report, .text = name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->report, ",\"file\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->report, .text = args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->report, ",\"project\":");
    if (args->project == NULL)
    {
        mmgr_line_put(args->report, "null");
    }
    else
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->report,
                   .text = args->project,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->project, .cap = CLI_POOL_BYTES}));
    }
    mmgr_line_put(args->report, passed ? ",\"passes\":true,\"verdict\":" : ",\"passes\":false,\"verdict\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->report, .text = verdict.out,
               .length = verdict.at);
    mmgr_line_put(args->report, ",\"checks\":[");
    mmgr_line_put_n(args->report, g_cli_validate->checks, g_cli_validate->checks_size);
    mmgr_line_put(args->report, "],\"findings\":[");
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        mmgr_line_put(args->report, (at > 0u) ? "," : "");
        EMBED_CALL(report_finding_write, ValidateReportCtx, .finding = &g_cli_validate->findings[at],
                   .out = args->report);
    }
    mmgr_line_put(args->report, "]}");
}
