// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_kept.c: each request's last answers kept in orior's own folder, under requests/, a folder for the file named
// by the SHA-256 of its path and a file for the request named by the SHA-256 of its key, the last answer first, ten of
// them, each body cut to 1 MiB; and the answers kept read back
#include "../text/digest.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the most one answer kept takes, written, and the bytes the answers are held in before each piece is added to the file
#define CLI_REQUESTS_PIECE (16u << 20u)
#define CLI_REQUESTS_ROOM 16384u

static CliLock s_keeping = CLI_LOCK_INIT;

// The file the answers are added to as their line fills: its path, and whether the next piece makes it anew
typedef struct
{
    const char *path;
    int fresh;
} CliKeptWriting;

// a piece added to the file the answers are written in, the first made anew: 1, or 0 where it could not be
static int kept_appended(void *context, const char *bytes, unsigned long long size)
{
    CliKeptWriting *const writing = context;
    const int put = cli_file_appended(writing->path, bytes, size, writing->fresh);
    writing->fresh = 0;
    return put;
}

// the first 16 hex digits of the SHA-256 of `size` bytes written into `out`
static void kept_named(const char *bytes, unsigned long long size, CliLine *out)
{
    char digest[65];
    CliLine digest_line = {digest, sizeof(digest), 0ull};
    cli_sha256_text(bytes, size, &digest_line);
    cli_line_counted(out, digest, 16u);
}

// the folder in orior's own folder that keeps the answers of the requests of the file at `path`, into `folder`, which
// holds CLI_PATH_LONGEST: 1, or 0 with why said. The path is named with the system's own separator, as Rust's PathBuf
// writes it
static int kept_folder(const char *root, const char *path, char *folder, CliLine *said)
{
    char file[CLI_PATH_LONGEST];
    char home[CLI_PATH_LONGEST];
    char why[CLI_SAID_LONGEST];
    CliLine none = {why, sizeof(why), 0ull};
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    const int known = cli_inside(root, path, file, &none) && cli_home_folder(home);
    if (known)
    {
#if defined(_WIN32)
        for (char *at = file; *at != '\0'; at += 1)
        {
            *at = (*at == '/') ? '\\' : *at;
        }
#endif
        cli_line_text(&line, home);
        cli_line_text(&line, "/requests/");
        kept_named(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), &line);
    }
    if (!known || (cli_line_closed(&line) == 0ull))
    {
        cli_line_text(said, "orior's own folder is not known");
        return 0;
    }
    return 1;
}

// the file in `folder` that keeps the answers of the request named `key`, and the file it is written as before it
// takes that one's place, into `file` and `writing`, which each hold CLI_PATH_LONGEST: 1, or 0 where a path does not
// fit
static int kept_file(const char *folder, const char *key, unsigned long long key_size, char *file, char *writing)
{
    CliLine line = {file, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    kept_named(key, key_size, &line);
    cli_line_text(&line, ".json");
    CliLine writing_line = {writing, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&writing_line, file, line.at);
    cli_line_text(&writing_line, ".writing");
    return (cli_line_closed(&line) != 0ull) && (cli_line_closed(&writing_line) != 0ull);
}

// whether a byte is white as JSON has it
static int kept_white(unsigned char byte)
{
    return (byte == ' ') || (byte == '\t') || (byte == '\r') || (byte == '\n');
}

// the answers the file at `file` keeps, an array of them, its bytes mapped into `*bytes`, `*size` of them, and where
// its first item may start into `*first`: 1, or 0 where there is none or it holds no array, nothing then mapped
static int kept_read(const char *file, const unsigned char **bytes, unsigned long long *size, unsigned long long *first)
{
    *bytes = cli_file_held(file, size);
    CliLine none = {NULL, 0ull, 0ull};
    unsigned long long at = 0ull;
    while ((*bytes != NULL) && (at < *size) && kept_white((*bytes)[at]))
    {
        at += 1u;
    }
    if ((*bytes != NULL) &&
        ((at == *size) || ((*bytes)[at] != '[') || !cli_json_compacted((const char *)*bytes, *size, &none)))
    {
        cli_file_let_go(*bytes, *size);
        *bytes = NULL;
    }
    *first = at + 1u;
    return *bytes != NULL;
}

// the answers kept before, from `at` of `size` bytes, the first CLI_REQUESTS_KEEP - 1 of them written into `piece`,
// each after a comma: 1, or 0 where one could not be
static int kept_before(CliLine *piece, const unsigned char *bytes, unsigned long long size, unsigned long long at)
{
    for (unsigned int kept = 1u; kept < CLI_REQUESTS_KEEP; kept += 1u)
    {
        const unsigned long long end = cli_json_value_end((const char *)bytes, size, at);
        if (end == at)
        {
            return 1;
        }
        cli_line_character(piece, ',');
        if (!cli_json_compacted((const char *)bytes + at, end - at, piece) || !cli_line_fits(piece))
        {
            return 0;
        }
        at = end;
        while ((at < size) && kept_white(bytes[at]))
        {
            at += 1u;
        }
        at += ((at < size) && (bytes[at] == ',')) ? 1u : 0u;
    }
    return 1;
}

int cli_requests_keep(const char *root, const char *path, const char *key, unsigned long long key_size,
                      const CliRequestsAnswer *answer, const CliRequestsSent *sent, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char writing[CLI_PATH_LONGEST];
    if (!kept_folder(root, path, folder, said))
    {
        return 0;
    }
    if (!kept_file(folder, key, key_size, file, writing))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": its path is too long");
        return 0;
    }
    cli_lock_take(&s_keeping);
    const int made = query_folder_made(folder);
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long first = 0ull;
    const int before = made && kept_read(file, &bytes, &size, &first);
    // the answers written into a room of their own, each piece added to the file as the room fills
    char room[CLI_REQUESTS_ROOM];
    CliKeptWriting target = {writing, 1};
    CliLine piece = {room, sizeof(room), 0ull, kept_appended, &target, 0ull};
    int done = made;
    if (done)
    {
        cli_line_character(&piece, '[');
        cli_requests_answer_written(answer, sent, CLI_REQUESTS_KEPT_BODY, &piece);
        done = cli_line_fits(&piece) && ((piece.flushed + piece.at) <= CLI_REQUESTS_PIECE) &&
               (!before || kept_before(&piece, bytes, size, first));
    }
    if (before)
    {
        cli_file_let_go(bytes, size);
    }
    cli_line_character(&piece, ']');
    done = done && cli_line_flushed(&piece) && cli_moved(writing, file);
    cli_lock_give(&s_keeping);
    if (!done)
    {
        cli_line_text(said, made ? file : folder);
        cli_line_text(said, ": it could not be written");
    }
    return done;
}

int cli_requests_answers(const char *root, const char *path, const char *text, unsigned long long size,
                         unsigned long long line, CliLine *out, CliLine *said)
{
    char key[CLI_REQUESTS_ADDRESS + CLI_REQUESTS_NAME];
    CliLine key_line = {key, sizeof(key), 0ull};
    char folder[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char writing[CLI_PATH_LONGEST];
    if (!cli_requests_key_at(text, size, line, &key_line, said) || !kept_folder(root, path, folder, said))
    {
        return 0;
    }
    if (!cli_line_fits(&key_line) || !kept_file(folder, key, key_line.at, file, writing))
    {
        cli_line_text(said, "the request is larger than orior_cli holds");
        return 0;
    }
    cli_line_text(out, "{\"key\":");
    cli_json_quoted(out, key, key_line.at);
    cli_line_text(out, ",\"answers\":");
    cli_lock_take(&s_keeping);
    const unsigned char *bytes = NULL;
    unsigned long long held = 0ull;
    unsigned long long first = 0ull;
    if (kept_read(file, &bytes, &held, &first))
    {
        (void)cli_json_compacted((const char *)bytes, held, out);
        cli_file_let_go(bytes, held);
    }
    else
    {
        cli_line_text(out, "[]");
    }
    cli_lock_give(&s_keeping);
    cli_line_character(out, '}');
    return 1;
}
