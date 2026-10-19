// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_steps.c: the job that builds a program the tree's scripts make: what it says of itself and its steps,
// each run with the build's own tool
#include "../toolchains/toolchains.h"
#include "executables_internal.h"

// whether Meson is to set up Visual Studio's environment itself: on Windows, where MSVC's cl is on the PATH and the
// environment it compiles in is not set, Meson takes cl and it cannot compile. Meson sets the environment up only where
// no cl is on the PATH, and its setup and compile run without cl's folders there
static int cli_needs_vsenv(void)
{
#if defined(_WIN32)
    char include[CLI_PATH_LONGEST];
    if (query_environment_read("INCLUDE", include, sizeof(include)))
    {
        return 0;
    }
    static const char *const s_cl[1] = {"cl"};
    char path[CLI_PATH_LONGEST];
    return cli_toolchains_on_path(s_cl, 1u, path);
#else
    return 0;
#endif
}

// .NET's name for this system and processor, as `-r` takes it
static const char *cli_dotnet_runtime(void)
{
#if defined(_WIN32) && (defined(__aarch64__) || defined(_M_ARM64))
    return "win-arm64";
#elif defined(_WIN32)
    return "win-x64";
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(_M_ARM64))
    return "osx-arm64";
#elif defined(__APPLE__)
    return "osx-x64";
#elif defined(__aarch64__)
    return "linux-arm64";
#else
    return "linux-x64";
#endif
}

// an argument written as it is
static void cli_lit(CliCatalog *catalog, const char *text)
{
    cli_catalog_arg(catalog, CLI_ARG_LIT, text, NULL, NULL, NULL, 0);
}

// an argument of the words listed, `count` of them, where the param `key`'s value is `value`
static void cli_when(CliCatalog *catalog, const char *key, const char *value, const char *const *words,
                     unsigned int count)
{
    cli_catalog_arg(catalog, CLI_ARG_WHEN, key, value, NULL, NULL, 0);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_catalog_word(catalog, words[at]);
    }
}

// the words a build tool says every command it runs by, where the build's trace is verbose
static void cli_verbose(CliCatalog *catalog, const char *const *words, unsigned int count)
{
    cli_when(catalog, CLI_TRACE, CLI_TRACE_VERBOSE, words, count);
}

// a step of the toolchain `tool`'s program `program`
static void cli_tool(CliCatalog *catalog, const char *tool, const char *program)
{
    cli_catalog_step(catalog, CLI_PROGRAM_TOOL, tool, program, 0);
}

// the script's path in the tree into `out`, which holds CLI_PATH_LONGEST
static void cli_script_of(const char *root, const CliExecutable *exe, char *out)
{
    cli_relative(root, exe->script, out);
}

// `inner` under the script's folder, as a path in the tree, into `out`
static void cli_under_folder(const char *folder, const char *inner, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    if (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = ".", .cap = CLI_CELLS_POOL}))
    {
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
    }
    cli_line_text(&line, inner);
    cli_line_closed(&line);
}

int cli_executables_job_begun(const char *root, const CliExecutable *exe, const char *title, CliCatalog *catalog)
{
    static char s_about[CLI_PATH_LONGEST * 4u];
    static CliLock s_lock = CLI_LOCK_INIT;
    char script[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char build[CLI_PATH_LONGEST];
    cli_script_of(root, exe, script);
    cli_folder_of(root, exe->script, folder);
    cli_lock_take(&s_lock);
    CliLine about = {s_about, sizeof(s_about), 0ull};
    if (exe->kind == CLI_KIND_CARGO)
    {
        cli_line_text(&about, "cargo builds the ");
        cli_line_text(&about, exe->name);
        cli_line_text(&about, " binary of ");
        cli_line_text(&about, script);
        cli_line_text(&about, ", by the profile chosen, for this machine or for the target chosen.");
    }
    else if ((exe->kind == CLI_KIND_CMAKE) || (exe->kind == CLI_KIND_MESON))
    {
        const int cmake = exe->kind == CLI_KIND_CMAKE;
        cli_build_folder(root, exe->script, cmake ? "cmake" : "meson", build);
        cli_line_text(&about, cmake ? "CMake configures " : "Meson sets up ");
        cli_line_text(&about, script);
        cli_line_text(&about, cmake ? " into " : " in ");
        cli_line_text(&about, build);
        cli_line_text(&about, cmake ? "<config> and builds its " : "<buildtype> and compiles its ");
        cli_line_text(&about, exe->name);
        cli_line_text(&about, cmake ? " target." : " executable.");
    }
    else if (exe->kind == CLI_KIND_GO)
    {
        cli_line_text(&about, "go builds the ");
        cli_line_text(&about, (exe->detail[0] != '\0') ? exe->detail : ".");
        cli_line_text(&about, " package of ");
        cli_line_text(&about, script);
        cli_line_text(&about, " into build/exe/<GOOS>-<GOARCH>/, for the system and processor chosen, with cgo as Go "
                              "chooses or as chosen.");
    }
    else if (exe->kind == CLI_KIND_DOTNET)
    {
        cli_line_text(&about, "dotnet builds ");
        cli_line_text(&about, script);
        cli_line_text(&about, " by the configuration chosen, as its files or published as one file with its native "
                              "libraries inside it.");
    }
    else if (exe->kind == CLI_KIND_ZIG)
    {
        cli_line_text(&about, "zig builds ");
        cli_line_text(&about, script);
        cli_line_text(&about, " into its zig-out, ");
        cli_line_text(&about, exe->name);
        cli_line_text(&about, " among the executables it installs.");
    }
    else if (exe->kind == CLI_KIND_PYINSTALLER)
    {
        char made[CLI_PATH_LONGEST];
        cli_under_folder(folder, exe->detail, made);
        cli_line_text(&about, "PyInstaller, run by the tree's Python, freezes ");
        cli_line_text(&about, script);
        cli_line_text(&about, " into ");
        cli_line_text(&about, made);
        cli_line_character(&about, '.');
    }
    else if (exe->kind == CLI_KIND_NPM)
    {
        cli_line_text(&about, "builds the ");
        cli_line_text(&about, exe->name);
        cli_line_text(&about, " bin of ");
        cli_line_text(&about, script);
        cli_line_text(&about, " into build/exe/");
        cli_line_text(&about, exe->name);
        cli_line_text(&about, ": with Node's own single executable, which holds Node's modules alone, with bun build "
                              "--compile, or with deno compile, given every permission a Node program has.");
    }
    cli_line_closed(&about);
    char id[CLI_PATH_LONGEST + CLI_NAME_LONGEST];
    CliLine named = {id, sizeof(id), 0ull};
    cli_line_text(&named, "executable/");
    cli_line_text(&named, script);
    cli_line_character(&named, ':');
    cli_line_text(&named, exe->name);
    cli_line_closed(&named);
    const unsigned int job = cli_catalog_job(catalog, id, "executable", title, script, s_about, "program");
    cli_lock_give(&s_lock);
    return job != CLI_CATALOG_NONE;
}

// the steps of a Cargo build
static void cli_cargo_steps(const char *root, const CliExecutable *exe, const char *folder, CliCatalog *catalog)
{
    static const char *const s_vv[1] = {"-vv"};
    char remap[CLI_PATH_LONGEST * 2u];
    CliLine line = {remap, sizeof(remap), 0ull};
    cli_line_text(&line, "build.rustflags=['--remap-path-prefix=");
    cli_line_text(&line, root);
    cli_line_text(&line, "=.']");
    cli_line_closed(&line);
    const char *const release[2] = {"--config", remap};
    cli_tool(catalog, "rust", "cargo");
    cli_catalog_arg(catalog, CLI_ARG_FOLDER, folder, NULL, NULL, NULL, 0);
    cli_lit(catalog, "build");
    cli_lit(catalog, "--bin");
    cli_lit(catalog, exe->name);
    cli_catalog_arg(catalog, CLI_ARG_FLAG, "--profile", "profile", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_FLAG_BUT, "--target", "target", "this machine", NULL, 0);
    cli_when(catalog, "profile", "release", release, 2u);
    cli_verbose(catalog, s_vv, 1u);
    cli_catalog_arg(catalog, CLI_ARG_CROSS, "rust", NULL, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
}

// the steps of a CMake build: the file API's query asked for its code model and toolchains, which the code help reads
// the build's compilers from, then the configure and the build
static void cli_cmake_steps(const char *root, const CliExecutable *exe, const char *folder, CliCatalog *catalog)
{
    static const char *const s_log[1] = {"--log-level=VERBOSE"};
    static const char *const s_verbose[1] = {"--verbose"};
    static const char *const s_queries[3][2] = {{"make_directory", "/.cmake/api/v1/query"},
                                                {"touch", "/.cmake/api/v1/query/codemodel-v2"},
                                                {"touch", "/.cmake/api/v1/query/toolchains-v1"}};
    char build[CLI_PATH_LONGEST];
    cli_build_folder(root, exe->script, "cmake", build);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_tool(catalog, "cmake", "cmake");
        cli_lit(catalog, "-E");
        cli_lit(catalog, s_queries[at][0]);
        cli_catalog_arg(catalog, CLI_ARG_AROUND, build, "config", s_queries[at][1], NULL, 0);
        cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
    }
    cli_tool(catalog, "cmake", "cmake");
    cli_lit(catalog, "-S");
    cli_lit(catalog, folder);
    cli_lit(catalog, "-B");
    cli_catalog_arg(catalog, CLI_ARG_AROUND, build, "config", "", NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_AROUND, "-DCMAKE_BUILD_TYPE=", "config", "", NULL, 0);
    cli_verbose(catalog, s_log, 1u);
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
    cli_tool(catalog, "cmake", "cmake");
    cli_lit(catalog, "--build");
    cli_catalog_arg(catalog, CLI_ARG_AROUND, build, "config", "", NULL, 0);
    cli_lit(catalog, "--target");
    cli_lit(catalog, exe->name);
    cli_lit(catalog, "--config");
    cli_catalog_arg(catalog, CLI_ARG_VALUE, "config", NULL, NULL, NULL, 0);
    cli_verbose(catalog, s_verbose, 1u);
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
}

// the steps of a Meson build: its setup, and its compile, each without cl's folders where Meson sets up Visual
// Studio's environment itself
static void cli_meson_steps(const char *root, const CliExecutable *exe, const char *folder, int vsenv,
                            CliCatalog *catalog)
{
    static const char *const s_verbose[1] = {"--verbose"};
    char build[CLI_PATH_LONGEST];
    cli_build_folder(root, exe->script, "meson", build);
    cli_tool(catalog, "meson", "meson");
    cli_lit(catalog, "setup");
    cli_lit(catalog, "--reconfigure");
    cli_catalog_arg(catalog, CLI_ARG_AROUND, build, "buildtype", "", NULL, 0);
    cli_lit(catalog, folder);
    cli_catalog_arg(catalog, CLI_ARG_AROUND, "--buildtype=", "buildtype", "", NULL, 0);
    if (vsenv)
    {
        cli_lit(catalog, "--vsenv");
        cli_catalog_arg(catalog, CLI_ARG_UNPATH, "cl", NULL, NULL, NULL, 0);
    }
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
    cli_tool(catalog, "meson", "meson");
    cli_lit(catalog, "compile");
    cli_lit(catalog, "-C");
    cli_catalog_arg(catalog, CLI_ARG_AROUND, build, "buildtype", "", NULL, 0);
    cli_lit(catalog, exe->name);
    cli_verbose(catalog, s_verbose, 1u);
    if (vsenv)
    {
        cli_catalog_arg(catalog, CLI_ARG_UNPATH, "cl", NULL, NULL, NULL, 0);
    }
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
}

// the step of a Go build, into build/exe/<GOOS>-<GOARCH>/ under the tree's top folder
static void cli_go_steps(const char *root, const CliExecutable *exe, const char *folder, CliCatalog *catalog)
{
    static const char *const s_verbose[2] = {"-v", "-x"};
    static const char s_out[] = "build/exe/{GOOS}-{GOARCH}/";
    cli_tool(catalog, "go", "go");
    cli_catalog_arg(catalog, CLI_ARG_ENV, "GOOS", NULL, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_ENV, "GOARCH", NULL, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_ENV_BUT, "CGO_ENABLED", "as Go chooses", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_CROSS, "go", NULL, NULL, NULL, 0);
    cli_lit(catalog, "build");
    char out[CLI_PATH_LONGEST];
    CliLine line = {out, sizeof(out), 0ull};
    if (!cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = ".", .cap = CLI_CELLS_POOL}))
    {
        cli_lit(catalog, "-C");
        cli_lit(catalog, folder);
        for (const char *at = root; at[0] != '\0'; at += 1)
        {
            cli_line_character(&line, (at[0] == '\\') ? '/' : at[0]);
        }
        cli_line_character(&line, '/');
    }
    cli_line_text(&line, s_out);
    cli_line_closed(&line);
    cli_lit(catalog, "-trimpath");
    cli_verbose(catalog, s_verbose, 2u);
    cli_lit(catalog, "-o");
    cli_catalog_arg(catalog, CLI_ARG_FORMAT, out, NULL, NULL, NULL, 0);
    cli_lit(catalog, (exe->detail[0] != '\0') ? exe->detail : ".");
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
}

// the step of a .NET build, a build of its files or a publish of one file with its native libraries inside it
static void cli_dotnet_steps(const char *script, CliCatalog *catalog, const char *runtime)
{
    static const char *const s_build[1] = {"build"};
    static const char *const s_publish[1] = {"publish"};
    static const char *const s_ci[1] = {"-p:ContinuousIntegrationBuild=true"};
    static const char *const s_detailed[2] = {"-v", "detailed"};
    const char *const one_file[6] = {"-r",
                                     runtime,
                                     "--self-contained",
                                     "true",
                                     "-p:PublishSingleFile=true",
                                     "-p:IncludeNativeLibrariesForSelfExtract=true"};
    cli_tool(catalog, "dotnet", "dotnet");
    cli_catalog_arg(catalog, CLI_ARG_SET, "DOTNET_CLI_TELEMETRY_OPTOUT", "1", NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_SET, "DOTNET_NOLOGO", "1", NULL, NULL, 0);
    cli_when(catalog, "output", "files", s_build, 1u);
    cli_when(catalog, "output", "one file", s_publish, 1u);
    cli_lit(catalog, script);
    cli_lit(catalog, "-c");
    cli_catalog_arg(catalog, CLI_ARG_VALUE, "configuration", NULL, NULL, NULL, 0);
    cli_when(catalog, "output", "one file", one_file, 6u);
    cli_when(catalog, "configuration", "Release", s_ci, 1u);
    cli_verbose(catalog, s_detailed, 2u);
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
}

// the steps of a Node bin's build: Node's own single executable, run as orior's own `sea`, bun build --compile, or
// deno compile, the one the job's `with` names
static void cli_npm_steps(const CliExecutable *exe, CliCatalog *catalog)
{
    static const char *const s_debug[1] = {"--log-level=debug"};
    char out[CLI_PATH_LONGEST];
    CliLine line = {out, sizeof(out), 0ull};
    cli_line_text(&line, "build/exe/");
    cli_line_text(&line, exe->name);
    cli_line_closed(&line);
    cli_catalog_step(catalog, CLI_PROGRAM_ORIOR, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_ONLY, "with", "node", NULL, NULL, 0);
    cli_lit(catalog, "sea");
    cli_lit(catalog, exe->detail);
    cli_lit(catalog, out);
    if (!exe->commonjs)
    {
        cli_lit(catalog, "module");
    }
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
    cli_tool(catalog, "bun", "bun");
    cli_catalog_arg(catalog, CLI_ARG_ONLY, "with", "bun", NULL, NULL, 0);
    cli_lit(catalog, "build");
    cli_lit(catalog, "--compile");
    cli_lit(catalog, exe->detail);
    cli_lit(catalog, "--outfile");
    cli_lit(catalog, out);
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
    cli_tool(catalog, "deno", "deno");
    cli_catalog_arg(catalog, CLI_ARG_ONLY, "with", "deno", NULL, NULL, 0);
    cli_lit(catalog, "compile");
    cli_lit(catalog, "-A");
    if (exe->commonjs)
    {
        cli_lit(catalog, "--unstable-detect-cjs");
    }
    cli_verbose(catalog, s_debug, 1u);
    cli_lit(catalog, "--output");
    cli_lit(catalog, out);
    cli_lit(catalog, exe->detail);
    cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
}

void cli_executables_steps(const char *root, const CliExecutable *exe, CliCatalog *catalog)
{
    static const char *const s_zig_verbose[1] = {"--verbose"};
    static const char *const s_pyinstaller_verbose[2] = {"--log-level", "DEBUG"};
    char script[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_script_of(root, exe, script);
    cli_folder_of(root, exe->script, folder);
    if (exe->kind == CLI_KIND_CARGO)
    {
        cli_cargo_steps(root, exe, folder, catalog);
    }
    else if (exe->kind == CLI_KIND_CMAKE)
    {
        cli_cmake_steps(root, exe, folder, catalog);
    }
    else if (exe->kind == CLI_KIND_MESON)
    {
        cli_meson_steps(root, exe, folder, cli_needs_vsenv(), catalog);
    }
    else if (exe->kind == CLI_KIND_GO)
    {
        cli_go_steps(root, exe, folder, catalog);
    }
    else if (exe->kind == CLI_KIND_DOTNET)
    {
        cli_dotnet_steps(script, catalog, cli_dotnet_runtime());
    }
    else if (exe->kind == CLI_KIND_ZIG)
    {
        cli_tool(catalog, "zig", "zig");
        cli_lit(catalog, "build");
        cli_lit(catalog, "--build-file");
        cli_lit(catalog, script);
        cli_verbose(catalog, s_zig_verbose, 1u);
        if (exe->optimize)
        {
            cli_catalog_arg(catalog, CLI_ARG_AROUND, "-Doptimize=", "optimize", "", NULL, 0);
        }
        cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
    }
    else if (exe->kind == CLI_KIND_PYINSTALLER)
    {
        char dist[CLI_PATH_LONGEST];
        char work[CLI_PATH_LONGEST];
        cli_under_folder(folder, "dist", dist);
        cli_under_folder(folder, "build", work);
        cli_tool(catalog, "python", "python");
        cli_lit(catalog, "-m");
        cli_lit(catalog, "PyInstaller");
        cli_lit(catalog, "--noconfirm");
        cli_verbose(catalog, s_pyinstaller_verbose, 2u);
        cli_lit(catalog, "--distpath");
        cli_lit(catalog, dist);
        cli_lit(catalog, "--workpath");
        cli_lit(catalog, work);
        cli_lit(catalog, script);
        cli_catalog_arg(catalog, CLI_ARG_SOURCE_DATE, NULL, NULL, NULL, NULL, 0);
    }
    else if (exe->kind == CLI_KIND_NPM)
    {
        cli_npm_steps(exe, catalog);
    }
    cli_catalog_step(catalog, CLI_PROGRAM_MADE, NULL, NULL, 0);
    cli_catalog_arg(catalog, CLI_ARG_ONLY, "then", "run", NULL, NULL, 0);
    cli_catalog_step(catalog, CLI_PROGRAM_MADE, NULL, NULL, 1);
    cli_catalog_arg(catalog, CLI_ARG_ONLY, "then", "run as the system runs it", NULL, NULL, 0);
}
