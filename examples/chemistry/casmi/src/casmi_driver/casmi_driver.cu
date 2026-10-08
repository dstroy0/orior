// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// casmi_driver: the CASMI library and its ranking on the engine, each part one job on the device's tessera daemon.
// --ingest reads a parquet file into its forms on the device and seals them as the crystals of a set (src/ingest);
// --rank reads a set back and ranks each query molecule's structures (src/rank)
#include "../ingest/ingest.h"
#include "../rank/rank.h"
#include "../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "sim.h"

#include <string.h>

int main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    const int ingest = (count == 4) && (strcmp(arguments[1], "--ingest") == 0);
    const int rank = (count == 5) && (strcmp(arguments[1], "--rank") == 0);
    if (!ingest && !rank)
    {
        scriptura_text(&results.line, "usage: casmi_driver --ingest FILE.parquet SET\n"
                                      "       casmi_driver --rank SET CFG validate\n"
                                      "  --ingest: read the file's columns into their forms on the device and seal"
                                      " them as crystals of the set SET\n"
                                      "  --rank: rank each query molecule's structures from the set, CFG's envelope"
                                      " and query library\n");
        sim_flush(&results);
        return 2;
    }
    if (ingest)
    {
        (void)ingest_run(&results, count, arguments);
    }
    else
    {
        (void)rank_run(&results, count, arguments);
    }
    return sim_close(&results, "casmi driver");
}
