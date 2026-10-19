// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_names.c: what an SConstruct or SConscript says of names and of other files: the names its `Import` calls take,
// each word of their strings; the names it exports, by `Export`'s strings, keywords and bare names and by the strings
// of an `exports` given an `SConscript` call; and the SConscripts its `SConscript` calls read, by the strings of their
// first argument or by their `dirs`, each joined to their `name`, `SConscript` where none is given
#include "scons_internal.h"

// The hand an import is handed to: the function and its context
typedef struct
{
    CliSconsSpan each;
    void *context;
} CliSconsHand;

// each word of a string handed to the hand
static void names_word_handed(void *context, unsigned long long from, unsigned long long to)
{
    const CliSconsHand *const hand = context;
    const char *const text = g_cli_scons.text;
    unsigned long long walk = 0ull;
    unsigned long long word = 0ull;
    unsigned long long length = 0ull;
    while (cli_text_word(text + from, to - from, &walk, &word, &length))
    {
        hand->each(hand->context, from + word, from + word + length);
    }
}

// the strings of an `Import` call's arguments handed on, word by word
static void names_import_call(void *context, unsigned long long from, unsigned long long to)
{
    cli_scons_strings(from, to, names_word_handed, context);
}

void cli_scons_imported(CliSconsSpan each, void *context)
{
    CliSconsHand hand = {each, context};
    cli_scons_calls("Import", names_import_call, &hand);
}

// The hand an exported name is handed to: the function and its context
typedef struct
{
    void (*named)(void *context, const char *name, unsigned long long size);
    void *context;
} CliSconsNamed;

// each word of a string handed on as an exported name
static void names_exported_words(void *context, unsigned long long from, unsigned long long to)
{
    const CliSconsNamed *const hand = context;
    const char *const text = g_cli_scons.text;
    unsigned long long walk = 0ull;
    unsigned long long word = 0ull;
    unsigned long long length = 0ull;
    while (cli_text_word(text + from, to - from, &walk, &word, &length))
    {
        hand->named(hand->context, text + from + word, length);
    }
}

// an argument of an `Export` call: its keyword's name, or its strings' words and its bare name
static void names_export_argument(void *context, unsigned long long from, unsigned long long to)
{
    const CliSconsNamed *const hand = context;
    const char *const text = g_cli_scons.text;
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long value = 0ull;
    if (cli_scons_keyword(from, to, &name, &name_size, &value))
    {
        hand->named(hand->context, text + name, name_size);
        return;
    }
    cli_scons_strings(from, to, names_exported_words, context);
    unsigned long long bare_from = 0ull;
    unsigned long long bare_to = 0ull;
    cli_text_trimmed(text + from, to - from, &bare_from, &bare_to);
    if (bare_to == bare_from)
    {
        return;
    }
    for (unsigned long long walk = from + bare_from; walk < from + bare_to;)
    {
        unsigned long long length = 0ull;
        if (!cli_build_word_letter(cli_text_letter(text, from + bare_to, walk, &length)))
        {
            return;
        }
        walk += length;
    }
    hand->named(hand->context, text + from + bare_from, bare_to - bare_from);
}

// the arguments of an `Export` call
static void names_export_call(void *context, unsigned long long from, unsigned long long to)
{
    cli_scons_arguments(from, to, names_export_argument, context);
}

// an argument of an `SConscript` call: the strings of an `exports` keyword's value
static void names_exports_argument(void *context, unsigned long long from, unsigned long long to)
{
    unsigned long long name = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long value = 0ull;
    if (cli_scons_keyword(from, to, &name, &name_size, &value) && (name_size == 7u) &&
        (memor.cmp(&(MemoriaCfg){.src = g_cli_scons.text + name, .other = "exports", .bytes = 7u}) == 0))
    {
        cli_scons_strings(value, to, names_exported_words, context);
    }
}

// the arguments of an `SConscript` call, for what it exports
static void names_exports_call(void *context, unsigned long long from, unsigned long long to)
{
    cli_scons_arguments(from, to, names_exports_argument, context);
}

void cli_scons_exported(void (*named)(void *context, const char *name, unsigned long long size), void *context)
{
    CliSconsNamed hand = {named, context};
    cli_scons_calls("Export", names_export_call, &hand);
    cli_scons_calls("SConscript", names_exports_call, &hand);
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
static void names_dir_string(void *context, unsigned long long from, unsigned long long to)
{
    const CliSconsCall *const call = context;
    const char *const text = g_cli_scons.text;
    unsigned long long end = to;
    while ((end > from) && (text[end - 1u] == '/'))
    {
        end -= 1u;
    }
    static char s_written[1u << 12u];
    CliLine written = {s_written, sizeof(s_written), 0ull};
    cli_line_counted(&written, text + from, end - from);
    cli_line_character(&written, '/');
    cli_line_counted(&written, call->name, call->name_size);
    if (cli_line_fits(&written))
    {
        call->each(call->context, s_written, written.at, from, to);
    }
}

// an argument's span kept
static void names_argument_kept(void *context, unsigned long long from, unsigned long long to)
{
    CliSconsCall *const call = context;
    if (call->count < CLI_SCONS_ARGUMENTS)
    {
        call->from[call->count] = from;
        call->to[call->count] = to;
        call->count += 1u;
    }
}

// a string of the first argument handed on as a script
static void names_script_string(void *context, unsigned long long from, unsigned long long to)
{
    const CliSconsCall *const call = context;
    call->each(call->context, g_cli_scons.text + from, to - from, from, to);
}

// the first string from `from` to `to`, into `first`: 1, or 0 where there is none
static void names_first_string(void *context, unsigned long long from, unsigned long long to)
{
    unsigned long long *const first = context;
    if (first[2] == 0u)
    {
        first[0] = from;
        first[1] = to;
        first[2] = 1u;
    }
}

// the scripts an `SConscript` call reads
static void names_script_call(void *context, unsigned long long from, unsigned long long to)
{
    static CliSconsCall s_call;
    const CliSconsCall *const outer = context;
    const char *const text = g_cli_scons.text;
    s_call.count = 0u;
    s_call.each = outer->each;
    s_call.context = outer->context;
    cli_scons_arguments(from, to, names_argument_kept, &s_call);
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
        if (cli_scons_keyword(s_call.from[at], s_call.to[at], &key, &key_size, &value))
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
                cli_scons_strings(value, s_call.to[at], names_first_string, first);
                if (first[2] != 0u)
                {
                    name = text + first[0];
                    name_size = first[1] - first[0];
                }
            }
            continue;
        }
        const unsigned long long lead =
            s_call.from[at] + cli_text_lead(text + s_call.from[at], s_call.to[at] - s_call.from[at]);
        if ((at == 0u) && (lead < s_call.to[at]) &&
            ((text[lead] == '"') || (text[lead] == '\'') || (text[lead] == '[')))
        {
            cli_scons_strings(s_call.from[at], s_call.to[at], names_script_string, &s_call);
        }
    }
    if (dirs)
    {
        s_call.name = name;
        s_call.name_size = name_size;
        cli_scons_strings(dirs_from, dirs_to, names_dir_string, &s_call);
    }
}

void cli_scons_read_scripts(CliSconsScript each, void *context)
{
    static CliSconsCall s_outer;
    s_outer.each = each;
    s_outer.context = context;
    cli_scons_calls("SConscript", names_script_call, &s_outer);
}
