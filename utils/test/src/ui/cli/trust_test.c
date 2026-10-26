// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust_test.c: the trust's check: a hook is a program and not git's own monitor, a folder is under itself and not
// under a name that starts the same, and the code a tree runs is found and the nearest answer stands
#include "../../../../../ui/cli/src/machine/trust.h"
#include "cli_check.h"

// the file `name` of the folder written with `text`
static void trust_file(const char *dir, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(dir, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the answer `tree` has by the file of answers at `answers_file`: 1 trusted, 0 not, -1 none
static int trust_answer(const char *answers_file, const char *tree)
{
    int trusted = 0;
    return cli_trust_answer_in(answers_file, tree, &trusted) ? trusted : -1;
}

// the code a tree runs is found and the nearest answer stands
static void trust_answers(void)
{
    char home[CLI_PATH_MAX_LEN];
    char trees[CLI_PATH_MAX_LEN];
    char tree[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char answers_file[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("trust", home);
    cli_full(home, "trees", trees);
    cli_full(trees, "crate", tree);
    cli_full(tree, "src", path);
    (void)query_folder_made(path);
    trust_file(tree, "Cargo.toml", "[package]\nname = \"crate\"\n");
    trust_file(tree, "build.rs", "fn main() {}\n");
    trust_file(tree, "src/lib.rs", "");
    trust_file(tree, "prettier.config.js", "module.exports = {};\n");
    CliTrustCode code = {NULL, 0ull, 0u};
    const int looked = cli_trust_looked_for(tree, &code);
    const char *const first = (code.paths != NULL) ? code.paths : "";
    const char *const second = first + cellul.len(&(CatenaFinitaCfg){.src = first, .cap = CLI_CELLS_POOL}) + 1u;
    cli_check(looked && (code.count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "build.rs", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = second, .other = "prettier.config.js", .cap = CLI_CELLS_POOL})),
              "the code a tree runs is found, in a cell of its paths");
    cli_trust_code_given(&code);
    cli_check(g_cli_trust_cells.in_use == 0ull, "and the cell and the files read are given back");
    cli_full(home, "trust.json", answers_file);
    cli_check(trust_answer(answers_file, tree) == -1, "a tree with no answer has none");
    (void)cli_trust_set_in(answers_file, trees, 1, &err_msg);
    cli_check(trust_answer(answers_file, tree) == 1, "the folder above's answer applies to the tree");
    (void)cli_trust_set_in(answers_file, tree, 0, &err_msg);
    cli_check(trust_answer(answers_file, tree) == 0, "the tree's own answer stands over the folder's");
    (void)cli_trust_set_in(answers_file, trees, 1, &err_msg);
    cli_check(trust_answer(answers_file, tree) == 1, "the folder's answer takes the place of the tree's");
    cli_full(tree, ".git", path);
    (void)query_folder_made(path);
    trust_file(tree, ".git/config", "[core]\n\tfsmonitor = watch\n");
    // the tree lies inside the folders this machine's own answers may trust; git is therefore queried under a home of
    // the check's own that contains no answer
    char bare_home[CLI_PATH_MAX_LEN];
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
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check(cli_trust_names_a_hook("[core]\n\tfsmonitor = .git/hooks/watch\n"), "a hook is a program");
    cli_check(!cli_trust_names_a_hook("[core]\n\tfsmonitor = true\n"), "and not git's own monitor");
    cli_check(!cli_trust_names_a_hook("[remote \"origin\"]\n\tfsmonitor = run\n"), "nor a key of another section");
    cli_check(cli_trust_under("d:/trees/one", "d:/trees") && cli_trust_under("d:/trees", "d:/trees"),
              "a folder is under itself and contains what is under it");
    cli_check(!cli_trust_under("d:/treesmore/one", "d:/trees"), "and not a name that starts the same");
    trust_answers();
    return cli_checked();
}
