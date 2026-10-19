// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// who.h: the who of a write, as numbers in place of names: the number the who table gives the source, the line in it,
// the number it gives the program's own source, and the time on the run's one clock. Its two words are the source,
// the line and the program sixteen bits each above sixteen of 0, and the time; its code is the two words in hex
#ifndef WHO_H
#define WHO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the who table, from the top of the tree: a line a source, its number in hex and its path from the top
#define WHO_TABLE "utils/maint/who/who.tsv"

// the hex digits of a who's code
#define WHO_CODE_LETTERS 32u

    typedef struct
    {
        unsigned int source;
        unsigned int line;
        unsigned int program;
        unsigned long long time;
    } Who;

    // the who of the code at line `line` of the source `file`, as __FILE__ and __LINE__ give them, in this program,
    // now on the run's one clock; a number the table gives none is 0
    Who who_here(const char *file, unsigned int line);

// the who of the code this stands in
#define WHO_HERE who_here(__FILE__, (unsigned int)__LINE__)

    // `who` as its two words, into `words`
    void who_words(Who who, unsigned long long *words);

    // the who the two words `words` hold
    Who who_of_words(const unsigned long long *words);

    // `who`'s code into `code`, which holds `room`, WHO_CODE_LETTERS hex digits and its end; empty where it does not
    // fit
    void who_code(Who who, char *code, size_t room);

    // the code `code` read back into `who`: 1, or 0 where it is not WHO_CODE_LETTERS hex digits
    int who_read(const char *code, Who *who);

    // `who` named into `text`, which holds `room`: the source's path from the top of the tree and the line, the
    // program's source, and the time on the run's clock in nanoseconds, `-` for a number the table does not give: 1,
    // or 0 where it does not fit
    int who_named(Who who, char *text, size_t room);

#ifdef __cplusplus
}
#endif

#endif
