// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text_note.h: a note added to the end of a text. The note is a line of verba_linea.h over a buffer on the local
// stack. The line passes the buffer to the text each time it fills, and the end of the note passes what is left. A
// note of any length goes to the text whole. Only a C++ unit includes this header
#ifndef TEXT_NOTE_H
#define TEXT_NOTE_H

#include "../../platform/platform.h"

#include <string>

// the bytes that the note holds before it passes them to the text
#define TEXT_NOTE_BYTES 256u

// One note: its bytes, at the alignment that MMgr reads its buffers at, and the line over them
struct TextNote
{
    EMBED_ALIGN(MMGR_ALIGN_BYTES) char text[TEXT_NOTE_BYTES];
    VerbaLinea line;
};

// the bytes of a full line added to the end of the text that `context` names. The text takes every byte
static inline int text_note_passed(void *context, const char *bytes, size_t length)
{
    static_cast<std::string *>(context)->append(bytes, length);
    return 1;
}

// the note begun, empty, over its own bytes, for the end of `store`
static inline void text_note_begun(TextNote *note, std::string *store)
{
    note->line = VerbaLinea{note->text, sizeof(note->text), 0u, text_note_passed, store, 0u};
}

// the note begun as a line of no room. Each write to it goes nowhere
static inline void text_note_closed(TextNote *note)
{
    note->line = mmgr_line_over(NULL, 0u);
}

// what the note holds added to the end of its text
static inline void text_note_written(TextNote *note)
{
    (void)mmgr_line_flush(&note->line);
}

#endif
