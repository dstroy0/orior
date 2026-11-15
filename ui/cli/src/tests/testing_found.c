// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// testing_found.c: a tree's tests found. A file's tests are read off the declarations its grammar plugin's walk names:
// a class's body is the lines after its own indented deeper than it, blank lines among them, and a function in a
// class's body is a method of it where no function of the class contains it; a function outside every class is a
// test where no function contains it. The tree's Python files are its files named as the rules name a test file,
// outside the folders not searched and inside the rules' folders where they name any, in the order of their paths;
// its Rust tests follow them
#include "../code/symbols_internal.h"
#include "../command_line/pool.h"
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "cargo_tests.h"

#include "../text/text.h"

// the folders a search for tests does not go into: environments, what a build writes, and caches
static const char *const s_not_searched[10] = {"venv",        ".venv", "env",  "node_modules", "site-packages",
                                               "__pycache__", "build", "dist", ".tox",         ".nox"};

// no function or class of a test
#define TESTS_NONE 0xffffffffu

// the test frameworks' own names: the base a test case inherits, the method that builds a class, and the start of a
// test case's test methods' names
#define CLI_TESTS_CASE_BASE "TestCase"
#define CLI_TESTS_BUILDER "__init__"
#define CLI_TESTS_CASE_START "test"

// the declarations a file's cell holds first
#define CLI_TESTS_DECLARED 64u

// A declaration a file names: 1 in `class_of` where it declares a class, 0 a function; where its name's bytes start in
// the text and how many; its line; its body's last line; and, for a class, 1 where it inherits a test case's base
typedef struct
{
    int class_of;
    unsigned long long at;
    unsigned int size;
    unsigned int line;
    unsigned int body_end;
    int test_case;
} TestsDeclared;

// A search's work state, under the tests' lock: the rules and the Rust tests; and a file's part: the text, its
// declarations in a cell grown as they come, their count and capacity, 1 in `refused` where the cells refused one, the
// leaves the walk saved, its lines' starts and their count, and its tests, each its function and its class,
// TESTS_NONE where it has none
typedef struct
{
    const char *text;
    TestsDeclared *declared;
    unsigned int count;
    unsigned int capacity;
    int refused;
    CliLeafRead read;
    unsigned int *line_start;
    unsigned int lines;
    unsigned int *test_function;
    unsigned int *test_class;
    CliTestRules rules;
    CliCargoTests cargo;
} CliTestsWorkState;

static CliTestsWorkState s_response_storage;
static CliTestsWorkState *s_work_state;

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned int line;
    int *blank;
    unsigned int at;
    unsigned int class_at;
    const CliTestRules *rules;
    int *test_case;
    unsigned int function;
    int bounded;
    unsigned int after;
    const char *file;
    CliTestEach each;
    void *context;
    const char *root;
} TestingFoundCtx;
// the lines of `size` bytes of `text`: one more than its newlines, the last line's start after a final newline counted
// as a line's
EMBED_INLINE unsigned int found_lines(const TestingFoundCtx *args)
{
    unsigned int lines = 1u;
    for (const char *at = args->text; at < args->text + args->size; lines += 1u)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){
            .src = at, .val = '\n', .bytes = (embed_word)(args->size - (unsigned long long)(at - args->text))});
        if (newline == NULL)
        {
            break;
        }
        at = newline + 1;
    }
    return lines;
}

// the declarations' cell grown to hold one more: 1, or 0 where the cells refuse it
EMBED_INLINE int found_declared_alloc(void)
{
    CliTestsWorkState *const work = s_work_state;
    const unsigned int capacity = (work->capacity == 0u) ? CLI_TESTS_DECLARED : (work->capacity * 2u);
    TestsDeclared *const grown =
        g_cli_guard->temporary_buf_alloc((unsigned long long)capacity * sizeof(TestsDeclared), g_cli_tests_owner);
    if (grown == NULL)
    {
        return 0;
    }
    if (work->count != 0u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = grown, .src = work->declared, .bytes = (embed_word)work->count * sizeof(TestsDeclared)});
    }
    g_cli_guard->persistent_buf_release(work->declared, g_cli_tests_owner);
    work->declared = grown;
    work->capacity = capacity;
    return 1;
}

// a declaration the walk names put on the file's declarations, where it declares a function or a class
static void found_declared(const CliDeclaredCfg *args)
{
    CliTestsWorkState *const work = (CliTestsWorkState *)args->context;
    const int class_of = cellul.eq(&(CatenaFinitaCfg){.src = args->type, .other = "class", .cap = CLI_POOL_BYTES});
    if (!class_of && !cellul.eq(&(CatenaFinitaCfg){.src = args->type, .other = "function", .cap = CLI_POOL_BYTES}))
    {
        return;
    }
    if ((work->count == work->capacity) && !found_declared_alloc())
    {
        work->refused = 1;
        return;
    }
    // the name is in the copy the walk reads, which holds each line at the text's own bytes
    work->declared[work->count] = (TestsDeclared){
        class_of, (unsigned long long)(args->name - work->read.first), (unsigned int)args->length, args->line, args->line, 0};
    work->count += 1u;
}

// the indent of the line at `line` of `size` bytes of `text`, and whether the line is blank
EMBED_INLINE unsigned int found_indent(const TestingFoundCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    int *blank = args->blank;
    const unsigned long long start = s_work_state->line_start[args->line];
    const unsigned long long end =
        (args->line + 1u < s_work_state->lines) ? s_work_state->line_start[args->line + 1u] - 1u : size;
    unsigned long long at = start;
    at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = at, .size = end,
                    .set = " \t\r\f\v");
    *blank = (at == end);
    return (unsigned int)(at - start);
}

// each line's start, and each declaration's body's last line, from its own line, the lines of `size` bytes of `text`
EMBED_INLINE void found_bodies(const TestingFoundCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int lines = 0u;
    for (unsigned long long at = 0ULL; (at < size) && (lines < s_work_state->lines); lines += 1u)
    {
        s_work_state->line_start[lines] = (unsigned int)at;
        at = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size).next;
    }
    for (unsigned int at = 0u; at < s_work_state->count; at += 1u)
    {
        const unsigned int start = s_work_state->declared[at].line;
        int blank = 0;
        const unsigned int own = (start < lines) ? EMBED_CALL(found_indent, TestingFoundCtx, .text = text, .size = size,
                                                              .line = start, .blank = &blank)
                                                 : 0u;
        unsigned int end = start;
        for (unsigned int line = start + 1u; line < lines; line += 1u)
        {
            const unsigned int indent =
                EMBED_CALL(found_indent, TestingFoundCtx, .text = text, .size = size, .line = line, .blank = &blank);
            if (!blank && (indent <= own))
            {
                break;
            }
            end = line;
        }
        s_work_state->declared[at].body_end = end;
    }
}

// each class marked where a name on its line after its own ends in a test case's base
EMBED_INLINE void found_bases(const TestingFoundCtx *args)
{
    const unsigned long long base_size = sizeof(CLI_TESTS_CASE_BASE) - 1u;
    for (unsigned int at = 0u; at < s_work_state->count; at += 1u)
    {
        TestsDeclared *const class_of = &s_work_state->declared[at];
        if (!class_of->class_of)
        {
            continue;
        }
        for (unsigned int leaf = 0u; leaf < s_work_state->read.count; leaf += 1u)
        {
            const CliLeaf *const one = &s_work_state->read.leaf[leaf];
            const unsigned long long size = one->to - one->from;
            if ((one->line == class_of->line) && (one->from > class_of->at) && ((one->is & CLI_LEAF_NAME) != 0u) &&
                (size >= base_size) &&
                (memor.cmp(&(MemoriaCfg){.src = args->text + one->to - base_size,
                                         .other = CLI_TESTS_CASE_BASE,
                                         .bytes = (embed_word)base_size}) == 0))
            {
                class_of->test_case = 1;
            }
        }
    }
}

// the innermost class whose body contains `line`, TESTS_NONE where none does
EMBED_INLINE unsigned int found_class_get(const TestingFoundCtx *args)
{
    unsigned int found = TESTS_NONE;
    for (unsigned int at = 0u; at < s_work_state->count; at += 1u)
    {
        const TestsDeclared *const one = &s_work_state->declared[at];
        if (one->class_of && (one->line < args->line) && (args->line <= one->body_end))
        {
            found = at;
        }
    }
    return found;
}

// whether the declaration at `at` is named `word`
EMBED_INLINE int found_named(const TestingFoundCtx *args)
{
    const TestsDeclared *const one = &s_work_state->declared[args->at];
    const embed_word size = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES});
    return (one->size == size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text + one->at, .other = args->file, .bytes = size}) == 0);
}

// whether the class at `class_at` collects its tests: it inherits a test case's base, or is named as the rules name a
// test class and no function of its body builds it; and whether it inherits one
EMBED_INLINE int found_collect(const TestingFoundCtx *args)
{
    const CliTestRules *rules = args->rules;
    int *test_case = args->test_case;
    const TestsDeclared *const class_of = &s_work_state->declared[args->class_at];
    *test_case = class_of->test_case;
    if (*test_case)
    {
        return 1;
    }
    for (unsigned int at = 0u; at < s_work_state->count; at += 1u)
    {
        if (!s_work_state->declared[at].class_of &&
            EMBED_CALL(found_named, TestingFoundCtx, .text = args->text, .at = at, .file = CLI_TESTS_BUILDER) &&
            (EMBED_CALL(found_class_get, TestingFoundCtx, .line = s_work_state->declared[at].line) == args->class_at))
        {
            return 0;
        }
    }
    return EMBED_CALL(cli_testing_rules_rule_size.test_by_name, CliTestingRulesRuleSizeCfg, .rules_read = rules,
                      .rule = &rules->classes, .name = args->text + class_of->at, .size = class_of->size);
}

// whether a function other than the one at `function` contains it, one on a line after `after` where `bounded`
EMBED_INLINE int found_nest(const TestingFoundCtx *args)
{
    const TestsDeclared *const inner = &s_work_state->declared[args->function];
    for (unsigned int at = 0u; at < s_work_state->count; at += 1u)
    {
        const TestsDeclared *const outer = &s_work_state->declared[at];
        if ((at != args->function) && !outer->class_of && (!args->bounded || (outer->line > args->after)) &&
            (outer->line < inner->line) && (inner->body_end <= outer->body_end))
        {
            return 1;
        }
    }
    return 0;
}

// the tests among the declarations, in the order of their lines: their count
EMBED_INLINE unsigned int found_tests(const TestingFoundCtx *args)
{
    const CliTestRules *rules = args->rules;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_work_state->count; at += 1u)
    {
        const TestsDeclared *const function = &s_work_state->declared[at];
        if (function->class_of)
        {
            continue;
        }
        const unsigned int class_at = EMBED_CALL(found_class_get, TestingFoundCtx, .line = function->line);
        const char *const name = args->text + function->at;
        const unsigned int size = function->size;
        int is_test = 0;
        if (class_at != TESTS_NONE)
        {
            int test_case = 0;
            const int collected = EMBED_CALL(found_collect, TestingFoundCtx, .text = args->text, .class_at = class_at,
                                             .rules = rules, .test_case = &test_case);
            const int method = !EMBED_CALL(found_nest, TestingFoundCtx, .function = at, .bounded = 1,
                                           .after = s_work_state->declared[class_at].line);
            const int test_name =
                test_case ? ((size >= sizeof(CLI_TESTS_CASE_START) - 1u) &&
                             (memor.cmp(&(MemoriaCfg){.src = name,
                                                      .other = CLI_TESTS_CASE_START,
                                                      .bytes = sizeof(CLI_TESTS_CASE_START) - 1u}) == 0))
                          : EMBED_CALL(cli_testing_rules_rule_size.test_by_name, CliTestingRulesRuleSizeCfg,
                                       .rules_read = rules, .rule = &rules->functions, .name = name, .size = size);
            is_test = collected && method && test_name;
        }
        else
        {
            is_test = !EMBED_CALL(found_nest, TestingFoundCtx, .function = at, .bounded = 0, .after = 0u) &&
                      EMBED_CALL(cli_testing_rules_rule_size.test_by_name, CliTestingRulesRuleSizeCfg,
                                 .rules_read = rules, .rule = &rules->functions, .name = name, .size = size);
        }
        if (is_test)
        {
            s_work_state->test_function[count] = at;
            s_work_state->test_class[count] = class_at;
            count += 1u;
        }
    }
    return count;
}

// the file's leaves and declarations read off the walk, and its lines' starts and tests given cells: 1, or 0 where the
// walk does not read the file or the cells refuse it
EMBED_INLINE int found_read(const TestingFoundCtx *args)
{
    CliTestsWorkState *const work = s_work_state;
    work->text = args->text;
    work->declared = NULL;
    work->count = 0u;
    work->capacity = 0u;
    work->refused = 0;
    work->line_start = NULL;
    CliSymbolsLanguages *const languages = cli_symbols_languages_read();
    const int leaved =
        (languages != NULL) &&
        EMBED_CALL(cli_symbols_path_internal.leaves_declare, CliSymbolsPathCtx, .languages = languages,
                   .path = args->file, .state = "root", .text = args->text, .size = args->size, .read = &work->read,
                   .each = found_declared, .context = work);
    if (languages != NULL)
    {
        g_cli_guard->persistent_buf_release(languages->languages.pair, g_cli_plugins_owner);
    }
    g_cli_guard->persistent_buf_release(languages, g_cli_code_owner);
    if (!leaved || work->refused)
    {
        return 0;
    }
    work->lines = EMBED_CALL(found_lines, TestingFoundCtx, .text = args->text, .size = args->size);
    const unsigned long long bytes = ((unsigned long long)work->lines + (2ULL * work->count)) * sizeof(unsigned int);
    work->line_start = g_cli_guard->temporary_buf_alloc(bytes, g_cli_tests_owner);
    if (work->line_start == NULL)
    {
        return 0;
    }
    work->test_function = work->line_start + work->lines;
    work->test_class = work->test_function + work->count;
    return 1;
}

// the file's cells given back
EMBED_INLINE void found_release(void)
{
    CliTestsWorkState *const work = s_work_state;
    g_cli_guard->persistent_buf_release(work->read.leaf, g_cli_leaves_owner);
    work->read.leaf = NULL;
    work->read.count = 0u;
    work->read.capacity = 0u;
    g_cli_guard->persistent_buf_release(work->declared, g_cli_tests_owner);
    work->declared = NULL;
    work->count = 0u;
    work->capacity = 0u;
    g_cli_guard->persistent_buf_release(work->line_start, g_cli_tests_owner);
    work->line_start = NULL;
}

// the tests of `file`, its `size` bytes of `text`, each passed to `each`, in cells taken for the file and given back
// after
static void found_in(const TestingFoundCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const int read = EMBED_CALL(found_read, TestingFoundCtx, .file = args->file, .text = text, .size = size);
    if (read)
    {
        EMBED_CALL(found_bodies, TestingFoundCtx, .text = text, .size = size);
        EMBED_CALL(found_bases, TestingFoundCtx, .text = text);
        const unsigned int count = EMBED_CALL(found_tests, TestingFoundCtx, .text = text, .rules = args->rules);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const TestsDeclared *const function = &s_work_state->declared[s_work_state->test_function[at]];
            const unsigned int class_at = s_work_state->test_class[at];
            const TestsDeclared *const class_of =
                (class_at != TESTS_NONE) ? &s_work_state->declared[class_at] : NULL;
            char id[CLI_PATH_MAX_LEN * 2u];
            char class_name[CLI_NAME_MAX_LEN];
            char name[CLI_NAME_MAX_LEN];
            VerbaLinea line = {id, sizeof(id), 0ULL};
            VerbaLinea class_line = {class_name, sizeof(class_name), 0ULL};
            VerbaLinea name_line = {name, sizeof(name), 0ULL};
            mmgr_line_put(&line, args->file);
            mmgr_line_put(&line, "::");
            if (class_of != NULL)
            {
                mmgr_line_put_n(&line, text + class_of->at, class_of->size);
                mmgr_line_put(&line, "::");
                mmgr_line_put_n(&class_line, text + class_of->at, class_of->size);
            }
            mmgr_line_put_n(&line, text + function->at, function->size);
            mmgr_line_put_n(&name_line, text + function->at, function->size);
            if (!mmgr_line_finish(&line) || !mmgr_line_finish(&name_line) ||
                ((class_of != NULL) && !mmgr_line_finish(&class_line)))
            {
                continue;
            }
            CliTest test = {id, args->file, (class_of != NULL) ? class_name : NULL, name, function->line};
            EMBED_CALL(args->each, CliTestEachCfg, .context = args->context, .test = &test);
        }
    }
    found_release();
}

// whether the path `file` is one the rules search for tests: a Python file outside the folders not searched, named as
// a test file, and inside the rules' folders where they name any
EMBED_INLINE int found_search(const TestingFoundCtx *args)
{
    const CliTestRules *rules = args->rules;
    const embed_word length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES});
    if ((length < 3u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->file + length - 3u, .other = ".py", .cap = CLI_POOL_BYTES})))
    {
        return 0;
    }
    for (const char *part = args->file; *part != '\0';)
    {
        const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES, .byte = '/'});
        const embed_word size = (slash != NULL) ? (embed_word)(slash - part)
                                                : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES});
        for (unsigned int at = 0u; at < 10u; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = s_not_searched[at], .cap = CLI_POOL_BYTES}) == size) &&
                (memor.cmp(&(MemoriaCfg){.src = part, .other = s_not_searched[at], .bytes = size}) == 0))
            {
                return 0;
            }
        }
        part += size + ((slash != NULL) ? 1u : 0u);
    }
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->file,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_POOL_BYTES}), .byte = '/');
    const char *const base = (slash != NULL) ? slash + 1 : args->file;
    if (!EMBED_CALL(cli_testing_rules_rule_size.test_by_name, CliTestingRulesRuleSizeCfg, .rules_read = rules,
                    .rule = &rules->files, .name = base,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_POOL_BYTES})))
    {
        return 0;
    }
    int inside = (rules->paths.count == 0u);
    for (unsigned int at = 0u; !inside && (at < rules->paths.count); at += 1u)
    {
        const char *const place = EMBED_CALL(cli_testing_rules_rule.test_rule_value, CliTestingRulesRuleCfg,
                                             .rules_read = rules, .rule = &rules->paths, .at = at);
        embed_word position_size = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_POOL_BYTES});
        inside = (cellul.eq(&(CatenaFinitaCfg){.src = args->file, .other = place, .cap = CLI_POOL_BYTES}));
        position_size = (embed_word)EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                               .text = place, .at = position_size, .set = "/");
        inside =
            inside || ((length > position_size) &&
                       (memor.cmp(&(MemoriaCfg){.src = args->file, .other = place, .bytes = position_size}) == 0) &&
                       (args->file[position_size] == '/'));
    }
    return inside;
}

// the Python tests of the tree at `root`, under the finder's lock
EMBED_INLINE void found_python(const TestingFoundCtx *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_testing_rules.test_rules_get, CliTestingRulesCfg, .root = root, .rules = &s_work_state->rules);
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = index);
        if (!EMBED_CALL(found_search, TestingFoundCtx, .file = file, .rules = &s_work_state->rules))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = file, .joined = path);
        unsigned long long size = 0ULL;
        if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size))
        {
            continue;
        }
        unsigned char *const bytes = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_tests_owner);
        if (bytes == NULL)
        {
            continue;
        }
        const unsigned long long read = qry_member_file_read(path, bytes, size + 1ULL);
        const char *const text = (const char *)bytes;
        if ((read != QRY_MEMBER_UNREAD) &&
            EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = text, .size = read))
        {
            EMBED_CALL(found_in, TestingFoundCtx, .file = file, .text = text, .size = read,
                       .rules = &s_work_state->rules, .each = args->each, .context = args->context);
        }
        g_cli_guard->persistent_buf_release(bytes, g_cli_tests_owner);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}

// the tests' lock taken and the retained work state set under it: 1
static int found_capacity_reserve(void)
{
    s_work_state = &s_response_storage;
    return 1;
}

void cli_tests_in(const CliTestingRulesTextCfg *args)
{
    if (found_capacity_reserve())
    {
        EMBED_CALL(found_in, TestingFoundCtx, .file = args->file, .text = args->text, .size = args->size,
                   .rules = args->rules_read, .each = args->each, .context = args->context);
        s_work_state = NULL;
    }
}

void cli_tests_python_find(const CliTestingRootCfg *args)
{
    if (found_capacity_reserve())
    {
        EMBED_CALL(found_python, TestingFoundCtx, .root = args->root, .each = args->each, .context = args->context);
        s_work_state = NULL;
    }
}

void cli_tests_find(const CliTestingRootCfg *args)
{
    const char *root = args->root;
    if (!found_capacity_reserve())
    {
        return;
    }
    EMBED_CALL(found_python, TestingFoundCtx, .root = root, .each = args->each, .context = args->context);
    EMBED_CALL(cli_cargo_tests_table.cargo_find, CliCargoTestsCfg, .root = root, .tests = &s_work_state->cargo);
    for (unsigned int at = 0u; at < s_work_state->cargo.count; at += 1u)
    {
        EMBED_CALL(args->each, CliTestEachCfg, .context = args->context, .test = &s_work_state->cargo.found[at].test);
    }
    CliCargoPiece *piece = s_work_state->cargo.pieces;
    while (piece != NULL)
    {
        CliCargoPiece *const next = piece->next;
        g_cli_guard->persistent_buf_release(piece, g_cli_cargo_owner);
        piece = next;
    }
    g_cli_guard->persistent_buf_release(s_work_state->cargo.found, g_cli_cargo_owner);
    *(&s_work_state->cargo) = (CliCargoTests){NULL, 0u, 0u, NULL, 0ULL, 0};
    s_work_state = NULL;
}
