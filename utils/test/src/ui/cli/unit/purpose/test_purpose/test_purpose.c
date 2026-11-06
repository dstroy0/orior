// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_purpose.c: the first comment past the license says what a file is for, a file uses another by its name or an import of its
// name, and a file's purpose names the files of the tree that use it
#include "ui/cli/src/code/purpose.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[1u << 14u];

// the first comment of `text`, the text of the file at `path`, is `required`
static void assert_comment(const char *required, const char *path, const char *text)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_purpose_path_text.first_comment, CliPurposePathTextCfg, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .out = &out);
    CLI_TEST_ASSERT_TEXT(required, s_out, out.at);
}

// whether `text` uses the file `name`, `stem` its name without its extension
static int text_uses(const char *text, const char *name, const char *stem)
{
    return EMBED_CALL(cli_purpose_text.uses, CliPurposeTextCfg, .text = text, .size = CLI_UNITY_LENGTH(text), .name = name, .stem = stem);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_scripts_comment_end_its_license(void)
{
    assert_comment("The live preview: the file in the editor shown as it reads.", "a.js",
                   "// orior - Copyright (C) 2026\n// SPDX-License-Identifier: AGPL-3.0\n\n// The live preview: the file\n"
                   "// in the editor shown as it reads.\n\nimport x from \"./x.js\";\n// later\n");
}

void test_a_rust_files_comment(void)
{
    assert_comment("Archives as folders: a zip opens as a folder.", "a.rs", "// Copyright\n\n//! Archives as folders: a zip\n//! opens as a folder.\n\nuse std::io;\n");
}

void test_a_python_modules_docstring(void)
{
    assert_comment("Rebuilds the oracles from their scripts.", "a.py", "#!/usr/bin/env python3\n\"\"\"Rebuilds the oracles\nfrom their scripts.\"\"\"\nimport sys\n");
}

void test_a_stylesheets_block_comment(void)
{
    assert_comment("The window's look: its colors and its type.", "a.css", "/* The window's look:\n * its colors and its type. */\nbody { }\n");
}

void test_a_comment_after_the_code_is_not_the_first(void)
{
    assert_comment("", "a.js", "const a = 1;\n// late\n");
}

void test_a_markdown_files_first_paragraph(void)
{
    assert_comment("An IDE of its own, with no libraries from outside.", "README.md",
                   "# orior\n\n![badge](x.svg)\n\nAn IDE of its own, with **no** [libraries](https://x) from "
                   "outside.\n\nMore.\n");
}

void test_a_long_comment_is_cut_at_a_sentence(void)
{
    static char s_long[4096];
    VerbaLinea line = {s_long, sizeof(s_long), 0ull};
    mmgr_line_put(&line, "// ");
    for (unsigned int at = 0u; at < 60u; at += 1u)
    {
        mmgr_line_put(&line, "A sentence of words. ");
    }
    mmgr_line_ch(&line, '\n');
    (void)mmgr_line_finish(&line);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_purpose_path_text.first_comment, CliPurposePathTextCfg, .path = "a.js", .text = s_long, .size = line.at, .out = &out);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(6ull, out.at);
    TEST_ASSERT_LESS_OR_EQUAL_UINT64(CLI_PURPOSE_TEXT_MAX_LEN, out.at);
    TEST_ASSERT_EQUAL_MEMORY("words.", s_out + out.at - 6u, 6u);
}

void test_a_comments_memory_is_sized_to_its_text_and_released(void)
{
    assert_comment("", "a.js", "const a = 1;\n// late\n");
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_purpose_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_purpose_owner));
    TEST_ASSERT_LESS_THAN_UINT64(65536ull, mmgr_owner_hw(g_cli_purpose_owner));
}

void test_an_import_uses_a_script(void)
{
    TEST_ASSERT_TRUE(text_uses("import { x } from \"./preview.js\";", "preview.js", "preview"));
}

void test_a_longer_name_is_another_file(void)
{
    TEST_ASSERT_FALSE(text_uses("import { x } from \"./mdpreview.js\";", "preview.js", "preview"));
}

void test_a_python_import_uses_a_module(void)
{
    TEST_ASSERT_TRUE(text_uses("from tools.oracle import check\n", "oracle.py", "oracle"));
}

void test_a_rust_mod_uses_a_module(void)
{
    TEST_ASSERT_TRUE(text_uses("pub mod archive;\nuse crate::archive;\n", "archive.rs", "archive"));
}

void test_an_include_uses_a_header(void)
{
    TEST_ASSERT_TRUE(text_uses("#include \"qry_buffer.h\"\n", "qry_buffer.h", "qry_buffer"));
}

void test_tex_input_uses_a_file(void)
{
    TEST_ASSERT_TRUE(text_uses("\\input{chapters/one}\n", "one.tex", "one"));
}

void test_a_word_in_prose_does_not_use_a_file(void)
{
    TEST_ASSERT_FALSE(text_uses("the preview shows\n", "preview.js", "preview"));
}

