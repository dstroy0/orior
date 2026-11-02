// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_symbols.c: Python's, Rust's, scripts', C's and the other languages' declarations are read; scores rank the name itself first; a first reading reports
// how far it has gone and a stop stores what it read; the tree is read and a file read again once it changes; a call's signature is read from the tree; a
// name's definition in effect is found through includes, conditions and the compilation database's defines; a slice is cut for a view; and a name's usages,
// definition, completions, hints, spans, bracket pair and call hierarchy are read
#include "ui/cli/src/code/symbols.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the tree a group of cases reads: the walk of a tree is one for the process, each group walking its own once on arriving
typedef enum
{
    TREE_NONE = 0,
    TREE_SIGNATURE,
    TREE_EFFECT,
    TREE_SLICE,
    TREE_SLICES,
    TREE_USAGES
} SymbolsTree;

static const char s_use_text[] = "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + area(1);\n}\n";
static const char s_indent_text[] = "def total(n):\n    w = area(n) + 1\n    return w\n\nx = 2\n";
static const unsigned int s_slice_openers[2] = {6u, 10u};

static const char *const s_area_usages[5] = {"src/u/use.c:3:12-16", "src/u/use.c:4:15-19", "src/u/shape.h:2:4-8", "src/u/area.c:1:4-8", NULL};
static const char *const s_sides_usages[4] = {"src/u/use.c:3:22-27", "src/u/shape.h:3:8-13", "src/u/area.c:3:21-26", NULL};
static const char *const s_parameter_usages[2] = {"src/u/area.c:1:13-14", "src/u/area.c:3:17-18"};
static const char *const s_local_usages[2] = {"src/u/use.c:3:8-9", "src/u/use.c:4:11-12"};

static char s_out[1u << 20u];
static char s_why[256];
static SymbolsTree s_tree;

static int s_stop_ran;
static int s_stop_seen;
static unsigned long long s_stop_total;
static unsigned long long s_stop_done;
static int s_stop_ended;
static int s_stop_unwalked;
static unsigned int s_stop_found;
static unsigned int s_stop_answered;

static int s_slices_ran;
static int s_slices_cut;

// a declaration written after the others, `name kind line` joined by `|`
static void declaration_recorded(const CliDeclaredCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put_n(out, args->name, args->length);
    mmgr_line_ch(out, ' ');
    mmgr_line_put(out, args->kind);
    mmgr_line_ch(out, ' ');
    mmgr_line_decimal(out, args->line, 1u);
}

// the declarations of `text` read as the file `path`, asserted against `wanted`
static void assert_declarations(const char *path, const char *text, const char *wanted)
{
    char out[1024];
    VerbaLinea line = {out, sizeof(out), 0ull};
    EMBED_CALL(cli_symbols_path_text_size.declare, CliSymbolsPathTextSizeCfg, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .each_declared = declaration_recorded, .context = &line);
    CLI_TEST_ASSERT_TEXT(wanted, out, line.at);
}

// the score `query` assigns `name`, -1 where it does not name it
static long long score_of(const char *query, const char *name)
{
    long long points = 0ll;
    return EMBED_CALL(cli_symbols_query.score, CliSymbolsQueryCfg, .query = query, .name = name, .name_length = CLI_UNITY_LENGTH(name), .points = &points) ? points : -1ll;
}

// the symbols answering `query`, at most `most`, read into the shared tree: the array's node, `reading` set where the tree was still being read
static unsigned int symbols_found(const char *query, unsigned int most, const CliJson **json, int *reading)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    unsigned int found = CLI_JSON_NONE;
    EMBED_CALL(cli_symbols_query_most.tree_declarations_find, CliSymbolsQueryMostCfg, .query = query, .most = most, .out = &out);
    *json = cli_unity_parsed(s_out, out.at, &found);
    *reading = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = *json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = *json, .object = found, .name = "reading"), .kind = CFG_JSON_TRUE);
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = *json, .object = found, .name = "symbols");
}

// the tree waited for until it is no longer being read
static void reading_waited(void)
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
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 10u);
    }
}

// the tree `root` forgotten and walked, waited for
static void tree_walked_fresh(const char *root)
{
    cli_tree_forgotten();
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = root);
    reading_waited();
}

// a tree of 3000 files walked and stopped, what it reported recorded, once for the process
static void stop_ran(void)
{
    static char s_names[3000][16];
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char read_name[32];
    if (s_stop_ran)
    {
        return;
    }
    s_stop_ran = 1;
    s_tree = TREE_NONE;
    cli_unity_folder("symbols_stop", root);
    for (unsigned int at = 0u; at < 3000u; at += 1u)
    {
        char text[64];
        VerbaLinea name = {s_names[at], sizeof(s_names[at]), 0ull};
        VerbaLinea line = {text, sizeof(text), 0ull};
        mmgr_line_ch(&name, 'f');
        mmgr_line_decimal(&name, at, 4u);
        mmgr_line_put(&name, ".py");
        (void)mmgr_line_finish(&name);
        mmgr_line_put(&line, "def step_");
        mmgr_line_decimal(&line, at, 1u);
        mmgr_line_put(&line, "():\n    pass\n");
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = s_names[at], .joined = path);
        (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = line.at);
    }
    cli_tree_forgotten();
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = root);
    unsigned long long done = 0ull;
    for (unsigned int tries = 0u; !s_stop_seen && (tries < 500u); tries += 1u)
    {
        s_stop_seen = EMBED_CALL(cli_symbols_done.tree_walk_progress, CliSymbolsDoneCfg, .done = &done, .total = &s_stop_total) && (done > 0ull);
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 1u);
    }
    s_stop_done = done;
    cli_tree_walk_stop();
    for (unsigned int tries = 0u; EMBED_CALL(cli_symbols_done.tree_walk_progress, CliSymbolsDoneCfg, .done = &done, .total = &s_stop_total) && (tries < 500u); tries += 1u)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 2u);
    }
    s_stop_ended = !EMBED_CALL(cli_symbols_done.tree_walk_progress, CliSymbolsDoneCfg, .done = &done, .total = &s_stop_total);
    s_stop_unwalked = cli_tree_unwalked();
    const CliJson *json = NULL;
    int reading = 0;
    const unsigned int found_node = symbols_found("step_", 3000u, &json, &reading);
    s_stop_found = json->node[found_node].count;
    // a walk goes in the listing's order, which the system's folders set: a name it read, whichever that was
    unsigned long long read_length = 0ull;
    const char *const named = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = found_node, .index = 0u), .name = "name"), .length_pointer = &read_length);
    (void)cellul.copy(&(CatenaFinitaCfg){.dst = read_name, .src = (named != NULL) ? named : "", .cap = sizeof(read_name)});
    const unsigned int answered = symbols_found(read_name, 1u, &json, &reading);
    s_stop_answered = json->node[answered].count;
}

// `text` written as the file `name` under `root`, the folders above it made
static void file_written(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    for (char *at = path + CLI_UNITY_LENGTH(root) + 1u; at[0] != '\0'; at += 1)
    {
        if (at[0] == '/')
        {
            at[0] = '\0';
            (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
            at[0] = '/';
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = CLI_UNITY_LENGTH(text)), name);
}

// the signature group's tree active
static void signature_tree(void)
{
    char root[CLI_PATH_MAX_LEN];
    if (s_tree == TREE_SIGNATURE)
    {
        return;
    }
    s_tree = TREE_SIGNATURE;
    cli_unity_folder("symbols_signature", root);
    file_written(root, "lib.c", "#define LIMIT 4\nint total(int first, const char *second);\nstatic int total(int "
                                "first, const char *second)\n{\n    return first;\n}\n");
    file_written(root, "other.c", "int total(int first);\n");
    file_written(root, "lib.py", "def total(first: int, second=\"x\"):\n    pass\n");
    file_written(root, "served.c", "int add(int a, int b) { return a + b; }\n\nint main(void) {\n  return add(1, 2)\n}\n");
    tree_walked_fresh(root);
}

// the signature of the call the cursor is in at `line`, `col` of `text`, sent as the file `path`, asserted against `wanted` whole, or containing it where `part`
static void assert_signature(const char *path, const char *text, unsigned int line, unsigned int col, const char *wanted, int part)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_text_size_out.signature, CliSymbolsPathTextSizeOutCfg, .path = path, .state = "root", .text = text, .size = CLI_UNITY_LENGTH(text), .line = line, .col = col, .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    if (part)
    {
        CLI_TEST_ASSERT_CONTAINS(wanted, s_out);
    }
    else
    {
        TEST_ASSERT_EQUAL_STRING(wanted, s_out);
    }
}

// `path` with its backslashes made slashes into `out`, which is sized for CLI_PATH_MAX_LEN
static void path_slashed(const char *path, char *out)
{
    VerbaLinea slashed = {out, CLI_PATH_MAX_LEN, 0ull};
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        mmgr_line_ch(&slashed, (at[0] == '\\') ? '/' : at[0]);
    }
    (void)mmgr_line_finish(&slashed);
}

// the effect group's tree active: two headers redefining a macro under conditions, the compilation database giving one define to a folder and another to a file
// under it, a declaration a condition chooses, macros called with arguments in a condition, a define of a folder outside the tree its include folder joins to it,
// a file included before the first line, and a define the database gives called with arguments
static void effect_tree(void)
{
    static char s_database[4096];
    static const char *const s_entries[4][2] = {
        {"src/a/use.c", "\"arguments\": [\"cc\", \"-DFAST\", \"-DLEVEL=2\", \"-c\", \"src/a/use.c\"]"},
        {"src/a/more.c", "\"arguments\": [\"cc\", \"-DFAST\", \"-c\", \"src/a/more.c\"]"},
        {"src/b/other.c", "\"command\": \"cc -D FAST -c src/b/other.c\""},
        {"tools/t.c", "\"arguments\": [\"cc\", \"-c\", \"tools/t.c\"]"},
    };
    char root[CLI_PATH_MAX_LEN];
    char outside[CLI_PATH_MAX_LEN];
    char top[CLI_PATH_MAX_LEN];
    char beside[CLI_PATH_MAX_LEN];
    if (s_tree == TREE_EFFECT)
    {
        return;
    }
    s_tree = TREE_EFFECT;
    cli_unity_folder("symbols_effect", root);
    cli_unity_folder("symbols_effect_inc", outside);
    file_written(outside, "out.h", "#define OUTSIDE 0x2A\n");
    file_written(root, "src/three.h", "#define PICK(a, b) ((a) > (b) ? (a) : (b))\n#define JOIN(x, y) x ## y\n#define FIRST(...) "
                                      "PICK(__VA_ARGS__)\n#define SIZE_4 0x10\n#define TWICE(x) (x * 2)\n");
    file_written(root, "src/forced.h", "#define FORCED 7\n");
    file_written(root, "src/c/calls.c", "#include \"../three.h\"\n#include <out.h>\n#if PICK(3, 010) == 8\n#define CHOSEN 1\n#endif\n#if "
                                        "JOIN(SIZE_, 4) == 16\n#define JOINED 1\n#endif\n#if FIRST(OUTSIDE, 1) == 42\n#define PAST 1\n#endif\n"
                                        "#if FORCED == 7\n#define BEFORE 1\n#endif\n#if TWICE(1 + 2) == 5 && GIVEN(3) == 4\n#define ORDER "
                                        "1\n#endif\nint w = 0;\n");
    file_written(root, "src/one.h", "#ifndef ONE_H\n#define ONE_H\n#ifdef FAST\n#define LIMIT 8\nint speed(void);\n#else\n#define "
                                    "LIMIT 4\nint speed(int slow);\n#endif\n#endif\n");
    file_written(root, "src/two.h", "#if defined(LEVEL) && LEVEL > 1\n#undef LIMIT\n#define LIMIT 16\n#endif\n");
    file_written(root, "src/a/use.c", "#include \"../one.h\"\n#include \"../two.h\"\nint x = LIMIT;\n");
    file_written(root, "src/a/more.c", "#include \"../one.h\"\n#include \"../two.h\"\nint x = LIMIT;\n");
    file_written(root, "src/b/other.c", "int z;\n");
    file_written(root, "tools/t.c", "#include \"../src/one.h\"\nint y = LIMIT;\n");
    // the database's folder written with forward slashes, as JSON reads them whole
    path_slashed(root, top);
    path_slashed(outside, beside);
    VerbaLinea database = {s_database, sizeof(s_database), 0ull};
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
    mmgr_line_put(&database, "\", \"-include\", \"src/forced.h\", \"-DFAST\", \"-DGIVEN(v)=v+1\", \"-c\", \"src/c/calls.c\"]}");
    mmgr_line_put(&database, "\n]\n");
    (void)mmgr_line_finish(&database);
    file_written(root, "compile_commands.json", s_database);
    tree_walked_fresh(root);
}

// the definition of `name` in effect at `line` of the file `path`, asserted against `wanted`
static void assert_effect(const char *path, unsigned int line, const char *name, const char *wanted)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    effect_tree();
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_symbols_path_name.effect, CliSymbolsPathNameCfg, .path = path, .line = line, .name = name, .out = &out, .err_msg = &err_msg), name);
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// a usage written after the others, `path:line:from-to` joined by `|`, `!` after a span past its line's text
static void usage_recorded(const CliUsageCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put(out, args->path);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, args->line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, args->from, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, args->to, 1u);
    mmgr_line_put(out, (args->to <= args->length) && (args->from < args->to) ? "" : "!");
}

// the usages group's tree active
static void usages_tree(void)
{
    char root[CLI_PATH_MAX_LEN];
    if (s_tree == TREE_USAGES)
    {
        return;
    }
    s_tree = TREE_USAGES;
    cli_unity_folder("symbols_usages", root);
    file_written(root, "src/u/shape.h", "#ifndef SHAPE_H\n#define SHAPE_H\nint area(int w);\n#define SIDES 4\n#endif\n");
    file_written(root, "src/u/area.c", "#include \"shape.h\"\nint area(int w)\n{\n    int scaled = w * SIDES;\n    return scaled;\n}\n");
    file_written(root, "src/u/use.c", s_use_text);
    file_written(root, "src/u/other.c", "static int area(int x)\n{\n    return x;\n}\nint call(void)\n{\n    return area(2);\n}\n");
    file_written(root, "src/u/calc.py", "def area(w):\n    return w * 2\n\ndef total(n):\n    w = area(n)\n    return area(w)\n");
    tree_walked_fresh(root);
}

// the usages of the name at `line`, `col` of `path`, across the tree where `across`, in s_out: what the walk answered
static int usages_of(const char *path, unsigned int line, unsigned int col, int across)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    usages_tree();
    const int read = EMBED_CALL(cli_symbols_path_context.usages, CliSymbolsPathContextCfg, .path = path, .line = line, .col = col, .across = across, .open_text = NULL, .open_text_context = NULL, .each_usage = usage_recorded, .context = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    return read;
}

// the definition in effect of the name at `line`, `col` of `path` in s_out: what the walk answered
static int definition_of(const char *path, unsigned int line, unsigned int col)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    usages_tree();
    const int read = EMBED_CALL(cli_symbols_path_context.definition, CliSymbolsPathContextCfg, .path = path, .line = line, .col = col, .open_text = NULL, .open_text_context = NULL, .each_usage = usage_recorded, .context = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    return read;
}

// the usages in s_out asserted to be `wanted`, `count` of them
static void assert_usages(const char *const *wanted, unsigned int count)
{
    unsigned int found = (s_out[0] != '\0') ? 1u : 0u;
    for (const char *at = s_out; *at != '\0'; at += 1)
    {
        found += (*at == '|') ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT_MESSAGE(count, found, s_out);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        CLI_TEST_ASSERT_CONTAINS(wanted[at], s_out);
    }
}

// the names that complete `partial` at the end of the line `    return w + ` of the usages' src/u/use.c, as the editor stores it, asserted against `wanted`, each
// `name kind line` joined by `|`
static void assert_completed(const char *partial, const char *wanted)
{
    char text[256];
    char why[256];
    VerbaLinea open_text = {text, sizeof(text), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    usages_tree();
    mmgr_line_put(&open_text, "#include \"shape.h\"\nint total(int n)\n{\n    int w = area(n) + SIDES;\n    return w + ");
    mmgr_line_put(&open_text, partial);
    // a word typed here is a few letters
    const unsigned int col = 15u + (unsigned int)CLI_UNITY_LENGTH(partial);
    mmgr_line_put(&open_text, "\n}\n");
    const unsigned long long size = mmgr_line_finish(&open_text);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_text_size.complete, CliSymbolsPathTextSizeCfg, .path = "src/u/use.c", .text = text, .size = size, .line = 4u, .col = col, .each_declared = declaration_recorded, .context = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// a hint written after the others, `line:col:name` joined by `|`
static void hint_recorded(const CliHintedCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_decimal(out, args->line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, args->col, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_put_n(out, args->name, args->length);
}

// the hints on the lines `from` to `to` of the usages' src/u/use.c, asserted against `wanted`
static void assert_hints(unsigned int from, unsigned int to, const char *wanted)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    usages_tree();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_text_size_context.hints, CliSymbolsPathTextSizeContextCfg, .path = "src/u/use.c", .text = s_use_text, .size = sizeof(s_use_text) - 1u, .from = from, .to = to, .each_hinted = hint_recorded, .context = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// a span written after the others, `line:col-line:col` joined by `|`
static void span_recorded(const CliSpannedCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_decimal(out, args->from.line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, args->from.col, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, args->to.line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, args->to.col, 1u);
}

// the spans around the place `line`, `col` of `text`, the file at `path`, asserted against `wanted`
static void assert_spans(const char *path, const char *text, unsigned int line, unsigned int col, const char *wanted)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const CliSymbolPlace place = {line, col};
    usages_tree();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_text_size_context_err_msg_from_symbol_place.spans, CliSymbolsPathTextSizeContextErrMsgFromSymbolPlaceCfg, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .from_symbol_place = place, .to_symbol_place = place, .each_spanned = span_recorded, .context = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// the bracket pair at the place `line`, `col` of `text`, the file at `path`, its first line starting at its grammar's root, written `line:col-line:col`, the bracket
// at the place first, empty where none is there, asserted against `wanted`
static void assert_pair(const char *path, const char *text, unsigned int line, unsigned int col, const char *wanted)
{
    CliSymbolPlace pair[2] = {{0u, 0u}, {0u, 0u}};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    usages_tree();
    if (EMBED_CALL(cli_symbols_path_text_size_state.pair, CliSymbolsPathTextSizeStateCfg, .path = path, .state = "root", .text = text, .size = CLI_UNITY_LENGTH(text), .at_symbol_place = (CliSymbolPlace){line, col}, .pair = pair))
    {
        EMBED_CALL(span_recorded, CliSpannedCfg, .context = &out, .from = pair[0], .to = pair[1]);
    }
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// a function a call hierarchy reaches written after the others, `path:line:from-to:name[line:col,...]` joined by `|`
static void call_recorded(const CliCallReachedCfg *args)
{
    VerbaLinea *const out = args->context;
    mmgr_line_put(out, (out->at > 0ull) ? "|" : "");
    mmgr_line_put(out, args->path);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, args->line, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_decimal(out, args->from, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, args->to, 1u);
    mmgr_line_ch(out, ':');
    mmgr_line_put_n(out, args->name, args->length);
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        mmgr_line_decimal(out, args->sites[at].line, 1u);
        mmgr_line_ch(out, ':');
        mmgr_line_decimal(out, args->sites[at].col, 1u);
    }
    mmgr_line_ch(out, ']');
}

// the function at line `line` of `path` where `asked` is 0, its callers where 1 and its callees where 2, asserted against `wanted`
static void assert_calls(int asked, const char *path, unsigned int line, const char *wanted)
{
    char why[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    usages_tree();
    const int found = (asked == 0) ? EMBED_CALL(cli_symbols_path_context_line.function_at, CliSymbolsPathContextLineCfg, .path = path, .line = line, .open_text = NULL, .open_text_context = NULL, .each = call_recorded, .context = &out, .err_msg = &err_msg) : (asked == 1) ? EMBED_CALL(cli_symbols_path_context_line.callers, CliSymbolsPathContextLineCfg, .path = path, .line = line, .open_text = NULL, .open_text_context = NULL, .each = call_recorded, .context = &out, .err_msg = &err_msg) : EMBED_CALL(cli_symbols_path_context_line.callees, CliSymbolsPathContextLineCfg, .path = path, .line = line, .open_text = NULL, .open_text_context = NULL, .each = call_recorded, .context = &out, .err_msg = &err_msg);
    TEST_ASSERT_EQUAL_INT(1, found);
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// the slice group's tree active
static void slice_tree(void)
{
    char root[CLI_PATH_MAX_LEN];
    if (s_tree == TREE_SLICE)
    {
        return;
    }
    s_tree = TREE_SLICE;
    cli_unity_folder("symbols_slice", root);
    file_written(root, "src/s/shape.h", "#ifndef SHAPE_H\n#define SHAPE_H\n#include <stddef.h>\n#define SIDES 4\n#define AREA(w, h) ((w) * "
                                        "(h))\nstruct box {\n    int w;\n    int h;\n};\nstruct unused {\n    int z;\n};\nint perimeter(const "
                                        "struct box *b);\nint unrelated(int q);\n#endif\n");
    file_written(root, "src/s/use.c", "#include \"shape.h\"\n#define SCALE 2\nstatic int twice(int v)\n{\n    return v * SCALE;\n}\nint "
                                      "measure(struct box *b, int n)\n{\n    int total = 0;\n    int other = 1;\n    for (int i = 0; i < n; "
                                      "i++)\n    {\n        total += AREA(b[i].w, b[i].h) +\n                 perimeter(&b[i]) *\n         "
                                      "        SIDES;\n        total += twice(i);\n    }\n    return total + other;\n}\n");
    tree_walked_fresh(root);
}

// the slice of `path`'s lines `first` to `last` with `openers`, from `text` where it is not NULL, in s_out, asserted cut
static void slice_cut(const char *text, unsigned long long size, unsigned int first, unsigned int last, const unsigned int *openers)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_symbols_path_text.slice, CliSymbolsPathTextCfg, .path = "src/s/use.c", .text = text, .size = size, .first = first, .last = last, .opener = openers, .openers = 2u, .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
}

// one line of a window's slice written as `path:line:text`, `-:-` for a bracket the slice closes
static void window_line_recorded(const CliSliceLineCfg *args)
{
    VerbaLinea *const out = (VerbaLinea *)args->context;
    if (args->path == NULL)
    {
        mmgr_line_put(out, "-:-");
    }
    else
    {
        mmgr_line_put(out, args->path);
        mmgr_line_ch(out, ':');
        mmgr_line_decimal(out, args->line, 1u);
    }
    mmgr_line_ch(out, ':');
    // a line of the fixture is below 2^32 bytes
    mmgr_line_put_n(out, args->text, (size_t)args->length);
    mmgr_line_ch(out, '\n');
}

// a window written after its slice's lines as `window first-last openers` and each opener's line
static int window_recorded(const CliSliceWindowCfg *args)
{
    VerbaLinea *const out = (VerbaLinea *)args->context;
    mmgr_line_put(out, "window ");
    mmgr_line_decimal(out, args->first, 1u);
    mmgr_line_ch(out, '-');
    mmgr_line_decimal(out, args->last, 1u);
    mmgr_line_put(out, " openers");
    for (unsigned int at = 0u; at < args->openers; at += 1u)
    {
        mmgr_line_ch(out, ' ');
        mmgr_line_decimal(out, args->opener[at], 1u);
    }
    mmgr_line_ch(out, '\n');
    return 1;
}

// a file of two windows cut into its slices in one walk, in s_out, once for the process: the second is inside an else's region inside a function, the brace of an
// inactive region above it no opener of it
static void slices_ran(void)
{
    static char s_slices_out[1u << 18u];
    static char s_use[8192];
    char root[CLI_PATH_MAX_LEN];
    char why[256];
    if (!s_slices_ran)
    {
        VerbaLinea use = {s_use, sizeof(s_use), 0ull};
        VerbaLinea out = {s_slices_out, sizeof(s_slices_out), 0ull};
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        s_slices_ran = 1;
        s_tree = TREE_SLICES;
        cli_unity_folder("symbols_slices", root);
        file_written(root, "src/w/shape.h", "int area(int w);\n");
        mmgr_line_put(&use, "#include \"shape.h\"\n#if 0\nstatic void gone(void) {\n#endif\nint helper(int v)\n{\n    return "
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
        file_written(root, "src/w/use.c", s_use);
        tree_walked_fresh(root);
        s_slices_cut = EMBED_CALL(cli_symbols_path_text_size_context_err_msg.slices_each, CliSymbolsPathTextSizeContextErrMsgCfg, .path = "src/w/use.c", .text = NULL, .size = 0ull, .height = 64u, .each_slice_line = window_line_recorded, .window = window_recorded, .context = &out, .err_msg = &err_msg);
        (void)mmgr_line_finish(&out);
    }
    memor.cpy(&(MemoriaCfg){.dst = s_out, .src = s_slices_out, .bytes = sizeof(s_slices_out)});
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_pythons_declarations_are_read(void)
{
    assert_declarations("x.py",
                        "import os\nLIMIT = 3\nlimit = 4\nX: int = 5\nclass Page:\n    def read(self):\n        pass\nasync def "
                        "go():\n    if a == b:\n        pass\n",
                        "LIMIT constant 1|X constant 3|Page class 4|read function 5|go function 7");
}

void test_rusts_declarations_are_read(void)
{
    assert_declarations("x.rs",
                        "pub(crate) fn alpha() {}\npub struct Beta;\nconst GAMMA: u32 = 1;\nmacro_rules! delta {\n}\npub async "
                        "unsafe fn epsilon() {}\nmod zeta;\nimpl Beta {\nextern \"C\" fn eta() {}\nstatic mut THETA: u8 = 0;\n",
                        "alpha function 0|Beta class 1|GAMMA constant 2|delta function 3|epsilon function 5|zeta module 6|eta "
                        "function 8|THETA constant 9");
}

void test_a_scripts_declarations_are_read(void)
{
    assert_declarations("x.js",
                        "export async function load(path) {\nclass Editor {\n  paint() {\n    if (x) {\n  }\nconst STEP = "
                        "4;\nexport const go = (a) => a;\nlet run = async () => 1;\n",
                        "load function 0|Editor class 1|paint method 2|STEP constant 5|go function 6|run function 7");
}

void test_cs_declarations_are_read(void)
{
    assert_declarations("x.c",
                        "#define LIMIT 4\n#  define TWICE(x) ((x) * 2)\nstruct grid {\ntypedef enum shade\nstatic int "
                        "count_cells(const struct grid *g) {\nint scale(int a,\n  return helper(1);\nint value = make(2);\nvoid "
                        "declared(int);\nif (x) {\n",
                        "LIMIT constant 0|TWICE constant 1|grid class 2|shade class 3|count_cells function 4|scale function 5|declared "
                        "function 8");
}

void test_a_shells_declarations_are_read(void)
{
    assert_declarations("x.sh", "build() {\nfunction clean {\necho hi\n", "build function 0|clean function 1");
}

void test_powershells_declarations_are_read(void)
{
    assert_declarations("x.ps1", "Function Get-Thing {\n", "Get-Thing function 0");
}

void test_tomls_tables_are_read(void)
{
    assert_declarations("x.toml", "[package]\n[[bin]]\nname = 1\n", "package module 0|bin module 1");
}

void test_markdowns_headings_are_read_none_in_code(void)
{
    assert_declarations("x.md", "# Title\n```\n# not this\n```\n## Part two ##\n", "Title heading 0|Part two heading 4");
}

void test_texs_sections_are_read(void)
{
    assert_declarations("x.tex", "\\section*{Start}\n\\subsection[short]{Long}\n", "Start heading 0|Long heading 1");
}

void test_scores_rank_the_name_itself_first(void)
{
    const long long exact = score_of("paint", "paint");
    const long long begins = score_of("paint", "paintStrip");
    const long long contains = score_of("paint", "repaint");
    const long long initials = score_of("gtf", "go_to_file");
    const long long letters = score_of("pnt", "paint");
    TEST_ASSERT_GREATER_THAN_INT64(begins, exact);
    TEST_ASSERT_GREATER_THAN_INT64(contains, begins);
    TEST_ASSERT_GREATER_THAN_INT64(initials, contains);
    TEST_ASSERT_GREATER_THAN_INT64(letters, initials);
}

void test_a_words_start_ranks_over_letters(void)
{
    TEST_ASSERT_GREATER_THAN_INT64(score_of("pnt", "paint"), score_of("gtf", "goToFile"));
}

void test_a_query_that_names_none_does_not_answer(void)
{
    TEST_ASSERT_EQUAL_INT64(-1ll, score_of("xyz", "paint"));
}

void test_the_reading_reports_how_far_it_has_gone(void)
{
    stop_ran();
    TEST_ASSERT_TRUE(s_stop_seen);
    TEST_ASSERT_EQUAL_UINT64(3000ull, s_stop_total);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(s_stop_total, s_stop_done);
}

void test_the_reading_stopped_ends_and_is_no_longer_the_first_reading(void)
{
    stop_ran();
    TEST_ASSERT_TRUE(s_stop_ended);
    TEST_ASSERT_FALSE(s_stop_unwalked);
}

void test_what_a_stopped_reading_read_stays_and_no_further(void)
{
    stop_ran();
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_stop_found);
    TEST_ASSERT_LESS_THAN_UINT(3000u, s_stop_found);
}

void test_go_to_symbol_answers_from_what_a_stopped_reading_read(void)
{
    stop_ran();
    TEST_ASSERT_EQUAL_UINT(1u, s_stop_answered);
}

void test_the_tree_is_read_and_a_file_read_again_once_it_changes(void)
{
    static const char *const s_again[1] = {"a.py"};
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char text[512];
    const CliJson *json = NULL;
    int reading = 0;
    s_tree = TREE_NONE;
    cli_unity_folder("symbols_tree", root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "a.py", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "def first():\n    pass\n", .size = 22ull));
    tree_walked_fresh(root);
    unsigned int found = symbols_found("first", 10u, &json, &reading);
    VerbaLinea line = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = found, .out = &line, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT("[{\"name\":\"first\",\"kind\":\"function\",\"path\":\"a.py\",\"line\":0,\"text\":\"def first():\"}]", text, line.at);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 20u);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = "\ndef second():\n    pass\n", .size = 24ull));
    EMBED_CALL(cli_symbols_root.tree_branches_walk, CliSymbolsRootCfg, .root = root, .paths = s_again, .count = 1u);
    found = symbols_found("first", 10u, &json, &reading);
    TEST_ASSERT_EQUAL_UINT(0u, json->node[found].count);
    found = symbols_found("second", 10u, &json, &reading);
    line.at = 0ull;
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = found, .index = 0u), .name = "line"), .out = &line, .indent = 0u, .sorted = 0);
    CLI_TEST_ASSERT_TEXT("1", text, line.at);
}

void test_a_calls_declarations_are_listed_in_order_of_fit_with_argument_types(void)
{
    signature_tree();
    assert_signature("use.c", "static void run(unsigned int count)\n{\n    const char *name = \"x\";\n    total(count, name", 3u, 21u,
                     "{\"name\":\"total\",\"active\":1,\"arguments\":[{\"text\":\"count\",\"type\":\"unsigned int\"},{\"text\":"
                     "\"name\",\"type\":\"char*\"}],\"declarations\":[{\"path\":\"lib.c\",\"line\":1,\"label\":\"int total(int "
                     "first, const char *second)\",\"params\":[[10,19],[21,39]],\"types\":[\"int\",\"char*\"],\"count\":true,"
                     "\"variadic\":false,\"differs\":[0]},{\"path\":\"lib.c\",\"line\":2,\"label\":\"static int total(int "
                     "first, const char *second)\",\"params\":[[17,26],[28,46]],\"types\":[\"int\",\"char*\"],\"count\":true,"
                     "\"variadic\":false,\"differs\":[0]},{\"path\":\"other.c\",\"line\":0,\"label\":\"int total(int first)\","
                     "\"params\":[[10,19]],\"types\":[\"int\"],\"count\":false,\"variadic\":false,\"differs\":[0]}],"
                     "\"reading\":false}",
                     0);
}

void test_a_bracket_in_a_string_is_not_counted_and_a_names_type_is_read_from_the_tree(void)
{
    signature_tree();
    assert_signature("use.c", "    total(LIMIT, \"(\" ", 0u, 21u,
                     "\"arguments\":[{\"text\":\"LIMIT\",\"type\":\"number\"},{\"text\":\"\\\"(\\\"\",\"type\":"
                     "\"string\"}],\"declarations\":[{\"path\":\"lib.c\",\"line\":1",
                     1);
}

void test_literals_fit_the_parameters_they_are_compared_against(void)
{
    signature_tree();
    assert_signature("use.c", "    total(LIMIT, \"(\" ", 0u, 21u, "\"differs\":[]},{\"path\":\"lib.c\",\"line\":2", 1);
}

void test_pythons_call_is_compared_against_its_own_languages_declarations_alone(void)
{
    signature_tree();
    assert_signature("use.py", "def run(count: int):\n    label = \"y\"\n    total(count, label", 2u, 22u,
                     "{\"name\":\"total\",\"active\":1,\"arguments\":[{\"text\":\"count\",\"type\":\"int\"},{\"text\":\"label\","
                     "\"type\":\"string\"}],\"declarations\":[{\"path\":\"lib.py\",\"line\":0,\"label\":\"def total(first: int, "
                     "second=\\\"x\\\")\",\"params\":[[10,20],[22,32]],\"types\":[\"int\",\"string\"],\"count\":true,"
                     "\"variadic\":false,\"differs\":[]}],\"reading\":false}",
                     0);
}

void test_a_cursor_in_a_bracket_that_opens_no_call_has_no_signature(void)
{
    signature_tree();
    assert_signature("use.py", "x = [total(1)]\n", 0u, 5u, "null", 0);
}

void test_a_call_of_a_function_its_own_file_defines_on_one_line_is_answered(void)
{
    signature_tree();
    assert_signature("served.c", "int add(int a, int b) { return a + b; }\n\nint main(void) {\n  return add(1, 2)", 3u, 16u, "\"declarations\":[{\"path\":\"served.c\",\"line\":0,\"label\":\"int add(int a, int b)\"", 1);
}

void test_a_macro_redefined_under_a_condition_the_files_own_define_makes_true(void)
{
    assert_effect("src/a/use.c", 2u, "LIMIT", "{\"name\":\"LIMIT\",\"path\":\"src/two.h\",\"line\":2,\"text\":\"16\"}");
}

void test_a_macro_under_a_condition_its_folders_define_makes_true(void)
{
    assert_effect("src/a/more.c", 2u, "LIMIT", "{\"name\":\"LIMIT\",\"path\":\"src/one.h\",\"line\":3,\"text\":\"8\"}");
}

void test_a_macro_on_the_other_arm_where_no_define_makes_a_condition_true(void)
{
    assert_effect("tools/t.c", 1u, "LIMIT", "{\"name\":\"LIMIT\",\"path\":\"src/one.h\",\"line\":6,\"text\":\"4\"}");
}

void test_a_define_every_entry_under_a_folder_gives_is_the_folders(void)
{
    assert_effect("src/a/use.c", 2u, "FAST", "{\"name\":\"FAST\",\"path\":\"src\",\"line\":null,\"text\":\"1\"}");
}

void test_a_define_the_folders_other_entries_do_not_give_is_the_files(void)
{
    assert_effect("src/a/use.c", 2u, "LEVEL", "{\"name\":\"LEVEL\",\"path\":\"src/a/use.c\",\"line\":null,\"text\":\"2\"}");
}

void test_a_declaration_in_effect_is_the_one_in_the_active_region(void)
{
    assert_effect("src/a/more.c", 2u, "speed", "{\"name\":\"speed\",\"path\":\"src/one.h\",\"line\":4,\"text\":\"int speed(void);\"}");
    assert_effect("tools/t.c", 1u, "speed", "{\"name\":\"speed\",\"path\":\"src/one.h\",\"line\":7,\"text\":\"int speed(int slow);\"}");
}

void test_a_name_nothing_defines_has_no_effect(void)
{
    assert_effect("tools/t.c", 1u, "none_such", "null");
}

void test_a_macro_called_with_arguments_in_a_condition_an_argument_octal(void)
{
    assert_effect("src/c/calls.c", 17u, "CHOSEN", "{\"name\":\"CHOSEN\",\"path\":\"src/c/calls.c\",\"line\":3,\"text\":\"1\"}");
}

void test_two_arguments_joined_into_the_name_of_a_macro_whose_value_is_hexadecimal(void)
{
    assert_effect("src/c/calls.c", 17u, "JOINED", "{\"name\":\"JOINED\",\"path\":\"src/c/calls.c\",\"line\":6,\"text\":\"1\"}");
}

void test_the_arguments_past_the_parameters_are_passed_on(void)
{
    assert_effect("src/c/calls.c", 17u, "PAST", "{\"name\":\"PAST\",\"path\":\"src/c/calls.c\",\"line\":9,\"text\":\"1\"}");
}

void test_a_define_of_a_file_included_before_the_first_line_is_in_effect(void)
{
    assert_effect("src/c/calls.c", 17u, "BEFORE", "{\"name\":\"BEFORE\",\"path\":\"src/c/calls.c\",\"line\":12,\"text\":\"1\"}");
}

void test_an_argument_is_substituted_as_its_leaves_and_a_database_define_is_called_with_arguments(void)
{
    assert_effect("src/c/calls.c", 17u, "ORDER", "{\"name\":\"ORDER\",\"path\":\"src/c/calls.c\",\"line\":15,\"text\":\"1\"}");
}

void test_a_define_of_an_include_folder_outside_the_tree_is_found_under_its_mount(void)
{
    assert_effect("src/c/calls.c", 17u, "OUTSIDE", "{\"name\":\"OUTSIDE\",\"path\":\"@symbols_effect_inc/out.h\",\"line\":0,\"text\":\"0x2A\"}");
}

void test_a_define_the_database_gives_called_with_arguments_is_its_folders(void)
{
    assert_effect("src/c/calls.c", 17u, "GIVEN", "{\"name\":\"GIVEN\",\"path\":\"src/c\",\"line\":null,\"text\":\"v+1\"}");
}

void test_a_slice_writes_the_definitions_the_openers_the_lines_and_the_closing_braces(void)
{
    slice_tree();
    slice_cut(NULL, 0ull, 13u, 15u, s_slice_openers);
    TEST_ASSERT_EQUAL_STRING("{\"lines\":[{\"path\":\"src/s/shape.h\",\"line\":2,\"text\":\"#include <stddef.h>\"},{\"path\":\"src/s/"
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
                             s_out);
}

void test_lines_whose_last_is_inside_a_statement_run_on_to_its_end(void)
{
    slice_tree();
    slice_cut(NULL, 0ull, 12u, 13u, s_slice_openers);
    CLI_TEST_ASSERT_CONTAINS("\"line\":14,\"text\":\"                 SIDES;\"", s_out);
    CLI_TEST_ASSERT_NOT_CONTAINS("\"line\":15", s_out);
}

void test_the_text_the_editor_stores_replaces_the_disks(void)
{
    static const char s_edited[] = "#include \"shape.h\"\n#define SCALE 2\nstatic int twice(int v)\n{\n    return v * SCALE;\n}\nint "
                                   "measure(struct "
                                   "box *b, int n)\n{\n    int total = 0;\n    int other = 1;\n    for (int i = 0; i < n; i++)\n    {\n        "
                                   "total "
                                   "+= AREA(b[i].w, b[i].h) +\n                 perimeter(&b[i]) *\n                 SIDES;\n        total += "
                                   "twice(i) * 2;\n    }\n    return total + other;\n}\n";
    slice_tree();
    slice_cut(s_edited, sizeof(s_edited) - 1u, 15u, 15u, s_slice_openers);
    CLI_TEST_ASSERT_CONTAINS("\"line\":15,\"text\":\"        total += twice(i) * 2;\"", s_out);
}

void test_what_the_editors_text_declares_is_on_its_own_lines_not_the_disks(void)
{
    static const char s_moved[] = "\n#include \"shape.h\"\n#define SCALE 2\nstatic int twice(int v)\n{\n    return v * SCALE;\n}\nint "
                                  "measure(struct "
                                  "box *b, int n)\n{\n    int total = 0;\n    int other = 1;\n    for (int i = 0; i < n; i++)\n    {\n        "
                                  "total += AREA(b[i].w, b[i].h) +\n                 perimeter(&b[i]) *\n                 SIDES;\n        total "
                                  "+= twice(i);\n    }\n    return total + other;\n}\n";
    static const unsigned int s_moved_openers[2] = {7u, 11u};
    slice_tree();
    slice_cut(s_moved, sizeof(s_moved) - 1u, 16u, 16u, s_moved_openers);
    CLI_TEST_ASSERT_CONTAINS("\"line\":3,\"text\":\"static int twice(int v)\"", s_out);
}

void test_a_slice_cut_before_the_tree_reads_its_files_writes_the_include_and_the_files_own_leaves(void)
{
    slice_tree();
    cli_tree_forgotten();
    s_tree = TREE_NONE;
    slice_cut(NULL, 0ull, 13u, 15u, s_slice_openers);
    CLI_TEST_ASSERT_CONTAINS("\"line\":0,\"text\":\"#include \\\"shape.h\\\"\"", s_out);
    CLI_TEST_ASSERT_CONTAINS("\"line\":2,\"text\":\"static int twice(int v)\"", s_out);
}

void test_a_files_slices_are_cut_in_one_walk_each_windows_openers_the_regions_its_first_is_in(void)
{
    slices_ran();
    TEST_ASSERT_TRUE(s_slices_cut);
    const char *const second = cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL, .other = "window 0-63 openers\n", .other_cap = CLI_CELLS_POOL});
    TEST_ASSERT_NOT_NULL(second);
    CLI_TEST_ASSERT_CONTAINS("window 64-127 openers 8 14\n", second);
    CLI_TEST_ASSERT_NOT_CONTAINS("window 128", second);
}

void test_the_window_inside_the_else_writes_its_functions_head_and_what_its_names_follow_to(void)
{
    slices_ran();
    const char *const second = cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL, .other = "window 0-63 openers\n", .other_cap = CLI_CELLS_POOL});
    TEST_ASSERT_NOT_NULL(second);
    CLI_TEST_ASSERT_CONTAINS("src/w/use.c:8:int measure(int n)\nsrc/w/use.c:9:{\nsrc/w/use.c:10:    int total = 0;\nsrc/w/use.c:14:else {\n", second);
    CLI_TEST_ASSERT_CONTAINS("src/w/use.c:70:        total = area(total);\n", second);
    CLI_TEST_ASSERT_CONTAINS("src/w/shape.h:0:int area(int w);\n", second);
    CLI_TEST_ASSERT_CONTAINS("src/w/use.c:4:int helper(int v)\n", second);
}

void test_a_functions_usages_are_its_declaration_definition_and_each_call_a_static_set_aside(void)
{
    TEST_ASSERT_EQUAL_INT(1, usages_of("src/u/use.c", 3u, 13u, 1));
    assert_usages(s_area_usages, 4u);
}

void test_a_macros_usages_are_its_define_and_each_name_of_it_in_effect(void)
{
    TEST_ASSERT_EQUAL_INT(1, usages_of("src/u/use.c", 3u, 24u, 1));
    assert_usages(s_sides_usages, 3u);
}

void test_a_parameters_usages_are_in_the_function_that_declares_it_alone(void)
{
    TEST_ASSERT_EQUAL_INT(1, usages_of("src/u/area.c", 3u, 17u, 1));
    assert_usages(s_parameter_usages, 2u);
}

void test_a_locals_usages_are_in_its_function_a_parameter_of_the_same_name_set_aside(void)
{
    TEST_ASSERT_EQUAL_INT(1, usages_of("src/u/use.c", 4u, 11u, 1));
    assert_usages(s_local_usages, 2u);
}

void test_a_functions_usages_in_the_file_requested_alone(void)
{
    TEST_ASSERT_EQUAL_INT(1, usages_of("src/u/use.c", 4u, 16u, 0));
    assert_usages(s_area_usages, 2u);
}

void test_a_place_that_names_no_name_has_no_usages(void)
{
    TEST_ASSERT_EQUAL_INT(0, usages_of("src/u/use.c", 3u, 2u, 1));
    TEST_ASSERT_EQUAL_STRING("", s_out);
}

void test_a_functions_definition_from_a_call_is_the_headers_declaration_met_first(void)
{
    TEST_ASSERT_EQUAL_INT(1, definition_of("src/u/use.c", 3u, 13u));
    assert_usages(s_area_usages + 2, 1u);
}

void test_a_macros_definition_is_its_define_in_effect(void)
{
    TEST_ASSERT_EQUAL_INT(1, definition_of("src/u/use.c", 3u, 24u));
    assert_usages(s_sides_usages + 1, 1u);
}

void test_a_parameters_definition_is_its_name_in_the_functions_head(void)
{
    TEST_ASSERT_EQUAL_INT(1, definition_of("src/u/area.c", 3u, 17u));
    assert_usages(s_parameter_usages, 1u);
}

void test_a_locals_definition_is_its_declaration(void)
{
    TEST_ASSERT_EQUAL_INT(1, definition_of("src/u/use.c", 4u, 11u));
    assert_usages(s_local_usages, 1u);
}

void test_a_place_that_names_no_name_has_no_definition(void)
{
    TEST_ASSERT_EQUAL_INT(0, definition_of("src/u/use.c", 3u, 2u));
    TEST_ASSERT_EQUAL_STRING("", s_out);
}

void test_a_word_is_completed_by_the_declarations_it_begins(void)
{
    assert_completed("ar", "area function 2");
}

void test_a_word_is_completed_by_the_defines_in_effect_it_begins(void)
{
    assert_completed("S", "SHAPE_H constant 1|SIDES constant 3");
}

void test_a_word_is_completed_by_the_parameters_of_the_function_the_place_is_in(void)
{
    assert_completed("n", "n parameter 1");
}

void test_a_word_is_completed_by_the_locals_declared_above_the_place(void)
{
    assert_completed("w", "w local 3");
}

void test_each_calls_argument_has_a_hint_of_its_parameters_name(void)
{
    assert_hints(1u, 4u, "3:17:w|4:20:w");
}

void test_the_lines_of_no_call_have_no_hints(void)
{
    assert_hints(1u, 2u, "");
}

void test_the_spans_around_a_place_are_its_word_statement_region_and_the_whole_text(void)
{
    assert_spans("src/u/use.c", s_use_text, 3u, 13u, "3:12-3:16|3:4-3:28|2:1-5:0|2:0-5:1|1:0-5:1|0:0-6:0");
}

void test_the_spans_around_an_argument_include_the_calls_brackets_each_span_once(void)
{
    assert_spans("src/u/use.c", s_use_text, 3u, 17u, "3:17-3:18|3:16-3:19|3:4-3:28|2:1-5:0|2:0-5:1|1:0-5:1|0:0-6:0");
}

void test_the_spans_around_a_place_in_a_regions_head_are_its_brackets_the_head_and_the_region(void)
{
    assert_spans("src/u/use.c", s_use_text, 1u, 14u, "1:14-1:15|1:10-1:15|1:9-1:16|1:0-1:16|1:0-5:1|0:0-6:0");
}

void test_the_spans_of_a_block_by_indent_end_at_its_last_line_of_code(void)
{
    assert_spans("src/u/calc.py", s_indent_text, 1u, 9u, "1:8-1:12|1:4-1:19|0:13-2:12|0:12-2:12|0:0-2:12|0:0-5:0");
}

void test_the_bracket_that_ends_at_a_place_pairs_with_its_close(void)
{
    assert_pair("src/u/use.c", s_use_text, 1u, 10u, "1:9-1:15");
}

void test_a_bracket_that_starts_at_a_place_pairs_where_none_ends_at_it(void)
{
    assert_pair("src/u/use.c", s_use_text, 1u, 15u, "1:15-1:9");
}

void test_a_regions_brackets_pair_lines_apart(void)
{
    assert_pair("src/u/use.c", s_use_text, 2u, 1u, "2:0-5:0");
}

void test_a_place_at_no_bracket_has_no_pair(void)
{
    assert_pair("src/u/use.c", s_use_text, 3u, 2u, "");
}

void test_a_bracket_in_a_string_is_skipped_by_a_pair(void)
{
    assert_pair("src/u/pair.c", "f(a, \")\");\n", 0u, 2u, "0:1-0:8");
}

void test_a_region_a_mark_of_no_letters_closes_has_no_pair(void)
{
    assert_pair("src/u/calc.py", s_indent_text, 0u, 13u, "");
}

void test_a_lines_function_is_the_one_its_body_is_in(void)
{
    assert_calls(0, "src/u/use.c", 3u, "src/u/use.c:1:4-9:total[]");
}

void test_a_functions_callers_each_have_their_calls_a_statics_caller_set_aside(void)
{
    assert_calls(1, "src/u/area.c", 1u, "src/u/use.c:1:4-9:total[3:12,4:15]");
}

void test_a_functions_callees_are_each_its_definition_in_effect_with_the_calls(void)
{
    assert_calls(2, "src/u/use.c", 1u, "src/u/shape.h:2:4-8:area[3:12,4:15]");
}

void test_a_line_of_a_block_by_indent_is_in_its_function(void)
{
    assert_calls(0, "src/u/calc.py", 4u, "src/u/calc.py:3:4-9:total[]");
}

void test_a_block_by_indents_callers_each_have_their_calls(void)
{
    assert_calls(1, "src/u/calc.py", 0u, "src/u/calc.py:3:4-9:total[4:8,5:11]");
}

void test_a_block_by_indents_callees_are_listed(void)
{
    assert_calls(2, "src/u/calc.py", 3u, "src/u/calc.py:0:4-8:area[4:8,5:11]");
}
