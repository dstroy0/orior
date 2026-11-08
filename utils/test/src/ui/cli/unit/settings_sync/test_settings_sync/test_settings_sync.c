// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_settings_sync.c: settings travel by a file, are compared before they are applied, and an application is undone; and settings travel by a
// folder that is a repository and are committed
#include "ui/cli/src/code/git.h"
#include "ui/cli/src/machine/settings_sync_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliTransferred s_packed;
static CliTransferred s_there;
static char s_out[1u << 20u];
static char s_now[4096];
static char s_root[CLI_PATH_MAX_LEN];
static char s_one[CLI_PATH_MAX_LEN];
static char s_two[CLI_PATH_MAX_LEN];
static char s_file[CLI_PATH_MAX_LEN];
static char s_home[CLI_PATH_MAX_LEN];
static char s_repo[CLI_PATH_MAX_LEN];
static char s_id[256];
static char s_error[1024];
static VerbaLinea s_err_msg;
static const CliJson *s_json;
static unsigned int s_settings;

// `text` written to the file `name` under the root, its folder made
static void file_written(const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    if (EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = path, .out = folder))
    {
        TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder));
    }
    cli_unity_file(s_root, name, text);
}

// the file `name` under the root read into s_now, NUL-terminated
static void file_read(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_now, sizeof(s_now) - 1u);
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    s_now[size] = '\0';
}

// whether the file `name` is under the root
static int file_is(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = name, .joined = path);
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
}

// two homes written and the first packed into s_packed
static void first_home_packed(void)
{
    cli_unity_folder("settings_sync", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "one", .joined = s_one);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "two", .joined = s_two);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_two));
    file_written("one/settings.json", "{\"zoom\": 1.25, \"vim\": true, \"memory-budget\": 900, \"machine\": {\"fonts\": {}}}");
    file_written("one/themes/Night.json", "{\"name\": \"Night\"}");
    file_written("one/plugins/marks/plugin.json", "{\"id\": \"marks\"}");
    file_written("one/plugin_settings/marks.json", "{\"every\": 3}");
    file_written("one/state.json", "{\"panes\": {}}");
    file_written("two/settings.json", "{\"zoom\": 1.0, \"sticky\": false, \"memory-budget\": 400, \"machine\": {\"zoom\": 2}}");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync.pack, CliSettingsSyncCfg, .home = s_one, .packed = &s_packed, .err_msg = &s_err_msg));
}

// the first home sent to a file and read back into s_there
static void file_read_back(void)
{
    first_home_packed();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "synced.json", .joined = s_file);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync_home_to.send, CliSettingsSyncHomeToCfg, .home = s_one, .to = s_file, .out = &out, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync_from.read, CliSettingsSyncFromCfg, .from = s_file, .there_out = &s_there, .err_msg = &s_err_msg));
}

// the differences of the second home from the file, parsed: the array's node
static unsigned int differences_read(void)
{
    file_read_back();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_settings_sync_home.differences, CliSettingsSyncHomeCfg, .home = s_two, .there = &s_there, .out = &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// the chosen files and settings applied to the second home, the record's id in s_id and its settings file parsed: s_settings its node
static void settings_applied(void)
{
    static const char *const s_chosen_files[3] = {"settings.json", "themes/Night.json", "../outside.json"};
    static const char *const s_chosen_keys[2] = {"zoom", "sticky"};
    file_read_back();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync_home.take, CliSettingsSyncHomeCfg, .home = s_two, .from = "synced.json", .there = &s_there, .positions = s_chosen_files, .count = 3u, .keys = s_chosen_keys, .key_count = 2u, .out = &out, .err_msg = &s_err_msg));
    unsigned int allocating = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &allocating);
    const char *const id = cli_unity_member_text(allocating, "id");
    memor.cpy(&(MemoriaCfg){.dst = s_id, .src = id, .bytes = CLI_UNITY_LENGTH(id) + 1ull});
    file_read("two/settings.json");
    s_json = cli_unity_parsed(s_now, CLI_UNITY_LENGTH(s_now), &s_settings);
}

// the count of applications recorded in the second home
static unsigned int applications_recorded(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_settings_sync_home_to.allocations, CliSettingsSyncHomeToCfg, .home = s_two, .out = &out);
    unsigned int allocations = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &allocations);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = allocations, .type = CFG_JSON_ARRAY));
    return s_json->node[allocations].count;
}

// git run in the repository with `count` words, its output in `out`: whether it ended well
static int git_run_result(const char *const *words, unsigned int count, VerbaLinea *out)
{
    out->at = 0ull;
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_repo, .request = words, .count = count, .out = out, .err_msg = &err_msg);
}

// a home written and a folder made a repository
static void repository_created(void)
{
    static const char *const s_init[2] = {"init", "-q"};
    static const char *const s_email[3] = {"config", "user.email", "orior@example.com"};
    static const char *const s_name[3] = {"config", "user.name", "orior"};
    cli_unity_folder("settings_sync_repo", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "home", .joined = s_home);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "repo", .joined = s_repo);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = s_repo));
    file_written("home/settings.json", "{\"zoom\": 1.1}");
    file_written("home/themes/Day.json", "{\"name\": \"Day\"}");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(git_run_result(s_init, 2u, &out));
    TEST_ASSERT_TRUE(git_run_result(s_email, 3u, &out));
    TEST_ASSERT_TRUE(git_run_result(s_name, 3u, &out));
    file_written("repo/themes/Gone.json", "{}");
}

// the home sent to the repository, its report NUL-terminated in s_out: whether it was sent
static int repository_sent(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    s_err_msg.at = 0ull;
    const int sent = EMBED_CALL(cli_settings_sync_home_to.send, CliSettingsSyncHomeToCfg, .home = s_home, .to = s_repo, .out = &out, .err_msg = &s_err_msg);
    s_out[out.at] = '\0';
    return sent;
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
}

void tearDown(void)
{
}

void test_what_travels_is_the_settings_themes_plugins_and_their_settings(void)
{
    static const char *const s_positions[4] = {"plugin_settings/marks.json", "plugins/marks/plugin.json", "settings.json", "themes/Night.json"};
    first_home_packed();
    TEST_ASSERT_EQUAL_UINT(4u, s_packed.count);
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        TEST_ASSERT_EQUAL_STRING(s_positions[at], EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position, CliSettingsSyncTransferredAtCtx, .synced = &s_packed, .at = at));
    }
}

void test_this_machines_own_settings_stay(void)
{
    first_home_packed();
    const unsigned int settings = EMBED_CALL(cli_settings_sync_transferred_internal.transferred_find, CliSettingsSyncTransferredCtx, .synced = &s_packed, .place = "settings.json");
    TEST_ASSERT_LESS_THAN_UINT(s_packed.count, settings);
    CLI_TEST_ASSERT_NOT_CONTAINS("memory-budget", EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value, CliSettingsSyncTransferredAtCtx, .synced = &s_packed, .at = settings));
    CLI_TEST_ASSERT_NOT_CONTAINS("machine", EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value, CliSettingsSyncTransferredAtCtx, .synced = &s_packed, .at = settings));
}

void test_a_file_reads_back_as_what_was_packed(void)
{
    file_read_back();
    TEST_ASSERT_EQUAL_UINT(s_packed.count, s_there.count);
    for (unsigned int at = 0u; at < s_there.count; at += 1u)
    {
        TEST_ASSERT_EQUAL_STRING(EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position, CliSettingsSyncTransferredAtCtx, .synced = &s_packed, .at = at), EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_position, CliSettingsSyncTransferredAtCtx, .synced = &s_there, .at = at));
        TEST_ASSERT_EQUAL_STRING(EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value, CliSettingsSyncTransferredAtCtx, .synced = &s_packed, .at = at), EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value, CliSettingsSyncTransferredAtCtx, .synced = &s_there, .at = at));
    }
}

void test_the_settings_that_differ_are_named_and_this_machines_own_are_not(void)
{
    static const char *const s_keys[3] = {"sticky", "vim", "zoom"};
    const unsigned int found = differences_read();
    unsigned int settings = CLI_JSON_NONE;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int place = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at);
        settings = cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(place, "place"), .other = "settings.json", .cap = CLI_POOL_BYTES}) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = place, .name = "settings") : settings;
    }
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = settings, .type = CFG_JSON_ARRAY));
    TEST_ASSERT_EQUAL_UINT(3u, s_json->node[settings].count);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        TEST_ASSERT_EQUAL_STRING(s_keys[at], cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = settings, .index = at), "key"));
    }
}

void test_a_file_only_there_is_reported_to_be_there(void)
{
    const unsigned int found = differences_read();
    const char *how = "";
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int place = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = at);
        how = cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(place, "place"), .other = "themes/Night.json", .cap = CLI_POOL_BYTES}) ? cli_unity_member_text(place, "how") : how;
    }
    TEST_ASSERT_EQUAL_STRING("there", how);
}

void test_a_chosen_setting_is_applied(void)
{
    settings_applied();
    const unsigned int zoom = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_settings, .name = "zoom");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = zoom, .type = CFG_JSON_NUMBER));
    CLI_TEST_ASSERT_TEXT("1.25", s_json->text + s_json->node[zoom].text, s_json->node[zoom].length);
}

void test_a_chosen_setting_the_file_does_not_set_is_removed(void)
{
    settings_applied();
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_settings, .name = "sticky"));
}

void test_a_setting_not_chosen_is_not_applied(void)
{
    settings_applied();
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_settings, .name = "vim"));
}

void test_this_machines_own_setting_stays(void)
{
    settings_applied();
    const unsigned int budget = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_settings, .name = "memory-budget");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = budget, .type = CFG_JSON_NUMBER));
    CLI_TEST_ASSERT_TEXT("400", s_json->text + s_json->node[budget].text, s_json->node[budget].length);
}

void test_the_part_for_this_machine_alone_stays(void)
{
    settings_applied();
    const unsigned int zoom = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = s_settings, .name = "machine"), .name = "zoom");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = s_json, .node = zoom, .type = CFG_JSON_NUMBER));
    TEST_ASSERT_EQUAL_CHAR('2', s_json->text[s_json->node[zoom].text]);
}

void test_a_chosen_file_is_applied_and_none_outside_the_folder(void)
{
    settings_applied();
    TEST_ASSERT_TRUE(file_is("two/themes/Night.json"));
    TEST_ASSERT_FALSE(file_is("outside.json"));
}

void test_an_application_is_recorded(void)
{
    settings_applied();
    TEST_ASSERT_EQUAL_UINT(1u, applications_recorded());
}

void test_an_undo_writes_a_file_back_as_it_was(void)
{
    settings_applied();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync_home_out.undo, CliSettingsSyncHomeOutCfg, .home = s_two, .id = s_id, .out = &out, .err_msg = &s_err_msg));
    file_read("two/settings.json");
    CLI_TEST_ASSERT_CONTAINS("\"sticky\": false", s_now);
}

void test_an_undo_removes_a_file_the_application_created(void)
{
    settings_applied();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync_home_out.undo, CliSettingsSyncHomeOutCfg, .home = s_two, .id = s_id, .out = &out, .err_msg = &s_err_msg));
    TEST_ASSERT_FALSE(file_is("two/themes/Night.json"));
}

void test_an_undo_removes_the_record(void)
{
    settings_applied();
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync_home_out.undo, CliSettingsSyncHomeOutCfg, .home = s_two, .id = s_id, .out = &out, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_UINT(0u, applications_recorded());
}

void test_a_file_that_transfers_no_settings_is_refused(void)
{
    cli_unity_folder("settings_sync", s_root);
    file_written("other.json", "{\"files\": {}}");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "other.json", .joined = s_file);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_settings_sync_from.read, CliSettingsSyncFromCfg, .from = s_file, .there_out = &s_there, .err_msg = &s_err_msg));
    (void)mmgr_line_finish(&s_err_msg);
    CLI_TEST_ASSERT_CONTAINS("no settings orior_cli synced", s_error);
}

void test_settings_are_sent_to_a_repository_and_committed(void)
{
    repository_created();
    TEST_ASSERT_TRUE(repository_sent());
    CLI_TEST_ASSERT_CONTAINS("committed", s_out);
}

void test_the_repository_contains_what_travels_and_no_more(void)
{
    repository_created();
    TEST_ASSERT_TRUE(repository_sent());
    TEST_ASSERT_TRUE(file_is("repo/themes/Day.json"));
    TEST_ASSERT_FALSE(file_is("repo/themes/Gone.json"));
}

void test_one_commit_is_created(void)
{
    static const char *const s_log[2] = {"log", "--format=%s"};
    repository_created();
    TEST_ASSERT_TRUE(repository_sent());
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)git_run_result(s_log, 2u, &out);
    // git's answer comes back trimmed: a log that is not empty has one line more than it has line ends
    unsigned int lines = (out.at != 0ull) ? 1u : 0u;
    for (unsigned long long at = 0ull; at < out.at; at += 1ull)
    {
        lines += (s_out[at] == '\n') ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT(1u, lines);
}

void test_sending_what_is_there_commits_nothing(void)
{
    repository_created();
    TEST_ASSERT_TRUE(repository_sent());
    TEST_ASSERT_TRUE(repository_sent());
    CLI_TEST_ASSERT_CONTAINS("already", s_out);
}

void test_the_repository_reads_back_what_was_sent(void)
{
    repository_created();
    TEST_ASSERT_TRUE(repository_sent());
    s_err_msg.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_settings_sync_from.read, CliSettingsSyncFromCfg, .from = s_repo, .there_out = &s_there, .err_msg = &s_err_msg));
    const unsigned int day = EMBED_CALL(cli_settings_sync_transferred_internal.transferred_find, CliSettingsSyncTransferredCtx, .synced = &s_there, .place = "themes/Day.json");
    TEST_ASSERT_LESS_THAN_UINT(s_there.count, day);
    TEST_ASSERT_EQUAL_STRING("{\"name\": \"Day\"}", EMBED_CALL(cli_settings_sync_transferred_at_internal.transferred_value, CliSettingsSyncTransferredAtCtx, .synced = &s_there, .at = day));
}
