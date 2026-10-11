// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank.h: each query molecule's train structures ranked by their reference spectra against the molecule's own, read
// from the parquet files themselves, every value an exact integer and every verdict a sign the device returned
#ifndef CASMI_RANK_H
#define CASMI_RANK_H

#include "sim.h"

// casmi_driver --rank TRAIN.parquet CFG validate, or --rank TRAIN.parquet CFG test TEST.parquet: arguments[2] the train
// file, arguments[3] the cfg, arguments[4] the mode and arguments[5] the test file; the job submitted on the device's
// tessera daemon here. 1 where every stage ran
int rank_run(SimResults *results, int count, char **arguments);

#endif
