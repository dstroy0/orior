// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// assembled.cu: texts of the vendor's language put into code by the vendor's writer, a member of the run started
// through the interface; the identity contains no machine file and assembles nothing itself
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"
#include "memoria_operor/memoria_operor.h"

// the blob the texts go in and the blob the vendor's writer passes their code back in
#define ASSEMBLE_TEXTS "assemble.txt"

#define ASSEMBLE_CODE "assembled.bin"

// what the vendor's writer reported while it assembled, recorded for the reason where it did not
static char s_writer_output[4096];

// The texts put into code as vendor_assembled puts them, each code begun on the temporary tier where `ephemeral` is
// 1 and after the codes `codes` holds
static void vendor_assembled_on(const char *machine, const char *const *texts, unsigned long long count_wanted,
                                PlatformArray *codes, int ephemeral)
{
    static unsigned long long s_call = 0ULL;
    // the place in `codes` of the first text's code
    const unsigned long long first_code = codes->count;
    for (unsigned long long at = 0ULL; at < count_wanted; at += 1ULL)
    {
        IdentityCode empty;
        identity_code_begun(&empty, ephemeral);
        (void)platform_array_appended(codes, &empty);
    }
    // the vendor's writer the query crate named, the latest IDENTITY_WRITER of the run
    static IdentityWriter s_writer;
    const unsigned char *named = NULL;
    unsigned long long named_size = 0ULL;
    if (!qry_member_blob(IDENTITY_WRITER, &named, &named_size, NULL) || (named_size != sizeof(s_writer)) ||
        (count_wanted == 0ULL))
    {
        return;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &s_writer, .src = named, .bytes = sizeof(s_writer));
    s_writer.program[sizeof(s_writer.program) - 1u] = '\0';
    s_writer.part[sizeof(s_writer.part) - 1u] = '\0';
    s_writer.layout[sizeof(s_writer.layout) - 1u] = '\0';
    s_writer.mnemonics[sizeof(s_writer.mnemonics) - 1u] = '\0';
    s_call += 1ULL;
    PlatformText passed;
    identity_text_begun(&passed);
    mmgr_line_put(&passed.line, "call ");
    mmgr_line_decimal(&passed.line, s_call, 1u);
    mmgr_line_ch(&passed.line, '\n');
    for (unsigned long long at = 0ULL; at < count_wanted; at += 1ULL)
    {
        mmgr_line_put(&passed.line, texts[at]);
        mmgr_line_put(&passed.line, "\f\n");
    }
    const unsigned long long passed_size = platform_text_size(&passed);
    const char *const passed_bytes = platform_text_bytes(&passed);
    const int appended =
        (passed_bytes != NULL) && (qry_member_append(ASSEMBLE_TEXTS, passed_bytes, passed_size, QRY_ORDINARY) != 0ULL);
    platform_text_ended(&passed);
    if (!appended)
    {
        return;
    }
    // the writer's words: its program and part, the machine file, the layout and mnemonics, the blob and the mode
    char *command[] = {s_writer.program,
                       s_writer.part,
                       const_cast<char *>(machine),
                       s_writer.layout,
                       s_writer.mnemonics,
                       const_cast<char *>(ASSEMBLE_TEXTS),
                       const_cast<char *>("assemble"),
                       NULL};
    const InterfaceProbe probe = {command, NULL, 0ULL};
    InterfaceAnswer answer = {};
    answer.output = s_writer_output;
    answer.output_capacity = sizeof(s_writer_output);
    EngineError error = {};
    // what this process passed moved into the run before the writer reads it, and what the writer passed once it ends
    qry_member_settled();
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_member_rings_settled();
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ULL))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity: the vendor's writer did not assemble: ");
        mmgr_line_put(output_line, s_writer_output);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return;
    }
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    unsigned long long call = 0ULL;
    unsigned int count = 0u;
    if (!qry_member_blob(ASSEMBLE_CODE, &bytes, &size, NULL) || (size < (sizeof(call) + sizeof(count))))
    {
        return;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &call, .src = bytes, .bytes = sizeof(call));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &count, .src = bytes + sizeof(call), .bytes = sizeof(count));
    if ((call != s_call) || (count != count_wanted))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity: the vendor's writer passed call ");
        mmgr_line_decimal(output_line, call, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, count, 1u);
        mmgr_line_put(output_line, " texts for call ");
        mmgr_line_decimal(output_line, s_call, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, count_wanted, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return;
    }
    unsigned long long at = sizeof(call) + sizeof(count);
    for (unsigned long long text = 0ULL; (text < count_wanted) && ((at + sizeof(count)) <= size); text += 1ULL)
    {
        unsigned int instructions = 0u;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &instructions, .src = bytes + at, .bytes = sizeof(instructions));
        at += sizeof(instructions);
        const unsigned long long length = 16ULL * instructions;
        if ((at + length) > size)
        {
            break;
        }
        IdentityCode *const code = (IdentityCode *)platform_array_at(codes, first_code + text);
        if ((code != NULL) && platform_array_counted(code, length) && (length != 0ULL))
        {
            // a code of one text is at max the blob's bytes, which a size_t counts
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = code->cell, .src = bytes + at, .bytes = (size_t)length);
        }
        at += length;
    }
}

void vendor_assembled(const char *machine, const char *const *texts, unsigned long long count, PlatformArray *codes)
{
    vendor_assembled_on(machine, texts, count, codes, codes->ephemeral);
}

void vendor_assembled(const char *machine, const char *text, IdentityCode *code)
{
    const char *const texts[1] = {text};
    PlatformArray codes;
    platform_array_begun(&codes, sizeof(IdentityCode), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    vendor_assembled_on(machine, texts, 1ULL, &codes, code->ephemeral);
    platform_array_ended(code);
    const IdentityCode *const made = (const IdentityCode *)platform_array_at(&codes, 0ULL);
    if (made != NULL)
    {
        *code = *made;
    }
    else
    {
        identity_code_begun(code, code->ephemeral);
    }
    platform_array_ended(&codes);
}

void identity_codes_ended(PlatformArray *codes)
{
    for (unsigned long long at = 0ULL; at < codes->count; at += 1ULL)
    {
        platform_array_ended((IdentityCode *)platform_array_at(codes, at));
    }
    platform_array_ended(codes);
}
