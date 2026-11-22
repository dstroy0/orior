// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chains.c: the chains: a chain read, a form put at a link, the chains written for each relation and read back
#include "writings_internal.h"
#include "../../../../../../platform/platform.h"
#include "../../../../../engine/engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "../../../../../../../includes/parsers/line_read.h"

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
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_put_max(&buffer_line, *walk, (size_t)((int)length));
        (void)mmgr_line_finish(&buffer_line);
    }
    *walk += length;
    if (**walk != '(')
    {
        const char *const leaves[4][2] = {
            {"w0", WRITINGS_LEFT}, {"w1", WRITINGS_RIGHT}, {"zero", "RZ"}, {"ones", WRITINGS_ONES}};
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            if (mmgr_text_equal(name, leaves[at][0], PLATFORM_TEXT_TO_TERMINATOR))
            {
                VerbaLinea formed_line_1 = mmgr_line_over(operand, room);
                mmgr_line_put(&formed_line_1, leaves[at][1]);
                (void)mmgr_line_finish(&formed_line_1);
                return (int)formed_line_1.at < (int)room;
            }
        }
        return 0;
    }
    unsigned int precept = PRECEPT_COUNT;
    for (unsigned int at = 0u; at < PRECEPT_COUNT; at += 1u)
    {
        precept = (mmgr_text_equal(name, s_precept_text[at], PLATFORM_TEXT_TO_TERMINATOR)) ? at : precept;
    }
    WritingsNode node;
    mmgr_bytes_set(&node, 0, sizeof(node));
    {
        VerbaLinea buffer_line = mmgr_line_over(node.operand[1], sizeof(node.operand[1]));
        mmgr_line_put(&buffer_line, "RZ");
        (void)mmgr_line_finish(&buffer_line);
    }
    node.precept = precept;
    *walk += 1;
    int read =
        (precept < PRECEPT_COUNT) && writings_chain_read(walk, nodes, count, node.operand[0], sizeof(node.operand[0]));
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
    {
        VerbaLinea buffer_line = mmgr_line_over(operand, room);
        mmgr_line_put(&buffer_line, "R");
        mmgr_line_decimal(&buffer_line, WRITINGS_NODE_FIRST + (2u * *count), 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    *count += 1u;
    return 1;
}

// `writing`, a form as the search put it, written to `out` with its result register WRITINGS_RESULT and its two sources
// WRITINGS_LEFT and WRITINGS_RIGHT replaced by `to`, `left` and `right`, each only where it is a whole register token.
// 1, or 0 where `out` will not fit it
static int writings_put(const char *writing, const char *to, const char *left, const char *right, char *out,
                        size_t room)
{
    const char *const from[3] = {WRITINGS_RESULT, WRITINGS_LEFT, WRITINGS_RIGHT};
    const char *const into[3] = {to, left, right};
    size_t at = 0u;
    const char *walk = writing;
    while ((*walk != '\0') && (at < room))
    {
        const int starts =
            (*walk == 'R') &&
            ((walk == writing) || (mmgr_text_char(" ,[-~!", walk[-1], PLATFORM_TEXT_TO_TERMINATOR) != NULL));
        unsigned int which = 3u;
        for (unsigned int one = 0u; starts && (one < 3u); one += 1u)
        {
            const size_t length = mmgr_text_length(from[one], PLATFORM_TEXT_TO_TERMINATOR);
            if ((mmgr_text_order(walk, from[one], (size_t)(length)) == 0) &&
                ((walk[length] < '0') || (walk[length] > '9')))
            {
                which = one;
            }
        }
        if (which < 3u)
        {
            VerbaLinea formed_line_2 = mmgr_line_over(&out[at], room - at);
            mmgr_line_put(&formed_line_2, into[which]);
            (void)mmgr_line_finish(&formed_line_2);
            at += (size_t)(int)formed_line_2.at;
            walk += mmgr_text_length(from[which], PLATFORM_TEXT_TO_TERMINATOR);
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

// the first form the search found containing `precept`, by its number among the forms, or s_form_count where none does
static unsigned int writings_for(unsigned int precept)
{
    unsigned int relation = s_relations;
    for (unsigned int at = 0u; at < s_relations; at += 1u)
    {
        relation = (s_relation[at].precept && (s_relation[at].which == precept)) ? at : relation;
    }
    for (unsigned int number = 0u; (relation < s_relations) && (number < s_form_count); number += 1u)
    {
        if (s_ran[number] && s_relation_agreed[number][relation])
        {
            return number;
        }
    }
    return s_form_count;
}

// Every chain of the .kdm at `kdm` written into a cubin of its own in `into`, each node the first writing the search in
// `folder` found for its precept, with <into>/list.txt and <into>/chains.txt beside them. A row's sixth column, a
// verdict a descent gave it, is recorded in chains.txt. 0, or 2 where nothing could be read or written
int writings_chains_write(const char *pattern, const char *kernel_text, const char *machine, const char *folder,
                          const char *kdm, const char *into)
{
    s_pattern_size = writings_file_read(pattern, s_pattern, sizeof(s_pattern));
    const unsigned long long kernel_text_size =
        writings_file_read(kernel_text, (unsigned char *)s_kernel_text, sizeof(s_kernel_text) - 1u);
    s_kernel_text[kernel_text_size] = '\0';
    if ((s_pattern_size == 0ULL) || (kernel_text_size == 0ULL) ||
        !writings_kernel(s_kernel_text, s_kernel, sizeof(s_kernel)) || !sass_machine_read(&s_machine, machine) ||
        !writings_load(folder))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the pattern, the kernel, the machine file or the search in ");
            mmgr_line_put(&engine_note.line, folder);
            mmgr_line_put(&engine_note.line, " did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, into);
        mmgr_line_put(&buffer_line, "/list.txt");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const list = platform_file_open(path, "wb");
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, into);
        mmgr_line_put(&buffer_line, "/chains.txt");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const chains = platform_file_open(path, "wb");
    // the .kdm as the run stores it (kdm.h)
    static KdmFile s_rows;
    if ((list == NULL) || (chains == NULL) || !kdm_read(&s_rows, kdm) || (s_rows.lines == 0u))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the folder ");
            mmgr_line_put(&engine_note.line, into);
            mmgr_line_put(&engine_note.line, " or the .kdm ");
            mmgr_line_put(&engine_note.line, kdm);
            mmgr_line_put(&engine_note.line, " was not reached");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    for (unsigned int precept = 0u; precept < PRECEPT_COUNT; precept += 1u)
    {
        const unsigned int writing = writings_for(precept);
        if (writing < s_form_count)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, s_precept_text[precept], 4u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, s_forms[writing]);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    unsigned int read = 0u;
    unsigned int unwritten = 0u;
    unsigned int written = 0u;
    static KdmRow s_row;
    for (unsigned int row = 0u; (written < WRITINGS_CHAINS_MAX) && (row < s_rows.lines); row += 1u)
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
        VerbaLinea formed_line_head = mmgr_line_over(lines, sizeof(lines));
        mmgr_line_put(&formed_line_head, WRITINGS_HEAD WRITINGS_CHAIN_HEAD);
        (void)mmgr_line_finish(&formed_line_head);
        size_t at = formed_line_head.at;
        for (unsigned int node = 0u; fits && (node < count); node += 1u)
        {
            const unsigned int writing = writings_for(nodes[node].precept);
            char to[8];
            {
                VerbaLinea buffer_line = mmgr_line_over(to, sizeof(to));
                mmgr_line_put(&buffer_line, "R");
                mmgr_line_decimal(&buffer_line, WRITINGS_NODE_FIRST + (2u * node), 1u);
                (void)mmgr_line_finish(&buffer_line);
            }
            char put[256];
            VerbaLinea formed_line_9;
            fits = (writing < s_form_count) &&
                   writings_put(s_forms[writing], (node + 1u == count) ? WRITINGS_RESULT : to, nodes[node].operand[0],
                                nodes[node].operand[1], put, sizeof(put)) &&
                   ((at += (size_t)(formed_line_9 = mmgr_line_over(&lines[at], sizeof(lines) - at),
                                    mmgr_line_put(&formed_line_9, (node == 0u) ? "" : "\n"),
                                    mmgr_line_put(&formed_line_9, put), (void)mmgr_line_finish(&formed_line_9),
                                    (int)formed_line_9.at)) < sizeof(lines));
        }
        VerbaLinea formed_line_tail;
        fits = fits &&
               ((formed_line_tail = mmgr_line_over(&lines[at], sizeof(lines) - at),
                 mmgr_line_put(&formed_line_tail, WRITINGS_TAIL), (void)mmgr_line_finish(&formed_line_tail),
                 (int)formed_line_tail.at) < (int)(sizeof(lines) - at)) &&
               writings_splice(s_kernel_text, lines, s_query_text, sizeof(s_query_text));
        const unsigned int instructions =
            fits ? sass_assemble_lines(&s_machine, s_query_text, SASS_CONTROL_SAFE, s_code, sizeof(s_code)) : 0u;
        char cubin[1024];
        {
            VerbaLinea buffer_line = mmgr_line_over(cubin, sizeof(cubin));
            mmgr_line_put(&buffer_line, into);
            mmgr_line_put(&buffer_line, "/chain_");
            mmgr_line_decimal(&buffer_line, written, 5u);
            mmgr_line_put(&buffer_line, ".cubin");
            (void)mmgr_line_finish(&buffer_line);
        }
        if ((instructions == 0u) || !writings_cubin(instructions, cubin))
        {
            unwritten += 1u;
            continue;
        }
        {
            mmgr_line_put(platform_file_line(list), cubin);
            mmgr_line_put(platform_file_line(list), " ");
            mmgr_line_put(platform_file_line(list), s_kernel);
            mmgr_line_put(platform_file_line(list), "\n");
        }
        {
            mmgr_line_decimal(platform_file_line(chains), written, 1u);
            mmgr_line_put(platform_file_line(chains), "\t");
            mmgr_line_put(platform_file_line(chains), operator_name);
            mmgr_line_put(platform_file_line(chains), "\t");
            mmgr_line_put(platform_file_line(chains), chain);
            mmgr_line_put(platform_file_line(chains), "\t");
            mmgr_line_put(platform_file_line(chains), (verdict == 1u) ? "located" : ((verdict == 0u) ? "out" : "-"));
            mmgr_line_put(platform_file_line(chains), "\n");
        }
        written += 1u;
    }
    kdm_free(&s_rows);
    platform_file_close(list);
    platform_file_close(chains);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass chains: ");
        mmgr_line_decimal(output_line, read, 1u);
        mmgr_line_put(output_line, " chains of the .kdm read, ");
        mmgr_line_decimal(output_line, written, 1u);
        mmgr_line_put(output_line, " written and checked by cubin_safe, ");
        mmgr_line_decimal(output_line, unwritten, 1u);
        mmgr_line_put(output_line, " not written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}

// The chains' answers read back against the relation each chain is a row of, written to `record`: 0, or 2 where
// nothing could be read
int writings_chains_read(const char *folder, const char *record)
{
    writings_cases();
    static char s_chain[WRITINGS_CHAINS_MAX][512];
    static unsigned int s_chain_anchor[WRITINGS_CHAINS_MAX];
    static unsigned int s_chain_answered[WRITINGS_CHAINS_MAX][WRITINGS_CASES];
    static unsigned char s_chain_ran[WRITINGS_CHAINS_MAX];
    char path[1024];
    char line[WRITINGS_LINE];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/chains/chains.txt");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const chains = platform_file_open(path, "rb");
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/chains/answers.txt");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const answers = platform_file_open(path, "rb");
    PlatformFile *const out = platform_file_open(record, "wb");
    if ((chains == NULL) || (answers == NULL) || (out == NULL))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the chains, their answers in ");
            mmgr_line_put(&engine_note.line, folder);
            mmgr_line_put(&engine_note.line, " or the record ");
            mmgr_line_put(&engine_note.line, record);
            mmgr_line_put(&engine_note.line, " was not reached");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    unsigned int count = 0u;
    while ((count < WRITINGS_CHAINS_MAX) && (platform_file_text_read(line, sizeof(line), chains) != NULL))
    {
        unsigned int number = 0u;
        char operator_name[32];
        const char *read = line;
        int whole = line_unsigned(&read, &number) && (*read == '\t');
        read += whole ? 1 : 0;
        whole = whole && line_until(&read, '\t', operator_name, sizeof(operator_name)) && (*read == '\t');
        read += whole ? 1 : 0;
        if (!whole || !line_until(&read, '\t', s_chain[count], sizeof(s_chain[count])))
        {
            continue;
        }
        s_chain_anchor[count] = LADDER_ANCHOR_COUNT;
        for (unsigned int anchor = 0u; anchor < LADDER_ANCHOR_COUNT; anchor += 1u)
        {
            s_chain_anchor[count] = (mmgr_text_equal(operator_name, s_anchor_text[anchor], PLATFORM_TEXT_TO_TERMINATOR))
                                        ? anchor
                                        : s_chain_anchor[count];
        }
        count += 1u;
    }
    platform_file_close(chains);
    unsigned int ran = 0u;
    unsigned int refused = 0u;
    writings_answers_read(answers, count, s_chain_answered, s_chain_ran, &ran, &refused);
    platform_file_close(answers);
    unsigned int put[LADDER_ANCHOR_COUNT] = {0u};
    unsigned int anchor_counts[LADDER_ANCHOR_COUNT] = {0u};
    {
        mmgr_line_put(platform_file_line(out), "# Chains run on the part\n\n");
    }
    {
        mmgr_line_put(
            platform_file_line(out),
            "Written by `interface_sass_writings.sh` whole on every run. Every arrangement of the part's .kdm is "
            "written node by node, each node the first writing `interface_sass_writings.md` found for its precept, run "
            "on the part over the search's cases, and read back against the relation the arrangement is a row of. ");
        mmgr_line_decimal(platform_file_line(out), ran, 1u);
        mmgr_line_put(platform_file_line(out), " chains ran and ");
        mmgr_line_decimal(platform_file_line(out), refused, 1u);
        mmgr_line_put(platform_file_line(out), " were refused by the part.\n\n");
    }
    {
        mmgr_line_put(platform_file_line(out), "## Chains that did not answer as their relation\n\n");
    }
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
        anchor_counts[anchor] += (agreed == s_cases) ? 1u : 0u;
        if (agreed != s_cases)
        {
            {
                mmgr_line_put(platform_file_line(out), "- ");
                mmgr_line_put(platform_file_line(out), s_anchor_text[anchor]);
                mmgr_line_put(platform_file_line(out), " `");
                mmgr_line_put(platform_file_line(out), s_chain[number]);
                mmgr_line_put(platform_file_line(out), "`: ");
                mmgr_line_decimal(platform_file_line(out), agreed, 1u);
                mmgr_line_put(platform_file_line(out), " of ");
                mmgr_line_decimal(platform_file_line(out), s_cases, 1u);
                mmgr_line_put(platform_file_line(out), " cases\n");
            }
            differed += 1u;
        }
    }
    {
        mmgr_line_put(platform_file_line(out), (differed == 0u) ? "None.\n" : "");
        mmgr_line_put(platform_file_line(out),
                      "\n## By relation\n\n| relation | chains run | answering every case |\n|---|---|---|\n");
    }
    for (unsigned int anchor = 0u; anchor < LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        if (put[anchor] != 0u)
        {
            {
                mmgr_line_put(platform_file_line(out), "| ");
                mmgr_line_put(platform_file_line(out), s_anchor_text[anchor]);
                mmgr_line_put(platform_file_line(out), " | ");
                mmgr_line_decimal(platform_file_line(out), put[anchor], 1u);
                mmgr_line_put(platform_file_line(out), " | ");
                mmgr_line_decimal(platform_file_line(out), anchor_counts[anchor], 1u);
                mmgr_line_put(platform_file_line(out), " |\n");
            }
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put_columns(output_line, s_anchor_text[anchor], 8u);
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal(output_line, put[anchor], 1u);
                mmgr_line_put(output_line, " chains run, ");
                mmgr_line_decimal(output_line, anchor_counts[anchor], 1u);
                mmgr_line_put(output_line, " answering every case");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    platform_file_close(out);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "interface sass chains: ");
        mmgr_line_decimal(output_line, ran, 1u);
        mmgr_line_put(output_line, " ran, ");
        mmgr_line_decimal(output_line, refused, 1u);
        mmgr_line_put(output_line, " refused, ");
        mmgr_line_decimal(output_line, differed, 1u);
        mmgr_line_put(output_line, " answered otherwise than their relation, the record written to ");
        mmgr_line_put(output_line, record);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
