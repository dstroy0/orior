// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_read.c: nvcc's listing of the measuring stick read kernel by kernel, the answer key what the engine
// builds for each function of the language is compared against. A kernel's operations are its instructions' operations,
// the guard cut and NOP left out, each as many times as it is written. Every operation nvcc writes that no form of
// sass.krs writes is where sass.krs falls short, and the record counts the kernels each is in.
//
// Given the run of `orior protocol stick engine`, the reader compares each kernel that the run writes through cu.krs
// and sass.krs against nvcc's. It compares the operations nvdisasm reads back from the kernel's encodings against the
// operations of nvcc's listing. A kernel is at parity where the two are the same multiset. The reader also checks the
// read back against the kernel's own text, instruction by instruction. The assembler and the disassembler then agree
// on every operation the comparison counts.
//
// The reader stores each operation as a number, the place of its name in the table of names, and compares it as one.
// It reads every file as the run stores it (qry_member_file_blob), and gives the record to the run's writer.
//
//     orior_measuring_stick_read <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine directory>]
#include "../../../engine/engine_note.h"
#include "measuring_stick_read/stick_internal.h"

// the record, a movable text that the run's writer gets whole as the read ends
static PlatformText s_record;

// a manifest line: a kernel's name, its category and its function
typedef struct
{
    char name[STICK_WORD_BYTES];
    char category[STICK_CATEGORY_BYTES];
    char function[STICK_FUNCTION_BYTES];
} StickManifest;

// a category of the stick, in the order its first kernel is: its name, its kernels and nvcc's instructions in them
typedef struct
{
    const char *name;
    unsigned int kernels;
    unsigned int instructions;
} StickCategory;

static StickCategory s_category[STICK_CATEGORIES_MAX];
static unsigned int s_categories = 0u;

// a question the engine puts: its note, the kernels that put it, and the first of them
typedef struct
{
    const char *note;
    unsigned int kernels;
    unsigned int first;
} StickNote;

// an operation no form writes: its number, the kernels it is in, and the first of them
typedef struct
{
    unsigned int operation;
    unsigned int kernels;
    unsigned int first;
} StickMissing;

static int manifest_order(const void *left, const void *right)
{
    return stick_text_order(((const StickManifest *)left)->name, ((const StickManifest *)right)->name);
}

// The manifest at `path`, each line `<number>\t<category>\t<function>` past the head, in one cell taken at the count of
// its lines. A kernel the manifest names twice keeps its last line, as the lines go in the order of their kernels, each
// once. The answer is the count of the lines kept
static unsigned int manifest_read(const char *path, StickManifest **manifest)
{
    const StickText text = stick_text_read(path);
    unsigned int lines = 0u;
    for (size_t at = 0u; at < text.size;)
    {
        StickPiece line;
        at = stick_line(&text, at, &line);
        lines += 1u;
    }
    StickManifest *const read = (StickManifest *)stick_allocated(platform_cell_cleared(
        (lines != 0u) ? lines : 1u, sizeof(StickManifest), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    unsigned int count = 0u;
    for (size_t at = 0u; at < text.size;)
    {
        StickPiece line;
        at = stick_line(&text, at, &line);
        StickPiece cell[3];
        if ((stick_cells(line, cell, 3u) != 3u) ||
            ((cell[0].length == 6u) &&
             (EMBED_CALL(memor.cmp, MemoriaCfg, .src = cell[0].at, .other = "number", .bytes = 6u) == 0)))
        {
            continue;
        }
        StickManifest *const one = &read[count];
        VerbaLinea name = mmgr_line_over(one->name, sizeof(one->name));
        mmgr_line_put(&name, s_prefix);
        (void)mmgr_line_put_n(&name, cell[0].at, cell[0].length);
        if (!mmgr_line_ok(&name))
        {
            stick_refused("a kernel's name passes its room: ", cell[0].at, cell[0].length);
        }
        (void)mmgr_line_finish(&name);
        stick_put(one->category, sizeof(one->category), cell[1].at, cell[1].length, "a category passes its room: ");
        stick_put(one->function, sizeof(one->function), cell[2].at, cell[2].length, "a function passes its room: ");
        count += 1u;
    }
    stick_sorted(read, count, sizeof(StickManifest), manifest_order);
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (((at + 1u) == count) || (manifest_order(&read[at], &read[at + 1u]) != 0))
        {
            read[kept] = read[at];
            kept += 1u;
        }
    }
    *manifest = read;
    return kept;
}

// the manifest's line of the kernel named `name` among the `count` at `manifest`, or NULL where it has none
static const StickManifest *manifest_of(const StickManifest *manifest, unsigned int count, const char *name)
{
    unsigned int low = 0u;
    unsigned int high = count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (stick_text_order(manifest[middle].name, name) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < count) && (stick_text_order(manifest[low].name, name) == 0)) ? &manifest[low] : NULL;
}

// `count` counts of the persistent tier, one for each name of the table, each 0
static unsigned int *counts_taken(void)
{
    return (unsigned int *)stick_allocated(platform_cell_cleared(
        s_names + 1u, sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
}

// 1 where the kernel's line of the engine's run answers
static int engine_answers(const StickEngine *entry)
{
    return (entry != NULL) && EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = entry->answer,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "answered", .ci = EMBED_FALSE);
}

static int note_text_order(const void *left, const void *right)
{
    return stick_text_order(((const StickNote *)left)->note, ((const StickNote *)right)->note);
}

// the notes in the order of the kernels that put each, the most first, and then of their texts
static int note_rank_order(const void *left, const void *right)
{
    const StickNote *const one = (const StickNote *)left;
    const StickNote *const other = (const StickNote *)right;
    if (one->kernels != other->kernels)
    {
        return (one->kernels > other->kernels) ? -1 : 1;
    }
    return stick_text_order(one->note, other->note);
}

// the operations in the order of the kernels each is in, the most first, and then of their names
static int missing_order(const void *left, const void *right)
{
    const StickMissing *const one = (const StickMissing *)left;
    const StickMissing *const other = (const StickMissing *)right;
    if (one->kernels != other->kernels)
    {
        return (one->kernels > other->kernels) ? -1 : 1;
    }
    return (one->operation == other->operation) ? 0 : ((one->operation < other->operation) ? -1 : 1);
}

// The engine against nvcc: the count of the kernels the engine answers, of those at parity, and of the instructions.
// Then the table of each kernel it answers, then the questions it puts
static void record_engine(const StickKernel *stick, unsigned int sticks, unsigned int *count, unsigned int *other)
{
    VerbaLinea *const line = &s_record.line;
    unsigned int answered = 0u;
    unsigned int parity = 0u;
    unsigned int read_back = 0u;
    unsigned int agree = 0u;
    unsigned int text = 0u;
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        const StickEngine *const entry = stick_engine_of(stick[at].name);
        if (!engine_answers(entry))
        {
            continue;
        }
        answered += 1u;
        read_back += entry->operations.count;
        agree += entry->agree;
        text += entry->text;
        stick_counted(&entry->operations, count);
        stick_counted(&stick[at].operations, other);
        parity += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = count, .other = other,
                              .bytes = s_names * sizeof(unsigned int)) == 0)
                      ? 1u
                      : 0u;
    }
    mmgr_line_put(line, "- kernels the engine answers: ");
    mmgr_line_decimal(line, answered, 1u);
    mmgr_line_put(line, ", of which at parity with nvcc: ");
    mmgr_line_decimal(line, parity, 1u);
    mmgr_line_put(line, "; kernels that put a question: ");
    mmgr_line_decimal(line, sticks - answered, 1u);
    mmgr_line_put(line, "\n- the engine's instructions: ");
    mmgr_line_decimal(line, text, 1u);
    mmgr_line_put(line, " in its lanes' text, ");
    mmgr_line_decimal(line, read_back, 1u);
    mmgr_line_put(line, " read back by nvdisasm, ");
    mmgr_line_decimal(line, agree, 1u);
    mmgr_line_put(line, " of them the operation the text wrote\n\n");
    // a table has its rows, and a table with no row is one line that says so
    mmgr_line_put(line, "## The engine against nvcc\n\nEach kernel the engine answers: its record steps, nvcc's "
                        "instructions and the engine's as nvdisasm reads them back, and the operations only one of the "
                        "two writes, each by how many more times it writes it.\n\n");
    mmgr_line_put(line, (answered != 0u) ? "| kernel | function | steps | nvcc | engine | only nvcc | only "
                                           "the engine |\n|---|---|---|---|---|---|---|\n"
                                         : "The engine answers no kernel.\n");
    const size_t prefix = sizeof(s_prefix) - 1u;
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        const StickEngine *const entry = stick_engine_of(stick[at].name);
        if (!engine_answers(entry))
        {
            continue;
        }
        stick_counted(&entry->operations, count);
        stick_counted(&stick[at].operations, other);
        mmgr_line_put(line, "| ");
        mmgr_line_put(line, stick[at].name + prefix);
        mmgr_line_put(line, " | `");
        stick_escaped(line, stick[at].function);
        mmgr_line_put(line, "` | ");
        mmgr_line_decimal(line, entry->steps, 1u);
        mmgr_line_put(line, " | ");
        mmgr_line_decimal(line, stick[at].operations.count, 1u);
        mmgr_line_put(line, " | ");
        mmgr_line_decimal(line, entry->operations.count, 1u);
        mmgr_line_put(line, " | ");
        stick_counts_written(line, other, count);
        mmgr_line_put(line, " | ");
        stick_counts_written(line, count, other);
        mmgr_line_put(line, " |\n");
    }
    // The questions the engine puts: each kernel's note that it does not answer, in one cell at the count of the
    // kernels. They go in the order of their texts once. Each run of one text is one question, and its first kernel
    // is the first of the run. The questions then go in the order of their kernels, the most first
    StickNote *const notes = (StickNote *)stick_allocated(
        platform_cell_cleared(sticks + 1u, sizeof(StickNote), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
    unsigned int noted = 0u;
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        const StickEngine *const entry = stick_engine_of(stick[at].name);
        if ((entry == NULL) || engine_answers(entry))
        {
            continue;
        }
        notes[noted].note = entry->note;
        notes[noted].kernels = 1u;
        notes[noted].first = at;
        noted += 1u;
    }
    stick_sorted(notes, noted, sizeof(StickNote), note_text_order);
    unsigned int questions = 0u;
    for (unsigned int at = 0u; at < noted; at += 1u)
    {
        if ((questions != 0u) && (note_text_order(&notes[questions - 1u], &notes[at]) == 0))
        {
            notes[questions - 1u].kernels += 1u;
            continue;
        }
        notes[questions] = notes[at];
        questions += 1u;
    }
    stick_sorted(notes, questions, sizeof(StickNote), note_rank_order);
    mmgr_line_put(line, "\n## The questions the engine puts\n\nEach kernel the engine does not answer, by the question "
                        "it puts.\n\n");
    mmgr_line_put(line, (questions != 0u) ? "| question | kernels | first kernel |\n|---|---|---|\n"
                                          : "The engine puts no question.\n");
    for (unsigned int at = 0u; at < questions; at += 1u)
    {
        mmgr_line_put(line, "| ");
        stick_escaped(line, notes[at].note);
        mmgr_line_put(line, " | ");
        mmgr_line_decimal(line, notes[at].kernels, 1u);
        mmgr_line_put(line, " | ");
        mmgr_line_put(line, stick[notes[at].first].name + prefix);
        mmgr_line_put(line, " |\n");
    }
    mmgr_line_put(line, "\n");
    platform_cell_released(notes, platform_place_g_program_owner);
}

// the line of the category named `name`, put after the categories where they hold none
static StickCategory *category_of(const char *name)
{
    for (unsigned int at = 0u; at < s_categories; at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_category[at].name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = name, .ci = EMBED_FALSE))
        {
            return &s_category[at];
        }
    }
    if (s_categories == STICK_CATEGORIES_MAX)
    {
        stick_refused("more categories than the table holds: ", name,
                      EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    }
    StickCategory *const category = &s_category[s_categories];
    s_categories += 1u;
    category->name = name;
    category->kernels = 0u;
    category->instructions = 0u;
    return category;
}

int program_main(int argc, char **argv)
{
    if ((argc != 5) && (argc != 7))
    {
        static const char s_usage[] =
            "orior_measuring_stick_read <nvcc listing> <manifest> <sass.krs> <record> [<engine.tsv> <engine "
            "directory>]\n";
        platform_terminal_write(1, s_usage, (unsigned long long)(sizeof(s_usage) - 1u));
        return 2;
    }
    if (argc == 7)
    {
        stick_engine_read(argv[5], argv[6]);
    }
    stick_nvcc_read(argv[1]);
    StickManifest *manifest = NULL;
    const unsigned int manifests = manifest_read(argv[2], &manifest);
    stick_ruleset_operations(argv[3]);
    stick_names_made();
    // each operation a form of the ruleset writes
    unsigned char *const operations = (unsigned char *)stick_allocated(
        platform_cell_cleared(s_names + 1u, 1u, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    for (unsigned int at = 0u; at < s_ruleset.count; at += 1u)
    {
        operations[s_ruleset.operation[at].number] = 1u;
    }
    // the stick: every kernel of nvcc's listing the manifest names, in the order of their names
    StickKernel *const stick = s_kernel;
    unsigned int sticks = 0u;
    for (unsigned int at = 0u; at < s_kernels; at += 1u)
    {
        const StickManifest *const named = manifest_of(manifest, manifests, s_kernel[at].name);
        if (named == NULL)
        {
            continue;
        }
        stick[sticks] = s_kernel[at];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = stick[sticks].category, .src = named->category,
                   .bytes = sizeof(stick[sticks].category));
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = stick[sticks].function, .src = named->function,
                   .bytes = sizeof(stick[sticks].function));
        sticks += 1u;
    }
    // by category, in the order each first is; and each operation no form writes, the kernels it is in
    unsigned int *const missing = counts_taken();
    unsigned int *const missing_first = counts_taken();
    unsigned char *const written = (unsigned char *)stick_allocated(
        platform_cell_cleared(s_names + 1u, 1u, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    unsigned int *const count = counts_taken();
    unsigned int *const other = counts_taken();
    unsigned int instructions = 0u;
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        StickCategory *const category = category_of(stick[at].category);
        category->kernels += 1u;
        category->instructions += stick[at].operations.count;
        instructions += stick[at].operations.count;
        stick_counted(&stick[at].operations, count);
        for (unsigned int operation = 0u; operation < s_names; operation += 1u)
        {
            if (count[operation] == 0u)
            {
                continue;
            }
            written[operation] = 1u;
            if (!operations[operation])
            {
                missing_first[operation] = (missing[operation] == 0u) ? at : missing_first[operation];
                missing[operation] += 1u;
            }
        }
    }
    unsigned int writings = 0u;
    unsigned int writing_count = 0u;
    for (unsigned int operation = 0u; operation < s_names; operation += 1u)
    {
        writings += written[operation];
        writing_count += (written[operation] && operations[operation]) ? 1u : 0u;
    }

    platform_text_begun(&s_record, QRY_WRITER_RING_BYTES / 2ULL, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    VerbaLinea *const line = &s_record.line;
    const size_t prefix = sizeof(s_prefix) - 1u;
    mmgr_line_put(line, "# The measuring stick: nvcc's listing, the answer key\n\n");
    mmgr_line_put(line, "Written by `orior protocol stick` whole on every run. Each kernel of the measuring stick "
                        "(measuring_stick.py), every function of the CUDA language in a kernel of its own, is compiled "
                        "by nvcc for sm_86 and read here. What the engine builds for each function is compared against "
                        "it, a kernel at parity where the engine's code uses the same operations as nvcc's, each as "
                        "many times.\n\n");
    mmgr_line_put(line, "- kernels: ");
    mmgr_line_decimal(line, sticks, 1u);
    mmgr_line_put(line, "\n- instructions: ");
    mmgr_line_decimal(line, instructions, 1u);
    mmgr_line_put(line, "\n- operations nvcc writes over the stick: ");
    mmgr_line_decimal(line, writings, 1u);
    mmgr_line_put(line, ", of which sass.krs writes ");
    mmgr_line_decimal(line, writing_count, 1u);
    mmgr_line_put(line, "\n\n");
    if (argc == 7)
    {
        record_engine(stick, sticks, count, other);
    }
    mmgr_line_put(line, "## By category\n\n| category | kernels | nvcc instructions |\n|---|---|---|\n");
    for (unsigned int at = 0u; at < s_categories; at += 1u)
    {
        mmgr_line_put(line, "| ");
        mmgr_line_put(line, s_category[at].name);
        mmgr_line_put(line, " | ");
        mmgr_line_decimal(line, s_category[at].kernels, 1u);
        mmgr_line_put(line, " | ");
        mmgr_line_decimal(line, s_category[at].instructions, 1u);
        mmgr_line_put(line, " |\n");
    }
    // the operations no form writes, in one cell at the count of the names, in the order of the kernels each is in
    StickMissing *const unwritten = (StickMissing *)stick_allocated(platform_cell_cleared(
        s_names + 1u, sizeof(StickMissing), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
    unsigned int unwritten_count = 0u;
    for (unsigned int operation = 0u; operation < s_names; operation += 1u)
    {
        if (missing[operation] != 0u)
        {
            unwritten[unwritten_count].operation = operation;
            unwritten[unwritten_count].kernels = missing[operation];
            unwritten[unwritten_count].first = missing_first[operation];
            unwritten_count += 1u;
        }
    }
    stick_sorted(unwritten, unwritten_count, sizeof(StickMissing), missing_order);
    mmgr_line_put(line, "\n## Operations nvcc writes that no form of sass.krs writes\n\n");
    mmgr_line_put(line, (unwritten_count != 0u) ? "| operation | kernels it is in | first kernel | its function |\n"
                                                  "|---|---|---|---|\n"
                                                : "Every operation nvcc writes, a form of sass.krs writes.\n");
    for (unsigned int at = 0u; at < unwritten_count; at += 1u)
    {
        const StickKernel *const first = &stick[unwritten[at].first];
        mmgr_line_put(line, "| `");
        mmgr_line_put(line, s_name[unwritten[at].operation].text);
        mmgr_line_put(line, "` | ");
        mmgr_line_decimal(line, unwritten[at].kernels, 1u);
        mmgr_line_put(line, " | ");
        mmgr_line_put(line, first->name + prefix);
        mmgr_line_put(line, " | `");
        stick_escaped(line, first->function);
        mmgr_line_put(line, "` |\n");
    }
    platform_cell_released(unwritten, platform_place_g_program_owner);
    mmgr_line_put(
        line, "\n## Every kernel\n\n| kernel | category | function | nvcc | its operations |\n|---|---|---|---|---|\n");
    for (unsigned int at = 0u; at < sticks; at += 1u)
    {
        mmgr_line_put(line, "| ");
        mmgr_line_put(line, stick[at].name + prefix);
        mmgr_line_put(line, " | ");
        mmgr_line_put(line, stick[at].category);
        mmgr_line_put(line, " | `");
        stick_escaped(line, stick[at].function);
        mmgr_line_put(line, "` | ");
        mmgr_line_decimal(line, stick[at].operations.count, 1u);
        mmgr_line_put(line, " | ");
        stick_counted(&stick[at].operations, count);
        stick_counts_written(line, count, NULL);
        mmgr_line_put(line, " |\n");
    }
    const int whole = mmgr_line_ok(line) != 0;
    const char *const bytes = platform_text_bytes(&s_record);
    const int filed = whole && (bytes != NULL) && qry_member_filed(argv[4], bytes, platform_text_size(&s_record));
    platform_text_ended(&s_record);
    if (!filed)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "orior_measuring_stick_read: the record ");
        mmgr_line_put(&engine_note.line, argv[4]);
        mmgr_line_put(&engine_note.line, " was not written");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "measuring stick: ");
    mmgr_line_decimal(output_line, sticks, 1u);
    mmgr_line_put(output_line, " kernels, ");
    mmgr_line_decimal(output_line, instructions, 1u);
    mmgr_line_put(output_line, " instructions in nvcc's listing, ");
    mmgr_line_decimal(output_line, writings, 1u);
    mmgr_line_put(output_line, " operations, ");
    mmgr_line_decimal(output_line, writing_count, 1u);
    mmgr_line_put(output_line, " of them written by sass.krs");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}
