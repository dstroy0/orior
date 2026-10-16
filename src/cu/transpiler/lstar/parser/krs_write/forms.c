// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: the vendor's run, the forms, costs and givens read, a form's cost and given, and the forms of a word ranked
#include "krs_write_internal.h"

// each form's cost as the part's .ksc keeps it: its encoding, its least launch m and its spread s
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned long long least;
    unsigned long long spread;
} KrsCost;

#define LADDER_TEXT(name_, text_, words_, measured_) text_,

static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};

#undef LADDER_TEXT

KrsForm s_form[KRS_FORMS];
unsigned int s_forms;
KrsFrameForm s_frame_form[KRS_FRAME_FORMS];
unsigned int s_frame_forms;
static KrsCost s_cost[KRS_FORMS];
static unsigned int s_costs;

// the lines the part's ruleset gave before this run, a word's line by its name, to keep where P6 does not separate
static char s_given[KRS_FORMS][KRS_LINE * 2u];

static char s_given_name[KRS_FORMS][KRS_NAME];
static unsigned int s_givens;

// The vendor's writer run through the interface in its krs mode, handed the part, the machine file, the container
// layout, the vendor's table and the name to give the forms as. Its output comes back over the interface's pipe, is
// printed, and is handed to the run's buffer as KRS_MACHINE_NAME. 1 where it ended clean
int krs_vendor_run(char *const *word)
{
    // the interface's command is a list of words it does not write to; the casts only meet its declared type
    char *const command[] = {word[6], word[2], word[3], word[4], word[5], (char *)KRS_FORMS_NAME, (char *)"krs", NULL};
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
        qry_hand(qry_run(), KRS_MACHINE_NAME, s_output, kept, QRY_ORDINARY);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("  krs_write: the vendor's writer did not end clean (%s, code %llx)\n", interface_ending_name(answer.ending),
               answer.code);
        return 0;
    }
    return 1;
}

// the forms the vendor's writer handed back, the `size` bytes at `bytes`, read into s_form, and those answering a frame
// relation into s_frame_form. A line whose relation is none the ladder holds and no frame relation is left out. The
// count read into s_form
unsigned int krs_forms_read(const unsigned char *bytes, unsigned long long size)
{
    char line[KRS_LINE + 300u];
    s_forms = 0u;
    s_frame_forms = 0u;
    unsigned long long next = 0ull;
    while ((next < size) && (s_forms < KRS_FORMS))
    {
        const unsigned char *const end = (const unsigned char *)memchr(bytes + next, '\n', (size_t)(size - next));
        const size_t length = (end != NULL) ? (size_t)(end - (bytes + next)) : (size_t)(size - next);
        snprintf(line, sizeof(line), "%.*s", (int)length, (const char *)(bytes + next));
        next += (unsigned long long)length + 1ull;
        line[strcspn(line, "\r\n")] = '\0';
        char relation[256];
        int signed_read = 0;
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        int at = 0;
        if (sscanf(line, "%255s %d %llx %llx %n", relation, &signed_read, &low, &high, &at) != 4)
        {
            continue;
        }
        if (strncmp(relation, KRS_READS_FRAME, strlen(KRS_READS_FRAME)) == 0)
        {
            if ((s_frame_forms < KRS_FRAME_FORMS) && (strlen(line + at) < KRS_FRAME_TEXT))
            {
                KrsFrameForm *const held = &s_frame_form[s_frame_forms];
                snprintf(held->relation, sizeof(held->relation), "%.*s", (int)(sizeof(held->relation) - 1u), relation);
                snprintf(held->text, sizeof(held->text), "%s", line + at);
                s_frame_forms += 1u;
            }
            continue;
        }
        const size_t prefix = strlen(KRS_READS_LADDER);
        const char *const name = (strncmp(relation, KRS_READS_LADDER, prefix) == 0) ? (relation + prefix) : relation;
        unsigned int anchor = (unsigned int)LADDER_ANCHOR_COUNT;
        for (unsigned int each = 0u; each < (unsigned int)LADDER_ANCHOR_COUNT; each += 1u)
        {
            anchor = (strcmp(s_anchor_text[each], name) == 0) ? each : anchor;
        }
        if ((anchor == (unsigned int)LADDER_ANCHOR_COUNT) || (strlen(line + at) >= KRS_LINE))
        {
            continue;
        }
        s_form[s_forms].anchor = anchor;
        s_form[s_forms].signed_read = signed_read;
        s_form[s_forms].low = low;
        s_form[s_forms].high = high;
        snprintf(s_form[s_forms].text, sizeof(s_form[s_forms].text), "%s", line + at);
        s_forms += 1u;
    }
    return s_forms;
}

// every form's cost the part's .ksc at `path` keeps, `run answers <least> cost <low> <high> <spread>`, into s_cost. The
// count read, 0 where the file is not there
unsigned int krs_costs_read(const char *path)
{
    s_costs = 0u;
    static KscFile s_file;
    static KscRow s_row;
    ksc_read(&s_file, path);
    for (unsigned int at = 0u; (at < s_file.lines) && (s_costs < KRS_FORMS); at += 1u)
    {
        KrsCost held;
        if (ksc_row_read(s_file.line[at], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            (sscanf(s_row.question, "cost %llx %llx %llx", &held.low, &held.high, &held.spread) == 3))
        {
            held.least = s_row.value;
            s_cost[s_costs] = held;
            s_costs += 1u;
        }
    }
    ksc_free(&s_file);
    return s_costs;
}

// the cost kept for the form first seen as `low` and `high`, or NULL where none is kept
static const KrsCost *krs_cost_of(unsigned long long low, unsigned long long high)
{
    for (unsigned int at = 0u; at < s_costs; at += 1u)
    {
        if ((s_cost[at].low == low) && (s_cost[at].high == high))
        {
            return &s_cost[at];
        }
    }
    return NULL;
}

// every form line the part's ruleset at `path` gave before this run, by the name it gives, into s_given. The count
unsigned int krs_given_read(const char *path)
{
    s_givens = 0u;
    unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ull;
    unsigned long long file_at = 0ull;
    if (!qry_file_read(qry_run(), path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[KRS_LINE * 2u];
    while ((qry_line_next(file_bytes, file_size, &file_at, line, sizeof(line))) && (s_givens < KRS_FORMS))
    {
        char kind[KRS_NAME];
        char name[KRS_NAME];
        if ((sscanf(line, "%63s %63s", kind, name) == 2) && (strcmp(kind, "form") == 0))
        {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(s_given[s_givens], sizeof(s_given[s_givens]), "%s", line);
            snprintf(s_given_name[s_givens], sizeof(s_given_name[s_givens]), "%s", name);
            s_givens += 1u;
        }
    }
    free(file_bytes);
    return s_givens;
}

// the line the part's ruleset gave `name` before this run, or NULL where it gave none
const char *krs_given_of(const char *name)
{
    for (unsigned int at = 0u; at < s_givens; at += 1u)
    {
        if (strcmp(s_given_name[at], name) == 0)
        {
            return s_given[at];
        }
    }
    return NULL;
}

unsigned int krs_fewest_operands(const unsigned int *writer, unsigned int writers)
{
    unsigned int found = 0u;
    unsigned int fewest = 0u;
    for (unsigned int at = 0u; at < writers; at += 1u)
    {
        const char *const text = s_form[writer[at]].text;
        unsigned int operands = (strchr(text, ' ') != NULL) ? 1u : 0u;
        for (const char *walk = text; *walk != '\0'; walk += 1)
        {
            operands += (*walk == ',') ? 1u : 0u;
        }
        if ((at == 0u) || (operands < fewest))
        {
            fewest = operands;
            found = at;
        }
    }
    return found;
}

// The form P6 gives a word, of the `writers` forms in `writer`: c_1, the form of least m, where the next least m is
// past it by more than s, the greatest spread of them. Its place in `writer`, or `writers` where a form has no cost kept
// or the least do not separate, the survivors then one cost
unsigned int krs_ranked(const unsigned int *writer, unsigned int writers)
{
    unsigned int first = writers;
    unsigned long long least = 0ull;
    unsigned long long next = 0ull;
    unsigned long long spread = 0ull;
    for (unsigned int at = 0u; at < writers; at += 1u)
    {
        const KrsCost *const cost = krs_cost_of(s_form[writer[at]].low, s_form[writer[at]].high);
        if (cost == NULL)
        {
            return writers;
        }
        spread = (cost->spread > spread) ? cost->spread : spread;
        if ((first == writers) || (cost->least < least))
        {
            next = (first == writers) ? cost->least : least;
            least = cost->least;
            first = at;
        }
        else if ((at == 1u) || (cost->least < next))
        {
            next = cost->least;
        }
    }
    return ((writers > 1u) && ((next - least) > spread)) ? first : writers;
}
