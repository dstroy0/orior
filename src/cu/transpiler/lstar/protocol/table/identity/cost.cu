// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cost.cu: the cost: each arrangement put at a link of its operator's word and the part's clock read around it
#include "../../../../../../../includes/parsers/who.h"
#include "../../../../../../platform/platform.h"
#include "identity_internal.h"
#include "memoria_operor/memoria_operor.h"

// The link an operator's arrangements go at. The word they produce, the link, its two operands and the register it
// writes, and the first register past the chain's own. Then the link's line and its lines. Last, the chain cut with the
// clock read twice in the link's place, a text of the temporary tier the caller gives back
struct OperatorLink
{
    const Word *word;
    FoundLink link;
    IdentityName operands[2];
    IdentityName to;
    unsigned int base;
    long link_at;
    long form_lines;
    PlatformText clock_cut;
};

// the arrangement of the one node `precept` over `left` and `right`, written as arrangement_written writes it onto
// `out`: 1, or 0 and nothing put
static int node_written(const Ruleset *sass, unsigned char precept, unsigned char left, unsigned char right,
                        const char *const *operands, unsigned int count, const char *ones, const char *to,
                        unsigned int next, PlatformText *out)
{
    PreceptTree tree;
    tree.count = 1u;
    const PreceptNode node = {precept, left, right};
    tree.node[0] = node;
    return arrangement_written(sass, &tree, operands, count, ones, to, next, out);
}

// 1 where the terminated `text`, whole, is a register of the file, R and its number
static int named_register(const char *text)
{
    static TablePatternOnce s_named;
    TableMatch match;
    return table_matched(table_once(&s_named, "R[0-9]+", 1), text,
                         EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR),
                         &match);
}

// 1 where `after` names the register `written` again, a register of the file not followed by a digit
static int named_again(CarrierPiece after, const char *written)
{
    PlatformText source;
    identity_text_begun(&source);
    mmgr_line_put(&source.line, "(^|[^A-Za-z0-9_])");
    mmgr_line_put(&source.line, written);
    mmgr_line_put(&source.line, "([^0-9]|$)");
    TablePattern pattern;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &pattern, .bytes = sizeof(pattern), .val = 0);
    const int read = table_pattern_read(&pattern, platform_text_bytes(&source));
    platform_text_ended(&source);
    TableMatch match;
    const int found = read && table_search(&pattern, after.text, after.length, &match);
    if (read)
    {
        table_pattern_ended(&pattern);
    }
    return found;
}

// the lines a found text holds: its line ends, and one more where it does not end at one
static long found_lines(CarrierPiece found)
{
    long lines = 0;
    for (size_t at = 0u; at < found.length; at += 1u)
    {
        lines += (found.text[at] == '\n') ? 1 : 0;
    }
    return lines + (((found.length != 0u) && (found.text[found.length - 1u] == '\n')) ? 0 : 1);
}

// The clock read twice at the link `found_link` of `chain`, with the difference of the two, cut into the chain in the
// link's place, onto `cut`. 1, or 0 where it cannot cut the chain there
static int clock_cut_written(const Ruleset *sass, const CarrierChain *chain, const FoundLink *found_link,
                             CarrierPiece found, PlatformText *cut)
{
    const unsigned int base = chain->registers;
    const IdentityName start = ruleset_register_name(sass, "file", base + 1u);
    const IdentityName end = ruleset_register_name(sass, "file", base + 2u);
    const char *const clock_operands[2] = {end.text, start.text};
    PlatformText clock;
    identity_text_begun(&clock);
    mmgr_line_put(&clock.line, "\tCS2R.32 \t");
    mmgr_line_put(&clock.line, start.text);
    mmgr_line_put(&clock.line, ", SR_CLOCKLO;\n\tCS2R.32 \t");
    mmgr_line_put(&clock.line, end.text);
    mmgr_line_put(&clock.line, ", SR_CLOCKLO;\n");
    (void)node_written(sass, PRECEPT_SUB, PRECEPT_LEFT, PRECEPT_RIGHT, clock_operands, 2u, "", end.text, base + 4u,
                       &clock);
    const char *const clock_bytes = platform_text_bytes(&clock);
    const int cut_ok =
        carrier_cut_at_read(&chain->links, chain->cases_defined, chain->text, found_link->link_at, found_lines(found),
                            end.text, (clock_bytes != NULL) ? clock_bytes : "", 0, cut);
    platform_text_ended(&clock);
    return cut_ok;
}

// The link the arrangements of an operator go at, its `own_count` rows of `rows` at the places `own`. It is the first
// of the links of the word of the web they produce (word_produced), in the order the pair puts links. A later link
// reads its result, it carries the cases, and its operands and result are registers. Its chain names its registers,
// and its chain takes the clock read in its place. 1; 0 with `link->word` NULL where they produce no word the ruleset
// writes with two reads, and 0 where no link is such
static int operator_link(const Ruleset *sass, const Carriers *carriers, unsigned long seed, const char *operator_name,
                         const CostRows *rows, const unsigned long long *own, unsigned long long own_count,
                         OperatorLink *link)
{
    link->word = NULL;
    identity_text_begun(&link->clock_cut);
    for (unsigned long long at = 0ULL; (link->word == NULL) && (at < own_count); at += 1ULL)
    {
        const CostRow *const row = &rows->row[own[at]];
        link->word = (row->tree.count == 0u) ? NULL : word_produced(&row->tree);
    }
    FormNames names;
    names.count = 0u;
    PlatformText text;
    identity_text_begun(&text);
    const int written_form = (link->word != NULL) && marked(sass, link->word->name, &names, &text);
    const char *const text_bytes = platform_text_bytes(&text);
    if (!written_form || (text_bytes == NULL) || (text_bytes[0] == '\0') || (names.count != 3u))
    {
        link->word = NULL;
        platform_text_ended(&text);
        return 0;
    }
    static thread_local FormGroups t_group;
    FormGroups &group = t_group;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &group, .bytes = sizeof(group), .val = 0);
    PlatformArray found_links;
    platform_array_begun(&found_links, sizeof(FoundLink), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    links_of(carriers, seed, text_bytes, &group, operator_name, &found_links);
    platform_text_ended(&text);
    int linked = 0;
    for (unsigned long long found_at = 0ULL; !linked && (found_at < found_links.count); found_at += 1ULL)
    {
        const FoundLink found_link = *(const FoundLink *)platform_array_at(&found_links, found_at);
        const CarrierChain *const chain = carrier_chain_of(carriers, found_link.chain.text);
        if (chain == NULL)
        {
            continue;
        }
        const char *const chain_text = chain->text;
        // the value of each of the word's three parameters, by its place: what the group of its marker matched
        IdentityName given[3];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = given, .bytes = sizeof(given), .val = 0);
        for (unsigned int marker = 0u; marker < FORM_MARKERS; marker += 1u)
        {
            const unsigned int parameter = marker - (unsigned int)'A';
            const unsigned int at_group = group.group[marker];
            if ((at_group != 0u) && (marker >= (unsigned int)'A') && (parameter < 3u) && (at_group < FOUND_GROUPS))
            {
                const CarrierPiece value = found_group(&found_link, at_group, chain_text);
                (void)IDENTITY_SET(given[parameter], value.text, value.length);
            }
        }
        const CarrierPiece found = found_group(&found_link, 0u, chain_text);
        const CarrierPiece after = {found.text + found.length,
                                    chain->length - (size_t)(found.text + found.length - chain_text)};
        if (!named_register(given[0].text) || !named_register(given[1].text) || !named_register(given[2].text) ||
            !named_again(after, given[0].text) ||
            !carrier_form_carries(&chain->links, chain->cases_defined, found, found_link.link_at) ||
            (chain->registers == 0u))
        {
            continue;
        }
        platform_text_reset(&link->clock_cut);
        if (!clock_cut_written(sass, chain, &found_link, found, &link->clock_cut))
        {
            continue;
        }
        link->link = found_link;
        link->operands[0] = given[1];
        link->operands[1] = given[2];
        link->to = given[0];
        link->base = chain->registers;
        link->link_at = found_link.link_at;
        link->form_lines = found_lines(found);
        linked = 1;
    }
    platform_array_ended(&found_links);
    return linked;
}

// `placed` with each place `{n}` filled as the relation's tuple is, onto `line`: `{0}` the first word, `{1}` the second
// and `{2}` the answer, the register written `to`
static void placed_filled(const char *placed, const char *to, const char *left, const char *right, VerbaLinea *line)
{
    const char *const filled[3] = {left, right, to};
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = placed, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (size_t at = 0u; at < length; at += 1u)
    {
        const int place = ((at + 2u) < length) && (placed[at] == '{') && (placed[at + 1u] >= '0') &&
                          (placed[at + 1u] <= '2') && (placed[at + 2u] == '}');
        if (place)
        {
            mmgr_line_put(line, filled[placed[at + 1u] - '0']);
            at += 2u;
            continue;
        }
        mmgr_line_ch(line, placed[at]);
    }
}

// the machine file's forms a cost queries, as the vendor's writer placed them. The blob's row, then the least and the
// max its clock read, and what came of it
struct CostForm
{
    FormsBlobRow row;
    unsigned long long least;
    unsigned long long max;
    CostVerdict verdict;
};

// Every question of the job, written before the job puts any. Its code, the registers it declares, the row it queries
// of, or the link whose clock it reads alone where it is past every row. Then whether it checks the row against the
// host or reads its clock. `form` is the machine file's form the question reads the clock over, -1 where it is an
// arrangement's or a link's own
struct Planned
{
    IdentityCode code;
    unsigned int mark;
    unsigned long long row;
    int host_checked;
    IdentityName chain;
    long form;
};

// the terminated `text` put into code by the vendor's writer into `code`, which it replaces; nothing where the text is
// empty
static void assembled(const char *machine_path, const char *text, IdentityCode *code)
{
    platform_array_ended(code);
    if ((text != NULL) && (text[0] != '\0'))
    {
        vendor_assembled(machine_path, text, code);
    }
}

// a line of the cost's table on `table`: `head`, `first` and `second`, then a line's end
static void table_said(RunWritten *table, const char *head, const char *first, const char *second)
{
    mmgr_line_put(&table->text.line, head);
    mmgr_line_put(&table->text.line, first);
    mmgr_line_put(&table->text.line, second);
    mmgr_line_ch(&table->text.line, '\n');
}

// the greatest common divisor of two counts, 0 where both are 0
static unsigned long long cost_divisor(unsigned long long one, unsigned long long other)
{
    while (other != 0ULL)
    {
        const unsigned long long rest = one % other;
        one = other;
        other = rest;
    }
    return one;
}

// a row's verdict where it has none
static void row_left(CostRow *row, const char *verdict)
{
    if (row->verdict.text[0] == '\0')
    {
        (void)IDENTITY_SET_TEXT(row->verdict, verdict);
    }
}

// the end of the run of one operator's rows in the rows' order by operator that begins at `at`
static unsigned long long operator_end(const CostRows *rows, unsigned long long at)
{
    unsigned long long end = at;
    while ((end < rows->row_count) && identity_same(rows->row[rows->by_operator[end]].field[0].text,
                                                    rows->row[rows->by_operator[at]].field[0].text))
    {
        end += 1ULL;
    }
    return end;
}

// the places of the cases the cost goes over
static unsigned int s_cost_places[RUN_CASES_MAX];

// What the cost holds while it writes and puts its questions, each a cell of the temporary tier. The forms, the plan,
// the least each link's clock read alone, and the clock of each row's link, COST_NONE where it has none
#define COST_NONE (~0ULL)

struct CostHeld
{
    CostForm *form;
    unsigned long long form_count;
    Planned *planned;
    unsigned long long planned_count;
    unsigned long long *clock_least;
    unsigned long long clock_count;
    unsigned long long *row_link;
};

static void cost_held_ended(CostHeld *held)
{
    for (unsigned long long at = 0ULL; (held->planned != NULL) && (at < held->planned_count); at += 1ULL)
    {
        platform_array_ended(&held->planned[at].code);
    }
    platform_cell_released(held->row_link, platform_place_g_program_owner);
    platform_cell_released(held->clock_least, platform_place_g_program_owner);
    platform_cell_released(held->planned, platform_place_g_program_owner);
    platform_cell_released(held->form, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = held, .bytes = sizeof(*held), .val = 0);
}

// The forms of the blob `forms`, the plan's room and the clocks' room, into `held`, each cell taken once at its bound.
// Two questions a row and one clock an operator, at max one a row, and one question a form. 1, or 0 where the pool
// cannot meet a cell
static int cost_held_taken(const char *forms, const CostRows *rows, CostHeld *held)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = held, .bytes = sizeof(*held), .val = 0);
    // the latest blob of `forms` read where the run stores it, one row a form (forms_blob.h)
    const unsigned char *blob = NULL;
    unsigned long long blob_size = 0ULL;
    if ((forms == NULL) || !qry_member_blob(forms, &blob, &blob_size, NULL))
    {
        blob = NULL;
        blob_size = 0ULL;
    }
    const unsigned long long form_count = blob_size / sizeof(FormsBlobRow);
    const unsigned long long rows_count = rows->row_count;
    held->form = (CostForm *)platform_cell_cleared((form_count != 0ULL) ? form_count : 1ULL, sizeof(CostForm),
                                                   platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    const unsigned long long planned_room = (3ULL * rows_count) + form_count + 1ULL;
    held->planned = (Planned *)platform_cell_cleared(planned_room, sizeof(Planned), platform_place_g_program_owner,
                                                     PLATFORM_TIER_TEMPORARY);
    held->clock_least = (unsigned long long *)platform_cell_cleared(
        rows_count + 1ULL, sizeof(unsigned long long), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    held->row_link = (unsigned long long *)platform_cell_cleared(
        rows_count + 1ULL, sizeof(unsigned long long), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((held->form == NULL) || (held->planned == NULL) || (held->clock_least == NULL) || (held->row_link == NULL))
    {
        cost_held_ended(held);
        return 0;
    }
    for (unsigned long long at = 0ULL; at < rows_count; at += 1ULL)
    {
        held->row_link[at] = COST_NONE;
    }
    for (unsigned long long row_at = 0ULL; row_at < form_count; row_at += 1ULL)
    {
        CostForm *const form = &held->form[row_at];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &form->row, .src = blob + (row_at * sizeof(FormsBlobRow)),
                   .bytes = sizeof(form->row));
        form->row.relation[sizeof(form->row.relation) - 1u] = '\0';
        form->row.text[sizeof(form->row.text) - 1u] = '\0';
        const int two_words = identity_has(form->row.text, "{0}") && identity_has(form->row.text, "{1}") &&
                              identity_has(form->row.text, "{2}") && !identity_has(form->row.text, "{3}");
        form->least = ULLONG_MAX;
        form->max = 0ULL;
        (void)IDENTITY_SET_TEXT(form->verdict, two_words ? "" : "no two-word places");
    }
    held->form_count = form_count;
    return 1;
}

// a question put after the plan's others
static void planned_added(CostHeld *held, const IdentityCode *code, unsigned int mark, unsigned long long row,
                          int host_checked, const IdentityName *chain, long form)
{
    Planned *const plan = &held->planned[held->planned_count];
    plan->code = *code;
    plan->mark = mark;
    plan->row = row;
    plan->host_checked = host_checked;
    plan->chain = *chain;
    plan->form = form;
    held->planned_count += 1ULL;
}
// The questions of one operator's arrangements at its link onto the plan. The link's clock alone, then each
// arrangement written at the link, and its clock read over it written COST_REPEATS times. Each time reads the last
// time's result as its first operand
static void operator_planned(const Ruleset *sass, const char *machine_path, OperatorLink *at_link,
                             const CarrierChain *chain, CostRows *rows, const unsigned long long *own,
                             unsigned long long own_count, CostHeld *held)
{
    const unsigned int base = at_link->base;
    const RegexpSpan whole = at_link->link.group[0];
    const IdentityName ones = ruleset_register_name(sass, "file", base);
    const IdentityName start = ruleset_register_name(sass, "file", base + 1u);
    const IdentityName end = ruleset_register_name(sass, "file", base + 2u);
    const IdentityName last = ruleset_register_name(sass, "file", base + 3u);
    const unsigned int mark = base + 4u + PRECEPT_NODES;
    PlatformText ones_written;
    identity_text_begun(&ones_written);
    (void)node_written(sass, PRECEPT_NOT, PRECEPT_ZERO, PRECEPT_NONE, NULL, 0u, "", ones.text, base + 4u,
                       &ones_written);
    const char *const ones_bytes = platform_text_bytes(&ones_written);
    const char *const ones_text = (ones_bytes != NULL) ? ones_bytes : "";
    const unsigned long long clock_at = held->clock_count;
    held->clock_least[clock_at] = ULLONG_MAX;
    held->clock_count += 1ULL;
    {
        IdentityCode code;
        identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
        assembled(machine_path, platform_text_bytes(&at_link->clock_cut), &code);
        planned_added(held, &code, mark, rows->row_count + clock_at, 0, &at_link->link.chain, -1);
    }
    const char *const operands[2] = {at_link->operands[0].text, at_link->operands[1].text};
    for (unsigned long long own_at = 0ULL; own_at < own_count; own_at += 1ULL)
    {
        const unsigned long long at = own[own_at];
        CostRow *const row = &rows->row[at];
        if (row->verdict.text[0] != '\0')
        {
            continue;
        }
        PlatformText located;
        identity_text_begun(&located);
        if (!arrangement_written(sass, &row->tree, operands, 2u, ones.text, at_link->to.text, base + 4u, &located) ||
            (ones_text[0] == '\0'))
        {
            platform_text_ended(&located);
            (void)IDENTITY_SET_TEXT(row->verdict, "unwritable");
            continue;
        }
        PlatformText whole_text;
        identity_text_begun(&whole_text);
        (void)mmgr_line_put_n(&whole_text.line, chain->text, (size_t)whole.from);
        mmgr_line_put(&whole_text.line, ones_text);
        identity_text_put(&whole_text, &located);
        mmgr_line_put(&whole_text.line, chain->text + whole.to);
        platform_text_ended(&located);
        IdentityCode code;
        identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
        assembled(machine_path, platform_text_bytes(&whole_text), &code);
        platform_text_ended(&whole_text);
        if (code.count == 0ULL)
        {
            platform_array_ended(&code);
            (void)IDENTITY_SET_TEXT(row->verdict, "unassembled");
            continue;
        }
        PlatformText block;
        identity_text_begun(&block);
        mmgr_line_put(&block.line, ones_text);
        mmgr_line_put(&block.line, "\tCS2R.32 \t");
        mmgr_line_put(&block.line, start.text);
        mmgr_line_put(&block.line, ", SR_CLOCKLO;\n");
        for (unsigned int again = 0u; again < COST_REPEATS; again += 1u)
        {
            const char *const repeated[2] = {(again == 0u) ? at_link->operands[0].text : last.text,
                                             at_link->operands[1].text};
            (void)arrangement_written(sass, &row->tree, repeated, 2u, ones.text, last.text, base + 4u, &block);
        }
        const char *const clock_operands[2] = {end.text, start.text};
        mmgr_line_put(&block.line, "\tCS2R.32 \t");
        mmgr_line_put(&block.line, end.text);
        mmgr_line_put(&block.line, ", SR_CLOCKLO;\n");
        (void)node_written(sass, PRECEPT_SUB, PRECEPT_LEFT, PRECEPT_RIGHT, clock_operands, 2u, "", end.text, base + 4u,
                           &block);
        PlatformText cut;
        identity_text_begun(&cut);
        const char *const block_bytes = platform_text_bytes(&block);
        (void)carrier_cut_at_read(&chain->links, chain->cases_defined, chain->text, at_link->link_at,
                                  at_link->form_lines, end.text, (block_bytes != NULL) ? block_bytes : "", 0, &cut);
        platform_text_ended(&block);
        IdentityCode clocked;
        identity_code_begun(&clocked, PLATFORM_TIER_TEMPORARY);
        assembled(machine_path, platform_text_bytes(&cut), &clocked);
        platform_text_ended(&cut);
        if (clocked.count == 0ULL)
        {
            platform_array_ended(&code);
            platform_array_ended(&clocked);
            (void)IDENTITY_SET_TEXT(row->verdict, "unassembled");
            continue;
        }
        planned_added(held, &code, mark, at, 1, &at_link->link.chain, -1);
        planned_added(held, &clocked, mark, at, 0, &at_link->link.chain, -1);
        held->row_link[at] = clock_at;
    }
    platform_text_ended(&ones_written);
}

// Each form of the machine file written at the forms' link onto the plan, its clock read over it written COST_REPEATS
// times. Each time reads the last time's result as its first operand
static void forms_planned(const Ruleset *sass, const char *machine_path, const Carriers *carriers,
                          const OperatorLink *forms_link, int forms_linked, CostHeld *held)
{
    for (unsigned long long at = 0ULL; at < held->form_count; at += 1ULL)
    {
        CostForm *const form = &held->form[at];
        if (form->verdict.text[0] != '\0')
        {
            continue;
        }
        if (!forms_linked)
        {
            (void)IDENTITY_SET_TEXT(form->verdict, "no_link");
            continue;
        }
        const CarrierChain *const chain = carrier_chain_of(carriers, forms_link->link.chain.text);
        const IdentityName start = ruleset_register_name(sass, "file", forms_link->base + 1u);
        const IdentityName end = ruleset_register_name(sass, "file", forms_link->base + 2u);
        const IdentityName last = ruleset_register_name(sass, "file", forms_link->base + 3u);
        const unsigned int mark = forms_link->base + 4u + PRECEPT_NODES;
        const char *const clock_operands[2] = {end.text, start.text};
        PlatformText block;
        identity_text_begun(&block);
        mmgr_line_put(&block.line, "\tCS2R.32 \t");
        mmgr_line_put(&block.line, start.text);
        mmgr_line_put(&block.line, ", SR_CLOCKLO;\n");
        for (unsigned int again = 0u; again < COST_REPEATS; again += 1u)
        {
            mmgr_line_ch(&block.line, '\t');
            placed_filled(form->row.text, last.text, (again == 0u) ? forms_link->operands[0].text : last.text,
                          forms_link->operands[1].text, &block.line);
            mmgr_line_put(&block.line, ";\n");
        }
        mmgr_line_put(&block.line, "\tCS2R.32 \t");
        mmgr_line_put(&block.line, end.text);
        mmgr_line_put(&block.line, ", SR_CLOCKLO;\n");
        (void)node_written(sass, PRECEPT_SUB, PRECEPT_LEFT, PRECEPT_RIGHT, clock_operands, 2u, "", end.text,
                           forms_link->base + 4u, &block);
        PlatformText cut;
        identity_text_begun(&cut);
        const char *const block_bytes = platform_text_bytes(&block);
        if (chain != NULL)
        {
            (void)carrier_cut_at_read(&chain->links, chain->cases_defined, chain->text, forms_link->link_at,
                                      forms_link->form_lines, end.text, (block_bytes != NULL) ? block_bytes : "", 0,
                                      &cut);
        }
        platform_text_ended(&block);
        IdentityCode clocked;
        identity_code_begun(&clocked, PLATFORM_TIER_TEMPORARY);
        assembled(machine_path, platform_text_bytes(&cut), &clocked);
        platform_text_ended(&cut);
        if (clocked.count == 0ULL)
        {
            platform_array_ended(&clocked);
            (void)IDENTITY_SET_TEXT(form->verdict, "unassembled");
            continue;
        }
        // a form's place among the forms is far below 2^63
        planned_added(held, &clocked, mark, 0ULL, 0, &forms_link->link.chain, (long)at);
    }
}

// What came back of the round, each question at its place in the plan. A row alike where the part answers it as the
// host does on every case, and the least each clock read
static void round_read(const CostHeld *held, const PlatformArray *questions, const RunQuestion *question,
                       const HostAnswers *host, CostRows *rows, CostForm *forms, unsigned long long *clock_least)
{
    for (unsigned long long at = 0ULL; at < held->planned_count; at += 1ULL)
    {
        const Planned *const each = &held->planned[at];
        const RunQuestion *const answer = (const RunQuestion *)platform_array_at(questions, at);
        if (answer == NULL)
        {
            continue;
        }
        if (each->form >= 0)
        {
            CostForm *const form = &forms[each->form];
            if (answer->outcome != RUN_ANSWERED)
            {
                VerbaLinea verdict_line = mmgr_line_over(form->verdict.text, sizeof(form->verdict.text));
                mmgr_line_put(&verdict_line, "refused ");
                mmgr_line_put(&verdict_line, answer->refused);
                (void)mmgr_line_finish(&verdict_line);
                continue;
            }
            for (unsigned int place = 0u; place < question->cases; place += 1u)
            {
                const unsigned long long read = answer->answered[place] & 0xffffffffULL;
                form->least = (read < form->least) ? read : form->least;
                form->max = (read > form->max) ? read : form->max;
            }
            continue;
        }
        if (each->host_checked)
        {
            CostRow *const row = &rows->row[each->row];
            if (answer->outcome != RUN_ANSWERED)
            {
                VerbaLinea verdict_line = mmgr_line_over(row->verdict.text, sizeof(row->verdict.text));
                mmgr_line_put(&verdict_line, "refused ");
                mmgr_line_put(&verdict_line, answer->refused);
                (void)mmgr_line_finish(&verdict_line);
                continue;
            }
            const HostCases *const cases = host_cases_of(host, each->chain.text);
            for (unsigned int place = 0u;
                 (row->verdict.text[0] == '\0') && (cases != NULL) && (place < question->cases); place += 1u)
            {
                const unsigned int case_at = s_cost_places[place];
                if (bracket_contains(host_case(cases, case_at), answer->answered[place]) == 0)
                {
                    VerbaLinea verdict_line = mmgr_line_over(row->verdict.text, sizeof(row->verdict.text));
                    mmgr_line_put(&verdict_line, "apart at case ");
                    mmgr_line_decimal(&verdict_line, case_at, 1u);
                    (void)mmgr_line_finish(&verdict_line);
                }
            }
            row_left(row, "alike");
            continue;
        }
        if (answer->outcome != RUN_ANSWERED)
        {
            continue;
        }
        unsigned long long least = ULLONG_MAX;
        for (unsigned int place = 0u; place < question->cases; place += 1u)
        {
            const unsigned long long read = answer->answered[place] & 0xffffffffULL;
            least = (read < least) ? read : least;
        }
        unsigned long long *const smallest =
            (each->row < rows->row_count) ? &rows->row[each->row].least : &clock_least[each->row - rows->row_count];
        *smallest = (least < *smallest) ? least : *smallest;
    }
}

// The reasons of the rows left, each its count, written after the summary in the order of their names
static void reasons_said(const IdentityName *reasons, unsigned long long count)
{
    for (unsigned long long at = 0ULL; at < count;)
    {
        unsigned long long end = at;
        while ((end < count) && identity_same(reasons[end].text, reasons[at].text))
        {
            end += 1ULL;
        }
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, end - at, 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, reasons[at].text);
        platform_line_out_ended();
        at = end;
    }
}

// Each cost an exact rational in the part's clock count, written into its row's line, and each row's outcome onto
// `table`. The reason of each row left goes into `reasons`, which holds the rows' count, and its count into `left`. The
// answer is the count of rows costed
static unsigned int rows_costed(CostRows *rows, const CostHeld *held, RunWritten *table, IdentityName *reasons,
                                unsigned long long *left)
{
    unsigned int costed = 0u;
    *left = 0ULL;
    for (unsigned long long at = 0ULL; at < rows->row_count; at += 1ULL)
    {
        CostRow *const row = &rows->row[at];
        const unsigned long long linked = held->row_link[at];
        const unsigned long long clock = (linked == COST_NONE) ? ULLONG_MAX : held->clock_least[linked];
        const char *const verdict = row->verdict.text;
        const int alike = EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = verdict, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                     .other = "alike", .ci = EMBED_FALSE);
        if (!alike || (row->least == ULLONG_MAX) || (clock == ULLONG_MAX))
        {
            const size_t reason_length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = verdict,
                                                    .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " ,");
            (void)IDENTITY_SET(reasons[*left], alike ? "untimed" : verdict, alike ? 7u : reason_length);
            *left += 1ULL;
            mmgr_line_put(&table->text.line, "  ");
            mmgr_line_put(&table->text.line, row->field[2].text);
            mmgr_line_put(&table->text.line, ": ");
            mmgr_line_put(&table->text.line, (verdict[0] == '\0') ? "unwritable" : verdict);
            mmgr_line_ch(&table->text.line, '\n');
            continue;
        }
        long long numerator = (long long)row->least - (long long)clock;
        long long denominator = COST_REPEATS;
        const long long divisor =
            (long long)cost_divisor((unsigned long long)((numerator < 0) ? -numerator : numerator), COST_REPEATS);
        numerator /= (divisor == 0) ? 1 : divisor;
        denominator /= (divisor == 0) ? 1 : divisor;
        VerbaLinea cost_line = mmgr_line_over(row->field[3].text, sizeof(row->field[3].text));
        mmgr_line_decimal_signed(&cost_line, numerator);
        if (denominator != 1)
        {
            mmgr_line_ch(&cost_line, '/');
            mmgr_line_decimal_signed(&cost_line, denominator);
        }
        (void)mmgr_line_finish(&cost_line);
        (void)IDENTITY_SET_TEXT(row->field[4], "1");
        VerbaLinea joined = mmgr_line_over(rows->line[row->line].text, sizeof(rows->line[row->line].text));
        for (unsigned int field = 0u; field < COST_FIELDS; field += 1u)
        {
            mmgr_line_put(&joined, (field == 0u) ? "" : "\t");
            mmgr_line_put(&joined, row->field[field].text);
        }
        (void)mmgr_line_finish(&joined);
        mmgr_line_put(&table->text.line, "  ");
        mmgr_line_put(&table->text.line, row->field[2].text);
        mmgr_line_put(&table->text.line, ": alike, ");
        mmgr_line_decimal(&table->text.line, row->least, 1u);
        mmgr_line_put(&table->text.line, " clocks over ");
        mmgr_line_decimal(&table->text.line, COST_REPEATS, 1u);
        mmgr_line_put(&table->text.line, ", the clock alone ");
        mmgr_line_decimal(&table->text.line, clock, 1u);
        mmgr_line_put(&table->text.line, ": ");
        mmgr_line_put(&table->text.line, row->field[3].text);
        mmgr_line_ch(&table->text.line, '\n');
        costed += 1u;
    }
    identity_sorted_in_place(reasons, *left, [](const IdentityName &one, const IdentityName &other) {
        return identity_order_text(one.text, other.text) < 0;
    });
    return costed;
}

// the bytes of one cost row of the .ksc, its terminator among them
#define COST_ROW_BYTES 192u

struct CostKscRow
{
    char text[COST_ROW_BYTES];
};

// Each form's cost and spread in the part's clock count onto `table` and as a row of the .ksc. Every cost row the .ksc
// contained let go for them. The answer is the count of forms costed
static unsigned int forms_costed_written(const CostHeld *held, unsigned long long clock_alone, const char *ksc,
                                         RunWritten *table)
{
    unsigned int forms_costed = 0u;
    CostKscRow *const rows =
        (CostKscRow *)platform_cell_taken(((held->form_count != 0ULL) ? held->form_count : 1ULL) * sizeof(CostKscRow),
                                          platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    const char **const texts = (const char **)platform_cell_taken(
        ((held->form_count != 0ULL) ? held->form_count : 1ULL) * sizeof(const char *), platform_place_g_program_owner,
        PLATFORM_TIER_TEMPORARY);
    unsigned int row_count = 0u;
    for (unsigned long long at = 0ULL; (rows != NULL) && (texts != NULL) && (at < held->form_count); at += 1ULL)
    {
        const CostForm *const form = &held->form[at];
        const char *const verdict = form->verdict.text;
        if ((verdict[0] != '\0') || (form->least == ULLONG_MAX) || (clock_alone == ULLONG_MAX))
        {
            mmgr_line_put(&table->text.line, "  ");
            mmgr_line_put(&table->text.line, form->row.text);
            mmgr_line_put(&table->text.line, ": ");
            mmgr_line_put(&table->text.line, (verdict[0] == '\0') ? "untimed" : verdict);
            mmgr_line_ch(&table->text.line, '\n');
            continue;
        }
        const unsigned long long least = (form->least > clock_alone) ? (form->least - clock_alone) : 0ULL;
        const unsigned long long spread = form->max - form->least;
        VerbaLinea row_line = mmgr_line_over(rows[row_count].text, sizeof(rows[row_count].text));
        mmgr_line_put(&row_line, "run answers ");
        mmgr_line_hex(&row_line, least, 16u);
        mmgr_line_put(&row_line, " cost ");
        mmgr_line_hex(&row_line, form->row.low, 16u);
        mmgr_line_put(&row_line, " ");
        mmgr_line_hex(&row_line, form->row.high, 16u);
        mmgr_line_put(&row_line, " ");
        mmgr_line_hex(&row_line, spread, 16u);
        (void)mmgr_line_finish(&row_line);
        texts[row_count] = rows[row_count].text;
        row_count += 1u;
        mmgr_line_put(&table->text.line, "  ");
        mmgr_line_put(&table->text.line, form->row.text);
        mmgr_line_put(&table->text.line, ": ");
        mmgr_line_decimal(&table->text.line, form->least, 1u);
        mmgr_line_put(&table->text.line, " clocks over ");
        mmgr_line_decimal(&table->text.line, COST_REPEATS, 1u);
        mmgr_line_put(&table->text.line, ", the clock alone ");
        mmgr_line_decimal(&table->text.line, clock_alone, 1u);
        mmgr_line_put(&table->text.line, ", the spread ");
        mmgr_line_decimal(&table->text.line, spread, 1u);
        mmgr_line_ch(&table->text.line, '\n');
        forms_costed += 1u;
    }
    if ((row_count != 0u) && !ksc_answers_write(ksc, "cost", texts, row_count))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity cost: the forms' costs could not be written to ");
        mmgr_line_put(output_line, ksc);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(texts, platform_place_g_program_owner);
    platform_cell_released(rows, platform_place_g_program_owner);
    return forms_costed;
}

// The cost pass writes the .kdm's section, every line after its first to its tail with each row costed in place of
// the one before. Its tail stays as it was (kdm.h). 1, or 0 where the write failed
static int kdm_costs_written(const CostRows *rows, const char *kdm)
{
    KdmFile written = {};
    int written_ok = 1;
    for (unsigned long long at = 0ULL; at < rows->line_count; at += 1ULL)
    {
        written_ok = written_ok && kfile_lines_line_add(&written, rows->line[at].text);
    }
    const unsigned int tail = kdm_tail_at(&written);
    const char **const section =
        (const char **)platform_cell_taken(((written.lines != 0u) ? written.lines : 1u) * sizeof(const char *),
                                           platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int section_count = 0u;
    Who who;
    int who_present = 0;
    for (unsigned int at = 1u; (section != NULL) && (at < tail); at += 1u)
    {
        if (!kfile_lines_who_line(written.line[at], &who, &who_present))
        {
            section[section_count] = written.line[at];
            section_count += 1u;
        }
    }
    char part[CARRIER_TEXT_MAX];
    VerbaLinea part_line = mmgr_line_over(part, sizeof(part));
    const char *const first_line = (rows->line_count != 0ULL) ? rows->line[0].text : "";
    mmgr_line_put(&part_line, EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = first_line,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "kdm ", .ci = EMBED_FALSE)
                                  ? (first_line + 4)
                                  : "-");
    (void)mmgr_line_finish(&part_line);
    written_ok = written_ok && (section != NULL) && kdm_section_written(&written, part, section, section_count) &&
                 kdm_write(&written, kdm);
    platform_cell_released(section, platform_place_g_program_owner);
    kdm_free(&written);
    return written_ok;
}

// a line on the output stream: the mode's name, then `first`, then `second`
static void cost_said(const char *first, const char *second)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity cost: ");
    mmgr_line_put(output_line, first);
    mmgr_line_put(output_line, second);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// The plan of every operator's arrangements, the link's clock alone with each, onto `held`. The link the machine
// file's forms go at, the first any operator's arrangements go at, into `forms_link`. The answer is 1 where one is
static int operators_planned(const Ruleset *sass, const char *machine_path, const Carriers *carriers,
                             unsigned long seed, CostRows *rows, RunWritten *table, CostHeld *held,
                             OperatorLink *forms_link, unsigned long long *forms_clock_at)
{
    int forms_linked = 0;
    for (unsigned long long at = 0ULL; at < rows->row_count;)
    {
        const unsigned long long end = operator_end(rows, at);
        const unsigned long long *const own = &rows->by_operator[at];
        const unsigned long long own_count = end - at;
        const char *const name = rows->row[own[0]].field[0].text;
        at = end;
        OperatorLink operator_at_link;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &operator_at_link, .bytes = sizeof(operator_at_link), .val = 0);
        if (!operator_link(sass, carriers, seed, name, rows, own, own_count, &operator_at_link))
        {
            platform_text_ended(&operator_at_link.clock_cut);
            if (operator_at_link.word == NULL)
            {
                table_said(table, name, ": its arrangements produce no word of two reads the ruleset writes", "");
                for (unsigned long long each = 0ULL; each < own_count; each += 1ULL)
                {
                    row_left(&rows->row[own[each]], "unwritable");
                }
                continue;
            }
            mmgr_line_put(&table->text.line, name);
            mmgr_line_put(&table->text.line, ": no link of ");
            mmgr_line_put(&table->text.line, operator_at_link.word->name);
            mmgr_line_put(&table->text.line, " carries the cases with registers alone and can be cut");
            mmgr_line_ch(&table->text.line, '\n');
            for (unsigned long long each = 0ULL; each < own_count; each += 1ULL)
            {
                row_left(&rows->row[own[each]], "no_link");
            }
            continue;
        }
        const CarrierChain *const chain = carrier_chain_of(carriers, operator_at_link.link.chain.text);
        char address[64];
        VerbaLinea address_line = mmgr_line_over(address, sizeof(address));
        mmgr_line_put(&address_line, operator_at_link.link.chain.text);
        mmgr_line_put(&address_line, ":");
        // a carrier's lines are far below 2^28
        mmgr_line_hex(&address_line, 16u * (unsigned int)operator_at_link.link_at, 4u);
        (void)mmgr_line_finish(&address_line);
        const CarrierPiece found_text = found_group(&operator_at_link.link, 0u, chain->text);
        mmgr_line_put(&table->text.line, name);
        mmgr_line_put(&table->text.line, " at ");
        mmgr_line_put(&table->text.line, address);
        mmgr_line_put(&table->text.line, ", ");
        mmgr_line_put(&table->text.line, operator_at_link.word->name);
        mmgr_line_put(&table->text.line, " in place of ");
        (void)mmgr_line_put_n(&table->text.line, found_text.text, found_text.length);
        const int ends = (found_text.length != 0u) && (found_text.text[found_text.length - 1u] == '\n');
        mmgr_line_put(&table->text.line, ends ? "" : "\n");
        if (!forms_linked)
        {
            // the forms' link keeps its own copy of the clock's cut, and the plan reads this one
            *forms_link = operator_at_link;
            identity_text_begun(&forms_link->clock_cut);
            *forms_clock_at = held->clock_count;
            forms_linked = 1;
        }
        operator_planned(sass, machine_path, &operator_at_link, chain, rows, own, own_count, held);
        platform_text_ended(&operator_at_link.clock_cut);
    }
    return forms_linked;
}
// The arrangements of a part's .kdm costed on the part (P6, the rank). An operator's arrangements go at one link of a
// carrier of ours that stores the word they produce (word_web.h). The first link in the pair's order whose result a
// later link reads, that carries the cases, and whose operands and result are registers. The link's
// operands are the arrangement's and its result the arrangement's root's. The ruleset writes an arrangement node by
// node where each of its nodes is the precept of a word it writes. The cost leaves one with a node no such word
// writes.
//
// Each queries two questions over every case the host computes the carrier on. The first is the carrier with the
// arrangement in the link's place, checked against the host's answers. The second is the part's clock read before and
// after the arrangement written COST_REPEATS times, each time over the last time's result. The difference goes in
// place of the carrier's answer.
//
// Each link queries its clock read twice with nothing between, the clock's own. No question waits on another's
// answer, and the cost writes every one before it puts any. All go in one round, in an order drawn from the run's
// IDENTITY_SEED. The round after puts again only those the part refused, and declares every register its file uses
// (cuts_queried). The clock's reading is the words the part stores and no launch's time. Each is a query the query
// record answers once queried, and the cost never puts again a question the record contains.
//
// An arrangement the part answers apart or refuses records no cost. Its cost is the least difference any case read,
// less the least its link's clock read alone, over COST_REPEATS, in the part's clock count. It goes in its row of the
// .kdm with 1 run, the one reading. The run's blob cost.txt contains every arrangement with its link and what came of
// it.
//
// The pass costs the machine file's forms the same way where `forms` names the blob the vendor's writer placed them
// in (orior_khw_machine_write, krs). Each is a line `<relation> <signed> <low> <high> <text>`, the text with each
// operand of the relation's tuple as its place. A form with the three places of a two-word relation goes at the first
// link any operator's arrangements go at. It goes COST_REPEATS times over its own result between the clock reads, and
// queries its clock alone. No host computes it, since the protocol classified it.
//
// Its cost is the least difference any case read less the link's clock read alone. Its spread is the max difference
// less the least, both in the part's clock count over COST_REPEATS. Each goes as the part's .ksc stores a cost, `run
// answers <least> cost <low> <high> <spread>`. The pass lets go every cost row the file contained before, since one
// reading is for every form
int identity_cost(const char *engine, const char *answers, const char *ksc, const char *folder, const char *kdm,
                  const char *machine_path, const char *forms, const char *const *carrier)
{
    const Ruleset *const sass = identity_ruleset();
    if (sass == NULL)
    {
        cost_said("the ruleset ", IDENTITY_RULESET " names is not read");
        return 1;
    }
    HostAnswers host;
    Carriers carriers;
    CostRows rows;
    CostHeld held;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &carriers, .bytes = sizeof(carriers), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &rows, .bytes = sizeof(rows), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &held, .bytes = sizeof(held), .val = 0);
    if (!host_answers_read(answers, 1, &host) || !carriers_read(engine, &host, &carriers) ||
        !kdm_rows_read(kdm, &rows) || !cost_held_taken(forms, &rows, &held))
    {
        cost_said(kdm, ": the pool cannot meet the cost's cells");
        cost_held_ended(&held);
        cost_rows_ended(&rows);
        carriers_ended(&carriers);
        host_answers_ended(&host);
        return 1;
    }
    const unsigned long seed = identity_seed();
    if (!run_channel_open(carrier, 60000000ULL))
    {
        cost_held_ended(&held);
        cost_rows_ended(&rows);
        carriers_ended(&carriers);
        host_answers_ended(&host);
        return 1;
    }
    record_open(ksc, "cost");
    {
        char seed_text[24];
        VerbaLinea seed_line = mmgr_line_over(seed_text, sizeof(seed_text));
        mmgr_line_decimal(&seed_line, seed, 1u);
        (void)mmgr_line_finish(&seed_line);
        kqr_mark(&s_query_record, KQR_SEED_RECORD, seed_text);
    }
    RunWritten table;
    run_written_begun(&table, identity_path_joined(folder, "/cost.txt").text, 0);
    // an arrangement's chain contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    RunQuestion *const question = &s_question;
    question->place = RUN_PLACE_NONE;
    stall_cases(question, s_cost_places);
    unsigned long long queries = 0ULL;
    // the link the machine file's forms go at: the first any operator's arrangements go at, and its clock alone
    OperatorLink forms_link;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &forms_link, .bytes = sizeof(forms_link), .val = 0);
    unsigned long long forms_clock_at = 0ULL;
    const int forms_linked =
        operators_planned(sass, machine_path, &carriers, seed, &rows, &table, &held, &forms_link, &forms_clock_at);
    forms_planned(sass, machine_path, &carriers, &forms_link, forms_linked, &held);
    if (forms_linked)
    {
        platform_text_ended(&forms_link.clock_cut);
    }
    // every question in one round, in an order drawn from the seed
    {
        // the seed cut to the word the sequence takes
        const unsigned int words[1] = {(unsigned int)seed};
        TableDraw drawn;
        table_draw_sequenced(&drawn, words, 1u);
        table_shuffled(held.planned, held.planned_count, &drawn);
    }
    IdentityCode *const round = (IdentityCode *)platform_cell_taken(
        ((held.planned_count != 0ULL) ? held.planned_count : 1ULL) * sizeof(IdentityCode),
        platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int mark = 0u;
    for (unsigned long long at = 0ULL; (round != NULL) && (at < held.planned_count); at += 1ULL)
    {
        round[at] = held.planned[at].code;
        mark = (held.planned[at].mark > mark) ? held.planned[at].mark : mark;
    }
    // the plan passed as the teacher passes one, for the panel: the questions of the one round
    char plan[64];
    VerbaLinea plan_line = mmgr_line_over(plan, sizeof(plan));
    mmgr_line_put(&plan_line, "cost ");
    mmgr_line_decimal(&plan_line, held.planned_count, 1u);
    mmgr_line_put(&plan_line, "\n");
    const size_t plan_length = mmgr_line_finish(&plan_line);
    if ((plan_length > 0u) && (qry_member_run() != NULL))
    {
        qry_member_append("plan.txt", plan, (unsigned long long)plan_length, QRY_ORDINARY);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  plan: cost ");
        mmgr_line_decimal(output_line, held.planned_count, 1u);
        mmgr_line_ch(output_line, '\n');
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, held.planned_count, 1u);
        mmgr_line_put(output_line, " questions written, put in one round");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    PlatformArray questions;
    platform_array_begun(&questions, sizeof(RunQuestion), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (round != NULL)
    {
        cuts_queried(round, held.planned_count, NULL, NULL, question, &queries, mark, &questions);
    }
    platform_cell_released(round, platform_place_g_program_owner);
    round_read(&held, &questions, question, &host, &rows, held.form, held.clock_least);
    platform_array_ended(&questions);
    run_channel_close();
    IdentityName *const reasons = (IdentityName *)platform_cell_taken(
        (rows.row_count + 1ULL) * sizeof(IdentityName), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned long long left = 0ULL;
    const unsigned int costed = (reasons != NULL) ? rows_costed(&rows, &held, &table, reasons, &left) : 0u;
    const unsigned long long clock_alone = forms_linked ? held.clock_least[forms_clock_at] : ULLONG_MAX;
    const unsigned int forms_costed = forms_costed_written(&held, clock_alone, ksc, &table);
    const unsigned long long form_count = held.form_count;
    cost_held_ended(&held);
    carriers_ended(&carriers);
    host_answers_ended(&host);
    const int closed = run_closed(&table);
    run_written_ended(&table);
    if (!closed)
    {
        cost_said(folder, "/cost.txt could not be written");
        platform_cell_released(reasons, platform_place_g_program_owner);
        cost_rows_ended(&rows);
        return 1;
    }
    const int kdm_ok = kdm_costs_written(&rows, kdm);
    const unsigned long long row_count = rows.row_count;
    cost_rows_ended(&rows);
    if (!kdm_ok)
    {
        cost_said(kdm, " could not be written");
        platform_cell_released(reasons, platform_place_g_program_owner);
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity cost: ");
        mmgr_line_decimal(output_line, row_count, 1u);
        mmgr_line_put(output_line, " arrangements, ");
        mmgr_line_decimal(output_line, costed, 1u);
        mmgr_line_put(output_line, " costed over ");
        mmgr_line_decimal(output_line, queries, 1u);
        mmgr_line_put(output_line, " queries and written to ");
        mmgr_line_put(output_line, kdm);
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal(output_line, form_count, 1u);
        mmgr_line_put(output_line, " forms, ");
        mmgr_line_decimal(output_line, forms_costed, 1u);
        mmgr_line_put(output_line, " costed in the part's clock and written to ");
        mmgr_line_put(output_line, ksc);
        platform_line_out_ended();
    }
    if (reasons != NULL)
    {
        reasons_said(reasons, left);
    }
    platform_cell_released(reasons, platform_place_g_program_owner);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}

// the first word of the line `line`, into `word`: 1, or 0 where it holds none
static int line_first_word(CarrierPiece line, char *word, size_t capacity)
{
    static TablePatternOnce s_first_word;
    TableMatch found;
    if (!table_search(table_once(&s_first_word, "^\\s*(\\S+)", 0), line.text, line.length, &found))
    {
        return 0;
    }
    size_t length = 0u;
    const char *const bytes = table_group(&found, 1u, line.text, &length);
    VerbaLinea word_line = mmgr_line_over(word, capacity);
    (void)mmgr_line_put_n(&word_line, bytes, length);
    (void)mmgr_line_finish(&word_line);
    return 1;
}

// one pair the arrangements probe and how many probes it has
struct PairProbed
{
    StallPair pair;
    unsigned long long probes;
};

// the count of probes `probed`, a PlatformArray of PairProbed, holds of `pair`, and its record, NULL where it holds
// none
static PairProbed *pair_probed(PlatformArray *probed, const StallPair *pair)
{
    for (unsigned long long at = 0ULL; at < probed->count; at += 1ULL)
    {
        PairProbed *const each = (PairProbed *)platform_array_at(probed, at);
        if (identity_same(each->pair.text, pair->text))
        {
            return each;
        }
    }
    return NULL;
}

// 1 where the `count` keys at `excluded`, in the order of their bytes, hold `pair`
static int pair_excluded(const StallPair *excluded, unsigned long long count, const StallPair *pair)
{
    const unsigned long long at = identity_lower(
        excluded, count, [pair](const StallPair &one) { return identity_order_text(one.text, pair->text) < 0; });
    return (at < count) && identity_same(excluded[at].text, pair->text);
}

// Each pair of one arrangement's nodes, the writer the node before the reader, probed at every stall of the writer.
// Onto `queries` and `codes`, each count of `probed` at max `max`
static void arrangement_pairs_probed(const char *machine_path, const OperatorLink *at_link, const CostRow *row,
                                     const char *before, const char *ones_text, CarrierPiece located, const char *past,
                                     unsigned int ahead, StallField stall, unsigned long long longest,
                                     const StallPair *excluded, unsigned long long excluded_count,
                                     unsigned long long max, PlatformArray *probed, PlatformArray *queries,
                                     PlatformArray *codes)
{
    // the arrangement's lines, one a node, each a piece of its text
    PlatformArray node_lines;
    identity_pieces_begun(&node_lines);
    size_t cut = 0u;
    CarrierPiece piece;
    while (carrier_piece_next(located, '\n', &cut, &piece))
    {
        if (trimmed(piece).length != 0u)
        {
            (void)platform_array_appended(&node_lines, &piece);
        }
    }
    if ((ahead == 0u) || (ones_text[0] == '\0') || (node_lines.count != row->tree.count))
    {
        platform_array_ended(&node_lines);
        return;
    }
    for (unsigned int node = 1u; node < row->tree.count; node += 1u)
    {
        // a node's place is below PRECEPT_NODES, which an unsigned char holds
        const unsigned char writer = (unsigned char)(node - 1u);
        const PreceptNode reading = row->tree.node[node];
        char writer_word[CARRIER_TEXT_MAX];
        char reader_word[CARRIER_TEXT_MAX];
        if (((reading.left != writer) && (reading.right != writer)) ||
            !line_first_word(identity_piece_at(&node_lines, node - 1u), writer_word, sizeof(writer_word)) ||
            !line_first_word(identity_piece_at(&node_lines, node), reader_word, sizeof(reader_word)))
        {
            continue;
        }
        const StallPair pair = stall_pair_key(writer_word, reader_word);
        PairProbed *const counted = pair_probed(probed, &pair);
        if (pair_excluded(excluded, excluded_count, &pair) || ((counted != NULL) && (counted->probes >= max)))
        {
            continue;
        }
        PlatformText whole_text;
        identity_text_begun(&whole_text);
        mmgr_line_put(&whole_text.line, before);
        mmgr_line_put(&whole_text.line, ones_text);
        (void)mmgr_line_put_n(&whole_text.line, located.text, located.length);
        mmgr_line_put(&whole_text.line, past);
        IdentityCode written;
        identity_code_begun(&written, PLATFORM_TIER_TEMPORARY);
        vendor_assembled(machine_path, platform_text_bytes(&whole_text), &written);
        platform_text_ended(&whole_text);
        const unsigned long long count = written.count / 16ULL;
        if (count == 0ULL)
        {
            platform_array_ended(&written);
            continue;
        }
        for (unsigned long long index = 0ULL; index < count; index += 1ULL)
        {
            stall_bits_write(&written, index, stall, longest);
        }
        for (unsigned long long step = 0ULL; step <= longest; step += 1ULL)
        {
            IdentityCode turned;
            identity_code_begun(&turned, codes->ephemeral);
            (void)identity_code_copied(&turned, &written);
            stall_bits_write(&turned, ahead + node - 1u, stall, step);
            (void)platform_array_appended(codes, &turned);
            StallQuery query;
            query.pair = pair;
            query.number = at_link->link.chain;
            query.step = step;
            (void)platform_array_appended(queries, &query);
        }
        platform_array_ended(&written);
        if (counted != NULL)
        {
            counted->probes += 1ULL;
        }
        else
        {
            const PairProbed added = {pair, 1ULL};
            (void)platform_array_appended(probed, &added);
        }
    }
    platform_array_ended(&node_lines);
}

void arrangement_probes(const char *engine, const HostAnswers *host, const char *kdm, const Ruleset *sass,
                        const char *machine_path, StallField stall, unsigned long long longest,
                        const StallPair *excluded, unsigned long long excluded_count, unsigned long long max,
                        PlatformArray *queries, PlatformArray *codes)
{
    Carriers carriers;
    CostRows rows;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &carriers, .bytes = sizeof(carriers), .val = 0);
    if (!carriers_read(engine, host, &carriers) || !kdm_rows_read(kdm, &rows))
    {
        carriers_ended(&carriers);
        return;
    }
    const unsigned long seed = identity_seed();
    PlatformArray probed;
    platform_array_begun(&probed, sizeof(PairProbed), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long at = 0ULL; at < rows.row_count;)
    {
        const unsigned long long end = operator_end(&rows, at);
        const unsigned long long *const own = &rows.by_operator[at];
        const unsigned long long own_count = end - at;
        at = end;
        OperatorLink at_link;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &at_link, .bytes = sizeof(at_link), .val = 0);
        const int linked =
            operator_link(sass, &carriers, seed, rows.row[own[0]].field[0].text, &rows, own, own_count, &at_link);
        platform_text_ended(&at_link.clock_cut);
        if (!linked)
        {
            continue;
        }
        const CarrierChain *const chain = carrier_chain_of(&carriers, at_link.link.chain.text);
        const RegexpSpan whole = at_link.link.group[0];
        PlatformText before;
        identity_text_begun(&before);
        (void)mmgr_line_put_n(&before.line, chain->text, (size_t)whole.from);
        const char *const past = chain->text + whole.to;
        const IdentityName ones = ruleset_register_name(sass, "file", at_link.base);
        PlatformText ones_written;
        identity_text_begun(&ones_written);
        (void)node_written(sass, PRECEPT_NOT, PRECEPT_ZERO, PRECEPT_NONE, NULL, 0u, "", ones.text, at_link.base + 4u,
                           &ones_written);
        const char *const ones_bytes = platform_text_bytes(&ones_written);
        const char *const ones_text = (ones_bytes != NULL) ? ones_bytes : "";
        const char *const before_bytes = platform_text_bytes(&before);
        const char *const before_text = (before_bytes != NULL) ? before_bytes : "";
        // the instructions before the arrangement's first node
        PlatformText ahead_text;
        identity_text_begun(&ahead_text);
        mmgr_line_put(&ahead_text.line, before_text);
        mmgr_line_put(&ahead_text.line, ones_text);
        IdentityCode ahead_code;
        identity_code_begun(&ahead_code, PLATFORM_TIER_TEMPORARY);
        vendor_assembled(machine_path, platform_text_bytes(&ahead_text), &ahead_code);
        platform_text_ended(&ahead_text);
        // a code's instructions are far below 2^32
        const unsigned int ahead = (unsigned int)(ahead_code.count / 16ULL);
        platform_array_ended(&ahead_code);
        const char *const operands[2] = {at_link.operands[0].text, at_link.operands[1].text};
        for (unsigned long long own_at = 0ULL; own_at < own_count; own_at += 1ULL)
        {
            const CostRow *const row = &rows.row[own[own_at]];
            PlatformText located;
            identity_text_begun(&located);
            if (row->verdict.text[0] == '\0')
            {
                (void)arrangement_written(sass, &row->tree, operands, 2u, ones.text, at_link.to.text, at_link.base + 4u,
                                          &located);
            }
            const unsigned long long located_size = platform_text_size(&located);
            const char *const located_bytes = platform_text_bytes(&located);
            const CarrierPiece located_piece = {(located_bytes != NULL) ? located_bytes : "", (size_t)located_size};
            arrangement_pairs_probed(machine_path, &at_link, row, before_text, ones_text, located_piece, past, ahead,
                                     stall, longest, excluded, excluded_count, max, &probed, queries, codes);
            platform_text_ended(&located);
        }
        platform_text_ended(&ones_written);
        platform_text_ended(&before);
    }
    platform_array_ended(&probed);
    cost_rows_ended(&rows);
    carriers_ended(&carriers);
}
