// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_arguments.c: what the requests to an adapter are made of and read by: numbers, truths and texts read from a
// tree, the files a session takes, bytes from base64, a launch's arguments with their names filled in, the arguments
// that set stepping, and breakpoints as the protocol writes them
#include "../text/text.h"
#include "debug_internal.h"

// the bytes of a string filled in, and the lock the capacity it is filled in is guarded by
#define CLI_DEBUG_FILLED (1u << 16u)

static char s_filled[2][CLI_DEBUG_FILLED];

// the spaces a text is trimmed of
static const char s_spaces[] = " \t\n\r\f\v";

int cli_debug_sign(const CliDebugJsonNodeCtx *args)
{
    long long *value = args->value;
    const char *const text = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = args->node, .kind = CFG_JSON_NUMBER)
                                 ? args->json->text + args->json->node[args->node].text
                                 : NULL;
    const unsigned long long length = (text != NULL) ? args->json->node[args->node].length : 0ull;
    const int negative = (length > 0ull) && (text[0] == '-');
    const unsigned long long first = negative ? 1ull : 0ull;
    unsigned long long whole = 0ull;
    int read = (length > first) && (length - first <= 19ull);
    for (unsigned long long at = first; read && (at < length); at += 1ull)
    {
        read = (text[at] >= '0') && (text[at] <= '9');
        whole = (whole * 10ull) + (unsigned long long)(text[at] - '0');
    }
    read = read && (whole <= (negative ? 9223372036854775808ull : 9223372036854775807ull));
    // the least number is its own negative's bits, as two's complement writes it
    *value = read ? (negative ? (long long)(0ull - whole) : (long long)whole) : 0ll;
    return read;
}

int cli_debug_unsigned(const CliDebugJsonNodeValueU64sCtx *args)
{
    unsigned long long *value = args->value_u64s;
    const char *const text = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = args->node, .kind = CFG_JSON_NUMBER)
                                 ? args->json->text + args->json->node[args->node].text
                                 : NULL;
    const unsigned long long length = (text != NULL) ? args->json->node[args->node].length : 0ull;
    unsigned long long whole = 0ull;
    int read = (length > 0ull) && (length <= 20ull);
    for (unsigned long long at = 0ull; read && (at < length); at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        read = (text[at] >= '0') && (text[at] <= '9') && (whole <= (18446744073709551615ull - digit) / 10ull);
        whole = (whole * 10ull) + digit;
    }
    *value = read ? whole : 0ull;
    return read;
}

int cli_debug_true(const CliDebugJsonNodeCtx *args)
{
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                      .node = args->node, .kind = CFG_JSON_TRUE);
}

const char *cli_debug_text(const CliDebugJsonNodeFallbackCtx *args)
{
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->json, .node = args->node, .length_pointer = NULL);
    return (text != NULL) ? text : args->fallback;
}

typedef struct
{
    const char *text;
    const char *ending;
    unsigned long long length;
    const char *const *names;
    const char *const *withs;
    unsigned int count;
    unsigned long long *size;
    unsigned long long *value;
    CliJson *to;
    const CliJson *json;
    unsigned int from;
    int numbered;
    unsigned long long *length_pointer;
    const char *one;
} DebugArgumentsCtx;
// whether `text` ends with `ending`, case aside
EMBED_INLINE int arguments_ends_with(const DebugArgumentsCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->ending, .cap = CLI_CELLS_POOL});
    for (size_t at = 0u; (size <= length) && (at < size); at += 1u)
    {
        char one = args->text[length - size + at];
        one = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
        if (one != args->ending[at])
        {
            return 0;
        }
    }
    return size <= length;
}

int cli_debug_allocs(const CliDebugFileCtx *args)
{
    const int python = EMBED_CALL(arguments_ends_with, DebugArgumentsCtx, .text = args->file, .ending = ".py") ||
                       EMBED_CALL(arguments_ends_with, DebugArgumentsCtx, .text = args->file, .ending = ".pyw");
    return (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_CELLS_POOL})) == python;
}

unsigned long long cli_debug_base64(const CliDebugTextCtx *args)
{
    unsigned long long count = 0ull;
    unsigned int accum = 0u;
    unsigned int bits = 0u;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        const char one = args->text[at];
        const int value = ((one >= 'A') && (one <= 'Z'))   ? (one - 'A')
                          : ((one >= 'a') && (one <= 'z')) ? (one - 'a' + 26)
                          : ((one >= '0') && (one <= '9')) ? (one - '0' + 52)
                          : ((one == '+') || (one == '-')) ? 62
                          : ((one == '/') || (one == '_')) ? 63
                                                           : -1;
        if (value < 0)
        {
            continue;
        }
        accum = (accum << 6u) | (unsigned int)value;
        bits += 6u;
        if (bits >= 8u)
        {
            bits -= 8u;
            if (count < args->capacity)
            {
                args->out_bytes[count] = (unsigned char)((accum >> bits) & 0xffu);
                count += 1ull;
            }
            accum &= (1u << bits) - 1u;
        }
    }
    return count;
}

// a string filled in: each "{name}" of `names` replaced by its `withs`, one name after another, into the filled
// capacity
EMBED_INLINE const char *arguments_string_fill(const DebugArgumentsCtx *args)
{
    unsigned long long *size = args->size;
    VerbaLinea first = {s_filled[0], sizeof(s_filled[0]), 0ull};
    mmgr_line_put_n(&first, args->text, args->length);
    unsigned int from = 0u;
    unsigned long long at_end = first.at;
    for (unsigned int one = 0u; one < args->count; one += 1u)
    {
        char mark[CLI_NAME_MAX_LEN];
        VerbaLinea marked = {mark, sizeof(mark), 0ull};
        mmgr_line_ch(&marked, '{');
        mmgr_line_put(&marked, args->names[one]);
        mmgr_line_ch(&marked, '}');
        const unsigned long long mark_size = marked.at;
        VerbaLinea next = {s_filled[1u - from], sizeof(s_filled[0]), 0ull};
        const char *const was = s_filled[from];
        for (unsigned long long at = 0ull; at < at_end;)
        {
            if ((at + mark_size <= at_end) &&
                (memor.cmp(&(MemoriaCfg){.src = was + at, .other = mark, .bytes = mark_size}) == 0))
            {
                mmgr_line_put(&next, args->withs[one]);
                at += mark_size;
            }
            else
            {
                mmgr_line_ch(&next, was[at]);
                at += 1ull;
            }
        }
        at_end = next.at;
        from = 1u - from;
    }
    *size = at_end;
    return s_filled[from];
}

// whether a string is digits alone, its number into `value`: a number past 64 bits stays a string
EMBED_INLINE int arguments_digits(const DebugArgumentsCtx *args)
{
    unsigned long long *value = args->value;
    unsigned long long whole = 0ull;
    int digits = (args->length > 0ull);
    for (unsigned long long at = 0ull; digits && (at < args->length); at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(args->text[at] - '0');
        digits =
            (args->text[at] >= '0') && (args->text[at] <= '9') && (whole <= (18446744073709551615ull - digit) / 10ull);
        whole = (whole * 10ull) + digit;
    }
    *value = whole;
    return digits;
}

static unsigned int arguments_fill(const DebugArgumentsCtx *args)
{
    CliJson *to = args->to;
    const CliJson *json = args->json;
    unsigned int from = args->from;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = from,
                   .kind = CFG_JSON_STRING))
    {
        unsigned long long length = 0ull;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = from, .length_pointer = &length);
        unsigned long long size = 0ull;
        const char *const filled =
            EMBED_CALL(arguments_string_fill, DebugArgumentsCtx, .text = text, .length = length, .names = args->names,
                       .withs = args->withs, .count = args->count, .size = &size);
        unsigned long long value = 0ull;
        if (args->numbered &&
            EMBED_CALL(arguments_digits, DebugArgumentsCtx, .text = filled, .length = size, .value = &value))
        {
            char number[32];
            VerbaLinea written = {number, sizeof(number), 0ull};
            mmgr_line_decimal(&written, value, 1u);
            mmgr_line_finish(&written);
            return EMBED_CALL(cli_json_tree_text_length_kind.json_number_init, CliJsonTreeTextLengthKindCfg, .json = to,
                              .text = number);
        }
        return EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = to,
                          .text = filled, .length = size);
    }
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = from,
                   .kind = CFG_JSON_ARRAY))
    {
        const unsigned int array =
            EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = to, .kind = CFG_JSON_ARRAY);
        for (unsigned int item = json->node[from].first; item != CLI_JSON_NONE; item = json->node[item].next)
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = to, .array = array,
                       .value = EMBED_CALL(arguments_fill, DebugArgumentsCtx, .to = to, .json = json, .from = item,
                                           .names = args->names, .withs = args->withs, .count = args->count,
                                           .numbered = args->numbered));
        }
        return array;
    }
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = from,
                   .kind = CFG_JSON_OBJECT))
    {
        const unsigned int object =
            EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = to, .kind = CFG_JSON_OBJECT);
        for (unsigned int member = json->node[from].first; member != CLI_JSON_NONE; member = json->node[member].next)
        {
            const unsigned int value = EMBED_CALL(arguments_fill, DebugArgumentsCtx, .to = to, .json = json,
                                                  .from = member, .names = args->names, .withs = args->withs,
                                                  .count = args->count, .numbered = args->numbered);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = to, .object = object,
                       .name = json->text + json->node[member].key, .value = value);
        }
        return object;
    }
    return (from == CLI_JSON_NONE)
               ? EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = to, .kind = CFG_JSON_NULL)
               : EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .to = to, .from = json,
                            .node = from);
}

unsigned int cli_debug_fill(const CliDebugJsonCtx *args)
{
    const unsigned int filled =
        EMBED_CALL(arguments_fill, DebugArgumentsCtx, .to = args->to, .json = args->json, .from = args->from,
                   .names = args->names, .withs = args->withs, .count = args->count, .numbered = args->numbered);
    return filled;
}

// a text trimmed of spaces at both ends: its start, its length into `length`
EMBED_INLINE const char *arguments_trim(const DebugArgumentsCtx *args)
{
    unsigned long long *length = args->length_pointer;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    unsigned long long start = 0ull;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = s_spaces, .cap = CLI_CELLS_POOL, .byte = args->text[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_spaces, .cap = CLI_CELLS_POOL, .byte = args->text[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    *length = end - start;
    return args->text + start;
}

// whether a pattern names a path, as one with a slash, a star or a dot that is not a module's dotted name does
EMBED_INLINE int arguments_names_path(const DebugArgumentsCtx *args)
{
    int marked = 0;
    int dotted = 1;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->one, .cap = CLI_CELLS_POOL});
    marked = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->one, .at = 0ull,
                        .size = size, .set = "/\\*.") < size;
    dotted = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = args->one, .at = 0ull,
                        .size = size, .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS | CLI_TEXT_END_ASCII,
                        .set = "_.") == size;
    return marked && !dotted;
}

void cli_debug_step_arguments(const CliDebugJsonLanguageCtx *args)
{
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->to,
                    .node = args->arguments, .kind = CFG_JSON_OBJECT))
    {
        return;
    }
    const unsigned int skip = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->stepping, .name = "skip");
    const unsigned int ours_index = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = args->json, .object = args->stepping, .name = "mine_only");
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_CELLS_POOL}))
    {
        const int ours_only =
            (ours_index == CLI_JSON_NONE) || EMBED_CALL(cli_debug_json_node_internal.debug_true, CliDebugJsonNodeCtx,
                                                        .json = args->json, .node = ours_index);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->to,
                   .object = args->arguments, .name = "justMyCode",
                   .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->to,
                                       .kind = ours_only ? CFG_JSON_TRUE : CFG_JSON_FALSE));
        const unsigned int rules =
            EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->to, .kind = CFG_JSON_ARRAY);
        unsigned int count = 0u;
        for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = args->json, .array = skip, .index = at) != CLI_JSON_NONE;
             at += 1u)
        {
            const char *const one =
                EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = args->json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = args->json, .array = skip, .index = at),
                           .fallback = "");
            unsigned long long length = 0ull;
            const char *const trimmed =
                EMBED_CALL(arguments_trim, DebugArgumentsCtx, .text = one, .length_pointer = &length);
            if (length == 0ull)
            {
                continue;
            }
            const unsigned int rule =
                EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->to, .kind = CFG_JSON_OBJECT);
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->to, .object = rule,
                       .name = EMBED_CALL(arguments_names_path, DebugArgumentsCtx, .one = one) ? "path" : "module",
                       .value =
                           EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                      .json = args->to, .text = trimmed, .length = length));
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->to, .object = rule,
                       .name = "include",
                       .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->to,
                                           .kind = CFG_JSON_FALSE));
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->to, .array = rules,
                       .value = rule);
            count += 1u;
        }
        if (count > 0u)
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->to,
                       .object = args->arguments, .name = "rules", .value = rules);
        }
        return;
    }
    char patterns[CLI_DEBUG_FILLED];
    VerbaLinea joined = {patterns, sizeof(patterns), 0ull};
    mmgr_line_put(&joined, "settings set target.process.thread.step-avoid-regexp ");
    unsigned int count = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = args->json, .array = skip, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const trimmed =
            EMBED_CALL(arguments_trim, DebugArgumentsCtx,
                       .text = EMBED_CALL(
                           cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = args->json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = args->json, .array = skip, .index = at),
                           .fallback = ""),
                       .length_pointer = &length);
        if (length > 0ull)
        {
            mmgr_line_put(&joined, (count > 0u) ? "|" : "");
            mmgr_line_put_n(&joined, trimmed, length);
            count += 1u;
        }
    }
    if (count == 0u)
    {
        return;
    }
    const unsigned int had = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->to,
                                        .object = args->arguments, .name = "initCommands");
    const unsigned int commands =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->to, .node = had,
                   .kind = CFG_JSON_ARRAY)
            ? EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = args->to, .node = had)
            : EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->to, .kind = CFG_JSON_ARRAY);
    EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->to, .array = commands,
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = args->to, .text = joined.out, .length = joined.at));
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = args->to,
               .object = args->arguments, .name = "initCommands", .value = commands);
}

void cli_debug_breakpoints_write(const CliDebugSessionCtx *args)
{
    static const char *const s_keys[3] = {"condition", "hitCondition", "logMessage"};
    static const char *const s_fields[3] = {"condition", "hits", "log"};
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u;
         EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = args->json,
                    .array = args->breakpoints, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = args->json, .array = args->breakpoints, .index = at);
        unsigned long long line = 0ull;
        (void)EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx,
                         .json = args->json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                            .json_read = args->json, .object = one, .name = "line"),
                         .value_u64s = &line);
        mmgr_line_put(args->out, (at > 0u) ? ",{\"line\":" : "{\"line\":");
        mmgr_line_decimal(args->out, (line & 0xffffffffull) + 1ull, 1u);
        for (unsigned int field = 0u; field < 3u; field += 1u)
        {
            unsigned long long length = 0ull;
            const char *const trimmed = EMBED_CALL(
                arguments_trim, DebugArgumentsCtx,
                .text = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                   .json = args->json,
                                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                      .json_read = args->json, .object = one, .name = s_fields[field]),
                                   .fallback = ""),
                .length_pointer = &length);
            if (length > 0ull)
            {
                mmgr_line_put(args->out, ",\"");
                mmgr_line_put(args->out, s_keys[field]);
                mmgr_line_put(args->out, "\":");
                EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                           .text_bytes = (const unsigned char *)trimmed, .length = length);
            }
        }
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
}
