// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_take.c: every instruction of a listing whose form the machine file lacks, taken into it with its
// fields.
//
// The forms are taken into a machine of their own, and their fields found by the probe's own reading: each form's 128
// bits turned over one at a time and decoded, the runs of bits that change one printed operand recorded as that
// operand's (sass_machine_fields). Each is then put into the machine file with those runs and the file written again. A
// form the machine file already contains is left as it is, its fields and its encoding untouched.
//
//     interface_sass_probe_take <listing> <machine file> <architecture> <folder>
//     interface_sass_probe_take --fields <machine file> <architecture> <folder>
//
// The second form gives every form the machine file contains its fields again by the probe's reading, prints each form
// whose fields that changes, and writes the file.
//
// The listing is what cuobjdump -sass or nvdisasm prints. The architecture is named as the disassembler names it,
// SM86; the folder contains the encodings the disassembler is given.
#include "../../../../../../includes/platform/platform.h"
#include "../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "interface_sass_probe.h"
#include "memoria_operor/memoria_operor.h"

#include "../sass_machine.h"

static SassMachine s_machine;
static SassMachine s_taken;
static SassListing s_listing;

// the file at `path` read as text, or NULL where it does not read
static char *take_file(const char *path)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    platform_file_seek(file, 0L, PLATFORM_FILE_FROM_END);
    const long size = platform_file_tell(file);
    platform_file_seek(file, 0L, PLATFORM_FILE_FROM_START);
    // a size the file reported is at least 0, and a byte past it ends the text
    char *const text =
        (size >= 0L) ? (char *)platform_cell_taken((size_t)size + 1u, platform_place_g_program_owner, 0) : NULL;
    const int read = (text != NULL) && (platform_file_read_items(text, 1u, (size_t)size, file) == (size_t)size);
    platform_file_close(file);
    if (read == 0)
    {
        platform_cell_released(text, platform_place_g_program_owner);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

// 1 where two forms' runs of bits are the same
static int take_runs_same(const SassForm *one, const SassForm *other)
{
    int same = (one->runs == other->runs) ? 1 : 0;
    for (unsigned int at = 0u; same && (at < one->runs); at += 1u)
    {
        same = (one->run[at].operand == other->run[at].operand) && (one->run[at].first == other->run[at].first) &&
               (one->run[at].last == other->run[at].last);
    }
    return same;
}

// a form's runs written as the machine file writes them
static void take_runs_print(const SassForm *form)
{
    for (unsigned int at = 0u; at < form->runs; at += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (at == 0u) ? "" : ";");
        mmgr_line_decimal(output_line, form->run[at].operand, 1u);
        mmgr_line_put(output_line, ":");
        mmgr_line_decimal(output_line, form->run[at].first, 1u);
        mmgr_line_put(output_line, "-");
        mmgr_line_decimal(output_line, form->run[at].last, 1u);
        platform_line_out_ended();
    }
}

// every form of the machine file at `path` given its fields again by the probe's reading, and the file written: the
// forms whose fields the reading changed counted and printed
static int take_fields(const char *path, const char *architecture, const char *folder)
{
    static SassMachine s_was;
    if (!sass_machine_read(&s_machine, path) || !sass_machine_read(&s_was, path))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, path);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    if (!sass_machine_fields(&s_machine, architecture, folder))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "a form's fields were not found");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    unsigned int changed = 0u;
    for (unsigned int number = 0u; number < s_machine.forms; number += 1u)
    {
        if (take_runs_same(&s_machine.form[number], &s_was.form[number]))
        {
            continue;
        }
        changed += 1u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, s_machine.form[number].text);
            mmgr_line_put(output_line, "\n    was ");
            platform_line_out_ended();
        }
        take_runs_print(&s_was.form[number]);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\n    now ");
            platform_line_out_ended();
        }
        take_runs_print(&s_machine.form[number]);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass take: ");
        mmgr_line_decimal(output_line, changed, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, s_machine.forms, 1u);
        mmgr_line_put(output_line, " forms given other fields");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (!sass_machine_write(&s_machine, path))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, path);
            mmgr_line_put(&engine_note.line, " was not written");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    return 0;
}

int program_main(int count, char **words)
{
    if ((count == 5) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "--fields", .ci = EMBED_FALSE)))
    {
        return take_fields(words[2], words[3], words[4]);
    }
    if (count != 5)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line,
                          "interface_sass_probe_take <listing> <machine file> <architecture> "
                          "<folder>\ninterface_sass_probe_take --fields <machine file> <architecture> <folder>");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    char *const output = take_file(words[1]);
    if ((output == NULL) || !sass_listing_read(output, &s_listing))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the listing ");
            mmgr_line_put(&engine_note.line, words[1]);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    platform_cell_released(output, platform_place_g_program_owner);
    if (!sass_machine_read(&s_machine, words[2]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, words[2]);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    const unsigned int forms_before = s_machine.forms;
    for (unsigned int number = 0u; number < s_listing.count; number += 1u)
    {
        const SassInstruction *const instruction = &s_listing.instructions[number];
        if ((instruction->low == 0ULL) && (instruction->high == 0ULL))
        {
            continue;
        }
        SassInstructionParts parts;
        sass_instruction_read(instruction->text, &parts);
        if (sass_machine_form(&s_machine, &parts) != NULL)
        {
            continue;
        }
        SassForm *found_form = NULL;
        sass_machine_take(&s_taken, instruction->text, instruction->low, instruction->high, &found_form);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass take: ");
        mmgr_line_decimal(output_line, s_listing.count, 1u);
        mmgr_line_put(output_line, " instructions read, ");
        mmgr_line_decimal(output_line, s_taken.forms, 1u);
        mmgr_line_put(output_line, " forms the machine file of ");
        mmgr_line_decimal(output_line, forms_before, 1u);
        mmgr_line_put(output_line, " forms lacks");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (s_taken.forms == 0u)
    {
        return 0;
    }
    if (!sass_machine_fields(&s_taken, words[3], words[4]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "a taken form's fields were not found");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    for (unsigned int number = 0u; number < s_taken.forms; number += 1u)
    {
        const SassForm *const form = &s_taken.form[number];
        SassForm *found_form = NULL;
        const unsigned int before = s_machine.forms;
        if (!sass_machine_take(&s_machine, form->text, form->low, form->high, &found_form) || (found_form == NULL) ||
            (s_machine.forms == before))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  not taken: ");
                mmgr_line_put(output_line, form->text);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        found_form->runs = form->runs;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = found_form->run, .src = form->run, .bytes = sizeof(form->run));
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  taken: ");
            mmgr_line_put(output_line, form->text);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, form->runs, 1u);
            mmgr_line_put(output_line, " runs of bits");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    if (!sass_machine_write(&s_machine, words[2]))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the machine file ");
            mmgr_line_put(&engine_note.line, words[2]);
            mmgr_line_put(&engine_note.line, " was not written");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass take: the machine file ");
        mmgr_line_put(output_line, words[2]);
        mmgr_line_put(output_line, " contains ");
        mmgr_line_decimal(output_line, s_machine.forms, 1u);
        mmgr_line_put(output_line, " forms");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
