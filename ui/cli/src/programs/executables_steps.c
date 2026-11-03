// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_steps.c: the job that builds a program the tree's scripts make: what it declares of itself and its steps,
// each run with the build's own tool
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "executables_internal.h"

// whether Meson is to set up Visual Studio's environment itself: on Windows, where MSVC's cl is on the PATH and the
// environment it compiles in is not set, Meson takes cl and it cannot compile. Meson sets the environment up only where
// no cl is on the PATH, and its setup and compile run without cl's folders there
static int cli_needs_vsenv(void)
{
#if defined(_WIN32)
    char include[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "INCLUDE",
                   .value_out = include, .capacity = sizeof(include)))
    {
        return 0;
    }
    static const char *const s_cl[1] = {"cl"};
    char path[CLI_PATH_MAX_LEN];
    return EMBED_CALL(cli_toolchains_names.on_path, CliToolchainsNamesCfg, .names = s_cl, .count = 1u, .program = path);
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

typedef struct
{
    CliCatalog *catalog;
    const char *text;
    const char *key;
    const char *value;
    const char *const *words;
    unsigned int count;
    const char *tool;
    const char *program;
    const char *root;
    const CliExecutable *exe;
    char *out;
    const char *folder;
    const char *inner;
    int vsenv;
    const char *script;
    const char *runtime;
} ExecutablesStepsCtx;
// an argument written as it is
EMBED_INLINE void cli_lit(const ExecutablesStepsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_LIT, .a = args->text,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// an argument of the words listed, `count` of them, where the param `key`'s value is `value`
EMBED_INLINE void cli_when(const ExecutablesStepsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_WHEN, .a = args->key,
               .b = args->value, .c = NULL, .d = NULL, .separated = 0);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(cli_catalog_word_table.word_text, CliCatalogWordCfg, .catalog = catalog,
                   .word_text = args->words[at]);
    }
}

// the words a build tool prints every command it runs by, where the build's trace is verbose
EMBED_INLINE void cli_verbose(const ExecutablesStepsCtx *args)
{
    EMBED_CALL(cli_when, ExecutablesStepsCtx, .catalog = args->catalog, .key = CLI_TRACE, .value = CLI_TRACE_VERBOSE,
               .words = args->words, .count = args->count);
}

// a step of the toolchain `tool`'s program `program`
EMBED_INLINE void cli_tool(const ExecutablesStepsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    const char *program = args->program;
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog, .program = CLI_PROGRAM_TOOL,
               .text = args->tool, .tool_program = program, .system = 0);
}

// the script's path in the tree into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_script_get(const ExecutablesStepsCtx *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = args->exe->script,
               .relative_out = args->out);
}

// `inner` under the script's folder, as a path in the tree, into `out`
EMBED_INLINE void cli_under_folder(const ExecutablesStepsCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->folder, .other = ".", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(&line, args->folder);
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_put(&line, args->inner);
    mmgr_line_finish(&line);
}

int cli_executables_job_begin(const CliExecutablesRootCtx *args)
{
    static char s_about[CLI_PATH_MAX_LEN * 4u];
    char script[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    char build[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_script_get, ExecutablesStepsCtx, .root = args->root, .exe = args->exe, .out = script);
    EMBED_CALL(cli_executables_out_internal.folder_get, CliExecutablesOutCtx, .root = args->root,
               .script = args->exe->script, .out = folder);
    VerbaLinea about = {s_about, sizeof(s_about), 0ULL};
    if (args->exe->kind == CLI_KIND_CARGO)
    {
        mmgr_line_put(&about, "cargo builds the ");
        mmgr_line_put(&about, args->exe->name);
        mmgr_line_put(&about, " binary of ");
        mmgr_line_put(&about, script);
        mmgr_line_put(&about, ", by the profile chosen, for this machine or for the target chosen.");
    }
    else if ((args->exe->kind == CLI_KIND_CMAKE) || (args->exe->kind == CLI_KIND_MESON))
    {
        const int cmake = args->exe->kind == CLI_KIND_CMAKE;
        EMBED_CALL(cli_executables_out_internal.build_folder, CliExecutablesOutCtx, .root = args->root,
                   .script = args->exe->script, .tool = cmake ? "cmake" : "meson", .out = build);
        mmgr_line_put(&about, cmake ? "CMake configures " : "Meson sets up ");
        mmgr_line_put(&about, script);
        mmgr_line_put(&about, cmake ? " into " : " in ");
        mmgr_line_put(&about, build);
        mmgr_line_put(&about, cmake ? "<config> and builds its " : "<buildtype> and compiles its ");
        mmgr_line_put(&about, args->exe->name);
        mmgr_line_put(&about, cmake ? " target." : " executable.");
    }
    else if (args->exe->kind == CLI_KIND_GO)
    {
        mmgr_line_put(&about, "go builds the ");
        mmgr_line_put(&about, (args->exe->detail[0] != '\0') ? args->exe->detail : ".");
        mmgr_line_put(&about, " package of ");
        mmgr_line_put(&about, script);
        mmgr_line_put(&about, " into build/exe/<GOOS>-<GOARCH>/, for the system and processor chosen, with cgo as Go "
                              "chooses or as chosen.");
    }
    else if (args->exe->kind == CLI_KIND_DOTNET)
    {
        mmgr_line_put(&about, "dotnet builds ");
        mmgr_line_put(&about, script);
        mmgr_line_put(&about, " by the configuration chosen, as its files or published as one file with its native "
                              "libraries inside it.");
    }
    else if (args->exe->kind == CLI_KIND_ZIG)
    {
        mmgr_line_put(&about, "zig builds ");
        mmgr_line_put(&about, script);
        mmgr_line_put(&about, " into its zig-out, ");
        mmgr_line_put(&about, args->exe->name);
        mmgr_line_put(&about, " among the executables it installs.");
    }
    else if (args->exe->kind == CLI_KIND_PYINSTALLER)
    {
        char made[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_under_folder, ExecutablesStepsCtx, .folder = folder, .inner = args->exe->detail, .out = made);
        mmgr_line_put(&about, "PyInstaller, run by the tree's Python, freezes ");
        mmgr_line_put(&about, script);
        mmgr_line_put(&about, " into ");
        mmgr_line_put(&about, made);
        mmgr_line_ch(&about, '.');
    }
    else if (args->exe->kind == CLI_KIND_NPM)
    {
        mmgr_line_put(&about, "builds the ");
        mmgr_line_put(&about, args->exe->name);
        mmgr_line_put(&about, " bin of ");
        mmgr_line_put(&about, script);
        mmgr_line_put(&about, " into build/exe/");
        mmgr_line_put(&about, args->exe->name);
        mmgr_line_put(&about,
                      ": with Node's own single executable, which contains Node's modules alone, with bun build "
                      "--compile, or with deno compile, given every permission a Node program has.");
    }
    mmgr_line_finish(&about);
    char id[CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN];
    VerbaLinea named = {id, sizeof(id), 0ULL};
    mmgr_line_put(&named, "executable/");
    mmgr_line_put(&named, script);
    mmgr_line_ch(&named, ':');
    mmgr_line_put(&named, args->exe->name);
    mmgr_line_finish(&named);
    const unsigned int job =
        EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = args->catalog, .id = id,
                   .group = "executable", .title = args->title, .file = script, .about = s_about, .opens = "program");
    return job != CLI_CATALOG_NONE;
}

// the steps of a Cargo build
EMBED_INLINE void cli_cargo_steps(const ExecutablesStepsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    static const char *const s_vv[1] = {"-vv"};
    char remap[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {remap, sizeof(remap), 0ULL};
    mmgr_line_put(&line, "build.rustflags=['--remap-path-prefix=");
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "=.']");
    mmgr_line_finish(&line);
    const char *const release[2] = {"--config", remap};
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "rust", .program = "cargo");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_FOLDER,
               .a = args->folder, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "build");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--bin");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->exe->name);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_FLAG, .a = "--profile",
               .b = "profile", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_FLAG_BUT,
               .a = "--target", .b = "target", .c = "this machine", .d = NULL, .separated = 0);
    EMBED_CALL(cli_when, ExecutablesStepsCtx, .catalog = catalog, .key = "profile", .value = "release",
               .words = release, .count = 2u);
    EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = catalog, .words = s_vv, .count = 1u);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_CROSS, .a = "rust",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// the steps of a CMake build: the file API's query requested for its code model and toolchains, which the code help
// reads the build's compilers from, then the configure and the build
EMBED_INLINE void cli_cmake_steps(const ExecutablesStepsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    static const char *const s_log[1] = {"--log-level=VERBOSE"};
    static const char *const s_verbose[1] = {"--verbose"};
    static const char *const s_queries[3][2] = {{"make_directory", "/.cmake/api/v1/query"},
                                                {"touch", "/.cmake/api/v1/query/codemodel-v2"},
                                                {"touch", "/.cmake/api/v1/query/toolchains-v1"}};
    char build[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_executables_out_internal.build_folder, CliExecutablesOutCtx, .root = root,
               .script = args->exe->script, .tool = "cmake", .out = build);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "cmake", .program = "cmake");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-E");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = s_queries[at][0]);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_AROUND, .a = build,
                   .b = "config", .c = s_queries[at][1], .d = NULL, .separated = 0);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE,
                   .a = NULL, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "cmake", .program = "cmake");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-S");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->folder);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-B");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_AROUND, .a = build,
               .b = "config", .c = "", .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_AROUND,
               .a = "-DCMAKE_BUILD_TYPE=", .b = "config", .c = "", .d = NULL, .separated = 0);
    EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = catalog, .words = s_log, .count = 1u);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "cmake", .program = "cmake");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--build");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_AROUND, .a = build,
               .b = "config", .c = "", .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--target");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->exe->name);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--config");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_VALUE, .a = "config",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = catalog, .words = s_verbose, .count = 1u);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// the steps of a Meson build: its setup, and its compile, each without cl's folders where Meson sets up Visual
// Studio's environment itself
EMBED_INLINE void cli_meson_steps(const ExecutablesStepsCtx *args)
{
    const char *root = args->root;
    CliCatalog *catalog = args->catalog;
    static const char *const s_verbose[1] = {"--verbose"};
    char build[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_executables_out_internal.build_folder, CliExecutablesOutCtx, .root = root,
               .script = args->exe->script, .tool = "meson", .out = build);
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "meson", .program = "meson");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "setup");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--reconfigure");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_AROUND, .a = build,
               .b = "buildtype", .c = "", .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->folder);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_AROUND,
               .a = "--buildtype=", .b = "buildtype", .c = "", .d = NULL, .separated = 0);
    if (args->vsenv)
    {
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--vsenv");
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_UNPATH, .a = "cl",
                   .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "meson", .program = "meson");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "compile");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-C");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_AROUND, .a = build,
               .b = "buildtype", .c = "", .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->exe->name);
    EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = catalog, .words = s_verbose, .count = 1u);
    if (args->vsenv)
    {
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_UNPATH, .a = "cl",
                   .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// the step of a Go build, into build/exe/<GOOS>-<GOARCH>/ under the tree's top folder
EMBED_INLINE void cli_go_steps(const ExecutablesStepsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    static const char *const s_verbose[2] = {"-v", "-x"};
    static const char s_out[] = "build/exe/{GOOS}-{GOARCH}/";
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "go", .program = "go");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_ENV, .a = "GOOS",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_ENV, .a = "GOARCH",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_ENV_BUT,
               .a = "CGO_ENABLED", .b = "as Go chooses", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_CROSS, .a = "go",
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "build");
    char out[CLI_PATH_MAX_LEN];
    VerbaLinea line = {out, sizeof(out), 0ULL};
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->folder, .other = ".", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-C");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->folder);
        (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = args->root, .from = '\\',
                         .to = '/');
        mmgr_line_ch(&line, '/');
    }
    mmgr_line_put(&line, s_out);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-trimpath");
    EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = catalog, .words = s_verbose, .count = 2u);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-o");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_FORMAT, .a = out,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog,
               .text = (args->exe->detail[0] != '\0') ? args->exe->detail : ".");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// the step of a .NET build, a build of its files or a publish of one file with its native libraries inside it
EMBED_INLINE void cli_dotnet_steps(const ExecutablesStepsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    static const char *const s_build[1] = {"build"};
    static const char *const s_publish[1] = {"publish"};
    static const char *const s_ci[1] = {"-p:ContinuousIntegrationBuild=true"};
    static const char *const s_detailed[2] = {"-v", "detailed"};
    const char *const one_file[6] = {"-r",
                                     args->runtime,
                                     "--self-contained",
                                     "true",
                                     "-p:PublishSingleFile=true",
                                     "-p:IncludeNativeLibrariesForSelfExtract=true"};
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "dotnet", .program = "dotnet");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SET,
               .a = "DOTNET_CLI_TELEMETRY_OPTOUT", .b = "1", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SET,
               .a = "DOTNET_NOLOGO", .b = "1", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_when, ExecutablesStepsCtx, .catalog = catalog, .key = "output", .value = "files", .words = s_build,
               .count = 1u);
    EMBED_CALL(cli_when, ExecutablesStepsCtx, .catalog = catalog, .key = "output", .value = "one file",
               .words = s_publish, .count = 1u);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->script);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-c");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_VALUE,
               .a = "configuration", .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_when, ExecutablesStepsCtx, .catalog = catalog, .key = "output", .value = "one file",
               .words = one_file, .count = 6u);
    EMBED_CALL(cli_when, ExecutablesStepsCtx, .catalog = catalog, .key = "configuration", .value = "Release",
               .words = s_ci, .count = 1u);
    EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = catalog, .words = s_detailed, .count = 2u);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

// the steps of a Node bin's build: Node's own single executable, run as orior's own `sea`, bun build --compile, or
// deno compile, the one the job's `with` names
EMBED_INLINE void cli_npm_steps(const ExecutablesStepsCtx *args)
{
    CliCatalog *catalog = args->catalog;
    static const char *const s_debug[1] = {"--log-level=debug"};
    char out[CLI_PATH_MAX_LEN];
    VerbaLinea line = {out, sizeof(out), 0ULL};
    mmgr_line_put(&line, "build/exe/");
    mmgr_line_put(&line, args->exe->name);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = catalog,
               .program = CLI_PROGRAM_ORIOR, .text = NULL, .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_ONLY, .a = "with",
               .b = "node", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "sea");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->exe->detail);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = out);
    if (!args->exe->commonjs)
    {
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "module");
    }
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "bun", .program = "bun");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_ONLY, .a = "with",
               .b = "bun", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "build");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--compile");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->exe->detail);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--outfile");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = out);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = catalog, .tool = "deno", .program = "deno");
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_ONLY, .a = "with",
               .b = "deno", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "compile");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "-A");
    if (args->exe->commonjs)
    {
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--unstable-detect-cjs");
    }
    EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = catalog, .words = s_debug, .count = 1u);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = "--output");
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = out);
    EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = catalog, .text = args->exe->detail);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = catalog, .kind_u32 = CLI_ARG_SOURCE_DATE, .a = NULL,
               .b = NULL, .c = NULL, .d = NULL, .separated = 0);
}

void cli_executables_steps(const CliExecutablesRootCtx *args)
{
    static const char *const s_zig_verbose[1] = {"--verbose"};
    static const char *const s_pyinstaller_verbose[2] = {"--log-level", "DEBUG"};
    char script[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_script_get, ExecutablesStepsCtx, .root = args->root, .exe = args->exe, .out = script);
    EMBED_CALL(cli_executables_out_internal.folder_get, CliExecutablesOutCtx, .root = args->root,
               .script = args->exe->script, .out = folder);
    if (args->exe->kind == CLI_KIND_CARGO)
    {
        EMBED_CALL(cli_cargo_steps, ExecutablesStepsCtx, .root = args->root, .exe = args->exe, .folder = folder,
                   .catalog = args->catalog);
    }
    else if (args->exe->kind == CLI_KIND_CMAKE)
    {
        EMBED_CALL(cli_cmake_steps, ExecutablesStepsCtx, .root = args->root, .exe = args->exe, .folder = folder,
                   .catalog = args->catalog);
    }
    else if (args->exe->kind == CLI_KIND_MESON)
    {
        EMBED_CALL(cli_meson_steps, ExecutablesStepsCtx, .root = args->root, .exe = args->exe, .folder = folder,
                   .vsenv = cli_needs_vsenv(), .catalog = args->catalog);
    }
    else if (args->exe->kind == CLI_KIND_GO)
    {
        EMBED_CALL(cli_go_steps, ExecutablesStepsCtx, .root = args->root, .exe = args->exe, .folder = folder,
                   .catalog = args->catalog);
    }
    else if (args->exe->kind == CLI_KIND_DOTNET)
    {
        EMBED_CALL(cli_dotnet_steps, ExecutablesStepsCtx, .script = script, .catalog = args->catalog,
                   .runtime = cli_dotnet_runtime());
    }
    else if (args->exe->kind == CLI_KIND_ZIG)
    {
        EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = args->catalog, .tool = "zig", .program = "zig");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = "build");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = "--build-file");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = script);
        EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = args->catalog, .words = s_zig_verbose, .count = 1u);
        if (args->exe->optimize)
        {
            EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_AROUND,
                       .a = "-Doptimize=", .b = "optimize", .c = "", .d = NULL, .separated = 0);
        }
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_SOURCE_DATE,
                   .a = NULL, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
    else if (args->exe->kind == CLI_KIND_PYINSTALLER)
    {
        char dist[CLI_PATH_MAX_LEN];
        char work[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_under_folder, ExecutablesStepsCtx, .folder = folder, .inner = "dist", .out = dist);
        EMBED_CALL(cli_under_folder, ExecutablesStepsCtx, .folder = folder, .inner = "build", .out = work);
        EMBED_CALL(cli_tool, ExecutablesStepsCtx, .catalog = args->catalog, .tool = "python", .program = "python");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = "-m");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = "PyInstaller");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = "--noconfirm");
        EMBED_CALL(cli_verbose, ExecutablesStepsCtx, .catalog = args->catalog, .words = s_pyinstaller_verbose,
                   .count = 2u);
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = "--distpath");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = dist);
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = "--workpath");
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = work);
        EMBED_CALL(cli_lit, ExecutablesStepsCtx, .catalog = args->catalog, .text = script);
        EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_SOURCE_DATE,
                   .a = NULL, .b = NULL, .c = NULL, .d = NULL, .separated = 0);
    }
    else if (args->exe->kind == CLI_KIND_NPM)
    {
        EMBED_CALL(cli_npm_steps, ExecutablesStepsCtx, .exe = args->exe, .catalog = args->catalog);
    }
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = args->catalog,
               .program = CLI_PROGRAM_CREATED, .text = NULL, .tool_program = NULL, .system = 0);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_ONLY, .a = "then",
               .b = "run", .c = NULL, .d = NULL, .separated = 0);
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = args->catalog,
               .program = CLI_PROGRAM_CREATED, .text = NULL, .tool_program = NULL, .system = 1);
    EMBED_CALL(cli_catalog_kind.arg, CliCatalogKindCfg, .catalog = args->catalog, .kind_u32 = CLI_ARG_ONLY, .a = "then",
               .b = "run as the system runs it", .c = NULL, .d = NULL, .separated = 0);
}
