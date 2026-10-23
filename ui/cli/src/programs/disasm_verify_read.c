// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify_read.c: the texts the verification reads: the functions an IR text defines, the program's symbol of a
// function as llvm-nm lists it, the instructions of a function as llvm-objdump prints them and as the plugin ghidra's
// script writes them, and whether two readings give the same instructions, the two readers' spellings of one
// instruction taken as one
#include "disasm_verify.h"

// the end of the line of `text` that holds `at`: its newline, or the text's end
static unsigned long long read_line_end(const char *text, unsigned long long size, unsigned long long at)
{
    unsigned long long end = at;
    while ((end < size) && (text[end] != '\n'))
    {
        end += 1ull;
    }
    return end;
}

// whether `letter` is one of a symbol's name as IR and the linker spell one
static int read_named(char letter)
{
    const int lower = (letter >= 'a') && (letter <= 'z');
    const int upper = (letter >= 'A') && (letter <= 'Z');
    const int digit = (letter >= '0') && (letter <= '9');
    return lower || upper || digit || (letter == '_') || (letter == '.') || (letter == '$');
}

// `length` bytes of `text` copied into `into`, which holds `room`: 1, or 0 where they do not fit
static int read_copied(char *into, unsigned long long room, const char *text, unsigned long long length)
{
    if (length >= room)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = into, .src = text, .bytes = length});
    into[length] = '\0';
    return 1;
}

unsigned int cli_disasm_functions_defined(const char *ir, unsigned long long size, char (*names)[CLI_NAME_LONGEST],
                                          unsigned int room)
{
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long end = read_line_end(ir, size, at);
        const int defines = ((end - at) > 7ull) &&
                            cellul.starts(&(CatenaFinitaCfg){.src = ir + at, .other = "define ", .cap = end - at});
        unsigned long long name = at;
        while (defines && (name < end) && (ir[name] != '@'))
        {
            name += 1ull;
        }
        if (defines && (name < end))
        {
            name += 1ull;
            unsigned long long name_end = name;
            while ((name_end < end) && read_named(ir[name_end]))
            {
                name_end += 1ull;
            }
            if ((name_end > name) && (count < room) &&
                read_copied(names[count], CLI_NAME_LONGEST, ir + name, name_end - name))
            {
                count += 1u;
            }
        }
        at = end + 1ull;
    }
    return count;
}

int cli_disasm_symbol_of(const char *text, unsigned long long size, const char *name, char *symbol)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_NAME_LONGEST});
    int promoted = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long end = read_line_end(text, size, at);
        // the line's last word is the symbol: an address, a kind letter and the name, each apart by one space
        unsigned long long word = end;
        while ((word > at) && (text[word - 1ull] != ' '))
        {
            word -= 1ull;
        }
        const unsigned long long found = end - word;
        const int same =
            (found == length) && cellul.starts(&(CatenaFinitaCfg){.src = text + word, .other = name, .cap = found});
        if (same)
        {
            return read_copied(symbol, CLI_NAME_LONGEST, text + word, found);
        }
        const int suffixed =
            !promoted && (found > length + 6ull) &&
            cellul.starts(&(CatenaFinitaCfg){.src = text + word, .other = name, .cap = found}) &&
            cellul.starts(&(CatenaFinitaCfg){.src = text + word + length, .other = ".llvm.", .cap = found - length});
        if (suffixed)
        {
            promoted = read_copied(symbol, CLI_NAME_LONGEST, text + word, found);
        }
        at = end + 1ull;
    }
    return promoted;
}

// the hex digits at `text` from `at` read as a number into `value`, `at` moved past them: 1, or 0 where none stand
static int read_hex(const char *text, unsigned long long end, unsigned long long *at, unsigned long long *value)
{
    unsigned long long number = 0ull;
    unsigned long long digits = 0ull;
    while (*at < end)
    {
        const char letter = text[*at];
        unsigned long long digit = 16ull;
        if ((letter >= '0') && (letter <= '9'))
        {
            digit = (unsigned long long)(letter - '0');
        }
        else if ((letter >= 'a') && (letter <= 'f'))
        {
            digit = 10ull + (unsigned long long)(letter - 'a');
        }
        if (digit == 16ull)
        {
            break;
        }
        number = (number << 4u) | digit;
        digits += 1ull;
        *at += 1ull;
    }
    *value = number;
    return digits > 0ull;
}

// the decimal digits at `text` from `at` read as a number into `value`, `at` moved past them: 1, or 0 where none stand
static int read_decimal(const char *text, unsigned long long end, unsigned long long *at, unsigned long long *value)
{
    unsigned long long number = 0ull;
    unsigned long long digits = 0ull;
    while ((*at < end) && (text[*at] >= '0') && (text[*at] <= '9'))
    {
        number = (number * 10ull) + (unsigned long long)(text[*at] - '0');
        digits += 1ull;
        *at += 1ull;
    }
    *value = number;
    return digits > 0ull;
}

// `at` moved past the spaces and tabs at `text`
static void read_blanks(const char *text, unsigned long long end, unsigned long long *at)
{
    while ((*at < end) && ((text[*at] == ' ') || (text[*at] == '\t')))
    {
        *at += 1ull;
    }
}

// the word at `text` from `at`, up to a blank or the end, its length; `at` moved past it
static unsigned long long read_word(const char *text, unsigned long long end, unsigned long long *at)
{
    const unsigned long long from = *at;
    while ((*at < end) && (text[*at] != ' ') && (text[*at] != '\t') && (text[*at] != '\r'))
    {
        *at += 1ull;
    }
    return *at - from;
}

// whether the word of `length` at `word` is a prefix llvm-objdump prints as a word of its own ahead of the mnemonic
static int read_prefix(const char *word, unsigned long long length)
{
    static const char *const prefixes[] = {"lock",   "rep", "repe", "repz",    "repne", "repnz",
                                           "data16", "cs",  "ds",   "notrack", "bnd"};
    for (unsigned int each = 0u; each < (sizeof(prefixes) / sizeof(prefixes[0])); each += 1u)
    {
        const unsigned long long known = cellul.len(&(CatenaFinitaCfg){.src = prefixes[each], .cap = CLI_NAME_LONGEST});
        if ((known == length) && cellul.starts(&(CatenaFinitaCfg){.src = word, .other = prefixes[each], .cap = length}))
        {
            return 1;
        }
    }
    return 0;
}

// a unit put at the reading's end, its bytes zero and its length 0 for the reader to fill: the unit, or NULL with the
// reading marked cut where its cell is full
static CliDisasmInstruction *read_put(CliDisasmReading *reading, unsigned long long address, const char *mnemonic,
                                      unsigned long long length)
{
    if (reading->count >= CLI_DISASM_INSTRUCTIONS)
    {
        reading->cut = 1;
        return NULL;
    }
    CliDisasmInstruction *const one = &reading->instruction[reading->count];
    one->address = address;
    one->length = 0u;
    memor.set(&(MemoriaCfg){.dst = one->byte, .val = 0, .bytes = CLI_DISASM_BYTES});
    const unsigned long long kept = (length < (CLI_DISASM_MNEMONIC - 1u)) ? length : (CLI_DISASM_MNEMONIC - 1u);
    memor.cpy(&(MemoriaCfg){.dst = one->mnemonic, .src = mnemonic, .bytes = kept});
    one->mnemonic[kept] = '\0';
    reading->count += 1u;
    return one;
}

// two hex digits at `text` from `at` read as one byte put at the unit's end: 1 with `at` moved past them, or 0 where
// two digits do not stand there or the unit holds CLI_DISASM_BYTES already
static int read_pair(const char *text, unsigned long long end, unsigned long long *at, CliDisasmInstruction *unit)
{
    unsigned long long cursor = *at;
    unsigned long long value = 0ull;
    if ((*at + 2ull > end) || (unit->length >= CLI_DISASM_BYTES) || !read_hex(text, *at + 2ull, &cursor, &value) ||
        (cursor != *at + 2ull))
    {
        return 0;
    }
    // two hex digits bound the value to one byte
    unit->byte[unit->length] = (unsigned char)value;
    unit->length += 1u;
    *at = cursor;
    return 1;
}

int cli_disasm_objdump_read(const char *text, unsigned long long size, const char *symbol, CliDisasmReading *reading)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = symbol, .cap = CLI_NAME_LONGEST});
    reading->count = 0u;
    reading->cut = 0;
    int inside = 0;
    int found = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long end = read_line_end(text, size, at);
        unsigned long long cursor = at;
        unsigned long long address = 0ull;
        // a function's header: its address, then its symbol between < and >, then a colon
        if ((at < end) && (text[at] != ' ') && read_hex(text, end, &cursor, &address) && (cursor < end) &&
            (text[cursor] == ' '))
        {
            cursor += 2ull;
            const unsigned long long name_end = cursor + length;
            inside = (name_end + 1ull < end) && (text[name_end] == '>') &&
                     cellul.starts(&(CatenaFinitaCfg){.src = text + cursor, .other = symbol, .cap = length});
            found = found || inside;
        }
        else if (inside)
        {
            read_blanks(text, end, &cursor);
            if (read_hex(text, end, &cursor, &address) && (cursor < end) && (text[cursor] == ':'))
            {
                cursor += 1ull;
                read_blanks(text, end, &cursor);
                // the instruction's bytes, hex pairs a space apart, then blanks, then its mnemonic; a pair is a byte
                // only where a blank or the end follows it, so a two-letter mnemonic is not read as one
                CliDisasmInstruction bytes = {0ull, 0u, {0u}, {'\0'}};
                while (read_pair(text, end, &cursor, &bytes) &&
                       ((cursor == end) || (text[cursor] == ' ') || (text[cursor] == '\t')))
                {
                    if ((cursor < end) && (text[cursor] == ' ') && (cursor + 1ull < end) &&
                        (text[cursor + 1ull] != ' '))
                    {
                        cursor += 1ull;
                    }
                    else
                    {
                        break;
                    }
                }
                read_blanks(text, end, &cursor);
                unsigned long long word = cursor;
                unsigned long long word_length = read_word(text, end, &cursor);
                if (read_prefix(text + word, word_length))
                {
                    read_blanks(text, end, &cursor);
                    word = cursor;
                    word_length = read_word(text, end, &cursor);
                }
                if (word_length > 0ull)
                {
                    CliDisasmInstruction *const unit = read_put(reading, address, text + word, word_length);
                    if (unit == NULL)
                    {
                        return found;
                    }
                    unit->length = bytes.length;
                    memor.cpy(&(MemoriaCfg){.dst = unit->byte, .src = bytes.byte, .bytes = CLI_DISASM_BYTES});
                }
            }
        }
        at = end + 1ull;
    }
    return found;
}

int cli_disasm_ghidra_read(const char *text, unsigned long long size, const char *symbol, CliDisasmReading *reading)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = symbol, .cap = CLI_NAME_LONGEST});
    reading->count = 0u;
    reading->cut = 0;
    int inside = 0;
    int found = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long end = read_line_end(text, size, at);
        if (((end - at) > 3ull) && cellul.starts(&(CatenaFinitaCfg){.src = text + at, .other = "== ", .cap = end - at}))
        {
            const unsigned long long name = at + 3ull;
            const unsigned long long name_end = (end > name) && (text[end - 1ull] == '\r') ? end - 1ull : end;
            inside = ((name_end - name) == length) &&
                     cellul.starts(&(CatenaFinitaCfg){.src = text + name, .other = symbol, .cap = length});
            found = found || inside;
        }
        else if (inside)
        {
            unsigned long long cursor = at;
            unsigned long long address = 0ull;
            unsigned long long bytes = 0ull;
            if (read_decimal(text, end, &cursor, &address) && (cursor < end) && (text[cursor] == ' '))
            {
                cursor += 1ull;
                if (read_decimal(text, end, &cursor, &bytes) && (cursor < end) && (text[cursor] == ' '))
                {
                    cursor += 1ull;
                    const unsigned long long word = cursor;
                    const unsigned long long word_length = read_word(text, end, &cursor);
                    // a raw run is "?" then its bytes as one hex string, read as a unit with no mnemonic
                    const int raw = (word_length == 1ull) && (text[word] == '?');
                    CliDisasmInstruction *const unit =
                        (word_length > 0ull) ? read_put(reading, address, text + word, raw ? 0ull : word_length) : NULL;
                    if ((word_length > 0ull) && (unit == NULL))
                    {
                        return found;
                    }
                    if (unit != NULL)
                    {
                        read_blanks(text, end, &cursor);
                        while (raw && read_pair(text, end, &cursor, unit))
                        {
                        }
                        // the length the row states, where the row is an instruction's and carries no bytes
                        unit->length = raw ? unit->length : (unsigned int)bytes;
                    }
                }
            }
        }
        at = end + 1ull;
    }
    return found;
}

// One instruction's two spellings: LLVM's and Ghidra's, where they differ in letters alone
typedef struct
{
    const char *llvm;
    const char *ghidra;
} CliDisasmSpelling;

static const CliDisasmSpelling s_spellings[] = {
    {"je", "jz"},         {"jne", "jnz"},       {"jae", "jnc"},     {"jb", "jc"},     {"cmove", "cmovz"},
    {"cmovne", "cmovnz"}, {"cmovae", "cmovnc"}, {"cmovb", "cmovc"}, {"sete", "setz"}, {"setne", "setnz"},
    {"setae", "setnc"},   {"setb", "setc"},     {"movabs", "mov"},  {"retq", "ret"},  {"callq", "call"},
    {"jmpq", "jmp"},      {"nopw", "nop"},      {"nopl", "nop"},    {"ud2a", "ud2"},  {"leaq", "lea"},
};

// `mnemonic` as one spelling: in lower case, a suffix Ghidra adds for a prefix taken off, and LLVM's spelling of an
// instruction Ghidra spells otherwise turned into Ghidra's; into `out`, which holds CLI_DISASM_MNEMONIC
static void read_one_spelling(const char *mnemonic, char *out)
{
    unsigned long long length = 0ull;
    while ((mnemonic[length] != '\0') && (mnemonic[length] != '.') && (length < (CLI_DISASM_MNEMONIC - 1u)))
    {
        const char letter = mnemonic[length];
        out[length] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + ('a' - 'A')) : letter;
        length += 1ull;
    }
    out[length] = '\0';
    for (unsigned int each = 0u; each < (sizeof(s_spellings) / sizeof(s_spellings[0])); each += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = s_spellings[each].llvm, .cap = CLI_DISASM_MNEMONIC}))
        {
            const unsigned long long known =
                cellul.len(&(CatenaFinitaCfg){.src = s_spellings[each].ghidra, .cap = CLI_DISASM_MNEMONIC}) + 1ull;
            memor.cpy(&(MemoriaCfg){.dst = out, .src = s_spellings[each].ghidra, .bytes = known});
            return;
        }
    }
}

// whether `right` is a raw run: bytes the reader did not disassemble, given with no mnemonic
static int read_raw(const CliDisasmInstruction *right)
{
    return right->mnemonic[0] == '\0';
}

// the bytes of a raw run `right` from `raw_offset` read against the bytes of the instruction `left` from `left_offset`,
// as many as both hold from there: how many are the same from the same address, or 0 where the addresses differ or a
// byte does, or where either has none left
static unsigned int read_raw_against(const CliDisasmInstruction *left, unsigned int left_offset,
                                     const CliDisasmInstruction *right, unsigned int raw_offset)
{
    if ((left->address + left_offset) != (right->address + raw_offset))
    {
        return 0u;
    }
    const unsigned int left_room = (left->length > left_offset) ? left->length - left_offset : 0u;
    const unsigned int right_room = (right->length > raw_offset) ? right->length - raw_offset : 0u;
    const unsigned int room = (left_room < right_room) ? left_room : right_room;
    for (unsigned int each = 0u; each < room; each += 1u)
    {
        if (left->byte[left_offset + each] != right->byte[raw_offset + each])
        {
            return 0u;
        }
    }
    return room;
}

// whether two instructions are one: the same address, the same length where both readers gave one, and mnemonics of
// one spelling
static int read_one_instruction(const CliDisasmInstruction *left, const CliDisasmInstruction *right)
{
    char left_spelled[CLI_DISASM_MNEMONIC];
    char right_spelled[CLI_DISASM_MNEMONIC];
    read_one_spelling(left->mnemonic, left_spelled);
    read_one_spelling(right->mnemonic, right_spelled);
    const int sized = (left->length == 0u) || (right->length == 0u) || (left->length == right->length);
    return (left->address == right->address) && sized &&
           cellul.eq(&(CatenaFinitaCfg){.src = left_spelled, .other = right_spelled, .cap = CLI_DISASM_MNEMONIC});
}

int cli_disasm_same(const CliDisasmReading *one, const CliDisasmReading *other, unsigned int *at,
                    unsigned int *other_at)
{
    unsigned int first = 0u;
    unsigned int second = 0u;
    unsigned int left_offset = 0u;
    unsigned int raw_offset = 0u;
    while ((first < one->count) && (second < other->count))
    {
        const CliDisasmInstruction *const left = &one->instruction[first];
        const CliDisasmInstruction *const right = &other->instruction[second];
        if (read_raw(right))
        {
            const unsigned int same = read_raw_against(left, left_offset, right, raw_offset);
            if (same == 0u)
            {
                break;
            }
            left_offset += same;
            raw_offset += same;
            first += (left_offset == left->length) ? 1u : 0u;
            left_offset = (left_offset == left->length) ? 0u : left_offset;
            second += (raw_offset == right->length) ? 1u : 0u;
            raw_offset = (raw_offset == right->length) ? 0u : raw_offset;
        }
        else if ((left_offset == 0u) && (raw_offset == 0u) && read_one_instruction(left, right))
        {
            first += 1u;
            second += 1u;
        }
        else
        {
            break;
        }
    }
    *at = first;
    *other_at = second;
    if ((first < one->count) || ((first == 0u) && (other->count > 0u)))
    {
        return 0;
    }
    // what Ghidra gives past the end of the other reading is raw alone: bytes that reading made no claim on
    for (unsigned int each = second; each < other->count; each += 1u)
    {
        if (!read_raw(&other->instruction[each]))
        {
            return 0;
        }
    }
    return 1;
}
