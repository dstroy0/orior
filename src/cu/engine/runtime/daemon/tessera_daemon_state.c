// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_daemon_state.c: the daemon's lock, clocks, state file, history and tickets
#include "tessera_daemon_internal.h"
#include "../../../../platform/platform.h"
#include "../../engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

_Alignas(8) static const char s_daemon_history[] = "history";
_Alignas(8) static const char s_daemon_history_fresh[] = "history.fresh";
_Alignas(8) static const char s_daemon_ticket[] = "ticket";
_Alignas(8) static const char s_daemon_identity[] = "identity ";
_Alignas(8) static const char s_daemon_pid[] = "\npid ";
_Alignas(8) static const char s_daemon_declared[] = "\ndeclared ";
_Alignas(8) static const char s_daemon_peak[] = "\npeak ";
_Alignas(8) static const char s_daemon_measured[] = "\nmeasured ";
_Alignas(8) static const char s_daemon_grace[] = "\ngrace_microseconds ";
_Alignas(8) static const char s_daemon_sweep[] = "\nsweep_microseconds ";
_Alignas(8) static const char s_daemon_reason[] = "\nreason ";
_Alignas(8) static const char s_daemon_signum[] = "\nsignum ";
_Alignas(8) static const char s_daemon_seal[] = "seal ";
_Alignas(8) static const
    char s_daemon_history_bad[] = "  tessera daemon: the history did not read, or its seal did not verify: ";
_Alignas(8) static const
    char s_daemon_history_unsaved[] = "  tessera daemon: the history could not be sealed and saved in ";

TesseraDaemon g_daemon;
#if (defined(_WIN32))
static SRWLOCK s_daemon_lock = SRWLOCK_INIT;
CONDITION_VARIABLE g_daemon_changed = CONDITION_VARIABLE_INIT;
#endif
#if !(defined(_WIN32))
static pthread_mutex_t s_daemon_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_daemon_changed;
#endif

void daemon_lock(void)
{
#if defined(_WIN32)
    AcquireSRWLockExclusive(&s_daemon_lock);
#else
    pthread_mutex_lock(&s_daemon_lock);
#endif
}

void daemon_unlock(void)
{
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&s_daemon_lock);
#else
    pthread_mutex_unlock(&s_daemon_lock);
#endif
}

void daemon_signal_changed(void)
{
#if defined(_WIN32)
    WakeAllConditionVariable(&g_daemon_changed);
#else
    pthread_cond_broadcast(&g_daemon_changed);
#endif
}

unsigned long long daemon_now(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    // a performance counter and its frequency are positive
    const unsigned long long ticks = (unsigned long long)counter.QuadPart;
    // a performance counter and its frequency are positive
    const unsigned long long rate = (unsigned long long)frequency.QuadPart;
    return ((ticks / rate) * TESSERA_DAEMON_MILLION) + (((ticks % rate) * TESSERA_DAEMON_MILLION) / rate);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    // a monotonic clock's seconds and nanoseconds are not negative
    return ((unsigned long long)now.tv_sec * TESSERA_DAEMON_MILLION) + ((unsigned long long)now.tv_nsec / 1000ULL);
#endif
}

unsigned long long daemon_wall(void)
{
#if defined(_WIN32)
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    return ((((unsigned long long)now.dwHighDateTime) << 32u) | now.dwLowDateTime) / 10ULL;
#else
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    // a wall clock after the epoch has seconds and nanoseconds that are not negative
    return ((unsigned long long)now.tv_sec * TESSERA_DAEMON_MILLION) + ((unsigned long long)now.tv_nsec / 1000ULL);
#endif
}

void daemon_wait_until(unsigned long long when)
{
#if defined(_WIN32)
    DWORD milliseconds = INFINITE;
    if (when != TESSERA_DAEMON_FOREVER)
    {
        const unsigned long long now = daemon_now();
        const unsigned long long remaining = (when > now) ? (when - now) : 0ULL;
        const unsigned long long wait_milliseconds = (remaining + 999ULL) / 1000ULL;
        // a wait below INFINITE fits a DWORD
        milliseconds = (wait_milliseconds < (unsigned long long)INFINITE) ? (DWORD)wait_milliseconds : (INFINITE - 1u);
    }
    SleepConditionVariableSRW(&g_daemon_changed, &s_daemon_lock, milliseconds, 0u);
#else
    if (when == TESSERA_DAEMON_FOREVER)
    {
        pthread_cond_wait(&g_daemon_changed, &s_daemon_lock);
        return;
    }
    struct timespec until;
    // a monotonic time in seconds fits a time_t
    until.tv_sec = (time_t)(when / TESSERA_DAEMON_MILLION);
    // a remainder below a million microseconds is below a billion nanoseconds
    until.tv_nsec = (long)((when % TESSERA_DAEMON_MILLION) * 1000ULL);
    pthread_cond_timedwait(&g_daemon_changed, &s_daemon_lock, &until);
#endif
}

int daemon_directories_make(const char *path)
{
    char walk[ENGINE_PATH_CAPACITY];
    const size_t length = mmgr_text_length(path, PLATFORM_TEXT_TO_TERMINATOR);
    if (length >= sizeof(walk))
    {
        return 0;
    }
    mmgr_bytes_copy(walk, path, length + 1u);
    for (size_t at = 1u; at <= length; at += 1u)
    {
        const int separator = (walk[at] == '/') || (walk[at] == '\\') || (walk[at] == '\0');
        if (!separator || (walk[at - 1u] == ':'))
        {
            continue;
        }
        const char saved_char = walk[at];
        walk[at] = '\0';
#if defined(_WIN32)
        const int made = (_mkdir(walk) == 0) || (errno == EEXIST);
#else
        const int made = (mkdir(walk, 0700) == 0) || (errno == EEXIST);
#endif
        walk[at] = saved_char;
        if (!made)
        {
            return 0;
        }
    }
    return 1;
}

int daemon_state_file(const char *name, char *path)
{
    VerbaLinea formed_line = mmgr_line_over(path, ENGINE_PATH_CAPACITY);
    mmgr_line_put(&formed_line, g_daemon.state);
#if defined(_WIN32)
    mmgr_line_ch(&formed_line, '\\');
#else
    mmgr_line_ch(&formed_line, '/');
#endif
    mmgr_line_put(&formed_line, name);
    (void)mmgr_line_finish(&formed_line);
    // the line's count, or its capacity once a write did not fit
    const int written = (int)formed_line.at;
    // a non-negative length is compared whole against the capacity
    return (written >= 0) && ((unsigned int)written < ENGINE_PATH_CAPACITY);
}

static void daemon_put_long(unsigned char *bytes, unsigned long long value)
{
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        // one byte of the long, taken from the bottom
        bytes[byte] = (unsigned char)((value >> (8u * byte)) & 0xFFULL);
    }
}

static unsigned long long daemon_get_long(const unsigned char *bytes)
{
    unsigned long long value = 0ULL;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        value |= (unsigned long long)bytes[byte] << (8u * byte);
    }
    return value;
}

int daemon_history_load(void)
{
    char path[ENGINE_PATH_CAPACITY];
    if (!daemon_state_file(s_daemon_history, path))
    {
        return 0;
    }
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 1;
    }
    // the file is its whole records, then the seal over them; any other length is a broken history
    int ok = platform_file_seek(file, 0L, SEEK_END) == 0;
    const long length = ok ? platform_file_tell(file) : -1L;
    ok = ok && (length >= (long)OBSIGNATIO_SIGNUM_BYTES) &&
         ((((unsigned long long)length - OBSIGNATIO_SIGNUM_BYTES) % TESSERA_HISTORY_RECORD) == 0ULL) &&
         (platform_file_seek(file, 0L, SEEK_SET) == 0);
    // a length checked non-negative above is the file's byte count
    const unsigned long long total = ok ? (unsigned long long)length : 0ULL;
    unsigned char *const bytes =
        ok ? (unsigned char *)platform_cell_taken((size_t)total, platform_place_g_program_owner, 0) : NULL;
    ok = ok && (bytes != NULL) && (platform_file_read_items(bytes, 1u, (size_t)total, file) == (size_t)total);
    platform_file_close(file);
    const unsigned long long sealed = total - OBSIGNATIO_SIGNUM_BYTES;
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    const ObsignatioSealRequest seal = {bytes, sealed, (bytes != NULL) ? bytes + sealed : NULL, &error};
    ok = ok && (obsignatio_seal_verify(&seal) == 1L);
    for (unsigned long long at = 0ULL; ok && (at < sealed); at += TESSERA_HISTORY_RECORD)
    {
        TesseraHistory entry;
        mmgr_bytes_copy(entry.signum.bytes, bytes + at, ENGINE_SIGNUM_BYTES);
        entry.peak = daemon_get_long(bytes + at + ENGINE_SIGNUM_BYTES);
        entry.duration = daemon_get_long(bytes + at + ENGINE_SIGNUM_BYTES + 8u);
        ok = tessera_ledger_remember(&g_daemon.ledger, &entry);
    }
    platform_cell_released(bytes, platform_place_g_program_owner);
    if (!ok)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, s_daemon_history_bad);
        mmgr_line_put(&engine_note.line, path);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return ok;
}

int daemon_history_save(void)
{
    char path[ENGINE_PATH_CAPACITY];
    char fresh[ENGINE_PATH_CAPACITY];
    if (!daemon_state_file(s_daemon_history, path) || !daemon_state_file(s_daemon_history_fresh, fresh))
    {
        return 0;
    }
    // every record, then the seal over them all; a history that is not saved is reported, never dropped silently
    const unsigned long long sealed = g_daemon.ledger.history_count * TESSERA_HISTORY_RECORD;
    const unsigned long long total = sealed + OBSIGNATIO_SIGNUM_BYTES;
    unsigned char *const bytes = (unsigned char *)platform_cell_taken((size_t)total, platform_place_g_program_owner, 0);
    if (bytes == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, s_daemon_history_unsaved);
            mmgr_line_put(&engine_note.line, path);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    for (unsigned long long at = 0ULL; at < g_daemon.ledger.history_count; at += 1ULL)
    {
        const TesseraHistory *const entry = &g_daemon.ledger.history[at];
        unsigned char *const record = bytes + (at * TESSERA_HISTORY_RECORD);
        mmgr_bytes_copy(record, entry->signum.bytes, ENGINE_SIGNUM_BYTES);
        daemon_put_long(record + ENGINE_SIGNUM_BYTES, entry->peak);
        daemon_put_long(record + ENGINE_SIGNUM_BYTES + 8u, entry->duration);
    }
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    const ObsignatioSealRequest seal = {bytes, sealed, bytes + sealed, &error};
    int ok = obsignatio_seal(&seal) == 0L;
    PlatformFile *const file = ok ? platform_file_open(fresh, "wb") : NULL;
    ok = ok && (file != NULL) && (platform_file_write_items(bytes, 1u, (size_t)total, file) == (size_t)total);
    ok = (file != NULL) && (platform_file_close(file) == 0) && ok;
    platform_cell_released(bytes, platform_place_g_program_owner);
#if defined(_WIN32)
    ok = ok && (MoveFileExA(fresh, path, MOVEFILE_REPLACE_EXISTING) != 0);
#else
    ok = ok && platform_file_moved(fresh, path);
#endif
    if (!ok)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, s_daemon_history_unsaved);
        mmgr_line_put(&engine_note.line, path);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return ok;
}

static int daemon_ticket_path(unsigned long long identity, const TesseraPeer *peer, int make, char *path)
{
    char folder[ENGINE_PATH_CAPACITY];
    if (!tessera_path_lost(g_daemon.device, identity, &peer->signum, folder, ENGINE_PATH_CAPACITY) ||
        (make && !daemon_directories_make(folder)))
    {
        return 0;
    }
    VerbaLinea formed_line_1 = mmgr_line_over(path, ENGINE_PATH_CAPACITY);
    mmgr_line_put(&formed_line_1, folder);
    mmgr_line_ch(&formed_line_1, (char)(TESSERA_DAEMON_SEPARATOR));
    mmgr_line_put(&formed_line_1, s_daemon_ticket);
    (void)mmgr_line_finish(&formed_line_1);
    const int written = (int)formed_line_1.at;
    // a non-negative length is compared whole against the capacity
    return (written >= 0) && ((unsigned int)written < ENGINE_PATH_CAPACITY);
}

static int daemon_ticket_seal_write(const char *path, const char *text, unsigned long long length)
{
    // the ticket's text, then a last line sealing every byte above it
    static const char s_hex[] = "0123456789abcdef";
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    const ObsignatioSealRequest seal = {(const unsigned char *)text, length, signum, &error};
    if (obsignatio_seal(&seal) != 0L)
    {
        return 0;
    }
    char line[(2u * OBSIGNATIO_SIGNUM_BYTES) + 1u];
    for (unsigned int byte = 0u; byte < OBSIGNATIO_SIGNUM_BYTES; byte += 1u)
    {
        line[2u * byte] = s_hex[signum[byte] >> 4u];
        line[(2u * byte) + 1u] = s_hex[signum[byte] & 0x0Fu];
    }
    line[2u * OBSIGNATIO_SIGNUM_BYTES] = '\0';
    PlatformFile *const file = platform_file_open(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    unsigned long long formed_mark_1;
    const int printed =
        (platform_file_write_items(text, 1u, (size_t)length, file) == (size_t)length) &&
        ((formed_mark_1 = platform_file_mark(file), mmgr_line_put(platform_file_line(file), s_daemon_seal),
          mmgr_line_put(platform_file_line(file), line), mmgr_line_put(platform_file_line(file), "\n"),
          platform_file_written_since(file, formed_mark_1)) > 0);
    return (platform_file_close(file) == 0) && printed;
}

int daemon_ticket_write(unsigned long long identity, const TesseraPeer *peer, unsigned long long peak,
                        unsigned long long measured, const char *reason)
{
    char path[ENGINE_PATH_CAPACITY];
    char text[TESSERA_TICKET_CAPACITY];
    if (!daemon_ticket_path(identity, peer, 1, path))
    {
        return 0;
    }
    // the ticket names its job's signum whole: a lost job is found again by its request
    char signum[(2u * ENGINE_SIGNUM_BYTES) + 1u];
    for (unsigned int byte = 0u; byte < ENGINE_SIGNUM_BYTES; byte += 1u)
    {
        VerbaLinea buffer_line = mmgr_line_over(signum + (2u * byte), 3u);
        mmgr_line_hex(&buffer_line, peer->signum.bytes[byte], 2u);
        (void)mmgr_line_finish(&buffer_line);
    }
    VerbaLinea formed_line_2 = mmgr_line_over(text, sizeof(text));
    mmgr_line_put(&formed_line_2, s_daemon_identity);
    mmgr_line_hex(&formed_line_2, identity, 16u);
    mmgr_line_put(&formed_line_2, s_daemon_signum);
    mmgr_line_put(&formed_line_2, signum);
    mmgr_line_put(&formed_line_2, s_daemon_pid);
    mmgr_line_decimal(&formed_line_2, peer->pid, 1u);
    mmgr_line_put(&formed_line_2, s_daemon_declared);
    mmgr_line_decimal(&formed_line_2, peer->declared, 1u);
    mmgr_line_put(&formed_line_2, s_daemon_peak);
    mmgr_line_decimal(&formed_line_2, peak, 1u);
    mmgr_line_put(&formed_line_2, s_daemon_measured);
    mmgr_line_decimal(&formed_line_2, measured, 1u);
    mmgr_line_put(&formed_line_2, s_daemon_grace);
    mmgr_line_decimal(&formed_line_2, peer->grace_microseconds, 1u);
    mmgr_line_put(&formed_line_2, s_daemon_sweep);
    mmgr_line_decimal(&formed_line_2, peer->sweep_microseconds, 1u);
    mmgr_line_put(&formed_line_2, s_daemon_reason);
    mmgr_line_put(&formed_line_2, reason);
    mmgr_line_put(&formed_line_2, "\n");
    (void)mmgr_line_finish(&formed_line_2);
    const int written = (int)formed_line_2.at;
    // a non-negative length is compared whole against the capacity
    return (written >= 0) && ((unsigned int)written < TESSERA_TICKET_CAPACITY) &&
           daemon_ticket_seal_write(path, text, (unsigned long long)written);
}

int daemon_ticket_note(unsigned long long identity, const TesseraPeer *peer, const char *note)
{
    // the note goes in only on a ticket whose seal verifies, and the ticket is sealed again with it
    char path[ENGINE_PATH_CAPACITY];
    char text[TESSERA_TICKET_CAPACITY];
    if (!daemon_ticket_path(identity, peer, 0, path))
    {
        return 0;
    }
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    const size_t length = platform_file_read_items(text, 1u, sizeof(text) - 1u, file);
    const int at_end = platform_file_ended(file) != 0;
    platform_file_close(file);
    const size_t seal_line = (sizeof(s_daemon_seal) - 1u) + (2u * OBSIGNATIO_SIGNUM_BYTES) + 1u;
    if (!at_end || (length < seal_line) || (text[length - 1u] != '\n') ||
        (mmgr_bytes_compare(text + (length - seal_line), s_daemon_seal, sizeof(s_daemon_seal) - 1u) != 0))
    {
        return 0;
    }
    const size_t body = length - seal_line;
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    for (unsigned int byte = 0u; byte < OBSIGNATIO_SIGNUM_BYTES; byte += 1u)
    {
        const char *const pair = text + body + (sizeof(s_daemon_seal) - 1u) + (2u * byte);
        unsigned int value = 0u;
        for (unsigned int digit = 0u; digit < 2u; digit += 1u)
        {
            const char glyph = pair[digit];
            const int decimal = (glyph >= '0') && (glyph <= '9');
            const int character = (glyph >= 'a') && (glyph <= 'f');
            if (!decimal && !character)
            {
                return 0;
            }
            // a checked hex digit is one nibble
            value = (value << 4u) | (unsigned int)(decimal ? (glyph - '0') : (glyph - 'a' + 10));
        }
        // two nibbles are one byte
        signum[byte] = (unsigned char)value;
    }
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    const ObsignatioSealRequest seal = {(const unsigned char *)text, body, signum, &error};
    // the note is a whole line, its newline included
    const size_t added = mmgr_text_length(note, PLATFORM_TEXT_TO_TERMINATOR);
    if ((obsignatio_seal_verify(&seal) != 1L) || ((body + added) >= sizeof(text)))
    {
        return 0;
    }
    mmgr_bytes_copy(text + body, note, added);
    return daemon_ticket_seal_write(path, text, body + added);
}
