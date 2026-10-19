// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_internal.h: what the line readers of each language share: a piece of a line, the declarations of one line
// handed on, the pieces read (symbols_piece.c), and the languages read in symbols_lines.c
#ifndef SYMBOLS_INTERNAL_H
#define SYMBOLS_INTERNAL_H

#include "symbols.h"

// the languages read here
enum
{
    CLI_LANG_NONE,
    CLI_LANG_PYTHON,
    CLI_LANG_RUST,
    CLI_LANG_SCRIPT,
    CLI_LANG_C,
    CLI_LANG_SHELL,
    CLI_LANG_POWERSHELL,
    CLI_LANG_TOML,
    CLI_LANG_MARKDOWN,
    CLI_LANG_TEX
};

// A piece of a line: where it starts and its length
typedef struct
{
    const char *at;
    unsigned long long length;
} CliPiece;

// one line's declarations handed on: where to, and the line's number
typedef struct
{
    CliDeclared each;
    void *context;
    unsigned int line;
} CliOut;

// a name found handed on; a piece's helpers: ASCII's spaces, letters and digits, a piece trimmed, past a place,
// opening with or being a word, where a letter first stands, the name it opens with, past a word, past any of words,
// and among words
void cli_piece_put(const CliOut *out, CliPiece name, const char *kind);
int cli_piece_space(char letter);
int cli_piece_alpha(char letter);
int cli_piece_alnum(char letter);
CliPiece cli_piece_trim_start(CliPiece text);
CliPiece cli_piece_trim_end(CliPiece text);
CliPiece cli_piece_from(CliPiece text, unsigned long long at);
int cli_piece_starts(CliPiece text, const char *prefix);
int cli_piece_equals(CliPiece text, const char *word);
unsigned long long cli_piece_find(CliPiece text, char letter);
int cli_piece_name_at(CliPiece text, int dollar, CliPiece *name, CliPiece *rest);
int cli_piece_after(CliPiece text, const char *word, CliPiece *rest);
CliPiece cli_piece_without(CliPiece text, const char *const *words, unsigned int count);
int cli_piece_in(CliPiece word, const char *const *words, unsigned int count);
// one line read in the shell's, PowerShell's, TOML's, Markdown's and TeX's shapes (symbols_lines.c)
void cli_symbols_shell(CliPiece line, const CliOut *out);
void cli_symbols_powershell(CliPiece line, const CliOut *out);
void cli_symbols_toml(CliPiece line, const CliOut *out);
void cli_symbols_markdown(CliPiece line, const CliOut *out);
void cli_symbols_tex(CliPiece line, const CliOut *out);

#endif
