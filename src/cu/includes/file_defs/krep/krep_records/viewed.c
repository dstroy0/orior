// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// viewed.c: the file as a view: its rows viewed from a run's table, the rows gathered from a finished .qry, a text's
// number and text, and the who at a row
#include "../../../parsers/who.h"
#include "../../qry/qry_read.h"
#include "krep_internal.h"

// the file the rows among `bytes`, `size` of them, of the key `key` put together into `view`, which stands open and
// empty of its kind: the rows joined, each whole words, its opening, the numbers its change gives, then its bytes or
// its words. 1, or 0 where a row does not fit the file it puts together or the bytes are not whole rows
static int krep_rows_viewed(const unsigned char *bytes, unsigned long long size, unsigned long long key, KrepFile *view)
{
    const unsigned long long words = size / sizeof(unsigned long long);
    unsigned long long at = 0ull;
    int held = ((size % sizeof(unsigned long long)) == 0ull);
    while (held && ((at + KREP_ROW_OPENING) <= words))
    {
        unsigned long long opening[KREP_ROW_OPENING];
        memcpy(opening, bytes + (at * sizeof(unsigned long long)), sizeof(opening));
        at += KREP_ROW_OPENING;
        const unsigned long long change = opening[3];
        const unsigned int given = ((change == KREP_ROW_HEAD) || (change == KREP_ROW_PUT))   ? 3u
                                   : ((change == KREP_ROW_TEXT) || (change == KREP_ROW_SET)) ? 2u
                                   : (change == KREP_ROW_DROP)                               ? 1u
                                                                                             : 0u;
        if ((given == 0u) || ((at + given) > words))
        {
            held = 0;
            break;
        }
        unsigned long long number[3] = {0ull, 0ull, 0ull};
        memcpy(number, bytes + (at * sizeof(unsigned long long)), given * sizeof(unsigned long long));
        at += given;
        // a head's and a text's bytes, filled out to whole words, and a put's and a set's words
        const unsigned long long following = ((change == KREP_ROW_HEAD) || (change == KREP_ROW_TEXT))
                                                 ? ((number[given - 1u] + 7ull) / 8ull)
                                             : (change == KREP_ROW_PUT) ? number[2]
                                             : (change == KREP_ROW_SET) ? number[1]
                                                                        : 0ull;
        if (following > (words - at))
        {
            held = 0;
            break;
        }
        const unsigned char *const after = bytes + (at * sizeof(unsigned long long));
        at += following;
        // a head's key is 0 and is no file's; a row of another file's key is passed over
        if (opening[0] != key)
        {
            continue;
        }
        // a place, a code and a count read here are each at most what an unsigned int holds, as held to it
        const int counted =
            (number[0] <= 0xffffffffull) && (number[1] <= 0xffffffffull) && (number[2] <= 0xffffffffull);
        switch (change)
        {
        case KREP_ROW_TEXT:
            held = (number[0] == (view->texts + 1ull)) &&
                   (krep_text_taken(view, (const char *)after, number[1]) == number[0]);
            break;
        case KREP_ROW_PUT:
            held = counted && (number[0] <= view->records) &&
                   krep_record_put(view, (unsigned int)number[0], (unsigned int)number[1],
                                   (const unsigned long long *)(const void *)after, (unsigned int)number[2]);
            break;
        case KREP_ROW_SET:
            held = counted && (number[0] < view->records) &&
                   krep_record_set(view, (unsigned int)number[0], (const unsigned long long *)(const void *)after,
                                   (unsigned int)number[1]);
            break;
        case KREP_ROW_DROP:
            held = (number[0] < view->records);
            if (held)
            {
                krep_record_drop(view, (unsigned int)number[0]);
            }
            break;
        default:
            break;
        }
    }
    return held && (at == words);
}

int krep_viewed(const KrepFile *file, KrepFile *view)
{
    krep_open(view, file->kind);
    const size_t named = strlen(file->table);
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    int held = (file->key != 0ull);
    while (held && qry_member_read(&at, &record))
    {
        if ((record->name_length == named) && (memcmp(QRY_RECORD_NAME(record), file->table, named) == 0))
        {
            held = krep_rows_viewed(QRY_RECORD_BLOB(record), record->blob_length, file->key, view);
        }
    }
    if (!held)
    {
        krep_open(view, file->kind);
    }
    return held;
}

// 1 where `record` is a row of `table`
static int krep_row_of(const QryRecord *record, const char *table)
{
    const size_t named = strlen(table);
    return (record->name_length == named) && (memcmp(QRY_RECORD_NAME(record), table, named) == 0);
}

// The head row `record` of `table` read: 1 where it heads the file `name`, or any file where `name` is NULL, its
// kind into `kind`, which holds five
static int krep_head_of(const QryRecord *record, const char *table, const char *name, char *kind)
{
    const unsigned char *const bytes = QRY_RECORD_BLOB(record);
    const unsigned long long size = record->blob_length;
    unsigned long long opening[KREP_HEAD_WORDS];
    if (!krep_row_of(record, table) || ((size / sizeof(unsigned long long)) < KREP_HEAD_WORDS))
    {
        return 0;
    }
    memcpy(opening, bytes, sizeof(opening));
    const unsigned long long length = opening[KREP_HEAD_WORDS - 1u];
    const unsigned long long named_at = KREP_HEAD_WORDS * sizeof(unsigned long long);
    const int sought = (opening[3] == KREP_ROW_HEAD) &&
                       ((name == NULL) || ((length == strlen(name)) && (length <= (size - named_at)) &&
                                           (memcmp(bytes + named_at, name, (size_t)length) == 0)));
    for (unsigned int at = 0u; sought && (at < 4u); at += 1u)
    {
        // the kind's four bytes stand the low first in its word
        kind[at] = (char)((opening[KREP_ROW_OPENING] >> (8u * at)) & 0xffull);
    }
    kind[4] = '\0';
    return sought;
}

// `first`, `table` and `last` said into `error`, which holds `room`
static void krep_viewed_said(char *error, size_t room, const char *first, const char *table, const char *last)
{
    size_t at = verba_textus.put(&(VerbaTextusCfg){.out = error, .cap = room, .at = 0u, .text = first});
    at = verba_textus.put(&(VerbaTextusCfg){.out = error, .cap = room, .at = at, .text = table});
    at = verba_textus.put(&(VerbaTextusCfg){.out = error, .cap = room, .at = at, .text = last});
    verba_finis.finish(&(VerbaFinisCfg){.out = error, .cap = room, .at = at});
}

int krep_viewed_at(const char *path, const char *table, const char *name, KrepFile *view, char *error, size_t room)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    char kind[5] = "";
    unsigned long long key = 0ull;
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while ((bytes != NULL) && (key == 0ull) && qry_record_read(bytes, size, &at, &record))
    {
        key = krep_head_of(record, table, name, kind) ? record->time : 0ull;
    }
    if (key == 0ull)
    {
        krep_viewed_said(error, room, path, (bytes == NULL) ? "" : ": the table ",
                         (bytes == NULL) ? ": the run could not be read" : table);
    }
    krep_open(view, (key != 0ull) ? kind : "\0\0\0\0");
    int held = (key != 0ull);
    at = 0ull;
    while (held && qry_record_read(bytes, size, &at, &record))
    {
        held = !krep_row_of(record, table) || krep_rows_viewed(QRY_RECORD_BLOB(record), record->blob_length, key, view);
    }
    if ((key != 0ull) && !held)
    {
        krep_viewed_said(error, room, path, ": a row of ", table);
        krep_open(view, kind);
    }
    qry_member_file_unmapped(bytes, size);
    return held;
}

unsigned long long krep_text_number(KrepFile *file, const char *bytes, unsigned long long length)
{
    if ((file->text_slots != 0u) && (file->text_slot[krep_text_slot(file, bytes, length)] != 0u))
    {
        return file->text_slot[krep_text_slot(file, bytes, length)];
    }
    return krep_text_taken(file, bytes, length);
}

const char *krep_text(const KrepFile *file, unsigned long long number, unsigned long long *length)
{
    if ((number == 0ull) || (number > file->texts))
    {
        *length = 0ull;
        return NULL;
    }
    *length = file->text_length[number - 1u];
    return file->text[number - 1u];
}

int krep_who_at(const KrepFile *file, unsigned int at, Who *who)
{
    for (unsigned int before = (at < file->records) ? at : file->records; before != 0u; before -= 1u)
    {
        const KrepRecord *const held = &file->record[before - 1u];
        if (held->code != KREP_RECORD_WHO)
        {
            continue;
        }
        // a who record of no words says no who is in force from it on
        if (held->count != 2u)
        {
            return 0;
        }
        *who = who_of_words(held->word);
        return 1;
    }
    return 0;
}

int krep_who_same(Who one, Who other)
{
    return (one.source == other.source) && (one.line == other.line) && (one.program == other.program);
}
