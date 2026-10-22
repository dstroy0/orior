// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_test.c: the editor's files: a paste copies or moves and writes over nothing; a file and a folder go to a tree
// on another machine a part at a time; a file and a folder brought in are copied whole, each file telling its end; a
// tree in a folder its repository ignores lists its files from the disk; and a file in a folder made after git's view
// was read is tracked, and one in an ignored folder is not
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/editor/files.h"
#include "../../../../../ui/cli/src/editor/files_search_hits.h"
#include "../../../../../ui/cli/src/text/base64.h"
#include "cli_check.h"

// What a bringing told: each path, the bytes done and its size
typedef struct
{
    char path[64][CLI_PATH_LONGEST];
    unsigned long long done[64];
    unsigned long long size[64];
    unsigned int count;
} FilesHeard;

static FilesHeard s_heard;
static CliLock s_lock = CLI_LOCK_INIT;
static unsigned char s_big[3u << 20u];
static unsigned char s_read[(3u << 20u) + 1u];
static unsigned char s_part[2u << 20u];
static char s_out[1u << 16u];
static char s_there[CLI_PATH_LONGEST];

// a file of the tree `root` written at `name`, `size` bytes of `bytes`
static void files_written(const char *root, const char *name, const void *bytes, unsigned long long size)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    (void)query_file_written(path, bytes, size);
}

// the folder `name` of the tree `root` made, with every folder above it
static void files_folder(const char *root, const char *name, char *path)
{
    cli_full(root, name, path);
    (void)query_folder_made(path);
}

// the file `name` of `root` read into s_read: its size, or QRY_MEMBER_UNREAD
static unsigned long long files_read(const char *root, const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    return qry_member_file_read(path, s_read, sizeof(s_read) - 1u);
}

// whether the file `name` of `root` holds `text`
static int files_holds(const char *root, const char *name, const char *text)
{
    const unsigned long long size = files_read(root, name);
    return (size == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = s_read, .other = text, .bytes = size}) == 0);
}

// whether the file `name` of `root` is there
static int files_there(const char *root, const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    return query_is_file(path);
}

// the pairs a paste wrote, each `from>to`, `-` for a from of null, joined by `|`, into s_out
static const char *files_pairs(const char *written, unsigned long long size)
{
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(written, size, &read);
    static char s_pairs[4096];
    CliLine line = {s_pairs, sizeof(s_pairs), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, read, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int pair = cli_json_item(json, read, at);
        const char *const from = cli_json_string(json, cli_json_member(json, pair, "from"), NULL);
        const char *const to = cli_json_string(json, cli_json_member(json, pair, "to"), NULL);
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, (from != NULL) ? from : "-");
        cli_line_character(&line, '>');
        cli_line_text(&line, (to != NULL) ? to : "-");
    }
    (void)cli_line_closed(&line);
    return s_pairs;
}

// `from`, a path of the run's, pasted into `into` of `root`: 1, its pairs into s_out, or 0
static int files_pasted(const char *root, const char *into, const char *from, int moving)
{
    char why[1024];
    char written[4096];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {written, sizeof(written), 0ull};
    const char *const froms[1] = {from};
    if (!cli_files_paste(root, into, froms, 1u, moving, &out, &said))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = s_out,
        .src = files_pairs(written, out.at),
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = files_pairs(written, out.at), .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// a paste copies or moves and writes over nothing
static void files_paste(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("files_paste", root);
    files_folder(root, "a/inner", path);
    files_folder(root, "b", path);
    files_written(root, "a/notes.txt", "one", 3ull);
    files_written(root, "a/inner/deep.rs", "two", 3ull);
    cli_full(root, "a/notes.txt", path);
    cli_check(files_pasted(root, "a", path, 0) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_out, .other = "a/notes.txt>a/notes (2).txt", .cap = CLI_CELLS_POOL})) &&
                  files_holds(root, "a/notes (2).txt", "one"),
              "a copy beside its own name is numbered");
    cli_full(root, "a", path);
    cli_check(files_pasted(root, "b", path, 0) && files_holds(root, "b/a/inner/deep.rs", "two"),
              "a folder is copied with all it holds");
    cli_check(!files_pasted(root, "a/inner", path, 0), "a folder is not pasted into itself");
    cli_full(root, "a/notes.txt", path);
    cli_check(
        files_pasted(root, "b", path, 1) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "a/notes.txt>b/notes.txt", .cap = CLI_CELLS_POOL})) &&
            !files_there(root, "a/notes.txt"),
        "a move takes it from where it was");
    cli_full(root, "b/notes.txt", path);
    cli_check(
        files_pasted(root, "b", path, 1) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "b/notes.txt>b/notes.txt", .cap = CLI_CELLS_POOL})),
        "a move to where it is leaves it");
    char outside[CLI_PATH_LONGEST];
    cli_check_folder("files_paste_out", outside);
    files_written(outside, "out.txt", "three", 5ull);
    cli_full(outside, "out.txt", path);
    cli_check(files_pasted(root, "", path, 1) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "->", .cap = CLI_CELLS_POOL})) &&
                  !query_is_file(path),
              "a file from outside the tree has no from");
}

// what a bringing tells kept
static void files_told(void *context, const char *path, unsigned long long done, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_lock);
    if (s_heard.count < 64u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_heard.path[s_heard.count],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        s_heard.done[s_heard.count] = done;
        s_heard.size[s_heard.count] = size;
        s_heard.count += 1u;
    }
    cli_lock_give(&s_lock);
}

// how many times `path` was told, and whether it was told with `done` of `size`
static unsigned int files_heard(const char *path, unsigned long long done, unsigned long long size, int *with)
{
    unsigned int count = 0u;
    *with = 0;
    for (unsigned int at = 0u; at < s_heard.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_heard.path[at], .other = path, .cap = CLI_CELLS_POOL}))
        {
            count += 1u;
            *with = *with || ((s_heard.done[at] == done) && (s_heard.size[at] == size));
        }
    }
    return count;
}

// a whole number member of the shared tree's node
static unsigned long long files_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int member = cli_json_member(json, node, name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; (member != CLI_JSON_NONE) && (at < json->node[member].length); at += 1ull)
    {
        value = (value * 10ull) + (unsigned long long)(json->text[json->node[member].text + at] - '0');
    }
    return value;
}

// the tree's side, as serve.c answers the calls there
static int files_call(void *context, const char *name, const char *arguments, CliLine *out, CliLine *said)
{
    (void)context;
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json =
        cli_check_parsed(arguments, cellul.len(&(CatenaFinitaCfg){.src = arguments, .cap = CLI_CELLS_POOL}), &root);
    const char *const path = cli_json_string(json, cli_json_member(json, root, "path"), NULL);
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "bring_name", .cap = CLI_CELLS_POOL}))
    {
        char found[CLI_PATH_LONGEST] = {0};
        const int named = cli_files_free_in(s_there, cli_check_member_text(root, "into"),
                                            cli_check_member_text(root, "name"), found, said);
        cli_json_quoted(out, found, cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}));
        return named;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "folder_create", .cap = CLI_CELLS_POOL}))
    {
        cli_line_text(out, "null");
        return cli_files_create_folder(s_there, (path != NULL) ? path : "", said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "file_part", .cap = CLI_CELLS_POOL}))
    {
        unsigned long long length = 0ull;
        const char *const data = cli_json_string(json, cli_json_member(json, root, "data"), &length);
        unsigned long long size = 0ull;
        const int last = cli_json_is(json, cli_json_member(json, root, "last"), CFG_JSON_TRUE);
        cli_line_text(out, "null");
        return (data != NULL) && cli_unbase64(data, length, s_part, sizeof(s_part), &size) &&
               cli_files_write_part(s_there, (path != NULL) ? path : "", files_whole(json, root, "at"), s_part, size,
                                    last, said);
    }
    cli_line_text(said, "no call ");
    cli_line_text(said, name);
    return 0;
}

// the `to` of each pair written, joined by `|`
static const char *files_tos(const char *written, unsigned long long size)
{
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(written, size, &read);
    CliLine line = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, read, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_check_member_text(cli_json_item(json, read, at), "to"));
    }
    (void)cli_line_closed(&line);
    return s_out;
}

// a file and a folder go to a tree on another machine a part at a time
static void files_bring_over(void)
{
    char base[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[1024];
    static char s_written[4096];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_written, sizeof(s_written), 0ull};
    cli_check_folder("files_bring_over", base);
    files_folder(base, "there/in", path);
    files_folder(base, "here/folder", path);
    const unsigned long long big = 5ull << 19u;
    for (unsigned long long at = 0ull; at < big; at += 1ull)
    {
        // a byte of the count, below 251
        s_big[at] = (unsigned char)(at % 251ull);
    }
    files_written(base, "here/big.bin", s_big, big);
    files_written(base, "here/folder/note.txt", "note", 4ull);
    files_written(base, "there/in/big.bin", "here first", 10ull);
    cli_full(base, "there", s_there);
    char big_path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_full(base, "here/big.bin", big_path);
    cli_full(base, "here/folder", folder);
    const char *const from[2] = {big_path, folder};
    s_heard.count = 0u;
    cli_check(
        cli_files_bring_over(files_call, NULL, "in", from, 2u, files_told, NULL, &out, &said) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = files_tos(s_written, out.at), .other = "in/big (2).bin|in/folder", .cap = CLI_CELLS_POOL})),
        "a file and a folder go to the tree on another machine");
    const unsigned long long size = files_read(s_there, "in/big (2).bin");
    cli_check((size == big) && (memor.cmp(&(MemoriaCfg){.src = s_read, .other = s_big, .bytes = big}) == 0),
              "the file whole, part by part");
    cli_check(files_holds(s_there, "in/folder/note.txt", "note"), "the folder's file");
    cli_check(files_holds(s_there, "in/big.bin", "here first"), "nothing is written over");
    cli_check(!files_there(s_there, "in/.big (2).bin.orior-part"), "the parts' file is moved over the file");
    int with = 0;
    cli_check((files_heard("in/big (2).bin", big, big, &with) >= 4u) && with, "each part told, and the end");
    cli_check(!cli_files_write_part(s_there, "in/late.txt", 5ull, "x", 1ull, 1, &said),
              "a part out of its place is refused");
}

// a file and a folder brought in are copied whole, each file telling its end
static void files_bring(void)
{
    char base[CLI_PATH_LONGEST];
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[1024];
    static char s_written[4096];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_written, sizeof(s_written), 0ull};
    cli_check_folder("files_bring", base);
    files_folder(base, "tree/in", path);
    files_folder(base, "outside/folder", path);
    memor.set(&(MemoriaCfg){.dst = s_big, .val = 7, .bytes = 3u << 20u});
    files_written(base, "outside/big.bin", s_big, 3ull << 20u);
    files_written(base, "outside/folder/note.txt", "note", 4ull);
    files_written(base, "tree/in/big.bin", "here first", 10ull);
    cli_full(base, "tree", root);
    char big_path[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_full(base, "outside/big.bin", big_path);
    cli_full(base, "outside/folder", folder);
    const char *const from[2] = {big_path, folder};
    s_heard.count = 0u;
    cli_check(
        cli_files_bring(root, "in", from, 2u, files_told, NULL, &out, &said) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = files_tos(s_written, out.at), .other = "in/big (2).bin|in/folder", .cap = CLI_CELLS_POOL})),
        "a file and a folder are brought in");
    cli_check(files_read(root, "in/big (2).bin") == (3ull << 20u), "the file whole");
    cli_check(files_holds(root, "in/folder/note.txt", "note") && files_holds(root, "in/big.bin", "here first"),
              "the folder's file, and nothing written over");
    int with = 0;
    (void)files_heard("in/big (2).bin", 3ull << 20u, 3ull << 20u, &with);
    cli_check(with, "the big file tells its end");
    (void)files_heard("in/folder/note.txt", 4ull, 4ull, &with);
    cli_check(with, "and the small one");
}

// a tree in a folder its repository ignores lists its files from the disk; and what is tracked
static void files_listing(void)
{
    char repo[CLI_PATH_LONGEST];
    char tree[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    static const char *const s_init[2] = {"init", "-q"};
    cli_check_folder("files_listing", repo);
    files_folder(repo, "runs/tree/src", path);
    files_written(repo, ".gitignore", "runs/\n", 6ull);
    files_written(repo, "runs/tree/a.js", "one", 3ull);
    files_written(repo, "runs/tree/src/b.js", "two", 3ull);
    cli_check(cli_git_run(repo, s_init, 2u, &out, &said), "a repository is made");
    cli_full(repo, "runs/tree", tree);
    const CliFiles *const files = cli_files_held(tree);
    cli_check(
        (files != NULL) && (files->count == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_files_at(files, 0u), .other = "a.js", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_files_at(files, 1u), .other = "src/b.js", .cap = CLI_CELLS_POOL})),
        "a tree its repository ignores lists its files from the disk");
    cli_files_let(files);
    const CliFiles *const kinds = cli_files_kinds_held(tree);
    cli_check((kinds != NULL) && (kinds->count == 3u) && cli_files_holds(kinds, ".js") &&
                  cli_files_holds(kinds, "a.js") && cli_files_holds(kinds, "b.js") && !cli_files_holds(kinds, ".lean"),
              "and the view keeps each file's name and ending once, as it reads them");
    cli_files_let(kinds);
    cli_check_folder("files_tracked", repo);
    files_folder(repo, "made", path);
    files_written(repo, ".gitignore", "out/\n", 5ull);
    files_written(repo, "a.txt", "one", 3ull);
    cli_check(cli_git_run(repo, s_init, 2u, &out, &said), "a repository is made");
    cli_files_forget_seen();
    cli_check(cli_files_tracked(repo, "a.txt"), "a file git would track");
    files_written(repo, "made/new.txt", "two", 3ull);
    files_folder(repo, "made/deeper", path);
    files_written(repo, "made/deeper/more.txt", "three", 5ull);
    files_folder(repo, "out/deep", path);
    files_written(repo, "out/deep/x.o", "four", 4ull);
    cli_check(cli_files_tracked(repo, "made/new.txt"), "a file in a folder made since");
    cli_check(cli_files_tracked(repo, "made/deeper/more.txt"), "and one a folder further down");
    cli_check(!cli_files_tracked(repo, "out/deep/x.o"), "a file under a folder git ignores");
    cli_files_forget_seen();
}

// a search holds what it finds in cells that grow past their first room as lines are found, each given back as it ends
static void files_searched(void)
{
    char root[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("files_search", root);
    char many[100u * 12u];
    CliLine lines = {many, sizeof(many), 0ull};
    for (unsigned int at = 0u; at < 100u; at += 1u)
    {
        cli_line_text(&lines, "a needle\n");
    }
    files_written(root, "a.txt", many, lines.at);
    files_written(root, "b.txt", "no\nthe needle\n", 14ull);
    cli_files_forget_seen();
    const CliPatterns none = {0};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    const int found = cli_files_search(root, "needle", (CliSearching){0, 0, 0}, &none, &out, &said);
    (void)cli_line_closed(&out);
    unsigned int hits = 0u;
    for (const char *at = s_out;
         (at = cellul.find(&(CatenaFinitaCfg){
              .src = at, .cap = CLI_CELLS_POOL, .other = "\"line\"", .other_cap = CLI_CELLS_POOL})) != NULL;
         at += 1)
    {
        hits += 1u;
    }
    cli_check(found && (hits == 101u), "every line that holds the query is found");
    cli_check((g_cli_search_cells.holds == 0ull) && (g_cli_search_cells.hw > (64ull * sizeof(CliHeldHit))),
              "the lines found are held in cells grown past their first room, each given back");
    cli_files_forget_seen();
}

// a find walks the folders of a tree git does not read and names each file whose path holds the query's words, sorted
static void files_found(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("files_find", root);
    files_folder(root, "beta", path);
    files_written(root, "alpha.txt", "a", 1ull);
    files_written(root, "beta/alpha.js", "b", 1ull);
    files_written(root, "gamma.txt", "c", 1ull);
    cli_files_forget_seen();
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_files_find(root, "alpha", &out);
    cli_check_text(s_out, out.at, "[\"alpha.txt\",\"beta/alpha.js\"]", "a find names the files that hold the query");
    cli_files_forget_seen();
}

// a file written where a folder stands is refused with the system's reason, for the window to tell a refusal of rights
static void files_refused(void)
{
    char root[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_check_folder("files_refused", root);
    cli_full(root, "taken", folder);
    query_folder_made(folder);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    const int written = cli_files_write(root, "taken", "x", 1ull, &said);
    (void)cli_line_closed(&said);
    cli_check(!written &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "(os error ", .other_cap = CLI_CELLS_POOL}) != NULL),
              why);
}

int main(void)
{
    files_refused();
    files_found();
    files_searched();
    files_paste();
    files_bring_over();
    files_bring();
    files_listing();
    char report[256];
    CliLine said = {report, sizeof(report), 0ull};
    cli_line_text(&said, "the files count what the views and their lists hold in the cellblock, their report ");
    const unsigned long long numbers = said.at;
    cli_files_cells_said(&said);
    (void)cli_line_closed(&said);
    const char *const mark =
        cellul.chr(&(CatenaFinitaCfg){.src = report + numbers, .cap = CLI_CELLS_POOL, .byte = ' '});
    cli_check((mark != NULL) && (mark[1] != '0'), report);
    return cli_checked();
}
