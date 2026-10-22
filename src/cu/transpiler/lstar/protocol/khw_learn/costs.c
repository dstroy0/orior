// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// costs.c: the costs and the fields written, and the pass enumerated
#include "khw_learn_internal.h"
#include "../../../../includes/parsers/who.h"

// the code of a costs question: the kernel's places before the position, `copies` of the form `low` and `high` from the
// position on, and the kernel's places after the position after them; the kernel as the part accepted it where `copies` is 0
static unsigned char s_cost_code[(KHW_PLACES + KHW_COST_COPIES) * KHW_INSTRUCTION];

// A form timed on the timer register: the costs question of `copies` copies put over `count` cases from `word`
// at the position, KHW_LAUNCHES launches, the least and the most launch's advance of the clock into `least` and `most`,
// and `clock_address` the timer register's address where the launches were timed on it, 0 where on the driver's event
// timer. 1 where the
// part answered, 0 where it refused, the gate stopped it or nothing carried it
static int khw_cost_timed(unsigned int copies, unsigned long long low, unsigned long long high,
                          const unsigned int word[][2], unsigned int count, unsigned long long *least,
                          unsigned long long *most, unsigned int *clock_address)
{
    static RunQuestion s_cost_question;
    const unsigned int after = s_kernel_places - s_slot - 1u;
    unsigned int places = s_kernel_places;
    khw_kernel_put(s_cost_code);
    if (copies != 0u)
    {
        for (unsigned int copy = 0u; copy < copies; copy += 1u)
        {
            khw_word_write(&s_cost_code[(s_slot + copy) * KHW_INSTRUCTION], 0u, low);
            khw_word_write(&s_cost_code[(s_slot + copy) * KHW_INSTRUCTION], 8u, high);
        }
        memcpy(&s_cost_code[(s_slot + copies) * KHW_INSTRUCTION], &s_kernel_text[(s_slot + 1u) * KHW_INSTRUCTION],
               (size_t)after * KHW_INSTRUCTION);
        places = s_slot + copies + after;
    }
    memset(&s_cost_question, 0, sizeof(s_cost_question));
    s_cost_question.code = s_cost_code;
    s_cost_question.code_size = (unsigned long long)places * KHW_INSTRUCTION;
    s_cost_question.registers = s_registers;
    s_cost_question.cases = count;
    s_cost_question.slot = s_slot;
    s_cost_question.launches = KHW_LAUNCHES;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        s_cost_question.word[place][0] = word[place][0];
        s_cost_question.word[place][1] = word[place][1];
    }
    s_asks += 1ull;
    unsigned long long before[4] = {0ull, 0ull, 0ull, 0ull};
    run_channel_clocks(&before[0], &before[1], &before[2], &before[3]);
    const int asked = run_channel_ask(&s_cost_question);
    khw_round_printed(1u, before);
    *least = s_cost_question.least;
    *most = s_cost_question.most;
    *clock_address = s_cost_question.clock_address;
    return asked && (s_cost_question.most != 0ull);
}

// The cost of every form the machine file names, asked (P6): each classified form whose runs carry every place of its
// relation and which the machine file as the run carried it in names, one an operation and operand shape, is written
// KHW_COST_COPIES times over from the position on in a question of its own, over its relation's cases, and timed over
// KHW_LAUNCHES launches on the timer register, read through the BAR0 driver around each launch (the carrier's clock.c);
// the kernel alone is timed the same way first, the reference every form is read against. A form's cost is the
// least launch's advance less the kernel's own, in the clock's count over the copies, and its spread the most launch's
// advance less the least. Each is a row of the part's .ksc at `path`, every cost row the file kept before let go to
// the .krf beside it, since one reading stands for every form. Where the launches were timed on the driver's clock,
// the part having named no clock, nothing is kept: the clock is asked first (learn clock). The count timed
unsigned int khw_costs_asked(const char *path)
{
    static unsigned long long s_low[KHW_FORMS];
    static unsigned long long s_high[KHW_FORMS];
    static unsigned long long s_least[KHW_FORMS];
    static unsigned long long s_spread[KHW_FORMS];
    static unsigned int s_word[RUN_CASES_MOST][2];
    static unsigned int s_plain[RUN_CASES_MOST];
    static unsigned int s_signed[RUN_CASES_MOST];
    static unsigned int s_to_time[KHW_FORMS];
    unsigned int timed = 0u;
    // the machine file as the run carried it in, the first blob of its name: the file as the run holds it now is
    // the gate's for the costs questions, the kernel's places laid out as forms before the forms the part answered
    // for, and the forms a word is written by are the carried-in file's alone
    const unsigned char *machine = NULL;
    unsigned long long machine_size = 0ull;
    const char *const machine_name = qry_member_file_name(s_path);
    const size_t named = (machine_name != NULL) ? strlen(machine_name) : 0u;
    unsigned long long at = 0ull;
    const QryRecord *record = NULL;
    while ((named != 0u) && (machine == NULL) && qry_member_read(&at, &record))
    {
        if ((record->name_length == named) && (memcmp(QRY_RECORD_NAME(record), machine_name, named) == 0))
        {
            machine = QRY_RECORD_BLOB(record);
            machine_size = record->blob_length;
        }
    }
    if (machine == NULL)
    {
        printf("  khw_write: the run carried no %s in, and no form is timed\n", s_path);
        machine_size = 0ull;
    }
    unsigned int to_time = 0u;
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        const int fragment = (held->frame == (unsigned int)FRAME_RELATION_COUNT) &&
                             !khw_places_whole(held->place, held->runs, s_ladder_words[held->anchor]);
        if ((held->phase == KHW_PHASE_CLASSIFIED) && !fragment &&
            khw_form_listed(machine, machine_size, held->low, held->high))
        {
            s_to_time[to_time] = at;
            to_time += 1u;
        }
    }
    khw_plan_handed("costs", (unsigned long long)to_time + 1ull);
    // the kernel alone, the reference, over the kernel's own relation's cases
    const unsigned int kernel_count =
        (s_names != 0u)
            ? khw_anchor_cases(s_named[s_to_time[0]].anchor, s_named[s_to_time[0]].swapped, s_word, s_plain, s_signed)
            : 0u;
    unsigned long long kernel_least = 0ull;
    unsigned long long kernel_most = 0ull;
    unsigned int clock_address = 0u;
    if ((to_time == 0u) ||
        !khw_cost_timed(0u, 0ull, 0ull, s_word, kernel_count, &kernel_least, &kernel_most, &clock_address))
    {
        printf("  costs: the kernel alone could not be timed, and no form is\n");
        return 0u;
    }
    if (clock_address == 0u)
    {
        printf(
            "  costs: the launches were timed on the driver's clock, the part having named none; ask the clock first "
            "(learn clock), and no cost is kept\n");
        return 0u;
    }
    printf("  costs: the kernel alone, the least launch %llu and the most %llu on the timer register\n", kernel_least,
           kernel_most);
    for (unsigned int each = 0u; each < to_time; each += 1u)
    {
        const KhwAnswered *const held = &s_named[s_to_time[each]];
        if (held->frame != (unsigned int)FRAME_RELATION_COUNT)
        {
            continue;
        }
        const unsigned int count = khw_anchor_cases(held->anchor, held->swapped, s_word, s_plain, s_signed);
        unsigned long long least = 0ull;
        unsigned long long most = 0ull;
        if (!khw_cost_timed(KHW_COST_COPIES, held->low, held->high, s_word, count, &least, &most, &clock_address) ||
            (clock_address == 0u))
        {
            continue;
        }
        s_low[timed] = held->low;
        s_high[timed] = held->high;
        s_least[timed] = (least > kernel_least) ? (least - kernel_least) : 0ull;
        s_spread[timed] = most - least;
        printf("  cost: %016llx %016llx, %llu over %u copies above the kernel alone, the spread %llu\n", held->low,
               held->high, s_least[timed], KHW_COST_COPIES, s_spread[timed]);
        timed += 1u;
    }
    // the .ksc as the run holds it, every cost row before let go and the rows timed now put, handed to the run's
    // writer whole (ksc.h); the rows let go kept in the part's .krf beside it (krf.h)
    static KscFile s_file;
    static KfileLines s_removed;
    static char s_rows[KHW_FORMS][128];
    static const char *s_row[KHW_FORMS];
    const Who who = WHO_HERE;
    for (unsigned int at = 0u; at < timed; at += 1u)
    {
        snprintf(s_rows[at], sizeof(s_rows[at]), KHW_COST_ROW, s_least[at], s_low[at], s_high[at], s_spread[at]);
        s_row[at] = s_rows[at];
    }
    const int written = (timed != 0u) && ksc_read(&s_file, path) &&
                        ksc_rows_replaced_traced_by(&s_file, "run", "cost", s_row, timed, who, &s_removed) &&
                        ksc_write_by(&s_file, path, who);
    if ((timed != 0u) && !written)
    {
        printf("  khw_write: %s could not be written through the run's writer\n", path);
    }
    else if (s_removed.lines != 0u)
    {
        char krf[1200];
        krf_path_beside(path, krf, sizeof(krf));
        if (!krf_removed_by(krf, "ksc", "replaced", (const char *const *)s_removed.line, s_removed.lines, who))
        {
            printf("  khw_write: %u cost rows let go from %s could not be kept in %s\n", s_removed.lines, path, krf);
        }
    }
    kfile_lines_free(&s_removed);
    ksc_free(&s_file);
    return timed;
}

// The part's operand fields written to the part's .ksc at `path` as `field <name> <first> <bits>`, where the register
// walk and the predicate walk read them (orior_klq_identity): every field row of the instruction word's layout table
// `s_fields` (sass_nvidia.tsv, container_layout.h), the places the assembler fills. Every `field` line the file kept
// is let go, the rest kept, and the file handed to the run's writer whole (ksc.h). A table that does not read leaves
// the file as it is
void khw_fields_written(const char *path)
{
    static ContainerLayout s_table;
    if (!container_layout_read(&s_table, s_fields))
    {
        printf("  khw_write: %s gives no fields, and %s keeps those it holds\n", s_fields, path);
        return;
    }
    static KscFile s_read;
    static KscFile s_kept;
    const Who who = WHO_HERE;
    ksc_read(&s_read, path);
    for (unsigned int at = 0u; at < s_read.lines; at += 1u)
    {
        if (strncmp(s_read.line[at], "field ", 6u) != 0)
        {
            ksc_line_add(&s_kept, s_read.line[at]);
        }
    }
    ksc_free(&s_read);
    for (unsigned int at = 0u; at < s_table.rows; at += 1u)
    {
        if (s_table.row[at].kind != LAYOUT_FIELD)
        {
            continue;
        }
        char line[LAYOUT_NAME_LONGEST + 48u];
        snprintf(line, sizeof(line), "field %s %llu %u", s_table.row[at].name, s_table.row[at].offset,
                 s_table.row[at].width);
        ksc_line_add(&s_kept, line);
    }
    if (!ksc_write_by(&s_kept, path, who))
    {
        printf("  khw_write: the fields could not be written to %s\n", path);
    }
    ksc_free(&s_kept);
}

// The register-run probes of `held` emitted: each run set to the number of each of the two registers the kernel left
// a case word in, a question of the position, and the round carried. These are the questions khw_fields_asked puts after
// its turns, the ones a single bit's turn never makes, for the vendor's disassembler to read before the part does
static void khw_enumerate_runs(const KhwAnswered *held, const unsigned int word[1][2])
{
    if (held->runs == 0u)
    {
        return;
    }
    s_pooled = 0u;
    for (unsigned int run = 0u; run < held->runs; run += 1u)
    {
        unsigned long long low = held->low;
        unsigned long long high = held->high;
        khw_run_write(&low, &high, held->first[run], held->last[run], KHW_FIRST_REGISTER);
        khw_pooled(s_slot, low, high, word, 1u);
        low = held->low;
        high = held->high;
        khw_run_write(&low, &high, held->first[run], held->last[run], KHW_SECOND_REGISTER);
        khw_pooled(s_slot, low, high, word, 1u);
    }
    khw_round_carried();
}

// The position's questions emitted off the part for the vendor's disassembler to read back: each known form's fields and
// register-runs, none carried, a dry run handing them to the run (`orior protocol cross`). Where `forms`
// names a blob of the run holding forms a prior run learned, every form it holds is read and each one's bits
// turned one at a time (its fields and, with them, every bit a widening would turn) and its runs set to the two
// registers; where it names none, the kernel's own position form alone, its bits turned. The position and the forms are the
// protocol's own, read from a part before; the vendor names none of the questions and no byte of its knowledge enters a
// form. It is how the process is tuned against questions the part has not yet answered, off the part
void khw_enumerate(unsigned int slot, const char *forms)
{
    s_slot = slot;
    const unsigned int word[1][2] = {{3u, 5u}};
    const unsigned int known = (forms != NULL) ? khw_forms_read(forms) : 0u;
    if (known == 0u)
    {
        KhwAnswered held;
        memset(&held, 0, sizeof(held));
        held.low = khw_word_read(&s_kernel_text[slot * KHW_INSTRUCTION], 0u);
        held.high = khw_word_read(&s_kernel_text[slot * KHW_INSTRUCTION], 8u);
        khw_turns_asked(&held, word);
        return;
    }
    for (unsigned int at = 0u; at < known; at += 1u)
    {
        khw_turns_asked(&s_named[at], word);
        khw_enumerate_runs(&s_named[at], word);
    }
}
