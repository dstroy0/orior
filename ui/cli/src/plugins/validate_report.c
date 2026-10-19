// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_report.c: what the checks found, reported: each barrier the first check stopped at and what lifting it came
// to, the mathematics the last check found wrong, the flags, the axioms each theorem rests on past those trusted, and
// the verdict
#include "validate_internal.h"

#include "../text/text.h"

static CliFinding s_sorting[CLI_VALIDATE_FINDINGS];

void cli_validate_finding_put(const CliFinding *finding, const char *message, unsigned long long length,
                              const char *lifted)
{
    const unsigned long long lifted_size =
        (lifted != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = lifted, .cap = CLI_CELLS_POOL}) + 1ull : 0ull;
    if ((g_cli_validate->finding_count == CLI_VALIDATE_FINDINGS) ||
        (g_cli_validate->finding_used + length + 1ull + lifted_size > CLI_VALIDATE_FINDING_TEXT))
    {
        return;
    }
    CliFinding *const put = &g_cli_validate->findings[g_cli_validate->finding_count];
    *put = *finding;
    put->message = g_cli_validate->finding_used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_validate->finding_text + put->message, .src = message, .bytes = length});
    g_cli_validate->finding_text[put->message + length] = '\0';
    g_cli_validate->finding_used += length + 1ull;
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

// a limit said as `<name> <value>`, nothing where it was not grown
static void report_setting(CliLine *out, unsigned int key)
{
    if (g_cli_validate->has_grown[key])
    {
        cli_line_text(out, g_cli_validate->tool.barriers[key].name);
        cli_line_character(out, ' ');
        cli_line_decimal(out, g_cli_validate->grown[key], 1u);
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
        const int barrier = cli_validate_barrier_of(first, message);
        if ((message->severity != 1u) || (message->line >= g_cli_validate->lines) || (barrier < 0))
        {
            continue;
        }
        const CliBarrier *const stopping = &tool->barriers[barrier];
        const unsigned int key = cli_validate_key_of((unsigned int)barrier);
        int still = 0;
        for (unsigned int now = 0u; !still && (now < last->count); now += 1u)
        {
            const CliProverMessage *const again = &last->message[now];
            const int again_barrier = cli_validate_barrier_of(last, again);
            still = (again->severity == 1u) && (again->line == message->line) && (again_barrier >= 0) &&
                    (cellul.eq(&(CatenaFinitaCfg){
                        .src = tool->barriers[again_barrier].name, .other = stopping->name, .cap = CLI_CELLS_POOL}));
        }
        char lifted[1024];
        CliLine lifted_line = {lifted, sizeof(lifted), 0ull};
        if (g_cli_validate->stuck[key])
        {
            cli_line_text(&lifted_line, "still stops the check at its ceiling, ");
            cli_line_decimal(&lifted_line, g_cli_validate->stuck_at[key], 1u);
        }
        else if (still)
        {
            cli_line_text(&lifted_line, "still stops the check once the other limits are lifted");
        }
        else
        {
            g_cli_validate->lifted[key] = 1;
            cli_line_text(&lifted_line, "goes through at ");
            report_setting(&lifted_line, key);
        }
        cli_line_closed(&lifted_line);
        const char *const text = first->text + message->text;
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text, .val = '\n', .bytes = message->text_size});
        unsigned long long first_line = (newline != NULL) ? (unsigned long long)(newline - text) : message->text_size;
        first_line -= ((newline != NULL) && (first_line > 0ull) && (text[first_line - 1ull] == '\r')) ? 1ull : 0ull;
        char said[8192];
        CliLine said_line = {said, sizeof(said), 0ull};
        cli_line_text(&said_line, stopping->name);
        cli_line_text(&said_line, ": ");
        cli_line_text(&said_line, stopping->about);
        cli_line_text(&said_line, " stopped the check here: ");
        cli_line_counted(&said_line, text, first_line);
        const CliFinding finding = {
            message->line, message->col, message->end_line, message->end_col, still ? 1u : 3u, "barrier", barrier,
            0ull,          0ull};
        cli_validate_finding_put(&finding, said, said_line.at, lifted);
    }
}

// the mathematics a message of the kinds the audit says better goes taken out, the audit naming the gap with what
// fills it
static void report_repeats(void)
{
    const CliProverMessages *const last = &g_cli_validate->last;
    const CliJson *const json = &g_cli_validate->json;
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        const CliFinding *const finding = &g_cli_validate->findings[at];
        int repeated = 0;
        for (unsigned int now = 0u;
             !repeated &&
             (cellul.eq(&(CatenaFinitaCfg){.src = finding->kind, .other = "math", .cap = CLI_CELLS_POOL})) &&
             (now < last->count);
             now += 1u)
        {
            const CliProverMessage *const message = &last->message[now];
            repeated =
                (message->line == finding->line) && (message->col == finding->col) &&
                cli_validate_holds(json, g_cli_validate->tool.repeats, last->text + message->kind, message->kind_size);
        }
        if (!repeated)
        {
            g_cli_validate->findings[kept] = *finding;
            kept += 1u;
        }
    }
    g_cli_validate->finding_count = kept;
}

// the findings in the order of their places, as a stable sort keeps them, from `from` to `to`
static void report_sorted(unsigned int from, unsigned int to)
{
    if (to - from < 2u)
    {
        return;
    }
    const unsigned int middle = from + ((to - from) / 2u);
    report_sorted(from, middle);
    report_sorted(middle, to);
    CliFinding *const found = g_cli_validate->findings;
    unsigned int left = from;
    unsigned int right = middle;
    for (unsigned int at = from; at < to; at += 1u)
    {
        const int take_right =
            (right < to) && ((left == middle) || (found[right].line < found[left].line) ||
                             ((found[right].line == found[left].line) && (found[right].col < found[left].col)));
        s_sorting[at] = take_right ? found[right] : found[left];
        right += take_right ? 1u : 0u;
        left += take_right ? 0u : 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = found + from, .src = s_sorting + from, .bytes = (to - from) * sizeof(found[0])});
}

// a count and its noun, `one` where it is 1 and `many` otherwise, a "; " before it where the verdict says something
static void report_counted(CliLine *out, unsigned long long count, const char *one, const char *many, const char *after)
{
    cli_line_text(out, (out->at > 0ull) ? "; " : "");
    cli_line_decimal(out, count, 1u);
    cli_line_character(out, ' ');
    cli_line_text(out, (count == 1ull) ? one : many);
    cli_line_text(out, after);
}

// the verdict: whether the check ran out of time or holds, the errors, the places a limit stops, the limits lifted, the
// file's own settings changed and the theorems that rest on axioms past those trusted
static void report_verdict(CliLine *out, int holds, unsigned long long math, unsigned long long blocked,
                           const CliLine *untrusted)
{
    const CliTool *const tool = &g_cli_validate->tool;
    const CliJson *const json = &g_cli_validate->json;
    if (g_cli_validate->late)
    {
        cli_line_text(out, "the check ran past ");
        cli_line_decimal(out, tool->patience, 1u);
        cli_line_text(out, " seconds and was stopped");
    }
    else if (holds && (g_cli_validate->theorem_count == 0u))
    {
        cli_line_text(out, "it checks");
    }
    else if (holds)
    {
        cli_line_decimal(out, g_cli_validate->theorem_count, 1u);
        cli_line_text(out, " theorems check");
    }
    if (math > 0ull)
    {
        report_counted(out, math, "error", "errors", " in the mathematics");
    }
    if (blocked > 0ull)
    {
        report_counted(out, blocked, "place", "places", " a limit stops past its ceiling");
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
        while ((place > 0u) && (cli_text_order(tool->barriers[keys[place - 1u]].name, tool->barriers[key].name) > 0))
        {
            keys[place] = keys[place - 1u];
            place -= 1u;
        }
        keys[place] = key;
        lifted += 1u;
    }
    if (lifted > 0u)
    {
        cli_line_text(out, (out->at > 0ull) ? "; once lifted to " : "once lifted to ");
        int said = 0;
        for (unsigned int at = 0u; at < lifted; at += 1u)
        {
            if (g_cli_validate->has_grown[keys[at]])
            {
                cli_line_text(out, said ? ", " : "");
                report_setting(out, keys[at]);
                said = 1;
            }
        }
    }
    if (g_cli_validate->own_size > 0ull)
    {
        cli_line_text(out, (out->at > 0ull) ? "; the file's own " : "the file's own ");
        cli_line_counted(out, g_cli_validate->own, g_cli_validate->own_size);
    }
    if (untrusted->at > 0ull)
    {
        cli_line_text(out, (out->at > 0ull) ? "; " : "");
        cli_line_counted(out, untrusted->out, untrusted->at);
        cli_line_text(out, " rest on axioms past ");
        for (unsigned int index = 0u; cli_json_item(json, tool->trusted, index) != CLI_JSON_NONE; index += 1u)
        {
            const char *const axiom = cli_json_string(json, cli_json_item(json, tool->trusted, index), NULL);
            cli_line_text(out, (index > 0u) ? ", " : "");
            cli_line_text(out, axiom);
        }
    }
}

// a finding written as `{"line", "col", "end_line", "end_col", "severity", "kind", "barrier", "message", "lifted"}`
static void report_finding_written(const CliFinding *finding, CliLine *out)
{
    static const char *const keys[4] = {"{\"line\":", ",\"col\":", ",\"end_line\":", ",\"end_col\":"};
    const unsigned int places[4] = {finding->line, finding->col, finding->end_line, finding->end_col};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        cli_line_text(out, keys[at]);
        cli_line_decimal(out, places[at], 1u);
    }
    cli_line_text(out, ",\"severity\":");
    cli_line_decimal(out, finding->severity, 1u);
    cli_line_text(out, ",\"kind\":");
    cli_json_quoted(out, finding->kind, cellul.len(&(CatenaFinitaCfg){.src = finding->kind, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"barrier\":");
    if (finding->barrier < 0)
    {
        cli_line_text(out, "null");
    }
    else
    {
        const char *const name = g_cli_validate->tool.barriers[finding->barrier].name;
        cli_json_quoted(out, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    const char *const message = g_cli_validate->finding_text + finding->message;
    cli_line_text(out, ",\"message\":");
    cli_json_quoted(out, message, cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"lifted\":");
    if (finding->lifted == CLI_VALIDATE_NONE)
    {
        cli_line_text(out, "null");
    }
    else
    {
        const char *const lifted = g_cli_validate->finding_text + finding->lifted;
        cli_json_quoted(out, lifted, cellul.len(&(CatenaFinitaCfg){.src = lifted, .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, '}');
}

void cli_validate_reported(const char *path, const char *project, CliLine *report)
{
    static char s_untrusted[1u << 16u];
    CliLine untrusted = {s_untrusted, sizeof(s_untrusted), 0ull};
    report_barriers();
    const unsigned int answers = cli_validate_mathematics();
    cli_validate_flags_in();
    if (g_cli_validate->tool.audited)
    {
        report_repeats();
        cli_validate_axioms_reported(answers, &untrusted);
    }
    report_sorted(0u, g_cli_validate->finding_count);
    unsigned long long math = 0ull;
    unsigned long long blocked = 0ull;
    unsigned long long gaps = 0ull;
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        const CliFinding *const finding = &g_cli_validate->findings[at];
        const int error = (finding->severity == 1u);
        math += (error && (cellul.eq(&(CatenaFinitaCfg){.src = finding->kind, .other = "math", .cap = CLI_CELLS_POOL})))
                    ? 1ull
                    : 0ull;
        blocked +=
            (error && (cellul.eq(&(CatenaFinitaCfg){.src = finding->kind, .other = "barrier", .cap = CLI_CELLS_POOL})))
                ? 1ull
                : 0ull;
        gaps +=
            (error && ((cellul.eq(&(CatenaFinitaCfg){.src = finding->kind, .other = "flag", .cap = CLI_CELLS_POOL})) ||
                       (cellul.eq(&(CatenaFinitaCfg){.src = finding->kind, .other = "axiom", .cap = CLI_CELLS_POOL}))))
                ? 1ull
                : 0ull;
    }
    const int holds = !g_cli_validate->late && (math == 0ull) && (blocked == 0ull) && (gaps == 0ull);
    static char s_verdict[1u << 18u];
    CliLine verdict = {s_verdict, sizeof(s_verdict), 0ull};
    report_verdict(&verdict, holds, math, blocked, &untrusted);
    const char *const name = g_cli_validate->tool.name;
    cli_line_text(report, "{\"tool\":");
    cli_json_quoted(report, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_text(report, ",\"file\":");
    cli_json_quoted(report, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_text(report, ",\"project\":");
    if (project == NULL)
    {
        cli_line_text(report, "null");
    }
    else
    {
        cli_json_quoted(report, project, cellul.len(&(CatenaFinitaCfg){.src = project, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(report, holds ? ",\"holds\":true,\"verdict\":" : ",\"holds\":false,\"verdict\":");
    cli_json_quoted(report, verdict.out, verdict.at);
    cli_line_text(report, ",\"checks\":[");
    cli_line_counted(report, g_cli_validate->checks, g_cli_validate->checks_size);
    cli_line_text(report, "],\"findings\":[");
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        cli_line_text(report, (at > 0u) ? "," : "");
        report_finding_written(&g_cli_validate->findings[at], report);
    }
    cli_line_text(report, "]}");
}
