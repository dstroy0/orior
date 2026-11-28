// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// tessera_history_test.c: the history of the system's daemon, saved into the scratch root named on the command line
// and read back into a fresh ledger. The file is whole records and a seal. A history damaged by one byte, cut short by
// one byte, or with no seal does not read. Restored, it reads, and each tracked peak is there. The test runs the
// daemon's own save and read, and starts no daemon: the system has one, which other jobs hold.
#include "../../../../../../../src/cu/engine/runtime/daemon/tessera_daemon_internal.h"
#include "../../../../../../../includes/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// the tracked peaks that the test saves
#define HISTORY_TEST_ENTRIES 3u

static unsigned long long s_history_checks;
static unsigned long long s_history_failed;

static void history_check(int passed, const char *what)
{
    s_history_checks += 1ULL;
    s_history_failed += passed ? 0ULL : 1ULL;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, passed ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the whole file at `path` into a cell, its length into `length`, or NULL where it does not read
static unsigned char *history_file_read(const char *path, unsigned long long *length)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    int ok = platform_file_seek(file, 0L, PLATFORM_FILE_FROM_END) == 0;
    const long size = ok ? platform_file_tell(file) : -1L;
    ok = ok && (size > 0L) && (platform_file_seek(file, 0L, PLATFORM_FILE_FROM_START) == 0);
    // a length checked positive above is the file's byte count
    *length = ok ? (unsigned long long)size : 0ULL;
    unsigned char *const bytes =
        ok ? (unsigned char *)platform_cell_taken((size_t)*length, platform_place_g_program_owner, 0) : NULL;
    ok = ok && (bytes != NULL) && (platform_file_read_items(bytes, 1u, (size_t)*length, file) == (size_t)*length);
    platform_file_close(file);
    if (!ok)
    {
        platform_cell_released(bytes, platform_place_g_program_owner);
        return NULL;
    }
    return bytes;
}

// `length` bytes of `bytes` written whole over the file at `path`: 1, or 0 where the write fails
static int history_file_write(const char *path, const unsigned char *bytes, unsigned long long length)
{
    PlatformFile *const file = platform_file_open(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    const int written = platform_file_write_items(bytes, 1u, (size_t)length, file) == (size_t)length;
    return (platform_file_close(file) == 0) && written;
}

// the daemon's ledger emptied, and the history at the root read into it again: what the read answers
static int history_read_fresh(void)
{
    tessera_ledger_close(&g_daemon.ledger);
    return tessera_ledger_open(&g_daemon.ledger) && daemon_history_load();
}

int program_main(int count, char **arguments)
{
    if (count != 2)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  tessera history test: the scratch root is the one argument");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return 2;
    }
    VerbaLinea root = mmgr_line_over(g_daemon.root, sizeof(g_daemon.root));
    mmgr_line_put(&root, arguments[1]);
    const int rooted = (mmgr_line_finish(&root) != 0ULL) && platform_folders_made(g_daemon.root, 0700u);
    history_check(rooted, "the scratch root is made");
    char history[ENGINE_PATH_CAPACITY];
    const int named = rooted && daemon_state_file("history", history);
    TesseraHistory saved[HISTORY_TEST_ENTRIES];
    int remembered = named && tessera_ledger_open(&g_daemon.ledger);
    for (unsigned int entry = 0u; entry < HISTORY_TEST_ENTRIES; entry += 1u)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &saved[entry], .bytes = sizeof(saved[entry]), .val = 0);
        for (unsigned int at = 0u; at < ENGINE_SIGNUM_BYTES; at += 1u)
        {
            // a byte of each signum of its own: its place and its entry, below 256
            saved[entry].signum.bytes[at] = (unsigned char)((at * 7u) + (entry * 31u) + 1u);
        }
        saved[entry].peak = 4096ULL << entry;
        saved[entry].duration = 1000000ULL + entry;
        remembered = remembered && tessera_ledger_remember(&g_daemon.ledger, &saved[entry]);
    }
    history_check(remembered && daemon_history_save(), "the history saves under the root");
    unsigned long long length = 0ULL;
    unsigned char *const sealed = named ? history_file_read(history, &length) : NULL;
    history_check((sealed != NULL) && (length > OBSIGNATIO_SIGNUM_BYTES) &&
                      (((length - OBSIGNATIO_SIGNUM_BYTES) % TESSERA_HISTORY_RECORD) == 0ULL),
                  "the history on disk is whole records and a seal");
    int whole = history_read_fresh() && (g_daemon.ledger.history_count == HISTORY_TEST_ENTRIES);
    for (unsigned int entry = 0u; whole && (entry < HISTORY_TEST_ENTRIES); entry += 1u)
    {
        const TesseraHistory *const read = tessera_ledger_history(&g_daemon.ledger, &saved[entry].signum);
        whole = (read != NULL) && (read->peak == saved[entry].peak) && (read->duration == saved[entry].duration);
    }
    history_check(whole, "read back, each tracked peak and duration is the one saved");
    if (sealed != NULL)
    {
        unsigned char *const damaged =
            (unsigned char *)platform_cell_taken((size_t)length, platform_place_g_program_owner, 0);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = damaged, .src = sealed, .bytes = (size_t)length);
        // one byte of the first record's peak
        damaged[ENGINE_SIGNUM_BYTES] ^= 0x01u;
        history_check(history_file_write(history, damaged, length) && !history_read_fresh(),
                      "a history damaged by one byte does not read");
        history_check(history_file_write(history, sealed, length - 1ULL) && !history_read_fresh(),
                      "a history cut short by one byte does not read");
        history_check(history_file_write(history, sealed, length - OBSIGNATIO_SIGNUM_BYTES) && !history_read_fresh(),
                      "a history with no seal does not read");
        platform_cell_released(damaged, platform_place_g_program_owner);
        const TesseraHistory *restored = NULL;
        if (history_file_write(history, sealed, length) && history_read_fresh())
        {
            restored = tessera_ledger_history(&g_daemon.ledger, &saved[HISTORY_TEST_ENTRIES - 1u].signum);
        }
        history_check((restored != NULL) && (restored->peak == saved[HISTORY_TEST_ENTRIES - 1u].peak),
                      "restored, it reads, and the peak it tracked is there");
        platform_cell_released(sealed, platform_place_g_program_owner);
    }
    tessera_ledger_close(&g_daemon.ledger);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  tessera history test: ");
    mmgr_line_decimal(output_line, s_history_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, s_history_failed, 1u);
    mmgr_line_put(output_line, " failed");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (s_history_failed == 0ULL) ? 0 : 1;
}
