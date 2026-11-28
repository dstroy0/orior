// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_axioms.c: the last check's messages read: the mathematics it found wrong, and each axiom a theorem rests
// on past those trusted, found by the theorem's declaration in the code
#include "../text/text.h"
#include "validate_internal.h"

// A theorem's declaration found in the code: its axiom answer, and its line and column. The declarations are a cell
// of the report's call sized to the answers
typedef struct
{
    unsigned int axioms;
    unsigned int line;
    unsigned int col;
} CliDeclared;

unsigned int cli_validate_mathematics(const CliValidateAnswersCtx *args)
{
    const CliProverMessages *const last = &g_cli_validate->last;
    unsigned int answers = 0u;
    for (unsigned int at = 0u; at < last->count; at += 1u)
    {
        const CliProverMessage *const message = &last->message[at];
        if (message->line >= g_cli_validate->lines)
        {
            // an answer a message: the caller's cell holds one for each message
            CliAxioms *const axioms = &args->axioms[answers];
            if (EMBED_CALL(cli_validate_text_internal.axioms_get, CliValidateTextCtx,
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
    unsigned int declared;
    unsigned int answer;
    VerbaLinea *out;
    int *all_gaps;
    int *any_gap;
    unsigned int line;
    unsigned int ends;
    const CliAxioms *axioms;
    CliDeclared *list;
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

// each theorem's declaration found in the code into `list`, in the order of their lines, and the lines it runs to, up
// to the next theorem's: their count, one at most for each of the `answers` in `axioms`
EMBED_INLINE unsigned int report_declare(const ValidateAxiomsCtx *args)
{
    const CliJson *const json = &g_cli_validate->json;
    const char *const text = g_cli_validate->last.text;
    const char *const code = g_cli_validate->code;
    unsigned int count = 0u;
    for (unsigned int answer = 0u; answer < args->answers; answer += 1u)
    {
        const char *const name = text + args->axioms[answer].name;
        const unsigned long long size = args->axioms[answer].name_size;
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
            while ((place > 0u) && (args->list[place - 1u].line > next->line))
            {
                args->list[place] = args->list[place - 1u];
                place -= 1u;
            }
            args->list[place] = (CliDeclared){answer, next->line, next->col};
            count += 1u;
            break;
        }
    }
    return count;
}

// the place among the `declared` declarations of `list` of the axiom named by `name`, `size` bytes of the last
// answer's text: `declared` where none names it
EMBED_INLINE unsigned int report_declared_at(const ValidateAxiomsCtx *args)
{
    const char *const text = g_cli_validate->last.text;
    unsigned int at = 0u;
    while ((at < args->declared) &&
           !EMBED_CALL(report_same, ValidateAxiomsCtx, .text = text + args->axioms[args->list[at].axioms].name,
                       .size = args->axioms[args->list[at].axioms].name_size, .other = args->name,
                       .length = args->size))
    {
        at += 1u;
    }
    return at;
}

// the note the tool's `explains` gives the axiom `text`, `length` bytes, the first whose key it contains, put after
// `, ` in `out`; nothing where none does
EMBED_INLINE void report_explained(const ValidateAxiomsCtx *args)
{
    const CliJson *const json = &g_cli_validate->json;
    const GuiTool *const tool = &g_cli_validate->tool;
    for (unsigned int member = (tool->explains != CLI_JSON_NONE) ? json->node[tool->explains].first : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = json->node[member].next)
    {
        if (EMBED_CALL(cli_validate_text_length_internal.contains, CliValidateTextLengthCtx, .text = args->text,
                       .length = args->length, .part = json->text + json->node[member].key))
        {
            mmgr_line_put(args->out, ", ");
            mmgr_line_put(args->out, EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = json, .node = member, .length_pointer = NULL));
            return;
        }
    }
}

// the axioms the answer `answer` rests on that the tool does not trust written into `out`, `; ` between them, each with
// its note, and whether they are all gaps the tool names and whether any is into `all_gaps` and `any_gap`: whether any
// is written
EMBED_INLINE int report_axioms_listed(const ValidateAxiomsCtx *args)
{
    const CliJson *const json = &g_cli_validate->json;
    const GuiTool *const tool = &g_cli_validate->tool;
    const char *const list = g_cli_validate->last.text + args->axioms[args->answer].list;
    const unsigned long long list_size = args->axioms[args->answer].list_size;
    int others = 0;
    for (unsigned long long from = 0ULL; from <= list_size;)
    {
        const char *const comma = memor.chr(&(MemoriaCfg){.src = list + from, .val = ',', .bytes = list_size - from});
        const unsigned long long end = (comma != NULL) ? (unsigned long long)(comma - list) : list_size;
        unsigned long long start = from;
        unsigned long long length = end - from;
        while ((length > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                       .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = list[start]}) != NULL))
        {
            start += 1ULL;
            length -= 1ULL;
        }
        while ((length > 0ULL) &&
               (cellul.chr(&(CatenaFinitaCfg){
                    .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = list[start + length - 1ULL]}) != NULL))
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
        *args->all_gaps = *args->all_gaps && gap;
        *args->any_gap = *args->any_gap || gap;
        mmgr_line_put(args->out, others ? "; " : "");
        mmgr_line_put_n(args->out, list + start, length);
        EMBED_CALL(report_explained, ValidateAxiomsCtx, .text = list + start, .length = length, .out = args->out);
        others = 1;
    }
    return others;
}

// whether a proof failed between the line `line` and before `ends`: a proof that does not check is on a gap the
// prover put there, which the error reports
EMBED_INLINE int report_failed(const ValidateAxiomsCtx *args)
{
    for (unsigned int finding = 0u; finding < g_cli_validate->finding_count; finding += 1u)
    {
        const CliFinding *const found = &g_cli_validate->findings[finding];
        if ((found->severity == 1u) &&
            ((cellul.eq(&(CatenaFinitaCfg){.src = found->type, .other = "math", .cap = CLI_POOL_BYTES})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = found->type, .other = "barrier", .cap = CLI_POOL_BYTES}))) &&
            (found->line >= args->line) && (found->line < args->ends))
        {
            return 1;
        }
    }
    return 0;
}

void cli_validate_axioms_report(const CliValidateAnswersCtx *args)
{
    const char *const text = g_cli_validate->last.text;
    if (args->answers == 0u)
    {
        return;
    }
    // the declarations in a cell of the call, one at most for each answer
    CliDeclared *const list = g_platform_guard->temporary_buf_alloc(
        (unsigned long long)args->answers * sizeof(CliDeclared), g_cli_plugins_owner);
    if (list == NULL)
    {
        return;
    }
    const unsigned int declared =
        EMBED_CALL(report_declare, ValidateAxiomsCtx, .answers = args->answers, .axioms = args->axioms, .list = list);
    for (unsigned int answer = 0u; answer < args->answers; answer += 1u)
    {
        const char *const name = text + args->axioms[answer].name;
        const unsigned long long size = args->axioms[answer].name_size;
        const unsigned int at = EMBED_CALL(report_declared_at, ValidateAxiomsCtx, .declared = declared, .name = name,
                                           .size = size, .axioms = args->axioms, .list = list);
        char msg_buf[16384];
        VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ULL};
        int all_gaps = 1;
        int any_gap = 0;
        const int others = EMBED_CALL(report_axioms_listed, ValidateAxiomsCtx, .answer = answer, .out = &msg,
                                      .all_gaps = &all_gaps, .any_gap = &any_gap, .axioms = args->axioms);
        if (!others || (at == declared))
        {
            continue;
        }
        const unsigned int line = list[at].line;
        const unsigned int col = list[at].col;
        const unsigned int ends = (at + 1u < declared) ? list[at + 1u].line : 0xffffffffu;
        if (all_gaps && EMBED_CALL(report_failed, ValidateAxiomsCtx, .line = line, .ends = ends))
        {
            continue;
        }
        mmgr_line_put(args->untrusted, (args->untrusted->at > 0ULL) ? ", " : "");
        mmgr_line_put_n(args->untrusted, name, size);
        const unsigned long long cut = EMBED_CALL(report_short, ValidateAxiomsCtx, .name = name, .size = size);
        unsigned int characters = 0u;
        for (unsigned long long byte = cut; byte < size; byte += 1ULL)
        {
            characters += (((unsigned char)name[byte] & 0xc0u) != 0x80u) ? 1u : 0u;
        }
        char message[20480];
        VerbaLinea message_line = {message, sizeof(message), 0ULL};
        mmgr_line_put_n(&message_line, name, size);
        mmgr_line_put(&message_line, " rests on ");
        mmgr_line_put_n(&message_line, msg_buf, msg.at);
        const CliFinding finding = {line, col, line, col + characters, any_gap ? 1u : 2u, "axiom", -1, 0ULL, 0ULL};
        EMBED_CALL(cli_validate_length_internal.finding_append, CliValidateLengthCtx, .finding = &finding,
                   .message_text = message, .length = message_line.at, .lifted = NULL);
    }
    g_platform_guard->persistent_buf_release(list, g_cli_plugins_owner);
}
