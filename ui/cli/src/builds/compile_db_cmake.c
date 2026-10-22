// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_cmake.c: a CMake build's entries read from its file API reply: one for each source of each target in
// the configuration the build's folder is named for, as the build's own compiler is given it, C and C++ alone. Each
// reply file is held mapped and walked by key in one pass, the entries made straight into the gathered tree
#include "compile_db_internal.h"

// the reply file a member `jsonFile` of the object at `node` of `reply` names, in the reply folder `folder`, held
// mapped into `file`: 1, or 0 where the member names none or the file cannot be read
static int cli_reply_file_held(const CliReply *reply, unsigned long long node, const char *folder, CliReply *file)
{
    char name[CLI_PATH_LONGEST];
    if (!cli_reply_string(reply, cli_reply_member(reply, node, "jsonFile"), name, sizeof(name)))
    {
        return 0;
    }
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    return cli_reply_held(path, file);
}

// a flag put after the compile group's flags
static void cli_flag_put(const char *flag, unsigned long long size)
{
    CliLine *const text = &g_cli_gather.flags_text;
    const unsigned long long mark = text->at;
    if ((g_cli_gather.flags >= CLI_GATHER_FLAGS) || (mark > 0xffffffffull))
    {
        return;
    }
    cli_line_counted(text, flag, size);
    cli_line_character(text, '\0');
    // a flag whose room the cellblock cannot meet is left out, the flags before it kept
    if (!cli_line_fits(text))
    {
        text->at = mark;
        return;
    }
    // where the flag starts, below 4 GiB as the test above holds, which an unsigned int holds
    g_cli_gather.flag[g_cli_gather.flags] = (unsigned int)mark;
    g_cli_gather.flags += 1u;
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

// a fragment of a compile command at `at` of the target's reply decoded into the fragment line, which grows to its
// bytes as written, and its words put after the flags
static void cli_fragment_flags(const CliReply *target, unsigned long long at)
{
    CliLine *const text = &g_cli_gather.fragment_text;
    const unsigned long long span = cli_reply_string_span(target, at);
    text->at = 0ull;
    if (!cli_line_room(text, span + 1ull) || !cli_reply_string(target, at, text->out, span + 1ull))
    {
        return;
    }
    cli_compile_words_each(text->out, cellul.len(&(CatenaFinitaCfg){.src = text->out, .cap = CLI_CELLS_POOL}),
                           cli_fragment_word, NULL);
}

// a compile group's flags: its fragments' words, its include folders, its defines and its sysroot
static void cli_group_flags(const CliReply *target, unsigned long long group, int msvc)
{
    g_cli_gather.flags = 0u;
    g_cli_gather.flags_text.at = 0ull;
    const unsigned long long fragments = cli_reply_member(target, group, "compileCommandFragments");
    for (unsigned long long at = cli_reply_first(target, fragments); at != CLI_REPLY_NONE;
         at = cli_reply_next(target, at))
    {
        cli_fragment_flags(target, cli_reply_member(target, at, "fragment"));
    }
    const unsigned long long includes = cli_reply_member(target, group, "includes");
    for (unsigned long long at = cli_reply_first(target, includes); at != CLI_REPLY_NONE;
         at = cli_reply_next(target, at))
    {
        char path[CLI_PATH_LONGEST];
        const int system = cli_reply_true(target, cli_reply_member(target, at, "isSystem"));
        if (cli_reply_string(target, cli_reply_member(target, at, "path"), path, sizeof(path)))
        {
            cli_flag_made(msvc ? "/I" : (system ? "-isystem" : "-I"), path);
        }
    }
    const unsigned long long defines = cli_reply_member(target, group, "defines");
    for (unsigned long long at = cli_reply_first(target, defines); at != CLI_REPLY_NONE;
         at = cli_reply_next(target, at))
    {
        char define[CLI_PATH_LONGEST];
        if (cli_reply_string(target, cli_reply_member(target, at, "define"), define, sizeof(define)))
        {
            cli_flag_made(msvc ? "/D" : "-D", define);
        }
    }
    char sysroot[CLI_PATH_LONGEST];
    if (cli_reply_string(target, cli_reply_member(target, cli_reply_member(target, group, "sysroot"), "path"), sysroot,
                         sizeof(sysroot)))
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

// the compilers of the toolchains file at `node` of the index's reply read: each language's path and id
static void cli_toolchains_read(const CliReply *reply, unsigned long long node, const char *folder)
{
    CliReply chains_file = {NULL, 0ull};
    g_cli_gather.compilers = 0u;
    if (!cli_reply_file_held(reply, node, folder, &chains_file))
    {
        return;
    }
    const unsigned long long chains = cli_reply_member(&chains_file, cli_reply_root(&chains_file), "toolchains");
    for (unsigned long long at = cli_reply_first(&chains_file, chains);
         (g_cli_gather.compilers < CLI_GATHER_LANGUAGES) && (at != CLI_REPLY_NONE);
         at = cli_reply_next(&chains_file, at))
    {
        CliCompilerOf one;
        const unsigned long long compiler = cli_reply_member(&chains_file, at, "compiler");
        if (!cli_reply_string(&chains_file, cli_reply_member(&chains_file, at, "language"), one.language,
                              sizeof(one.language)) ||
            !cli_reply_string(&chains_file, cli_reply_member(&chains_file, compiler, "path"), one.path,
                              sizeof(one.path)))
        {
            continue;
        }
        if (!cli_reply_string(&chains_file, cli_reply_member(&chains_file, compiler, "id"), one.id, sizeof(one.id)))
        {
            one.id[0] = '\0';
        }
        const unsigned int place = cli_compiler_of(one.language);
        g_cli_gather.compiler[place] = one;
        g_cli_gather.compilers += (place == g_cli_gather.compilers) ? 1u : 0u;
    }
    cli_reply_let_go(&chains_file);
}

// one entry made for the source `path` of a compile group whose flags are read, appended to `entries`
static void cli_entry_made(const char *source, const char *build, const char *path, const CliCompilerOf *compiler,
                           int msvc, unsigned int entries)
{
    CliJson *const json = &g_cli_gather.json;
    char file[CLI_PATH_LONGEST];
    if ((path[0] == '/') || (path[0] == '\\') || ((path[0] != '\0') && (path[1] == ':')))
    {
        memor.cpy(&(MemoriaCfg){.dst = file,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_full(source, path, file);
    }
    const unsigned int arguments = cli_json_made(json, CFG_JSON_ARRAY);
    cli_json_append(json, arguments, cli_slashed_made(compiler->path));
    for (unsigned int flag = 0u; flag < g_cli_gather.flags; flag += 1u)
    {
        const char *const text = g_cli_gather.flags_text.out + g_cli_gather.flag[flag];
        cli_json_append(
            json, arguments,
            cli_json_string_made(json, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})));
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

// the entries of a target's C and C++ compile groups, one for each source, appended to `entries`: the group's source
// indexes rise, so its sources are walked once beside them
static void cli_target_entries(const CliReply *target, const char *source, const char *build, unsigned int entries)
{
    const unsigned long long root = cli_reply_root(target);
    const unsigned long long sources = cli_reply_member(target, root, "sources");
    const unsigned long long groups = cli_reply_member(target, root, "compileGroups");
    for (unsigned long long group = cli_reply_first(target, groups); group != CLI_REPLY_NONE;
         group = cli_reply_next(target, group))
    {
        char language[CLI_NAME_LONGEST];
        if (!cli_reply_string(target, cli_reply_member(target, group, "language"), language, sizeof(language)) ||
            ((!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "C", .cap = CLI_CELLS_POOL})) &&
             (!cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "CXX", .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        const unsigned int compiler = cli_compiler_of(language);
        if (compiler == g_cli_gather.compilers)
        {
            continue;
        }
        const CliCompilerOf *const one = &g_cli_gather.compiler[compiler];
        const int msvc = (cellul.eq(&(CatenaFinitaCfg){.src = one->id, .other = "MSVC", .cap = CLI_CELLS_POOL})) ||
                         cli_compile_db_is_cl(one->path);
        cli_group_flags(target, group, msvc);
        unsigned long long index = cli_reply_first(target, cli_reply_member(target, group, "sourceIndexes"));
        unsigned long long place = 0ull;
        for (unsigned long long item = cli_reply_first(target, sources);
             (item != CLI_REPLY_NONE) && (index != CLI_REPLY_NONE); item = cli_reply_next(target, item))
        {
            while ((index != CLI_REPLY_NONE) && (cli_reply_whole(target, index) < place))
            {
                index = cli_reply_next(target, index);
            }
            if ((index != CLI_REPLY_NONE) && (cli_reply_whole(target, index) == place))
            {
                char path[CLI_PATH_LONGEST];
                if (cli_reply_string(target, cli_reply_member(target, item, "path"), path, sizeof(path)))
                {
                    cli_entry_made(source, build, path, one, msvc, entries);
                }
                index = cli_reply_next(target, index);
            }
            place += 1ull;
        }
    }
}

// the configuration of the codemodel the build's folder is named for, as orior_cli names it, or the first
static unsigned long long cli_configuration_of(const CliReply *model, unsigned long long configurations,
                                               const char *build)
{
    const char *named = build;
    for (const char *at = build; at[0] != '\0'; at += 1)
    {
        named = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : named;
    }
    const unsigned long long first = cli_reply_first(model, configurations);
    for (unsigned long long at = first; at != CLI_REPLY_NONE; at = cli_reply_next(model, at))
    {
        char name[CLI_NAME_LONGEST];
        int same = cli_reply_string(model, cli_reply_member(model, at, "name"), name, sizeof(name)) &&
                   (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) ==
                    cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}));
        for (unsigned long long letter = 0ull; same && (name[letter] != '\0'); letter += 1ull)
        {
            same = (name[letter] | 0x20) == (named[letter] | 0x20);
        }
        if (same)
        {
            return at;
        }
    }
    return first;
}

unsigned int cli_compile_cmake_entries(const char *index, char *source)
{
    char folder[CLI_PATH_LONGEST];
    CliReply reply = {NULL, 0ull};
    if (!cli_path_parent(index, folder) || !cli_reply_held(index, &reply))
    {
        return CLI_JSON_NONE;
    }
    const unsigned long long answers = cli_reply_member(&reply, cli_reply_root(&reply), "reply");
    CliReply model = {NULL, 0ull};
    const int modelled = cli_reply_file_held(&reply, cli_reply_member(&reply, answers, "codemodel-v2"), folder, &model);
    cli_toolchains_read(&reply, cli_reply_member(&reply, answers, "toolchains-v1"), folder);
    cli_reply_let_go(&reply);
    if (!modelled)
    {
        return CLI_JSON_NONE;
    }
    const unsigned long long paths = cli_reply_member(&model, cli_reply_root(&model), "paths");
    const unsigned long long configurations = cli_reply_member(&model, cli_reply_root(&model), "configurations");
    char build[CLI_PATH_LONGEST];
    if (!cli_reply_string(&model, cli_reply_member(&model, paths, "source"), source, CLI_PATH_LONGEST) ||
        !cli_reply_string(&model, cli_reply_member(&model, paths, "build"), build, sizeof(build)) ||
        (cli_reply_first(&model, configurations) == CLI_REPLY_NONE))
    {
        cli_reply_let_go(&model);
        return CLI_JSON_NONE;
    }
    const unsigned long long configuration = cli_configuration_of(&model, configurations, build);
    const unsigned int entries = cli_json_made(&g_cli_gather.json, CFG_JSON_ARRAY);
    const unsigned long long targets = cli_reply_member(&model, configuration, "targets");
    for (unsigned long long at = cli_reply_first(&model, targets); at != CLI_REPLY_NONE;
         at = cli_reply_next(&model, at))
    {
        CliReply target = {NULL, 0ull};
        if (cli_reply_file_held(&model, at, folder, &target))
        {
            cli_target_entries(&target, source, build, entries);
            cli_reply_let_go(&target);
        }
    }
    cli_reply_let_go(&model);
    return entries;
}
