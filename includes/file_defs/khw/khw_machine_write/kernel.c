// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kernel.c: the kernel: its code read, its forms stored, the runs and the answers read, and the listing given
#include "khw_machine_write_internal.h"
#include "../../../../src/platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

// the kernel, read out of the container in the layout at `path`. That is the bytes of the container, the part where it
// enters, the registers that a thread of it uses, and its code. The result is 1, or 0 with the reason printed
int khw_kernel_read(const char *path)
{
    s_pattern_size = cubin_pattern_read(path, s_pattern, sizeof(s_pattern), s_kernel, sizeof(s_kernel));
    if (s_pattern_size == 0ULL)
    {
        return 0;
    }
    unsigned long long offsets[KHW_PLACES];
    unsigned long long sizes[KHW_PLACES];
    const unsigned int sections = cubin_code_sections(s_pattern, s_pattern_size, offsets, sizes, KHW_PLACES);
    if (sections == 0u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_khw_machine_write: the container at ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " contains no code the kernel is read from");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    if ((sizes[0] == 0ULL) || ((sizes[0] % KHW_INSTRUCTION) != 0ULL) ||
        (sizes[0] > (unsigned long long)sizeof(s_kernel_text)))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_khw_machine_write: the kernel is ");
            mmgr_line_decimal(output_line, sizes[0], 1u);
            mmgr_line_put(output_line, " bytes, which is no whole count of places");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    mmgr_bytes_copy(s_kernel_text, &s_pattern[offsets[0]], (size_t)sizes[0]);
    s_kernel_places = (unsigned int)(sizes[0] / KHW_INSTRUCTION);
    s_registers = cubin_registers_read(s_pattern, s_pattern_size, s_kernel);
    if (s_registers == 0u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line,
                          "  orior_khw_machine_write: the container reports how many registers no thread of ");
            mmgr_line_put(output_line, s_kernel);
            mmgr_line_put(output_line, " uses");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    return 1;
}

// Each place of the kernel, stored in the machine as a form. The gate then reads a question against what the part ran
// before (cubin_safe.h, its rules 2 and 3).
// A place that the container records as an end carries the name of the table for the end of the thread. The gate
// reads it to find where the code stops.
//
// Each other place carries its own encoding whole. The gate then stores it as the system wrote it, until the part names
// it. The result is the count stored
unsigned int khw_kernel_forms_store(void)
{
    unsigned int exits[KHW_EXITS];
    const unsigned int endings = cubin_exits_read(s_pattern, s_pattern_size, s_kernel, exits, KHW_EXITS);
    const char *const ends = khw_piece_reading("operation", "control_coherence");
    unsigned int stored_count = 0u;
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        int ending = 0;
        for (unsigned int at = 0u; at < endings; at += 1u)
        {
            ending = ending || (exits[at] == (place * KHW_INSTRUCTION));
        }
        char text[KHW_NAME];
        if ((ending != 0) && (ends != NULL))
        {
            VerbaLinea buffer_line = mmgr_line_over(text, sizeof(text));
            mmgr_line_put(&buffer_line, ends);
            (void)mmgr_line_finish(&buffer_line);
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(text, sizeof(text));
            mmgr_line_put(&buffer_line, "0x");
            mmgr_line_hex(&buffer_line, low, 16u);
            mmgr_line_hex(&buffer_line, high, 16u);
            (void)mmgr_line_finish(&buffer_line);
        }
        SassForm *taken = NULL;
        const int took = sass_machine_take(&s_machine, text, low, high, &taken);
        // the code stores an end whose encoding differs from the one under its name whole beside it. The gate then
        // stores each end as the system wrote it
        if (took && (taken != NULL) && ((taken->low != low) || (taken->high != high)) &&
            (s_machine.forms < SASS_MACHINE_FORMS))
        {
            s_machine.form[s_machine.forms] = *taken;
            s_machine.form[s_machine.forms].low = low;
            s_machine.form[s_machine.forms].high = high;
            s_machine.forms += 1u;
        }
        stored_count += took ? 1u : 0u;
    }
    return stored_count;
}

// the runs of `answered_form` read from `text`, `answered_form->runs` of them:
//   - three words a run: its first bit, its last, and its place with the character of its type after it, a register
//     where none.
//   - or two words, its first bit and its last, where the line holds no place. Each run then holds none.
//
// The result is 1, or 0 where the words are neither
static int khw_runs_read(KhwForm *answered_form, const char *text)
{
    unsigned int words = 0u;
    for (const char *at = text; *at != '\0';)
    {
        at += strspn(at, " \t\r\n");
        const size_t length = strcspn(at, " \t\r\n");
        words += (length != 0u) ? 1u : 0u;
        at += length;
    }
    const unsigned int each =
        (words == (3u * answered_form->runs)) ? 3u : ((words == (2u * answered_form->runs)) ? 2u : 0u);
    if (each == 0u)
    {
        return 0;
    }
    const char *rest = text;
    for (unsigned int run = 0u; run < answered_form->runs; run += 1u)
    {
        char place[16];
        answered_form->place[run] = SASS_RUN_NO_PLACE;
        answered_form->type[run] = KHW_KIND_REGISTER;
        if (each == 2u)
        {
            if (!line_unsigned(&rest, &answered_form->first[run]) || !line_unsigned(&rest, &answered_form->last[run]))
            {
                return 0;
            }
        }
        else
        {
            if (!line_unsigned(&rest, &answered_form->first[run]) || !line_unsigned(&rest, &answered_form->last[run]) ||
                !line_word(&rest, place, sizeof(place)))
            {
                return 0;
            }
            // the none mark, with its type's character after it where the run is no register's, or the place with its
            // character after it. The character's place among the four is its type, and no character is a register's
            char *end = NULL;
            const int none = (strncmp(place, SASS_RUN_NO_PLACE_TEXT,
                                      mmgr_text_length(SASS_RUN_NO_PLACE_TEXT, PLATFORM_TEXT_TO_TERMINATOR)) == 0);
            answered_form->place[run] = none ? SASS_RUN_NO_PLACE : (unsigned int)strtoul(place, &end, 10);
            end = none ? (place + mmgr_text_length(SASS_RUN_NO_PLACE_TEXT, PLATFORM_TEXT_TO_TERMINATOR)) : end;
            const char *const character = ((end != NULL) && (*end != '\0'))
                                              ? mmgr_text_char(KHW_KIND_CHARACTERS, *end, PLATFORM_TEXT_TO_TERMINATOR)
                                              : NULL;
            answered_form->type[run] =
                (character != NULL) ? (unsigned int)(character - KHW_KIND_CHARACTERS) : KHW_KIND_REGISTER;
        }
    }
    return 1;
}

// the forms that the protocol answered for, read from `answers` into `form`, as many as `room` has room for. That is
// the latest blob of that name in the run (qry_member.h). The result is the count read, 0 where no writer of a run runs
// or the run holds none
unsigned int khw_answers_read(const char *answers, KhwForm *form, unsigned int room)
{
    QryShared *const run = qry_member_run();
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (run == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_khw_machine_write: no run's writer runs to read ");
            mmgr_line_put(output_line, answers);
            mmgr_line_put(output_line, " from");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0u;
    }
    if (!qry_member_blob(answers, &bytes, &size, NULL))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_khw_machine_write: the run's buffer contains no ");
            mmgr_line_put(output_line, answers);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0u;
    }
    char line[2048];
    unsigned int read = 0u;
    unsigned long long next = 0ULL;
    while ((next < size) && (read < room))
    {
        const unsigned char *const end = (const unsigned char *)memchr(bytes + next, '\n', (size_t)(size - next));
        const size_t length = (end != NULL) ? (size_t)(end - (bytes + next)) : (size_t)(size - next);
        {
            VerbaLinea buffer_line = mmgr_line_over(line, sizeof(line));
            mmgr_line_put_max(&buffer_line, (const char *)(bytes + next), (size_t)((int)length));
            (void)mmgr_line_finish(&buffer_line);
        }
        next += (unsigned long long)length + 1ULL;
        if ((line[0] == '#') || (line[0] == '\0') || (line[0] == '\r'))
        {
            continue;
        }
        KhwForm answered_form;
        mmgr_bytes_set(&answered_form, 0, sizeof(answered_form));
        const char *runs_at = line;
        if (!line_word(&runs_at, answered_form.relation, sizeof(answered_form.relation)) ||
            !line_signed(&runs_at, &answered_form.signed_read) ||
            !line_signed(&runs_at, &answered_form.signedness_queried) ||
            !line_number(&runs_at, 1, &answered_form.low) || !line_number(&runs_at, 1, &answered_form.high) ||
            !line_unsigned(&runs_at, &answered_form.runs))
        {
            continue;
        }
        if ((answered_form.runs > SASS_MACHINE_RUNS) || !khw_runs_read(&answered_form, runs_at))
        {
            continue;
        }
        form[read] = answered_form;
        read += 1u;
    }
    return read;
}
