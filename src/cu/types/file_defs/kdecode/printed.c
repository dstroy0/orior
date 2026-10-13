// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// printed.c: what is printed: a row's word, a key named, a text, a who and a column printed, the columns of a table, the format oracle, a change, and the table
#include "kdecode_internal.h"

// the word at `place` of the kept row `row`
static unsigned long long kdecode_row_word(const KdecodeRun *run, const KdecodeRow *row, size_t place)
{
    unsigned long long word = 0ull;
    memcpy(&word, run->bytes + row->words_at + (place * 8u), 8u);
    return word;
}

// the name of the blob whose sequence is `key`, or NULL where the run holds none: the blobs stand in the order of
// their sequences, and a search halves them
static const char *kdecode_key_named(const KdecodeRun *run, unsigned long long key)
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
    return ((low < run->blobs) && (run->blob[low].sequence == key))
               ? (const char *)(run->bytes + run->blob[low].name_at)
               : NULL;
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
            return (const char *)(run->bytes + run->text[at].bytes_at);
        }
    }
    return NULL;
}

// ` [<number> <text>]` of the text `number` of the file `file`, `-` where the number is 0, and the number alone where
// the run holds no such text
static void kdecode_text_printed(const KdecodeRun *run, unsigned long long file, unsigned long long number)
{
    unsigned long long length = 0ull;
    const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
    if (number == 0ull)
    {
        printf(" -");
    }
    else if (text == NULL)
    {
        printf(" %llx", number);
    }
    else
    {
        // a text is printed to its length, which an int counts
        printf(" [%llx %.*s]", number, (int)length, text);
    }
}

// ` who <named>` of the who words `words` hold, nothing where both are 0, a row of no who
static void kdecode_who_printed(const unsigned long long *words)
{
    char named[KDECODE_NAMED];
    if (((words[0] != 0ull) || (words[1] != 0ull)) && qry_who_named(qry_who_of_words(words), named, sizeof(named)))
    {
        printf(" who %s", named);
    }
}

// the column `column` of value `value` printed as `oracle` gives it
static void kdecode_column_printed(const KdecodeRun *run, const KdecodeOracle *oracle, const KdecodeColumn *column,
                                   unsigned long long value)
{
    const char *const form = column->form;
    const char *const either = strchr(form, '|');
    char family[KDECODE_WORD] = "";
    if ((form[0] == '<') && (strchr(form, '>') != NULL))
    {
        // the family is the form's word, between its brackets, far shorter than an int counts
        snprintf(family, sizeof(family), "%.*s", (int)(strchr(form, '>') - form - 1), form + 1);
    }
    const char *const named = kdecode_code_named(oracle, family, value);
    if (strcmp(family, "key") == 0)
    {
        const char *const blob = kdecode_key_named(run, value);
        if (blob != NULL)
        {
            printf(" %llu:%s", value, blob);
        }
        else
        {
            printf(" %llu", value);
        }
    }
    else if (strcmp(family, "text") == 0)
    {
        kdecode_text_printed(run, 0ull, value);
    }
    else if ((either != NULL) && (value <= 1ull))
    {
        // the first word stands for 1 and the second for 0, each far shorter than an int counts
        printf(" %.*s", (value == 1ull) ? (int)(either - form) : (int)strlen(either + 1),
               (value == 1ull) ? form : (either + 1));
    }
    else if (named != NULL)
    {
        printf(" %s", named);
    }
    else if (kdecode_family_given(oracle, family) || (strcmp(family, "hash") == 0) || (either != NULL))
    {
        printf(" %llx", value);
    }
    else
    {
        printf(" %llu", value);
    }
}

// the row `row` of a table with an oracle, its who's two words then its columns, decoded: its key, the row's kind, each
// column, any word past the oracle's columns in hex, and its who named
static void kdecode_columns_printed(const KdecodeRun *run, const KdecodeOracle *oracle, const KdecodeRow *row)
{
    unsigned long long who[2] = {0ull, 0ull};
    for (size_t at = 0u; (at < 2u) && (at < row->words); at += 1u)
    {
        who[at] = kdecode_row_word(run, row, at);
    }
    printf("%llu %s", row->key, oracle->kind);
    for (size_t at = 2u; at < row->words; at += 1u)
    {
        const size_t place = at - 2u;
        const unsigned long long value = kdecode_row_word(run, row, at);
        if ((place < oracle->columns) && (oracle->column[place].form[0] != '\0'))
        {
            kdecode_column_printed(run, oracle, &oracle->column[place], value);
        }
        else
        {
            printf(" %llx", value);
        }
    }
    kdecode_who_printed(who);
    printf("\n");
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
        snprintf(path, sizeof(path), "%s/%s/%s.oracle.tsv", QRY_KFILE_ORACLES, stem, stem);
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
        printf("%llu", row->key);
        for (size_t at = 0u; at < row->words; at += 1u)
        {
            printf(" %llx", kdecode_row_word(run, row, at));
        }
        printf("\n");
        return;
    }
    const unsigned long long change = kdecode_row_word(run, row, 3u);
    const unsigned long long file = (change == KREP_ROW_HEAD) ? row->key : kdecode_row_word(run, row, 0u);
    const unsigned long long who[2] = {kdecode_row_word(run, row, 1u), kdecode_row_word(run, row, 2u)};
    const char *const named = kdecode_code_named(rows, "row", change);
    if (named != NULL)
    {
        printf("%llu %s", row->key, named);
    }
    else
    {
        printf("%llu %llx", row->key, change);
    }
    if ((change == KREP_ROW_HEAD) && (row->words >= 7u))
    {
        const unsigned long long kind = kdecode_row_word(run, row, 4u);
        const unsigned long long length = kdecode_row_word(run, row, 6u);
        const size_t room = (row->words - 7u) * 8u;
        char letters[5] = "";
        for (unsigned int at = 0u; (at < 4u) && (((kind >> (8u * at)) & 0xffull) != 0ull); at += 1u)
        {
            // the kind's four bytes stand the low first in its word, its zeros past its letters
            letters[at] = (char)((kind >> (8u * at)) & 0xffull);
        }
        // the name's length is at most its row's bytes, which an int counts
        printf(" %s %llu %.*s", letters, kdecode_row_word(run, row, 5u), (int)((length <= room) ? length : room),
               (const char *)(run->bytes + row->words_at + 56u));
    }
    else if ((change == KREP_ROW_TEXT) && (row->words >= 5u))
    {
        printf(" %llu", file);
        kdecode_text_printed(run, file, kdecode_row_word(run, row, 4u));
    }
    else
    {
        printf(" %llu", file);
        // a put gives its place, its code and its count; a set its place and its count; a drop its place
        const size_t opening = (change == KREP_ROW_PUT) ? 7u : ((change == KREP_ROW_SET) ? 6u : 5u);
        for (size_t at = 4u; (at < opening) && (at < row->words); at += 1u)
        {
            const unsigned long long value = kdecode_row_word(run, row, at);
            const KdecodeOracle *const kind =
                ((change == KREP_ROW_PUT) && (at == 5u)) ? kdecode_format_oracle(run, file, format, format_for) : NULL;
            const char *const code = (kind != NULL) ? kdecode_code_named(kind, "record", value) : NULL;
            if (code != NULL)
            {
                printf(" %s", code);
            }
            else
            {
                printf(" %llx", value);
            }
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            printf(" %llx", kdecode_row_word(run, row, at));
        }
        for (size_t at = opening; at < row->words; at += 1u)
        {
            unsigned long long length = 0ull;
            const unsigned long long number = kdecode_row_word(run, row, at);
            const char *const text = (number != 0ull) ? kdecode_text(run, file, number, &length) : NULL;
            if (text != NULL)
            {
                // a text a record names is printed to its length, which an int counts
                printf(" [%llx %.*s]", number, (int)length, text);
            }
        }
    }
    kdecode_who_printed(who);
    printf("\n");
}

// the table `table` of the run at `path` from `from` through `to`, `most` rows of it or every one where `most` is 0,
// raw or decoded: 1, or 0 where the run could not be read
int kdecode_table(const char *path, const char *table, int raw, unsigned long long from, unsigned long long to,
                         unsigned long long most)
{
    KdecodeRun run;
    memset(&run, 0, sizeof(run));
    run.table = table;
    run.from = from;
    run.to = to;
    run.most = most;
    char error[1400];
    const int read = qry_file_each(path, kdecode_blob, &run, error, sizeof(error));
    if (!read || run.failed)
    {
        fprintf(stderr, "kdecode: %s\n", run.failed ? "no memory to read the run" : error);
    }
    else if (error[0] != '\0')
    {
        fprintf(stderr, "kdecode: %s\n", error);
    }
    KdecodeOracle oracle;
    KdecodeOracle format;
    memset(&oracle, 0, sizeof(oracle));
    memset(&format, 0, sizeof(format));
    unsigned long long format_for = 0ull;
    char oracle_path[256];
    snprintf(oracle_path, sizeof(oracle_path), "%s/%s.oracle.tsv", QRY_ORACLES, table);
    const int texts = (strcmp(table, QRY_TEXTS_TABLE) == 0);
    const int columned = !raw && !texts && kdecode_oracle_read(&run, oracle_path, &oracle, 0) && (oracle.columns != 0u);
    for (unsigned int at = 0u; columned && (at < oracle.columns); at += 1u)
    {
        char from_path[256];
        const char *const named = oracle.column[at].codes_from;
        if ((named[0] != '\0') && (snprintf(from_path, sizeof(from_path), "%s/%s", QRY_ORACLES, named) > 0))
        {
            kdecode_oracle_read(&run, from_path, &oracle, 1);
        }
    }
    // a table with no oracle is a k-file's where the run holds a head of it, and is given raw where it holds none
    const int changed = !raw && !texts && !columned && (run.files != 0u);
    if (changed)
    {
        kdecode_oracle_free(&oracle);
        kdecode_oracle_read(&run, QRY_KFILE_ROWS, &oracle, 1);
    }
    const int bare = raw || (!texts && !columned && !changed);
    for (size_t at = 0u; read && !run.failed && (at < run.rows); at += 1u)
    {
        const KdecodeRow *const row = &run.row[at];
        if (bare || (texts && (row->words < 4u)))
        {
            printf("%llx", row->key);
            for (size_t place = 0u; place < row->words; place += 1u)
            {
                printf(" %llx", kdecode_row_word(&run, row, place));
            }
            printf("\n");
        }
        else if (texts)
        {
            printf("%llu text", row->key);
            kdecode_text_printed(&run, 0ull, kdecode_row_word(&run, row, 2u));
            printf("\n");
        }
        else if (columned)
        {
            kdecode_columns_printed(&run, &oracle, row);
        }
        else
        {
            kdecode_change_printed(&run, &oracle, row, &format, &format_for);
        }
    }
    kdecode_oracle_free(&oracle);
    kdecode_oracle_free(&format);
    free(run.blob);
    free(run.text);
    free(run.file);
    free(run.row);
    free(run.held);
    free(run.bytes);
    return read && !run.failed;
}
