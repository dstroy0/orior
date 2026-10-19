// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// testing_found.c: a tree's tests found. A file's tests are read from its facts: a class's body is the lines after its
// own indented deeper than it, blank lines among them, and a function in a class's body is a method of it where no
// function of the class holds it; a function outside every class is a test where no function holds it. The tree's
// Python files are its files named as the rules name a test file, outside the folders not searched and inside the
// rules' folders where they name any, in the order of their paths; its Rust tests follow them
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../inspect/inspect.h"
#include "../text/json_tree.h"
#include "cargo_tests.h"

#include "../text/text.h"

// the largest file read for its tests, the most lines of it, and the most tests of one file
#define CLI_TESTS_LARGEST (2ull << 20u)
#define CLI_TESTS_LINES ((2u << 20u) + 2u)
#define CLI_TESTS_OF_FILE 65536u

// the folders a search for tests does not go into: environments, what a build writes, and caches
static const char *const s_not_searched[10] = {"venv",        ".venv", "env",  "node_modules", "site-packages",
                                               "__pycache__", "build", "dist", ".tox",         ".nox"};

// no function or class of a test
#define CLI_TESTS_NONE 0xffffffffu

// A search's room, a cell taken with the tests' lock and given back with it: the facts a file is read into, its lines'
// starts, and each class's body's last line; a file's tests, each its function and its class, CLI_TESTS_NONE where it
// has none; and the tree's files, the rules, the Rust tests and a file read
typedef struct
{
    CliInspectImport imports[16384u];
    CliInspectClass classes[16384u];
    CliInspectFunction functions[65536u];
    CliInspectFinding findings[65536u];
    CliInspectFix fixes[65536u];
    CliInspectEdit edits[65536u];
    CliInspectName names[262144u];
    char text[16u << 20u];
    CliFacts facts;
    unsigned int line_start[CLI_TESTS_LINES];
    unsigned int body_end[16384u];
    unsigned int test_function[CLI_TESTS_OF_FILE];
    unsigned int test_class[CLI_TESTS_OF_FILE];
    CliTestRules rules;
    CliCargoTests cargo;
    unsigned char read[CLI_TESTS_LARGEST];
} CliTestsRoom;

static CliTestsRoom *s_room;
static CliLock s_tests_lock = CLI_LOCK_INIT;

// the indent of the line at `line` of `size` bytes of `text`, `lines` of them
static unsigned int found_indent(const char *text, unsigned long long size, unsigned int line, unsigned int lines,
                                 int *blank)
{
    const unsigned long long start = s_room->line_start[line];
    const unsigned long long end = (line + 1u < lines) ? s_room->line_start[line + 1u] - 1u : size;
    unsigned long long at = start;
    while ((at < end) &&
           ((text[at] == ' ') || (text[at] == '\t') || (text[at] == '\r') || (text[at] == '\f') || (text[at] == '\v')))
    {
        at += 1u;
    }
    *blank = (at == end);
    return (unsigned int)(at - start);
}

// each class's body's last line, from its own line, the lines of `size` bytes of `text`
static void found_bodies(const char *text, unsigned long long size)
{
    unsigned int lines = 0u;
    for (unsigned long long at = 0ull; (at < size) && (lines < CLI_TESTS_LINES); lines += 1u)
    {
        s_room->line_start[lines] = (unsigned int)at;
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = (size_t)(size - at)});
        at = (newline != NULL) ? (unsigned long long)(newline - text) + 1u : size;
    }
    for (unsigned int at = 0u; at < s_room->facts.classes; at += 1u)
    {
        const unsigned int start = s_room->facts.class_of[at].from.line;
        int blank = 0;
        const unsigned int own = (start < lines) ? found_indent(text, size, start, lines, &blank) : 0u;
        unsigned int end = start;
        for (unsigned int line = start + 1u; line < lines; line += 1u)
        {
            const unsigned int indent = found_indent(text, size, line, lines, &blank);
            if (!blank && (indent <= own))
            {
                break;
            }
            end = line;
        }
        s_room->body_end[at] = end;
    }
}

// the class whose body holds `line`, CLI_TESTS_NONE where none does
static unsigned int found_held_by(unsigned int line)
{
    for (unsigned int at = 0u; at < s_room->facts.classes; at += 1u)
    {
        if ((s_room->facts.class_of[at].from.line < line) && (line <= s_room->body_end[at]))
        {
            return at;
        }
    }
    return CLI_TESTS_NONE;
}

// a text of the facts
static const char *found_text(CliInspectText text)
{
    return s_room->facts.text + text.at;
}

// whether the class at `class_at` collects its tests: it inherits a `TestCase`, or is named as the rules name a test
// class and sets no `__init__`; and whether it inherits one
static int found_collected(unsigned int class_at, const CliTestRules *rules, int *test_case)
{
    const CliInspectClass *const class_of = &s_room->facts.class_of[class_at];
    *test_case = 0;
    for (unsigned int at = 0u; at < class_of->bases; at += 1u)
    {
        const CliInspectText base = s_room->facts.name[class_of->base_first + at].name;
        *test_case |=
            (base.size >= 8u) &&
            (memor.cmp(&(MemoriaCfg){.src = found_text(base) + base.size - 8u, .other = "TestCase", .bytes = 8u}) == 0);
    }
    if (*test_case)
    {
        return 1;
    }
    for (unsigned int at = 0u; at < s_room->facts.functions; at += 1u)
    {
        const CliInspectFunction *const one = &s_room->facts.function[at];
        const unsigned int other =
            (one->name.size == 8u) &&
                    (memor.cmp(&(MemoriaCfg){.src = found_text(one->name), .other = "__init__", .bytes = 8u}) == 0)
                ? found_held_by(one->from.line)
                : CLI_TESTS_NONE;
        if ((other != CLI_TESTS_NONE) && (s_room->facts.class_of[other].name.size == class_of->name.size) &&
            (memor.cmp(&(MemoriaCfg){.src = found_text(s_room->facts.class_of[other].name),
                                     .other = found_text(class_of->name),
                                     .bytes = class_of->name.size}) == 0))
        {
            return 0;
        }
    }
    return cli_test_named(rules, &rules->classes, found_text(class_of->name), class_of->name.size);
}

// whether a function other than the one at `function` holds it, one starting after the line `after` where `bounded`
static int found_nested(unsigned int function, int bounded, unsigned int after)
{
    const CliInspectFunction *const inner = &s_room->facts.function[function];
    for (unsigned int at = 0u; at < s_room->facts.functions; at += 1u)
    {
        const CliInspectFunction *const outer = &s_room->facts.function[at];
        if ((at != function) && (!bounded || (outer->first > after)) && (outer->first < inner->first) &&
            (inner->last <= outer->last))
        {
            return 1;
        }
    }
    return 0;
}

// the tests of the facts read, kept in the order of their lines, those of one line in the order of their functions:
// their count
static unsigned int found_tests(const CliTestRules *rules)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < s_room->facts.functions) && (count < CLI_TESTS_OF_FILE); at += 1u)
    {
        const CliInspectFunction *const function = &s_room->facts.function[at];
        const unsigned int class_at = found_held_by(function->from.line);
        const char *const name = found_text(function->name);
        const unsigned int size = function->name.size;
        int keep = 0;
        if (class_at != CLI_TESTS_NONE)
        {
            int test_case = 0;
            const int collected = found_collected(class_at, rules, &test_case);
            const int method = !found_nested(at, 1, s_room->facts.class_of[class_at].from.line);
            const int test_name =
                test_case ? ((size >= 4u) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "test", .bytes = 4u}) == 0))
                          : cli_test_named(rules, &rules->functions, name, size);
            keep = collected && method && test_name;
        }
        else
        {
            keep = !found_nested(at, 0, 0u) && cli_test_named(rules, &rules->functions, name, size);
        }
        if (keep)
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

// the tests of `file`, its `size` bytes of `text`, each handed to `each`, in the taken room
static void found_in(const char *file, const char *text, unsigned long long size, const CliTestRules *rules,
                     CliTestEach each, void *context)
{
    cli_facts_begun(&s_room->facts, s_room->imports, 16384u, s_room->classes, 16384u, s_room->functions, 65536u,
                    s_room->findings, 65536u, s_room->fixes, 65536u, s_room->edits, 65536u, s_room->names, 262144u,
                    s_room->text, sizeof(s_room->text));
    if (!cli_inspect_facts("python", text, size, &s_room->facts))
    {
        return;
    }
    found_bodies(text, size);
    const unsigned int count = found_tests(rules);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliInspectFunction *const function = &s_room->facts.function[s_room->test_function[at]];
        const unsigned int class_at = s_room->test_class[at];
        char id[CLI_PATH_LONGEST * 2u];
        CliLine line = {id, sizeof(id), 0ull};
        cli_line_text(&line, file);
        cli_line_text(&line, "::");
        if (class_at != CLI_TESTS_NONE)
        {
            cli_line_counted(&line, found_text(s_room->facts.class_of[class_at].name),
                             s_room->facts.class_of[class_at].name.size);
            cli_line_text(&line, "::");
        }
        cli_line_counted(&line, found_text(function->name), function->name.size);
        if (!cli_line_closed(&line))
        {
            continue;
        }
        CliTest test = {id, file,
                        (class_at != CLI_TESTS_NONE) ? found_text(s_room->facts.class_of[class_at].name) : NULL,
                        found_text(function->name), function->from.line};
        each(context, &test);
    }
}

// whether the path `file` is one the rules search for tests: a Python file outside the folders not searched, named as
// a test file, and inside the rules' folders where they name any
static int found_searched(const char *file, const CliTestRules *rules)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL});
    if ((length < 3u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = file + length - 3u, .other = ".py", .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    for (const char *part = file; *part != '\0';)
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
    const char *const slash =
        cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/');
    const char *const base = (slash != NULL) ? slash + 1 : file;
    if (!cli_test_named(rules, &rules->files, base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL})))
    {
        return 0;
    }
    int inside = (rules->paths.count == 0u);
    for (unsigned int at = 0u; !inside && (at < rules->paths.count); at += 1u)
    {
        const char *const place = cli_test_rule_value(rules, &rules->paths, at);
        size_t place_size = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL});
        inside = (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = place, .cap = CLI_CELLS_POOL}));
        while ((place_size > 0u) && (place[place_size - 1u] == '/'))
        {
            place_size -= 1u;
        }
        inside = inside || ((length > place_size) &&
                            (memor.cmp(&(MemoriaCfg){.src = file, .other = place, .bytes = place_size}) == 0) &&
                            (file[place_size] == '/'));
    }
    return inside;
}

// the Python tests of the tree at `root`, under the finder's lock
static void found_python(const char *root, CliTestEach each, void *context)
{
    cli_test_rules_of(root, &s_room->rules);
    const CliFiles *const files = cli_files_held(root);
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = cli_files_at(files, index);
        if (!found_searched(file, &s_room->rules))
        {
            continue;
        }
        char path[CLI_PATH_LONGEST];
        cli_full(root, file, path);
        unsigned long long size = 0ull;
        if (!cli_file_size(path, &size) || (size > CLI_TESTS_LARGEST))
        {
            continue;
        }
        const unsigned long long read = qry_member_file_read(path, s_room->read, sizeof(s_room->read));
        const char *const text = (const char *)s_room->read;
        if ((read != QRY_MEMBER_UNREAD) && cli_utf8_whole(text, read))
        {
            found_in(file, text, read, &s_room->rules, each, context);
        }
    }
    cli_files_let(files);
}

// the tests' lock taken and the room a cell under it: 1, or 0 with the lock given back where no cell is left
static int found_held(void)
{
    cli_lock_take(&s_tests_lock);
    s_room = cli_cell_taken(sizeof(*s_room), "a search for tests");
    if (s_room == NULL)
    {
        cli_lock_give(&s_tests_lock);
        return 0;
    }
    return 1;
}

// the room given back and the tests' lock with it
static void found_released(void)
{
    cli_cell_given(s_room);
    s_room = NULL;
    cli_lock_give(&s_tests_lock);
}

void cli_tests_in(const char *file, const char *text, unsigned long long size, const CliTestRules *rules,
                  CliTestEach each, void *context)
{
    if (found_held())
    {
        found_in(file, text, size, rules, each, context);
        found_released();
    }
}

void cli_tests_python_found(const char *root, CliTestEach each, void *context)
{
    if (found_held())
    {
        found_python(root, each, context);
        found_released();
    }
}

void cli_tests_found(const char *root, CliTestEach each, void *context)
{
    if (!found_held())
    {
        return;
    }
    found_python(root, each, context);
    cli_cargo_found(root, &s_room->cargo);
    for (unsigned int at = 0u; at < s_room->cargo.count; at += 1u)
    {
        each(context, &s_room->cargo.found[at].test);
    }
    found_released();
}
