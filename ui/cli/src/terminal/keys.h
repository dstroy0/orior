// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keys.h: what a key sends a shell, as an xterm sends it: the cursor keys, plain or in the application mode, with the
// modifiers in their parameter; Enter, Backspace, Tab, Escape, Insert, Delete, the page keys and F1 to F12, Alt
// putting an escape before them; Control with a letter or one of @ [ \ ] ^ _ its control letter; and Alt with a letter
// or a sign its escape and the letter. A letter or a sign alone is text typed, which reaches the shell as typed
// (keys.c)
#ifndef CLI_TERMINAL_KEYS_H
#define CLI_TERMINAL_KEYS_H
#include "embed_dispatch_layout.h"
#include "verba_scribo/verba_linea.h"

typedef struct CliTerminalKeyCfg CliTerminalKeyCfg;

// the bytes the key `name`, `name_size` bytes as the window names it, with the modifier bits `mods` down
// (CLI_INPUT_MOD_*), sends a shell put after `out`, the cursor keys as the application mode sends them where
// `application_cursor` is 1: 1, or 0 for a key the terminal leaves alone
int cli_terminal_key_bytes(const CliTerminalKeyCfg *args);
struct CliTerminalKeyCfg
{
    const char *name;
    unsigned int name_size;
    unsigned int mods;
    int application_cursor;
    VerbaLinea *out;
};

typedef struct
{
    int (*bytes)(const CliTerminalKeyCfg *args);
} CliTerminalKeyNs;
EMBED_TABLE_LAYOUT(CliTerminalKeyNs, bytes);
EMBED_TABLE_STORAGE CliTerminalKeyNs cli_terminal_key EMBED_UNUSED = {
    .bytes = cli_terminal_key_bytes,
};

#endif
