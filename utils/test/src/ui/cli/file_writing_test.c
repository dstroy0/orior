// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_writing_test.c: the command line's files written whole and sealed: a .gsm written in pieces contains its bytes
// and has its seal beside it, whose root is the root of those bytes sealed again; a file of a kind ksg does not seal
// has none; a .gsm written again contains its new bytes, and its seal the new root; and a growing line contains what it
// is given in one run of bytes
#include "../../../../../ui/cli/src/command_line/cli_internal.h"
#include "cli_check.h"

static char s_read[1u << 16u];
static KsgSeal s_seal;

// a leaf let go: the root alone is retained here
static int writing_leaf_let(void *context, const unsigned char *leaf, unsigned long long line)
{
    (void)context;
    (void)leaf;
    (void)line;
    return 1;
}

// the root of `size` bytes of `bytes` sealed into `root`
static void writing_root_of(const char *bytes, unsigned long long size, unsigned char *root)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    (void)ksg_seal_begun(&s_seal, writing_leaf_let, NULL);
    (void)ksg_seal_bytes(&s_seal, bytes, size);
    (void)ksg_seal_ended(&s_seal, root, trailer);
}

// whether the file at `path` contains exactly `text`
static int writing_file_is(const char *path, const char *text)
{
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_read, sizeof(s_read));
    return (size == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = s_read, .other = text, .bytes = size}) == 0);
}

// whether the seal beside `path` ends with the root of `text`
static int writing_sealed(const char *path, const char *text)
{
    char sealed[CLI_PATH_MAX_LEN];
    unsigned char root[KSG_SIGNUM_BYTES];
    memor.cpy(&(MemoriaCfg){
        .dst = sealed, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = sealed, .cap = CLI_CELLS_POOL});
    memor.cpy(&(MemoriaCfg){.dst = sealed + length, .src = KSG_SUFFIX, .bytes = sizeof(KSG_SUFFIX)});
    const unsigned long long size = qry_member_file_read(sealed, (unsigned char *)s_read, sizeof(s_read));
    writing_root_of(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), root);
    return (size != QRY_MEMBER_UNREAD) && (size >= KSG_SIGNUM_BYTES) &&
           (memor.cmp(
                &(MemoriaCfg){.src = s_read + size - KSG_SIGNUM_BYTES, .other = root, .bytes = KSG_SIGNUM_BYTES}) == 0);
}

// a growing line begun small contains all that is written in one run of bytes, takes back what it rewinds past, makes
// room for a read of its own, and a fixed line makes none past what it contains
static void writing_line_grown(void)
{
    VerbaLinea line;
    cli_check(cli_growing_line_made(&line, 16ull, &g_checks_cells) && (line.capacity == 16ull),
              "a growing line is begun in a cell of the bytes requested");
    for (unsigned int at = 0u; at < 100u; at += 1u)
    {
        mmgr_line_put(&line, "0123456789");
    }
    int whole = mmgr_line_ok(&line) && (line.at == 1000ull) && (line.capacity >= 1001ull);
    for (unsigned int at = 0u; whole && (at < 1000u); at += 1u)
    {
        whole = (line.out[at] == (char)('0' + (at % 10u)));
    }
    cli_check(whole, "a thousand bytes written to it are stored in one run, in order");
    line.at = 5ull;
    mmgr_line_put(&line, "x");
    cli_check(mmgr_line_ok(&line) && (line.at == 6ull) && (line.out[5] == 'x'),
              "a rewind writes over what it contained");
    cli_check(cli_growing_line_room(&line, 5000ull) && ((line.capacity - line.at) > 5000ull) && (line.out[5] == 'x'),
              "room is made for five thousand bytes more, what it contained retained");
    cli_cell_release(line.out, &g_checks_cells);
    char fixed[8];
    VerbaLinea fixed_line = {fixed, sizeof(fixed), 0ull};
    cli_check(cli_growing_line_room(&fixed_line, 7ull) && !cli_growing_line_room(&fixed_line, 8ull),
              "a fixed line has room for what fits before its terminator and no more");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    char root[CLI_PATH_MAX_LEN];
    char gsm[CLI_PATH_MAX_LEN];
    char md[CLI_PATH_MAX_LEN];
    char seal[CLI_PATH_MAX_LEN];
    cli_check_folder("file_writing", root);
    cli_full(root, "rel.gsm", gsm);
    cli_full(root, "notes.md", md);
    static const char s_first[] = "1,1->2\n3,5->8\n";
    static const char s_second[] = "1,1->2\n3,5->9\n";
    const CliFilePiece first[2] = {{"1,1->2\n", 7ull}, {"3,5->8\n", 7ull}};
    cli_check(cli_file_pieces_written(gsm, first, 2u), "a .gsm is written in pieces");
    cli_check(writing_file_is(gsm, s_first), "it contains its bytes");
    cli_check(writing_sealed(gsm, s_first), "and its seal beside it has the root of those bytes");
    const CliFilePiece notes[1] = {{"# notes\n", 8ull}};
    cli_check(cli_file_pieces_written(md, notes, 1u) && writing_file_is(md, "# notes\n"), "a .md is written");
    cli_full(root, "notes.md" KSG_SUFFIX, seal);
    cli_check(!query_is_file(seal), "and has no seal, being of no kind ksg seals");
    const CliFilePiece second[1] = {{s_second, sizeof(s_second) - 1u}};
    cli_check(cli_file_pieces_written(gsm, second, 1u) && writing_file_is(gsm, s_second), "the .gsm written again");
    cli_check(writing_sealed(gsm, s_second) && !writing_sealed(gsm, s_first), "its seal has the new root");
    cli_check(ksg_kind_sealed("a/b.g") && ksg_kind_sealed("sm_86.krs") && ksg_kind_sealed("x.kqr") &&
                  !ksg_kind_sealed("x.ksg") && !ksg_kind_sealed("x.ksa") && !ksg_kind_sealed("x.ksk") &&
                  !ksg_kind_sealed("x.kpk") && !ksg_kind_sealed("x.c") && !ksg_kind_sealed("a.krs/b") &&
                  !ksg_kind_sealed("x.k") && !ksg_kind_sealed("x.KRS"),
              "the kinds sealed are .g, .gsm and the k-files, not a seal's own files or an approver's keys");
    writing_line_grown();
    return cli_checked();
}
