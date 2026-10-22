// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_test.c: the settings' check: a file takes another and a chain that takes a file twice stops; and the tree
// stands over the reader, a system's part over its file, and values name others
#include "../../../../../ui/cli/src/machine/settings.h"
#include "cli_check.h"

static CliChain s_chain;
static CliChain s_other;
static CliLayers s_layers;
static char s_written[1u << 20u];
static char s_root[CLI_PATH_LONGEST];

// `text` written to the file `name` under the root
static void settings_written(const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the chain written and read into the shared tree: the tree, its node into `read`
static const CliJson *settings_chain_read(const CliChain *chain, unsigned int *read)
{
    CliLine out = {s_written, sizeof(s_written), 0ull};
    cli_settings_chain_written(chain, &out);
    return cli_check_parsed(s_written, out.at, read);
}

// whether the strings of an array hold one that holds `part`
static int settings_any_holds(const CliJson *json, unsigned int array, const char *part)
{
    for (unsigned int at = 0u; cli_json_item(json, array, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const text = cli_json_string(json, cli_json_item(json, array, at), NULL);
        if ((text != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = text, .cap = CLI_CELLS_POOL, .other = part, .other_cap = CLI_CELLS_POOL}) != NULL))
        {
            return 1;
        }
    }
    return 0;
}

// a file takes another, and a chain that takes a file twice stops
static void settings_chain_checked(void)
{
    char path[CLI_PATH_LONGEST];
    char tree[CLI_PATH_LONGEST];
    cli_check_folder("settings_chain", s_root);
    cli_full(s_root, "tree/.orior", path);
    (void)query_folder_made(path);
    cli_full(s_root, "tree", tree);
    settings_written(
        "tree/" CLI_SETTINGS_TREE,
        "{\n  // the shared settings, changed\n  \"takes\": \"../../shared.json\",\n  \"zoom\": 1.25,\n}\n");
    settings_written("shared.json", "{ \"takes\": \"base.json\", \"zoom\": 1.1, \"vim\": true }");
    settings_written("base.json", "{ \"sticky\": false }");
    cli_settings_tree(tree, &s_chain);
    unsigned int read = CLI_JSON_NONE;
    const CliJson *json = settings_chain_read(&s_chain, &read);
    const unsigned int files = cli_json_member(json, read, "files");
    const unsigned int problems = cli_json_member(json, read, "problems");
    cli_check(cli_json_is(json, problems, CFG_JSON_ARRAY) && (json->node[problems].count == 0u),
              "the chain has no problem");
    const char *const first = cli_check_member_text(cli_json_item(json, files, 0u), "file");
    const char *const second = cli_check_member_text(cli_json_item(json, files, 1u), "file");
    const char *const third = cli_check_member_text(cli_json_item(json, files, 2u), "file");
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = first, .cap = CLI_CELLS_POOL});
    const size_t shared = cellul.len(&(CatenaFinitaCfg){.src = second, .cap = CLI_CELLS_POOL});
    cli_check(
        (json->node[files].count == 3u) && (length >= 9u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = first + length - 9u, .other = "base.json", .cap = CLI_CELLS_POOL})) &&
            (shared >= 11u) &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = second + shared - 11u, .other = "shared.json", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = third, .other = CLI_SETTINGS_TREE, .cap = CLI_CELLS_POOL})),
        "the files run from the one taken first to the tree's own");
    settings_written("base.json", "{ \"takes\": \"shared.json\" }");
    cli_settings_chain_given(&s_chain);
    cli_settings_tree(tree, &s_chain);
    json = settings_chain_read(&s_chain, &read);
    cli_check((json->node[cli_json_member(json, read, "files")].count == 3u) &&
                  settings_any_holds(json, cli_json_member(json, read, "problems"), "taken twice"),
              "a chain that takes a file twice stops, and says so");
    settings_written("base.json", "{ \"takes\": \"gone.json\" }");
    cli_settings_chain_given(&s_chain);
    cli_settings_tree(tree, &s_chain);
    json = settings_chain_read(&s_chain, &read);
    cli_check(settings_any_holds(json, cli_json_member(json, read, "problems"), "gone.json"),
              "a file not there is said");
    cli_full(s_root, "none.json", path);
    cli_settings_chain_given(&s_chain);
    cli_settings_chain(path, s_root, &s_chain);
    json = settings_chain_read(&s_chain, &read);
    cli_check(json->node[cli_json_member(json, read, "problems")].count == 0u, "a file not there is no problem");
    cli_settings_chain_given(&s_chain);
}

// a text of the layers' map `map` at `key`, the empty text where it has none
static const char *settings_at(const CliJson *json, unsigned int layers, const char *map, const char *key)
{
    const char *const text =
        cli_json_string(json, cli_json_member(json, cli_json_member(json, layers, map), key), NULL);
    return (text != NULL) ? text : "";
}

// the tree stands over the reader, a system's part over its file, and values name others
static void settings_layers_checked(void)
{
    const char *const system = cli_settings_system();
    const char *const other =
        (cellul.eq(&(CatenaFinitaCfg){.src = system, .other = "windows", .cap = CLI_CELLS_POOL})) ? "linux" : "windows";
    static char s_reader[4096];
    CliLine reader = {s_reader, sizeof(s_reader), 0ull};
    cli_line_text(&reader, "{\"files\":[{\"file\":\"base.json\",\"values\":{\"vim\":\"true\",\"margin\":\"true\"}},"
                           "{\"file\":\"settings.json\",\"values\":{\"takes\":\"base.json\",\"zoom\":1.1,"
                           "\"margin\":\"false\",\"");
    cli_line_text(&reader, system);
    cli_line_text(&reader, "\":{\"sticky\":\"false\"},\"");
    cli_line_text(&reader, other);
    cli_line_text(&reader, "\":{\"zoom\":3},\"fonts\":{\"--code\":\"${setting:code}\"},\"code\":\"Fira Code\","
                           "\"machine\":{\"brackets\":\"false\"}}}],\"problems\":[]}");
    static const char s_tree[] = "{\"files\":[{\"file\":\"" CLI_SETTINGS_TREE "\",\"values\":{\"zoom\":1.25,"
                                 "\"signing.windows\":\"mine\",\"crumbs\":\"${system}\",\"path\":\"${env:PATH}\","
                                 "\"lost\":\"${nothing}\",\"[markdown]\":{\"theme.dark\":\"Paper\",\"vim\":true}}}],"
                                 "\"problems\":[]}";
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_settings_chain_read(s_reader, reader.at, &s_chain, &said) &&
                  cli_settings_chain_read(s_tree, sizeof(s_tree) - 1u, &s_other, &said),
              "the two chains read");
    cli_settings_layers(&s_chain, &s_other, "D:/tree", &s_layers);
    CliLine out = {s_written, sizeof(s_written), 0ull};
    cli_settings_layers_written(&s_layers, &out);
    unsigned int layers = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_written, out.at, &layers);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = settings_at(json, layers, "over", "zoom"), .other = "1.25", .cap = CLI_CELLS_POOL}),
              "the tree's stands over the reader's");
    const unsigned int zoom = cli_json_member(json, cli_json_member(json, layers, "from"), "zoom");
    cli_check((cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, zoom, 0u), "file"),
                                            .other = CLI_SETTINGS_TREE,
                                            .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, zoom, 0u), "text"),
                                                .other = "1.25",
                                                .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, zoom, 1u), "file"),
                                                .other = "settings.json",
                                                .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, zoom, 1u), "text"),
                                                .other = "1.1",
                                                .cap = CLI_CELLS_POOL})),
              "each file that sets zoom is named, the one in force first");
    const unsigned int sticky = cli_json_member(json, cli_json_member(json, layers, "from"), "sticky");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = settings_at(json, layers, "over", "sticky"), .other = "false", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, sticky, 0u), "part"),
                                                .other = system,
                                                .cap = CLI_CELLS_POOL})),
              "the system's part of the reader's own stands over its others");
    cli_check((cli_json_member(json, cli_json_member(json, layers, "over"), "margin") == CLI_JSON_NONE) &&
                  (cli_json_member(json, cli_json_member(json, layers, "under"), "margin") == CLI_JSON_NONE),
              "the reader's own settings.json is the page's own");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = settings_at(json, layers, "under", "vim"), .other = "true", .cap = CLI_CELLS_POOL}),
              "a file the reader's takes stands under it");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = settings_at(json, layers, "over", "fonts"),
                                           .other = "{\"--code\":\"Fira Code\"}",
                                           .cap = CLI_CELLS_POOL}),
              "a value names another's");
    int own = 0;
    const unsigned int owns = cli_json_member(json, layers, "own");
    for (unsigned int at = 0u; cli_json_item(json, owns, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const key = cli_json_string(json, cli_json_item(json, owns, at), NULL);
        own |= (key != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "fonts", .cap = CLI_CELLS_POOL}));
    }
    cli_check(own, "fonts is the reader's own");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = settings_at(json, layers, "over", "crumbs"), .other = system, .cap = CLI_CELLS_POOL}),
              "${system} names the system");
    char path[1u << 15u];
    const int has_path = query_environment_read("PATH", path, sizeof(path));
    cli_check(!has_path ||
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = settings_at(json, layers, "over", "path"), .other = path, .cap = CLI_CELLS_POOL})),
              "${env:PATH} names the PATH");
    const unsigned int problems = cli_json_member(json, layers, "problems");
    cli_check((cli_json_member(json, cli_json_member(json, layers, "over"), "signing.windows") == CLI_JSON_NONE) &&
                  settings_any_holds(json, problems, "signing.windows"),
              "a setting with a system in its name the file does not take is a problem");
    cli_check(settings_any_holds(json, problems, "${nothing}"), "a value that names nothing is a problem");
    const unsigned int markdown = cli_json_member(json, cli_json_member(json, layers, "kinds"), "markdown");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(markdown, "theme.dark"), .other = "Paper", .cap = CLI_CELLS_POOL}),
              "a kind's setting is kept for it");
    cli_check((cli_json_member(json, markdown, "vim") == CLI_JSON_NONE) &&
                  settings_any_holds(json, problems, "[markdown] sets vim"),
              "a kind may not set what no kind sets");
    const unsigned int theme = cli_json_member(json, cli_json_member(json, layers, "from"), "[markdown] theme.dark");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, theme, 0u), "file"),
                                           .other = CLI_SETTINGS_TREE,
                                           .cap = CLI_CELLS_POOL}),
              "a kind's setting names its file");
    const unsigned int files = cli_json_member(json, layers, "files");
    const char *const named[3] = {cli_json_string(json, cli_json_item(json, files, 0u), NULL),
                                  cli_json_string(json, cli_json_item(json, files, 1u), NULL),
                                  cli_json_string(json, cli_json_item(json, files, 2u), NULL)};
    cli_check((named[0] != NULL) && (named[1] != NULL) && (named[2] != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = named[0], .other = "base.json", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = named[1], .other = "settings.json", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = named[2], .other = CLI_SETTINGS_TREE, .cap = CLI_CELLS_POOL})),
              "the files are the reader's then the tree's");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(layers, "own_file"), .other = "settings.json", .cap = CLI_CELLS_POOL}),
              "the reader's own file is named");
    const unsigned int brackets = cli_json_member(json, cli_json_member(json, layers, "from"), "brackets");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = settings_at(json, layers, "over", "brackets"), .other = "false", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, brackets, 0u), "part"),
                                                .other = "machine",
                                                .cap = CLI_CELLS_POOL})),
              "the part for this machine alone stands over the rest");
    cli_settings_layers_given(&s_layers);
    cli_settings_chain_given(&s_chain);
    cli_settings_chain_given(&s_other);
}

int main(void)
{
    settings_chain_checked();
    settings_layers_checked();
    cli_check((g_cli_settings_cells.holds == 0ull) && (g_cli_settings_cells.hw != 0ull),
              "the settings are read and laid in cells each given back");
    return cli_checked();
}
