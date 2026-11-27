// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// assembled.c: the texts the protocol passes, each assembled against the machine file with every stall safe, and passed
// back to the run
#include "../../../../src/platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "khw_machine_write_internal.h"
#include "memoria_operor/memoria_operor.h"

#include "../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble.h"

// what goes back to the run, the call's number, the count of texts and each text's count and code, and one text with
// its end, each sized once
#define KHW_ASSEMBLED_ROOM                                                                                             \
    (sizeof(unsigned long long) + sizeof(unsigned int) +                                                               \
     (KHW_ASSEMBLED_TEXTS * (sizeof(unsigned int) + (KHW_INSTRUCTION * KHW_ASSEMBLED_MAX))))
static unsigned char s_assembled[KHW_ASSEMBLED_ROOM];
static char s_text[KHW_ASSEMBLED_TEXT_BYTES + 1u];
static char s_reason[QRY_NAME_MAX_LEN + 64u];

// `first` and then `second` appended to the run as KHW_ASSEMBLED_REASON, the reason the texts were not assembled: 0
static int khw_assembled_reason(const char *first, const char *second)
{
    const size_t first_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = first, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t second_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = second, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if ((first_length + second_length) <= sizeof(s_reason))
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_reason, .src = first, .bytes = first_length);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_reason + first_length, .src = second, .bytes = second_length);
        qry_member_append(KHW_ASSEMBLED_REASON, s_reason, first_length + second_length, QRY_ORDINARY);
    }
    return 0;
}

// The texts of the latest blob of `answers`, read where the run stores it: a first line `call <n>`, then each text
// ended by a line containing a form feed alone. Each is assembled against the machine file at `path` with every stall
// safe, and appended to the run as KHW_ASSEMBLED: the call's number and the count of texts, eight bytes and four,
// then for each text the count of its instructions, four bytes, 0 where a line of it does not assemble, and its code,
// sixteen bytes an instruction. 1, or 0 with the reason appended as KHW_ASSEMBLED_REASON
int khw_texts_assembled(const char *path, const char *answers)
{
    const unsigned char *texts = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_blob(answers, &texts, &size, NULL))
    {
        return khw_assembled_reason("the run contains no ", answers);
    }
    if (!sass_machine_read(&s_machine, path))
    {
        return khw_assembled_reason(path, " is not read");
    }
    // the call's number, the digits after `call ` on the first line
    static const char s_call[] = "call ";
    unsigned long long from = sizeof(s_call) - 1u;
    if ((size < from) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = texts, .other = s_call, .bytes = (size_t)from) != 0))
    {
        return khw_assembled_reason(answers, " names no call");
    }
    unsigned long long call = 0ULL;
    while ((from < size) && EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = texts[from]))
    {
        call = (call * 10ULL) + (unsigned long long)(texts[from] - '0');
        from += 1ULL;
    }
    if ((from >= size) || (texts[from] != '\n'))
    {
        return khw_assembled_reason(answers, " names no call");
    }
    from += 1ULL;
    unsigned int taken = 0u;
    size_t written = sizeof(call) + sizeof(taken);
    while (from < size)
    {
        // the rest of the blob, which a size_t counts as the blob fits the buffer
        const unsigned char *const end = (const unsigned char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = texts + from,
                                                                           .bytes = (size_t)(size - from), .val = '\f');
        const size_t length = (end == NULL) ? 0u : (size_t)(end - (texts + from));
        if ((end == NULL) || (taken == KHW_ASSEMBLED_TEXTS) || (length > KHW_ASSEMBLED_TEXT_BYTES))
        {
            return khw_assembled_reason(answers, " contains a text past the room to assemble it");
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_text, .src = texts + from, .bytes = length);
        s_text[length] = '\0';
        const unsigned int instructions =
            sass_assemble_lines(&s_machine, s_text, SASS_CONTROL_SAFE, s_assembled + written + sizeof(unsigned int),
                                KHW_INSTRUCTION * KHW_ASSEMBLED_MAX);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_assembled + written, .src = &instructions,
                   .bytes = sizeof(instructions));
        written += sizeof(instructions) + ((size_t)instructions * KHW_INSTRUCTION);
        taken += 1u;
        // past the form feed and the line's end after it
        from = (unsigned long long)(end - texts) + 2ULL;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_assembled, .src = &call, .bytes = sizeof(call));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_assembled + sizeof(call), .src = &taken, .bytes = sizeof(taken));
    if (!qry_member_append(KHW_ASSEMBLED, s_assembled, written, QRY_ORDINARY))
    {
        return khw_assembled_reason(KHW_ASSEMBLED, " could not be appended to the run");
    }
    return 1;
}
