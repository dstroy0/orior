// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raised.c: writing a file the system refuses to write for the reader, once the system has asked the reader for the
// rights the write needs. The text is put in a file of orior's own first, and copied over the file with the rights:
// on Windows by orior's own program run again as `orior_cli write-raised`, raised by the system's prompt, which writes
// that one file and ends; on Linux by cp run through pkexec; on macOS by cp run with the system's authorization. A
// reader who declines the prompt leaves the file as it was
#include "raised.h"

// whether `said`, what a write that failed said, is the system refusing it the rights
int cli_raised_is_refused(const char *said)
{
    static const char *const s_parts[6] = {"access is denied", "permission denied", "operation not permitted",
                                           "os error 5)",      "os error 13)",      "os error 1)"};
    char lower[CLI_SAID_LONGEST];
    size_t length = 0u;
    for (; (said[length] != '\0') && (length < (sizeof(lower) - 1u)); length += 1u)
    {
        const char letter = said[length];
        lower[length] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter - 'A' + 'a') : letter;
    }
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

int cli_raised_copy_over(const char *from, const char *to, CliLine *said)
{
    if (!query_file_copied(from, to))
    {
        cli_line_text(said, to);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_raised_write(const char *path, const void *text, unsigned long long size, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    char staged[CLI_PATH_LONGEST];
    char base[CLI_PATH_LONGEST];
    if (!query_environment_read("TEMP", base, sizeof(base)) && !query_environment_read("TMPDIR", base, sizeof(base)))
    {
        memor.cpy(&(MemoriaCfg){.dst = base,
                                .src = "/tmp",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "/tmp", .cap = CLI_CELLS_POOL}) + 1u});
    }
    CliLine folder_line = {folder, sizeof(folder), 0ull};
    cli_line_text(&folder_line, base);
    cli_line_text(&folder_line, "/orior-raised");
    // the folder ended before the staged file's path is written from it
    const unsigned long long ended = cli_line_closed(&folder_line);
    CliLine staged_line = {staged, sizeof(staged), 0ull};
    cli_line_text(&staged_line, (ended != 0ull) ? folder : "");
    cli_line_character(&staged_line, '/');
    cli_line_decimal(&staged_line, cli_process_own(), 0u);
    cli_line_character(&staged_line, '-');
    cli_line_decimal(&staged_line, cli_clock_milliseconds(), 0u);
    if ((ended == 0ull) || (cli_line_closed(&staged_line) == 0ull) || !query_folder_made(folder) ||
        !query_file_written(staged, text, size))
    {
        cli_line_text(said, staged);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    const int done = cli_raised_platform(staged, path, said);
    query_removed(staged);
    return done;
}
