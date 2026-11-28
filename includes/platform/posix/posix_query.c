// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_query.c: the query's calls on POSIX: files and folders removed, copied, renamed and written; a file's time; a
// path made full; the terminal; and the engine's library of a link
#if !defined(_WIN32)
// folders, links, a file's time and utimensat are POSIX, outside strict C11, and realpath is an X/Open extension
// that glibc declares under _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#endif
#include "../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../ui/query/src/query_internal.h"
#include "../../../ui/query/src/query_loops.h"
#include "../../../ui/query/src/tools/tools_internal.h"

#if !defined(_WIN32)
#include "../../../ui/cli/src/text/text.h"
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// what a folder's removal walks: the folder and whether every entry went
typedef struct
{
    const char *folder;
    int whole;
} QueryRemoval;

static void query_entry_remove(const CliEachNameIsFolderCfg *args)
{
    QueryRemoval *const removal = (QueryRemoval *)args->context;
    char path[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, removal->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    removal->whole = (mmgr_line_finish(&line) != 0ULL) &&
                     EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path) &&
                     removal->whole;
}

int query_remove(const QueryPathSecondsCtx *args)
{
    struct stat st;
    if (lstat(args->path, &st) != 0)
    {
        return 1;
    }
    if (!S_ISDIR(st.st_mode))
    {
        return unlink(args->path) == 0;
    }
    QueryRemoval removal = {args->path, 1};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->path,
               .each = query_entry_remove, .context_bytess = &removal);
    return removal.whole && (rmdir(args->path) == 0);
}

int query_file_copy(const QueryFromCtx *args)
{
    const int source = open(args->from, O_RDONLY);
    if (source < 0)
    {
        return 0;
    }
    const int target = open(args->to, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    int whole = target >= 0;
    char piece[65536];
    ssize_t got = whole ? read(source, piece, sizeof(piece)) : 0;
    while (whole && (got > 0))
    {
        // a read that gave bytes gave a positive count, which a write puts whole or fails. They are the bytes of a
        // file, and the call makes no path of them
        whole = write(target, piece, (size_t)got) == got; // NOSONAR
        got = read(source, piece, sizeof(piece));
    }
    close(source);
    if (target >= 0)
    {
        close(target);
    }
    return whole && (got == 0);
}

int query_file_rename(const QueryFromCtx *args)
{
    // the file there let go, the name linked to the file moved, and its old name let go
    unlink(args->to);
    return (link(args->from, args->to) == 0) && (unlink(args->from) == 0);
}

int query_file_write(const QueryPathBytesSizeCtx *args)
{
    const int file = open(args->path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (file < 0)
    {
        return 0;
    }
    unsigned long long put = 0ULL;
    int whole = 1;
    while (whole && (put < args->size))
    {
        const ssize_t wrote = write(file, (const unsigned char *)args->bytes + put, (size_t)(args->size - put));
        whole = wrote > 0;
        // a write that put bytes put a positive count
        put += whole ? (unsigned long long)wrote : 0ULL;
    }
    return (close(file) == 0) && whole;
}

int query_is_file(const QueryPathSecondsCtx *args)
{
    struct stat st;
    return (stat(args->path, &st) == 0) && S_ISREG(st.st_mode);
}

int query_modify(const QueryPathSecondsCtx *args)
{
    unsigned long long *seconds = args->seconds;
    struct stat st;
    if (stat(args->path, &st) != 0)
    {
        return 0;
    }
    // a time a file changed is after 1970 here
    *seconds = (st.st_mtime > 0) ? (unsigned long long)st.st_mtime : 0ULL;
    return 1;
}

int query_modified_set(const QueryPathSecondsTypeCtx *args)
{
    struct timespec times[2];
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;
    // a time after 1970 that a time_t represents
    times[1].tv_sec = (time_t)args->seconds_value;
    times[1].tv_nsec = 0;
    return utimensat(AT_FDCWD, args->path, times, 0) == 0;
}

int query_path_full(const QueryCapacityPathCtx *args)
{
    char resolved[PATH_MAX];
    if (realpath(args->path, resolved) != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->full,
                                .src = resolved,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = resolved, .cap = CLI_POOL_BYTES}) + 1u});
        return args->full[0] != '\0';
    }
    // the call makes a path that is not there yet full against the folder that this process is in
    if (args->path[0] == '/')
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->full,
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
        return args->full[0] != '\0';
    }
    char here[PATH_MAX];
    if (getcwd(here, sizeof(here)) == NULL)
    {
        return 0;
    }
    VerbaLinea line = {args->full, args->capacity, 0ULL};
    mmgr_line_put(&line, here);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->path);
    return mmgr_line_finish(&line) != 0ULL;
}

int query_terminal_is(void)
{
    return isatty(0);
}

unsigned long long query_terminal_line_read(const QueryCapacityLineCtx *args)
{
    size_t length = 0u;
    unsigned long long whole = 0ULL;
    int read_any = 0;
    int going = 1;
    while (going)
    {
        char character = '\0';
        if (read(0, &character, 1u) != 1)
        {
            if (!read_any)
            {
                return QRY_MEMBER_UNREAD;
            }
            going = 0;
            continue;
        }
        read_any = 1;
        if (character == '\n')
        {
            going = 0;
            continue;
        }
        if (character != '\r')
        {
            whole += 1ULL;
            if ((length + 1u) < args->capacity)
            {
                args->line[length] = character;
                length += 1u;
            }
        }
    }
    args->line[length] = '\0';
    return whole;
}

void query_terminal_write(const QueryErrorsCtx *args)
{
    unsigned long long put = 0ULL;
    while (put < args->length)
    {
        const ssize_t wrote = write(args->errors ? 2 : 1, args->text + put, (size_t)(args->length - put));
        if (wrote <= 0)
        {
            return;
        }
        // a write that put bytes put a positive count
        put += (unsigned long long)wrote;
    }
}

// the words every program's link ends with: the threads and the loader, which the platform calls
const char *const g_query_mmgr_link[] = {"-lpthread", "-ldl", NULL};

void query_nvcc_host(const QueryNvccCtx *args)
{
    // nvcc takes the system's own host compiler
    (void)args;
}

unsigned int query_engine_objects(const QueryNvccCtx *args)
{
    // the engine is a library, and its link names no object of its own
    *args->text = NULL;
    return 0u;
}

// the engine's library linked in, which the program finds at run time in the engine's folder
void query_engine_link(const QueryNvccCtx *args)
{
    char library[QUERY_PATH_MAX_LEN + 2u];
    VerbaLinea library_line = {library, sizeof(library), 0ULL};
    mmgr_line_put(&library_line, "-L");
    mmgr_line_put(&library_line, args->engine);
    mmgr_line_finish(&library_line);
    static const char *const s_after[5] = {"-lcell_tracking_engine", "-Xlinker", "-rpath", "-Xlinker", NULL};
    EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = args->words,
               .text = library);
    for (unsigned int at = 0u; s_after[at] != NULL; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = args->words,
                   .word_text = s_after[at]);
    }
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = args->words,
               .word_text = args->engine);
}

#endif
