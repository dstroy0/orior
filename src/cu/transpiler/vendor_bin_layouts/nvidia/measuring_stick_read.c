// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_read.c: nvcc's listing of the measuring stick read kernel by kernel, the answer key what the engine
// builds for each function of the language is held against. A kernel's operations are its instructions' operations,
// the guard cut and NOP left out, each as many times as it is written. Every operation nvcc writes that no form of
// sass.krs writes is where sass.krs falls short, and the record counts the kernels each is in.
//
// Given the run of `orior protocol stick engine`, each kernel it writes through cu.krs and sass.krs is held against
// nvcc's: the operations nvdisasm reads back from the kernel's encodings, against the operations of nvcc's listing. A
// kernel is at parity where the two are the same multiset. The read back is checked against the kernel's own text,
// instruction by instruction: the assembler and the disassembler agree on every operation the comparison counts.
//
// Each operation is held as a number, the place of its name in the run's table of names, and compared as one. Every
// file is read as the run holds it (ktext.h), and the record is handed to the run's writer, which writes it.
//
//     measuring_stick_read <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine directory>]
#include "measuring_stick_read/stick_internal.h"

// a manifest line: a kernel's number, its category and its function
typedef struct
{
    char *name;
    char *category;
    char *function;
} StickManifest;

int main(int argc, char **argv)
{
    if ((argc != 5) && (argc != 7))
    {
        fputs("measuring_stick_read <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine directory>]\n",
              stderr);
        return 2;
    }
    if (argc == 7)
    {
        stick_engine_read(argv[5], argv[6]);
    }
    stick_nvcc_read(argv[1]);
    stick_read(argv[2]);
    StickManifest *const manifest = (StickManifest *)stick_held(calloc(s_file.lines + 1u, sizeof(StickManifest)));
    unsigned int manifests = 0u;
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        char *cell[3] = {NULL, NULL, NULL};
        const unsigned int cells = stick_cells(s_file.line[line], cell, 3u);
        if ((cells == 3u) && (strcmp(cell[0], "number") != 0))
        {
            char name[256];
            snprintf(name, sizeof(name), "%s%s", s_prefix, cell[0]);
            unsigned int at = 0u;
            while ((at < manifests) && (strcmp(manifest[at].name, name) != 0))
            {
                at += 1u;
            }
            if (at == manifests)
            {
                manifest[at].name = stick_copy(name, strlen(name));
                manifests += 1u;
            }
            free(manifest[at].category);
            free(manifest[at].function);
            manifest[at].category = cell[1];
            manifest[at].function = cell[2];
            cell[1] = NULL;
            cell[2] = NULL;
        }
        for (unsigned int one = 0u; one < cells; one += 1u)
        {
            free(cell[one]);
        }
    }
    unsigned char *held = NULL;
    unsigned int held_room = 0u;
    stick_ruleset_operations(argv[3], &held, &held_room);

    // the stick: every kernel of nvcc's listing the manifest names, in the order of their names
    StickKernel *const stick = (StickKernel *)stick_held(calloc(s_kernels + 1u, sizeof(StickKernel)));
    unsigned int sticks = 0u;
    for (unsigned int at = 0u; at < s_kernels; at += 1u)
    {
        for (unsigned int one = 0u; one < manifests; one += 1u)
        {
            if (strcmp(manifest[one].name, s_kernel[at].name) == 0)
            {
                stick[sticks] = s_kernel[at];
                stick[sticks].category = manifest[one].category;
                stick[sticks].function = manifest[one].function;
                sticks += 1u;
                break;
            }
        }
    }
    qsort(stick, sticks, sizeof(StickKernel), stick_kernel_order);
    // every name the table holds, in the order of the names, and each operation held where it is past the room
    unsigned int *const order = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        order[at] = at;
    }
    qsort(order, s_names, sizeof(unsigned int), stick_name_order);
    if (held_room < s_names)
    {
        held = (unsigned char *)stick_held(realloc(held, s_names + 1u));
        memset(&held[held_room], 0, (s_names + 1u) - held_room);
        held_room = s_names + 1u;
    }

    // by category, in the order each first stands; and each operation no form writes, the kernels it is in
    const char **const category = (const char **)stick_held(calloc(sticks + 1u, sizeof(char *)));
    unsigned int *const category_kernels = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
    unsigned int *const category_instructions = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
    unsigned int categories = 0u;
    unsigned int *const missing = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned int *const missing_first = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned char *const written = (unsigned char *)stick_held(calloc(s_names + 1u, 1u));
    unsigned int *const count = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned int *const other = (unsigned int *)stick_held(calloc(s_names + 1u, sizeof(unsigned int)));
    unsigned int instructions = 0u;
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        unsigned int one = 0u;
        while ((one < categories) && (strcmp(category[one], stick[at].category) != 0))
        {
            one += 1u;
        }
        categories += (one == categories) ? 1u : 0u;
        category[one] = stick[at].category;
        category_kernels[one] += 1u;
        category_instructions[one] += stick[at].operations.count;
        instructions += stick[at].operations.count;
        stick_counted(&stick[at].operations, count);
        for (unsigned int operation = 0u; operation < s_names; operation += 1u)
        {
            if (count[operation] == 0u)
            {
                continue;
            }
            written[operation] = 1u;
            if (!held[operation])
            {
                missing_first[operation] = (missing[operation] == 0u) ? at : missing_first[operation];
                missing[operation] += 1u;
            }
        }
    }
    unsigned int writings = 0u;
    unsigned int held_writings = 0u;
    for (unsigned int operation = 0u; operation < s_names; operation += 1u)
    {
        writings += written[operation];
        held_writings += (written[operation] && held[operation]) ? 1u : 0u;
    }

    const size_t prefix = sizeof(s_prefix) - 1u;
    QryText record = {NULL, 0ull, 0ull, 0};
    qry_text_printed(&record, "# The measuring stick: nvcc's listing, the answer key\n\n");
    qry_text_printed(&record,
                     "Written by `orior protocol stick` whole on every run. Each kernel of the measuring stick "
                     "(measuring_stick.py), every function of the CUDA language in a kernel of its own, is "
                     "compiled by nvcc for sm_86 and read here. What the engine builds for each function is "
                     "held against it, a kernel at parity where the engine's code uses the same operations as "
                     "nvcc's, each as many times.\n\n");
    qry_text_printed(&record,
                     "- kernels: %u\n- instructions: %u\n- operations nvcc writes over the stick: %u, of which "
                     "sass.krs writes %u\n\n",
                     sticks, instructions, writings, held_writings);
    if (argc == 7)
    {
        unsigned int answered = 0u;
        unsigned int parity = 0u;
        unsigned int read_back = 0u;
        unsigned int agree = 0u;
        unsigned int text = 0u;
        for (unsigned int at = 0u; at < sticks; at += 1u)
        {
            const StickEngine *const entry = stick_engine_of(stick[at].name);
            if ((entry == NULL) || (strcmp(entry->answer, "answered") != 0))
            {
                continue;
            }
            answered += 1u;
            read_back += entry->operations.count;
            agree += entry->agree;
            text += entry->text;
            stick_counted(&entry->operations, count);
            stick_counted(&stick[at].operations, other);
            parity += (memcmp(count, other, s_names * sizeof(unsigned int)) == 0) ? 1u : 0u;
        }
        qry_text_printed(&record,
                         "- kernels the engine answers: %u, of which at parity with nvcc: %u; kernels that put a "
                         "question: %u\n- the engine's instructions: %u in its lanes' text, %u read back by nvdisasm, "
                         "%u of them the operation the text wrote\n\n",
                         answered, parity, sticks - answered, text, read_back, agree);
        // a table is written with its rows, and a table with none is one line saying so
        qry_text_printed(&record,
                         "## The engine against nvcc\n\nEach kernel the engine answers: its record steps, "
                         "nvcc's instructions and the engine's as nvdisasm reads them back, and the operations "
                         "only one of the two writes, each by how many more times it writes it.\n\n");
        qry_text_printed(&record, (answered != 0u) ? "| kernel | function | steps | nvcc | engine | only nvcc | only "
                                                     "the engine |\n|---|---|---|---|---|---|---|\n"
                                                   : "The engine answers no kernel.\n");
        for (unsigned int at = 0u; at < sticks; at += 1u)
        {
            const StickEngine *const entry = stick_engine_of(stick[at].name);
            if ((entry == NULL) || (strcmp(entry->answer, "answered") != 0))
            {
                continue;
            }
            stick_counted(&entry->operations, count);
            stick_counted(&stick[at].operations, other);
            qry_text_printed(&record, "| %s | `", stick[at].name + prefix);
            stick_escaped(&record, stick[at].function);
            qry_text_printed(&record, "` | %u | %u | %u | ", entry->steps, stick[at].operations.count,
                             entry->operations.count);
            stick_counts_written(&record, order, other, count);
            qry_text_printed(&record, " | ");
            stick_counts_written(&record, order, count, other);
            qry_text_printed(&record, " |\n");
        }
        // the questions the engine puts: each note, the kernels that put it and the first of them, the most first
        char **const note = (char **)stick_held(calloc(sticks + 1u, sizeof(char *)));
        unsigned int *const note_kernels = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
        unsigned int *const note_first = (unsigned int *)stick_held(calloc(sticks + 1u, sizeof(unsigned int)));
        unsigned int notes = 0u;
        for (unsigned int at = 0u; at < sticks; at += 1u)
        {
            const StickEngine *const entry = stick_engine_of(stick[at].name);
            if ((entry == NULL) || (strcmp(entry->answer, "answered") == 0))
            {
                continue;
            }
            unsigned int one = 0u;
            while ((one < notes) && (strcmp(note[one], entry->note) != 0))
            {
                one += 1u;
            }
            note_first[one] = (one == notes) ? at : note_first[one];
            notes += (one == notes) ? 1u : 0u;
            note[one] = entry->note;
            note_kernels[one] += 1u;
        }
        qry_text_printed(&record, "\n## The questions the engine puts\n\nEach kernel the engine does not answer, by "
                                  "the question it puts.\n\n");
        qry_text_printed(&record, (notes != 0u) ? "| question | kernels | first kernel |\n|---|---|---|\n"
                                                : "The engine puts no question.\n");
        for (unsigned int written_notes = 0u; written_notes < notes; written_notes += 1u)
        {
            unsigned int best = notes;
            for (unsigned int one = 0u; one < notes; one += 1u)
            {
                if (note[one] == NULL)
                {
                    continue;
                }
                best = ((best == notes) || (note_kernels[one] > note_kernels[best]) ||
                        ((note_kernels[one] == note_kernels[best]) && (strcmp(note[one], note[best]) < 0)))
                           ? one
                           : best;
            }
            qry_text_printed(&record, "| ");
            stick_escaped(&record, note[best]);
            qry_text_printed(&record, " | %u | %s |\n", note_kernels[best], stick[note_first[best]].name + prefix);
            note[best] = NULL;
        }
        qry_text_printed(&record, "\n");
        free(note_first);
        free(note_kernels);
        free(note);
    }
    qry_text_printed(&record, "## By category\n\n| category | kernels | nvcc instructions |\n|---|---|---|\n");
    for (unsigned int at = 0u; at < categories; at += 1u)
    {
        qry_text_printed(&record, "| %s | %u | %u |\n", category[at], category_kernels[at], category_instructions[at]);
    }
    unsigned int unwritten = 0u;
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        unwritten += (missing[at] != 0u) ? 1u : 0u;
    }
    qry_text_printed(&record, "\n## Operations nvcc writes that no form of sass.krs writes\n\n");
    qry_text_printed(&record, (unwritten != 0u) ? "| operation | kernels it is in | first kernel | its function |\n"
                                                  "|---|---|---|---|\n"
                                                : "Every operation nvcc writes, a form of sass.krs writes.\n");
    for (;;)
    {
        unsigned int best = s_names;
        for (unsigned int at = 0u; at < s_names; at += 1u)
        {
            const unsigned int operation = order[at];
            best = ((missing[operation] != 0u) && ((best == s_names) || (missing[operation] > missing[best])))
                       ? operation
                       : best;
        }
        if (best == s_names)
        {
            break;
        }
        const StickKernel *const first = &stick[missing_first[best]];
        qry_text_printed(&record, "| `%s` | %u | %s | `", s_name[best], missing[best], first->name + prefix);
        stick_escaped(&record, first->function);
        qry_text_printed(&record, "` |\n");
        missing[best] = 0u;
    }
    qry_text_printed(&record, "\n## Every kernel\n\n| kernel | category | function | nvcc | its operations |\n"
                              "|---|---|---|---|---|\n");
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        qry_text_printed(&record, "| %s | %s | `", stick[at].name + prefix, stick[at].category);
        stick_escaped(&record, stick[at].function);
        qry_text_printed(&record, "` | %u | ", stick[at].operations.count);
        stick_counted(&stick[at].operations, count);
        stick_counts_written(&record, order, count, NULL);
        qry_text_printed(&record, " |\n");
    }
    if (qry_text_filed(qry_run(), argv[4], &record) == 0ull)
    {
        fprintf(stderr, "measuring_stick_read: the record %s was not written\n", argv[4]);
        return 1;
    }
    printf("measuring stick: %u kernels, %u instructions in nvcc's listing, %u operations, %u of them written by "
           "sass.krs\n",
           sticks, instructions, writings, held_writings);
    return 0;
}
