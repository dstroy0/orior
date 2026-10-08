// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ingest.h: a CASMI parquet file read into its forms on the device and sealed as the crystals of a set
#ifndef CASMI_INGEST_H
#define CASMI_INGEST_H

#include "sim.h"

// casmi_driver --ingest FILE.parquet SET: arguments[2] the file and arguments[3] the set; the job submitted on the
// device's tessera daemon here. 1 where every column sealed and read back
int ingest_run(SimResults *results, int count, char **arguments);

#endif
