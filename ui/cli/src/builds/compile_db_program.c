// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_program.c: the program a CMake or Meson build orior_cli configured makes from a source, read from CMake's
// file API reply and from Meson's intro-targets.json, each file held mapped and walked by key in one pass
#include "../text/text.h"
#include "compile_db_internal.h"

// The search: the folders found, the path wanted, and the best program yet with how it ranks
typedef struct
{
    CliFoundFolders folders;
    char wanted[CLI_PATH_LONGEST];
    int found;
    int debugging;
    unsigned long long time;
    CliProgram best;
} CliProgramSearch;

static CliProgramSearch s_search;

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
    const int higher = !s_search.found || (debugging > s_search.debugging) ||
                       ((debugging == s_search.debugging) && (time >= s_search.time));
    if (higher)
    {
        s_search.found = 1;
        s_search.debugging = debugging;
        s_search.time = time;
        s_search.best = *program;
    }
}

// `text` copied whole into `out`, which holds `size`
static void cli_copied(char *out, unsigned long long size, const char *text)
{
    CliLine line = {out, size, 0ull};
    cli_line_text(&line, text);
    cli_line_closed(&line);
}

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

// whether the source at `item` of a target's reply is the path wanted, joined to `source` where it is not absolute
static int cli_source_wanted(const CliReply *target, unsigned long long item, const char *source)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_reply_string(target, cli_reply_member(target, item, "path"), path, sizeof(path)))
    {
        return 0;
    }
    char full[CLI_PATH_LONGEST];
    char compared[CLI_PATH_LONGEST];
    if (cli_absolute(path))
    {
        cli_copied(full, sizeof(full), path);
    }
    else
    {
        cli_full(source, path, full);
    }
    cli_same(full, compared);
    return cellul.eq(&(CatenaFinitaCfg){.src = compared, .other = s_search.wanted, .cap = CLI_CELLS_POOL});
}

// the first artifact of the target that is a program, not its symbols or import library, into `program`: 1, or 0
static int cli_target_program(const CliReply *target, unsigned long long root, char *program)
{
    const unsigned long long artifacts = cli_reply_member(target, root, "artifacts");
    for (unsigned long long at = cli_reply_first(target, artifacts); at != CLI_REPLY_NONE;
         at = cli_reply_next(target, at))
    {
        if (!cli_reply_string(target, cli_reply_member(target, at, "path"), program, CLI_PATH_LONGEST))
        {
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = program, .cap = CLI_CELLS_POOL});
        const int skipped =
            (length >= 4ull) &&
            ((cellul.eq(&(CatenaFinitaCfg){.src = program + length - 4ull, .other = ".pdb", .cap = CLI_CELLS_POOL})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = program + length - 4ull, .other = ".lib", .cap = CLI_CELLS_POOL})));
        if (!skipped)
        {
            return 1;
        }
    }
    return 0;
}

// the id of the compiler the toolchains at `chains` give `language`, into `compiler`, "" where none is
static void cli_chain_compiler(const CliReply *chains_file, unsigned long long chains, const char *language,
                               char *compiler)
{
    compiler[0] = '\0';
    for (unsigned long long at = cli_reply_first(chains_file, chains); at != CLI_REPLY_NONE;
         at = cli_reply_next(chains_file, at))
    {
        char chain_language[CLI_NAME_LONGEST];
        if (cli_reply_string(chains_file, cli_reply_member(chains_file, at, "language"), chain_language,
                             sizeof(chain_language)) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = chain_language, .other = language, .cap = CLI_CELLS_POOL})))
        {
            (void)cli_reply_string(chains_file,
                                   cli_reply_member(chains_file, cli_reply_member(chains_file, at, "compiler"), "id"),
                                   compiler, CLI_NAME_LONGEST);
        }
    }
}

// one executable target of a CMake build put among those found where it makes `wanted` from a source
static void cli_cmake_target(const CliReply *target, const CliReply *chains_file, unsigned long long chains,
                             const char *source, const char *build, const char *config, unsigned long long time)
{
    const unsigned long long root = cli_reply_root(target);
    if (!cli_reply_string_is(target, cli_reply_member(target, root, "type"), "EXECUTABLE"))
    {
        return;
    }
    unsigned long long matched = CLI_REPLY_NONE;
    for (unsigned long long one = cli_reply_first(target, cli_reply_member(target, root, "sources"));
         (matched == CLI_REPLY_NONE) && (one != CLI_REPLY_NONE); one = cli_reply_next(target, one))
    {
        matched = cli_source_wanted(target, one, source) ? one : CLI_REPLY_NONE;
    }
    CliProgram found;
    if ((matched == CLI_REPLY_NONE) || !cli_target_program(target, root, found.program))
    {
        return;
    }
    // the source's language by its compile group, C where it has none
    const unsigned long long place = cli_reply_whole(target, cli_reply_member(target, matched, "compileGroupIndex"));
    const unsigned long long group =
        (place != ~0ull) ? cli_reply_item(target, cli_reply_member(target, root, "compileGroups"), place)
                         : CLI_REPLY_NONE;
    char language[CLI_NAME_LONGEST];
    if (!cli_reply_string(target, cli_reply_member(target, group, "language"), language, sizeof(language)))
    {
        cli_copied(language, sizeof(language), "C");
    }
    cli_copied(found.tool, sizeof(found.tool), "cmake");
    cli_copied(found.build, sizeof(found.build), build);
    if (!cli_reply_string(target, cli_reply_member(target, root, "name"), found.target, sizeof(found.target)))
    {
        found.target[0] = '\0';
    }
    cli_copied(found.config, sizeof(found.config), config);
    if (!cli_absolute(found.program))
    {
        char relative[CLI_PATH_LONGEST];
        cli_copied(relative, sizeof(relative), found.program);
        cli_full(build, relative, found.program);
    }
    cli_chain_compiler(chains_file, chains, language, found.compiler);
    cli_program_put(cli_one_of(config, "debug", "relwithdebinfo"), time, &found);
}

// the executable targets of one CMake build that make `wanted` from a source, each put among those found
static void cli_cmake_programs(const char *reply_folder)
{
    char index[CLI_PATH_LONGEST];
    CliStamped stamped;
    CliReply reply = {NULL, 0ull};
    if (!cli_compile_newest_index(reply_folder, index) || !cli_compile_stamped(index, &stamped) ||
        !cli_reply_held(index, &reply))
    {
        return;
    }
    const unsigned long long answers = cli_reply_member(&reply, cli_reply_root(&reply), "reply");
    CliReply model = {NULL, 0ull};
    CliReply chains_file = {NULL, 0ull};
    const int modelled =
        cli_reply_file_held(&reply, cli_reply_member(&reply, answers, "codemodel-v2"), reply_folder, &model);
    (void)cli_reply_file_held(&reply, cli_reply_member(&reply, answers, "toolchains-v1"), reply_folder, &chains_file);
    cli_reply_let_go(&reply);
    const unsigned long long chains = cli_reply_member(&chains_file, cli_reply_root(&chains_file), "toolchains");
    const unsigned long long paths = cli_reply_member(&model, cli_reply_root(&model), "paths");
    char source[CLI_PATH_LONGEST];
    char build[CLI_PATH_LONGEST];
    if (modelled && cli_reply_string(&model, cli_reply_member(&model, paths, "source"), source, sizeof(source)) &&
        cli_reply_string(&model, cli_reply_member(&model, paths, "build"), build, sizeof(build)))
    {
        // the configuration the build's folder is named for, or the first
        const char *named = build;
        for (const char *at = build; at[0] != '\0'; at += 1)
        {
            named = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : named;
        }
        const unsigned long long configurations = cli_reply_member(&model, cli_reply_root(&model), "configurations");
        unsigned long long configuration = cli_reply_first(&model, configurations);
        for (unsigned long long at = configuration; at != CLI_REPLY_NONE; at = cli_reply_next(&model, at))
        {
            char name[CLI_NAME_LONGEST];
            if (cli_reply_string(&model, cli_reply_member(&model, at, "name"), name, sizeof(name)) &&
                cli_one_of(name, named, named))
            {
                configuration = at;
                break;
            }
        }
        char config[CLI_NAME_LONGEST];
        if (!cli_reply_string(&model, cli_reply_member(&model, configuration, "name"), config, sizeof(config)))
        {
            config[0] = '\0';
        }
        const unsigned long long targets = cli_reply_member(&model, configuration, "targets");
        for (unsigned long long at = cli_reply_first(&model, targets); at != CLI_REPLY_NONE;
             at = cli_reply_next(&model, at))
        {
            CliReply target = {NULL, 0ull};
            if (cli_reply_file_held(&model, at, reply_folder, &target))
            {
                cli_cmake_target(&target, &chains_file, chains, source, build, config, stamped.time);
                cli_reply_let_go(&target);
            }
        }
    }
    cli_reply_let_go(&chains_file);
    cli_reply_let_go(&model);
}

// the compiler's kind by its stem in lower case: MSVC for cl, Clang where the stem holds clang, else GNU
static const char *cli_meson_compiler_kind(const char *compiler)
{
    char stem[CLI_NAME_LONGEST] = "";
    const char *name = compiler;
    for (const char *letter = name; letter[0] != '\0'; letter += 1)
    {
        name = ((letter[0] == '/') || (letter[0] == '\\')) ? (letter + 1) : name;
    }
    cli_copied(stem, sizeof(stem), name);
    char *const dot = cli_text_last(stem, cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}), '.');
    if ((dot != NULL) && (dot != stem))
    {
        dot[0] = '\0';
    }
    for (char *letter = stem; letter[0] != '\0'; letter += 1)
    {
        letter[0] = ((letter[0] >= 'A') && (letter[0] <= 'Z')) ? (char)(letter[0] + 32) : letter[0];
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = stem, .other = "cl", .cap = CLI_CELLS_POOL}))
    {
        return "MSVC";
    }
    return (cellul.find(&(CatenaFinitaCfg){
                .src = stem, .cap = CLI_CELLS_POOL, .other = "clang", .other_cap = CLI_CELLS_POOL}) != NULL)
               ? "Clang"
               : "GNU";
}

// the executables of one Meson build that make `wanted` from a source, each put among those found
static void cli_meson_programs(const char *info)
{
    char targets_path[CLI_PATH_LONGEST];
    char build[CLI_PATH_LONGEST];
    CliStamped stamped;
    CliReply reply = {NULL, 0ull};
    cli_full(info, "intro-targets.json", targets_path);
    if (!cli_compile_stamped(targets_path, &stamped) || !cli_path_parent(info, build) ||
        !cli_reply_held(targets_path, &reply))
    {
        return;
    }
    const char *config = build;
    for (const char *at = build; at[0] != '\0'; at += 1)
    {
        config = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : config;
    }
    for (unsigned long long target = cli_reply_first(&reply, cli_reply_root(&reply)); target != CLI_REPLY_NONE;
         target = cli_reply_next(&reply, target))
    {
        if (!cli_reply_string_is(&reply, cli_reply_member(&reply, target, "type"), "executable"))
        {
            continue;
        }
        const unsigned long long groups = cli_reply_member(&reply, target, "target_sources");
        unsigned long long group = CLI_REPLY_NONE;
        for (unsigned long long one = cli_reply_first(&reply, groups);
             (group == CLI_REPLY_NONE) && (one != CLI_REPLY_NONE); one = cli_reply_next(&reply, one))
        {
            const unsigned long long sources = cli_reply_member(&reply, one, "sources");
            for (unsigned long long place = cli_reply_first(&reply, sources); place != CLI_REPLY_NONE;
                 place = cli_reply_next(&reply, place))
            {
                char path[CLI_PATH_LONGEST];
                char compared[CLI_PATH_LONGEST];
                if (!cli_reply_string(&reply, place, path, sizeof(path)))
                {
                    continue;
                }
                cli_same(path, compared);
                if (cellul.eq(&(CatenaFinitaCfg){.src = compared, .other = s_search.wanted, .cap = CLI_CELLS_POOL}))
                {
                    group = one;
                    break;
                }
            }
        }
        CliProgram found;
        if ((group == CLI_REPLY_NONE) ||
            !cli_reply_string(&reply, cli_reply_item(&reply, cli_reply_member(&reply, target, "filename"), 0ull),
                              found.program, sizeof(found.program)))
        {
            continue;
        }
        char compiler[CLI_PATH_LONGEST];
        if (!cli_reply_string(&reply, cli_reply_item(&reply, cli_reply_member(&reply, group, "compiler"), 0ull),
                              compiler, sizeof(compiler)))
        {
            compiler[0] = '\0';
        }
        cli_copied(found.tool, sizeof(found.tool), "meson");
        cli_copied(found.build, sizeof(found.build), build);
        if (!cli_reply_string(&reply, cli_reply_member(&reply, target, "name"), found.target, sizeof(found.target)))
        {
            found.target[0] = '\0';
        }
        cli_copied(found.config, sizeof(found.config), config);
        cli_copied(found.compiler, sizeof(found.compiler), cli_meson_compiler_kind(compiler));
        cli_program_put(cli_one_of(config, "debug", "debugoptimized"), stamped.time, &found);
    }
    cli_reply_let_go(&reply);
}

int cli_compile_db_program_of(const char *root, const char *file, CliProgram *program)
{
    static const char *const s_reply[4] = {".cmake", "api", "v1", "reply"};
    static const char *const s_info[1] = {"meson-info"};
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    cli_same(file, s_search.wanted);
    s_search.found = 0;
    s_search.folders = (CliFoundFolders){NULL, 0u, 0u};
    char top[CLI_PATH_LONGEST];
    cli_full(root, "build/cmake", top);
    cli_compile_folders_named(top, s_reply, 4u, &s_search.folders);
    for (unsigned int at = 0u; at < s_search.folders.count; at += 1u)
    {
        cli_cmake_programs(s_search.folders.path[at]);
    }
    cli_full(root, "build/meson", top);
    cli_compile_folders_named(top, s_info, 1u, &s_search.folders);
    for (unsigned int at = 0u; at < s_search.folders.count; at += 1u)
    {
        cli_meson_programs(s_search.folders.path[at]);
    }
    const int found = s_search.found;
    if (found)
    {
        *program = s_search.best;
    }
    cli_compile_found_given(&s_search.folders);
    cli_lock_give(&s_lock);
    return found;
}
