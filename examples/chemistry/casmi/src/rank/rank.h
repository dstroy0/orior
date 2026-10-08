// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank.h: each query molecule's train structures ranked by their reference spectra against the molecule's own, read
// from a set the ingest sealed, every value an exact integer and every verdict a sign the device returned
#ifndef CASMI_RANK_H
#define CASMI_RANK_H

#include "sim.h"

// casmi_driver --rank SET CFG validate: arguments[2] the set, arguments[3] the cfg and arguments[4] the mode; the job
// submitted on the device's tessera daemon here. 1 where every stage ran
int rank_run(SimResults *results, int count, char **arguments);

#endif
