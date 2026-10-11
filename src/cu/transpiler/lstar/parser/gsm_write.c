// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gsm_write.c: a part's relations written in gnascor's assembly, each as the cases the part answered it on
//
//     gsm_write <part> <path.khw> <layout> <mnemonics> <machine writer>
//
// The face is relational (src/lng/transpiler_gsm_parser.md): a case is the operands and then the answer the relation
// holds, 1,1->2. A form enters the part's machine file only where the part answered one relation on every case of it
// (khw_write.c), and those cases are the ladder's (ladder.h), which the host computes. The vendor's writer (`machine
// writer` above, run through the interface) reads the machine file and hands back each form that answered a relation
// and whether the answers read a word signed; each relation so answered is written once, as every two-word case of it
// the ladder holds, the answer read signed where the part's was. <part>.gsm is written beside the machine file, a case
// a line in the ladder's order, and each word in decimal.
//
// It runs in a query run, the .qry QRY names (qry_buffer.h): the forms the vendor's writer hands back are the blob
// GSM_FORMS_NAME of the run's buffer, and what it prints is handed to the run's buffer as GSM_MACHINE_NAME.
#include "../../../types/file_defs/qry/qry_buffer.h"
#include "../interface/interface.h"
#include "../protocol/counterexample/ladder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the name a form's relation is handed back under, as khw_write writes it
#define GSM_READS_LADDER "ladder."

// the blobs of the run's buffer the vendor's writer hands: the forms it hands back, and what it printed
#define GSM_FORMS_NAME "gsm_forms.txt"
#define GSM_MACHINE_NAME "machine.txt"

#define LADDER_TEXT(name_, text_, words_, measured_) text_,
static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};
#undef LADDER_TEXT

// each relation the part answered, read unsigned and read signed
static int s_answered[LADDER_ANCHOR_COUNT][2];

// The vendor's writer run through the interface in its krs mode, handed the part, the machine file, the container
// layout, the vendor's table and the name to give the forms as. Its output comes back over the interface's pipe, is
// printed, and is handed to the run's buffer as GSM_MACHINE_NAME. 1 where it ended clean
static int gsm_vendor_run(char *const *word)
{
    // the interface's command is a list of words it does not write to; the casts only meet its declared type
    char *const command[] = {word[5], word[1], word[2], word[3], word[4], (char *)GSM_FORMS_NAME, (char *)"krs", NULL};
    static char s_output[65536];
    const InterfaceProbe probe = {command, NULL, 0ull};
    InterfaceAnswer answer = {0};
    answer.output = s_output;
    answer.output_capacity = sizeof(s_output);
    EngineError error = {0};
    const long ran = interface_probe_run(&probe, &answer, &error);
    if (answer.output_bytes != 0ull)
    {
        printf("%s", s_output);
        const unsigned long long kept =
            (answer.output_bytes < (sizeof(s_output) - 1u)) ? answer.output_bytes : (sizeof(s_output) - 1u);
        qry_hand(qry_run(), GSM_MACHINE_NAME, s_output, kept, QRY_ORDINARY);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("  gsm_write: the vendor's writer did not end clean (%s, code %llx)\n", interface_ending_name(answer.ending),
               answer.code);
        return 0;
    }
    return 1;
}

// the relations the forms the vendor's writer handed back, the `size` bytes at `bytes`, answered, into s_answered. A
// line whose relation is none the ladder holds is left out. The count of forms read
static unsigned int gsm_relations_read(const unsigned char *bytes, unsigned long long size)
{
    char line[1024];
    unsigned int read = 0u;
    unsigned long long next = 0ull;
    while (next < size)
    {
        const unsigned char *const end = (const unsigned char *)memchr(bytes + next, '\n', (size_t)(size - next));
        const size_t length = (end != NULL) ? (size_t)(end - (bytes + next)) : (size_t)(size - next);
        snprintf(line, sizeof(line), "%.*s", (int)length, (const char *)(bytes + next));
        next += (unsigned long long)length + 1ull;
        char relation[256];
        int signed_read = 0;
        if (sscanf(line, "%255s %d", relation, &signed_read) != 2)
        {
            continue;
        }
        const size_t prefix = strlen(GSM_READS_LADDER);
        const char *const name = (strncmp(relation, GSM_READS_LADDER, prefix) == 0) ? (relation + prefix) : relation;
        for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
        {
            if (strcmp(s_anchor_text[anchor], name) == 0)
            {
                s_answered[anchor][(signed_read != 0) ? 1 : 0] = 1;
                read += 1u;
            }
        }
    }
    return read;
}

// the folder `path` lies in, into `folder`, which holds `room`: "." where it names none
static void gsm_folder_of(const char *path, char *folder, size_t room)
{
    const char *const slash = strrchr(path, '/');
    const char *const back = strrchr(path, '\\');
    const char *const last = ((slash != NULL) && ((back == NULL) || (slash > back))) ? slash : back;
    if (last == NULL)
    {
        snprintf(folder, room, ".");
        return;
    }
    snprintf(folder, room, "%.*s", (int)(last - path), path);
}

int main(int count, char **word)
{
    if (count != 6)
    {
        printf("gsm_write <part> <path.khw> <layout> <mnemonics> <machine writer>\n");
        return 2;
    }
    const char *const part = word[1];
    const char *const machine = word[2];
    QryBuffer *const run = qry_run();
    if (run == NULL)
    {
        printf("  gsm_write: no query writer runs: %s names no .qry a writer holds open\n", QRY_ENVIRONMENT);
        return 1;
    }
    // the forms the vendor's writer hands, told from any handed before them by their sequence
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long before = 0ull;
    if (!qry_latest(run, GSM_FORMS_NAME, &bytes, &size, &before))
    {
        before = 0ull;
    }
    unsigned long long sequence = 0ull;
    if (!gsm_vendor_run(word))
    {
        return 1;
    }
    if (!qry_latest(run, GSM_FORMS_NAME, &bytes, &size, &sequence) || (sequence <= before))
    {
        printf("  gsm_write: the vendor's writer handed the run's buffer no %s\n", GSM_FORMS_NAME);
        return 1;
    }
    const unsigned int forms = gsm_relations_read(bytes, size);
    char rulesets[1024];
    gsm_folder_of(machine, rulesets, sizeof(rulesets));
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s.gsm", rulesets, part);
    FILE *const out = fopen(path, "wb");
    if (out == NULL)
    {
        printf("  gsm_write: %s could not be written\n", path);
        return 1;
    }
    unsigned int relations = 0u;
    unsigned int cases = 0u;
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        for (unsigned int read_as = 0u; read_as < 2u; read_as += 1u)
        {
            if ((s_answered[anchor][read_as] == 0) || (s_ladder_measured[anchor] != 0))
            {
                continue;
            }
            relations += 1u;
            for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
            {
                const LadderQuestion *const held = &s_ladder_cases[at];
                unsigned int answer = 0u;
                if ((held->anchor != anchor) || (held->words != 2u) ||
                    !ladder_answer(anchor, held->word, held->words, &answer))
                {
                    continue;
                }
                answer = (read_as != 0u) ? ladder_signed_answer(anchor, held->word) : answer;
                fprintf(out, "%u,%u->%u\n", held->word[0], held->word[1], answer);
                cases += 1u;
            }
        }
    }
    if (fclose(out) != 0)
    {
        printf("  gsm_write: %s could not be written\n", path);
        return 1;
    }
    printf("  gsm_write: %u relations the %u forms answered, %u cases, written to %s\n", relations, forms, cases, path);
    return 0;
}
