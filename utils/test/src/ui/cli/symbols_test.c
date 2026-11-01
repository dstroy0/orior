// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_test.c: the tree's symbols' check: Python's, Rust's, scripts', C's and the other languages' declarations are
// read; scores rank the name itself first; a first reading reports how far it has gone and a stop retains what it read;
// the tree is read and a file read again once it changes; a call's signature is read from the tree; and a name's
// definition in effect is found through includes, conditions and the compilation database's defines
#include "../../../../../ui/cli/src/code/symbols.h"
#include "cli_check.h"

static char s_out[1u << 20u];

// a declaration put after the others, `name kind line` joined by `|`
static void symbols_declaration_capture(void *context, const char *name, unsigned long long length, const char *kind,
                                        unsigned int line)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put_n(out, name, length);
    mmgr_line_ch(out, ' ');
    mmgr_line_put(out, kind);
    mmgr_line_ch(out, ' ');
    mmgr_line_decimal(out, line, 1u);
}

// the declarations of `text` read as the file `path`, checked against `wanted`
static void symbols_names(const char *path, const char *text, const char *wanted, const char *what)
{
    char out[1024];
    VerbaLinea line = {out, sizeof(out), 0ull};
    cli_symbols_declared(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                         symbols_declaration_capture, &line);
    cli_check_text(out, line.at, wanted, what);
}

// the score `query` gives `name`, -1 where it does not name it
static long long symbols_score(const char *query, const char *name)
{
    long long points = 0ll;
    return cli_symbols_score(query, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), &points)
               ? points
               : -1ll;
}

// the symbols answering `query`, at most `most`, read into the shared tree: the array's node, `reading` set where the
// tree was still being read
static unsigned int symbols_found(const char *query, unsigned int most, const CliJson **json, int *reading)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_symbols_find(query, most, &out);
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    *reading = cli_json_is(*json, cli_json_member(*json, found, "reading"), CFG_JSON_TRUE);
    return cli_json_member(*json, found, "symbols");
}

// a first reading reports how far it has gone and a stop retains what it read
static void symbols_stop(void)
{
    static char s_names[3000][16];
    static const char *s_paths[3000];
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_check_folder("symbols_stop", root);
    for (unsigned int at = 0u; at < 3000u; at += 1u)
    {
        VerbaLinea name = {s_names[at], sizeof(s_names[at]), 0ull};
        mmgr_line_ch(&name, 'f');
        mmgr_line_decimal(&name, at, 4u);
        mmgr_line_put(&name, ".py");
        (void)mmgr_line_finish(&name);
        s_paths[at] = s_names[at];
        char text[64];
        VerbaLinea line = {text, sizeof(text), 0ull};
        mmgr_line_put(&line, "def step_");
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, "():\n    pass\n");
        cli_full(root, s_names[at], path);
        (void)query_file_written(path, text, line.at);
    }
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 3000u);
    unsigned long long done = 0ull;
    unsigned long long total = 0ull;
    int seen = 0;
    for (unsigned int tries = 0u; !seen && (tries < 500u); tries += 1u)
    {
        seen = cli_symbols_progress(&done, &total) && (done > 0ull);
        query_paused(1u);
    }
    cli_check(seen && (total == 3000ull) && (done <= total), "the reading reports how far it has gone");
    cli_symbols_stop();
    for (unsigned int tries = 0u; cli_symbols_progress(&done, &total) && (tries < 500u); tries += 1u)
    {
        query_paused(2u);
    }
    cli_check(!cli_symbols_progress(&done, &total), "the reading ended");
    cli_check(!cli_symbols_unread(), "and is no longer the first reading");
    const CliJson *json = NULL;
    int reading = 0;
    const unsigned int found_node = symbols_found("step_", 3000u, &json, &reading);
    cli_check((json->node[found_node].count > 0u) && (json->node[found_node].count < 3000u),
              "what it read stays, and no further");
    cli_check(json->node[symbols_found("step_0", 1u, &json, &reading)].count == 1u, "Go to Symbol answers from it");
}

// the tree waited for until it is no longer being read
static void symbols_read_waited(void)
{
    const CliJson *json = NULL;
    int reading = 1;
    for (unsigned int tries = 0u; tries < 200u; tries += 1u)
    {
        (void)symbols_found("", 1u, &json, &reading);
        if (!reading)
        {
            return;
        }
        query_paused(10u);
    }
}

// the tree is read and a file read again once it changes
static void symbols_tree(void)
{
    static const char *const s_first[2] = {"a.py", "notes.txt"};
    static const char *const s_again[1] = {"a.py"};
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char text[512];
    cli_check_folder("symbols_tree", root);
    cli_full(root, "a.py", path);
    (void)query_file_written(path, "def first():\n    pass\n", 22ull);
    cli_symbols_forget();
    cli_symbols_refresh(root, s_first, 2u);
    symbols_read_waited();
    const CliJson *json = NULL;
    int reading = 0;
    unsigned int found = symbols_found("first", 10u, &json, &reading);
    VerbaLinea line = {text, sizeof(text), 0ull};
    cli_json_write(json, found, &line, 0u, 0);
    cli_check_text(
        text, line.at,
        "[{\"name\":\"first\",\"kind\":\"function\",\"path\":\"a.py\",\"line\":0,\"text\":\"def first():\"}]",
        "the tree is read");
    query_paused(20u);
    (void)query_file_written(path, "\ndef second():\n    pass\n", 24ull);
    // a tree is brought up to date again once four seconds have passed since the last
    query_paused(4100u);
    cli_symbols_refresh(root, s_again, 1u);
    symbols_read_waited();
    cli_check(json->node[symbols_found("first", 10u, &json, &reading)].count == 0u,
              "a file that changed is read again");
    found = symbols_found("second", 10u, &json, &reading);
    line.at = 0ull;
    cli_json_write(json, cli_json_member(json, cli_json_item(json, found, 0u), "line"), &line, 0u, 0);
    cli_check_text(text, line.at, "1", "with its new declarations");
}

// the signature of the call the cursor stands in at `line`, `col` of `text`, handed as the file `path`, checked
// against `wanted` whole, or against its arguments and first declaration where `part` is set
static void symbols_signature_open_text(const char *path, const char *text, unsigned int line, unsigned int col,
                                        const char *wanted, int part, const char *what)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int answered =
        cli_symbols_signature(path, "root", text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                              line, col, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    if (part)
    {
        cli_check(answered &&
                      (cellul.find(&(CatenaFinitaCfg){
                           .src = s_out, .cap = CLI_CELLS_POOL, .other = wanted, .other_cap = CLI_CELLS_POOL}) != NULL),
                  what);
        return;
    }
    if (!answered)
    {
        cli_check(0, what);
        return;
    }
    cli_check_text(s_out, out.at, wanted, what);
}

// a call's signature from the tree: its name's declarations in its language in order of fit, each argument's type
// from a parameter, a local, a literal and the tree and whether it mirrors its parameter's node, and a bracket in a
// string not counted
static void symbols_signature(void)
{
    static const char *const s_paths[4] = {"lib.c", "other.c", "lib.py", "served.c"};
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_check_folder("symbols_signature", root);
    static const char s_lib[] = "#define LIMIT 4\nint total(int first, const char *second);\nstatic int total(int "
                                "first, const char *second)\n{\n    return first;\n}\n";
    static const char s_other[] = "int total(int first);\n";
    static const char s_python[] = "def total(first: int, second=\"x\"):\n    pass\n";
    static const char s_served[] =
        "int add(int a, int b) { return a + b; }\n\nint main(void) {\n  return add(1, 2)\n}\n";
    cli_full(root, "lib.c", path);
    (void)query_file_written(path, s_lib, sizeof(s_lib) - 1u);
    cli_full(root, "other.c", path);
    (void)query_file_written(path, s_other, sizeof(s_other) - 1u);
    cli_full(root, "lib.py", path);
    (void)query_file_written(path, s_python, sizeof(s_python) - 1u);
    cli_full(root, "served.c", path);
    (void)query_file_written(path, s_served, sizeof(s_served) - 1u);
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 4u);
    symbols_read_waited();
    symbols_signature_open_text(
        "use.c", "static void run(unsigned int count)\n{\n    const char *name = \"x\";\n    total(count, name", 3u,
        21u,
        "{\"name\":\"total\",\"active\":1,\"arguments\":[{\"text\":\"count\",\"type\":\"unsigned int\"},{\"text\":"
        "\"name\",\"type\":\"char*\"}],\"declarations\":[{\"path\":\"lib.c\",\"line\":1,\"label\":\"int total(int "
        "first, const char *second)\",\"params\":[[10,19],[21,39]],\"types\":[\"int\",\"char*\"],\"count\":true,"
        "\"variadic\":false,\"differs\":[0]},{\"path\":\"lib.c\",\"line\":2,\"label\":\"static int total(int "
        "first, const char *second)\",\"params\":[[17,26],[28,46]],\"types\":[\"int\",\"char*\"],\"count\":true,"
        "\"variadic\":false,\"differs\":[0]},{\"path\":\"other.c\",\"line\":0,\"label\":\"int total(int first)\","
        "\"params\":[[10,19]],\"types\":[\"int\"],\"count\":false,\"variadic\":false,\"differs\":[0]}],"
        "\"reading\":false}",
        0, "a call's declarations in order of fit, a parameter's and a local's types open_text against each");
    symbols_signature_open_text(
        "use.c", "    total(LIMIT, \"(\" ", 0u, 21u,
        "\"arguments\":[{\"text\":\"LIMIT\",\"type\":\"number\"},{\"text\":\"\\\"(\\\"\",\"type\":"
        "\"string\"}],\"declarations\":[{\"path\":\"lib.c\",\"line\":1",
        1, "a bracket in a string is not counted, and a name's type is read from the tree");
    symbols_signature_open_text("use.c", "    total(LIMIT, \"(\" ", 0u, 21u,
                                "\"differs\":[]},{\"path\":\"lib.c\",\"line\":2", 1,
                                "and literals fit the parameters they are open_text against");
    symbols_signature_open_text(
        "use.py", "def run(count: int):\n    label = \"y\"\n    total(count, label", 2u, 22u,
        "{\"name\":\"total\",\"active\":1,\"arguments\":[{\"text\":\"count\",\"type\":\"int\"},{\"text\":\"label\","
        "\"type\":\"string\"}],\"declarations\":[{\"path\":\"lib.py\",\"line\":0,\"label\":\"def total(first: int, "
        "second=\\\"x\\\")\",\"params\":[[10,20],[22,32]],\"types\":[\"int\",\"string\"],\"count\":true,"
        "\"variadic\":false,\"differs\":[]}],\"reading\":false}",
        0, "Python's call open_text against its own language's declarations alone");
    symbols_signature_open_text("use.py", "x = [total(1)]\n", 0u, 5u, "null", 0,
                                "and a cursor in a bracket that opens no call has none");
    symbols_signature_open_text(
        "served.c", "int add(int a, int b) { return a + b; }\n\nint main(void) {\n  return add(1, 2)", 3u, 16u,
        "\"declarations\":[{\"path\":\"served.c\",\"line\":0,\"label\":\"int add(int a, int b)\"", 1,
        "a call of a function its own file defines on one line, its call left without its end");
}

// `text` written as the file `name` under `root`, the folders above it made
static void symbols_written(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    cli_full(root, name, path);
    if (cli_path_parent(path, folder))
    {
        (void)query_folder_made(folder);
    }
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the definition of `name` in effect at `line` of the file `path`, checked against `wanted`
static void symbols_effect_open_text(const char *path, unsigned int line, const char *name, const char *wanted,
                                     const char *what)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_symbols_effect(path, line, name, &out, &err_msg))
    {
        cli_check(0, what);
        return;
    }
    cli_check_text(s_out, out.at, wanted, what);
}

// the definition in effect: two headers redefining a macro under conditions, the compilation database giving one
// define to a folder and another to a file under it, and a declaration a condition chooses; macros called with
// arguments in a condition, a define of a folder outside the tree its include folder joins to it, a file included
// before the first line, and a define the database gives called with arguments
static void symbols_effect(void)
{
    static const char *const s_paths[9] = {"src/one.h", "src/two.h",   "src/a/use.c",  "src/a/more.c", "src/b/other.c",
                                           "tools/t.c", "src/three.h", "src/forced.h", "src/c/calls.c"};
    char root[CLI_PATH_MAX_LEN];
    cli_check_folder("symbols_effect", root);
    char outside[CLI_PATH_MAX_LEN];
    cli_check_folder("symbols_effect_inc", outside);
    symbols_written(outside, "out.h", "#define OUTSIDE 0x2A\n");
    symbols_written(root, "src/three.h",
                    "#define PICK(a, b) ((a) > (b) ? (a) : (b))\n#define JOIN(x, y) x ## y\n#define FIRST(...) "
                    "PICK(__VA_ARGS__)\n#define SIZE_4 0x10\n#define TWICE(x) (x * 2)\n");
    symbols_written(root, "src/forced.h", "#define FORCED 7\n");
    symbols_written(
        root, "src/c/calls.c",
        "#include \"../three.h\"\n#include <out.h>\n#if PICK(3, 010) == 8\n#define CHOSEN 1\n#endif\n#if "
        "JOIN(SIZE_, 4) == 16\n#define JOINED 1\n#endif\n#if FIRST(OUTSIDE, 1) == 42\n#define PAST 1\n#endif\n"
        "#if FORCED == 7\n#define BEFORE 1\n#endif\n#if TWICE(1 + 2) == 5 && GIVEN(3) == 4\n#define ORDER "
        "1\n#endif\nint w = 0;\n");
    symbols_written(root, "src/one.h",
                    "#ifndef ONE_H\n#define ONE_H\n#ifdef FAST\n#define LIMIT 8\nint speed(void);\n#else\n#define "
                    "LIMIT 4\nint speed(int slow);\n#endif\n#endif\n");
    symbols_written(root, "src/two.h", "#if defined(LEVEL) && LEVEL > 1\n#undef LIMIT\n#define LIMIT 16\n#endif\n");
    symbols_written(root, "src/a/use.c", "#include \"../one.h\"\n#include \"../two.h\"\nint x = LIMIT;\n");
    symbols_written(root, "src/a/more.c", "#include \"../one.h\"\n#include \"../two.h\"\nint x = LIMIT;\n");
    symbols_written(root, "src/b/other.c", "int z;\n");
    symbols_written(root, "tools/t.c", "#include \"../src/one.h\"\nint y = LIMIT;\n");
    // the database's folder written with forward slashes, as JSON reads them whole
    char top[CLI_PATH_MAX_LEN];
    VerbaLinea slashed = {top, sizeof(top), 0ull};
    for (const char *at = root; at[0] != '\0'; at += 1)
    {
        mmgr_line_ch(&slashed, (at[0] == '\\') ? '/' : at[0]);
    }
    (void)mmgr_line_finish(&slashed);
    char beside[CLI_PATH_MAX_LEN];
    VerbaLinea beside_slashed = {beside, sizeof(beside), 0ull};
    for (const char *at = outside; at[0] != '\0'; at += 1)
    {
        mmgr_line_ch(&beside_slashed, (at[0] == '\\') ? '/' : at[0]);
    }
    (void)mmgr_line_finish(&beside_slashed);
    static char s_database[4096];
    VerbaLinea database = {s_database, sizeof(s_database), 0ull};
    static const char *const s_entries[4][2] = {
        {"src/a/use.c", "\"arguments\": [\"cc\", \"-DFAST\", \"-DLEVEL=2\", \"-c\", \"src/a/use.c\"]"},
        {"src/a/more.c", "\"arguments\": [\"cc\", \"-DFAST\", \"-c\", \"src/a/more.c\"]"},
        {"src/b/other.c", "\"command\": \"cc -D FAST -c src/b/other.c\""},
        {"tools/t.c", "\"arguments\": [\"cc\", \"-c\", \"tools/t.c\"]"},
    };
    mmgr_line_ch(&database, '[');
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        mmgr_line_put(&database, (at != 0u) ? ",\n{\"directory\": \"" : "\n{\"directory\": \"");
        mmgr_line_put(&database, top);
        mmgr_line_put(&database, "\", \"file\": \"");
        mmgr_line_put(&database, s_entries[at][0]);
        mmgr_line_put(&database, "\", ");
        mmgr_line_put(&database, s_entries[at][1]);
        mmgr_line_ch(&database, '}');
    }
    mmgr_line_put(&database, ",\n{\"directory\": \"");
    mmgr_line_put(&database, top);
    mmgr_line_put(&database, "\", \"file\": \"src/c/calls.c\", \"arguments\": [\"cc\", \"-I");
    mmgr_line_put(&database, beside);
    mmgr_line_put(&database, "\", \"-include\", \"src/forced.h\", \"-DFAST\", \"-DGIVEN(v)=v+1\", \"-c\", "
                             "\"src/c/calls.c\"]}");
    mmgr_line_put(&database, "\n]\n");
    (void)mmgr_line_finish(&database);
    symbols_written(root, "compile_commands.json", s_database);
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 9u);
    symbols_read_waited();
    symbols_effect_open_text("src/a/use.c", 2u, "LIMIT",
                             "{\"name\":\"LIMIT\",\"path\":\"src/two.h\",\"line\":2,\"text\":\"16\"}",
                             "a macro redefined under a condition the file's own define makes hold");
    symbols_effect_open_text("src/a/more.c", 2u, "LIMIT",
                             "{\"name\":\"LIMIT\",\"path\":\"src/one.h\",\"line\":3,\"text\":\"8\"}",
                             "and under one its folder's define makes hold, where the redefinition's does not");
    symbols_effect_open_text("tools/t.c", 1u, "LIMIT",
                             "{\"name\":\"LIMIT\",\"path\":\"src/one.h\",\"line\":6,\"text\":\"4\"}",
                             "and where neither holds, the other arm's");
    symbols_effect_open_text("src/a/use.c", 2u, "FAST",
                             "{\"name\":\"FAST\",\"path\":\"src\",\"line\":null,\"text\":\"1\"}",
                             "a define every entry under a folder gives is the folder's");
    symbols_effect_open_text("src/a/use.c", 2u, "LEVEL",
                             "{\"name\":\"LEVEL\",\"path\":\"src/a/use.c\",\"line\":null,\"text\":\"2\"}",
                             "and one the folder's other entries do not give is the file's");
    symbols_effect_open_text("src/a/more.c", 2u, "speed",
                             "{\"name\":\"speed\",\"path\":\"src/one.h\",\"line\":4,\"text\":\"int speed(void);\"}",
                             "a declaration in effect is the one in the region that holds");
    symbols_effect_open_text("tools/t.c", 1u, "speed",
                             "{\"name\":\"speed\",\"path\":\"src/one.h\",\"line\":7,\"text\":\"int speed(int slow);\"}",
                             "and the other where the other holds");
    symbols_effect_open_text("tools/t.c", 1u, "none_such", "null", "and a name nothing defines has none");
    symbols_effect_open_text("src/c/calls.c", 17u, "CHOSEN",
                             "{\"name\":\"CHOSEN\",\"path\":\"src/c/calls.c\",\"line\":3,\"text\":\"1\"}",
                             "a macro called with arguments in a condition, its value a choice and an argument octal");
    symbols_effect_open_text("src/c/calls.c", 17u, "JOINED",
                             "{\"name\":\"JOINED\",\"path\":\"src/c/calls.c\",\"line\":6,\"text\":\"1\"}",
                             "two arguments joined into the name of a macro whose value is hexadecimal");
    symbols_effect_open_text(
        "src/c/calls.c", 17u, "PAST", "{\"name\":\"PAST\",\"path\":\"src/c/calls.c\",\"line\":9,\"text\":\"1\"}",
        "the arguments past the parameters handed on, one the define of an include folder outside the "
        "tree");
    symbols_effect_open_text("src/c/calls.c", 17u, "BEFORE",
                             "{\"name\":\"BEFORE\",\"path\":\"src/c/calls.c\",\"line\":12,\"text\":\"1\"}",
                             "a define of a file included before the first line");
    symbols_effect_open_text("src/c/calls.c", 17u, "ORDER",
                             "{\"name\":\"ORDER\",\"path\":\"src/c/calls.c\",\"line\":15,\"text\":\"1\"}",
                             "an argument put as its leaves, and a define the database gives called with arguments");
    symbols_effect_open_text(
        "src/c/calls.c", 17u, "OUTSIDE",
        "{\"name\":\"OUTSIDE\",\"path\":\"@symbols_effect_inc/out.h\",\"line\":0,\"text\":\"0x2A\"}",
        "a define of an include folder outside the tree is found under its mount");
    symbols_effect_open_text("src/c/calls.c", 17u, "GIVEN",
                             "{\"name\":\"GIVEN\",\"path\":\"src/c\",\"line\":null,\"text\":\"v+1\"}",
                             "and a define the database gives called with arguments is its folder's");
}

// the slice for lines inside a loop inside a function: before them the include of no file of the tree, the defines,
// the struct and the prototype the lines name, and the function they name that has no prototype whole with the define
// its body names, in the order a walk meets them, and none they do not name; the openers' heads with the local the
// lines name; the lines; and the two braces left open closed
// a usage put after the others, `path:line:from-to` joined by `|`, its line's text checked to stand at its span
static void symbols_usage_capture(void *context, const char *path, unsigned int line, unsigned int from,
                                  unsigned int to, const char *text, unsigned long long length)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put(out, path);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, to, 1u);
    mmgr_line_put(out, (to <= length) && (from < to) ? "" : "!");
    (void)text;
}

// the usages of the name at `line`, `col` of `path`, across the tree where `across`, into s_out: what the walk
// answered
static int symbols_usages_of(const char *path, unsigned int line, unsigned int col, int across)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int read = cli_symbols_usages(path, line, col, across, NULL, NULL, symbols_usage_capture, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    return read;
}

// the definition in effect of the name at `line`, `col` of `path` into s_out: what the walk answered
static int symbols_definition_of(const char *path, unsigned int line, unsigned int col)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int read = cli_symbols_definition(path, line, col, NULL, NULL, symbols_usage_capture, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    return read;
}

// the names that complete `typed` at the end of the line `    return w + ` of the usages' src/u/use.c, as the editor
// holds it, checked against `wanted`, each `name kind line` joined by `|`
static void symbols_completed(const char *typed, const char *wanted, const char *what)
{
    char text[256];
    VerbaLinea open_text = {text, sizeof(text), 0ull};
    mmgr_line_put(&open_text,
                  "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + ");
    mmgr_line_put(&open_text, typed);
    const unsigned int col = 15u + (unsigned int)cellul.len(&(CatenaFinitaCfg){.src = typed, .cap = CLI_CELLS_POOL});
    mmgr_line_put(&open_text, "\n}\n");
    const unsigned long long size = mmgr_line_finish(&open_text);
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int done =
        cli_symbols_complete("src/u/use.c", text, size, 4u, col, symbols_declaration_capture, &out, &err_msg);
    cli_check(done, what);
    cli_check_text(s_out, out.at, wanted, what);
}

// a hint put after the others, `line:col:name` joined by `|`
static void symbols_hint_capture(void *context, unsigned int line, unsigned int col, const char *name,
                                 unsigned long long length)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, col, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_put_n(out, name, length);
}

// the hints on the lines `from` to `to` of the usages' src/u/use.c, checked against `wanted`
static void symbols_hinted(unsigned int from, unsigned int to, const char *wanted, const char *what)
{
    static const char s_use[] =
        "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n";
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int done =
        cli_symbols_hints("src/u/use.c", s_use, sizeof(s_use) - 1u, from, to, symbols_hint_capture, &out, &err_msg);
    cli_check(done, what);
    cli_check_text(s_out, out.at, wanted, what);
}

// a span put after the others, `line:col-line:col` joined by `|`
static void symbols_span_capture(void *context, CliSymbolPlace from, CliSymbolPlace to)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_decimal(out, from.line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, from.col, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, to.line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, to.col, 1u);
}

// the spans around the place `line`, `col` of `text`, the file at `path`, checked against `wanted`
static void symbols_spanned_in(const char *path, const char *text, unsigned int line, unsigned int col,
                               const char *wanted, const char *what)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const CliSymbolPlace place = {line, col};
    const int done = cli_symbols_spans(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                                       place, place, symbols_span_capture, &out, &err_msg);
    cli_check(done, what);
    cli_check_text(s_out, out.at, wanted, what);
}

// the bracket pair at the place `line`, `col` of `text`, the file at `path`, its first line starting at its grammar's
// root, written `line:col-line:col`, the bracket at the place first, empty where none stands there, checked against
// `wanted`
static void symbols_paired(const char *path, const char *text, unsigned int line, unsigned int col, const char *wanted,
                           const char *what)
{
    CliSymbolPlace pair[2] = {{0u, 0u}, {0u, 0u}};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (cli_symbols_pair(path, "root", text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                         (CliSymbolPlace){line, col}, pair))
    {
        symbols_span_capture(&out, pair[0], pair[1]);
    }
    cli_check_text(s_out, out.at, wanted, what);
}

// the spans around the place `line`, `col` of the usages' src/u/use.c, checked against `wanted`
static void symbols_spanned(unsigned int line, unsigned int col, const char *wanted, const char *what)
{
    symbols_spanned_in(
        "src/u/use.c",
        "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n", line,
        col, wanted, what);
}

// a function a call hierarchy reaches put after the others, `path:line:from-to:name[line:col,...]` joined by `|`
static void symbols_call_capture(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                                 const char *name, unsigned long long length, const CliSymbolPlace *sites,
                                 unsigned int count)
{
    VerbaLinea *const out = context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put(out, path);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, to, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_put_n(out, name, length);
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        mmgr_line_decimal(out, sites[at].line, 1u);
        mmgr_line_ch(out, ':');
        mmgr_line_decimal(out, sites[at].col, 1u);
    }
    mmgr_line_ch(out, ']');
}

// the function at line `line` of `path` where `asked` is 0, its callers where 1 and its callees where 2, checked
// against `wanted`
static void symbols_called(int asked, const char *path, unsigned int line, const char *wanted, const char *what)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int found =
        (asked == 0)   ? cli_symbols_function_at(path, line, NULL, NULL, symbols_call_capture, &out, &err_msg)
        : (asked == 1) ? cli_symbols_callers(path, line, NULL, NULL, symbols_call_capture, &out, &err_msg)
                       : cli_symbols_callees(path, line, NULL, NULL, symbols_call_capture, &out, &err_msg);
    cli_check(found == 1, what);
    cli_check_text(s_out, out.at, wanted, what);
}

// whether the usages s_out holds are `wanted`, `count` of them
static int symbols_usages_are(const char *const *wanted, unsigned int count)
{
    unsigned int found = (s_out[0] != '\0') ? 1u : 0u;
    for (const char *at = s_out; *at != '\0'; at += 1)
    {
        found += (*at == '|') ? 1u : 0u;
    }
    int all = found == count;
    for (unsigned int at = 0u; all && (at < count); at += 1u)
    {
        all = cellul.find(&(CatenaFinitaCfg){
                  .src = s_out, .cap = CLI_CELLS_POOL, .other = wanted[at], .other_cap = CLI_CELLS_POOL}) != NULL;
    }
    return all;
}

// a function's usages across the files that include the header declaring it, a static of the same name elsewhere set
// aside; a macro's the same; and a parameter's and a local's in the function that declares it alone
static void symbols_usages(void)
{
    static const char *const s_paths[5] = {"src/u/shape.h", "src/u/area.c", "src/u/use.c", "src/u/other.c",
                                           "src/u/calc.py"};
    char root[CLI_PATH_MAX_LEN];
    cli_check_folder("symbols_usages", root);
    symbols_written(root, "src/u/shape.h",
                    "#ifndef SHAPE_H\n#define SHAPE_H\nint area(int w);\n#define SIDES 4\n#endif\n");
    symbols_written(root, "src/u/area.c",
                    "#include \"shape.h\"\nint area(int w)\n{\n    int scaled = w * SIDES;\n    return scaled;\n}\n");
    symbols_written(
        root, "src/u/use.c",
        "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n");
    symbols_written(root, "src/u/other.c",
                    "static int area(int x)\n{\n    return x;\n}\nint call(void)\n{\n    return area(2);\n}\n");
    symbols_written(root, "src/u/calc.py",
                    "def area(w):\n    return w * 2\n\ndef total(n):\n    w = area(n)\n    return area(w)\n");
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 5u);
    symbols_read_waited();
    static const char *const s_area[5] = {"src/u/use.c:3:12-16", "src/u/use.c:4:15-19", "src/u/shape.h:2:4-8",
                                          "src/u/area.c:1:4-8", NULL};
    cli_check((symbols_usages_of("src/u/use.c", 3u, 13u, 1) == 1) && symbols_usages_are(s_area, 4u),
              "a function's usages: the header's declaration, the definition and each call, a static of the same name "
              "in a file that includes no header declaring it set aside");
    static const char *const s_sides[4] = {"src/u/use.c:3:22-27", "src/u/shape.h:3:8-13", "src/u/area.c:3:21-26", NULL};
    cli_check((symbols_usages_of("src/u/use.c", 3u, 24u, 1) == 1) && symbols_usages_are(s_sides, 3u),
              "a macro's usages: its define and each name of it in effect");
    static const char *const s_parameter[2] = {"src/u/area.c:1:13-14", "src/u/area.c:3:17-18"};
    cli_check((symbols_usages_of("src/u/area.c", 3u, 17u, 1) == 1) && symbols_usages_are(s_parameter, 2u),
              "a parameter's usages: in the function that declares it alone");
    static const char *const s_local[2] = {"src/u/use.c:3:8-9", "src/u/use.c:4:11-12"};
    cli_check((symbols_usages_of("src/u/use.c", 4u, 11u, 1) == 1) && symbols_usages_are(s_local, 2u),
              "and a local's, a parameter of the same name in another file set aside");
    static const char *const s_alone[2] = {"src/u/use.c:3:12-16", "src/u/use.c:4:15-19"};
    cli_check((symbols_usages_of("src/u/use.c", 4u, 16u, 0) == 1) && symbols_usages_are(s_alone, 2u),
              "a function's usages in the file asked about alone");
    cli_check((symbols_usages_of("src/u/use.c", 3u, 2u, 1) == 0) && (s_out[0] == '\0'),
              "and a place that names no name has none");
    cli_check((symbols_definition_of("src/u/use.c", 3u, 13u) == 1) && symbols_usages_are(s_area + 2, 1u),
              "a function's definition from a call: the header's declaration the file's walk meets first");
    cli_check((symbols_definition_of("src/u/use.c", 3u, 24u) == 1) && symbols_usages_are(s_sides + 1, 1u),
              "a macro's: its define in effect");
    cli_check((symbols_definition_of("src/u/area.c", 3u, 17u) == 1) && symbols_usages_are(s_parameter, 1u),
              "a parameter's: its name in the function's head");
    cli_check((symbols_definition_of("src/u/use.c", 4u, 11u) == 1) && symbols_usages_are(s_local, 1u),
              "a local's: its declaration");
    cli_check((symbols_definition_of("src/u/use.c", 3u, 2u) == 0) && (s_out[0] == '\0'),
              "and a place that names no name has none");
    symbols_completed("ar", "area function 2", "what completes a word: the declarations the walk met that it begins");
    symbols_completed("S", "SHAPE_H constant 1|SIDES constant 3", "and the defines in effect it begins");
    symbols_completed("n", "n parameter 1", "and the parameters of the function the place stands in");
    symbols_completed("w", "w local 3", "and its locals declared above the place");
    symbols_hinted(1u, 4u, "3:17:w|4:20:w", "each call's argument is given the name of the parameter it is given to");
    symbols_hinted(1u, 2u, "", "and the lines of no call have none");
    symbols_spanned(3u, 13u, "3:12-3:16|3:4-3:28|2:1-5:0|2:0-5:1|1:0-5:1|0:0-6:0",
                    "the spans around a place: its word, its statement, its region inside and whole, the region with "
                    "its head and the whole text");
    symbols_spanned(3u, 17u, "3:17-3:18|3:16-3:19|3:4-3:28|2:1-5:0|2:0-5:1|1:0-5:1|0:0-6:0",
                    "and a call's brackets around an argument, a span the same as one before it once");
    symbols_spanned(1u, 14u, "1:14-1:15|1:10-1:15|1:9-1:16|1:0-1:16|1:0-5:1|0:0-6:0",
                    "and a place in a region's head: its brackets, the head, and the region with it");
    symbols_spanned_in("src/u/calc.py", "def total(n):\n    w = area(n) + 1\n    return w\n\nx = 2\n", 1u, 9u,
                       "1:8-1:12|1:4-1:19|0:13-2:12|0:12-2:12|0:0-2:12|0:0-5:0",
                       "a block by indent: its statement ended at its line's end, the block to its last line of code, "
                       "and the function with its head");
    static const char s_use[] =
        "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n";
    symbols_paired("src/u/use.c", s_use, 1u, 10u, "1:9-1:15", "the bracket that ends at a place pairs with its close");
    symbols_paired("src/u/use.c", s_use, 1u, 15u, "1:15-1:9", "one that starts at it where none ends at it");
    symbols_paired("src/u/use.c", s_use, 2u, 1u, "2:0-5:0", "a region's brackets lines apart");
    symbols_paired("src/u/use.c", s_use, 3u, 2u, "", "and a place at no bracket has no pair");
    symbols_paired("src/u/pair.c", "f(a, \")\");\n", 0u, 2u, "0:1-0:8", "a bracket in a string is passed over");
    symbols_paired("src/u/calc.py", "def total(n):\n    w = area(n) + 1\n    return w\n\nx = 2\n", 0u, 13u, "",
                   "a region a mark of no letters closes has no pair");
    symbols_called(0, "src/u/use.c", 3u, "src/u/use.c:1:4-9:total[]", "the function a line of a body stands in");
    symbols_called(1, "src/u/area.c", 1u, "src/u/use.c:1:4-9:total[3:12,4:15]",
                   "a function's callers, each with its calls of it, a static of the same name's caller set aside");
    symbols_called(2, "src/u/use.c", 1u, "src/u/shape.h:2:4-8:area[3:12,4:15]",
                   "and the functions it calls, each its definition in effect with the calls of it");
    symbols_called(0, "src/u/calc.py", 4u, "src/u/calc.py:3:4-9:total[]",
                   "a block by indent's function a line of its body stands in");
    symbols_called(1, "src/u/calc.py", 0u, "src/u/calc.py:3:4-9:total[4:8,5:11]",
                   "and its callers, each with its calls of it");
    symbols_called(2, "src/u/calc.py", 3u, "src/u/calc.py:0:4-8:area[4:8,5:11]", "and the functions it calls");
}

static void symbols_slice(void)
{
    static const char *const s_paths[2] = {"src/s/shape.h", "src/s/use.c"};
    char root[CLI_PATH_MAX_LEN];
    cli_check_folder("symbols_slice", root);
    symbols_written(
        root, "src/s/shape.h",
        "#ifndef SHAPE_H\n#define SHAPE_H\n#include <stddef.h>\n#define SIDES 4\n#define AREA(w, h) ((w) * "
        "(h))\nstruct box {\n    int w;\n    int h;\n};\nstruct unused {\n    int z;\n};\nint perimeter(const "
        "struct box *b);\nint unrelated(int q);\n#endif\n");
    symbols_written(
        root, "src/s/use.c",
        "#include \"shape.h\"\n#define SCALE 2\nstatic int twice(int v)\n{\n    return v * SCALE;\n}\nint "
        "measure(struct box *b, int n)\n{\n    int total = 0;\n    int other = 1;\n    for (int i = 0; i < n; "
        "i++)\n    {\n        total += AREA(b[i].w, b[i].h) +\n                 perimeter(&b[i]) *\n         "
        "        SIDES;\n        total += twice(i);\n    }\n    return total + other;\n}\n");
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 2u);
    symbols_read_waited();
    static const unsigned int s_openers[2] = {6u, 10u};
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_symbols_slice("src/s/use.c", NULL, 0ull, 13u, 15u, s_openers, 2u, &out, &err_msg))
    {
        cli_check(0, "the slice for lines inside a loop inside a function is cut");
        return;
    }
    cli_check_text(
        s_out, out.at,
        "{\"lines\":[{\"path\":\"src/s/shape.h\",\"line\":2,\"text\":\"#include <stddef.h>\"},{\"path\":\"src/s/"
        "shape.h\",\"line\":3,\"text\":\"#define SIDES 4\"},{\"path\":\"src/s/shape.h\",\"line\":4,\"text\":\"#define "
        "AREA(w, h) ((w) * (h))\"},{\"path\":\"src/s/shape.h\",\"line\":5,\"text\":\"struct box {\"},{\"path\":\"src/"
        "s/shape.h\",\"line\":6,\"text\":\"    int w;\"},{\"path\":\"src/s/shape.h\",\"line\":7,\"text\":\"    int "
        "h;\"},{\"path\":\"src/s/shape.h\",\"line\":8,\"text\":\"};\"},{\"path\":\"src/s/shape.h\",\"line\":12,"
        "\"text\":\"int perimeter(const struct box *b);\"},{\"path\":\"src/s/use.c\",\"line\":1,\"text\":\"#define "
        "SCALE 2\"},{\"path\":\"src/s/use.c\",\"line\":2,\"text\":\"static int twice(int "
        "v)\"},{\"path\":\"src/s/use.c\",\"line\":3,\"text\":\"{\"},{\"path\":\"src/s/use.c\",\"line\":4,\"text\":\"   "
        " "
        "return v * SCALE;\"},{\"path\":\"src/s/use.c\",\"line\":5,\"text\":\"}\"},{\"path\":\"src/s/"
        "use.c\",\"line\":6,\"text\":\"int measure(struct box *b, int "
        "n)\"},{\"path\":\"src/s/use.c\",\"line\":7,\"text\":\"{\"},{\"path\":\"src/s/use.c\",\"line\":8,\"text\":\"   "
        " "
        "int total = 0;\"},{\"path\":\"src/s/use.c\",\"line\":10,\"text\":\"    for (int i = 0; i < n; "
        "i++)\"},{\"path\":\"src/s/use.c\",\"line\":11,\"text\":\"    {\"},{\"path\":\"src/s/use.c\",\"line\":12,"
        "\"text\":\"        total += AREA(b[i].w, b[i].h) +\"},{\"path\":\"src/s/use.c\",\"line\":13,\"text\":\"     "
        "            perimeter(&b[i]) *\"},{\"path\":\"src/s/use.c\",\"line\":14,\"text\":\"                 "
        "SIDES;\"},{\"path\":\"src/s/use.c\",\"line\":15,\"text\":\"        total += "
        "twice(i);\"},{\"path\":null,\"line\":null,\"text\":\"}\"},{\"path\":null,\"line\":null,\"text\":\"}\"}]}",
        "the slice for lines inside a loop inside a function, the first inside a statement: the definitions they name, "
        "a function with no prototype whole, in the order a walk meets them, the openers with the local they name, the "
        "lines from the statement's first, and the braces left open closed");
    out.at = 0ull;
    const int cut = cli_symbols_slice("src/s/use.c", NULL, 0ull, 12u, 13u, s_openers, 2u, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    cli_check(
        cut &&
            (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "\"line\":14,\"text\":\"                 SIDES;\"",
                                            .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_out, .cap = CLI_CELLS_POOL, .other = "\"line\":15", .other_cap = CLI_CELLS_POOL}) == NULL),
        "and lines whose last stands inside a statement run on to its end");
    static const char s_edited[] =
        "#include \"shape.h\"\n#define SCALE 2\nstatic int twice(int v)\n{\n    return v * SCALE;\n}\nint "
        "measure(struct "
        "box *b, int n)\n{\n    int total = 0;\n    int other = 1;\n    for (int i = 0; i < n; i++)\n    {\n        "
        "total "
        "+= AREA(b[i].w, b[i].h) +\n                 perimeter(&b[i]) *\n                 SIDES;\n        total += "
        "twice(i) * 2;\n    }\n    return total + other;\n}\n";
    out.at = 0ull;
    const int edited =
        cli_symbols_slice("src/s/use.c", s_edited, sizeof(s_edited) - 1u, 15u, 15u, s_openers, 2u, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    cli_check(edited &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "\"line\":15,\"text\":\"        total += twice(i) * 2;\"",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "and the file's text the editor holds stands in for the disk's");
    static const char s_moved[] =
        "\n#include \"shape.h\"\n#define SCALE 2\nstatic int twice(int v)\n{\n    return v * SCALE;\n}\nint "
        "measure(struct "
        "box *b, int n)\n{\n    int total = 0;\n    int other = 1;\n    for (int i = 0; i < n; i++)\n    {\n        "
        "total += AREA(b[i].w, b[i].h) +\n                 perimeter(&b[i]) *\n                 SIDES;\n        total "
        "+= twice(i);\n    }\n    return total + other;\n}\n";
    static const unsigned int s_moved_openers[2] = {7u, 11u};
    out.at = 0ull;
    const int moved =
        cli_symbols_slice("src/s/use.c", s_moved, sizeof(s_moved) - 1u, 16u, 16u, s_moved_openers, 2u, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    cli_check(moved && (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                                       .cap = CLI_CELLS_POOL,
                                                       .other = "\"line\":3,\"text\":\"static int twice(int v)\"",
                                                       .other_cap = CLI_CELLS_POOL}) != NULL),
              "and what the editor's text declares stands on its own lines, not on the disk's");
    cli_symbols_forget();
    out.at = 0ull;
    const int unread = cli_symbols_slice("src/s/use.c", NULL, 0ull, 13u, 15u, s_openers, 2u, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    cli_check(unread &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "\"line\":0,\"text\":\"#include \\\"shape.h\\\"\"",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "\"line\":2,\"text\":\"static int twice(int v)\"",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "and a slice cut before the tree reads its files writes the include of a header it has not read for "
              "the server to read, and declares what the file's own leaves name");
}

// one line of a window's slice written as `path:line:text`, `-:-` for a bracket the slice closes
static void symbols_window_line(void *context, const char *path, unsigned int line, const char *text,
                                unsigned long long length)
{
    VerbaLinea *const out = (VerbaLinea *)context;
    if (path == NULL)
    {
        mmgr_line_put(out, "-:-");
    }
    else
    {
        mmgr_line_put(out, path);
        mmgr_line_ch(out, ':');
        mmgr_line_decimal(out, line, 1u);
    }
    mmgr_line_ch(out, ':');
    // a line of the fixture is below 2^32 bytes
    mmgr_line_put_n(out, text, (size_t)length);
    mmgr_line_ch(out, '\n');
}

// a window written after its slice's lines as `window first-last openers` and each opener's line
static int symbols_window(void *context, unsigned int first, unsigned int last, const unsigned int *opener,
                          unsigned int openers)
{
    VerbaLinea *const out = (VerbaLinea *)context;
    mmgr_line_put(out, "window ");
    mmgr_line_decimal(out, first, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, last, 1u);
    mmgr_line_put(out, " openers");
    for (unsigned int at = 0u; at < openers; at += 1u)
    {
        mmgr_line_ch(out, ' ');
        mmgr_line_decimal(out, opener[at], 1u);
    }
    mmgr_line_ch(out, '\n');
    return 1;
}

// whether `text` stands in what is written from `from` on
static int symbols_written_after(const char *from, const char *text)
{
    return (from != NULL) &&
           (cellul.find(&(CatenaFinitaCfg){
                .src = from, .cap = CLI_CELLS_POOL, .other = text, .other_cap = CLI_CELLS_POOL}) != NULL);
}

// a file of two windows cut into its slices in one walk: the second stands inside an else's region inside a function,
// the brace of an inactive region above it no opener of it
static void symbols_slices(void)
{
    static const char *const s_paths[2] = {"src/w/shape.h", "src/w/use.c"};
    char root[CLI_PATH_MAX_LEN];
    cli_check_folder("symbols_slices", root);
    symbols_written(root, "src/w/shape.h", "int area(int w);\n");
    static char s_use[8192];
    VerbaLinea use = {s_use, sizeof(s_use), 0ull};
    mmgr_line_put(&use,
                  "#include \"shape.h\"\n#if 0\nstatic void gone(void) {\n#endif\nint helper(int v)\n{\n    return "
                  "v + 1;\n}\nint measure(int n)\n{\n    int total = 0;\n    if (n > 0)\n    {\n        total = "
                  "1;\n    } else {\n        total = 2;\n");
    for (unsigned int line = 16u; line < 70u; line += 1u)
    {
        mmgr_line_put(&use, "        total += ");
        mmgr_line_decimal(&use, line, 1u);
        mmgr_line_put(&use, ";\n");
    }
    mmgr_line_put(&use, "        total = area(total);\n    }\n    return helper(total);\n}\n");
    (void)mmgr_line_finish(&use);
    symbols_written(root, "src/w/use.c", s_use);
    cli_symbols_forget();
    cli_symbols_refresh(root, s_paths, 2u);
    symbols_read_waited();
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int cut =
        cli_symbols_slices_each("src/w/use.c", NULL, 0ull, 64u, symbols_window_line, symbols_window, &out, &err_msg);
    (void)mmgr_line_finish(&out);
    const char *const second = cellul.find(&(CatenaFinitaCfg){
        .src = s_out, .cap = CLI_CELLS_POOL, .other = "window 0-63 openers\n", .other_cap = CLI_CELLS_POOL});
    cli_check(cut && (second != NULL) && symbols_written_after(second, "window 64-127 openers 8 14\n") &&
                  !symbols_written_after(second, "window 128"),
              "a file's slices cut in one walk a window at a time, each window's openers the lines that open the "
              "regions its first stands in, an inactive region's brace none of them");
    cli_check(symbols_written_after(second, "src/w/use.c:8:int measure(int n)\nsrc/w/use.c:9:{\nsrc/w/use.c:10:    "
                                            "int total = 0;\nsrc/w/use.c:14:else {\n") &&
                  symbols_written_after(second, "src/w/use.c:70:        total = area(total);\n") &&
                  symbols_written_after(second, "src/w/shape.h:0:int area(int w);\n") &&
                  symbols_written_after(second, "src/w/use.c:4:int helper(int v)\n"),
              "and the window inside the else gives its function's head, the local it names and the else's head past "
              "the brace closing the region before it, with what its names follow to as the walk met them");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    symbols_names(
        "x.py",
        "import os\nLIMIT = 3\nlimit = 4\nX: int = 5\nclass Page:\n    def read(self):\n        pass\nasync def "
        "go():\n    if a == b:\n        pass\n",
        "LIMIT constant 1|X constant 3|Page class 4|read function 5|go function 7", "Python's declarations");
    symbols_names(
        "x.rs",
        "pub(crate) fn alpha() {}\npub struct Beta;\nconst GAMMA: u32 = 1;\nmacro_rules! delta {\n}\npub async "
        "unsafe fn epsilon() {}\nmod zeta;\nimpl Beta {\nextern \"C\" fn eta() {}\nstatic mut THETA: u8 = 0;\n",
        "alpha function 0|Beta class 1|GAMMA constant 2|delta function 3|epsilon function 5|zeta module 6|eta "
        "function 8|THETA constant 9",
        "Rust's declarations");
    symbols_names("x.js",
                  "export async function load(path) {\nclass Editor {\n  paint() {\n    if (x) {\n  }\nconst STEP = "
                  "4;\nexport const go = (a) => a;\nlet run = async () => 1;\n",
                  "load function 0|Editor class 1|paint method 2|STEP constant 5|go function 6|run function 7",
                  "a script's declarations");
    symbols_names(
        "x.c",
        "#define LIMIT 4\n#  define TWICE(x) ((x) * 2)\nstruct grid {\ntypedef enum shade\nstatic int "
        "count_cells(const struct grid *g) {\nint scale(int a,\n  return helper(1);\nint value = make(2);\nvoid "
        "declared(int);\nif (x) {\n",
        "LIMIT constant 0|TWICE constant 1|grid class 2|shade class 3|count_cells function 4|scale function 5|declared "
        "function 8",
        "C's declarations");
    symbols_names("x.sh", "build() {\nfunction clean {\necho hi\n", "build function 0|clean function 1", "a shell's");
    symbols_names("x.ps1", "Function Get-Thing {\n", "Get-Thing function 0", "PowerShell's");
    symbols_names("x.toml", "[package]\n[[bin]]\nname = 1\n", "package module 0|bin module 1", "TOML's tables");
    symbols_names("x.md", "# Title\n```\n# not this\n```\n## Part two ##\n", "Title heading 0|Part two heading 4",
                  "Markdown's headings, none in code");
    symbols_names("x.tex", "\\section*{Start}\n\\subsection[short]{Long}\n", "Start heading 0|Long heading 1",
                  "TeX's sections");
    const long long exact = symbols_score("paint", "paint");
    const long long begins = symbols_score("paint", "paintStrip");
    const long long contains = symbols_score("paint", "repaint");
    const long long initials = symbols_score("gtf", "go_to_file");
    const long long camel = symbols_score("gtf", "goToFile");
    const long long letters = symbols_score("pnt", "paint");
    cli_check((exact > begins) && (begins > contains) && (contains > initials) && (initials > letters),
              "scores rank the name itself first");
    cli_check((initials == symbols_score("gtf", "go_to_file")) && (camel > letters),
              "a word's start ranks over letters");
    cli_check(symbols_score("xyz", "paint") == -1ll, "and a query that names none does not answer");
    symbols_stop();
    symbols_tree();
    symbols_signature();
    symbols_effect();
    symbols_slice();
    symbols_slices();
    symbols_usages();
    return cli_checked();
}
