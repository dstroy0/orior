// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_program.c: the program a CMake or Meson build orior_cli configured makes from a source, read from CMake's
// file API reply and from Meson's intro-targets.json
#include "../command_line/cells.h"
#include "../text/text.h"
#include "compile_db_internal.h"

// the most nodes and bytes of a build's reply read
#define CLI_PROGRAM_NODES (1u << 19u)
#define CLI_PROGRAM_TEXT (1u << 24u)

// The search: the reply tree and its tokens, the folders found, the path wanted, and the best program yet with how it
// ranks
typedef struct
{
    CliJsonNode nodes[CLI_PROGRAM_NODES];
    char text[CLI_PROGRAM_TEXT];
    CliJson json;
    CfgJsonToken tokens[CLI_PROGRAM_NODES];
    CliFoundFolders folders;
    char wanted[CLI_PATH_LONGEST];
    int found;
    int debugging;
    unsigned long long time;
    CliProgram best;
} CliProgramSearch;

static CliProgramSearch *s_search;

// a path as the search compares paths: with forward slashes, and on Windows in lower case
static void cli_same(const char *path, char *out)
{
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = out; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
#if defined(_WIN32)
        at[0] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
#endif
    }
}

// whether a path stands alone, not under a folder
static int cli_absolute(const char *path)
{
    return (path[0] == '/') || (path[0] == '\\') || ((path[0] != '\0') && (path[1] == ':'));
}

// a member's string in the reply, NULL where it is none
static const char *cli_string(unsigned int node, const char *key)
{
    return cli_json_string(&s_search->json, cli_json_member(&s_search->json, node, key), NULL);
}

// a whole number node of the reply, as written: its value, or all ones where it is none
static unsigned long long cli_whole(unsigned int node)
{
    if (!cli_json_is(&s_search->json, node, CFG_JSON_NUMBER))
    {
        return ~0ull;
    }
    const CliJsonNode *const one = &s_search->json.node[node];
    const char *const digits = s_search->json.text + one->text;
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < one->length; at += 1ull)
    {
        if ((digits[at] < '0') || (digits[at] > '9') || (value > 0xffffffffull))
        {
            return ~0ull;
        }
        value = (value * 10ull) + (unsigned long long)(digits[at] - '0');
    }
    return (one->length > 0ull) ? value : ~0ull;
}

// whether a text is one of two, ASCII's case aside
static int cli_one_of(const char *text, const char *one, const char *other)
{
    char lower[CLI_NAME_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = lower, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = lower; at[0] != '\0'; at += 1)
    {
        at[0] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = one, .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = other, .cap = CLI_CELLS_POOL}));
}

// a program found kept where it ranks as high as the best yet: with debugging information first, then the later
static void cli_program_put(int debugging, unsigned long long time, const CliProgram *program)
{
    const int higher = !s_search->found || (debugging > s_search->debugging) ||
                       ((debugging == s_search->debugging) && (time >= s_search->time));
    if (higher)
    {
        s_search->found = 1;
        s_search->debugging = debugging;
        s_search->time = time;
        s_search->best = *program;
    }
}

// the reply file a member `jsonFile` of `node` names, read: its root, or CLI_JSON_NONE
static unsigned int cli_reply_file(const char *reply, unsigned int node)
{
    const char *const name = cli_string(node, "jsonFile");
    if (name == NULL)
    {
        return CLI_JSON_NONE;
    }
    char path[CLI_PATH_LONGEST];
    cli_full(reply, name, path);
    return cli_compile_json_read(&s_search->json, path, s_search->tokens, CLI_PROGRAM_NODES);
}

// the executable targets of one CMake build that make `wanted` from a source, each put among those found
static void cli_cmake_programs(const char *reply)
{
    CliJson *const json = &s_search->json;
    char index[CLI_PATH_LONGEST];
    CliStamped stamped;
    if (!cli_compile_newest_index(reply, index) || !cli_compile_stamped(index, &stamped))
    {
        return;
    }
    cli_json_begun(json, s_search->nodes, CLI_PROGRAM_NODES, s_search->text, CLI_PROGRAM_TEXT);
    const unsigned int told = cli_compile_json_read(json, index, s_search->tokens, CLI_PROGRAM_NODES);
    const unsigned int answers = cli_json_member(json, told, "reply");
    const unsigned int model = cli_reply_file(reply, cli_json_member(json, answers, "codemodel-v2"));
    const unsigned int chains =
        cli_json_member(json, cli_reply_file(reply, cli_json_member(json, answers, "toolchains-v1")), "toolchains");
    const unsigned int paths = cli_json_member(json, model, "paths");
    const char *const source = cli_string(paths, "source");
    const char *const build = cli_string(paths, "build");
    if ((source == NULL) || (build == NULL))
    {
        return;
    }
    const char *named = build;
    for (const char *at = build; at[0] != '\0'; at += 1)
    {
        named = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : named;
    }
    const unsigned int configurations = cli_json_member(json, model, "configurations");
    unsigned int configuration = cli_json_item(json, configurations, 0u);
    for (unsigned int at = 0u; cli_json_item(json, configurations, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const name = cli_string(cli_json_item(json, configurations, at), "name");
        if ((name != NULL) && cli_one_of(name, named, named))
        {
            configuration = cli_json_item(json, configurations, at);
            break;
        }
    }
    if (configuration == CLI_JSON_NONE)
    {
        return;
    }
    const char *const config_name = cli_string(configuration, "name");
    const char *const config = (config_name != NULL) ? config_name : "";
    const unsigned int targets = cli_json_member(json, configuration, "targets");
    for (unsigned int at = 0u; cli_json_item(json, targets, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int target = cli_reply_file(reply, cli_json_item(json, targets, at));
        const char *const type = cli_string(target, "type");
        if ((type == NULL) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "EXECUTABLE", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        const unsigned int sources = cli_json_member(json, target, "sources");
        unsigned int matched = CLI_JSON_NONE;
        for (unsigned int one = 0u; (matched == CLI_JSON_NONE) && (cli_json_item(json, sources, one) != CLI_JSON_NONE);
             one += 1u)
        {
            const char *const path = cli_string(cli_json_item(json, sources, one), "path");
            char full[CLI_PATH_LONGEST];
            char compared[CLI_PATH_LONGEST];
            if (path == NULL)
            {
                continue;
            }
            if (cli_absolute(path))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = full,
                                  .src = path,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            }
            else
            {
                cli_full(source, path, full);
            }
            cli_same(full, compared);
            matched = (cellul.eq(&(CatenaFinitaCfg){.src = compared, .other = s_search->wanted, .cap = CLI_CELLS_POOL}))
                          ? cli_json_item(json, sources, one)
                          : CLI_JSON_NONE;
        }
        if (matched == CLI_JSON_NONE)
        {
            continue;
        }
        // the source's language by its compile group, C where it has none
        const unsigned long long place = cli_whole(cli_json_member(json, matched, "compileGroupIndex"));
        // a group's place is below 2^32 where it is one
        const unsigned int group =
            (place < 0xffffffffull)
                ? cli_json_item(json, cli_json_member(json, target, "compileGroups"), (unsigned int)place)
                : CLI_JSON_NONE;
        const char *const group_language = cli_string(group, "language");
        const char *const language = (group_language != NULL) ? group_language : "C";
        const unsigned int artifacts = cli_json_member(json, target, "artifacts");
        const char *program = NULL;
        for (unsigned int one = 0u; (program == NULL) && (cli_json_item(json, artifacts, one) != CLI_JSON_NONE);
             one += 1u)
        {
            const char *const path = cli_string(cli_json_item(json, artifacts, one), "path");
            const unsigned long long length =
                (path != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) : 0ull;
            const int skipped =
                (length >= 4ull) &&
                ((cellul.eq(&(CatenaFinitaCfg){.src = path + length - 4ull, .other = ".pdb", .cap = CLI_CELLS_POOL})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = path + length - 4ull, .other = ".lib", .cap = CLI_CELLS_POOL})));
            program = ((path != NULL) && !skipped) ? path : NULL;
        }
        if (program == NULL)
        {
            continue;
        }
        CliProgram found;
        memor.cpy(&(MemoriaCfg){.dst = found.tool,
                                .src = "cmake",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "cmake", .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = found.build,
                                .src = build,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = build, .cap = CLI_CELLS_POOL}) + 1u});
        const char *const target_name = cli_string(target, "name");
        memor.cpy(&(MemoriaCfg){.dst = found.target,
                                .src = (target_name != NULL) ? target_name : "",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = (target_name != NULL) ? target_name : "",
                                                                       .cap = CLI_CELLS_POOL}) +
                                         1u});
        memor.cpy(&(MemoriaCfg){.dst = found.config,
                                .src = config,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = config, .cap = CLI_CELLS_POOL}) + 1u});
        if (cli_absolute(program))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = found.program,
                              .src = program,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}) + 1u});
        }
        else
        {
            cli_full(build, program, found.program);
        }
        found.compiler[0] = '\0';
        for (unsigned int one = 0u; cli_json_item(json, chains, one) != CLI_JSON_NONE; one += 1u)
        {
            const unsigned int chain = cli_json_item(json, chains, one);
            const char *const chain_language = cli_string(chain, "language");
            const char *const id = cli_string(cli_json_member(json, chain, "compiler"), "id");
            if ((chain_language != NULL) && (id != NULL) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = chain_language, .other = language, .cap = CLI_CELLS_POOL})))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = found.compiler,
                                  .src = id,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
            }
        }
        cli_program_put(cli_one_of(config, "debug", "relwithdebinfo"), stamped.time, &found);
    }
}

// the executables of one Meson build that make `wanted` from a source, each put among those found
static void cli_meson_programs(const char *info)
{
    CliJson *const json = &s_search->json;
    char targets_path[CLI_PATH_LONGEST];
    char build[CLI_PATH_LONGEST];
    CliStamped stamped;
    cli_full(info, "intro-targets.json", targets_path);
    if (!cli_compile_stamped(targets_path, &stamped) || !cli_path_parent(info, build))
    {
        return;
    }
    const char *config = build;
    for (const char *at = build; at[0] != '\0'; at += 1)
    {
        config = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : config;
    }
    cli_json_begun(json, s_search->nodes, CLI_PROGRAM_NODES, s_search->text, CLI_PROGRAM_TEXT);
    const unsigned int all = cli_compile_json_read(json, targets_path, s_search->tokens, CLI_PROGRAM_NODES);
    for (unsigned int at = 0u; cli_json_item(json, all, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int target = cli_json_item(json, all, at);
        const char *const type = cli_string(target, "type");
        if ((type == NULL) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "executable", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        const unsigned int groups = cli_json_member(json, target, "target_sources");
        unsigned int group = CLI_JSON_NONE;
        for (unsigned int one = 0u; (group == CLI_JSON_NONE) && (cli_json_item(json, groups, one) != CLI_JSON_NONE);
             one += 1u)
        {
            const unsigned int sources = cli_json_member(json, cli_json_item(json, groups, one), "sources");
            for (unsigned int place = 0u; cli_json_item(json, sources, place) != CLI_JSON_NONE; place += 1u)
            {
                const char *const path = cli_json_string(json, cli_json_item(json, sources, place), NULL);
                char compared[CLI_PATH_LONGEST];
                cli_same((path != NULL) ? path : "", compared);
                if ((path != NULL) &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = compared, .other = s_search->wanted, .cap = CLI_CELLS_POOL})))
                {
                    group = cli_json_item(json, groups, one);
                    break;
                }
            }
        }
        const char *const program =
            cli_json_string(json, cli_json_item(json, cli_json_member(json, target, "filename"), 0u), NULL);
        if ((group == CLI_JSON_NONE) || (program == NULL))
        {
            continue;
        }
        const char *const compiler =
            cli_json_string(json, cli_json_item(json, cli_json_member(json, group, "compiler"), 0u), NULL);
        // the compiler's stem in lower case names its kind
        char stem[CLI_NAME_LONGEST] = "";
        const char *name = (compiler != NULL) ? compiler : "";
        for (const char *letter = name; letter[0] != '\0'; letter += 1)
        {
            name = ((letter[0] == '/') || (letter[0] == '\\')) ? (letter + 1) : name;
        }
        memor.cpy(&(MemoriaCfg){.dst = stem,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        char *const dot = cli_text_last(stem, cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}), '.');
        if ((dot != NULL) && (dot != stem))
        {
            dot[0] = '\0';
        }
        for (char *letter = stem; letter[0] != '\0'; letter += 1)
        {
            letter[0] = ((letter[0] >= 'A') && (letter[0] <= 'Z')) ? (char)(letter[0] + 32) : letter[0];
        }
        CliProgram found;
        memor.cpy(&(MemoriaCfg){.dst = found.tool,
                                .src = "meson",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "meson", .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = found.build,
                                .src = build,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = build, .cap = CLI_CELLS_POOL}) + 1u});
        const char *const target_name = cli_string(target, "name");
        memor.cpy(&(MemoriaCfg){.dst = found.target,
                                .src = (target_name != NULL) ? target_name : "",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = (target_name != NULL) ? target_name : "",
                                                                       .cap = CLI_CELLS_POOL}) +
                                         1u});
        memor.cpy(&(MemoriaCfg){.dst = found.config,
                                .src = config,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = config, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = found.program,
                                .src = program,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = found.compiler,
            .src =
                (cellul.eq(&(CatenaFinitaCfg){.src = stem, .other = "cl", .cap = CLI_CELLS_POOL}))
                    ? "MSVC"
                    : ((cellul.find(&(CatenaFinitaCfg){
                            .src = stem, .cap = CLI_CELLS_POOL, .other = "clang", .other_cap = CLI_CELLS_POOL}) != NULL)
                           ? "Clang"
                           : "GNU"),
            .bytes = cellul.len(&(CatenaFinitaCfg){
                         .src = (cellul.eq(&(CatenaFinitaCfg){.src = stem, .other = "cl", .cap = CLI_CELLS_POOL}))
                                    ? "MSVC"
                                    : ((cellul.find(&(CatenaFinitaCfg){.src = stem,
                                                                       .cap = CLI_CELLS_POOL,
                                                                       .other = "clang",
                                                                       .other_cap = CLI_CELLS_POOL}) != NULL)
                                           ? "Clang"
                                           : "GNU"),
                         .cap = CLI_CELLS_POOL}) +
                     1u});
        cli_program_put(cli_one_of(config, "debug", "debugoptimized"), stamped.time, &found);
    }
}

int cli_compile_db_program_of(const char *root, const char *file, CliProgram *program)
{
    static const char *const s_reply[4] = {".cmake", "api", "v1", "reply"};
    static const char *const s_info[1] = {"meson-info"};
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_search = cli_cell_taken(sizeof(*s_search), "a search for a file's program");
    if (s_search == NULL)
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    cli_same(file, s_search->wanted);
    s_search->found = 0;
    s_search->folders = (CliFoundFolders){NULL, 0u, 0u};
    char top[CLI_PATH_LONGEST];
    cli_full(root, "build/cmake", top);
    cli_compile_folders_named(top, s_reply, 4u, &s_search->folders);
    for (unsigned int at = 0u; at < s_search->folders.count; at += 1u)
    {
        cli_cmake_programs(s_search->folders.path[at]);
    }
    cli_full(root, "build/meson", top);
    cli_compile_folders_named(top, s_info, 1u, &s_search->folders);
    for (unsigned int at = 0u; at < s_search->folders.count; at += 1u)
    {
        cli_meson_programs(s_search->folders.path[at]);
    }
    const int found = s_search->found;
    if (found)
    {
        *program = s_search->best;
    }
    cli_compile_found_given(&s_search->folders);
    cli_cell_given(s_search);
    s_search = NULL;
    cli_lock_give(&s_lock);
    return found;
}
