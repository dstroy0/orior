// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_servers.c: the projects a server is sent are found to six folders down, in order; a check stopped releases its files waiting and reports it is
// done; clangd serves C and names CUDA files as CUDA; a hover reads as Markdown in each shape; a workspace edit reads in either shape; edits are written
// by UTF-16 columns and preserve line ends; a signature names each parameter by its span; a server requesting settings is sent its defaults; a file the
// check releases stores its diagnostics and wakes its checker; a file names another in its includes and imports only; a report of an attribute names it
// and its class; and a completion is named by what it inserts
#include "ui/cli/src/servers/servers_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliServerSpec s_spec;
static CliCompletions s_items;
static char s_out[1u << 16u];
static char s_events[16][4096];
static unsigned int s_event_count;
static const CliJson *s_json;
static CliServersSink s_sink;
static char s_passed_on[256];
static unsigned long long s_passed_on_at;
static unsigned int s_check_moves;
static unsigned long long s_check_done;
static unsigned long long s_check_total;

static int s_release_run_result;
static int s_release_woken;
static char s_passed_on_after_clear[256];
static unsigned int s_release_items;
static char s_passed_on_final[256];

// an event received recorded as `event json`
static void event_recorded(const CliEachEventJsonSizeCfg *args)
{
    if (s_event_count < 16u)
    {
        VerbaLinea line = {s_events[s_event_count], sizeof(s_events[0]), 0ull};
        mmgr_line_put(&line, args->event);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put_n(&line, args->json, args->size);
        (void)mmgr_line_finish(&line);
        s_event_count += 1u;
    }
}

// a file's diagnostics handed on, how many counted after those handed on before, a comma between
static void diagnostics_counted(const CliServerDiagnosticsCfg *args)
{
    VerbaLinea line = {s_passed_on, sizeof(s_passed_on), s_passed_on_at};
    mmgr_line_put(&line, (line.at > 0ull) ? "," : "");
    mmgr_line_decimal(&line, args->count, 1u);
    s_passed_on_at = line.at;
    (void)mmgr_line_finish(&line);
}

// how far the tree's check has gone, as it last moved
static void check_counted(const CliServerCheckCfg *args)
{
    s_check_moves += 1u;
    s_check_done = args->done;
    s_check_total = args->total;
}

// the program of the server of `language`, empty where none serves it
static const char *server_program(const char *language)
{
    return EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = language, .spec = &s_spec) ? s_spec.program : "";
}

// `text` read into the shared tree: its root
static unsigned int text_parsed(const char *text)
{
    unsigned int read = CLI_JSON_NONE;
    s_json = cli_unity_parsed(text, CLI_UNITY_LENGTH(text), &read);
    return read;
}

// an edit's JSON at `line`, writing `text`
static void edit_written(VerbaLinea *out, unsigned int line, const char *text)
{
    mmgr_line_put(out, "{\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, ",\"character\":0},\"end\":{\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, ",\"character\":1}},\"newText\":\"");
    mmgr_line_put(out, text);
    mmgr_line_put(out, "\"}");
}

// each file's edits a workspace edit lists, `path:text,text` joined by `|`, in s_out
static const char *files_edited(const char *edit)
{
    static char s_written[4096];
    const unsigned int read = text_parsed(edit);
    VerbaLinea written = {s_written, sizeof(s_written), 0ull};
    EMBED_CALL(cli_servers_json_tree_out_internal.workspace_edit_get, CliServersJsonTreeOutCtx, .json = s_json, .edit = read, .tree = NULL, .out = &written);
    (void)mmgr_line_finish(&written);
    const unsigned int files = text_parsed(s_written);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int file = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = files, .index = at);
        const unsigned int edits = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = file, .name = "edits");
        mmgr_line_put(&out, (at > 0u) ? "|" : "");
        mmgr_line_ch(&out, ':');
        for (unsigned int one = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = edits, .index = one) != CLI_JSON_NONE; one += 1u)
        {
            mmgr_line_put(&out, (one > 0u) ? "," : "");
            mmgr_line_put(&out, cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = edits, .index = one), "text"));
        }
    }
    (void)mmgr_line_finish(&out);
    return s_out;
}

// the file URI of this platform's sample path, written into `uri`
static void sample_uri(char *uri)
{
#if defined(_WIN32)
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = "D:\\t\\a.c", .uri_out = uri);
#else
    EMBED_CALL(cli_servers_path_uri_internal.uri_get, CliServersPathUriCtx, .path = "/t/a.c", .uri_out = uri);
#endif
}

// what a server sends on its own received, its answer in s_out
static const char *server_requested(const char *method, const char *params)
{
    VerbaLinea answer = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_lsp_recv.recv, CliLspRecvCfg, .context = (void *)&s_sink, .method = method, .params = params, .size = CLI_UNITY_LENGTH(params), .answer = &answer);
    (void)mmgr_line_finish(&answer);
    return s_out;
}

// the count of the items of the array `member` of what was received at `at`, its event after the first space
static unsigned int event_items(unsigned int at, const char *member)
{
    const char *const json = cellul.chr(&(CatenaFinitaCfg){.src = s_events[at], .cap = CLI_POOL_BYTES, .byte = ' '});
    const unsigned int read = text_parsed((json != NULL) ? json + 1 : "null");
    const unsigned int array = (member != NULL) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = read, .name = member) : read;
    return EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = array, .type = CFG_JSON_ARRAY) ? s_json->node[array].count : 0u;
}

// a file's diagnostics published, `items` of them
static void diagnostics_published(const char *uri, unsigned int items)
{
    static char s_params[1024];
    VerbaLinea line = {s_params, sizeof(s_params), 0ull};
    mmgr_line_put(&line, "{\"uri\":\"");
    mmgr_line_put(&line, uri);
    mmgr_line_put(&line, "\",\"diagnostics\":[");
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "," : "");
        mmgr_line_put(&line, "{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}},"
                             "\"severity\":1,\"message\":\"no\"}");
    }
    mmgr_line_put(&line, "]}");
    (void)mmgr_line_finish(&line);
    (void)server_requested("textDocument/publishDiagnostics", s_params);
}

// a file's checker woken, the file released, and its diagnostics published again, what each passed on recorded once for the process
static void release_run_result(void)
{
#if defined(_WIN32)
    static const char s_uri[] = "file:///C:/t/a.c";
    static const char s_written[] = "file:///c%3A/t/a.c";
#else
    static const char s_uri[] = "file:///t/a.c";
    static const char s_written[] = "file:///t/a.c";
#endif
    char key[CLI_PATH_MAX_LEN];
    if (s_release_run_result)
    {
        return;
    }
    s_release_run_result = 1;
    EMBED_CALL(cli_lsp_length.key_get, CliLspLengthCfg, .uri = s_uri, .length = sizeof(s_uri) - 1u, .key = key);
    s_passed_on_at = 0ull;
    s_passed_on[0] = '\0';
    EMBED_CALL(cli_servers_key_checker_internal.wait_on, CliServersKeyCheckerCtx, .checker = 0u, .key = key);
    diagnostics_published(s_uri, 1u);
    s_release_woken = EMBED_CALL(cli_servers_key_checker_internal.was_wake, CliServersKeyCheckerCtx, .checker = 0u);
    EMBED_CALL(cli_servers_key_checker_internal.let_go_get, CliServersKeyCheckerCtx, .key = key);
    diagnostics_published(s_uri, 1u);
    diagnostics_published(s_written, 0u);
    memor.cpy(&(MemoriaCfg){.dst = s_passed_on_after_clear, .src = s_passed_on, .bytes = sizeof(s_passed_on)});
    const unsigned int place = EMBED_CALL(cli_keyed_key_adjacent.find, CliKeyedKeyAdjacentCfg, .keyed = &g_cli_servers_published, .key = key);
    if (place != CLI_KEYED_NONE)
    {
        // the diagnostics published read out of their cell into the check's own bytes for the parse
        static char s_published[1u << 16u];
        const CliKeyedEntry one = EMBED_CALL(cli_keyed_table.entry, CliKeyedCfg, .keyed = &g_cli_servers_published, .at = place);
        TEST_ASSERT_TRUE(one.text_size < sizeof(s_published));
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = one.cell, .offset = (embed_word)one.text_at, .dst = s_published, .bytes = (embed_word)one.text_size});
        unsigned int read = CLI_JSON_NONE;
        const CliJson *const json = cli_unity_parsed(s_published, one.text_size, &read);
        s_release_items = json->node[read].count;
    }
    diagnostics_published(s_uri, 0u);
    memor.cpy(&(MemoriaCfg){.dst = s_passed_on_final, .src = s_passed_on, .bytes = sizeof(s_passed_on)});
}

// whether a line of `text` names the stem `stem`
static int text_names(const char *text, const char *stem)
{
    const char *line = text;
    while (line[0] != '\0')
    {
        const char *const newline = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES, .byte = '\n'});
        const char *const end = (newline != NULL) ? newline : (line + CLI_UNITY_LENGTH(line));
        // a line's end is at or past its start
        if (EMBED_CALL(cli_servers_size_length_internal.line_names, CliServersSizeLengthCtx, .line_text = line, .length = (unsigned long long)(end - line), .stem = stem, .size = CLI_UNITY_LENGTH(stem)))
        {
            return 1;
        }
        line = (end[0] != '\0') ? (end + 1) : end;
    }
    return 0;
}

void setUp(void)
{
    cli_unity_begun();
    s_sink = (CliServersSink){.emit = event_recorded, .diagnostics = diagnostics_counted, .check_moved = check_counted};
    s_event_count = 0u;
    s_check_moves = 0u;
}

void tearDown(void)
{
}

void test_params_that_did_not_fit_their_line_are_refused_with_no_server_reached(void)
{
    char params_text[4];
    char why[128];
    VerbaLinea params = {params_text, sizeof(params_text), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliLspAnswer answer;
    mmgr_line_put(&params, "{\"longer\":1}");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_servers_err_msg_internal.request, CliServersErrMsgCtx, .server = NULL, .method = "textDocument/hover", .params = &params, .wait = 0ull, .answer = &answer, .err_msg = &err_msg));
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, err_msg.at);
    TEST_ASSERT_NULL(answer.message);
}

void test_the_projects_a_server_is_sent_are_found_to_six_folders_down_in_order(void)
{
    static const char *const s_created[6] = {"b/Cargo.toml", "a/x/Cargo.toml", "target/Cargo.toml", "c/d/e/f/g/h/i/Cargo.toml", ".hidden/Cargo.toml", "Cargo.toml"};
    static const char *const s_found[3] = {"Cargo.toml", "a/x/Cargo.toml", "b/Cargo.toml"};
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char list_entries[4096];
    char required[4096];
    cli_unity_folder("servers_projects", root);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = s_created[at], .joined = path);
        for (char *step = path + CLI_UNITY_LENGTH(root) + 1u; step[0] != '\0'; step += 1)
        {
            if (step[0] == '/')
            {
                step[0] = '\0';
                (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
                step[0] = '/';
            }
        }
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "", .size = 0ull), s_created[at]);
    }
    VerbaLinea out = {list_entries, sizeof(list_entries), 0ull};
    EMBED_CALL(cli_servers_root_out_internal.projects_in, CliServersRootOutCtx, .root = root, .name = "Cargo.toml", .out = &out);
    VerbaLinea want = {required, sizeof(required), 0ull};
    mmgr_line_ch(&want, '[');
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = s_found[at], .joined = path);
        mmgr_line_put(&want, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &want, .text_bytes = (const unsigned char *)path, .length = CLI_UNITY_LENGTH(path));
    }
    mmgr_line_ch(&want, ']');
    (void)mmgr_line_finish(&want);
    CLI_TEST_ASSERT_TEXT(required, list_entries, out.at);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_servers_launch_owner));
}

void test_a_check_stopped_releases_its_files_waiting_and_stays_on_and_reports_done(void)
{
    int on = 0;
    unsigned int waiting = 1u;
    unsigned long long done = 0ull;
    unsigned long long total = 0ull;
    EMBED_CALL(cli_servers_sink_internal.check_set, CliServersSinkCtx, .on = 1, .sink = &s_sink, .done = 3ull, .total = 10ull);
    cli_servers_stop_check();
    EMBED_CALL(cli_servers_on_internal.check_count, CliServersOnCtx, .on_pointer = &on, .waiting_u32s = &waiting, .done_pointer = &done, .total_pointer = &total);
    const unsigned int moves = s_check_moves;
    EMBED_CALL(cli_servers_sink_internal.check_set, CliServersSinkCtx, .on = 0, .sink = NULL, .done = 0ull, .total = 0ull);
    TEST_ASSERT_EQUAL_UINT(0u, waiting);
    TEST_ASSERT_EQUAL_UINT64(3ull, done);
    TEST_ASSERT_EQUAL_UINT64(3ull, total);
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_UINT(1u, moves);
    TEST_ASSERT_EQUAL_UINT64(3ull, s_check_done);
    TEST_ASSERT_EQUAL_UINT64(3ull, s_check_total);
}

void test_clangd_serves_c(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = "c", .spec = &s_spec));
    TEST_ASSERT_EQUAL_STRING("llvm", s_spec.tool);
    TEST_ASSERT_EQUAL_STRING("clangd", s_spec.program);
}

void test_clangd_names_cuda_files_as_cuda(void)
{
    int cuda = 0;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = "c", .spec = &s_spec));
    for (unsigned int at = 0u; at < s_spec.ids; at += 1u)
    {
        cuda = cuda || (cellul.eq(&(CatenaFinitaCfg){.src = s_spec.id_extension[at], .other = "cu", .cap = CLI_POOL_BYTES}) && cellul.eq(&(CatenaFinitaCfg){.src = s_spec.id_name[at], .other = "cuda-cpp", .cap = CLI_POOL_BYTES}));
    }
    TEST_ASSERT_TRUE(cuda);
}

void test_plain_text_has_no_server(void)
{
    TEST_ASSERT_FALSE(EMBED_CALL(cli_servers_language_internal.spec_for, CliServersLanguageCtx, .language = "plaintext", .spec = &s_spec));
}

void test_zig_mojo_ansible_rust_and_python_each_have_a_server(void)
{
    TEST_ASSERT_EQUAL_STRING("zls", server_program("zig"));
    TEST_ASSERT_EQUAL_STRING("mojo-lsp-server", server_program("mojo"));
    TEST_ASSERT_EQUAL_STRING("ansible-language-server", server_program("ansible"));
    TEST_ASSERT_EQUAL_STRING("rust-analyzer", server_program("rust"));
    TEST_ASSERT_EQUAL_STRING("pyright-langserver", server_program("python"));
}

void test_a_hover_of_markdown_reads_as_markdown(void)
{
    const unsigned int read = text_parsed("{\"kind\":\"markdown\",\"value\":\"**x**\"}");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_servers_json_out_internal.markdown_get, CliServersJsonOutCtx, .json = s_json, .contents = read, .out = &out);
    CLI_TEST_ASSERT_TEXT("**x**", s_out, out.at);
}

void test_a_hover_of_code_and_words_reads_as_markdown(void)
{
    const unsigned int read = text_parsed("[{\"language\":\"cpp\",\"value\":\"int x\"},\"a note\"]");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_servers_json_out_internal.markdown_get, CliServersJsonOutCtx, .json = s_json, .contents = read, .out = &out);
    CLI_TEST_ASSERT_TEXT("```cpp\nint x\n```\n\na note", s_out, out.at);
}

void test_a_workspace_edits_changes_read_the_first_edit_first(void)
{
    static char s_edit[4096];
    char uri[CLI_PATH_MAX_LEN];
    VerbaLinea line = {s_edit, sizeof(s_edit), 0ull};
    sample_uri(uri);
    mmgr_line_put(&line, "{\"changes\":{\"");
    mmgr_line_put(&line, uri);
    mmgr_line_put(&line, "\":[");
    edit_written(&line, 3u, "b");
    mmgr_line_ch(&line, ',');
    edit_written(&line, 1u, "a");
    mmgr_line_put(&line, "]}}");
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING(":a,b", files_edited(s_edit));
}

void test_a_workspace_edits_document_changes_read_a_file_created_not_among_them(void)
{
    static char s_edit[4096];
    char uri[CLI_PATH_MAX_LEN];
    VerbaLinea line = {s_edit, sizeof(s_edit), 0ull};
    sample_uri(uri);
    mmgr_line_put(&line, "{\"documentChanges\":[{\"textDocument\":{\"uri\":\"");
    mmgr_line_put(&line, uri);
    mmgr_line_put(&line, "\",\"version\":2},\"edits\":[");
    edit_written(&line, 0u, "c");
    mmgr_line_put(&line, "]},{\"kind\":\"create\",\"uri\":\"file:///t/b.c\"}]}");
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING(":c", files_edited(s_edit));
}

void test_a_workspace_edit_is_in_a_cell_counted_to_its_files_and_edits_and_released(void)
{
    char uri[CLI_PATH_MAX_LEN];
    static char s_edit[4096];
    VerbaLinea line = {s_edit, sizeof(s_edit), 0ull};
    sample_uri(uri);
    mmgr_line_put(&line, "{\"changes\":{\"");
    mmgr_line_put(&line, uri);
    mmgr_line_put(&line, "\":[");
    edit_written(&line, 1u, "a");
    mmgr_line_put(&line, "]}}");
    (void)mmgr_line_finish(&line);
    (void)files_edited(s_edit);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_servers_edits_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_servers_edits_owner));
    // the workspace edits read are a few files of a few edits each, far below the capacity of every edit and file
    TEST_ASSERT_LESS_THAN_UINT64(16384ull, mmgr_owner_hw(g_cli_servers_edits_owner));
}

void test_edits_are_written_by_utf16_columns_the_line_ends_preserved(void)
{
    static const char s_text[] = "a\xF0\x9F\x98\x80"
                                 "b x\r\nsecond x\r\n";
    const CliTextEdit edits[3] = {{{0u, 3u}, {0u, 4u}, "B", 1ull}, {{1u, 7u}, {1u, 8u}, "yy", 2ull}, {{0u, 5u}, {0u, 9u}, "!", 1ull}};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = s_text, .size = sizeof(s_text) - 1u, .edits = edits, .count = 3u, .out = &out));
    CLI_TEST_ASSERT_TEXT("a\xF0\x9F\x98\x80"
                         "B !\r\nsecond yy\r\n",
                         s_out, out.at);
}

void test_a_completion_is_named_by_what_it_inserts(void)
{
    const unsigned int read = text_parsed("{\"label\":\" printf(const char *, ...)\",\"kind\":3,\"detail\":\"int\",\"filterText\":"
                                          "\"printf\",\"insertText\":\"printf\"}");
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    EMBED_CALL(cli_servers_json_one_internal.item_get, CliServersJsonOneCtx, .json = s_json, .one = read, .items = &s_items);
    const CliCompletion *const item = &s_items.item[0];
    TEST_ASSERT_EQUAL_UINT(1u, s_items.count);
    TEST_ASSERT_TRUE(cli_unity_completes(&s_items, "printf"));
    TEST_ASSERT_EQUAL_STRING("function", item->type);
    CLI_TEST_ASSERT_TEXT("int printf(const char *, ...)", s_items.text + item->detail_at, item->detail_size);
}

void test_a_setting_orior_does_not_send_is_left_to_the_servers_default(void)
{
    TEST_ASSERT_EQUAL_STRING("[null,null]", server_requested("workspace/configuration", "{\"items\":[{},{}]}"));
}

void test_the_settings_on_by_default_stay_on(void)
{
    const unsigned int python = text_parsed(server_requested("workspace/configuration", "{\"items\":[{\"section\":\"python\"},{\"section\":\"python.analysis\"}]}"));
    const unsigned int analysis = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = python, .index = 0u), .name = "analysis");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = analysis, .name = "useLibraryCodeForTypes"), .type = CFG_JSON_TRUE));
}

void test_pythons_analysis_is_alike_either_way(void)
{
    char first[4096];
    char second[4096];
    const unsigned int python = text_parsed(server_requested("workspace/configuration", "{\"items\":[{\"section\":\"python\"},{\"section\":\"python.analysis\"}]}"));
    const unsigned int analysis = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = python, .index = 0u), .name = "analysis");
    VerbaLinea one = {first, sizeof(first), 0ull};
    VerbaLinea two = {second, sizeof(second), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = analysis, .out = &one, .indent = 0u, .sorted = 1);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = python, .index = 1u), .out = &two, .indent = 0u, .sorted = 1);
    TEST_ASSERT_EQUAL_UINT64(one.at, two.at);
    TEST_ASSERT_EQUAL_MEMORY(first, second, one.at);
}

void test_an_edit_the_server_requests_is_applied_and_passed_on(void)
{
    TEST_ASSERT_EQUAL_STRING("{\"applied\":true}", server_requested("workspace/applyEdit", "{\"edit\":{\"changes\":{\"file:///t/a.c\":[]}}}"));
    TEST_ASSERT_EQUAL_UINT(1u, s_event_count);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_events[0], .other = "lsp-edits ", .cap = CLI_POOL_BYTES}));
    TEST_ASSERT_EQUAL_UINT(1u, event_items(0u, NULL));
}

void test_a_files_diagnostics_wake_the_checker_waiting_on_it(void)
{
    release_run_result();
    TEST_ASSERT_TRUE(s_release_woken);
}

void test_diagnostics_after_a_release_pass_on_and_its_clearing_does_not_however_its_uri_is_written(void)
{
    release_run_result();
    TEST_ASSERT_EQUAL_STRING("1,1", s_passed_on_after_clear);
}

void test_a_released_files_diagnostics_are_stored(void)
{
    release_run_result();
    TEST_ASSERT_EQUAL_UINT(1u, s_release_items);
}

void test_a_clearing_after_the_release_ends_passes_on(void)
{
    release_run_result();
    TEST_ASSERT_EQUAL_STRING("1,1,0", s_passed_on_final);
}

void test_an_include_names_a_file(void)
{
    TEST_ASSERT_TRUE(text_names("#include \"shape.h\"\nint x;\n", "shape"));
}

void test_a_longer_name_and_a_comment_do_not_name_a_file(void)
{
    TEST_ASSERT_FALSE(text_names("#include \"shapes.h\"\n/* shape */\n", "shape"));
}

void test_an_import_names_a_module(void)
{
    TEST_ASSERT_TRUE(text_names("import os\nfrom geometry.shape import area\n", "shape"));
}

void test_a_name_set_does_not_name_a_module(void)
{
    TEST_ASSERT_FALSE(text_names("shape = 1\n", "shape"));
}

void test_a_report_of_an_attribute_names_it_and_its_class(void)
{
    static const char s_report[] = "Cannot access attribute \"render\" for class \"Saving*\"\n  Attribute \"render\" "
                                   "is unknown";
    const char *name = NULL;
    const char *class_name = NULL;
    unsigned long long name_size = 0ull;
    unsigned long long class_size = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_servers_length_internal.attribute_get, CliServersLengthCtx, .message = s_report, .length = sizeof(s_report) - 1u, .name_pointer = &name, .name_size = &name_size, .class_name = &class_name, .class_size = &class_size));
    CLI_TEST_ASSERT_TEXT("render", name, name_size);
    CLI_TEST_ASSERT_TEXT("Saving", class_name, class_size);
}

void test_another_report_names_no_attribute(void)
{
    static const char s_other[] = "Import \"x\" could not be resolved";
    const char *name = NULL;
    const char *class_name = NULL;
    unsigned long long name_size = 0ull;
    unsigned long long class_size = 0ull;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_servers_length_internal.attribute_get, CliServersLengthCtx, .message = s_other, .length = sizeof(s_other) - 1u, .name_pointer = &name, .name_size = &name_size, .class_name = &class_name, .class_size = &class_size));
}

// the jobs handed over: how many ran with their bytes, how many ran with none, and each value's count of runs
#define JOBS_THREADS 4u
#define JOBS_EACH 64u
static _Atomic unsigned int s_jobs_run;
static _Atomic unsigned int s_jobs_empty;
static _Atomic unsigned int s_jobs_refused;
static _Atomic unsigned int s_jobs_value[JOBS_THREADS * JOBS_EACH];
static CliThread s_jobs_thread[JOBS_THREADS];
static unsigned int s_jobs_first[JOBS_THREADS];

// a job: its value counted, or the run counted as empty where the bytes are gone
static void jobs_counted(const CliServersJobCfg *args)
{
    if ((args->job == NULL) || (args->size != sizeof(unsigned int)))
    {
        (void)atomic_fetch_add(&s_jobs_empty, 1u);
        return;
    }
    const unsigned int value = *(const unsigned int *)args->job;
    (void)atomic_fetch_add(&s_jobs_value[value], 1u);
    (void)atomic_fetch_add(&s_jobs_run, 1u);
}

// a thread that hands JOBS_EACH jobs over, each a value of its own, and counts the hand-overs refused
static void jobs_handed(void *context)
{
    const unsigned int first = *(const unsigned int *)context;
    for (unsigned int at = 0u; at < JOBS_EACH; at += 1u)
    {
        unsigned int value = first + at;
        if (!EMBED_CALL(cli_servers_run_internal.later, CliServersRunCtx, .run = jobs_counted, .job = &value, .size = sizeof(value)))
        {
            (void)atomic_fetch_add(&s_jobs_refused, 1u);
        }
    }
}

void test_jobs_handed_over_from_four_threads_each_run_once_with_their_bytes(void)
{
    atomic_store(&s_jobs_run, 0u);
    atomic_store(&s_jobs_empty, 0u);
    atomic_store(&s_jobs_refused, 0u);
    for (unsigned int at = 0u; at < JOBS_THREADS * JOBS_EACH; at += 1u)
    {
        atomic_store(&s_jobs_value[at], 0u);
    }
    for (unsigned int at = 0u; at < JOBS_THREADS; at += 1u)
    {
        s_jobs_first[at] = at * JOBS_EACH;
        s_jobs_thread[at] = (CliThread){jobs_handed, &s_jobs_first[at], CLI_PRIORITY_WORK};
        TEST_ASSERT_TRUE(EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_jobs_thread[at]));
    }
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while ((atomic_load(&s_jobs_run) + atomic_load(&s_jobs_empty) < JOBS_THREADS * JOBS_EACH) && (cli_clock_milliseconds() < until))
    {
        cli_thread_yield();
    }
    TEST_ASSERT_EQUAL_UINT(0u, atomic_load(&s_jobs_refused));
    TEST_ASSERT_EQUAL_UINT(0u, atomic_load(&s_jobs_empty));
    TEST_ASSERT_EQUAL_UINT(JOBS_THREADS * JOBS_EACH, atomic_load(&s_jobs_run));
    for (unsigned int at = 0u; at < JOBS_THREADS * JOBS_EACH; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(1u, atomic_load(&s_jobs_value[at]));
    }
}
