// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_note.h: a note of the engine to the error stream. The note is a line of verba_linea.h over a buffer on the
// local stack, and the platform writes it whole. A C++ unit includes this header as it is
#ifndef ENGINE_NOTE_H
#define ENGINE_NOTE_H

#include "../../platform/platform.h"

// the bytes of one note. A note past them keeps what fits
#define ENGINE_NOTE_BYTES 512u

// One note: its bytes, at the alignment that MMgr reads its buffers at, and the line over them
typedef struct
{
    EMBED_ALIGN(MMGR_ALIGN_BYTES) char text[ENGINE_NOTE_BYTES];
    VerbaLinea line;
} EngineNote;

// the note begun, empty, over its own bytes
static inline void engine_note_begun(EngineNote *note)
{
    note->line = mmgr_line_over(note->text, sizeof(note->text));
}

// the note written whole to the error stream
static inline void engine_note_written(EngineNote *note)
{
    const size_t length = mmgr_line_finish(&note->line);
    platform_terminal_write(1, note->text, (unsigned long long)((length != 0u) ? length : (sizeof(note->text) - 1u)));
}

#endif
