// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// casmi_driver: the CASMI library and its ranking on the engine, each part one job on the device's tessera daemon.
// --ingest seals every member of a parquet file as a crystal of a set through the engine's ingest (src/ingest);
// --rank reads the parquet files and ranks each query molecule's structures (src/rank)
#include "../../../../../src/platform/platform.h"
#include "../ingest/ingest.h"
#include "../rank/rank.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "sim.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>

int program_main(int count, char **arguments)
{
    char capacity[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, capacity);
    const int ingest =
        (count == 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "--ingest", .ci = EMBED_FALSE));
    const int rank = ((count == 5) || (count == 6)) &&
                     (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = "--rank", .ci = EMBED_FALSE));
    if (!ingest && !rank)
    {
        mmgr_line_put(&results.line, "usage: casmi_driver --ingest FILE.parquet SET\n"
                                      "       casmi_driver --rank TRAIN.parquet CFG validate\n"
                                      "       casmi_driver --rank TRAIN.parquet CFG test TEST.parquet\n"
                                      "  --ingest: seal every member of the file, its footer and each leaf of each"
                                      " row group, as a crystal of the set SET\n"
                                      "  --rank: rank each query molecule's structures from the train file, CFG's"
                                      " envelope and query library; test ranks TEST.parquet's molecules against every"
                                      " spectrum of the train file and writes CFG's output.submission\n");
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
