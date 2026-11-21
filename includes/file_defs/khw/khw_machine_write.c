// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_machine_write.c: the vendor's writer, which reads what the query protocol answered and writes the .khw the gate
// reads. The protocol queries and records the answers; this names them and positions them out in the vendor's own
// machine file (P14). It is a program the protocol runs through the interface, and nothing of it reaches back into the
// protocol.
//
//     orior_khw_machine_write <part> <path.khw> <layout> <mnemonics> <answers> <mode>
//
// Six modes. `assemble` assembles the texts that the protocol passes, and passes them back (assembled.c).
//
// `krs` and `kernel` write their output for the protocol to `answers`. `final` and `costs` read their input from the
// protocol from `answers`. Where a writer of a run runs, that is the blob of that name in the run (qry_member.h).
// Where none runs, it is the file.
//
// In `krs`, the writer reads the file at `path.khw`. It gives each form that answered a relation, one a line:
//   - the relation.
//   - 1 where the answers read a word as signed, and 0 where not.
//   - the encoding that the form first came with, low word first.
//   - the text of the form. Each operand in the tuple of the relation shows as its place, {<place>}, for the protocol
//     to name.
//
// The writer reads nothing else in `krs`.
//
// In `kernel`, the writer reads the container in the layout. It appends the kernel that the system accepted, in the
// form of kernel_blob.h:
//   - its head, the places, and the registers that a thread uses.
//   - the place where the least opening begins, and the probe place. The .ksc of the part beside the machine file
//     answers each (Open 6.3).
//   - the encoding of each place, as the container stores it.
//
// In `gate`, the file holds those same places, one form a place. The gate then reads each question of a round against
// what the part ran before (cubin_safe.h).
//
// In `final`, the file holds the forms that the part answered for, read from `answers` as the protocol gave them. The
// table of the vendor (mnemonic_nvidia.tsv) names each. Each goes out as the instruction that its runs make. A form
// that the table does not name is no form of the file. The answers are one form a line:
//   - the relation that the protocol read.
//   - whether it read a word as signed, and whether it queried.
//   - the encoding, and how many runs its operands sit in.
//   - the first and last bit of each run, and its place in the tuple of the relation, or - where it has none.
//
// The writer writes each form with its relation, its signedness and the places of its runs, as the protocol read them.
//
// In `costs`, the file holds both. First come the places of the kernel, as `gate` puts them out. Then come the forms
// that the part answered for, as `final` puts them out. A costs question is a form, written many times over from the
// probe place inside the places of the kernel. The gate then reads a costs question as stored whole.
#include "khw_machine_write/khw_machine_write_internal.h"
#include "../../../src/platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "khw_machine_write/opening.h"

// the path of the .ksc of the part beside the machine file at `path`, into `ksc`, which has room for `room`. Its
// extension takes the place of the extension of the machine file. The result is 1, or 0 where it does not fit
static int khw_ksc_path(const char *path, char *ksc, size_t room)
{
    static const char s_extension[] = ".ksc";
    const char *const dot = strrchr(path, '.');
    const size_t stem = (dot != NULL) ? (size_t)(dot - path) : mmgr_text_length(path, PLATFORM_TEXT_TO_TERMINATOR);
    const size_t extension = (dot != NULL) ? sizeof(s_extension) : 1u;
    if ((stem + extension) > room)
    {
        return 0;
    }
    mmgr_bytes_copy(ksc, path, stem);
    mmgr_bytes_copy(ksc + stem, (dot != NULL) ? s_extension : "", extension);
    return 1;
}

int main(int count, char **word)
{
    if (count != 7)
    {
        printf("orior_khw_machine_write <part> <path.khw> <layout> <mnemonics> <answers> <mode>\n");
        return 2;
    }
    const char *const part = word[1];
    const char *const path = word[2];
    const char *const layout = word[3];
    const char *const mnemonics = word[4];
    const char *const answers = word[5];
    const char *const mode = word[6];
    if (mmgr_text_equal(mode, "kernel", PLATFORM_TEXT_TO_TERMINATOR))
    {
        // the kernel, read out of the container and appended for the protocol as `answers` (kernel_blob.h). It is its
        // head, then the encoding of each place as the container stores it
        char ksc[1024];
        if (!khw_kernel_read(layout) || !khw_ksc_path(path, ksc, sizeof(ksc)))
        {
            return 1;
        }
        // the place where the least opening begins, and the probe place. Each is the answer of the part in its .ksc
        // beside the machine file. Each is 0 and none where the part gave no answer, or names a place past the kernel
        const unsigned int from = khw_opening_read(ksc);
        const unsigned int place = khw_probe_place_read(ksc);
        const KernelBlobHead head = {
            s_kernel_places, s_registers, (from < s_kernel_places) ? from : 0u,
            ((place != KHW_PROBE_PLACE_NONE) && (place < s_kernel_places)) ? place : KERNEL_BLOB_PLACE_NONE};
        static unsigned char s_kernel_blob[sizeof(KernelBlobHead) + sizeof(s_kernel_text)];
        const size_t code = (size_t)s_kernel_places * KHW_INSTRUCTION;
        mmgr_bytes_copy(s_kernel_blob, &head, sizeof(head));
        mmgr_bytes_copy(s_kernel_blob + sizeof(head), s_kernel_text, code);
        QryShared *const run = qry_member_run();
        const int appended =
            (run != NULL) && (qry_member_append(answers, s_kernel_blob, sizeof(head) + code, QRY_ORDINARY) != 0);
        return appended ? 0 : 1;
    }
    if (mmgr_text_equal(mode, "assemble", PLATFORM_TEXT_TO_TERMINATOR))
    {
        // the texts the protocol passed as `answers`, assembled against the machine file and passed back
        return khw_texts_assembled(path, answers) ? 0 : 1;
    }
    if (mmgr_text_equal(mode, "krs", PLATFORM_TEXT_TO_TERMINATOR))
    {
        // each form of the machine file that answered a relation, one row a form (forms_blob.h), appended for the
        // protocol as `answers`
        if (!sass_machine_read(&s_machine, path))
        {
            return 1;
        }
        static FormsBlobRow s_rows[SASS_MACHINE_FORMS];
        unsigned int written = 0u;
        for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
        {
            const SassForm *const form = &s_machine.form[at];
            FormsBlobRow *const row = &s_rows[written];
            mmgr_bytes_set(row, 0, sizeof(*row));
            const size_t relation = mmgr_text_length(form->relation, PLATFORM_TEXT_TO_TERMINATOR);
            if ((relation == 0u) || (relation >= sizeof(row->relation)) ||
                !sass_text_placed(form, row->text, sizeof(row->text)))
            {
                continue;
            }
            mmgr_bytes_copy(row->relation, form->relation, relation);
            row->signed_read = (form->signed_read != 0) ? 1u : 0u;
            row->low = form->low;
            row->high = form->high;
            written += 1u;
        }
        const unsigned long long bytes = (unsigned long long)written * sizeof(FormsBlobRow);
        QryShared *const run = qry_member_run();
        const int appended = (run != NULL) && (qry_member_append(answers, s_rows, bytes, QRY_ORDINARY) != 0);
        return appended ? 0 : 1;
    }
    if (!khw_pieces_read(mnemonics))
    {
        return 1;
    }
    // the guard field, as the .ksc of the part beside the machine file names it. A predicate run inside it prints as
    // the guard. A predicate run outside it prints as an operand
    {
        char ksc[1024];
        unsigned int guard_first = 0u;
        unsigned int guard_bits = 0u;
        if (khw_ksc_path(path, ksc, sizeof(ksc)) && khw_guard_field_read(ksc, &guard_first, &guard_bits))
        {
            khw_guard_field_set(guard_first, guard_bits);
        }
    }
    mmgr_bytes_set(&s_machine, 0, sizeof(s_machine));
    {
        VerbaLinea buffer_line = mmgr_line_over(s_machine.part, sizeof(s_machine.part));
        mmgr_line_put(&buffer_line, part);
        (void)mmgr_line_finish(&buffer_line);
    }
    const int gate = mmgr_text_equal(mode, "gate", PLATFORM_TEXT_TO_TERMINATOR);
    const int costs = mmgr_text_equal(mode, "costs", PLATFORM_TEXT_TO_TERMINATOR);
    if (!gate && !costs && (!mmgr_text_equal(mode, "final", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        printf("  orior_khw_machine_write: %s is no mode this writes\n", mode);
        return 2;
    }
    // the kernel's places, one form a place, where the gate reads questions against them
    unsigned int kernel_forms = 0u;
    if (gate || costs)
    {
        if (!khw_kernel_read(layout))
        {
            return 1;
        }
        kernel_forms = khw_kernel_forms_store();
    }
    // the forms that the part answered for, each named from the table of the vendor. They come after the places of the
    // kernel, where the file stores those
    static KhwForm s_form[SASS_MACHINE_FORMS];
    unsigned int forms = 0u;
    unsigned int named = 0u;
    if (!gate)
    {
        forms = khw_answers_read(answers, s_form, SASS_MACHINE_FORMS);
        for (unsigned int at = 0u; at < forms; at += 1u)
        {
            char text[SASS_MACHINE_TEXT];
            khw_text_composed(&s_form[at], text, sizeof(text));
            if (text[0] == '\0')
            {
                continue;
            }
            if (!khw_form_store(&s_machine, &s_form[at], text))
            {
                printf("  orior_khw_machine_write: %s contains no more forms\n", path);
                return 1;
            }
            named += 1u;
        }
    }
    if (!sass_machine_write(&s_machine, path))
    {
        printf("  orior_khw_machine_write: %s could not be written\n", path);
        return 1;
    }
    if (gate)
    {
        printf("  the kernel's places: %u forms the gate reads a question against\n", kernel_forms);
        return 0;
    }
    if (costs)
    {
        printf("  the kernel's places, %u forms, and %u of the %u forms the part answered for named by the vendor's "
               "table: what the gate reads a costs question against, written to %s\n",
               kernel_forms, named, forms, path);
        return 0;
    }
    printf("  orior_khw_machine_write: %u of the %u forms the part answered for named by the vendor's table and "
           "written to "
           "%s\n",
           named, forms, path);
    return 0;
}
