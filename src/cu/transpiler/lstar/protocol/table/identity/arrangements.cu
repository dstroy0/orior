// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// arrangements.cu: the arrangements of primitives that produce each operator, read from the .kdm and written as a chain of ours
#include "identity_internal.h"
#include "precept_value.h"

#define COST_PRECEPT_TEXT(name_, text_, arity_) text_,
#define COST_PRECEPT_ARITY(name_, text_, arity_) arity_,
static const char *const s_precept_text[] = {PRECEPTS(COST_PRECEPT_TEXT)};
static const unsigned int s_precept_arity[] = {PRECEPTS(COST_PRECEPT_ARITY)};
#undef COST_PRECEPT_TEXT
#undef COST_PRECEPT_ARITY

// The tree the arrangement `text` writes from `at`, as chain_build.c writes one: a precept applied to its children, or
// a leaf, an operand `w<place>`, `zero` or `ones`. Its nodes appended to `nodes` children first, and the node or the
// leaf it is into `child`. 1, or 0 where the text is no tree over the precepts
static int arrangement_read(const std::string &text, size_t *at, std::vector<PreceptNode> *nodes, unsigned char *child)
{
    while ((*at < text.size()) && (text[*at] == ' '))
    {
        *at += 1u;
    }
    size_t end = *at;
    while ((end < text.size()) && (isalnum((unsigned char)text[end]) != 0))
    {
        end += 1u;
    }
    const std::string name = text.substr(*at, end - *at);
    *at = end;
    if ((name == "zero") || (name == "ones"))
    {
        *child = (name == "zero") ? PRECEPT_ZERO : PRECEPT_ONES;
        return 1;
    }
    if ((name.size() == 2u) && (name[0] == 'w') && (isdigit((unsigned char)name[1]) != 0))
    {
        *child = PRECEPT_ARG_AT((unsigned int)(name[1] - '0'));
        return 1;
    }
    unsigned int precept = PRECEPT_COUNT;
    for (unsigned int each = 0u; each < PRECEPT_COUNT; each += 1u)
    {
        precept = (name == s_precept_text[each]) ? each : precept;
    }
    if ((precept == PRECEPT_COUNT) || (s_precept_arity[precept] == 0u) || (*at >= text.size()) || (text[*at] != '('))
    {
        return 0;
    }
    *at += 1u;
    unsigned char left = PRECEPT_NONE;
    unsigned char right = PRECEPT_NONE;
    if (!arrangement_read(text, at, nodes, &left))
    {
        return 0;
    }
    if (s_precept_arity[precept] == 2u)
    {
        if ((*at >= text.size()) || (text[*at] != ','))
        {
            return 0;
        }
        *at += 1u;
        if (!arrangement_read(text, at, nodes, &right))
        {
            return 0;
        }
    }
    if ((*at >= text.size()) || (text[*at] != ')') || (nodes->size() >= PRECEPT_NODES))
    {
        return 0;
    }
    *at += 1u;
    // a node the tree holds already is that node, as chain_build.c counts it, and is written once
    for (size_t held = 0u; held < nodes->size(); held += 1u)
    {
        const PreceptNode &node = (*nodes)[held];
        if ((node.precept == precept) && (node.left == left) && (node.right == right))
        {
            *child = (unsigned char)held;
            return 1;
        }
    }
    nodes->push_back(PreceptNode{(unsigned char)precept, left, right});
    *child = (unsigned char)(nodes->size() - 1u);
    return 1;
}

// the word of the web of two reads whose tree answers as `nodes` does on every pair of the low words of the values a
// question is put with: the word an operator's arrangements produce, or NULL where none is
const Word *word_produced(const std::vector<PreceptNode> &nodes)
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
                given.operand[0] = (unsigned int)(s_values[left] & 0xffffffffull);
                given.operand[1] = (unsigned int)(s_values[right] & 0xffffffffull);
                unsigned int by_word = 0u;
                unsigned int by_nodes = 0u;
                alike = precept_value(word->node, word->nodes, &given, &by_word) &&
                        precept_value(nodes.data(), (unsigned int)nodes.size(), &given, &by_nodes) &&
                        (by_word == by_nodes);
            }
        }
        if (alike)
        {
            return word;
        }
    }
    return NULL;
}

// The arrangement `nodes` written through `sass` node by node, each node in the form of the web's word that is its
// precept alone (word_web.h): a leaf the operand of `operands` at its place, RZ for a word of zeroes and `ones` for a
// word of ones, every node but the root written to a register of its own numbered from `next` on, and the root to
// `to`. Empty where a precept has no such word or the ruleset writes none of it
std::string arrangement_written(const Ruleset *sass, const std::vector<PreceptNode> &nodes,
                                       const std::vector<std::string> &operands, const std::string &ones,
                                       const std::string &to, unsigned int next)
{
    const auto none = [](const std::string &) { return std::string(); };
    std::vector<std::string> held;
    std::string written;
    for (size_t at = 0u; at < nodes.size(); at += 1u)
    {
        const Word *const word = word_web_word_for(nodes[at].precept);
        // the ruleset writes nothing of a form given another count of arguments
        if (word == NULL)
        {
            return std::string();
        }
        held.push_back(((at + 1u) == nodes.size()) ? to : ("R" + std::to_string(next + at)));
        std::vector<std::string> arguments = {held[at]};
        const unsigned char child[2] = {nodes[at].left, nodes[at].right};
        for (unsigned int which = 0u; which < word->reads; which += 1u)
        {
            const unsigned char leaf = child[which];
            const size_t place = (size_t)(leaf - PRECEPT_ARG);
            if (((leaf == PRECEPT_ONES) && ones.empty()) || ((leaf >= PRECEPT_ARG) && (leaf != PRECEPT_ZERO) &&
                                                             (leaf != PRECEPT_ONES) && (place >= operands.size())))
            {
                return std::string();
            }
            arguments.push_back((leaf == PRECEPT_ZERO)   ? std::string("RZ")
                                : (leaf == PRECEPT_ONES) ? ones
                                : (leaf >= PRECEPT_ARG)  ? operands[place]
                                                         : held[leaf]);
        }
        std::string line;
        if (ruleset_opcode(sass, word->name, arguments, none, line) == 0)
        {
            return std::string();
        }
        written += line;
    }
    return written;
}

// the lines of the .kdm at `kdm` into `lines`, each arrangement's row into `rows`, and each operator's rows by their
// place into `operators`; a row whose chain is no tree over the precepts is `unread`
void kdm_rows_read(const char *kdm, std::vector<std::string> *lines, std::vector<CostRow> *rows,
                          std::map<std::string, std::vector<size_t>> *operators)
{
    // the .kdm as the run holds it (kdm.h), a line each
    KdmFile held = {};
    kdm_read(&held, kdm);
    for (unsigned int at = 0u; at < held.lines; at += 1u)
    {
        lines->push_back(held.line[at]);
    }
    kdm_free(&held);
    for (size_t at = 0u; at < lines->size(); at += 1u)
    {
        std::vector<std::string> fields;
        std::stringstream fielded((*lines)[at]);
        std::string field;
        while (std::getline(fielded, field, '\t'))
        {
            fields.push_back(field);
        }
        if ((fields.size() != 5u) || fields[0].empty() || (fields[0][0] == '#') || (fields[2] == "-"))
        {
            continue;
        }
        CostRow row{at, fields, {}, "", ULLONG_MAX};
        size_t read_at = 0u;
        unsigned char root = PRECEPT_NONE;
        if (!arrangement_read(fields[2], &read_at, &row.nodes, &root) || (read_at != fields[2].size()) ||
            (root >= PRECEPT_ARG))
        {
            row.verdict = "unread";
        }
        (*operators)[fields[0]].push_back(rows->size());
        rows->push_back(row);
    }
}
