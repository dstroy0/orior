// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_ptx_probe_main.cu: the probe's main: each probe run on the device given, and its findings printed
#include "interface_ptx_probe_internal.h"

int program_main(int count, char **arguments)
{
    const char *const question = (count > 1) ? arguments[1] : "";
    int device = 0;
    cudaDeviceProp properties;
    if ((cudaGetDevice(&device) != cudaSuccess) || (cudaGetDeviceProperties(&properties, device) != cudaSuccess))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "no device");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const int major = properties.major;
    const int minor = properties.minor;
    char header[PROBE_HEADER_BYTES];
    const size_t header_length = probe_header(major, minor, header, sizeof(header));
    ProbeWriter writer = {ptx_target().ruleset(1), 0, PROBE_TEMPORARIES, PROBE_WIDES, PROBE_PREDICATES};
    if ((writer.rules == NULL) || (header_length == 0u))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "the ruleset errored, or NVRTC gave no header");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "sm_");
        mmgr_line_decimal_signed(output_line, major);
        mmgr_line_decimal_signed(output_line, minor);
        mmgr_line_put(output_line, ", ");
        mmgr_line_put(output_line, header);
        platform_line_out_ended();
    }
    const ProbeName t8 = probe_temporary(&writer, 8u);
    const ProbeName t0 = probe_temporary(&writer, 0u);
    const ProbeName w0 = probe_wide(&writer, 0u);
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "membership", .ci = EMBED_FALSE))
    {
        return probe_membership(&writer, header, major, minor);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cubins",
                    .ci = EMBED_FALSE)) &&
        (count > 2))
    {
        return probe_cubins(&writer, header, major, minor, arguments[2]);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "run",
                    .ci = EMBED_FALSE)) &&
        (count > 2))
    {
        return probe_cubin_run(arguments[2], count, arguments);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "clocks",
                   .ci = EMBED_FALSE))
    {
        return probe_clocks(device);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "alive",
                   .ci = EMBED_FALSE))
    {
        const ProbeName t1 = probe_temporary(&writer, 1u);
        char body[PROBE_HEADER_BYTES];
        VerbaLinea body_line = mmgr_line_over(body, sizeof(body));
        PROBE_FORM(&writer, &body_line, "word_add", t8.text, t0.text, t1.text);
        (void)mmgr_line_finish(&body_line);
        return probe_single(&writer, header, body, major, minor);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "address",
                   .ci = EMBED_FALSE))
    {
        // the load reads the sixteenth byte of the device's address space
        char body[PROBE_HEADER_BYTES];
        VerbaLinea body_line = mmgr_line_over(body, sizeof(body));
        PROBE_FORM(&writer, &body_line, "word_set", t0.text, "16");
        PROBE_FORM(&writer, &body_line, "wide_from_word", w0.text, t0.text);
        PROBE_FORM(&writer, &body_line, "global_load_constant_word", t8.text, w0.text, "0");
        (void)mmgr_line_finish(&body_line);
        return probe_single(&writer, header, body, major, minor);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "misaligned", .ci = EMBED_FALSE))
    {
        // the case's own input, one byte in: a 32-bit load from an address that is not a multiple of 4
        char body[PROBE_HEADER_BYTES];
        VerbaLinea body_line = mmgr_line_over(body, sizeof(body));
        mmgr_line_put(&body_line, "\tadd.s64 \t%interface_wide2, %interface_wide2, 1;\n");
        PROBE_FORM(&writer, &body_line, "global_load_constant_word", t8.text, "%interface_wide2", "0");
        (void)mmgr_line_finish(&body_line);
        return probe_single(&writer, header, body, major, minor);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "trap",
                   .ci = EMBED_FALSE))
    {
        return probe_single(&writer, header, "\ttrap;\n", major, minor);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = question, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "lacking",
                   .ci = EMBED_FALSE))
    {
        return probe_single(&writer, header,
                            "\t{\n\t.reg .pred \t%interface_elected;\n\t.reg .b32 \t%interface_leader;\n"
                            "\telect.sync \t%interface_leader|%interface_elected, 0xffffffff;\n\t}\n",
                            major, minor);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "no question \"");
        mmgr_line_put(output_line, question);
        mmgr_line_put(output_line, "\"");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 2;
}
