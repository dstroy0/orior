// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format_test.c: the formatting's check: each formatter names a language and reads the file, a width is read from
// each formatter's own settings, and a file's language is the window's
#include "../../../../../ui/cli/src/toolchains/format.h"
#include "../../../../../ui/cli/src/toolchains/format_internal.h"
#include "../../../../../ui/cli/src/toolchains/toolchains.h"
#include "cli_check.h"

// each formatter names a language and reads the file
static void format_formatters(void)
{
    static CliJson s_json;
    const unsigned int manifest = cli_toolchains_manifest(&s_json, 0u, 0ull);
    int named = cli_json_is(&s_json, manifest, CFG_JSON_ARRAY);
    for (unsigned int at = 0u; cli_json_item(&s_json, manifest, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int tool = cli_json_item(&s_json, manifest, at);
        const unsigned int formats = cli_json_member(&s_json, tool, "formats");
        named &= (cli_json_member(&s_json, tool, "format") == CLI_JSON_NONE) ||
                 (cli_json_is(&s_json, formats, CFG_JSON_ARRAY) && (s_json.node[formats].count > 0u));
    }
    cli_json_given(&s_json);
    cli_check(named, "each formatter names a language");
    char program[CLI_NAME_LONGEST];
    cli_check(cli_format_program_of("python", program) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "black", .cap = CLI_CELLS_POOL})),
              "Python is Black's");
    cli_check(cli_format_program_of("c", program) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = program, .other = "clang-format", .cap = CLI_CELLS_POOL})),
              "C is clang-format's");
    cli_check(!cli_format_program_of("tex", program), "TeX has no formatter");
}

// the number `key` of `text`, within `table` where it is not NULL, -1 where it has none
static long long format_number(const char *text, const char *table, const char *key)
{
    unsigned int number = 0u;
    return cli_format_number_in(text, table, key, &number) ? (long long)number : -1ll;
}

// the width the formatter of `language` keeps the file `name` of the folder to, -1 where it gives none
static long long format_width(const char *folder, const char *name, const char *language)
{
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    unsigned int width = 0u;
    return cli_format_width(path, language, &width) ? (long long)width : -1ll;
}

// `text` written to the file `name` of the folder
static void format_file(const char *folder, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// a width is read from each formatter's own settings
static void format_widths(void)
{
    cli_check(format_number("[project]\nname = \"x\"\n[tool.black]\nline-length = 120 # wide\n", "tool.black",
                            "line-length") == 120ll,
              "Black's width is read within its table");
    cli_check(format_number("[tool.ruff]\nline-length = 99\n", "tool.black", "line-length") == -1ll,
              "and not within another's");
    cli_check(format_number("max_width = 110\n[other]\nmax_width = 3\n", NULL, "max_width") == 110ll,
              "rustfmt's before any table");
    cli_check(format_number("BasedOnStyle: LLVM\nColumnLimit: 140\n", NULL, "ColumnLimit") == 140ll,
              "clang-format's in its YAML");
    cli_check(format_number("{ \"semi\": false, \"printWidth\": 100 }", NULL, "printWidth") == 100ll,
              "Prettier's in its JSON");
    char dir[CLI_PATH_LONGEST];
    char pkg[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("format_width", dir);
    cli_full(dir, "pkg", pkg);
    (void)query_folder_made(pkg);
    format_file(dir, "pyproject.toml", "[tool.black]\nline-length = 100\n");
    format_file(dir, "Cargo.toml", "[package]\nname = \"x\"\nedition = \"2024\"\n");
    cli_check(format_width(pkg, "a.py", "python") == 100ll, "a width is read from the nearest folder above");
    char edition[CLI_NAME_LONGEST];
    cli_full(pkg, "a.rs", path);
    cli_lock_take(&g_cli_format_lock);
    cli_format_edition_of(path, edition);
    cli_lock_give(&g_cli_format_lock);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = edition, .other = "2024", .cap = CLI_CELLS_POOL}),
              "a crate's edition is read");
    cli_check(format_width(pkg, "a.tex", "tex") == -1ll, "a language no formatter formats has no width");
    format_file(dir, ".prettierrc", "{ \"printWidth\": 100 }");
    cli_check(format_width(pkg, "a.md", "markdown") == -1ll, "Markdown's prose is left as it is");
    char by[CLI_NAME_LONGEST];
    unsigned int width = 0u;
    cli_full(pkg, "a.js", path);
    cli_check(cli_format_margin(path, "javascript", &width, by) && (width == 100u), "Prettier's width is read");
    format_file(dir, ".prettierrc", "{ \"printWidth\": 100, \"proseWrap\": \"always\" }");
    cli_check(format_width(pkg, "a.md", "markdown") == 100ll, "and kept by Markdown where its prose wraps");
}

// the language `path` opens in, the empty text where none takes it
static const char *format_language(const char *path)
{
    static char s_id[CLI_NAME_LONGEST];
    return cli_format_language_of(path, s_id) ? s_id : "";
}

int main(void)
{
    format_formatters();
    format_widths();
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = format_language("a/b.py"), .other = "python", .cap = CLI_CELLS_POOL}),
              "a Python file is Python");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = format_language("k.CU"), .other = "c", .cap = CLI_CELLS_POOL}),
              "an extension's case is set aside");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = format_language("smooth.m"), .other = "matlab", .cap = CLI_CELLS_POOL}),
        "of two languages the first by id");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = format_language("none"), .other = "", .cap = CLI_CELLS_POOL}),
              "a file of no extension has no language");
    return cli_checked();
}
