// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify_read.c: the texts the verification reads: the functions an IR text defines, the program's symbol of a
// function as llvm-nm lists it, the instructions of a function as llvm-objdump prints them and as the plugin ghidra's
// script writes them, and whether two readings give the same instructions, the two readers' spellings of one
// instruction taken as one
#include "../text/text.h"
#include "disasm_verify.h"

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at_value;
    char letter;
    char *into;
    unsigned long long capacity;
    unsigned long long length;
    unsigned long long end;
    unsigned long long *at;
    unsigned long long *value;
    const char *word_text;
    CliDisasmReadState *read_state;
    unsigned long long address;
    const char *mnemonic;
    CliDisasmInstruction *unit;
    char *out;
    const CliDisasmInstruction *right;
    const CliDisasmInstruction *left;
    unsigned int left_offset;
    unsigned int raw_offset;
} DisasmVerifyReadCtx;

// the end of the line of `text` that contains `at`: its newline, or the text's end
EMBED_INLINE unsigned long long read_line_end(const DisasmVerifyReadCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = args->at_value;
    unsigned long long end = at;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end, .size = size,
                     .set = "\n");
    return end;
}

// whether `letter` is one of a symbol's name as IR and the linker spell one
EMBED_INLINE int read_by_name(const DisasmVerifyReadCtx *args)
{
    const int lower = (args->letter >= 'a') && (args->letter <= 'z');
    const int upper = (args->letter >= 'A') && (args->letter <= 'Z');
    const int digit = (args->letter >= '0') && (args->letter <= '9');
    return lower || upper || digit || (args->letter == '_') || (args->letter == '.') || (args->letter == '$');
}

// `length` bytes of `text` copied into `into`, sized for `capacity`: 1, or 0 where they do not fit
EMBED_INLINE int read_copy(const DisasmVerifyReadCtx *args)
{
    if (args->length >= args->capacity)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->into, .src = args->text, .bytes = args->length});
    args->into[args->length] = '\0';
    return 1;
}

unsigned int cli_disasm_functions_define(const CliDisasmVerifyIrSizeCfg *args)
{
    unsigned long long size = args->size;
    unsigned int capacity = args->capacity;
    unsigned int count = 0u;
    for (unsigned long long at = 0ULL; at < size;)
    {
        const unsigned long long end =
            EMBED_CALL(read_line_end, DisasmVerifyReadCtx, .text = args->ir, .size = size, .at_value = at);
        const int defines =
            ((end - at) > 7ULL) &&
            cellul.starts(&(CatenaFinitaCfg){.src = args->ir + at, .other = "define ", .cap = end - at});
        unsigned long long name = at;
        if (defines)
        {
            name = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->ir, .at = name,
                              .size = end, .set = "@");
        }
        if (defines && (name < end))
        {
            name += 1ULL;
            unsigned long long name_end = name;
            while ((name_end < end) && EMBED_CALL(read_by_name, DisasmVerifyReadCtx, .letter = args->ir[name_end]))
            {
                name_end += 1ULL;
            }
            if ((name_end > name) && (count < capacity) &&
                EMBED_CALL(read_copy, DisasmVerifyReadCtx, .into = args->names[count], .capacity = CLI_NAME_MAX_LEN,
                           .text = args->ir + name, .length = name_end - name))
            {
                count += 1u;
            }
        }
        at = end + 1ULL;
    }
    return count;
}

int cli_disasm_symbol_get(const CliDisasmVerifySizeTextCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_NAME_MAX_LEN});
    int promoted = 0;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned long long end =
            EMBED_CALL(read_line_end, DisasmVerifyReadCtx, .text = args->text, .size = args->size, .at_value = at);
        // the line's last word is the symbol: an address, a kind letter and the name, each apart by one space
        unsigned long long word_text = end;
        word_text = (at + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                     .text = args->text + at, .at = word_text - at, .set = " "));
        const unsigned long long found = end - word_text;
        const int same =
            (found == length) &&
            cellul.starts(&(CatenaFinitaCfg){.src = args->text + word_text, .other = args->name, .cap = found});
        if (same)
        {
            return EMBED_CALL(read_copy, DisasmVerifyReadCtx, .into = args->symbol_out, .capacity = CLI_NAME_MAX_LEN,
                              .text = args->text + word_text, .length = found);
        }
        const int suffixed =
            !promoted && (found > length + 6ULL) &&
            cellul.starts(&(CatenaFinitaCfg){.src = args->text + word_text, .other = args->name, .cap = found}) &&
            cellul.starts(
                &(CatenaFinitaCfg){.src = args->text + word_text + length, .other = ".llvm.", .cap = found - length});
        if (suffixed)
        {
            promoted = EMBED_CALL(read_copy, DisasmVerifyReadCtx, .into = args->symbol_out,
                                  .capacity = CLI_NAME_MAX_LEN, .text = args->text + word_text, .length = found);
        }
        at = end + 1ULL;
    }
    return promoted;
}

// the hex digits at `text` from `at` read as a number into `value`, `at` moved past them: 1, or 0 where none stand
EMBED_INLINE int read_hex(const DisasmVerifyReadCtx *args)
{
    unsigned long long *at = args->at;
    unsigned long long *value = args->value;
    unsigned long long number = 0ULL;
    unsigned long long digits = 0ULL;
    while (*at < args->end)
    {
        const char letter = args->text[*at];
        unsigned long long digit = 16ULL;
        if ((letter >= '0') && (letter <= '9'))
        {
            digit = (unsigned long long)(letter - '0');
        }
        else if ((letter >= 'a') && (letter <= 'f'))
        {
            digit = 10ULL + (unsigned long long)(letter - 'a');
        }
        if (digit == 16ULL)
        {
            break;
        }
        number = (number << 4u) | digit;
        digits += 1ULL;
        *at += 1ULL;
    }
    *value = number;
    return digits > 0ULL;
}

// the decimal digits at `text` from `at` read as a number into `value`, `at` moved past them: 1, or 0 where none stand
EMBED_INLINE int read_decimal(const DisasmVerifyReadCtx *args)
{
    const char *text = args->text;
    unsigned long long end = args->end;
    unsigned long long *at = args->at;
    unsigned long long *value = args->value;
    unsigned long long number = 0ULL;
    unsigned long long digits = 0ULL;
    unsigned long long stop = *at;
    number = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = text, .at = *at, .size = end, .end = &stop);
    digits = stop - *at;
    *at = stop;
    *value = number;
    return digits > 0ULL;
}

// `at` moved past the spaces and tabs at `text`
EMBED_INLINE void read_blanks(const DisasmVerifyReadCtx *args)
{
    const char *text = args->text;
    unsigned long long *at = args->at;
    *at = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = *at,
                     .size = args->end, .set = " \t");
}

// the word at `text` from `at`, up to a blank or the end, its length; `at` moved past it
EMBED_INLINE unsigned long long read_word(const DisasmVerifyReadCtx *args)
{
    const char *text = args->text;
    unsigned long long *at = args->at;
    const unsigned long long from = *at;
    *at = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = *at,
                     .size = args->end, .set = " \t\r");
    return *at - from;
}

// whether the word of `length` at `word` is a prefix llvm-objdump prints as a word of its own ahead of the mnemonic
EMBED_INLINE int read_prefix(const DisasmVerifyReadCtx *args)
{
    static const char *const prefixes[] = {"lock",   "rep", "repe", "repz",    "repne", "repnz",
                                           "data16", "cs",  "ds",   "notrack", "bnd"};
    for (unsigned int each = 0u; each < (sizeof(prefixes) / sizeof(prefixes[0])); each += 1u)
    {
        const unsigned long long known = cellul.len(&(CatenaFinitaCfg){.src = prefixes[each], .cap = CLI_NAME_MAX_LEN});
        if ((known == args->length) &&
            cellul.starts(&(CatenaFinitaCfg){.src = args->word_text, .other = prefixes[each], .cap = args->length}))
        {
            return 1;
        }
    }
    return 0;
}

// a unit put at the reading's end, its bytes zero and its length 0 for the reader to fill: the unit, or NULL with the
// reading marked cut where its cell is full
EMBED_INLINE CliDisasmInstruction *read_append(const DisasmVerifyReadCtx *args)
{
    unsigned long long address = args->address;
    unsigned long long length = args->length;
    if (args->read_state->count >= CLI_DISASM_INSTRUCTIONS)
    {
        args->read_state->cut = 1;
        return NULL;
    }
    CliDisasmInstruction *const one = &args->read_state->instruction[args->read_state->count];
    one->address = address;
    one->length = 0u;
    memor.set(&(MemoriaCfg){.dst = one->byte, .val = 0, .bytes = CLI_DISASM_BYTES});
    const unsigned long long copied_len = (length < (CLI_DISASM_MNEMONIC - 1u)) ? length : (CLI_DISASM_MNEMONIC - 1u);
    memor.cpy(&(MemoriaCfg){.dst = one->mnemonic, .src = args->mnemonic, .bytes = copied_len});
    one->mnemonic[copied_len] = '\0';
    args->read_state->count += 1u;
    return one;
}

// two hex digits at `text` from `at` read as one byte put at the unit's end: 1 with `at` moved past them, or 0 where
// two digits do not stand there or the unit contains CLI_DISASM_BYTES already
EMBED_INLINE int read_pair(const DisasmVerifyReadCtx *args)
{
    unsigned long long *at = args->at;
    unsigned long long cursor = *at;
    unsigned long long value = 0ULL;
    if ((*at + 2ULL > args->end) || (args->unit->length >= CLI_DISASM_BYTES) ||
        !EMBED_CALL(read_hex, DisasmVerifyReadCtx, .text = args->text, .end = *at + 2ULL, .at = &cursor,
                    .value = &value) ||
        (cursor != *at + 2ULL))
    {
        return 0;
    }
    // two hex digits bound the value to one byte
    args->unit->byte[args->unit->length] = (unsigned char)value;
    args->unit->length += 1u;
    *at = cursor;
    return 1;
}

int cli_disasm_objdump_read(const CliDisasmVerifySizeCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->symbol, .cap = CLI_NAME_MAX_LEN});
    args->read_state->count = 0u;
    args->read_state->cut = 0;
    int inside = 0;
    int found = 0;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned long long end =
            EMBED_CALL(read_line_end, DisasmVerifyReadCtx, .text = args->text, .size = args->size, .at_value = at);
        unsigned long long cursor = at;
        unsigned long long address = 0ULL;
        // a function's header: its address, then its symbol between < and >, then a colon
        if ((at < end) && (args->text[at] != ' ') &&
            EMBED_CALL(read_hex, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor,
                       .value = &address) &&
            (cursor < end) && (args->text[cursor] == ' '))
        {
            cursor += 2ULL;
            const unsigned long long name_end = cursor + length;
            inside =
                (name_end + 1ULL < end) && (args->text[name_end] == '>') &&
                cellul.starts(&(CatenaFinitaCfg){.src = args->text + cursor, .other = args->symbol, .cap = length});
            found = found || inside;
        }
        else if (inside)
        {
            EMBED_CALL(read_blanks, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
            if (EMBED_CALL(read_hex, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor,
                           .value = &address) &&
                (cursor < end) && (args->text[cursor] == ':'))
            {
                cursor += 1ULL;
                EMBED_CALL(read_blanks, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
                // the instruction's bytes, hex pairs a space apart, then blanks, then its mnemonic; a pair is a byte
                // only where a blank or the end follows it, so a two-letter mnemonic is not read as one
                CliDisasmInstruction bytes = {0ULL, 0u, {0u}, {'\0'}};
                while (EMBED_CALL(read_pair, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor,
                                  .unit = &bytes) &&
                       ((cursor == end) || (args->text[cursor] == ' ') || (args->text[cursor] == '\t')))
                {
                    if ((cursor < end) && (args->text[cursor] == ' ') && (cursor + 1ULL < end) &&
                        (args->text[cursor + 1ULL] != ' '))
                    {
                        cursor += 1ULL;
                    }
                    else
                    {
                        break;
                    }
                }
                EMBED_CALL(read_blanks, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
                unsigned long long word_text = cursor;
                unsigned long long word_length =
                    EMBED_CALL(read_word, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
                if (EMBED_CALL(read_prefix, DisasmVerifyReadCtx, .word_text = args->text + word_text,
                               .length = word_length))
                {
                    EMBED_CALL(read_blanks, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
                    word_text = cursor;
                    word_length =
                        EMBED_CALL(read_word, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
                }
                if (word_length > 0ULL)
                {
                    CliDisasmInstruction *const unit =
                        EMBED_CALL(read_append, DisasmVerifyReadCtx, .read_state = args->read_state, .address = address,
                                   .mnemonic = args->text + word_text, .length = word_length);
                    if (unit == NULL)
                    {
                        return found;
                    }
                    unit->length = bytes.length;
                    memor.cpy(&(MemoriaCfg){.dst = unit->byte, .src = bytes.byte, .bytes = CLI_DISASM_BYTES});
                }
            }
        }
        at = end + 1ULL;
    }
    return found;
}

int cli_disasm_ghidra_read(const CliDisasmVerifySizeCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->symbol, .cap = CLI_NAME_MAX_LEN});
    args->read_state->count = 0u;
    args->read_state->cut = 0;
    int inside = 0;
    int found = 0;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned long long end =
            EMBED_CALL(read_line_end, DisasmVerifyReadCtx, .text = args->text, .size = args->size, .at_value = at);
        if (((end - at) > 3ULL) &&
            cellul.starts(&(CatenaFinitaCfg){.src = args->text + at, .other = "== ", .cap = end - at}))
        {
            const unsigned long long name = at + 3ULL;
            const unsigned long long name_end = (end > name) && (args->text[end - 1ULL] == '\r') ? end - 1ULL : end;
            inside = ((name_end - name) == length) &&
                     cellul.starts(&(CatenaFinitaCfg){.src = args->text + name, .other = args->symbol, .cap = length});
            found = found || inside;
        }
        else if (inside)
        {
            unsigned long long cursor = at;
            unsigned long long address = 0ULL;
            unsigned long long bytes = 0ULL;
            if (EMBED_CALL(read_decimal, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor,
                           .value = &address) &&
                (cursor < end) && (args->text[cursor] == ' '))
            {
                cursor += 1ULL;
                if (EMBED_CALL(read_decimal, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor,
                               .value = &bytes) &&
                    (cursor < end) && (args->text[cursor] == ' '))
                {
                    cursor += 1ULL;
                    const unsigned long long word_text = cursor;
                    const unsigned long long word_length =
                        EMBED_CALL(read_word, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
                    // a raw run is "?" then its bytes as one hex string, read as a unit with no mnemonic
                    const int raw = (word_length == 1ULL) && (args->text[word_text] == '?');
                    CliDisasmInstruction *unit;
                    if (word_length > 0ULL)
                    {
                        unit = EMBED_CALL(read_append, DisasmVerifyReadCtx, .read_state = args->read_state,
                                          .address = address, .mnemonic = args->text + word_text,
                                          .length = raw ? 0ULL : word_length);
                    }
                    else
                    {
                        unit = NULL;
                    }
                    if ((word_length > 0ULL) && (unit == NULL))
                    {
                        return found;
                    }
                    if (unit != NULL)
                    {
                        EMBED_CALL(read_blanks, DisasmVerifyReadCtx, .text = args->text, .end = end, .at = &cursor);
                        while (raw && EMBED_CALL(read_pair, DisasmVerifyReadCtx, .text = args->text, .end = end,
                                                 .at = &cursor, .unit = unit))
                        {
                            // each pass reads one pair of the row's bytes
                        }
                        // the length the row states, where the row is an instruction's and carries no bytes
                        unit->length = raw ? unit->length : (unsigned int)bytes;
                    }
                }
            }
        }
        at = end + 1ULL;
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
// instruction Ghidra spells otherwise turned into Ghidra's; into `out`, sized for CLI_DISASM_MNEMONIC
EMBED_INLINE void read_one_spelling(const DisasmVerifyReadCtx *args)
{
    unsigned long long length = 0ULL;
    length =
        EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->mnemonic, .at = 0ULL,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = args->mnemonic, .cap = CLI_CELLS_POOL}), .set = ".");
    length = (length < (CLI_DISASM_MNEMONIC - 1u)) ? length : (CLI_DISASM_MNEMONIC - 1u);
    memor.cpy(&(MemoriaCfg){.dst = args->out, .src = args->mnemonic, .bytes = (size_t)length});
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->out, .size = length);
    args->out[length] = '\0';
    for (unsigned int each = 0u; each < (sizeof(s_spellings) / sizeof(s_spellings[0])); each += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = args->out, .other = s_spellings[each].llvm, .cap = CLI_DISASM_MNEMONIC}))
        {
            const unsigned long long known =
                cellul.len(&(CatenaFinitaCfg){.src = s_spellings[each].ghidra, .cap = CLI_DISASM_MNEMONIC}) + 1ULL;
            memor.cpy(&(MemoriaCfg){.dst = args->out, .src = s_spellings[each].ghidra, .bytes = known});
            return;
        }
    }
}

// whether `right` is a raw run: bytes the reader did not disassemble, given with no mnemonic
EMBED_INLINE int read_raw(const DisasmVerifyReadCtx *args)
{
    return args->right->mnemonic[0] == '\0';
}

// the bytes of a raw run `right` from `raw_offset` read against the bytes of the instruction `left` from `left_offset`,
// as many as both contain from there: how many are the same from the same address, or 0 where the addresses differ or a
// byte does, or where either has none left
EMBED_INLINE unsigned int read_raw_against(const DisasmVerifyReadCtx *args)
{
    if ((args->left->address + args->left_offset) != (args->right->address + args->raw_offset))
    {
        return 0u;
    }
    const unsigned int left_capacity =
        (args->left->length > args->left_offset) ? args->left->length - args->left_offset : 0u;
    const unsigned int right_capacity =
        (args->right->length > args->raw_offset) ? args->right->length - args->raw_offset : 0u;
    const unsigned int capacity = (left_capacity < right_capacity) ? left_capacity : right_capacity;
    for (unsigned int each = 0u; each < capacity; each += 1u)
    {
        if (args->left->byte[args->left_offset + each] != args->right->byte[args->raw_offset + each])
        {
            return 0u;
        }
    }
    return capacity;
}

// whether two instructions are one: the same address, the same length where both readers gave one, and mnemonics of
// one spelling
EMBED_INLINE int read_one_instruction(const DisasmVerifyReadCtx *args)
{
    char left_spelled[CLI_DISASM_MNEMONIC];
    char right_spelled[CLI_DISASM_MNEMONIC];
    EMBED_CALL(read_one_spelling, DisasmVerifyReadCtx, .mnemonic = args->left->mnemonic, .out = left_spelled);
    EMBED_CALL(read_one_spelling, DisasmVerifyReadCtx, .mnemonic = args->right->mnemonic, .out = right_spelled);
    const int sized =
        (args->left->length == 0u) || (args->right->length == 0u) || (args->left->length == args->right->length);
    return (args->left->address == args->right->address) && sized &&
           cellul.eq(&(CatenaFinitaCfg){.src = left_spelled, .other = right_spelled, .cap = CLI_DISASM_MNEMONIC});
}

int cli_disasm_same(const CliDisasmVerifyOneCfg *args)
{
    unsigned int *at = args->at;
    unsigned int *other_at = args->other_at;
    unsigned int first = 0u;
    unsigned int second = 0u;
    unsigned int left_offset = 0u;
    unsigned int raw_offset = 0u;
    int going = 1;
    while (going && ((first < args->one->count) && (second < args->other->count)))
    {
        const CliDisasmInstruction *const left = &args->one->instruction[first];
        const CliDisasmInstruction *const right = &args->other->instruction[second];
        if (EMBED_CALL(read_raw, DisasmVerifyReadCtx, .right = right))
        {
            const unsigned int same = EMBED_CALL(read_raw_against, DisasmVerifyReadCtx, .left = left,
                                                 .left_offset = left_offset, .right = right, .raw_offset = raw_offset);
            if (same == 0u)
            {
                going = 0;
                continue;
            }
            left_offset += same;
            raw_offset += same;
            first += (left_offset == left->length) ? 1u : 0u;
            left_offset = (left_offset == left->length) ? 0u : left_offset;
            second += (raw_offset == right->length) ? 1u : 0u;
            raw_offset = (raw_offset == right->length) ? 0u : raw_offset;
        }
        else if ((left_offset == 0u) && (raw_offset == 0u) &&
                 EMBED_CALL(read_one_instruction, DisasmVerifyReadCtx, .left = left, .right = right))
        {
            first += 1u;
            second += 1u;
        }
        else
        {
            going = 0;
            continue;
        }
    }
    *at = first;
    *other_at = second;
    if ((first < args->one->count) || ((first == 0u) && (args->other->count > 0u)))
    {
        return 0;
    }
    // what Ghidra gives past the end of the other reading is raw alone: bytes that reading made no claim on
    for (unsigned int each = second; each < args->other->count; each += 1u)
    {
        if (!EMBED_CALL(read_raw, DisasmVerifyReadCtx, .right = &args->other->instruction[each]))
        {
            return 0;
        }
    }
    return 1;
}
