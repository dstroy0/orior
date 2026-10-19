// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// assembled.c: the texts the protocol hands, each assembled against the machine file with every stall safe, and handed
// back to the run
#include "khw_machine_write_internal.h"

#include "../../../../transpiler/vendor_bin_layouts/nvidia/sass_assemble.h"

// what goes back to the run, the call's number, the count of texts and each text's count and code, and one text with
// its end, each sized once
#define KHW_ASSEMBLED_ROOM                                                                                             \
    (sizeof(unsigned long long) + sizeof(unsigned int) +                                                               \
     (KHW_ASSEMBLED_TEXTS * (sizeof(unsigned int) + (KHW_INSTRUCTION * KHW_ASSEMBLED_MOST))))
static unsigned char s_assembled[KHW_ASSEMBLED_ROOM];
static char s_text[KHW_ASSEMBLED_TEXT_BYTES + 1u];
static char s_said[QRY_NAME_LONGEST + 64u];

// `first` and then `second` appended to the run as KHW_ASSEMBLED_SAID, the reason the texts were not assembled: 0
static int khw_assembled_said(const char *first, const char *second)
{
    const size_t first_length = strlen(first);
    const size_t second_length = strlen(second);
    if ((first_length + second_length) <= sizeof(s_said))
    {
        memcpy(s_said, first, first_length);
        memcpy(s_said + first_length, second, second_length);
        qry_member_append(KHW_ASSEMBLED_SAID, s_said, first_length + second_length, QRY_ORDINARY);
    }
    return 0;
}

// The texts of the latest blob of `answers`, read where the run holds it: a first line `call <n>`, then each text
// ended by a line holding a form feed alone. Each is assembled against the machine file at `path` with every stall
// safe, and appended to the run as KHW_ASSEMBLED: the call's number and the count of texts, eight bytes and four,
// then for each text the count of its instructions, four bytes, 0 where a line of it does not assemble, and its code,
// sixteen bytes an instruction. 1, or 0 with the reason appended as KHW_ASSEMBLED_SAID
int khw_texts_assembled(const char *path, const char *answers)
{
    const unsigned char *texts = NULL;
    unsigned long long size = 0ull;
    if (!qry_member_blob(answers, &texts, &size, NULL))
    {
        return khw_assembled_said("the run holds no ", answers);
    }
    if (!sass_machine_read(&s_machine, path))
    {
        return khw_assembled_said(path, " is not read");
    }
    // the call's number, the digits after `call ` on the first line
    static const char s_call[] = "call ";
    unsigned long long from = sizeof(s_call) - 1u;
    if ((size < from) || (memcmp(texts, s_call, (size_t)from) != 0))
    {
        return khw_assembled_said(answers, " names no call");
    }
    unsigned long long call = 0ull;
    while ((from < size) && (texts[from] >= '0') && (texts[from] <= '9'))
    {
        call = (call * 10ull) + (unsigned long long)(texts[from] - '0');
        from += 1ull;
    }
    if ((from >= size) || (texts[from] != '\n'))
    {
        return khw_assembled_said(answers, " names no call");
    }
    from += 1ull;
    unsigned int taken = 0u;
    size_t written = sizeof(call) + sizeof(taken);
    while (from < size)
    {
        // the rest of the blob, which a size_t counts as the blob fits the buffer
        const unsigned char *const end = (const unsigned char *)memchr(texts + from, '\f', (size_t)(size - from));
        const size_t length = (end == NULL) ? 0u : (size_t)(end - (texts + from));
        if ((end == NULL) || (taken == KHW_ASSEMBLED_TEXTS) || (length > KHW_ASSEMBLED_TEXT_BYTES))
        {
            return khw_assembled_said(answers, " holds a text past the room to assemble it");
        }
        memcpy(s_text, texts + from, length);
        s_text[length] = '\0';
        const unsigned int instructions =
            sass_assemble_lines(&s_machine, s_text, SASS_CONTROL_SAFE, s_assembled + written + sizeof(unsigned int),
                                KHW_INSTRUCTION * KHW_ASSEMBLED_MOST);
        memcpy(s_assembled + written, &instructions, sizeof(instructions));
        written += sizeof(instructions) + ((size_t)instructions * KHW_INSTRUCTION);
        taken += 1u;
        // past the form feed and the line's end after it
        from = (unsigned long long)(end - texts) + 2ull;
    }
    memcpy(s_assembled, &call, sizeof(call));
    memcpy(s_assembled + sizeof(call), &taken, sizeof(taken));
    if (!qry_member_append(KHW_ASSEMBLED, s_assembled, written, QRY_ORDINARY))
    {
        return khw_assembled_said(KHW_ASSEMBLED, " could not be appended to the run");
    }
    return 1;
}
