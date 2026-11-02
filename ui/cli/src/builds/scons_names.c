// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_names.c: what an SConstruct or SConscript declares about names and of other files: the names its `Import` calls
// take, each word of their strings; the names it exports, by `Export`'s strings, keywords and bare names and by the
// strings of an `exports` given an `SConscript` call; and the SConscripts its `SConscript` calls read, by the strings
// of their first argument or by their `dirs`, each joined to their `name`, `SConscript` where none is given
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "scons_internal.h"

// The hand an import is handed to: the function and its context
typedef struct
{
    CliSconsSpan each;
    void *context;
} CliSconsHand;

// each word of a string handed to the hand
static void names_word_dispatch(const CliSconsSpanCfg *args)
{
    const CliSconsHand *const hand = args->context;
    const char *const text = g_cli_scons.text;
    unsigned long long walk = 0ull;
    unsigned long long word = 0ull;
    unsigned long long length = 0ull;
    while (EMBED_CALL(cli_text_size_length.word, CliTextSizeLengthCfg, .text = text + args->from,
                      .size = args->to - args->from, .at_pointer = &walk, .word = &word, .length = &length))
    {
        EMBED_CALL(hand->each, CliSconsSpanCfg, .context = hand->context, .from = args->from + word,
                   .to = args->from + word + length);
    }
}

// the strings of an `Import` call's arguments handed on, word by word
static void names_import_call(const CliSconsSpanCfg *args)
{
    void *context = args->context;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    EMBED_CALL(cli_scons_each_internal.strings, CliSconsEachCtx, .from = from, .to = to, .each = names_word_dispatch,
               .context = context);
}

void cli_scons_import(const CliSconsNameCtx *args)
{
    CliSconsHand hand = {args->each, args->context};
    EMBED_CALL(cli_scons_name_internal.calls, CliSconsNameCtx, .name = "Import", .each = names_import_call,
               .context = &hand);
}

// The hand an exported name is handed to: the function and its context
typedef struct
{
    void (*named)(const CliEachNameSizeCfg *args);
    void *context;
} CliSconsNamed;

// each word of a string handed on as an exported name
static void names_exported_words(const CliSconsSpanCfg *args)
{
    const CliSconsNamed *const hand = args->context;
    const char *const text = g_cli_scons.text;
    unsigned long long walk = 0ull;
    unsigned long long word = 0ull;
    unsigned long long length = 0ull;
    while (EMBED_CALL(cli_text_size_length.word, CliTextSizeLengthCfg, .text = text + args->from,
                      .size = args->to - args->from, .at_pointer = &walk, .word = &word, .length = &length))
    {
        EMBED_CALL(hand->named, CliEachNameSizeCfg, .context = hand->context, .name = text + args->from + word,
                   .size = length);
    }
}

// an argument of an `Export` call: its keyword's name, or its strings' words and its bare name
static void names_export_argument(const CliSconsSpanCfg *args)
{
    void *context = args->context;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    const CliSconsNamed *const hand = context;
    const char *const text = g_cli_scons.text;
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long value = 0ull;
    if (EMBED_CALL(cli_scons_from_internal.keyword, CliSconsFromCtx, .from = from, .to = to, .name_u64s = &name,
                   .name_size = &name_size, .value = &value))
    {
        EMBED_CALL(hand->named, CliEachNameSizeCfg, .context = hand->context, .name = text + name, .size = name_size);
        return;
    }
    EMBED_CALL(cli_scons_each_internal.strings, CliSconsEachCtx, .from = from, .to = to, .each = names_exported_words,
               .context = context);
    unsigned long long bare_from = 0ull;
    unsigned long long bare_to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + from, .size = to - from,
               .from_u64s = &bare_from, .to_u64s = &bare_to);
    if (bare_to == bare_from)
    {
        return;
    }
    for (unsigned long long walk = from + bare_from; walk < from + bare_to;)
    {
        unsigned long long length = 0ull;
        if (!EMBED_CALL(cli_each_letter.build_word_letter, CliEachLetterCfg,
                        .letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = from + bare_to,
                                             .at = walk, .length = &length)))
        {
            return;
        }
        walk += length;
    }
    EMBED_CALL(hand->named, CliEachNameSizeCfg, .context = hand->context, .name = text + from + bare_from,
               .size = bare_to - bare_from);
}

// the arguments of an `Export` call
static void names_export_call(const CliSconsSpanCfg *args)
{
    void *context = args->context;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    EMBED_CALL(cli_scons_each_internal.arguments, CliSconsEachCtx, .from = from, .to = to,
               .each = names_export_argument, .context = context);
}

// an argument of an `SConscript` call: the strings of an `exports` keyword's value
static void names_exports_argument(const CliSconsSpanCfg *args)
{
    void *context = args->context;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long value = 0ull;
    if (EMBED_CALL(cli_scons_from_internal.keyword, CliSconsFromCtx, .from = from, .to = to, .name_u64s = &name,
                   .name_size = &name_size, .value = &value) &&
        (name_size == 7u) &&
        (memor.cmp(&(MemoriaCfg){.src = g_cli_scons.text + name, .other = "exports", .bytes = 7u}) == 0))
    {
        EMBED_CALL(cli_scons_each_internal.strings, CliSconsEachCtx, .from = value, .to = to,
                   .each = names_exported_words, .context = context);
    }
}

// the arguments of an `SConscript` call, for what it exports
static void names_exports_call(const CliSconsSpanCfg *args)
{
    void *context = args->context;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    EMBED_CALL(cli_scons_each_internal.arguments, CliSconsEachCtx, .from = from, .to = to,
               .each = names_exports_argument, .context = context);
}

void cli_scons_export(const CliSconsContextCtx *args)
{
    void *context = args->context;
    CliSconsNamed hand = {args->named, context};
    EMBED_CALL(cli_scons_name_internal.calls, CliSconsNameCtx, .name = "Export", .each = names_export_call,
               .context = &hand);
    EMBED_CALL(cli_scons_name_internal.calls, CliSconsNameCtx, .name = "SConscript", .each = names_exports_call,
               .context = &hand);
}

// the most arguments of one `SConscript` call and strings of its `dirs` read
#define CLI_SCONS_ARGUMENTS 256u

// An `SConscript` call being read: its arguments' spans, how many, the name its `dirs` are joined to, and the scripts'
// hand
typedef struct
{
    unsigned long long from[CLI_SCONS_ARGUMENTS];
    unsigned long long to[CLI_SCONS_ARGUMENTS];
    unsigned int count;
    const char *name;
    unsigned long long name_size;
    CliSconsScript each;
    void *context;
} CliSconsCall;

// a string of `dirs` handed on as a script, its slashes at its end left off and the call's name joined to it
static void names_dir_string(const CliSconsSpanCfg *args)
{
    const CliSconsCall *const call = args->context;
    const char *const text = g_cli_scons.text;
    unsigned long long end = args->to;
    end = (args->from + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg,
                                   .text = text + args->from, .at = end - args->from, .set = "/"));
    static char s_written[1u << 12u];
    VerbaLinea written = {s_written, sizeof(s_written), 0ull};
    mmgr_line_put_n(&written, text + args->from, end - args->from);
    mmgr_line_ch(&written, '/');
    mmgr_line_put_n(&written, call->name, call->name_size);
    if (mmgr_line_ok(&written))
    {
        EMBED_CALL(call->each, CliSconsScriptCfg, .context = call->context, .written = s_written, .size = written.at,
                   .from = args->from, .to = args->to);
    }
}

// an argument's span recorded
static void names_argument_store(const CliSconsSpanCfg *args)
{
    CliSconsCall *const call = args->context;
    if (call->count < CLI_SCONS_ARGUMENTS)
    {
        call->from[call->count] = args->from;
        call->to[call->count] = args->to;
        call->count += 1u;
    }
}

// a string of the first argument handed on as a script
static void names_script_string(const CliSconsSpanCfg *args)
{
    const CliSconsCall *const call = args->context;
    EMBED_CALL(call->each, CliSconsScriptCfg, .context = call->context, .written = g_cli_scons.text + args->from,
               .size = args->to - args->from, .from = args->from, .to = args->to);
}

// the first string from `from` to `to`, into `first`: 1, or 0 where there is none
static void names_first_string(const CliSconsSpanCfg *args)
{
    unsigned long long *const first = args->context;
    if (first[2] == 0u)
    {
        first[0] = args->from;
        first[1] = args->to;
        first[2] = 1u;
    }
}

// the scripts an `SConscript` call reads
static void names_script_call(const CliSconsSpanCfg *args)
{
    void *context = args->context;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    static CliSconsCall s_call;
    const CliSconsCall *const outer = context;
    const char *const text = g_cli_scons.text;
    s_call.count = 0u;
    s_call.each = outer->each;
    s_call.context = outer->context;
    EMBED_CALL(cli_scons_each_internal.arguments, CliSconsEachCtx, .from = from, .to = to, .each = names_argument_store,
               .context = &s_call);
    const char *name = "SConscript";
    unsigned long long name_size = 10u;
    unsigned long long dirs_from = 0ull;
    unsigned long long dirs_to = 0ull;
    int dirs = 0;
    for (unsigned int at = 0u; at < s_call.count; at += 1u)
    {
        unsigned long long key = 0ull;
        unsigned long long key_size = 0ull;
        unsigned long long value = 0ull;
        if (EMBED_CALL(cli_scons_from_internal.keyword, CliSconsFromCtx, .from = s_call.from[at], .to = s_call.to[at],
                       .name_u64s = &key, .name_size = &key_size, .value = &value))
        {
            if ((key_size == 4u) && (memor.cmp(&(MemoriaCfg){.src = text + key, .other = "dirs", .bytes = 4u}) == 0))
            {
                dirs = 1;
                dirs_from = value;
                dirs_to = s_call.to[at];
            }
            else if ((key_size == 4u) &&
                     (memor.cmp(&(MemoriaCfg){.src = text + key, .other = "name", .bytes = 4u}) == 0))
            {
                unsigned long long first[3] = {0ull, 0ull, 0ull};
                EMBED_CALL(cli_scons_each_internal.strings, CliSconsEachCtx, .from = value, .to = s_call.to[at],
                           .each = names_first_string, .context = first);
                if (first[2] != 0u)
                {
                    name = text + first[0];
                    name_size = first[1] - first[0];
                }
            }
            continue;
        }
        const unsigned long long lead =
            s_call.from[at] + EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                         .text = text + s_call.from[at], .size = s_call.to[at] - s_call.from[at]);
        if ((at == 0u) && (lead < s_call.to[at]) &&
            ((text[lead] == '"') || (text[lead] == '\'') || (text[lead] == '[')))
        {
            EMBED_CALL(cli_scons_each_internal.strings, CliSconsEachCtx, .from = s_call.from[at], .to = s_call.to[at],
                       .each = names_script_string, .context = &s_call);
        }
    }
    if (dirs)
    {
        s_call.name = name;
        s_call.name_size = name_size;
        EMBED_CALL(cli_scons_each_internal.strings, CliSconsEachCtx, .from = dirs_from, .to = dirs_to,
                   .each = names_dir_string, .context = &s_call);
    }
}

void cli_scons_read_scripts(const CliSconsContextEachScriptCtx *args)
{
    CliSconsScript each = args->each_scons_script;
    void *context = args->context;
    static CliSconsCall s_outer;
    s_outer.each = each;
    s_outer.context = context;
    EMBED_CALL(cli_scons_name_internal.calls, CliSconsNameCtx, .name = "SConscript", .each = names_script_call,
               .context = &s_outer);
}
