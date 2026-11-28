// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.c: the forms: the vendor's run, the forms, costs and givens read, a form's cost and given, and the forms of a
// word ranked
#include "../../../platform/platform.h"
#include "../../../parsers/line_read.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "krs_write_internal.h"
#include "memoria_operor/memoria_operor.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int forms_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// each form's cost as the part's .ksc stores it: its encoding, its least launch m and its spread s
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

// the lines the part's ruleset gave before this run, a word's line by its name, to retain where P6 does not separate
static char s_given[KRS_FORMS][KRS_LINE * 2u];

static char s_given_name[KRS_FORMS][KRS_NAME];
static unsigned int s_givens;

// The writer of the vendor, run through the interface in its krs mode (vendor_writer_run.h). It gets the part, the
// machine file, the container layout, the table of the vendor and the name for the forms.
// Its output goes to the run as KRS_MACHINE_NAME. The result is 1 where it ended clean
int krs_vendor_run(char *const *word)
{
    // the interface's command is a list of words it does not write to; the casts only meet its declared type
    char *const command[] = {word[6], word[2], word[3], word[4], word[5], (char *)FORMS_BLOB_NAME, (char *)"krs", NULL};
    return vendor_writer_run(command, KRS_MACHINE_NAME, "orior_krs_write", NULL);
}

// the forms that the writer of the vendor appended, the `size` bytes at `bytes` (forms_blob.h), read row by row into
// s_form. Those that answer a frame relation also go into s_frame_form.
// The code skips a row whose relation is none that the ladder satisfies, and no frame relation. The result is the
// count read into s_form
unsigned int krs_forms_read(const unsigned char *bytes, unsigned long long size)
{
    s_forms = 0u;
    s_frame_forms = 0u;
    const unsigned long long rows = size / sizeof(FormsBlobRow);
    for (unsigned long long at = 0ULL; (at < rows) && (s_forms < KRS_FORMS); at += 1ULL)
    {
        FormsBlobRow row;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &row, .src = bytes + (at * sizeof(FormsBlobRow)),
                   .bytes = sizeof(row));
        row.relation[sizeof(row.relation) - 1u] = '\0';
        row.text[sizeof(row.text) - 1u] = '\0';
        const size_t prefix =
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = KRS_READS_LADDER, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        const char *const name = (forms_text_order(row.relation, KRS_READS_LADDER, (size_t)(prefix)) == 0)
                                     ? (row.relation + prefix)
                                     : row.relation;
        unsigned int anchor = (unsigned int)LADDER_ANCHOR_COUNT;
        for (unsigned int each = 0u; each < (unsigned int)LADDER_ANCHOR_COUNT; each += 1u)
        {
            anchor = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_anchor_text[each],
                                 .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = name, .ci = EMBED_FALSE))
                         ? each
                         : anchor;
        }
        // the form of a frame relation or of a predicate relation goes out as a form of the schema. That is the form
        // that the table beside this writer maps it to. A predicate relation read as signed goes under its relation
        // with `.signed` after it
        const int predicate = (anchor != (unsigned int)LADDER_ANCHOR_COUNT) && ladder_predicate(anchor);
        if ((forms_text_order(row.relation, KRS_READS_FRAME,
                              (size_t)(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = KRS_READS_FRAME,
                                                  .cap = PLATFORM_TEXT_TO_TERMINATOR))) == 0) ||
            predicate)
        {
            static const char s_signed[] = ".signed";
            const size_t relation =
                EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = row.relation, .cap = PLATFORM_TEXT_TO_TERMINATOR);
            const size_t marked = (predicate && (row.signed_read != 0u)) ? (sizeof(s_signed) - 1u) : 0u;
            if ((s_frame_forms < KRS_FRAME_FORMS) && ((relation + marked) < KRS_FRAME_RELATION))
            {
                KrsFrameForm *const frame_form = &s_frame_form[s_frame_forms];
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = frame_form->relation, .src = row.relation, .bytes = relation);
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = frame_form->relation + relation, .src = s_signed,
                           .bytes = marked);
                frame_form->relation[relation + marked] = '\0';
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = frame_form->text, .src = row.text, .bytes = sizeof(row.text));
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
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_form[s_forms].text, .src = row.text, .bytes = sizeof(row.text));
        s_forms += 1u;
    }
    return s_forms;
}

// every form's cost the part's .ksc at `path` stores, `run answers <least> cost <low> <high> <spread>`, into s_cost.
// The count read, 0 where the file is not there
unsigned int krs_costs_read(const char *path)
{
    s_costs = 0u;
    static KscFile s_file;
    static KscRow s_row;
    ksc_read(&s_file, path);
    for (unsigned int at = 0u; (at < s_file.lines) && (s_costs < KRS_FORMS); at += 1u)
    {
        KrsCost cost;
        const char *question = s_row.question;
        if (ksc_row_read(s_file.line[at], "run", &s_row) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "answers", .ci = EMBED_FALSE)) &&
            line_characters(&question, "cost ") && line_number(&question, 1, &cost.low) &&
            line_number(&question, 1, &cost.high) && line_number(&question, 1, &cost.spread))
        {
            cost.least = s_row.value;
            s_cost[s_costs] = cost;
            s_costs += 1u;
        }
    }
    ksc_free(&s_file);
    return s_costs;
}

// the stored cost for the form that first came as `low` and `high`, or NULL where the store holds none
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
    unsigned long long file_size = 0ULL;
    unsigned long long file_at = 0ULL;
    if (!qry_member_file_blob(path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[KRS_LINE * 2u];
    while ((line_read(file_bytes, file_size, &file_at, line, sizeof(line))) && (s_givens < KRS_FORMS))
    {
        char type[KRS_NAME];
        char name[KRS_NAME];
        const char *at = line;
        if (line_word(&at, type, sizeof(type)) && line_word(&at, name, sizeof(name)) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "form",
                        .ci = EMBED_FALSE)))
        {
            line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "\r\n")] = '\0';
            {
                VerbaLinea buffer_line = mmgr_line_over(s_given[s_givens], sizeof(s_given[s_givens]));
                mmgr_line_put(&buffer_line, line);
                (void)mmgr_line_finish(&buffer_line);
            }
            {
                VerbaLinea buffer_line = mmgr_line_over(s_given_name[s_givens], sizeof(s_given_name[s_givens]));
                mmgr_line_put(&buffer_line, name);
                (void)mmgr_line_finish(&buffer_line);
            }
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
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_given_name[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = name, .ci = EMBED_FALSE))
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
            char place_text[8];
            {
                VerbaLinea buffer_line = mmgr_line_over(place_text, sizeof(place_text));
                mmgr_line_put(&buffer_line, "{");
                mmgr_line_decimal(&buffer_line, place, 1u);
                mmgr_line_put(&buffer_line, "}");
                (void)mmgr_line_finish(&buffer_line);
            }
            carries = carries && (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = form->text,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = place_text,
                                             .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL);
        }
        if (!carries)
        {
            continue;
        }
        unsigned int operands = (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = form->text,
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ' ') != NULL)
                                    ? 1u
                                    : 0u;
        // each comma, reached from the one before it
        for (const char *comma = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = form->text,
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ',');
             comma != NULL; comma = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = comma + 1,
                                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ','))
        {
            operands += 1u;
        }
        if ((found == writers) || (operands < fewest))
        {
            fewest = operands;
            found = at;
        }
    }
    return (found == writers) ? 0u : found;
}

// The form that P6 gives a word, of the `writers` forms in `writer`. It is c_1, the form of least m, where the next
// least m is past it by more than s, the greatest spread of them. The result is its place in `writer`. It is `writers`
// where a form has no stored cost or the least do not separate. The survivors are then one cost
unsigned int krs_ranked(const unsigned int *writer, unsigned int writers)
{
    unsigned int first = writers;
    unsigned long long least = 0ULL;
    unsigned long long next = 0ULL;
    unsigned long long spread = 0ULL;
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
