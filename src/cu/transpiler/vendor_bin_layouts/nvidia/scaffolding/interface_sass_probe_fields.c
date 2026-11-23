// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_fields.c: a form's fields found by running it on the part, not by reading a disassembler. An
// instruction is assembled through the machine file, put in place of the kernel's IADD3, and run once for its answer;
// then each of its 104 operation bits is turned over one at a time and run again. A turned bit that changes the answer
// or stops the instruction running is a bit the part reads; one that changes nothing is control or unused.
//
// Which operand a read bit belongs to is matched instead of guessed: the same instruction is run with each source
// operand's register number or number moved by a known amount, and a turned encoding bit belongs to the operand whose
// move gives the same answer. A read bit no operand's move explains is the operation's own, or a field the text does
// not print.
//
// Nothing is compiled and no disassembler is run. The cubins are written here and run by interface_sass_run, which
// loads each through the driver and answers over one case; a cubin the part refuses ends that runner, and this restarts
// it past the refusal.
//
//     interface_sass_probe_fields <pattern cubin> <kernel text> <machine file> <folder> <instructions>
//
// The instructions file contains one instruction a line, as sass_krs_assemble writes the forms a ruleset uses. The tool
// writes <folder>/list.txt and the cubins it names; interface_sass_run runs that list and writes the answers, which a
// later pass reads back against the form's runs. The kernel opens with its section's label, loads the case into
// registers and stores the answer's first word from R7.
#include "../../../../../platform/platform.h"
#include "../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "interface_sass_probe_fields/fields_internal.h"
#include "memoria_operor/memoria_operor.h"

int program_main(int count, char **words)
{
    if ((count == 6) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "read", .ci = EMBED_FALSE)))
    {
        return fields_read(words[2], words[3], words[4], words[5]);
    }
    if ((count == 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "names", .ci = EMBED_FALSE)))
    {
        return fields_names(words[2], words[3]);
    }
    if (count != 6)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line,
                          "interface_sass_probe_fields <pattern cubin> <kernel text> <machine file> <folder> "
                          "<instructions>\ninterface_sass_probe_fields read <machine file> <instructions> <answers> "
                          "<record>\ninterface_sass_probe_fields names <machine file> <instructions>");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const char *const folder = words[4];
    s_pattern_size = fields_file_read(words[1], s_pattern, sizeof(s_pattern));
    const unsigned long long kernel_text_size =
        fields_file_read(words[2], (unsigned char *)s_kernel_text, sizeof(s_kernel_text) - 1u);
    s_kernel_text[kernel_text_size] = '\0';
    if ((s_pattern_size == 0ULL) || (kernel_text_size == 0ULL) ||
        !fields_kernel(s_kernel_text, s_kernel, sizeof(s_kernel)))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the pattern cubin ");
            mmgr_line_put(&engine_note.line, words[1]);
            mmgr_line_put(&engine_note.line, " or the kernel ");
            mmgr_line_put(&engine_note.line, words[2]);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    if (!sass_machine_read(&s_machine, words[3]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, words[3]);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    fields_keys();
    PlatformFile *const instructions = platform_file_open(words[5], "rb");
    if (instructions == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the instructions ");
            mmgr_line_put(&engine_note.line, words[5]);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    // step one: the first instruction turned bit by bit into cubins and a list, which interface_sass_run runs
    char line[512];
    while (platform_file_text_read(line, sizeof(line), instructions) != NULL)
    {
        line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "\r\n")] = '\0';
        const char *walk = line + EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = line,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t");
        if ((*walk == '\0') || (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = walk,
                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "IADD3 R8", .ci = EMBED_FALSE)))
        {
            continue;
        }
        // the first operand moved to R8, the register the kernel stores as the answer, which then contains what the
        // form writes
        char redirected[512];
        char observe[64];
        if (!fields_redirect(walk, redirected, sizeof(redirected), observe, sizeof(observe)))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "skip ");
                mmgr_line_put(output_line, walk);
                mmgr_line_put(output_line, ": could not be redirected");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        char lines[2048];
        {
            VerbaLinea lines_line = mmgr_line_over(lines, sizeof(lines));
            mmgr_line_put(&lines_line, FIELDS_LINES_HEAD);
            mmgr_line_put(&lines_line, redirected);
            mmgr_line_put(&lines_line, observe);
            mmgr_line_put(&lines_line, FIELDS_LINES_TAIL);
            (void)mmgr_line_finish(&lines_line);
        }
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        const unsigned int assembled =
            fields_splice(s_kernel_text, lines, s_query_text, sizeof(s_query_text))
                ? sass_assemble_lines(&s_machine, s_query_text, SASS_CONTROL_SAFE, s_code, sizeof(s_code))
                : 0u;
        const int alone =
            (assembled != 0u) && sass_assemble(&s_machine, redirected, 0ULL, 0ULL, SASS_CONTROL_SAFE, &low, &high);
        const unsigned int place = alone ? fields_find(s_code, assembled, low, high) : assembled;
        if (place == assembled)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "skip ");
                mmgr_line_put(output_line, walk);
                mmgr_line_put(output_line, ": did not assemble or was not found once");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        // the kernel as it will run, each instruction read back through the machine file, for every register and
        // predicate it writes to be checked before anything reaches the part. A line no form reads back is printed as
        // its encoding
        for (unsigned int number = 0u; number < assembled; number += 1u)
        {
            const unsigned long long one = fields_word_read(&s_code[16u * number]);
            const unsigned long long two = fields_word_read(&s_code[(16u * number) + 8u]);
            char text[256];
            if (sass_encoding_read(&s_machine, one, two, 16ULL * number, text, sizeof(text)))
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "CODE\t");
                mmgr_line_decimal(output_line, number, 1u);
                mmgr_line_put(output_line, "\t");
                mmgr_line_put(output_line, text);
                mmgr_line_put(output_line, (number == place) ? "\t<- turned" : "");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            else
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "CODE\t");
                mmgr_line_decimal(output_line, number, 1u);
                mmgr_line_put(output_line, "\t0x");
                mmgr_line_hex(output_line, one, 16u);
                mmgr_line_put(output_line, " 0x");
                mmgr_line_hex(output_line, two, 16u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        char path[1024];
        {
            VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
            mmgr_line_put(&buffer_line, folder);
            mmgr_line_put(&buffer_line, "/list.txt");
            (void)mmgr_line_finish(&buffer_line);
        }
        PlatformFile *const made = platform_file_open(path, "wb");
        // the baseline first, its bit 128 turning nothing, then one line an operation bit. A bit whose turned operation
        // key contains a control transfer, a wait or no form is blocked from the part with a skip line in its place: a
        // bit-flip never loops or stalls the part, and the line order still names the bit a row belongs to
        for (unsigned int bit = 0u; bit <= FIELDS_OPERATION_BITS; bit += 1u)
        {
            const unsigned int turned = (bit == 0u) ? 128u : (bit - 1u);
            char name[64];
            char cubin[1024];
            {
                VerbaLinea buffer_line = mmgr_line_over(cubin, sizeof(cubin));
                mmgr_line_put(&buffer_line, folder);
                mmgr_line_put(&buffer_line, "/bit_");
                mmgr_line_decimal(&buffer_line, bit, 3u);
                mmgr_line_put(&buffer_line, ".cubin");
                (void)mmgr_line_finish(&buffer_line);
            }
            const int safe = fields_flip_safe(place, turned, name, sizeof(name));
            if (safe && fields_cubin(s_code, assembled, place, turned, cubin))
            {
                if (made != NULL)
                {
                    mmgr_line_put(platform_file_line(made), cubin);
                    mmgr_line_put(platform_file_line(made), " ");
                    mmgr_line_put(platform_file_line(made), s_kernel);
                    mmgr_line_put(platform_file_line(made), "\n");
                }
            }
            else if (made != NULL)
            {
                mmgr_line_put(platform_file_line(made), "skip ");
                mmgr_line_put(platform_file_line(made), name);
                mmgr_line_put(platform_file_line(made), "\n");
            }
        }
        if (made != NULL)
        {
            platform_file_close(made);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "wrote ");
            mmgr_line_decimal(output_line, FIELDS_OPERATION_BITS + 1u, 1u);
            mmgr_line_put(output_line, " cubins for ");
            mmgr_line_put(output_line, walk);
            mmgr_line_put(output_line, " to ");
            mmgr_line_put(output_line, path);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        break;
    }
    platform_file_close(instructions);
    return 0;
}
