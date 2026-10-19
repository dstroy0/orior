// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// assembled.cu: texts of the vendor's language put into code by the vendor's writer, a member of the run started
// through the interface; the identity holds no machine file and assembles nothing itself
#include "identity_internal.h"

// the blob the texts are handed in and the blob the vendor's writer hands their code back in
#define ASSEMBLE_TEXTS "assemble.txt"

#define ASSEMBLE_CODE "assembled.bin"

// what the vendor's writer said while it assembled, kept for the reason where it did not
static char s_writer_output[4096];

std::vector<std::vector<unsigned char>> vendor_assembled(const char *machine, const std::vector<std::string> &texts)
{
    static unsigned long long s_call = 0ull;
    std::vector<std::vector<unsigned char>> codes(texts.size());
    // the vendor's writer the query crate named, the latest IDENTITY_WRITER of the run
    static IdentityWriter s_writer;
    const unsigned char *named = NULL;
    unsigned long long named_size = 0ull;
    if (!qry_member_blob(IDENTITY_WRITER, &named, &named_size, NULL) || (named_size != sizeof(s_writer)) || texts.empty())
    {
        return codes;
    }
    memcpy(&s_writer, named, sizeof(s_writer));
    s_writer.program[sizeof(s_writer.program) - 1u] = '\0';
    s_writer.part[sizeof(s_writer.part) - 1u] = '\0';
    s_writer.layout[sizeof(s_writer.layout) - 1u] = '\0';
    s_writer.mnemonics[sizeof(s_writer.mnemonics) - 1u] = '\0';
    s_call += 1ull;
    std::string handed = "call " + std::to_string(s_call) + "\n";
    for (const std::string &text : texts)
    {
        handed += text + "\f\n";
    }
    if (qry_member_append(ASSEMBLE_TEXTS, handed.data(), handed.size(), QRY_ORDINARY) == 0ull)
    {
        return codes;
    }
    // the writer's words: its program and part, the machine file, the layout and mnemonics, the blob and the mode
    char *command[] = {s_writer.program,        s_writer.part,          const_cast<char *>(machine),
                       s_writer.layout,         s_writer.mnemonics,     const_cast<char *>(ASSEMBLE_TEXTS),
                       const_cast<char *>("assemble"), NULL};
    const InterfaceProbe probe = {command, NULL, 0ull};
    InterfaceAnswer answer = {};
    answer.output = s_writer_output;
    answer.output_capacity = sizeof(s_writer_output);
    EngineError error = {};
    // what this process handed moved into the run before the writer reads it, and what the writer handed once it ends
    qry_member_settled();
    const long ran = interface_probe_run(&probe, &answer, &error);
    qry_member_rings_settled();
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("klq_identity: the vendor's writer did not assemble: %s\n", s_writer_output);
        return codes;
    }
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long call = 0ull;
    unsigned int count = 0u;
    if (!qry_member_blob(ASSEMBLE_CODE, &bytes, &size, NULL) || (size < (sizeof(call) + sizeof(count))))
    {
        return codes;
    }
    memcpy(&call, bytes, sizeof(call));
    memcpy(&count, bytes + sizeof(call), sizeof(count));
    if ((call != s_call) || (count != texts.size()))
    {
        printf("klq_identity: the vendor's writer handed call %llu of %u texts for call %llu of %zu\n", call, count,
               s_call, texts.size());
        return codes;
    }
    unsigned long long at = sizeof(call) + sizeof(count);
    for (size_t text = 0u; (text < texts.size()) && ((at + sizeof(count)) <= size); text += 1u)
    {
        unsigned int instructions = 0u;
        memcpy(&instructions, bytes + at, sizeof(instructions));
        at += sizeof(instructions);
        const unsigned long long length = 16ull * instructions;
        if ((at + length) > size)
        {
            break;
        }
        codes[text].assign(bytes + at, bytes + at + length);
        at += length;
    }
    return codes;
}

std::vector<unsigned char> vendor_assembled(const char *machine, const std::string &text)
{
    return vendor_assembled(machine, std::vector<std::string>{text})[0];
}
