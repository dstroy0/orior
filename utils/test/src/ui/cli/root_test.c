// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// root_test.c: the tree's check: a folder and a file mounted beside it are reached by their names and contained inside
#include "cli_check.h"

// the file at `path` read whole and checked against `wanted`
static void root_read_checked(const char *path, const char *wanted, const char *what)
{
    char text[256];
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text));
    cli_check_text(text, (size == QRY_MEMBER_UNREAD) ? 0ull : size, wanted, what);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char base[CLI_PATH_MAX_LEN];
    char tree[CLI_PATH_MAX_LEN];
    char other[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char lone[CLI_PATH_MAX_LEN];
    cli_check_folder("mounts", base);
    cli_full(base, "tree", tree);
    cli_full(base, "other", other);
    cli_full(other, "sub", path);
    (void)query_folder_made(path);
    (void)query_folder_made(tree);
    cli_full(other, "sub/a.txt", path);
    (void)query_file_written(path, "a", 1ull);
    cli_full(base, "lone.txt", lone);
    (void)query_file_written(lone, "lone", 4ull);
    char folder[CLI_NAME_MAX_LEN];
    char file[CLI_NAME_MAX_LEN];
    char again[CLI_NAME_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check(cli_mounted(other, folder, &err_msg) && cli_mounted(lone, file, &err_msg), "a folder and a file mount");
    cli_check(cli_mounted(other, again, &err_msg) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = again, .other = folder, .cap = CLI_CELLS_POOL})),
              "a folder mounted again retains its name");
    char relative[CLI_PATH_MAX_LEN];
    VerbaLinea named = {relative, sizeof(relative), 0ull};
    mmgr_line_put(&named, "@");
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/sub/a.txt");
    (void)mmgr_line_finish(&named);
    char reached[CLI_PATH_MAX_LEN];
    cli_check(cli_inside(tree, relative, reached, &err_msg), "the mounted file is reached by its name");
    root_read_checked(reached, "a", "it reads as the file it is");
    char file_word[CLI_PATH_MAX_LEN];
    VerbaLinea word = {file_word, sizeof(file_word), 0ull};
    mmgr_line_put(&word, "@");
    mmgr_line_put(&word, file);
    (void)mmgr_line_finish(&word);
    char lone_reached[CLI_PATH_MAX_LEN];
    cli_check(cli_inside(tree, file_word, lone_reached, &err_msg), "a mounted file is reached by its name alone");
    root_read_checked(lone_reached, "lone", "it reads as the file it is");
    char back[CLI_PATH_MAX_LEN];
    cli_relative(tree, reached, back);
    cli_check_text(back, cellul.len(&(CatenaFinitaCfg){.src = back, .cap = CLI_CELLS_POOL}), relative,
                   "the path written relative to the tree names the mount");
    named.at = 0ull;
    mmgr_line_put(&named, "@");
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/../escape");
    (void)mmgr_line_finish(&named);
    err_msg.at = 0ull;
    cli_check(!cli_inside(tree, relative, path, &err_msg), "a path that climbs out of a mount is refused");
    err_msg.at = 0ull;
    cli_check(!cli_inside(tree, "@nothing/a", path, &err_msg), "a mount that is not there is refused");
    cli_full(tree, "x/y.rs", path);
    cli_relative(tree, path, back);
    cli_check_text(back, cellul.len(&(CatenaFinitaCfg){.src = back, .cap = CLI_CELLS_POOL}), "x/y.rs",
                   "a path of the tree is written from its top");
    cli_unmounted(folder);
    cli_unmounted(file);
    named.at = 0ull;
    mmgr_line_put(&named, "@");
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/sub/a.txt");
    (void)mmgr_line_finish(&named);
    err_msg.at = 0ull;
    cli_check(!cli_inside(tree, relative, path, &err_msg), "a mount taken away is no longer reached");
    return cli_checked();
}
