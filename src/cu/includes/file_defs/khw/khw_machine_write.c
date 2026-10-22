// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_machine_write.c: the vendor's writer, which reads what the query protocol answered and writes the .khw the gate
// reads. The protocol asks and keeps the answers; this names them and lays them out in the vendor's own machine file
// (P14). It is a program the protocol runs through the interface, and nothing of it reaches back into the protocol.
//
//     orior_khw_machine_write <part> <path.khw> <layout> <mnemonics> <answers> <mode>
//
// Six modes; `assemble` assembles the texts the protocol hands and hands them back (assembled.c). What `krs` and
// `kernel` write for the protocol is given as `answers`, and what `final` and `costs` read from the protocol is read
// from it: the blob of that name in the run where a run's writer runs (qry_member.h), and the file where none
// does. In `krs` the file at `path.khw` is read and each form that answered a relation is given one a line: the
// relation, 1 where the answers read a word signed and 0 where not, the encoding the form was first seen with, low
// word first, and the form's text with each operand that stands in the relation's tuple written as its place,
// {<place>}, for the protocol to name; nothing else is read. In `kernel` the container the layout holds is read and
// the kernel the system accepted is appended as kernel_blob.h lays it out: its head, the places, the registers a
// thread holds, the place the least opening begins at and the slot's place, each of the two as the part's .ksc beside
// the machine file answers it (The frame asked, Open 6.3), then every place's encoding as the container holds it. In
// `gate` the file holds those same places, one form a place, so that the gate reads every question of a round
// against what the part has already run (cubin_safe.h). In `final` the file holds the forms the part answered for,
// read from `answers` as the protocol gave them, each named from the vendor's table (mnemonic_nvidia.tsv) and laid
// out as the instruction its runs make; a form the table names none of is no form of the file. The answers are one
// form a line: the relation the protocol read, whether it read a word signed and whether it asked, the encoding, how
// many runs its operands sit in, and each run's first and last bit and its place in the relation's tuple, - where it
// has none. Each form is written with its relation, its signedness and its runs' places as the protocol read them.
// In `costs` the file holds both: the kernel's places as `gate` lays them out, then the forms the part answered for
// as `final` does, so that the gate reads a costs question, a form written many times over from the slot inside the
// kernel's places, as held whole.
#include "khw_machine_write/khw_machine_write_internal.h"
#include "khw_machine_write/opening.h"

// the path of the part's .ksc beside the machine file at `path`, its extension put in place of the machine file's,
// into `ksc`, which holds `room`: 1, or 0 where it does not fit
static int khw_ksc_path(const char *path, char *ksc, size_t room)
{
    static const char s_extension[] = ".ksc";
    const char *const dot = strrchr(path, '.');
    const size_t stem = (dot != NULL) ? (size_t)(dot - path) : strlen(path);
    const size_t extension = (dot != NULL) ? sizeof(s_extension) : 1u;
    if ((stem + extension) > room)
    {
        return 0;
    }
    memcpy(ksc, path, stem);
    memcpy(ksc + stem, (dot != NULL) ? s_extension : "", extension);
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
    if (strcmp(mode, "kernel") == 0)
    {
        // the kernel read out of the container and appended for the protocol as `answers` (kernel_blob.h): its head,
        // then every place's encoding as the container holds it
        char ksc[1024];
        if (!khw_kernel_read(layout) || !khw_ksc_path(path, ksc, sizeof(ksc)))
        {
            return 1;
        }
        // the place the least opening begins at and the slot's place, each the part's answer in its .ksc beside the
        // machine file: 0 and none where it has not answered or names a place past the kernel
        const unsigned int from = khw_opening_read(ksc);
        const unsigned int slot = khw_slot_read(ksc);
        const KernelBlobHead head = {s_kernel_places, s_registers, (from < s_kernel_places) ? from : 0u,
                                     ((slot != KHW_SLOT_NONE) && (slot < s_kernel_places)) ? slot
                                                                                           : KERNEL_BLOB_SLOT_NONE};
        static unsigned char s_kernel_blob[sizeof(KernelBlobHead) + sizeof(s_kernel_text)];
        const size_t code = (size_t)s_kernel_places * KHW_INSTRUCTION;
        memcpy(s_kernel_blob, &head, sizeof(head));
        memcpy(s_kernel_blob + sizeof(head), s_kernel_text, code);
        QryShared *const run = qry_member_run();
        const int appended =
            (run != NULL) && (qry_member_append(answers, s_kernel_blob, sizeof(head) + code, QRY_ORDINARY) != 0);
        return appended ? 0 : 1;
    }
    if (strcmp(mode, "assemble") == 0)
    {
        // the texts the protocol handed as `answers`, assembled against the machine file and handed back
        return khw_texts_assembled(path, answers) ? 0 : 1;
    }
    if (strcmp(mode, "krs") == 0)
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
            memset(row, 0, sizeof(*row));
            const size_t relation = strlen(form->relation);
            if ((relation == 0u) || (relation >= sizeof(row->relation)) ||
                !sass_text_placed(form, row->text, sizeof(row->text)))
            {
                continue;
            }
            memcpy(row->relation, form->relation, relation);
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
    // the guard field as the part's .ksc beside the machine file names it: a predicate run inside it prints as the
    // guard, and one outside it as an operand
    {
        char ksc[1024];
        unsigned int guard_first = 0u;
        unsigned int guard_bits = 0u;
        if (khw_ksc_path(path, ksc, sizeof(ksc)) && khw_guard_field_read(ksc, &guard_first, &guard_bits))
        {
            khw_guard_field_set(guard_first, guard_bits);
        }
    }
    memset(&s_machine, 0, sizeof(s_machine));
    snprintf(s_machine.part, sizeof(s_machine.part), "%s", part);
    const int gate = strcmp(mode, "gate") == 0;
    const int costs = strcmp(mode, "costs") == 0;
    if (!gate && !costs && (strcmp(mode, "final") != 0))
    {
        printf("  orior_khw_machine_write: %s is no mode this writes\n", mode);
        return 2;
    }
    // the kernel's places, one form a place, where the gate reads questions against them
    unsigned int kept = 0u;
    if (gate || costs)
    {
        if (!khw_kernel_read(layout))
        {
            return 1;
        }
        kept = khw_kernel_forms_kept();
    }
    // the forms the part answered for, each named from the vendor's table, after the kernel's places where those are
    // held
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
            if (!khw_form_kept(&s_machine, &s_form[at], text))
            {
                printf("  orior_khw_machine_write: %s holds no more forms\n", path);
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
        printf("  the kernel's places: %u forms the gate reads a question against\n", kept);
        return 0;
    }
    if (costs)
    {
        printf("  the kernel's places, %u forms, and %u of the %u forms the part answered for named by the vendor's "
               "table: what the gate reads a costs question against, written to %s\n",
               kept, named, forms, path);
        return 0;
    }
    printf("  orior_khw_machine_write: %u of the %u forms the part answered for named by the vendor's table and written to "
           "%s\n",
           named, forms, path);
    return 0;
}
