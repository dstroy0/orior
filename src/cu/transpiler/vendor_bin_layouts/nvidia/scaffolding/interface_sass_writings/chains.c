// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chains.c: the chains: a chain read, a form put at a link, the chains written for each relation and read back
#include "writings_internal.h"

// one node of a chain: its precept, and the register text of each operand, a leaf's or an earlier node's
typedef struct
{
    unsigned int precept;
    char operand[2][8];
} WritingsNode;

// the expression at `*walk`, a leaf or a precept applied to one or two expressions as the .kdm writes a chain, read
// into `nodes` with its children first, its register text into `operand`: 1, or 0 where it does not read
static int writings_chain_read(const char **walk, WritingsNode *nodes, unsigned int *count, char *operand, size_t room)
{
    *walk += strspn(*walk, " ");
    const size_t length = strspn(*walk, "abcdefghijklmnopqrstuvwxyz0123456789");
    char name[16];
    if ((length == 0u) || (length >= sizeof(name)))
    {
        return 0;
    }
    snprintf(name, sizeof(name), "%.*s", (int)length, *walk);
    *walk += length;
    if (**walk != '(')
    {
        const char *const leaves[4][2] = {{"w0", WRITINGS_LEFT}, {"w1", WRITINGS_RIGHT}, {"zero", "RZ"},
                                          {"ones", WRITINGS_ONES}};
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            if (strcmp(name, leaves[at][0]) == 0)
            {
                return snprintf(operand, room, "%s", leaves[at][1]) < (int)room;
            }
        }
        return 0;
    }
    unsigned int precept = PRECEPT_COUNT;
    for (unsigned int at = 0u; at < PRECEPT_COUNT; at += 1u)
    {
        precept = (strcmp(name, s_precept_text[at]) == 0) ? at : precept;
    }
    WritingsNode node;
    memset(&node, 0, sizeof(node));
    snprintf(node.operand[1], sizeof(node.operand[1]), "RZ");
    node.precept = precept;
    *walk += 1;
    int read = (precept < PRECEPT_COUNT) &&
               writings_chain_read(walk, nodes, count, node.operand[0], sizeof(node.operand[0]));
    *walk += strspn(*walk, " ");
    if (read && (**walk == ','))
    {
        *walk += 1;
        read = writings_chain_read(walk, nodes, count, node.operand[1], sizeof(node.operand[1]));
        *walk += strspn(*walk, " ");
    }
    if (!read || (**walk != ')') || (*count >= WRITINGS_CHAIN_NODES))
    {
        return 0;
    }
    *walk += 1;
    nodes[*count] = node;
    snprintf(operand, room, "R%u", WRITINGS_NODE_FIRST + (2u * *count));
    *count += 1u;
    return 1;
}

// `writing`, a form as the search put it, written to `out` with its result register WRITINGS_RESULT and its two sources
// WRITINGS_LEFT and WRITINGS_RIGHT replaced by `to`, `left` and `right`, each only where it is a whole register token.
// 1, or 0 where `out` will not hold it
static int writings_put(const char *writing, const char *to, const char *left, const char *right, char *out,
                        size_t room)
{
    const char *const from[3] = {WRITINGS_RESULT, WRITINGS_LEFT, WRITINGS_RIGHT};
    const char *const into[3] = {to, left, right};
    size_t at = 0u;
    const char *walk = writing;
    while ((*walk != '\0') && (at < room))
    {
        const int starts = (*walk == 'R') && ((walk == writing) || (strchr(" ,[-~!", walk[-1]) != NULL));
        unsigned int which = 3u;
        for (unsigned int one = 0u; starts && (one < 3u); one += 1u)
        {
            const size_t length = strlen(from[one]);
            if ((strncmp(walk, from[one], length) == 0) && ((walk[length] < '0') || (walk[length] > '9')))
            {
                which = one;
            }
        }
        if (which < 3u)
        {
            at += (size_t)snprintf(&out[at], room - at, "%s", into[which]);
            walk += strlen(from[which]);
            continue;
        }
        out[at] = *walk;
        at += 1u;
        walk += 1;
    }
    if (at >= room)
    {
        return 0;
    }
    out[at] = '\0';
    return 1;
}

// the first form the search found holding `precept`, by its number among the forms, or s_form_count where none does
static unsigned int writings_for(unsigned int precept)
{
    unsigned int relation = s_relations;
    for (unsigned int at = 0u; at < s_relations; at += 1u)
    {
        relation = (s_relation[at].precept && (s_relation[at].which == precept)) ? at : relation;
    }
    for (unsigned int number = 0u; (relation < s_relations) && (number < s_form_count); number += 1u)
    {
        if (s_ran[number] && s_holds[number][relation])
        {
            return number;
        }
    }
    return s_form_count;
}

// Every chain of the .kdm at `kdm` written into a cubin of its own in `into`, each node the first writing the search in
// `folder` found for its precept, with <into>/list.txt and <into>/chains.txt beside them. A row's sixth column, a
// verdict a descent gave it, is kept in chains.txt. 0, or 2 where nothing could be read or written
int writings_chains_write(const char *pattern, const char *kernel_text, const char *machine, const char *folder,
                                 const char *kdm, const char *into)
{
    s_pattern_size = writings_file_read(pattern, s_pattern, sizeof(s_pattern));
    const unsigned long long kernel_text_size = writings_file_read(kernel_text, (unsigned char *)s_kernel_text, sizeof(s_kernel_text) - 1u);
    s_kernel_text[kernel_text_size] = '\0';
    if ((s_pattern_size == 0ull) || (kernel_text_size == 0ull) || !writings_kernel(s_kernel_text, s_kernel, sizeof(s_kernel)) ||
        !sass_machine_read(&s_machine, machine) || !writings_load(folder))
    {
        fprintf(stderr, "the pattern, the kernel, the machine file or the search in %s did not read\n", folder);
        return 2;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/list.txt", into);
    FILE *const list = fopen(path, "wb");
    snprintf(path, sizeof(path), "%s/chains.txt", into);
    FILE *const chains = fopen(path, "wb");
    // the .kdm as the run holds it (kdm.h)
    static KdmFile s_rows;
    if ((list == NULL) || (chains == NULL) || !kdm_read(&s_rows, kdm) || (s_rows.lines == 0u))
    {
        fprintf(stderr, "the folder %s or the .kdm %s was not reached\n", into, kdm);
        return 2;
    }
    for (unsigned int precept = 0u; precept < PRECEPT_COUNT; precept += 1u)
    {
        const unsigned int writing = writings_for(precept);
        if (writing < s_form_count)
        {
            printf("  %-4s %s\n", s_precept_text[precept], s_forms[writing]);
        }
    }
    unsigned int read = 0u;
    unsigned int unwritten = 0u;
    unsigned int written = 0u;
    static KdmRow s_row;
    for (unsigned int row = 0u; (written < WRITINGS_CHAINS_MOST) && (row < s_rows.lines); row += 1u)
    {
        if (!kdm_row_read(s_rows.line[row], &s_row) || (s_row.nodes == 0u))
        {
            continue;
        }
        const char *const operator_name = s_row.operation;
        const char *const chain = s_row.chain;
        const unsigned int verdict = (s_row.verdict < 0) ? 2u : (unsigned int)s_row.verdict;
        read += 1u;
        WritingsNode nodes[WRITINGS_CHAIN_NODES];
        unsigned int count = 0u;
        char root[8];
        const char *walk = chain;
        int fits = writings_chain_read(&walk, nodes, &count, root, sizeof(root)) && (count != 0u);
        char lines[4096];
        size_t at = (size_t)snprintf(lines, sizeof(lines), WRITINGS_HEAD WRITINGS_CHAIN_HEAD);
        for (unsigned int node = 0u; fits && (node < count); node += 1u)
        {
            const unsigned int writing = writings_for(nodes[node].precept);
            char to[8];
            snprintf(to, sizeof(to), "R%u", WRITINGS_NODE_FIRST + (2u * node));
            char put[256];
            fits = (writing < s_form_count) &&
                   writings_put(s_forms[writing], (node + 1u == count) ? WRITINGS_RESULT : to, nodes[node].operand[0],
                                nodes[node].operand[1], put, sizeof(put)) &&
                   ((at += (size_t)snprintf(&lines[at], sizeof(lines) - at, "%s%s", (node == 0u) ? "" : "\n", put)) <
                    sizeof(lines));
        }
        fits = fits && (snprintf(&lines[at], sizeof(lines) - at, WRITINGS_TAIL) < (int)(sizeof(lines) - at)) &&
               writings_splice(s_kernel_text, lines, s_asking, sizeof(s_asking));
        const unsigned int instructions =
            fits ? sass_assemble_lines(&s_machine, s_asking, SASS_CONTROL_SAFE, s_code, sizeof(s_code)) : 0u;
        char cubin[1024];
        snprintf(cubin, sizeof(cubin), "%s/chain_%05u.cubin", into, written);
        if ((instructions == 0u) || !writings_cubin(instructions, cubin))
        {
            unwritten += 1u;
            continue;
        }
        fprintf(list, "%s %s\n", cubin, s_kernel);
        fprintf(chains, "%u\t%s\t%s\t%s\n", written, operator_name, chain,
                (verdict == 1u) ? "standing" : ((verdict == 0u) ? "out" : "-"));
        written += 1u;
    }
    kdm_free(&s_rows);
    fclose(list);
    fclose(chains);
    printf("interface sass chains: %u chains of the .kdm read, %u written and held to cubin_safe, %u not written\n", read,
           written, unwritten);
    return 0;
}

// The chains' answers read back against the relation each chain is a row of, written to `record`: 0, or 2 where
// nothing could be read
int writings_chains_read(const char *folder, const char *record)
{
    writings_cases();
    static char s_chain[WRITINGS_CHAINS_MOST][512];
    static unsigned int s_chain_anchor[WRITINGS_CHAINS_MOST];
    static unsigned int s_chain_answered[WRITINGS_CHAINS_MOST][WRITINGS_CASES];
    static unsigned char s_chain_ran[WRITINGS_CHAINS_MOST];
    char path[1024];
    char line[WRITINGS_LINE];
    snprintf(path, sizeof(path), "%s/chains/chains.txt", folder);
    FILE *const chains = fopen(path, "rb");
    snprintf(path, sizeof(path), "%s/chains/answers.txt", folder);
    FILE *const answers = fopen(path, "rb");
    FILE *const out = fopen(record, "wb");
    if ((chains == NULL) || (answers == NULL) || (out == NULL))
    {
        fprintf(stderr, "the chains, their answers in %s or the record %s was not reached\n", folder, record);
        return 2;
    }
    unsigned int count = 0u;
    while ((count < WRITINGS_CHAINS_MOST) && (fgets(line, sizeof(line), chains) != NULL))
    {
        unsigned int number = 0u;
        char operator_name[32];
        if (sscanf(line, "%u\t%31[^\t]\t%511[^\t\r\n]", &number, operator_name, s_chain[count]) != 3)
        {
            continue;
        }
        s_chain_anchor[count] = LADDER_ANCHOR_COUNT;
        for (unsigned int anchor = 0u; anchor < LADDER_ANCHOR_COUNT; anchor += 1u)
        {
            s_chain_anchor[count] = (strcmp(operator_name, s_anchor_text[anchor]) == 0) ? anchor : s_chain_anchor[count];
        }
        count += 1u;
    }
    fclose(chains);
    unsigned int ran = 0u;
    unsigned int refused = 0u;
    writings_answers_read(answers, count, s_chain_answered, s_chain_ran, &ran, &refused);
    fclose(answers);
    unsigned int put[LADDER_ANCHOR_COUNT] = {0u};
    unsigned int held[LADDER_ANCHOR_COUNT] = {0u};
    fprintf(out, "# Chains run on the part\n\n");
    fprintf(out, "Written by `interface_sass_writings.sh` whole on every run. Every arrangement of the part's .kdm is "
                 "written node by node, each node the first writing `interface_sass_writings.md` found for its "
                 "precept, run on the part over the search's cases, and read back against the relation the "
                 "arrangement is a row of. %u chains ran and %u were refused by the part.\n\n",
            ran, refused);
    fprintf(out, "## Chains that did not answer as their relation\n\n");
    unsigned int differed = 0u;
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        const unsigned int anchor = s_chain_anchor[number];
        if (!s_chain_ran[number] || (anchor >= LADDER_ANCHOR_COUNT))
        {
            continue;
        }
        unsigned int agreed = 0u;
        for (unsigned int at = 0u; at < s_cases; at += 1u)
        {
            unsigned int expected = 0u;
            ladder_answer(anchor, s_case[at], 2u, &expected);
            agreed += (s_chain_answered[number][at] == expected) ? 1u : 0u;
        }
        put[anchor] += 1u;
        held[anchor] += (agreed == s_cases) ? 1u : 0u;
        if (agreed != s_cases)
        {
            fprintf(out, "- %s `%s`: %u of %u cases\n", s_anchor_text[anchor], s_chain[number], agreed, s_cases);
            differed += 1u;
        }
    }
    fprintf(out, "%s\n## By relation\n\n| relation | chains run | answering every case |\n|---|---|---|\n",
            (differed == 0u) ? "None.\n" : "");
    for (unsigned int anchor = 0u; anchor < LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        if (put[anchor] != 0u)
        {
            fprintf(out, "| %s | %u | %u |\n", s_anchor_text[anchor], put[anchor], held[anchor]);
            printf("  %-8s %u chains run, %u answering every case\n", s_anchor_text[anchor], put[anchor], held[anchor]);
        }
    }
    fclose(out);
    printf("interface sass chains: %u ran, %u refused, %u answered otherwise than their relation, the record written to "
           "%s\n",
           ran, refused, differed, record);
    return 0;
}
