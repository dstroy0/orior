// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: the vendor's run, the forms, costs and givens read, a form's cost and given, and the forms of a
// word ranked
#include "../../../parsers/line_read.h"
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
// printed, and is handed to the run as KRS_MACHINE_NAME. 1 where it ended clean
int krs_vendor_run(char *const *word)
{
    // the interface's command is a list of words it does not write to; the casts only meet its declared type
    char *const command[] = {word[6], word[2], word[3], word[4], word[5], (char *)FORMS_BLOB_NAME, (char *)"krs", NULL};
    static char s_output[65536];
    const InterfaceProbe probe = {command, NULL, 0ull};
    InterfaceAnswer answer = {0};
    answer.output = s_output;
    answer.output_capacity = sizeof(s_output);
    EngineError error = {0};
    // what this process handed moved into the run before the writer reads it, and what the writer handed once it ends
    qry_member_settled();
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_member_rings_settled();
    if (answer.output_bytes != 0ull)
    {
        printf("%s", s_output);
        const unsigned long long kept =
            (answer.output_bytes < (sizeof(s_output) - 1u)) ? answer.output_bytes : (sizeof(s_output) - 1u);
        qry_member_append(KRS_MACHINE_NAME, s_output, kept, QRY_ORDINARY);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("  krs_write: the vendor's writer did not end clean (%s, code %llx)\n",
               interface_ending_name(answer.ending), answer.code);
        return 0;
    }
    return 1;
}

// the forms the vendor's writer appended, the `size` bytes at `bytes` (forms_blob.h), read row by row into s_form, and
// those answering a frame relation into s_frame_form. A row whose relation is none the ladder holds and no frame
// relation is left out. The count read into s_form
unsigned int krs_forms_read(const unsigned char *bytes, unsigned long long size)
{
    s_forms = 0u;
    s_frame_forms = 0u;
    const unsigned long long rows = size / sizeof(FormsBlobRow);
    for (unsigned long long at = 0ull; (at < rows) && (s_forms < KRS_FORMS); at += 1ull)
    {
        FormsBlobRow row;
        memcpy(&row, bytes + (at * sizeof(FormsBlobRow)), sizeof(row));
        row.relation[sizeof(row.relation) - 1u] = '\0';
        row.text[sizeof(row.text) - 1u] = '\0';
        const size_t prefix = strlen(KRS_READS_LADDER);
        const char *const name =
            (strncmp(row.relation, KRS_READS_LADDER, prefix) == 0) ? (row.relation + prefix) : row.relation;
        unsigned int anchor = (unsigned int)LADDER_ANCHOR_COUNT;
        for (unsigned int each = 0u; each < (unsigned int)LADDER_ANCHOR_COUNT; each += 1u)
        {
            anchor = (strcmp(s_anchor_text[each], name) == 0) ? each : anchor;
        }
        // a frame relation's form, and a predicate relation's, is written as the schema's form the table beside this
        // writer maps it to, a predicate relation read signed under its relation with `.signed` after it
        const int predicate = (anchor != (unsigned int)LADDER_ANCHOR_COUNT) && ladder_predicate(anchor);
        if ((strncmp(row.relation, KRS_READS_FRAME, strlen(KRS_READS_FRAME)) == 0) || predicate)
        {
            static const char s_signed[] = ".signed";
            const size_t relation = strlen(row.relation);
            const size_t marked = (predicate && (row.signed_read != 0u)) ? (sizeof(s_signed) - 1u) : 0u;
            if ((s_frame_forms < KRS_FRAME_FORMS) && ((relation + marked) < KRS_FRAME_RELATION))
            {
                KrsFrameForm *const held = &s_frame_form[s_frame_forms];
                memcpy(held->relation, row.relation, relation);
                memcpy(held->relation + relation, s_signed, marked);
                held->relation[relation + marked] = '\0';
                memcpy(held->text, row.text, sizeof(row.text));
                s_frame_forms += 1u;
            }
            continue;
        }
        if (anchor == (unsigned int)LADDER_ANCHOR_COUNT)
        {
            continue;
        }
        s_form[s_forms].anchor = anchor;
        s_form[s_forms].signed_read = (int)row.signed_read;
        s_form[s_forms].low = row.low;
        s_form[s_forms].high = row.high;
        memcpy(s_form[s_forms].text, row.text, sizeof(row.text));
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
        const char *question = s_row.question;
        if (ksc_row_read(s_file.line[at], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            line_letters(&question, "cost ") && line_number(&question, 1, &held.low) &&
            line_number(&question, 1, &held.high) && line_number(&question, 1, &held.spread))
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
    const unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ull;
    unsigned long long file_at = 0ull;
    if (!qry_member_file_blob(path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[KRS_LINE * 2u];
    while ((line_read(file_bytes, file_size, &file_at, line, sizeof(line))) && (s_givens < KRS_FORMS))
    {
        char kind[KRS_NAME];
        char name[KRS_NAME];
        const char *at = line;
        if (line_word(&at, kind, sizeof(kind)) && line_word(&at, name, sizeof(name)) && (strcmp(kind, "form") == 0))
        {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(s_given[s_givens], sizeof(s_given[s_givens]), "%s", line);
            snprintf(s_given_name[s_givens], sizeof(s_given_name[s_givens]), "%s", name);
            s_givens += 1u;
        }
    }
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
    unsigned int found = writers;
    unsigned int fewest = 0u;
    for (unsigned int at = 0u; at < writers; at += 1u)
    {
        const KrsForm *const form = &s_form[writer[at]];
        // a form whose text leaves a place of its relation's tuple unwritten carries no word there and writes none
        const unsigned int words = s_ladder_words[form->anchor];
        int carries = 1;
        for (unsigned int place = 0u; place <= words; place += 1u)
        {
            char held[8];
            snprintf(held, sizeof(held), "{%u}", place);
            carries = carries && (strstr(form->text, held) != NULL);
        }
        if (!carries)
        {
            continue;
        }
        unsigned int operands = (strchr(form->text, ' ') != NULL) ? 1u : 0u;
        for (const char *walk = form->text; *walk != '\0'; walk += 1)
        {
            operands += (*walk == ',') ? 1u : 0u;
        }
        if ((found == writers) || (operands < fewest))
        {
            fewest = operands;
            found = at;
        }
    }
    return (found == writers) ? 0u : found;
}

// The form P6 gives a word, of the `writers` forms in `writer`: c_1, the form of least m, where the next least m is
// past it by more than s, the greatest spread of them. Its place in `writer`, or `writers` where a form has no cost
// kept or the least do not separate, the survivors then one cost
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
