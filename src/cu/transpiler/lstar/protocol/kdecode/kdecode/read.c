// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: what is read: the text handed to the run, the records and the numbers and fields of a row, an oracle parsed
// and emptied, a code named and its family given, the blobs of a run kept where they stand, and the oracle read from
// the run
#include "kdecode_internal.h"
#include "../../../../../includes/parsers/line_read.h"
#include "../../../../../includes/parsers/who.h"
#include "../../table_text.h"

KqrRecord s_record;
KrepFile s_file;

static char s_shown_bytes[QRY_WRITER_RING_BYTES / 2ull];
ScripturaLine s_shown = {s_shown_bytes, sizeof(s_shown_bytes), 0ull};

// what s_shown holds handed to the run as KDECODE_SHOWN and emptied: where `whole` is 1, or once it holds past half its
// room
void kdecode_shown_handed(int whole)
{
    if (!whole && (s_shown.at < (s_shown.capacity / 2ull)))
    {
        return;
    }
    if (scriptura_fits(&s_shown) && (s_shown.at != 0ull))
    {
        qry_member_append(KDECODE_SHOWN, s_shown.out, s_shown.at, QRY_ORDINARY);
    }
    s_shown.at = 0ull;
}

// the records of `file` from `from`, `count` of them or every one past it where `count` is 0, a line each: raw, its
// code and its numbers in hex, where `raw`; and where not, each who named and every other record with its code, its
// numbers and the texts it names by number after it
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
            char code[WHO_CODE_LETTERS + 1u];
            char named[KDECODE_NAMED];
            const Who who = who_of_words(held->word);
            who_code(who, code, sizeof(code));
            scriptura_text(&s_shown, "who ");
            scriptura_text(&s_shown, code);
            scriptura_character(&s_shown, ' ');
            scriptura_text(&s_shown, who_named(who, named, sizeof(named)) ? named : "");
            scriptura_character(&s_shown, '\n');
            kdecode_shown_handed(0);
            continue;
        }
        scriptura_hex(&s_shown, held->code, 0u);
        for (unsigned int place = 0u; place < held->count; place += 1u)
        {
            scriptura_character(&s_shown, ' ');
            scriptura_hex(&s_shown, held->word[place], 0u);
        }
        for (unsigned int place = 0u; !raw && (place < held->count); place += 1u)
        {
            unsigned long long length = 0ull;
            const char *const text = krep_text(file, held->word[place], &length);
            if ((held->code != KREP_RECORD_WHO) && (text != NULL) && (held->word[place] != 0ull))
            {
                scriptura_text(&s_shown, " [");
                scriptura_hex(&s_shown, held->word[place], 0u);
                scriptura_character(&s_shown, ' ');
                scriptura_counted(&s_shown, text, length);
                scriptura_character(&s_shown, ']');
            }
        }
        scriptura_character(&s_shown, '\n');
        kdecode_shown_handed(0);
    }
}

// the number the word `word` writes into `value`: 1, or 0 where it is not one written plain in decimal
int kdecode_number(const char *word, unsigned long long *value)
{
    const char *at = word;
    return line_number(&at, 0, value) && (*at == '\0');
}

// `field` of the line `line`, its fields apart by tabs, copied into `word`, which holds `room`, cut to it: 1, or 0
// where the line has no such field
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
    ScripturaLine copied = {word, room, 0ull};
    scriptura_text_most(&copied, at, (length < room) ? length : (room - 1u));
    scriptura_closed(&copied);
    return 1;
}

// the oracle `bytes` hold, `size` of them, read into `oracle`: its codes, and where `codes_only` is 0 its row's kind
// and its columns too
static void kdecode_oracle_parsed(const unsigned char *bytes, unsigned long long size, KdecodeOracle *oracle,
                                  int codes_only)
{
    static char s_line[KDECODE_LINE];
    unsigned long long at = 0ull;
    while (line_read(bytes, size, &at, s_line, sizeof(s_line)))
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
            scriptura_text_into(oracle->kind, sizeof(oracle->kind), kind);
        }
        if (!codes_only && (strncmp(where, "column ", 7u) == 0))
        {
            const char *place_at = where + 7;
            unsigned long long place = 0ull;
            line_number(&place_at, 0, &place);
            if (place < TABLE_ROW_COLUMNS)
            {
                // a column's place is below TABLE_ROW_COLUMNS, which an unsigned int holds
                KdecodeColumn *const column = &oracle->column[place];
                scriptura_text_into(column->name, sizeof(column->name), kind);
                scriptura_text_into(column->form, sizeof(column->form), form);
                const char *const gloss = strrchr(s_line, '\t');
                const char *const named = (gloss != NULL) ? strstr(gloss, ".oracle.tsv") : NULL;
                const char *start = named;
                while ((start != NULL) && (start > gloss) && (isalnum((unsigned char)start[-1]) || (start[-1] == '_')))
                {
                    start -= 1;
                }
                if ((named != NULL) && (start < named))
                {
                    // the oracle's name, its stem and .oracle.tsv, is far shorter than its room
                    ScripturaLine codes_from = {column->codes_from, sizeof(column->codes_from), 0ull};
                    scriptura_text_most(&codes_from, start,
                                        (unsigned long long)(named - start) + (sizeof(".oracle.tsv") - 1u));
                    scriptura_closed(&codes_from);
                }
                oracle->columns =
                    ((unsigned int)place >= oracle->columns) ? ((unsigned int)place + 1u) : oracle->columns;
            }
        }
        if ((strncmp(code, "0x", 2u) == 0) && (oracle->codes < KDECODE_CODES_MOST))
        {
            // a family numbered past its name, `record 1`, is the family its name gives
            KdecodeCode *const one = &oracle->code[oracle->codes];
            ScripturaLine family = {one->family, sizeof(one->family), 0ull};
            scriptura_text_most(&family, where, strcspn(where, " "));
            scriptura_closed(&family);
            scriptura_text_into(one->name, sizeof(one->name), kind);
            const char *value_at = code;
            one->value = 0ull;
            line_number(&value_at, 1, &one->value);
            oracle->codes += 1u;
        }
    }
}

void kdecode_oracle_free(KdecodeOracle *oracle)
{
    oracle->kind[0] = '\0';
    memset(oracle->column, 0, sizeof(oracle->column));
    oracle->columns = 0u;
    oracle->codes = 0u;
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

// a text `length` bytes at `bytes` of the file `file` kept under `number`, where it stands
static void kdecode_text_kept(KdecodeRun *run, unsigned long long file, unsigned long long number,
                              const unsigned char *bytes, unsigned long long length)
{
    if (run->texts >= KDECODE_RECORDS_MOST)
    {
        run->failed = 1;
        return;
    }
    run->text[run->texts] = (KdecodeText){file, number, bytes, length};
    run->texts += 1u;
}

// 1 where the `name_length` letters at `name` are the text `text`
static int kdecode_name_is(const char *name, unsigned int name_length, const char *text)
{
    return (strlen(text) == name_length) && (memcmp(name, text, name_length) == 0);
}

// an oracle the run holds kept, `size` bytes at `bytes` under its name, in place of an earlier blob of the name: the
// blobs come in the order of their sequences, and the latest is kept
static void kdecode_held_kept(KdecodeRun *run, const char *name, unsigned int name_length, const unsigned char *bytes,
                              unsigned long long size)
{
    size_t place = 0u;
    while ((place < run->helds) && ((run->held[place].name_length != name_length) ||
                                     (memcmp(run->held[place].name, name, name_length) != 0)))
    {
        place += 1u;
    }
    if ((place == run->helds) && (run->helds >= KDECODE_HELDS_MOST))
    {
        run->failed = 1;
        return;
    }
    run->held[place] = (KdecodeHeld){name, name_length, bytes, size};
    run->helds += (place == run->helds) ? 1u : 0u;
}

// one blob of the run, handed by kdecode_table in the order of the sequences: its sequence and name kept; an oracle
// and a text of the run's table of texts kept; and where it is of the table asked, a k-file's head and text kept and
// a row of the range kept, each where it stands in the mapped run
void kdecode_blob(void *context, const char *name, unsigned int name_length, unsigned long long sequence,
                  const unsigned char *bytes, unsigned long long size)
{
    KdecodeRun *const run = (KdecodeRun *)context;
    if (run->failed)
    {
        return;
    }
    if (run->blobs >= KDECODE_RECORDS_MOST)
    {
        run->failed = 1;
        return;
    }
    run->blob[run->blobs] = (KdecodeBlob){sequence, name, name_length};
    run->blobs += 1u;
    const unsigned int suffix = (unsigned int)(sizeof(".oracle.tsv") - 1u);
    if ((name_length > suffix) && (memcmp(name + name_length - suffix, ".oracle.tsv", suffix) == 0))
    {
        kdecode_held_kept(run, name, name_length, bytes, size);
    }
    const unsigned long long words = (size + 7ull) / 8ull;
    if (kdecode_name_is(name, name_length, TABLE_TEXTS) && (words >= 4ull) &&
        (kdecode_word(bytes, size, 3ull) <= (size - 32ull)))
    {
        kdecode_text_kept(run, 0ull, kdecode_word(bytes, size, 2ull), bytes + 32u, kdecode_word(bytes, size, 3ull));
    }
    if (!kdecode_name_is(name, name_length, run->table))
    {
        return;
    }
    const unsigned long long change = (words >= 4ull) ? kdecode_word(bytes, size, 3ull) : ~0ull;
    if ((change == KREP_ROW_HEAD) && (words >= 5ull) && (run->files < KDECODE_FILES_MOST))
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
    if (!ranged)
    {
        return;
    }
    if (run->rows >= KDECODE_RECORDS_MOST)
    {
        run->failed = 1;
        return;
    }
    // a row's words are its blob's, which the mapped run holds and a size_t counts
    run->row[run->rows] = (KdecodeRow){sequence, bytes, (size_t)words};
    run->rows += 1u;
}

// the oracle at `path` from the top of the tree read into `oracle` as kdecode_oracle_parsed reads it: the one the run
// read holds, the latest blob of its name, or where it holds none, the tree's, read from the top of the tree where
// orior_kdecode stands. 1, or 0 where neither could be read
int kdecode_oracle_read(const KdecodeRun *run, const char *path, KdecodeOracle *oracle, int codes_only)
{
    for (size_t at = 0u; at < run->helds; at += 1u)
    {
        if (kdecode_name_is(run->held[at].name, run->held[at].name_length, path))
        {
            kdecode_oracle_parsed(run->held[at].bytes, run->held[at].size, oracle, codes_only);
            return 1;
        }
    }
    // an oracle is far below a mebibyte of lines
    static unsigned char s_oracle[1ull << 22u];
    const unsigned long long size = qry_member_file_read(path, s_oracle, sizeof(s_oracle));
    if (size == QRY_MEMBER_UNREAD)
    {
        return 0;
    }
    kdecode_oracle_parsed(s_oracle, size, oracle, codes_only);
    return 1;
}
