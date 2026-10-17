// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_machine_write.c: the vendor's writer, which reads what the query protocol answered and writes the .khw the gate
// reads. The protocol asks and keeps the answers; this names them and lays them out in the vendor's own machine file
// (P14). It is a program the protocol runs through the interface, and nothing of it reaches back into the protocol.
//
//     khw_machine_write <part> <path.khw> <layout> <mnemonics> <answers> <mode>
//
// Five modes. What `krs` and `kernel` write for the protocol is given as `answers`, and what `final` and `costs` read
// from the protocol is read from it: the blob of that name in the run's buffer where a run's writer runs
// (qry_buffer.h), and the file where none does. In `krs` the file at `path.khw` is
// read and each form that answered a relation is given one a line: the relation, 1 where the answers read a word
// signed and 0 where not, the encoding the form was first seen with, low word first, and the form's text with each
// operand that stands in the relation's tuple written as its place, {<place>}, for the protocol to name; nothing else
// is read. In `kernel` the container the layout holds is read and the kernel the system accepted is given: a header of
// its places, the registers a thread holds, the place the least opening begins at and the slot's place, each of the
// two as the part's .ksc beside the machine file answers it (The frame asked, Open 6.3), the slot `-` where it has not
// answered, then one line a place with the place's encoding. In `gate` the file
// holds those same places, one form a place, so that the gate reads every question of a round against what the part has
// already run (cubin_safe.h). In `final` the file holds the forms the part answered for, read from `answers` as the
// protocol gave them, each named from the vendor's table (mnemonic_nvidia.tsv) and laid out as the instruction its runs
// make; a form the table names none of is no form of the file. The answers are one form a line: the relation the
// protocol read, whether it read a word signed and whether it asked, the encoding, how many runs its operands sit in,
// and each run's first and last bit and its place in the relation's tuple, - where it has none. Each form is written
// with its relation, its signedness and its runs' places as the protocol read them. In `costs` the file holds both:
// the kernel's places as `gate` lays them out, then the forms the part answered for as `final` does, so that the gate
// reads a costs question, a form written many times over from the slot inside the kernel's places, as held whole.
#include "khw_machine_write/khw_machine_write_internal.h"
#include "khw_machine_write/opening.h"

int main(int count, char **word)
{
    if (count != 7)
    {
        printf("khw_machine_write <part> <path.khw> <layout> <mnemonics> <answers> <mode>\n");
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
        // the kernel read out of the container and written for the protocol: its places, the registers a thread
        // holds and the place the least opening begins at, then one line a place with the place's encoding. It is the
        // blob `answers` of the run's buffer where a run's writer runs (qry_buffer.h), and the file `answers` where
        // none does
        if (!khw_kernel_read(layout))
        {
            return 1;
        }
        // the place the least opening begins at, the part's answer in its .ksc beside the machine file; 0 where it
        // has not answered, and 0 said where it names a place past the kernel
        char ksc[1024];
        snprintf(ksc, sizeof(ksc), "%s", path);
        char *const extension = strrchr(ksc, '.');
        if (extension != NULL)
        {
            snprintf(extension, sizeof(ksc) - (size_t)(extension - ksc), ".ksc");
        }
        unsigned int from = khw_opening_read(ksc);
        if (from >= s_kernel_places)
        {
            printf("  khw_machine_write: the place %u the least opening begins at is past the kernel's %u places, and "
                   "every place is handed\n",
                   from, s_kernel_places);
            from = 0u;
        }
        // the slot's place, the part's answer in the same .ksc, - where it has not answered or names a place past
        // the kernel
        unsigned int slot = khw_slot_read(ksc);
        if ((slot != KHW_SLOT_NONE) && (slot >= s_kernel_places))
        {
            printf("  khw_machine_write: the slot's place %u is past the kernel's %u places, and none is named\n", slot,
                   s_kernel_places);
            slot = KHW_SLOT_NONE;
        }
        char slot_text[16];
        (slot == KHW_SLOT_NONE) ? snprintf(slot_text, sizeof(slot_text), "-")
                                : snprintf(slot_text, sizeof(slot_text), "%u", slot);
        KhwListing listing = {NULL, 0u, 0u, 1};
        khw_listed(&listing, "kernel %u %u %u %s\n", s_kernel_places, s_registers, from, slot_text);
        for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
        {
            const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
            const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
            khw_listed(&listing, "%016llx %016llx\n", low, high);
        }
        if (!khw_listing_given(&listing, answers))
        {
            return 1;
        }
        printf("  the kernel %s: %u places, %u registers a thread, the least opening from place %u, the slot %s%s\n",
               s_kernel, s_kernel_places, s_registers, from, (slot == KHW_SLOT_NONE) ? "not answered" : "at place ",
               (slot == KHW_SLOT_NONE) ? "" : slot_text);
        return 0;
    }
    if (strcmp(mode, "krs") == 0)
    {
        if (!sass_machine_read(&s_machine, path))
        {
            return 1;
        }
        KhwListing listing = {NULL, 0u, 0u, 1};
        unsigned int written = 0u;
        for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
        {
            const SassForm *const form = &s_machine.form[at];
            char placed[SASS_MACHINE_TEXT];
            if ((form->relation[0] == '\0') || !sass_text_placed(form, placed, sizeof(placed)))
            {
                continue;
            }
            khw_listed(&listing, "%s %d %016llx %016llx %s\n", form->relation, (form->signed_read != 0) ? 1 : 0,
                       form->low, form->high, placed);
            written += 1u;
        }
        if (!khw_listing_given(&listing, answers))
        {
            return 1;
        }
        printf("  khw_machine_write: %u of the %u forms of %s answered a relation, given as %s\n", written,
               s_machine.forms, path, answers);
        return 0;
    }
    if (!khw_pieces_read(mnemonics))
    {
        return 1;
    }
    // the guard field as the part's .ksc beside the machine file names it: a predicate run inside it prints as the
    // guard, and one outside it as an operand
    {
        char ksc[1024];
        snprintf(ksc, sizeof(ksc), "%s", path);
        char *const extension = strrchr(ksc, '.');
        if (extension != NULL)
        {
            snprintf(extension, sizeof(ksc) - (size_t)(extension - ksc), ".ksc");
        }
        unsigned int guard_first = 0u;
        unsigned int guard_bits = 0u;
        if (khw_guard_field_read(ksc, &guard_first, &guard_bits))
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
        printf("  khw_machine_write: %s is no mode this writes\n", mode);
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
        khw_tables_read(path);
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
                printf("  khw_machine_write: %s holds no more forms\n", path);
                return 1;
            }
            named += 1u;
        }
    }
    if (!sass_machine_write(&s_machine, path))
    {
        printf("  khw_machine_write: %s could not be written\n", path);
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
    printf("  khw_machine_write: %u of the %u forms the part answered for named by the vendor's table and written to "
           "%s\n",
           named, forms, path);
    return 0;
}
