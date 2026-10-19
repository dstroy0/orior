// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// write.c: the writings written to the part and read back: each relation's forms written and answered, the answers read, the writings loaded and read
#include "writings_internal.h"

// every fitting form written into a cubin of its own, with the list, the cases and the forms beside them: 0, or 2
// where nothing could be read or written
int writings_write(const char *pattern, const char *kernel_text, const char *machine, const char *folder)
{
    s_pattern_size = writings_file_read(pattern, s_pattern, sizeof(s_pattern));
    const unsigned long long kernel_text_size = writings_file_read(kernel_text, (unsigned char *)s_kernel_text, sizeof(s_kernel_text) - 1u);
    s_kernel_text[kernel_text_size] = '\0';
    if ((s_pattern_size == 0ull) || (kernel_text_size == 0ull) || !writings_kernel(s_kernel_text, s_kernel, sizeof(s_kernel)) ||
        !sass_machine_read(&s_machine, machine))
    {
        fprintf(stderr, "the pattern %s, the kernel %s or the machine file %s did not read\n", pattern, kernel_text, machine);
        return 2;
    }
    writings_cases();
    char path[1024];
    snprintf(path, sizeof(path), "%s/cases.txt", folder);
    FILE *const cases = fopen(path, "wb");
    snprintf(path, sizeof(path), "%s/list.txt", folder);
    FILE *const list = fopen(path, "wb");
    snprintf(path, sizeof(path), "%s/forms.txt", folder);
    FILE *const forms = fopen(path, "wb");
    if ((cases == NULL) || (list == NULL) || (forms == NULL))
    {
        fprintf(stderr, "the folder %s was not written\n", folder);
        return 2;
    }
    for (unsigned int at = 0u; at < s_cases; at += 1u)
    {
        fprintf(cases, "%08x %08x\n", s_case[at][0], s_case[at][1]);
    }
    unsigned int fitting = 0u;
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
    {
        const SassForm *const form = &s_machine.form[at];
        if (!writings_form_fits(form))
        {
            continue;
        }
        fitting += 1u;
        // A form holding a truth table is put with every value of it, its sources in order: a table over three inputs
        // already holds every order of them. Any other form is put with every assignment of the two words and RZ to
        // its register sources that gives both words, and with the first word alone where it reads one source
        const unsigned int table = writings_table(form);
        const unsigned int values = (table < form->operands) ? 256u : 1u;
        const unsigned int sources = writings_sources(form);
        unsigned int assignments = 1u;
        for (unsigned int source = 0u; source < sources; source += 1u)
        {
            assignments *= 3u;
        }
        for (unsigned int put = 0u; put < (values * assignments); put += 1u)
        {
            const unsigned int value = put / assignments;
            const unsigned int assignment = put % assignments;
            unsigned int lefts = 0u;
            unsigned int rights = 0u;
            for (unsigned int source = 0u; source < sources; source += 1u)
            {
                lefts += (writings_digit(assignment, source) == 0u) ? 1u : 0u;
                rights += (writings_digit(assignment, source) == 1u) ? 1u : 0u;
            }
            const int in_order = (assignment == writings_in_order(sources));
            const int both = (lefts != 0u) && (rights != 0u);
            if (!in_order && ((table < form->operands) || !both))
            {
                continue;
            }
            char instruction[256];
            char lines[2048];
            if (!writings_instruction(form, table, value, assignment, instruction, sizeof(instruction)) ||
                (snprintf(lines, sizeof(lines), WRITINGS_HEAD "%s" WRITINGS_TAIL, instruction) >= (int)sizeof(lines)) ||
                !writings_splice(s_kernel_text, lines, s_asking, sizeof(s_asking)))
            {
                continue;
            }
            const unsigned int count =
                sass_assemble_lines(&s_machine, s_asking, SASS_CONTROL_SAFE, s_code, sizeof(s_code));
            char cubin[1024];
            snprintf(cubin, sizeof(cubin), "%s/form_%05u.cubin", folder, written);
            if ((count == 0u) || !writings_cubin(count, cubin))
            {
                continue;
            }
            fprintf(list, "%s %s\n", cubin, s_kernel);
            fprintf(forms, "%u\t%s\n", written, instruction);
            written += 1u;
        }
    }
    fclose(cases);
    fclose(list);
    fclose(forms);
    printf("interface sass writings: %u forms of %u fit, %u written and held to cubin_safe, %u cases\n", fitting,
           s_machine.forms, written, s_cases);
    return 0;
}

// the answers `answers` holds, a line a cubin as interface_sass_run writes them, read into `answered`, which holds
// s_cases words a cubin: s_ran set for each cubin that answered every case, and the counts of those that answered and
// those the part refused through `ran` and `refused`
void writings_answers_read(FILE *answers, unsigned int count, unsigned int (*answered)[WRITINGS_CASES],
                                  unsigned char *ran_one, unsigned int *ran, unsigned int *refused)
{
    char line[WRITINGS_LINE];
    while (fgets(line, sizeof(line), answers) != NULL)
    {
        char *walk = NULL;
        const unsigned long number = strtoul(line, &walk, 10);
        if ((walk == line) || (number >= count))
        {
            continue;
        }
        walk += strspn(walk, " ");
        if (strncmp(walk, "answered", 8u) != 0)
        {
            *refused += 1u;
            continue;
        }
        walk += 8;
        unsigned int read = 0u;
        while ((read < s_cases) && (*walk != '\0'))
        {
            char *next = NULL;
            answered[number][read] = (unsigned int)strtoul(walk, &next, 16);
            if (next == walk)
            {
                break;
            }
            walk = next;
            read += 1u;
        }
        ran_one[number] = (unsigned char)(read == s_cases);
        *ran += (read == s_cases) ? 1u : 0u;
    }
}

char s_forms[WRITINGS_FORMS_MOST][256];
unsigned int s_form_count;
static unsigned int s_answered[WRITINGS_FORMS_MOST][WRITINGS_CASES];
unsigned char s_ran[WRITINGS_FORMS_MOST];
unsigned char s_holds[WRITINGS_FORMS_MOST][WRITINGS_RELATIONS];
static unsigned int s_forms_ran;
static unsigned int s_forms_refused;

// <folder>/forms.txt and <folder>/answers.txt read, and each form that answered held to every relation: 1, or 0
// where either file did not read
int writings_load(const char *folder)
{
    writings_cases();
    writings_relations();
    char path[1024];
    char line[WRITINGS_LINE];
    snprintf(path, sizeof(path), "%s/forms.txt", folder);
    FILE *const forms = fopen(path, "rb");
    snprintf(path, sizeof(path), "%s/answers.txt", folder);
    FILE *const answers = fopen(path, "rb");
    if ((forms == NULL) || (answers == NULL))
    {
        fprintf(stderr, "the forms or the answers in %s did not read\n", folder);
        return 0;
    }
    s_form_count = 0u;
    while ((s_form_count < WRITINGS_FORMS_MOST) && (fgets(line, sizeof(line), forms) != NULL))
    {
        const char *const tab = strchr(line, '\t');
        if (tab != NULL)
        {
            snprintf(s_forms[s_form_count], sizeof(s_forms[0]), "%.*s", (int)strcspn(tab + 1, "\r\n"), tab + 1);
            s_form_count += 1u;
        }
    }
    fclose(forms);
    s_forms_ran = 0u;
    s_forms_refused = 0u;
    writings_answers_read(answers, s_form_count, s_answered, s_ran, &s_forms_ran, &s_forms_refused);
    fclose(answers);
    for (unsigned int number = 0u; number < s_form_count; number += 1u)
    {
        for (unsigned int relation = 0u; s_ran[number] && (relation < s_relations); relation += 1u)
        {
            unsigned int agreed = 0u;
            for (unsigned int at = 0u; at < s_cases; at += 1u)
            {
                agreed +=
                    (s_answered[number][at] == writings_expected(&s_relation[relation], s_case[at])) ? 1u : 0u;
            }
            s_holds[number][relation] = (unsigned char)(agreed == s_cases);
        }
    }
    return 1;
}

// The answers read back against every relation: the forms that give each relation's every case its word, written to
// `record`. A relation no form gives in one instruction is one the part has no single instruction for, among the forms
// the machine file holds. 0, or 2 where nothing could be read
int writings_read(const char *folder, const char *record)
{
    FILE *const out = writings_load(folder) ? fopen(record, "wb") : NULL;
    if (out == NULL)
    {
        fprintf(stderr, "the record %s was not written\n", record);
        return 2;
    }
    const unsigned int form_count = s_form_count;
    const unsigned int ran = s_forms_ran;
    const unsigned int refused = s_forms_refused;
    fprintf(out, "# Writings found on the part\n\n");
    fprintf(out, "Written by `interface_sass_writings.sh` whole on every run. Every form of the machine file that writes a "
                 "register from registers, predicates and numbers alone is run on the part in place of the kernel's "
                 "IADD3, its first two register sources given each case's two words and every other register source "
                 "RZ. The cases are the ladder's own two-word cases, the words a width turns on against the counts a "
                 "shift turns on, and drawn words, put at once. A form is listed under a ladder relation or a precept "
                 "where it gives every case the word that relation or precept gives it. %u forms ran over %u cases "
                 "and %u were refused by the part.\n\n",
            ran, s_cases, refused);
    unsigned int found_total = 0u;
    for (unsigned int relation = 0u; relation < s_relations; relation += 1u)
    {
        unsigned int found = 0u;
        for (unsigned int number = 0u; number < form_count; number += 1u)
        {
            found += (s_ran[number] && s_holds[number][relation]) ? 1u : 0u;
        }
        fprintf(out, "## %s %s\n\n%u forms give every case its word.\n\n",
                s_relation[relation].precept ? "precept" : "relation", s_relation[relation].name, found);
        for (unsigned int number = 0u; number < form_count; number += 1u)
        {
            if (s_ran[number] && s_holds[number][relation])
            {
                fprintf(out, "- `%s`\n", s_forms[number]);
            }
        }
        fprintf(out, "\n");
        found_total += found;
        printf("  %-8s %-8s %u forms give every case its word\n", s_relation[relation].precept ? "precept" : "relation",
               s_relation[relation].name, found);
    }
    fclose(out);
    printf("interface sass writings: %u forms ran, %u refused, %u writings found, the record written to %s\n", ran,
           refused, found_total, record);
    return 0;
}
