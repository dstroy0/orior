// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_axioms.c: the last check's messages read: the mathematics it found wrong, and each axiom a theorem rests
// on past those trusted, found by the theorem's declaration in the code
#include "validate_internal.h"

// An axiom answer the last check gave: where the theorem's name and its list of axioms are in the messages' texts
typedef struct
{
    unsigned long long name;
    unsigned long long name_size;
    unsigned long long list;
    unsigned long long list_size;
} CliAxioms;

// A theorem's declaration found in the code: its axiom answer, and its line and column
typedef struct
{
    unsigned int axioms;
    unsigned int line;
    unsigned int col;
} CliDeclared;

static CliAxioms s_axioms[CLI_VALIDATE_THEOREMS];
static CliDeclared s_declared[CLI_VALIDATE_THEOREMS];

unsigned int cli_validate_mathematics(void)
{
    const CliProverMessages *const last = &g_cli_validate->last;
    unsigned int answers = 0u;
    for (unsigned int at = 0u; at < last->count; at += 1u)
    {
        const CliProverMessage *const message = &last->message[at];
        if (message->line >= g_cli_validate->lines)
        {
            CliAxioms *const axioms = &s_axioms[answers];
            if ((answers < CLI_VALIDATE_THEOREMS) &&
                cli_validate_axioms_of(last->text + message->text, message->text_size, &axioms->name,
                                       &axioms->name_size, &axioms->list, &axioms->list_size))
            {
                axioms->name += message->text;
                axioms->list += message->text;
                answers += 1u;
            }
            continue;
        }
        if ((message->severity == 1u) && (cli_validate_barrier_of(last, message) >= 0))
        {
            continue;
        }
        const CliFinding finding = {
            message->line, message->col, message->end_line, message->end_col, message->severity, "math", -1,
            0ull,          0ull};
        cli_validate_finding_put(&finding, last->text + message->text, message->text_size, NULL);
    }
    return answers;
}

// whether `size` bytes of `text` and `length` bytes of `other` are the same
static int report_same(const char *text, unsigned long long size, const char *other, unsigned long long length)
{
    return (size == length) && (memor.cmp(&(MemoriaCfg){.src = text, .other = other, .bytes = size}) == 0);
}

// the last part of a dotted name, after its last dot, as its start
static unsigned long long report_short(const char *name, unsigned long long size)
{
    unsigned long long start = size;
    while ((start > 0ull) && (name[start - 1ull] != '.'))
    {
        start -= 1ull;
    }
    return start;
}

// each theorem's declaration found in the code, and the lines it runs to, up to the next theorem's: their count
static unsigned int report_declared(unsigned int answers)
{
    const CliJson *const json = &g_cli_validate->json;
    const char *const text = g_cli_validate->last.text;
    const char *const code = g_cli_validate->code;
    unsigned int count = 0u;
    for (unsigned int answer = 0u; answer < answers; answer += 1u)
    {
        const char *const name = text + s_axioms[answer].name;
        const unsigned long long size = s_axioms[answer].name_size;
        const unsigned long long cut = report_short(name, size);
        for (unsigned int at = 0u; at + 1u < g_cli_validate->word_count; at += 1u)
        {
            const CliCodeWord *const word = &g_cli_validate->words[at];
            const CliCodeWord *const next = &g_cli_validate->words[at + 1u];
            if (!cli_validate_holds(json, g_cli_validate->tool.declarations, code + word->start, word->size) ||
                !(report_same(code + next->start, next->size, name + cut, size - cut) ||
                  report_same(code + next->start, next->size, name, size)))
            {
                continue;
            }
            // in the order of their lines, as a stable sort keeps them
            unsigned int place = count;
            while ((place > 0u) && (s_declared[place - 1u].line > next->line))
            {
                s_declared[place] = s_declared[place - 1u];
                place -= 1u;
            }
            s_declared[place] = (CliDeclared){answer, next->line, next->col};
            count += 1u;
            break;
        }
    }
    return count;
}

void cli_validate_axioms_reported(unsigned int answers, CliLine *untrusted)
{
    const CliJson *const json = &g_cli_validate->json;
    const CliTool *const tool = &g_cli_validate->tool;
    const char *const text = g_cli_validate->last.text;
    const unsigned int declared = report_declared(answers);
    for (unsigned int answer = 0u; answer < answers; answer += 1u)
    {
        const char *const name = text + s_axioms[answer].name;
        const unsigned long long size = s_axioms[answer].name_size;
        unsigned int at = 0u;
        while ((at < declared) && !report_same(text + s_axioms[s_declared[at].axioms].name,
                                               s_axioms[s_declared[at].axioms].name_size, name, size))
        {
            at += 1u;
        }
        char said[16384];
        CliLine said_line = {said, sizeof(said), 0ull};
        int others = 0;
        int all_gaps = 1;
        int any_gap = 0;
        const char *const list = text + s_axioms[answer].list;
        const unsigned long long list_size = s_axioms[answer].list_size;
        for (unsigned long long from = 0ull; from <= list_size;)
        {
            const char *const comma =
                memor.chr(&(MemoriaCfg){.src = list + from, .val = ',', .bytes = list_size - from});
            const unsigned long long end = (comma != NULL) ? (unsigned long long)(comma - list) : list_size;
            unsigned long long start = from;
            unsigned long long length = end - from;
            while ((length > 0ull) && (cellul.chr(&(CatenaFinitaCfg){
                                           .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = list[start]}) != NULL))
            {
                start += 1ull;
                length -= 1ull;
            }
            while ((length > 0ull) &&
                   (cellul.chr(&(CatenaFinitaCfg){
                        .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = list[start + length - 1ull]}) != NULL))
            {
                length -= 1ull;
            }
            from = end + 1ull;
            if ((length == 0ull) || cli_validate_holds(json, tool->trusted, list + start, length))
            {
                continue;
            }
            const int gap = cli_validate_holds(json, tool->gaps, list + start, length);
            all_gaps = all_gaps && gap;
            any_gap = any_gap || gap;
            cli_line_text(&said_line, others ? "; " : "");
            cli_line_counted(&said_line, list + start, length);
            for (unsigned int member = (tool->explains != CLI_JSON_NONE) ? json->node[tool->explains].first
                                                                         : CLI_JSON_NONE;
                 member != CLI_JSON_NONE; member = json->node[member].next)
            {
                if (cli_validate_contains(list + start, length, json->text + json->node[member].key))
                {
                    cli_line_text(&said_line, ", ");
                    cli_line_text(&said_line, cli_json_string(json, member, NULL));
                    break;
                }
            }
            others = 1;
        }
        if (!others || (at == declared))
        {
            continue;
        }
        const unsigned int line = s_declared[at].line;
        const unsigned int col = s_declared[at].col;
        const unsigned int ends = (at + 1u < declared) ? s_declared[at + 1u].line : 0xffffffffu;
        int failed = 0;
        for (unsigned int finding = 0u; !failed && (finding < g_cli_validate->finding_count); finding += 1u)
        {
            const CliFinding *const found = &g_cli_validate->findings[finding];
            // a proof that does not check stands on a gap the prover put there: the error says it
            failed = (found->severity == 1u) &&
                     ((cellul.eq(&(CatenaFinitaCfg){.src = found->kind, .other = "math", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = found->kind, .other = "barrier", .cap = CLI_CELLS_POOL}))) &&
                     (found->line >= line) && (found->line < ends);
        }
        if (failed && all_gaps)
        {
            continue;
        }
        cli_line_text(untrusted, (untrusted->at > 0ull) ? ", " : "");
        cli_line_counted(untrusted, name, size);
        const unsigned long long cut = report_short(name, size);
        unsigned int letters = 0u;
        for (unsigned long long byte = cut; byte < size; byte += 1ull)
        {
            letters += (((unsigned char)name[byte] & 0xc0u) != 0x80u) ? 1u : 0u;
        }
        char message[20480];
        CliLine message_line = {message, sizeof(message), 0ull};
        cli_line_counted(&message_line, name, size);
        cli_line_text(&message_line, " rests on ");
        cli_line_counted(&message_line, said, said_line.at);
        const CliFinding finding = {line, col, line, col + letters, any_gap ? 1u : 2u, "axiom", -1, 0ull, 0ull};
        cli_validate_finding_put(&finding, message, message_line.at, NULL);
    }
}
