// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_wire.c: a database server's failure made of a message alone, and written as the reader reads it
#include "db_wire.h"

void cli_db_failure_set(CliDbFailure *failure, const char *message)
{
    memor.set(&(MemoriaCfg){.dst = failure, .val = 0, .bytes = sizeof(*failure)});
    memor.cpy(&(MemoriaCfg){.dst = failure->message,
                            .src = message,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}) + 1u});
}

void cli_db_failure_written(const CliDbFailure *failure, VerbaLinea *out)
{
    mmgr_line_put(out, failure->message);
    if (failure->code[0] != '\0')
    {
        mmgr_line_put(out, " (");
        mmgr_line_put(out, failure->code);
        mmgr_line_ch(out, ')');
    }
    if (failure->detail[0] != '\0')
    {
        mmgr_line_ch(out, '\n');
        mmgr_line_put(out, failure->detail);
    }
    if (failure->hint[0] != '\0')
    {
        mmgr_line_ch(out, '\n');
        mmgr_line_put(out, failure->hint);
    }
}
