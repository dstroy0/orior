// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_test.c: the archives' check: deflate and inflate give back every byte; a CRC checks as zip and gzip reckon
// it; the DOS page has a character for each high byte; a zip opens as a folder, its files read and one saved back; a
// run's .qry opens as a folder of its blobs and is never written back; an archive inside an archive opens and saves
// too; a gzipped tar and a gzipped file open and save back; archives Python writes are read here and those orior
// writes are read by Python; and a path of the tree runs into an archive only past an archive's file
#include "../../../../../src/cu/includes/file_defs/qry/qry.h"
#include "../../../../../src/cu/includes/file_defs/qry/qry_read.h"
#include "../../../../../ui/cli/src/editor/archive.h"
#include "../../../../../ui/cli/src/runs/runner.h"
#include "cli_check.h"

// An entry of a zip written as other tools write one: deflated or stored, its name in UTF-8 or in the DOS page
typedef struct
{
    const char *name;
    const char *data;
    unsigned long long size;
    int deflated;
    int utf8;
} ArchiveEntry;

static unsigned char s_text[200000];
static unsigned char s_packed[1u << 20u];
static unsigned char s_back[1u << 20u];
static unsigned char s_zip[1u << 16u];
static unsigned char s_directory[1u << 14u];
static unsigned char s_tar[1u << 16u];
static unsigned char s_run[4096];
static char s_read[1u << 16u];
static CliChildren s_children;

// `value` written little-endian in `width` bytes at `out` from `at`, `at` moved past them
static void archive_le(unsigned char *out, unsigned long long *at, unsigned long long value, unsigned int width)
{
    for (unsigned int each = 0u; each < width; each += 1u)
    {
        // a byte of the value, its low eight bits kept
        out[*at + each] = (unsigned char)((value >> (8u * each)) & 0xffu);
    }
    *at += width;
}

// an entry's record head, a local one or the central directory's, put after the `at` bytes of `out`
static void archive_head(unsigned char *out, unsigned long long *at, const ArchiveEntry *entry, unsigned long long body,
                         unsigned long long offset, int central)
{
    const unsigned long long name_size = cellul.len(&(CatenaFinitaCfg){.src = entry->name, .cap = CLI_CELLS_POOL});
    const unsigned long long data_size = entry->size;
    archive_le(out, at, central ? 0x02014b50ull : 0x04034b50ull, 4u);
    if (central)
    {
        archive_le(out, at, 20ull, 2u);
    }
    archive_le(out, at, 20ull, 2u);
    archive_le(out, at, entry->utf8 ? (1ull << 11u) : 0ull, 2u);
    archive_le(out, at, entry->deflated ? 8ull : 0ull, 2u);
    archive_le(out, at, 0ull, 4u);
    archive_le(out, at, cli_crc32((const unsigned char *)entry->data, data_size), 4u);
    archive_le(out, at, body, 4u);
    archive_le(out, at, data_size, 4u);
    archive_le(out, at, name_size, 2u);
    archive_le(out, at, 0ull, 2u);
    if (central)
    {
        // the comment's length, the disk, and the inner and outer attributes
        archive_le(out, at, 0ull, 4u);
        archive_le(out, at, 0ull, 2u);
        archive_le(out, at, 0ull, 4u);
        archive_le(out, at, offset, 4u);
    }
    memor.cpy(&(MemoriaCfg){.dst = out + *at, .src = entry->name, .bytes = name_size});
    *at += name_size;
}

// a zip of `count` entries into s_zip: its size
static unsigned long long archive_zip_of(const ArchiveEntry *entries, unsigned int count)
{
    unsigned long long at = 0ull;
    unsigned long long directory = 0ull;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const ArchiveEntry *const entry = &entries[index];
        const unsigned long long data_size = entry->size;
        const unsigned long long body =
            entry->deflated ? cli_deflate((const unsigned char *)entry->data, data_size, s_packed, sizeof(s_packed))
                            : data_size;
        const unsigned long long offset = at;
        archive_head(s_zip, &at, entry, body, offset, 0);
        memor.cpy(&(MemoriaCfg){.dst = s_zip + at,
                                .src = entry->deflated ? (const void *)s_packed : (const void *)entry->data,
                                .bytes = body});
        at += body;
        archive_head(s_directory, &directory, entry, body, offset, 1);
    }
    const unsigned long long start = at;
    memor.cpy(&(MemoriaCfg){.dst = s_zip + at, .src = s_directory, .bytes = directory});
    at += directory;
    static const unsigned int s_widths[7] = {4u, 4u, 2u, 2u, 4u, 4u, 2u};
    const unsigned long long tail[7] = {0x06054b50ull, 0ull, count, count, directory, start, 0ull};
    for (unsigned int field = 0u; field < 7u; field += 1u)
    {
        archive_le(s_zip, &at, tail[field], s_widths[field]);
    }
    return at;
}

// `value` written in `digits` octal digits at `out`, a 0 after them
static void archive_octal(unsigned char *out, unsigned long long value, unsigned int digits)
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
static unsigned long long archive_tar_of(const char *const (*entries)[2], unsigned int count)
{
    unsigned long long at = 0ull;
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const name = entries[index][0];
        const unsigned long long name_size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        const unsigned long long data_size =
            cellul.len(&(CatenaFinitaCfg){.src = entries[index][1], .cap = CLI_CELLS_POOL});
        const unsigned long long padded = ((data_size + 511ull) / 512ull) * 512ull;
        unsigned char *const header = s_tar + at;
        memor.set(&(MemoriaCfg){.dst = header, .val = 0, .bytes = 512ull + padded});
        memor.cpy(&(MemoriaCfg){.dst = header, .src = name, .bytes = name_size});
        memor.cpy(&(MemoriaCfg){.dst = header + 100u, .src = "0000644", .bytes = 8u});
        archive_octal(header + 124u, data_size, 11u);
        header[156] = (unsigned char)((name[name_size - 1u] == '/') ? '5' : '0');
        memor.cpy(&(MemoriaCfg){.dst = header + 257u, .src = "ustar", .bytes = 6u});
        memor.set(&(MemoriaCfg){.dst = header + 148u, .val = ' ', .bytes = 8u});
        unsigned long long sum = 0ull;
        for (unsigned int byte = 0u; byte < 512u; byte += 1u)
        {
            sum += header[byte];
        }
        archive_octal(header + 148u, sum, 6u);
        header[155] = ' ';
        memor.cpy(&(MemoriaCfg){.dst = header + 512u, .src = entries[index][1], .bytes = data_size});
        at += 512ull + padded;
    }
    memor.set(&(MemoriaCfg){.dst = s_tar + at, .val = 0, .bytes = 1024u});
    return at + 1024ull;
}

// a record of `name` holding `blob`, written at `time`, ordinary, put after the `at` bytes of s_run: the bytes after it
static unsigned long long archive_record(unsigned long long at, const char *name, const char *blob,
                                         unsigned long long time)
{
    const unsigned long long name_size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const unsigned long long blob_size = cellul.len(&(CatenaFinitaCfg){.src = blob, .cap = CLI_CELLS_POOL});
    const unsigned long long whole = ((24ull + name_size + blob_size + 7ull) & ~7ull) + 8ull;
    unsigned char *const bytes = s_run + at;
    memor.set(&(MemoriaCfg){.dst = bytes, .val = 0, .bytes = whole});
    unsigned long long place = 0ull;
    archive_le(bytes, &place, time, 8u);
    archive_le(bytes, &place, blob_size, 8u);
    archive_le(bytes, &place, name_size, 4u);
    archive_le(bytes, &place, QRY_ORDINARY, 4u);
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u, .src = name, .bytes = name_size});
    memor.cpy(&(MemoriaCfg){.dst = bytes + 24u + name_size, .src = blob, .bytes = blob_size});
    place = whole - 8ull;
    archive_le(bytes, &place, qry_record_crc(bytes, whole - 8ull), 8u);
    return at + whole;
}

// a file of the folder `dir` written: 1, or 0
static int archive_written(const char *dir, const char *name, const void *bytes, unsigned long long size)
{
    char path[CLI_PATH_LONGEST];
    cli_full(dir, name, path);
    return query_file_written(path, bytes, size);
}

// what the folder `path` of the tree `dir` holds, each child `name dir archive` joined by `|`, into `out`, which holds
// `room`: 1, or 0 where it does not list
static int archive_listed(const char *dir, const char *path, char *out, unsigned long long room)
{
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine line = {out, room, 0ull};
    const int listed = cli_archive_list(dir, path, &s_children, &said);
    for (unsigned int at = 0u; listed && (at < s_children.count); at += 1u)
    {
        const CliChild *const child = &s_children.item[at];
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, s_children.text + child->name);
        cli_line_text(&line, child->dir ? " 1" : " 0");
        cli_line_text(&line, child->archive ? " 1" : " 0");
    }
    (void)cli_line_closed(&line);
    return listed;
}

// the file `path` of the tree `dir` read, checked against `wanted`
static void archive_read_is(const char *dir, const char *path, const char *wanted, const char *what)
{
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_read, sizeof(s_read), 0ull};
    cli_check(cli_archive_read(dir, path, &out, &said), what);
    cli_check_text(s_read, out.at, wanted, what);
}

// `text` saved as the file `path` of the tree `dir`: 1, or 0 with why into `why`
static int archive_saved(const char *dir, const char *path, const char *text, char *why, unsigned long long room)
{
    CliLine said = {why, room, 0ull};
    const int saved = cli_archive_write(dir, path, (const unsigned char *)text,
                                        cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &said);
    (void)cli_line_closed(&said);
    return saved;
}

// `size` bytes deflated and inflated again: 1 where every byte comes back
static int archive_round(const unsigned char *data, unsigned long long size)
{
    const unsigned long long packed = cli_deflate(data, size, s_packed, sizeof(s_packed));
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned long long back = 0ull;
    unsigned long long used = 0ull;
    return cli_inflate(s_packed, packed, s_back, sizeof(s_back), &back, &used, &said) && (back == size) &&
           ((size == 0ull) || (memor.cmp(&(MemoriaCfg){.src = s_back, .other = data, .bytes = size}) == 0));
}

// deflate and inflate give back every byte, and gzip and gunzip
static void archive_deflate(void)
{
    static const char s_words[] = "orior_cli reads archives ";
    for (unsigned int at = 0u; at < sizeof(s_text); at += 1u)
    {
        // a letter of the words, its low bit turned at every 251st
        s_text[at] = (unsigned char)(s_words[(at * 7u) % 21u] ^ (((at % 251u) == 0u) ? 1 : 0));
    }
    static const char s_runs[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    cli_check(archive_round((const unsigned char *)"", 0ull) && archive_round((const unsigned char *)"a", 1ull) &&
                  archive_round((const unsigned char *)s_runs, sizeof(s_runs) - 1u) &&
                  archive_round(s_text, sizeof(s_text)),
              "deflate and inflate give back every byte");
    cli_check(cli_deflate(s_text, sizeof(s_text), s_packed, sizeof(s_packed)) < (sizeof(s_text) / 3u),
              "runs come back as lengths and distances");
    const unsigned long long zipped = cli_gzip(s_text, sizeof(s_text), s_packed, sizeof(s_packed));
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned long long back = 0ull;
    cli_check(cli_gunzip(s_packed, zipped, s_back, sizeof(s_back), &back, &said) && (back == sizeof(s_text)) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_back, .other = s_text, .bytes = sizeof(s_text)}) == 0),
              "gzip and gunzip give back every byte");
}

// the DOS page has a character for each high byte
static void archive_dos(void)
{
    unsigned char high[128];
    for (unsigned int at = 0u; at < 128u; at += 1u)
    {
        high[at] = (unsigned char)(0x80u + at);
    }
    char out[512];
    CliLine line = {out, sizeof(out), 0ull};
    cli_dos(high, sizeof(high), &line);
    unsigned int characters = 0u;
    for (unsigned long long at = 0ull; at < line.at; at += 1ull)
    {
        characters += ((((unsigned char)out[at]) & 0xc0u) != 0x80u) ? 1u : 0u;
    }
    cli_check(characters == 128u, "the DOS page has a character for each high byte");
    static const unsigned char s_some[4] = {'a', 0x81u, 0x9cu, 0xe1u};
    line.at = 0ull;
    cli_dos(s_some, sizeof(s_some), &line);
    cli_check_text(out, line.at, "a\xC3\xBC\xC2\xA3\xC3\x9F", "and reads as the page gives it");
}

// a zip opens as a folder, its files read and one saved back
static void archive_zip(void)
{
    static const ArchiveEntry s_entries[4] = {{"docs/read.me", "hello from a zip\n", 17ull, 1, 1},
                                              {"docs/", "", 0ull, 0, 1},
                                              {"top.txt", "top", 3ull, 0, 1},
                                              {"caf\x82.txt", "dos", 3ull, 0, 0}};
    char dir[CLI_PATH_LONGEST];
    char listed[1024];
    char why[1024];
    cli_check_folder("archive_zip", dir);
    cli_check(archive_written(dir, "a.jar", s_zip, archive_zip_of(s_entries, 4u)), "a zip is written");
    cli_check(archive_listed(dir, "a.jar", listed, sizeof(listed)) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = listed, .other = "docs 1 0|caf\xC3\xA9.txt 0 0|top.txt 0 0", .cap = CLI_CELLS_POOL})),
              "a zip opens as a folder, a name in the DOS page read in it");
    cli_check(
        s_children.item[1].has_raw &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = s_children.text + s_children.item[1].raw, .other = "636166822e747874", .cap = CLI_CELLS_POOL})),
        "a name not in UTF-8 keeps its bytes");
    cli_check(archive_listed(dir, "a.jar/docs", listed, sizeof(listed)) && (s_children.count > 0u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_children.text + s_children.item[0].path,
                                                .other = "a.jar/docs/read.me",
                                                .cap = CLI_CELLS_POOL})),
              "a folder in it lists");
    archive_read_is(dir, "a.jar/docs/read.me", "hello from a zip\n", "its file reads");
    cli_check(archive_saved(dir, "a.jar/docs/read.me", "changed in place\n", why, sizeof(why)), "a file is saved back");
    archive_read_is(dir, "a.jar/docs/read.me", "changed in place\n", "and reads as saved");
    archive_read_is(dir, "a.jar/top.txt", "top", "the other files are as they were");
    cli_check(archive_listed(dir, "a.jar", listed, sizeof(listed)) && (s_children.count == 3u), "and as many");
    char saving[CLI_PATH_LONGEST];
    cli_full(dir, ".a.jar.saving", saving);
    cli_check(!query_is_file(saving), "the archive written anew is put in the old one's place");
}

// a run's .qry opens as a folder of its blobs, each name's joined, and is never written back
static void archive_qry(void)
{
    char dir[CLI_PATH_LONGEST];
    char listed[1024];
    char why[1024];
    cli_check_folder("archive_qry", dir);
    unsigned long long size = archive_record(0ull, "bench/sqlite3.c", "int main(void)\n", 1ull);
    size = archive_record(size, "window.trace", "paint 0.000 0.400\n", 2ull);
    size = archive_record(size, "questions.txt", "why\n", 3ull);
    size = archive_record(size, "window.trace", "paint 6.900 0.380\n", 4ull);
    cli_check(archive_written(dir, "query1-x.qry", s_run, size), "a run's file is written");
    cli_check(cli_archive_kind_of("build/qry/query1-x.QRY") == CLI_ARCHIVE_QRY, "a .qry is a run's, case aside");
    cli_check(archive_listed(dir, "query1-x.qry", listed, sizeof(listed)) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = listed, .other = "bench 1 0|questions.txt 0 0|window.trace 0 0", .cap = CLI_CELLS_POOL})),
              "a name with slashes in folders of its own");
    archive_read_is(dir, "query1-x.qry/bench/sqlite3.c", "int main(void)\n", "a blob reads");
    archive_read_is(dir, "query1-x.qry/window.trace", "paint 0.000 0.400\npaint 6.900 0.380\n",
                    "each name's blobs joined in the order handed");
    cli_check(
        !archive_saved(dir, "query1-x.qry/questions.txt", "changed", why, sizeof(why)) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "not written back", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a run's record is not written back");
    char path[CLI_PATH_LONGEST];
    cli_full(dir, "query1-x.qry", path);
    const unsigned long long read = qry_member_file_read(path, (unsigned char *)s_read, sizeof(s_read));
    cli_check((read == size) && (memor.cmp(&(MemoriaCfg){.src = s_read, .other = s_run, .bytes = size}) == 0),
              "the run's file as it was");
}

// an archive inside an archive opens and saves as a folder too
static void archive_nested(void)
{
    static const char *const s_inner[2][2] = {{"lib/", ""}, {"lib/x.py", "print(1)\n"}};
    char dir[CLI_PATH_LONGEST];
    char listed[1024];
    char why[1024];
    cli_check_folder("archive_nested", dir);
    const ArchiveEntry outer = {"inner.tar", (const char *)s_tar, archive_tar_of(s_inner, 2u), 1, 1};
    cli_check(archive_written(dir, "outer.zip", s_zip, archive_zip_of(&outer, 1u)), "a tar in a zip is written");
    cli_check(archive_listed(dir, "outer.zip", listed, sizeof(listed)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = listed, .other = "inner.tar 1 1", .cap = CLI_CELLS_POOL})),
              "the archive inside is a folder and an archive");
    cli_check(archive_listed(dir, "outer.zip/inner.tar", listed, sizeof(listed)) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = listed, .other = "lib 1 ", .cap = CLI_CELLS_POOL})),
              "and opens");
    archive_read_is(dir, "outer.zip/inner.tar/lib/x.py", "print(1)\n", "its file reads");
    cli_check(archive_saved(dir, "outer.zip/inner.tar/lib/x.py", "print(2)\n", why, sizeof(why)),
              "and is saved back through both");
    archive_read_is(dir, "outer.zip/inner.tar/lib/x.py", "print(2)\n", "and reads as saved");
}

// a gzipped tar and a gzipped file open and save back
static void archive_gz(void)
{
    static const char *const s_entries[2][2] = {{"a/b.txt", "one"}, {"c.txt", "two"}};
    char dir[CLI_PATH_LONGEST];
    char listed[1024];
    char why[1024];
    cli_check_folder("archive_gz", dir);
    const unsigned long long tar = archive_tar_of(s_entries, 2u);
    cli_check(archive_written(dir, "src.tgz", s_back, cli_gzip(s_tar, tar, s_back, sizeof(s_back))) &&
                  archive_written(dir, "big.log.gz", s_back,
                                  cli_gzip((const unsigned char *)"line\n", 5ull, s_back, sizeof(s_back))),
              "a gzipped tar and a gzipped file are written");
    cli_check(archive_listed(dir, "src.tgz", listed, sizeof(listed)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = listed, .other = "a 1 0|c.txt 0 0", .cap = CLI_CELLS_POOL})),
              "a gzipped tar opens");
    cli_check(archive_saved(dir, "src.tgz/a/b.txt", "one, longer than it was", why, sizeof(why)), "and saves back");
    archive_read_is(dir, "src.tgz/a/b.txt", "one, longer than it was", "the file saved reads as saved");
    archive_read_is(dir, "src.tgz/c.txt", "two", "and the other as it was");
    cli_check(archive_listed(dir, "big.log.gz", listed, sizeof(listed)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = listed, .other = "big.log 0 0", .cap = CLI_CELLS_POOL})),
              "a gzipped file opens as its one file");
    archive_read_is(dir, "big.log.gz/big.log", "line\n", "which reads");
}

// what Python wrote out, its lines joined by `|`, into `out`, which holds `room`: 1, or 0 where it did not run whole
static int archive_python(const char *python, const char *code, const char *dir, char *out, unsigned long long room)
{
    const char *const words[5] = {python, "-c", code, dir, NULL};
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    const int whole = cli_ran(words, NULL, NULL, 0u, ran, &said) && (ran->code == 0ll);
    CliLine line = {out, room, 0ull};
    for (unsigned long long at = 0ull; whole && (at < ran->out_size); at += 1ull)
    {
        const char letter = ran->out[at];
        if ((letter == '\n') && (at + 1ull < ran->out_size))
        {
            cli_line_character(&line, '|');
        }
        else if ((letter != '\n') && (letter != '\r'))
        {
            cli_line_character(&line, letter);
        }
    }
    (void)cli_line_closed(&line);
    cli_ran_given(ran);
    return whole;
}

// archives Python's own zipfile and tarfile write are read here, and what orior_cli writes back is read by them in turn
static void archive_python_made(void)
{
    static const char s_make[] =
        "import sys, zipfile, tarfile, io\nd = sys.argv[1]\nwith zipfile.ZipFile(d + '/py.zip', 'w', "
        "zipfile.ZIP_DEFLATED) as z:\n    z.writestr('src/m.py', 'x = 1\\n' * 500)\n    z.writestr('notes.txt', "
        "'plain')\nwith tarfile.open(d + '/py.tar.gz', 'w:gz') as t:\n    data = b'tar text'\n    info = "
        "tarfile.TarInfo('pkg/a.txt'); info.size = len(data); t.addfile(info, io.BytesIO(data))\n";
    static const char s_check[] =
        "import sys, zipfile, tarfile\nd = sys.argv[1]\nz = zipfile.ZipFile(d + '/py.zip')\nassert z.testzip() is "
        "None\nprint(z.read('notes.txt').decode())\nprint(len(z.read('src/m.py')))\nprint(tarfile.open(d + "
        "'/py.tar.gz').extractfile('pkg/a.txt').read().decode())\n";
    char dir[CLI_PATH_LONGEST];
    char python[CLI_PATH_LONGEST];
    char out[1024];
    char why[1024];
    cli_check_folder("archive_python", dir);
    cli_runner_python(python);
    if (!archive_python(python, s_make, dir, out, sizeof(out)))
    {
        cli_check(1, "no Python to write the archives with");
        return;
    }
    static char s_lines[3001];
    for (unsigned int at = 0u; at < 500u; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_lines + (at * 6u), .src = "x = 1\n", .bytes = 6u});
    }
    archive_read_is(dir, "py.zip/src/m.py", s_lines, "a zip Python wrote reads");
    archive_read_is(dir, "py.tar.gz/pkg/a.txt", "tar text", "and a gzipped tar");
    cli_check(archive_saved(dir, "py.zip/notes.txt", "written by orior", why, sizeof(why)) &&
                  archive_saved(dir, "py.tar.gz/pkg/a.txt", "tar text, written by orior", why, sizeof(why)),
              "each is saved back");
    cli_check(archive_python(python, s_check, dir, out, sizeof(out)) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = out, .other = "written by orior|3000|tar text, written by orior", .cap = CLI_CELLS_POOL})),
              "and Python reads what orior_cli wrote");
}

// `path` with its backslashes made slashes, into `out`, which holds CLI_PATH_LONGEST
static void archive_slashed(const char *path, char *out)
{
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = out; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
}

// a path of the tree runs into an archive only past an archive's file
static void archive_split(void)
{
    static const ArchiveEntry s_entry = {"x", "x", 1ull, 0, 1};
    char dir[CLI_PATH_LONGEST];
    char lib[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char inner[CLI_PATH_LONGEST];
    char wanted[CLI_PATH_LONGEST];
    cli_check_folder("archive_split", dir);
    cli_full(dir, "lib", lib);
    (void)query_folder_made(lib);
    cli_check(archive_written(dir, "lib/a.zip", s_zip, archive_zip_of(&s_entry, 1u)) &&
                  archive_written(dir, "lib/plain.txt", "", 0ull),
              "a zip and a plain file are written");
    char full[CLI_PATH_LONGEST];
    cli_full(dir, "lib/a.zip", full);
    archive_slashed(full, wanted);
    const int split = cli_archive_split(dir, "lib/a.zip/x", file, inner);
    archive_slashed(file, full);
    cli_check(split && (cellul.eq(&(CatenaFinitaCfg){.src = full, .other = wanted, .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = inner, .other = "x", .cap = CLI_CELLS_POOL})),
              "a path runs into an archive");
    cli_check(!cli_archive_split(dir, "lib/plain.txt", file, inner), "a plain file is no archive");
    cli_check(!cli_archive_split(dir, "lib", file, inner), "nor a folder");
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_read, sizeof(s_read), 0ull};
    cli_check(!cli_archive_read(dir, "lib/a.zip/missing", &out, &said),
              "a file an archive does not hold does not read");
}

int main(void)
{
    archive_deflate();
    cli_check((cli_crc32((const unsigned char *)"123456789", 9ull) == 0xCBF43926u) &&
                  (cli_crc32((const unsigned char *)"", 0ull) == 0u),
              "a CRC checks as zip and gzip reckon it");
    archive_dos();
    archive_zip();
    archive_qry();
    archive_nested();
    archive_gz();
    archive_python_made();
    archive_split();
    return cli_checked();
}
