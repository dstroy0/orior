// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tls.c: a whole write over whichever TLS the system gives, its pieces written in turn, and the count of the cells the
// sessions occupy
#include "tls.h"

int cli_tls_on_write(const CliTlsDataCfg *args)
{
    const unsigned char *const bytes = (const unsigned char *)args->data;
    unsigned long long done = 0ull;
    while (done < args->size)
    {
        const long long reserved = EMBED_CALL(cli_tls_data.write, CliTlsDataCfg, .tls = args->tls, .data = bytes + done,
                                              .size = args->size - done, .err_msg = args->err_msg);
        if (reserved <= 0ll)
        {
            return 0;
        }
        // a count written is not negative here
        done += (unsigned long long)reserved;
    }
    return 1;
}
