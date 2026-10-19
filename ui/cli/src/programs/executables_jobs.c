// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_jobs.c: the facts of this machine a build job is made from, the C compiler and linker a build for another
// machine needs, and the params of the job that builds each program the tree's scripts make
#include "executables_internal.h"

// the target choice that builds for the machine the build runs on, and the cgo choice that leaves it to Go
static const char s_this_machine[] = "this machine";

static const char s_as_go_chooses[] = "as Go chooses";

// the targets a Rust binary can be built for, by Rust's own names: the musl ones link as a whole file that asks for
// no glibc
static const char *const s_rust_targets[8] = {
    s_this_machine,           "x86_64-unknown-linux-musl", "aarch64-unknown-linux-musl", "x86_64-unknown-linux-gnu",
    "x86_64-pc-windows-msvc", "aarch64-pc-windows-msvc",   "x86_64-apple-darwin",        "aarch64-apple-darwin"};

void cli_program_file(const char *name, char *out)
{
    CliLine line = {out, CLI_NAME_LONGEST, 0ull};
    cli_line_text(&line, name);
#if defined(_WIN32)
    cli_line_text(&line, ".exe");
#endif
    cli_line_closed(&line);
}

void cli_folder_of(const char *root, const char *script, char *out)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(script, folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_relative(root, folder, out);
    if (out[0] == '\0')
    {
        memor.cpy(&(MemoriaCfg){
            .dst = out, .src = ".", .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
}

void cli_build_folder(const char *root, const char *script, const char *tool, char *out)
{
    char folder[CLI_PATH_LONGEST];
    cli_folder_of(root, script, folder);
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, "build/");
    cli_line_text(&line, tool);
    cli_line_character(&line, '/');
    if (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = ".", .cap = CLI_CELLS_POOL}))
    {
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
    }
    cli_line_closed(&line);
}

void cli_go_host(const char **system, const char **processor)
{
#if defined(_WIN32)
    *system = "windows";
#elif defined(__APPLE__)
    *system = "darwin";
#else
    *system = "linux";
#endif
#if defined(__x86_64__) || defined(_M_X64)
    *processor = "amd64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    *processor = "arm64";
#elif defined(__i386__) || defined(_M_IX86)
    *processor = "x86";
#elif defined(__arm__) || defined(_M_ARM)
    *processor = "arm";
#else
    *processor = "riscv64";
#endif
}

const char *cli_first(const CliValues *values, const char *key, const char *fallback)
{
    const char *const first = cli_values_first(values, key);
    return ((first != NULL) && (first[0] != '\0')) ? first : fallback;
}

// Zig's name for a system and processor, as `zig cc -target` takes it, from Go's names or Rust's target, into `out`,
// which holds CLI_NAME_LONGEST: 1, or 0 where Zig has none
static int cli_zig_target(const char *system, const char *processor, char *out)
{
    const char *const zig_processor =
        ((cellul.eq(&(CatenaFinitaCfg){.src = processor, .other = "amd64", .cap = CLI_CELLS_POOL})) ||
         (cellul.eq(&(CatenaFinitaCfg){.src = processor, .other = "x86_64", .cap = CLI_CELLS_POOL})))
            ? "x86_64"
        : ((cellul.eq(&(CatenaFinitaCfg){.src = processor, .other = "arm64", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = processor, .other = "aarch64", .cap = CLI_CELLS_POOL})))
            ? "aarch64"
            : NULL;
    const char *const zig_system =
        (cellul.eq(&(CatenaFinitaCfg){.src = system, .other = "linux", .cap = CLI_CELLS_POOL}))     ? "linux-musl"
        : (cellul.eq(&(CatenaFinitaCfg){.src = system, .other = "windows", .cap = CLI_CELLS_POOL})) ? "windows-gnu"
        : ((cellul.eq(&(CatenaFinitaCfg){.src = system, .other = "darwin", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = system, .other = "macos", .cap = CLI_CELLS_POOL})))
            ? "macos"
            : NULL;
    if ((zig_processor == NULL) || (zig_system == NULL))
    {
        return 0;
    }
    CliLine line = {out, CLI_NAME_LONGEST, 0ull};
    cli_line_text(&line, zig_processor);
    cli_line_character(&line, '-');
    cli_line_text(&line, zig_system);
    return cli_line_closed(&line) != 0ull;
}

// `zig cc` or `zig c++` for a Zig target, as a variable of the environment holds it, into `out`
static void cli_zig_command(const char *zig, const char *language, const char *target, char *out,
                            unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    cli_line_text(&line, zig);
    cli_line_character(&line, ' ');
    cli_line_text(&line, language);
    cli_line_text(&line, " -target ");
    cli_line_text(&line, target);
    cli_line_closed(&line);
}

void cli_executables_cross_env(const char *kind, const CliValues *values,
                               void (*each)(void *context, const char *name, const char *value), void *context)
{
    char zig[CLI_PATH_LONGEST + 2u];
    char found[CLI_PATH_LONGEST];
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const int has_zig = cli_executables_program("zig", "zig", found, &said);
    CliLine quoted = {zig, sizeof(zig), 0ull};
    const int spaced = cellul.chr(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL, .byte = ' '}) != NULL;
    cli_line_text(&quoted, (has_zig && spaced) ? "\"" : "");
    cli_line_text(&quoted, has_zig ? found : "");
    cli_line_text(&quoted, (has_zig && spaced) ? "\"" : "");
    cli_line_closed(&quoted);
    char target[CLI_NAME_LONGEST];
    char command[CLI_PATH_LONGEST + CLI_NAME_LONGEST];
    if (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "go", .cap = CLI_CELLS_POOL}))
    {
        const char *host_system = NULL;
        const char *host_processor = NULL;
        cli_go_host(&host_system, &host_processor);
        const char *const system = cli_first(values, "GOOS", host_system);
        const char *const processor = cli_first(values, "GOARCH", host_processor);
        const int other =
            (!cellul.eq(&(CatenaFinitaCfg){.src = system, .other = host_system, .cap = CLI_CELLS_POOL})) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = processor, .other = host_processor, .cap = CLI_CELLS_POOL}));
        if ((cellul.eq(&(CatenaFinitaCfg){
                .src = cli_first(values, "CGO_ENABLED", s_as_go_chooses), .other = "1", .cap = CLI_CELLS_POOL})) &&
            other && has_zig && cli_zig_target(system, processor, target))
        {
            cli_zig_command(zig, "cc", target, command, sizeof(command));
            each(context, "CC", command);
            cli_zig_command(zig, "c++", target, command, sizeof(command));
            each(context, "CXX", command);
        }
        return;
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "rust", .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    const char *const rust = cli_first(values, "target", s_this_machine);
    if (cellul.eq(&(CatenaFinitaCfg){.src = rust, .other = s_this_machine, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    // the target's variables' names: the target in upper case, `_` for each `-`
    char upper[CLI_NAME_LONGEST];
    char lower[CLI_NAME_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = upper, .src = rust, .bytes = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = lower, .src = rust, .bytes = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL}) + 1u});
    for (unsigned long long at = 0ull; upper[at] != '\0'; at += 1ull)
    {
        upper[at] = (upper[at] == '-')
                        ? '_'
                        : (((upper[at] >= 'a') && (upper[at] <= 'z')) ? (char)(upper[at] - 32) : upper[at]);
        lower[at] = (lower[at] == '-') ? '_' : lower[at];
    }
    char linker[CLI_NAME_LONGEST * 2u];
    CliLine named = {linker, sizeof(linker), 0ull};
    cli_line_text(&named, "CARGO_TARGET_");
    cli_line_text(&named, upper);
    cli_line_text(&named, "_LINKER");
    cli_line_closed(&named);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL});
    char set[CLI_PATH_LONGEST];
#if defined(__linux__)
    const int on_linux = 1;
#else
    const int on_linux = 0;
#endif
    if ((length >= 11ull) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = rust + length - 11ull, .other = "-linux-musl", .cap = CLI_CELLS_POOL})) &&
        !on_linux && !query_environment_read(linker, set, sizeof(set)))
    {
        each(context, linker, "rust-lld");
    }
    char processor[CLI_NAME_LONGEST];
    const char *const dash = cellul.chr(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL, .byte = '-'});
    CliLine part = {processor, sizeof(processor), 0ull};
    cli_line_counted(&part, rust, (dash != NULL) ? (unsigned long long)(dash - rust) : length);
    cli_line_closed(&part);
    const char *const system =
        (cellul.find(&(CatenaFinitaCfg){
             .src = rust, .cap = CLI_CELLS_POOL, .other = "linux", .other_cap = CLI_CELLS_POOL}) != NULL)
            ? "linux"
        : (cellul.find(&(CatenaFinitaCfg){
               .src = rust, .cap = CLI_CELLS_POOL, .other = "windows-gnu", .other_cap = CLI_CELLS_POOL}) != NULL)
            ? "windows"
        : (cellul.find(&(CatenaFinitaCfg){
               .src = rust, .cap = CLI_CELLS_POOL, .other = "apple", .other_cap = CLI_CELLS_POOL}) != NULL)
            ? "darwin"
            : "";
    if (has_zig && cli_zig_target(system, processor, target))
    {
        char compiler[CLI_NAME_LONGEST * 2u];
        CliLine name = {compiler, sizeof(compiler), 0ull};
        cli_line_text(&name, "CC_");
        cli_line_text(&name, lower);
        cli_line_closed(&name);
        cli_zig_command(zig, "cc", target, command, sizeof(command));
        each(context, compiler, command);
    }
}

// a choice param put on the last job, its first choice its default
static void cli_choice(CliCatalog *catalog, const char *key, const char *const *choices, unsigned int count)
{
    cli_catalog_param(catalog, key, "choice", 1);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_catalog_choice(catalog, choices[at]);
    }
}

// the params a kind of build takes before those every build takes
static void cli_kind_params(const CliExecutable *exe, CliCatalog *catalog)
{
    static const char *const s_profiles[2] = {"dev", "release"};
    static const char *const s_configs[4] = {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"};
    static const char *const s_buildtypes[4] = {"debug", "debugoptimized", "release", "minsize"};
    static const char *const s_cgo[3] = {s_as_go_chooses, "0", "1"};
    static const char *const s_dotnet_configs[2] = {"Debug", "Release"};
    static const char *const s_outputs[2] = {"files", "one file"};
    static const char *const s_optimize[4] = {"Debug", "ReleaseSafe", "ReleaseFast", "ReleaseSmall"};
    if (exe->kind == CLI_KIND_CARGO)
    {
        cli_choice(catalog, "profile", s_profiles, 2u);
        cli_choice(catalog, "target", s_rust_targets, 8u);
    }
    else if (exe->kind == CLI_KIND_CMAKE)
    {
        cli_choice(catalog, "config", s_configs, 4u);
    }
    else if (exe->kind == CLI_KIND_MESON)
    {
        cli_choice(catalog, "buildtype", s_buildtypes, 4u);
    }
    else if (exe->kind == CLI_KIND_GO)
    {
        const char *system = NULL;
        const char *processor = NULL;
        cli_go_host(&system, &processor);
        // each choice with this machine's first
        const char *const systems_all[3] = {"windows", "linux", "darwin"};
        const char *const processors_all[2] = {"amd64", "arm64"};
        const char *systems[4] = {system, NULL, NULL, NULL};
        const char *processors[3] = {processor, NULL, NULL};
        unsigned int system_count = 1u;
        unsigned int processor_count = 1u;
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            if (!cellul.eq(&(CatenaFinitaCfg){.src = systems_all[at], .other = system, .cap = CLI_CELLS_POOL}))
            {
                systems[system_count] = systems_all[at];
                system_count += 1u;
            }
        }
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            if (!cellul.eq(&(CatenaFinitaCfg){.src = processors_all[at], .other = processor, .cap = CLI_CELLS_POOL}))
            {
                processors[processor_count] = processors_all[at];
                processor_count += 1u;
            }
        }
        cli_choice(catalog, "GOOS", systems, system_count);
        cli_choice(catalog, "GOARCH", processors, processor_count);
        cli_choice(catalog, "CGO_ENABLED", s_cgo, 3u);
    }
    else if (exe->kind == CLI_KIND_DOTNET)
    {
        cli_choice(catalog, "configuration", s_dotnet_configs, 2u);
        cli_choice(catalog, "output", s_outputs, 2u);
    }
    else if ((exe->kind == CLI_KIND_ZIG) && exe->optimize)
    {
        cli_choice(catalog, "optimize", s_optimize, 4u);
    }
    else if ((exe->kind == CLI_KIND_NPM) && (exe->withs != 0u))
    {
        cli_choice(catalog, "with", exe->with, exe->withs);
    }
}

void cli_executables_jobs(const char *root, CliCatalog *catalog)
{
    static CliExecutables s_found;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    cli_executables_read(root, &s_found);
    for (unsigned int index = 0u; index < s_found.count; index += 1u)
    {
        const CliExecutable *const exe = &s_found.item[s_found.order[index]];
        unsigned int named = 0u;
        for (unsigned int other = 0u; other < s_found.count; other += 1u)
        {
            named += (cellul.eq(&(CatenaFinitaCfg){
                         .src = s_found.item[other].name, .other = exe->name, .cap = CLI_CELLS_POOL}))
                         ? 1u
                         : 0u;
        }
        char title[CLI_PATH_LONGEST];
        CliLine line = {title, sizeof(title), 0ull};
        if (named > 1u)
        {
            char folder[CLI_PATH_LONGEST];
            const char *base = "";
            if (cli_path_parent(exe->script, folder))
            {
                base = folder;
                for (const char *at = folder; at[0] != '\0'; at += 1)
                {
                    base = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : base;
                }
            }
            cli_line_text(&line, base);
            cli_line_character(&line, '/');
        }
        cli_line_text(&line, exe->name);
        cli_line_closed(&line);
        if (cli_executables_job_begun(root, exe, title, catalog))
        {
            cli_kind_params(exe, catalog);
            static const char *const s_then[3] = {"build", "run", "run as the system runs it"};
            cli_choice(catalog, "then", s_then, 3u);
            cli_catalog_trace(catalog);
            cli_catalog_param(catalog, "folder", "dir", 0);
            cli_executables_steps(root, exe, catalog);
        }
    }
    cli_lock_give(&s_lock);
}
