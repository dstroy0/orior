// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: what is read: the records and the numbers and fields of a row, an oracle parsed and freed, a code named and its family given, the blobs of a run grown and kept, each blob read, and the oracle read from the run
#include "kdecode_internal.h"

KqrRecord s_record;
KrepFile s_file;

// the records of `file` from `from`, `count` of them or every one past it where `count` is 0, a line each: raw, its
// code and its numbers in hex, where `raw`; and where not, each who named and every other record with its code, its
// numbers and the texts it names by number given after it
void kdecode_records(const KrepFile *file, int raw, unsigned int from, unsigned int count)
{
    const unsigned int past =
        (from >= file->records)
            ? from
            : (((count == 0u) || (count > (file->records - from))) ? file->records : (from + count));
    for (unsigned int at = from; at < past; at += 1u)
    {
        const KrepRecord *const held = &file->record[at];
        if (!raw && (held->code == KREP_RECORD_WHO) && (held->count == 2u))
        {
            char code[QRY_WHO_CODE_LETTERS + 1u];
            char named[KDECODE_NAMED];
            const QryWho who = qry_who_of_words(held->word);
            qry_who_code(who, code, sizeof(code));
            printf("who %s %s\n", code, qry_who_named(who, named, sizeof(named)) ? named : "");
            continue;
        }
        printf("%x", held->code);
        for (unsigned int place = 0u; place < held->count; place += 1u)
        {
            printf(" %llx", held->word[place]);
        }
        for (unsigned int place = 0u; !raw && (place < held->count); place += 1u)
        {
            unsigned long long length = 0ull;
            const char *const text = krep_text(file, held->word[place], &length);
            // a text a record names is printed to its length, which an int counts
            if ((held->code != KREP_RECORD_WHO) && (text != NULL) && (held->word[place] != 0ull))
            {
                printf(" [%llx %.*s]", held->word[place], (int)length, text);
            }
        }
        printf("\n");
    }
}

// the number the word `word` writes into `value`: 1, or 0 where it is not one written plain in decimal
int kdecode_number(const char *word, unsigned long long *value)
{
    char *after = NULL;
    *value = strtoull(word, &after, 10);
    return (after != word) && (*after == '\0');
}

// `field` of the line `line`, its fields apart by tabs, copied into `word`, which holds `room`: 1, or 0 where the line
// has no such field
static int kdecode_field(const char *line, unsigned int field, char *word, size_t room)
{
    const char *at = line;
    for (unsigned int passed = 0u; passed < field; passed += 1u)
    {
        at = strchr(at, '\t');
        if (at == NULL)
        {
            return 0;
        }
        at += 1;
    }
    const size_t length = strcspn(at, "\t\r\n");
    // a field cut to its room is the field's start, which no oracle field this reads reaches
    const int written = snprintf(word, room, "%.*s", (int)length, at);
    return written >= 0;
}

// the oracle `bytes` hold, `size` of them, read into `oracle`: its codes, and where `codes_only` is 0 its row's kind
// and its columns too
static void kdecode_oracle_parsed(const unsigned char *bytes, unsigned long long size, KdecodeOracle *oracle,
                                  int codes_only)
{
    static char s_line[KDECODE_LINE];
    unsigned long long at = 0ull;
    while (qry_line_next(bytes, size, &at, s_line, sizeof(s_line)))
    {
        char where[KDECODE_WORD];
        char kind[KDECODE_WORD];
        char code[KDECODE_WORD];
        char form[KDECODE_WORD];
        if (!kdecode_field(s_line, 0u, where, sizeof(where)) || !kdecode_field(s_line, 2u, kind, sizeof(kind)) ||
            !kdecode_field(s_line, 3u, code, sizeof(code)) || !kdecode_field(s_line, 4u, form, sizeof(form)))
        {
            continue;
        }
        if (!codes_only && (strcmp(where, "row") == 0) && (oracle->kind[0] == '\0'))
        {
            snprintf(oracle->kind, sizeof(oracle->kind), "%s", kind);
        }
        if (!codes_only && (strncmp(where, "column ", 7u) == 0))
        {
            const unsigned long place = strtoul(where + 7, NULL, 10);
            if (place < QRY_ROW_COLUMNS)
            {
                // a column's place is below QRY_ROW_COLUMNS, which an unsigned int holds
                KdecodeColumn *const column = &oracle->column[place];
                snprintf(column->name, sizeof(column->name), "%s", kind);
                snprintf(column->form, sizeof(column->form), "%s", form);
                const char *const gloss = strrchr(s_line, '\t');
                const char *const named = (gloss != NULL) ? strstr(gloss, ".oracle.tsv") : NULL;
                const char *start = named;
                while ((start != NULL) && (start > gloss) && (isalnum((unsigned char)start[-1]) || (start[-1] == '_')))
                {
                    start -= 1;
                }
                if ((named != NULL) && (start < named))
                {
                    // the oracle's name, its stem and .oracle.tsv, is far shorter than an int counts
                    snprintf(column->codes_from, sizeof(column->codes_from), "%.*s",
                             (int)((named - start) + (ptrdiff_t)strlen(".oracle.tsv")), start);
                }
                oracle->columns =
                    ((unsigned int)place >= oracle->columns) ? ((unsigned int)place + 1u) : oracle->columns;
            }
        }
        if (strncmp(code, "0x", 2u) == 0)
        {
            if (oracle->codes == oracle->code_room)
            {
                const unsigned int room = (oracle->code_room == 0u) ? 32u : (oracle->code_room * 2u);
                KdecodeCode *const larger = (KdecodeCode *)realloc(oracle->code, room * sizeof(KdecodeCode));
                if (larger == NULL)
                {
                    continue;
                }
                oracle->code = larger;
                oracle->code_room = room;
            }
            // a family numbered past its name, `record 1`, is the family its name gives
            KdecodeCode *const one = &oracle->code[oracle->codes];
            snprintf(one->family, sizeof(one->family), "%.*s", (int)strcspn(where, " "), where);
            snprintf(one->name, sizeof(one->name), "%s", kind);
            one->value = strtoull(code, NULL, 16);
            oracle->codes += 1u;
        }
    }
}

void kdecode_oracle_free(KdecodeOracle *oracle)
{
    free(oracle->code);
    memset(oracle, 0, sizeof(*oracle));
}

// the name of the code `value` of the family `family` in `oracle`, or NULL where it gives none
const char *kdecode_code_named(const KdecodeOracle *oracle, const char *family, unsigned long long value)
{
    for (unsigned int at = 0u; at < oracle->codes; at += 1u)
    {
        if ((oracle->code[at].value == value) && (strcmp(oracle->code[at].family, family) == 0))
        {
            return oracle->code[at].name;
        }
    }
    return NULL;
}

// 1 where `oracle` gives a code of the family `family`
int kdecode_family_given(const KdecodeOracle *oracle, const char *family)
{
    for (unsigned int at = 0u; at < oracle->codes; at += 1u)
    {
        if (strcmp(oracle->code[at].family, family) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// room in `*items` for one more of `width` bytes, `*count` held and `*room` given: 1, or 0 where the system gives none
static int kdecode_grown(void **items, size_t width, size_t count, size_t *room)
{
    if (count < *room)
    {
        return 1;
    }
    const size_t larger = (*room == 0u) ? 256u : (*room * 2u);
    void *const moved = realloc(*items, larger * width);
    if (moved == NULL)
    {
        return 0;
    }
    *items = moved;
    *room = larger;
    return 1;
}

// `size` bytes at `bytes` added to the run's bytes, a word's boundary first: where they stand, or 0 with the run marked
// failed where the system gives no memory. The bytes start past a first word of zeros, so that no bytes stand at 0
static size_t kdecode_kept(KdecodeRun *run, const void *bytes, size_t size)
{
    const size_t at = (run->size == 0u) ? 8u : ((run->size + 7u) & ~(size_t)7u);
    if ((at + size) > run->room)
    {
        size_t larger = (run->room == 0u) ? (1u << 20u) : run->room;
        while (larger < (at + size))
        {
            larger *= 2u;
        }
        unsigned char *const moved = (unsigned char *)realloc(run->bytes, larger);
        if (moved == NULL)
        {
            run->failed = 1;
            return 0u;
        }
        run->bytes = moved;
        run->room = larger;
    }
    memset(run->bytes + run->size, 0, at - run->size);
    memcpy(run->bytes + at, bytes, size);
    run->size = at + size;
    return at;
}

// the word at `place` of the `size` bytes at `bytes`, the low byte first, its bytes past `size` taken as zeros
static unsigned long long kdecode_word(const unsigned char *bytes, unsigned long long size, unsigned long long place)
{
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const unsigned long long byte = (place * 8ull) + at;
        word |= (byte < size) ? ((unsigned long long)bytes[byte] << (8u * at)) : 0ull;
    }
    return word;
}

// a text `length` bytes at `bytes` of the file `file` kept under `number`
static void kdecode_text_kept(KdecodeRun *run, unsigned long long file, unsigned long long number,
                              const unsigned char *bytes, unsigned long long length)
{
    if (!kdecode_grown((void **)&run->text, sizeof(KdecodeText), run->texts, &run->text_room))
    {
        run->failed = 1;
        return;
    }
    // a text is a blob's bytes, which the mapped run holds and a size_t counts
    const size_t at = kdecode_kept(run, bytes, (size_t)length);
    run->text[run->texts] = (KdecodeText){file, number, at, length};
    run->texts += (at != 0u) ? 1u : 0u;
}

// an oracle the run holds kept, `size` bytes at `bytes` under the name at `name_at`, in place of an earlier blob of
// the name: the blobs come in the order of their sequences, and the latest is kept
static void kdecode_held_kept(KdecodeRun *run, size_t name_at, const unsigned char *bytes, unsigned long long size)
{
    size_t place = 0u;
    while ((place < run->helds) &&
           (strcmp((const char *)(run->bytes + run->held[place].name_at), (const char *)(run->bytes + name_at)) != 0))
    {
        place += 1u;
    }
    if ((place == run->helds) && !kdecode_grown((void **)&run->held, sizeof(KdecodeHeld), run->helds, &run->held_room))
    {
        run->failed = 1;
        return;
    }
    // an oracle is a blob's bytes, which the mapped run holds and a size_t counts
    const size_t at = kdecode_kept(run, bytes, (size_t)size);
    run->held[place] = (KdecodeHeld){name_at, at, size};
    run->helds += ((place == run->helds) && (at != 0u)) ? 1u : 0u;
}

// one blob of the run, handed by qry_file_each in the order of the sequences: its sequence and name kept; an oracle
// and a text of the run's table of texts kept; and where it is of the table asked, a k-file's head and text kept and a
// row of the range kept
void kdecode_blob(void *context, const char *name, unsigned long long sequence, const unsigned char *bytes,
                         unsigned long long size)
{
    KdecodeRun *const run = (KdecodeRun *)context;
    if (run->failed)
    {
        return;
    }
    if (!kdecode_grown((void **)&run->blob, sizeof(KdecodeBlob), run->blobs, &run->blob_room))
    {
        run->failed = 1;
        return;
    }
    const size_t name_at = kdecode_kept(run, name, strlen(name) + 1u);
    run->blob[run->blobs] = (KdecodeBlob){sequence, name_at};
    run->blobs += 1u;
    const size_t length = strlen(name);
    const size_t suffix = strlen(".oracle.tsv");
    if ((length > suffix) && (strcmp(name + length - suffix, ".oracle.tsv") == 0))
    {
        kdecode_held_kept(run, name_at, bytes, size);
    }
    const unsigned long long words = (size + 7ull) / 8ull;
    if ((strcmp(name, QRY_TEXTS_TABLE) == 0) && (words >= 4ull) && (kdecode_word(bytes, size, 3ull) <= (size - 32ull)))
    {
        kdecode_text_kept(run, 0ull, kdecode_word(bytes, size, 2ull), bytes + 32u, kdecode_word(bytes, size, 3ull));
    }
    if (strcmp(name, run->table) != 0)
    {
        return;
    }
    const unsigned long long change = (words >= 4ull) ? kdecode_word(bytes, size, 3ull) : ~0ull;
    if ((change == KREP_ROW_HEAD) && (words >= 5ull) &&
        kdecode_grown((void **)&run->file, sizeof(KdecodeFile), run->files, &run->file_room))
    {
        KdecodeFile *const file = &run->file[run->files];
        file->key = sequence;
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            // the kind's four bytes stand the low first in its word
            file->kind[at] = (char)((kdecode_word(bytes, size, 4ull) >> (8u * at)) & 0xffull);
        }
        file->kind[4] = '\0';
        run->files += 1u;
    }
    if ((change == KREP_ROW_TEXT) && (words >= 6ull) && (kdecode_word(bytes, size, 5ull) <= (size - 48ull)))
    {
        kdecode_text_kept(run, kdecode_word(bytes, size, 0ull), kdecode_word(bytes, size, 4ull), bytes + 48u,
                          kdecode_word(bytes, size, 5ull));
    }
    const int ranged =
        (sequence >= run->from) && (sequence <= run->to) && ((run->most == 0ull) || (run->rows < run->most));
    if (!ranged || !kdecode_grown((void **)&run->row, sizeof(KdecodeRow), run->rows, &run->row_room))
    {
        run->failed = run->failed || ranged;
        return;
    }
    // a row is a blob's bytes, which the mapped run holds and a size_t counts
    const size_t at = kdecode_kept(run, bytes, (size_t)size);
    run->row[run->rows] = (KdecodeRow){sequence, at, (size_t)words};
    run->rows += (at != 0u) ? 1u : 0u;
}

// the oracle at `path` from the top of the tree read into `oracle` as kdecode_oracle_parsed reads it: the one the run
// holds, the latest blob of its name, or where the run holds none, the tree's. 1, or 0 where neither could be read
int kdecode_oracle_read(const KdecodeRun *run, const char *path, KdecodeOracle *oracle, int codes_only)
{
    for (size_t at = 0u; at < run->helds; at += 1u)
    {
        if (strcmp((const char *)(run->bytes + run->held[at].name_at), path) == 0)
        {
            kdecode_oracle_parsed(run->bytes + run->held[at].bytes_at, run->held[at].size, oracle, codes_only);
            return 1;
        }
    }
    char top[1024];
    char full[1400];
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_top(top, sizeof(top)) || (snprintf(full, sizeof(full), "%s/%s", top, path) <= 0) ||
        !qry_file_read(qry_run(), full, &bytes, &size))
    {
        return 0;
    }
    kdecode_oracle_parsed(bytes, size, oracle, codes_only);
    free(bytes);
    return 1;
}
