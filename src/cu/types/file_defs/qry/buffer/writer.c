// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// writer.c: the writer: the oracles put in, the write until stopped, the stop, and the table's texts a row names
#include "qry_buffer_internal.h"

// the oracle named `name`, its path from the top of the tree `top`, put into the run where the run holds no blob of
// the name: read from the tree and handed at the buffer's tail under the lock. 1 where it was put in, 0 where the run
// holds one already or the tree holds none
static int qry_oracle_put(QryBuffer *buffer, const char *top, const char *name)
{
    const unsigned char *held = NULL;
    unsigned long long held_size = 0ull;
    char path[1400];
    const int written = snprintf(path, sizeof(path), "%s/%s", top, name);
    // a path that does not fit its room names no file
    if (qry_latest(buffer, name, &held, &held_size, NULL) || (written <= 0) || ((size_t)written >= sizeof(path)))
    {
        return 0;
    }
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_file_read(NULL, path, &bytes, &size))
    {
        return 0;
    }
    qry_lock(buffer);
    int placed = 0;
    qry_appended(buffer, name, strlen(name), bytes, size, QRY_ORDINARY, 0u, buffer->head->capacity, 0ull, &placed);
    qry_unlock(buffer);
    free(bytes);
    return placed;
}

// The oracle of every table the run holds, put into the run as it ends where it holds none of the oracle's name
// (qry_oracle_put): each name the run holds read once, by its first blob; a name of no folder with an oracle under
// QRY_ORACLES a table of the run's own, and a name whose first blob is the head row of a k-file (krep_records.h: a
// key of 0, a change of 0, then the kind's four bytes, the low first, its letters and numbers past zeros, the version,
// and the name's length within the row) a k-file's table, whose oracles are QRY_KFILE_ROWS and its kind's. The count
// put in
static unsigned long long qry_oracles_added(QryBuffer *buffer)
{
    char top[1024];
    if (!qry_top(top, sizeof(top)))
    {
        return 0ull;
    }
    // the hashes of the names read, a power of two slots each a hash or 0, grown to stay at most half full
    size_t slots = 1024u;
    size_t count = 0u;
    unsigned long long *seen = (unsigned long long *)calloc(slots, sizeof(unsigned long long));
    unsigned long long added = 0ull;
    const unsigned long long tail = atomic_load(&buffer->head->tail);
    QryBlob blob;
    for (unsigned long long walk = 0ull;
         (seen != NULL) && (walk < tail) && qry_blob_read(buffer->data, tail, walk, &blob); walk = blob.next)
    {
        const char *const name = (const char *)(buffer->data + blob.name_at);
        // a name's hash is never 0, the mark of a slot no name holds
        const unsigned long long hash = qry_hash(name, (size_t)blob.name_length);
        size_t probe = (size_t)hash & (slots - 1u);
        while ((seen[probe] != 0ull) && (seen[probe] != hash))
        {
            probe = (probe + 1u) & (slots - 1u);
        }
        if (seen[probe] == hash)
        {
            continue;
        }
        seen[probe] = hash;
        count += 1u;
        // an oracle's name that does not fit the longest name a blob takes names no oracle
        char oracle[QRY_NAME_LONGEST + 1u];
        int named = snprintf(oracle, sizeof(oracle), "%s/%.*s.oracle.tsv", QRY_ORACLES, (int)blob.name_length, name);
        if ((memchr(name, '/', (size_t)blob.name_length) == NULL) && (named > 0) && ((size_t)named < sizeof(oracle)))
        {
            added += (unsigned long long)qry_oracle_put(buffer, top, oracle);
        }
        unsigned long long words[7] = {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull};
        if (blob.size >= sizeof(words))
        {
            memcpy(words, buffer->data + blob.bytes_at, sizeof(words));
        }
        char kind[5] = {0, 0, 0, 0, 0};
        int lettered = (blob.size >= sizeof(words)) && (words[0] == 0ull) && (words[3] == 0ull) &&
                       (words[6] <= (blob.size - sizeof(words)));
        for (unsigned int at = 0u; lettered && (at < 4u); at += 1u)
        {
            // the kind's four bytes stand the low first in its word
            const char letter = (char)((words[4] >> (8u * at)) & 0xffull);
            lettered = isalnum((unsigned char)letter) || ((letter == '\0') && (at != 0u));
            kind[at] = (char)tolower((unsigned char)letter);
        }
        named = snprintf(oracle, sizeof(oracle), "%s/%s/%s.oracle.tsv", QRY_KFILE_ORACLES, kind, kind);
        if (lettered && (named > 0) && ((size_t)named < sizeof(oracle)))
        {
            added += (unsigned long long)qry_oracle_put(buffer, top, QRY_KFILE_ROWS);
            added += (unsigned long long)qry_oracle_put(buffer, top, oracle);
        }
        if ((count * 2u) >= slots)
        {
            unsigned long long *const larger = (unsigned long long *)calloc(slots * 2u, sizeof(unsigned long long));
            for (size_t at = 0u; (larger != NULL) && (at < slots); at += 1u)
            {
                size_t place = (size_t)seen[at] & ((slots * 2u) - 1u);
                while ((seen[at] != 0ull) && (larger[place] != 0ull))
                {
                    place = (place + 1u) & ((slots * 2u) - 1u);
                }
                larger[place] = seen[at];
            }
            free(seen);
            seen = larger;
            slots *= 2u;
        }
    }
    free(seen);
    return added;
}

int qry_write_until_stopped(QryBuffer *buffer)
{
    QryHead *const head = buffer->head;
    QryFile file;
    QryWriterState *const state = (QryWriterState *)calloc(1u, sizeof(QryWriterState));
    if ((state == NULL) || !qry_file_opened(&file, buffer->path))
    {
        free(state);
        atomic_store(&head->writing, 0u);
        return 0;
    }
    unsigned long long last = qry_milliseconds();
    int stopped = 0;
    int moved = 0;
    while (!stopped)
    {
        // a pass that moved a member's ring is followed by another at once: what a member added while it was taken is
        // taken on its next turn
        const unsigned long long waited = qry_milliseconds() - last;
        if (!moved)
        {
            qry_wait(buffer, (waited < QRY_WRITE_MILLISECONDS) ? (QRY_WRITE_MILLISECONDS - waited) : 1ull);
        }
        const int stopping = (atomic_load(&head->stopping) != 0u);
        // a dump no blob after it came to say, said by the writer once the run stops
        if (stopping)
        {
            qry_lock(buffer);
            qry_dumped_said(buffer);
            qry_unlock(buffer);
        }
        // panics and critical blobs first, every time the writer wakes
        if (!qry_firsts_written(buffer, &file, state))
        {
            // more were handed than the buffer keeps places for: the queue written in order through the tail read with
            // them and flushed through, every marked one written where its marks say, and every one of them handed by
            // then is on disk with it
            state->spans_seen = state->spans_read;
            const unsigned long long from = atomic_load(&head->written);
            qry_range_written(buffer, &file, from, state->tail_seen, state);
            state->writes.failed |= !qry_file_durable(&file);
            state->writes.failed |= !qry_marks_written(buffer, from, state->tail_seen);
            if (state->handed_read > atomic_load(&head->critical_written))
            {
                atomic_store(&head->critical_written, state->handed_read);
            }
            last = qry_milliseconds();
        }
        // then each member in its turn
        moved = qry_members_moved(buffer);
        const unsigned long long tail = state->tail_seen;
        const unsigned long long written = atomic_load(&head->written);
        const int due = stopping || ((tail - written) >= QRY_WRITE_BYTES) ||
                        ((qry_milliseconds() - last) >= QRY_WRITE_MILLISECONDS) ||
                        (state->early_count >= QRY_CRITICAL_SPANS);
        if ((tail > written) && due)
        {
            qry_range_written(buffer, &file, written, tail, state);
            last = qry_milliseconds();
        }
        stopped = stopping && !moved && qry_members_empty(buffer) &&
                  (atomic_load(&head->tail) == atomic_load(&head->written)) &&
                  (atomic_load(&head->critical_written) >= atomic_load(&head->critical_handed)) &&
                  (head->dumped_blobs == 0ull);
    }
    // the oracle of every table the run holds, once every member has moved its last; then the writer's own account, the
    // run's last blob, written and flushed through to the disk
    const unsigned long long oracles = qry_oracles_added(buffer);
    char account[384];
    const QryWrites *const writes = &state->writes;
    const int length = snprintf(account, sizeof(account),
                                "%llu blobs handed, %llu dumped, %llu bytes in %llu writes, the slowest %.3f ms for "
                                "%llu bytes, %.3f ms writing in all; %llu panics, the slowest %.3f ms from its hand to "
                                "the disk%s\n",
                                head->sequence, head->dumped_all, writes->bytes, writes->writes, writes->slowest,
                                writes->slowest_bytes, writes->all, writes->panics, writes->panic_slowest,
                                writes->failed ? "; a write did not reach the file whole" : "");
    qry_lock(buffer);
    int placed = 0;
    qry_appended(buffer, "writer", strlen("writer"), account, (unsigned long long)length, QRY_ORDINARY, 0u,
                 head->capacity, 0ull, &placed);
    qry_unlock(buffer);
    qry_range_written(buffer, &file, atomic_load(&head->written), atomic_load(&head->tail), state);
    const int closed = qry_file_closed(&file);
    const int failed = state->writes.failed;
    free(state);
    // the run dumped with its table at its front, in place of the .qry written as it ran
    const unsigned long long began = qry_nanoseconds();
    unsigned long long rows = 0ull;
    const int tabled = closed && qry_table_dumped(buffer, &rows);
    // then every current file the run holds in its place, before the writer says it has stopped: a script that stops
    // its run and reads a file the run wrote reads it as the run left it
    const unsigned long long put_began = qry_nanoseconds();
    unsigned long long files = 0ull;
    const int put = qry_files_put(buffer, atomic_load(&head->tail), &files);
    atomic_store(&head->writing, 0u);
    printf("  %s: %s", buffer->path, account);
    if (oracles != 0ull)
    {
        printf("  %s: the oracles of its tables, %llu, put in from the tree\n", buffer->path, oracles);
    }
    if (tabled)
    {
        printf("  %s: dumped with its table of %llu rows in %.3f ms\n", buffer->path, rows,
               (double)(put_began - began) / 1000000.0);
    }
    else
    {
        printf("  %s: not dumped with its table; it stands as written as the run ran\n", buffer->path);
    }
    if (files != 0ull)
    {
        printf("  %s: %llu files written in their places in %.3f ms%s\n", buffer->path, files,
               (double)(qry_nanoseconds() - put_began) / 1000000.0, put ? "" : "; a file was not written whole");
    }
    return closed && !failed && put;
}

int qry_stop(QryBuffer *buffer, unsigned int milliseconds)
{
    atomic_store(&buffer->head->stopping, 1u);
    qry_wake(buffer);
    const unsigned long long began = qry_milliseconds();
    while (atomic_load(&buffer->head->writing) != 0u)
    {
        if ((qry_milliseconds() - began) > milliseconds)
        {
            return 0;
        }
        qry_sleep_millisecond();
    }
    return 1;
}

// the numbers of the texts this process has handed, a power of two slots each a number or 0, and the count held; one
// thread at a time numbers a text
static unsigned long long *s_texts = NULL;

static size_t s_text_slots = 0u;
static size_t s_text_count = 0u;
static atomic_flag s_texts_held = ATOMIC_FLAG_INIT;

int qry_row_handed_by(QryBuffer *buffer, const char *table, const unsigned long long *columns, unsigned int count,
                      unsigned int priority, QryWho who)
{
    if ((buffer == NULL) || (count > QRY_ROW_COLUMNS))
    {
        return 0;
    }
    unsigned long long row[QRY_ROW_OPENING + QRY_ROW_COLUMNS];
    qry_who_words(who, row);
    if (count != 0u)
    {
        memcpy(row + QRY_ROW_OPENING, columns, (size_t)count * sizeof(unsigned long long));
    }
    return qry_hand(buffer, table, row, (unsigned long long)(QRY_ROW_OPENING + count) * sizeof(unsigned long long),
                    priority) != 0ull;
}

// the 64-bit FNV-1a hash of `length` bytes at `bytes`, a hash of 0 taken as 1: 0 names no text
static unsigned long long qry_text_hash(const char *bytes, unsigned long long length)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        hash = (hash ^ (unsigned char)bytes[at]) * 0x100000001b3ull;
    }
    return (hash != 0ull) ? hash : 1ull;
}

// the slot of `number` in the texts this process has handed: the one holding it, or the empty one it would go in
static size_t qry_text_slot(unsigned long long number)
{
    size_t at = (size_t)(number & (s_text_slots - 1u));
    while ((s_texts[at] != 0ull) && (s_texts[at] != number))
    {
        at = (at + 1u) & (s_text_slots - 1u);
    }
    return at;
}

// the texts this process has handed given room for one more within half their slots: 1, or 0 where there was no memory
static int qry_texts_roomed(void)
{
    if ((s_text_slots != 0u) && ((2u * (s_text_count + 1u)) <= s_text_slots))
    {
        return 1;
    }
    const size_t grown = (s_text_slots == 0u) ? 1024u : (2u * s_text_slots);
    unsigned long long *const larger = (unsigned long long *)calloc(grown, sizeof(unsigned long long));
    if (larger == NULL)
    {
        return 0;
    }
    unsigned long long *const before = s_texts;
    const size_t before_slots = s_text_slots;
    s_texts = larger;
    s_text_slots = grown;
    for (size_t at = 0u; at < before_slots; at += 1u)
    {
        if (before[at] != 0ull)
        {
            s_texts[qry_text_slot(before[at])] = before[at];
        }
    }
    free(before);
    return 1;
}

unsigned long long qry_row_text(QryBuffer *buffer, const char *bytes, unsigned long long length)
{
    if ((buffer == NULL) || (length == 0ull))
    {
        return 0ull;
    }
    const unsigned long long number = qry_text_hash(bytes, length);
    while (atomic_flag_test_and_set(&s_texts_held))
    {
    }
    unsigned long long numbered = 0ull;
    if (qry_texts_roomed())
    {
        const size_t slot = qry_text_slot(number);
        numbered = number;
        if (s_texts[slot] == 0ull)
        {
            // the text's row: of no who, its number, its length and its bytes filled out to a whole word with zeros
            static const unsigned char s_zeros[8] = {0u};
            const unsigned long long opening[QRY_ROW_OPENING + 2u] = {0ull, 0ull, number, length};
            QryText row = {0};
            qry_text_added(&row, opening, sizeof(opening));
            qry_text_added(&row, bytes, length);
            qry_text_added(&row, s_zeros, (8ull - (length % 8ull)) % 8ull);
            const int handed =
                !row.failed && (qry_hand(buffer, QRY_TEXTS_TABLE, row.bytes, row.size, QRY_ORDINARY) != 0ull);
            qry_text_freed(&row);
            if (handed)
            {
                s_texts[slot] = number;
                s_text_count += 1u;
            }
            numbered = handed ? number : 0ull;
        }
    }
    atomic_flag_clear(&s_texts_held);
    return numbered;
}
