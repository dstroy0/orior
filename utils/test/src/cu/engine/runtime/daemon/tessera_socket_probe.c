// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// a client with no device of its own: it submits one job for the device named on its command line through the
// system's endpoint that tessera_path_endpoint names, with no daemon path. Only the system's daemon already listening
// there, or the socket that systemd owns, can answer. It prints what came back.
#include "../../../../../../../src/cu/engine/runtime/daemon/tessera.h"
#include "../../../../../../../includes/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>

static int probe_hex(const char *text, unsigned char *bytes, unsigned int count)
{
    if (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR) != (2u * count))
    {
        return 0;
    }
    for (unsigned int byte = 0u; byte < count; byte += 1u)
    {
        unsigned int value = 0u;
        if (sscanf(text + (2u * byte), "%2x", &value) != 1)
        {
            return 0;
        }
        // two hex digits are one byte
        bytes[byte] = (unsigned char)value;
    }
    return 1;
}

int program_main(int count, char **arguments)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    TesseraJobSubmit submit;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &submit, .bytes = sizeof(submit), .val = 0);
    if ((count < 2) || !probe_hex(arguments[1], submit.device, TESSERA_DEVICE_BYTES))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line,
                          "  tessera socket probe: the device's UUID as 32 lowercase hex digits is the one argument");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    char endpoint[ENGINE_PATH_CAPACITY];
    const int named = tessera_path_endpoint(endpoint, ENGINE_PATH_CAPACITY);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  endpoint ");
        mmgr_line_put(output_line, named ? endpoint : "(none)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    submit.signum.bytes[0] = 0x50u;
    submit.declared = 1ull;
    submit.grace_microseconds = 1000000ull;
    submit.sweep_microseconds = 20000ull;
    submit.idle_microseconds = 1000000ull;
    submit.daemon_path = NULL;
    submit.error = &error;
    TesseraClient *client = NULL;
    TesseraTicket ticket;
    const long answer = tessera_job_submit(&submit, &client, &ticket);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  submit ");
        mmgr_line_put(output_line, (answer == 0L) ? "admitted" : "errored");
        mmgr_line_put(output_line, " (error type ");
        mmgr_line_decimal_signed(output_line, (int)error.type);
        mmgr_line_put(output_line, ", module ");
        mmgr_line_decimal_signed(output_line, (int)error.module);
        mmgr_line_put(output_line, ", site ");
        mmgr_line_decimal(output_line, error.site, 1u);
        mmgr_line_put(output_line, ", status ");
        mmgr_line_decimal_signed(output_line, error.status);
        mmgr_line_put(output_line, ")");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (answer == 0L)
    {
        tessera_job_release(client, &ticket, &error);
    }
    return (answer == 0L) ? 0 : 1;
}
