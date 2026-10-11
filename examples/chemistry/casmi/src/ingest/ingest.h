// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ingest.h: every member of a CASMI parquet file sealed as a crystal of a set by the engine's ingest
#ifndef CASMI_INGEST_H
#define CASMI_INGEST_H

#include "sim.h"

// casmi_driver --ingest FILE.parquet SET: arguments[2] the file and arguments[3] the set; the job submitted on the
// device's tessera daemon here. 1 where every member sealed
int ingest_run(SimResults *results, int count, char **arguments);

#endif
