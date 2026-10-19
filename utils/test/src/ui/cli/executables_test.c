// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_test.c: the programs a tree's builds make: Cargo gives its named and found binaries; CMake its
// executables past imports, aliases and comments; Meson the executables written out; Zig the names of both forms of
// its calls; a spec its file by whether it collects a folder; a package its bins by name; Go its main packages by
// folder; .NET the program of an exe project, found in its lines; a tree a job for each program, titling the names it
// shares; and a build says every command it runs by default and its own words where quiet
#include "../../../../../ui/cli/src/command_line/catalog.h"
#include "../../../../../ui/cli/src/programs/executables_internal.h"
#include "cli_check.h"

static CliNames s_names;
static CliLines s_lines;
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

// a file of the tree `dir` written at `path`, its folder made
static void executables_put(const char *dir, const char *path, const char *text)
{
    char file[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_full(dir, path, file);
    if (cli_path_parent(file, folder))
    {
        (void)query_folder_made(folder);
    }
    (void)query_file_written(file, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the names s_names holds, joined by `|`, each `name=value` where `values`, checked against `wanted`
static void executables_names_are(int values, const char *wanted, const char *what)
{
    char out[1024];
    CliLine line = {out, sizeof(out), 0ull};
    for (unsigned int at = 0u; at < s_names.count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, s_names.name[at]);
        if (values)
        {
            cli_line_character(&line, '=');
            cli_line_text(&line, s_names.value[at]);
        }
    }
    cli_check_text(out, line.at, wanted, what);
}

// Cargo gives its named and found binaries
static void executables_cargo(void)
{
    char dir[CLI_PATH_LONGEST];
    char manifest[CLI_PATH_LONGEST];
    static const char s_manifest[] = "[package]\nname = \"tool\" # the package\n\n[[bin]]\nname = 'second'\npath = "
                                     "\"src/second.rs\"\n\n[dependencies]\nname = \"not this\"\n";
    cli_check_folder("executables_cargo", dir);
    executables_put(dir, "Cargo.toml", s_manifest);
    executables_put(dir, "src/main.rs", "fn main() {}\n");
    executables_put(dir, "src/bin/third.rs", "fn main() {}\n");
    executables_put(dir, "src/bin/fourth/main.rs", "fn main() {}\n");
    cli_full(dir, "Cargo.toml", manifest);
    s_names.count = 0u;
    cli_cargo_bins(manifest, s_manifest, &s_names);
    executables_names_are(0, "second|tool|fourth|third", "Cargo gives its named and found binaries");
    s_names.count = 0u;
    cli_cargo_bins(manifest, "[package]\nname = \"tool\"\nautobins = false\n", &s_names);
    cli_check(s_names.count == 0u, "none where autobins is off");
    cli_cargo_bins(manifest, "[workspace]\nmembers = [\"a\"]\n", &s_names);
    cli_check(s_names.count == 0u, "nor of a workspace");
}

// CMake, Meson and Zig give the executables written out
static void executables_scripts(void)
{
    static const char s_cmake[] = "project(demo C)\n# add_executable(commented main.c)\nadd_executable(${PROJECT_NAME} "
                                  "main.c)\nADD_EXECUTABLE ( \"tool\"\n  tool.c)\nadd_executable(other "
                                  "IMPORTED)\nadd_executable(short ALIAS tool)\nadd_executable(${NAME} "
                                  "x.c)\nadd_library(lib STATIC a.c)\n";
    char project[CLI_NAME_LONGEST];
    cli_check(cli_cmake_project(s_cmake, project) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = project, .other = "demo", .cap = CLI_CELLS_POOL})),
              "CMake's project");
    s_names.count = 0u;
    cli_cmake_targets(s_cmake, "demo", &s_names);
    executables_names_are(0, "demo|tool", "CMake's executables past imports, aliases and comments");
    static const char s_meson[] = "project('me', 'c')\n# executable('commented', 'x.c')\nhello = executable('hello', "
                                  "'main.c',\n  install : true)\nexecutable(name_var, 'y.c')\nshared_library('lib', "
                                  "'a.c')\n";
    cli_check(cli_meson_project(s_meson, project) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = project, .other = "me", .cap = CLI_CELLS_POOL})),
              "Meson's project");
    s_names.count = 0u;
    cli_meson_targets(s_meson, &s_names);
    executables_names_are(0, "hello", "Meson's executables written out");
    static const char s_zig[] = "const exe = b.addExecutable(.{\n    .name = \"zg\",\n    .root_module = "
                                "b.createModule(.{ .imports = &.{ .{ .name = \"other\", .module = mod } } }),\n});\n// "
                                "b.addExecutable(.{ .name = \"commented\" });\nconst old = b.addExecutable(\"older\", "
                                "\"src/main.zig\");\n";
    s_names.count = 0u;
    cli_zig_exes(s_zig, &s_names);
    executables_names_are(0, "zg|older", "Zig's names of both forms of its calls");
}

// a spec gives its file by whether it collects a folder, and a package its bins by name
static void executables_spec(void)
{
    char exe[CLI_NAME_LONGEST];
    char path[CLI_PATH_LONGEST];
    char file[CLI_NAME_LONGEST];
    char wanted[CLI_PATH_LONGEST];
    cli_program_file("app", file);
    CliLine line = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&line, "dist/");
    cli_line_text(&line, file);
    (void)cli_line_closed(&line);
    cli_check(cli_spec_made("a = Analysis(['app.py'])\npyz = PYZ(a.pure)\nexe = EXE(pyz, a.scripts, a.binaries, "
                            "name='app', console=True)\n",
                            exe, path) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = exe, .other = "app", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = wanted, .cap = CLI_CELLS_POOL})),
              "a spec of one file");
    line.at = 0ull;
    cli_line_text(&line, "dist/bundle/");
    cli_line_text(&line, file);
    (void)cli_line_closed(&line);
    cli_check(cli_spec_made("a = Analysis(['app.py'])\nexe = EXE(pyz, exclude_binaries=True, name=\"app\")\ncoll = "
                            "COLLECT(exe, a.binaries, name=\"bundle\")\n",
                            exe, path) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = exe, .other = "app", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = path, .other = wanted, .cap = CLI_CELLS_POOL})),
              "a spec that collects a folder");
    cli_check(!cli_spec_made("Name: rpm-spec\nVersion: 1\n", exe, path), "and no other spec");
    int module = 0;
    s_names.count = 0u;
    cli_npm_bins("{\"name\":\"@x/greet\",\"bin\":\"cli.js\"}", &s_names, &module);
    executables_names_are(1, "greet=cli.js", "a package's one bin by its name");
    s_names.count = 0u;
    cli_npm_bins("{\"name\":\"tools\",\"bin\":{\"a\":\"a.js\",\"b\":\"bin/b.mjs\"}}", &s_names, &module);
    executables_names_are(1, "a=a.js|b=bin/b.mjs", "its bins by name");
    s_names.count = 0u;
    cli_npm_bins("{\"name\":\"lib\"}", &s_names, &module);
    cli_check(s_names.count == 0u, "and none where it has none");
}

// Go gives the main packages by folder
static void executables_go(void)
{
    char dir[CLI_PATH_LONGEST];
    char manifest[CLI_PATH_LONGEST];
    static const char s_module[] = "module example.com/top/thing\n\ngo 1.22\n";
    cli_check_folder("executables_go", dir);
    executables_put(dir, "go.mod", s_module);
    executables_put(dir, "main.go", "// Command thing.\n//go:build linux || windows\n\npackage main\n");
    executables_put(dir, "cmd/serve/serve.go", "/* the server\n */\npackage main\n");
    executables_put(dir, "cmd/serve/serve_test.go", "package main_test\n");
    executables_put(dir, "lib/lib.go", "package lib\n");
    executables_put(dir, "inner/go.mod", "module example.com/inner\n");
    executables_put(dir, "inner/main.go", "package main\n");
    cli_full(dir, "go.mod", manifest);
    s_names.count = 0u;
    cli_go_mains(manifest, s_module, &s_names);
    executables_names_are(1, "serve=./cmd/serve|thing=.", "Go gives the main packages by folder");
}

// .NET gives the program of an exe project and finds it in its lines
static void executables_dotnet(void)
{
    static const char s_project[] = "app/Hello.csproj";
    char out[CLI_PATH_LONGEST];
    cli_check(cli_dotnet_program(
                  s_project, "<Project><PropertyGroup><OutputType>Exe</OutputType></PropertyGroup></Project>", out) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "Hello", .cap = CLI_CELLS_POOL})),
              "an exe project's program by its name");
    cli_check(cli_dotnet_program(s_project, "<OutputType>WinExe</OutputType><AssemblyName>hi</AssemblyName>", out) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "hi", .cap = CLI_CELLS_POOL})),
              "or the assembly's");
    cli_check(cli_dotnet_program(s_project, "<OutputType>Exe</OutputType><AssemblyName>$(Name)x</AssemblyName>", out) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "Hello", .cap = CLI_CELLS_POOL})),
              "an assembly name made of properties passed over");
    cli_check(!cli_dotnet_program(s_project, "<OutputType>Library</OutputType>", out), "and none of a library");
    char file[CLI_NAME_LONGEST];
    char wanted[CLI_PATH_LONGEST];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_program_file("Hello", file);
    static const char s_built[] = "  Hello -> C:/t/bin/Debug/net10.0/Hello.dll";
    cli_lines_put(&s_lines, s_built, sizeof(s_built) - 1u);
    CliLine line = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&line, "C:/t/bin/Debug/net10.0/");
    cli_line_text(&line, file);
    (void)cli_line_closed(&line);
    cli_check(cli_dotnet_artifact("Hello", &s_lines, out, &said) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = wanted, .cap = CLI_CELLS_POOL})),
              "the program beside the assembly built");
    static const char s_published[] = "  Hello -> C:/t/bin/Release/net10.0/win-x64/publish/";
    cli_lines_put(&s_lines, s_published, sizeof(s_published) - 1u);
    line.at = 0ull;
    cli_line_text(&line, "C:/t/bin/Release/net10.0/win-x64/publish/");
    cli_line_text(&line, file);
    (void)cli_line_closed(&line);
    cli_check(cli_dotnet_artifact("Hello", &s_lines, out, &said) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = wanted, .cap = CLI_CELLS_POOL})),
              "and in the folder published to, the last line first");
}

// the catalog made empty and the tree's programs' jobs put on it
static void executables_jobs(const char *dir)
{
    cli_catalog_begun(&s_catalog, s_jobs, 256u, s_params, 1024u, s_steps, 256u, s_args, 2048u, s_lists, 4096u, s_text,
                      sizeof(s_text));
    cli_executables_jobs(dir, &s_catalog);
}

// a line a step shows kept
static void executables_line(void *context, const char *shown)
{
    (void)context;
    if (s_count < 8u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_shown[s_count],
                                .src = shown,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL}) + 1u});
        s_count += 1u;
    }
}

// the lines the job `id` shows with s_values: 1, or 0 where it shows none
static int executables_shown(const char *dir, const char *id)
{
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    s_count = 0u;
    const unsigned int job = cli_catalog_find(&s_catalog, id);
    return (job != CLI_CATALOG_NONE) &&
           cli_runner_shown(dir, &s_catalog, job, &s_values, executables_line, NULL, &said);
}

// a tree gives a job for each program and titles the names it shares
static void executables_tree(void)
{
    char dir[CLI_PATH_LONGEST];
    cli_check_folder("executables_tree", dir);
    executables_put(dir, "a/Cargo.toml", "[package]\nname = \"same\"\n");
    executables_put(dir, "a/src/main.rs", "fn main() {}\n");
    executables_put(dir, "b/Cargo.toml", "[package]\nname = \"same\"\n");
    executables_put(dir, "b/src/main.rs", "fn main() {}\n");
    executables_put(dir, "c/CMakeLists.txt", "project(c)\nadd_subdirectory(tools)\n");
    executables_put(dir, "c/tools/CMakeLists.txt", "project(tools)\nadd_executable(cli cli.c)\n");
    executables_put(dir, "target/x/Cargo.toml", "[package]\nname = \"built\"\n");
    executables_put(dir, "target/x/src/main.rs", "fn main() {}\n");
    executables_jobs(dir);
    char ids[1024];
    CliLine line = {ids, sizeof(ids), 0ull};
    for (unsigned int at = 0u; at < s_catalog.jobs; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_catalog_at(&s_catalog, s_catalog.job[at].id));
        cli_line_character(&line, ' ');
        cli_line_text(&line, cli_catalog_at(&s_catalog, s_catalog.job[at].title));
    }
    cli_check_text(ids, line.at,
                   "executable/a/Cargo.toml:same a/same|executable/b/Cargo.toml:same b/same|executable/c/"
                   "CMakeLists.txt:cli cli",
                   "a job for each program, the names it shares titled by their folders");
    cli_values_begun(&s_values);
    cli_values_put(&s_values, "config", "Release");
    if (executables_shown(dir, "executable/c/CMakeLists.txt:cli") && (s_count >= 5u))
    {
        cli_check(cellul.eq(&(CatenaFinitaCfg){
                      .src = s_shown[2],
                      .other = "cmake -E touch build/cmake/c/Release/.cmake/api/v1/query/toolchains-v1",
                      .cap = CLI_CELLS_POOL}),
                  "the compilers asked of CMake");
        cli_check(
            cellul.eq(&(CatenaFinitaCfg){.src = s_shown[3],
                                         .other = "cmake -S c -B build/cmake/c/Release -DCMAKE_BUILD_TYPE=Release",
                                         .cap = CLI_CELLS_POOL}),
            "CMake configured");
        cli_check(
            cellul.eq(&(CatenaFinitaCfg){.src = s_shown[4],
                                         .other = "cmake --build build/cmake/c/Release --target cli --config Release",
                                         .cap = CLI_CELLS_POOL}),
            "and built");
    }
}

// the values of a build's trace and the one more value given
static void executables_given(const char *trace, const char *key, const char *value)
{
    cli_values_begun(&s_values);
    cli_values_put(&s_values, CLI_TRACE, trace);
    if (key != NULL)
    {
        cli_values_put(&s_values, key, value);
    }
}

// a build tool says every command it runs by default and its own words where quiet
static void executables_trace(void)
{
    char dir[CLI_PATH_LONGEST];
    cli_check_folder("executables_trace", dir);
    executables_put(dir, "Cargo.toml", "[package]\nname = \"tool\"\n");
    executables_put(dir, "src/main.rs", "fn main() {}\n");
    executables_put(dir, "c/CMakeLists.txt", "project(c)\nadd_executable(cli cli.c)\n");
    executables_put(dir, "g/go.mod", "module example.com/g\n");
    executables_put(dir, "g/main.go", "package main\n\nfunc main() {}\n");
    executables_jobs(dir);
    int verbose = s_catalog.jobs > 0u;
    for (unsigned int at = 0u; at < s_catalog.jobs; at += 1u)
    {
        const CliJob *const job = &s_catalog.job[at];
        int traced = 0;
        for (unsigned int param = job->param_first; param < (job->param_first + job->params); param += 1u)
        {
            traced = traced ||
                     ((cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, s_catalog.param[param].key),
                                                    .other = CLI_TRACE,
                                                    .cap = CLI_CELLS_POOL})) &&
                      (cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, s_catalog.param[param].fallback),
                                                    .other = CLI_TRACE_VERBOSE,
                                                    .cap = CLI_CELLS_POOL})));
        }
        verbose = verbose && traced;
    }
    cli_check(verbose, "each build's trace is verbose by default");
    executables_given(CLI_TRACE_VERBOSE, "profile", "dev");
    const int loud = executables_shown(dir, "executable/Cargo.toml:tool") && (s_count > 0u);
    char first[1024];
    memor.cpy(&(MemoriaCfg){.dst = first,
                            .src = s_shown[0],
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_shown[0], .cap = CLI_CELLS_POOL}) + 1u});
    executables_given(CLI_TRACE_QUIET, "profile", "dev");
    if (loud && executables_shown(dir, "executable/Cargo.toml:tool") && (s_count > 0u))
    {
        cli_check(
            (cellul.find(&(CatenaFinitaCfg){
                 .src = first, .cap = CLI_CELLS_POOL, .other = " -vv", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                (cellul.find(&(CatenaFinitaCfg){
                     .src = s_shown[0], .cap = CLI_CELLS_POOL, .other = " -vv", .other_cap = CLI_CELLS_POOL}) == NULL),
            "Cargo's -vv where verbose");
    }
    executables_given(CLI_TRACE_VERBOSE, "config", "Release");
    if (executables_shown(dir, "executable/c/CMakeLists.txt:cli") && (s_count >= 5u))
    {
        cli_check(
            cellul.eq(&(CatenaFinitaCfg){.src = s_shown[3],
                                         .other = "cmake -S c -B build/cmake/c/Release -DCMAKE_BUILD_TYPE=Release "
                                                  "--log-level=VERBOSE",
                                         .cap = CLI_CELLS_POOL}),
            "CMake configures verbose");
        cli_check(cellul.eq(&(CatenaFinitaCfg){
                      .src = s_shown[4],
                      .other = "cmake --build build/cmake/c/Release --target cli --config Release --verbose",
                      .cap = CLI_CELLS_POOL}),
                  "and builds verbose");
    }
    executables_given(CLI_TRACE_QUIET, "config", "Release");
    if (executables_shown(dir, "executable/c/CMakeLists.txt:cli") && (s_count >= 5u))
    {
        cli_check(
            cellul.eq(&(CatenaFinitaCfg){.src = s_shown[4],
                                         .other = "cmake --build build/cmake/c/Release --target cli --config Release",
                                         .cap = CLI_CELLS_POOL}),
            "and quiet where asked");
    }
    executables_given(CLI_TRACE_VERBOSE, NULL, NULL);
    if (executables_shown(dir, "executable/g/go.mod:g") && (s_count > 0u))
    {
        cli_check(cellul.find(&(CatenaFinitaCfg){.src = s_shown[0],
                                                 .cap = CLI_CELLS_POOL,
                                                 .other = " -trimpath -v -x -o ",
                                                 .other_cap = CLI_CELLS_POOL}) != NULL,
                  "Go's -v -x where verbose");
    }
}

int main(void)
{
    // the check's folders sit inside the tree's repository, whose commit would date every step; a date already given
    // is kept, so the steps are shown as they are outside any repository
    (void)query_environment_set("SOURCE_DATE_EPOCH", "0");
    executables_cargo();
    executables_scripts();
    executables_spec();
    executables_go();
    executables_dotnet();
    executables_tree();
    executables_trace();
    return cli_checked();
}
