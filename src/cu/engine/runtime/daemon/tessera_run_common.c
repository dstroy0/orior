// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_run_common.c: tessera_run's clock, arguments, paths, limits, record and log
// the internal header comes first, since it sets the macros of the system before any header
#include "tessera_run_internal.h"

#include "../../../../../includes/platform/platform.h"
#include "../../engine_note.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if (defined(_WIN32))
_Alignas(8) static const char s_run_daemon[] = "tessera_daemon.exe";
#endif
#if !(defined(_WIN32))
_Alignas(8) static const char s_run_daemon[] = "tessera_daemon";
#endif

unsigned long long run_now(void)
{
    return platform_clock_ns() / RUN_THOUSAND;
}

void run_pause(unsigned long long microseconds)
{
    // each pause of tessera_run is whole milliseconds
    platform_paused(microseconds / RUN_THOUSAND);
}

unsigned long long run_self_pid(void)
{
    return platform_process_id();
}

int run_arguments(int count, char **arguments, RunRequest *request)
{
    int at = 1;
    while ((at < count) && (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at],
                                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--", .ci = EMBED_FALSE)))
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "--child", .ci = EMBED_FALSE))
        {
            request->child = 1;
            at += 1;
            continue;
        }
        const int valued = (at + 1) < count;
        const char *end = NULL;
        if (valued && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "--processors", .ci = EMBED_FALSE)))
        {
            request->processors =
                EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = arguments[at + 1], .end = &end, .base = 10);
            if ((end == arguments[at + 1]) || (*end != '\0'))
            {
                return 0;
            }
        }
        else if (valued && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at],
                                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--name", .ci = EMBED_FALSE)))
        {
            request->name = arguments[at + 1];
        }
        else if (valued && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at],
                                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "--parent", .ci = EMBED_FALSE)))
        {
            request->parent = arguments[at + 1];
        }
        else
        {
            return 0;
        }
        at += 2;
    }
    request->command = at + 1;
    // a job requests its processors, or is the child of a tessera_run that has the ticket: one or the other
    const int requests_processors = request->processors != 0ULL;
    const int is_child = request->parent != NULL;
    return (at < count) && (request->command < count) && (requests_processors != is_child) &&
           !(is_child && request->child);
}

// the own path of this program and its length, or 0 where the call cannot read it whole
size_t run_own_path(char *path, size_t capacity)
{
#if defined(_WIN32)
    // the capacity is a buffer size this file allocates, which a DWORD counts on every Windows target
    const size_t length = (size_t)GetModuleFileNameA(NULL, path, (DWORD)capacity);
#else
    const ssize_t read = readlink("/proc/self/exe", path, capacity - 1u);
    // a failed read is -1, which the call records as no length at all
    const size_t length = (read > 0) ? (size_t)read : 0u;
#endif
    if ((length == 0u) || (length >= capacity))
    {
        return 0u;
    }
    path[length] = '\0';
    return length;
}

int run_daemon_path(char *path, size_t capacity)
{
    // $TESSERA_DAEMON names the daemon; otherwise it is the tessera_daemon beside this program
    char named_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const named =
        (platform_environment("TESSERA_DAEMON", named_text, sizeof(named_text)) != 0ULL) ? named_text : NULL;
    if ((named != NULL) && (named[0] != '\0'))
    {
        VerbaLinea formed_line_1 = mmgr_line_over(path, capacity);
        mmgr_line_put(&formed_line_1, named);
        (void)mmgr_line_finish(&formed_line_1);
        const int written = (int)formed_line_1.at;
        // the call compares a length that is not negative whole against the capacity
        return (written > 0) && ((size_t)written < capacity);
    }
    size_t directory_length = run_own_path(path, capacity);
    while ((directory_length != 0u) && (path[directory_length - 1u] != '/') && (path[directory_length - 1u] != '\\'))
    {
        directory_length -= 1u;
    }
    if ((directory_length == 0u) || ((directory_length + sizeof(s_run_daemon)) > capacity))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = path + directory_length, .src = s_run_daemon,
               .bytes = sizeof(s_run_daemon));
    return 1;
}

int run_signum(const RunRequest *request, char **arguments, int count, EngineSignum *signum, EngineError *error)
{
    // the request is the name, then every word of the command, each ended by a zero byte
    const char *const name = (request->name != NULL) ? request->name : "";
    unsigned long long bytes =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1ULL;
    for (int at = request->command; at < count; at += 1)
    {
        bytes +=
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1ULL;
    }
    unsigned char *const text = (unsigned char *)platform_cell_taken((unsigned long long)((size_t)bytes),
                                                                     (unsigned int)platform_place_g_engine_owner, 0);
    if (text == NULL)
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = text, .src = name,
               .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    unsigned long long at_byte =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1ULL;
    for (int at = request->command; at < count; at += 1)
    {
        EMBED_CALL(
            memor.cpy, MemoriaCfg, .dst = text + at_byte, .src = arguments[at],
            .bytes =
                EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
        at_byte +=
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1ULL;
    }
    const ObsignatioSignumRequest hashed = {text, bytes, NULL, OBSIGNATIO_MODE_HASH, signum->bytes, ENGINE_SIGNUM_BYTES,
                                            error};
    const int ok = obsignatio_signum(&hashed) == 0L;
    platform_cell_released(text, (unsigned int)platform_place_g_engine_owner);
    return ok;
}

// a text and a suffix as one path, 0 where the capacity does not fit it
int run_path_joined(const char *first, const char *second, char *path, size_t capacity)
{
    VerbaLinea formed_line_2 = mmgr_line_over(path, capacity);
    mmgr_line_put(&formed_line_2, first);
    mmgr_line_put(&formed_line_2, second);
    (void)mmgr_line_finish(&formed_line_2);
    const int written = (int)formed_line_2.at;
    // the call compares a length that is not negative whole against the capacity
    return (written > 0) && ((size_t)written < capacity);
}

// a limit in milliseconds from the environment, in microseconds. The return is the fallback where the environment does
// not set it, or it is not a count
unsigned long long run_limit(const char *name, unsigned long long fallback)
{
    char text_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const text = (platform_environment(name, text_text, sizeof(text_text)) != 0ULL) ? text_text : NULL;
    const char *end = NULL;
    const unsigned long long milliseconds =
        (text != NULL) ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = text, .end = &end, .base = 10) : 0ULL;
    const int given = (text != NULL) && (end != text) && (*end == '\0') && (milliseconds != 0ULL);
    return given ? (milliseconds * RUN_THOUSAND) : fallback;
}

// a record's line: its first word, then each number after it, a word naming each number after the first. The count of
// numbers read, or -1 for a line that is not whole
static int run_record_fields(const char *line, char state[RUN_STATE_CAPACITY], unsigned long long numbers[RUN_NUMBERS])
{
    const size_t length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \n");
    if ((length == 0u) || (length >= RUN_STATE_CAPACITY))
    {
        return -1;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = state, .src = line, .bytes = length);
    state[length] = '\0';
    const char *walk = line + length;
    int count = 0;
    // Explicit cast reads the byte as its code point
    while ((count < RUN_NUMBERS) && (walk[0] == ' ') &&
           EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)walk[1]))
    {
        const char *end = NULL;
        numbers[count] = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = walk + 1, .end = &end, .base = 10);
        count += 1;
        // past the word that names the next number
        walk = (end[0] == ' ') ? (end + 1 +
                                  EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = end + 1,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \n"))
                               : end;
    }
    return (walk[0] == '\n') ? count : -1;
}

// the line of a record file as its fields. The return is -1 where the read of the file fails, or its line is not
// whole
int run_record_read(const char *path, char state[RUN_STATE_CAPACITY], unsigned long long numbers[RUN_NUMBERS])
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return -1;
    }
    char line[RUN_RECORD_CAPACITY];
    const int read = platform_file_text_read(line, sizeof(line), file) != NULL;
    platform_file_close(file);
    return read ? run_record_fields(line, state, numbers) : -1;
}

// The time now in UTC, as a log entry opens with it, in the form 2026-11-21T23:07:38Z. The date comes from the count
// of days since 1970 by the eras of the calendar. Each era is 400 years, 146097 days, from the start of a March. A
// year of an era starts in March, and so the leap day is the last day of its year.
static void run_stamp(char *text, size_t capacity)
{
    const unsigned long long seconds = platform_epoch_seconds();
    const unsigned long long of_day = seconds % 86400ULL;
    // the days from the start of March of the year 0 to the start of 1970 are 719468
    const unsigned long long days = (seconds / 86400ULL) + 719468ULL;
    const unsigned long long era = days / 146097ULL;
    const unsigned long long of_era = days - (era * 146097ULL);
    const unsigned long long year_of_era =
        (of_era - (of_era / 1460ULL) + (of_era / 36524ULL) - (of_era / 146096ULL)) / 365ULL;
    const unsigned long long of_year =
        of_era - ((365ULL * year_of_era) + (year_of_era / 4ULL) - (year_of_era / 100ULL));
    const unsigned long long from_march = ((5ULL * of_year) + 2ULL) / 153ULL;
    const unsigned long long day = of_year - (((153ULL * from_march) + 2ULL) / 5ULL) + 1ULL;
    const unsigned long long month = (from_march < 10ULL) ? (from_march + 3ULL) : (from_march - 9ULL);
    const unsigned long long year = (era * 400ULL) + year_of_era + ((month <= 2ULL) ? 1ULL : 0ULL);
    VerbaLinea line = mmgr_line_over(text, capacity);
    mmgr_line_decimal(&line, year, 4u);
    mmgr_line_ch(&line, '-');
    mmgr_line_decimal(&line, month, 2u);
    mmgr_line_ch(&line, '-');
    mmgr_line_decimal(&line, day, 2u);
    mmgr_line_ch(&line, 'T');
    mmgr_line_decimal(&line, of_day / 3600ULL, 2u);
    mmgr_line_ch(&line, ':');
    mmgr_line_decimal(&line, (of_day / 60ULL) % 60ULL, 2u);
    mmgr_line_ch(&line, ':');
    mmgr_line_decimal(&line, of_day % 60ULL, 2u);
    mmgr_line_ch(&line, 'Z');
    (void)mmgr_line_finish(&line);
}

// an entry in the child's log, which outlives both processes where the parent did not end first, and on stderr
void run_log(const char *records, const char *entry)
{
    char stamp[32];
    run_stamp(stamp, sizeof(stamp));
    char path[ENGINE_PATH_CAPACITY];
    PlatformFile *const file =
        run_path_joined(records, ".log", path, sizeof(path)) ? platform_file_open(path, "ab") : NULL;
    if (file != NULL)
    {
        {
            mmgr_line_put(platform_file_line(file), stamp);
            mmgr_line_put(platform_file_line(file), " ");
            mmgr_line_put(platform_file_line(file), entry);
            mmgr_line_put(platform_file_line(file), "\n");
        }
        platform_file_close(file);
    }
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  tessera_run: ");
        mmgr_line_put(&engine_note.line, entry);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
}
