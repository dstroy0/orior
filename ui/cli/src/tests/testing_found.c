// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// testing_found.c: a tree's tests found. A file's tests are read from its facts: a class's body is the lines after its
// own indented deeper than it, blank lines among them, and a function in a class's body is a method of it where no
// function of the class contains it; a function outside every class is a test where no function contains it. The tree's
// Python files are its files named as the rules name a test file, outside the folders not searched and inside the
// rules' folders where they name any, in the order of their paths; its Rust tests follow them
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../inspect/inspect.h"
#include "../text/json_tree.h"
#include "cargo_tests.h"

#include "../text/text.h"

// the folders a search for tests does not go into: environments, what a build writes, and caches
static const char *const s_not_searched[10] = {"venv",        ".venv", "env",  "node_modules", "site-packages",
                                               "__pycache__", "build", "dist", ".tox",         ".nox"};

// no function or class of a test
#define CLI_TESTS_NONE 0xffffffffu

// A search's room, under the tests' lock: the rules and the Rust tests; and a file's part, one cell sized to the file
// by a walk that counts its facts and given back as the file's tests are handed on: the facts, its lines' starts, each
// class's body's last line, and its tests, each its function and its class, CLI_TESTS_NONE where it has none
typedef struct
{
    CliFacts facts;
    unsigned int *line_start;
    unsigned int lines;
    unsigned int *body_end;
    unsigned int *test_function;
    unsigned int *test_class;
    CliTestRules rules;
    CliCargoTests cargo;
} CliTestsRoom;

static CliTestsRoom s_room_storage;
static CliTestsRoom *s_room;
static CliLock s_tests_lock = CLI_LOCK_INIT;

typedef struct
{
    unsigned char **at;
    unsigned long long bytes;
    const char *text;
    unsigned long long size;
    unsigned int line;
    unsigned int lines;
    int *blank;
    CliInspectText text_inspect_text;
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
// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *found_part(const TestingFoundCtx *args)
{
    unsigned char **at = args->at;
    void *const part = *at;
    *at += (args->bytes + 7ull) & ~7ull;
    return part;
}

// the lines of `size` bytes of `text`: one more than its newlines, the last line's start after a final newline counted
// as a line's
EMBED_INLINE unsigned int found_lines(const TestingFoundCtx *args)
{
    unsigned int lines = 1u;
    for (const char *at = args->text; at < args->text + args->size; lines += 1u)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){
            .src = at, .val = '\n', .bytes = (size_t)(args->size - (unsigned long long)(at - args->text))});
        if (newline == NULL)
        {
            break;
        }
        at = newline + 1;
    }
    return lines;
}

// a file's part taken for the counts of a walk over its `size` bytes of `text` and the room's pointers set into it:
// the cell, or NULL where the file is not read or no cell is left
EMBED_INLINE void *found_part_take(const TestingFoundCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    CliFacts counted;
    EMBED_CALL(cli_inspect_facts_language.facts_counting, CliInspectFactsLanguageCfg, .facts = &counted);
    if (!EMBED_CALL(cli_inspect_facts_language.facts, CliInspectFactsLanguageCfg, .language = "python", .text = text,
                    .size = size, .facts = &counted))
    {
        return NULL;
    }
    const unsigned int lines = EMBED_CALL(found_lines, TestingFoundCtx, .text = text, .size = size);
    const unsigned long long bytes =
        (counted.imports * sizeof(CliInspectImport)) + (counted.classes * sizeof(CliInspectClass)) +
        (counted.functions * sizeof(CliInspectFunction)) + (counted.findings * sizeof(CliInspectFinding)) +
        (counted.fixes * sizeof(CliInspectFix)) + (counted.edits * sizeof(CliInspectEdit)) +
        (counted.names * sizeof(CliInspectName)) + counted.used + ((unsigned long long)lines * sizeof(unsigned int)) +
        ((unsigned long long)counted.classes * sizeof(unsigned int)) +
        (2ull * counted.functions * sizeof(unsigned int)) + (12ull * 8ull);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_tests_cells);
    if (cell == NULL)
    {
        return NULL;
    }
    unsigned char *at = cell;
    CliInspectImport *const imports =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.imports * sizeof(CliInspectImport));
    CliInspectClass *const classes =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.classes * sizeof(CliInspectClass));
    CliInspectFunction *const functions =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.functions * sizeof(CliInspectFunction));
    CliInspectFinding *const findings =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.findings * sizeof(CliInspectFinding));
    CliInspectFix *const fixes =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.fixes * sizeof(CliInspectFix));
    CliInspectEdit *const edits =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.edits * sizeof(CliInspectEdit));
    CliInspectName *const names =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.names * sizeof(CliInspectName));
    char *const pool = EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = counted.used);
    s_room->line_start =
        EMBED_CALL(found_part, TestingFoundCtx, .at = &at, .bytes = (unsigned long long)lines * sizeof(unsigned int));
    s_room->lines = lines;
    s_room->body_end = EMBED_CALL(found_part, TestingFoundCtx, .at = &at,
                                  .bytes = (unsigned long long)counted.classes * sizeof(unsigned int));
    s_room->test_function = EMBED_CALL(found_part, TestingFoundCtx, .at = &at,
                                       .bytes = (unsigned long long)counted.functions * sizeof(unsigned int));
    s_room->test_class = EMBED_CALL(found_part, TestingFoundCtx, .at = &at,
                                    .bytes = (unsigned long long)counted.functions * sizeof(unsigned int));
    EMBED_CALL(cli_inspect_facts_table.facts_begin, CliInspectFactsCfg, .facts = &s_room->facts, .imports = imports,
               .imports_most = counted.imports, .classes = classes, .classes_most = counted.classes,
               .functions = functions, .functions_most = counted.functions, .findings = findings,
               .findings_most = counted.findings, .fixes = fixes, .fixes_most = counted.fixes, .edits = edits,
               .edits_most = counted.edits, .names = names, .names_most = counted.names, .text_out = pool,
               .text_most = counted.used);
    return cell;
}

// the indent of the line at `line` of `size` bytes of `text`, `lines` of them
EMBED_INLINE unsigned int found_indent(const TestingFoundCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    int *blank = args->blank;
    const unsigned long long start = s_room->line_start[args->line];
    const unsigned long long end = (args->line + 1u < args->lines) ? s_room->line_start[args->line + 1u] - 1u : size;
    unsigned long long at = start;
    at = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = text, .at = at, .size = end,
                    .set = " \t\r\f\v");
    *blank = (at == end);
    return (unsigned int)(at - start);
}

// each class's body's last line, from its own line, the lines of `size` bytes of `text`
EMBED_INLINE void found_bodies(const TestingFoundCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int lines = 0u;
    for (unsigned long long at = 0ull; (at < size) && (lines < s_room->lines); lines += 1u)
    {
        s_room->line_start[lines] = (unsigned int)at;
        at = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size).next;
    }
    for (unsigned int at = 0u; at < s_room->facts.classes; at += 1u)
    {
        const unsigned int start = s_room->facts.class_of[at].from.line;
        int blank = 0;
        const unsigned int own = (start < lines) ? EMBED_CALL(found_indent, TestingFoundCtx, .text = text, .size = size,
                                                              .line = start, .lines = lines, .blank = &blank)
                                                 : 0u;
        unsigned int end = start;
        for (unsigned int line = start + 1u; line < lines; line += 1u)
        {
            const unsigned int indent = EMBED_CALL(found_indent, TestingFoundCtx, .text = text, .size = size,
                                                   .line = line, .lines = lines, .blank = &blank);
            if (!blank && (indent <= own))
            {
                break;
            }
            end = line;
        }
        s_room->body_end[at] = end;
    }
}

// the class whose body contains `line`, CLI_TESTS_NONE where none does
EMBED_INLINE unsigned int found_class_get(const TestingFoundCtx *args)
{
    for (unsigned int at = 0u; at < s_room->facts.classes; at += 1u)
    {
        if ((s_room->facts.class_of[at].from.line < args->line) && (args->line <= s_room->body_end[at]))
        {
            return at;
        }
    }
    return CLI_TESTS_NONE;
}

// a text of the facts
EMBED_INLINE const char *found_text(const TestingFoundCtx *args)
{
    return s_room->facts.text + args->text_inspect_text.at;
}

// whether the class at `class_at` collects its tests: it inherits a `TestCase`, or is named as the rules name a test
// class and sets no `__init__`; and whether it inherits one
EMBED_INLINE int found_collect(const TestingFoundCtx *args)
{
    const CliTestRules *rules = args->rules;
    int *test_case = args->test_case;
    const CliInspectClass *const class_of = &s_room->facts.class_of[args->class_at];
    *test_case = 0;
    for (unsigned int at = 0u; at < class_of->bases; at += 1u)
    {
        const CliInspectText base = s_room->facts.name[class_of->base_first + at].name;
        *test_case |= (base.size >= 8u) &&
                      (memor.cmp(&(MemoriaCfg){
                           .src = EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = base) + base.size - 8u,
                           .other = "TestCase",
                           .bytes = 8u}) == 0);
    }
    if (*test_case)
    {
        return 1;
    }
    for (unsigned int at = 0u; at < s_room->facts.functions; at += 1u)
    {
        const CliInspectFunction *const one = &s_room->facts.function[at];
        const unsigned int other =
            (one->name.size == 8u) && (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(found_text, TestingFoundCtx,
                                                                                 .text_inspect_text = one->name),
                                                               .other = "__init__",
                                                               .bytes = 8u}) == 0)
                ? EMBED_CALL(found_class_get, TestingFoundCtx, .line = one->from.line)
                : CLI_TESTS_NONE;
        if ((other != CLI_TESTS_NONE) && (s_room->facts.class_of[other].name.size == class_of->name.size) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = EMBED_CALL(found_text, TestingFoundCtx,
                                                 .text_inspect_text = s_room->facts.class_of[other].name),
                               .other = EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = class_of->name),
                               .bytes = class_of->name.size}) == 0))
        {
            return 0;
        }
    }
    return EMBED_CALL(cli_testing_rules_rule_size.test_by_name, CliTestingRulesRuleSizeCfg, .rules_read = rules,
                      .rule = &rules->classes,
                      .name = EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = class_of->name),
                      .size = class_of->name.size);
}

// whether a function other than the one at `function` contains it, one starting after the line `after` where `bounded`
EMBED_INLINE int found_nest(const TestingFoundCtx *args)
{
    const CliInspectFunction *const inner = &s_room->facts.function[args->function];
    for (unsigned int at = 0u; at < s_room->facts.functions; at += 1u)
    {
        const CliInspectFunction *const outer = &s_room->facts.function[at];
        if ((at != args->function) && (!args->bounded || (outer->first > args->after)) &&
            (outer->first < inner->first) && (inner->last <= outer->last))
        {
            return 1;
        }
    }
    return 0;
}

// the tests of the facts read, listed in the order of their lines, those of one line in the order of their functions:
// their count
EMBED_INLINE unsigned int found_tests(const TestingFoundCtx *args)
{
    const CliTestRules *rules = args->rules;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_room->facts.functions; at += 1u)
    {
        const CliInspectFunction *const function = &s_room->facts.function[at];
        const unsigned int class_at = EMBED_CALL(found_class_get, TestingFoundCtx, .line = function->from.line);
        const char *const name = EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = function->name);
        const unsigned int size = function->name.size;
        int is_test = 0;
        if (class_at != CLI_TESTS_NONE)
        {
            int test_case = 0;
            const int collected = EMBED_CALL(found_collect, TestingFoundCtx, .class_at = class_at, .rules = rules,
                                             .test_case = &test_case);
            const int method = !EMBED_CALL(found_nest, TestingFoundCtx, .function = at, .bounded = 1,
                                           .after = s_room->facts.class_of[class_at].from.line);
            const int test_name =
                test_case ? ((size >= 4u) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "test", .bytes = 4u}) == 0))
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
            s_room->test_function[count] = at;
            s_room->test_class[count] = class_at;
            count += 1u;
        }
    }
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        const unsigned int function = s_room->test_function[at];
        const unsigned int class_at = s_room->test_class[at];
        const unsigned int line = s_room->facts.function[function].from.line;
        unsigned int place = at;
        while ((place > 0u) && (s_room->facts.function[s_room->test_function[place - 1u]].from.line > line))
        {
            s_room->test_function[place] = s_room->test_function[place - 1u];
            s_room->test_class[place] = s_room->test_class[place - 1u];
            place -= 1u;
        }
        s_room->test_function[place] = function;
        s_room->test_class[place] = class_at;
    }
    return count;
}

// the tests of `file`, its `size` bytes of `text`, each handed to `each`, in a part taken for the file and given
// back after
static void found_in(const TestingFoundCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    void *const part = EMBED_CALL(found_part_take, TestingFoundCtx, .text = text, .size = size);
    if (part == NULL)
    {
        return;
    }
    if (!EMBED_CALL(cli_inspect_facts_language.facts, CliInspectFactsLanguageCfg, .language = "python", .text = text,
                    .size = size, .facts = &s_room->facts))
    {
        g_cli_cells->persistent_buf_release(part, &g_cli_tests_cells);
        return;
    }
    EMBED_CALL(found_bodies, TestingFoundCtx, .text = text, .size = size);
    const unsigned int count = found_tests(args);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliInspectFunction *const function = &s_room->facts.function[s_room->test_function[at]];
        const unsigned int class_at = s_room->test_class[at];
        char id[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea line = {id, sizeof(id), 0ull};
        mmgr_line_put(&line, args->file);
        mmgr_line_put(&line, "::");
        if (class_at != CLI_TESTS_NONE)
        {
            mmgr_line_put_n(
                &line,
                EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = s_room->facts.class_of[class_at].name),
                s_room->facts.class_of[class_at].name.size);
            mmgr_line_put(&line, "::");
        }
        mmgr_line_put_n(&line, EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = function->name),
                        function->name.size);
        if (!mmgr_line_finish(&line))
        {
            continue;
        }
        CliTest test = {
            id, args->file,
            (class_at != CLI_TESTS_NONE)
                ? EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = s_room->facts.class_of[class_at].name)
                : NULL,
            EMBED_CALL(found_text, TestingFoundCtx, .text_inspect_text = function->name), function->from.line};
        EMBED_CALL(args->each, CliTestEachCfg, .context = args->context, .test = &test);
    }
    g_cli_cells->persistent_buf_release(part, &g_cli_tests_cells);
}

// whether the path `file` is one the rules search for tests: a Python file outside the folders not searched, named as
// a test file, and inside the rules' folders where they name any
EMBED_INLINE int found_search(const TestingFoundCtx *args)
{
    const CliTestRules *rules = args->rules;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL});
    if ((length < 3u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->file + length - 3u, .other = ".py", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    for (const char *part = args->file; *part != '\0';)
    {
        const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = '/'});
        const size_t size = (slash != NULL) ? (size_t)(slash - part)
                                            : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        for (unsigned int at = 0u; at < 10u; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = s_not_searched[at], .cap = CLI_CELLS_POOL}) == size) &&
                (memor.cmp(&(MemoriaCfg){.src = part, .other = s_not_searched[at], .bytes = size}) == 0))
            {
                return 0;
            }
        }
        part += size + ((slash != NULL) ? 1u : 0u);
    }
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->file,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}), .byte = '/');
    const char *const base = (slash != NULL) ? slash + 1 : args->file;
    if (!EMBED_CALL(cli_testing_rules_rule_size.test_by_name, CliTestingRulesRuleSizeCfg, .rules_read = rules,
                    .rule = &rules->files, .name = base,
                    .size = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    int inside = (rules->paths.count == 0u);
    for (unsigned int at = 0u; !inside && (at < rules->paths.count); at += 1u)
    {
        const char *const place = EMBED_CALL(cli_testing_rules_rule.test_rule_value, CliTestingRulesRuleCfg,
                                             .rules_read = rules, .rule = &rules->paths, .at = at);
        size_t place_size = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL});
        inside = (cellul.eq(&(CatenaFinitaCfg){.src = args->file, .other = place, .cap = CLI_CELLS_POOL}));
        place_size = (size_t)EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = place,
                                        .at = place_size, .set = "/");
        inside = inside || ((length > place_size) &&
                            (memor.cmp(&(MemoriaCfg){.src = args->file, .other = place, .bytes = place_size}) == 0) &&
                            (args->file[place_size] == '/'));
    }
    return inside;
}

// the Python tests of the tree at `root`, under the finder's lock
EMBED_INLINE void found_python(const TestingFoundCtx *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_testing_rules.test_rules_get, CliTestingRulesCfg, .root = root, .rules = &s_room->rules);
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = index);
        if (!EMBED_CALL(found_search, TestingFoundCtx, .file = file, .rules = &s_room->rules))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = file, .joined = path);
        unsigned long long size = 0ull;
        if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size))
        {
            continue;
        }
        unsigned char *const bytes = g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_tests_cells);
        if (bytes == NULL)
        {
            continue;
        }
        const unsigned long long read = qry_member_file_read(path, bytes, size + 1ull);
        const char *const text = (const char *)bytes;
        if ((read != QRY_MEMBER_UNREAD) &&
            EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = text, .size = read))
        {
            EMBED_CALL(found_in, TestingFoundCtx, .file = file, .text = text, .size = read, .rules = &s_room->rules,
                       .each = args->each, .context = args->context);
        }
        g_cli_cells->persistent_buf_release(bytes, &g_cli_tests_cells);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}

// the tests' lock taken and the retained room set under it: 1
static int found_room_lock(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_tests_lock);
    s_room = &s_room_storage;
    return 1;
}

// the room let go and the tests' lock with it
static void found_release(void)
{
    s_room = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_tests_lock);
}

void cli_tests_in(const CliTestingRulesTextCfg *args)
{
    if (found_room_lock())
    {
        EMBED_CALL(found_in, TestingFoundCtx, .file = args->file, .text = args->text, .size = args->size,
                   .rules = args->rules_read, .each = args->each, .context = args->context);
        found_release();
    }
}

void cli_tests_python_find(const CliTestingRootCfg *args)
{
    if (found_room_lock())
    {
        EMBED_CALL(found_python, TestingFoundCtx, .root = args->root, .each = args->each, .context = args->context);
        found_release();
    }
}

void cli_tests_find(const CliTestingRootCfg *args)
{
    const char *root = args->root;
    if (!found_room_lock())
    {
        return;
    }
    EMBED_CALL(found_python, TestingFoundCtx, .root = root, .each = args->each, .context = args->context);
    EMBED_CALL(cli_cargo_tests_table.cargo_find, CliCargoTestsCfg, .root = root, .tests = &s_room->cargo);
    for (unsigned int at = 0u; at < s_room->cargo.count; at += 1u)
    {
        EMBED_CALL(args->each, CliTestEachCfg, .context = args->context, .test = &s_room->cargo.found[at].test);
    }
    EMBED_CALL(cli_cargo_tests_table.free, CliCargoTestsCfg, .tests = &s_room->cargo);
    found_release();
}
