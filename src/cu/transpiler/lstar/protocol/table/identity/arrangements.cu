// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// arrangements.cu: the arrangements of primitives that produce each operator, read from the .kdm and written as a chain
// of ours
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "identity_internal.h"
#include "precept_value.h"

#define COST_PRECEPT_TEXT(name_, text_, arity_) text_,
#define COST_PRECEPT_ARITY(name_, text_, arity_) arity_,
static const char *const s_precept_text[] = {PRECEPTS(COST_PRECEPT_TEXT)};
static const unsigned int s_precept_arity[] = {PRECEPTS(COST_PRECEPT_ARITY)};
#undef COST_PRECEPT_TEXT
#undef COST_PRECEPT_ARITY

// The tree the arrangement `text` writes from `at`, as chain_build.c writes one: a precept applied to its children, or
// a leaf, an operand `w<place>`, `zero` or `ones`. Its nodes go into `tree` children first, and the node or the leaf
// it is into `child`. 1, or 0 where the text is no tree over the precepts
static int arrangement_read(CarrierPiece text, size_t *at, PreceptTree *tree, unsigned char *child)
{
    while ((*at < text.length) && (text.text[*at] == ' '))
    {
        *at += 1u;
    }
    size_t end = *at;
    while ((end < text.length) &&
           (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (unsigned char)text.text[end]) != 0))
    {
        end += 1u;
    }
    const CarrierPiece name = {&text.text[*at], end - *at};
    *at = end;
    if (identity_piece_is(name, "zero") || identity_piece_is(name, "ones"))
    {
        *child = identity_piece_is(name, "zero") ? PRECEPT_ZERO : PRECEPT_ONES;
        return 1;
    }
    if ((name.length == 2u) && (name.text[0] == 'w') &&
        (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (unsigned char)name.text[1]) != 0))
    {
        *child = PRECEPT_ARG_AT((unsigned int)(name.text[1] - '0'));
        return 1;
    }
    unsigned int precept = PRECEPT_COUNT;
    for (unsigned int each = 0u; each < PRECEPT_COUNT; each += 1u)
    {
        precept = identity_piece_is(name, s_precept_text[each]) ? each : precept;
    }
    if ((precept == PRECEPT_COUNT) || (s_precept_arity[precept] == 0u) || (*at >= text.length) ||
        (text.text[*at] != '('))
    {
        return 0;
    }
    *at += 1u;
    unsigned char left = PRECEPT_NONE;
    unsigned char right = PRECEPT_NONE;
    if (!arrangement_read(text, at, tree, &left))
    {
        return 0;
    }
    if (s_precept_arity[precept] == 2u)
    {
        if ((*at >= text.length) || (text.text[*at] != ','))
        {
            return 0;
        }
        *at += 1u;
        if (!arrangement_read(text, at, tree, &right))
        {
            return 0;
        }
    }
    if ((*at >= text.length) || (text.text[*at] != ')') || (tree->count >= PRECEPT_NODES))
    {
        return 0;
    }
    *at += 1u;
    // a node the tree contains already is that node, as chain_build.c counts it, and the tree writes it once
    for (unsigned int node_at = 0u; node_at < tree->count; node_at += 1u)
    {
        const PreceptNode *const node = &tree->node[node_at];
        if ((node->precept == precept) && (node->left == left) && (node->right == right))
        {
            // a node's place is below PRECEPT_NODES, which an unsigned char holds
            *child = (unsigned char)node_at;
            return 1;
        }
    }
    // a precept's index is below PRECEPT_COUNT, which an unsigned char holds
    const PreceptNode node = {(unsigned char)precept, left, right};
    tree->node[tree->count] = node;
    *child = (unsigned char)tree->count;
    tree->count += 1u;
    return 1;
}

const Word *word_produced(const PreceptTree *tree)
{
    for (unsigned int at = 0u; at < WORD_WEB_COUNT; at += 1u)
    {
        const Word *const word = &s_word_web[at];
        int alike = (word->reads == 2u);
        for (size_t left = 0u; alike && (left < IDENTITY_VALUES); left += 1u)
        {
            for (size_t right = 0u; alike && (right < IDENTITY_VALUES); right += 1u)
            {
                PreceptCase given = {};
                given.operands = 2u;
                given.operand[0] = (unsigned int)(s_values[left] & 0xffffffffULL);
                given.operand[1] = (unsigned int)(s_values[right] & 0xffffffffULL);
                unsigned int by_word = 0u;
                unsigned int by_nodes = 0u;
                alike = precept_value(word->node, word->nodes, &given, &by_word) &&
                        precept_value(tree->node, tree->count, &given, &by_nodes) && (by_word == by_nodes);
            }
        }
        if (alike)
        {
            return word;
        }
    }
    return NULL;
}

// the scratch of a writing that takes none: it refuses every scratch register
static int arrangement_no_scratch(void *context, const char *bank, VerbaLinea *name)
{
    (void)context;
    (void)bank;
    (void)name;
    return 0;
}

IdentityName ruleset_register_name(const Ruleset *rules, const char *bank, unsigned int number)
{
    IdentityName name;
    VerbaLinea line = mmgr_line_over(name.text, sizeof(name.text));
    const int named = ruleset_register_written(rules, bank, number, &line);
    (void)mmgr_line_finish(&line);
    name.text[0] = (named != 0) ? name.text[0] : '\0';
    return name;
}

IdentityName ruleset_fixed_name(const Ruleset *rules, const char *name)
{
    IdentityName fixed;
    const char *const named = ruleset_fixed_named(rules, name);
    (void)IDENTITY_SET_TEXT(fixed, (named != NULL) ? named : "");
    return fixed;
}

int ruleset_form_put(const Ruleset *rules, const char *name, const char *const *arguments, unsigned int count,
                     const RulesetNamedScratch *scratch, PlatformText *out)
{
    PlatformText written;
    identity_text_begun(&written);
    const int written_ok = ruleset_form_written(rules, name, arguments, count, scratch, &written.line) &&
                           (mmgr_line_ok(&written.line) != 0);
    if (written_ok != 0)
    {
        identity_text_put(out, &written);
    }
    platform_text_ended(&written);
    return written_ok;
}

int arrangement_written(const Ruleset *sass, const PreceptTree *tree, const char *const *operands, unsigned int count,
                        const char *ones, const char *to, unsigned int next, PlatformText *out)
{
    const RulesetNamedScratch none = {arrangement_no_scratch, NULL};
    // the register each node goes to, and the zero, as names the arguments point into
    IdentityName lines[PRECEPT_NODES];
    const IdentityName zero = ruleset_fixed_name(sass, "zero");
    PlatformText written;
    identity_text_begun(&written);
    int whole = 1;
    for (unsigned int at = 0u; whole && (at < tree->count); at += 1u)
    {
        const PreceptNode node = tree->node[at];
        const Word *const word = word_web_word_for(node.precept);
        // the ruleset writes nothing of a form given another count of arguments
        if (word == NULL)
        {
            whole = 0;
            continue;
        }
        if ((at + 1u) == tree->count)
        {
            (void)IDENTITY_SET_TEXT(lines[at], to);
        }
        else
        {
            lines[at] = ruleset_register_name(sass, "file", next + at);
        }
        const char *arguments[3] = {lines[at].text, NULL, NULL};
        unsigned int argument_count = 1u;
        const unsigned char child[2] = {node.left, node.right};
        for (unsigned int which = 0u; whole && (which < word->reads) && (which < 2u); which += 1u)
        {
            const unsigned char leaf = child[which];
            const unsigned int place = (unsigned int)(leaf - PRECEPT_ARG);
            if (((leaf == PRECEPT_ONES) && (ones[0] == '\0')) ||
                ((leaf >= PRECEPT_ARG) && (leaf != PRECEPT_ZERO) && (leaf != PRECEPT_ONES) && (place >= count)))
            {
                whole = 0;
                continue;
            }
            arguments[argument_count] = (leaf == PRECEPT_ZERO)   ? zero.text
                                        : (leaf == PRECEPT_ONES) ? ones
                                        : (leaf >= PRECEPT_ARG)  ? operands[place]
                                                                 : lines[leaf].text;
            argument_count += 1u;
        }
        whole = whole && (ruleset_form_put(sass, word->name, arguments, argument_count, &none, &written) != 0);
    }
    if (whole)
    {
        identity_text_put(out, &written);
    }
    platform_text_ended(&written);
    return whole;
}

int kdm_rows_read(const char *kdm, CostRows *rows)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = rows, .bytes = sizeof(*rows), .val = 0);
    // the .kdm as the run stores it (kdm.h), a line each, each line and each row a cell taken once at their count
    KdmFile kdm_file = {};
    kdm_read(&kdm_file, kdm);
    const unsigned long long count = kdm_file.lines;
    rows->line = (CostLine *)platform_cell_cleared((count != 0ULL) ? count : 1ULL, sizeof(CostLine),
                                                   platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    rows->row = (CostRow *)platform_cell_cleared((count != 0ULL) ? count : 1ULL, sizeof(CostRow),
                                                 platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    rows->by_operator =
        (unsigned long long *)platform_cell_cleared((count != 0ULL) ? count : 1ULL, sizeof(unsigned long long),
                                                    platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    int whole = (rows->line != NULL) && (rows->row != NULL) && (rows->by_operator != NULL);
    for (unsigned long long at = 0ULL; whole && (at < count); at += 1ULL)
    {
        whole = IDENTITY_SET_TEXT(rows->line[at], kdm_file.line[at]);
    }
    kdm_free(&kdm_file);
    if (!whole)
    {
        cost_rows_ended(rows);
        identity_path_said(kdm, ": the pool cannot meet its lines, or a line passes COST_LINE_BYTES");
        return 0;
    }
    rows->line_count = count;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        const CarrierPiece line = carrier_piece_of(rows->line[at].text);
        CarrierPiece pieces[COST_FIELDS + 1u];
        unsigned int piece_count = 0u;
        size_t cut = 0u;
        CarrierPiece piece;
        while (carrier_piece_next(line, '\t', &cut, &piece))
        {
            if (piece_count <= COST_FIELDS)
            {
                pieces[piece_count] = piece;
            }
            piece_count += 1u;
        }
        const int counted = piece_count == COST_FIELDS;
        const CarrierPiece first = counted ? pieces[0] : line;
        if (!counted || (first.length == 0u) || (first.text[0] == '#') || identity_piece_is(pieces[2], "-"))
        {
            continue;
        }
        CostRow *const row = &rows->row[rows->row_count];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = row, .bytes = sizeof(*row), .val = 0);
        row->line = at;
        row->least = ULLONG_MAX;
        for (unsigned int field = 0u; field < COST_FIELDS; field += 1u)
        {
            (void)IDENTITY_SET(row->field[field], pieces[field].text, pieces[field].length);
        }
        const CarrierPiece chain = carrier_piece_of(row->field[2].text);
        size_t read_at = 0u;
        unsigned char root = PRECEPT_NONE;
        if (!arrangement_read(chain, &read_at, &row->tree, &root) || (read_at != chain.length) || (root >= PRECEPT_ARG))
        {
            (void)IDENTITY_SET_TEXT(row->verdict, "unread");
        }
        rows->by_operator[rows->row_count] = rows->row_count;
        rows->row_count += 1ULL;
    }
    // the rows by operator, a run of one operator in the order of its rows
    const CostRow *const each = rows->row;
    if (!identity_sorted(rows->by_operator, rows->row_count, [each](unsigned long long one, unsigned long long other) {
            return identity_order_text(each[one].field[0].text, each[other].field[0].text) < 0;
        }))
    {
        cost_rows_ended(rows);
        identity_path_said(kdm, ": the pool cannot meet the order of its rows");
        return 0;
    }
    return 1;
}
