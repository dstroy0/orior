// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_file_writing.c: files written whole and sealed: a .gsm written in pieces has its bytes and a seal beside it holding the
// root of those bytes, a file of an unsealed kind has no seal, and writing again replaces both
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_read[1u << 16u];
static KsgSeal s_seal;
static char s_root[CLI_PATH_MAX_LEN];
static char s_gsm[CLI_PATH_MAX_LEN];

static const char s_first[] = "1,1->2\n3,5->8\n";
static const char s_second[] = "1,1->2\n3,5->9\n";

// a leaf passed over: only the root is compared here
static int leaf_passed(void *context, const unsigned char *leaf, unsigned long long line)
{
    (void)context;
    (void)leaf;
    (void)line;
    return 1;
}

// the seal root of `text` into `root`
static void root_of(const char *text, unsigned char *root)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    TEST_ASSERT_TRUE(ksg_seal_begun(&s_seal, leaf_passed, NULL));
    TEST_ASSERT_TRUE(ksg_seal_bytes(&s_seal, text, CLI_UNITY_LENGTH(text)));
    TEST_ASSERT_TRUE(ksg_seal_ended(&s_seal, root, trailer));
}

// the file at `path` holds exactly `text`
static void assert_file(const char *path, const char *text)
{
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_read, sizeof(s_read));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    CLI_TEST_ASSERT_TEXT(text, s_read, size);
}

// whether the seal beside `path` ends with the root of `text`
static int seal_holds_root_of(const char *path, const char *text)
{
    char sealed[CLI_PATH_MAX_LEN];
    VerbaLinea line = {sealed, sizeof(sealed), 0ull};
    mmgr_line_put(&line, path);
    mmgr_line_put(&line, KSG_SUFFIX);
    (void)mmgr_line_finish(&line);
    const unsigned long long size = qry_member_file_read(sealed, (unsigned char *)s_read, sizeof(s_read));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT64(KSG_SIGNUM_BYTES, size);
    unsigned char root[KSG_SIGNUM_BYTES];
    root_of(text, root);
    return memor.cmp(&(MemoriaCfg){.src = s_read + size - KSG_SIGNUM_BYTES, .other = root, .bytes = KSG_SIGNUM_BYTES}) == 0;
}

void setUp(void)
{
    cli_unity_begun();
    cli_unity_folder("file_writing", s_root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "rel.gsm", .joined = s_gsm);
    const CliFilePiece first[2] = {{"1,1->2\n", 7ull}, {"3,5->8\n", 7ull}};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = s_gsm, .pieces = first, .count = 2u));
}

void tearDown(void)
{
}

void test_a_gsm_written_in_pieces_holds_its_bytes(void)
{
    assert_file(s_gsm, s_first);
}

void test_the_seal_adjacent_it_holds_the_root_of_those_bytes(void)
{
    TEST_ASSERT_TRUE(seal_holds_root_of(s_gsm, s_first));
}

void test_a_markdown_file_is_written_with_no_seal(void)
{
    char md[CLI_PATH_MAX_LEN];
    char seal[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "notes.md", .joined = md);
    const CliFilePiece notes[1] = {{"# notes\n", 8ull}};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = md, .pieces = notes, .count = 1u));
    assert_file(md, "# notes\n");
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_root, .relative = "notes.md" KSG_SUFFIX, .joined = seal);
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = seal));
}

void test_writing_again_replaces_the_bytes_and_the_root(void)
{
    const CliFilePiece second[1] = {{s_second, sizeof(s_second) - 1u}};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = s_gsm, .pieces = second, .count = 1u));
    assert_file(s_gsm, s_second);
    TEST_ASSERT_TRUE(seal_holds_root_of(s_gsm, s_second));
    TEST_ASSERT_FALSE(seal_holds_root_of(s_gsm, s_first));
}

void test_sealed_kinds_are_g_gsm_and_the_k_files(void)
{
    TEST_ASSERT_TRUE(ksg_kind_sealed("a/b.g"));
    TEST_ASSERT_TRUE(ksg_kind_sealed("sm_86.krs"));
    TEST_ASSERT_TRUE(ksg_kind_sealed("x.kqr"));
}

void test_a_seal_and_key_files_are_not_sealed(void)
{
    TEST_ASSERT_FALSE(ksg_kind_sealed("x.ksg"));
    TEST_ASSERT_FALSE(ksg_kind_sealed("x.ksa"));
    TEST_ASSERT_FALSE(ksg_kind_sealed("x.ksk"));
    TEST_ASSERT_FALSE(ksg_kind_sealed("x.kpk"));
}

void test_other_names_are_not_sealed(void)
{
    TEST_ASSERT_FALSE(ksg_kind_sealed("x.c"));
    TEST_ASSERT_FALSE(ksg_kind_sealed("a.krs/b"));
    TEST_ASSERT_FALSE(ksg_kind_sealed("x.k"));
    TEST_ASSERT_FALSE(ksg_kind_sealed("x.KRS"));
}
