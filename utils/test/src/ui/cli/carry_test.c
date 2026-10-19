// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_test.c: carrying's check: settings go by a file, differ before they are taken, and a taking is undone; and
// settings go by a folder that is a repository and are committed
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/machine/carry_internal.h"
#include "cli_check.h"

static CliCarried s_packed;
static CliCarried s_there;
static char s_out[1u << 20u];
static char s_root[CLI_PATH_LONGEST];

// `text` written to the file `name` under the root, its folder made
static void carry_written(const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    if (cli_path_parent(path, folder))
    {
        (void)query_folder_made(folder);
    }
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the file `name` under the root read into `text`, which holds `room`, ended by a 0
static void carry_read(const char *name, char *text, unsigned long long room)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, room - 1u);
    text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
}

// whether the file `name` is under the root
static int carry_is(const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    return query_is_file(path);
}

// settings go by a file, differ before they are taken, and a taking is undone
static void carry_by_file(void)
{
    char one[CLI_PATH_LONGEST];
    char two[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("carry", s_root);
    cli_full(s_root, "one", one);
    cli_full(s_root, "two", two);
    (void)query_folder_made(two);
    carry_written("one/settings.json",
                  "{\"zoom\": 1.25, \"vim\": true, \"memory-budget\": 900, \"machine\": {\"fonts\": {}}}");
    carry_written("one/themes/Night.json", "{\"name\": \"Night\"}");
    carry_written("one/plugins/marks/plugin.json", "{\"id\": \"marks\"}");
    carry_written("one/plugin_settings/marks.json", "{\"every\": 3}");
    carry_written("one/state.json", "{\"panes\": {}}");
    carry_written("two/settings.json",
                  "{\"zoom\": 1.0, \"sticky\": false, \"memory-budget\": 400, \"machine\": {\"zoom\": 2}}");
    cli_check(cli_carry_pack(one, &s_packed, &said), "the reader's settings pack");
    static const char *const s_places[4] = {"plugin_settings/marks.json", "plugins/marks/plugin.json", "settings.json",
                                            "themes/Night.json"};
    int places = (s_packed.count == 4u);
    for (unsigned int at = 0u; places && (at < 4u); at += 1u)
    {
        places = (cellul.eq(
            &(CatenaFinitaCfg){.src = cli_carried_place(&s_packed, at), .other = s_places[at], .cap = CLI_CELLS_POOL}));
    }
    cli_check(places, "what travels is the settings, the themes, the plugins and their settings");
    const unsigned int settings = cli_carried_find(&s_packed, "settings.json");
    cli_check((settings < s_packed.count) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = cli_carried_value(&s_packed, settings),
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "memory-budget",
                                                  .other_cap = CLI_CELLS_POOL}) == NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = cli_carried_value(&s_packed, settings),
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "machine",
                                                  .other_cap = CLI_CELLS_POOL}) == NULL),
              "this machine's own settings stay");
    cli_full(s_root, "carried.json", file);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    said.at = 0ull;
    cli_check(cli_carry_send(one, file, &out, &said), "the settings are sent to a file");
    said.at = 0ull;
    int same = cli_carry_read(file, &s_there, &said) && (s_there.count == s_packed.count);
    for (unsigned int at = 0u; same && (at < s_there.count); at += 1u)
    {
        same = (cellul.eq(&(CatenaFinitaCfg){.src = cli_carried_place(&s_there, at),
                                             .other = cli_carried_place(&s_packed, at),
                                             .cap = CLI_CELLS_POOL})) &&
               (cellul.eq(&(CatenaFinitaCfg){.src = cli_carried_value(&s_there, at),
                                             .other = cli_carried_value(&s_packed, at),
                                             .cap = CLI_CELLS_POOL}));
    }
    cli_check(same, "the file reads back as what was packed");
    out.at = 0ull;
    cli_carry_differences(two, &s_there, &out);
    unsigned int found = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_out, out.at, &found);
    unsigned int settings_node = CLI_JSON_NONE;
    const char *night = "";
    for (unsigned int at = 0u; cli_json_item(json, found, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one_place = cli_json_item(json, found, at);
        settings_node =
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(one_place, "place"), .other = "settings.json", .cap = CLI_CELLS_POOL}))
                ? cli_json_member(json, one_place, "settings")
                : settings_node;
        night =
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(one_place, "place"), .other = "themes/Night.json", .cap = CLI_CELLS_POOL}))
                ? cli_check_member_text(one_place, "how")
                : night;
    }
    static const char *const s_keys[3] = {"sticky", "vim", "zoom"};
    int keys = cli_json_is(json, settings_node, CFG_JSON_ARRAY) && (json->node[settings_node].count == 3u);
    for (unsigned int at = 0u; keys && (at < 3u); at += 1u)
    {
        keys =
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, settings_node, at), "key"),
                                          .other = s_keys[at],
                                          .cap = CLI_CELLS_POOL}));
    }
    cli_check(keys, "the settings that differ are named, this machine's own differ not");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = night, .other = "there", .cap = CLI_CELLS_POOL}),
              "a file only there is said to be there");
    static const char *const s_take[3] = {"settings.json", "themes/Night.json", "../outside.json"};
    static const char *const s_take_keys[2] = {"zoom", "sticky"};
    out.at = 0ull;
    said.at = 0ull;
    const int taken = cli_carry_take(two, "carried.json", &s_there, s_take, 3u, s_take_keys, 2u, &out, &said);
    cli_check(taken, "the chosen files and settings are taken");
    unsigned int taking = CLI_JSON_NONE;
    json = cli_check_parsed(s_out, out.at, &taking);
    char id[256];
    memor.cpy(&(MemoriaCfg){
        .dst = id,
        .src = cli_check_member_text(taking, "id"),
        .bytes =
            cellul.len(&(CatenaFinitaCfg){.src = cli_check_member_text(taking, "id"), .cap = CLI_CELLS_POOL}) + 1u});
    static char s_now[4096];
    carry_read("two/settings.json", s_now, sizeof(s_now));
    unsigned int now = CLI_JSON_NONE;
    json = cli_check_parsed(s_now, cellul.len(&(CatenaFinitaCfg){.src = s_now, .cap = CLI_CELLS_POOL}), &now);
    const unsigned int zoom = cli_json_member(json, now, "zoom");
    cli_check(
        cli_json_is(json, zoom, CFG_JSON_NUMBER) && (json->node[zoom].length == 4u) &&
            (memor.cmp(&(MemoriaCfg){.src = json->text + json->node[zoom].text, .other = "1.25", .bytes = 4u}) == 0),
        "a chosen setting is taken");
    cli_check(cli_json_member(json, now, "sticky") == CLI_JSON_NONE,
              "a setting there does not set is taken out where chosen");
    cli_check(cli_json_member(json, now, "vim") == CLI_JSON_NONE, "one not chosen is not taken");
    const unsigned int budget = cli_json_member(json, now, "memory-budget");
    cli_check(
        cli_json_is(json, budget, CFG_JSON_NUMBER) && (json->node[budget].length == 3u) &&
            (memor.cmp(&(MemoriaCfg){.src = json->text + json->node[budget].text, .other = "400", .bytes = 3u}) == 0),
        "this machine's own stays");
    const unsigned int machine_zoom = cli_json_member(json, cli_json_member(json, now, "machine"), "zoom");
    cli_check(cli_json_is(json, machine_zoom, CFG_JSON_NUMBER) && (json->text[json->node[machine_zoom].text] == '2'),
              "the part for this machine alone stays");
    cli_check(carry_is("two/themes/Night.json") && !carry_is("outside.json"),
              "a chosen file is taken, and none outside the folder");
    out.at = 0ull;
    cli_carry_takings(two, &out);
    unsigned int takings = CLI_JSON_NONE;
    json = cli_check_parsed(s_out, out.at, &takings);
    cli_check(cli_json_is(json, takings, CFG_JSON_ARRAY) && (json->node[takings].count == 1u), "the taking is kept");
    out.at = 0ull;
    said.at = 0ull;
    cli_check(cli_carry_undo(two, id, &out, &said), "the taking is undone");
    carry_read("two/settings.json", s_now, sizeof(s_now));
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = s_now, .cap = CLI_CELLS_POOL, .other = "\"sticky\": false", .other_cap = CLI_CELLS_POOL}) !=
                  NULL,
              "a file it wrote over is written back as it was");
    cli_check(!carry_is("two/themes/Night.json"), "a file the taking made is taken out");
    out.at = 0ull;
    cli_carry_takings(two, &out);
    json = cli_check_parsed(s_out, out.at, &takings);
    cli_check(cli_json_is(json, takings, CFG_JSON_ARRAY) && (json->node[takings].count == 0u), "no taking is kept");
    carry_written("other.json", "{\"files\": {}}");
    cli_full(s_root, "other.json", file);
    said.at = 0ull;
    const int other = cli_carry_read(file, &s_there, &said);
    (void)cli_line_closed(&said);
    cli_check(!other && (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                        .cap = CLI_CELLS_POOL,
                                                        .other = "no settings orior carried",
                                                        .other_cap = CLI_CELLS_POOL}) != NULL),
              "a file that carries no settings is refused");
}

// git run in `repo` with `count` words: what it wrote into `out`
static int carry_git(const char *repo, const char *const *words, unsigned int count, CliLine *out)
{
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    out->at = 0ull;
    return cli_git_run(repo, words, count, out, &said);
}

// settings go by a folder that is a repository and are committed
static void carry_by_repository(void)
{
    char home[CLI_PATH_LONGEST];
    char repo[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("carry_repo", s_root);
    cli_full(s_root, "home", home);
    cli_full(s_root, "repo", repo);
    (void)query_folder_made(repo);
    carry_written("home/settings.json", "{\"zoom\": 1.1}");
    carry_written("home/themes/Day.json", "{\"name\": \"Day\"}");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    static const char *const s_init[2] = {"init", "-q"};
    static const char *const s_email[3] = {"config", "user.email", "orior@example.com"};
    static const char *const s_name[3] = {"config", "user.name", "orior"};
    cli_check(carry_git(repo, s_init, 2u, &out) && carry_git(repo, s_email, 3u, &out) &&
                  carry_git(repo, s_name, 3u, &out),
              "the folder is made a repository");
    carry_written("repo/themes/Gone.json", "{}");
    out.at = 0ull;
    const int sent = cli_carry_send(home, repo, &out, &said);
    s_out[out.at] = '\0';
    cli_check(
        sent && (cellul.find(&(CatenaFinitaCfg){
                     .src = s_out, .cap = CLI_CELLS_POOL, .other = "committed", .other_cap = CLI_CELLS_POOL}) != NULL),
        "the settings are sent and committed");
    cli_check(carry_is("repo/themes/Day.json") && !carry_is("repo/themes/Gone.json"),
              "the folder holds what travels and no more");
    static const char *const s_log[2] = {"log", "--format=%s"};
    (void)carry_git(repo, s_log, 2u, &out);
    // git's answer comes back trimmed, so a log that is not empty holds one line more than it holds line ends
    unsigned int lines = (out.at != 0ull) ? 1u : 0u;
    for (unsigned long long at = 0ull; at < out.at; at += 1ull)
    {
        lines += (s_out[at] == '\n') ? 1u : 0u;
    }
    cli_check(lines == 1u, "one commit is made");
    out.at = 0ull;
    said.at = 0ull;
    const int again = cli_carry_send(home, repo, &out, &said);
    s_out[out.at] = '\0';
    cli_check(again &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "already", .other_cap = CLI_CELLS_POOL}) != NULL),
              "sending what is there already commits nothing");
    said.at = 0ull;
    const int read = cli_carry_read(repo, &s_there, &said);
    const unsigned int day = cli_carried_find(&s_there, "themes/Day.json");
    cli_check(read && (day < s_there.count) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_carried_value(&s_there, day), .other = "{\"name\": \"Day\"}", .cap = CLI_CELLS_POOL})),
              "the repository reads back what was sent");
}

int main(void)
{
    carry_by_file();
    carry_by_repository();
    return cli_checked();
}
