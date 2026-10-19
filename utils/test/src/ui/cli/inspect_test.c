// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_test.c: the inspections of a file alone: a Python name set and never read is found and its fix keeps a call;
// one read in a closure, an f-string or with locals is not; Python code after a return is found and taken out; a
// docstring out of PEP 257's form is found and fixed; a JavaScript declaration never read and code after a return are
// found; a Python docstring is drawn up in each form and a JSDoc comment for a JavaScript function; and a class's
// methods are sorted by name, its other members staying in place
#include "../../../../../ui/cli/src/inspect/inspect.h"
#include "../../../../../ui/cli/src/text/edits.h"
#include "cli_check.h"

static CliInspectImport s_imports[256];
static CliInspectClass s_classes[64];
static CliInspectFunction s_functions[256];
static CliInspectFinding s_findings[256];
static CliInspectFix s_fixes[256];
static CliInspectEdit s_edits[1024];
static CliInspectName s_names[1024];
static char s_pool[1u << 16u];
static CliFacts s_facts;
static char s_applied[1u << 16u];
static char s_wanted[1u << 16u];

// the facts of `text` in `language` read into s_facts: 1, or 0
static int inspect_facts(const char *language, const char *text)
{
    cli_facts_begun(&s_facts, s_imports, 256u, s_classes, 64u, s_functions, 256u, s_findings, 256u, s_fixes, 256u,
                    s_edits, 1024u, s_names, 1024u, s_pool, sizeof(s_pool));
    return cli_inspect_facts(language, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                             &s_facts);
}

// `text` with the edits of the first fix of finding `finding` written over it, into s_applied
static const char *inspect_applied(const char *text, unsigned int finding)
{
    static CliTextEdit s_text_edits[64];
    const CliInspectFix *const fix = &s_facts.fix[s_facts.finding[finding].fix_first];
    unsigned int count = 0u;
    for (unsigned int at = 0u; (s_facts.finding[finding].fixes > 0u) && (at < fix->edits) && (count < 64u); at += 1u)
    {
        const CliInspectEdit *const edit = &s_facts.edit[fix->edit_first + at];
        const char *const written = cli_inspect_text(s_facts.text, edit->text);
        s_text_edits[count] = (CliTextEdit){{edit->from.line, edit->from.col},
                                            {edit->to.line, edit->to.col},
                                            written,
                                            cellul.len(&(CatenaFinitaCfg){.src = written, .cap = CLI_CELLS_POOL})};
        count += 1u;
    }
    CliLine out = {s_applied, sizeof(s_applied), 0ull};
    (void)cli_text_applied(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), s_text_edits,
                           count, &out);
    (void)cli_line_closed(&out);
    return s_applied;
}

// `text` with its first `from` made `to`, into s_wanted
static const char *inspect_replaced(const char *text, const char *from, const char *to)
{
    const char *const at =
        cellul.find(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = from, .other_cap = CLI_CELLS_POOL});
    CliLine out = {s_wanted, sizeof(s_wanted), 0ull};
    if (at != NULL)
    {
        cli_line_counted(&out, text, (unsigned long long)(at - text));
        cli_line_text(&out, to);
        cli_line_text(&out, at + cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}));
    }
    (void)cli_line_closed(&out);
    return s_wanted;
}

// each finding of s_facts written: `line:code name` where `codes`, else its name, the first word of its message without
// its backticks, joined by `|`
static const char *inspect_said(int codes)
{
    static char s_said[4096];
    CliLine line = {s_said, sizeof(s_said), 0ull};
    for (unsigned int at = 0u; at < s_facts.findings; at += 1u)
    {
        const CliInspectFinding *const one = &s_facts.finding[at];
        const char *const message = cli_inspect_text(s_facts.text, one->message);
        cli_line_text(&line, (at > 0u) ? "|" : "");
        if (codes)
        {
            cli_line_decimal(&line, one->from.line, 1u);
            cli_line_character(&line, ':');
            cli_line_text(&line, one->code);
            cli_line_character(&line, ' ');
        }
        for (const char *letter = message; (letter[0] != '\0') && (letter[0] != ' '); letter += 1)
        {
            if (letter[0] != '`')
            {
                cli_line_character(&line, letter[0]);
            }
        }
    }
    (void)cli_line_closed(&line);
    return s_said;
}

// a Python name set and never read is found and its fix keeps a call; and one read in a closure, an f-string or with
// locals is not
static void inspect_python_unread(void)
{
    static const char s_text[] =
        "def f(a):\n    total = 0\n    seen = g(a)\n    for x in a:\n        total += x\n    "
        "with open(a) as handle:\n        pass\n    try:\n        pass\n    except ValueError as "
        "error:\n        pass\n    return total\n";
    cli_check(inspect_facts("python", s_text) &&
                  (cellul.eq(
                      &(CatenaFinitaCfg){.src = inspect_said(0), .other = "seen|handle|error", .cap = CLI_CELLS_POOL})),
              "a Python name set and never read is found");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = inspect_applied(s_text, 0u),
                                           .other = inspect_replaced(s_text, "    seen = g(a)\n", "    g(a)\n"),
                                           .cap = CLI_CELLS_POOL}),
              "its fix keeps the call");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = inspect_applied(s_text, 1u),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "with open(a):",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "a with's name is taken out");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = inspect_applied(s_text, 2u),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "except ValueError:",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "and an except's");
    static const char s_read[] =
        "def f():\n    a = 1\n    b = 2\n    c = 3\n    _d = 4\n    def inner():\n        "
        "return a\n    print(f\"{b:>{c}}\")\n    return inner\n\ndef g():\n    e = 5\n    return "
        "locals()\n";
    cli_check(inspect_facts("python", s_read) && (s_facts.findings == 0u),
              "a name read in a closure, an f-string or with locals is not found");
}

// Python code after a return is found and taken out; and a docstring out of PEP 257's form is found and fixed
static void inspect_python_shape(void)
{
    static const char s_text[] =
        "def f(x):\n    if x:\n        return 1\n        print(x)\n    return 2\n    x += 1\n    "
        "if x:\n        print(x)\n\nprint(f(1))\n";
    (void)inspect_facts("python", s_text);
    unsigned int unreachable[4];
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < s_facts.findings) && (count < 4u); at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = s_facts.finding[at].code, .other = "unreachable", .cap = CLI_CELLS_POOL}))
        {
            unreachable[count] = at;
            count += 1u;
        }
    }
    cli_check((count == 2u) && (s_facts.finding[unreachable[0]].from.line == 3u) &&
                  (s_facts.finding[unreachable[0]].to.line == 3u) &&
                  (s_facts.finding[unreachable[1]].from.line == 5u) && (s_facts.finding[unreachable[1]].to.line == 7u),
              "Python code after a return is found");
    cli_check(
        (count == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = inspect_applied(s_text, unreachable[1]),
                .other = "def f(x):\n    if x:\n        return 1\n        print(x)\n    return 2\n\nprint(f(1))\n",
                .cap = CLI_CELLS_POOL})),
        "and taken out");
    static const char s_docs[] = "def f():\n    '''Return one'''\n    return 1\n\ndef g():\n    \"\"\" Return two. "
                                 "\"\"\"\n\ndef h():\n    \"\"\"Return three.\n    More.\"\"\"\n\ndef k():\n    "
                                 "\"\"\"Return four.\n\n    More.\n    \"\"\"\n";
    (void)inspect_facts("python", s_docs);
    char codes[256];
    CliLine line = {codes, sizeof(codes), 0ull};
    for (unsigned int at = 0u; at < s_facts.findings; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, s_facts.finding[at].code);
    }
    cli_check_text(codes, line.at, "docstring-quotes|docstring-period|docstring-space|docstring-blank|docstring-close",
                   "a docstring out of PEP 257's form is found");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = inspect_applied(s_docs, 0u),
                                     .other = inspect_replaced(s_docs, "'''Return one'''", "\"\"\"Return one\"\"\""),
                                     .cap = CLI_CELLS_POOL}),
        "its quotes made double");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = inspect_applied(s_docs, 1u),
                                           .other = inspect_replaced(s_docs, "'''Return one'''", "'''Return one.'''"),
                                           .cap = CLI_CELLS_POOL}),
              "its period put");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = inspect_applied(s_docs, 2u),
                  .other = inspect_replaced(s_docs, "\"\"\" Return two. \"\"\"", "\"\"\"Return two.\"\"\""),
                  .cap = CLI_CELLS_POOL}),
              "its spaces taken out");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = inspect_applied(s_docs, 4u),
                                     .other = inspect_replaced(s_docs, "    More.\"\"\"", "    More.\n    \"\"\""),
                                     .cap = CLI_CELLS_POOL}),
        "and its close put on a line of its own");
}

// a JavaScript declaration never read and code after a return are found
static void inspect_javascript(void)
{
    static const char s_text[] =
        "export function f(a) {\n  const unused = 1;\n  const kept = g(a);\n  let used = a + 1;\n  const obj = { used: "
        "1, "
        "other };\n  const after = `${obj.used}`;\n  if (a) return after;\n  return used;\n  "
        "console.log(a);\n}\n\nfunction "
        "h(x) {\n  switch (x) {\n    case 1:\n      return 1;\n      x += 1;\n    case 2:\n      break;\n  }\n  return "
        "/a\\/b/.test(x) ? x / 2 : x;\n}\n";
    cli_check(
        inspect_facts("javascript", s_text) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = inspect_said(1),
                                          .other = "1:unread unused|2:unread kept|8:unreachable No|15:unreachable No",
                                          .cap = CLI_CELLS_POOL})),
        "a JavaScript declaration never read and code after a return are found");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = inspect_applied(s_text, 0u),
                                           .other = inspect_replaced(s_text, "  const unused = 1;\n", ""),
                                           .cap = CLI_CELLS_POOL}),
              "a declaration of nothing is taken out");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = inspect_applied(s_text, 1u),
                                           .other = inspect_replaced(s_text, "const kept = g(a);", "g(a);"),
                                           .cap = CLI_CELLS_POOL}),
              "one of a call keeps the call");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = inspect_applied(s_text, 2u),
                                           .other = inspect_replaced(s_text, "  console.log(a);\n", ""),
                                           .cap = CLI_CELLS_POOL}),
              "and code after a return is taken out");
}

// `edit`, its text in `text`, written over `source` into s_applied
static const char *inspect_edited(const char *source, const CliInspectEdit *edit, const char *text,
                                  unsigned long long size)
{
    const CliTextEdit one = {{edit->from.line, edit->from.col}, {edit->to.line, edit->to.col}, text, size};
    CliLine out = {s_applied, sizeof(s_applied), 0ull};
    (void)cli_text_applied(source, cellul.len(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL}), &one, 1u,
                           &out);
    (void)cli_line_closed(&out);
    return s_applied;
}

// the docstring of `form` drawn up for line `line` of `source` in `language`, written over it into s_applied: 1, its
// summary's place into `summary`, or 0
static int inspect_docstring(const char *language, const char *source, unsigned int line, const char *form,
                             CliInspectPlace *summary)
{
    static char s_text[1u << 14u];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine text = {s_text, sizeof(s_text), 0ull};
    CliInspectEdit edit;
    if (!cli_inspect_docstring(language, source, cellul.len(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL}),
                               line, form, &edit, &text, summary, &said))
    {
        return 0;
    }
    (void)inspect_edited(source, &edit, s_text, text.at);
    return 1;
}

// a Python docstring is drawn up in each form, and a JSDoc comment for a JavaScript function
static void inspect_docstrings(void)
{
    static const char s_text[] = "class Shape:\n    def area(self, width: int, height=2, *rest) -> float:\n        if "
                                 "width < 0:\n            raise ValueError(\"width\")\n        return width * height\n";
    CliInspectPlace summary = {0u, 0u};
    cli_check(inspect_docstring("python", s_text, 3u, "google", &summary) && (summary.line == 2u) &&
                  (summary.col == 11u) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_applied,
                       .cap = CLI_CELLS_POOL,
                       .other = "        \"\"\"\n\n        Args:\n            width (int):\n            "
                                "height:\n            *rest:\n\n        Returns:\n            float:\n\n        "
                                "Raises:\n            ValueError:\n        \"\"\"\n        if width",
                       .other_cap = CLI_CELLS_POOL}) != NULL),
              "Google's form, its summary to be written after the quotes");
    cli_check(inspect_docstring("python", s_text, 3u, "numpy", &summary) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_applied,
                       .cap = CLI_CELLS_POOL,
                       .other = "        Parameters\n        ----------\n        width : int\n        height\n   "
                                "     *rest\n\n        Returns\n        -------\n        float\n\n        Raises\n "
                                "       ------\n        ValueError\n        \"\"\"",
                       .other_cap = CLI_CELLS_POOL}) != NULL),
              "NumPy's");
    cli_check(inspect_docstring("python", s_text, 3u, "rest", &summary) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_applied,
                       .cap = CLI_CELLS_POOL,
                       .other = "        \"\"\"\n\n        :param width:\n        :type width: int\n        "
                                ":param height:\n        :param *rest:\n        :return:\n        :rtype: float\n "
                                "       :raises ValueError:\n        \"\"\"",
                       .other_cap = CLI_CELLS_POOL}) != NULL),
              "reStructuredText's");
    cli_check(inspect_docstring("python", s_text, 3u, "plain", &summary) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_applied,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "-> float:\n        \"\"\"\"\"\"\n        if width",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a summary line alone");
    static char s_documented[1024];
    memor.cpy(&(MemoriaCfg){.dst = s_documented,
                            .src = s_applied,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_applied, .cap = CLI_CELLS_POOL}) + 1u});
    cli_check(!inspect_docstring("python", s_documented, 3u, "google", &summary),
              "a function with a docstring is left as it is");
    static const char s_js[] = "export function area(width, height = 2) {\n  if (width < 0) {\n    throw new "
                               "RangeError(\"width\");\n  }\n  return width * height;\n}\n";
    char wanted[512];
    CliLine line = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&line,
                  "/**\n * \n *\n * @param width\n * @param height\n * @returns\n * @throws {RangeError}\n */\n");
    cli_line_text(&line, s_js);
    (void)cli_line_closed(&line);
    cli_check(inspect_docstring("javascript", s_js, 1u, "google", &summary) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_applied, .other = wanted, .cap = CLI_CELLS_POOL})) &&
                  (summary.line == 1u) && (summary.col == 3u),
              "a JSDoc comment is drawn up for a JavaScript function");
}

// the methods of the class at `line` of `source` sorted, written over it into s_applied: 1, or 0
static int inspect_sorted(const char *language, const char *source, unsigned int line)
{
    static char s_text[1u << 14u];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine text = {s_text, sizeof(s_text), 0ull};
    CliInspectEdit edit;
    if (!cli_inspect_sort_methods(language, source,
                                  cellul.len(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL}), line, &edit,
                                  &text, &said))
    {
        return 0;
    }
    (void)inspect_edited(source, &edit, s_text, text.at);
    return 1;
}

// a class's methods are sorted by name, its other members staying in place
static void inspect_sort(void)
{
    static const char s_text[] = "class Shape:\n    \"\"\"A shape.\"\"\"\n\n    sides = 0\n\n    # the area\n    "
                                 "@property\n    def area(self):\n        return 1\n\n    def __init__(self):\n        "
                                 "pass\n\n    def draw(self):\n        return self.area\n";
    cli_check(inspect_sorted("python", s_text, 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_applied,
                      .other = "class Shape:\n    \"\"\"A shape.\"\"\"\n\n    sides = 0\n\n    def "
                               "__init__(self):\n        pass\n\n    # the area\n    @property\n    def "
                               "area(self):\n        return 1\n\n    def draw(self):\n        return "
                               "self.area\n",
                      .cap = CLI_CELLS_POOL})),
              "a Python class's methods are sorted by name, each with its comments and decorators");
    static const char s_js[] = "class Shape {\n  sides = 0;\n\n  // draws it\n  draw() {\n    return 1;\n  }\n\n  "
                               "area() {\n    return 2;\n  }\n}\n";
    cli_check(inspect_sorted("javascript", s_js, 4u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_applied,
                      .other = "class Shape {\n  sides = 0;\n\n  area() {\n    return 2;\n  }\n\n  // draws "
                               "it\n  draw() {\n    return 1;\n  }\n}\n",
                      .cap = CLI_CELLS_POOL})),
              "and a JavaScript class's");
    cli_check(
        !inspect_sorted("python", "class A:\n    def a(self):\n        pass\n\n    def b(self):\n        pass\n", 1u),
        "methods in order are left as they are");
}

int main(void)
{
    inspect_python_unread();
    inspect_python_shape();
    inspect_javascript();
    inspect_docstrings();
    inspect_sort();
    return cli_checked();
}
