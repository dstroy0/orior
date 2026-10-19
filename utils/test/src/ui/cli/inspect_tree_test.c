// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_test.c: what the tree's index says across its files: a function's callers and callees are found in
// Python and JavaScript; an import that leads back round is found in both; and a method a mixin supplies is found, and
// one no class sets is not
#include "../../../../../ui/cli/src/inspect/inspect_tree.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

// What a walk of the index gave: each name, or each path and message, and the count of the calls of the first
typedef struct
{
    char item[64][1024];
    unsigned int count;
    unsigned int first_sites;
} TreeHeard;

static TreeHeard s_heard;

// the files of `files`, `count` of them, each a path and its text, set in an index made empty
static void tree_of(const char *const (*files)[2], unsigned int count)
{
    cli_inspect_tree_cleared();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const language = cli_inspect_language_of(files[at][0]);
        cli_check((language != NULL) &&
                      cli_inspect_tree_set(files[at][0], language, files[at][1],
                                           cellul.len(&(CatenaFinitaCfg){.src = files[at][1], .cap = CLI_CELLS_POOL})),
                  files[at][0]);
    }
}

// a function reached kept by its name
static void tree_reached(void *context, const char *path, const CliFacts *facts, unsigned int function,
                         const CliInspectSpan *sites, unsigned int count)
{
    (void)context;
    (void)path;
    (void)sites;
    if (s_heard.count < 64u)
    {
        if (s_heard.count == 0u)
        {
            s_heard.first_sites = count;
        }
        memor.cpy(&(MemoriaCfg){
            .dst = s_heard.item[s_heard.count],
            .src = cli_inspect_text(facts->text, facts->function[function].name),
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_inspect_text(facts->text, facts->function[function].name),
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        s_heard.count += 1u;
    }
}

// what was heard, joined by `|`
static const char *tree_heard(void)
{
    static char s_joined[8192];
    CliLine line = {s_joined, sizeof(s_joined), 0ull};
    for (unsigned int at = 0u; at < s_heard.count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, s_heard.item[at]);
    }
    (void)cli_line_closed(&line);
    return s_joined;
}

// the name of the function at `at` of `path`
static const char *tree_name(const char *path, unsigned int at)
{
    s_heard.count = 0u;
    (void)cli_inspect_tree_function(path, at, tree_reached, NULL);
    return tree_heard();
}

// the callers, or the callees, of the function at `at` of `path`, joined by `|`
static const char *tree_calls(const char *path, unsigned int at, int callers)
{
    s_heard.count = 0u;
    s_heard.first_sites = 0u;
    if (callers)
    {
        cli_inspect_tree_callers(path, at, tree_reached, NULL);
    }
    else
    {
        cli_inspect_tree_callees(path, at, tree_reached, NULL);
    }
    return tree_heard();
}

// a function's callers and callees are found in Python and JavaScript
static void tree_hierarchy(void)
{
    static const char *const s_files[4][2] = {
        {"geo/area.py",
         "def square(side):\n    return side * side\n\n\ndef ring(a, b):\n    def inner(x):\n        return "
         "square(x)\n    return square(a) - square(b) + inner(1)\n"},
        {"geo/use.py", "from geo.area import ring\n\n\nclass Shape:\n    def total(self):\n        return ring(2, 1) + "
                       "self.part()\n\n    def part(self):\n        return 1\n"},
        {"web/lib.js",
         "export function twice(x) {\n  return x * 2;\n}\n\nexport const four = () => {\n  return twice(2);\n};\n"},
        {"web/main.js", "import { twice, four } from \"./lib.js\";\nclass App {\n  run() {\n    return twice(four()) + "
                        "this.more();\n  }\n  more() {\n    return 1;\n  }\n}\n"}};
    tree_of(s_files, 4u);
    unsigned int square = 0u;
    unsigned int ring = 0u;
    cli_check(cli_inspect_tree_function_on("geo/area.py", 0u, &square) &&
                  cli_inspect_tree_function_at("geo/area.py", 7u, &ring) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = tree_name("geo/area.py", ring), .other = "ring", .cap = CLI_CELLS_POOL})),
              "a function by the line its name is on, and by a line it holds");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = tree_calls("geo/area.py", square, 1), .other = "ring|inner", .cap = CLI_CELLS_POOL}),
              "a function's callers");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = tree_calls("geo/area.py", ring, 0), .other = "square|inner", .cap = CLI_CELLS_POOL})) &&
                  (s_heard.first_sites == 2u),
              "its callees, both calls of square in ring and not the one inner makes");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = tree_calls("geo/area.py", ring, 1), .other = "total", .cap = CLI_CELLS_POOL}),
              "a caller in another file");
    unsigned int total = 0u;
    cli_check(cli_inspect_tree_function_on("geo/use.py", 4u, &total) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = tree_calls("geo/use.py", total, 0), .other = "ring|part", .cap = CLI_CELLS_POOL})),
              "a method's callees, its own class's with them");
    unsigned int twice = 0u;
    cli_check(cli_inspect_tree_function_on("web/lib.js", 0u, &twice) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = tree_calls("web/lib.js", twice, 1), .other = "four|run", .cap = CLI_CELLS_POOL})),
              "a JavaScript function's callers");
    unsigned int run = 0u;
    cli_check(cli_inspect_tree_function_at("web/main.js", 3u, &run) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = tree_calls("web/main.js", run, 0), .other = "twice|four|more", .cap = CLI_CELLS_POOL})),
              "and a method's callees");
}

// a finding kept as `path` then its message
static void tree_found(void *context, const char *path, const char *finding, unsigned long long size)
{
    (void)context;
    if ((finding == NULL) || (size == 0ull) || (s_heard.count >= 64u))
    {
        return;
    }
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(finding, size, &read);
    CliLine line = {s_heard.item[s_heard.count], sizeof(s_heard.item[0]), 0ull};
    cli_line_text(&line, path);
    cli_line_character(&line, '\t');
    cli_line_text(&line, cli_check_member_text(cli_json_member(json, read, "message"), "value"));
    (void)cli_line_closed(&line);
    s_heard.count += 1u;
}

// the messages found in `path`, joined by `|`
static const char *tree_said(const char *path)
{
    static char s_said[8192];
    CliLine line = {s_said, sizeof(s_said), 0ull};
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_heard.count; at += 1u)
    {
        if (cli_text_same(s_heard.item[at], path, length) && (s_heard.item[at][length] == '\t'))
        {
            cli_line_text(&line, (count > 0u) ? "|" : "");
            cli_line_text(&line, s_heard.item[at] + length + 1u);
            count += 1u;
        }
    }
    (void)cli_line_closed(&line);
    return s_said;
}

// every finding of the index heard
static void tree_findings(void)
{
    s_heard.count = 0u;
    cli_inspect_tree_findings(tree_found, NULL);
}

// an import that leads back round is found in Python and JavaScript
static void tree_cycles(void)
{
    static const char *const s_files[8][2] = {
        {"pkg/__init__.py", ""},
        {"pkg/a.py", "from pkg import b\n\ndef f():\n    from pkg import c\n"},
        {"pkg/b.py", "from . import a\n"},
        {"pkg/c.py", "import pkg.a\n"},
        {"pkg/d.py", "import os\nfrom typing import TYPE_CHECKING\nif TYPE_CHECKING:\n    from pkg import a\n"},
        {"web/x.js", "import { y } from \"./y.js\";\n"},
        {"web/y.js", "export { x } from \"./x\";\n"},
        {"web/z.js", "import { x } from \"./x.js\";\n"}};
    tree_of(s_files, 8u);
    tree_findings();
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = tree_said("pkg/a.py"),
                                           .other = "This import leads back round to the file that makes it, by way "
                                                    "of:\n\n1. `pkg/a.py`\n2. `pkg/b.py`\n3. `pkg/a.py`",
                                           .cap = CLI_CELLS_POOL}),
              "an import that leads back round, named with the files it passes through");
    cli_check(
        (tree_said("pkg/b.py")[0] != '\0') &&
            (cellul.chr(&(CatenaFinitaCfg){.src = tree_said("pkg/b.py"), .cap = CLI_CELLS_POOL, .byte = '|'}) == NULL),
        "found in the other file of the round once");
    cli_check((tree_said("pkg/c.py")[0] == '\0') && (tree_said("pkg/d.py")[0] == '\0'),
              "an import inside a function and one only a type checker makes are not rounds");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = tree_said("web/x.js"),
                                           .other = "This import leads back round to the file that makes it, by way "
                                                    "of:\n\n1. `web/x.js`\n2. `web/y.js`\n3. `web/x.js`",
                                           .cap = CLI_CELLS_POOL}),
              "a JavaScript round, a re-export with them");
    cli_check(tree_said("web/z.js")[0] == '\0', "and a file that only reaches one is not in it");
}

// an attribute a class's family supplies kept as `path|class|attribute`
static void tree_supplied(void *context, const char *path, const char *class_name, const char *attribute)
{
    (void)context;
    if (s_heard.count < 64u)
    {
        CliLine line = {s_heard.item[s_heard.count], sizeof(s_heard.item[0]), 0ull};
        cli_line_text(&line, path);
        cli_line_character(&line, '|');
        cli_line_text(&line, class_name);
        cli_line_character(&line, '|');
        cli_line_text(&line, attribute);
        (void)cli_line_closed(&line);
        s_heard.count += 1u;
    }
}

// whether what was heard holds `wanted`
static int tree_heard_holds(const char *wanted)
{
    for (unsigned int at = 0u; at < s_heard.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_heard.item[at], .other = wanted, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// a method a mixin supplies is found, and one no class sets is not
static void tree_mixins(void)
{
    static const char *const s_files[3][2] = {
        {"shapes/mixins.py",
         "class Saving:\n    def save(self):\n        return self.render() + self.missing()\n\nclass "
         "Drawing:\n    def draw(self):\n        return self.render()\n"},
        {"shapes/square.py",
         "from shapes.mixins import Saving\nimport shapes.mixins as mixins\n\nclass Square(Saving, "
         "mixins.Drawing):\n    def __init__(self):\n        self.side = self.edge = 2\n\n    def "
         "render(self):\n        hook = self.hook() if getattr(self, \"hook\", None) else 0\n        "
         "return self.side * self.edge * self.save() + self.draw() + self.colour + hook\n"},
        {"shapes/outside.py", "from somewhere import Base\n\nclass Loose(Base):\n    def go(self):\n        return "
                              "self.anything()\n"}};
    tree_of(s_files, 3u);
    s_heard.count = 0u;
    cli_inspect_tree_supplied(tree_supplied, NULL);
    cli_check(tree_heard_holds("shapes/mixins.py|Saving|render"),
              "the class a mixin is mixed into supplies its method");
    cli_check(!tree_heard_holds("shapes/mixins.py|Saving|missing"), "and not what none sets");
    tree_findings();
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = tree_said("shapes/mixins.py"),
                                     .other = "`Saving` has no `missing`: neither it, a class it comes from, nor a "
                                              "class that comes from it sets `missing`.",
                                     .cap = CLI_CELLS_POOL}),
        "an attribute no class of a mixin's family sets is found");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = tree_said("shapes/square.py"),
                                     .other = "`Square` has no `colour`: neither it, a class it comes from, nor a "
                                              "class that comes from it sets `colour`.",
                                     .cap = CLI_CELLS_POOL}),
        "and one of the class it is mixed into");
    cli_check(tree_said("shapes/outside.py")[0] == '\0', "a class whose family is outside the tree is left alone");
}

int main(void)
{
    tree_hierarchy();
    tree_cycles();
    tree_mixins();
    return cli_checked();
}
