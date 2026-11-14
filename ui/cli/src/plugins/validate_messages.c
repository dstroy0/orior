// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_messages.c: the prover's messages as `lean --json` writes them, the barrier a message shows stopped the
// check, the axioms a theorem rests on, and the copy of a file that is checked
#include "../text/text.h"
#include "validate_internal.h"

// the max bytes of the lines of a check's output that are no message, recorded as one error
#define CLI_VALIDATE_LOOSE (1u << 20u)

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long *at;
    unsigned long long *start;
    unsigned long long *length_pointer;
    const CliJson *json;
    unsigned int object;
    const char *key;
    const char *part;
    unsigned long long *value;
    CliProverMessages *messages;
    unsigned long long length;
    CliProverMessage *message;
    unsigned long long text_size;
    const char *type;
    unsigned long long type_size;
    unsigned int array;
    const char *line;
    unsigned long long number;
    const char *const *names;
    const unsigned long long *values;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *changed;
} ValidateMessagesCtx;
// the next line of `size` bytes from `at`, split at "\n", a "\r" before it taken off: 1 with its start and size, or 0
// where none is left
EMBED_INLINE int messages_line(const ValidateMessagesCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long *at = args->at;
    unsigned long long *start = args->start;
    unsigned long long *length = args->length_pointer;
    if (*at >= size)
    {
        return 0;
    }
    *start = *at;
    const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = *at, .size = size);
    unsigned long long end = bounds.stop;
    *at = bounds.next;
    end -= bounds.stop - bounds.cut;
    *length = end - *start;
    return 1;
}

// `length` bytes from `start` trimmed of ASCII whitespace, as their start and length
EMBED_INLINE void messages_trim(const ValidateMessagesCtx *args)
{
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->text + *args->start,
               .size = *args->length_pointer, .from_u64s = &from, .to_u64s = &to);
    *args->start += from;
    *args->length_pointer = to - from;
}

// the whole number `part` of the member `key` of `object`, as serde reads a u64: 1, or 0 where there is none
EMBED_INLINE int messages_position(const ValidateMessagesCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    unsigned long long *value = args->value;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                                .json_read = json, .object = object, .name = args->key),
                                           .name = args->part);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = member,
                    .type = CFG_JSON_NUMBER))
    {
        return 0;
    }
    const char *const text = json->text + json->node[member].text;
    const unsigned long long length = json->node[member].length;
    unsigned long long sum = 0ULL;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        if ((text[at] < '0') || (text[at] > '9') || (sum > (~0ULL - digit) / 10ULL))
        {
            return 0;
        }
        sum = (sum * 10ULL) + digit;
    }
    *value = sum;
    return length > 0ULL;
}

// a text put in the messages' texts: its place, or CLI_VALIDATE_NONE where it does not fit
EMBED_INLINE unsigned long long messages_write(const ValidateMessagesCtx *args)
{
    if (args->messages->used + args->length > CLI_VALIDATE_MESSAGE_TEXT)
    {
        return CLI_VALIDATE_NONE;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = args->messages->text + args->messages->used, .src = args->text, .bytes = args->length});
    args->messages->used += args->length;
    return args->messages->used - args->length;
}

// a message put with its text and type, where it and they fit
EMBED_INLINE void messages_append(const ValidateMessagesCtx *args)
{
    const char *text = args->text;
    unsigned long long text_size = args->text_size;
    const char *type = args->type;
    unsigned long long type_size = args->type_size;
    args->message->text =
        EMBED_CALL(messages_write, ValidateMessagesCtx, .messages = args->messages, .text = text, .length = text_size);
    args->message->text_size = text_size;
    args->message->type =
        EMBED_CALL(messages_write, ValidateMessagesCtx, .messages = args->messages, .text = type, .length = type_size);
    args->message->type_size = type_size;
    if ((args->messages->count < CLI_VALIDATE_MESSAGES) && (args->message->text != CLI_VALIDATE_NONE) &&
        (args->message->type != CLI_VALIDATE_NONE))
    {
        args->messages->message[args->messages->count] = *args->message;
        args->messages->count += 1u;
    }
}

void cli_validate_messages_get(const CliValidateSizeCtx *args)
{
    static char s_loose[CLI_VALIDATE_LOOSE];
    VerbaLinea loose = {s_loose, sizeof(s_loose), 0ULL};
    args->messages->count = 0u;
    args->messages->used = 0ULL;
    CliJson json;
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    while (EMBED_CALL(messages_line, ValidateMessagesCtx, .text = args->output, .size = args->size, .at = &at,
                      .start = &start, .length_pointer = &length))
    {
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
                   .node_json_node_pointer = g_cli_validate->line_nodes, .nodes_max = CLI_VALIDATE_LINE_NODES,
                   .text_out = g_cli_validate->line_text, .text_max = CLI_VALIDATE_LINE_TEXT);
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unread = {why, sizeof(why), 0ULL};
        const unsigned int value =
            EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = &json,
                       .text = args->output + start, .size = length, .tokens = g_cli_validate->line_tokens,
                       .tokens_max = CLI_VALIDATE_LINE_NODES, .err_msg = &unread);
        if (value == CLI_JSON_NONE)
        {
            EMBED_CALL(messages_trim, ValidateMessagesCtx, .text = args->output, .start = &start,
                       .length_pointer = &length);
            if (length > 0ULL)
            {
                mmgr_line_put(&loose, (loose.at > 0ULL) ? "\n" : "");
                mmgr_line_put_n(&loose, args->output + start, length);
            }
            continue;
        }
        unsigned long long line = 1ULL;
        unsigned long long col = 0ULL;
        unsigned long long end_line = 0ULL;
        unsigned long long end_col = 0ULL;
        (void)EMBED_CALL(messages_position, ValidateMessagesCtx, .json = &json, .object = value, .key = "pos",
                         .part = "line", .value = &line);
        (void)EMBED_CALL(messages_position, ValidateMessagesCtx, .json = &json, .object = value, .key = "pos",
                         .part = "column", .value = &col);
        // lines from 1 are read as lines from 0, a 32-bit number's wrapping as Rust's has it
        CliProverMessage message;
        message.line = (unsigned int)((line > 1ULL) ? line : 1ULL) - 1u;
        message.col = (unsigned int)col;
        if (EMBED_CALL(messages_position, ValidateMessagesCtx, .json = &json, .object = value, .key = "endPos",
                       .part = "line", .value = &end_line))
        {
            message.end_line = (unsigned int)((end_line > 1ULL) ? end_line : 1ULL) - 1u;
        }
        else
        {
            message.end_line = message.line;
        }
        message.end_col = EMBED_CALL(messages_position, ValidateMessagesCtx, .json = &json, .object = value,
                                     .key = "endPos", .part = "column", .value = &end_col)
                              ? (unsigned int)end_col
                              : message.col + 1u;
        const char *const severity =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = value, .name = "severity"),
                       .length_pointer = NULL);
        if (severity == NULL)
        {
            message.severity = 3u;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = severity, .other = "error", .cap = CLI_POOL_BYTES}))
        {
            message.severity = 1u;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = severity, .other = "warning", .cap = CLI_POOL_BYTES}))
        {
            message.severity = 2u;
        }
        else
        {
            message.severity = 3u;
        }
        unsigned long long text_size = 0ULL;
        unsigned long long type_size = 0ULL;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = value, .name = "data"),
                       .length_pointer = &text_size);
        const char *const type =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = value, .name = "type"),
                       .length_pointer = &type_size);
        EMBED_CALL(messages_append, ValidateMessagesCtx, .messages = args->messages, .message = &message,
                   .text = (text != NULL) ? text : "", .text_size = text_size, .type = (type != NULL) ? type : "",
                   .type_size = type_size);
    }
    if (loose.at > 0ULL)
    {
        CliProverMessage message = {1u, 0u, 0u, 0u, 1u, 0ULL, 0ULL, 0ULL, 0ULL};
        EMBED_CALL(messages_append, ValidateMessagesCtx, .messages = args->messages, .message = &message,
                   .text = s_loose, .text_size = loose.at, .type = "", .type_size = 0ULL);
    }
}

// whether the JSON array `array` contains a string `text` contains
EMBED_INLINE int messages_any_in(const ValidateMessagesCtx *args)
{
    const CliJson *json = args->json;
    unsigned int array = args->array;
    const char *text = args->text;
    unsigned long long length = args->length;
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = array, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const char *const part =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = array, .index = index),
                       .length_pointer = NULL);
        if ((part != NULL) && EMBED_CALL(cli_validate_text_length_internal.contains, CliValidateTextLengthCtx,
                                         .text = text, .length = length, .part = part))
        {
            return 1;
        }
    }
    return 0;
}

int cli_validate_barrier_get(const CliValidateMessagesCtx *args)
{
    const CliJson *const json = &g_cli_validate->json;
    const GuiTool *const tool = &g_cli_validate->tool;
    const char *const text = args->messages_read->text + args->message->text;
    for (unsigned int at = 0u; at < tool->barrier_count; at += 1u)
    {
        const CliBarrier *const barrier = &tool->barriers[at];
        const int stopped =
            EMBED_CALL(cli_validate_json_internal.in_array, CliValidateJsonCtx, .json = json, .array = barrier->types,
                       .text = args->messages_read->text + args->message->type, .length = args->message->type_size) ||
            EMBED_CALL(messages_any_in, ValidateMessagesCtx, .json = json, .array = barrier->phrases, .text = text,
                       .length = args->message->text_size);
        if (stopped && !EMBED_CALL(messages_any_in, ValidateMessagesCtx, .json = json, .array = barrier->unless,
                                   .text = text, .length = args->message->text_size))
        {
            return (int)at;
        }
    }
    return -1;
}

int cli_validate_axioms_get(const CliValidateTextCtx *args)
{
    unsigned long long *name = args->name;
    unsigned long long *name_size = args->name_size;
    unsigned long long *list = args->list;
    unsigned long long *list_size = args->list_size;
    if ((args->size == 0ULL) || (args->text[0] != '\''))
    {
        return 0;
    }
    const char *const closed = memor.chr(&(MemoriaCfg){.src = args->text + 1, .val = '\'', .bytes = args->size - 1ULL});
    *name = 1ULL;
    *name_size = ((closed != NULL) ? (unsigned long long)(closed - args->text) : args->size) - 1ULL;
    *list = 0ULL;
    *list_size = 0ULL;
    if (EMBED_CALL(cli_validate_text_length_internal.contains, CliValidateTextLengthCtx, .text = args->text,
                   .length = args->size, .part = "does not depend on any axioms"))
    {
        return 1;
    }
    static const char s_mark[] = "depends on axioms:";
    unsigned long long at = 0ULL;
    while ((at + sizeof(s_mark) - 1ULL <= args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text + at, .other = s_mark, .bytes = sizeof(s_mark) - 1ULL}) != 0))
    {
        at += 1ULL;
    }
    if (at + sizeof(s_mark) - 1ULL > args->size)
    {
        return 0;
    }
    unsigned long long start = at + sizeof(s_mark) - 1ULL;
    unsigned long long length = args->size - start;
    EMBED_CALL(messages_trim, ValidateMessagesCtx, .text = args->text, .start = &start, .length_pointer = &length);
    const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                               .text = args->text, .at = start, .size = start + length, .set = "[") -
                                    start;
    start += lead;
    length -= lead;
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->text + start,
                        .at = length, .set = "]");
    *list = start;
    *list_size = length;
    return 1;
}

// one line of the copy: each option it sets that is grown set to its grown value where that is larger, each change
// reported, written into `out` with "\n" after it
EMBED_INLINE void copy_line(const ValidateMessagesCtx *args)
{
    char line_buf[8192];
    VerbaLinea work_line = {line_buf, sizeof(line_buf), 0ULL};
    mmgr_line_put_n(&work_line, args->line, args->length);
    if (!mmgr_line_ok(&work_line))
    {
        mmgr_line_put_n(args->out, args->line, args->length);
        mmgr_line_ch(args->out, '\n');
        return;
    }
    for (unsigned int option = 0u; option < args->count; option += 1u)
    {
        char set[512];
        VerbaLinea set_line = {set, sizeof(set), 0ULL};
        mmgr_line_put(&set_line, "set_option ");
        mmgr_line_put(&set_line, args->names[option]);
        mmgr_line_ch(&set_line, ' ');
        const unsigned long long set_size = set_line.at;
        unsigned long long at = 0ULL;
        while ((at + set_size <= work_line.at) &&
               (memor.cmp(&(MemoriaCfg){.src = line_buf + at, .other = set, .bytes = set_size}) != 0))
        {
            at += 1ULL;
        }
        if (at + set_size > work_line.at)
        {
            continue;
        }
        const unsigned long long digits_at = at + set_size;
        unsigned long long digits = 0ULL;
        unsigned long long own = 0ULL;
        int fits = 1;
        unsigned long long end = digits_at;
        own = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = line_buf, .at = digits_at,
                         .size = work_line.at, .end = &end);
        digits = end - digits_at;
        fits = digits <= 19ULL;
        if ((digits == 0ULL) || !fits || (own == 0ULL) || (own >= args->values[option]))
        {
            continue;
        }
        char value[24];
        VerbaLinea value_line = {value, sizeof(value), 0ULL};
        mmgr_line_decimal(&value_line, args->values[option], 1u);
        if (work_line.at - digits + value_line.at > sizeof(line_buf))
        {
            continue;
        }
        ((value_line.at > digits) ? memor.move_up
                                  : memor.move_down)(&(MemoriaCfg){.dst = line_buf + digits_at + value_line.at,
                                                                   .src = line_buf + digits_at + digits,
                                                                   .bytes = work_line.at - digits_at - digits});
        memor.cpy(&(MemoriaCfg){.dst = line_buf + digits_at, .src = value, .bytes = value_line.at});
        work_line.at = work_line.at - digits + value_line.at;
        mmgr_line_put(args->changed, (args->changed->at > 0ULL) ? "; line " : "line ");
        mmgr_line_decimal(args->changed, args->number, 1u);
        mmgr_line_put(args->changed, ": set_option ");
        mmgr_line_put(args->changed, args->names[option]);
        mmgr_line_ch(args->changed, ' ');
        mmgr_line_decimal(args->changed, own, 1u);
        mmgr_line_put(args->changed, ", checked at ");
        mmgr_line_put_n(args->changed, value, value_line.at);
    }
    mmgr_line_put_n(args->out, line_buf, work_line.at);
    mmgr_line_ch(args->out, '\n');
}

unsigned long long cli_validate_copy_get(const CliValidateTextSizeCtx *args)
{
    VerbaLinea out = {g_cli_validate->copy, CLI_VALIDATE_COPY, 0ULL};
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    unsigned long long number = 0ULL;
    while (EMBED_CALL(messages_line, ValidateMessagesCtx, .text = args->text, .size = args->size, .at = &at,
                      .start = &start, .length_pointer = &length))
    {
        number += 1ULL;
        EMBED_CALL(copy_line, ValidateMessagesCtx, .line = args->text + start, .length = length, .number = number,
                   .names = args->names, .values = args->values, .count = args->count, .out = &out,
                   .changed = args->changed);
    }
    if (number == 0ULL)
    {
        mmgr_line_ch(&out, '\n');
    }
    const GuiTool *const tool = &g_cli_validate->tool;
    for (unsigned int theorem = 0u; tool->audited && (theorem < g_cli_validate->theorem_count); theorem += 1u)
    {
        const char *const name = g_cli_validate->names + g_cli_validate->theorems[theorem];
        EMBED_CALL(cli_text_out_name.append_fill, CliTextOutNameCfg, .out = &out, .text = tool->print, .name = "{name}",
                   .value = name);
        mmgr_line_ch(&out, '\n');
    }
    return mmgr_line_ok(&out) ? out.at : CLI_VALIDATE_COPY + 1ULL;
}
