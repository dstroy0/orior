// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_files.c: a paste copies or moves and writes over nothing; a file and a folder go to a tree on another machine a part at a time; a file and a folder
// brought in are copied whole, each file reporting its end; a tree in a folder its repository ignores lists its files from the disk; a file in a folder made
// after git's view was read is tracked, and one in an ignored folder is not; and a file past the size the window pages is read without a cell of its size
#include "ui/cli/src/code/git.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/code/symbols.h"
#include "ui/cli/src/editor/files.h"
#include "ui/cli/src/editor/files_search_hits.h"
#include "ui/cli/src/text/base64.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// What a bringing reported: each path, the bytes done and its size
typedef struct
{
    char path[64][CLI_PATH_MAX_LEN];
    unsigned long long done[64];
    unsigned long long size[64];
    unsigned int count;
} FilesProgress;

static const char *const s_init[2] = {"init", "-q"};

static FilesProgress s_progress;
static CliLock s_lock = CLI_LOCK_INIT;
static unsigned char s_big[3u << 20u];
static unsigned char s_read[(3u << 20u) + 1u];
static unsigned char s_part[2u << 20u];
static char s_out[1u << 16u];
static char s_there[CLI_PATH_MAX_LEN];
static char s_written[4096];
static char s_root[CLI_PATH_MAX_LEN];
static char s_why[1024];

// a file past the size the window pages, in lines of a hundred bytes
static unsigned char s_huge[128u << 20u];

static int s_over_ran;
static int s_over_brought;
static char s_over_tos[256];
static unsigned long long s_over_size;
static int s_over_whole;
static int s_over_note;
static int s_over_first;
static int s_over_part_file;
static unsigned int s_over_reports;
static int s_over_end_reported;
static int s_over_late_part;

static int s_bring_ran;
static int s_bring_brought;
static char s_bring_tos[256];
static unsigned long long s_bring_size;
static int s_bring_note;
static int s_bring_first;
static int s_bring_big_end;
static int s_bring_note_end;

static int s_large_ran;
static int s_large_read;
static int s_large_windowed;
static int s_large_sized;
static unsigned long long s_large_peak;
static int s_large_sliced;
static int s_large_slice_start;
static int s_large_slice_end;
static unsigned long long s_large_slice_peak;

// a file of the tree `root` written at `name`, `size` bytes of `bytes`
static void bytes_written(const char *root, const char *name, const void *bytes, unsigned long long size)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = bytes, .size = size), name);
}

// the folder `name` of the tree `root` made with every folder above it, its path in `path`
static void folder_made(const char *root, const char *name, char *path)
{
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    for (char *at = path + CLI_UNITY_LENGTH(root) + 1u; at[0] != '\0'; at += 1)
    {
        if (at[0] == '/')
        {
            at[0] = '\0';
            (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
            at[0] = '/';
        }
    }
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path);
}

// the file `name` of `root` read into s_read: its size, or QRY_MEMBER_UNREAD
static unsigned long long file_read(const char *root, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    return qry_member_file_read(path, s_read, sizeof(s_read) - 1u);
}

// whether the file `name` of `root` contains `text`
static int file_is(const char *root, const char *name, const char *text)
{
    const unsigned long long size = file_read(root, name);
    return (size == CLI_UNITY_LENGTH(text)) && (memor.cmp(&(MemoriaCfg){.src = s_read, .other = text, .bytes = size}) == 0);
}

// whether the file `name` of `root` is there
static int file_there(const char *root, const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    return EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path);
}

// the pairs a paste wrote, each `from>to`, `-` for a from of null, joined by `|`, in s_out
static void pairs_written(const char *written, unsigned long long size)
{
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(written, size, &read);
    VerbaLinea line = {s_out, sizeof(s_out), 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int pair = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = at);
        const char *const from = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = pair, .name = "from"), .length_pointer = NULL);
        const char *const to = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = pair, .name = "to"), .length_pointer = NULL);
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, (from != NULL) ? from : "-");
        mmgr_line_ch(&line, '>');
        mmgr_line_put(&line, (to != NULL) ? to : "-");
    }
    (void)mmgr_line_finish(&line);
}

// `from`, a path of the run's, pasted into `into` of s_root: 1, its pairs in s_out, or 0
static int path_pasted(const char *into, const char *from, int moving)
{
    char why[1024];
    char written[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {written, sizeof(written), 0ull};
    const char *const froms[1] = {from};
    if (!EMBED_CALL(cli_files_root_count.paste, CliFilesRootCountCfg, .root = s_root, .into = into, .from = froms, .count = 1u, .moving = moving, .out = &out, .err_msg = &err_msg))
    {
        return 0;
    }
    pairs_written(written, out.at);
    return 1;
}

// a tree of a folder of a note and a deeper file, and an empty folder, for the paste cases
static void paste_tree_written(void)
{
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder("files_paste", s_root);
    folder_made(s_root, "a/inner", path);
    folder_made(s_root, "b", path);
    bytes_written(s_root, "a/notes.txt", "one", 3ull);
    bytes_written(s_root, "a/inner/deep.rs", "two", 3ull);
}

// what a bringing reports recorded
static void progress_recorded(const CliProgressCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (s_progress.count < 64u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_progress.path[s_progress.count], .src = args->path, .bytes = CLI_UNITY_LENGTH(args->path) + 1u});
        s_progress.done[s_progress.count] = args->done;
        s_progress.size[s_progress.count] = args->size;
        s_progress.count += 1u;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// how many times `path` was reported, and whether it was reported with `done` of `size`
static unsigned int progress_count(const char *path, unsigned long long done, unsigned long long size, int *with)
{
    unsigned int count = 0u;
    *with = 0;
    for (unsigned int at = 0u; at < s_progress.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_progress.path[at], .other = path, .cap = CLI_CELLS_POOL}))
        {
            count += 1u;
            *with = *with || ((s_progress.done[at] == done) && (s_progress.size[at] == size));
        }
    }
    return count;
}

// a whole number member of the shared tree's node
static unsigned long long whole_member(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = node, .name = name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; (member != CLI_JSON_NONE) && (at < json->node[member].length); at += 1ull)
    {
        // a digit's value is below 10
        value = (value * 10ull) + (unsigned long long)(json->text[json->node[member].text + at] - '0');
    }
    return value;
}

// the tree's side, as the server answers the calls there
static int call_answered(const CliCallCfg *args)
{
    const char *name = args->name;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(args->arguments, CLI_UNITY_LENGTH(args->arguments), &root);
    const char *const path = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "path"), .length_pointer = NULL);
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "bring_name", .cap = CLI_CELLS_POOL}))
    {
        char found[CLI_PATH_MAX_LEN] = {0};
        const int named = EMBED_CALL(cli_files_root_err_msg.free_in, CliFilesRootErrMsgCfg, .root = s_there, .into = cli_unity_member_text(root, "into"), .name = cli_unity_member_text(root, "name"), .path_out = found, .err_msg = err_msg);
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = found, .length = CLI_UNITY_LENGTH(found));
        return named;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "folder_create", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(out, "null");
        return EMBED_CALL(cli_files_root_out_dir.create_folder, CliFilesRootOutDirCfg, .root = s_there, .dir = (path != NULL) ? path : "", .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "file_part", .cap = CLI_CELLS_POOL}))
    {
        unsigned long long length = 0ull;
        unsigned long long size = 0ull;
        const char *const data = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "data"), .length_pointer = &length);
        const int last = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "last"), .kind = CFG_JSON_TRUE);
        mmgr_line_put(out, "null");
        return (data != NULL) && EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = data, .length = length, .out_bytes = s_part, .room = sizeof(s_part), .size_pointer = &size) && EMBED_CALL(cli_files_root_path.write_part, CliFilesRootPathCfg, .root = s_there, .path = (path != NULL) ? path : "", .at = whole_member(json, root, "at"), .data = s_part, .size = size, .last = last, .err_msg = err_msg);
    }
    mmgr_line_put(err_msg, "no call ");
    mmgr_line_put(err_msg, name);
    return 0;
}

// the `to` of each pair written, joined by `|`, into `out`, which is sized for 256
static void tos_written(const char *written, unsigned long long size, char *out)
{
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(written, size, &read);
    VerbaLinea line = {out, 256u, 0ull};
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = at), "to"));
    }
    (void)mmgr_line_finish(&line);
}

// a file and a folder sent to another machine's tree, what each answered recorded, once for the process
static void over_ran(void)
{
    char base[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char big_path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    const unsigned long long big = 5ull << 19u;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_written, sizeof(s_written), 0ull};
    if (s_over_ran)
    {
        return;
    }
    s_over_ran = 1;
    cli_unity_folder("files_bring_over", base);
    folder_made(base, "there/in", path);
    folder_made(base, "here/folder", path);
    for (unsigned long long at = 0ull; at < big; at += 1ull)
    {
        // a byte of the count, below 251
        s_big[at] = (unsigned char)(at % 251ull);
    }
    bytes_written(base, "here/big.bin", s_big, big);
    bytes_written(base, "here/folder/note.txt", "note", 4ull);
    bytes_written(base, "there/in/big.bin", "here first", 10ull);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "there", .joined = s_there);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "here/big.bin", .joined = big_path);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "here/folder", .joined = folder);
    const char *const from[2] = {big_path, folder};
    s_progress.count = 0u;
    s_over_brought = EMBED_CALL(cli_files_context.bring_over, CliFilesContextCfg, .call = call_answered, .call_context = NULL, .into = "in", .from = from, .count = 2u, .progress = progress_recorded, .context = NULL, .out = &out, .err_msg = &err_msg);
    tos_written(s_written, out.at, s_over_tos);
    s_over_size = file_read(s_there, "in/big (2).bin");
    s_over_whole = (s_over_size == big) && (memor.cmp(&(MemoriaCfg){.src = s_read, .other = s_big, .bytes = big}) == 0);
    s_over_note = file_is(s_there, "in/folder/note.txt", "note");
    s_over_first = file_is(s_there, "in/big.bin", "here first");
    s_over_part_file = file_there(s_there, "in/.big (2).bin.orior-part");
    s_over_reports = progress_count("in/big (2).bin", big, big, &s_over_end_reported);
    err_msg.at = 0ull;
    s_over_late_part = EMBED_CALL(cli_files_root_path.write_part, CliFilesRootPathCfg, .root = s_there, .path = "in/late.txt", .at = 5ull, .data = "x", .size = 1ull, .last = 1, .err_msg = &err_msg);
}

// a file and a folder brought in, what each answered recorded, once for the process
static void bring_ran(void)
{
    char base[CLI_PATH_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char big_path[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_written, sizeof(s_written), 0ull};
    if (s_bring_ran)
    {
        return;
    }
    s_bring_ran = 1;
    cli_unity_folder("files_bring", base);
    folder_made(base, "tree/in", path);
    folder_made(base, "outside/folder", path);
    memor.set(&(MemoriaCfg){.dst = s_big, .val = 7, .bytes = 3u << 20u});
    bytes_written(base, "outside/big.bin", s_big, 3ull << 20u);
    bytes_written(base, "outside/folder/note.txt", "note", 4ull);
    bytes_written(base, "tree/in/big.bin", "here first", 10ull);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "tree", .joined = root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "outside/big.bin", .joined = big_path);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = "outside/folder", .joined = folder);
    const char *const from[2] = {big_path, folder};
    s_progress.count = 0u;
    s_bring_brought = EMBED_CALL(cli_files_root.bring, CliFilesRootCfg, .root = root, .into = "in", .from = from, .count = 2u, .progress = progress_recorded, .context = NULL, .out = &out, .err_msg = &err_msg);
    tos_written(s_written, out.at, s_bring_tos);
    s_bring_size = file_read(root, "in/big (2).bin");
    s_bring_note = file_is(root, "in/folder/note.txt", "note");
    s_bring_first = file_is(root, "in/big.bin", "here first");
    (void)progress_count("in/big (2).bin", 3ull << 20u, 3ull << 20u, &s_bring_big_end);
    (void)progress_count("in/folder/note.txt", 4ull, 4ull, &s_bring_note_end);
}

// a repository whose ignored folder holds a tree, the tree's path in `tree`
static void ignored_tree_written(char *tree)
{
    char repo[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_listing", repo);
    folder_made(repo, "runs/tree/src", path);
    bytes_written(repo, ".gitignore", "runs/\n", 6ull);
    bytes_written(repo, "runs/tree/a.js", "one", 3ull);
    bytes_written(repo, "runs/tree/src/b.js", "two", 3ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = repo, .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = repo, .relative = "runs/tree", .joined = tree);
}

// a repository with an ignored folder, and folders made after git's view was read, at s_root
static void tracked_tree_written(void)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_tracked", s_root);
    folder_made(s_root, "made", path);
    bytes_written(s_root, ".gitignore", "out/\n", 5ull);
    bytes_written(s_root, "a.txt", "one", 3ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = s_root, .request = s_init, .count = 2u, .out = &out, .err_msg = &err_msg));
    cli_tree_forgotten();
    (void)EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "a.txt");
    bytes_written(s_root, "made/new.txt", "two", 3ull);
    folder_made(s_root, "made/deeper", path);
    bytes_written(s_root, "made/deeper/more.txt", "three", 5ull);
    folder_made(s_root, "out/deep", path);
    bytes_written(s_root, "out/deep/x.o", "four", 4ull);
}

// whether `text` contains `part`
static int text_contains(const char *text, const char *part)
{
    const unsigned long long length = CLI_UNITY_LENGTH(part);
    for (const char *at = text; *at != '\0'; at += 1)
    {
        if (cellul.starts(&(CatenaFinitaCfg){.src = at, .other = part, .cap = length}))
        {
            return 1;
        }
    }
    return 0;
}

// a file past the paging size read for the window, then a slice of its lines, what each answered recorded, once for the process
static void large_ran(void)
{
    char root[CLI_PATH_MAX_LEN];
    char why[512];
    unsigned long long pool = 0ull;
    unsigned long long temporary = 0ull;
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_large_ran)
    {
        return;
    }
    s_large_ran = 1;
    cli_unity_folder("files_large", root);
    for (unsigned long long at = 0ull; at < sizeof(s_huge); at += 1ull)
    {
        // lines of a hundred bytes, the letters in turn
        s_huge[at] = ((at % 100ull) == 99ull) ? (unsigned char)'\n' : (unsigned char)('a' + (at % 26ull));
    }
    bytes_written(root, "huge.log", s_huge, sizeof(s_huge));
    s_large_read = EMBED_CALL(cli_files_root_out_err_msg_file.read, CliFilesRootOutErrMsgFileCfg, .root = root, .file = "huge.log", .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    s_large_windowed = text_contains(s_out, "\"windowed\":true");
    s_large_sized = text_contains(s_out, "\"size\":134217728");
    EMBED_CALL(cli_cells_table.report, CliCellsCfg, .pool = &pool, .persistent = &s_large_peak, .temporary = &temporary);
    out.at = 0ull;
    s_large_sliced = EMBED_CALL(cli_files_root_out_err_msg_file.slice, CliFilesRootOutErrMsgFileCfg, .root = root, .file = "huge.log", .start = 8000000ull, .end = 8001000ull, .out = &out, .err_msg = &err_msg);
    (void)mmgr_line_finish(&out);
    s_large_slice_start = text_contains(s_out, "\"start\":8000000");
    s_large_slice_end = text_contains(s_out, "\"end\":8001000");
    EMBED_CALL(cli_cells_table.report, CliCellsCfg, .pool = &pool, .persistent = &s_large_slice_peak, .temporary = &temporary);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    cli_tree_forgotten();
}

void test_a_file_written_where_a_folder_stands_is_refused_with_the_systems_reason(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder("files_refused", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "taken", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_err_msg_text.write, CliFilesRootErrMsgTextCfg, .root = s_root, .file = "taken", .text = "x", .length = 1ull, .err_msg = &err_msg));
    (void)mmgr_line_finish(&err_msg);
    CLI_TEST_ASSERT_CONTAINS("(os error ", why);
}

void test_a_find_names_the_files_whose_paths_contain_the_query_sorted(void)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_find", s_root);
    folder_made(s_root, "beta", path);
    bytes_written(s_root, "alpha.txt", "a", 1ull);
    bytes_written(s_root, "beta/alpha.js", "b", 1ull);
    bytes_written(s_root, "gamma.txt", "c", 1ull);
    cli_tree_forgotten();
    EMBED_CALL(cli_files_root_out_err_msg.find, CliFilesRootOutErrMsgCfg, .root = s_root, .query = "alpha", .out = &out);
    CLI_TEST_ASSERT_TEXT("[\"alpha.txt\",\"beta/alpha.js\"]", s_out, out.at);
}

void test_a_search_finds_every_line_in_cells_grown_past_their_first_room_each_released(void)
{
    char many[100u * 12u];
    const CliPatterns none = {0};
    VerbaLinea lines = {many, sizeof(many), 0ull};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_unity_folder("files_search", s_root);
    for (unsigned int at = 0u; at < 100u; at += 1u)
    {
        mmgr_line_put(&lines, "a needle\n");
    }
    bytes_written(s_root, "a.txt", many, lines.at);
    bytes_written(s_root, "b.txt", "no\nthe needle\n", 14ull);
    cli_tree_forgotten();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = s_root, .query = "needle", .how = (CliSearching){0, 0, 0}, .set = &none, .out = &out, .err_msg = &err_msg));
    (void)mmgr_line_finish(&out);
    unsigned int hits = 0u;
    for (const char *at = cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL, .other = "\"line\"", .other_cap = CLI_CELLS_POOL}); at != NULL; at = cellul.find(&(CatenaFinitaCfg){.src = at + 1, .cap = CLI_CELLS_POOL, .other = "\"line\"", .other_cap = CLI_CELLS_POOL}))
    {
        hits += 1u;
    }
    TEST_ASSERT_EQUAL_UINT(101u, hits);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_search_cells.in_use));
    TEST_ASSERT_GREATER_THAN_UINT64(64ull * sizeof(CliSearchHit), embed_atomic_usize_load(&g_cli_search_cells.hw));
}

void test_a_copy_beside_its_own_name_is_numbered(void)
{
    char path[CLI_PATH_MAX_LEN];
    paste_tree_written();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "a/notes.txt", .joined = path);
    TEST_ASSERT_TRUE(path_pasted("a", path, 0));
    TEST_ASSERT_EQUAL_STRING("a/notes.txt>a/notes (2).txt", s_out);
    TEST_ASSERT_TRUE(file_is(s_root, "a/notes (2).txt", "one"));
}

void test_a_folder_is_copied_with_all_it_contains(void)
{
    char path[CLI_PATH_MAX_LEN];
    paste_tree_written();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "a", .joined = path);
    TEST_ASSERT_TRUE(path_pasted("b", path, 0));
    TEST_ASSERT_TRUE(file_is(s_root, "b/a/inner/deep.rs", "two"));
}

void test_a_folder_is_not_pasted_into_itself(void)
{
    char path[CLI_PATH_MAX_LEN];
    paste_tree_written();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "a", .joined = path);
    TEST_ASSERT_FALSE(path_pasted("a/inner", path, 0));
}

void test_a_move_removes_it_from_where_it_was(void)
{
    char path[CLI_PATH_MAX_LEN];
    paste_tree_written();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "a/notes.txt", .joined = path);
    TEST_ASSERT_TRUE(path_pasted("b", path, 1));
    TEST_ASSERT_EQUAL_STRING("a/notes.txt>b/notes.txt", s_out);
    TEST_ASSERT_FALSE(file_there(s_root, "a/notes.txt"));
}

void test_a_move_to_where_it_is_leaves_it(void)
{
    char path[CLI_PATH_MAX_LEN];
    paste_tree_written();
    bytes_written(s_root, "b/notes.txt", "one", 3ull);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "b/notes.txt", .joined = path);
    TEST_ASSERT_TRUE(path_pasted("b", path, 1));
    TEST_ASSERT_EQUAL_STRING("b/notes.txt>b/notes.txt", s_out);
}

void test_a_file_from_outside_the_tree_has_no_from(void)
{
    char outside[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    paste_tree_written();
    cli_unity_folder("files_paste_out", outside);
    bytes_written(outside, "out.txt", "three", 5ull);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = outside, .relative = "out.txt", .joined = path);
    TEST_ASSERT_TRUE(path_pasted("", path, 1));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "->", .cap = CLI_CELLS_POOL}));
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = path));
}

void test_a_file_and_a_folder_go_to_the_tree_on_another_machine(void)
{
    over_ran();
    TEST_ASSERT_TRUE(s_over_brought);
    TEST_ASSERT_EQUAL_STRING("in/big (2).bin|in/folder", s_over_tos);
}

void test_the_file_goes_whole_part_by_part(void)
{
    over_ran();
    TEST_ASSERT_TRUE(s_over_whole);
}

void test_the_folders_file_goes_and_nothing_is_written_over(void)
{
    over_ran();
    TEST_ASSERT_TRUE(s_over_note);
    TEST_ASSERT_TRUE(s_over_first);
}

void test_the_parts_file_is_moved_over_the_file(void)
{
    over_ran();
    TEST_ASSERT_FALSE(s_over_part_file);
}

void test_each_part_and_the_end_are_reported(void)
{
    over_ran();
    TEST_ASSERT_GREATER_OR_EQUAL_UINT(4u, s_over_reports);
    TEST_ASSERT_TRUE(s_over_end_reported);
}

void test_a_part_out_of_its_place_is_refused(void)
{
    over_ran();
    TEST_ASSERT_FALSE(s_over_late_part);
}

void test_a_file_and_a_folder_are_brought_in(void)
{
    bring_ran();
    TEST_ASSERT_TRUE(s_bring_brought);
    TEST_ASSERT_EQUAL_STRING("in/big (2).bin|in/folder", s_bring_tos);
}

void test_the_brought_file_is_whole(void)
{
    bring_ran();
    TEST_ASSERT_EQUAL_UINT64(3ull << 20u, s_bring_size);
}

void test_the_brought_folders_file_is_copied_and_nothing_is_written_over(void)
{
    bring_ran();
    TEST_ASSERT_TRUE(s_bring_note);
    TEST_ASSERT_TRUE(s_bring_first);
}

void test_each_brought_file_reports_its_end(void)
{
    bring_ran();
    TEST_ASSERT_TRUE(s_bring_big_end);
    TEST_ASSERT_TRUE(s_bring_note_end);
}

void test_a_tree_its_repository_ignores_lists_its_files_from_the_disk(void)
{
    char tree[CLI_PATH_MAX_LEN];
    ignored_tree_written(tree);
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = tree);
    TEST_ASSERT_NOT_NULL(files);
    TEST_ASSERT_EQUAL_UINT64(2ull, files->count);
    TEST_ASSERT_EQUAL_STRING("a.js", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 0u));
    TEST_ASSERT_EQUAL_STRING("src/b.js", EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = 1u));
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}

void test_the_kinds_view_records_each_files_name_and_ending_once(void)
{
    char tree[CLI_PATH_MAX_LEN];
    ignored_tree_written(tree);
    const CliFiles *const kinds = EMBED_CALL(cli_files_root_paths.kinds_view, CliFilesRootPathsCfg, .root = tree);
    TEST_ASSERT_NOT_NULL(kinds);
    TEST_ASSERT_EQUAL_UINT64(3ull, kinds->count);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_kind.contains, CliFilesPathKindCfg, .files = kinds, .path = ".js"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_kind.contains, CliFilesPathKindCfg, .files = kinds, .path = "a.js"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_path_kind.contains, CliFilesPathKindCfg, .files = kinds, .path = "b.js"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_path_kind.contains, CliFilesPathKindCfg, .files = kinds, .path = ".lean"));
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = kinds);
}

void test_a_file_git_would_track_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "a.txt"));
}

void test_a_file_in_a_folder_made_since_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "made/new.txt"));
}

void test_a_file_a_folder_further_down_is_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "made/deeper/more.txt"));
}

void test_a_file_under_a_folder_git_ignores_is_not_tracked(void)
{
    tracked_tree_written();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = s_root, .path = "out/deep/x.o"));
}

void test_a_file_past_the_paging_size_is_read_as_windowed_with_its_size(void)
{
    large_ran();
    TEST_ASSERT_TRUE(s_large_read);
    TEST_ASSERT_TRUE(s_large_windowed);
    TEST_ASSERT_TRUE(s_large_sized);
}

void test_the_cellblocks_peak_stays_under_the_files_size(void)
{
    large_ran();
    TEST_ASSERT_LESS_THAN_UINT64(sizeof(s_huge), s_large_peak);
}

void test_a_slice_of_its_lines_is_read_from_the_line_start_to_the_one_at_the_end(void)
{
    large_ran();
    TEST_ASSERT_TRUE(s_large_sliced);
    TEST_ASSERT_TRUE(s_large_slice_start);
    TEST_ASSERT_TRUE(s_large_slice_end);
}

void test_a_slice_stores_only_the_bytes_about_it(void)
{
    large_ran();
    TEST_ASSERT_LESS_THAN_UINT64(sizeof(s_huge), s_large_slice_peak);
}

void test_the_files_report_what_the_views_and_their_lists_use_of_the_cellblock(void)
{
    char tree[CLI_PATH_MAX_LEN];
    char report[256];
    VerbaLinea report_line = {report, sizeof(report), 0ull};
    ignored_tree_written(tree);
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = tree));
    EMBED_CALL(cli_files_root_out_dir.cells_usage_write, CliFilesRootOutDirCfg, .out = &report_line);
    (void)mmgr_line_finish(&report_line);
    const char *const mark = cellul.chr(&(CatenaFinitaCfg){.src = report, .cap = CLI_CELLS_POOL, .byte = ' '});
    TEST_ASSERT_NOT_NULL_MESSAGE(mark, report);
    TEST_ASSERT_NOT_EQUAL_MESSAGE('0', mark[1], report);
}
