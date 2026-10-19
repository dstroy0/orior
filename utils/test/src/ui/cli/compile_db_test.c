// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compile_db_test.c: the compilation database's check: a CMake reply gives each source the build's compiler, includes
// and defines; a tree's own database is kept and read for its compilers; and a command line's words keep quoted ones
// whole
#include "../../../../../ui/cli/src/builds/compile_db_internal.h"
#include "cli_check.h"

static char s_text[1u << 16u];
static CliJsonNode s_node[1u << 12u];
static char s_tree_text[1u << 16u];
static CfgJsonToken s_token[1u << 12u];
static CliWords s_words;

// `text` written to the file `name` of the folder `dir`
static void compile_file(const char *dir, const char *name, const char *text, unsigned long long size)
{
    char path[CLI_PATH_LONGEST];
    cli_full(dir, name, path);
    (void)query_file_written(path, text, size);
}

// a path with each `\` a `/` into `out`, which holds CLI_PATH_LONGEST
static void compile_forward(const char *path, char *out)
{
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = out; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
}

// whether the words clangd starts with hold `word`
static int compile_holds(const char *word)
{
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[at], .other = word, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the entry of `name` in the folder `source` as the database should write it, built in `built` by the words
// `flags`, `count` of them
static void compile_entry(CliLine *line, const char *built, const char *source, const char *name,
                          const char *const *flags, unsigned int count)
{
    char file[CLI_PATH_LONGEST];
    CliLine named = {file, sizeof(file), 0ull};
    cli_line_text(&named, source);
    cli_line_text(&named, "/src/");
    cli_line_text(&named, name);
    (void)cli_line_closed(&named);
    cli_line_text(line, "{\"directory\":");
    cli_json_quoted(line, built, cellul.len(&(CatenaFinitaCfg){.src = built, .cap = CLI_CELLS_POOL}));
    cli_line_text(line, ",\"file\":");
    cli_json_quoted(line, file, named.at);
    cli_line_text(line, ",\"arguments\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_json_quoted(line, flags[at], cellul.len(&(CatenaFinitaCfg){.src = flags[at], .cap = CLI_CELLS_POOL}));
        cli_line_character(line, ',');
    }
    cli_json_quoted(line, file, named.at);
    cli_line_text(line, "]}");
}

// a CMake reply gives each source the build's compiler, includes and defines
static void compile_cmake(void)
{
    static const char s_index[] = "{\"reply\": {\"codemodel-v2\": {\"jsonFile\": \"codemodel-v2-a.json\"}, "
                                  "\"toolchains-v1\": {\"jsonFile\": \"toolchains-v1-a.json\"}}}";
    static const char s_toolchains[] =
        "{\"toolchains\": [{\"language\": \"C\", \"compiler\": {\"path\": \"/usr/bin/gcc\", \"id\": \"GNU\"}}, "
        "{\"language\": \"CXX\", \"compiler\": {\"path\": \"C:/VS/bin/cl.exe\", \"id\": \"MSVC\"}}]}";
    static const char s_target[] =
        "{\"sources\": [{\"path\": \"src/main.c\"}, {\"path\": \"src/more.cpp\"}, {\"path\": \"src/kernel.cu\"}], "
        "\"compileGroups\": [{\"language\": \"C\", \"compileCommandFragments\": [{\"fragment\": \"-g -std=gnu11\"}], "
        "\"includes\": [{\"path\": \"/x/include\"}, {\"path\": \"/x/sys\", \"isSystem\": true}], \"defines\": "
        "[{\"define\": \"NEEDED=1\"}], \"sourceIndexes\": [0]}, {\"language\": \"CXX\", \"compileCommandFragments\": "
        "[{\"fragment\": \"/W3\"}], \"includes\": [{\"path\": \"C:/x/include\"}], \"defines\": [{\"define\": "
        "\"WIN\"}], \"sourceIndexes\": [1]}, {\"language\": \"CUDA\", \"compileCommandFragments\": [], "
        "\"sourceIndexes\": [2]}]}";
    static const char *const s_c[7] = {"/usr/bin/gcc",   "-g",         "-std=gnu11", "-I/x/include",
                                       "-isystem/x/sys", "-DNEEDED=1", "-c"};
    static const char *const s_cxx[5] = {"C:/VS/bin/cl.exe", "/W3", "/IC:/x/include", "/DWIN", "/c"};
    char dir[CLI_PATH_LONGEST];
    char build[CLI_PATH_LONGEST];
    char reply[CLI_PATH_LONGEST];
    char source[CLI_PATH_LONGEST];
    char built[CLI_PATH_LONGEST];
    cli_check_folder("compile_db_cmake", dir);
    cli_full(dir, "build/cmake/Debug", build);
    cli_full(build, ".cmake/api/v1/reply", reply);
    (void)query_folder_made(reply);
    compile_forward(dir, source);
    compile_forward(build, built);
    compile_file(reply, "index-2026-01-01T00-00-00-0000.json", s_index, sizeof(s_index) - 1u);
    CliLine line = {s_text, sizeof(s_text), 0ull};
    cli_line_text(&line, "{\"paths\": {\"source\": ");
    cli_json_quoted(&line, source, cellul.len(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, ", \"build\": ");
    cli_json_quoted(&line, built, cellul.len(&(CatenaFinitaCfg){.src = built, .cap = CLI_CELLS_POOL}));
    cli_line_text(&line, "}, \"configurations\": [{\"name\": \"Release\", \"targets\": []}, {\"name\": \"Debug\", "
                         "\"targets\": [{\"name\": \"probe\", \"jsonFile\": \"target-probe.json\"}]}]}");
    compile_file(reply, "codemodel-v2-a.json", s_text, line.at);
    compile_file(reply, "toolchains-v1-a.json", s_toolchains, sizeof(s_toolchains) - 1u);
    compile_file(reply, "target-probe.json", s_target, sizeof(s_target) - 1u);
    static CliDatabase s_database;
    (void)cli_compile_db_refresh(dir, &s_database);
    line.at = 0ull;
    cli_line_character(&line, '[');
    compile_entry(&line, built, source, "main.c", s_c, 7u);
    cli_line_character(&line, ',');
    compile_entry(&line, built, source, "more.cpp", s_cxx, 5u);
    cli_line_character(&line, ']');
    CliJson wanted;
    cli_json_begun(&wanted, s_node, sizeof(s_node) / sizeof(s_node[0]), s_tree_text, sizeof(s_tree_text));
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int wanted_root =
        cli_json_read(&wanted, s_text, line.at, s_token, sizeof(s_token) / sizeof(s_token[0]), &said);
    char path[CLI_PATH_LONGEST];
    cli_full(dir, CLI_COMPILE_WRITTEN, path);
    static char s_written[1u << 16u];
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_written, sizeof(s_written));
    unsigned int written = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_written, (size == QRY_MEMBER_UNREAD) ? 0ull : size, &written);
    cli_check(cli_json_same(json, written, &wanted, wanted_root), "the Debug configuration's, CUDA's left out");
    char folder[CLI_PATH_LONGEST];
    cli_full(dir, "build/orior", folder);
    cli_check(s_database.has_folder &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_database.folder, .other = folder, .cap = CLI_CELLS_POOL})),
              "clangd is given the folder written");
    cli_check(s_database.msvc, "a database that names cl is MSVC's");
    s_words.count = 0u;
    s_words.used = 0u;
    cli_compile_db_words(&s_database, &s_words);
    cli_check(compile_holds("--query-driver=/usr/bin/gcc") && (s_words.count == 2u),
              "cl answers no question of its headers");
}

// a tree's own database is kept and read for its compilers
static void compile_own(void)
{
    static const char s_clangd[] = "CompileFlags:\n  CompilationDatabase: out  # the tree's own\n";
    static const char s_own[] = "[{\"directory\": \"/t\", \"file\": \"/t/a.c\", \"command\": "
                                "\"\\\"/opt/cross/bin/arm-none-eabi-gcc\\\" -c a.c\"}]";
    static CliInputs s_inputs;
    static CliDatabase s_database;
    char dir[CLI_PATH_LONGEST];
    char out[CLI_PATH_LONGEST];
    char own[CLI_PATH_LONGEST];
    cli_check_folder("compile_db_own", dir);
    cli_full(dir, "out", out);
    (void)query_folder_made(out);
    compile_file(dir, ".clangd", s_clangd, sizeof(s_clangd) - 1u);
    compile_file(out, "compile_commands.json", s_own, sizeof(s_own) - 1u);
    cli_compile_inputs(dir, &s_inputs);
    cli_full(out, "compile_commands.json", own);
    cli_check(s_inputs.own && (s_inputs.count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_inputs.file[0].path, .other = own, .cap = CLI_CELLS_POOL})),
              "the database the tree's .clangd names is its own");
    cli_compile_gather(dir, &s_inputs, &s_database);
    cli_check(
        !s_database.has_folder && (s_database.compilers == 1u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = s_database.compiler[0], .other = "/opt/cross/bin/arm-none-eabi-gcc", .cap = CLI_CELLS_POOL})) &&
            !s_database.msvc,
        "it is read for its compiler");
    s_words.count = 0u;
    s_words.used = 0u;
    cli_compile_db_words(&s_database, &s_words);
    cli_check((s_words.count == 1u) && compile_holds("--query-driver=/opt/cross/bin/arm-none-eabi-gcc"),
              "clangd may ask that compiler");
    char path[CLI_PATH_LONGEST];
    cli_full(dir, CLI_COMPILE_WRITTEN, path);
    cli_check(!query_is_file(path), "nothing written over the tree's own");
}

// a word of a command line put after the others, `|` between them
static void compile_word(void *context, const char *word, unsigned long long size)
{
    CliLine *const line = context;
    cli_line_text(line, (line->at > 0ull) ? "|" : "");
    cli_line_counted(line, word, size);
}

int main(void)
{
    compile_cmake();
    compile_own();
    static const char s_line[] = "\"C:/Program Files/LLVM/bin/clang.exe\" -I \"a b\" -c x.c";
    CliLine words = {s_text, sizeof(s_text), 0ull};
    cli_compile_words_each(s_line, sizeof(s_line) - 1u, compile_word, &words);
    cli_check_text(s_text, words.at, "C:/Program Files/LLVM/bin/clang.exe|-I|a b|-c|x.c",
                   "a command line's words keep quoted ones whole");
    return cli_checked();
}
