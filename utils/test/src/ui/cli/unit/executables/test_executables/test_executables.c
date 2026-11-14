// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_executables.c: the programs a tree's builds make: Cargo's named and found binaries, CMake's executables past imports, aliases and comments,
// Meson's written out, Zig's of both forms of its calls, a spec's file by whether it collects a folder, a package's bins by name, Go's main packages by
// folder, .NET's program of an exe project, a tree's job for each program, and a build printing every command it runs by default
#include "ui/cli/src/command_line/catalog.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/programs/executables_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_cargo_manifest[] = "[package]\nname = \"tool\" # the package\n\n[[bin]]\nname = 'second'\npath = "
                                       "\"src/second.rs\"\n\n[dependencies]\nname = \"not this\"\n";
static const char s_cmake_lists[] = "project(demo C)\n# add_executable(commented main.c)\nadd_executable(${PROJECT_NAME} "
                                    "main.c)\nADD_EXECUTABLE ( \"tool\"\n  tool.c)\nadd_executable(other "
                                    "IMPORTED)\nadd_executable(short ALIAS tool)\nadd_executable(${NAME} "
                                    "x.c)\nadd_library(lib STATIC a.c)\n";
static const char s_meson_build[] = "project('me', 'c')\n# executable('commented', 'x.c')\nhello = executable('hello', "
                                    "'main.c',\n  install : true)\nexecutable(name_var, 'y.c')\nshared_library('lib', "
                                    "'a.c')\n";
static const char s_dotnet_project[] = "app/Hello.csproj";

static CliNames s_names;
// the lines' rings over bytes of the suite's own, as the runner lays its rings over its span of the rings block
static CliRunLines s_lines;
static embed_u8 s_lines_text[CLI_LINES_TEXT];
static CliRunnerLine s_lines_records[CLI_LINES_MAX];
static CliJob s_jobs[256];
static CliParam s_params[1024];
static CliStep s_steps[256];
static CliArg s_args[2048];
static unsigned int s_lists[4096];
static char s_text[1u << 18u];
static CliCatalog s_catalog;
static CliValues s_values;
static char s_shown[8][1024];
static unsigned int s_count;
static char s_dir[CLI_PATH_MAX_LEN];

// a file of the tree `dir` written at `path`, its folder made
static void file_written(const char *dir, const char *path, const char *text)
{
    char file[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = path, .joined = file);
    if (EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = file, .out = folder))
    {
        (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    }
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = text, .size = CLI_UNITY_LENGTH(text)), path);
}

// the names s_names contains, joined by `|`, each `name=value` where `values`, asserted against `required`
static void assert_names(int values, const char *required)
{
    char out[1024];
    VerbaLinea line = {out, sizeof(out), 0ull};
    for (unsigned int at = 0u; at < s_names.count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, s_names.name[at]);
        if (values)
        {
            mmgr_line_ch(&line, '=');
            mmgr_line_put(&line, s_names.value[at]);
        }
    }
    CLI_TEST_ASSERT_TEXT(required, out, line.at);
}

// a tree of a Cargo package with four binaries; its manifest's path in `manifest`
static void cargo_tree_written(char *manifest)
{
    char dir[CLI_PATH_MAX_LEN];
    cli_unity_folder("executables_cargo", dir);
    file_written(dir, "Cargo.toml", s_cargo_manifest);
    file_written(dir, "src/main.rs", "fn main() {}\n");
    file_written(dir, "src/bin/third.rs", "fn main() {}\n");
    file_written(dir, "src/bin/fourth/main.rs", "fn main() {}\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "Cargo.toml", .joined = manifest);
    s_names.count = 0u;
}

// `path` joined under `folder`, in a static text, with the program's file name for this platform
static const char *program_in(const char *folder, const char *program)
{
    static char s_requested[CLI_PATH_MAX_LEN];
    char file[CLI_NAME_MAX_LEN];
    VerbaLinea line = {s_requested, sizeof(s_requested), 0ull};
    EMBED_CALL(cli_executables_name_out_folder_internal.program_file, CliExecutablesNameOutFolderCtx, .name = program, .out = file);
    mmgr_line_put(&line, folder);
    mmgr_line_put(&line, file);
    (void)mmgr_line_finish(&line);
    return s_requested;
}

// the catalog made empty and the tree's programs' jobs added to it
static void jobs_listing(const char *dir)
{
    EMBED_CALL(cli_catalog_jobs_max.begin, CliCatalogJobsMaxCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_max = 256u, .params = s_params, .params_max = 1024u, .steps = s_steps, .steps_max = 256u, .request = s_args, .args_max = 2048u, .lists = s_lists, .lists_max = 4096u, .text_out = s_text, .text_max = sizeof(s_text));
    EMBED_CALL(cli_catalog_root.executables_jobs, CliCatalogRootCfg, .root = dir, .catalog = &s_catalog);
}

// a line a step shows recorded
static void line_recorded(const CliEachDisplayedCfg *args)
{
    if (s_count < 8u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_shown[s_count], .src = args->displayed, .bytes = CLI_UNITY_LENGTH(args->displayed) + 1u});
        s_count += 1u;
    }
}

// the lines the job `id` shows with s_values: 1, or 0 where it shows none
static int job_displayed(const char *dir, const char *id)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    s_count = 0u;
    const unsigned int job = EMBED_CALL(cli_catalog_id.find, CliCatalogIdCfg, .catalog_read = &s_catalog, .id = id);
    return (job != CLI_CATALOG_NONE) && EMBED_CALL(cli_runner_err_msg_root.show, CliRunnerErrMsgRootCfg, .root = dir, .catalog = &s_catalog, .job = job, .values = &s_values, .each = line_recorded, .context = NULL, .err_msg = &err_msg);
}

// the lines of `id` shown, the case ignored where its tool is not on this machine
static void displayed_or_ignored(const char *id, unsigned int min)
{
    if (!job_displayed(s_dir, id) || (s_count < min))
    {
        TEST_IGNORE_MESSAGE("the build's tool is not on this machine");
    }
}

// a tree of three projects, two of one name, and a built folder
static void projects_written(void)
{
    cli_unity_folder("executables_tree", s_dir);
    file_written(s_dir, "a/Cargo.toml", "[package]\nname = \"same\"\n");
    file_written(s_dir, "a/src/main.rs", "fn main() {}\n");
    file_written(s_dir, "b/Cargo.toml", "[package]\nname = \"same\"\n");
    file_written(s_dir, "b/src/main.rs", "fn main() {}\n");
    file_written(s_dir, "c/CMakeLists.txt", "project(c)\nadd_subdirectory(tools)\n");
    file_written(s_dir, "c/tools/CMakeLists.txt", "project(tools)\nadd_executable(cli cli.c)\n");
    file_written(s_dir, "target/x/Cargo.toml", "[package]\nname = \"built\"\n");
    file_written(s_dir, "target/x/src/main.rs", "fn main() {}\n");
    jobs_listing(s_dir);
}

// the values of a build's trace and the one more value sent
static void trace_values(const char *trace, const char *key, const char *value)
{
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = &s_values);
    EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = CLI_TRACE, .value = trace);
    if (key != NULL)
    {
        EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = key, .value = value);
    }
}

// a tree of a Cargo, a CMake and a Go project
static void traced_tree_written(void)
{
    cli_unity_folder("executables_trace", s_dir);
    file_written(s_dir, "Cargo.toml", "[package]\nname = \"tool\"\n");
    file_written(s_dir, "src/main.rs", "fn main() {}\n");
    file_written(s_dir, "c/CMakeLists.txt", "project(c)\nadd_executable(cli cli.c)\n");
    file_written(s_dir, "g/go.mod", "module example.com/g\n");
    file_written(s_dir, "g/main.go", "package main\n\nfunc main() {}\n");
    jobs_listing(s_dir);
}

void setUp(void)
{
    s_lines.text_bytes = s_lines_text;
    s_lines.line_bytes = s_lines_records;
    cli_unity_begun();
    // the suite's folders are inside the tree's repository, whose commit would date every step; a date already sent is retained: the steps are shown
    // as they are outside any repository
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "SOURCE_DATE_EPOCH", .value = "0");
    s_names.count = 0u;
}

void tearDown(void)
{
}

void test_cargo_lists_its_named_and_found_binaries(void)
{
    char manifest[CLI_PATH_MAX_LEN];
    cargo_tree_written(manifest);
    EMBED_CALL(cli_executables_names_internal.cargo_bins, CliExecutablesNamesCtx, .manifest = manifest, .text = s_cargo_manifest, .names = &s_names);
    assert_names(0, "second|tool|fourth|third");
}

void test_cargo_lists_no_binaries_where_autobins_is_off(void)
{
    char manifest[CLI_PATH_MAX_LEN];
    cargo_tree_written(manifest);
    EMBED_CALL(cli_executables_names_internal.cargo_bins, CliExecutablesNamesCtx, .manifest = manifest, .text = "[package]\nname = \"tool\"\nautobins = false\n", .names = &s_names);
    TEST_ASSERT_EQUAL_UINT(0u, s_names.count);
}

void test_cargo_lists_no_binaries_of_a_workspace(void)
{
    char manifest[CLI_PATH_MAX_LEN];
    cargo_tree_written(manifest);
    EMBED_CALL(cli_executables_names_internal.cargo_bins, CliExecutablesNamesCtx, .manifest = manifest, .text = "[workspace]\nmembers = [\"a\"]\n", .names = &s_names);
    TEST_ASSERT_EQUAL_UINT(0u, s_names.count);
}

void test_cmakes_project_is_read(void)
{
    char project[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_text_out_internal.cmake_project, CliExecutablesTextOutCtx, .text = s_cmake_lists, .out = project));
    TEST_ASSERT_EQUAL_STRING("demo", project);
}

void test_cmakes_executables_skip_imports_aliases_and_comments(void)
{
    EMBED_CALL(cli_executables_names_text_internal.cmake_targets, CliExecutablesNamesTextCtx, .text = s_cmake_lists, .project = "demo", .names = &s_names);
    assert_names(0, "demo|tool");
}

void test_mesons_project_is_read(void)
{
    char project[CLI_NAME_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_text_out_internal.meson_project, CliExecutablesTextOutCtx, .text = s_meson_build, .out = project));
    TEST_ASSERT_EQUAL_STRING("me", project);
}

void test_mesons_executables_are_those_written_out(void)
{
    EMBED_CALL(cli_executables_names_internal.meson_targets, CliExecutablesNamesCtx, .text = s_meson_build, .names = &s_names);
    assert_names(0, "hello");
}

void test_zigs_names_are_read_from_both_forms_of_its_calls(void)
{
    static const char s_zig[] = "const exe = b.addExecutable(.{\n    .name = \"zg\",\n    .root_module = "
                                "b.createModule(.{ .imports = &.{ .{ .name = \"other\", .module = mod } } }),\n});\n// "
                                "b.addExecutable(.{ .name = \"commented\" });\nconst old = b.addExecutable(\"older\", "
                                "\"src/main.zig\");\n";
    EMBED_CALL(cli_executables_names_internal.zig_exes, CliExecutablesNamesCtx, .text = s_zig, .names = &s_names);
    assert_names(0, "zg|older");
}

void test_a_spec_of_one_file_names_its_program(void)
{
    char exe[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_text_exe_internal.spec_init, CliExecutablesTextExeCtx, .text = "a = Analysis(['app.py'])\npyz = PYZ(a.pure)\nexe = EXE(pyz, a.scripts, a.binaries, "
                                   "name='app', console=True)\n", .exe_capacity = exe, .path_out = path));
    TEST_ASSERT_EQUAL_STRING("app", exe);
    TEST_ASSERT_EQUAL_STRING(program_in("dist/", "app"), path);
}

void test_a_spec_that_collects_a_folder_names_its_program_in_the_folder(void)
{
    char exe[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_text_exe_internal.spec_init, CliExecutablesTextExeCtx, .text = "a = Analysis(['app.py'])\nexe = EXE(pyz, exclude_binaries=True, name=\"app\")\ncoll = "
                                   "COLLECT(exe, a.binaries, name=\"bundle\")\n", .exe_capacity = exe, .path_out = path));
    TEST_ASSERT_EQUAL_STRING("app", exe);
    TEST_ASSERT_EQUAL_STRING(program_in("dist/bundle/", "app"), path);
}

void test_another_type_of_spec_names_no_program(void)
{
    char exe[CLI_NAME_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    TEST_ASSERT_FALSE(EMBED_CALL(cli_executables_text_exe_internal.spec_init, CliExecutablesTextExeCtx, .text = "Name: rpm-spec\nVersion: 1\n", .exe_capacity = exe, .path_out = path));
}

void test_a_package_names_its_one_bin_by_its_name(void)
{
    int module = 0;
    EMBED_CALL(cli_executables_names_text_module_internal.npm_bins, CliExecutablesNamesTextModuleCtx, .text = "{\"name\":\"@x/greet\",\"bin\":\"cli.js\"}", .names = &s_names, .module = &module);
    assert_names(1, "greet=cli.js");
}

void test_a_package_names_its_bins(void)
{
    int module = 0;
    EMBED_CALL(cli_executables_names_text_module_internal.npm_bins, CliExecutablesNamesTextModuleCtx, .text = "{\"name\":\"tools\",\"bin\":{\"a\":\"a.js\",\"b\":\"bin/b.mjs\"}}", .names = &s_names, .module = &module);
    assert_names(1, "a=a.js|b=bin/b.mjs");
}

void test_a_package_with_no_bins_names_none(void)
{
    int module = 0;
    EMBED_CALL(cli_executables_names_text_module_internal.npm_bins, CliExecutablesNamesTextModuleCtx, .text = "{\"name\":\"lib\"}", .names = &s_names, .module = &module);
    TEST_ASSERT_EQUAL_UINT(0u, s_names.count);
}

void test_each_script_is_read_in_cells_released_after_it(void)
{
    int module = 0;
    EMBED_CALL(cli_executables_names_text_module_internal.npm_bins, CliExecutablesNamesTextModuleCtx, .text = "{\"name\":\"lib\"}", .names = &s_names, .module = &module);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_executables_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_executables_owner));
}

void test_go_lists_its_main_packages_by_folder(void)
{
    static const char s_module[] = "module example.com/top/thing\n\ngo 1.22\n";
    char dir[CLI_PATH_MAX_LEN];
    char manifest[CLI_PATH_MAX_LEN];
    cli_unity_folder("executables_go", dir);
    file_written(dir, "go.mod", s_module);
    file_written(dir, "main.go", "// Command thing.\n//go:build linux || windows\n\npackage main\n");
    file_written(dir, "cmd/serve/serve.go", "/* the server\n */\npackage main\n");
    file_written(dir, "cmd/serve/serve_test.go", "package main_test\n");
    file_written(dir, "lib/lib.go", "package lib\n");
    file_written(dir, "inner/go.mod", "module example.com/inner\n");
    file_written(dir, "inner/main.go", "package main\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "go.mod", .joined = manifest);
    EMBED_CALL(cli_executables_text_module_internal.go_mains, CliExecutablesTextModuleCtx, .module_text = manifest, .text = s_module, .mains = &s_names);
    assert_names(1, "serve=./cmd/serve|thing=.");
}

void test_a_dotnet_exe_projects_program_has_its_name(void)
{
    char out[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_text_out_internal.dotnet_program, CliExecutablesTextOutCtx, .project = s_dotnet_project, .text = "<Project><PropertyGroup><OutputType>Exe</OutputType></PropertyGroup></Project>", .out = out));
    TEST_ASSERT_EQUAL_STRING("Hello", out);
}

void test_a_dotnet_exe_projects_program_has_the_assemblys_name(void)
{
    char out[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_text_out_internal.dotnet_program, CliExecutablesTextOutCtx, .project = s_dotnet_project, .text = "<OutputType>WinExe</OutputType><AssemblyName>hi</AssemblyName>", .out = out));
    TEST_ASSERT_EQUAL_STRING("hi", out);
}

void test_an_assembly_name_of_properties_is_ignored(void)
{
    char out[CLI_PATH_MAX_LEN];
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_text_out_internal.dotnet_program, CliExecutablesTextOutCtx, .project = s_dotnet_project, .text = "<OutputType>Exe</OutputType><AssemblyName>$(Name)x</AssemblyName>", .out = out));
    TEST_ASSERT_EQUAL_STRING("Hello", out);
}

void test_a_dotnet_library_has_no_program(void)
{
    char out[CLI_PATH_MAX_LEN];
    TEST_ASSERT_FALSE(EMBED_CALL(cli_executables_text_out_internal.dotnet_program, CliExecutablesTextOutCtx, .project = s_dotnet_project, .text = "<OutputType>Library</OutputType>", .out = out));
}

void test_a_dotnet_program_is_adjacent_the_assembly_built(void)
{
    static const char s_built[] = "  Hello -> C:/t/bin/Debug/net10.0/Hello.dll";
    char out[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(cli_runner_lines.lines_append, CliRunnerLinesCfg, .lines = &s_lines, .text = s_built, .length = sizeof(s_built) - 1u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_name_out_err_msg_internal.dotnet_artifact, CliExecutablesNameOutErrMsgCtx, .name = "Hello", .lines = &s_lines, .out = out, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING(program_in("C:/t/bin/Debug/net10.0/", "Hello"), out);
}

void test_a_dotnet_program_is_in_the_folder_published_to_the_last_line_first(void)
{
    static const char s_published[] = "  Hello -> C:/t/bin/Release/net10.0/win-x64/publish/";
    char out[CLI_PATH_MAX_LEN];
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(cli_runner_lines.lines_append, CliRunnerLinesCfg, .lines = &s_lines, .text = s_published, .length = sizeof(s_published) - 1u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_executables_name_out_err_msg_internal.dotnet_artifact, CliExecutablesNameOutErrMsgCtx, .name = "Hello", .lines = &s_lines, .out = out, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_STRING(program_in("C:/t/bin/Release/net10.0/win-x64/publish/", "Hello"), out);
}

void test_a_tree_has_a_job_for_each_program_its_shared_names_titled_by_folder(void)
{
    char ids[1024];
    VerbaLinea line = {ids, sizeof(ids), 0ull};
    projects_written();
    for (unsigned int at = 0u; at < s_catalog.jobs; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.job[at].id));
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.job[at].title));
    }
    CLI_TEST_ASSERT_TEXT("executable/a/Cargo.toml:same a/same|executable/b/Cargo.toml:same b/same|executable/c/"
                         "CMakeLists.txt:cli cli",
                         ids, line.at);
}

void test_the_compilers_are_queried_from_cmake_before_it_configures_and_builds(void)
{
    projects_written();
    EMBED_CALL(cli_catalog_key_values.values_begin, CliCatalogKeyValuesCfg, .values_out = &s_values);
    EMBED_CALL(cli_catalog_key_values.values_append, CliCatalogKeyValuesCfg, .values_out = &s_values, .key = "config", .value = "Release");
    displayed_or_ignored("executable/c/CMakeLists.txt:cli", 5u);
    TEST_ASSERT_EQUAL_STRING("cmake -E touch build/cmake/c/Release/.cmake/api/v1/query/toolchains-v1", s_shown[2]);
    TEST_ASSERT_EQUAL_STRING("cmake -S c -B build/cmake/c/Release -DCMAKE_BUILD_TYPE=Release", s_shown[3]);
    TEST_ASSERT_EQUAL_STRING("cmake --build build/cmake/c/Release --target cli --config Release", s_shown[4]);
}

void test_each_builds_trace_is_verbose_by_default(void)
{
    traced_tree_written();
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_catalog.jobs);
    for (unsigned int at = 0u; at < s_catalog.jobs; at += 1u)
    {
        const CliJob *const job = &s_catalog.job[at];
        int traced = 0;
        for (unsigned int param = job->param_first; param < (job->param_first + job->params); param += 1u)
        {
            const int names_trace = cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.param[param].key), .other = CLI_TRACE, .cap = CLI_POOL_BYTES});
            const int verbose = cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.param[param].fallback), .other = CLI_TRACE_VERBOSE, .cap = CLI_POOL_BYTES});
            traced = traced || (names_trace && verbose);
        }
        TEST_ASSERT_TRUE(traced);
    }
}

void test_cargo_is_vv_where_verbose_and_not_where_quiet(void)
{
    char first[1024];
    traced_tree_written();
    trace_values(CLI_TRACE_VERBOSE, "profile", "dev");
    displayed_or_ignored("executable/Cargo.toml:tool", 1u);
    memor.cpy(&(MemoriaCfg){.dst = first, .src = s_shown[0], .bytes = CLI_UNITY_LENGTH(s_shown[0]) + 1u});
    trace_values(CLI_TRACE_QUIET, "profile", "dev");
    displayed_or_ignored("executable/Cargo.toml:tool", 1u);
    CLI_TEST_ASSERT_CONTAINS(" -vv", first);
    CLI_TEST_ASSERT_NOT_CONTAINS(" -vv", s_shown[0]);
}

void test_cmake_configures_and_builds_verbose(void)
{
    traced_tree_written();
    trace_values(CLI_TRACE_VERBOSE, "config", "Release");
    displayed_or_ignored("executable/c/CMakeLists.txt:cli", 5u);
    TEST_ASSERT_EQUAL_STRING("cmake -S c -B build/cmake/c/Release -DCMAKE_BUILD_TYPE=Release --log-level=VERBOSE", s_shown[3]);
    TEST_ASSERT_EQUAL_STRING("cmake --build build/cmake/c/Release --target cli --config Release --verbose", s_shown[4]);
}

void test_cmake_builds_quiet_where_requested(void)
{
    traced_tree_written();
    trace_values(CLI_TRACE_QUIET, "config", "Release");
    displayed_or_ignored("executable/c/CMakeLists.txt:cli", 5u);
    TEST_ASSERT_EQUAL_STRING("cmake --build build/cmake/c/Release --target cli --config Release", s_shown[4]);
}

void test_go_is_v_x_where_verbose(void)
{
    traced_tree_written();
    trace_values(CLI_TRACE_VERBOSE, NULL, NULL);
    displayed_or_ignored("executable/g/go.mod:g", 1u);
    CLI_TEST_ASSERT_CONTAINS(" -trimpath -v -x -o ", s_shown[0]);
}
