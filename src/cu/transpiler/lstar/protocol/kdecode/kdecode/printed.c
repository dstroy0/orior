// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// printed.c: what is written into the text passed to the run: a row's word, a key named, a text, a who and a column,
// the columns of a table, the format oracle, a change, and the table
#include "../../../../../../../includes/parsers/who.h"
#include "../../table_text.h"
#include "kdecode_internal.h"

// the word at `place` of the retained row `row`
static unsigned long long kdecode_row_word(const KdecodeRow *row, size_t place)
{
    unsigned long long word = 0ull;
    memcpy(&word, row->words_at + (place * 8u), 8u);
    return word;
}

// the blob whose sequence is `key`, or NULL where the run contains none: the blobs are in the order of their
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

// the text of the file `file` numbered `number`, its length in `length`, or NULL where the run contains none
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
static void kdecode_hex_displayed(unsigned long long number)
{
    mmgr_line_ch(&s_displayed, ' ');
    mmgr_line_hex(&s_displayed, number, 0u);
}

// ` <number>` in decimal
void kdecode_decimal_displayed(unsigned long long number)
{
    mmgr_line_ch(&s_displayed, ' ');
    mmgr_line_decimal(&s_displayed, number, 0u);
}

// ` [<number> <text>]` of the `length` bytes of `text` numbered `number`
static void kdecode_numbered_displayed(unsigned long long number, const char *text, unsigned long long length)
{
    mmgr_line_put(&s_displayed, " [");
    mmgr_line_hex(&s_displayed, number, 0u);
    mmgr_line_ch(&s_displayed, ' ');
    mmgr_line_put_n(&s_displayed, text, length);
    mmgr_line_ch(&s_displayed, ']');
}

// a line ended, and the text passed where it contains past half its room
void kdecode_line_ended(void)
{
    mmgr_line_ch(&s_displayed, '\n');
    kdecode_displayed_passed(0);
}

// ` [<number> <text>]` of the text `number` of the file `file`, ` -` where the number is 0, and the number alone where
// the run contains no such text
static void kdecode_text_printed(const KdecodeRun *run, unsigned long long file, unsigned long long number)
{
    unsigned long long length = 0ull;
    const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
    if (number == 0ull)
    {
        mmgr_line_put(&s_displayed, " -");
    }
    else if (text == NULL)
    {
        kdecode_hex_displayed(number);
    }
    else
    {
        kdecode_numbered_displayed(number, text, length);
    }
}

// ` who <named>` of the who words `words` carry, nothing where both are 0, a row of no who
static void kdecode_who_printed(const unsigned long long *words)
{
    char named[KDECODE_NAMED];
    if (((words[0] != 0ull) || (words[1] != 0ull)) && who_named(who_of_words(words), named, sizeof(named)))
    {
        mmgr_line_put(&s_displayed, " who ");
        mmgr_line_put(&s_displayed, named);
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
        VerbaLinea word = {family, sizeof(family), 0ull};
        mmgr_line_put_max(&word, form + 1, (unsigned long long)(strchr(form, '>') - form - 1));
        mmgr_line_finish(&word);
    }
    const char *const named = kdecode_code_named(oracle, family, value);
    if (strcmp(family, "key") == 0)
    {
        const KdecodeBlob *const blob = kdecode_key_named(run, value);
        kdecode_decimal_displayed(value);
        if (blob != NULL)
        {
            mmgr_line_ch(&s_displayed, ':');
            mmgr_line_put_n(&s_displayed, blob->name, blob->name_length);
        }
    }
    else if (strcmp(family, "text") == 0)
    {
        kdecode_text_printed(run, 0ull, value);
    }
    else if ((either != NULL) && (value <= 1ull))
    {
        // the first word is for 1 and the second for 0
        mmgr_line_ch(&s_displayed, ' ');
        if (value == 1ull)
        {
            mmgr_line_put_n(&s_displayed, form, (unsigned long long)(either - form));
        }
        else
        {
            mmgr_line_put(&s_displayed, either + 1);
        }
    }
    else if (named != NULL)
    {
        mmgr_line_ch(&s_displayed, ' ');
        mmgr_line_put(&s_displayed, named);
    }
    else if (kdecode_family_given(oracle, family) || (strcmp(family, "hash") == 0) || (either != NULL))
    {
        kdecode_hex_displayed(value);
    }
    else
    {
        kdecode_decimal_displayed(value);
    }
}

// the row `row` of a table with an oracle, its who's two words then its columns, decoded: its key, the row's type, each
// column, any word past the oracle's columns in hex, and its who named
static void kdecode_columns_printed(const KdecodeRun *run, const KdecodeOracle *oracle, const KdecodeRow *row)
{
    unsigned long long who[2] = {0ull, 0ull};
    for (size_t at = 0u; (at < 2u) && (at < row->words); at += 1u)
    {
        who[at] = kdecode_row_word(row, at);
    }
    mmgr_line_decimal(&s_displayed, row->key, 0u);
    mmgr_line_ch(&s_displayed, ' ');
    mmgr_line_put(&s_displayed, oracle->type);
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
            kdecode_hex_displayed(value);
        }
    }
    kdecode_who_printed(who);
    kdecode_line_ended();
}

// the oracle of the type of the file `key` of the table, read into `oracle` once a file: the type's own, its folder and
// its stem the type's characters in lower case
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
        while ((length < 4u) && (run->file[at].type[length] != '\0'))
        {
            stem[length] = (char)tolower((unsigned char)run->file[at].type[length]);
            length += 1u;
        }
        stem[length] = '\0';
        char path[256];
        VerbaLinea oracle_path = {path, sizeof(path), 0ull};
        mmgr_line_put(&oracle_path, TABLE_KFILE_ORACLES);
        mmgr_line_ch(&oracle_path, '/');
        mmgr_line_put(&oracle_path, stem);
        mmgr_line_ch(&oracle_path, '/');
        mmgr_line_put(&oracle_path, stem);
        mmgr_line_put(&oracle_path, ".oracle.tsv");
        mmgr_line_finish(&oracle_path);
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
        mmgr_line_decimal(&s_displayed, row->key, 0u);
        for (size_t at = 0u; at < row->words; at += 1u)
        {
            kdecode_hex_displayed(kdecode_row_word(row, at));
        }
        kdecode_line_ended();
        return;
    }
    const unsigned long long change = kdecode_row_word(row, 3u);
    const unsigned long long file = (change == KREP_ROW_HEAD) ? row->key : kdecode_row_word(row, 0u);
    const unsigned long long who[2] = {kdecode_row_word(row, 1u), kdecode_row_word(row, 2u)};
    const char *const named = kdecode_code_named(rows, "row", change);
    mmgr_line_decimal(&s_displayed, row->key, 0u);
    if (named != NULL)
    {
        mmgr_line_ch(&s_displayed, ' ');
        mmgr_line_put(&s_displayed, named);
    }
    else
    {
        kdecode_hex_displayed(change);
    }
    if ((change == KREP_ROW_HEAD) && (row->words >= 7u))
    {
        const unsigned long long type = kdecode_row_word(row, 4u);
        const unsigned long long length = kdecode_row_word(row, 6u);
        const unsigned long long room = (row->words - 7u) * 8u;
        char characters[5] = "";
        for (unsigned int at = 0u; (at < 4u) && (((type >> (8u * at)) & 0xffull) != 0ull); at += 1u)
        {
            // the type's four bytes are the low first in its word, its zeros past its characters
            characters[at] = (char)((type >> (8u * at)) & 0xffull);
        }
        // the name is at max its row's bytes
        mmgr_line_ch(&s_displayed, ' ');
        mmgr_line_put(&s_displayed, characters);
        kdecode_decimal_displayed(kdecode_row_word(row, 5u));
        mmgr_line_ch(&s_displayed, ' ');
        mmgr_line_put_n(&s_displayed, (const char *)(row->words_at + 56u), (length <= room) ? length : room);
    }
    else if ((change == KREP_ROW_TEXT) && (row->words >= 5u))
    {
        kdecode_decimal_displayed(file);
        kdecode_text_printed(run, file, kdecode_row_word(row, 4u));
    }
    else
    {
        kdecode_decimal_displayed(file);
        // a put gives its place, its code and its count; a set its place and its count; a drop its place
        const size_t opening = (change == KREP_ROW_PUT) ? 7u : ((change == KREP_ROW_SET) ? 6u : 5u);
        for (size_t at = 4u; (at < opening) && (at < row->words); at += 1u)
        {
            const unsigned long long value = kdecode_row_word(row, at);
            const KdecodeOracle *const type =
                ((change == KREP_ROW_PUT) && (at == 5u)) ? kdecode_format_oracle(run, file, format, format_for) : NULL;
            const char *const code = (type != NULL) ? kdecode_code_named(type, "record", value) : NULL;
            if (code != NULL)
            {
                mmgr_line_ch(&s_displayed, ' ');
                mmgr_line_put(&s_displayed, code);
            }
            else
            {
                kdecode_hex_displayed(value);
            }
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            kdecode_hex_displayed(kdecode_row_word(row, at));
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            unsigned long long length = 0ull;
            const unsigned long long number = kdecode_row_word(row, at);
            const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
            if (text != NULL)
            {
                kdecode_numbered_displayed(number, text, length);
            }
        }
    }
    kdecode_who_printed(who);
    kdecode_line_ended();
}

// the table `table` of the run at `path` from `from` through `to`, `max` rows of it or every one where `max` is 0,
// raw or decoded: 1, or 0 where the run could not be read
int kdecode_table(const char *path, const char *table, int raw, unsigned long long from, unsigned long long to,
                  unsigned long long max)
{
    static KdecodeBlob s_blobs[KDECODE_RECORDS_MAX];
    static KdecodeText s_texts[KDECODE_RECORDS_MAX];
    static KdecodeFile s_files[KDECODE_FILES_MAX];
    static KdecodeRow s_rows[KDECODE_RECORDS_MAX];
    static KdecodeContent s_contents[KDECODE_CONTENTS_MAX];
    KdecodeRun run = {table, from, to, max, s_blobs, 0u, s_texts, 0u, s_files, 0u, s_rows, 0u, s_contents, 0u, 0};
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    const int read = kdecode_records_each(bytes, size, kdecode_blob, &run);
    if (!read || run.failed)
    {
        kdecode_displayed_passed(1);
        mmgr_line_put(&s_displayed, run.failed ? "orior_kdecode: the run contains more than a walk retains\n"
                                           : "orior_kdecode: the run could not be read whole\n");
        kdecode_failure_passed();
    }
    static KdecodeOracle s_oracle;
    static KdecodeOracle s_format;
    kdecode_oracle_free(&s_oracle);
    kdecode_oracle_free(&s_format);
    unsigned long long format_for = 0ull;
    char oracle_path[256];
    VerbaLinea oracle_line = {oracle_path, sizeof(oracle_path), 0ull};
    mmgr_line_put(&oracle_line, TABLE_ORACLES);
    mmgr_line_ch(&oracle_line, '/');
    mmgr_line_put(&oracle_line, table);
    mmgr_line_put(&oracle_line, ".oracle.tsv");
    mmgr_line_finish(&oracle_line);
    const int texts = (strcmp(table, TABLE_TEXTS) == 0);
    const int columned =
        !raw && !texts && kdecode_oracle_read(&run, oracle_path, &s_oracle, 0) && (s_oracle.columns != 0u);
    for (unsigned int at = 0u; columned && (at < s_oracle.columns); at += 1u)
    {
        char from_path[256];
        const char *const named = s_oracle.column[at].codes_from;
        if (named[0] != '\0')
        {
            VerbaLinea from_line = {from_path, sizeof(from_path), 0ull};
            mmgr_line_put(&from_line, TABLE_ORACLES);
            mmgr_line_ch(&from_line, '/');
            mmgr_line_put(&from_line, named);
            if (mmgr_line_finish(&from_line) != 0ull)
            {
                kdecode_oracle_read(&run, from_path, &s_oracle, 1);
            }
        }
    }
    // a table with no oracle is a k-file's where the run contains a head of it, and is given raw where it contains none
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
            mmgr_line_hex(&s_displayed, row->key, 0u);
            for (size_t place = 0u; place < row->words; place += 1u)
            {
                kdecode_hex_displayed(kdecode_row_word(row, place));
            }
            kdecode_line_ended();
        }
        else if (texts)
        {
            mmgr_line_decimal(&s_displayed, row->key, 0u);
            mmgr_line_put(&s_displayed, " text");
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
