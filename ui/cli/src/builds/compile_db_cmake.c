// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_cmake.c: a CMake build's entries read from its file API reply: one for each source of each target in
// the configuration the build's folder is named for, as the build's own compiler is given it, C and C++ alone
#include "compile_db_internal.h"

// a member's string in the reply, NULL where it is none
static const char *cli_reply_string(unsigned int node, const char *key)
{
    return cli_json_string(&g_cli_gather.reply, cli_json_member(&g_cli_gather.reply, node, key), NULL);
}

// the reply file a member `jsonFile` of `node` names, read into the reply tree: its root, or CLI_JSON_NONE
static unsigned int cli_reply_file(const char *reply, unsigned int node)
{
    const char *const name = cli_reply_string(node, "jsonFile");
    if (name == NULL)
    {
        return CLI_JSON_NONE;
    }
    char path[CLI_PATH_LONGEST];
    cli_full(reply, name, path);
    return cli_compile_json_read(&g_cli_gather.reply, path, g_cli_gather.tokens, CLI_GATHER_NODES);
}

// a whole number node of the reply, as written: its value, or all ones where it is none
static unsigned long long cli_reply_whole(unsigned int node)
{
    if (!cli_json_is(&g_cli_gather.reply, node, CFG_JSON_NUMBER))
    {
        return ~0ull;
    }
    const CliJsonNode *const one = &g_cli_gather.reply.node[node];
    const char *const digits = g_cli_gather.reply.text + one->text;
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

// a flag put after the compile group's flags
static void cli_flag_put(const char *flag, unsigned long long size)
{
    if ((g_cli_gather.flags >= CLI_GATHER_FLAGS) ||
        ((g_cli_gather.flags_used + size + 1ull) > sizeof(g_cli_gather.flags_text)))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = g_cli_gather.flags_text + g_cli_gather.flags_used, .src = flag, .bytes = size});
    g_cli_gather.flags_text[g_cli_gather.flags_used + size] = '\0';
    g_cli_gather.flag[g_cli_gather.flags] = g_cli_gather.flags_used;
    g_cli_gather.flags += 1u;
    // the flags are below their room's bytes, which an unsigned int holds
    g_cli_gather.flags_used += (unsigned int)size + 1u;
}

static void cli_fragment_word(void *context, const char *word, unsigned long long size)
{
    (void)context;
    cli_flag_put(word, size);
}

// a flag made of `lead` and `text` put after the flags
static void cli_flag_made(const char *lead, const char *text)
{
    char flag[CLI_PATH_LONGEST * 2u];
    CliLine line = {flag, sizeof(flag), 0ull};
    cli_line_text(&line, lead);
    cli_line_text(&line, text);
    cli_line_closed(&line);
    cli_flag_put(flag, cellul.len(&(CatenaFinitaCfg){.src = flag, .cap = CLI_CELLS_POOL}));
}

// a string made in the entries tree with every backslash written as a slash
static unsigned int cli_slashed_made(const char *text)
{
    char slashed[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = slashed, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = slashed; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    return cli_json_string_made(&g_cli_gather.json, slashed,
                                cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_CELLS_POOL}));
}

// a compile group's flags: its fragments' words, its include folders, its defines and its sysroot
static void cli_group_flags(unsigned int group, int msvc)
{
    CliJson *const reply = &g_cli_gather.reply;
    g_cli_gather.flags = 0u;
    g_cli_gather.flags_used = 0u;
    const unsigned int fragments = cli_json_member(reply, group, "compileCommandFragments");
    for (unsigned int at = 0u; cli_json_item(reply, fragments, at) != CLI_JSON_NONE; at += 1u)
    {
        unsigned long long size = 0ull;
        const char *const fragment =
            cli_json_string(reply, cli_json_member(reply, cli_json_item(reply, fragments, at), "fragment"), &size);
        if (fragment != NULL)
        {
            cli_compile_words_each(fragment, size, cli_fragment_word, NULL);
        }
    }
    const unsigned int includes = cli_json_member(reply, group, "includes");
    for (unsigned int at = 0u; cli_json_item(reply, includes, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int include = cli_json_item(reply, includes, at);
        const char *const path = cli_reply_string(include, "path");
        const int system = cli_json_is(reply, cli_json_member(reply, include, "isSystem"), CFG_JSON_TRUE);
        if (path != NULL)
        {
            cli_flag_made(msvc ? "/I" : (system ? "-isystem" : "-I"), path);
        }
    }
    const unsigned int defines = cli_json_member(reply, group, "defines");
    for (unsigned int at = 0u; cli_json_item(reply, defines, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const define = cli_reply_string(cli_json_item(reply, defines, at), "define");
        if (define != NULL)
        {
            cli_flag_made(msvc ? "/D" : "-D", define);
        }
    }
    const char *const sysroot = cli_reply_string(cli_json_member(reply, group, "sysroot"), "path");
    if (sysroot != NULL)
    {
        cli_flag_made("--sysroot=", sysroot);
    }
}

// the compiler a CMake build gives `language`: its index among those read, or the count where it gives none
static unsigned int cli_compiler_of(const char *language)
{
    unsigned int at = 0u;
    while ((at < g_cli_gather.compilers) &&
           (!cellul.eq(&(CatenaFinitaCfg){
               .src = g_cli_gather.compiler[at].language, .other = language, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    return at;
}

// the entries of a target's C and C++ compile groups, one for each source, appended to `entries`
static void cli_target_entries(unsigned int target, const char *source, const char *build, unsigned int entries)
{
    CliJson *const reply = &g_cli_gather.reply;
    CliJson *const json = &g_cli_gather.json;
    const unsigned int sources = cli_json_member(reply, target, "sources");
    const unsigned int groups = cli_json_member(reply, target, "compileGroups");
    for (unsigned int index = 0u; cli_json_item(reply, groups, index) != CLI_JSON_NONE; index += 1u)
    {
        const unsigned int group = cli_json_item(reply, groups, index);
        const char *const language = cli_reply_string(group, "language");
        const unsigned int compiler = (language != NULL) ? cli_compiler_of(language) : g_cli_gather.compilers;
        if ((language == NULL) ||
            ((!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "C", .cap = CLI_CELLS_POOL})) &&
             (!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "CXX", .cap = CLI_CELLS_POOL}))) ||
            (compiler == g_cli_gather.compilers))
        {
            continue;
        }
        const CliCompilerOf *const one = &g_cli_gather.compiler[compiler];
        const int msvc = (cellul.eq(&(CatenaFinitaCfg){.src = one->id, .other = "MSVC", .cap = CLI_CELLS_POOL})) ||
                         cli_compile_db_is_cl(one->path);
        cli_group_flags(group, msvc);
        const unsigned int indexes = cli_json_member(reply, group, "sourceIndexes");
        for (unsigned int at = 0u; cli_json_item(reply, indexes, at) != CLI_JSON_NONE; at += 1u)
        {
            const unsigned long long place = cli_reply_whole(cli_json_item(reply, indexes, at));
            // a source's place in the target's sources is below 2^32
            const char *const path = (place < 0xffffffffull)
                                         ? cli_reply_string(cli_json_item(reply, sources, (unsigned int)place), "path")
                                         : NULL;
            if (path == NULL)
            {
                continue;
            }
            char file[CLI_PATH_LONGEST];
            if ((path[0] == '/') || (path[0] == '\\') || ((path[0] != '\0') && (path[1] == ':')))
            {
                memor.cpy(
                    &(MemoriaCfg){.dst = file,
                                  .src = path,
                                  .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            }
            else
            {
                cli_full(source, path, file);
            }
            const unsigned int arguments = cli_json_made(json, CFG_JSON_ARRAY);
            cli_json_append(json, arguments, cli_slashed_made(one->path));
            for (unsigned int flag = 0u; flag < g_cli_gather.flags; flag += 1u)
            {
                const char *const text = g_cli_gather.flags_text + g_cli_gather.flag[flag];
                cli_json_append(json, arguments,
                                cli_json_string_made(
                                    json, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})));
            }
            cli_json_append(json, arguments, cli_json_string_made(json, msvc ? "/c" : "-c", 2ull));
            cli_json_append(json, arguments, cli_slashed_made(file));
            const unsigned int entry = cli_json_made(json, CFG_JSON_OBJECT);
            cli_json_set(
                json, entry, "directory",
                cli_json_string_made(json, build, cellul.len(&(CatenaFinitaCfg){.src = build, .cap = CLI_CELLS_POOL})));
            cli_json_set(json, entry, "file", cli_slashed_made(file));
            cli_json_set(json, entry, "arguments", arguments);
            cli_json_append(json, entries, entry);
        }
    }
}

unsigned int cli_compile_cmake_entries(const char *index, char *source)
{
    CliJson *const reply = &g_cli_gather.reply;
    cli_json_begun(reply, g_cli_gather.reply_nodes, CLI_REPLY_NODES, g_cli_gather.reply_text, CLI_REPLY_TEXT);
    char folder[CLI_PATH_LONGEST];
    const unsigned int told = cli_path_parent(index, folder)
                                  ? cli_compile_json_read(reply, index, g_cli_gather.tokens, CLI_GATHER_NODES)
                                  : CLI_JSON_NONE;
    const unsigned int answers = cli_json_member(reply, told, "reply");
    const unsigned int model = cli_reply_file(folder, cli_json_member(reply, answers, "codemodel-v2"));
    if (model == CLI_JSON_NONE)
    {
        return CLI_JSON_NONE;
    }
    g_cli_gather.compilers = 0u;
    const unsigned int chains_file = cli_reply_file(folder, cli_json_member(reply, answers, "toolchains-v1"));
    const unsigned int chains = cli_json_member(reply, chains_file, "toolchains");
    for (unsigned int at = 0u;
         (g_cli_gather.compilers < CLI_GATHER_LANGUAGES) && (cli_json_item(reply, chains, at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int one = cli_json_item(reply, chains, at);
        const unsigned int compiler = cli_json_member(reply, one, "compiler");
        const char *const language = cli_reply_string(one, "language");
        const char *const path = cli_reply_string(compiler, "path");
        const char *const id = cli_reply_string(compiler, "id");
        if ((language != NULL) && (path != NULL))
        {
            const unsigned int place = cli_compiler_of(language);
            g_cli_gather.compiler[place] = (CliCompilerOf){language, path, (id != NULL) ? id : ""};
            g_cli_gather.compilers += (place == g_cli_gather.compilers) ? 1u : 0u;
        }
    }
    const unsigned int paths = cli_json_member(reply, model, "paths");
    const char *const source_path = cli_reply_string(paths, "source");
    const char *const build = cli_reply_string(paths, "build");
    const unsigned int configurations = cli_json_member(reply, model, "configurations");
    if ((source_path == NULL) || (build == NULL) || (cli_json_item(reply, configurations, 0u) == CLI_JSON_NONE))
    {
        return CLI_JSON_NONE;
    }
    // the configuration the build's folder is named for, as orior names it, or the first
    const char *named = build;
    for (const char *at = build; at[0] != '\0'; at += 1)
    {
        named = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : named;
    }
    unsigned int configuration = cli_json_item(reply, configurations, 0u);
    for (unsigned int at = 0u; cli_json_item(reply, configurations, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const name = cli_reply_string(cli_json_item(reply, configurations, at), "name");
        int same = (name != NULL) && (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) ==
                                      cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}));
        for (unsigned long long letter = 0ull; same && (name[letter] != '\0'); letter += 1ull)
        {
            same = (name[letter] | 0x20) == (named[letter] | 0x20);
        }
        if (same)
        {
            configuration = cli_json_item(reply, configurations, at);
            break;
        }
    }
    memor.cpy(&(MemoriaCfg){.dst = source,
                            .src = source_path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = source_path, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned int entries = cli_json_made(&g_cli_gather.json, CFG_JSON_ARRAY);
    const unsigned int targets = cli_json_member(reply, configuration, "targets");
    for (unsigned int at = 0u; cli_json_item(reply, targets, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int target = cli_reply_file(folder, cli_json_item(reply, targets, at));
        if (target != CLI_JSON_NONE)
        {
            cli_target_entries(target, source, build, entries);
        }
    }
    return entries;
}
