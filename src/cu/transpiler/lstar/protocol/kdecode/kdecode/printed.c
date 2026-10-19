// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// printed.c: what is written into the text handed to the run: a row's word, a key named, a text, a who and a column,
// the columns of a table, the format oracle, a change, a blob, and the table
#include "kdecode_internal.h"
#include "../../../../../includes/parsers/who.h"
#include "../../table_text.h"

// the word at `place` of the kept row `row`
static unsigned long long kdecode_row_word(const KdecodeRow *row, size_t place)
{
    unsigned long long word = 0ull;
    memcpy(&word, row->words_at + (place * 8u), 8u);
    return word;
}

// the blob whose sequence is `key`, or NULL where the run holds none: the blobs stand in the order of their
// sequences, and a search halves them
static const KdecodeBlob *kdecode_key_named(const KdecodeRun *run, unsigned long long key)
{
    size_t low = 0u;
    size_t high = run->blobs;
    while (low < high)
    {
        const size_t middle = low + ((high - low) / 2u);
        if (run->blob[middle].sequence < key)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < run->blobs) && (run->blob[low].sequence == key)) ? &run->blob[low] : NULL;
}

// the text of the file `file` numbered `number`, its length in `length`, or NULL where the run holds none
static const char *kdecode_text(const KdecodeRun *run, unsigned long long file, unsigned long long number,
                                unsigned long long *length)
{
    for (size_t at = 0u; at < run->texts; at += 1u)
    {
        if ((run->text[at].file == file) && (run->text[at].number == number))
        {
            *length = run->text[at].length;
            return (const char *)run->text[at].bytes;
        }
    }
    return NULL;
}

// ` <number>` in hex
static void kdecode_hex_shown(unsigned long long number)
{
    scriptura_character(&s_shown, ' ');
    scriptura_hex(&s_shown, number, 0u);
}

// ` <number>` in decimal
static void kdecode_decimal_shown(unsigned long long number)
{
    scriptura_character(&s_shown, ' ');
    scriptura_decimal(&s_shown, number, 0u);
}

// ` [<number> <text>]` of the `length` bytes of `text` numbered `number`
static void kdecode_numbered_shown(unsigned long long number, const char *text, unsigned long long length)
{
    scriptura_text(&s_shown, " [");
    scriptura_hex(&s_shown, number, 0u);
    scriptura_character(&s_shown, ' ');
    scriptura_counted(&s_shown, text, length);
    scriptura_character(&s_shown, ']');
}

// a line ended, and the text handed where it holds past half its room
static void kdecode_line_ended(void)
{
    scriptura_character(&s_shown, '\n');
    kdecode_shown_handed(0);
}

// ` [<number> <text>]` of the text `number` of the file `file`, ` -` where the number is 0, and the number alone where
// the run holds no such text
static void kdecode_text_printed(const KdecodeRun *run, unsigned long long file, unsigned long long number)
{
    unsigned long long length = 0ull;
    const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
    if (number == 0ull)
    {
        scriptura_text(&s_shown, " -");
    }
    else if (text == NULL)
    {
        kdecode_hex_shown(number);
    }
    else
    {
        kdecode_numbered_shown(number, text, length);
    }
}

// ` who <named>` of the who words `words` hold, nothing where both are 0, a row of no who
static void kdecode_who_printed(const unsigned long long *words)
{
    char named[KDECODE_NAMED];
    if (((words[0] != 0ull) || (words[1] != 0ull)) && who_named(who_of_words(words), named, sizeof(named)))
    {
        scriptura_text(&s_shown, " who ");
        scriptura_text(&s_shown, named);
    }
}

// the column `column` of value `value` written as `oracle` gives it
static void kdecode_column_printed(const KdecodeRun *run, const KdecodeOracle *oracle, const KdecodeColumn *column,
                                   unsigned long long value)
{
    const char *const form = column->form;
    const char *const either = strchr(form, '|');
    char family[KDECODE_WORD] = "";
    if ((form[0] == '<') && (strchr(form, '>') != NULL))
    {
        // the family is the form's word, between its brackets
        ScripturaLine word = {family, sizeof(family), 0ull};
        scriptura_text_most(&word, form + 1, (unsigned long long)(strchr(form, '>') - form - 1));
        scriptura_closed(&word);
    }
    const char *const named = kdecode_code_named(oracle, family, value);
    if (strcmp(family, "key") == 0)
    {
        const KdecodeBlob *const blob = kdecode_key_named(run, value);
        kdecode_decimal_shown(value);
        if (blob != NULL)
        {
            scriptura_character(&s_shown, ':');
            scriptura_counted(&s_shown, blob->name, blob->name_length);
        }
    }
    else if (strcmp(family, "text") == 0)
    {
        kdecode_text_printed(run, 0ull, value);
    }
    else if ((either != NULL) && (value <= 1ull))
    {
        // the first word stands for 1 and the second for 0
        scriptura_character(&s_shown, ' ');
        if (value == 1ull)
        {
            scriptura_counted(&s_shown, form, (unsigned long long)(either - form));
        }
        else
        {
            scriptura_text(&s_shown, either + 1);
        }
    }
    else if (named != NULL)
    {
        scriptura_character(&s_shown, ' ');
        scriptura_text(&s_shown, named);
    }
    else if (kdecode_family_given(oracle, family) || (strcmp(family, "hash") == 0) || (either != NULL))
    {
        kdecode_hex_shown(value);
    }
    else
    {
        kdecode_decimal_shown(value);
    }
}

// the row `row` of a table with an oracle, its who's two words then its columns, decoded: its key, the row's kind, each
// column, any word past the oracle's columns in hex, and its who named
static void kdecode_columns_printed(const KdecodeRun *run, const KdecodeOracle *oracle, const KdecodeRow *row)
{
    unsigned long long who[2] = {0ull, 0ull};
    for (size_t at = 0u; (at < 2u) && (at < row->words); at += 1u)
    {
        who[at] = kdecode_row_word(row, at);
    }
    scriptura_decimal(&s_shown, row->key, 0u);
    scriptura_character(&s_shown, ' ');
    scriptura_text(&s_shown, oracle->kind);
    for (size_t at = 2u; at < row->words; at += 1u)
    {
        const size_t place = at - 2u;
        const unsigned long long value = kdecode_row_word(row, at);
        if ((place < oracle->columns) && (oracle->column[place].form[0] != '\0'))
        {
            kdecode_column_printed(run, oracle, &oracle->column[place], value);
        }
        else
        {
            kdecode_hex_shown(value);
        }
    }
    kdecode_who_printed(who);
    kdecode_line_ended();
}

// the oracle of the kind of the file `key` of the table, read into `oracle` once a file: the kind's own, its folder and
// its stem the kind's letters in lower case
static const KdecodeOracle *kdecode_format_oracle(const KdecodeRun *run, unsigned long long key, KdecodeOracle *oracle,
                                                  unsigned long long *read_for)
{
    if (*read_for == key)
    {
        return oracle;
    }
    kdecode_oracle_free(oracle);
    *read_for = key;
    for (size_t at = 0u; at < run->files; at += 1u)
    {
        if (run->file[at].key != key)
        {
            continue;
        }
        char stem[5];
        size_t length = 0u;
        while ((length < 4u) && (run->file[at].kind[length] != '\0'))
        {
            stem[length] = (char)tolower((unsigned char)run->file[at].kind[length]);
            length += 1u;
        }
        stem[length] = '\0';
        char path[256];
        ScripturaLine oracle_path = {path, sizeof(path), 0ull};
        scriptura_text(&oracle_path, TABLE_KFILE_ORACLES);
        scriptura_character(&oracle_path, '/');
        scriptura_text(&oracle_path, stem);
        scriptura_character(&oracle_path, '/');
        scriptura_text(&oracle_path, stem);
        scriptura_text(&oracle_path, ".oracle.tsv");
        scriptura_closed(&oracle_path);
        kdecode_oracle_read(run, path, oracle, 1);
    }
    return oracle;
}

// the row `row` of a k-file's table decoded: its key, its change as `rows` names it, the file's key and the numbers the
// change gives, a put record's code named by its format's oracle, the texts of the file a record names, and its who
static void kdecode_change_printed(const KdecodeRun *run, const KdecodeOracle *rows, const KdecodeRow *row,
                                   KdecodeOracle *format, unsigned long long *format_for)
{
    if (row->words < 4u)
    {
        scriptura_decimal(&s_shown, row->key, 0u);
        for (size_t at = 0u; at < row->words; at += 1u)
        {
            kdecode_hex_shown(kdecode_row_word(row, at));
        }
        kdecode_line_ended();
        return;
    }
    const unsigned long long change = kdecode_row_word(row, 3u);
    const unsigned long long file = (change == KREP_ROW_HEAD) ? row->key : kdecode_row_word(row, 0u);
    const unsigned long long who[2] = {kdecode_row_word(row, 1u), kdecode_row_word(row, 2u)};
    const char *const named = kdecode_code_named(rows, "row", change);
    scriptura_decimal(&s_shown, row->key, 0u);
    if (named != NULL)
    {
        scriptura_character(&s_shown, ' ');
        scriptura_text(&s_shown, named);
    }
    else
    {
        kdecode_hex_shown(change);
    }
    if ((change == KREP_ROW_HEAD) && (row->words >= 7u))
    {
        const unsigned long long kind = kdecode_row_word(row, 4u);
        const unsigned long long length = kdecode_row_word(row, 6u);
        const unsigned long long room = (row->words - 7u) * 8u;
        char letters[5] = "";
        for (unsigned int at = 0u; (at < 4u) && (((kind >> (8u * at)) & 0xffull) != 0ull); at += 1u)
        {
            // the kind's four bytes stand the low first in its word, its zeros past its letters
            letters[at] = (char)((kind >> (8u * at)) & 0xffull);
        }
        // the name is at most its row's bytes
        scriptura_character(&s_shown, ' ');
        scriptura_text(&s_shown, letters);
        kdecode_decimal_shown(kdecode_row_word(row, 5u));
        scriptura_character(&s_shown, ' ');
        scriptura_counted(&s_shown, (const char *)(row->words_at + 56u), (length <= room) ? length : room);
    }
    else if ((change == KREP_ROW_TEXT) && (row->words >= 5u))
    {
        kdecode_decimal_shown(file);
        kdecode_text_printed(run, file, kdecode_row_word(row, 4u));
    }
    else
    {
        kdecode_decimal_shown(file);
        // a put gives its place, its code and its count; a set its place and its count; a drop its place
        const size_t opening = (change == KREP_ROW_PUT) ? 7u : ((change == KREP_ROW_SET) ? 6u : 5u);
        for (size_t at = 4u; (at < opening) && (at < row->words); at += 1u)
        {
            const unsigned long long value = kdecode_row_word(row, at);
            const KdecodeOracle *const kind =
                ((change == KREP_ROW_PUT) && (at == 5u)) ? kdecode_format_oracle(run, file, format, format_for) : NULL;
            const char *const code = (kind != NULL) ? kdecode_code_named(kind, "record", value) : NULL;
            if (code != NULL)
            {
                scriptura_character(&s_shown, ' ');
                scriptura_text(&s_shown, code);
            }
            else
            {
                kdecode_hex_shown(value);
            }
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            kdecode_hex_shown(kdecode_row_word(row, at));
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            unsigned long long length = 0ull;
            const unsigned long long number = kdecode_row_word(row, at);
            const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
            if (text != NULL)
            {
                kdecode_numbered_shown(number, text, length);
            }
        }
    }
    kdecode_who_printed(who);
    kdecode_line_ended();
}

// a blob written whole: a line of its sequence, its name and its size, then its bytes, a NUL as a space, and a newline
static void blob_printed(const char *name, unsigned int name_length, unsigned long long sequence,
                         const unsigned char *bytes, unsigned long long size)
{
    scriptura_decimal(&s_shown, sequence, 0u);
    scriptura_character(&s_shown, ' ');
    scriptura_counted(&s_shown, name, name_length);
    kdecode_decimal_shown(size);
    scriptura_character(&s_shown, '\n');
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        // the text is handed every 64 KiB at most, far inside the half of its room it may still take
        if ((at & 0xffffull) == 0ull)
        {
            kdecode_shown_handed(0);
        }
        scriptura_character(&s_shown, (bytes[at] == 0u) ? ' ' : (char)bytes[at]);
    }
    kdecode_line_ended();
}

// every record of the `size` bytes at `bytes`, a .qry mapped, handed to `each` with `context`, in the order the writer
// wrote them, its time in place of its place: 1, or 0 where the bytes are not records whole
static int kdecode_records_each(const unsigned char *bytes, unsigned long long size,
                                void (*each)(void *context, const char *name, unsigned int name_length,
                                             unsigned long long time, const unsigned char *blob,
                                             unsigned long long blob_size),
                                void *context)
{
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while ((bytes != NULL) && qry_record_read(bytes, size, &at, &record))
    {
        each(context, QRY_RECORD_NAME(record), record->name_length, record->time, QRY_RECORD_BLOB(record),
             record->blob_length);
    }
    return (bytes != NULL) && (at == size);
}

// what the blob walk of kdecode_blobs keeps: the name wanted, whether the latest alone is written, and that one where
// it stands in the mapped run
typedef struct
{
    const char *name;
    int latest;
    unsigned long long sequence;
    const unsigned char *bytes;
    unsigned long long size;
} KdecodeBlobs;

static void blobs_each(void *context, const char *name, unsigned int name_length, unsigned long long sequence,
                       const unsigned char *bytes, unsigned long long size)
{
    KdecodeBlobs *const walk = (KdecodeBlobs *)context;
    if ((strlen(walk->name) != name_length) || (memcmp(name, walk->name, name_length) != 0))
    {
        return;
    }
    if (!walk->latest)
    {
        blob_printed(name, name_length, sequence, bytes, size);
        return;
    }
    walk->bytes = bytes;
    walk->size = size;
    walk->sequence = sequence;
}

int kdecode_blobs(const char *path, const char *name, int latest)
{
    KdecodeBlobs walk = {name, latest, 0ull, NULL, 0ull};
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    const int read = kdecode_records_each(bytes, size, blobs_each, &walk);
    if (!read)
    {
        scriptura_text(&s_shown, "kdecode: the run could not be read whole\n");
    }
    if (read && latest && (walk.bytes != NULL))
    {
        blob_printed(name, (unsigned int)strlen(name), walk.sequence, walk.bytes, walk.size);
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    return read;
}

// the table `table` of the run at `path` from `from` through `to`, `most` rows of it or every one where `most` is 0,
// raw or decoded: 1, or 0 where the run could not be read
int kdecode_table(const char *path, const char *table, int raw, unsigned long long from, unsigned long long to,
                  unsigned long long most)
{
    static KdecodeBlob s_blobs[KDECODE_RECORDS_MOST];
    static KdecodeText s_texts[KDECODE_RECORDS_MOST];
    static KdecodeFile s_files[KDECODE_FILES_MOST];
    static KdecodeRow s_rows[KDECODE_RECORDS_MOST];
    static KdecodeHeld s_helds[KDECODE_HELDS_MOST];
    KdecodeRun run = {table, from, to, most, s_blobs, 0u, s_texts, 0u, s_files, 0u, s_rows, 0u, s_helds, 0u, 0};
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    const int read = kdecode_records_each(bytes, size, kdecode_blob, &run);
    if (!read || run.failed)
    {
        scriptura_text(&s_shown, run.failed ? "kdecode: the run holds more than a walk keeps\n"
                                             : "kdecode: the run could not be read whole\n");
    }
    static KdecodeOracle s_oracle;
    static KdecodeOracle s_format;
    kdecode_oracle_free(&s_oracle);
    kdecode_oracle_free(&s_format);
    unsigned long long format_for = 0ull;
    char oracle_path[256];
    ScripturaLine oracle_line = {oracle_path, sizeof(oracle_path), 0ull};
    scriptura_text(&oracle_line, TABLE_ORACLES);
    scriptura_character(&oracle_line, '/');
    scriptura_text(&oracle_line, table);
    scriptura_text(&oracle_line, ".oracle.tsv");
    scriptura_closed(&oracle_line);
    const int texts = (strcmp(table, TABLE_TEXTS) == 0);
    const int columned =
        !raw && !texts && kdecode_oracle_read(&run, oracle_path, &s_oracle, 0) && (s_oracle.columns != 0u);
    for (unsigned int at = 0u; columned && (at < s_oracle.columns); at += 1u)
    {
        char from_path[256];
        const char *const named = s_oracle.column[at].codes_from;
        if (named[0] != '\0')
        {
            ScripturaLine from_line = {from_path, sizeof(from_path), 0ull};
            scriptura_text(&from_line, TABLE_ORACLES);
            scriptura_character(&from_line, '/');
            scriptura_text(&from_line, named);
            if (scriptura_closed(&from_line) != 0ull)
            {
                kdecode_oracle_read(&run, from_path, &s_oracle, 1);
            }
        }
    }
    // a table with no oracle is a k-file's where the run holds a head of it, and is given raw where it holds none
    const int changed = !raw && !texts && !columned && (run.files != 0u);
    if (changed)
    {
        kdecode_oracle_free(&s_oracle);
        kdecode_oracle_read(&run, TABLE_KFILE_ROWS, &s_oracle, 1);
    }
    const int bare = raw || (!texts && !columned && !changed);
    for (size_t at = 0u; read && !run.failed && (at < run.rows); at += 1u)
    {
        const KdecodeRow *const row = &run.row[at];
        if (bare || (texts && (row->words < 4u)))
        {
            scriptura_hex(&s_shown, row->key, 0u);
            for (size_t place = 0u; place < row->words; place += 1u)
            {
                kdecode_hex_shown(kdecode_row_word(row, place));
            }
            kdecode_line_ended();
        }
        else if (texts)
        {
            scriptura_decimal(&s_shown, row->key, 0u);
            scriptura_text(&s_shown, " text");
            kdecode_text_printed(&run, 0ull, kdecode_row_word(row, 2u));
            kdecode_line_ended();
        }
        else if (columned)
        {
            kdecode_columns_printed(&run, &s_oracle, row);
        }
        else
        {
            kdecode_change_printed(&run, &s_oracle, row, &s_format, &format_for);
        }
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    return read && !run.failed;
}
