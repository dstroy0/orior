// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.cu: what a form's text states: its tokens, its launches, conditions and instructions, and what its opcode
// controls, compares, operates on or commutes
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "identity_internal.h"

int vector_alike(const IdentityCode *code, const HostCases *host, const unsigned int *places, RunQuestion *question,
                 unsigned long long *queries, unsigned int mark)
{
    const unsigned int max = 255u;
    question->registers = ((mark != 0u) && (mark < max)) ? mark : max;
    const int least_alike = question_alike(code, host, places, question, queries);
    if ((question->outcome == RUN_ANSWERED) || (question->registers == max))
    {
        return least_alike;
    }
    const unsigned int least = question->registers;
    question->registers = max;
    const int max_alike = question_alike(code, host, places, question, queries);
    if (question->outcome != RUN_ANSWERED)
    {
        return max_alike;
    }
    const auto answers_declaring = [&](long long count) -> int {
        // a count of registers is below 2^32
        question->registers = (unsigned int)count;
        (void)question_alike(code, host, places, question, queries);
        return (question->outcome == RUN_ANSWERED) ? 1 : 0;
    };
    // the query at the fewest is in R once the walk put it, and the walk reads it from there. The fewest lies at or
    // below max
    question->registers = (unsigned int)walk_halved(answers_declaring, (long long)max, (long long)least);
    return question_alike(code, host, places, question, queries);
}

// the lines of `text` cut at each line's end, each a piece of `text`, onto `lines`, a PlatformArray of CarrierPiece
static void text_lines(CarrierPiece text, PlatformArray *lines)
{
    size_t cut = 0u;
    CarrierPiece line;
    while (carrier_piece_next(text, '\n', &cut, &line))
    {
        (void)platform_array_appended(lines, &line);
    }
}

// 1 where `piece` holds the terminated `needle`
static int piece_has(CarrierPiece piece, const char *needle)
{
    return EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = piece.text, .cap = piece.length, .other = needle,
                      .other_cap = piece.length, .ci = EMBED_FALSE) != NULL;
}

int carrier_cut_at_read(const CarrierLinks *links, long cases_defined, const char *text, long link_at, long form_lines,
                        const char *read, const char *located, int wide, PlatformText *out)
{
    PlatformArray lines;
    identity_pieces_begun(&lines);
    text_lines(carrier_piece_of(text), &lines);
    long stored_at = -1;
    for (unsigned long long at = 0ULL; at < lines.count; at += 1ULL)
    {
        // a carrier's lines are far below 2^63
        stored_at = piece_has(identity_piece_at(&lines, at), "STG") ? (long)at : stored_at;
    }
    if ((stored_at < (link_at + form_lines)) || (read[0] == '\0') || (read[0] == 'U'))
    {
        platform_array_ended(&lines);
        return 0;
    }
    const CarrierLink store = carrier_link_read(identity_piece_at(&lines, (unsigned long long)stored_at));
    if (store.reads == 0u)
    {
        platform_array_ended(&lines);
        return 0;
    }
    const char *const stored = store.read[store.reads - 1u].text;
    PlatformText cut;
    identity_text_begun(&cut);
    VerbaLinea *const line = &cut.line;
    for (long at = 0; at < link_at; at += 1)
    {
        const CarrierPiece each = identity_piece_at(&lines, (unsigned long long)at);
        (void)mmgr_line_put_n(line, each.text, each.length);
        mmgr_line_ch(line, '\n');
    }
    const size_t located_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = located, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if (located_length != 0u)
    {
        mmgr_line_put(line, located);
        if (located[located_length - 1u] != '\n')
        {
            mmgr_line_ch(line, '\n');
        }
    }
    if (read[0] == 'P')
    {
        mmgr_line_put(line, "\tSEL \t");
        mmgr_line_put(line, stored);
        mmgr_line_put(line, ", RZ, 1, !");
        mmgr_line_put(line, read);
        mmgr_line_put(line, ";\n");
    }
    else
    {
        mmgr_line_put(line, "\tMOV \t");
        mmgr_line_put(line, stored);
        mmgr_line_put(line, ", ");
        mmgr_line_put(line, read);
        mmgr_line_put(line, ";\n");
    }
    mmgr_line_put(line, "\tMOV \t");
    mmgr_line_put(line, stored);
    mmgr_line_put(line, ".hi, ");
    if (wide)
    {
        mmgr_line_put(line, read);
        mmgr_line_put(line, ".hi");
    }
    else
    {
        mmgr_line_put(line, "RZ");
    }
    mmgr_line_put(line, ";\n");
    char stored_high[CARRIER_NAME_BYTES + 4u];
    VerbaLinea high_line = mmgr_line_over(stored_high, sizeof(stored_high));
    mmgr_line_put(&high_line, stored);
    mmgr_line_put(&high_line, ".hi");
    (void)mmgr_line_finish(&high_line);
    int whole = 1;
    for (long at = link_at + form_lines; whole && (at < stored_at); at += 1)
    {
        const CarrierPiece each = identity_piece_at(&lines, (unsigned long long)at);
        if (carrier_form_carries(links, cases_defined, each, at))
        {
            continue;
        }
        // the stored register's high word written by a link of the carrier's own is the store's width, and the cut
        // writes it already
        const CarrierLink own = carrier_link_read(each);
        CarrierName named[CARRIER_READS_MAX];
        const unsigned int named_count = carrier_registers(each, 1, named, CARRIER_READS_MAX);
        if (carrier_is(&own.written, stored) && (named_count != 0u) && carrier_is(&named[0], stored_high))
        {
            continue;
        }
        int reads_stored = 0;
        for (unsigned int read_at = 0u; read_at < own.reads; read_at += 1u)
        {
            reads_stored |= carrier_is(&own.read[read_at], stored) ? 1 : 0;
        }
        if (carrier_is(&own.written, stored) || reads_stored)
        {
            whole = 0;
            continue;
        }
        (void)mmgr_line_put_n(line, each.text, each.length);
        mmgr_line_ch(line, '\n');
    }
    for (unsigned long long at = (unsigned long long)stored_at; whole && (at < lines.count); at += 1ULL)
    {
        const CarrierPiece each = identity_piece_at(&lines, at);
        (void)mmgr_line_put_n(line, each.text, each.length);
        mmgr_line_ch(line, '\n');
    }
    if (whole)
    {
        identity_text_put(out, &cut);
    }
    platform_text_ended(&cut);
    platform_array_ended(&lines);
    return whole;
}

// the pattern of a register a carrier names, which a shape writes as R
static const TablePattern *register_pattern(void)
{
    static TablePatternOnce s_register;
    return table_once(&s_register, "\\b(U?R[0-9]+|P[0-9]+)\\b", 0);
}

void read_shape(const CarrierLinks *links, CarrierPiece chain_text, long cases_defined, CarrierPiece form_text, long at,
                PlatformText *out)
{
    PlatformArray lines;
    identity_pieces_begun(&lines);
    text_lines(chain_text, &lines);
    PlatformText &shape = *out;
    table_replaced(register_pattern(), form_text.text, form_text.length, "R", &shape);
    static thread_local CarrierReads t_reads;
    CarrierReads &reads = t_reads;
    reads.count = 0u;
    carrier_form_reads(links, cases_defined, form_text, at, &reads);
    for (unsigned int read_at = 0u; read_at < reads.count; read_at += 1u)
    {
        const CarrierName read = reads.name[read_at];
        char base[CARRIER_NAME_BYTES];
        VerbaLinea base_line = mmgr_line_over(base, sizeof(base));
        const char *const dot =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = read.text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
        // the dot lies within the name
        (void)mmgr_line_put_n(&base_line, read.text,
                              (dot != NULL) ? (size_t)(dot - read.text)
                                            : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = read.text,
                                                         .cap = PLATFORM_TEXT_TO_TERMINATOR));
        (void)mmgr_line_finish(&base_line);
        const long defined = (read.text[0] == '[') ? -1 : carrier_definition(links, base, at);
        mmgr_line_ch(&shape.line, '|');
        if (defined < 0)
        {
            mmgr_line_put(&shape.line, read.text);
        }
        else
        {
            const CarrierPiece defining = identity_piece_at(&lines, (unsigned long long)defined);
            table_replaced(register_pattern(), defining.text, defining.length, "R", &shape);
        }
    }
    platform_array_ended(&lines);
}

void form_tokens(CarrierPiece text, PlatformArray *tokens)
{
    (void)platform_array_counted(tokens, 0ULL);
    for (size_t at = 0u; at < text.length;)
    {
        const char character = text.text[at];
        if ((character == '\x01') && ((at + 2u) < text.length))
        {
            size_t end = at + 3u;
            if ((end < text.length) && (text.text[end] == '.'))
            {
                end += 1u;
                while ((end < text.length) && (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM,
                                                          .byte = (unsigned char)text.text[end]) ||
                                               (text.text[end] == '_')))
                {
                    end += 1u;
                }
            }
            const CarrierPiece token = {&text.text[at], end - at};
            (void)platform_array_appended(tokens, &token);
            at = end;
            continue;
        }
        if (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_SPACE, .byte = (unsigned char)character))
        {
            at += 1u;
            continue;
        }
        size_t end = at + 1u;
        if (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (unsigned char)character) ||
            (character == '_') || (character == '.'))
        {
            while ((end < text.length) &&
                   (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (unsigned char)text.text[end]) ||
                    (text.text[end] == '_') || (text.text[end] == '.')))
            {
                end += 1u;
            }
        }
        const CarrierPiece token = {&text.text[at], end - at};
        (void)platform_array_appended(tokens, &token);
        at = end;
    }
}

// 1 where two pieces hold the same bytes
static int pieces_same(CarrierPiece one, CarrierPiece other)
{
    return (one.length == other.length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = one.text, .other = other.text, .bytes = one.length) == 0);
}

// the place of a marker's parameter in a binding: its letter's byte
static unsigned char marker_key(CarrierPiece marker)
{
    return (unsigned char)marker.text[1];
}

// `value` bound to the marker `key` in `binding`, where it binds none yet
static void binding_kept(FormBinding *binding, unsigned char key, CarrierPiece value)
{
    if (binding->held[key] == 0u)
    {
        binding->held[key] = 1u;
        binding->count += 1u;
    }
    binding->value[key] = value;
}

int form_unified(const Ruleset *sass, const char *to_text, const char *from_text, FormBinding *bound,
                 FormBinding *fixed)
{
    PlatformArray to_tokens;
    PlatformArray from_tokens;
    identity_pieces_begun(&to_tokens);
    identity_pieces_begun(&from_tokens);
    form_tokens(carrier_piece_of(to_text), &to_tokens);
    form_tokens(carrier_piece_of(from_text), &from_tokens);
    const char *const zero_name = ruleset_fixed_named(sass, "zero");
    const CarrierPiece zero = carrier_piece_of((zero_name != NULL) ? zero_name : "");
    form_binding_cleared(bound);
    form_binding_cleared(fixed);
    int unified = (to_tokens.count != 0ULL) && (to_tokens.count == from_tokens.count);
    for (unsigned long long at = 0ULL; unified && (at < to_tokens.count); at += 1ULL)
    {
        const CarrierPiece to_token = identity_piece_at(&to_tokens, at);
        const CarrierPiece from_token = identity_piece_at(&from_tokens, at);
        const int to_marker = (to_token.length >= 3u) && (to_token.text[0] == '\x01');
        const int from_marker = (from_token.length >= 3u) && (from_token.text[0] == '\x01');
        const CarrierPiece to_part = to_marker ? carrier_after(to_token, 3u) : carrier_after(to_token, to_token.length);
        const CarrierPiece from_part =
            from_marker ? carrier_after(from_token, 3u) : carrier_after(from_token, from_token.length);
        if (to_marker)
        {
            // a part of a pair is against the same part of another, and a whole against a marker or a literal
            const CarrierPiece located = from_marker ? CarrierPiece{from_token.text, 3u} : from_token;
            const int parts_are = from_marker ? pieces_same(to_part, from_part)
                                              : ((to_part.length == 0u) || pieces_same(from_token, zero));
            const unsigned char key = marker_key(to_token);
            if (!parts_are || ((bound->held[key] != 0u) && !pieces_same(located, bound->value[key])))
            {
                unified = 0;
                continue;
            }
            binding_kept(bound, key, located);
            continue;
        }
        if (from_marker)
        {
            const unsigned char key = marker_key(from_token);
            if (((from_part.length != 0u) && !pieces_same(to_token, zero)) ||
                ((fixed->held[key] != 0u) && !pieces_same(to_token, fixed->value[key])))
            {
                unified = 0;
                continue;
            }
            binding_kept(fixed, key, to_token);
            continue;
        }
        unified = pieces_same(to_token, from_token);
    }
    platform_array_ended(&from_tokens);
    platform_array_ended(&to_tokens);
    return unified;
}

int form_launched(const char *text)
{
    static TablePatternOnce s_launch;
    TableMatch found;
    return table_search(table_once(&s_launch, "SR_(TID|CTAID|NTID|NCTAID)|c\\[0x0\\]\\[0x(0|4|8|c|10|14)\\]", 0), text,
                        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR),
                        &found)
               ? 1
               : 0;
}

void form_conditions(const char *text, FormConditions *conditions)
{
    static TablePatternOnce s_condition;
    conditions->count = 0u;
    const CarrierPiece read = carrier_piece_of(text);
    TableWalk walk;
    table_walk_begun(&walk, table_once(&s_condition, "[A-Z]*SETP\\.([A-Z]+)", 0), read.text, read.length);
    while (table_walk_next(&walk))
    {
        size_t length = 0u;
        const char *const condition = table_walk_group(&walk, 1u, &length);
        IdentityName named;
        (void)IDENTITY_SET(named, condition, length);
        int held = 0;
        for (unsigned int at = 0u; at < conditions->count; at += 1u)
        {
            held |= identity_same(conditions->condition[at].text, named.text) ? 1 : 0;
        }
        if (held || (conditions->count >= FORM_CONDITIONS_MAX))
        {
            continue;
        }
        unsigned int place = conditions->count;
        while ((place > 0u) && (identity_order_text(conditions->condition[place - 1u].text, named.text) > 0))
        {
            conditions->condition[place] = conditions->condition[place - 1u];
            place -= 1u;
        }
        conditions->condition[place] = named;
        conditions->count += 1u;
    }
    table_walk_ended(&walk);
}

// the operands an instruction of a form holds at max
#define FORM_OPERANDS_MAX 32u

// an instruction of a form: its opcode with its modifiers, and its operands, each a piece of the form's text
struct FormInstruction
{
    CarrierPiece opcode;
    CarrierPiece operands[FORM_OPERANDS_MAX];
    unsigned int count;
};

// The next instruction of the form `text` from `*cut` into `instruction`, a label and a guard before it passed over:
// 1, or 0 past the last instruction. The read cuts the instructions at each `;`. It reads the operands to the line's
// end after the opcode
static int form_instruction_next(CarrierPiece text, size_t *cut, FormInstruction *instruction)
{
    CarrierPiece line;
    while (carrier_piece_next(text, ';', cut, &line))
    {
        const CarrierPiece words = carrier_trimmed(line);
        size_t at = 0u;
        CarrierPiece opcode = {words.text, 0u};
        int found = identity_word_next(words, &at, &opcode);
        while (found && ((opcode.text[opcode.length - 1u] == ':') || (opcode.text[0] == '@')))
        {
            found = identity_word_next(words, &at, &opcode);
        }
        if (!found || !EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_UPPER, .byte = (unsigned char)opcode.text[0]))
        {
            continue;
        }
        CarrierPiece rest = carrier_after(words, at);
        const char *const line_end =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = rest.text, .cap = rest.length, .byte = '\n');
        // the line's end lies within the rest
        rest.length = (line_end != NULL) ? (size_t)(line_end - rest.text) : rest.length;
        instruction->opcode = opcode;
        instruction->count = 0u;
        size_t operand_cut = 0u;
        CarrierPiece operand;
        while (carrier_piece_next(rest, ',', &operand_cut, &operand))
        {
            if (instruction->count < FORM_OPERANDS_MAX)
            {
                instruction->operands[instruction->count] = carrier_trimmed(operand);
                instruction->count += 1u;
            }
        }
        return 1;
    }
    return 0;
}

// the operation `opcode` names, its word before the first `.`
static CarrierPiece opcode_stem(CarrierPiece opcode)
{
    const char *const dot =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = opcode.text, .cap = opcode.length, .byte = '.');
    // the dot lies within the opcode
    const CarrierPiece stem = {opcode.text, (dot != NULL) ? (size_t)(dot - opcode.text) : opcode.length};
    return stem;
}

// 1 where `pattern`, a whole pattern, matches the stem of `opcode`
static int stem_matched(TablePatternOnce *pattern, const char *source, CarrierPiece opcode)
{
    const CarrierPiece stem = opcode_stem(opcode);
    TableMatch match;
    return table_matched(table_once(pattern, source, 1), stem.text, stem.length, &match);
}

// 1 where `opcode` decides which instruction runs next: an exit, a return, a branch, a call or a convergence
static int opcode_controls(CarrierPiece opcode)
{
    static TablePatternOnce s_control;
    return stem_matched(&s_control, "(EXIT|RET|BRA|BRX|JMP|JMX|CALL|BREAK|BSSY|BSYNC)", opcode);
}

// 1 where `opcode` tests its operands and sets a flag
static int opcode_compares(CarrierPiece opcode)
{
    static TablePatternOnce s_compare;
    return stem_matched(&s_compare, "U?[A-Z]?SETP2?", opcode);
}

// 1 where `opcode` computes a value from its operands at any width, a bit, a word or many words. An add, a multiply,
// a shift, a bitwise or a floating operation. `IMAD.MOV` moves a value and computes nothing
static int opcode_operates(CarrierPiece opcode)
{
    static TablePatternOnce s_operation;
    return stem_matched(&s_operation,
                        "U?(IADD3|IADD|IMAD|IMUL|IMNMX|IABS|ISCADD|LEA|LOP3|LOP|SHF|SHL|SHR|POPC|FLO|"
                        "BREV|BMSK|SGXT|PRMT|BFE|BFI|IDP|VABSDIFF|FADD|FMUL|FFMA|FMNMX|DADD|DMUL|DFMA|"
                        "HADD2|HMUL2|HFMA2|MUFU)",
                        opcode) &&
           !piece_has(opcode, ".MOV");
}

// 1 where `instruction` gives one value with its first two sources taken either way round. An add, a multiply, a
// least or a max, a bitwise table that reads its first two inputs alike, or an equality
static int instruction_commutes(const FormInstruction *instruction)
{
    static TablePatternOnce s_commutes;
    static TablePatternOnce s_lop3;
    static TablePatternOnce s_lop;
    const CarrierPiece opcode = instruction->opcode;
    if (opcode_compares(opcode))
    {
        const char *const dot =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = opcode.text, .cap = opcode.length, .byte = '.');
        if (dot == NULL)
        {
            return 0;
        }
        // the dot lies within the opcode, and the condition runs from past it to the next dot
        const CarrierPiece after = carrier_after(opcode, (size_t)(dot - opcode.text) + 1u);
        const char *const next =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = after.text, .cap = after.length, .byte = '.');
        const CarrierPiece condition = {after.text, (next != NULL) ? (size_t)(next - after.text) : after.length};
        return identity_piece_is(condition, "EQ") || identity_piece_is(condition, "NE");
    }
    if (stem_matched(&s_lop3, "U?LOP3", opcode))
    {
        // the table's bit at `a`, `b`, `c` sits at its place `4a + 2b + c`: the first two inputs read alike where
        // the bits at `a` = 1, `b` = 0 and at `a` = 0, `b` = 1 agree for each `c`
        if ((instruction->count < 5u) || (instruction->operands[4].length < 2u) ||
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = instruction->operands[4].text, .other = "0x", .bytes = 2u) != 0))
        {
            return 0;
        }
        char number[32] = {0};
        const CarrierPiece table_piece = instruction->operands[4];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = number, .src = &table_piece.text[2],
                   .bytes = (table_piece.length - 2u < 31u) ? (table_piece.length - 2u) : 31u);
        const unsigned long long table =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = number, .end = NULL, .base = 16);
        return (((table >> 4u) & 1u) == ((table >> 2u) & 1u)) && (((table >> 5u) & 1u) == ((table >> 3u) & 1u));
    }
    if (stem_matched(&s_lop, "U?LOP", opcode))
    {
        return piece_has(opcode, ".AND") || piece_has(opcode, ".OR") || piece_has(opcode, ".XOR");
    }
    return stem_matched(&s_commutes,
                        "U?(IADD3|IMAD|IMUL|IMNMX|VABSDIFF|FADD|FMUL|FFMA|FMNMX|DADD|DMUL|DFMA|HADD2|HMUL2|HFMA2)",
                        opcode) &&
           !piece_has(opcode, ".MOV");
}

int form_controls(const char *text)
{
    const CarrierPiece form = carrier_piece_of(text);
    int controls = 0;
    size_t cut = 0u;
    FormInstruction instruction;
    while (form_instruction_next(form, &cut, &instruction))
    {
        controls |= opcode_controls(instruction.opcode);
    }
    return controls;
}

int form_addresses(const char *text)
{
    const CarrierPiece form = carrier_piece_of(text);
    size_t cut = 0u;
    FormInstruction instruction;
    while (form_instruction_next(form, &cut, &instruction))
    {
        for (unsigned int at = 0u; at < instruction.count; at += 1u)
        {
            const CarrierPiece operand = instruction.operands[at];
            const char *const bracket =
                EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = operand.text, .cap = operand.length, .byte = '[');
            if ((bracket != NULL) && ((bracket == operand.text) || (bracket[-1] != 'c')))
            {
                return 1;
            }
        }
    }
    return 0;
}

int form_dangerous(const char *text)
{
    return form_controls(text) || form_addresses(text);
}

int form_operates(const char *text)
{
    const CarrierPiece form = carrier_piece_of(text);
    int operates = 0;
    size_t cut = 0u;
    FormInstruction instruction;
    while (form_instruction_next(form, &cut, &instruction))
    {
        operates |= opcode_operates(instruction.opcode);
    }
    return operates;
}

int form_commutes(const char *text)
{
    const CarrierPiece form = carrier_piece_of(text);
    int any = 0;
    int commutes = 1;
    size_t cut = 0u;
    FormInstruction instruction;
    while (form_instruction_next(form, &cut, &instruction))
    {
        any = 1;
        commutes &= instruction_commutes(&instruction);
    }
    return any && commutes;
}

int form_verbs(const char *text)
{
    const CarrierPiece form = carrier_piece_of(text);
    int verbs = 0;
    size_t cut = 0u;
    FormInstruction instruction;
    while (form_instruction_next(form, &cut, &instruction))
    {
        const CarrierPiece opcode = instruction.opcode;
        verbs |= (!opcode_operates(opcode) && !opcode_compares(opcode) && !opcode_controls(opcode)) ? 1 : 0;
    }
    return verbs;
}

int form_const_loaded(const char *first, const char *second)
{
    PlatformArray first_tokens;
    PlatformArray second_tokens;
    identity_pieces_begun(&first_tokens);
    identity_pieces_begun(&second_tokens);
    form_tokens(carrier_piece_of(first), &first_tokens);
    form_tokens(carrier_piece_of(second), &second_tokens);
    int loaded = (first_tokens.count != 0ULL) && (first_tokens.count == second_tokens.count);
    for (unsigned long long at = 1ULL; loaded && (at < first_tokens.count); at += 1ULL)
    {
        loaded = pieces_same(identity_piece_at(&first_tokens, at), identity_piece_at(&second_tokens, at));
    }
    if (loaded)
    {
        const CarrierPiece one = identity_piece_at(&first_tokens, 0ULL);
        const CarrierPiece other = identity_piece_at(&second_tokens, 0ULL);
        const size_t suffix = 9u;
        loaded =
            ((other.length == (one.length + suffix)) &&
             (EMBED_CALL(memor.cmp, MemoriaCfg, .src = other.text, .other = one.text, .bytes = one.length) == 0) &&
             (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &other.text[one.length], .other = ".CONSTANT",
                         .bytes = suffix) == 0)) ||
            ((one.length == (other.length + suffix)) &&
             (EMBED_CALL(memor.cmp, MemoriaCfg, .src = one.text, .other = other.text, .bytes = other.length) == 0) &&
             (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &one.text[other.length], .other = ".CONSTANT",
                         .bytes = suffix) == 0));
    }
    platform_array_ended(&second_tokens);
    platform_array_ended(&first_tokens);
    return loaded;
}

int form_switches(const char *text)
{
    PlatformArray tokens;
    identity_pieces_begun(&tokens);
    form_tokens(carrier_piece_of(text), &tokens);
    int ends = 0;
    for (unsigned long long at = 0ULL; at < tokens.count; at += 1ULL)
    {
        ends |= identity_piece_is(identity_piece_at(&tokens, at), ";") ? 1 : 0;
    }
    const int switches = (tokens.count != 0ULL) && !ends;
    platform_array_ended(&tokens);
    return switches;
}

int form_ranged(const char *text)
{
    PlatformArray tokens;
    identity_pieces_begun(&tokens);
    form_tokens(carrier_piece_of(text), &tokens);
    int ranged = 0;
    for (unsigned long long at = 0ULL; at < tokens.count; at += 1ULL)
    {
        const CarrierPiece token = identity_piece_at(&tokens, at);
        ranged |=
            ((token.length > 3u) && (token.text[0] == '\x01') && identity_piece_is(carrier_after(token, 3u), ".hi"))
                ? 1
                : 0;
    }
    platform_array_ended(&tokens);
    return ranged;
}
