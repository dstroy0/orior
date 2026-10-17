// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// written.c: what the pass writes: the run's texts, the runs read, the answers, the working set, the costs and the
// fields written, the vendor's run, the forms read, and the pass enumerated
#include "khw_write_internal.h"

// run `run` of `held` written to `file` as its first bit, its last and its place, KHW_PLACE_NONE_TEXT where it has none
// text gathered in memory for a blob of the run's buffer: its bytes, their count and the room they have, and 0 once a
// line did not fit
typedef struct
{
    char *bytes;
    size_t size;
    size_t room;
    int whole;
} KhwText;

// a line added to `text`, the room grown once to fit it
static void khw_text_added(KhwText *text, const char *format, ...)
{
    for (unsigned int tried = 0u; text->whole && (tried < 2u); tried += 1u)
    {
        char *const end = (text->bytes != NULL) ? (text->bytes + text->size) : NULL;
        va_list words;
        va_start(words, format);
        const int length = vsnprintf(end, text->room - text->size, format, words);
        va_end(words);
        // a length vsnprintf returns is never negative for these formats
        if ((length >= 0) && (((size_t)length + 1u) <= (text->room - text->size)))
        {
            text->size += (size_t)length;
            return;
        }
        const size_t room = (text->room * 2u) + (size_t)((length > 0) ? length : 0) + 1u;
        char *const larger = (char *)realloc(text->bytes, room);
        text->whole = (larger != NULL);
        text->bytes = (larger != NULL) ? larger : text->bytes;
        text->room = (larger != NULL) ? room : text->room;
    }
    text->whole = 0;
}

// `text` handed to the run's buffer as the blob `name`, and let go. 1, or 0 with the reason printed
static int khw_text_handed(KhwText *text, const char *name)
{
    const int handed =
        text->whole && (qry_hand(qry_run(), name, text->bytes, (unsigned long long)text->size, QRY_ORDINARY) != 0ull);
    free(text->bytes);
    text->bytes = NULL;
    if (!handed)
    {
        printf("  khw_write: %s was not handed to the run's buffer\n", name);
    }
    return handed;
}

// run `run` of `held` written to `text` as its first bit, its last and its place with its kind's letter after it,
// KHW_PLACE_NONE_TEXT where it has none
static void khw_run_printed(KhwText *text, const KhwAnswered *held, unsigned int run)
{
    if (held->place[run] == KHW_PLACE_NONE)
    {
        // a run with no place carries its kind's letter after the none mark where it is not a register's
        const unsigned int kind = (held->kind[run] <= KHW_KIND_CONSTANT) ? held->kind[run] : KHW_KIND_REGISTER;
        (kind == KHW_KIND_REGISTER)
            ? khw_text_added(text, " %u %u %s", held->first[run], held->last[run], KHW_PLACE_NONE_TEXT)
            : khw_text_added(text, " %u %u %s%c", held->first[run], held->last[run], KHW_PLACE_NONE_TEXT,
                             KHW_KIND_LETTERS[kind]);
        return;
    }
    khw_text_added(text, " %u %u %u%c", held->first[run], held->last[run], held->place[run],
                   KHW_KIND_LETTERS[(held->kind[run] <= KHW_KIND_CONSTANT) ? held->kind[run] : KHW_KIND_REGISTER]);
}

// the runs of `held` read from `text`, `held->runs` of them: three words a run, its first bit, its last and its place
// with its kind's letter after it, a register where none; or two, its first bit and its last, where the line holds no
// place, every run then holding none. 1, or 0 where the words are neither
static int khw_runs_read(KhwAnswered *held, const char *text)
{
    unsigned int words = 0u;
    for (const char *at = text; *at != '\0';)
    {
        at += strspn(at, " \t\r\n");
        const size_t length = strcspn(at, " \t\r\n");
        words += (length != 0u) ? 1u : 0u;
        at += length;
    }
    const unsigned int each = (words == (3u * held->runs)) ? 3u : ((words == (2u * held->runs)) ? 2u : 0u);
    if (each == 0u)
    {
        return 0;
    }
    const char *rest = text;
    for (unsigned int run = 0u; run < held->runs; run += 1u)
    {
        int step = 0;
        char place[16];
        held->place[run] = KHW_PLACE_NONE;
        if (each == 2u)
        {
            if (sscanf(rest, "%u %u %n", &held->first[run], &held->last[run], &step) != 2)
            {
                return 0;
            }
        }
        else
        {
            if (sscanf(rest, "%u %u %15s %n", &held->first[run], &held->last[run], place, &step) != 3)
            {
                return 0;
            }
            // the none mark, with its kind's letter after it where the run is no register's, or the place with its
            // letter after it; the letter's place among the four is its kind, and no letter is a register's
            char *end = NULL;
            const int none = (strncmp(place, KHW_PLACE_NONE_TEXT, strlen(KHW_PLACE_NONE_TEXT)) == 0);
            held->place[run] = none ? KHW_PLACE_NONE : (unsigned int)strtoul(place, &end, 10);
            end = none ? (place + strlen(KHW_PLACE_NONE_TEXT)) : end;
            const char *const letter = ((end != NULL) && (*end != '\0')) ? strchr(KHW_KIND_LETTERS, *end) : NULL;
            held->kind[run] = (letter != NULL) ? (unsigned int)(letter - KHW_KIND_LETTERS) : KHW_KIND_REGISTER;
        }
        rest += step;
    }
    return 1;
}

// the relation `held` answered as the answers and the working set write it, `ladder.<anchor>` or `frame.<relation>`,
// the frame's with its reading after a dot where `with_reading` is not 0, into `written` of `room` bytes. The text
const char *khw_relation_named(const KhwAnswered *held, int with_reading, char *written, size_t room)
{
    if (held->frame == (unsigned int)FRAME_RELATION_COUNT)
    {
        snprintf(written, room, "%s%s", KHW_READS_LADDER, s_anchor_text[held->anchor]);
    }
    else if (with_reading != 0)
    {
        snprintf(written, room, "%s%s.%llx", FRAME_READS, s_frame_text[held->frame], held->reading);
    }
    else
    {
        snprintf(written, room, "%s%s", FRAME_READS, s_frame_text[held->frame]);
    }
    return written;
}

// the forms the part answered for handed to the run's buffer as KHW_FORMS_NAME, one a line, for the vendor's writer to
// name and lay out: the relation the gate read, a frame relation's with its reading, whether the answers read a word
// signed and whether the part was asked,
// the encoding, and each run's first and last bit and its place in the relation's tuple. A form not yet classified, or
// one with no operand run, names nothing; the vendor lays out none of them. 1, or 0 with the reason printed
int khw_answers_written(void)
{
    KhwText text = {NULL, 0u, 0u, 1};
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        if ((held->phase != KHW_PHASE_CLASSIFIED) || (held->runs == 0u))
        {
            continue;
        }
        char relation[64];
        khw_text_added(&text, "%s %d %d %016llx %016llx %u", khw_relation_named(held, 1, relation, sizeof(relation)),
                       held->signed_read, held->signedness_asked, held->low, held->high, held->runs);
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            khw_run_printed(&text, held, run);
        }
        khw_text_added(&text, "\n");
    }
    return khw_text_handed(&text, KHW_FORMS_NAME);
}

// The working set handed to the run's buffer as KHW_WORK_NAME, every form of it a line read back by khw_forms_read the
// next pass: its phase and the order its words were read, the relation, the signedness, the encoding, and its runs
// with their places. A discovered form carries no run, a turned form the candidates its register-runs will sort, each
// with no place, a classified form the operands kept. It is the state the passes of the split hand each other, kept
// apart from what the vendor lays out. 1, or 0 with the reason printed
int khw_forms_work_written(void)
{
    KhwText text = {NULL, 0u, 0u, 1};
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        char relation[64];
        khw_text_added(&text, "%u %d %s %d %d %016llx %016llx %u", held->phase, held->swapped,
                       khw_relation_named(held, 1, relation, sizeof(relation)), held->signed_read,
                       held->signedness_asked, held->low, held->high, held->runs);
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            khw_run_printed(&text, held, run);
        }
        khw_text_added(&text, "\n");
    }
    return khw_text_handed(&text, KHW_WORK_NAME);
}

// 1 where the machine file's text, `size` bytes at `bytes`, names the encoding `low` and `high` as one of its forms, a
// line `form <operation> <kinds> 0x<low> 0x<high> ...`: the forms a word of the web is written by and ranked among
static int khw_form_listed(const unsigned char *bytes, unsigned long long size, unsigned long long low,
                           unsigned long long high)
{
    unsigned long long at = 0ull;
    char line[512];
    while (qry_line_next(bytes, size, &at, line, sizeof(line)))
    {
        char operation[64];
        char kinds[256];
        unsigned long long listed_low = 0ull;
        unsigned long long listed_high = 0ull;
        if ((strncmp(line, "form ", 5u) == 0) &&
            (sscanf(line + 5, "%63s %255s 0x%llx 0x%llx", operation, kinds, &listed_low, &listed_high) == 4) &&
            (listed_low == low) && (listed_high == high))
        {
            return 1;
        }
    }
    return 0;
}

// the code of a costs question: the kernel's places before the slot, `copies` of the form `low` and `high` from the
// slot on, and the kernel's places after the slot after them; the kernel as the part accepted it where `copies` is 0
static unsigned char s_cost_code[(KHW_PLACES + KHW_COST_COPIES) * KHW_INSTRUCTION];

// A form timed on the timer register: the costs question of `copies` copies put over `count` cases from `word`
// at the slot, KHW_LAUNCHES launches, the least and the most launch's advance of the clock into `least` and `most`,
// and `clock_address` the timer register's address where the launches were timed on it, 0 where on the driver's event
// timer. 1 where the
// part answered, 0 where it refused, the gate held it or nothing carried it
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
// KHW_COST_COPIES times over from the slot on in a question of its own, over its relation's cases, and timed over
// KHW_LAUNCHES launches on the timer register, read through the BAR0 driver around each launch (the carrier's clock.c);
// the kernel alone is timed the same way first, the reference every form is read against. A form's cost is the
// least launch's advance less the kernel's own, in the clock's count over the copies, and its spread the most launch's
// advance less the least. Each is a row of the part's .ksc at `path`, every cost row the file held before let go to
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
    char machine_name[1024];
    if (!qry_file_named(s_path, machine_name, sizeof(machine_name)) ||
        !qry_first(qry_run(), machine_name, &machine, &machine_size))
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
    static KtextFile s_removed;
    static char s_rows[KHW_FORMS][128];
    static const char *s_row[KHW_FORMS];
    const QryWho who = QRY_WHO_HERE;
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
    ktext_free(&s_removed);
    ksc_free(&s_file);
    return timed;
}

// The part's operand fields written to the part's .ksc at `path` as `field <name> <first> <bits>`, where the register
// walk and the predicate walk read them (klq_identity): every field row of the instruction word's layout table
// `s_fields` (sass_nvidia.tsv, container_layout.h), the places the assembler fills. Every `field` line the file held
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
    const QryWho who = QRY_WHO_HERE;
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

// The vendor's writer run through the interface in `mode`, handed the part, the file to write, the container layout,
// the vendor's table and the answers file. Its output comes back over the interface's pipe, is printed, and is handed
// to the run's buffer as KHW_MACHINE_NAME. 1 where it ended clean
int khw_vendor_run(const char *mode, const char *answers)
{
    char *const command[] = {(char *)s_machine_writer, (char *)s_part,  (char *)s_path, (char *)s_layout,
                             (char *)s_mnemonics,      (char *)answers, (char *)mode,   NULL};
    static char s_output[65536];
    const InterfaceProbe probe = {command, NULL, 0ull};
    InterfaceAnswer answer = {0};
    answer.output = s_output;
    answer.output_capacity = sizeof(s_output);
    EngineError error = {0};
    // what this process handed moved into the run before the writer reads it, and what the writer handed once it ends
    qry_moved(qry_run());
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_settled(qry_run());
    if (answer.output_bytes != 0ull)
    {
        printf("%s", s_output);
        const unsigned long long kept =
            (answer.output_bytes < (sizeof(s_output) - 1u)) ? answer.output_bytes : (sizeof(s_output) - 1u);
        qry_hand(qry_run(), KHW_MACHINE_NAME, s_output, kept, QRY_ORDINARY);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("  khw_write: the vendor's writer %s did not end clean (%s, code %llx)\n", mode,
               interface_ending_name(answer.ending), answer.code);
        return 0;
    }
    return 1;
}

// The forms read from the latest blob of `name` in the run's buffer into s_named: the working set the split passes
// keep, a line to a form with its phase and the order its words were read ahead of the relation, or the forms the part
// answered for, a line without those, every form of it classified and read in order. Each form's encoding and the runs
// its operands sit in, the relation and the signedness read past. The count read, 0 where the run holds none
unsigned int khw_forms_read(const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_latest(qry_run(), name, &bytes, &size, NULL))
    {
        return 0u;
    }
    char line[2048];
    unsigned int read = 0u;
    unsigned long long next = 0ull;
    while ((read < KHW_FORMS) && khw_line_read(bytes, size, &next, line, sizeof(line)))
    {
        if ((line[0] == '#') || (line[0] == '\n') || (line[0] == '\0'))
        {
            continue;
        }
        KhwAnswered held;
        memset(&held, 0, sizeof(held));
        char relation[256];
        int at = 0;
        // a working-set line leads with the phase and the read order; a vendor answers line leads with the relation,
        // and every form it holds is classified and read in the order the host put the words
        if (sscanf(line, "%u %d %255s %d %d %llx %llx %u %n", &held.phase, &held.swapped, relation, &held.signed_read,
                   &held.signedness_asked, &held.low, &held.high, &held.runs, &at) != 8)
        {
            held.phase = KHW_PHASE_CLASSIFIED;
            held.swapped = 0;
            if (sscanf(line, "%255s %d %d %llx %llx %u %n", relation, &held.signed_read, &held.signedness_asked,
                       &held.low, &held.high, &held.runs, &at) != 6)
            {
                continue;
            }
        }
        // the relation named back to its anchor or its frame relation, for a form read in to write the same relation
        // out: a frame's with its reading after a dot
        held.anchor = (unsigned int)LADDER_ANCHOR_COUNT;
        held.frame = (unsigned int)FRAME_RELATION_COUNT;
        const size_t ladder_prefix = strlen(KHW_READS_LADDER);
        const size_t frame_prefix = strlen(FRAME_READS);
        if (strncmp(relation, FRAME_READS, frame_prefix) == 0)
        {
            char *const dot = strchr(relation + frame_prefix, '.');
            held.reading = (dot != NULL) ? strtoull(dot + 1, NULL, 16) : 0ull;
            if (dot != NULL)
            {
                *dot = '\0';
            }
            for (unsigned int each = 0u; each < (unsigned int)FRAME_RELATION_COUNT; each += 1u)
            {
                held.frame = (strcmp(s_frame_text[each], relation + frame_prefix) == 0) ? each : held.frame;
            }
        }
        else
        {
            const char *const name =
                (strncmp(relation, KHW_READS_LADDER, ladder_prefix) == 0) ? (relation + ladder_prefix) : relation;
            for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
            {
                if (strcmp(s_anchor_text[anchor], name) == 0)
                {
                    held.anchor = anchor;
                    break;
                }
            }
        }
        if ((held.runs > KHW_RUNS) || !khw_runs_read(&held, line + at))
        {
            continue;
        }
        s_named[read] = held;
        read += 1u;
    }
    return read;
}

// The register-run probes of `held` emitted: each run set to the number of each of the two registers the kernel left
// a case word in, a question of the slot, and the round carried. These are the questions khw_fields_asked puts after
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

// The slot's questions emitted off the part for the vendor's disassembler to read back: each known form's fields and
// register-runs, none carried, a dry run handing them to the run's buffer (`orior protocol cross`). Where `forms`
// names a blob of the run's buffer holding forms a prior run learned, every form it holds is read and each one's bits
// turned one at a time (its fields and, with them, every bit a widening would turn) and its runs set to the two
// registers; where it names none, the kernel's own slot form alone, its bits turned. The slot and the forms are the
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
