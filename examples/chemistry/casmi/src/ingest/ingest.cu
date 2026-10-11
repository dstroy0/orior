// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ingest.cu: casmi_driver --ingest. The parquet file is a source of the engine's ingest, and its members are its
// samples: the footer, then every leaf of every row group, as engine_source_samples lists them. engine_ingest_set seals
// each member whole as a crystal of the set and rebuilds it against the lanes the file gave, lane for lane
#include "../../../../../src/cu/engine/analysis/compression/compression.h"
#include "../../../../../src/cu/engine/analysis/tower/tower.h"
#include "../../../../../src/cu/engine/engine.h"
#include "../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ingest_line_decimal(SimResults *results, const char *before, unsigned long long value)
{
    scriptura_text(&results->line, before);
    scriptura_decimal(&results->line, value, 1u);
}

static void ingest_line_end(SimResults *results)
{
    scriptura_character(&results->line, '\n');
    sim_flush(results);
}

static void ingest_error_line(SimResults *results, const char *what, const EngineError *error)
{
    scriptura_text(&results->line, what);
    ingest_line_decimal(results, ": module ", (unsigned long long)error->module);
    ingest_line_decimal(results, ", site ", (unsigned long long)error->site);
    ingest_line_decimal(results, ", kind ", (unsigned long long)error->kind);
    ingest_line_decimal(results, ", status ", (unsigned long long)(unsigned int)error->status);
    ingest_line_end(results);
}

static void ingest_names_release(char **names, unsigned int count)
{
    for (unsigned int at = 0u; (names != NULL) && (at < count); at += 1u)
    {
        free(names[at]);
    }
    free(names);
}

int ingest_run(SimResults *results, int count, char **arguments)
{
    const char *const path = arguments[2];
    const char *const set = arguments[3];
    char **names = NULL;
    const unsigned int members = engine_source_samples(path, &names);
    if (members == 0u)
    {
        scriptura_text(&results->line, "  the file lists no member the engine reads");
        ingest_line_end(results);
        ingest_names_release(names, 0u);
        return 0;
    }
    // the job holds one member's lanes on the device at a time, with the tower's and the coder's pools for them: the
    // widest member's
    unsigned long long widest = 0ull;
    unsigned long long lanes_total = 0ull;
    int ok = 1;
    for (unsigned int member = 0u; ok && (member < members); member += 1u)
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        unsigned long long lanes = 0ull;
        ok = engine_source_lanes(path, names[member], &lanes, &error) == 0L;
        if (!ok)
        {
            scriptura_text(&results->line, "  ");
            scriptura_text(&results->line, names[member]);
            ingest_error_line(results, " did not describe", &error);
        }
        widest = (lanes > widest) ? lanes : widest;
        lanes_total += lanes;
    }
    const unsigned long long declared =
        (widest * sizeof(unsigned short)) + tower_reserve_bytes(widest) + compression_reserve_bytes(widest);
    ingest_line_decimal(results, "  members: ", members);
    ingest_line_decimal(results, ", lanes ", lanes_total);
    ingest_line_decimal(results, ", the widest ", widest);
    ingest_line_decimal(results, "; device bytes declared ", declared);
    ingest_line_end(results);
    ok = ok && (sim_job_submit(results, "casmi_driver", count, arguments, declared) != 0);
    EngineError error;
    memset(&error, 0, sizeof(error));
    EngineSetReport report;
    memset(&report, 0, sizeof(report));
    report.samples = ok ? (EngineSampleRecord *)calloc((size_t)members + 1u, sizeof(EngineSampleRecord)) : NULL;
    ok = ok && (report.samples != NULL);
    const EngineIngestRequest ingest = {path, set, names, members, NULL, 0u, &error, &report};
    const long ingested = ok ? engine_ingest_set(&ingest) : ENGINE_ERROR;
    if (ok)
    {
        engine_ingest_print(&ingest, stdout);
        if (ingested != 0L)
        {
            ingest_error_line(results, "  the ingest stopped", &error);
        }
    }
    ok = ok && (ingested == 0L);
    sim_check(results, ok, "every member of the file seals as a crystal of the set and rebuilds to the file's lanes");
    free(report.samples);
    ingest_names_release(names, members);
    return ok;
}
