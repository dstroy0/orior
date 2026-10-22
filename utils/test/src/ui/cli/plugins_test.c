// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_test.c: the plugins' check: every bundled plugin reads and names itself, a file opens by its name, its path
// or its extension, a name becomes an id, a plugin from nothing holds what was asked, a plugin from another takes its
// grammar, and creating writes once and replaces only when asked
#include "../../../../../ui/cli/src/plugins/plugins.h"
#include "../../../../../ui/cli/src/window/window.h"
#include "cli_check.h"

static CliJsonNode s_node[1u << 16u];
static char s_text[1u << 20u];
static CfgJsonToken s_token[1u << 16u];
static CliJson s_json;
static char s_out[1u << 18u];
static CliLanguages s_languages;

// `size` bytes of JSON read into the tree, begun again: its root
static unsigned int plugins_read(const char *text, unsigned long long size)
{
    cli_json_begun(&s_json, s_node, sizeof(s_node) / sizeof(s_node[0]), s_text, sizeof(s_text));
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_json_read(&s_json, text, size, s_token, sizeof(s_token) / sizeof(s_token[0]), &said);
}

// a member of a node of the tree written compact, checked against `wanted`
static void plugins_member(unsigned int node, const char *wanted, const char *what)
{
    char text[512];
    CliLine line = {text, sizeof(text), 0ull};
    cli_json_write(&s_json, node, &line, 0u, 0);
    cli_check_text(text, line.at, wanted, what);
}

// every bundled plugin reads and names itself
static void plugins_bundled(void)
{
    int whole = g_cli_bundled_count >= 16u;
    for (unsigned int at = 0u; at < g_cli_bundled_count; at += 1u)
    {
        const unsigned int root = plugins_read((const char *)g_cli_bundled[at].text, g_cli_bundled[at].size);
        const char *const id = cli_json_string(&s_json, cli_json_member(&s_json, root, "id"), NULL);
        const char *const kind = cli_json_string(&s_json, cli_json_member(&s_json, root, "kind"), NULL);
        whole &= (id != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = g_cli_bundled[at].id, .cap = CLI_CELLS_POOL})) &&
                 (kind != NULL);
        if ((kind != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "language", .cap = CLI_CELLS_POOL})))
        {
            static const char *const s_keys[3] = {"extensions", "names", "paths"};
            int any = 0;
            for (unsigned int key = 0u; key < 3u; key += 1u)
            {
                const unsigned int list = cli_json_member(&s_json, root, s_keys[key]);
                any |= cli_json_is(&s_json, list, CFG_JSON_ARRAY) && (s_json.node[list].count > 0u);
            }
            whole &= any;
        }
        else if ((kind != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "tool", .cap = CLI_CELLS_POOL})))
        {
            const unsigned int list = cli_json_member(&s_json, root, "languages");
            whole &= cli_json_is(&s_json, list, CFG_JSON_ARRAY) && (s_json.node[list].count > 0u);
        }
        else if ((kind != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "script", .cap = CLI_CELLS_POOL})))
        {
            // a script plugin that comes with orior runs the code built into the program beside its plugin.json
            whole &= (g_cli_bundled[at].code != NULL) && (g_cli_bundled[at].code_size > 0ull);
        }
        else
        {
            whole = 0;
        }
    }
    cli_check(whole, "every bundled plugin reads and names itself, and a script plugin carries its code");
}

// the language `path` opens as, the empty text where none
static const char *plugins_of(const char *path)
{
    static char s_id[CLI_NAME_LONGEST];
    return cli_plugins_language_for(&s_languages, path, s_id) ? s_id : "";
}

// the id a name becomes
static const char *plugins_slug(const char *name)
{
    static char s_id[CLI_NAME_LONGEST];
    cli_plugins_slug(name, s_id);
    return s_id;
}

// the toy language's answers read into the tree: their node
static unsigned int plugins_spec(void)
{
    static const char s_spec[] = "{\"name\": \"Toy Lang\", \"extensions\": [\".toy\"], \"lineComment\": \"--\", "
                                 "\"blockComment\": [\"{-\", \"-}\"], \"keywords\": [\"let\", \"in\", \"let\"], "
                                 "\"types\": [\"Int\"], \"constants\": [\"true\"]}";
    return plugins_read(s_spec, sizeof(s_spec) - 1u);
}

// a plugin from nothing holds what was asked
static void plugins_from_nothing(void)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int plugin = cli_plugins_draft(&s_json, plugins_spec(), &said);
    cli_check(plugin != CLI_JSON_NONE, "a plugin is drafted");
    if (plugin == CLI_JSON_NONE)
    {
        return;
    }
    const unsigned int grammar = cli_json_member(&s_json, plugin, "grammar");
    const unsigned int whitespace =
        cli_json_member(&s_json, cli_json_member(&s_json, grammar, "tokenizer"), "whitespace");
    plugins_member(cli_json_member(&s_json, plugin, "id"), "\"toy-lang\"", "its id is its name's");
    plugins_member(cli_json_member(&s_json, plugin, "extensions"), "[\"toy\"]", "its extensions without their dots");
    plugins_member(cli_json_member(&s_json, plugin, "comments"), "{\"line\":\"--\",\"block\":[\"{-\",\"-}\"]}",
                   "its comments");
    plugins_member(cli_json_member(&s_json, grammar, "keywords"), "[\"let\",\"in\"]", "its keywords, each once");
    plugins_member(cli_json_item(&s_json, whitespace, 1u), "[\"\\\\{-\",\"comment\",\"@comment\"]",
                   "a block comment opens the comment state");
    plugins_member(cli_json_item(&s_json, whitespace, 2u), "[\"--.*$\",\"comment\"]", "a line comment runs to its end");
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_plugins_text_of(&s_json, plugin, &out);
    const unsigned long long text_size = out.at;
    char written[8192];
    CliLine line = {written, sizeof(written), 0ull};
    cli_json_write(&s_json, plugin, &line, 0u, 1);
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const read = cli_check_parsed(s_out, text_size, &root);
    char again[8192];
    CliLine again_line = {again, sizeof(again), 0ull};
    cli_json_write(read, root, &again_line, 0u, 1);
    cli_check((line.at == again_line.at) &&
                  (memor.cmp(&(MemoriaCfg){.src = written, .other = again, .bytes = line.at}) == 0),
              "its text reads back as itself");
}

// a plugin from another takes its grammar
static void plugins_from_another(void)
{
    static const char s_rusty[] =
        "{\"name\": \"Rusty\", \"extensions\": [\"rsy\"], \"from\": \"rust\", \"keywords\": [\"fn\"]}";
    static const char s_none[] = "{\"name\": \"x\", \"extensions\": [\"x\"], \"from\": \"nothing-here\"}";
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int plugin = cli_plugins_draft(&s_json, plugins_read(s_rusty, sizeof(s_rusty) - 1u), &said);
    if (plugin == CLI_JSON_NONE)
    {
        cli_check(0, "a plugin from another is drafted");
        return;
    }
    const unsigned int grammar = cli_json_member(&s_json, plugin, "grammar");
    plugins_member(cli_json_member(&s_json, plugin, "id"), "\"rusty\"", "a plugin from another has its own id");
    plugins_member(cli_json_member(&s_json, plugin, "extensions"), "[\"rsy\"]", "its own extensions");
    plugins_member(cli_json_member(&s_json, grammar, "words"), "[\"fn\"]", "and its own words");
    const unsigned int types = cli_json_member(&s_json, grammar, "types");
    cli_check(cli_json_is(&s_json, types, CFG_JSON_ARRAY) && (s_json.node[types].count > 10u),
              "and the other's grammar");
    cli_check(cli_plugins_draft(&s_json, plugins_read(s_none, sizeof(s_none) - 1u), &said) == CLI_JSON_NONE,
              "a plugin from one not there is refused");
}

// a folder of the folder counted
static void plugins_counted(void *context, const char *name, int is_folder)
{
    unsigned int *const count = context;
    (void)name;
    *count += is_folder ? 1u : 0u;
}

// creating writes once and replaces only when asked
static void plugins_creating(void)
{
    char dir[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("plugins", dir);
    cli_check(cli_plugins_create_in(dir, &s_json, plugins_spec(), 0, folder, &said), "a plugin is created");
    cli_full(folder, "plugin.json", path);
    cli_check(query_is_file(path), "with its plugin.json");
    // the room holds the sample a plugin of one language is written with, a few hundred bytes, and its 0
    static char s_sample[1u << 16u];
    cli_full(folder, "sample.toy", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_sample, sizeof(s_sample) - 1u);
    s_sample[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    cli_check(cellul.starts(&(CatenaFinitaCfg){.src = s_sample, .other = "-- Toy Lang", .cap = CLI_CELLS_POOL}),
              "and a sample in its language");
    cli_check(!cli_plugins_create_in(dir, &s_json, plugins_spec(), 0, folder, &said), "it is not written over");
    cli_check(cli_plugins_create_in(dir, &s_json, plugins_spec(), 1, folder, &said), "unless asked");
    unsigned int count = 0u;
    (void)query_folder_each(dir, plugins_counted, &count);
    cli_check(count == 1u, "and is one plugin");
    // answers read into a tree of their own size, starting from a plugin many times the room they take
    static const char s_from[] = "{\"name\": \"Snake\", \"extensions\": [\"snk\"], \"from\": \"python\"}";
    CliJson tight = {0};
    const unsigned int answers = cli_json_taken(&tight, s_from, sizeof(s_from) - 1u, 0u, 0ull, &said);
    said.at = 0ull;
    cli_check(cli_plugins_create_in(dir, &tight, answers, 0, folder, &said),
              "a plugin starts from another larger than the answers' own tree holds");
    cli_json_given(&tight);
}

// the plugin of the id looked for, as the plugins are handed on, kept where it is the reader's
static void plugins_reader_found(void *context, const CliPlugin *plugin)
{
    int *const found = context;
    *found |= cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = "readers", .cap = CLI_CELLS_POOL}) &&
              cellul.eq(&(CatenaFinitaCfg){.src = plugin->source, .other = "user", .cap = CLI_CELLS_POOL});
}

// a plugin just written in the plugins folder of orior's own folder is listed as the reader's, and the window's list
// of plugins holds it
static void plugins_reader(void)
{
    char home[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char manifest[CLI_PATH_LONGEST];
    cli_check_folder("plugins_reader_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
    cli_full(home, "plugins/readers", folder);
    query_folder_made(folder);
    cli_full(folder, "plugin.json", manifest);
    static const char s_manifest[] = "{\"id\": \"readers\", \"name\": \"Readers\", \"version\": \"1.0.0\", \"kind\": "
                                     "\"script\", \"api\": 1}";
    (void)query_file_written(manifest, s_manifest, sizeof(s_manifest) - 1u);
    int found = 0;
    cli_plugins_each(plugins_reader_found, &found);
    cli_check(found, "a plugin just written in the plugins folder of orior's own folder is listed as the reader's");
    const unsigned int room = cli_window_called(1, "plugins_read", "{}", 2ull);
    const char *text = NULL;
    unsigned long long size = 0ull;
    const int answered = cli_window_answer(room, &text, &size);
    cli_check(answered &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = text, .cap = size, .other = "\"id\":\"readers\"", .other_cap = CLI_CELLS_POOL}) != NULL),
              "and the window's list of plugins holds it");
    cli_window_given(room);
}

// a script plugin's code read from each file its `main` names, in order, and a name that leaves its folder refused
static void plugins_code(void)
{
    char home[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("plugins_code_home", home);
    (void)query_environment_set("ORIOR_HOME", home);
    cli_full(home, "plugins/parts", folder);
    query_folder_made(folder);
    cli_full(folder, "plugin.json", path);
    static const char s_parts[] =
        "{\"id\": \"parts\", \"name\": \"Parts\", \"version\": \"1.0.0\", \"kind\": \"script\", "
        "\"main\": [\"one.js\", \"lib/two.js\"]}";
    (void)query_file_written(path, s_parts, sizeof(s_parts) - 1u);
    cli_full(folder, "one.js", path);
    (void)query_file_written(path, "const one = 1;", 14ull);
    cli_full(folder, "lib", path);
    query_folder_made(path);
    cli_full(folder, "lib/two.js", path);
    (void)query_file_written(path, "const two = one + 1;", 20ull);
    char code[256];
    CliLine out = {code, sizeof(code), 0ull};
    char why[CLI_PATH_LONGEST * 2u];
    CliLine said = {why, sizeof(why), 0ull};
    const int read = cli_plugins_code_of("parts", &out, &said);
    (void)cli_line_closed(&out);
    cli_check(read && cellul.eq(&(CatenaFinitaCfg){
                          .src = code, .other = "const one = 1;\nconst two = one + 1;", .cap = sizeof(code)}),
              "a plugin's code is each file its `main` names, in order");
    static const char s_out[] =
        "{\"id\": \"parts\", \"name\": \"Parts\", \"version\": \"1.0.0\", \"kind\": \"script\", "
        "\"main\": [\"one.js\", \"../two.js\"]}";
    cli_full(folder, "plugin.json", path);
    (void)query_file_written(path, s_out, sizeof(s_out) - 1u);
    out.at = 0ull;
    said.at = 0ull;
    cli_check(!cli_plugins_code_of("parts", &out, &said), "and a file it names outside its folder is refused");
}

int main(void)
{
    plugins_bundled();
    plugins_reader();
    plugins_code();
    cli_plugins_languages(&s_languages);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = plugins_of("src/SConscript"), .other = "python", .cap = CLI_CELLS_POOL}),
        "a file opens by its whole name");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = plugins_of("ci/Jenkinsfile"), .other = "jenkins", .cap = CLI_CELLS_POOL}),
        "Jenkins's file too");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = plugins_of("deploy/roles/web/tasks/main.yml"), .other = "ansible", .cap = CLI_CELLS_POOL}),
              "a file opens by its path");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = plugins_of("deploy\\playbooks\\site.yaml"), .other = "ansible", .cap = CLI_CELLS_POOL}),
              "whichever way its slashes lean");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = plugins_of("config/app.yml"), .other = "yaml", .cap = CLI_CELLS_POOL}),
        "a file opens by its extension");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = plugins_of("build.zig"), .other = "zig", .cap = CLI_CELLS_POOL}),
              "of any language");
    cli_check((s_languages.count > 0u) && (s_languages.room >= s_languages.count) &&
                  (s_languages.room <= (2u * s_languages.count) + CLI_LANGUAGE_PAIRS_FIRST),
              "the languages' keys are held in a room that grew to them and no further than twice");
    cli_plugins_languages_given(&s_languages);
    cli_check((g_cli_plugins_cells.holds == 0ull) && (g_cli_plugins_cells.hw != 0ull),
              "the plugins are read in cells each given back");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = plugins_slug("Toy Lang"), .other = "toy-lang", .cap = CLI_CELLS_POOL}),
        "a name becomes an id");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = plugins_slug("  C++ & More!! "), .other = "c-more", .cap = CLI_CELLS_POOL}),
        "each run of anything else one dash");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = plugins_slug("!!!"), .other = "", .cap = CLI_CELLS_POOL}),
              "and a name of none is none");
    plugins_from_nothing();
    plugins_from_another();
    plugins_creating();
    return cli_checked();
}
