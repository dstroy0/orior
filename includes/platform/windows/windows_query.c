// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_query.c: the query's calls on Windows: files and folders removed, copied, renamed and written; a file's time;
// a path made full; the terminal; and the host compiler and the engine's objects of a link
#include "../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../ui/cli/src/text/text.h"
#include "../../../ui/query/src/query_internal.h"
#include "../../../ui/query/src/query_loops.h"
#include "../../../ui/query/src/tools/tools_internal.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

// the hundreds of nanoseconds from 1601 to 1970, which the system's file times count from
#define QUERY_EPOCH_TICKS 116444736000000000ULL

#define QUERY_TICKS_A_SECOND 10000000ULL

typedef struct
{
#if (defined(_WIN32))
    FILETIME time;
#endif
} WindowsCtx;

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
    mmgr_line_ch(&line, '\\');
    mmgr_line_put(&line, args->name);
    removal->whole = (mmgr_line_finish(&line) != 0ULL) &&
                     EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = path) &&
                     removal->whole;
}

int query_remove(const QueryPathSecondsCtx *args)
{
    const DWORD attributes = GetFileAttributesA(args->path);
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return 1;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u)
    {
        // the call removes a file the system has as read-only, as git has its objects, as any other
        if ((attributes & FILE_ATTRIBUTE_READONLY) != 0u)
        {
            (void)SetFileAttributesA(args->path, attributes & ~(DWORD)FILE_ATTRIBUTE_READONLY);
        }
        return DeleteFileA(args->path) != 0;
    }
    QueryRemoval removal = {args->path, 1};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->path,
               .each = query_entry_remove, .context_bytess = &removal);
    return removal.whole && (RemoveDirectoryA(args->path) != 0);
}

int query_file_copy(const QueryFromCtx *args)
{
    return CopyFileA(args->from, args->to, FALSE) != 0;
}

int query_file_rename(const QueryFromCtx *args)
{
    return MoveFileExA(args->from, args->to, MOVEFILE_REPLACE_EXISTING) != 0;
}

int query_file_write(const QueryPathBytesSizeCtx *args)
{
    const HANDLE file = CreateFileA(args->path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    unsigned long long put = 0ULL;
    int whole = 1;
    while (whole && (put < args->size))
    {
        // a piece is at max a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((args->size - put) < (1ULL << 30u)) ? (args->size - put) : (1ULL << 30u));
        DWORD wrote = 0u;
        whole = WriteFile(file, (const unsigned char *)args->bytes + put, piece, &wrote, NULL) && (wrote == piece);
        put += wrote;
    }
    return CloseHandle(file) && whole;
}

int query_is_file(const QueryPathSecondsCtx *args)
{
    const DWORD attributes = GetFileAttributesA(args->path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u);
}

// a file time as seconds since 1970, 0 for one before it
EMBED_INLINE unsigned long long query_seconds_get(const WindowsCtx *args)
{
    const unsigned long long ticks = ((unsigned long long)args->time.dwHighDateTime << 32u) | args->time.dwLowDateTime;
    return (ticks > QUERY_EPOCH_TICKS) ? ((ticks - QUERY_EPOCH_TICKS) / QUERY_TICKS_A_SECOND) : 0ULL;
}

int query_modify(const QueryPathSecondsCtx *args)
{
    unsigned long long *seconds = args->seconds;
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(args->path, GetFileExInfoStandard, &data))
    {
        return 0;
    }
    *seconds = EMBED_CALL(query_seconds_get, WindowsCtx, .time = data.ftLastWriteTime);
    return 1;
}

int query_modified_set(const QueryPathSecondsTypeCtx *args)
{
    const HANDLE file = CreateFileA(args->path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    const unsigned long long ticks = (args->seconds_value * QUERY_TICKS_A_SECOND) + QUERY_EPOCH_TICKS;
    FILETIME time;
    // a FILETIME is the ticks' two halves
    time.dwLowDateTime = (DWORD)(ticks & 0xffffffffULL);
    time.dwHighDateTime = (DWORD)(ticks >> 32u);
    const int set = SetFileTime(file, NULL, NULL, &time) != 0;
    CloseHandle(file);
    return set;
}

int query_path_full(const QueryCapacityPathCtx *args)
{
    // a capacity is far below what a DWORD counts
    const DWORD length = GetFullPathNameA(args->path, (DWORD)args->capacity, args->full, NULL);
    if ((length == 0u) || (length >= args->capacity))
    {
        return 0;
    }
    if (cellul.starts(&(CatenaFinitaCfg){.src = args->full, .other = "\\\\?\\", .cap = CLI_POOL_BYTES}))
    {
        memor.move_down(
            &(MemoriaCfg){.dst = args->full,
                          .src = args->full + 4,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->full + 4, .cap = CLI_POOL_BYTES}) + 1u});
    }
    return 1;
}

int query_terminal_is(void)
{
    return GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_CHAR;
}

unsigned long long query_terminal_line_read(const QueryCapacityLineCtx *args)
{
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    size_t length = 0u;
    unsigned long long whole = 0ULL;
    int read_any = 0;
    for (;;)
    {
        char character = '\0';
        DWORD got = 0u;
        if (!ReadFile(input, &character, 1u, &got, NULL) || (got == 0u))
        {
            if (!read_any)
            {
                return QRY_MEMBER_UNREAD;
            }
            break;
        }
        read_any = 1;
        if (character == '\n')
        {
            break;
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
    const HANDLE output = GetStdHandle(args->errors ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    unsigned long long put = 0ULL;
    while (put < args->length)
    {
        // a piece is at max a gibibyte, which a DWORD counts
        const DWORD piece = (DWORD)(((args->length - put) < (1ULL << 30u)) ? (args->length - put) : (1ULL << 30u));
        DWORD wrote = 0u;
        if (!WriteFile(output, args->text + put, piece, &wrote, NULL) || (wrote == 0u))
        {
            return;
        }
        put += wrote;
    }
}

// the words every program's link ends with: the library the address wait is in
const char *const g_query_mmgr_link[] = {"-lsynchronization", NULL};

void query_nvcc_host(const QueryNvccCtx *args)
{
    char cl[QUERY_PATH_MAX_LEN];
    char ignored_text[QUERY_LINE_MAX_LEN];
    VerbaLinea ignored = {ignored_text, sizeof(ignored_text), 0ULL};
    if (!EMBED_CALL(args->programs->program, QueryEachRolePathCapacityErrMsgCfg, .context = args->programs->context,
                    .role = QUERY_ROLE_CL, .path = cl, .capacity = sizeof(cl), .err_msg = &ignored))
    {
        return;
    }
    // the folder cl is in, nvcc's host compiler. The byte found is in `cl`, which this call owns and writes
    char *const slash = (char *)EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = cl,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = cl, .cap = CLI_POOL_BYTES}), .byte = '\\');
    if (slash == NULL)
    {
        return;
    }
    *slash = '\0';
    EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = args->words,
               .word_text = "-ccbin");
    EMBED_CALL(query_loops_words_words_text.words_text, QueryLoopsWordsWordsTextCfg, .words_words_pointer = args->words,
               .text = cl);
    if (args->msvc)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = args->words,
                   .word_text = "-Xcompiler");
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = args->words,
                   .word_text = "/std:c11 /O2 /Zc:preprocessor /D_CRT_SECURE_NO_WARNINGS");
    }
}

// the engine's objects, listed one a line in cell_tracking_engine.objects
unsigned int query_engine_objects(const QueryNvccCtx *args)
{
    unsigned int objects = 0u;
    char list_entries[QUERY_PATH_MAX_LEN];
    VerbaLinea listing_line = {list_entries, sizeof(list_entries), 0ULL};
    mmgr_line_put(&listing_line, args->engine);
    mmgr_line_put(&listing_line, "/cell_tracking_engine.objects");
    mmgr_line_finish(&listing_line);
    unsigned long long listed_size = 0ULL;
    const unsigned char *const list = qry_member_file_mapped(list_entries, &listed_size);
    char *const text =
        (list != NULL) ? g_platform_guard->persistent_buf_alloc(listed_size + 1ULL, g_query_owner) : NULL;
    *args->text = text;
    if (text != NULL)
    {
        // a size_t counts a file that the system maps whole
        memor.cpy(&(MemoriaCfg){.dst = text, .src = list, .bytes = (size_t)listed_size});
        text[listed_size] = '\0';
        char *line = text;
        while ((*line != '\0') && (objects < QUERY_WORDS_MAX))
        {
            char *const end_of = line + EMBED_CALL(cli_text_set_table.upto, CliTextSetCfg, .text = line, .set = "\r\n");
            const char saved_char = *end_of;
            *end_of = '\0';
            if (*line != '\0')
            {
                args->objects[objects] = line;
                objects += 1u;
            }
            line = end_of + ((saved_char != '\0') ? 1 : 0);
            line += EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = line, .set = "\r\n");
        }
    }
    if (list != NULL)
    {
        qry_member_file_unmapped(list, listed_size);
    }
    return objects;
}

// the engine's objects linked in
void query_engine_link(const QueryNvccCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(query_loops_words_words.words_add, QueryLoopsWordsWordsCfg, .words_words_pointer = args->words,
                   .word_text = args->objects[at]);
    }
}

#endif
