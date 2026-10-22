// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls.c: a whole write over whichever TLS the system gives, its pieces written in turn, and the count of the cells the
// sessions hold
#include "tls.h"

CliCellsTally g_cli_tls_cells = CLI_CELLS_TALLY("cli_tls");

int cli_tls_written(CliTls *tls, const void *data, unsigned long long size, CliLine *said)
{
    const unsigned char *const bytes = (const unsigned char *)data;
    unsigned long long done = 0ull;
    while (done < size)
    {
        const long long taken = cli_tls_write(tls, bytes + done, size - done, said);
        if (taken <= 0ll)
        {
            return 0;
        }
        // a count written is not negative here
        done += (unsigned long long)taken;
    }
    return 1;
}
