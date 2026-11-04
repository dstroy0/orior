// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keys.c: what a key sends a shell (keys.h)
#include "keys.h"
#include "../command_line/cli_internal.h"

// A key the terminal sends a sequence for: its name as the window names it, and what it sends
typedef struct
{
    const char *name;
    const char *sends;
} KeysSequence;

typedef struct
{
    const char *name;
    unsigned int name_size;
    const char *other;
    unsigned int mods;
    int application_cursor;
    VerbaLinea *out;
} KeysCtx;

// whether the key's name is `other`, a C string
EMBED_INLINE int keys_name_is(const KeysCtx *args)
{
    unsigned int at = 0u;
    while ((at < args->name_size) && (args->other[at] != '\0') && (args->name[at] == args->other[at]))
    {
        at += 1u;
    }
    return (at == args->name_size) && (args->other[at] == '\0');
}

// a cursor key: ESC [ 1 ; mods+1 letter with modifiers, else ESC O letter in the application mode and ESC [ letter
// out of it: 1, or 0 where the key is none of them
EMBED_INLINE int keys_cursor(const KeysCtx *args)
{
    static const KeysSequence s_cursor[6] = {{"Up", "A"},   {"Down", "B"}, {"Right", "C"},
                                             {"Left", "D"}, {"Home", "H"}, {"End", "F"}};
    // the modifiers as an xterm counts them: Shift 1, Alt 2, Control 4
    const unsigned int counted = (((args->mods & CLI_TERMINAL_MOD_SHIFT) != 0u) ? 1u : 0u) +
                                 (((args->mods & CLI_TERMINAL_MOD_ALT) != 0u) ? 2u : 0u) +
                                 (((args->mods & CLI_TERMINAL_MOD_CONTROL) != 0u) ? 4u : 0u);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (!EMBED_CALL(keys_name_is, KeysCtx, .name = args->name, .name_size = args->name_size,
                        .other = s_cursor[at].name))
        {
            continue;
        }
        mmgr_line_put(args->out, (counted != 0u) ? "\x1b[1;" : (args->application_cursor ? "\x1bO" : "\x1b["));
        if (counted != 0u)
        {
            mmgr_line_decimal(args->out, (unsigned long long)counted + 1ULL, 1u);
        }
        mmgr_line_put(args->out, s_cursor[at].sends);
        return 1;
    }
    return 0;
}

// a key with a sequence of its own, Alt putting an escape before it: 1, or 0 where the key is none of them
EMBED_INLINE int keys_special(const KeysCtx *args)
{
    static const KeysSequence s_special[20] = {
        {"Enter", "\r"},       {"Backspace", "\x7f"}, {"Tab", "\t"},         {"Escape", "\x1b"},
        {"Insert", "\x1b[2~"}, {"Delete", "\x1b[3~"}, {"PageUp", "\x1b[5~"}, {"PageDown", "\x1b[6~"},
        {"F1", "\x1bOP"},      {"F2", "\x1bOQ"},      {"F3", "\x1bOR"},      {"F4", "\x1bOS"},
        {"F5", "\x1b[15~"},    {"F6", "\x1b[17~"},    {"F7", "\x1b[18~"},    {"F8", "\x1b[19~"},
        {"F9", "\x1b[20~"},    {"F10", "\x1b[21~"},   {"F11", "\x1b[23~"},   {"F12", "\x1b[24~"}};
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        if (EMBED_CALL(keys_name_is, KeysCtx, .name = args->name, .name_size = args->name_size,
                       .other = s_special[at].name))
        {
            mmgr_line_put(args->out, ((args->mods & CLI_TERMINAL_MOD_ALT) != 0u) ? "\x1b" : "");
            mmgr_line_put(args->out, s_special[at].sends);
            return 1;
        }
    }
    return 0;
}

// a key whose name is one letter or sign, or Space, with Control or Alt down: Control's control letter, or Alt's escape
// and the letter, a letter in its upper case only with Shift: 1, or 0 where it sends nothing
EMBED_INLINE int keys_modified(const KeysCtx *args)
{
    const int space =
        EMBED_CALL(keys_name_is, KeysCtx, .name = args->name, .name_size = args->name_size, .other = "Space");
    if ((args->name_size != 1u) && !space)
    {
        return 0;
    }
    const char key = space ? ' ' : args->name[0];
    const int control = (args->mods & CLI_TERMINAL_MOD_CONTROL) != 0u;
    const int alt = (args->mods & CLI_TERMINAL_MOD_ALT) != 0u;
    const int shift = (args->mods & CLI_TERMINAL_MOD_SHIFT) != 0u;
    const int letter = (key >= 'A') && (key <= 'Z');
    if (control && !alt)
    {
        // a letter or one of @ [ \ ] ^ _ is its code less 64; Space, / and ? have control letters of their own
        const int lettered =
            letter || (key == '@') || (key == '[') || (key == '\\') || (key == ']') || (key == '^') || (key == '_');
        const int sends = lettered || (key == ' ') || (key == '/');
        const char sent = lettered                  ? (char)(key - '@')
                          : (key == ' ')            ? '\0'
                          : ((key == '/') && shift) ? '\x7f'
                                                    : '\x1f';
        if (sends)
        {
            mmgr_line_ch(args->out, sent);
        }
        return sends;
    }
    if (alt && !control)
    {
        mmgr_line_ch(args->out, '\x1b');
        mmgr_line_ch(args->out, (letter && !shift) ? (char)(key + ('a' - 'A')) : key);
        return 1;
    }
    return 0;
}

int cli_terminal_key_bytes(const CliTerminalKeyCfg *args)
{
    const int shift = (args->mods & CLI_TERMINAL_MOD_SHIFT) != 0u;
    const int control = (args->mods & CLI_TERMINAL_MOD_CONTROL) != 0u;
    if (EMBED_CALL(keys_cursor, KeysCtx, .name = args->name, .name_size = args->name_size, .mods = args->mods,
                   .application_cursor = args->application_cursor, .out = args->out))
    {
        return 1;
    }
    if (shift && EMBED_CALL(keys_name_is, KeysCtx, .name = args->name, .name_size = args->name_size, .other = "Tab"))
    {
        mmgr_line_put(args->out, "\x1b[Z");
        return 1;
    }
    if (control &&
        EMBED_CALL(keys_name_is, KeysCtx, .name = args->name, .name_size = args->name_size, .other = "Backspace"))
    {
        mmgr_line_ch(args->out, '\x08');
        return 1;
    }
    return EMBED_CALL(keys_special, KeysCtx, .name = args->name, .name_size = args->name_size, .mods = args->mods,
                      .out = args->out) ||
           EMBED_CALL(keys_modified, KeysCtx, .name = args->name, .name_size = args->name_size, .mods = args->mods,
                      .out = args->out);
}
