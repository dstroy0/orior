// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lsp_files.c: the files a language server has open, each by its URI with its version and the text it was last given,
// that text kept in a file of orior's own in the system's folder for passing files, so a file's whole text is held on
// the disk and not in memory
#include "lsp_internal.h"

// the most files every server has open at once, the places the table of them is begun with, doubling as files are
// opened, and the longest URI of one
#define CLI_LSP_FILES 2048u
#define CLI_LSP_FILES_FIRST 16u
#define CLI_LSP_URI 2048u

CliCellsTally g_cli_lsp_files_cells = CLI_CELLS_TALLY("cli_lsp_files");

// A file a server has open: whether its place is taken, the server, its URI in a cell of its length, and its version
typedef struct
{
    int taken;
    unsigned int server;
    char *uri;
    unsigned long long version;
} CliLspFile;

// the files open, by place, in a cell whose room doubles as places are wanted; a place keeps its number while its file
// is open, the file its text is kept in named by it
static CliLspFile *s_files;
static unsigned int s_files_room;
static CliLock s_files_lock = CLI_LOCK_INIT;

// the file the text of the open file at `place` is kept in, into `path`, which holds CLI_PATH_LONGEST, its folder made
static void files_kept_path(unsigned int place, char *path)
{
    char temporary[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char name[64];
    CliLine name_line = {name, sizeof(name), 0ull};
    cli_line_text(&name_line, "orior-lsp-");
    cli_line_decimal(&name_line, cli_process_own(), 1u);
    cli_line_closed(&name_line);
    cli_temp_folder(temporary);
    cli_full(temporary, name, folder);
    (void)query_folder_made(folder);
    name_line.at = 0ull;
    cli_line_decimal(&name_line, place, 1u);
    cli_line_text(&name_line, ".txt");
    cli_line_closed(&name_line);
    cli_full(folder, name, path);
}

// the place of the open file of `uri` the server has, under the files' lock: CLI_LSP_FILES where it has none
static unsigned int files_place(const CliLsp *lsp, const char *uri)
{
    for (unsigned int at = 0u; at < s_files_room; at += 1u)
    {
        if (s_files[at].taken && (s_files[at].server == lsp->number) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_files[at].uri, .other = uri, .cap = CLI_CELLS_POOL})))
        {
            return at;
        }
    }
    return CLI_LSP_FILES;
}

// the table's room doubled, under the files' lock, its new places free: 1, or 0 where it holds the most or the larger
// cell is refused
static int files_grown(void)
{
    const unsigned int doubled = (s_files_room == 0u) ? CLI_LSP_FILES_FIRST : (s_files_room * 2u);
    const unsigned int room = (doubled < CLI_LSP_FILES) ? doubled : CLI_LSP_FILES;
    CliLspFile *const larger =
        (s_files_room < CLI_LSP_FILES)
            ? cli_cell_taken_by(&g_cli_lsp_files_cells, (unsigned long long)room * sizeof(CliLspFile), "the files open")
            : NULL;
    if (larger == NULL)
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = larger, .val = 0, .bytes = (size_t)room * sizeof(CliLspFile)});
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = s_files, .bytes = (size_t)s_files_room * sizeof(CliLspFile)});
    cli_cell_given_by(&g_cli_lsp_files_cells, s_files);
    s_files = larger;
    s_files_room = room;
    return 1;
}

// the place at `at` let go, under the files' lock, the file its text was kept in removed and its URI's cell given back
static void files_let_go(unsigned int at)
{
    char kept[CLI_PATH_LONGEST];
    files_kept_path(at, kept);
    (void)query_removed(kept);
    cli_cell_given_by(&g_cli_lsp_files_cells, s_files[at].uri);
    s_files[at].uri = NULL;
    s_files[at].taken = 0;
}

// the place of the open file of `uri` the server has, taken for it where it has none, its version 1: CLI_LSP_FILES
// where no place is left, the URI is too long or its cell is refused
static unsigned int files_place_taken(const CliLsp *lsp, const char *uri)
{
    unsigned int place = files_place(lsp, uri);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL});
    if ((place < CLI_LSP_FILES) || (length >= CLI_LSP_URI))
    {
        return place;
    }
    unsigned int vacant = 0u;
    while ((vacant < s_files_room) && s_files[vacant].taken)
    {
        vacant += 1u;
    }
    char *const kept = ((vacant < s_files_room) || files_grown())
                           ? cli_cell_taken_by(&g_cli_lsp_files_cells, length + 1ull, "an open file's URI")
                           : NULL;
    if (kept == NULL)
    {
        return CLI_LSP_FILES;
    }
    memor.cpy(&(MemoriaCfg){.dst = kept, .src = uri, .bytes = length + 1u});
    s_files[vacant] = (CliLspFile){1, lsp->number, kept, 1ull};
    return vacant;
}

// the text of an open file kept: 1, or 0 with why said
static int files_kept(unsigned int place, const char *text, unsigned long long length, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    files_kept_path(place, path);
    if (!query_file_written(path, text, length))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_lsp_opened(CliLsp *lsp, const char *path, const char *language_id, const char *text, unsigned long long length,
                   CliLine *said)
{
    char uri[CLI_LSP_URI];
    cli_lsp_uri_of(path, uri, sizeof(uri));
    cli_lock_take(&s_files_lock);
    const unsigned int place = files_place_taken(lsp, uri);
    if (place < CLI_LSP_FILES)
    {
        s_files[place].version = 1ull;
    }
    const int kept = (place < CLI_LSP_FILES) && files_kept(place, text, length, said);
    cli_lock_give(&s_files_lock);
    if (place == CLI_LSP_FILES)
    {
        cli_line_text(said, "no more files are held open at once");
        return 0;
    }
    if (!kept)
    {
        return 0;
    }
    const CliLspPart parts[7] = {
        CLI_LSP_RAW("{\"textDocument\":{\"uri\":\""),
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\",\"languageId\":\""),
        {language_id, cellul.len(&(CatenaFinitaCfg){.src = language_id, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\",\"version\":1,\"text\":\""),
        {text, length, 1},
        CLI_LSP_RAW("\"}}")};
    const CliLspPart head = CLI_LSP_RAW("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":");
    const CliLspPart tail = CLI_LSP_RAW("}");
    CliLspPart message[9];
    message[0] = head;
    memor.cpy(&(MemoriaCfg){.dst = message + 1, .src = parts, .bytes = sizeof(parts)});
    message[8] = tail;
    return cli_lsp_send_parts(&lsp->input, &lsp->input_lock, message, 9u, said);
}

int cli_lsp_changed(CliLsp *lsp, const char *path, const char *text, unsigned long long length, CliLine *said)
{
    char uri[CLI_LSP_URI];
    cli_lsp_uri_of(path, uri, sizeof(uri));
    cli_lock_take(&s_files_lock);
    const unsigned int place = files_place_taken(lsp, uri);
    unsigned long long version = 0ull;
    if (place < CLI_LSP_FILES)
    {
        s_files[place].version += 1ull;
        version = s_files[place].version;
    }
    const int kept = (place < CLI_LSP_FILES) && files_kept(place, text, length, said);
    cli_lock_give(&s_files_lock);
    if (place == CLI_LSP_FILES)
    {
        cli_line_text(said, "no more files are held open at once");
        return 0;
    }
    if (!kept)
    {
        return 0;
    }
    char number[24];
    CliLine number_line = {number, sizeof(number), 0ull};
    cli_line_decimal(&number_line, version, 1u);
    const CliLspPart message[7] = {
        CLI_LSP_RAW(
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\""),
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\",\"version\":"),
        {number, number_line.at, 0},
        CLI_LSP_RAW("},\"contentChanges\":[{\"text\":\""),
        {text, length, 1},
        CLI_LSP_RAW("\"}]}}")};
    return cli_lsp_send_parts(&lsp->input, &lsp->input_lock, message, 7u, said);
}

int cli_lsp_text(CliLsp *lsp, const char *path, CliLine *out)
{
    char uri[CLI_LSP_URI];
    cli_lsp_uri_of(path, uri, sizeof(uri));
    cli_lock_take(&s_files_lock);
    const unsigned int place = files_place(lsp, uri);
    int read = 0;
    if (place < CLI_LSP_FILES)
    {
        char kept[CLI_PATH_LONGEST];
        files_kept_path(place, kept);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_file_held(kept, &size);
        cli_line_counted(out, (bytes != NULL) ? (const char *)bytes : "", (bytes != NULL) ? size : 0ull);
        if (bytes != NULL)
        {
            cli_file_let_go(bytes, size);
        }
        read = 1;
    }
    cli_lock_give(&s_files_lock);
    return read;
}

int cli_lsp_read_again(CliLsp *lsp, const char *path, CliLine *said)
{
    char uri[CLI_LSP_URI];
    cli_lsp_uri_of(path, uri, sizeof(uri));
    cli_lock_take(&s_files_lock);
    const unsigned int place = files_place(lsp, uri);
    char kept[CLI_PATH_LONGEST];
    if (place < CLI_LSP_FILES)
    {
        files_kept_path(place, kept);
    }
    cli_lock_give(&s_files_lock);
    if (place == CLI_LSP_FILES)
    {
        return 1;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(kept, &size);
    const int changed =
        cli_lsp_changed(lsp, path, (bytes != NULL) ? (const char *)bytes : "", (bytes != NULL) ? size : 0ull, said);
    if (bytes != NULL)
    {
        cli_file_let_go(bytes, size);
    }
    return changed;
}

int cli_lsp_closed(CliLsp *lsp, const char *path, CliLine *said)
{
    char uri[CLI_LSP_URI];
    cli_lsp_uri_of(path, uri, sizeof(uri));
    cli_lock_take(&s_files_lock);
    const unsigned int place = files_place(lsp, uri);
    if (place < CLI_LSP_FILES)
    {
        files_let_go(place);
    }
    cli_lock_give(&s_files_lock);
    const CliLspPart message[3] = {
        CLI_LSP_RAW(
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{\"textDocument\":{\"uri\":\""),
        {uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), 1},
        CLI_LSP_RAW("\"}}}")};
    return cli_lsp_send_parts(&lsp->input, &lsp->input_lock, message, 3u, said);
}

int cli_lsp_has(CliLsp *lsp, const char *path)
{
    char uri[CLI_LSP_URI];
    cli_lsp_uri_of(path, uri, sizeof(uri));
    cli_lock_take(&s_files_lock);
    const int has = (files_place(lsp, uri) < CLI_LSP_FILES);
    cli_lock_give(&s_files_lock);
    return has;
}

void cli_lsp_held_each(CliLsp *lsp,
                       void (*each)(void *context, const char *path, const char *text, unsigned long long length),
                       void *context)
{
    // the server's files are copied as they stand under the lock into cells of their count and their URIs' bytes, and
    // each is handed on with the lock given back, so that `each` may open it in another server
    cli_lock_take(&s_files_lock);
    unsigned int count = 0u;
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < s_files_room; at += 1u)
    {
        if (s_files[at].taken && (s_files[at].server == lsp->number))
        {
            count += 1u;
            bytes += cellul.len(&(CatenaFinitaCfg){.src = s_files[at].uri, .cap = CLI_LSP_URI}) + 1ull;
        }
    }
    unsigned int *const places = cli_cell_taken_by(
        &g_cli_lsp_files_cells, ((unsigned long long)count + 1ull) * sizeof(unsigned int), "the places of files held");
    char *const uris = cli_cell_taken_by(&g_cli_lsp_files_cells, bytes + 1ull, "the URIs of files held");
    count = ((places != NULL) && (uris != NULL)) ? count : 0u;
    unsigned int copied = 0u;
    unsigned long long used = 0ull;
    for (unsigned int at = 0u; (copied < count) && (at < s_files_room); at += 1u)
    {
        if (s_files[at].taken && (s_files[at].server == lsp->number))
        {
            const unsigned long long length =
                cellul.len(&(CatenaFinitaCfg){.src = s_files[at].uri, .cap = CLI_LSP_URI}) + 1ull;
            places[copied] = at;
            memor.cpy(&(MemoriaCfg){.dst = uris + used, .src = s_files[at].uri, .bytes = (size_t)length});
            used += length;
            copied += 1u;
        }
    }
    cli_lock_give(&s_files_lock);
    const char *uri = uris;
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_LSP_URI});
        const char *const this_uri = uri;
        uri += length + 1ull;
        char path[CLI_PATH_LONGEST];
        if (!cli_lsp_path_of(this_uri, length, path))
        {
            continue;
        }
        char kept[CLI_PATH_LONGEST];
        files_kept_path(places[one], kept);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_file_held(kept, &size);
        each(context, path, (bytes != NULL) ? (const char *)bytes : "", (bytes != NULL) ? size : 0ull);
        if (bytes != NULL)
        {
            cli_file_let_go(bytes, size);
        }
    }
    cli_cell_given_by(&g_cli_lsp_files_cells, uris);
    cli_cell_given_by(&g_cli_lsp_files_cells, places);
}

void cli_lsp_files_let_go(const CliLsp *lsp)
{
    cli_lock_take(&s_files_lock);
    for (unsigned int at = 0u; at < s_files_room; at += 1u)
    {
        if (s_files[at].taken && (s_files[at].server == lsp->number))
        {
            files_let_go(at);
        }
    }
    cli_lock_give(&s_files_lock);
}
