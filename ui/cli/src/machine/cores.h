// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cores.h: the machine's cores orior's own work spreads over: every core the machine has less those kept for the
// reader, never fewer than one kept and never fewer than one to work on (cores.c). How many are kept is the setting
// `cores-kept`, one where nothing sets it, the reader's to raise in Preferences or in their settings file; a tree's
// file does not set it. The window gives it as it starts and as it changes, and the command line reads it from the
// reader's settings the first time it is asked
#ifndef CORES_H
#define CORES_H

#include "../command_line/cli_internal.h"

// the setting's key, as settings.json keeps it, and the fewest cores kept for the reader
#define CLI_CORES_KEY "cores-kept"

#define CLI_CORES_KEPT_LEAST 1u

// the cores the machine has; `kept` cores kept for the reader, CLI_CORES_KEPT_LEAST at the fewest; the cores kept, as
// given or as the reader's settings set them; and the cores orior's work may take, the machine's less those kept, one
// at the fewest
unsigned int cli_cores_machine(void);
void cli_cores_keep(unsigned int kept);
unsigned int cli_cores_kept(void);
unsigned int cli_cores_for_work(void);

#endif
