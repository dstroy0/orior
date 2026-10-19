// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// prints_test.c: the prints' check: a line prints by its file's language, the lines a diff puts in are read with their
// lines in the file as it stands, and the prints a change added are found in its files and in files new to git
#include "../../../../../ui/cli/src/code/prints.h"
#include "cli_check.h"

static char s_out[1u << 14u];

// whether `line` of the file `path` prints
static int prints_line(const char *path, const char *line)
{
    return cli_prints_line(path, line, cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL}));
}

// git run in `root` with `count` words after it, checked to have ended well
static void prints_git(const char *root, const char *const *words, unsigned int count)
{
    const char *line[8] = {"git"};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line[at + 1u] = words[at];
    }
    line[count + 1u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_ran(line, root, NULL, 0u, ran, &said) && (ran->code == 0), words[0]);
    cli_ran_given(ran);
}

// the prints a change added are found in its files and in files new to git
static void prints_added(void)
{
    static const char *const s_init[2] = {"init", "-q"};
    static const char *const s_name[3] = {"config", "user.name", "Tester"};
    static const char *const s_email[3] = {"config", "user.email", "tester@example.com"};
    static const char *const s_add[2] = {"add", "."};
    static const char *const s_first[4] = {"commit", "-q", "-m", "first"};
    static const char s_kept[] = "def f(x):\n    print(\"kept\")\n    return x\n";
    static const char s_changed[] = "def f(x):\n    print(\"kept\")\n    print(x)\n    return x\n";
    static const char s_new[] = "const a = 1;\nconsole.log(a);\n";
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("prints_added", root);
    prints_git(root, s_init, 2u);
    prints_git(root, s_name, 3u);
    prints_git(root, s_email, 3u);
    cli_full(root, "a.py", path);
    (void)query_file_written(path, s_kept, sizeof(s_kept) - 1u);
    prints_git(root, s_add, 2u);
    prints_git(root, s_first, 4u);
    (void)query_file_written(path, s_changed, sizeof(s_changed) - 1u);
    cli_full(root, "b.js", path);
    (void)query_file_written(path, s_new, sizeof(s_new) - 1u);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_prints_added(root, &out, &said), "the prints of a tree are read");
    cli_check_text(s_out, out.at,
                   "[{\"path\":\"a.py\",\"line\":2,\"text\":\"    print(x)\"},"
                   "{\"path\":\"b.js\",\"line\":1,\"text\":\"console.log(a);\"}]",
                   "the print already committed is not one the change added");
}

int main(void)
{
    cli_check(prints_line("a.py", "    print(total)"), "Python's print prints");
    cli_check(!prints_line("a.py", "    printer(total)"), "a longer name is no print");
    cli_check(!prints_line("a.py", "    # print(total)"), "a comment is no print");
    cli_check(prints_line("a.js", "  console.log('x', y);"), "JavaScript's console.log prints");
    cli_check(prints_line("a.rs", "    dbg!(&value);"), "Rust's dbg! prints");
    cli_check(prints_line("a.c", "  printf(\"%d\\n\", n);"), "C's printf prints");
    cli_check(prints_line("a.c", "  fprintf(stderr, \"x\");"), "and its fprintf");
    cli_check(prints_line("a.go", "\tfmt.Println(x)"), "Go's fmt.Println prints");
    cli_check(prints_line("a.rb", "puts value"), "Ruby's puts prints");
    cli_check(!prints_line("a.rb", "putsch value"), "and not a longer word");
    cli_check(!prints_line("a.md", "print(x)"), "a file of no language read here has none");
    static const char s_diff[] = "diff --git a/a.py b/a.py\n--- a/a.py\n+++ b/a.py\n@@ -3,0 +4,2 @@ def f():\n"
                                 "+    print(x)\n+    y = 1\n@@ -10 +12 @@\n-    old\n+    print(\"new\")\n";
    CliLine out = {s_out, sizeof(s_out), 0ull};
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_prints_put_in(s_diff, sizeof(s_diff) - 1u, &out, &said), "the prints of a diff are read");
    cli_check_text(s_out, out.at,
                   "[{\"path\":\"a.py\",\"line\":3,\"text\":\"    print(x)\"},"
                   "{\"path\":\"a.py\",\"line\":11,\"text\":\"    print(\\\"new\\\")\"}]",
                   "the lines a diff puts in are read with their lines in the file as it stands");
    prints_added();
    return cli_checked();
}
