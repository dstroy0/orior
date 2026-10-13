// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_ptx_probe_main.cu: the probe's main: each probe run on the device given, and its findings printed
#include "interface_ptx_probe_internal.h"

int main(int count, char **arguments)
{
    const char *const question = (count > 1) ? arguments[1] : "";
    int device = 0;
    cudaDeviceProp properties;
    if ((cudaGetDevice(&device) != cudaSuccess) || (cudaGetDeviceProperties(&properties, device) != cudaSuccess))
    {
        printf("no device\n");
        return 2;
    }
    const int major = properties.major;
    const int minor = properties.minor;
    const std::string header = probe_header(major, minor);
    ProbeWriter writer = {ptx_target().ruleset(1), 0, PROBE_TEMPORARIES, PROBE_WIDES, PROBE_PREDICATES};
    if ((writer.rules == NULL) || header.empty())
    {
        printf("the ruleset errored, or NVRTC gave no header\n");
        return 2;
    }
    printf("sm_%d%d, %s", major, minor, header.c_str());
    const std::string t8 = probe_temporary(&writer, 8u);
    const std::string t0 = probe_temporary(&writer, 0u);
    const std::string w0 = probe_wide(&writer, 0u);
    if (strcmp(question, "membership") == 0)
    {
        return probe_membership(&writer, header, major, minor);
    }
    if ((strcmp(question, "cubins") == 0) && (count > 2))
    {
        return probe_cubins(&writer, header, major, minor, arguments[2]);
    }
    if ((strcmp(question, "run") == 0) && (count > 2))
    {
        return probe_cubin_run(arguments[2], count, arguments);
    }
    if (strcmp(question, "clocks") == 0)
    {
        return probe_clocks(device);
    }
    if (strcmp(question, "alive") == 0)
    {
        std::string body;
        probe_form(&writer, body, "word_add", {t8, t0, probe_temporary(&writer, 1u)});
        return probe_single(&writer, header, body, major, minor);
    }
    if (strcmp(question, "address") == 0)
    {
        // the load reads the sixteenth byte of the device's address space
        std::string body;
        probe_form(&writer, body, "word_set", {t0, "16"});
        probe_form(&writer, body, "wide_from_word", {w0, t0});
        probe_form(&writer, body, "global_load_constant_word", {t8, w0, "0"});
        return probe_single(&writer, header, body, major, minor);
    }
    if (strcmp(question, "misaligned") == 0)
    {
        // the case's own input, one byte in: a 32-bit load from an address that is not a multiple of 4
        std::string body = "\tadd.s64 \t%interface_wide2, %interface_wide2, 1;\n";
        probe_form(&writer, body, "global_load_constant_word", {t8, "%interface_wide2", "0"});
        return probe_single(&writer, header, body, major, minor);
    }
    if (strcmp(question, "trap") == 0)
    {
        return probe_single(&writer, header, "\ttrap;\n", major, minor);
    }
    if (strcmp(question, "lacking") == 0)
    {
        return probe_single(&writer, header,
                            "\t{\n\t.reg .pred \t%interface_elected;\n\t.reg .b32 \t%interface_leader;\n"
                            "\telect.sync \t%interface_leader|%interface_elected, 0xffffffff;\n\t}\n",
                            major, minor);
    }
    printf("no question \"%s\"\n", question);
    return 2;
}
