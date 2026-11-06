// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_validate.c: validation: the Lean tool reads, comments and strings are not code, flags find their words, theorems take their
// namespaces, messages read Lean's JSON, instance timeouts are their own barrier, and the copy raises the file's own limits
#include "ui/cli/src/plugins/validate.h"
#include "ui/cli/src/plugins/validate_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_lean_output[] = "{\"severity\":\"error\",\"pos\":{\"line\":3,\"column\":2},\"endPos\":{\"line\":3,\"column\":9},"
                                    "\"keepFullRange\":false,\"fileName\":\"x.lean\",\"data\":\"(deterministic) timeout at `whnf`, maximum number of "
                                    "heartbeats (200000) has been reached\",\"caption\":\"\"}\n"
                                    "{\"severity\":\"information\",\"pos\":{\"line\":9,\"column\":0},\"endPos\":null,\"fileName\":\"x.lean\","
                                    "\"data\":\"'A.one' depends on axioms: [propext, sorryAx]\",\"caption\":\"\"}\nStack overflow detected. "
                                    "Aborting.\n";
static const char *const s_limit_names[1] = {"maxHeartbeats"};
static const unsigned long long s_limit_values[1] = {800000ull};

static CliProverMessages s_messages;
static char s_lean[CLI_NAME_MAX_LEN];
static char s_words[256];

// the code of `text` read, with its words
static void code_read(const char *text)
{
    (void)EMBED_CALL(cli_validate_text_internal.code_get, CliValidateTextCtx, .text = text, .size = CLI_UNITY_LENGTH(text));
    cli_validate_words_get();
}

// the name of the barrier a message stopped at, the empty string where none
static const char *barrier_of(const CliProverMessage *message)
{
    const int barrier = EMBED_CALL(cli_validate_messages_internal.barrier_get, CliValidateMessagesCtx, .messages_read = &s_messages, .message = message);
    return (barrier >= 0) ? g_cli_validate->tool.barriers[barrier].name : "";
}

// a message of `text` and `kind` placed in the messages' texts, of the first line
static CliProverMessage message_of(const char *text, const char *kind)
{
    CliProverMessage message = {1u, 0u, 0u, 0u, 1u, s_messages.used, CLI_UNITY_LENGTH(text), 0ull, CLI_UNITY_LENGTH(kind)};
    memor.cpy(&(MemoriaCfg){.dst = s_messages.text + s_messages.used, .src = text, .bytes = CLI_UNITY_LENGTH(text)});
    s_messages.used += CLI_UNITY_LENGTH(text);
    message.kind = s_messages.used;
    memor.cpy(&(MemoriaCfg){.dst = s_messages.text + s_messages.used, .src = kind, .bytes = CLI_UNITY_LENGTH(kind)});
    s_messages.used += CLI_UNITY_LENGTH(kind);
    return message;
}

// the flags of a sample of four found
static void flags_found(void)
{
    code_read("def f (x : Float) : UInt64 := 0\ntheorem t : 1 = 1 := by native_decide\nexample : True := sorry\n#eval f 1.0");
    g_cli_validate->finding_count = 0u;
    g_cli_validate->finding_used = 0ull;
    cli_validate_flags_in();
}

// the messages emptied
static void messages_emptied(void)
{
    s_messages.count = 0u;
    s_messages.used = 0ull;
}

void setUp(void)
{
    cli_unity_begun();
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_validate.tool_for, CliValidateCfg, .language = "lean", .id = s_lean), "the tool for a language is found with no validation's memory allocated");
    TEST_ASSERT_TRUE_MESSAGE(cli_validate_lock(), "a validation's memory is allocated");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_validate_id_internal.tool_read, CliValidateIdCtx, .id = s_lean), "orior_cli comes with a tool for Lean");
}

void tearDown(void)
{
    g_cli_guard->persistent_buf_release(g_cli_validate->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_validate->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_validate->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(g_cli_validate->manifest.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_validate->manifest.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_validate->manifest),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(g_cli_validate->output, g_cli_plugins_owner);
    g_cli_guard->persistent_buf_release(g_cli_validate, g_cli_plugins_owner);
    g_cli_validate = NULL;
}

void test_the_lean_tool_reads_its_toolchain(void)
{
    TEST_ASSERT_EQUAL_STRING("lean", g_cli_validate->tool.toolchain);
}

void test_the_lean_tool_reads_its_barriers(void)
{
    int heartbeats = 0;
    for (unsigned int at = 0u; at < g_cli_validate->tool.barrier_count; at += 1u)
    {
        heartbeats |= cellul.eq(&(CatenaFinitaCfg){.src = g_cli_validate->tool.barriers[at].name, .other = "maxHeartbeats", .cap = CLI_POOL_BYTES});
    }
    TEST_ASSERT_TRUE(heartbeats);
}

void test_comments_and_strings_are_not_code(void)
{
    code_read("def x : Float := 1 -- Float\n/- Float /- nested -/ Float -/ \"Float\" theorem y");
    unsigned int floats = 0u;
    unsigned int theorem = 0u;
    unsigned int lines = 1u;
    const char *const code = g_cli_validate->code;
    for (unsigned long long at = 0ull; at < g_cli_validate->code_size; at += 1ull)
    {
        lines += (code[at] == '\n') ? 1u : 0u;
        floats += ((at + 5u <= g_cli_validate->code_size) && (memor.cmp(&(MemoriaCfg){.src = code + at, .other = "Float", .bytes = 5u}) == 0)) ? 1u : 0u;
        theorem += ((at + 9u <= g_cli_validate->code_size) && (memor.cmp(&(MemoriaCfg){.src = code + at, .other = "theorem y", .bytes = 9u}) == 0)) ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT(1u, floats);
    TEST_ASSERT_EQUAL_UINT(1u, theorem);
    TEST_ASSERT_EQUAL_UINT(2u, lines);
}

void test_flags_find_their_words(void)
{
    flags_found();
    VerbaLinea line = {s_words, sizeof(s_words), 0ull};
    for (unsigned int at = 0u; at < g_cli_validate->finding_count; at += 1u)
    {
        const char *const message = g_cli_validate->finding_text + g_cli_validate->findings[at].message;
        const char *const colon = cellul.chr(&(CatenaFinitaCfg){.src = message, .cap = CLI_POOL_BYTES, .byte = ':'});
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        // the colon lies in the message, at or past its start
        mmgr_line_put_n(&line, message, (colon != NULL) ? (unsigned long long)(colon - message) : CLI_UNITY_LENGTH(message));
    }
    CLI_TEST_ASSERT_TEXT("Float|UInt64|native_decide|#eval", s_words, line.at);
}

void test_each_flag_has_its_position_and_severity(void)
{
    flags_found();
    TEST_ASSERT_EQUAL_UINT(4u, g_cli_validate->finding_count);
    TEST_ASSERT_EQUAL_UINT(0u, g_cli_validate->findings[0].line);
    TEST_ASSERT_EQUAL_UINT(11u, g_cli_validate->findings[0].col);
    TEST_ASSERT_EQUAL_UINT(3u, g_cli_validate->findings[3].severity);
}

void test_theorems_alloc_their_namespaces(void)
{
    code_read("namespace A\ntheorem one : True := trivial\nnamespace B\nlemma two : True := trivial\nend B\nsection\ntheorem three : True := trivial\nend\nend A\ntheorem four : True := trivial");
    cli_validate_theorems_in();
    VerbaLinea line = {s_words, sizeof(s_words), 0ull};
    for (unsigned int at = 0u; at < g_cli_validate->theorem_count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, g_cli_validate->names + g_cli_validate->theorems[at]);
    }
    CLI_TEST_ASSERT_TEXT("A.one|A.B.two|A.three|four", s_words, line.at);
}

void test_messages_read_leans_json_each_at_its_position(void)
{
    EMBED_CALL(cli_validate_size_internal.messages_get, CliValidateSizeCtx, .output = s_lean_output, .size = sizeof(s_lean_output) - 1u, .messages = &s_messages);
    TEST_ASSERT_EQUAL_UINT(3u, s_messages.count);
    TEST_ASSERT_EQUAL_UINT(2u, s_messages.message[0].line);
    TEST_ASSERT_EQUAL_UINT(2u, s_messages.message[0].col);
    TEST_ASSERT_EQUAL_UINT(9u, s_messages.message[0].end_col);
}

void test_information_is_a_note(void)
{
    EMBED_CALL(cli_validate_size_internal.messages_get, CliValidateSizeCtx, .output = s_lean_output, .size = sizeof(s_lean_output) - 1u, .messages = &s_messages);
    TEST_ASSERT_EQUAL_UINT(3u, s_messages.message[1].severity);
}

void test_a_timeout_is_the_heartbeats_barrier(void)
{
    EMBED_CALL(cli_validate_size_internal.messages_get, CliValidateSizeCtx, .output = s_lean_output, .size = sizeof(s_lean_output) - 1u, .messages = &s_messages);
    TEST_ASSERT_EQUAL_STRING("maxHeartbeats", barrier_of(&s_messages.message[0]));
}

void test_a_stack_overflow_is_the_kernels_recursion_barrier(void)
{
    EMBED_CALL(cli_validate_size_internal.messages_get, CliValidateSizeCtx, .output = s_lean_output, .size = sizeof(s_lean_output) - 1u, .messages = &s_messages);
    TEST_ASSERT_EQUAL_STRING("kernel deep recursion", barrier_of(&s_messages.message[2]));
}

void test_a_theorems_axioms_are_read(void)
{
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long list = 0ull;
    unsigned long long list_size = 0ull;
    EMBED_CALL(cli_validate_size_internal.messages_get, CliValidateSizeCtx, .output = s_lean_output, .size = sizeof(s_lean_output) - 1u, .messages = &s_messages);
    const CliProverMessage *const axioms = &s_messages.message[1];
    const char *const text = s_messages.text + axioms->text;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_validate_text_internal.axioms_get, CliValidateTextCtx, .text = text, .size = axioms->text_size, .name = &name, .name_size = &name_size, .list = &list, .list_size = &list_size));
    CLI_TEST_ASSERT_TEXT("A.one", text + name, name_size);
    CLI_TEST_ASSERT_TEXT("propext, sorryAx", text + list, list_size);
}

void test_a_theorem_with_no_axioms_is_read(void)
{
    static const char s_none[] = "'t' does not depend on any axioms";
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long list = 0ull;
    unsigned long long list_size = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_validate_text_internal.axioms_get, CliValidateTextCtx, .text = s_none, .size = sizeof(s_none) - 1u, .name = &name, .name_size = &name_size, .list = &list, .list_size = &list_size));
    CLI_TEST_ASSERT_TEXT("t", s_none + name, name_size);
    TEST_ASSERT_EQUAL_UINT64(0ull, list_size);
}

void test_an_instance_timeout_is_its_own_barrier(void)
{
    messages_emptied();
    const CliProverMessage synth = message_of("failed to synthesize\n  Foo\n(deterministic) timeout at `isDefEq`, maximum number of heartbeats (20000) has "
                                              "been reached\nUse `set_option synthInstance.maxHeartbeats <num>` to set the limit.",
                                              "runtime.maxHeartbeats");
    TEST_ASSERT_EQUAL_STRING("synthInstance.maxHeartbeats", barrier_of(&synth));
}

void test_a_messages_kind_names_its_barrier(void)
{
    messages_emptied();
    const CliProverMessage depth = message_of("anything", "runtime.maxRecDepth");
    TEST_ASSERT_EQUAL_STRING("maxRecDepth", barrier_of(&depth));
}

void test_a_fault_of_the_proof_is_no_barrier(void)
{
    messages_emptied();
    const CliProverMessage mismatch = message_of("type mismatch\n  h\nhas type", "[anonymous]");
    TEST_ASSERT_EQUAL_STRING("", barrier_of(&mismatch));
}

void test_the_copy_raises_the_files_limits_and_queries_each_theorems_axioms(void)
{
    static const char s_text[] = "set_option maxHeartbeats 400000 in\ntheorem t : True := trivial";
    static const char s_tail[] = "#print axioms t\n";
    char changed[256];
    code_read(s_text);
    cli_validate_theorems_in();
    VerbaLinea line = {changed, sizeof(changed), 0ull};
    const unsigned long long size = EMBED_CALL(cli_validate_text_size_internal.copy_get, CliValidateTextSizeCtx, .text = s_text, .size = sizeof(s_text) - 1u, .names = s_limit_names, .values = s_limit_values, .count = 1u, .changed = &line);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(sizeof(s_tail) - 1u, size);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = g_cli_validate->copy, .other = "set_option maxHeartbeats 800000 in\n", .cap = CLI_POOL_BYTES}));
    TEST_ASSERT_EQUAL_MEMORY(s_tail, g_cli_validate->copy + size - (sizeof(s_tail) - 1u), sizeof(s_tail) - 1u);
}

void test_the_copy_reports_what_it_changed(void)
{
    static const char s_text[] = "set_option maxHeartbeats 400000 in\ntheorem t : True := trivial";
    char changed[256];
    code_read(s_text);
    cli_validate_theorems_in();
    VerbaLinea line = {changed, sizeof(changed), 0ull};
    (void)EMBED_CALL(cli_validate_text_size_internal.copy_get, CliValidateTextSizeCtx, .text = s_text, .size = sizeof(s_text) - 1u, .names = s_limit_names, .values = s_limit_values, .count = 1u, .changed = &line);
    CLI_TEST_ASSERT_TEXT("line 1: set_option maxHeartbeats 400000, checked at 800000", changed, line.at);
}

void test_a_limit_of_zero_is_unchanged(void)
{
    static const char s_zero[] = "set_option maxHeartbeats 0 in\n";
    char changed[256];
    code_read(s_zero);
    g_cli_validate->theorem_count = 0u;
    VerbaLinea line = {changed, sizeof(changed), 0ull};
    const unsigned long long size = EMBED_CALL(cli_validate_text_size_internal.copy_get, CliValidateTextSizeCtx, .text = s_zero, .size = sizeof(s_zero) - 1u, .names = s_limit_names, .values = s_limit_values, .count = 1u, .changed = &line);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(25ull, size);
    CLI_TEST_ASSERT_CONTAINS("maxHeartbeats 0 in", g_cli_validate->copy);
    TEST_ASSERT_EQUAL_UINT64(0ull, line.at);
}
