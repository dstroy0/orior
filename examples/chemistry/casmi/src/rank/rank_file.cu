// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_file.cu: a parquet file read for the ranker, every row group in order, its pages decoded by the engine's own
// codecs through the CASMI parquet reader. Each double keeps the 64 bits it is stored as; each text column's strings
// are merged over every row group of every file read into one table a column, each row's id read into it
#include "rank_internal.h"

#include "../parquet/parquet.h"

#include <stdlib.h>
#include <string.h>

// the double columns' paths, and the text columns' in RANK_TEXTS order
static const char *const RANK_PRECURSOR_PATH = "precursor_mz";
static const char *const RANK_MZ_PATH = "ms2_mzs.list.element";
static const char *const RANK_INTENSITY_PATH = "ms2_normalized_intensities.list.element";
static const char *const RANK_TEXT_PATH[RANK_TEXTS] = {"ingest_lib",        "ionization_mode", "adduct",
                                                       "molecular_formula", "inchikey14",      "normalized_smiles",
                                                       "molecule_id"};

static CasmiParquetFooter s_footer;

void rank_line_decimal(SimResults *results, const char *before, unsigned long long value)
{
    scriptura_text(&results->line, before);
    scriptura_decimal(&results->line, value, 1u);
}

void rank_line_end(SimResults *results)
{
    scriptura_character(&results->line, '\n');
    sim_flush(results);
}

void rank_error_line(SimResults *results, const char *what, const EngineError *error)
{
    scriptura_text(&results->line, "  ");
    scriptura_text(&results->line, what);
    rank_line_decimal(results, ": module ", (unsigned long long)error->module);
    rank_line_decimal(results, ", site ", (unsigned long long)error->site);
    rank_line_decimal(results, ", kind ", (unsigned long long)error->kind);
    rank_line_end(results);
}

// one leaf of one row group decoded: its values and row r's from row_start[r] up to row_start[r + 1]; a text leaf's
// value v is value_length[v] bytes at value_bytes + value_offset[v]
static int rank_leaf_read(const char *path, unsigned int group, long long leaf, CasmiParquetColumn *column)
{
    memset(column, 0, sizeof(*column));
    // the leaf is below CASMI_PARQUET_LEAVES_MOST
    const CasmiParquetColumnRead read = {path, &s_footer, group, (unsigned int)leaf, column};
    int ok = casmi_parquet_column_bound(&read) >= 0ll;
    column->row_start = ok ? (unsigned long long *)malloc((size_t)(column->row_start_room * 8ull) + 8u) : NULL;
    column->value_bytes = ok ? (unsigned char *)malloc((size_t)column->value_bytes_room + 8u) : NULL;
    column->value_offset = ok ? (unsigned long long *)malloc((size_t)(column->value_offset_room * 8ull) + 8u) : NULL;
    column->value_length = ok ? (unsigned long long *)malloc((size_t)(column->value_offset_room * 8ull) + 8u) : NULL;
    column->scratch = ok ? (unsigned char *)malloc((size_t)column->scratch_room + 8u) : NULL;
    ok = ok && (column->row_start != NULL) && (column->value_bytes != NULL) && (column->value_offset != NULL) &&
         (column->value_length != NULL) && (column->scratch != NULL) && (casmi_parquet_column_read(&read) >= 0ll);
    if (ok)
    {
        column->row_start[column->rows] = column->values;
    }
    return ok;
}

static void rank_leaf_release(CasmiParquetColumn *column)
{
    free(column->row_start);
    free(column->value_bytes);
    free(column->value_offset);
    free(column->value_length);
    free(column->scratch);
    memset(column, 0, sizeof(*column));
}

// a double column's row group appended: each value's stored bits and each row's start; 0 where the leaf is not a
// double or did not read
static int rank_double_load(const char *path, unsigned int group, const char *leaf_path, RankColumn *out,
                            unsigned long long *rows)
{
    const CasmiParquetLeafFind find = {&s_footer, leaf_path};
    const long long leaf = casmi_parquet_leaf_find(&find);
    CasmiParquetColumn column;
    const int ok = (leaf >= 0ll) && (s_footer.leaf[leaf].physical == CASMI_PARQUET_DOUBLE) &&
                   rank_leaf_read(path, group, leaf, &column);
    if (!ok)
    {
        return 0;
    }
    const unsigned long long base = out->bits.size();
    out->bits.resize((size_t)(base + column.values));
    memcpy(&out->bits[base], column.value_bytes, (size_t)(column.values * 8ull));
    if (out->row_start.empty())
    {
        out->row_start.push_back(0ull);
    }
    for (unsigned long long row = 0ull; row < column.rows; row += 1ull)
    {
        out->row_start.push_back(base + column.row_start[row + 1ull]);
    }
    *rows = column.rows;
    rank_leaf_release(&column);
    return 1;
}

// A text column's row group: its strings merged into the table and each row's id appended, RANK_ABSENT for a row with
// none; a file without the column gives every row RANK_ABSENT
static int rank_text_load(const char *path, unsigned int group, unsigned int text, unsigned long long rows,
                          RankSet *out)
{
    const CasmiParquetLeafFind find = {&s_footer, RANK_TEXT_PATH[text]};
    const long long leaf = casmi_parquet_leaf_find(&find);
    if ((leaf < 0ll) || (s_footer.leaf[leaf].physical != CASMI_PARQUET_BYTE_ARRAY))
    {
        out->text[text].insert(out->text[text].end(), (size_t)rows, RANK_ABSENT);
        return 1;
    }
    CasmiParquetColumn column;
    const int ok = rank_leaf_read(path, group, leaf, &column) && (column.rows == rows);
    for (unsigned long long row = 0ull; ok && (row < rows); row += 1ull)
    {
        if (column.row_start[row + 1ull] == column.row_start[row])
        {
            out->text[text].push_back(RANK_ABSENT);
            continue;
        }
        const unsigned long long value = column.row_start[row];
        const std::string held((const char *)(column.value_bytes + column.value_offset[value]),
                               (size_t)column.value_length[value]);
        const auto found = out->table[text].find(held);
        if (found != out->table[text].end())
        {
            out->text[text].push_back(found->second);
        }
        else
        {
            const unsigned int named = (unsigned int)out->strings[text].size();
            out->table[text].emplace(held, named);
            out->strings[text].push_back(held);
            out->text[text].push_back(named);
        }
    }
    rank_leaf_release(&column);
    return ok;
}

int rank_file_load(SimResults *results, const char *path, RankSet *out)
{
    const CasmiParquetFooterRead footer = {path, &s_footer};
    if (casmi_parquet_footer_read(&footer) < 0ll)
    {
        scriptura_text(&results->line, "  the parquet footer did not read: ");
        scriptura_text(&results->line, path);
        rank_line_end(results);
        return 0;
    }
    const unsigned long long spectra_before = out->spectra;
    int ok = 1;
    for (unsigned int group = 0u; ok && (group < s_footer.row_group_count); group += 1u)
    {
        // each row one spectrum: its one precursor, and its peaks' m/z and intensities, as many of each
        unsigned long long rows = 0ull;
        unsigned long long mz_rows = 0ull;
        unsigned long long intensity_rows = 0ull;
        const unsigned long long first = out->spectra;
        ok = rank_double_load(path, group, RANK_PRECURSOR_PATH, &out->precursor, &rows) &&
             rank_double_load(path, group, RANK_MZ_PATH, &out->mz, &mz_rows) &&
             rank_double_load(path, group, RANK_INTENSITY_PATH, &out->intensity, &intensity_rows) &&
             (mz_rows == rows) && (intensity_rows == rows);
        for (unsigned long long row = first; ok && (row < (first + rows)); row += 1ull)
        {
            ok = ((out->precursor.row_start[row + 1ull] - out->precursor.row_start[row]) == 1ull) &&
                 ((out->mz.row_start[row + 1ull] - out->mz.row_start[row]) ==
                  (out->intensity.row_start[row + 1ull] - out->intensity.row_start[row]));
        }
        for (unsigned int text = 0u; ok && (text < RANK_TEXTS); text += 1u)
        {
            ok = rank_text_load(path, group, text, rows, out);
        }
        out->spectra += ok ? rows : 0ull;
        out->row_groups += ok ? 1u : 0u;
        if (!ok)
        {
            rank_line_decimal(results, "  row group ", group);
            scriptura_text(&results->line, " did not read, or a row is not one precursor and as many intensities as m/z: ");
            scriptura_text(&results->line, path);
            rank_line_end(results);
        }
    }
    return ok && (out->spectra != spectra_before);
}
