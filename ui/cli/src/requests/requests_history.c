// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_history.c: each request's last answers stored in orior's own folder, under requests/, a folder for the file
// named by the SHA-256 of its path and a file for the request named by the SHA-256 of its key, the last answer first,
// ten of them, each body cut to 1 MiB; and the answers stored read back
#include "../text/digest.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the most one answer stored takes, written, and the bytes the answers are buffered in before each piece is added to
// the file
#define CLI_REQUESTS_PIECE (16u << 20u)
#define CLI_REQUESTS_ROOM 16384u

static CliLock s_history_lock = CLI_LOCK_INIT;

// The file the answers are added to as their line fills: its path, and whether the next piece makes it anew
typedef struct
{
    const char *path;
    int fresh;
} CliStateWriting;

// a piece added to the file the answers are written in, the first made anew: 1, or 0 where it could not be
static int history_appended(void *context, const char *bytes, size_t size)
{
    CliStateWriting *const writing = context;
    const int put = cli_file_appended(writing->path, bytes, size, writing->fresh);
    writing->fresh = 0;
    return put;
}

// the first 16 hex digits of the SHA-256 of `size` bytes written into `out`
static void history_named(const char *bytes, unsigned long long size, VerbaLinea *out)
{
    char digest[65];
    VerbaLinea digest_line = {digest, sizeof(digest), 0ull};
    cli_sha256_text(bytes, size, &digest_line);
    mmgr_line_put_n(out, digest, 16u);
}

// the folder in orior's own folder that stores the answers of the requests of the file at `path`, into `folder`, which
// is sized for CLI_PATH_MAX_LEN: 1, or 0 with why given. The path is named with the system's own separator, as Rust's
// PathBuf writes it
static int history_folder(const char *root, const char *path, char *folder, VerbaLinea *err_msg)
{
    char file[CLI_PATH_MAX_LEN];
    char home[CLI_PATH_MAX_LEN];
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ull};
    VerbaLinea line = {folder, CLI_PATH_MAX_LEN, 0ull};
    const int known = cli_inside(root, path, file, &none) && cli_home_folder(home);
    if (known)
    {
#if defined(_WIN32)
        for (char *at = file; *at != '\0'; at += 1)
        {
            *at = (*at == '/') ? '\\' : *at;
        }
#endif
        mmgr_line_put(&line, home);
        mmgr_line_put(&line, "/requests/");
        history_named(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), &line);
    }
    if (!known || (mmgr_line_finish(&line) == 0ull))
    {
        mmgr_line_put(err_msg, "orior's own folder is not known");
        return 0;
    }
    return 1;
}

// the file in `folder` that stores the answers of the request named `key`, and the file it is written as before it
// takes that one's place, into `file` and `writing`, which each are sized for CLI_PATH_MAX_LEN: 1, or 0 where a path
// does not fit
static int history_file(const char *folder, const char *key, unsigned long long key_size, char *file, char *writing)
{
    VerbaLinea line = {file, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    history_named(key, key_size, &line);
    mmgr_line_put(&line, ".json");
    VerbaLinea writing_line = {writing, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put_n(&writing_line, file, line.at);
    mmgr_line_put(&writing_line, ".writing");
    return (mmgr_line_finish(&line) != 0ull) && (mmgr_line_finish(&writing_line) != 0ull);
}

// whether a byte is white as JSON has it
static int history_white(unsigned char byte)
{
    return (byte == ' ') || (byte == '\t') || (byte == '\r') || (byte == '\n');
}

// the answers the file at `file` stores, an array of them, its bytes mapped into `*bytes`, `*size` of them, and where
// its first item may start into `*first`: 1, or 0 where there is none or it contains no array, nothing then mapped
static int history_read(const char *file, const unsigned char **bytes, unsigned long long *size,
                        unsigned long long *first)
{
    *bytes = cli_file_read(file, size);
    VerbaLinea none = {NULL, 0ull, 0ull};
    unsigned long long at = 0ull;
    while ((*bytes != NULL) && (at < *size) && history_white((*bytes)[at]))
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

// the answers stored before, from `at` of `size` bytes, the first CLI_REQUESTS_HISTORY_MAX - 1 of them written into
// `piece`, each after a comma: 1, or 0 where one could not be
static int history_before(VerbaLinea *piece, const unsigned char *bytes, unsigned long long size, unsigned long long at)
{
    for (unsigned int entry = 1u; entry < CLI_REQUESTS_HISTORY_MAX; entry += 1u)
    {
        const unsigned long long end = cli_json_value_end((const char *)bytes, size, at);
        if (end == at)
        {
            return 1;
        }
        mmgr_line_ch(piece, ',');
        if (!cli_json_compacted((const char *)bytes + at, end - at, piece) || !mmgr_line_ok(piece))
        {
            return 0;
        }
        at = end;
        while ((at < size) && history_white(bytes[at]))
        {
            at += 1u;
        }
        at += ((at < size) && (bytes[at] == ',')) ? 1u : 0u;
    }
    return 1;
}

int cli_requests_history_save(const char *root, const char *path, const char *key, unsigned long long key_size,
                              const CliRequestsAnswer *answer, const CliRequestsSent *sent, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char writing[CLI_PATH_MAX_LEN];
    if (!history_folder(root, path, folder, err_msg))
    {
        return 0;
    }
    if (!history_file(folder, key, key_size, file, writing))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": its path is too long");
        return 0;
    }
    cli_lock_take(&s_history_lock);
    const int made = query_folder_made(folder);
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long first = 0ull;
    const int before = made && history_read(file, &bytes, &size, &first);
    // the answers written into a room of their own, each piece added to the file as the room fills
    char room[CLI_REQUESTS_ROOM];
    CliStateWriting target = {writing, 1};
    VerbaLinea piece = {room, sizeof(room), 0ull, history_appended, &target, 0ull};
    int done = made;
    if (done)
    {
        mmgr_line_ch(&piece, '[');
        cli_requests_answer_written(answer, sent, CLI_REQUESTS_HISTORY_BODY_MAX, &piece);
        done = mmgr_line_ok(&piece) && ((piece.flushed + piece.at) <= CLI_REQUESTS_PIECE) &&
               (!before || history_before(&piece, bytes, size, first));
    }
    if (before)
    {
        cli_file_let_go(bytes, size);
    }
    mmgr_line_ch(&piece, ']');
    done = done && mmgr_line_flush(&piece) && cli_moved(writing, file);
    cli_lock_give(&s_history_lock);
    if (!done)
    {
        mmgr_line_put(err_msg, made ? file : folder);
        mmgr_line_put(err_msg, ": it could not be written");
    }
    return done;
}

int cli_requests_answers(const char *root, const char *path, const char *text, unsigned long long size,
                         unsigned long long line, VerbaLinea *out, VerbaLinea *err_msg)
{
    char key[CLI_REQUESTS_ADDRESS + CLI_REQUESTS_NAME];
    VerbaLinea key_line = {key, sizeof(key), 0ull};
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char writing[CLI_PATH_MAX_LEN];
    if (!cli_requests_key_at(text, size, line, &key_line, err_msg) || !history_folder(root, path, folder, err_msg))
    {
        return 0;
    }
    if (!mmgr_line_ok(&key_line) || !history_file(folder, key, key_line.at, file, writing))
    {
        mmgr_line_put(err_msg, "the request is larger than orior_cli supports");
        return 0;
    }
    mmgr_line_put(out, "{\"key\":");
    cli_json_quoted(out, key, key_line.at);
    mmgr_line_put(out, ",\"answers\":");
    cli_lock_take(&s_history_lock);
    const unsigned char *bytes = NULL;
    unsigned long long byte_count = 0ull;
    unsigned long long first = 0ull;
    if (history_read(file, &bytes, &byte_count, &first))
    {
        (void)cli_json_compacted((const char *)bytes, byte_count, out);
        cli_file_let_go(bytes, byte_count);
    }
    else
    {
        mmgr_line_put(out, "[]");
    }
    cli_lock_give(&s_history_lock);
    mmgr_line_ch(out, '}');
    return 1;
}
