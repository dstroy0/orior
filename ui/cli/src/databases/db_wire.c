// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_wire.c: a database server's failure made of a message alone, and written as the reader reads it
#include "db_wire.h"

void cli_db_failure_set(const CliDbWireCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->failure, .val = 0, .bytes = sizeof(*args->failure)});
    memor.cpy(&(MemoriaCfg){.dst = args->failure->message,
                            .src = args->message,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->message, .cap = CLI_POOL_BYTES}) + 1u});
}

void cli_db_failure_write(const CliDbWireOutCfg *args)
{
    mmgr_line_put(args->out, args->failure_read->message);
    if (args->failure_read->code[0] != '\0')
    {
        mmgr_line_put(args->out, " (");
        mmgr_line_put(args->out, args->failure_read->code);
        mmgr_line_ch(args->out, ')');
    }
    if (args->failure_read->detail[0] != '\0')
    {
        mmgr_line_ch(args->out, '\n');
        mmgr_line_put(args->out, args->failure_read->detail);
    }
    if (args->failure_read->hint[0] != '\0')
    {
        mmgr_line_ch(args->out, '\n');
        mmgr_line_put(args->out, args->failure_read->hint);
    }
}
