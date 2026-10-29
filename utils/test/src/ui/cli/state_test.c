// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// state_test.c: the window's entries' check: what the window stores reads back as it was
#include "../../../../../ui/cli/src/machine/state.h"
#include "cli_check.h"

static char s_folder[CLI_PATH_MAX_LEN];

// whether the file `name` is in the folder
static int state_file_exists(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(s_folder, name, path);
    return query_is_file(path);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    static const char *const s_entries[10][2] = {
        {"scheme", "dark"},
        {"zoom", "1.1"},
        {"fonts", "{\"--code\":\"Fira Code\"}"},
        {"theme.dark", "Night"},
        {"patterns.explorer", "target\n*.log"},
        {"panes", "{\"folder\":true}"},
        {"session.D:/tree", "[\"a.rs\"]"},
        {"search.set", "\"quoted\""},
        {"themes", "[{\"name\":\"A/"
                   "B\",\"base\":\"light\",\"colors\":{}},{\"name\":\"Night\",\"base\":\"dark\",\"colors\":{\"--bg\":"
                   "\"#000\"}}]"},
        {"backup.D:/tree\nsrc/a.rs", "unsaved text"},
    };
    static char s_text[1u << 16u];
    static char s_read[1u << 16u];
    char path[CLI_PATH_MAX_LEN];
    cli_check_folder("state_folder", s_folder);
    cli_full(s_folder, "settings.json", path);
    (void)query_file_written(path, "{\"auto_report\": true}", 21ull);
    cli_full(s_folder, "state.json", path);
    VerbaLinea read = {s_read, sizeof(s_read), 0ull};
    cli_check(!query_is_file(path) && !cli_state_read(s_folder, &read),
              "nothing is read before the window stores anything");
    VerbaLinea changes = {s_text, sizeof(s_text), 0ull};
    mmgr_line_ch(&changes, '{');
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        mmgr_line_put(&changes, (at > 0u) ? "," : "");
        cli_json_quoted(&changes, s_entries[at][0],
                        cellul.len(&(CatenaFinitaCfg){.src = s_entries[at][0], .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(&changes, ':');
        cli_json_quoted(&changes, s_entries[at][1],
                        cellul.len(&(CatenaFinitaCfg){.src = s_entries[at][1], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(&changes, '}');
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_state_save(s_folder, s_text, changes.at, &err_msg), "the entries are stored");
    read.at = 0ull;
    cli_check(cli_state_read(s_folder, &read), "they read back");
    unsigned int back = CLI_JSON_NONE;
    (void)cli_check_parsed(s_read, read.at, &back);
    int same = 1;
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        same &= (cellul.eq(&(CatenaFinitaCfg){
            .src = cli_check_member_text(back, s_entries[at][0]), .other = s_entries[at][1], .cap = CLI_CELLS_POOL}));
    }
    cli_check(same, "every entry reads back as it was");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(back, "auto_report"), .other = "true", .cap = CLI_CELLS_POOL}),
              "a setting of settings.json reads too");
    cli_full(s_folder, "settings.json", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    cli_check(
        (cellul.find(&(CatenaFinitaCfg){
             .src = s_text, .cap = CLI_CELLS_POOL, .other = "\"zoom\": 1.1", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = s_text,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "\"theme.dark\": \"Night\"",
                                            .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_text, .cap = CLI_CELLS_POOL, .other = "panes", .other_cap = CLI_CELLS_POOL}) == NULL),
        "the settings are written to settings.json, and the window's own state is not");
    cli_check(state_file_exists("themes/Night.json") && state_file_exists("themes/A_B.json"),
              "each theme has a file of its own");
    static const char s_gone[] = "{\"zoom\":null,\"themes\":\"[{\\\"name\\\":\\\"Night\\\",\\\"base\\\":\\\"dark\\\","
                                 "\\\"colors\\\":{}}]\",\"backup.D:/tree\\nsrc/a.rs\":null}";
    err_msg.at = 0ull;
    cli_check(cli_state_save(s_folder, s_gone, sizeof(s_gone) - 1u, &err_msg), "the entries gone are stored as gone");
    read.at = 0ull;
    (void)cli_state_read(s_folder, &read);
    (void)cli_check_parsed(s_read, read.at, &back);
    s_read[read.at] = '\0';
    cli_check(
        (cli_check_member_text(back, "zoom")[0] == '\0') &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_read, .cap = CLI_CELLS_POOL, .other = "\"backup.", .other_cap = CLI_CELLS_POOL}) == NULL),
        "zoom and the backup are gone");
    cli_check(!state_file_exists("themes/A_B.json"), "the theme taken out has no file");

    // a stored file that cannot be read whole is not saved over: the text that is not a JSON object stands
    cli_full(s_folder, "state.json", path);
    (void)query_file_written(path, "not an object of entries", 24ull);
    err_msg.at = 0ull;
    static const char s_late[] = "{\"late\":\"x\"}";
    cli_check(!cli_state_save(s_folder, s_late, sizeof(s_late) - 1u, &err_msg) && (err_msg.at != 0ull),
              "a save over a stored file that cannot be read whole is refused with why");
    const unsigned long long left = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    cli_check((left == 24ull) && (memor.cmp(&(MemoriaCfg){.src = s_text, .other = "not an object of entries", .bytes = 24u}) == 0),
              "the stored file stands as it was");
    return cli_checked();
}
