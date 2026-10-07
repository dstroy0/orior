// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// casmi_driver: the CASMI library and its ranking on the engine. --ingest reads a parquet file's double columns as the
// bytes they are stored as, each byte pair one 16-bit lane, and seals each column as a crystal of the set through
// engine_crystal_write, a value's four lanes to a row; the set is then proved and every crystal loaded back against the
// stored bytes. The run is one job on the device's tessera daemon.
#include "../../../../../src/cu/engine/analysis/compression/compression.h"
#include "../../../../../src/cu/engine/analysis/tower/tower.h"
#include "../../../../../src/cu/engine/engine.h"
#include "../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "../parquet/parquet.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stdlib.h>
#include <string.h>

// the columns sealed, each a sample of the set named by its column path
#define CASMI_DRIVER_COLUMNS 4u

static const char *const CASMI_DRIVER_COLUMN_PATH[CASMI_DRIVER_COLUMNS] = {
    "precursor_mz", "ms2_mzs.list.element", "ms2_normalized_intensities.list.element", "base_peak_intensity"};

// lanes of one double
#define CASMI_DRIVER_LANES_PER_VALUE 4ull

static CasmiParquetFooter s_footer;

typedef struct
{
    unsigned long long values;
    unsigned char *bytes;
} DriverColumn;

static void driver_line_decimal(SimResults *results, const char *before, unsigned long long value)
{
    scriptura_text(&results->line, before);
    scriptura_decimal(&results->line, value, 1u);
}

static void driver_line_end(SimResults *results)
{
    scriptura_character(&results->line, '\n');
    sim_flush(results);
}

static void driver_error_line(SimResults *results, const char *what, const EngineError *error)
{
    scriptura_text(&results->line, "  ");
    scriptura_text(&results->line, what);
    driver_line_decimal(results, ": module ", (unsigned long long)error->module);
    driver_line_decimal(results, ", site ", (unsigned long long)error->site);
    driver_line_decimal(results, ", kind ", (unsigned long long)error->kind);
    driver_line_end(results);
}

// every value of one double column over every row group, as the stored bytes, eight to a value
static int driver_column_read(const char *path, const char *leaf_path, DriverColumn *out)
{
    memset(out, 0, sizeof(*out));
    const CasmiParquetLeafFind find = {&s_footer, leaf_path};
    const long long leaf = casmi_parquet_leaf_find(&find);
    if ((leaf < 0ll) || (s_footer.leaf[leaf].physical != CASMI_PARQUET_DOUBLE))
    {
        return 0;
    }
    unsigned long long room = 0ull;
    for (unsigned int group = 0u; group < s_footer.row_group_count; group += 1u)
    {
        room += s_footer.row_group[group].chunk[leaf].level_count;
    }
    out->bytes = (unsigned char *)malloc((size_t)(room * 8ull) + 8u);
    int ok = out->bytes != NULL;
    for (unsigned int group = 0u; ok && (group < s_footer.row_group_count); group += 1u)
    {
        CasmiParquetColumn column;
        memset(&column, 0, sizeof(column));
        // the leaf is below CASMI_PARQUET_LEAVES_MOST
        const CasmiParquetColumnRead read = {path, &s_footer, group, (unsigned int)leaf, &column};
        ok = casmi_parquet_column_bound(&read) >= 0ll;
        column.row_start = ok ? (unsigned long long *)malloc((size_t)(column.row_start_room * 8ull) + 8u) : NULL;
        column.value_bytes = ok ? (unsigned char *)malloc((size_t)column.value_bytes_room + 8u) : NULL;
        column.value_offset = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
        column.value_length = ok ? (unsigned long long *)malloc((size_t)(column.value_offset_room * 8ull) + 8u) : NULL;
        column.scratch = ok ? (unsigned char *)malloc((size_t)column.scratch_room + 8u) : NULL;
        ok = ok && (column.row_start != NULL) && (column.value_bytes != NULL) && (column.value_offset != NULL) &&
             (column.value_length != NULL) && (column.scratch != NULL) && (casmi_parquet_column_read(&read) >= 0ll) &&
             ((out->values + column.values) <= room);
        if (ok)
        {
            memcpy(out->bytes + (out->values * 8ull), column.value_bytes, (size_t)(column.values * 8ull));
            out->values += column.values;
        }
        free(column.row_start);
        free(column.value_bytes);
        free(column.value_offset);
        free(column.value_length);
        free(column.scratch);
    }
    return ok;
}

// One column's stored lanes as four planes, plane j lane j of every value in value order, each plane one row: the seal
// keeps a signum for every row; a lattice is laid as few long rows. The planes are gathered on the device from the
// stored lanes by a strided copy, and the crystal is sealed for the column. `planes` takes them back for the load check
static int driver_column_seal(SimResults *results, const char *set, const char *sample, const DriverColumn *column,
                              unsigned short *planes, EngineSampleRecord *record)
{
    const unsigned long long lanes = column->values * CASMI_DRIVER_LANES_PER_VALUE;
    const size_t lane_bytes = sizeof(unsigned short);
    unsigned short *device_lanes = NULL;
    unsigned short *device_planes = NULL;
    EngineError error;
    memset(&error, 0, sizeof(error));
    int ok = (cudaMalloc((void **)&device_lanes, (size_t)lanes * lane_bytes) == cudaSuccess) &&
             (cudaMalloc((void **)&device_planes, (size_t)lanes * lane_bytes) == cudaSuccess) &&
             (cudaMemcpy(device_lanes, column->bytes, (size_t)lanes * lane_bytes, cudaMemcpyHostToDevice) == cudaSuccess);
    for (unsigned long long plane = 0ull; ok && (plane < CASMI_DRIVER_LANES_PER_VALUE); plane += 1ull)
    {
        ok = cudaMemcpy2D(device_planes + (plane * column->values), lane_bytes, device_lanes + plane,
                          (size_t)CASMI_DRIVER_LANES_PER_VALUE * lane_bytes, lane_bytes, (size_t)column->values,
                          cudaMemcpyDeviceToDevice) == cudaSuccess;
    }
    ok = ok && (cudaMemcpy(planes, device_planes, (size_t)lanes * lane_bytes, cudaMemcpyDeviceToHost) == cudaSuccess);
    cudaFree(device_lanes);
    const unsigned long long started = engine_clock_microseconds();
    if (ok)
    {
        const EngineCrystalRequest write = {set,
                                            sample,
                                            device_planes,
                                            {1ull, 1ull, CASMI_DRIVER_LANES_PER_VALUE, column->values},
                                            NULL,
                                            0ull,
                                            NULL,
                                            record,
                                            &error};
        ok = engine_crystal_write(&write) == 0L;
        if (!ok)
        {
            driver_error_line(results, "the crystal did not seal", &error);
        }
    }
    const unsigned long long sealed = engine_clock_microseconds();
    cudaFree(device_planes);
    scriptura_text(&results->line, "  ");
    scriptura_text(&results->line, sample);
    driver_line_decimal(results, ": ", column->values);
    driver_line_decimal(results, " values; ", record->floors);
    driver_line_decimal(results, " floors, ", record->crystal_bytes);
    driver_line_decimal(results, " bytes of ", record->raw_bytes);
    driver_line_decimal(results, ", that is ",
                        (record->raw_bytes != 0ull) ? ((record->crystal_bytes * 1000ull) / record->raw_bytes) : 0ull);
    driver_line_decimal(results, " per mille, floored; sealed in ", sealed - started);
    scriptura_text(&results->line, " us");
    driver_line_end(results);
    return ok;
}

// a crystal of the set loaded back and held against the planes it was sealed from
static int driver_column_load(SimResults *results, const char *set, const char *sample, const DriverColumn *column,
                              const unsigned short *planes)
{
    unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
    unsigned short *volume = NULL;
    EngineSignum root;
    memset(&root, 0, sizeof(root));
    EngineError error;
    memset(&error, 0, sizeof(error));
    const int loaded = engine_iapx_load(set, sample, extent, &volume, &root, NULL, &error) == 0L;
    const int same = loaded && (extent[2] == CASMI_DRIVER_LANES_PER_VALUE) && (extent[3] == column->values) &&
                     (memcmp(volume, planes, (size_t)(column->values * 8ull)) == 0);
    if (!loaded)
    {
        driver_error_line(results, "the crystal did not load", &error);
    }
    free(volume);
    return same;
}

// the device bytes one column's seal holds: its lanes and its planes, and the tower's and the coder's pools for them
static unsigned long long driver_declared(const DriverColumn *columns)
{
    unsigned long long widest = 0ull;
    for (unsigned int each = 0u; each < CASMI_DRIVER_COLUMNS; each += 1u)
    {
        const unsigned long long lanes = columns[each].values * CASMI_DRIVER_LANES_PER_VALUE;
        widest = (lanes > widest) ? lanes : widest;
    }
    return (widest * 2ull * 2ull) + tower_reserve_bytes(widest) + compression_reserve_bytes(widest);
}

static int driver_ingest(SimResults *results, int count, char **arguments)
{
    const char *const path = arguments[2];
    const char *const set = arguments[3];
    const CasmiParquetFooterRead footer = {path, &s_footer};
    if (casmi_parquet_footer_read(&footer) < 0ll)
    {
        scriptura_text(&results->line, "  the parquet footer did not read");
        driver_line_end(results);
        return 0;
    }
    char *samples[CASMI_DRIVER_COLUMNS];
    DriverColumn columns[CASMI_DRIVER_COLUMNS];
    memset(columns, 0, sizeof(columns));
    int ok = 1;
    for (unsigned int each = 0u; ok && (each < CASMI_DRIVER_COLUMNS); each += 1u)
    {
        samples[each] = (char *)CASMI_DRIVER_COLUMN_PATH[each];
        ok = driver_column_read(path, CASMI_DRIVER_COLUMN_PATH[each], &columns[each]);
        sim_check(results, ok, "the column reads as its stored bytes");
    }
    if (ok && (sim_job_submit(results, "casmi_driver", count, arguments, driver_declared(columns)) == 0))
    {
        ok = 0;
    }
    EngineSampleRecord records[CASMI_DRIVER_COLUMNS];
    memset(records, 0, sizeof(records));
    for (unsigned int each = 0u; ok && (each < CASMI_DRIVER_COLUMNS); each += 1u)
    {
        unsigned short *const planes = (unsigned short *)malloc((size_t)(columns[each].values * 8ull) + 8u);
        ok = (planes != NULL) && driver_column_seal(results, set, samples[each], &columns[each], planes, &records[each]);
        sim_check(results, ok, "the column seals as a crystal through engine_crystal_write");
        const int same = ok && driver_column_load(results, set, samples[each], &columns[each], planes);
        sim_check(results, same, "the crystal loads back to the planes it was sealed from");
        ok = ok && same;
        free(planes);
    }
    for (unsigned int each = 0u; each < CASMI_DRIVER_COLUMNS; each += 1u)
    {
        free(columns[each].bytes);
    }
    if (ok)
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        EngineSampleRecord proved[CASMI_DRIVER_COLUMNS];
        EngineSetReport report;
        memset(&report, 0, sizeof(report));
        report.samples = proved;
        const EngineSetRequest prove = {set, samples, CASMI_DRIVER_COLUMNS, &error, &report};
        ok = engine_iapx_prove_set(&prove) == 0L;
        driver_line_decimal(results, "  the set proved: ", report.sealed);
        driver_line_decimal(results, " of ", (unsigned long long)CASMI_DRIVER_COLUMNS);
        driver_line_decimal(results, " crystals sealed, ", report.crystal_bytes);
        driver_line_decimal(results, " bytes of ", report.raw_bytes);
        driver_line_end(results);
        sim_check(results, ok, "the set proves");
    }
    return ok;
}

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    if ((count != 4) || (strcmp(arguments[1], "--ingest") != 0))
    {
        scriptura_text(&results.line, "usage: casmi_driver --ingest FILE.parquet SET\n"
                                      "  --ingest: seal the file's double columns as crystals of the set SET\n");
        sim_flush(&results);
        return 2;
    }
    (void)driver_ingest(&results, count, arguments);
    return sim_close(&results, "casmi driver");
}
