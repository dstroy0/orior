// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_jobs.c: the facts of this machine a build job is made from, the C compiler and linker a build for another
// machine needs, and the params of the job that builds each program the tree's scripts make
#include "../text/text.h"
#include "executables_internal.h"

// the target choice that builds for the machine the build runs on, and the cgo choice that leaves it to Go
static const char s_this_machine[] = "this machine";

static const char s_as_go_chooses[] = "as Go chooses";

// the targets a Rust binary can be built for, by Rust's own names: the musl ones link as a whole file that requires
// no glibc
static const char *const s_rust_targets[8] = {
    s_this_machine,           "x86_64-unknown-linux-musl", "aarch64-unknown-linux-musl", "x86_64-unknown-linux-gnu",
    "x86_64-pc-windows-msvc", "aarch64-pc-windows-msvc",   "x86_64-apple-darwin",        "aarch64-apple-darwin"};

void cli_program_file(const char *name, char *out)
{
    VerbaLinea line = {out, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, name);
#if defined(_WIN32)
    mmgr_line_put(&line, ".exe");
#endif
    mmgr_line_finish(&line);
}

void cli_folder_of(const char *root, const char *script, char *out)
{
    char folder[CLI_PATH_MAX_LEN];
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
    char folder[CLI_PATH_MAX_LEN];
    cli_folder_of(root, script, folder);
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, "build/");
    mmgr_line_put(&line, tool);
    mmgr_line_ch(&line, '/');
    if (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = ".", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(&line, folder);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_finish(&line);
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
// sized for CLI_NAME_MAX_LEN: 1, or 0 where Zig has none
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
    VerbaLinea line = {out, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, zig_processor);
    mmgr_line_ch(&line, '-');
    mmgr_line_put(&line, zig_system);
    return mmgr_line_finish(&line) != 0ull;
}

// `zig cc` or `zig c++` for a Zig target, as a variable of the environment stores it, into `out`
static void cli_zig_command(const char *zig, const char *language, const char *target, char *out,
                            unsigned long long room)
{
    VerbaLinea line = {out, room, 0ull};
    mmgr_line_put(&line, zig);
    mmgr_line_ch(&line, ' ');
    mmgr_line_put(&line, language);
    mmgr_line_put(&line, " -target ");
    mmgr_line_put(&line, target);
    mmgr_line_finish(&line);
}

void cli_executables_cross_env(const char *kind, const CliValues *values,
                               void (*each)(void *context, const char *name, const char *value), void *context)
{
    char zig[CLI_PATH_MAX_LEN + 2u];
    char found[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int has_zig = cli_executables_program("zig", "zig", found, &err_msg);
    VerbaLinea quoted = {zig, sizeof(zig), 0ull};
    const int spaced = cellul.chr(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL, .byte = ' '}) != NULL;
    mmgr_line_put(&quoted, (has_zig && spaced) ? "\"" : "");
    mmgr_line_put(&quoted, has_zig ? found : "");
    mmgr_line_put(&quoted, (has_zig && spaced) ? "\"" : "");
    mmgr_line_finish(&quoted);
    char target[CLI_NAME_MAX_LEN];
    char command[CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN];
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
    char upper[CLI_NAME_MAX_LEN];
    char lower[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = upper, .src = rust, .bytes = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = lower, .src = rust, .bytes = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned long long upper_size = cellul.len(&(CatenaFinitaCfg){.src = upper, .cap = CLI_CELLS_POOL});
    cli_text_swapped(upper, upper_size, '-', '_');
    cli_text_raised(upper, upper_size);
    cli_text_swapped(lower, upper_size, '-', '_');
    char linker[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea named = {linker, sizeof(linker), 0ull};
    mmgr_line_put(&named, "CARGO_TARGET_");
    mmgr_line_put(&named, upper);
    mmgr_line_put(&named, "_LINKER");
    mmgr_line_finish(&named);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL});
    char set[CLI_PATH_MAX_LEN];
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
    char processor[CLI_NAME_MAX_LEN];
    const char *const dash = cellul.chr(&(CatenaFinitaCfg){.src = rust, .cap = CLI_CELLS_POOL, .byte = '-'});
    VerbaLinea part = {processor, sizeof(processor), 0ull};
    mmgr_line_put_n(&part, rust, (dash != NULL) ? (unsigned long long)(dash - rust) : length);
    mmgr_line_finish(&part);
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
        char compiler[CLI_NAME_MAX_LEN * 2u];
        VerbaLinea name = {compiler, sizeof(compiler), 0ull};
        mmgr_line_put(&name, "CC_");
        mmgr_line_put(&name, lower);
        mmgr_line_finish(&name);
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
    CliExecutables found = {NULL, 0u, 0u, NULL};
    cli_executables_read(root, &found);
    for (unsigned int index = 0u; index < found.count; index += 1u)
    {
        const CliExecutable *const exe = &found.item[found.order[index]];
        unsigned int named = 0u;
        for (unsigned int other = 0u; other < found.count; other += 1u)
        {
            named += (cellul.eq(
                         &(CatenaFinitaCfg){.src = found.item[other].name, .other = exe->name, .cap = CLI_CELLS_POOL}))
                         ? 1u
                         : 0u;
        }
        char title[CLI_PATH_MAX_LEN];
        VerbaLinea line = {title, sizeof(title), 0ull};
        if (named > 1u)
        {
            char folder[CLI_PATH_MAX_LEN];
            const char *base = "";
            if (cli_path_parent(exe->script, folder))
            {
                base = folder;
                const unsigned long long base_past = cli_text_back_any(
                    folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), "/\\");
                base = (base_past != 0ull) ? (folder + base_past) : base;
            }
            mmgr_line_put(&line, base);
            mmgr_line_ch(&line, '/');
        }
        mmgr_line_put(&line, exe->name);
        mmgr_line_finish(&line);
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
    cli_executables_given(&found);
}
