// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_archive.c: deflate and inflate restore every byte; a CRC is what zip and gzip compute; the DOS page has a character for each high byte; a zip opens as
// a folder, its files read and one saved back; a run's .qry opens as a folder of its blobs and is never written back; an archive inside an archive opens and
// saves too; a gzipped tar and a gzipped file open and save back; archives Python writes are read here and those orior writes are read by Python; and a path
// of the tree runs into an archive only past an archive's file
#include "includes/file_defs/qry/qry.h"
#include "includes/file_defs/qry/qry_read.h"
#include "ui/cli/src/editor/archive.h"
#include "ui/cli/src/runs/runner.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// An entry of a zip written as other tools write one: deflated or stored, its name in UTF-8 or in the DOS page
typedef struct
{
    const char *name;
    const char *data;
    unsigned long long size;
    int deflated;
    int utf8;
} ArchiveEntry;

static const ArchiveEntry s_zip_entries[4] = {{"docs/read.me", "hello from a zip\n", 17ull, 1, 1}, {"docs/", "", 0ull, 0, 1}, {"top.txt", "top", 3ull, 0, 1}, {"caf\x82.txt", "dos", 3ull, 0, 0}};

static unsigned char s_text[200000];
static unsigned char s_packed[1u << 20u];
static unsigned char s_back[1u << 20u];
static unsigned char s_zip[1u << 16u];
static unsigned char s_directory[1u << 14u];
static unsigned char s_tar[1u << 16u];
static unsigned char s_run[4096];
static char s_read[1u << 16u];
static CliChildren s_children;
static char s_dir[CLI_PATH_MAX_LEN];
static char s_list_result[1024];
static char s_error[1024];
static unsigned long long s_run_size;
static char s_python[CLI_PATH_MAX_LEN];

// `value` written little-endian in `width` bytes at `out` from `at`, `at` moved past them
static void le_written(unsigned char *out, unsigned long long *at, unsigned long long value, unsigned int width)
{
    for (unsigned int each = 0u; each < width; each += 1u)
    {
        // a byte of the value, its low eight bits
        out[*at + each] = (unsigned char)((value >> (8u * each)) & 0xffu);
    }
    *at += width;
}

// an entry's record head, a local one or the central directory's, written after the `at` bytes of `out`
static void head_written(unsigned char *out, unsigned long long *at, const ArchiveEntry *entry, unsigned long long body, unsigned long long offset, int central)
{
    const unsigned long long name_size = CLI_UNITY_LENGTH(entry->name);
    const unsigned long long data_size = entry->size;
    le_written(out, at, central ? 0x02014b50ull : 0x04034b50ull, 4u);
    if (central)
    {
        le_written(out, at, 20ull, 2u);
    }
    le_written(out, at, 20ull, 2u);
    le_written(out, at, entry->utf8 ? (1ull << 11u) : 0ull, 2u);
    le_written(out, at, entry->deflated ? 8ull : 0ull, 2u);
    le_written(out, at, 0ull, 4u);
    le_written(out, at, EMBED_CALL(cli_archive_bytes_size.crc32, CliArchiveBytesSizeCfg, .bytes = (const unsigned char *)entry->data, .size = data_size), 4u);
    le_written(out, at, body, 4u);
    le_written(out, at, data_size, 4u);
    le_written(out, at, name_size, 2u);
    le_written(out, at, 0ull, 2u);
    if (central)
    {
        // the comment's length, the disk, and the inner and outer attributes
        le_written(out, at, 0ull, 4u);
        le_written(out, at, 0ull, 2u);
        le_written(out, at, 0ull, 4u);
        le_written(out, at, offset, 4u);
    }
    memor.cpy(&(MemoriaCfg){.dst = out + *at, .src = entry->name, .bytes = name_size});
    *at += name_size;
}

// a zip of `count` entries into s_zip: its size
static unsigned long long zip_written(const ArchiveEntry *entries, unsigned int count)
{
    static const unsigned int s_widths[7] = {4u, 4u, 2u, 2u, 4u, 4u, 2u};
    unsigned long long at = 0ull;
    unsigned long long directory = 0ull;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const ArchiveEntry *const entry = &entries[index];
        const unsigned long long data_size = entry->size;
        const unsigned long long body = entry->deflated ? EMBED_CALL(cli_archive_size.deflate, CliArchiveSizeCfg, .data = (const unsigned char *)entry->data, .size = data_size, .out = s_packed, .capacity = sizeof(s_packed)) : data_size;
        const unsigned long long offset = at;
        head_written(s_zip, &at, entry, body, offset, 0);
        memor.cpy(&(MemoriaCfg){.dst = s_zip + at, .src = entry->deflated ? (const void *)s_packed : (const void *)entry->data, .bytes = body});
        at += body;
        head_written(s_directory, &directory, entry, body, offset, 1);
    }
    const unsigned long long start = at;
    memor.cpy(&(MemoriaCfg){.dst = s_zip + at, .src = s_directory, .bytes = directory});
    at += directory;
    const unsigned long long tail[7] = {0x06054b50ull, 0ull, count, count, directory, start, 0ull};
    for (unsigned int field = 0u; field < 7u; field += 1u)
    {
        le_written(s_zip, &at, tail[field], s_widths[field]);
    }
    return at;
}

// `value` written in `digits` octal digits at `out`, a 0 after them
static void octal_written(unsigned char *out, unsigned long long value, unsigned int digits)
{
    for (unsigned int place = digits; place > 0u; place -= 1u)
    {
        // an octal digit's character
        out[place - 1u] = (unsigned char)('0' + (value & 7ull));
        value >>= 3u;
    }
    out[digits] = 0u;
}

// a tar of `count` entries, each a name and its text, into s_tar: its size
static unsigned long long tar_written(const char *const (*entries)[2], unsigned int count)
{
    unsigned long long at = 0ull;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const name = entries[index][0];
        const unsigned long long name_size = CLI_UNITY_LENGTH(name);
        const unsigned long long data_size = CLI_UNITY_LENGTH(entries[index][1]);
        const unsigned long long padded = ((data_size + 511ull) / 512ull) * 512ull;
        unsigned char *const header = s_tar + at;
        memor.set(&(MemoriaCfg){.dst = header, .val = 0, .bytes = 512ull + padded});
        memor.cpy(&(MemoriaCfg){.dst = header, .src = name, .bytes = name_size});
        memor.cpy(&(MemoriaCfg){.dst = header + 100u, .src = "0000644", .bytes = 8u});
        octal_written(header + 124u, data_size, 11u);
        header[156] = (unsigned char)((name[name_size - 1u] == '/') ? '5' : '0');
        memor.cpy(&(MemoriaCfg){.dst = header + 257u, .src = "ustar", .bytes = 6u});
        memor.set(&(MemoriaCfg){.dst = header + 148u, .val = ' ', .bytes = 8u});
        unsigned long long sum = 0ull;
        for (unsigned int byte = 0u; byte < 512u; byte += 1u)
        {
            sum += header[byte];
        }
        octal_written(header + 148u, sum, 6u);
        header[155] = ' ';
        memor.cpy(&(MemoriaCfg){.dst = header + 512u, .src = entries[index][1], .bytes = data_size});
        at += 512ull + padded;
    }
    memor.set(&(MemoriaCfg){.dst = s_tar + at, .val = 0, .bytes = 1024u});
    return at + 1024ull;
}

// a record of `name` containing `blob`, written at `time`, ordinary, written after the `at` bytes of s_run: the bytes after it
static unsigned long long record_written(unsigned long long at, const char *name, const char *blob, unsigned long long time)
{
    const unsigned long long name_size = CLI_UNITY_LENGTH(name);
    const unsigned long long blob_size = CLI_UNITY_LENGTH(blob);
    const unsigned long long whole = ((24ull + name_size + blob_size + 7ull) & ~7ull) + 8ull;
    unsigned char *const bytes = s_run + at;
    memor.set(&(MemoriaCfg){.dst = bytes, .val = 0, .bytes = whole});
    unsigned long long place = 0ull;
    le_written(bytes, &place, time, 8u);
    le_written(bytes, &place, blob_size, 8u);
    le_written(bytes, &place, name_size, 4u);
    le_written(bytes, &place, QRY_ORDINARY, 4u);
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u, .src = name, .bytes = name_size});
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u + name_size, .src = blob, .bytes = blob_size});
    place = whole - 8ull;
    le_written(bytes, &place, qry_record_crc(bytes, whole - 8ull), 8u);
    return at + whole;
}

// a file of s_dir written, asserted
static void bytes_written(const char *name, const void *bytes, unsigned long long size)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = name, .joined = path);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = bytes, .size = size), name);
}

// what the folder `path` of s_dir contains, each child `name dir archive` joined by `|`, in s_list_result: 1, or 0 where it does not list
static int folder_listing(const char *path)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea line = {s_list_result, sizeof(s_list_result), 0ull};
    const int list_entries = EMBED_CALL(cli_archive_err_msg_root.list, CliArchiveErrMsgRootCfg, .root = s_dir, .path = path, .found = &s_children, .err_msg = &err_msg);
    for (unsigned int at = 0u; list_entries && (at < s_children.count); at += 1u)
    {
        const CliChild *const child = &s_children.item[at];
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, s_children.text + child->name);
        mmgr_line_put(&line, child->dir ? " 1" : " 0");
        mmgr_line_put(&line, child->archive ? " 1" : " 0");
    }
    (void)mmgr_line_finish(&line);
    return list_entries;
}

// the file `path` of s_dir read, asserted to be `required` with a 0 after it
static void assert_file_reads(const char *path, const char *required)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long size = 0ull;
    char *const read = EMBED_CALL(cli_archive_err_msg.take, CliArchiveErrMsgCfg, .root = s_dir, .path = path, .size_pointer = &size, .owner = g_cli_editor_owner, .err_msg = &err_msg);
    TEST_ASSERT_NOT_NULL_MESSAGE(read, path);
    CLI_TEST_ASSERT_TEXT(required, read, size);
    TEST_ASSERT_EQUAL_CHAR('\0', read[size]);
    g_cli_guard->persistent_buf_release(read, g_cli_editor_owner);
}

// `text` saved as the file `path` of s_dir: 1, or 0 with why in s_error
static int text_saved(const char *path, const char *text)
{
    VerbaLinea err_msg = {s_error, sizeof(s_error), 0ull};
    const int saved = EMBED_CALL(cli_archive_size_data.write, CliArchiveSizeDataCfg, .root = s_dir, .path = path, .data = (const unsigned char *)text, .size = CLI_UNITY_LENGTH(text), .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    return saved;
}

// `size` bytes deflated and inflated again: 1 where every byte comes back
static int deflated_and_back(const unsigned char *data, unsigned long long size)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long back = 0ull;
    unsigned long long used = 0ull;
    const unsigned long long packed = EMBED_CALL(cli_archive_size.deflate, CliArchiveSizeCfg, .data = data, .size = size, .out = s_packed, .capacity = sizeof(s_packed));
    return EMBED_CALL(cli_archive_size.inflate, CliArchiveSizeCfg, .data = s_packed, .size = packed, .out = s_back, .capacity = sizeof(s_back), .out_size = &back, .used = &used, .err_msg = &err_msg) && (back == size) && ((size == 0ull) || (memor.cmp(&(MemoriaCfg){.src = s_back, .other = data, .bytes = size}) == 0));
}

// the long text of repeating words written in s_text
static void text_of_words(void)
{
    static const char s_words[] = "impero reads archives ";
    for (unsigned int at = 0u; at < sizeof(s_text); at += 1u)
    {
        // a character of the words, its low bit turned at every 251st
        s_text[at] = (unsigned char)(s_words[(at * 7u) % 21u] ^ (((at % 251u) == 0u) ? 1 : 0));
    }
}

// a zip of four entries written at a.jar in a fresh folder
static void jar_written(void)
{
    cli_unity_folder("archive_zip", s_dir);
    bytes_written("a.jar", s_zip, zip_written(s_zip_entries, 4u));
}

// a run's file of four records written at query1-x.qry in a fresh folder
static void run_written(void)
{
    cli_unity_folder("archive_qry", s_dir);
    unsigned long long size = record_written(0ull, "bench/sqlite3.c", "int main(void)\n", 1ull);
    size = record_written(size, "window.trace", "paint 0.000 0.400\n", 2ull);
    size = record_written(size, "questions.txt", "why\n", 3ull);
    s_run_size = record_written(size, "window.trace", "paint 6.900 0.380\n", 4ull);
    bytes_written("query1-x.qry", s_run, s_run_size);
}

// a tar inside a zip written at outer.zip in a fresh folder
static void nested_written(void)
{
    static const char *const s_inner[2][2] = {{"lib/", ""}, {"lib/x.py", "print(1)\n"}};
    cli_unity_folder("archive_nested", s_dir);
    const ArchiveEntry outer = {"inner.tar", (const char *)s_tar, tar_written(s_inner, 2u), 1, 1};
    bytes_written("outer.zip", s_zip, zip_written(&outer, 1u));
}

// a tar whose first member's name a GNU long name member gives and whose second's a pax extended header gives, written
// at long.tar in a fresh folder
static void long_named_written(void)
{
    static const char *const s_entries[4][2] = {{"././@LongLink", "deep/long/name.txt"},
                                                {"short.txt", "one"},
                                                {"PaxHeader", "21 path=pax/name.txt\n"},
                                                {"other.txt", "two"}};
    cli_unity_folder("archive_long", s_dir);
    const unsigned long long size = tar_written(s_entries, 4u);
    s_tar[156] = 'L';
    s_tar[2048u + 156u] = 'x';
    bytes_written("long.tar", s_tar, size);
}

// a gzipped tar and a gzipped file written in a fresh folder
static void gzipped_written(void)
{
    static const char *const s_entries[2][2] = {{"a/b.txt", "one"}, {"c.txt", "two"}};
    cli_unity_folder("archive_gz", s_dir);
    const unsigned long long tar = tar_written(s_entries, 2u);
    bytes_written("src.tgz", s_back, EMBED_CALL(cli_archive_size.gzip, CliArchiveSizeCfg, .data = s_tar, .size = tar, .out = s_back, .capacity = sizeof(s_back)));
    bytes_written("big.log.gz", s_back, EMBED_CALL(cli_archive_size.gzip, CliArchiveSizeCfg, .data = (const unsigned char *)"line\n", .size = 5ull, .out = s_back, .capacity = sizeof(s_back)));
}

// what Python wrote out, its lines joined by `|`, into `out`, which is sized for `capacity`: 1, or 0 where it did not run whole
static int python_run_result(const char *code, char *out, unsigned long long capacity)
{
    const char *const words[5] = {s_python, "-c", code, s_dir, NULL};
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    CliRunResult *const run_result = cli_run_result_alloc();
    const int whole = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &err_msg) && (run_result->code == 0ll);
    VerbaLinea line = {out, capacity, 0ull};
    for (unsigned long long at = 0ull; whole && (at < run_result->out_size); at += 1ull)
    {
        const char character = run_result->out[at];
        if ((character == '\n') && (at + 1ull < run_result->out_size))
        {
            mmgr_line_ch(&line, '|');
        }
        else if ((character != '\n') && (character != '\r'))
        {
            mmgr_line_ch(&line, character);
        }
    }
    (void)mmgr_line_finish(&line);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return whole;
}

// archives Python's own zipfile and tarfile write made in a fresh folder, the case ignored with no Python
static void python_archives_created(void)
{
    static const char s_make[] = "import sys, zipfile, tarfile, io\nd = sys.argv[1]\nwith zipfile.ZipFile(d + '/py.zip', 'w', "
                                 "zipfile.ZIP_DEFLATED) as z:\n    z.writestr('src/m.py', 'x = 1\\n' * 500)\n    z.writestr('notes.txt', "
                                 "'plain')\nwith tarfile.open(d + '/py.tar.gz', 'w:gz') as t:\n    data = b'tar text'\n    info = "
                                 "tarfile.TarInfo('pkg/a.txt'); info.size = len(data); t.addfile(info, io.BytesIO(data))\n";
    char out[1024];
    cli_unity_folder("archive_python", s_dir);
    EMBED_CALL(cli_runner_name.python, CliRunnerNameCfg, .path = s_python);
    if (!python_run_result(s_make, out, sizeof(out)))
    {
        TEST_IGNORE_MESSAGE("no Python to write the archives with");
    }
}

// `path` with its backslashes made slashes, into `out`, which is sized for CLI_PATH_MAX_LEN
static void path_slashed(const char *path, char *out)
{
    memor.cpy(&(MemoriaCfg){.dst = out, .src = path, .bytes = CLI_UNITY_LENGTH(path) + 1u});
    for (char *at = out; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
}

// a folder with a zip and a plain file in lib written in a fresh folder
static void split_tree_written(void)
{
    static const ArchiveEntry s_entry = {"x", "x", 1ull, 0, 1};
    char lib[CLI_PATH_MAX_LEN];
    cli_unity_folder("archive_split", s_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "lib", .joined = lib);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = lib);
    bytes_written("lib/a.zip", s_zip, zip_written(&s_entry, 1u));
    bytes_written("lib/plain.txt", "", 0ull);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_deflate_and_inflate_restore_every_byte(void)
{
    static const char s_runs[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    text_of_words();
    TEST_ASSERT_TRUE(deflated_and_back((const unsigned char *)"", 0ull));
    TEST_ASSERT_TRUE(deflated_and_back((const unsigned char *)"a", 1ull));
    TEST_ASSERT_TRUE(deflated_and_back((const unsigned char *)s_runs, sizeof(s_runs) - 1u));
    TEST_ASSERT_TRUE(deflated_and_back(s_text, sizeof(s_text)));
}

void test_deflate_compresses_runs_to_lengths_and_distances(void)
{
    text_of_words();
    TEST_ASSERT_LESS_THAN_UINT64(sizeof(s_text) / 3u, EMBED_CALL(cli_archive_size.deflate, CliArchiveSizeCfg, .data = s_text, .size = sizeof(s_text), .out = s_packed, .capacity = sizeof(s_packed)));
}

void test_gzip_and_gunzip_restore_every_byte(void)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long back = 0ull;
    text_of_words();
    const unsigned long long zipped = EMBED_CALL(cli_archive_size.gzip, CliArchiveSizeCfg, .data = s_text, .size = sizeof(s_text), .out = s_packed, .capacity = sizeof(s_packed));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_archive_bytes.gunzip, CliArchiveBytesCfg, .bytes = s_packed, .size = zipped, .out = s_back, .capacity = sizeof(s_back), .out_size = &back, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT64(sizeof(s_text), back);
    TEST_ASSERT_EQUAL_MEMORY(s_text, s_back, sizeof(s_text));
}

void test_a_crc_is_what_zip_and_gzip_compute(void)
{
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, EMBED_CALL(cli_archive_bytes_size.crc32, CliArchiveBytesSizeCfg, .bytes = (const unsigned char *)"123456789", .size = 9ull));
    TEST_ASSERT_EQUAL_HEX32(0u, EMBED_CALL(cli_archive_bytes_size.crc32, CliArchiveBytesSizeCfg, .bytes = (const unsigned char *)"", .size = 0ull));
}

void test_the_dos_page_has_a_character_for_each_high_byte(void)
{
    unsigned char high[128];
    char out[512];
    VerbaLinea line = {out, sizeof(out), 0ull};
    for (unsigned int at = 0u; at < 128u; at += 1u)
    {
        // a high byte fits a byte
        high[at] = (unsigned char)(0x80u + at);
    }
    EMBED_CALL(cli_archive_bytes_size.dos, CliArchiveBytesSizeCfg, .bytes = high, .size = sizeof(high), .out_verba_linea_pointer = &line);
    unsigned int characters = 0u;
    for (unsigned long long at = 0ull; at < line.at; at += 1ull)
    {
        // a character's bits, as a byte
        characters += ((((unsigned char)out[at]) & 0xc0u) != 0x80u) ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT(128u, characters);
}

void test_the_dos_page_reads_as_the_page_defines_it(void)
{
    static const unsigned char s_some[4] = {'a', 0x81u, 0x9cu, 0xe1u};
    char out[512];
    VerbaLinea line = {out, sizeof(out), 0ull};
    EMBED_CALL(cli_archive_bytes_size.dos, CliArchiveBytesSizeCfg, .bytes = s_some, .size = sizeof(s_some), .out_verba_linea_pointer = &line);
    CLI_TEST_ASSERT_TEXT("a\xC3\xBC\xC2\xA3\xC3\x9F", out, line.at);
}

void test_a_zip_opens_as_a_folder_a_dos_name_read_in_it(void)
{
    jar_written();
    TEST_ASSERT_TRUE(folder_listing("a.jar"));
    TEST_ASSERT_EQUAL_STRING("docs 1 0|caf\xC3\xA9.txt 0 0|top.txt 0 0", s_list_result);
}

void test_a_name_not_in_utf8_stores_its_bytes(void)
{
    jar_written();
    TEST_ASSERT_TRUE(folder_listing("a.jar"));
    TEST_ASSERT_TRUE(s_children.item[1].has_raw);
    TEST_ASSERT_EQUAL_STRING("636166822e747874", s_children.text + s_children.item[1].raw);
}

void test_a_folder_in_a_zip_lists(void)
{
    jar_written();
    TEST_ASSERT_TRUE(folder_listing("a.jar/docs"));
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_children.count);
    TEST_ASSERT_EQUAL_STRING("a.jar/docs/read.me", s_children.text + s_children.item[0].path);
}

void test_a_file_in_a_zip_reads(void)
{
    jar_written();
    assert_file_reads("a.jar/docs/read.me", "hello from a zip\n");
}

void test_a_file_saved_back_in_a_zip_reads_as_saved_and_the_others_are_unchanged(void)
{
    char saving[CLI_PATH_MAX_LEN];
    jar_written();
    TEST_ASSERT_TRUE_MESSAGE(text_saved("a.jar/docs/read.me", "changed in place\n"), s_error);
    assert_file_reads("a.jar/docs/read.me", "changed in place\n");
    assert_file_reads("a.jar/top.txt", "top");
    TEST_ASSERT_TRUE(folder_listing("a.jar"));
    TEST_ASSERT_EQUAL_UINT(3u, s_children.count);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = ".a.jar.saving", .joined = saving);
    TEST_ASSERT_FALSE(EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = saving));
}

void test_a_qry_is_a_runs_file_regardless_of_case(void)
{
    TEST_ASSERT_EQUAL_INT(CLI_ARCHIVE_QRY, EMBED_CALL(cli_archive.type_get, CliArchiveCfg, .name = "build/qry/query1-x.QRY"));
}

void test_a_runs_file_lists_a_name_with_slashes_in_folders_of_its_own(void)
{
    run_written();
    TEST_ASSERT_TRUE(folder_listing("query1-x.qry"));
    TEST_ASSERT_EQUAL_STRING("bench 1 0|questions.txt 0 0|window.trace 0 0", s_list_result);
}

void test_a_blob_of_a_runs_file_reads(void)
{
    run_written();
    assert_file_reads("query1-x.qry/bench/sqlite3.c", "int main(void)\n");
}

void test_each_names_blobs_are_joined_in_the_order_written(void)
{
    run_written();
    assert_file_reads("query1-x.qry/window.trace", "paint 0.000 0.400\npaint 6.900 0.380\n");
}

void test_a_runs_record_is_not_written_back_and_its_file_is_unchanged(void)
{
    char path[CLI_PATH_MAX_LEN];
    run_written();
    TEST_ASSERT_FALSE(text_saved("query1-x.qry/questions.txt", "changed"));
    CLI_TEST_ASSERT_CONTAINS("not written back", s_error);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "query1-x.qry", .joined = path);
    const unsigned long long read = qry_member_file_read(path, (unsigned char *)s_read, sizeof(s_read));
    TEST_ASSERT_EQUAL_UINT64(s_run_size, read);
    TEST_ASSERT_EQUAL_MEMORY(s_run, s_read, s_run_size);
}

void test_an_archive_inside_an_archive_is_a_folder_and_an_archive(void)
{
    nested_written();
    TEST_ASSERT_TRUE(folder_listing("outer.zip"));
    TEST_ASSERT_EQUAL_STRING("inner.tar 1 1", s_list_result);
}

void test_an_archive_inside_an_archive_opens_and_its_file_reads(void)
{
    nested_written();
    TEST_ASSERT_TRUE(folder_listing("outer.zip/inner.tar"));
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_list_result, .other = "lib 1 ", .cap = CLI_POOL_BYTES}));
    assert_file_reads("outer.zip/inner.tar/lib/x.py", "print(1)\n");
}

void test_a_file_is_saved_back_through_both_archives(void)
{
    nested_written();
    TEST_ASSERT_TRUE_MESSAGE(text_saved("outer.zip/inner.tar/lib/x.py", "print(2)\n"), s_error);
    assert_file_reads("outer.zip/inner.tar/lib/x.py", "print(2)\n");
}

void test_a_gzipped_tar_opens(void)
{
    gzipped_written();
    TEST_ASSERT_TRUE(folder_listing("src.tgz"));
    TEST_ASSERT_EQUAL_STRING("a 1 0|c.txt 0 0", s_list_result);
}

void test_a_tar_member_named_by_a_long_name_or_a_pax_header_goes_by_that_name(void)
{
    long_named_written();
    TEST_ASSERT_TRUE(folder_listing("long.tar"));
    TEST_ASSERT_EQUAL_STRING("deep 1 0|pax 1 0", s_list_result);
    assert_file_reads("long.tar/deep/long/name.txt", "one");
    assert_file_reads("long.tar/pax/name.txt", "two");
}

void test_a_gzipped_tar_saves_back_and_the_other_file_is_unchanged(void)
{
    gzipped_written();
    TEST_ASSERT_TRUE_MESSAGE(text_saved("src.tgz/a/b.txt", "one, longer than it was"), s_error);
    assert_file_reads("src.tgz/a/b.txt", "one, longer than it was");
    assert_file_reads("src.tgz/c.txt", "two");
}

void test_a_gzipped_file_opens_as_its_one_file_which_reads(void)
{
    gzipped_written();
    TEST_ASSERT_TRUE(folder_listing("big.log.gz"));
    TEST_ASSERT_EQUAL_STRING("big.log 0 0", s_list_result);
    assert_file_reads("big.log.gz/big.log", "line\n");
}

void test_a_zip_python_wrote_reads(void)
{
    static char s_lines[3001];
    python_archives_created();
    for (unsigned int at = 0u; at < 500u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_lines + (at * 6u), .src = "x = 1\n", .bytes = 6u});
    }
    assert_file_reads("py.zip/src/m.py", s_lines);
}

void test_a_gzipped_tar_python_wrote_reads(void)
{
    python_archives_created();
    assert_file_reads("py.tar.gz/pkg/a.txt", "tar text");
}

void test_python_reads_what_orior_saved_back(void)
{
    static const char s_check[] = "import sys, zipfile, tarfile\nd = sys.argv[1]\nz = zipfile.ZipFile(d + '/py.zip')\nassert z.testzip() is "
                                  "None\nprint(z.read('notes.txt').decode())\nprint(len(z.read('src/m.py')))\nprint(tarfile.open(d + "
                                  "'/py.tar.gz').extractfile('pkg/a.txt').read().decode())\n";
    char out[1024];
    python_archives_created();
    TEST_ASSERT_TRUE_MESSAGE(text_saved("py.zip/notes.txt", "written by orior"), s_error);
    TEST_ASSERT_TRUE_MESSAGE(text_saved("py.tar.gz/pkg/a.txt", "tar text, written by orior"), s_error);
    TEST_ASSERT_TRUE(python_run_result(s_check, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("written by orior|3000|tar text, written by orior", out);
}

void test_a_path_runs_into_an_archive(void)
{
    char full[CLI_PATH_MAX_LEN];
    char required[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    split_tree_written();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "lib/a.zip", .joined = full);
    path_slashed(full, required);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = s_dir, .path = "lib/a.zip/x", .file = file, .inner = inner));
    path_slashed(file, full);
    TEST_ASSERT_EQUAL_STRING(required, full);
    TEST_ASSERT_EQUAL_STRING("x", inner);
}

void test_a_plain_file_is_no_archive(void)
{
    char file[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    split_tree_written();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = s_dir, .path = "lib/plain.txt", .file = file, .inner = inner));
}

void test_a_folder_is_no_archive(void)
{
    char file[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    split_tree_written();
    TEST_ASSERT_FALSE(EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = s_dir, .path = "lib", .file = file, .inner = inner));
}

void test_a_file_an_archive_does_not_contain_does_not_read(void)
{
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    unsigned long long size = 0ull;
    split_tree_written();
    TEST_ASSERT_NULL(EMBED_CALL(cli_archive_err_msg.take, CliArchiveErrMsgCfg, .root = s_dir, .path = "lib/a.zip/missing", .size_pointer = &size, .owner = g_cli_editor_owner, .err_msg = &err_msg));
}
