// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// viewed.c: the file as a view: its rows viewed from a run's table, the rows gathered from a finished .qry, a text's number and text, and the who at a row
#include "krep_internal.h"

// the rows of one table gathered from a finished .qry (krep_row_gathered): the table's name; the name of the file
// sought, NULL for the first the table holds; the rows' bytes joined in the order of their sequences, their count and
// the room they have; the key of the file's head row and the file's kind, 0 and empty until the head is seen; and
// whether memory ran out
typedef struct
{
    const char *table;
    const char *name;
    unsigned char *bytes;
    unsigned long long size;
    unsigned long long room;
    unsigned long long key;
    char kind[5];
    int failed;
} KrepGathered;

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
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if ((file->key == 0ull) || !qry_joined(file->run, file->table, &bytes, &size))
    {
        return 0;
    }
    const int held = krep_rows_viewed(bytes, size, file->key, view);
    free(bytes);
    if (!held)
    {
        krep_open(view, file->kind);
    }
    return held;
}

// one blob of the .qry, handed by qry_file_each in the order of the sequences: a row of the table gathered whole, and
// the first head row of the file sought, or of any file where none is sought, giving the key and the kind
static void krep_row_gathered(void *context, const char *name, unsigned long long sequence, const unsigned char *bytes,
                              unsigned long long size)
{
    KrepGathered *const gathered = (KrepGathered *)context;
    if (gathered->failed || (strcmp(name, gathered->table) != 0))
    {
        return;
    }
    const unsigned long long words = size / sizeof(unsigned long long);
    unsigned long long opening[KREP_HEAD_WORDS] = {0ull, 0ull, 0ull, 0ull, 0ull, 0ull, 0ull};
    if (words >= KREP_HEAD_WORDS)
    {
        memcpy(opening, bytes, sizeof(opening));
    }
    if ((gathered->key == 0ull) && (words >= KREP_HEAD_WORDS) && (opening[3] == KREP_ROW_HEAD))
    {
        const unsigned long long length = opening[KREP_HEAD_WORDS - 1u];
        const unsigned long long named_at = KREP_HEAD_WORDS * sizeof(unsigned long long);
        const int sought = (gathered->name == NULL) ||
                           ((length == strlen(gathered->name)) && (length <= (size - named_at)) &&
                            (memcmp(bytes + named_at, gathered->name, (size_t)length) == 0));
        if (sought)
        {
            gathered->key = sequence;
            for (unsigned int at = 0u; at < 4u; at += 1u)
            {
                // the kind's four bytes stand the low first in its word
                gathered->kind[at] = (char)((opening[KREP_ROW_OPENING] >> (8u * at)) & 0xffull);
            }
            gathered->kind[4] = '\0';
        }
    }
    if ((gathered->size + size) > gathered->room)
    {
        unsigned long long room = (gathered->room != 0ull) ? gathered->room : 65536ull;
        while (room < (gathered->size + size))
        {
            room *= 2ull;
        }
        // a row's bytes fit a size_t: every blob of a .qry is mapped whole
        unsigned char *const grown = (unsigned char *)realloc(gathered->bytes, (size_t)room);
        if (grown == NULL)
        {
            gathered->failed = 1;
            return;
        }
        gathered->bytes = grown;
        gathered->room = room;
    }
    memcpy(gathered->bytes + gathered->size, bytes, (size_t)size);
    gathered->size += size;
}

int krep_viewed_at(const char *path, const char *table, const char *name, KrepFile *view, char *error, size_t room)
{
    KrepGathered gathered = {table, name, NULL, 0ull, 0ull, 0ull, "", 0};
    const int read = qry_file_each(path, krep_row_gathered, &gathered, error, room);
    int held = read && !gathered.failed && (gathered.key != 0ull);
    if (read && gathered.failed)
    {
        snprintf(error, room, "%s: no memory for the rows of %s", path, table);
    }
    else if (read && (gathered.key == 0ull))
    {
        snprintf(error, room, "%s: the table %s holds no head of %s", path, table, (name != NULL) ? name : "a file");
    }
    krep_open(view, held ? gathered.kind : "\0\0\0\0");
    if (held && !krep_rows_viewed(gathered.bytes, gathered.size, gathered.key, view))
    {
        snprintf(error, room, "%s: a row of %s does not fit the file it puts together", path, table);
        krep_open(view, gathered.kind);
        held = 0;
    }
    free(gathered.bytes);
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

int krep_who_at(const KrepFile *file, unsigned int at, QryWho *who)
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
        *who = qry_who_of_words(held->word);
        return 1;
    }
    return 0;
}

int krep_who_same(QryWho one, QryWho other)
{
    return (one.source == other.source) && (one.line == other.line) && (one.program == other.program);
}
