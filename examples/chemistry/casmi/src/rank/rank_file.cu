// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_file.cu: a parquet file read for the ranker, every row group in order, its pages decoded by the engine's own
// codecs through the CASMI parquet reader. Each double preserves the 64 bits it is stored as; each text column's
// strings are merged over every row group of every file read into one table a column, each row's id read into it
#include "rank_internal.h"
#include "../../../../../includes/platform/platform.h"

#include "../parquet/parquet.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdlib.h>

// the double columns' paths, and the text columns' in RANK_TEXTS order
static const char *const RANK_PRECURSOR_PATH = "precursor_mz";
static const char *const RANK_MZ_PATH = "ms2_mzs.list.element";
static const char *const RANK_INTENSITY_PATH = "ms2_normalized_intensities.list.element";
static const char *const RANK_TEXT_PATH[RANK_TEXTS] = {
    "ingest_lib", "ionization_mode", "adduct", "molecular_formula", "inchikey14", "normalized_smiles", "molecule_id"};

static CasmiParquetFooter s_footer;

void rank_line_decimal(SimResults *results, const char *before, unsigned long long value)
{
    mmgr_line_put(&results->line, before);
    mmgr_line_decimal(&results->line, value, 1u);
}

void rank_line_end(SimResults *results)
{
    mmgr_line_ch(&results->line, '\n');
    sim_flush(results);
}

void rank_error_line(SimResults *results, const char *what, const EngineError *error)
{
    mmgr_line_put(&results->line, "  ");
    mmgr_line_put(&results->line, what);
    rank_line_decimal(results, ": module ", (unsigned long long)error->module);
    rank_line_decimal(results, ", site ", (unsigned long long)error->site);
    rank_line_decimal(results, ", type ", (unsigned long long)error->type);
    rank_line_end(results);
}

// one leaf of one row group decoded: its values and row r's from row_start[r] up to row_start[r + 1]; a text leaf's
// value v is value_length[v] bytes at value_bytes + value_offset[v]
static int rank_leaf_read(const char *path, unsigned int group, long long leaf, CasmiParquetColumn *column)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = column, .bytes = sizeof(*column), .val = 0);
    // the leaf is below CASMI_PARQUET_LEAVES_MAX
    const CasmiParquetColumnRead read = {path, &s_footer, group, (unsigned int)leaf, column};
    int ok = casmi_parquet_column_bound(&read) >= 0ll;
    column->row_start = ok ? (unsigned long long *)platform_cell_taken((size_t)(column->row_start_room * 8ull) + 8u,
                                                                       platform_place_g_program_owner, 0)
                           : NULL;
    column->value_bytes = ok ? (unsigned char *)platform_cell_taken((size_t)column->value_bytes_room + 8u,
                                                                    platform_place_g_program_owner, 0)
                             : NULL;
    column->value_offset = ok ? (unsigned long long *)platform_cell_taken(
                                    (size_t)(column->value_offset_room * 8ull) + 8u, platform_place_g_program_owner, 0)
                              : NULL;
    column->value_length = ok ? (unsigned long long *)platform_cell_taken(
                                    (size_t)(column->value_offset_room * 8ull) + 8u, platform_place_g_program_owner, 0)
                              : NULL;
    column->scratch =
        ok ? (unsigned char *)platform_cell_taken((size_t)column->scratch_room + 8u, platform_place_g_program_owner, 0)
           : NULL;
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
    platform_cell_released(column->row_start, platform_place_g_program_owner);
    platform_cell_released(column->value_bytes, platform_place_g_program_owner);
    platform_cell_released(column->value_offset, platform_place_g_program_owner);
    platform_cell_released(column->value_length, platform_place_g_program_owner);
    platform_cell_released(column->scratch, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = column, .bytes = sizeof(*column), .val = 0);
}

// ---- the ranker's cells ----

// the cells the pool refused the ranker
static unsigned int s_rank_refused;

void *rank_cell(unsigned long long count, unsigned long long size, int ephemeral)
{
    void *const cell =
        platform_cell_cleared((count != 0ull) ? count : 1ull, size, platform_place_g_program_owner, ephemeral);
    s_rank_refused += (cell == NULL) ? 1u : 0u;
    return cell;
}

int rank_host_room(void **cell, unsigned long long *room, unsigned long long count, unsigned long long held,
                   unsigned long long size)
{
    if ((*cell != NULL) && (count <= *room))
    {
        return 1;
    }
    const unsigned long long doubled = 2ull * *room;
    const unsigned long long wanted = (count > doubled) ? count : doubled;
    const unsigned long long grown_room = (wanted != 0ull) ? wanted : 1ull;
    void *const grown =
        (*cell == NULL) ? platform_cell_taken(grown_room * size, platform_place_g_program_owner, 0)
                        : platform_cell_grown(*cell, held * size, grown_room * size, platform_place_g_program_owner, 0);
    if (grown == NULL)
    {
        s_rank_refused += 1u;
        return 0;
    }
    *cell = grown;
    *room = grown_room;
    return 1;
}

int rank_cells_refused(SimResults *results)
{
    if (s_rank_refused == 0u)
    {
        return 0;
    }
    rank_line_decimal(results, "  the pool did not meet ", (unsigned long long)s_rank_refused);
    mmgr_line_put(&results->line, " cells of the ranker: run with a larger g_program_pool_bytes");
    rank_line_end(results);
    return 1;
}

void rank_cell_released(void *cell)
{
    platform_cell_released(cell, platform_place_g_program_owner);
}

// ---- the set ----

// A string of a row as the load reads it: `length` bytes at `offset` of its column's bytes, and its row. The settle
// sorts these by their bytes, and the first row of each run of equal bytes gives the string its id
typedef struct
{
    unsigned long long offset;
    unsigned int length;
    unsigned int row;
} RankEntry;

// a run of equal strings among the settled entries: its first row, its first entry and its entries, and its place in
// the order of the bytes
typedef struct
{
    unsigned int first_row;
    unsigned int sorted_at;
    unsigned long long start;
    unsigned long long count;
} RankRun;

// The load's work on the temporary tier: each text column's bytes, the row groups' bytes put one after the other,
// `bytes_used` of `bytes_room`, and its entries, `entry_count` of them
typedef struct
{
    unsigned char *bytes[RANK_TEXTS];
    unsigned long long bytes_used[RANK_TEXTS];
    unsigned long long bytes_room[RANK_TEXTS];
    RankEntry *entries[RANK_TEXTS];
    unsigned long long entry_count[RANK_TEXTS];
} RankLoad;

// the bounds of the set's cells over every file: the rows, each double column's values, and each text column's bytes
typedef struct
{
    unsigned long long rows;
    unsigned long long values[3];
    unsigned long long bytes[RANK_TEXTS];
} RankBounds;

// the bytes the entries of the settle point into, for the orders of memor.sort
static const unsigned char *s_settled_bytes;

// the order of two strings by their bytes, and the shorter first where one begins the other
static int rank_bytes_order(const unsigned char *left, unsigned long long left_length, const unsigned char *right,
                            unsigned long long right_length)
{
    const unsigned long long shared = (left_length < right_length) ? left_length : right_length;
    const embed_iword compared =
        (shared != 0ull) ? EMBED_CALL(memor.cmp, MemoriaCfg, .src = left, .other = right, .bytes = (size_t)shared) : 0;
    return (compared != 0) ? ((compared < 0) ? -1 : 1)
                           : ((left_length < right_length) ? -1 : ((left_length > right_length) ? 1 : 0));
}

static int rank_entry_order(const void *left, const void *right)
{
    const RankEntry *const one = (const RankEntry *)left;
    const RankEntry *const other = (const RankEntry *)right;
    return rank_bytes_order(s_settled_bytes + one->offset, one->length, s_settled_bytes + other->offset, other->length);
}

static int rank_run_order(const void *left, const void *right)
{
    const RankRun *const one = (const RankRun *)left;
    const RankRun *const other = (const RankRun *)right;
    return (one->first_row < other->first_row) ? -1 : ((one->first_row > other->first_row) ? 1 : 0);
}

// a column's leaf in the footer read last, -1 where the file has none of that physical type
static long long rank_leaf_of(const char *leaf_path, CasmiParquetPhysical physical)
{
    const CasmiParquetLeafFind find = {&s_footer, leaf_path};
    const long long leaf = casmi_parquet_leaf_find(&find);
    return ((leaf >= 0ll) && (s_footer.leaf[leaf].physical == physical)) ? leaf : -1ll;
}

// the rooms a leaf of a row group takes, from the footer read last alone
static CasmiParquetColumn rank_leaf_bound(const char *path, unsigned int group, long long leaf)
{
    CasmiParquetColumn column;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &column, .bytes = sizeof(column), .val = 0);
    // the leaf is below CASMI_PARQUET_LEAVES_MAX
    const CasmiParquetColumnRead read = {path, &s_footer, group, (unsigned int)leaf, &column};
    if (casmi_parquet_column_bound(&read) < 0ll)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &column, .bytes = sizeof(column), .val = 0);
    }
    return column;
}

static int rank_footer_read(SimResults *results, const char *path)
{
    const CasmiParquetFooterRead footer = {path, &s_footer};
    if (casmi_parquet_footer_read(&footer) < 0ll)
    {
        mmgr_line_put(&results->line, "  the parquet footer did not read: ");
        mmgr_line_put(&results->line, path);
        rank_line_end(results);
        return 0;
    }
    return 1;
}

// the first pass: each file's footer read and its row groups' rooms summed. 0 where a footer did not read
static int rank_set_bounded(SimResults *results, const char *const *paths, unsigned int files, RankBounds *bounds)
{
    static const char *const s_double_path[3] = {RANK_PRECURSOR_PATH, RANK_MZ_PATH, RANK_INTENSITY_PATH};
    EMBED_CALL(memor.set, MemoriaCfg, .dst = bounds, .bytes = sizeof(*bounds), .val = 0);
    for (unsigned int file = 0u; file < files; file += 1u)
    {
        if (!rank_footer_read(results, paths[file]))
        {
            return 0;
        }
        for (unsigned int group = 0u; group < s_footer.row_group_count; group += 1u)
        {
            bounds->rows += s_footer.row_group[group].rows;
            for (unsigned int column = 0u; column < 3u; column += 1u)
            {
                const long long leaf = rank_leaf_of(s_double_path[column], CASMI_PARQUET_DOUBLE);
                bounds->values[column] +=
                    (leaf >= 0ll) ? (rank_leaf_bound(paths[file], group, leaf).value_bytes_room / 8ull) : 0ull;
            }
            for (unsigned int text = 0u; text < RANK_TEXTS; text += 1u)
            {
                const long long leaf = rank_leaf_of(RANK_TEXT_PATH[text], CASMI_PARQUET_BYTE_ARRAY);
                bounds->bytes[text] +=
                    (leaf >= 0ll) ? rank_leaf_bound(paths[file], group, leaf).value_bytes_room : 0ull;
            }
        }
    }
    return 1;
}

// a double column's row group appended: each value's stored bits and each row's start; 0 where the leaf is not a
// double, did not read, or passes the column's room
static int rank_double_load(const char *path, unsigned int group, const char *leaf_path, RankColumn *out,
                            unsigned long long *rows)
{
    const long long leaf = rank_leaf_of(leaf_path, CASMI_PARQUET_DOUBLE);
    CasmiParquetColumn column;
    const int read = (leaf >= 0ll) && rank_leaf_read(path, group, leaf, &column);
    if (!read)
    {
        return 0;
    }
    const unsigned long long base = out->values;
    const int fits = ((base + column.values) <= out->values_room) && ((out->rows + column.rows) <= out->rows_room);
    if (fits)
    {
        // the values put block by block, each copy up to the end of the block it starts in
        unsigned long long copied = 0ull;
        while (copied < column.values)
        {
            const unsigned long long at = base + copied;
            const unsigned long long room = RANK_BLOCK_VALUES - (at & (RANK_BLOCK_VALUES - 1ull));
            const unsigned long long piece = ((column.values - copied) < room) ? (column.values - copied) : room;
            EMBED_CALL(memor.cpy, MemoriaCfg,
                       .dst = &out->block[at >> RANK_BLOCK_SHIFT][at & (RANK_BLOCK_VALUES - 1ull)],
                       .src = column.value_bytes + (copied * 8ull), .bytes = (size_t)(piece * 8ull));
            copied += piece;
        }
        for (unsigned long long row = 0ull; row < column.rows; row += 1ull)
        {
            out->row_start[out->rows + row + 1ull] = base + column.row_start[row + 1ull];
        }
        out->values = base + column.values;
        out->rows += column.rows;
        *rows = column.rows;
    }
    rank_leaf_release(&column);
    return fits;
}

// A text column's row group: its bytes put after the bytes before it, and an entry for each row that has a string,
// RANK_ABSENT for a row with none; a file without the column gives every row RANK_ABSENT. The rows start at `first`
static int rank_text_load(const char *path, unsigned int group, unsigned int text, unsigned long long first,
                          unsigned long long rows, RankLoad *load, RankSet *out)
{
    const long long leaf = rank_leaf_of(RANK_TEXT_PATH[text], CASMI_PARQUET_BYTE_ARRAY);
    if (leaf < 0ll)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &out->text[text][first],
                   .bytes = (size_t)(rows * sizeof(unsigned int)), .val = 0xFF);
        return 1;
    }
    CasmiParquetColumn column;
    int ok = rank_leaf_read(path, group, leaf, &column) && (column.rows == rows);
    // the bytes the group's values reach to, from each value's place and length
    unsigned long long reach = 0ull;
    for (unsigned long long value = 0ull; ok && (value < column.values); value += 1ull)
    {
        const unsigned long long end = column.value_offset[value] + column.value_length[value];
        reach = (end > reach) ? end : reach;
    }
    const unsigned long long base = load->bytes_used[text];
    // the double columns of the group passed their rooms of rows first, and so the ids and the entries fit theirs
    ok = ok && ((base + reach) <= load->bytes_room[text]);
    if (ok && (reach != 0ull))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = load->bytes[text] + base, .src = column.value_bytes,
                   .bytes = (size_t)reach);
    }
    for (unsigned long long row = 0ull; ok && (row < rows); row += 1ull)
    {
        out->text[text][first + row] = RANK_ABSENT;
        if (column.row_start[row + 1ull] != column.row_start[row])
        {
            const unsigned long long value = column.row_start[row];
            // a row number of a set is below 2^32, as its ids are, and a string's length is below 2^32
            load->entries[text][load->entry_count[text]] =
                RankEntry{base + column.value_offset[value], (unsigned int)column.value_length[value],
                          (unsigned int)(first + row)};
            load->entry_count[text] += 1ull;
        }
    }
    load->bytes_used[text] = ok ? (base + reach) : base;
    rank_leaf_release(&column);
    return ok;
}

// The strings of one text column settled: the entries sorted by their bytes, each run of equal bytes one string, the
// strings given their ids in the order of their first rows, each row's id written, and the strings' order by their
// bytes kept for the search. 0 where the pool cannot meet a cell
static int rank_strings_settled(RankLoad *load, unsigned int text, RankSet *out)
{
    RankEntry *const entries = load->entries[text];
    const unsigned long long count = load->entry_count[text];
    RankStrings *const strings = &out->strings[text];
    RankEntry *const entry_room = (RankEntry *)rank_cell(count, sizeof(RankEntry), 1);
    if (entry_room == NULL)
    {
        return 0;
    }
    s_settled_bytes = load->bytes[text];
    EMBED_CALL(memor.sort, MemoriaCfg, .dst = entries, .bytes = sizeof(RankEntry), .scratch = entry_room,
               .count = count, .order = rank_entry_order);
    rank_cell_released(entry_room);
    unsigned long long runs = 0ull;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        runs += ((at == 0ull) || (rank_entry_order(&entries[at - 1ull], &entries[at]) != 0)) ? 1ull : 0ull;
    }
    RankRun *const run = (RankRun *)rank_cell(runs, sizeof(RankRun), 1);
    RankRun *const run_room = (RankRun *)rank_cell(runs, sizeof(RankRun), 1);
    if ((run == NULL) || (run_room == NULL))
    {
        rank_cell_released(run_room);
        rank_cell_released(run);
        return 0;
    }
    unsigned long long bytes = 0ull;
    unsigned long long made = 0ull;
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        if ((at == 0ull) || (rank_entry_order(&entries[at - 1ull], &entries[at]) != 0))
        {
            // the stable sort keeps a run's entries in the order of their rows, and so its first entry has its first
            // row
            run[made] = RankRun{entries[at].row, (unsigned int)made, at, 0ull};
            bytes += entries[at].length + 1ull;
            made += 1ull;
        }
        run[made - 1ull].count += 1ull;
    }
    EMBED_CALL(memor.sort, MemoriaCfg, .dst = run, .bytes = sizeof(RankRun), .scratch = run_room, .count = runs,
               .order = rank_run_order);
    rank_cell_released(run_room);
    strings->bytes = (char *)rank_cell(bytes, 1u, 0);
    strings->offset = (unsigned long long *)rank_cell(runs + 1ull, sizeof(unsigned long long), 0);
    strings->order = (unsigned int *)rank_cell(runs, sizeof(unsigned int), 0);
    const int taken = (strings->bytes != NULL) && (strings->offset != NULL) && (strings->order != NULL);
    unsigned long long at_byte = 0ull;
    for (unsigned long long id = 0ull; taken && (id < runs); id += 1ull)
    {
        const RankEntry *const first = &entries[run[id].start];
        strings->offset[id] = at_byte;
        if (first->length != 0u)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = strings->bytes + at_byte, .src = load->bytes[text] + first->offset,
                       .bytes = (size_t)first->length);
        }
        at_byte += first->length + 1ull;
        strings->order[run[id].sorted_at] = (unsigned int)id;
        for (unsigned long long each = 0ull; each < run[id].count; each += 1ull)
        {
            out->text[text][entries[run[id].start + each].row] = (unsigned int)id;
        }
    }
    if (taken)
    {
        strings->offset[runs] = at_byte;
        strings->count = (unsigned int)runs;
    }
    rank_cell_released(run);
    return taken;
}

// the load's work given back
static void rank_load_released(RankLoad *load)
{
    for (unsigned int text = 0u; text < RANK_TEXTS; text += 1u)
    {
        rank_cell_released(load->entries[text]);
        rank_cell_released(load->bytes[text]);
    }
}

// the second pass over one file: every row group read into the set's cells in order. 0 where a row group did not read
static int rank_file_filled(SimResults *results, const char *path, RankLoad *load, RankSet *out)
{
    if (!rank_footer_read(results, path))
    {
        return 0;
    }
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
            ok = rank_text_load(path, group, text, first, rows, load, out);
        }
        out->spectra += ok ? rows : 0ull;
        out->row_groups += ok ? 1u : 0u;
        if (!ok)
        {
            rank_line_decimal(results, "  row group ", group);
            mmgr_line_put(&results->line,
                          " did not read, or a row is not one precursor and as many intensities as m/z: ");
            mmgr_line_put(&results->line, path);
            rank_line_end(results);
        }
    }
    return ok;
}

int rank_set_load(SimResults *results, const char *const *paths, unsigned int files, RankSet *out)
{
    RankBounds bounds;
    if ((files == 0u) || (files > RANK_FILES) || !rank_set_bounded(results, paths, files, &bounds))
    {
        return 0;
    }
    // the set's cells on the persistent tier, which the process keeps; the load's work on the temporary tier
    RankColumn *const column[3] = {&out->precursor, &out->mz, &out->intensity};
    for (unsigned int each = 0u; each < 3u; each += 1u)
    {
        column[each]->values_room = bounds.values[each];
        column[each]->rows_room = bounds.rows;
        // the blocks, each RANK_BLOCK_VALUES values but the last, which holds the rest
        const unsigned long long blocks = (bounds.values[each] + RANK_BLOCK_VALUES - 1ull) >> RANK_BLOCK_SHIFT;
        if (blocks > RANK_BLOCKS_MAX)
        {
            rank_line_decimal(results, "  a double column holds more than its blocks can: values ",
                              bounds.values[each]);
            rank_line_end(results);
            return 0;
        }
        column[each]->blocks = (unsigned int)blocks;
        for (unsigned long long block = 0ull; block < blocks; block += 1ull)
        {
            const unsigned long long left = bounds.values[each] - (block << RANK_BLOCK_SHIFT);
            column[each]->block[block] = (unsigned long long *)rank_cell(
                (left < RANK_BLOCK_VALUES) ? left : RANK_BLOCK_VALUES, sizeof(unsigned long long), 0);
        }
        column[each]->row_start = (unsigned long long *)rank_cell(bounds.rows + 1ull, sizeof(unsigned long long), 0);
    }
    RankLoad load;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &load, .bytes = sizeof(load), .val = 0);
    for (unsigned int text = 0u; text < RANK_TEXTS; text += 1u)
    {
        out->text[text] = (unsigned int *)rank_cell(bounds.rows, sizeof(unsigned int), 0);
        load.bytes_room[text] = bounds.bytes[text];
        load.bytes[text] = (unsigned char *)rank_cell(bounds.bytes[text], 1u, 1);
        load.entries[text] = (RankEntry *)rank_cell(bounds.rows, sizeof(RankEntry), 1);
    }
    if (rank_cells_refused(results))
    {
        rank_load_released(&load);
        return 0;
    }
    int ok = 1;
    for (unsigned int file = 0u; ok && (file < files); file += 1u)
    {
        const unsigned long long spectra_before = out->spectra;
        ok = rank_file_filled(results, paths[file], &load, out) && (out->spectra != spectra_before);
        out->file_spectra[file] = out->spectra;
    }
    for (unsigned int text = 0u; ok && (text < RANK_TEXTS); text += 1u)
    {
        ok = rank_strings_settled(&load, text, out);
    }
    rank_load_released(&load);
    return ok && !rank_cells_refused(results);
}

const char *rank_string(const RankSet *set, unsigned int text, unsigned int id)
{
    return set->strings[text].bytes + set->strings[text].offset[id];
}

unsigned long long rank_string_length(const RankSet *set, unsigned int text, unsigned int id)
{
    return set->strings[text].offset[id + 1u] - set->strings[text].offset[id] - 1ull;
}

unsigned int rank_string_find(const RankSet *set, unsigned int text, const char *wanted)
{
    const RankStrings *const strings = &set->strings[text];
    const unsigned long long wanted_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = wanted, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    unsigned long long low = 0ull;
    unsigned long long high = strings->count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        const unsigned int id = strings->order[middle];
        const int order =
            rank_bytes_order((const unsigned char *)rank_string(set, text, id), rank_string_length(set, text, id),
                             (const unsigned char *)wanted, wanted_length);
        if (order == 0)
        {
            return id;
        }
        low = (order < 0) ? (middle + 1ull) : low;
        high = (order < 0) ? high : middle;
    }
    return RANK_ABSENT;
}
