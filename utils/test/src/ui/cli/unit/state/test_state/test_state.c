// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_state.c: the window's saved entries read back as saved, settings go to settings.json, each theme has its own file, a null
// entry removes it, and a stored file that cannot be read whole is not saved over
#include "ui/cli/src/machine/state.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define STATE_ENTRIES 10u

static const char *const s_entries[STATE_ENTRIES][2] = {
    {"scheme", "dark"},
    {"zoom", "1.1"},
    {"fonts", "{\"--code\":\"Fira Code\"}"},
    {"theme.dark", "Night"},
    {"patterns.explorer", "target\n*.log"},
    {"panes", "{\"folder\":true}"},
    {"session.D:/tree", "[\"a.rs\"]"},
    {"search.set", "\"quoted\""},
    {"themes", "[{\"name\":\"A/B\",\"base\":\"light\",\"colors\":{}},{\"name\":\"Night\",\"base\":\"dark\",\"colors\":{\"--bg\":\"#000\"}}]"},
    {"backup.D:/tree\nsrc/a.rs", "unsaved text"},
};

static char s_folder[CLI_PATH_MAX_LEN];
static char s_text[1u << 16u];
static char s_read[1u << 16u];
static char s_error[1024];

// whether the file `name` is in the folder
static int file_exists(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = name, .joined = path);
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
}

// the entries saved
static void entries_saved(void)
{
    VerbaLinea changes = {s_text, sizeof(s_text), 0ull};
    mmgr_line_ch(&changes, '{');
    for (unsigned int at = 0u; at < STATE_ENTRIES; at += 1u)
    {
        mmgr_line_put(&changes, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &changes, .text = s_entries[at][0], .length = CLI_UNITY_LENGTH(s_entries[at][0]));
        mmgr_line_ch(&changes, ':');
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &changes, .text = s_entries[at][1], .length = CLI_UNITY_LENGTH(s_entries[at][1]));
    }
    mmgr_line_ch(&changes, '}');
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_state_folder.save, CliStateFolderCfg, .folder = s_folder, .changes = s_text, .length = changes.at, .err_msg = &err_msg));
}

// what the folder's state reads, parsed: the root's node, the text NUL-terminated in s_read
static unsigned int state_read(void)
{
    VerbaLinea read = {s_read, sizeof(s_read), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_state_out.read, CliStateOutCfg, .folder = s_folder, .out = &read));
    unsigned int root = CLI_JSON_NONE;
    (void)cli_unity_parsed(s_read, read.at, &root);
    s_read[read.at] = '\0';
    return root;
}

// the entries removed by null, the theme A/B gone from the list
static void entries_removed(void)
{
    static const char s_removed[] = "{\"zoom\":null,\"themes\":\"[{\\\"name\\\":\\\"Night\\\",\\\"base\\\":\\\"dark\\\","
                                    "\\\"colors\\\":{}}]\",\"backup.D:/tree\\nsrc/a.rs\":null}";
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_state_folder.save, CliStateFolderCfg, .folder = s_folder, .changes = s_removed, .length = sizeof(s_removed) - 1u, .err_msg = &err_msg));
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("state_folder", s_folder);
    cli_unity_file(s_folder, "settings.json", "{\"auto_report\": true}");
}

void tearDown(void)
{
}

void test_nothing_reads_before_anything_is_saved(void)
{
    VerbaLinea read = {s_read, sizeof(s_read), 0ull};
    TEST_ASSERT_FALSE(file_exists("state.json"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_state_out.read, CliStateOutCfg, .folder = s_folder, .out = &read));
}

void test_every_entry_reads_back_as_saved(void)
{
    entries_saved();
    const unsigned int root = state_read();
    for (unsigned int at = 0u; at < STATE_ENTRIES; at += 1u)
    {
        TEST_ASSERT_EQUAL_STRING_MESSAGE(s_entries[at][1], cli_unity_member_text(root, s_entries[at][0]), s_entries[at][0]);
    }
}

void test_a_setting_in_settings_json_reads_too(void)
{
    entries_saved();
    TEST_ASSERT_EQUAL_STRING("true", cli_unity_member_text(state_read(), "auto_report"));
}

void test_settings_are_written_to_settings_json(void)
{
    entries_saved();
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "settings.json", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    s_text[size] = '\0';
    CLI_TEST_ASSERT_CONTAINS("\"zoom\": 1.1", s_text);
    CLI_TEST_ASSERT_CONTAINS("\"theme.dark\": \"Night\"", s_text);
}

void test_the_windows_own_state_is_not_written_to_settings_json(void)
{
    entries_saved();
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "settings.json", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    s_text[size] = '\0';
    CLI_TEST_ASSERT_NOT_CONTAINS("panes", s_text);
}

void test_each_theme_has_its_own_file(void)
{
    entries_saved();
    TEST_ASSERT_TRUE(file_exists("themes/Night.json"));
    TEST_ASSERT_TRUE(file_exists("themes/A_B.json"));
}

void test_a_null_entry_removes_it(void)
{
    entries_saved();
    entries_removed();
    TEST_ASSERT_EQUAL_STRING("", cli_unity_member_text(state_read(), "zoom"));
    CLI_TEST_ASSERT_NOT_CONTAINS("\"backup.", s_read);
}

void test_a_removed_theme_loses_its_file(void)
{
    entries_saved();
    entries_removed();
    TEST_ASSERT_FALSE(file_exists("themes/A_B.json"));
    TEST_ASSERT_TRUE(file_exists("themes/Night.json"));
}

void test_a_save_over_a_file_that_is_not_an_object_is_refused_with_a_reason(void)
{
    static const char s_late[] = "{\"late\":\"x\"}";
    cli_unity_file(s_folder, "state.json", "not an object of entries");
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_state_folder.save, CliStateFolderCfg, .folder = s_folder, .changes = s_late, .length = sizeof(s_late) - 1u, .err_msg = &err_msg));
    TEST_ASSERT_NOT_EQUAL(0ull, err_msg.at);
}

void test_a_refused_save_leaves_the_stored_file_unchanged(void)
{
    static const char s_late[] = "{\"late\":\"x\"}";
    cli_unity_file(s_folder, "state.json", "not an object of entries");
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    (void)EMBED_CALL(cli_state_folder.save, CliStateFolderCfg, .folder = s_folder, .changes = s_late, .length = sizeof(s_late) - 1u, .err_msg = &err_msg);
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "state.json", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    CLI_TEST_ASSERT_TEXT("not an object of entries", s_text, size);
}

void test_an_entry_is_read_by_its_place_and_its_size_by_its_key(void)
{
    entries_saved();
    CliStateEntries entries = cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = s_folder, .owner = g_cli_text_owner});
    TEST_ASSERT_NOT_NULL(entries.cell);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(STATE_ENTRIES, entries.count);
    int zoom_read = 0;
    for (unsigned int at = 0u; at < entries.count; at += 1u)
    {
        char key_text[CLI_NAME_MAX_LEN];
        char value_text[256];
        const char *const key = cli_state_entry_key(&(CliStateEntryCfg){.entries = &entries, .at = at, .out = key_text, .capacity = sizeof(key_text)});
        const char *const text = cli_state_entry_text(&(CliStateEntryCfg){.entries = &entries, .at = at, .out = value_text, .capacity = sizeof(value_text)});
        TEST_ASSERT_NOT_NULL(key);
        TEST_ASSERT_NOT_NULL(text);
        if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "zoom", .cap = CLI_NAME_MAX_LEN}))
        {
            TEST_ASSERT_EQUAL_STRING("1.1", text);
            zoom_read = 1;
        }
    }
    TEST_ASSERT_TRUE(zoom_read);
    TEST_ASSERT_EQUAL_UINT64(3ull, cli_state_entry_size(&(CliStateEntryCfg){.entries = &entries, .key = "zoom"}));
    TEST_ASSERT_EQUAL_UINT64(CLI_STATE_NONE, cli_state_entry_size(&(CliStateEntryCfg){.entries = &entries, .key = "none"}));
    cli_state_entries_release(&(CliStateEntriesCfg){.entries = &entries, .owner = g_cli_text_owner});
}

void test_the_synced_entries_are_the_settings_and_the_themes_and_a_value_is_read_back_as_the_page_stores_it(void)
{
    static const char s_values[] = "{\"a\":\"text\",\"b\":[1,2]}";
    char text[64];
    VerbaLinea out = {text, sizeof(text), 0ull};
    unsigned int root = CLI_JSON_NONE;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_state_table.is_synced, CliStateCfg, .key = "themes"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_state_table.is_synced, CliStateCfg, .key = "backup.D:/tree\nsrc/a.rs"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_state_table.is_synced, CliStateCfg, .key = "session.D:/tree"));
    const CliJson *const json = cli_unity_parsed(s_values, sizeof(s_values) - 1u, &root);
    EMBED_CALL(cli_state_json.read_back, CliStateJsonCfg, .json = json, .value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "a"), .out = &out);
    CLI_TEST_ASSERT_TEXT("text", text, out.at);
    out.at = 0ull;
    EMBED_CALL(cli_state_json.read_back, CliStateJsonCfg, .json = json, .value = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "b"), .out = &out);
    CLI_TEST_ASSERT_TEXT("[1,2]", text, out.at);
}
