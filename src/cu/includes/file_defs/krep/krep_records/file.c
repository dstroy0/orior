// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file.c: the file: opened, read, filed in the run, written by a who and freed, a record put, added, dropped and set,
// and the file tabled by a who
#include "../../../../transpiler/lstar/protocol/table_text.h"
#include "../../../parsers/who.h"
#include "krep_internal.h"

void krep_open(KrepFile *file, const char *kind)
{
    krep_free(file);
    memcpy(file->kind, kind, 4u);
}

int krep_read(KrepFile *file, const char *path, const char *kind, char *error, size_t room)
{
    krep_open(file, kind);
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_member_file_blob(path, &bytes, &size))
    {
        return 1;
    }
    if ((size < KREP_HEAD_BYTES) || (memcmp(bytes, KREP_MAGIC, 8u) != 0))
    {
        return 2;
    }
    const char *stopped = NULL;
    if (memcmp(bytes + 8u, kind, 4u) != 0)
    {
        stopped = "is not of the kind read";
    }
    else if (krep_half_of(bytes + 12u) != KREP_VERSION)
    {
        stopped = "is of another version";
    }
    unsigned long long at = KREP_HEAD_BYTES;
    static unsigned long long s_words[1u << 16u];
    while ((stopped == NULL) && (at < size))
    {
        unsigned long long code = 0ull;
        unsigned long long count = 0ull;
        if (!krep_number_read(bytes, size, &at, &code) || !krep_number_read(bytes, size, &at, &count) ||
            (code > 0xffffffffull))
        {
            stopped = "holds a record that runs past its end";
            break;
        }
        if (code == KREP_RECORD_TEXT)
        {
            // the count of a text is its bytes, each one of the file's
            if (count > (size - at))
            {
                stopped = "holds a text that runs past its end";
                break;
            }
            if (krep_text_taken(file, (const char *)(bytes + at), count) == 0ull)
            {
                stopped = "has a text there was no memory for";
            }
            at += count;
            continue;
        }
        // a record's count is at most its numbers, each at least one of the file's bytes
        if (count > (size - at))
        {
            stopped = "holds a record that runs past its end";
            break;
        }
        unsigned long long *const owned = (count > (sizeof(s_words) / sizeof(s_words[0])))
                                              ? (unsigned long long *)malloc((size_t)count * sizeof(unsigned long long))
                                              : NULL;
        unsigned long long *const words = (owned != NULL) ? owned : s_words;
        if ((count > (sizeof(s_words) / sizeof(s_words[0]))) && (owned == NULL))
        {
            stopped = "has a record there was no memory for";
            break;
        }
        for (unsigned long long word = 0ull; (stopped == NULL) && (word < count); word += 1ull)
        {
            stopped = krep_number_read(bytes, size, &at, &words[word]) ? NULL : "holds a record that runs past its end";
        }
        // a code and a count read past those above are at most what an unsigned int holds
        if ((stopped == NULL) && !krep_record_add(file, (unsigned int)code, words, (unsigned int)count))
        {
            stopped = "has a record there was no memory for";
        }
        free(owned);
    }
    if (stopped != NULL)
    {
        snprintf(error, room, "%s %s", path, stopped);
        krep_open(file, kind);
        return 0;
    }
    return 1;
}

// `file` put together in memory and handed whole to the run's writer as the file at `path`, written by `who`, which
// writes it in the file's place: the sequence of its bytes' blob, or 0 where it could not be written
static unsigned long long krep_filed(const KrepFile *file, const char *path, Who who)
{
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ull];
    const size_t room = sizeof(s_text_bytes);
    unsigned char head[KREP_HEAD_BYTES];
    memcpy(head, KREP_MAGIC, 8u);
    memcpy(head + 8u, file->kind, 4u);
    krep_half_put(head + 12u, KREP_VERSION);
    size_t text = verba_textus.put_n(&(VerbaTextusCfg){
        .out = s_text_bytes, .cap = room, .at = 0u, .text = (const char *)head, .text_len = sizeof(head)});
    for (unsigned int number = 0u; number < file->texts; number += 1u)
    {
        text = krep_number_added(s_text_bytes, room, text, KREP_RECORD_TEXT);
        text = krep_number_added(s_text_bytes, room, text, file->text_length[number]);
        text = verba_textus.put_n(&(VerbaTextusCfg){.out = s_text_bytes,
                                                    .cap = room,
                                                    .at = text,
                                                    .text = file->text[number],
                                                    .text_len = file->text_length[number]});
    }
    for (unsigned int at = 0u; at < file->records; at += 1u)
    {
        const KrepRecord *const held = &file->record[at];
        text = krep_number_added(s_text_bytes, room, text, held->code);
        text = krep_number_added(s_text_bytes, room, text, held->count);
        for (unsigned int place = 0u; place < held->count; place += 1u)
        {
            text = krep_number_added(s_text_bytes, room, text, held->word[place]);
        }
    }
    const unsigned long long time =
        verba_finis.ok(&(VerbaFinisCfg){.cap = room, .at = text}) ? qry_member_filed(path, s_text_bytes, text) : 0ull;
    const char *const name = qry_member_file_name(path);
    if ((time != 0ull) && (name != NULL))
    {
        // the file's row of the table files (files.oracle.tsv): the who that wrote it, its record and its name
        unsigned long long row[4] = {0ull, 0ull, time, table_text(name, strlen(name))};
        who_words(who, row);
        qry_member_append(TABLE_FILES, row, sizeof(row), QRY_CRITICAL);
    }
    return time;
}

int krep_write_by(const KrepFile *file, const char *path, Who who)
{
    if (file->key == 0ull)
    {
        return krep_filed(file, path, who) != 0ull;
    }
    // the file its rows put together written, the view of its table, and not the one held: its bytes' blob linked to
    // the head row of the rows it was written from
    KrepFile view;
    memset(&view, 0, sizeof(view));
    const unsigned long long written = krep_viewed(file, &view) ? krep_filed(&view, path, who) : 0ull;
    krep_free(&view);
    if (written != 0ull)
    {
        unsigned long long link[2u + 3] = {0ull, 0ull, written, file->key, TABLE_LINK_WRITTEN_FROM};
        who_words(who, link);
        qry_member_append(TABLE_LINKS, link, sizeof(link), QRY_ORDINARY);
    }
    return written != 0ull;
}

void krep_free(KrepFile *file)
{
    for (unsigned int at = 0u; at < file->records; at += 1u)
    {
        free(file->record[at].word);
    }
    for (unsigned int number = 0u; number < file->texts; number += 1u)
    {
        free(file->text[number]);
    }
    free(file->record);
    free(file->text);
    free(file->text_length);
    free(file->text_slot);
    memset(file, 0, sizeof(*file));
}

int krep_record_put(KrepFile *file, unsigned int at, unsigned int code, const unsigned long long *words,
                    unsigned int count)
{
    if (at > file->records)
    {
        return 0;
    }
    if (file->records == file->room)
    {
        const unsigned int grown = (file->room == 0u) ? 1024u : (2u * file->room);
        KrepRecord *const larger = (KrepRecord *)realloc(file->record, (size_t)grown * sizeof(KrepRecord));
        if (larger == NULL)
        {
            return 0;
        }
        file->record = larger;
        file->room = grown;
    }
    unsigned long long *const copy = (unsigned long long *)malloc(((size_t)count + 1u) * sizeof(unsigned long long));
    if (copy == NULL)
    {
        return 0;
    }
    if (count != 0u)
    {
        memcpy(copy, words, (size_t)count * sizeof(unsigned long long));
    }
    memmove(&file->record[at + 1u], &file->record[at], (size_t)(file->records - at) * sizeof(KrepRecord));
    file->record[at].code = code;
    file->record[at].count = count;
    file->record[at].word = copy;
    file->records += 1u;
    if (file->key != 0ull)
    {
        unsigned long long who[2];
        krep_who_words_at(file, at, who);
        krep_record_row(file, at, who);
    }
    return 1;
}

int krep_record_add(KrepFile *file, unsigned int code, const unsigned long long *words, unsigned int count)
{
    return krep_record_put(file, file->records, code, words, count);
}

void krep_record_drop(KrepFile *file, unsigned int at)
{
    if (at >= file->records)
    {
        return;
    }
    if (file->key != 0ull)
    {
        unsigned long long who[2];
        krep_who_words_at(file, at, who);
        const unsigned long long numbers[1] = {at};
        krep_row_handed(file, file->key, who, KREP_ROW_DROP, numbers, 1u, NULL, 0ull, QRY_ORDINARY);
    }
    free(file->record[at].word);
    memmove(&file->record[at], &file->record[at + 1u], (size_t)(file->records - at - 1u) * sizeof(KrepRecord));
    file->records -= 1u;
}

int krep_record_set(KrepFile *file, unsigned int at, const unsigned long long *words, unsigned int count)
{
    if (at >= file->records)
    {
        return 0;
    }
    unsigned long long *const copy = (unsigned long long *)malloc(((size_t)count + 1u) * sizeof(unsigned long long));
    if (copy == NULL)
    {
        return 0;
    }
    if (count != 0u)
    {
        memcpy(copy, words, (size_t)count * sizeof(unsigned long long));
    }
    free(file->record[at].word);
    file->record[at].word = copy;
    file->record[at].count = count;
    if (file->key != 0ull)
    {
        unsigned long long who[2];
        krep_who_words_at(file, at, who);
        const unsigned long long numbers[2] = {at, count};
        krep_row_handed(file, file->key, who, KREP_ROW_SET, numbers, 2u, copy,
                        (unsigned long long)count * sizeof(unsigned long long), QRY_ORDINARY);
    }
    return 1;
}

int krep_tabled_by(KrepFile *file, QryShared *buffer, const char *table, const char *path, Who who)
{
    file->key = 0ull;
    const char *const name = qry_member_file_name(path);
    if ((buffer == NULL) || (name == NULL))
    {
        return 0;
    }
    file->run = buffer;
    file->table = table;
    unsigned long long words[2];
    who_words(who, words);
    const unsigned long long length = strlen(name);
    const unsigned long long head[3] = {krep_half_of((const unsigned char *)file->kind), KREP_VERSION, length};
    file->key = krep_row_handed(file, 0ull, words, KREP_ROW_HEAD, head, 3u, name, length, QRY_CRITICAL);
    for (unsigned int number = 1u; (file->key != 0ull) && (number <= file->texts); number += 1u)
    {
        krep_text_row(file, number);
    }
    // each record's who the who in force over it, carried down the records from each who record
    words[0] = 0ull;
    words[1] = 0ull;
    for (unsigned int at = 0u; (file->key != 0ull) && (at < file->records); at += 1u)
    {
        const KrepRecord *const held = &file->record[at];
        if (held->code == KREP_RECORD_WHO)
        {
            words[0] = (held->count == 2u) ? held->word[0] : 0ull;
            words[1] = (held->count == 2u) ? held->word[1] : 0ull;
        }
        krep_record_row(file, at, words);
    }
    return file->key != 0ull;
}
