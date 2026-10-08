// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_set.cu: a set the ingest sealed, read back for the ranker. Each row group is read in order, a directory of the
// set whose crystals engine_set_samples names: the double columns' planes are joined into each value's integer, its
// form, each row's term and the kept values' stored bits; the text columns' distinct strings are merged over every row
// group into one table a column, each row's index read into it
#include "rank_internal.h"

#include "../../../../../src/cu/engine/engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unordered_map>

// the parts' paths the ingest names, and the text columns' in RANK_TEXTS order
static const char *const RANK_PRECURSOR_PATH = "precursor_mz";
static const char *const RANK_MZ_PATH = "ms2_mzs.list.element";
static const char *const RANK_INTENSITY_PATH = "ms2_normalized_intensities.list.element";
static const char *const RANK_BASE_PATH = "base_peak_intensity";
static const char *const RANK_TEXT_PATH[RANK_TEXTS] = {"ingest_lib",        "ionization_mode", "adduct",
                                                       "molecular_formula", "inchikey14",      "normalized_smiles",
                                                       "molecule_id"};

// an integer's planes, its form, its kept values' planes and the bits a plane holds
#define RANK_PLANES 4u
#define RANK_KEPT_PLANES 5u
#define RANK_PLANE_BITS 15u
#define RANK_SAMPLE_BYTES 256u

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

// one row group's samples, each the part's name past the row group's prefix: the set's samples as engine_set_samples
// names them, row_group_N.column.part
typedef struct
{
    std::vector<std::string> names;
    unsigned int count;
} RankGroupSamples;

static int rank_group_has(const RankGroupSamples *samples, const char *name)
{
    for (unsigned int each = 0u; each < samples->count; each += 1u)
    {
        if (samples->names[each] == name)
        {
            return 1;
        }
    }
    return 0;
}

// One part's crystal loaded: its lanes and extent, the part named `column`.`part` in row group `group`; 0 where it did
// not load, the error printed
static int rank_part_load(SimResults *results, const char *set, unsigned int group, const char *column, const char *part,
                          unsigned long long extent[4], unsigned short **lanes)
{
    char sample[RANK_SAMPLE_BYTES];
    const int named = snprintf(sample, sizeof(sample), "row_group_%u.%s.%s", group, column, part);
    EngineSignum root;
    EngineError error;
    memset(&error, 0, sizeof(error));
    *lanes = NULL;
    const int loaded = (named > 0) && (named < (int)sizeof(sample)) &&
                       (engine_iapx_load(set, sample, extent, lanes, &root, NULL, &error) == 0L);
    if (!loaded)
    {
        scriptura_text(&results->line, "  ");
        scriptura_text(&results->line, sample);
        rank_error_line(results, "did not load", &error);
    }
    return loaded;
}

// A double column's row group appended: its planes joined into each value's integer, its form, the 64 stored bits of
// each kept value, each row's term, ROW_EACH where the column holds none, and each row's start from `starts`
static int rank_column_load(SimResults *results, const char *set, unsigned int group, const RankGroupSamples *samples,
                            const char *path, unsigned int first_form, const std::vector<unsigned long long> &counts,
                            RankColumn *column)
{
    unsigned long long values = 0ull;
    for (unsigned long long row = 0ull; row < counts.size(); row += 1ull)
    {
        values += counts[row];
    }
    const unsigned long long base = column->unit.size();
    column->unit.resize(base + values, 0ull);
    column->form.resize(base + values, (unsigned char)first_form);
    column->kept.resize(base + values, 0ull);
    if (column->row_start.empty())
    {
        column->row_start.push_back(0ull);
    }
    for (unsigned long long row = 0ull; row < counts.size(); row += 1ull)
    {
        column->row_start.push_back(column->row_start.back() + counts[row]);
    }
    char part[RANK_SAMPLE_BYTES];
    int ok = 1;
    for (unsigned int plane = 0u; ok && (plane < RANK_PLANES); plane += 1u)
    {
        snprintf(part, sizeof(part), "%s.unit%u", path, plane);
        if (!rank_group_has(samples, part))
        {
            continue;
        }
        unsigned long long extent[4];
        unsigned short *lanes = NULL;
        snprintf(part, sizeof(part), "unit%u", plane);
        ok = rank_part_load(results, set, group, path, part, extent, &lanes) && (extent[3] == values);
        for (unsigned long long value = 0ull; ok && (value < values); value += 1ull)
        {
            column->unit[base + value] |= (unsigned long long)lanes[value] << (RANK_PLANE_BITS * plane);
        }
        free(lanes);
    }
    snprintf(part, sizeof(part), "%s.form", path);
    if (ok && rank_group_has(samples, part))
    {
        unsigned long long extent[4];
        unsigned short *lanes = NULL;
        ok = rank_part_load(results, set, group, path, "form", extent, &lanes) && (extent[3] == values);
        for (unsigned long long value = 0ull; ok && (value < values); value += 1ull)
        {
            column->form[base + value] = (unsigned char)lanes[value];
        }
        free(lanes);
    }
    snprintf(part, sizeof(part), "%s.kept", path);
    if (ok && rank_group_has(samples, part))
    {
        unsigned long long extent[4];
        unsigned short *lanes = NULL;
        ok = rank_part_load(results, set, group, path, "kept", extent, &lanes) && (extent[2] == RANK_KEPT_PLANES);
        const unsigned long long kept = ok ? extent[3] : 0ull;
        unsigned long long at = 0ull;
        for (unsigned long long value = 0ull; ok && (value < values); value += 1ull)
        {
            if (column->form[base + value] != RANK_FORM_KEPT)
            {
                continue;
            }
            ok = at < kept;
            unsigned long long stored = 0ull;
            for (unsigned int plane = RANK_KEPT_PLANES; ok && (plane > 0u); plane -= 1u)
            {
                stored = (stored << RANK_PLANE_BITS) | lanes[((plane - 1u) * kept) + at];
            }
            column->kept[base + value] = stored;
            at += 1ull;
        }
        ok = ok && (at == kept);
        free(lanes);
    }
    const unsigned long long rows = counts.size();
    snprintf(part, sizeof(part), "%s.row", path);
    const int termed = rank_group_has(samples, part);
    unsigned short *lanes = NULL;
    unsigned long long extent[4];
    ok = ok && (!termed || (rank_part_load(results, set, group, path, "row", extent, &lanes) && (extent[3] == rows)));
    for (unsigned long long row = 0ull; ok && (row < rows); row += 1ull)
    {
        column->term.push_back(termed ? lanes[row] : (unsigned short)RANK_ROW_EACH);
    }
    free(lanes);
    return ok;
}

// A text column's row group: its distinct strings merged into the set's table and each row's id appended, RANK_ABSENT
// for a row with none; a column the row group does not hold gives every row RANK_ABSENT
static int rank_text_load(SimResults *results, const char *set, unsigned int group, const RankGroupSamples *samples,
                          unsigned int text, unsigned long long rows, std::unordered_map<std::string, unsigned int> *table,
                          RankSet *out)
{
    const char *const path = RANK_TEXT_PATH[text];
    char part[RANK_SAMPLE_BYTES];
    snprintf(part, sizeof(part), "%s.index", path);
    if (!rank_group_has(samples, part))
    {
        out->text[text].insert(out->text[text].end(), (size_t)rows, RANK_ABSENT);
        return 1;
    }
    unsigned long long text_extent[4];
    unsigned long long length_extent[4];
    unsigned long long index_extent[4];
    unsigned short *bytes = NULL;
    unsigned short *lengths = NULL;
    unsigned short *index = NULL;
    int ok = rank_part_load(results, set, group, path, "text", text_extent, &bytes) &&
             rank_part_load(results, set, group, path, "lengths", length_extent, &lengths) &&
             rank_part_load(results, set, group, path, "index", index_extent, &index) && (index_extent[3] == rows);
    // each distinct string of the row group, its id in the set's table
    std::vector<unsigned int> id;
    const unsigned char *const text_bytes = (const unsigned char *)bytes;
    unsigned long long at = 0ull;
    for (unsigned long long each = 0ull; ok && (each < length_extent[3]); each += 1ull)
    {
        const std::string held((const char *)(text_bytes + at), (size_t)lengths[each]);
        at += lengths[each];
        const auto found = table->find(held);
        if (found != table->end())
        {
            id.push_back(found->second);
        }
        else
        {
            const unsigned int named = (unsigned int)out->strings[text].size();
            table->emplace(held, named);
            out->strings[text].push_back(held);
            id.push_back(named);
        }
    }
    ok = ok && (at <= (text_extent[3] * 2ull));
    const int wide = ok && (index_extent[2] > 1ull);
    for (unsigned long long row = 0ull; ok && (row < rows); row += 1ull)
    {
        const unsigned long long named = index[row] | (wide ? ((unsigned long long)index[rows + row] << 16u) : 0ull);
        ok = named <= id.size();
        out->text[text].push_back((ok && (named != 0ull)) ? id[named - 1ull] : RANK_ABSENT);
    }
    free(bytes);
    free(lengths);
    free(index);
    return ok;
}

int rank_set_load(SimResults *results, const char *set, RankSet *out)
{
    out->spectra = 0ull;
    out->row_groups = 0u;
    std::unordered_map<std::string, unsigned int> table[RANK_TEXTS];
    char **listed = NULL;
    const unsigned int listed_count = engine_set_samples(set, &listed);
    int ok = 1;
    for (unsigned int group = 0u; ok; group += 1u)
    {
        char prefix[RANK_SAMPLE_BYTES];
        const int prefixed = snprintf(prefix, sizeof(prefix), "row_group_%u.", group);
        RankGroupSamples samples;
        samples.count = 0u;
        for (unsigned int each = 0u; (prefixed > 0) && (each < listed_count); each += 1u)
        {
            if (strncmp(listed[each], prefix, (size_t)prefixed) == 0)
            {
                samples.names.push_back(listed[each] + prefixed);
                samples.count += 1u;
            }
        }
        if (samples.count == 0u)
        {
            break;
        }
        // each row's peak count, and each row's base: present where its row count is 1
        unsigned long long extent[4];
        unsigned short *peaks = NULL;
        unsigned short *presence = NULL;
        unsigned short *base_lanes = NULL;
        ok = rank_part_load(results, set, group, RANK_MZ_PATH, "rows", extent, &peaks);
        const unsigned long long rows = ok ? extent[3] : 0ull;
        std::vector<unsigned long long> counts((size_t)rows);
        for (unsigned long long row = 0ull; ok && (row < rows); row += 1ull)
        {
            counts[row] = peaks[row];
        }
        free(peaks);
        char part[RANK_SAMPLE_BYTES];
        snprintf(part, sizeof(part), "%s.rows", RANK_BASE_PATH);
        const int based = ok && rank_group_has(&samples, part);
        unsigned long long base_extent[4] = {0ull, 0ull, 0ull, 0ull};
        ok = ok && (!based || (rank_part_load(results, set, group, RANK_BASE_PATH, "rows", extent, &presence) &&
                               (extent[3] == rows) &&
                               rank_part_load(results, set, group, RANK_BASE_PATH, "kept", base_extent, &base_lanes)));
        unsigned long long base_at = 0ull;
        for (unsigned long long row = 0ull; ok && (row < rows); row += 1ull)
        {
            const int held = based && (presence[row] != 0u);
            unsigned long long bits = 0ull;
            for (unsigned int lane = 0u; held && (lane < 4u); lane += 1u)
            {
                bits |= (unsigned long long)base_lanes[(lane * base_extent[3]) + base_at] << (16u * lane);
            }
            base_at += held ? 1ull : 0ull;
            out->base.push_back(bits);
            out->base_held.push_back((unsigned char)held);
        }
        free(presence);
        free(base_lanes);
        const std::vector<unsigned long long> ones((size_t)rows, 1ull);
        ok = ok &&
             rank_column_load(results, set, group, &samples, RANK_PRECURSOR_PATH, RANK_FORM_DECIMAL, ones,
                              &out->precursor) &&
             rank_column_load(results, set, group, &samples, RANK_MZ_PATH, RANK_FORM_DECIMAL, counts, &out->mz) &&
             rank_column_load(results, set, group, &samples, RANK_INTENSITY_PATH, RANK_FORM_COUNT, counts,
                              &out->intensity);
        for (unsigned int text = 0u; ok && (text < RANK_TEXTS); text += 1u)
        {
            ok = rank_text_load(results, set, group, &samples, text, rows, &table[text], out);
        }
        out->spectra += rows;
        out->row_groups += 1u;
    }
    for (unsigned int each = 0u; each < listed_count; each += 1u)
    {
        free(listed[each]);
    }
    free(listed);
    ok = ok && (out->spectra != 0ull);
    return ok;
}
