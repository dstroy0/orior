// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_test.c: the runs' check: a finished run is read whole and then from where a read stopped, a run's bytes open as
// its names each joined, and a lesson line is read and any other passed over
#include "../../../../../src/cu/includes/file_defs/qry/qry.h"
#include "../../../../../src/cu/includes/file_defs/qry/qry_read.h"
#include "../../../../../ui/cli/src/runs/qry.h"
#include "cli_check.h"

static unsigned char s_run[4096];
static char s_out[1u << 16u];

// a record of `name` containing `blob`, written at `time` with `priority`, put after the `at` bytes of s_run: the bytes
// after it
static unsigned long long run_record(unsigned long long at, const char *name, const char *blob, unsigned long long time,
                                     unsigned int priority)
{
    const unsigned long long name_size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const unsigned long long blob_size = cellul.len(&(CatenaFinitaCfg){.src = blob, .cap = CLI_CELLS_POOL});
    const unsigned long long filled = 24ull + name_size + blob_size;
    const unsigned long long whole = ((filled + 7ull) & ~7ull) + 8ull;
    unsigned char *const bytes = s_run + at;
    memor.set(&(MemoriaCfg){.dst = bytes, .val = 0, .bytes = whole});
    const unsigned long long fields[4] = {time, blob_size, name_size, priority};
    const unsigned int widths[4] = {8u, 8u, 4u, 4u};
    unsigned int place = 0u;
    for (unsigned int field = 0u; field < 4u; field += 1u)
    {
        for (unsigned int each = 0u; each < widths[field]; each += 1u)
        {
            // a byte of the field, its low eight bits retained
            bytes[place + each] = (unsigned char)((fields[field] >> (8u * each)) & 0xffu);
        }
        place += widths[field];
    }
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u, .src = name, .bytes = name_size});
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u + name_size, .src = blob, .bytes = blob_size});
    const unsigned long long crc = qry_record_crc(bytes, whole - 8ull);
    for (unsigned int each = 0u; each < 8u; each += 1u)
    {
        // a byte of the CRC, its low eight bits retained
        bytes[whole - 8ull + each] = (unsigned char)((crc >> (8u * each)) & 0xffu);
    }
    return at + whole;
}

// a whole number member of a node of the shared tree, 0 where it has none
static unsigned long long run_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = cli_json_member(json, node, name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; cli_json_is(json, number, CFG_JSON_NUMBER) && (at < json->node[number].length);
         at += 1ull)
    {
        value = value * 10ull + (unsigned long long)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

// a finished run is read whole and then from where a read stopped
static void run_finished(void)
{
    char dir[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_check_folder("qry_read", dir);
    cli_full(dir, "query1-x.qry", path);
    unsigned long long size = run_record(0ull, "a.txt", "one\n", 10ull, QRY_ORDINARY);
    size = run_record(size, "panic", "why\n", 20ull, QRY_PANIC);
    const unsigned long long first = size;
    (void)query_file_written(path, s_run, first);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_qry_read_file_from(path, 0ull, &out, &err_msg), "a finished run reads");
    unsigned int read = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_out, out.at, &read);
    const unsigned int blobs = cli_json_member(json, read, "blobs");
    const unsigned int one = cli_json_item(json, blobs, 0u);
    const unsigned int panic = cli_json_item(json, blobs, 1u);
    cli_check(cli_json_is(json, cli_json_member(json, read, "live"), CFG_JSON_FALSE), "it is not live");
    cli_check((json->node[blobs].count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(one, "name"), .other = "a.txt", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(panic, "name"), .other = "panic", .cap = CLI_CELLS_POOL})),
              "its records read in order");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(one, "text"), .other = "one\n", .cap = CLI_CELLS_POOL}),
              "with what they contain");
    cli_check(run_whole(json, panic, "nanoseconds") == 20ull, "and when");
    cli_check(run_whole(json, read, "panicked") == run_whole(json, panic, "sequence"), "its panic is the panic record");
    const unsigned long long next = run_whole(json, read, "next");
    cli_check(next == first, "the next read starts past its last record");
    size = run_record(size, "a.txt", "two\n", 30ull, QRY_ORDINARY);
    (void)query_file_written(path, s_run, size);
    out.at = 0ull;
    (void)cli_qry_read_file_from(path, next, &out, &err_msg);
    json = cli_check_parsed(s_out, out.at, &read);
    cli_check(json->node[cli_json_member(json, read, "blobs")].count == 1u,
              "a read goes on from where the last stopped");
    out.at = 0ull;
    cli_check(cli_qry_joined_bytes(path, "a.txt", &out, &err_msg), "a name's records join");
    cli_check_text(s_out, out.at, "one\ntwo\n", "in the order they were written");
}

// a run's bytes open as its names each joined
static void run_members(void)
{
    unsigned long long size = run_record(0ull, "b.txt", "1", 1ull, QRY_ORDINARY);
    size = run_record(size, "a.txt", "2", 2ull, QRY_ORDINARY);
    size = run_record(size, "b.txt", "3", 3ull, QRY_ORDINARY);
    char names[4][CLI_NAME_MAX_LEN];
    const unsigned int count = cli_qry_member_names(s_run, size, names, 4u);
    cli_check((count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = names[0], .other = "b.txt", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = names[1], .other = "a.txt", .cap = CLI_CELLS_POOL})),
              "a run's names come in the order of their first records");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_qry_member_joined(s_run, size, "b.txt", &out);
    cli_check_text(s_out, out.at, "13", "each name's records joined");
    out.at = 0ull;
    cli_qry_member_joined(s_run, size, "a.txt", &out);
    cli_check_text(s_out, out.at, "2", "and the other's");
}

// a tree's runs are listed with their folder and its lessons, each list in cells given back as the listing ends
static void run_listed(void)
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_check_folder("qry_listed", root);
    cli_full(root, "runs", path);
    (void)query_folder_made(path);
    cli_full(root, "runs/one.qry", path);
    (void)query_file_written(path, "", 0ull);
    cli_full(root, "runs/overseer.txt", path);
    (void)query_file_written(path, "lesson time * a stand in\n", 25ull);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_qry_listed(root, &out);
    mmgr_line_finish(&out);
    cli_check((cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                              .cap = CLI_CELLS_POOL,
                                              .other = "{\"path\":\"runs/one.qry\",\"folder\":\"runs\"",
                                              .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_out,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "\"lessons\":[{\"cause\":\"time\"",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "a tree's run is listed with its folder and the folder's lessons");
    cli_check((embed_atomic_usize_load(&g_cli_qry_listed_cells.in_use) != 0ull) && (embed_atomic_usize_load(&g_cli_qry_listed_cells.hw) != 0ull),
              "and the walk stores its folders in a cell while the runs listed are given back");
}

// whether the runs `root` lists hold `wanted`
static int run_listed_holds(const char *root, const char *wanted)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_qry_listed(root, &out);
    mmgr_line_finish(&out);
    return cellul.find(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL, .other = wanted, .other_cap = 64u}) !=
           NULL;
}

// a tree once walked is not walked again: a run in a folder new since shows once the tree's watch says it was
// written, and a folder made while the watch lost changes shows once the tree is walked again for that
static void run_watched(void)
{
    char root[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_check_folder("qry_watched", root);
    cli_full(root, "first", path);
    (void)query_folder_made(path);
    cli_full(root, "first/one.qry", path);
    (void)query_file_written(path, "", 0ull);
    cli_check(run_listed_holds(root, "\"first/one.qry\""), "a tree's first listing walks it");
    cli_full(root, "later", path);
    (void)query_folder_made(path);
    cli_full(root, "later/two.qry", path);
    (void)query_file_written(path, "", 0ull);
    cli_check(!run_listed_holds(root, "\"later/two.qry\""), "a folder new since is not walked for");
    cli_qry_run_written(root, "later/two.qry", 13ull);
    cli_check(run_listed_holds(root, "\"later/two.qry\""), "a run the watch saw written shows, its folder kept");
    cli_full(root, "lost", path);
    (void)query_folder_made(path);
    cli_full(root, "lost/three.qry", path);
    (void)query_file_written(path, "", 0ull);
    cli_qry_runs_lost(root);
    cli_check(run_listed_holds(root, "\"lost/three.qry\""), "the watch's changes lost, the tree is walked again");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    run_finished();
    run_members();
    run_listed();
    run_watched();
    static const char s_overseer[] = "lesson time * a stand in\nnot a lesson\nlesson register 3\n";
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_qry_lessons_written(s_overseer, sizeof(s_overseer) - 1u, &out);
    cli_check_text(s_out, out.at, "{\"cause\":\"time\",\"carrier\":\"*\",\"stand_in\":\"a stand in\"}",
                   "a lesson line is read and any other passed over");
    return cli_checked();
}
