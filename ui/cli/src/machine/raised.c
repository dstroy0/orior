// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raised.c: writing a file the system refuses to write for the reader, once the system has prompted the reader for the
// rights the write needs. The text is put in a file of orior's own first, and copied over the file with the rights:
// on Windows by orior's own program run again as `orior_cli write-raised`, raised by the system's prompt, which writes
// that one file and ends; on Linux by cp run through pkexec; on macOS by cp run with the system's authorization. A
// reader who declines the prompt leaves the file as it was
#include "raised.h"
#include "../text/text.h"

// whether `err_text`, what a write that failed reported, is the system refusing it the rights
int cli_raised_is_refused(const char *err_text)
{
    static const char *const s_parts[6] = {"access is denied", "permission denied", "operation not permitted",
                                           "os error 5)",      "os error 13)",      "os error 1)"};
    char lower[CLI_ERR_MSG_MAX_LEN];
    size_t length = 0u;
    length = (size_t)cli_text_lowered_into(lower, sizeof(lower), err_text);
    lower[length] = '\0';
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (cellul.find(&(CatenaFinitaCfg){
                .src = lower, .cap = CLI_CELLS_POOL, .other = s_parts[at], .other_cap = CLI_CELLS_POOL}) != NULL)
        {
            return 1;
        }
    }
    return cellul.find(&(CatenaFinitaCfg){
               .src = lower, .cap = CLI_CELLS_POOL, .other = "read-only file system", .other_cap = CLI_CELLS_POOL}) !=
           NULL;
}

int cli_raised_copy_over(const char *from, const char *to, VerbaLinea *err_msg)
{
    if (!query_file_copied(from, to))
    {
        mmgr_line_put(err_msg, to);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_raised_write(const char *path, const void *text, unsigned long long size, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    char staged[CLI_PATH_MAX_LEN];
    char base[CLI_PATH_MAX_LEN];
    if (!query_environment_read("TEMP", base, sizeof(base)) && !query_environment_read("TMPDIR", base, sizeof(base)))
    {
        memor.cpy(&(MemoriaCfg){.dst = base,
                                .src = "/tmp",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "/tmp", .cap = CLI_CELLS_POOL}) + 1u});
    }
    VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
    mmgr_line_put(&folder_line, base);
    mmgr_line_put(&folder_line, "/orior-raised");
    // the folder ended before the staged file's path is written from it
    const unsigned long long ended = mmgr_line_finish(&folder_line);
    VerbaLinea staged_line = {staged, sizeof(staged), 0ull};
    mmgr_line_put(&staged_line, (ended != 0ull) ? folder : "");
    mmgr_line_ch(&staged_line, '/');
    mmgr_line_decimal(&staged_line, cli_process_own(), 0u);
    mmgr_line_ch(&staged_line, '-');
    mmgr_line_decimal(&staged_line, cli_clock_milliseconds(), 0u);
    if ((ended == 0ull) || (mmgr_line_finish(&staged_line) == 0ull) || !query_folder_made(folder) ||
        !query_file_written(staged, text, size))
    {
        mmgr_line_put(err_msg, staged);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    const int done = cli_raised_platform(staged, path, err_msg);
    query_removed(staged);
    return done;
}
