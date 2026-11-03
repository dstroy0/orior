// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_axioms.c: the last check's messages read: the mathematics it found wrong, and each axiom a theorem rests
// on past those trusted, found by the theorem's declaration in the code
#include "../text/text.h"
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
                EMBED_CALL(cli_validate_text_internal.axioms_get, CliValidateTextCtx,
                           .text = last->text + message->text, .size = message->text_size, .name = &axioms->name,
                           .name_size = &axioms->name_size, .list = &axioms->list, .list_size = &axioms->list_size))
            {
                axioms->name += message->text;
                axioms->list += message->text;
                answers += 1u;
            }
            continue;
        }
        if ((message->severity == 1u) && (EMBED_CALL(cli_validate_messages_internal.barrier_get, CliValidateMessagesCtx,
                                                     .messages_read = last, .message = message) >= 0))
        {
            continue;
        }
        const CliFinding finding = {
            message->line, message->col, message->end_line, message->end_col, message->severity, "math", -1,
            0ULL,          0ULL};
        EMBED_CALL(cli_validate_length_internal.finding_append, CliValidateLengthCtx, .finding = &finding,
                   .message_text = last->text + message->text, .length = message->text_size, .lifted = NULL);
    }
    return answers;
}

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *other;
    unsigned long long length;
    const char *name;
    unsigned int answers;
} ValidateAxiomsCtx;
// whether `size` bytes of `text` and `length` bytes of `other` are the same
EMBED_INLINE int report_same(const ValidateAxiomsCtx *args)
{
    const char *other = args->other;
    return (args->size == args->length) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text, .other = other, .bytes = args->size}) == 0);
}

// the last part of a dotted name, after its last dot, as its start
EMBED_INLINE unsigned long long report_short(const ValidateAxiomsCtx *args)
{
    unsigned long long start = args->size;
    start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->name, .at = start,
                       .set = ".");
    return start;
}

// each theorem's declaration found in the code, and the lines it runs to, up to the next theorem's: their count
EMBED_INLINE unsigned int report_declare(const ValidateAxiomsCtx *args)
{
    const CliJson *const json = &g_cli_validate->json;
    const char *const text = g_cli_validate->last.text;
    const char *const code = g_cli_validate->code;
    unsigned int count = 0u;
    for (unsigned int answer = 0u; answer < args->answers; answer += 1u)
    {
        const char *const name = text + s_axioms[answer].name;
        const unsigned long long size = s_axioms[answer].name_size;
        const unsigned long long cut = EMBED_CALL(report_short, ValidateAxiomsCtx, .name = name, .size = size);
        for (unsigned int at = 0u; at + 1u < g_cli_validate->word_count; at += 1u)
        {
            const CliCodeWord *const word_text = &g_cli_validate->words[at];
            const CliCodeWord *const next = &g_cli_validate->words[at + 1u];
            if (!EMBED_CALL(cli_validate_json_internal.in_array, CliValidateJsonCtx, .json = json,
                            .array = g_cli_validate->tool.declarations, .text = code + word_text->start,
                            .length = word_text->size) ||
                !(EMBED_CALL(report_same, ValidateAxiomsCtx, .text = code + next->start, .size = next->size,
                             .other = name + cut, .length = size - cut) ||
                  EMBED_CALL(report_same, ValidateAxiomsCtx, .text = code + next->start, .size = next->size,
                             .other = name, .length = size)))
            {
                continue;
            }
            // in the order of their lines, as a stable sort orders them
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

void cli_validate_axioms_report(const CliValidateAnswersCtx *args)
{
    const CliJson *const json = &g_cli_validate->json;
    const CliTool *const tool = &g_cli_validate->tool;
    const char *const text = g_cli_validate->last.text;
    const unsigned int declared = EMBED_CALL(report_declare, ValidateAxiomsCtx, .answers = args->answers);
    for (unsigned int answer = 0u; answer < args->answers; answer += 1u)
    {
        const char *const name = text + s_axioms[answer].name;
        const unsigned long long size = s_axioms[answer].name_size;
        unsigned int at = 0u;
        while ((at < declared) &&
               !EMBED_CALL(report_same, ValidateAxiomsCtx, .text = text + s_axioms[s_declared[at].axioms].name,
                           .size = s_axioms[s_declared[at].axioms].name_size, .other = name, .length = size))
        {
            at += 1u;
        }
        char msg_buf[16384];
        VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ULL};
        int others = 0;
        int all_gaps = 1;
        int any_gap = 0;
        const char *const list = text + s_axioms[answer].list;
        const unsigned long long list_size = s_axioms[answer].list_size;
        for (unsigned long long from = 0ULL; from <= list_size;)
        {
            const char *const comma =
                memor.chr(&(MemoriaCfg){.src = list + from, .val = ',', .bytes = list_size - from});
            const unsigned long long end = (comma != NULL) ? (unsigned long long)(comma - list) : list_size;
            unsigned long long start = from;
            unsigned long long length = end - from;
            while ((length > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                           .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = list[start]}) != NULL))
            {
                start += 1ULL;
                length -= 1ULL;
            }
            while ((length > 0ULL) &&
                   (cellul.chr(&(CatenaFinitaCfg){
                        .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = list[start + length - 1ULL]}) != NULL))
            {
                length -= 1ULL;
            }
            from = end + 1ULL;
            if ((length == 0ULL) || EMBED_CALL(cli_validate_json_internal.in_array, CliValidateJsonCtx, .json = json,
                                               .array = tool->trusted, .text = list + start, .length = length))
            {
                continue;
            }
            const int gap = EMBED_CALL(cli_validate_json_internal.in_array, CliValidateJsonCtx, .json = json,
                                       .array = tool->gaps, .text = list + start, .length = length);
            all_gaps = all_gaps && gap;
            any_gap = any_gap || gap;
            mmgr_line_put(&msg, others ? "; " : "");
            mmgr_line_put_n(&msg, list + start, length);
            for (unsigned int member = (tool->explains != CLI_JSON_NONE) ? json->node[tool->explains].first
                                                                         : CLI_JSON_NONE;
                 member != CLI_JSON_NONE; member = json->node[member].next)
            {
                if (EMBED_CALL(cli_validate_text_length_internal.contains, CliValidateTextLengthCtx,
                               .text = list + start, .length = length, .part = json->text + json->node[member].key))
                {
                    mmgr_line_put(&msg, ", ");
                    mmgr_line_put(&msg, EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                   .json_read = json, .node = member, .length_pointer = NULL));
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
            // a proof that does not check stands on a gap the prover put there: the error reports it
            failed = (found->severity == 1u) &&
                     ((cellul.eq(&(CatenaFinitaCfg){.src = found->kind, .other = "math", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = found->kind, .other = "barrier", .cap = CLI_CELLS_POOL}))) &&
                     (found->line >= line) && (found->line < ends);
        }
        if (failed && all_gaps)
        {
            continue;
        }
        mmgr_line_put(args->untrusted, (args->untrusted->at > 0ULL) ? ", " : "");
        mmgr_line_put_n(args->untrusted, name, size);
        const unsigned long long cut = EMBED_CALL(report_short, ValidateAxiomsCtx, .name = name, .size = size);
        unsigned int letters = 0u;
        for (unsigned long long byte = cut; byte < size; byte += 1ULL)
        {
            letters += (((unsigned char)name[byte] & 0xc0u) != 0x80u) ? 1u : 0u;
        }
        char message[20480];
        VerbaLinea message_line = {message, sizeof(message), 0ULL};
        mmgr_line_put_n(&message_line, name, size);
        mmgr_line_put(&message_line, " rests on ");
        mmgr_line_put_n(&message_line, msg_buf, msg.at);
        const CliFinding finding = {line, col, line, col + letters, any_gap ? 1u : 2u, "axiom", -1, 0ULL, 0ULL};
        EMBED_CALL(cli_validate_length_internal.finding_append, CliValidateLengthCtx, .finding = &finding,
                   .message_text = message, .length = message_line.at, .lifted = NULL);
    }
}
