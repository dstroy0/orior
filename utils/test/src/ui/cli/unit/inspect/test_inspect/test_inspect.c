// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_inspect.c: the inspections of a file alone: a Python name set and never read is found and its fix preserves a call, one read in a closure, an
// f-string or with locals is not; Python code after a return is found and removed; a docstring out of PEP 257's form is found and fixed; a JavaScript
// declaration never read and code after a return are found; a docstring is generated in each form; and a class's methods are sorted by name
#include "ui/cli/src/inspect/inspect.h"
#include "ui/cli/src/text/edits.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_unread_text[] = "def f(a):\n    total = 0\n    seen = g(a)\n    for x in a:\n        total += x\n    "
                                    "with open(a) as handle:\n        pass\n    try:\n        pass\n    except ValueError as "
                                    "error:\n        pass\n    return total\n";
static const char s_unreachable_text[] = "def f(x):\n    if x:\n        return 1\n        print(x)\n    return 2\n    x += 1\n    "
                                         "if x:\n        print(x)\n\nprint(f(1))\n";
static const char s_docs_text[] = "def f():\n    '''Return one'''\n    return 1\n\ndef g():\n    \"\"\" Return two. "
                                  "\"\"\"\n\ndef h():\n    \"\"\"Return three.\n    More.\"\"\"\n\ndef k():\n    "
                                  "\"\"\"Return four.\n\n    More.\n    \"\"\"\n";
static const char s_js_text[] = "export function f(a) {\n  const unused = 1;\n  const retained = g(a);\n  let used = a + 1;\n  const obj = { "
                                "used: "
                                "1, "
                                "other };\n  const after = `${obj.used}`;\n  if (a) return after;\n  return used;\n  "
                                "console.log(a);\n}\n\nfunction "
                                "h(x) {\n  switch (x) {\n    case 1:\n      return 1;\n      x += 1;\n    case 2:\n      break;\n  }\n  return "
                                "/a\\/b/.test(x) ? x / 2 : x;\n}\n";
static const char s_method_text[] = "class Shape:\n    def area(self, width: int, height=2, *rest) -> float:\n        if "
                                    "width < 0:\n            raise ValueError(\"width\")\n        return width * height\n";

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
static char s_requested[1u << 16u];

// the facts of `text` in `language` read into s_facts: 1, or 0
static int facts_read(const char *language, const char *text)
{
    EMBED_CALL(cli_inspect_facts_table.facts_begin, CliInspectFactsCfg, .facts = &s_facts, .imports = s_imports, .imports_max = 256u, .classes = s_classes, .classes_max = 64u, .functions = s_functions, .functions_max = 256u, .findings = s_findings, .findings_max = 256u, .fixes = s_fixes, .fixes_max = 256u, .edits = s_edits, .edits_max = 1024u, .names = s_names, .names_max = 1024u, .text_out = s_pool, .text_max = sizeof(s_pool));
    return EMBED_CALL(cli_inspect_facts_language.facts, CliInspectFactsLanguageCfg, .language = language, .text = text, .size = CLI_UNITY_LENGTH(text), .facts = &s_facts);
}

// `text` with the edits of the first fix of finding `finding` written over it, into s_applied
static const char *fix_applied(const char *text, unsigned int finding)
{
    static CliTextEdit s_text_edits[64];
    const CliInspectFix *const fix = &s_facts.fix[s_facts.finding[finding].fix_first];
    unsigned int count = 0u;
    for (unsigned int at = 0u; (s_facts.finding[finding].fixes > 0u) && (at < fix->edits) && (count < 64u); at += 1u)
    {
        const CliInspectEdit *const edit = &s_facts.edit[fix->edit_first + at];
        const char *const written = EMBED_CALL(cli_inspect_pool.text, CliInspectPoolCfg, .pool = s_facts.text, .text_inspect_text = edit->text);
        s_text_edits[count] = (CliTextEdit){{edit->from.line, edit->from.col}, {edit->to.line, edit->to.col}, written, CLI_UNITY_LENGTH(written)};
        count += 1u;
    }
    VerbaLinea out = {s_applied, sizeof(s_applied), 0ull};
    (void)EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = text, .size = CLI_UNITY_LENGTH(text), .edits = s_text_edits, .count = count, .out = &out);
    (void)mmgr_line_finish(&out);
    return s_applied;
}

// `text` with its first `from` replaced by `to`, into s_requested
static const char *text_replaced(const char *text, const char *from, const char *to)
{
    const char *const at = cellul.find(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES, .other = from, .other_cap = CLI_POOL_BYTES});
    VerbaLinea out = {s_requested, sizeof(s_requested), 0ull};
    if (at != NULL)
    {
        // a find inside the text, at or past its start
        mmgr_line_put_n(&out, text, (unsigned long long)(at - text));
        mmgr_line_put(&out, to);
        mmgr_line_put(&out, at + CLI_UNITY_LENGTH(from));
    }
    (void)mmgr_line_finish(&out);
    return s_requested;
}

// each finding of s_facts written: `line:code name` where `codes`, else its name, the first word of its message without its backticks, joined by `|`
static const char *findings_written(int codes)
{
    static char s_findings_text[4096];
    VerbaLinea line = {s_findings_text, sizeof(s_findings_text), 0ull};
    for (unsigned int at = 0u; at < s_facts.findings; at += 1u)
    {
        const CliInspectFinding *const one = &s_facts.finding[at];
        const char *const message = EMBED_CALL(cli_inspect_pool.text, CliInspectPoolCfg, .pool = s_facts.text, .text_inspect_text = one->message);
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        if (codes)
        {
            mmgr_line_decimal(&line, one->from.line, 1u);
            mmgr_line_ch(&line, ':');
            mmgr_line_put(&line, one->code);
            mmgr_line_ch(&line, ' ');
        }
        for (const char *character = message; (character[0] != '\0') && (character[0] != ' '); character += 1)
        {
            if (character[0] != '`')
            {
                mmgr_line_ch(&line, character[0]);
            }
        }
    }
    (void)mmgr_line_finish(&line);
    return s_findings_text;
}

// the findings whose code is unreachable, their indices into `unreachable`, which is sized for 4: their count
static unsigned int unreachable_found(unsigned int *unreachable)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < s_facts.findings) && (count < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_facts.finding[at].code, .other = "unreachable", .cap = CLI_POOL_BYTES}))
        {
            unreachable[count] = at;
            count += 1u;
        }
    }
    return count;
}

// `edit`, its text in `text`, written over `source` into s_applied
static void edit_applied(const char *source, const CliInspectEdit *edit, const char *text, unsigned long long size)
{
    const CliTextEdit one = {{edit->from.line, edit->from.col}, {edit->to.line, edit->to.col}, text, size};
    VerbaLinea out = {s_applied, sizeof(s_applied), 0ull};
    (void)EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = source, .size = CLI_UNITY_LENGTH(source), .edits = &one, .count = 1u, .out = &out);
    (void)mmgr_line_finish(&out);
}

// the docstring of `form` generated for line `line` of `source` in `language`, written over it into s_applied: 1, its summary's place into `summary`,
// or 0
static int docstring_written(const char *language, const char *source, unsigned int line, const char *form, CliInspectPosition *summary)
{
    static char s_text[1u << 14u];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    CliInspectEdit edit;
    if (!EMBED_CALL(cli_inspect_language.docstring, CliInspectLanguageCfg, .language = language, .source = source, .size = CLI_UNITY_LENGTH(source), .line = line, .form = form, .edit = &edit, .text_verba_linea_pointer = &text, .summary = summary, .err_msg = &err_msg))
    {
        return 0;
    }
    edit_applied(source, &edit, s_text, text.at);
    return 1;
}

// the methods of the class at `line` of `source` sorted, written over it into s_applied: 1, or 0
static int methods_sorted(const char *language, const char *source, unsigned int line)
{
    static char s_text[1u << 14u];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    CliInspectEdit edit;
    if (!EMBED_CALL(cli_inspect_language.sort_methods, CliInspectLanguageCfg, .language = language, .source = source, .size = CLI_UNITY_LENGTH(source), .line = line, .edit = &edit, .text_verba_linea_pointer = &text, .err_msg = &err_msg))
    {
        return 0;
    }
    edit_applied(source, &edit, s_text, text.at);
    return 1;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_python_name_set_and_never_read_is_found(void)
{
    TEST_ASSERT_TRUE(facts_read("python", s_unread_text));
    TEST_ASSERT_EQUAL_STRING("seen|handle|error", findings_written(0));
}

void test_an_unread_names_fix_preserves_the_call(void)
{
    TEST_ASSERT_TRUE(facts_read("python", s_unread_text));
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_unread_text, "    seen = g(a)\n", "    g(a)\n"), fix_applied(s_unread_text, 0u));
}

void test_a_withs_unread_name_is_removed(void)
{
    TEST_ASSERT_TRUE(facts_read("python", s_unread_text));
    CLI_TEST_ASSERT_CONTAINS("with open(a):", fix_applied(s_unread_text, 1u));
}

void test_an_excepts_unread_name_is_removed(void)
{
    TEST_ASSERT_TRUE(facts_read("python", s_unread_text));
    CLI_TEST_ASSERT_CONTAINS("except ValueError:", fix_applied(s_unread_text, 2u));
}

void test_a_name_read_in_a_closure_an_fstring_or_with_locals_is_not_found(void)
{
    static const char s_read[] = "def f():\n    a = 1\n    b = 2\n    c = 3\n    _d = 4\n    def inner():\n        "
                                 "return a\n    print(f\"{b:>{c}}\")\n    return inner\n\ndef g():\n    e = 5\n    return "
                                 "locals()\n";
    TEST_ASSERT_TRUE(facts_read("python", s_read));
    TEST_ASSERT_EQUAL_UINT(0u, s_facts.findings);
}

void test_an_fstrings_doubled_braces_escapes_and_triple_quotes_read_its_fields_alone(void)
{
    static const char s_read[] = "def f(a):\n    b = 1\n    c = 2\n    d = 3\n    e = 4\n    print(f\"{{b}} {c.real} \\\" {d!r}\")\n"
                                 "    print(rf'''{e} '{a}' ''')\n    return a\n";
    TEST_ASSERT_TRUE(facts_read("python", s_read));
    TEST_ASSERT_EQUAL_STRING("b", findings_written(0));
}

void test_python_code_after_a_return_is_found(void)
{
    unsigned int unreachable[4];
    (void)facts_read("python", s_unreachable_text);
    TEST_ASSERT_EQUAL_UINT(2u, unreachable_found(unreachable));
    TEST_ASSERT_EQUAL_UINT(3u, s_facts.finding[unreachable[0]].from.line);
    TEST_ASSERT_EQUAL_UINT(3u, s_facts.finding[unreachable[0]].to.line);
    TEST_ASSERT_EQUAL_UINT(5u, s_facts.finding[unreachable[1]].from.line);
    TEST_ASSERT_EQUAL_UINT(7u, s_facts.finding[unreachable[1]].to.line);
}

void test_python_code_after_a_return_is_removed(void)
{
    unsigned int unreachable[4];
    (void)facts_read("python", s_unreachable_text);
    TEST_ASSERT_EQUAL_UINT(2u, unreachable_found(unreachable));
    TEST_ASSERT_EQUAL_STRING("def f(x):\n    if x:\n        return 1\n        print(x)\n    return 2\n\nprint(f(1))\n", fix_applied(s_unreachable_text, unreachable[1]));
}

void test_a_docstring_out_of_pep_257s_form_is_found(void)
{
    char codes[256];
    VerbaLinea line = {codes, sizeof(codes), 0ull};
    (void)facts_read("python", s_docs_text);
    for (unsigned int at = 0u; at < s_facts.findings; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, s_facts.finding[at].code);
    }
    CLI_TEST_ASSERT_TEXT("docstring-quotes|docstring-period|docstring-space|docstring-blank|docstring-close", codes, line.at);
}

void test_a_docstrings_quotes_are_created_double(void)
{
    (void)facts_read("python", s_docs_text);
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_docs_text, "'''Return one'''", "\"\"\"Return one\"\"\""), fix_applied(s_docs_text, 0u));
}

void test_a_docstrings_period_is_added(void)
{
    (void)facts_read("python", s_docs_text);
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_docs_text, "'''Return one'''", "'''Return one.'''"), fix_applied(s_docs_text, 1u));
}

void test_a_docstrings_spaces_are_removed(void)
{
    (void)facts_read("python", s_docs_text);
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_docs_text, "\"\"\" Return two. \"\"\"", "\"\"\"Return two.\"\"\""), fix_applied(s_docs_text, 2u));
}

void test_a_docstrings_close_moves_to_a_line_of_its_own(void)
{
    (void)facts_read("python", s_docs_text);
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_docs_text, "    More.\"\"\"", "    More.\n    \"\"\""), fix_applied(s_docs_text, 4u));
}

void test_a_javascript_declaration_never_read_and_code_after_a_return_are_found(void)
{
    TEST_ASSERT_TRUE(facts_read("javascript", s_js_text));
    TEST_ASSERT_EQUAL_STRING("1:unread unused|2:unread retained|8:unreachable No|15:unreachable No", findings_written(1));
}

void test_a_declaration_of_nothing_is_removed(void)
{
    TEST_ASSERT_TRUE(facts_read("javascript", s_js_text));
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_js_text, "  const unused = 1;\n", ""), fix_applied(s_js_text, 0u));
}

void test_a_declaration_of_a_call_preserves_the_call(void)
{
    TEST_ASSERT_TRUE(facts_read("javascript", s_js_text));
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_js_text, "const retained = g(a);", "g(a);"), fix_applied(s_js_text, 1u));
}

void test_javascript_code_after_a_return_is_removed(void)
{
    TEST_ASSERT_TRUE(facts_read("javascript", s_js_text));
    TEST_ASSERT_EQUAL_STRING(text_replaced(s_js_text, "  console.log(a);\n", ""), fix_applied(s_js_text, 2u));
}

void test_a_docstring_in_googles_form_positions_its_summary_after_the_quotes(void)
{
    CliInspectPosition summary = {0u, 0u};
    TEST_ASSERT_TRUE(docstring_written("python", s_method_text, 3u, "google", &summary));
    TEST_ASSERT_EQUAL_UINT(2u, summary.line);
    TEST_ASSERT_EQUAL_UINT(11u, summary.col);
    CLI_TEST_ASSERT_CONTAINS("        \"\"\"\n\n        Args:\n            width (int):\n            "
                             "height:\n            *rest:\n\n        Returns:\n            float:\n\n        "
                             "Raises:\n            ValueError:\n        \"\"\"\n        if width",
                             s_applied);
}

void test_a_docstring_in_numpys_form(void)
{
    CliInspectPosition summary = {0u, 0u};
    TEST_ASSERT_TRUE(docstring_written("python", s_method_text, 3u, "numpy", &summary));
    CLI_TEST_ASSERT_CONTAINS("        Parameters\n        ----------\n        width : int\n        height\n   "
                             "     *rest\n\n        Returns\n        -------\n        float\n\n        Raises\n "
                             "       ------\n        ValueError\n        \"\"\"",
                             s_applied);
}

void test_a_docstring_in_restructured_texts_form(void)
{
    CliInspectPosition summary = {0u, 0u};
    TEST_ASSERT_TRUE(docstring_written("python", s_method_text, 3u, "rest", &summary));
    CLI_TEST_ASSERT_CONTAINS("        \"\"\"\n\n        :param width:\n        :type width: int\n        "
                             ":param height:\n        :param *rest:\n        :return:\n        :rtype: float\n "
                             "       :raises ValueError:\n        \"\"\"",
                             s_applied);
}

void test_a_plain_docstring_is_a_summary_line_alone(void)
{
    CliInspectPosition summary = {0u, 0u};
    TEST_ASSERT_TRUE(docstring_written("python", s_method_text, 3u, "plain", &summary));
    CLI_TEST_ASSERT_CONTAINS("-> float:\n        \"\"\"\"\"\"\n        if width", s_applied);
}

void test_a_function_with_a_docstring_is_unchanged(void)
{
    static char s_documented[1024];
    CliInspectPosition summary = {0u, 0u};
    TEST_ASSERT_TRUE(docstring_written("python", s_method_text, 3u, "plain", &summary));
    memor.cpy(&(MemoriaCfg){.dst = s_documented, .src = s_applied, .bytes = CLI_UNITY_LENGTH(s_applied) + 1u});
    TEST_ASSERT_FALSE(docstring_written("python", s_documented, 3u, "google", &summary));
}

void test_a_jsdoc_comment_is_generated_for_a_javascript_function(void)
{
    static const char s_js[] = "export function area(width, height = 2) {\n  if (width < 0) {\n    throw new "
                               "RangeError(\"width\");\n  }\n  return width * height;\n}\n";
    char required[512];
    CliInspectPosition summary = {0u, 0u};
    VerbaLinea line = {required, sizeof(required), 0ull};
    mmgr_line_put(&line, "/**\n * \n *\n * @param width\n * @param height\n * @returns\n * @throws {RangeError}\n */\n");
    mmgr_line_put(&line, s_js);
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_TRUE(docstring_written("javascript", s_js, 1u, "google", &summary));
    TEST_ASSERT_EQUAL_STRING(required, s_applied);
    TEST_ASSERT_EQUAL_UINT(1u, summary.line);
    TEST_ASSERT_EQUAL_UINT(3u, summary.col);
}

void test_a_jsdoc_comment_passes_over_the_functions_inside_and_names_each_thrown_once(void)
{
    static const char s_js[] = "function run(items) {\n  const pick = function () {\n    throw new TypeError(\"x\");\n"
                               "    return 1;\n  };\n  if (!items) {\n    throw items;\n  }\n  throw \"no\";\n}\n";
    char required[512];
    CliInspectPosition summary = {0u, 0u};
    VerbaLinea line = {required, sizeof(required), 0ull};
    mmgr_line_put(&line, "/**\n * \n *\n * @param items\n * @throws {Error}\n */\n");
    mmgr_line_put(&line, s_js);
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_TRUE(docstring_written("javascript", s_js, 0u, "google", &summary));
    TEST_ASSERT_EQUAL_STRING(required, s_applied);
}

void test_a_javascript_function_with_a_comment_over_it_is_unchanged(void)
{
    CliInspectPosition summary = {0u, 0u};
    TEST_ASSERT_FALSE(docstring_written("javascript", "/** draws */\nfunction draw() {\n  return 1;\n}\n", 2u, "google", &summary));
}

void test_a_python_classs_methods_are_sorted_with_their_comments_and_decorators(void)
{
    static const char s_text[] = "class Shape:\n    \"\"\"A shape.\"\"\"\n\n    sides = 0\n\n    # the area\n    "
                                 "@property\n    def area(self):\n        return 1\n\n    def __init__(self):\n        "
                                 "pass\n\n    def draw(self):\n        return self.area\n";
    TEST_ASSERT_TRUE(methods_sorted("python", s_text, 3u));
    TEST_ASSERT_EQUAL_STRING("class Shape:\n    \"\"\"A shape.\"\"\"\n\n    sides = 0\n\n    def "
                             "__init__(self):\n        pass\n\n    # the area\n    @property\n    def "
                             "area(self):\n        return 1\n\n    def draw(self):\n        return "
                             "self.area\n",
                             s_applied);
}

void test_a_javascript_classs_methods_are_sorted(void)
{
    static const char s_js[] = "class Shape {\n  sides = 0;\n\n  // draws it\n  draw() {\n    return 1;\n  }\n\n  "
                               "area() {\n    return 2;\n  }\n}\n";
    TEST_ASSERT_TRUE(methods_sorted("javascript", s_js, 4u));
    TEST_ASSERT_EQUAL_STRING("class Shape {\n  sides = 0;\n\n  area() {\n    return 2;\n  }\n\n  // draws "
                             "it\n  draw() {\n    return 1;\n  }\n}\n",
                             s_applied);
}

void test_methods_in_order_are_unchanged(void)
{
    TEST_ASSERT_FALSE(methods_sorted("python", "class A:\n    def a(self):\n        pass\n\n    def b(self):\n        pass\n", 1u));
}
