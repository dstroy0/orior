// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// editorconfig_test.c: the indentation's check: the nearest section that names a file sets its indent
#include "../../../../../ui/cli/src/editor/editorconfig.h"
#include "cli_check.h"

static char s_root[CLI_PATH_LONGEST];

// `text` written to the file `name` under the root
static void editorconfig_written(const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the indent of the file `name` under the root checked against `tabs`, -1 for none, and `size`, 0 for none
static void editorconfig_checked(const char *name, int tabs, unsigned int size)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_root, name, path);
    CliIndent indent;
    cli_indent(path, &indent);
    char what[256];
    CliLine line = {what, sizeof(what), 0ull};
    cli_line_text(&line, name);
    cli_line_text(&line, (tabs < 0) ? " has no indent set" : ((tabs > 0) ? " is indented by tabs" : " by spaces"));
    if (size > 0u)
    {
        cli_line_text(&line, ", ");
        cli_line_decimal(&line, size, 1u);
        cli_line_text(&line, " wide");
    }
    (void)cli_line_closed(&line);
    cli_check((indent.tabs == tabs) && ((size == 0u) ? !indent.sized : (indent.sized && (indent.size == size))), what);
}

int main(void)
{
    char deep[CLI_PATH_LONGEST];
    cli_check_folder("editorconfig", s_root);
    cli_full(s_root, "src/deep", deep);
    (void)query_folder_made(deep);
    editorconfig_written(".editorconfig", "root = true\n\n[*]\nindent_style = space\nindent_size = 4\n\n[*.{go,mk}]\n"
                                          "indent_style = tab\ntab_width = 8\n\n[Makefile]\nindent_style = tab\n\n"
                                          "[lib/**.js]\nindent_size = 2\n\n[file[0-9].txt]\nindent_size = 3\n");
    editorconfig_written("src/.editorconfig", "[*.py]\nindent_size = 2\n");
    editorconfig_checked("a.rs", 0, 4u);
    editorconfig_checked("main.go", 1, 4u);
    editorconfig_checked("rules.mk", 1, 4u);
    editorconfig_checked("src/deep/x.py", 0, 2u);
    editorconfig_checked("lib/a/b.js", 0, 2u);
    editorconfig_checked("file7.txt", 0, 3u);
    editorconfig_checked("filex.txt", 0, 4u);
    editorconfig_written(".editorconfig", "root = true\n[*.go]\nindent_style = tab\ntab_width = 8\n");
    editorconfig_checked("main.go", 1, 8u);
    editorconfig_checked("a.rs", -1, 0u);
    return cli_checked();
}
