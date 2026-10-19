// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_test.c: the deployments' check: patterns leave out names, paths and folders, a release is named by its UTC
// time, a place is a machine or a folder here, a tar stream holds each file with its mode and a long name in a PAX
// header, and a difference shows the lines taken out and put in
#include "../../../../../ui/cli/src/deploy/deploy_internal.h"
#include "cli_check.h"

static char s_said[1024];
static CliLine s_said_line = {s_said, sizeof(s_said), 0ull};
static unsigned char s_stream[8192];

// the pattern that leaves out `path`, the empty text where none does
static const char *deploy_left_out(const char *path)
{
    static const char *const s_patterns[4] = {".git/", "*.map", "drafts/", "secret/key.txt"};
    const char *const pattern = cli_deploy_left_out_by(s_patterns, 4u, path);
    return (pattern != NULL) ? pattern : "";
}

// the release named `seconds` after 1970 began, into `out`, which holds 32
static const char *deploy_release(unsigned long long seconds, char *out)
{
    CliLine line = {out, 32u, 0ull};
    cli_deploy_release_name(seconds, &line);
    (void)cli_line_closed(&line);
    return out;
}

// whether `text` reads as a place here, 1, or a machine, 0; -1 where it reads as none
static int deploy_here(const char *text)
{
    CliDeployPlace place;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_deploy_place(text, "C:/tree", &place, &said) ? place.here : -1;
}

// a line the run says kept after the others, `|` between them
static void deploy_said(void *context, const char *line, unsigned long long size)
{
    (void)context;
    cli_line_text(&s_said_line, (s_said_line.at > 0ull) ? "|" : "");
    cli_line_counted(&s_said_line, line, size);
}

// a tar stream holds each file with its mode and a long name in a PAX header
static void deploy_tar(void)
{
    static const char s_script[] = "#!/bin/sh\r\necho hi\r\n";
    char dir[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char tar[CLI_PATH_LONGEST];
    char long_name[256];
    CliLine name = {long_name, sizeof(long_name), 0ull};
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        cli_line_text(&name, "folder");
    }
    char folder[CLI_PATH_LONGEST];
    (void)cli_line_closed(&name);
    cli_check_folder("deploy_tar", dir);
    cli_full(dir, long_name, folder);
    (void)query_folder_made(folder);
    cli_line_text(&name, "/x.txt");
    (void)cli_line_closed(&name);
    cli_full(dir, "run.sh", path);
    (void)query_file_written(path, s_script, sizeof(s_script) - 1u);
    cli_full(dir, long_name, path);
    (void)query_file_written(path, s_script, sizeof(s_script) - 1u);
    cli_full(dir, "stream.tar", tar);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_lock_take(&g_cli_deploy_lock);
    g_cli_deploy.root = dir;
    g_cli_deploy.deployment.from = ".";
    CliDeploySent sent[2] = {{"run.sh", 0755u, ""}, {long_name, 0644u, ""}};
    g_cli_deploy.sent = sent;
    g_cli_deploy.sents = 2u;
    const int written = cli_deploy_tar(tar, NULL, 0u, 1000ull, &said);
    g_cli_deploy.sent = NULL;
    g_cli_deploy.sents = 0u;
    cli_lock_give(&g_cli_deploy_lock);
    cli_check(written, "the tar stream is written");
    const unsigned long long size = qry_member_file_read(tar, s_stream, sizeof(s_stream));
    const unsigned long long read = (size == QRY_MEMBER_UNREAD) ? 0ull : size;
    cli_check((read >= 2048ull) && (memor.cmp(&(MemoriaCfg){.src = s_stream, .other = "run.sh", .bytes = 6u}) == 0) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_stream + 100, .other = "0000755", .bytes = 7u}) == 0),
              "a file's header names it and its mode");
    cli_check((read >= 2048ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_stream + 512, .other = s_script, .bytes = 20u}) == 0),
              "its bytes follow its header");
    char pax[300];
    CliLine pax_line = {pax, sizeof(pax), 0ull};
    cli_line_text(&pax_line, "path=");
    cli_line_text(&pax_line, long_name);
    (void)cli_line_closed(&pax_line);
    int named = 0;
    for (unsigned long long at = 1536ull; (read >= 2048ull) && (at + pax_line.at <= 2048ull); at += 1ull)
    {
        named |= memor.cmp(&(MemoriaCfg){.src = s_stream + at, .other = pax, .bytes = pax_line.at}) == 0;
    }
    cli_check((read >= 2048ull) && (s_stream[1024 + 156] == 'x') && named, "a long name goes in a PAX header");
    cli_check((read % 512ull) == 0ull, "and the stream is whole blocks");
}

int main(void)
{
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = deploy_left_out("app.js.map"), .other = "*.map", .cap = CLI_CELLS_POOL}),
        "a pattern leaves out a name");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = deploy_left_out("deep/app.js.map"), .other = "*.map", .cap = CLI_CELLS_POOL}),
              "in any folder");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = deploy_left_out("drafts/one.md"), .other = "drafts/", .cap = CLI_CELLS_POOL}),
              "a folder's pattern leaves out what it holds");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = deploy_left_out("site/drafts/one.md"), .other = "drafts/", .cap = CLI_CELLS_POOL}),
              "wherever it stands");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = deploy_left_out("secret/key.txt"), .other = "secret/key.txt", .cap = CLI_CELLS_POOL}),
              "a path leaves out its file");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = deploy_left_out(".git/HEAD"), .other = ".git/", .cap = CLI_CELLS_POOL}),
        "and git's folder");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = deploy_left_out("index.html"), .other = "", .cap = CLI_CELLS_POOL}),
              "a file no pattern names goes");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = deploy_left_out("drafts.html"), .other = "", .cap = CLI_CELLS_POOL}),
              "and a name that starts as a folder does");
    char release[32];
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = deploy_release(0ull, release), .other = "19700101-000000", .cap = CLI_CELLS_POOL}),
              "a release is named by its UTC time");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = deploy_release(1791672293ull, release), .other = "20261010-224453", .cap = CLI_CELLS_POOL}),
              "to the second");
    cli_check(deploy_here("me@web1:/srv/site") == 0, "a place on a machine is the machine's");
    cli_check(deploy_here("docker:web:/srv") == 0, "and in a container");
    cli_check(deploy_here("C:/www/site") == 1, "a folder is a place here");
    cli_check(deploy_here("./out") == 1, "and one under the tree");
    deploy_tar();
    cli_lock_take(&g_cli_deploy_lock);
    g_cli_deploy.say = deploy_said;
    g_cli_deploy.context = NULL;
    cli_deploy_difference("a\nb\nc\n", 6ull, "a\nB\nc\n", 6ull);
    cli_lock_give(&g_cli_deploy_lock);
    cli_check_text(s_said, s_said_line.at, "  @@ line 2|  - b|  + B",
                   "a difference shows the lines taken out and put in");
    return cli_checked();
}
