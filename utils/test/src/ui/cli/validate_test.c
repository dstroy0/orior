// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_test.c: the validation's check: the Lean tool reads; comments and strings are not code; flags find their
// words; theorems take their namespaces; messages read Lean's JSON; instance timeouts are their own barrier; and the
// copy grows the file's own limits
#include "../../../../../ui/cli/src/plugins/validate.h"
#include "../../../../../ui/cli/src/plugins/validate_internal.h"
#include "cli_check.h"

static CliProverMessages s_messages;

// the id of the tool for Lean, looked for with no validation's room kept, as the command line looks for it
static char s_lean[CLI_NAME_LONGEST];

// the Lean tool read into the room: 1, or 0 where orior_cli has none
static int validate_lean(void)
{
    return cli_validate_tool_read(s_lean);
}

// the code of `text` read, with its words
static void validate_code(const char *text)
{
    (void)cli_validate_code_of(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    cli_validate_words_of();
}

// the name of the barrier a message stopped at, the empty text where none
static const char *validate_barrier(const CliProverMessage *message)
{
    const int barrier = cli_validate_barrier_of(&s_messages, message);
    return (barrier >= 0) ? g_cli_validate->tool.barriers[barrier].name : "";
}

// a message of `text` and `kind` put in the messages' texts, of the first line
static CliProverMessage validate_message(const char *text, const char *kind)
{
    CliProverMessage message = {1u,
                                0u,
                                0u,
                                0u,
                                1u,
                                s_messages.used,
                                cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                0ull,
                                cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL})};
    memor.cpy(&(MemoriaCfg){.dst = s_messages.text + s_messages.used,
                            .src = text,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})});
    s_messages.used += cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    message.kind = s_messages.used;
    memor.cpy(&(MemoriaCfg){.dst = s_messages.text + s_messages.used,
                            .src = kind,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL})});
    s_messages.used += cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL});
    return message;
}

// flags find their words, and theorems take their namespaces
static void validate_flags(void)
{
    validate_code("def f (x : Float) : UInt64 := 0\ntheorem t : 1 = 1 := by native_decide\nexample : True := sorry\n"
                  "#eval f 1.0");
    g_cli_validate->finding_count = 0u;
    g_cli_validate->finding_used = 0ull;
    cli_validate_flags_in();
    char words[256];
    CliLine line = {words, sizeof(words), 0ull};
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        const char *const message = g_cli_validate->finding_text + g_cli_validate->findings[at].message;
        const char *const colon = cellul.chr(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL, .byte = ':'});
        cli_line_text(&line, (at > 0u) ? "|" : "");
        // the colon lies in the message, at or past its start
        cli_line_counted(&line, message,
                         (colon != NULL) ? (unsigned long long)(colon - message)
                                         : cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}));
    }
    cli_check_text(words, line.at, "Float|UInt64|native_decide|#eval", "flags find their words");
    cli_check((g_cli_validate->finding_count == 4u) && (g_cli_validate->findings[0].line == 0u) &&
                  (g_cli_validate->findings[0].col == 11u) && (g_cli_validate->findings[3].severity == 3u),
              "each at its place and of its severity");
    validate_code(
        "namespace A\ntheorem one : True := trivial\nnamespace B\nlemma two : True := trivial\nend B\nsection\n"
        "theorem three : True := trivial\nend\nend A\ntheorem four : True := trivial");
    cli_validate_theorems_in();
    line.at = 0ull;
    for (unsigned int at = 0u; at < g_cli_validate->theorem_count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, g_cli_validate->names + g_cli_validate->theorems[at]);
    }
    cli_check_text(words, line.at, "A.one|A.B.two|A.three|four", "theorems take their namespaces");
}

// messages read Lean's JSON, and instance timeouts are their own barrier
static void validate_messages(void)
{
    static const char s_output[] =
        "{\"severity\":\"error\",\"pos\":{\"line\":3,\"column\":2},\"endPos\":{\"line\":3,\"column\":9},"
        "\"keepFullRange\":false,\"fileName\":\"x.lean\",\"data\":\"(deterministic) timeout at `whnf`, maximum number "
        "of "
        "heartbeats (200000) has been reached\",\"caption\":\"\"}\n"
        "{\"severity\":\"information\",\"pos\":{\"line\":9,\"column\":0},\"endPos\":null,\"fileName\":\"x.lean\","
        "\"data\":\"'A.one' depends on axioms: [propext, sorryAx]\",\"caption\":\"\"}\nStack overflow detected. "
        "Aborting.\n";
    cli_validate_messages_of(s_output, sizeof(s_output) - 1u, &s_messages);
    const CliProverMessage *const first = &s_messages.message[0];
    cli_check((s_messages.count == 3u) && (first->line == 2u) && (first->col == 2u) && (first->end_col == 9u),
              "messages read Lean's JSON, each at its place");
    cli_check(s_messages.message[1].severity == 3u, "information is a note");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = validate_barrier(first), .other = "maxHeartbeats", .cap = CLI_CELLS_POOL}),
        "a timeout is the heartbeats' barrier");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = validate_barrier(&s_messages.message[2]),
                                           .other = "kernel deep recursion",
                                           .cap = CLI_CELLS_POOL}),
              "and a stack overflow the kernel's recursion");
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long list = 0ull;
    unsigned long long list_size = 0ull;
    const CliProverMessage *const axioms = &s_messages.message[1];
    const char *const text = s_messages.text + axioms->text;
    cli_check(cli_validate_axioms_of(text, axioms->text_size, &name, &name_size, &list, &list_size) &&
                  (name_size == 5ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = text + name, .other = "A.one", .bytes = 5u}) == 0) &&
                  (list_size == 16ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = text + list, .other = "propext, sorryAx", .bytes = 16u}) == 0),
              "a theorem's axioms are read");
    static const char s_none[] = "'t' does not depend on any axioms";
    cli_check(cli_validate_axioms_of(s_none, sizeof(s_none) - 1u, &name, &name_size, &list, &list_size) &&
                  (name_size == 1ull) && (s_none[name] == 't') && (list_size == 0ull),
              "and a theorem that rests on none");
    s_messages.count = 0u;
    s_messages.used = 0ull;
    const CliProverMessage synth = validate_message(
        "failed to synthesize\n  Foo\n(deterministic) timeout at `isDefEq`, maximum number of heartbeats (20000) has "
        "been reached\nUse `set_option synthInstance.maxHeartbeats <num>` to set the limit.",
        "runtime.maxHeartbeats");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = validate_barrier(&synth), .other = "synthInstance.maxHeartbeats", .cap = CLI_CELLS_POOL}),
              "an instance timeout is its own barrier");
    const CliProverMessage depth = validate_message("anything", "runtime.maxRecDepth");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = validate_barrier(&depth), .other = "maxRecDepth", .cap = CLI_CELLS_POOL}),
        "a message's kind names its barrier");
    const CliProverMessage mismatch = validate_message("type mismatch\n  h\nhas type", "[anonymous]");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = validate_barrier(&mismatch), .other = "", .cap = CLI_CELLS_POOL}),
              "and a fault of the proof is none");
}

// the copy grows the file's own limits
static void validate_copy(void)
{
    static const char *const s_names[1] = {"maxHeartbeats"};
    static const unsigned long long s_values[1] = {800000ull};
    static const char s_text[] = "set_option maxHeartbeats 400000 in\ntheorem t : True := trivial";
    validate_code(s_text);
    cli_validate_theorems_in();
    char changed[256];
    CliLine line = {changed, sizeof(changed), 0ull};
    const unsigned long long size = cli_validate_copy_of(s_text, sizeof(s_text) - 1u, s_names, s_values, 1u, &line);
    const char *const copy = g_cli_validate->copy;
    static const char s_tail[] = "#print axioms t\n";
    cli_check(
        (size >= sizeof(s_tail) - 1u) &&
            (cellul.starts(&(CatenaFinitaCfg){
                .src = copy, .other = "set_option maxHeartbeats 800000 in\n", .cap = CLI_CELLS_POOL})) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = copy + size - (sizeof(s_tail) - 1u), .other = s_tail, .bytes = sizeof(s_tail) - 1u}) == 0),
        "the copy grows the file's own limits and asks each theorem's axioms");
    cli_check_text(changed, line.at, "line 1: set_option maxHeartbeats 400000, checked at 800000",
                   "and says what it changed");
    static const char s_zero[] = "set_option maxHeartbeats 0 in\n";
    validate_code(s_zero);
    g_cli_validate->theorem_count = 0u;
    line.at = 0ull;
    const unsigned long long kept = cli_validate_copy_of(s_zero, sizeof(s_zero) - 1u, s_names, s_values, 1u, &line);
    cli_check((kept >= 25ull) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = g_cli_validate->copy,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "maxHeartbeats 0 in",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (line.at == 0ull),
              "a limit of none is kept");
}

int main(void)
{
    cli_check(cli_validate_tool_for("lean", s_lean), "the tool for a language is found with no validation's room held");
    if (!cli_validate_held())
    {
        cli_check(0, "a validation's room is taken");
        return cli_checked();
    }
    if (!validate_lean())
    {
        cli_check(0, "orior_cli comes with a tool for Lean");
        cli_validate_released();
        return cli_checked();
    }
    cli_check(
        (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_validate->tool.toolchain, .other = "lean", .cap = CLI_CELLS_POOL})),
        "the Lean tool reads its toolchain");
    int heartbeats = 0;
    for (unsigned int at = 0u; at < g_cli_validate->tool.barrier_count; at += 1u)
    {
        heartbeats |= cellul.eq(&(CatenaFinitaCfg){
            .src = g_cli_validate->tool.barriers[at].name, .other = "maxHeartbeats", .cap = CLI_CELLS_POOL});
    }
    cli_check(heartbeats, "and its barriers");
    validate_code("def x : Float := 1 -- Float\n/- Float /- nested -/ Float -/ \"Float\" theorem y");
    unsigned int floats = 0u;
    unsigned int theorem = 0u;
    unsigned int lines = 1u;
    const char *const code = g_cli_validate->code;
    for (unsigned long long at = 0ull; at < g_cli_validate->code_size; at += 1ull)
    {
        lines += (code[at] == '\n') ? 1u : 0u;
        floats += ((at + 5u <= g_cli_validate->code_size) &&
                   (memor.cmp(&(MemoriaCfg){.src = code + at, .other = "Float", .bytes = 5u}) == 0))
                      ? 1u
                      : 0u;
        theorem += ((at + 9u <= g_cli_validate->code_size) &&
                    (memor.cmp(&(MemoriaCfg){.src = code + at, .other = "theorem y", .bytes = 9u}) == 0))
                       ? 1u
                       : 0u;
    }
    cli_check((floats == 1u) && (theorem == 1u) && (lines == 2u), "comments and strings are not code");
    validate_flags();
    validate_messages();
    validate_copy();
    cli_validate_released();
    return cli_checked();
}
