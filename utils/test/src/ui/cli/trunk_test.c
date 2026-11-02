// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trunk_test.c: the tree: the filesystem is its trunk and its files are its branches. It is listed one time as it
// opens, and each change after joins or leaves the one branch it names at its closest parent, nothing else listed
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/code/symbols_tree.h"
#include "../../../../../ui/cli/src/editor/files_internal.h"
#include "../../../../../ui/cli/src/editor/watch_internal.h"
#include "cli_check.h"

static CliLock s_lock = CLI_LOCK_INIT;
static unsigned int s_rounds;

// the file `name` of the tree written with `text`, and its folder made where `folder` names one
static void trunk_file(const char *root, const char *folder, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    if (folder != NULL)
    {
        cli_full(root, folder, path);
        (void)query_folder_made(path);
    }
    cli_full(root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the file `name` of the tree taken out
static void trunk_removed(const char *root, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(root, name, path);
    (void)query_removed(path);
}

// what the trunk holds at `path` mirrored, alone
static void trunk_mirrored(const char *root, const char *path)
{
    const char *const paths[1] = {path};
    cli_tree_mirrored(root, paths, 1u);
}

// a repository at `name` with two folders of files and an ignore rule, into `root`, sized for CLI_PATH_MAX_LEN
static void trunk_repository(const char *name, char *root)
{
    static const char *const s_init[2] = {"init", "-q"};
    char out_room[256];
    char err_room[256];
    VerbaLinea out = {out_room, sizeof(out_room), 0ull};
    VerbaLinea err_msg = {err_room, sizeof(err_room), 0ull};
    cli_check_folder(name, root);
    trunk_file(root, NULL, ".gitignore", "*.o\nout/\n");
    trunk_file(root, NULL, "top.txt", "t");
    trunk_file(root, "src", "src/a.c", "a");
    trunk_file(root, "lib", "lib/b.c", "b");
    cli_check(cli_git_run(root, s_init, 2u, &out, &err_msg), "a repository is made");
    cli_tree_forgotten();
    cli_tree_listed(root);
}

// a round of changes taken, counted
static void trunk_taken(void *context, const char *batch, unsigned long long size)
{
    (void)context;
    (void)batch;
    (void)size;
    cli_lock_take(&s_lock);
    s_rounds += 1u;
    cli_lock_give(&s_lock);
}

static unsigned int trunk_rounds(void)
{
    cli_lock_take(&s_lock);
    const unsigned int rounds = s_rounds;
    cli_lock_give(&s_lock);
    return rounds;
}

// the tree is listed as it opens, each file and folder a branch
static void trunk_opened(void)
{
    char root[CLI_PATH_MAX_LEN];
    trunk_repository("trunk_opened", root);
    cli_check(cli_tree_git(), "git lists the tree");
    cli_check(cli_tree_has_file("src/a.c") && cli_tree_has_file("top.txt"), "each file a branch");
    cli_check(cli_tree_has_folder("src") && cli_tree_has_folder("lib"), "and each folder");
    cli_check(!cli_tree_has_folder("src/a.c") && !cli_tree_has_file("src"), "and neither taken for the other");
    unsigned int count = 0u;
    unsigned long long bytes = 0ull;
    cli_tree_files_counted(&count, &bytes);
    cli_check(count == 4u, "the files are counted");
    char path[CLI_PATH_MAX_LEN];
    unsigned int at = 0u;
    unsigned int gone_through = 0u;
    while (cli_tree_next_file(&at, path))
    {
        gone_through += 1u;
    }
    cli_check(gone_through == count, "and every file is gone through");
    const CliFiles *const files = cli_files_view(root);
    cli_check((files != NULL) && (files->count == 4u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_files_at(files, 0u), .other = ".gitignore", .cap = 64u})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_files_at(files, 3u), .other = "top.txt", .cap = 64u})),
              "a call lays them flat, sorted");
    cli_files_let(files);
    cli_check(mmgr_owner_in_use(&g_cli_files_views_cells) == 0u, "and nothing of the list stands after it");
}

// a tree stands for as long as nothing changes
static void trunk_stands(void)
{
    char root[CLI_PATH_MAX_LEN];
    trunk_repository("trunk_stands", root);
    const unsigned long long listed_at = cli_tree_listed_at();
    cli_check(listed_at != 0ull, "the tree is listed as it opens");
    query_paused(4200u);
    cli_files_let(cli_files_view(root));
    query_paused(500u);
    cli_tree_listed(root);
    cli_check(cli_tree_listed_at() == listed_at, "and not listed again for the time that passed");
}

// a file added walks the file, not the tree, and joins at its closest parent
static void trunk_joined(void)
{
    char root[CLI_PATH_MAX_LEN];
    trunk_repository("trunk_joined", root);
    const unsigned long long listed_at = cli_tree_listed_at();
    const unsigned int lib = cli_branch_of("lib");
    const unsigned int file = cli_branch_of("lib/b.c");
    trunk_file(root, NULL, "src/new.c", "n");
    trunk_mirrored(root, "src/new.c");
    cli_check(cli_tree_has_file("src/new.c"), "a file added joins");
    cli_check(cli_tree_listed_at() == listed_at, "and the tree is not listed for it");
    cli_check((cli_branch_of("lib") == lib) && (cli_branch_of("lib/b.c") == file), "the other branches stand");
    const unsigned int parent = cli_branch_of("src");
    cli_check(g_cli_branches[cli_branch_of("src/new.c")].parent == parent, "its closest parent is its folder");
    cli_check(g_cli_branches[parent].below == 2u, "which counts it");
    trunk_file(root, NULL, "src/made.o", "o");
    trunk_mirrored(root, "src/made.o");
    cli_check(!cli_tree_has_file("src/made.o"), "a file git ignores does not join");
    trunk_file(root, "deep", "deep/x.c", "x");
    trunk_file(root, "deep/er", "deep/er/y.c", "y");
    trunk_mirrored(root, "deep/er/y.c");
    cli_check(cli_tree_has_file("deep/er/y.c") && cli_tree_has_folder("deep/er") && cli_tree_has_folder("deep"),
              "a file in a folder not yet joined brings its folders");
    cli_check(!cli_tree_has_file("deep/x.c"), "and no other file of them");
    trunk_file(root, "came", "came/in.c", "c");
    trunk_file(root, "came/under", "came/under/in.c", "c");
    trunk_mirrored(root, "came");
    cli_check(cli_tree_has_file("came/in.c") && cli_tree_has_file("came/under/in.c"), "a folder come in joins its files");
    cli_check(cli_tree_listed_at() == listed_at, "and the tree is still not listed");
    trunk_mirrored(root, "src/new.c");
    cli_check(g_cli_branches[parent].below == 2u, "a file that joined already joins once");
}

// a file taken out leaves, and a folder left with nothing under it leaves after it
static void trunk_left(void)
{
    char root[CLI_PATH_MAX_LEN];
    trunk_repository("trunk_left", root);
    const unsigned long long listed_at = cli_tree_listed_at();
    trunk_file(root, "lone", "lone/one.c", "o");
    trunk_file(root, "lone/er", "lone/er/two.c", "t");
    trunk_mirrored(root, "lone");
    const unsigned int place = cli_branch_of("lone/er/two.c");
    trunk_removed(root, "lone/er/two.c");
    trunk_removed(root, "lone/er");
    trunk_mirrored(root, "lone/er/two.c");
    cli_check(!cli_tree_has_file("lone/er/two.c"), "a file taken out leaves");
    cli_check(!cli_tree_has_folder("lone/er"), "and the folder it left empty");
    cli_check(cli_tree_has_folder("lone"), "and no folder that holds another");
    trunk_file(root, NULL, "src/after.c", "a");
    trunk_mirrored(root, "src/after.c");
    cli_check(!g_cli_branches[place].used || (cli_branch_of("src/after.c") == place) ||
                  (cli_branch_of("src/after.c") < g_cli_branch_count),
              "a place left is reused");
    trunk_removed(root, "lone/one.c");
    trunk_removed(root, "lone");
    trunk_mirrored(root, "lone");
    cli_check(!cli_tree_has_folder("lone") && !cli_tree_has_file("lone/one.c"), "a folder taken out takes all under it");
    cli_check(cli_tree_listed_at() == listed_at, "and the tree is not listed for any of it");
}

// a listing's rules changed lists the tree again, and only what differs moves
static void trunk_relisted(void)
{
    char root[CLI_PATH_MAX_LEN];
    trunk_repository("trunk_relisted", root);
    const unsigned int kept = cli_branch_of("lib/b.c");
    const CarcerMovableSlot *const kept_cell = g_cli_branches[kept].cell;
    trunk_file(root, NULL, ".gitignore", "*.o\nout/\nsrc/\n");
    cli_tree_relisted(root, NULL, NULL);
    cli_check(!cli_tree_has_file("src/a.c") && !cli_tree_has_folder("src"), "what the rules now leave out leaves");
    cli_check((cli_branch_of("lib/b.c") == kept) && (g_cli_branches[kept].cell == kept_cell),
              "and what they keep stands in its cell");
}

// a tree git cannot read walks its folders, and a path through a folder it does not go into does not join
static void trunk_no_git(void)
{
    char root[CLI_PATH_MAX_LEN];
    cli_check_folder("trunk_no_git", root);
    trunk_file(root, "src", "src/a.c", "a");
    trunk_file(root, "target", "target/made.c", "m");
    cli_tree_forgotten();
    cli_tree_listed(root);
    cli_check(!cli_tree_git() && cli_tree_has_file("src/a.c"), "a tree git cannot read is walked");
    cli_check(!cli_tree_has_file("target/made.c"), "less the folders a walk does not go into");
    trunk_file(root, NULL, "src/b.c", "b");
    trunk_mirrored(root, "src/b.c");
    cli_check(cli_tree_has_file("src/b.c"), "a file added joins");
    trunk_file(root, NULL, "target/more.c", "m");
    trunk_mirrored(root, "target/more.c");
    cli_check(!cli_tree_has_file("target/more.c"), "and not one under a folder a walk does not go into");
}

// a tree whose files were stamped, as where nothing reports its changes, is still walked: a stamp says when a file was
// last seen, and only a walk says what it declares
static void trunk_stamped_first(void)
{
    static char s_found[1u << 14u];
    char root[CLI_PATH_MAX_LEN];
    trunk_repository("trunk_stamped_first", root);
    trunk_file(root, NULL, "src/walked.py", "def stamped_first():\n    pass\n");
    trunk_mirrored(root, "src/walked.py");
    cli_check(cli_watch_first_reading(root), "the tree's files are stamped before it is walked");
    cli_tree_walked(root);
    for (unsigned int tries = 0u; cli_tree_unwalked() && (tries < 1000u); tries += 1u)
    {
        query_paused(10u);
    }
    VerbaLinea out = {s_found, sizeof(s_found), 0ull};
    cli_tree_declarations_found("stamped_first", 10u, &out);
    (void)mmgr_line_finish(&out);
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = s_found, .other = "src/walked.py", .cap = sizeof(s_found), .other_cap = 64u}) != NULL,
              "and the walk reads what each declares");
}

// a file added with a watch running joins, and the tree is not listed for it
static void trunk_watched(void)
{
    char root[CLI_PATH_MAX_LEN];
    trunk_repository("trunk_watched", root);
    const unsigned long long listed_at = cli_tree_listed_at();
    cli_watch_start(root, trunk_taken, NULL);
    query_paused(200u);
    const unsigned int before = trunk_rounds();
    trunk_file(root, NULL, "src/new.c", "n");
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while ((!cli_tree_has_file("src/new.c") || (trunk_rounds() == before)) && (cli_clock_milliseconds() < until))
    {
        query_paused(20u);
    }
    cli_check(trunk_rounds() != before, "the watch reports the file added");
    cli_check(cli_tree_has_file("src/new.c"), "and it joins");
    cli_check(cli_tree_listed_at() == listed_at, "and the tree is not listed for it");
    trunk_removed(root, "src/new.c");
    const unsigned long long left_by = cli_clock_milliseconds() + 10000ull;
    while (cli_tree_has_file("src/new.c") && (cli_clock_milliseconds() < left_by))
    {
        query_paused(20u);
    }
    cli_check(!cli_tree_has_file("src/new.c"), "taken out, it leaves");
    cli_watch_stop();
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    trunk_opened();
    trunk_stands();
    trunk_joined();
    trunk_left();
    trunk_relisted();
    trunk_no_git();
    trunk_stamped_first();
    trunk_watched();
    cli_tree_forgotten();
    return cli_checked();
}
