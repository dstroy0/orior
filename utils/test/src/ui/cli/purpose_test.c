// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose_test.c: a file's purpose's check: the first comment past the license is what the file is for, a file uses
// another by its file name or by an import of its name, and a file's purpose names the files of the tree that use it
#include "../../../../../ui/cli/src/code/purpose.h"
#include "cli_check.h"

static char s_out[1u << 14u];

// the first comment of `text`, the text of the file at `path`, checked against `wanted`
static void purpose_comment(const char *path, const char *text, const char *wanted, const char *what)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_purpose_first_comment(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &out);
    cli_check_text(s_out, out.at, wanted, what);
}

// whether `text` uses the file `name`, `stem` its name without its extension
static int purpose_uses(const char *text, const char *name, const char *stem)
{
    return cli_purpose_uses(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), name, stem);
}

// `text` written to the file `name` of the folder
static void purpose_file(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the first comment past the license is what the file is for
static void purpose_comments(void)
{
    purpose_comment(
        "a.js",
        "// orior - Copyright (C) 2026\n// SPDX-License-Identifier: AGPL-3.0\n\n// The live preview: the file\n"
        "// in the editor shown as it reads.\n\nimport x from \"./x.js\";\n// later\n",
        "The live preview: the file in the editor shown as it reads.", "a script's comment past its license");
    purpose_comment("a.rs", "// Copyright\n\n//! Archives as folders: a zip\n//! opens as a folder.\n\nuse std::io;\n",
                    "Archives as folders: a zip opens as a folder.", "a Rust file's comment");
    purpose_comment("a.py",
                    "#!/usr/bin/env python3\n\"\"\"Rebuilds the oracles\nfrom their scripts.\"\"\"\nimport sys\n",
                    "Rebuilds the oracles from their scripts.", "a Python module's docstring");
    purpose_comment("a.css", "/* The window's look:\n * its colors and its type. */\nbody { }\n",
                    "The window's look: its colors and its type.", "a stylesheet's block comment");
    purpose_comment("a.js", "const a = 1;\n// late\n", "", "a comment after the code is not the first");
    purpose_comment("README.md",
                    "# orior\n\n![badge](x.svg)\n\nAn IDE of its own, with **no** [libraries](https://x) from "
                    "outside.\n\nMore.\n",
                    "An IDE of its own, with no libraries from outside.", "a Markdown file's first paragraph");
    static char s_long[4096];
    CliLine line = {s_long, sizeof(s_long), 0ull};
    cli_line_text(&line, "// ");
    for (unsigned int at = 0u; at < 60u; at += 1u)
    {
        cli_line_text(&line, "A sentence of words. ");
    }
    cli_line_character(&line, '\n');
    (void)cli_line_closed(&line);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_purpose_first_comment("a.js", s_long, line.at, &out);
    cli_check((out.at >= 6ull) && (out.at <= CLI_PURPOSE_SAID) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_out + out.at - 6u, .other = "words.", .bytes = 6u}) == 0),
              "a long comment is cut at a sentence");
}

// a file's purpose names the files of the tree that use it
static void purpose_users(void)
{
    char root[CLI_PATH_LONGEST];
    char src[CLI_PATH_LONGEST];
    cli_check_folder("purpose", root);
    cli_full(root, "src", src);
    (void)query_folder_made(src);
    purpose_file(root, "src/lib.js",
                 "// SPDX-License-Identifier: MIT\n\n// Helpers for the window.\nexport const one = 1;\n");
    purpose_file(root, "src/main.js", "import { one } from \"./lib.js\";\n");
    purpose_file(root, "src/other.js", "const nothing = 0;\n");
    purpose_file(root, "index.html", "<script type=\"module\" src=\"src/lib.js\"></script>\n");
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_purpose(root, "src/lib.js", &out, &said), "a file's purpose is read");
    cli_check_text(s_out, out.at,
                   "{\"said\":\"Helpers for the window.\",\"users\":[\"index.html\",\"src/main.js\"],\"all\":2}",
                   "it names the files of the tree that use it");
}

int main(void)
{
    purpose_comments();
    cli_check(purpose_uses("import { x } from \"./preview.js\";", "preview.js", "preview"), "an import uses a script");
    cli_check(!purpose_uses("import { x } from \"./mdpreview.js\";", "preview.js", "preview"),
              "a longer name is another file");
    cli_check(purpose_uses("from tools.oracle import check\n", "oracle.py", "oracle"), "a Python import uses a module");
    cli_check(purpose_uses("pub mod archive;\nuse crate::archive;\n", "archive.rs", "archive"),
              "a Rust module is used");
    cli_check(purpose_uses("#include \"qry_buffer.h\"\n", "qry_buffer.h", "qry_buffer"), "an include uses a header");
    cli_check(purpose_uses("\\input{chapters/one}\n", "one.tex", "one"), "TeX's input uses a file");
    cli_check(!purpose_uses("the preview shows\n", "preview.js", "preview"), "a word in prose is no use");
    purpose_users();
    return cli_checked();
}
