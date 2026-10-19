// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust_test.c: the trust's check: a hook is a program and not git's own monitor, a folder is under itself and not
// under a name that starts the same, and the code a tree runs is found and the nearest answer stands
#include "../../../../../ui/cli/src/machine/trust.h"
#include "cli_check.h"

// the file `name` of the folder written with `text`
static void trust_file(const char *dir, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(dir, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the answer `tree` has by the file of answers at `kept`: 1 trusted, 0 not, -1 none
static int trust_answer(const char *kept, const char *tree)
{
    int trusted = 0;
    return cli_trust_answer_in(kept, tree, &trusted) ? trusted : -1;
}

// the code a tree runs is found and the nearest answer stands
static void trust_answers(void)
{
    static char s_code[8][CLI_PATH_LONGEST];
    char home[CLI_PATH_LONGEST];
    char trees[CLI_PATH_LONGEST];
    char tree[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char kept[CLI_PATH_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("trust", home);
    cli_full(home, "trees", trees);
    cli_full(trees, "crate", tree);
    cli_full(tree, "src", path);
    (void)query_folder_made(path);
    trust_file(tree, "Cargo.toml", "[package]\nname = \"crate\"\n");
    trust_file(tree, "build.rs", "fn main() {}\n");
    trust_file(tree, "src/lib.rs", "");
    trust_file(tree, "prettier.config.js", "module.exports = {};\n");
    const unsigned int found = cli_trust_looked_for(tree, s_code, 8u);
    cli_check(
        (found == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_code[0], .other = "build.rs", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_code[1], .other = "prettier.config.js", .cap = CLI_CELLS_POOL})),
        "the code a tree runs is found");
    cli_full(home, "trust.json", kept);
    cli_check(trust_answer(kept, tree) == -1, "a tree with no answer has none");
    (void)cli_trust_set_in(kept, trees, 1, &said);
    cli_check(trust_answer(kept, tree) == 1, "the folder above's answer holds for the tree");
    (void)cli_trust_set_in(kept, tree, 0, &said);
    cli_check(trust_answer(kept, tree) == 0, "the tree's own answer stands over the folder's");
    (void)cli_trust_set_in(kept, trees, 1, &said);
    cli_check(trust_answer(kept, tree) == 1, "the folder's answer takes the place of the tree's");
    cli_full(tree, ".git", path);
    (void)query_folder_made(path);
    trust_file(tree, ".git/config", "[core]\n\tfsmonitor = watch\n");
    // the tree lies inside the folders this machine's own answers may trust, so git is asked under a home of the
    // check's own that holds no answer
    char bare_home[CLI_PATH_LONGEST];
    cli_full(home, "bare_home", bare_home);
    (void)query_folder_made(bare_home);
    (void)query_environment_set("ORIOR_HOME", bare_home);
    cli_full(tree, "src", path);
    const char *words[2] = {NULL, NULL};
    cli_check(
        (cli_trust_git_words(path, words) == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "-c", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = words[1], .other = "core.fsmonitor=false", .cap = CLI_CELLS_POOL})),
        "git in a tree not trusted runs no hook");
}

int main(void)
{
    cli_check(cli_trust_names_a_hook("[core]\n\tfsmonitor = .git/hooks/watch\n"), "a hook is a program");
    cli_check(!cli_trust_names_a_hook("[core]\n\tfsmonitor = true\n"), "and not git's own monitor");
    cli_check(!cli_trust_names_a_hook("[remote \"origin\"]\n\tfsmonitor = run\n"), "nor a key of another section");
    cli_check(cli_trust_under("d:/trees/one", "d:/trees") && cli_trust_under("d:/trees", "d:/trees"),
              "a folder is under itself and holds what is under it");
    cli_check(!cli_trust_under("d:/treesmore/one", "d:/trees"), "and not a name that starts the same");
    trust_answers();
    return cli_checked();
}
