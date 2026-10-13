// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// written.c: what the pass writes: the run's texts, the runs read, the answers, the working set, the costs and the fields written, the vendor's run, the forms read, and the pass enumerated
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

static void khw_run_printed(KhwText *text, const KhwAnswered *held, unsigned int run)
{
    if (held->place[run] == KHW_PLACE_NONE)
    {
        khw_text_added(text, " %u %u %s", held->first[run], held->last[run], KHW_PLACE_NONE_TEXT);
        return;
    }
    khw_text_added(text, " %u %u %u", held->first[run], held->last[run], held->place[run]);
}

// the runs of `held` read from `text`, `held->runs` of them: three words a run, its first bit, its last and its place,
// or two, its first bit and its last, where the line holds no place, every run then holding none. 1, or 0 where the
// words are neither
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
            held->place[run] =
                (strcmp(place, KHW_PLACE_NONE_TEXT) == 0) ? KHW_PLACE_NONE : (unsigned int)strtoul(place, NULL, 10);
        }
        rest += step;
    }
    return 1;
}

// the forms the part answered for handed to the run's buffer as KHW_FORMS_NAME, one a line, for the vendor's writer to
// name and lay out: the relation the gate read, whether the answers read a word signed and whether the part was asked,
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
        khw_text_added(&text, "%s%s %d %d %016llx %016llx %u", KHW_READS_LADDER, s_anchor_text[held->anchor],
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
        khw_text_added(&text, "%u %d %s%s %d %d %016llx %016llx %u", held->phase, held->swapped, KHW_READS_LADDER,
                       s_anchor_text[held->anchor], held->signed_read, held->signedness_asked, held->low, held->high,
                       held->runs);
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            khw_run_printed(&text, held, run);
        }
        khw_text_added(&text, "\n");
    }
    return khw_text_handed(&text, KHW_WORK_NAME);
}

// The cost of every classified form, asked (P6): each put at the slot over its relation's cases, timed over
// KHW_LAUNCHES launches, each launch alone, and its runs read as the least one took and the spread past it, m_c and
// s_c. Each is written to the part's .ksc at `path`, the .ksc's every other line kept as it was and the row of a form
// timed again put in place of the one before. The count timed
unsigned int khw_costs_asked(const char *path)
{
    static unsigned long long s_low[KHW_FORMS];
    static unsigned long long s_high[KHW_FORMS];
    static unsigned long long s_least[KHW_FORMS];
    static unsigned long long s_spread[KHW_FORMS];
    static unsigned int s_word[RUN_CASES_MOST][2];
    static unsigned int s_plain[RUN_CASES_MOST];
    static unsigned int s_signed[RUN_CASES_MOST];
    static unsigned long long s_answered[RUN_CASES_MOST];
    unsigned int timed = 0u;
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        if (held->phase != KHW_PHASE_CLASSIFIED)
        {
            continue;
        }
        khw_code_put(s_code, s_slot, held->low, held->high);
        const unsigned int count = khw_anchor_cases(held->anchor, held->swapped, s_word, s_plain, s_signed);
        s_launches = KHW_LAUNCHES;
        const int answered = khw_asked(s_word, count, s_slot, s_answered);
        s_launches = 0u;
        if (!answered || (s_question.most == 0ull))
        {
            continue;
        }
        s_low[timed] = held->low;
        s_high[timed] = held->high;
        s_least[timed] = s_question.least;
        s_spread[timed] = s_question.most - s_question.least;
        timed += 1u;
    }
    // the .ksc as the run holds it, the row of each form timed now in place of the one before, after the run channel's
    // last row, and handed to the run's writer whole (ksc.h)
    static KscFile s_file;
    const QryWho who = QRY_WHO_HERE;
    int written = ksc_read(&s_file, path);
    for (unsigned int at = 0u; written && (at < timed); at += 1u)
    {
        char row[128];
        char asked[64];
        snprintf(row, sizeof(row), KHW_COST_ROW, s_least[at], s_low[at], s_high[at], s_spread[at]);
        snprintf(asked, sizeof(asked), KHW_COST_ASKED, s_low[at], s_high[at]);
        const char *const rows[1] = {row};
        written = ksc_rows_replaced_by(&s_file, "run", asked, rows, 1u, who);
    }
    if (!written || !ksc_write_by(&s_file, path, who))
    {
        printf("  khw_write: %s could not be written through the run's writer\n", path);
    }
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
        // the relation named back to its anchor, for a form read in to write the same relation out
        held.anchor = (unsigned int)LADDER_ANCHOR_COUNT;
        const size_t prefix = strlen(KHW_READS_LADDER);
        const char *const name = (strncmp(relation, KHW_READS_LADDER, prefix) == 0) ? (relation + prefix) : relation;
        for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
        {
            if (strcmp(s_anchor_text[anchor], name) == 0)
            {
                held.anchor = anchor;
                break;
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
// register-runs, none carried, a dry run handing them to the run's buffer (measuring_stick_query.sh). Where `forms`
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
