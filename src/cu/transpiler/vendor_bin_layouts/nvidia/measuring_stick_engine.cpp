// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_engine: the engine reads each kernel of the measuring stick form by form through cu.krs. It writes
// each form at once in sass.krs. The two are rulesets of one schema (cu_target.h). The engine assembles the kernel
// against the machine file of the part, and writes its encodings for nvdisasm to read back.
//
//     measuring_stick_engine <stick .cu> <nvcc listing> <out directory>
//
// The engine reads an expression against each form that cu.krs gives as `{to} = <text>;`. It reads a statement against
// each form that cu.krs gives as a statement of its own.
// The text is the pattern. Each parameter past the to of a form takes a value that the kernel stores. It can be a
// number as the bank of immediates of cu.krs writes one, or a parameter of the kernel. It can also be an expression,
// read the same way.
//
// Of the readings that fit, the engine keeps a form only where the sass.krs text for it assembles with its operands.
// Its to is a register, or a predicate where the text writes a predicate there. Of those, the engine takes the reading
// of the fewest forms. An expression read once goes into its register, and later reads take it from there.
//
// The stick has one arity. An operand (T)in[i] is the word at in + 8 . i.
// The value out[thread] = (unsigned long long)r is the wide stored at out + 8 . thread. The code writes out its
// conversion as C converts a signed word.
//
// The engine gates and ranks each of its forms as it does those of an expression. A statement that nothing here reads,
// and a value other than a 32-bit word, are each a question that the kernel puts. Its line in <out>/engine.tsv says
// which.
//
// The folds are the own folds of the system, stored in sass.ksc beside sass.krs. The engine finds each on the compile
// channel. It checks a kernel as written against the listing of nvcc for it:
//
//   - register put for to: a form whose sass.krs text moves one word of the constant bank into its to writes nothing.
//     The word is the operand of each form that reads it. The engine finds this where the listing of nvcc reads the
//     word as the operand of an instruction other than a move.
//   - pow2 put for a parameter: a multiply by a power of two becomes the shift of the stem of the multiply. The shift
//     is by the log2 of the power. The engine finds this where the listing of nvcc holds the operation of the shift
//     with that log2.
//   - offset put for address: in[i + k] loads from the address of in[i], k elements on in the offset of the load.
//     Operands that differ only in k share the address. The engine finds this where the listing of nvcc loads at that
//     offset.
//
// The engine reads the stick in rounds. The first writes each form unfolded. Each later round folds by what sass.ksc
// holds, which can show a fold that the round before hid.
//
// After each round, the engine writes sass.ksc. It drops the old lines of the file for each question that this run
// queried, and takes each fold that this run found.
// The engine writes out the last round: each kernel as <out>/NNNN.sass, and its encodings as <out>/NNNN.bin. Nothing
// goes to a device.
//
// The engine passes the trace of each kernel to the query run that QRY names, as the blob NNNN.trace. It does the same
// for the trace of each path (qry_member.h). With no writer of a run to take it, nothing reads it.
extern "C"
{
#include "../../../engine/engine_note.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "clz/clz.h"
#include "interface_sass_probe.h"
#include "memoria_operor/memoria_operor.h"
#include "sass_assemble.h"
#include "sass_machine.h"
}

#include "../../../../../includes/file_defs/krs/ruleset_core_words.h"
#include "../../../../../includes/file_defs/krs/ruleset_flat.h"
#include "../../../../../includes/file_defs/krs/ruleset_reader.h"
#include "../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../platform/platform.h"
#include "../../../lstar/protocol/table/table_regexp.h"
#include "cu_target.h"
#include "machine_ir_types.h"
#include "sass_target.h"
#include "target.h"
#include "verba_scribo/verba_linea.h"

#if defined(__GNUC__) && !defined(__ELF__)
extern "C" const char __ehdr_start = 0;
#endif

// The pool the engine asks for: the stick and nvcc's listing, each in one cell. Then the tables of the largest kernel,
// each one cell at its count
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 28u;

// the most rounds of the read of the stick
#define STICK_ROUNDS 4u

// the width of a value stored in a predicate
#define STICK_PREDICATE_BITS 1u

// The registers of the part. One stores STICK_REGISTER_BITS. A value wider than one goes in an even register and the
// one past it, with its low word in the even register. A value narrower than a register goes in the low bits of one
#define STICK_REGISTER_BITS 32u
#define STICK_PAIR_BITS 64u
// the width of the count of a shift, `bits` in the rulesets: one register. A value stored in a pair reads as its low
// word. C leaves a count past the width undefined, and the low word holds each count under it
#define STICK_COUNT_BITS 0u

// The bytes of a name, a register or a token, and of an expression or a line, each with its terminator. A text that
// passes its room puts the kernel's question, and the engine never cuts a text short
#define STICK_WORD_BYTES 64u
#define STICK_EXPR_BYTES 512u

// The tokens of one expression, the placeholders of one form, and the parameters of one kernel, each at max
#define STICK_TOKENS_MAX 1024u
#define STICK_PLACEHOLDERS_MAX 32u
#define STICK_PARAMETERS_MAX 16u

// the bytes of the texts a kernel's tables grow to at max: its text, its trace and its register lines
#define STICK_KERNEL_TEXT_MAX (1ULL << 26u)

// the schema's forms by their place in it, as machine_ir_types.h names them
#define STICK_FORM_NAME(name_, text_, parameters_, type_) text_,

static const char *const s_form_names[OPCODE_COUNT] = {OPCODES(STICK_FORM_NAME)};

// the folder of sass.krs, which holds the part's machine file and its .kdm. The run reads a file by its path from the
// top of the tree. So the folder is the folder that the run read sass.krs from
static char s_machines[PLATFORM_PATH_BYTES];

// the part whose machine file and classification the stick reads, as sass.krs names it in its part line
static const char *s_part = "";

// The types of C that the values of a kernel read in. Each has its width, whether it has a sign, and its name with its
// white space taken out. A bool goes in as a flag, in a predicate, or as a segment, the word 0 or 1
static const struct
{
    const char *type_name;
    const char *bare;
    unsigned int bits;
    int is_signed;
} s_types[] = {
    {"signed char", "signedchar", 8u, 1},
    {"unsigned char", "unsignedchar", 8u, 0},
    {"short", "short", 16u, 1},
    {"unsigned short", "unsignedshort", 16u, 0},
    {"int", "int", 32u, 1},
    {"unsigned int", "unsignedint", 32u, 0},
    {"long long", "longlong", 64u, 1},
    {"unsigned long long", "unsignedlonglong", 64u, 0},
    {"bool", "bool", STICK_PREDICATE_BITS, 0},
};

#define STICK_TYPES ((int)(sizeof(s_types) / sizeof(s_types[0])))

// ---- the texts ----

// a name, a register, a token or an operation, terminated
struct StickWord
{
    char text[STICK_WORD_BYTES];
};

// an expression, a span or a line, terminated
struct StickExpr
{
    char text[STICK_EXPR_BYTES];
};

// 1 once a text passed its room. The kernel then puts the question that its text holds more than the engine does
static int s_stick_cut = 0;

// a line over the terminated text in `text` of `room` bytes, which goes on after what the text holds
static VerbaLinea stick_after(char *text, size_t room)
{
    VerbaLinea line = mmgr_line_over(text, room);
    line.at = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = room);
    return line;
}

// the line finished, and s_stick_cut set where a write did not fit
static void stick_finished(VerbaLinea *line)
{
    s_stick_cut |= (mmgr_line_ok(line) == 0) ? 1 : 0;
    (void)mmgr_line_finish(line);
}

// `length` bytes of `from` put after what the `room` bytes at `to` hold
static void stick_add_n(char *to, size_t room, const char *from, size_t length)
{
    VerbaLinea line = stick_after(to, room);
    (void)mmgr_line_put_n(&line, from, length);
    stick_finished(&line);
}

static void stick_add(char *to, size_t room, const char *from)
{
    stick_add_n(to, room, from,
                EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = from, .cap = PLATFORM_TEXT_TO_TERMINATOR));
}

// `from` as the whole of `to`
static void stick_set_n(char *to, size_t room, const char *from, size_t length)
{
    to[0] = '\0';
    stick_add_n(to, room, from, length);
}

static void stick_set(char *to, size_t room, const char *from)
{
    to[0] = '\0';
    stick_add(to, room, from);
}

// `number` in decimal put after what `to` holds
static void stick_add_decimal(char *to, size_t room, unsigned long long number)
{
    VerbaLinea line = stick_after(to, room);
    mmgr_line_decimal(&line, number, 1u);
    stick_finished(&line);
}

#define STICK_ADD(to_, from_) stick_add((to_).text, sizeof((to_).text), from_)
#define STICK_ADD_N(to_, from_, length_) stick_add_n((to_).text, sizeof((to_).text), from_, length_)
#define STICK_SET(to_, from_) stick_set((to_).text, sizeof((to_).text), from_)
#define STICK_SET_N(to_, from_, length_) stick_set_n((to_).text, sizeof((to_).text), from_, length_)
#define STICK_ADD_DECIMAL(to_, number_) stick_add_decimal((to_).text, sizeof((to_).text), number_)

// the bytes the terminated `text` holds
static size_t stick_length(const char *text)
{
    return EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
}

// 1 where the two terminated texts are the same
static int stick_same(const char *left, const char *right)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = left, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = right,
                      .ci = EMBED_FALSE);
}

// 1 where the `length` bytes at `text` are the terminated `other`
static int stick_same_n(const char *text, size_t length, const char *other)
{
    return (stick_length(other) == length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = text, .other = other, .bytes = length) == 0);
}

// 1 where the terminated `text` begins with the terminated `prefix`. The walk stops at the end of `prefix`. A text that
// runs to the end of a whole file then costs no more than its prefix
static int stick_begins(const char *text, const char *prefix)
{
    return EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = prefix,
                      .ci = EMBED_FALSE);
}

// 1 where the terminated `text` ends with the terminated `suffix`
static int stick_ends(const char *text, const char *suffix)
{
    const size_t length = stick_length(text);
    const size_t size = stick_length(suffix);
    return (length >= size) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &text[length - size], .other = suffix, .bytes = size) == 0);
}

// the place of the terminated `needle` in the terminated `text` from `from`, or `none` where it is not there
static size_t stick_place(const char *text, const char *needle, size_t from, size_t none)
{
    const char *const found =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &text[from], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = needle, .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    // a found needle lies at or past its search's start
    return (found != NULL) ? (size_t)(found - text) : none;
}

// 1 where the terminated `text` holds the terminated `needle`
static int stick_has(const char *text, const char *needle)
{
    return stick_place(text, needle, 0u, (size_t)-1) != (size_t)-1;
}

// the place of `character` in the terminated `text` from `from`, or `none` where it is not there
static size_t stick_place_of(const char *text, char character, size_t from, size_t none)
{
    // a byte of the text read as the unsigned byte the search takes
    const char *const found = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &text[from],
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = (uint8_t)character);
    // a found byte lies at or past its search's start
    return (found != NULL) ? (size_t)(found - text) : none;
}

// the bytes of white space, and the bytes a name holds past its first: a character, a digit, `_` or `.`
static const char s_spaces[] = " \t\r\n";
static const char s_name_bytes[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.";

// the bytes at the start of the `cap` bytes at `text` that white space holds
static size_t stick_spaces(const char *text, size_t cap)
{
    return EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text, .cap = cap, .other = s_spaces);
}

// the bytes at the start of the `cap` bytes at `text` that hold no white space
static size_t stick_unspaced(const char *text, size_t cap)
{
    return EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = cap, .other = s_spaces);
}

// 1 where `character` joins a name: a character, a digit, `_` or `.`
static int stick_joins(char character)
{
    // a byte of a text read as the unsigned byte the search takes
    return (character != '\0') && (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = s_name_bytes,
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = (uint8_t)character) != NULL);
}

// 1 where `character` opens a name: a character or `_`
static int stick_opens_name(char character)
{
    // a byte of a text read as the unsigned byte the class takes
    return (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALPHA, .byte = (unsigned char)character) != 0) ||
           (character == '_');
}

// the `length` bytes at `text` with every space, tab and line's end taken out, into `bare`
static void stick_bare_n(const char *text, size_t length, StickExpr *bare)
{
    VerbaLinea line = mmgr_line_over(bare->text, sizeof(bare->text));
    size_t at = stick_spaces(text, length);
    while (at < length)
    {
        const size_t run = stick_unspaced(&text[at], length - at);
        (void)mmgr_line_put_n(&line, &text[at], run);
        at += run;
        at += stick_spaces(&text[at], length - at);
    }
    stick_finished(&line);
}

static StickExpr stick_bare(const char *text)
{
    StickExpr bare;
    stick_bare_n(text, stick_length(text), &bare);
    return bare;
}

// the `length` bytes at `text` with every run of white space cut to one space and none at either end, into `squeezed`
// of `room` bytes. 1, or 0 where it passes the room
static int stick_squeezed(const char *text, size_t length, char *squeezed, size_t room)
{
    VerbaLinea line = mmgr_line_over(squeezed, room);
    size_t at = stick_spaces(text, length);
    while (at < length)
    {
        const size_t run = stick_unspaced(&text[at], length - at);
        mmgr_line_put(&line, (line.at != 0u) ? " " : "");
        (void)mmgr_line_put_n(&line, &text[at], run);
        at += run;
        at += stick_spaces(&text[at], length - at);
    }
    const int fits = mmgr_line_ok(&line) != 0;
    (void)mmgr_line_finish(&line);
    return fits;
}

static StickExpr stick_squeeze(const char *text)
{
    StickExpr squeezed;
    s_stick_cut |= stick_squeezed(text, stick_length(text), squeezed.text, sizeof(squeezed.text)) ? 0 : 1;
    return squeezed;
}

// The depth of parentheses and brackets over the `length` bytes at `text`, from `*depth` on, into `*depth`. The walk
// jumps from bracket to bracket. 1, or 0 where the depth goes under 0
static int stick_depth_walked(const char *text, size_t length, int *depth)
{
    size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = length, .other = "()[]");
    while (at < length)
    {
        *depth += ((text[at] == '(') || (text[at] == '[')) ? 1 : -1;
        if (*depth < 0)
        {
            return 0;
        }
        at += 1u;
        at += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = "()[]");
    }
    return 1;
}

// 1 where the `length` bytes at `text` are not empty and their parentheses and brackets close in order
static int stick_balanced_n(const char *text, size_t length)
{
    int depth = 0;
    return (length != 0u) && stick_depth_walked(text, length, &depth) && (depth == 0);
}

// 1 where the terminated `text` is one parenthesis around a balanced inside, which the reader takes off
static int stick_wrapped(const char *text)
{
    const size_t length = stick_length(text);
    return (length > 2u) && (text[0] == '(') && (text[length - 1u] == ')') && stick_balanced_n(&text[1], length - 2u);
}

// ---- the types of C ----

// the width of the registers that hold a value of `bits`: that of a predicate, of one register, or of a pair
static unsigned int stick_register_bits(unsigned int bits)
{
    return (bits == STICK_PREDICATE_BITS) ? bits
                                          : ((bits <= STICK_REGISTER_BITS) ? STICK_REGISTER_BITS : STICK_PAIR_BITS);
}

// the place in s_types of the type `bare` names with its white space taken out, or -1
static int stick_type_bare(const char *bare)
{
    for (int type = 0; type < STICK_TYPES; type += 1)
    {
        if (stick_same(bare, s_types[type].bare))
        {
            return type;
        }
    }
    return -1;
}

// the type `text` names, or -1
static int stick_type(const char *text)
{
    return stick_type_bare(stick_bare(text).text);
}

// the cast to type `type` with its white space taken out
static StickWord stick_cast(int type)
{
    StickWord cast;
    STICK_SET(cast, "(");
    STICK_ADD(cast, s_types[type].bare);
    STICK_ADD(cast, ")");
    return cast;
}

// the type C promotes a value of type `type` to: int for a type narrower than int, else its own
static int stick_promoted(int type)
{
    return (s_types[type].bits < 32u) ? stick_type_bare("int") : type;
}

// the type that the usual arithmetic conversions of C give two promoted types. It is the wider of two of one
// signedness. Else it is the unsigned one where it is at least as wide. Else it is the signed one, which then holds
// each value of the other
static int stick_common(int left, int right)
{
    if (s_types[left].is_signed == s_types[right].is_signed)
    {
        return (s_types[left].bits >= s_types[right].bits) ? left : right;
    }
    const int unsigned_one = (s_types[left].is_signed != 0) ? right : left;
    const int signed_one = (s_types[left].is_signed != 0) ? left : right;
    return (s_types[unsigned_one].bits >= s_types[signed_one].bits) ? unsigned_one : signed_one;
}

// the binding of a binary operator of C, the loosest 1; 0 for a token that is no binary operator
static int stick_precedence(const char *token)
{
    static const char *const s_levels[][4] = {
        {"||"},       {"&&"},     {"|"},          {"^"}, {"&"}, {"==", "!="}, {"<", "<=", ">", ">="},
        {"<<", ">>"}, {"+", "-"}, {"*", "/", "%"}};
    for (int level = 0; level < (int)(sizeof(s_levels) / sizeof(s_levels[0])); level += 1)
    {
        for (unsigned int one = 0u; one < 4u; one += 1u)
        {
            if ((s_levels[level][one] != NULL) && stick_same(token, s_levels[level][one]))
            {
                return level + 1;
            }
        }
    }
    return 0;
}

// 1 where `token` compares two values
static int stick_compares(const char *token)
{
    return stick_same(token, "<") || stick_same(token, "<=") || stick_same(token, ">") || stick_same(token, ">=") ||
           stick_same(token, "==") || stick_same(token, "!=");
}

// ---- the tokens of an expression ----

// One token of C as the source has it: its text, and 1 where it is an operator. A name or a number is no operator
struct StickToken
{
    StickWord text;
    int operation;
};

// the tokens of one expression, the next one the parser takes, and their count
struct StickTokens
{
    StickToken token[STICK_TOKENS_MAX];
    unsigned int count;
};

// The terminated `expression` cut into the tokens of C, with its white space between them, into `tokens`. The tokens
// are names, a member of one taken with it, numbers with their suffixes, and each operator. The operator is the
// longest at its place. `a - -b` cuts as `a`, `-`, `-`, `b`, and `a--b` as `a`, `--`, `b`, as C cuts them. 1, or 0
// where the tokens pass the room
static int stick_tokens(const char *expression, StickTokens *tokens)
{
    static const char *const s_pairs[] = {"<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "++", "--"};
    tokens->count = 0u;
    const size_t length = stick_length(expression);
    size_t at = stick_spaces(expression, length);
    while (at < length)
    {
        const char character = expression[at];
        if (tokens->count >= STICK_TOKENS_MAX)
        {
            return 0;
        }
        StickToken *const token = &tokens->token[tokens->count];
        tokens->count += 1u;
        size_t end = at + 1u;
        // a byte of the text read as the unsigned byte the class takes
        if ((EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (unsigned char)character) != 0) ||
            stick_opens_name(character))
        {
            end += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &expression[end], .cap = length - end,
                              .other = s_name_bytes);
            STICK_SET_N(token->text, &expression[at], end - at);
            token->operation = 0;
            at = end + stick_spaces(&expression[end], length - end);
            continue;
        }
        for (unsigned int pair = 0u; pair < (unsigned int)(sizeof(s_pairs) / sizeof(s_pairs[0])); pair += 1u)
        {
            end = EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = &expression[at], .cap = length - at,
                             .other = s_pairs[pair], .ci = EMBED_FALSE)
                      ? (at + 2u)
                      : end;
        }
        STICK_SET_N(token->text, &expression[at], end - at);
        token->operation = 1;
        at = end + stick_spaces(&expression[end], length - end);
    }
    return 1;
}

// ---- the trace ----

// The trace of the kernel in its read. It holds each tried reading, nested as the engine tries it. It holds each form
// whose text matches, and each choice of its operands with the reason why it fails. It holds what each reading gives,
// each written line and each question put. It is a movable text that the run gets once the read of the kernel ends.
// Nothing collects where s_tracing is 0, and then each line goes to a sink that holds nothing
static PlatformText s_trace;
static int s_tracing = 0;
static unsigned int s_trace_depth = 0u;
static char s_trace_sink[64];
static VerbaLinea s_trace_sink_line;

// one line of the trace begun, indented by the depth of the reading it is of [BORROWS]
static VerbaLinea *stick_trace_begun(void)
{
    if (s_tracing == 0)
    {
        s_trace_sink_line = mmgr_line_over(s_trace_sink, sizeof(s_trace_sink));
        return &s_trace_sink_line;
    }
    for (unsigned int at = 0u; at < (2u * s_trace_depth); at += 1u)
    {
        mmgr_line_ch(&s_trace.line, ' ');
    }
    return &s_trace.line;
}

// the line of the trace ended
static void stick_trace_written(VerbaLinea *line)
{
    if (s_tracing != 0)
    {
        mmgr_line_ch(line, '\n');
    }
}

// ---- the parameters' names of sass.krs ----

// the names of one form's parameters, in the order of the ruleset's own line for it
struct StickNames
{
    StickWord name[STICK_PLACEHOLDERS_MAX];
    unsigned int count;
};

// the names of each form's parameters in sass.krs, by the form's place in the schema, read once
static StickNames s_parameter_names[OPCODE_COUNT];

// the place of form `name` in the schema, or OPCODE_COUNT
static unsigned int stick_form(const char *name)
{
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        if (stick_same(name, s_form_names[form]))
        {
            return form;
        }
    }
    return OPCODE_COUNT;
}

// The names of each form's parameters of `rules`, from its text, into s_parameter_names. A line `form`, `err`, `nop`
// or `construct`, then the form's name, names them up to its `=`
static void stick_parameter_names_read(const Ruleset *rules)
{
    PlatformText read;
    ruleset_text_begun(&read);
    char why[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    (void)ruleset_text(rules->schema, rules->path, &read, &error);
    const char *const bytes = platform_text_bytes(&read);
    // the text is at max what a size_t counts
    const size_t size = (bytes != NULL) ? (size_t)platform_text_size(&read) : 0u;
    size_t at = 0u;
    while (at < size)
    {
        // a line ends at its line's end, which lies past its start, or at the text's end
        const char *const ends =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &bytes[at], .cap = size - at, .byte = (uint8_t)'\n');
        const size_t end = (ends != NULL) ? (size_t)(ends - bytes) : size;
        // the words of the line, each at its first byte and its length
        const char *word[STICK_PLACEHOLDERS_MAX + 3u];
        size_t length[STICK_PLACEHOLDERS_MAX + 3u];
        unsigned int words = 0u;
        size_t cut = at + stick_spaces(&bytes[at], end - at);
        while ((cut < end) && (words < (STICK_PLACEHOLDERS_MAX + 3u)))
        {
            word[words] = &bytes[cut];
            length[words] = stick_unspaced(&bytes[cut], end - cut);
            cut += length[words];
            cut += stick_spaces(&bytes[cut], end - cut);
            words += 1u;
        }
        at = end + 1u;
        const int named =
            (words >= 2u) && (stick_same_n(word[0], length[0], "form") || stick_same_n(word[0], length[0], "err") ||
                              stick_same_n(word[0], length[0], "nop") || stick_same_n(word[0], length[0], "construct"));
        StickWord form_name;
        form_name.text[0] = '\0';
        if (named)
        {
            STICK_SET_N(form_name, word[1], length[1]);
        }
        const unsigned int form = named ? stick_form(form_name.text) : OPCODE_COUNT;
        if ((form == OPCODE_COUNT) || (s_parameter_names[form].count != 0u))
        {
            continue;
        }
        for (unsigned int each = 2u; (each < words) && !stick_same_n(word[each], length[each], "="); each += 1u)
        {
            StickNames *const names = &s_parameter_names[form];
            if (names->count < STICK_PLACEHOLDERS_MAX)
            {
                STICK_SET_N(names->name[names->count], word[each], length[each]);
                names->count += 1u;
            }
        }
    }
    platform_text_ended(&read);
}

// ---- the writes of sass.krs ----

// the register for the scratch of a form, where its text is only for a look or an assembly, and never runs. It is one
// that no stand-in names
static int stick_scratch_are_in(void *context, const char *bank, VerbaLinea *name)
{
    (void)context;
    mmgr_line_put(name, stick_same(bank, "wide") ? "R200" : (stick_same(bank, "predicate") ? "P5" : "R202"));
    return 1;
}

// a scratch that refuses every register: a form that takes scratch writes nothing
static int stick_scratch_none(void *context, const char *bank, VerbaLinea *name)
{
    (void)context;
    (void)bank;
    (void)name;
    return 0;
}

// Form `name` of `rules` written onto `text`, its `count` arguments in the order of its parameters, a scratch from
// `scratch`. 0, and nothing written, where the ruleset writes no such form or the text passes its room
static int stick_written(const Ruleset *rules, const char *name, const char *const *arguments, unsigned int count,
                         const RulesetNamedScratch *scratch, VerbaLinea *text)
{
    const size_t before = text->at;
    const int written = ruleset_form_written(rules, name, arguments, count, scratch, text) && (mmgr_line_ok(text) != 0);
    if (!written)
    {
        text->at = (text->at > before) ? before : text->at;
    }
    return written;
}

// Form `name` of `rules` written into the `room` bytes at `text`, terminated, its scratch from `taken`. 1, or 0, and
// the text empty, where the ruleset writes no such form or the text passes the room
static int stick_written_into(const Ruleset *rules, const char *name, const char *const *arguments, unsigned int count,
                              int (*taken)(void *context, const char *bank, VerbaLinea *name), char *text, size_t room)
{
    const RulesetNamedScratch scratch = {taken, NULL};
    VerbaLinea line = mmgr_line_over(text, room);
    const int written = stick_written(rules, name, arguments, count, &scratch, &line);
    (void)mmgr_line_finish(&line);
    if (!written)
    {
        text[0] = '\0';
    }
    return written;
}

// register `number` of the bank `bank` of `rules`, empty where the ruleset has no bank of that name
static StickWord stick_register(const Ruleset *rules, const char *bank, unsigned int number)
{
    StickWord name;
    VerbaLinea line = mmgr_line_over(name.text, sizeof(name.text));
    const int named = ruleset_register_written(rules, bank, number, &line);
    stick_finished(&line);
    name.text[0] = named ? name.text[0] : '\0';
    return name;
}

// The width of the registers that the sass.krs text for `form` takes each parameter in, by the place of the parameter,
// into `bits`. It is a pair where the text writes the high word of the parameter, or reads it as a 64-bit address. It
// is STICK_COUNT_BITS for the count of a shift, else one register
static void stick_parameter_bits(const Ruleset *sass, unsigned int form, unsigned int *bits)
{
    const unsigned int count = sass->schema->forms[form].parameters;
    StickWord markers[STICK_PLACEHOLDERS_MAX];
    const char *arguments[STICK_PLACEHOLDERS_MAX];
    for (unsigned int parameter = 0u; (parameter < count) && (parameter < STICK_PLACEHOLDERS_MAX); parameter += 1u)
    {
        markers[parameter].text[0] = '\x01';
        // the parameters of one form are far fewer than the letters past A
        markers[parameter].text[1] = (char)('A' + parameter);
        markers[parameter].text[2] = '\x01';
        markers[parameter].text[3] = '\0';
        arguments[parameter] = markers[parameter].text;
        bits[parameter] = STICK_REGISTER_BITS;
    }
    static StickExpr s_text;
    if ((count > STICK_PLACEHOLDERS_MAX) || !stick_written_into(sass, s_form_names[form], arguments, count,
                                                                stick_scratch_are_in, s_text.text, sizeof(s_text.text)))
    {
        return;
    }
    const StickNames *const names = &s_parameter_names[form];
    for (unsigned int parameter = 0u; parameter < count; parameter += 1u)
    {
        StickWord high = markers[parameter];
        StickWord address = markers[parameter];
        STICK_ADD(high, ".hi");
        STICK_ADD(address, ".64");
        const int pair = stick_has(s_text.text, high.text) || stick_has(s_text.text, address.text);
        const int shift_count = (parameter < names->count) && stick_same(names->name[parameter].text, "bits");
        bits[parameter] = pair ? STICK_PAIR_BITS : (shift_count ? STICK_COUNT_BITS : STICK_REGISTER_BITS);
    }
}

// the shift of a multiply's stem: its name with mul written shl, or OPCODE_COUNT where the schema has none
static unsigned int stick_shift_of(unsigned int form)
{
    const char *const name = s_form_names[form];
    const size_t at = stick_place(name, "mul", 0u, (size_t)-1);
    if (at == (size_t)-1)
    {
        return OPCODE_COUNT;
    }
    StickWord shift;
    STICK_SET_N(shift, name, at);
    STICK_ADD(shift, "shl");
    STICK_ADD(shift, &name[at + 3u]);
    return stick_form(shift.text);
}

// the operation sass.krs's text for `form` opens with, its operands stand-ins
static StickWord stick_operation_of(const Ruleset *sass, unsigned int form)
{
    StickWord operation;
    operation.text[0] = '\0';
    const char *arguments[STICK_PLACEHOLDERS_MAX];
    const unsigned int count = sass->schema->forms[form].parameters;
    for (unsigned int at = 0u; (at < count) && (at < STICK_PLACEHOLDERS_MAX); at += 1u)
    {
        arguments[at] = "R2";
    }
    static StickExpr s_text;
    if ((count > STICK_PLACEHOLDERS_MAX) || !stick_written_into(sass, s_form_names[form], arguments, count,
                                                                stick_scratch_are_in, s_text.text, sizeof(s_text.text)))
    {
        return operation;
    }
    const StickExpr squeezed = stick_squeeze(s_text.text);
    STICK_SET_N(operation, squeezed.text, stick_place_of(squeezed.text, ' ', 0u, stick_length(squeezed.text)));
    return operation;
}

// the line sass.ksc gives a fold of `type` put for parameter `parameter` of form `form`, or the line's opening where
// `opening` is not 0
static StickExpr stick_fold_line(unsigned int form, const char *type, unsigned int parameter, int opening)
{
    StickExpr line;
    STICK_SET(line, s_form_names[form]);
    STICK_ADD(line, ": the system folds the ");
    STICK_ADD(line, type);
    STICK_ADD(line, " put for ");
    const StickNames *const names = &s_parameter_names[form];
    if ((opening == 0) && (parameter < names->count))
    {
        STICK_ADD(line, names->name[parameter].text);
    }
    return line;
}

// ---- the patterns of cu.krs ----

// a piece of a pattern's text: its first byte in s_pattern_text and its bytes
struct StickPiece
{
    unsigned int at;
    unsigned int length;
};

// A form that cu.krs writes: its place, and its text cut at its parameters with white space taken out. Then the width
// that its casts give the value.
//
// The form of an expression is `{to} = <text>;`. Its pieces are the text after `=`, cut at the parameters past to. The
// form of a statement is its whole text, cut at each parameter.
//
// `order` gives each parameter that the text names, in the order of the text, its place among the parameters past to,
// as the schema orders them. `placeholder_bits` gives, in the order of the schema, the width of the registers of each.
// `select` is 1 where the text is the conditional of C, `({where}) ? chosen : otherwise`
struct StickPattern
{
    unsigned int form;
    unsigned int piece_first;
    unsigned int piece_count;
    unsigned int bits;
    int statement;
    unsigned int order[STICK_PLACEHOLDERS_MAX];
    unsigned int placeholder_bits[STICK_PLACEHOLDERS_MAX];
    unsigned int places;
    int select;
};

// The text of every form's pieces, bare, in one cell of the persistent tier taken once at the count of their bytes.
// Each form's pieces are ranges of it. Then the patterns and their pieces, each in one cell of the persistent tier
// taken once at its count. The engine reads them once and keeps them for every round
static char *s_pattern_text = NULL;
static StickPiece s_form_pieces[OPCODE_COUNT][STICK_PLACEHOLDERS_MAX + 1u];
static StickPattern *s_patterns = NULL;
static unsigned int s_pattern_count = 0u;
static StickPiece *s_pattern_pieces = NULL;
static unsigned int s_pattern_piece_count = 0u;

// the bytes of the piece `piece`
static const char *stick_piece_text(StickPiece piece)
{
    return &s_pattern_text[piece.at];
}

// 1 where the form `form` of `rules` reads as a pattern. A sign, a byte of -1, 0 or 1, is no type of C, and a form over
// one reads no text of C. Its text narrows to a signed char only what is already a sign
static int stick_form_patterned(const Ruleset *rules, unsigned int form)
{
    const char *const name = s_form_names[form];
    return (rules->form_given[form] == (unsigned char)RULESET_CORE_GIVEN) &&
           (rules->forms[form].placeholder_count != 0u) &&
           (rules->forms[form].placeholder_count < STICK_PLACEHOLDERS_MAX) && !stick_begins(name, "sign_") &&
           !stick_begins(name, "test_sign_");
}

// The text of every form's pieces of `rules`, bare, into s_pattern_text, each form's pieces into s_form_pieces. The
// code takes the cell once at the count of the raw bytes, which the bare bytes are at max. 1, or 0 where the pool
// cannot meet the cell
static int stick_pattern_text_read(const Ruleset *rules)
{
    unsigned long long raw = 0ULL;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        for (unsigned int piece = 0u;
             stick_form_patterned(rules, form) && (piece <= rules->forms[form].placeholder_count); piece += 1u)
        {
            raw += stick_length(rules->forms[form].pieces[piece]);
        }
    }
    s_pattern_text = (char *)platform_cell_taken(raw + 1ULL, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if (s_pattern_text == NULL)
    {
        return 0;
    }
    unsigned int size = 0u;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        for (unsigned int piece = 0u;
             stick_form_patterned(rules, form) && (piece <= rules->forms[form].placeholder_count); piece += 1u)
        {
            const char *const text = rules->forms[form].pieces[piece];
            const size_t length = stick_length(text);
            StickPiece *const bare = &s_form_pieces[form][piece];
            bare->at = size;
            size_t at = stick_spaces(text, length);
            while (at < length)
            {
                const size_t run = stick_unspaced(&text[at], length - at);
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &s_pattern_text[size], .src = &text[at], .bytes = run);
                // the bytes of the pieces are far below 2^32
                size += (unsigned int)run;
                at += run;
                at += stick_spaces(&text[at], length - at);
            }
            bare->length = size - bare->at;
        }
    }
    s_pattern_text[size] = '\0';
    return 1;
}

// the width of the registers of the C type, written bare, of the cast that ends `piece`. 0 where `piece` ends in no
// cast to the own type of a register, unsigned int or unsigned long long
static unsigned int stick_own_cast(StickPiece piece)
{
    static const char *const s_own[] = {"(unsignedint)", "(unsignedlonglong)"};
    for (unsigned int own = 0u; own < 2u; own += 1u)
    {
        const size_t size = stick_length(s_own[own]);
        if ((piece.length >= size) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &stick_piece_text(piece)[piece.length - size],
                        .other = s_own[own], .bytes = size) == 0))
        {
            return (own == 0u) ? STICK_REGISTER_BITS : STICK_PAIR_BITS;
        }
    }
    return 0u;
}

// 1 where `piece` holds the terminated `needle`
static int stick_piece_has(StickPiece piece, const char *needle)
{
    return EMBED_CALL(cellul.has, CatenaFinitaCfg, .src = stick_piece_text(piece), .cap = piece.length, .other = needle,
                      .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
}

// 1 where `piece` is the terminated `text`
static int stick_piece_is(StickPiece piece, const char *text)
{
    return stick_same_n(stick_piece_text(piece), piece.length, text);
}

// 1 where `piece` begins with the terminated `prefix`
static int stick_piece_begins(StickPiece piece, const char *prefix)
{
    return EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = stick_piece_text(piece), .cap = piece.length,
                      .other = prefix, .ci = EMBED_FALSE);
}

// 1 where the `count` pieces at `pieces`, an `x` put between each two, are one parenthesis around a balanced inside.
// The `x` is no bracket, and the walk passes it over
static int stick_pieces_wrapped(const StickPiece *pieces, unsigned int count)
{
    unsigned long long length = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        length += pieces[at].length + (((at + 1u) < count) ? 1ULL : 0ULL);
    }
    if ((length <= 2ULL) || (pieces[0].length == 0u) || (pieces[count - 1u].length == 0u) ||
        !stick_piece_begins(pieces[0], "(") ||
        (stick_piece_text(pieces[count - 1u])[pieces[count - 1u].length - 1u] != ')'))
    {
        return 0;
    }
    int depth = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int from = (at == 0u) ? 1u : 0u;
        const unsigned int to = ((at + 1u) == count) ? (pieces[at].length - 1u) : pieces[at].length;
        if ((to > from) && !stick_depth_walked(&stick_piece_text(pieces[at])[from], to - from, &depth))
        {
            return 0;
        }
    }
    return depth == 0;
}

// The patterns of every form that cu.krs gives as an expression or as a statement of its own. The code writes them
// where `filling` is 1 and counts them where it is 0, each count into the two counts. A cast to the own type of a
// register just before a parameter converts nothing, where a register of that width holds the parameter. The engine
// then also reads the form with the cast left out, with the parameter at the width of the cast
static void stick_patterns_made(const Ruleset *rules, const Ruleset *sass, int filling, unsigned int *patterns,
                                unsigned int *pieces_counted)
{
    *patterns = 0u;
    *pieces_counted = 0u;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        if (!stick_form_patterned(rules, form))
        {
            continue;
        }
        const InstrTemplate &text = rules->forms[form];
        StickPiece pieces[STICK_PLACEHOLDERS_MAX + 1u];
        unsigned int piece_count = text.placeholder_count + 1u;
        for (unsigned int piece = 0u; piece < piece_count; piece += 1u)
        {
            pieces[piece] = s_form_pieces[form][piece];
        }
        const int expression =
            (text.placeholders[0] == 0u) && (pieces[0].length == 0u) && stick_piece_begins(pieces[1], "=") &&
            (pieces[piece_count - 1u].length != 0u) &&
            (stick_piece_text(pieces[piece_count - 1u])[pieces[piece_count - 1u].length - 1u] == ';');
        StickPiece *cut = pieces;
        if (expression)
        {
            cut = &pieces[1];
            piece_count -= 1u;
            cut[0].at += 1u;
            cut[0].length -= 1u;
            cut[piece_count - 1u].length -= 1u;
            // parentheses around the whole expression are not part of it, as the reader takes them off an expression
            if (stick_pieces_wrapped(cut, piece_count))
            {
                cut[0].at += 1u;
                cut[0].length -= 1u;
                cut[piece_count - 1u].length -= 1u;
            }
        }
        const unsigned int first = expression ? 1u : 0u;
        const unsigned int placeholders = text.placeholder_count - first;
        unsigned int casts[STICK_PLACEHOLDERS_MAX];
        unsigned int cast_count = 0u;
        for (unsigned int placeholder = 0u; placeholder < placeholders; placeholder += 1u)
        {
            if (stick_own_cast(cut[placeholder]) != 0u)
            {
                casts[cast_count] = placeholder;
                cast_count += 1u;
            }
        }
        if (!filling)
        {
            *patterns += 1u << cast_count;
            *pieces_counted += (1u << cast_count) * piece_count;
            continue;
        }
        unsigned int bits = 0u;
        for (unsigned int piece = 0u; piece < piece_count; piece += 1u)
        {
            bits = (stick_piece_has(cut[piece], "(unsignedlonglong)") || stick_piece_has(cut[piece], "(longlong)"))
                       ? 64u
                       : bits;
        }
        unsigned int parameter_bits[STICK_PLACEHOLDERS_MAX];
        stick_parameter_bits(sass, form, parameter_bits);
        const int select = expression && (placeholders == 3u) && stick_piece_is(cut[0], "(") &&
                           stick_piece_begins(cut[1], ")?") && stick_piece_begins(cut[2], ":");
        unsigned int order[STICK_PLACEHOLDERS_MAX];
        // a parameter that the text leaves out still has its place: the place of the parameter fixes the widths
        unsigned int places = placeholders;
        for (unsigned int placeholder = 0u; placeholder < placeholders; placeholder += 1u)
        {
            order[placeholder] = text.placeholders[placeholder + first] - first;
            places = ((order[placeholder] + 1u) > places) ? (order[placeholder] + 1u) : places;
        }
        for (unsigned int variant = 0u; variant < (1u << cast_count); variant += 1u)
        {
            StickPiece variant_pieces[STICK_PLACEHOLDERS_MAX + 1u];
            unsigned int placeholder_bits[STICK_PLACEHOLDERS_MAX];
            for (unsigned int piece = 0u; piece < piece_count; piece += 1u)
            {
                variant_pieces[piece] = cut[piece];
            }
            for (unsigned int place = 0u; place < places; place += 1u)
            {
                placeholder_bits[place] = STICK_REGISTER_BITS;
            }
            for (unsigned int placeholder = 0u; placeholder < placeholders; placeholder += 1u)
            {
                placeholder_bits[order[placeholder]] = parameter_bits[text.placeholders[placeholder + first]];
            }
            for (unsigned int cast = 0u; cast < cast_count; cast += 1u)
            {
                if (((variant >> cast) & 1u) == 0u)
                {
                    continue;
                }
                StickPiece *const piece = &variant_pieces[casts[cast]];
                const unsigned int cast_bits = stick_own_cast(*piece);
                unsigned int *const width = &placeholder_bits[order[casts[cast]]];
                const char *const open = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = stick_piece_text(*piece),
                                                    .cap = piece->length, .byte = (uint8_t)'(');
                // the cast's parenthesis lies within the piece
                piece->length = (unsigned int)(open - stick_piece_text(*piece));
                *width = (*width == STICK_PAIR_BITS) ? STICK_PAIR_BITS : cast_bits;
            }
            // a text of nothing but parameters reads nothing of its own, and would read an expression as itself; an
            // expression's text is one statement
            int lettered = 0;
            for (unsigned int piece = 0u; piece < piece_count; piece += 1u)
            {
                lettered = (variant_pieces[piece].length == 0u) ? lettered : 1;
                lettered = (expression && (stick_piece_has(variant_pieces[piece], ";") ||
                                           stick_piece_has(variant_pieces[piece], "{")))
                               ? -1
                               : lettered;
                if (lettered < 0)
                {
                    break;
                }
            }
            if (lettered <= 0)
            {
                continue;
            }
            StickPattern *const pattern = &s_patterns[*patterns];
            *patterns += 1u;
            pattern->form = form;
            pattern->piece_first = *pieces_counted;
            pattern->piece_count = piece_count;
            pattern->bits = bits;
            pattern->statement = expression ? 0 : 1;
            pattern->places = places;
            pattern->select = select;
            for (unsigned int placeholder = 0u; placeholder < placeholders; placeholder += 1u)
            {
                pattern->order[placeholder] = order[placeholder];
            }
            for (unsigned int place = 0u; place < places; place += 1u)
            {
                pattern->placeholder_bits[place] = placeholder_bits[place];
            }
            for (unsigned int piece = 0u; piece < piece_count; piece += 1u)
            {
                s_pattern_pieces[*pieces_counted] = variant_pieces[piece];
                *pieces_counted += 1u;
            }
        }
    }
}

// The patterns of cu.krs read once: the bare text of their pieces, then the patterns counted. Then their cells taken
// at the count, and the patterns written. 1, or 0 where the pool cannot meet a cell
static int stick_patterns_read(const Ruleset *rules, const Ruleset *sass)
{
    unsigned int patterns = 0u;
    unsigned int pieces = 0u;
    if (!stick_pattern_text_read(rules))
    {
        return 0;
    }
    stick_patterns_made(rules, sass, 0, &patterns, &pieces);
    s_patterns = (StickPattern *)platform_cell_cleared((patterns != 0u) ? patterns : 1u, sizeof(StickPattern),
                                                       platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    s_pattern_pieces = (StickPiece *)platform_cell_cleared((pieces != 0u) ? pieces : 1u, sizeof(StickPiece),
                                                           platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if ((s_patterns == NULL) || (s_pattern_pieces == NULL))
    {
        return 0;
    }
    stick_patterns_made(rules, sass, 1, &s_pattern_count, &s_pattern_piece_count);
    return 1;
}

// ---- the matcher ----

// a span of a text a pattern matches: its first byte and its bytes
struct StickSpan
{
    unsigned int at;
    unsigned int length;
};

// The spans of every match, as a stack, the spans of one match together. Each read of a text puts its matches on top,
// and takes them off before it returns. A read inside it puts its own above them. 1 in s_spans_full once a match found
// no room, and the kernel then puts its question
#define STICK_SPANS_MAX 65536u
static StickSpan s_spans[STICK_SPANS_MAX];
static unsigned int s_span_top = 0u;
static int s_spans_full = 0;

// Each way the pieces of `pattern` from `piece` on match the `length` bytes of `text` from `at` on, each parameter
// with a balanced span. Each match's spans, `bound` and the last, go onto s_spans. The answer is the count of the
// matches, and a pattern of no parameters puts no spans for its match
static unsigned int stick_match(const StickPattern *pattern, unsigned int piece, const char *text, size_t length,
                                size_t at, StickSpan *bound)
{
    const StickPiece own = s_pattern_pieces[pattern->piece_first + piece];
    if (((at + own.length) > length) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &text[at], .other = stick_piece_text(own), .bytes = own.length) != 0))
    {
        return 0u;
    }
    at += own.length;
    if (piece == (pattern->piece_count - 1u))
    {
        if (at != length)
        {
            return 0u;
        }
        if ((s_span_top + piece) > STICK_SPANS_MAX)
        {
            s_spans_full = 1;
            return 0u;
        }
        for (unsigned int each = 0u; each < piece; each += 1u)
        {
            s_spans[s_span_top] = bound[each];
            s_span_top += 1u;
        }
        return 1u;
    }
    unsigned int matches = 0u;
    // Each end of a balanced span from `at`. The depth of brackets runs on with the span. Where it is 0 each byte ends
    // a span of its own, and where it is past 0 the walk jumps to the next bracket. A depth under 0 ends every span
    int depth = 0;
    size_t end = at;
    while (end < length)
    {
        if (depth != 0)
        {
            end +=
                EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[end], .cap = length - end, .other = "()[]");
            if (end >= length)
            {
                return matches;
            }
        }
        const char character = text[end];
        depth += ((character == '(') || (character == '[')) ? 1 : (((character == ')') || (character == ']')) ? -1 : 0);
        end += 1u;
        if (depth < 0)
        {
            return matches;
        }
        if (depth == 0)
        {
            // a span of a text of one expression is far below 2^32
            bound[piece].at = (unsigned int)at;
            bound[piece].length = (unsigned int)(end - at);
            matches += stick_match(pattern, piece + 1u, text, length, end, bound);
        }
    }
    return matches;
}

// the matches of `pattern` over the terminated `text` put onto s_spans: their count. Each match is the pattern's
// parameters' spans, in the order of the text
static unsigned int stick_matches(const StickPattern *pattern, const char *text)
{
    StickSpan bound[STICK_PLACEHOLDERS_MAX];
    return stick_match(pattern, 0u, text, stick_length(text), 0u, bound);
}

// the span `span` of the terminated `text`, bare, as its own text
static StickExpr stick_span_text(const char *text, StickSpan span)
{
    StickExpr spanned;
    stick_bare_n(&text[span.at], span.length, &spanned);
    return spanned;
}

// ---- what the engine reads and writes ----

// what a reading is: a form over the readings of its parameters, or a value that the kernel stores. It can also be a
// number written in place, a number set into a register, or a parameter of the kernel
enum StickLeaf
{
    STICK_FORM = 0,
    STICK_NAME = 1,
    STICK_NUMBER = 2,
    STICK_NUMBER_SET = 3,
    STICK_PARAMETER = 4
};

// An expression as read: what it is, its form, its text, its number, its count of forms and its width. Then the
// readings of its parameters, `child_count` of them from `child_first` in s_nodes. Then where it folds, the word that
// it is. Then whether the engine reads a flag as its complement
struct StickReading
{
    StickLeaf leaf;
    unsigned int form;
    StickExpr text;
    unsigned int number;
    unsigned int cost;
    unsigned int bits;
    unsigned int child_first;
    unsigned int child_count;
    StickWord place;
    int negated;
};

// A reading begun as `leaf` of form `form` over the terminated `text`, its number, its cost and its width, with no
// children and no word
static StickReading stick_reading(StickLeaf leaf, unsigned int form, const char *text, unsigned int number,
                                  unsigned int cost, unsigned int bits)
{
    StickReading reading;
    reading.leaf = leaf;
    reading.form = form;
    STICK_SET(reading.text, text);
    reading.number = number;
    reading.cost = cost;
    reading.bits = bits;
    reading.child_first = 0u;
    reading.child_count = 0u;
    reading.place.text[0] = '\0';
    reading.negated = 0;
    return reading;
}

// The children of the readings of one statement, each a reading of its own, in one static table. A statement's
// readings take places in it, and the next statement takes them again. 1 in s_nodes_full once a reading found no
// room, and the kernel then puts its question
#define STICK_NODES_MAX 16384u
static StickReading s_nodes[STICK_NODES_MAX];
static unsigned int s_node_count = 0u;
static int s_nodes_full = 0;

// the `count` readings at `children` put into s_nodes as the children of `reading`: 1, or 0 where they find no room
static int stick_children_kept(StickReading *reading, const StickReading *children, unsigned int count)
{
    if ((s_node_count + count) > STICK_NODES_MAX)
    {
        s_nodes_full = 1;
        return 0;
    }
    reading->child_first = s_node_count;
    reading->child_count = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_nodes[s_node_count] = children[at];
        s_node_count += 1u;
    }
    return 1;
}

// the child at `at` of `reading` [BORROWS]
static const StickReading *stick_child(const StickReading *reading, unsigned int at)
{
    return &s_nodes[reading->child_first + at];
}

// a register that the kernel stores a value in, as sass.krs writes it, or the word that a folded value is. Its width,
// whether its type has a sign, and the place of its type in s_types
struct StickValue
{
    StickWord name;
    unsigned int bits;
    int is_signed;
    int type;
};

static StickValue stick_value(const char *name, unsigned int bits, int is_signed)
{
    StickValue value;
    STICK_SET(value.name, name);
    value.bits = bits;
    value.is_signed = is_signed;
    value.type = -1;
    return value;
}

// a parameter of the kernel: its name, its offset in the parameter bank, its width and its type's place in s_types
struct StickParameter
{
    StickWord name;
    unsigned int offset;
    unsigned int bits;
    int type;
};

// An expression written out with each conversion that C makes in it, and the place in s_types of the type that it
// gives. Then 1 where its value is never negative, a number or the select of 1 and 0 of a flag. A widening of it signed
// or unsigned then gives one value.
//
// Where `low_bits` is not 0, `low` is a text whose low `low_bits` bits are the own bits of the value. It leaves out
// conversions that those bits do not need: a narrowing reads no more of its operand than that.
//
// `zero_one` is 1 where the value is a word that holds 0 or 1 alone. A flag tested off such a word keeps it in `word`,
// of type `word_type`. The flag read back as a word is that word, [select(p, 1, 0) != 0] = p
struct StickTyped
{
    StickExpr text;
    int type;
    int nonnegative;
    StickExpr low;
    unsigned int low_bits;
    int zero_one;
    StickExpr word;
    int word_type;
};

// the terminated `text` of type `type`, never negative where `nonnegative` is 1, with no low bits and no word
static StickTyped stick_typed(const char *text, int type, int nonnegative)
{
    StickTyped typed;
    STICK_SET(typed.text, text);
    typed.type = type;
    typed.nonnegative = nonnegative;
    typed.low.text[0] = '\0';
    typed.low_bits = 0u;
    typed.zero_one = 0;
    typed.word.text[0] = '\0';
    typed.word_type = -1;
    return typed;
}

// ---- the tables of one kernel ----

// a value the kernel stores, by the text of what it is: a name, an expression read, or a number set into a register
struct StickBound
{
    StickExpr key;
    StickValue value;
};

// a typed expression the kernel keeps by a name: a bool stored as a flag, or a name read as an expression not yet
// written
struct StickNamed
{
    StickWord name;
    StickTyped typed;
};

// a reading made since the kernel last stored a value: its text, whether it read in place, whether it read, and what
// it read as
struct StickCached
{
    StickExpr key;
    int in_place;
    int read_given;
    StickReading reading;
};

// a wide that is the pair a word begins, and the word
struct StickTie
{
    StickWord wide;
    StickWord word;
};

// a word moved into another: the form that moves it, the word that it moves into, and the moved word
struct StickCopy
{
    unsigned int form;
    StickWord into;
    StickWord moved;
};

// A form and what the listing of nvcc shows of it. That is a word of the constant bank it moved, or a parameter and its
// log2. Or it is an offset, or the form of the complement of its test
struct StickAsked
{
    unsigned int form;
    unsigned int parameter;
    unsigned int number;
    StickWord word;
};

// What a round needs from the listing of nvcc about one kernel, each of its types a part of one cell of the kernel's.
// That is each form that moved a word of the constant bank, and the word. Then each multiply by a power of two with
// its parameter and the log2. Then the form of each load with its offset into the elements. Then each form that moved
// a word into a wide or into a word used once. Last, each test that a select read over, with the test of its
// complement
enum StickAskedType
{
    STICK_MOVED = 0,
    STICK_MULTIPLIED = 1,
    STICK_OFFSETS = 2,
    STICK_COPIED = 3,
    STICK_COPIED_ONCE = 4,
    STICK_COMPLEMENTED = 5,
    STICK_ASKED_TYPES = 6
};

// each write put to the assembler of a machine, and 1 where it assembled. The engine puts a write once, and reads its
// answer here each time after. A table full at its bound puts each further write again
#define STICK_ASSEMBLED_MAX 4096u

struct StickAssembled
{
    const SassMachine *machine;
    StickExpr lines;
    int assembled;
};

static StickAssembled s_assembled[STICK_ASSEMBLED_MAX];
static unsigned int s_assembled_count = 0u;

// the readings made since the kernel last stored a value, at a bound. A cache full at its bound keeps no more, and
// the reader reads again what it keeps not
#define STICK_CACHED_MAX 1024u
static StickCached s_cached[STICK_CACHED_MAX];
static unsigned int s_cached_count = 0u;

// the joins of two flags read exchanged, each read in that order once, at a bound of the depth of one read
#define STICK_EXCHANGING_MAX 32u
static StickExpr s_exchanging[STICK_EXCHANGING_MAX];
static unsigned int s_exchanging_count = 0u;

// ---- names, words and pipes ----

// the bytes of a line of a kernel's text or of nvcc's listing, its terminator among them
#define STICK_LINE_BYTES 256u

// one line of a kernel's text or one instruction of nvcc's listing, terminated
struct StickLine
{
    char text[STICK_LINE_BYTES];
};

// 1 where the terminated `text` is an expression other than `name` itself that reads `name`: the name located in it
// with no character, digit, `_` or `.` either side of it
static int stick_reads_name(const char *text, const char *name)
{
    if (stick_same(text, name))
    {
        return 0;
    }
    const size_t length = stick_length(text);
    const size_t size = stick_length(name);
    for (size_t at = stick_place(text, name, 0u, length); at < length; at = stick_place(text, name, at + 1u, length))
    {
        const size_t end = at + size;
        if (((at == 0u) || !stick_joins(text[at - 1u])) && ((end == length) || !stick_joins(text[end])))
        {
            return 1;
        }
    }
    return 0;
}

// the log2 of `number` where it is a power of two of 2 or more, else 0
static unsigned int stick_log2(unsigned int number)
{
    if ((number < 2u) || ((number & (number - 1u)) != 0u))
    {
        return 0u;
    }
    // a power of two of a word has its one bit below 2^32
    return (unsigned int)EMBED_CALL(clz.trail, ClzCfg, .val = number);
}

// `number` written in hexadecimal as nvcc's listing writes an immediate
static StickWord stick_hex(unsigned long long number)
{
    StickWord written;
    VerbaLinea line = mmgr_line_over(written.text, sizeof(written.text));
    mmgr_line_put(&line, "0x");
    mmgr_line_hex(&line, number, 1u);
    stick_finished(&line);
    return written;
}

// the group `group` of `match` over the terminated `text`, as a text of its own
static StickExpr stick_group(const TableMatch *match, unsigned int group, const char *text)
{
    size_t length = 0u;
    const char *const bytes = table_group(match, group, text, &length);
    StickExpr grouped;
    STICK_SET_N(grouped, bytes, length);
    return grouped;
}

// 1 where the whole pattern of `once`, read from `source` on its first use, matches the terminated `text`, its groups
// into `match`
static int stick_matched(TablePatternOnce *once, const char *source, const char *text, TableMatch *match)
{
    return table_matched(table_once(once, source, 1), text, stick_length(text), match);
}

// A word of the constant bank as nvcc's listing writes it, `c[bank][offset]` with each sum taken: empty where `text` is
// no such word
static StickWord stick_constant(const char *text)
{
    static TablePatternOnce s_word;
    StickWord written;
    written.text[0] = '\0';
    const StickExpr bare = stick_bare(text);
    TableMatch found;
    if (!stick_matched(&s_word, "c\\[([0-9a-fA-Fx]+)\\]\\[([0-9a-fA-Fx+]+)\\]", bare.text, &found))
    {
        return written;
    }
    const StickExpr bank = stick_group(&found, 1u, bare.text);
    const StickExpr terms = stick_group(&found, 2u, bare.text);
    unsigned long long offset = 0ULL;
    const size_t length = stick_length(terms.text);
    for (size_t at = 0u; at < length;)
    {
        const size_t plus = stick_place_of(terms.text, '+', at, length);
        StickExpr term;
        STICK_SET_N(term, &terms.text[at], plus - at);
        offset += EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = term.text, .end = NULL, .base = 0);
        at = plus + 1u;
    }
    VerbaLinea line = mmgr_line_over(written.text, sizeof(written.text));
    mmgr_line_put(&line, "c[0x");
    mmgr_line_hex(&line, EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = bank.text, .end = NULL, .base = 0), 1u);
    mmgr_line_put(&line, "][0x");
    mmgr_line_hex(&line, offset, 1u);
    mmgr_line_put(&line, "]");
    stick_finished(&line);
    return written;
}

// The pipe that an operation issues to on the part, as the profiler of NVIDIA names them: 1 the integer pipe, 2 the
// FMA pipe. 0 is any other: a load, a store, a branch, the double pipe or the transcendental unit
static int stick_pipe_of(const char *operation)
{
    static const char *const s_fma[] = {"IMAD",  "IMUL",  "FFMA",  "FMUL", "FADD",
                                        "HFMA2", "HADD2", "HMUL2", "I2FP", "F2FP"};
    static const char *const s_integer[] = {"IADD3", "LOP3",  "SHF",  "SEL",  "MOV",   "ISETP", "FSETP",
                                            "PRMT",  "IMNMX", "LEA",  "FSEL", "PLOP3", "IABS",  "FMNMX",
                                            "P2R",   "R2P",   "CS2R", "SGXT", "BMSK",  "IADD"};
    for (unsigned int one = 0u; one < (unsigned int)(sizeof(s_fma) / sizeof(s_fma[0])); one += 1u)
    {
        if (stick_begins(operation, s_fma[one]))
        {
            return 2;
        }
    }
    for (unsigned int one = 0u; one < (unsigned int)(sizeof(s_integer) / sizeof(s_integer[0])); one += 1u)
    {
        if (stick_begins(operation, s_integer[one]))
        {
            return 1;
        }
    }
    return 0;
}

// Where `instruction`, squeezed, is one that the system writes on either pipe. The result is the operation of its write
// on the integer pipe, and its write on the FMA pipe in `writing`. These are:
//   - a move of a word of the constant bank, IMAD.MOV.U32 over two zero registers.
//   - a left shift by a number, IMAD.SHL.U32 by the power of two.
//   - an add of two words and a carry, IMAD.X of the first by 1 added to the second.
//
// Empty where it is none of these
static StickWord stick_pipe_choice(const char *instruction, StickExpr *writing)
{
    static TablePatternOnce s_carried_add;
    static TablePatternOnce s_carried_product;
    static TablePatternOnce s_moved;
    static TablePatternOnce s_moved_product;
    static TablePatternOnce s_shifted;
    static TablePatternOnce s_shifted_product;
    StickWord operation;
    operation.text[0] = '\0';
    writing->text[0] = '\0';
    StickExpr text;
    STICK_SET(text, instruction);
    if (stick_ends(text.text, ";"))
    {
        text.text[stick_length(text.text) - 1u] = '\0';
        text = stick_squeeze(text.text);
    }
    TableMatch found;
    if (stick_matched(&s_carried_add,
                      "IADD3\\.X (R[0-9]+(?:\\.hi)?), (R[0-9]+(?:\\.hi)?), (R[0-9]+(?:\\.hi)?), RZ, (P[0-6]), !PT",
                      text.text, &found))
    {
        STICK_SET(*writing, "IMAD.X ");
        STICK_ADD(*writing, stick_group(&found, 1u, text.text).text);
        STICK_ADD(*writing, ", ");
        STICK_ADD(*writing, stick_group(&found, 2u, text.text).text);
        STICK_ADD(*writing, ", 0x1, ");
        STICK_ADD(*writing, stick_group(&found, 3u, text.text).text);
        STICK_ADD(*writing, ", ");
        STICK_ADD(*writing, stick_group(&found, 4u, text.text).text);
        STICK_SET(operation, "IADD3.X");
        return operation;
    }
    if (stick_matched(&s_carried_product,
                      "IMAD\\.X (R[0-9]+(?:\\.hi)?), (R[0-9]+(?:\\.hi)?), 0x1, (R[0-9]+(?:\\.hi)?), (P[0-6])",
                      text.text, &found))
    {
        *writing = text;
        STICK_SET(operation, "IADD3.X");
        return operation;
    }
    if (stick_matched(&s_moved, "MOV (R[0-9]+), (c\\[.+\\])", text.text, &found) ||
        stick_matched(&s_moved_product, "IMAD\\.MOV\\.U32 (R[0-9]+), RZ, RZ, (c\\[.+\\])", text.text, &found))
    {
        STICK_SET(*writing, "IMAD.MOV.U32 ");
        STICK_ADD(*writing, stick_group(&found, 1u, text.text).text);
        STICK_ADD(*writing, ", RZ, RZ, ");
        STICK_ADD(*writing, stick_group(&found, 2u, text.text).text);
        STICK_SET(operation, "MOV");
        return operation;
    }
    if (stick_matched(&s_shifted, "SHF\\.L\\.U32 (R[0-9]+), (R[0-9]+), (0x[0-9a-f]+|[0-9]+), RZ", text.text, &found))
    {
        const unsigned long long bits = EMBED_CALL(
            cellul.to_ulong_in, TransfiguroCfg, .src = stick_group(&found, 3u, text.text).text, .end = NULL, .base = 0);
        STICK_SET(*writing, "IMAD.SHL.U32 ");
        STICK_ADD(*writing, stick_group(&found, 1u, text.text).text);
        STICK_ADD(*writing, ", ");
        STICK_ADD(*writing, stick_group(&found, 2u, text.text).text);
        STICK_ADD(*writing, ", ");
        STICK_ADD_DECIMAL(*writing, 1ULL << bits);
        STICK_ADD(*writing, ", RZ");
        STICK_SET(operation, "SHF.L.U32");
        return operation;
    }
    if (stick_matched(&s_shifted_product, "IMAD\\.SHL\\.U32 (R[0-9]+), (R[0-9]+), (0x[0-9a-f]+|[0-9]+), RZ", text.text,
                      &found))
    {
        *writing = text;
        STICK_SET(operation, "SHF.L.U32");
        return operation;
    }
    return operation;
}

// the operations of `count` instructions at `lines`, each squeezed, on the integer pipe and on the FMA pipe, those the
// system writes on either pipe left out
struct StickLane
{
    unsigned int integer;
    unsigned int fma;
};

static StickLane stick_pipe_lane(const StickLine *lines, unsigned long long count)
{
    StickLane lane = {0u, 0u};
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        const char *const instruction = lines[at].text;
        StickExpr writing;
        if ((instruction[0] == '\0') || stick_ends(instruction, ":") ||
            (stick_pipe_choice(instruction, &writing).text[0] != '\0'))
        {
            continue;
        }
        StickWord operation;
        STICK_SET_N(operation, instruction, stick_place_of(instruction, ' ', 0u, stick_length(instruction)));
        const int pipe = stick_pipe_of(operation.text);
        lane.integer += (pipe == 1) ? 1u : 0u;
        lane.fma += (pipe == 2) ? 1u : 0u;
    }
    return lane;
}

// The pattern of `squeezed`, a line of the text of sass.krs written with the stand-ins \x01 to \x09 for its
// parameters, read into `pattern`. It matches the line written with any operands, with the operand of each parameter
// a group of its own. 1, or 0 where it does not read
static int stick_line_pattern(const char *squeezed, TablePattern *pattern)
{
    static char s_source[4u * STICK_EXPR_BYTES];
    VerbaLinea line = mmgr_line_over(s_source, sizeof(s_source));
    const size_t length = stick_length(squeezed);
    size_t at = 0u;
    while (at < length)
    {
        const size_t run = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &squeezed[at], .cap = length - at,
                                      .other = "\x01\x02\x03\x04\x05\x06\x07\x08\x09.^$|()[]{}*+?\\");
        (void)mmgr_line_put_n(&line, &squeezed[at], run);
        at += run;
        if (at >= length)
        {
            break;
        }
        if ((squeezed[at] >= '\x01') && (squeezed[at] <= '\x09'))
        {
            mmgr_line_put(&line, "([^,\\[\\]\\s;]+)");
        }
        else
        {
            mmgr_line_ch(&line, '\\');
            mmgr_line_ch(&line, squeezed[at]);
        }
        at += 1u;
    }
    const int fits = mmgr_line_ok(&line) != 0;
    (void)mmgr_line_finish(&line);
    return fits && table_pattern_whole(pattern, s_source);
}

// The place of the one && or || that joins two flags outside every parenthesis of `bare`, into `joined`. 1, or 0 where
// `bare` joins no two flags so, or more than two. The walk jumps from mark to mark
static int stick_flags_joined(const char *bare, size_t *joined)
{
    const size_t length = stick_length(bare);
    int depth = 0;
    int found = 0;
    size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = bare, .cap = length, .other = "()[]&|");
    while ((at + 1u) < length)
    {
        const char character = bare[at];
        depth += ((character == '(') || (character == '[')) ? 1 : 0;
        depth -= ((character == ')') || (character == ']')) ? 1 : 0;
        const int join = (depth == 0) && (((character == '&') || (character == '|')) && (bare[at + 1u] == character));
        if (join)
        {
            if (found)
            {
                return 0;
            }
            found = 1;
            *joined = at;
            at += 1u;
        }
        at += 1u;
        at += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &bare[at], .cap = length - at, .other = "()[]&|");
    }
    return found;
}

// `bare`, two flags C joins by one && or one || outside every parenthesis, written with the two exchanged; empty where
// `bare` joins no two flags so
static StickExpr stick_flags_exchanged(const char *bare)
{
    StickExpr exchanged;
    exchanged.text[0] = '\0';
    size_t joined = 0u;
    if (!stick_flags_joined(bare, &joined))
    {
        return exchanged;
    }
    STICK_SET(exchanged, &bare[joined + 2u]);
    STICK_ADD_N(exchanged, &bare[joined], 2u);
    STICK_ADD_N(exchanged, bare, joined);
    return exchanged;
}

// `bare`, a comparison of C, written as its complement: its outermost comparison's operator exchanged for the one C
// gives the opposite answer, == for !=, < for >=, > for <=. Empty where `bare` is no comparison
static StickExpr stick_complement(const char *bare)
{
    StickExpr complement;
    complement.text[0] = '\0';
    StickExpr inner;
    STICK_SET(inner, bare);
    while (stick_wrapped(inner.text))
    {
        StickExpr taken;
        STICK_SET_N(taken, &inner.text[1], stick_length(inner.text) - 2u);
        inner = taken;
    }
    static const char *const s_pairs[][2] = {{"==", "!="}, {"!=", "=="}, {"<=", ">"},
                                             {">=", "<"},  {"<", ">="},  {">", "<="}};
    const size_t length = stick_length(inner.text);
    int depth = 0;
    size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = inner.text, .cap = length, .other = "()[]<>=!");
    while (at < length)
    {
        const char character = inner.text[at];
        depth += ((character == '(') || (character == '[')) ? 1 : 0;
        depth -= ((character == ')') || (character == ']')) ? 1 : 0;
        const int shift = ((character == '<') || (character == '>')) &&
                          ((((at + 1u) < length) && (inner.text[at + 1u] == character)) ||
                           ((at > 0u) && (inner.text[at - 1u] == character)));
        for (unsigned int pair = 0u; (depth == 0) && !shift && (pair < 6u); pair += 1u)
        {
            if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = &inner.text[at], .cap = length - at,
                           .other = s_pairs[pair][0], .ci = EMBED_FALSE))
            {
                STICK_SET(complement, "(");
                STICK_ADD_N(complement, inner.text, at);
                STICK_ADD(complement, s_pairs[pair][1]);
                STICK_ADD(complement, &inner.text[at + stick_length(s_pairs[pair][0])]);
                STICK_ADD(complement, ")");
                return complement;
            }
        }
        at += 1u;
        at += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &inner.text[at], .cap = length - at,
                         .other = "()[]<>=!");
    }
    return complement;
}

// the count of the places in the terminated `text` that name `name` with no digit after it. Where `before` is 1 nothing
// that joins a name goes before it, and where `high` is 1 no `.hi` goes after it
static unsigned int stick_named_count(const char *text, const char *name, int before, int high)
{
    const size_t length = stick_length(text);
    const size_t size = stick_length(name);
    unsigned int count = 0u;
    for (size_t at = stick_place(text, name, 0u, length); at < length; at = stick_place(text, name, at + 1u, length))
    {
        const size_t end = at + size;
        // a byte of the text read as the unsigned byte the class takes
        const int digit_after = (end < length) && (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM,
                                                              .byte = (unsigned char)text[end]) != 0);
        const int joined_before = before && (at > 0u) && stick_joins(text[at - 1u]);
        const int high_after = high && EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = &text[end],
                                                  .cap = length - end, .other = ".hi", .ci = EMBED_FALSE);
        count += (!digit_after && !joined_before && !high_after) ? 1u : 0u;
    }
    return count;
}

// ---- the stacks of a read ----

// The texts a read passes on to its choices, and the options each choice weighs, each a stack. A read puts its own on
// top and takes them off before it returns, and a read inside it puts its own above them
#define STICK_TEXTS_MAX 4096u
#define STICK_OPTIONS_MAX 4096u
static StickExpr s_texts[STICK_TEXTS_MAX];
static unsigned int s_text_top = 0u;
static StickReading s_options[STICK_OPTIONS_MAX];
static unsigned int s_option_top = 0u;

// 1 once a stack or a table of the engine found no room. The kernel then puts the question that it passes the room of
// the engine
static int s_stick_room = 0;

// `count` texts taken off the top of s_texts [BORROWS], NULL where they find no room
static StickExpr *stick_texts_taken(unsigned int count)
{
    if ((s_text_top + count) > STICK_TEXTS_MAX)
    {
        s_stick_room = 1;
        return NULL;
    }
    StickExpr *const texts = &s_texts[s_text_top];
    s_text_top += count;
    return texts;
}

// ---- the records of the register assignment ----

// One name of the kernel, by the number it holds. It has the register it got, and the first and last lines that name
// it. It has the name whose pair it goes into. It has 1 where it is a wide, where it is a word a wide's pair begins,
// and where it has a register
struct StickRoot
{
    unsigned int given;
    unsigned int first_line;
    unsigned int last_line;
    unsigned int named_last;
    unsigned int tied_to;
    int wide;
    int paired;
    int named;
};

// one place the text names a name: its line and the name's root
struct StickOccurrence
{
    unsigned int line;
    unsigned int root;
};

// a name's claim, by the line it is first named on and the name, which the register lines go in the order of
struct StickClaim
{
    unsigned int first_line;
    unsigned int root;
    StickWord name;
};

// a register a form names by its own number, 1 where a pair begins there, and the register it moves to
struct StickPin
{
    unsigned int number;
    int pair;
    unsigned int to;
};

// the words of the bitmap of free registers, 64 registers a word
#define STICK_FREE_WORDS 64u

// the order of two terminated texts, as strcmp orders them: below 0, 0 or above 0
static int stick_order_text(const char *left, const char *right)
{
    const size_t left_length = stick_length(left);
    const size_t right_length = stick_length(right);
    const size_t shorter = ((left_length < right_length) ? left_length : right_length) + 1u;
    const size_t at = EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = shorter, .other = right);
    if (at >= shorter)
    {
        return 0;
    }
    // each byte read as the unsigned byte strcmp orders by
    return (int)(unsigned char)left[at] - (int)(unsigned char)right[at];
}

// the claims in the order of their first lines, then of their names
static int stick_claim_order(const void *left, const void *right)
{
    const StickClaim *const one = (const StickClaim *)left;
    const StickClaim *const other = (const StickClaim *)right;
    if (one->first_line != other->first_line)
    {
        return (one->first_line < other->first_line) ? -1 : 1;
    }
    return stick_order_text(one->name.text, other->name.text);
}

// the pins in the order of their numbers
static int stick_pin_order(const void *left, const void *right)
{
    const unsigned int one = ((const StickPin *)left)->number;
    const unsigned int other = ((const StickPin *)right)->number;
    return (one == other) ? 0 : ((one < other) ? -1 : 1);
}

// the pin of `number` among the `count` pins at `pins` in the order of their numbers [BORROWS], NULL where none
static const StickPin *stick_pin_of(const StickPin *pins, unsigned int count, unsigned int number)
{
    unsigned int low = 0u;
    unsigned int high = count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (pins[middle].number < number)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < count) && (pins[low].number == number)) ? &pins[low] : NULL;
}

// The `count` elements of `size` bytes at `elements` put in the order of `order`, stable. The sort goes through a spare
// cell of the temporary tier. 1, or 0 where the pool cannot meet the spare
static int stick_sorted(void *elements, unsigned long long count, unsigned long long size,
                        int (*order)(const void *left, const void *right))
{
    if (count < 2ULL)
    {
        return 1;
    }
    void *const spare = platform_cell_taken(count * size, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (spare == NULL)
    {
        return 0;
    }
    // the elements of one cell are far below what a size_t counts
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = elements, .bytes = (size_t)size, .scratch = spare,
                     .count = (size_t)count, .order = order);
    platform_cell_released(spare, platform_place_g_program_owner);
    return 1;
}

// 1 where the `length` bytes at `text` at `at` are a name of the kernel, `%r` or `%w` and its number. The number goes
// into `number`, and the end of the name into `end` where it is not NULL
static int stick_name_at(const char *text, size_t length, size_t at, unsigned int *number, size_t *end)
{
    if (((at + 2u) >= length) || (text[at] != '%') || ((text[at + 1u] != 'r') && (text[at + 1u] != 'w')))
    {
        return 0;
    }
    const size_t digits =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at + 2u], .cap = length - at - 2u, .other = "0123456789");
    if (digits == 0u)
    {
        return 0;
    }
    // a name's number is far below 2^32
    *number =
        (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &text[at + 2u], .end = NULL, .base = 10);
    if (end != NULL)
    {
        *end = at + 2u + digits;
    }
    return 1;
}

// the number the terminated name `name` holds, or 0xFFFFFFFF where it is no name of the kernel
static unsigned int stick_name_number(const char *name)
{
    unsigned int number = 0u;
    return stick_name_at(name, stick_length(name), 0u, &number, NULL) ? number : 0xFFFFFFFFu;
}

// The register a name takes from the free registers `bits`. A word takes the lowest free one. A wide, or a word a
// wide's pair begins, takes the lowest free even pair. Where none is free, the one at the high water mark, which rises
// over it
static unsigned int stick_free_taken(unsigned long long *bits, int even, unsigned int *high_water)
{
    const unsigned int words = (*high_water / 64u) + 1u;
    for (unsigned int word = 0u; (word < words) && (word < STICK_FREE_WORDS); word += 1u)
    {
        const unsigned long long free_here = bits[word];
        const unsigned long long mask = even ? (free_here & (free_here >> 1u) & 0x5555555555555555ULL) : free_here;
        if (mask == 0ULL)
        {
            continue;
        }
        // a register's place in a word of 64 is below 64
        const unsigned int bit = (unsigned int)EMBED_CALL(clz.trail, ClzCfg, .val = mask);
        bits[word] &= ~(1ULL << bit);
        if (even)
        {
            bits[word] &= ~(1ULL << (bit + 1u));
        }
        return (word * 64u) + bit;
    }
    unsigned int number = *high_water;
    if (even && ((*high_water % 2u) != 0u))
    {
        if ((*high_water / 64u) < STICK_FREE_WORDS)
        {
            bits[*high_water / 64u] |= 1ULL << (*high_water % 64u);
        }
        number = *high_water + 1u;
    }
    *high_water = number + (even ? 2u : 1u);
    s_stick_room |= (*high_water >= (STICK_FREE_WORDS * 64u)) ? 1 : 0;
    return number;
}

// the register `number` back among the free registers `bits`, and the one past it where `even` is 1
static void stick_free_given(unsigned long long *bits, unsigned int number, int even)
{
    for (unsigned int at = number; at < (number + (even ? 2u : 1u)); at += 1u)
    {
        if ((at / 64u) < STICK_FREE_WORDS)
        {
            bits[at / 64u] |= 1ULL << (at % 64u);
        }
    }
}

// the line, counted from 1, that a claim at line `line` of the text before goes to: the next kept line, or 0
static unsigned int stick_claimed_at(const unsigned int *retained_at, unsigned int count, unsigned int line)
{
    while ((line < count) && (retained_at[line] == 0u))
    {
        line += 1u;
    }
    return (line < count) ? retained_at[line] : 0u;
}

// the line, counted from 1, that a release at line `line` of the text before goes to: the kept line at it or before
// it, or 0
static unsigned int stick_released_at(const unsigned int *retained_at, unsigned int count, unsigned int line)
{
    unsigned int at = (line < count) ? (line + 1u) : count;
    while ((at != 0u) && (retained_at[at - 1u] == 0u))
    {
        at -= 1u;
    }
    return (at != 0u) ? retained_at[at - 1u] : 0u;
}

// 1 where the `length` bytes at `line` name the fixed register `binding`: no character, digit or `_` before it, and no
// digit after it
static int stick_fixed_named(const char *line, size_t length, const char *binding)
{
    const size_t size = stick_length(binding);
    for (size_t at = 0u; (at + size) <= length;)
    {
        const char *const found =
            EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &line[at], .cap = length - at, .other = binding,
                       .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        if (found == NULL)
        {
            return 0;
        }
        // a found needle lies at or past its search's start
        const size_t place = (size_t)(found - line);
        const size_t end = place + size;
        // each byte read as the unsigned byte the class takes
        const int before =
            (place != 0u) &&
            ((EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (unsigned char)line[place - 1u]) != 0) ||
             (line[place - 1u] == '_'));
        const int after = (end < length) && (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM,
                                                        .byte = (unsigned char)line[end]) != 0);
        if (!before && !after)
        {
            return 1;
        }
        at = place + 1u;
    }
    return 0;
}

// the scratch of a write of the kernel: a fresh register of the kernel's of the bank's width
static int stick_scratch_fresh(void *context, const char *bank, VerbaLinea *name);
// ---- one kernel ----

// the bytes of a kernel's question, its terminator among them
#define STICK_QUESTION_BYTES 1024u

// the tokens of the expression the parser reads. One parser reads at a time
static StickTokens s_tokens;

// A kernel read and written. It holds the rulesets, the machine file that gates a form, and whether it folds. Then its
// parameters, its text and the question it puts, both movable texts. Then the count of its forms and of the registers
// it names, and the lines of those registers. Then its tables, each one cell of the temporary tier taken once at the
// room the kernel's statements and tokens give it
struct StickKernel
{
    const Ruleset *cu;
    const Ruleset *sass;
    const SassMachine *machine;
    int folding;
    StickParameter parameter[STICK_PARAMETERS_MAX];
    unsigned int parameter_count;
    PlatformText text;
    char question[STICK_QUESTION_BYTES];
    unsigned int forms;
    unsigned int register_count;
    PlatformText register_lines;
    unsigned int next;
    unsigned int predicates;
    unsigned int steps_stored;
    unsigned int token_at;
    // each value the kernel stores, by the text of what it is
    StickBound *bound;
    unsigned int bound_count;
    unsigned int bound_room;
    // each bool stored as a flag, each name read as an expression not yet written, and each name never negative
    StickNamed *flags;
    unsigned int flag_count;
    StickNamed *located;
    unsigned int located_count;
    StickWord *nonnegatives;
    unsigned int nonnegative_count;
    unsigned int named_room;
    // each wide that is the pair a word begins, each word moved into another, and what the listing of nvcc must show
    StickTie *ties;
    unsigned int tie_count;
    StickCopy *copies;
    unsigned int copy_count;
    StickAsked *asked;
    unsigned int asked_count;
    unsigned int written_room;

    // ---- its tables ----

    // the value stored by `key` [BORROWS], NULL where the kernel stores none
    StickBound *bound_of(const char *key)
    {
        const size_t length = stick_length(key);
        for (unsigned int at = 0u; at < bound_count; at += 1u)
        {
            if (stick_same_n(bound[at].key.text, stick_length(bound[at].key.text), key) &&
                (stick_length(bound[at].key.text) == length))
            {
                return &bound[at];
            }
        }
        return NULL;
    }

    // `value` stored by `key`, in place of what it had before
    void bound_put(const char *key, const StickValue &value)
    {
        StickBound *const held = bound_of(key);
        if (held != NULL)
        {
            held->value = value;
            return;
        }
        if (bound_count >= bound_room)
        {
            s_stick_room = 1;
            return;
        }
        STICK_SET(bound[bound_count].key, key);
        bound[bound_count].value = value;
        bound_count += 1u;
    }

    // the typed expression `named` holds of `name` [BORROWS], NULL where it holds none
    static StickNamed *named_of(StickNamed *named, unsigned int count, const char *name)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            if (stick_same(named[at].name.text, name))
            {
                return &named[at];
            }
        }
        return NULL;
    }

    // `typed` stored by `name` in `named`, in place of what it had before
    void named_put(StickNamed *named, unsigned int *count, const char *name, const StickTyped &typed)
    {
        StickNamed *const held = named_of(named, *count, name);
        if (held != NULL)
        {
            held->typed = typed;
            return;
        }
        if (*count >= named_room)
        {
            s_stick_room = 1;
            return;
        }
        STICK_SET(named[*count].name, name);
        named[*count].typed = typed;
        *count += 1u;
    }

    // `name` let go of in `named`, the last record moved into its place
    static void named_dropped(StickNamed *named, unsigned int *count, const char *name)
    {
        StickNamed *const held = named_of(named, *count, name);
        if (held != NULL)
        {
            *held = named[*count - 1u];
            *count -= 1u;
        }
    }

    // 1 where the kernel holds `name` as a name never negative
    int nonnegative_held(const char *name) const
    {
        for (unsigned int at = 0u; at < nonnegative_count; at += 1u)
        {
            if (stick_same(nonnegatives[at].text, name))
            {
                return 1;
            }
        }
        return 0;
    }

    // `name` stored as never negative where `held` is 1, and let go of where it is 0
    void nonnegative_set(const char *name, int held)
    {
        for (unsigned int at = 0u; at < nonnegative_count; at += 1u)
        {
            if (stick_same(nonnegatives[at].text, name))
            {
                nonnegatives[at] = nonnegatives[nonnegative_count - 1u];
                nonnegative_count -= 1u;
                break;
            }
        }
        if (!held)
        {
            return;
        }
        if (nonnegative_count >= named_room)
        {
            s_stick_room = 1;
            return;
        }
        STICK_SET(nonnegatives[nonnegative_count], name);
        nonnegative_count += 1u;
    }

    // what the listing of nvcc must show of `form`, of `type`, kept for the round
    void asked_kept(StickAskedType type, unsigned int form, unsigned int parameter, unsigned int number,
                    const char *word)
    {
        if (asked_count >= written_room)
        {
            s_stick_room = 1;
            return;
        }
        StickAsked *const kept = &asked[asked_count];
        asked_count += 1u;
        kept->form = form;
        kept->parameter = parameter;
        kept->number = number;
        STICK_SET(kept->word, word);
        // the type sits in the parameter's high bits, and a parameter's place is far below 2^24
        kept->parameter |= (unsigned int)type << 24u;
    }

    // the parameter of the kernel named `name` [BORROWS], NULL where it has none
    const StickParameter *parameter_of(const char *name) const
    {
        for (unsigned int at = 0u; at < parameter_count; at += 1u)
        {
            if (stick_same(parameter[at].name.text, name))
            {
                return &parameter[at];
            }
        }
        return NULL;
    }

    // ---- what it stores ----

    // `why` recorded as the kernel's question where it has none yet
    void note_question(const char *why, const char *more = "", const char *tail = "")
    {
        VerbaLinea *const line = stick_trace_begun();
        mmgr_line_put(line, "question ");
        mmgr_line_put(line, why);
        mmgr_line_put(line, more);
        mmgr_line_put(line, tail);
        stick_trace_written(line);
        question_put(why, more, tail);
    }

    // `why`, `more` and `tail` put as the kernel's question where it has none, with no line on the trace
    void question_put(const char *why, const char *more = "", const char *tail = "")
    {
        if (question[0] == '\0')
        {
            VerbaLinea said = mmgr_line_over(question, sizeof(question));
            mmgr_line_put(&said, why);
            mmgr_line_put(&said, more);
            mmgr_line_put(&said, tail);
            (void)mmgr_line_finish(&said);
        }
    }

    // `why` recorded as the kernel's question, and the expression left untyped
    StickTyped refuse(const char *why, const char *more = "")
    {
        note_question(why, more);
        return stick_typed("", -1, 0);
    }

    // `number` as cu.krs's bank of immediates writes it
    StickExpr number_text(unsigned int number) const
    {
        const InstrTemplate &bank = cu->banks[REGCLASS_IMMEDIATE];
        StickExpr text = stick_bare(bank.pieces[0]);
        STICK_ADD_DECIMAL(text, number);
        STICK_ADD(text, stick_bare(bank.pieces[bank.placeholder_count]).text);
        return text;
    }

    // 1 where `bare` is a number as cu.krs's bank of immediates writes one, its value in `number`
    int number_of(const char *bare, unsigned int *number) const
    {
        const InstrTemplate &bank = cu->banks[REGCLASS_IMMEDIATE];
        const StickExpr before = stick_bare(bank.pieces[0]);
        const StickExpr after = stick_bare(bank.pieces[bank.placeholder_count]);
        const size_t length = stick_length(bare);
        const size_t before_length = stick_length(before.text);
        const size_t after_length = stick_length(after.text);
        if ((length <= (before_length + after_length)) || !stick_begins(bare, before.text) ||
            !stick_ends(bare, after.text))
        {
            return 0;
        }
        StickExpr digits;
        STICK_SET_N(digits, &bare[before_length], length - before_length - after_length);
        const char *end = NULL;
        const unsigned long long value =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = digits.text, .end = &end, .base = 0);
        if ((end == NULL) || (*end != '\0') || (value > 0xFFFFFFFFULL))
        {
            return 0;
        }
        // the value is a word, checked above
        *number = (unsigned int)value;
        return 1;
    }

    // 1 where this kernel folds and sass.ksc contains a fold of `type` put for form `form`'s parameter `parameter`
    int folded(unsigned int form, const char *type, unsigned int parameter) const
    {
        for (unsigned long long at = 0ULL; at < sass->folds.count; at += 1ULL)
        {
            const RulesetFold *const fold = (const RulesetFold *)platform_array_at(&sass->folds, at);
            if ((folding != 0) && (fold->form == form) && stick_same(fold->type, type) &&
                (fold->parameter == parameter))
            {
                return 1;
            }
        }
        return 0;
    }

    // 1 where the kernel stores `name`
    int binds(const char *name)
    {
        return bound_of(name) != NULL;
    }

    // A fresh register of `bits`. For a predicate, it is one of the predicates. For a word or a wide, it is one that
    // the kernel names until assign() gives it a register of the file
    StickValue fresh(unsigned int bits)
    {
        if (bits == STICK_PREDICATE_BITS)
        {
            predicates += 1u;
            return stick_value(stick_register(sass, "predicate", predicates - 1u).text, bits, 0);
        }
        next += 1u;
        StickWord name;
        STICK_SET(name, (bits == 64u) ? "%w" : "%r");
        STICK_ADD_DECIMAL(name, next - 1u);
        return stick_value(name.text, bits, 0);
    }

    // The readings the kernel made since it last stored a value let go of
    static void cached_cleared(void)
    {
        s_cached_count = 0u;
    }

    // `name`, stored in `value` as a value of type `type`, or as a word of no type of C where `type` is -1. It is
    // never negative where `nonnegative` is not 0.
    // A name stored anew reads from `value` alone, whatever was in it before. The code lets go of each stored
    // expression that reads the name. Its value is of what the name stored at the time of its read
    void bind(const char *name, StickValue value, int type, int nonnegative = 0)
    {
        value.is_signed = (type >= 0) ? s_types[type].is_signed : 0;
        value.type = type;
        unsigned int at = 0u;
        while (at < bound_count)
        {
            if (stick_reads_name(bound[at].key.text, name))
            {
                bound[at] = bound[bound_count - 1u];
                bound_count -= 1u;
                continue;
            }
            at += 1u;
        }
        bound_put(name, value);
        cached_cleared();
        named_dropped(located, &located_count, name);
        named_dropped(flags, &flag_count, name);
        nonnegative_set(name, nonnegative != 0);
    }

    // `name`, a bool, stored as the flag that `flag` reads as. The code reads it where the read of the name is. It
    // reads it with the word of 0 and 1 that it tests off, where it is one
    void bind_flag(const char *name, const StickTyped &flag)
    {
        StickTyped binding = stick_typed("(", flag.type, 0);
        STICK_ADD(binding.text, flag.text.text);
        STICK_ADD(binding.text, ")");
        binding.word = flag.word;
        binding.word_type = flag.word_type;
        named_put(flags, &flag_count, name, binding);
    }

    // `name` read as `value`. The code writes it where the read of the name is, and only as much of it as that read
    // takes
    void are(const char *name, const StickTyped &value)
    {
        named_put(located, &located_count, name, value);
    }

    // ---- the types of C over it ----

    // `expression`, typed, and written out with each conversion that C makes in it, for the reader to read through
    // cu.krs. Type -1 where it has no type, with the reason in `question`
    StickTyped typed(const char *expression)
    {
        if (!stick_tokens(expression, &s_tokens))
        {
            s_stick_room = 1;
            return refuse("an expression of more tokens than the engine holds: ", expression);
        }
        token_at = 0u;
        const StickTyped whole = conditional();
        if ((whole.type >= 0) && (token_at != s_tokens.count))
        {
            return refuse("a token nothing here types: ", token_shown(token_at).text);
        }
        return whole;
    }

    // the token at `at` as the source shows it, an operator between backticks
    StickExpr token_shown(unsigned int at) const
    {
        StickExpr shown;
        const StickToken *const token = &s_tokens.token[at];
        STICK_SET(shown, token->operation ? "`" : "");
        STICK_ADD(shown, token->text.text);
        STICK_ADD(shown, token->operation ? "`" : "");
        return shown;
    }

    // 1 where the token at `at` is the operator `operation`
    int token_is(unsigned int at, const char *operation) const
    {
        return (at < s_tokens.count) && (s_tokens.token[at].operation != 0) &&
               stick_same(s_tokens.token[at].text.text, operation);
    }

    // the operator the token at `at` holds, empty where it holds a name or a number
    const char *token_operation(unsigned int at) const
    {
        return ((at < s_tokens.count) && (s_tokens.token[at].operation != 0)) ? s_tokens.token[at].text.text : "";
    }

    // `value` converted to type `to` as C converts it:
    //   - a bool is a flag, and its segment is the select of 1 and 0 over it.
    //   - a value converted to a bool is the flag of its difference from 0.
    //   - a value of the width of a register, converted to another of that width, is the same register.
    //   - a word converted to a pair widens as its sign says.
    //   - a pair converted to a word is its low word.
    StickTyped converted(const StickTyped &value, int to)
    {
        if ((value.type < 0) || (value.type == to))
        {
            return value;
        }
        const int flag = stick_type_bare("bool");
        if (to == flag)
        {
            StickTyped tested = combined(value, "!=", stick_typed(number_text(0u).text, stick_type_bare("int"), 1));
            tested.word = value.text;
            tested.word.text[0] = (value.zero_one != 0) ? tested.word.text[0] : '\0';
            tested.word_type = (value.zero_one != 0) ? value.type : -1;
            return tested;
        }
        if ((value.type == flag) && (value.word.text[0] != '\0'))
        {
            StickTyped word = stick_typed(value.word.text, value.word_type, 1);
            word.zero_one = 1;
            return converted(word, to);
        }
        if (value.type == flag)
        {
            StickTyped selected = stick_typed("((", stick_type_bare("unsignedint"), 1);
            STICK_ADD(selected.text, value.text.text);
            STICK_ADD(selected.text, ")?");
            STICK_ADD(selected.text, number_text(1u).text);
            STICK_ADD(selected.text, ":");
            STICK_ADD(selected.text, number_text(0u).text);
            STICK_ADD(selected.text, ")");
            selected.zero_one = 1;
            return converted(selected, to);
        }
        const unsigned int from_bits = s_types[value.type].bits;
        const unsigned int to_bits = s_types[to].bits;
        // A byte or a halfword sits in a word, extended as its sign says. It is the narrowing of the low bits of the
        // word
        if (to_bits < STICK_REGISTER_BITS)
        {
            const int whole = (value.low_bits < to_bits);
            StickExpr low;
            STICK_SET(low, (whole && (from_bits > STICK_REGISTER_BITS)) ? "(unsignedint)" : "");
            STICK_ADD(low, whole ? value.text.text : value.low.text);
            StickTyped narrowed = stick_typed(stick_cast(to).text, to, (s_types[to].is_signed == 0) ? 1 : 0);
            STICK_ADD(narrowed.text, low.text);
            narrowed.low = low;
            narrowed.low_bits = to_bits;
            narrowed.zero_one = value.zero_one;
            return narrowed;
        }
        if (stick_register_bits(from_bits) == stick_register_bits(to_bits))
        {
            StickTyped same = stick_typed(value.text.text, to, value.nonnegative);
            same.low = value.low;
            same.low_bits = value.low_bits;
            same.zero_one = value.zero_one;
            return same;
        }
        if (to_bits > from_bits)
        {
            const int sign = (s_types[value.type].is_signed != 0) && (value.nonnegative == 0);
            StickTyped widened =
                stick_typed(sign ? "(unsignedlonglong)(longlong)(int)" : "(unsignedlonglong)", to, value.nonnegative);
            STICK_ADD(widened.text, value.text.text);
            widened.zero_one = value.zero_one;
            return widened;
        }
        StickTyped cut = stick_typed(stick_cast(to).text, to, 0);
        STICK_ADD(cut.text, value.text.text);
        cut.zero_one = value.zero_one;
        return cut;
    }

    // The type of `name`. It is a value or a parameter that the kernel stores, or a built-in variable that cu.krs reads
    // through a form of no parameters. It is of the type of a register of the width of the form. -1 where it has none
    int type_of(const char *name)
    {
        const StickBound *const binding = bound_of(name);
        if ((binding != NULL) && (binding->value.type >= 0))
        {
            return binding->value.type;
        }
        const StickParameter *const given = parameter_of(name);
        if (given != NULL)
        {
            return given->type;
        }
        StickReading reading;
        if ((read(name, 0, &reading) != 0) && (reading.leaf == STICK_FORM) && (reading.child_count == 0u))
        {
            return stick_type_bare((reading.bits == 64u) ? "unsignedlonglong" : "unsignedint");
        }
        return -1;
    }

    // C's conditional, `where ? chosen : otherwise`, or what binds tighter
    StickTyped conditional(void)
    {
        const StickTyped where = binary(1);
        if ((where.type < 0) || !token_is(token_at, "?"))
        {
            return where;
        }
        token_at += 1u;
        const StickTyped chosen = conditional();
        if ((chosen.type < 0) || !token_is(token_at, ":"))
        {
            return (chosen.type < 0) ? chosen : refuse("a conditional with no `:`");
        }
        token_at += 1u;
        const StickTyped otherwise = conditional();
        if (otherwise.type < 0)
        {
            return otherwise;
        }
        const int common = stick_common(stick_promoted(chosen.type), stick_promoted(otherwise.type));
        const StickTyped flag = converted(where, stick_type_bare("bool"));
        const StickTyped left = converted(chosen, common);
        const StickTyped right = converted(otherwise, common);
        if ((flag.type < 0) || (left.type < 0) || (right.type < 0))
        {
            return stick_typed("", -1, 0);
        }
        // a select between two words of 0 and 1 gives 0 or 1
        StickTyped selected = stick_typed("((", common, 0);
        STICK_ADD(selected.text, flag.text.text);
        STICK_ADD(selected.text, ")?");
        STICK_ADD(selected.text, left.text.text);
        STICK_ADD(selected.text, ":");
        STICK_ADD(selected.text, right.text.text);
        STICK_ADD(selected.text, ")");
        selected.zero_one = (left.zero_one != 0) && (right.zero_one != 0);
        return selected;
    }

    // the binary operators that bind at `lowest` or tighter, each left to right over what binds tighter still
    StickTyped binary(int lowest)
    {
        StickTyped left = unary();
        while ((left.type >= 0) && (token_at < s_tokens.count))
        {
            StickWord token;
            STICK_SET(token, token_operation(token_at));
            const int precedence = (token.text[0] != '\0') ? stick_precedence(token.text) : 0;
            if ((precedence == 0) || (precedence < lowest))
            {
                break;
            }
            token_at += 1u;
            const StickTyped right = binary(precedence + 1);
            if (right.type < 0)
            {
                return right;
            }
            left = combined(left, token.text, right);
        }
        return left;
    }

    // `left` and `right` under binary operator `token`, each converted as C converts it:
    //   - the logical operators take two flags and give one.
    //   - a shift takes each side promoted, and gives the type of the left.
    //   - each other operator takes both sides in their common type, and a comparison gives a flag.
    //
    // An ordering, a quotient, a remainder and a right shift are of the type of their operands. The code writes a
    // signed one with the cast of its type before each operand, which the signed forms read. Equality reads no sign,
    // and takes no cast
    StickTyped combined(const StickTyped &left, const char *token, const StickTyped &right)
    {
        const int flag = stick_type_bare("bool");
        if (stick_same(token, "&&") || stick_same(token, "||"))
        {
            const StickTyped first = converted(left, flag);
            const StickTyped second = converted(right, flag);
            if ((first.type < 0) || (second.type < 0))
            {
                return stick_typed("", -1, 0);
            }
            StickTyped joined = stick_typed("(", flag, 0);
            STICK_ADD(joined.text, first.text.text);
            STICK_ADD(joined.text, token);
            STICK_ADD(joined.text, second.text.text);
            STICK_ADD(joined.text, ")");
            return joined;
        }
        const int shift = stick_same(token, "<<") || stick_same(token, ">>");
        const int common =
            shift ? stick_promoted(left.type) : stick_common(stick_promoted(left.type), stick_promoted(right.type));
        StickTyped first = converted(left, common);
        StickTyped second = converted(right, shift ? stick_promoted(right.type) : common);
        if ((first.type < 0) || (second.type < 0))
        {
            return stick_typed("", -1, 0);
        }
        // two values, neither of which is negative, order alike signed and unsigned. The code orders them unsigned
        const int orders = stick_compares(token) && !stick_same(token, "==") && !stick_same(token, "!=") &&
                           ((first.nonnegative == 0) || (second.nonnegative == 0));
        // a quotient and a remainder of two values, neither of which is negative, come out alike signed and unsigned.
        // So does a right shift of a value that is not negative. The code writes them unsigned
        const int divides = (stick_same(token, "/") || stick_same(token, "%")) &&
                            ((first.nonnegative == 0) || (second.nonnegative == 0));
        const int shifts = stick_same(token, ">>") && (first.nonnegative == 0);
        const int signs = (orders || divides || shifts) && (s_types[common].is_signed != 0);
        if (signs)
        {
            StickExpr cast_first;
            STICK_SET(cast_first, stick_cast(common).text);
            STICK_ADD(cast_first, first.text.text);
            first.text = cast_first;
            if (!shift)
            {
                StickExpr cast_second;
                STICK_SET(cast_second, stick_cast(common).text);
                STICK_ADD(cast_second, second.text.text);
                second.text = cast_second;
            }
        }
        StickTyped whole = stick_typed("(", stick_compares(token) ? flag : common, 0);
        STICK_ADD(whole.text, first.text.text);
        STICK_ADD(whole.text, token);
        STICK_ADD(whole.text, second.text.text);
        STICK_ADD(whole.text, ")");
        // the low bits of a sum, a difference, a product, a bitwise operation and a left shift. They come from the low
        // bits of their operands alone. The count of a shift counts whole
        const int left_shift = stick_same(token, "<<");
        const int low_alone = stick_same(token, "+") || stick_same(token, "-") || stick_same(token, "*") ||
                              stick_same(token, "&") || stick_same(token, "|") || stick_same(token, "^") || left_shift;
        if (low_alone && ((first.low_bits != 0u) || (!left_shift && (second.low_bits != 0u))))
        {
            const unsigned int width = stick_register_bits(s_types[common].bits);
            const unsigned int left_bits = (first.low_bits != 0u) ? first.low_bits : width;
            const int right_low = !left_shift && (second.low_bits != 0u);
            const unsigned int right_bits = right_low ? second.low_bits : width;
            STICK_SET(whole.low, "(");
            STICK_ADD(whole.low, (first.low_bits != 0u) ? first.low.text : first.text.text);
            STICK_ADD(whole.low, token);
            STICK_ADD(whole.low, right_low ? second.low.text : second.text.text);
            STICK_ADD(whole.low, ")");
            whole.low_bits = left_shift ? left_bits : ((left_bits < right_bits) ? left_bits : right_bits);
        }
        return whole;
    }

    // `name` stepped by one, with `step` the `+` or the `-` of the step. The code reads the name once. It combines its
    // value and 1 under `step` as C does, and converts the result back to the type of the name. It stores that as the
    // value of the name from here on.
    //
    // `++` and `--` are each an assignment that gives a value, `a += 1` and `a -= 1`. That is the add primitive of the
    // own context of the name, and nothing past it.
    // The read value goes under a name of its own, and comes back through `before`. Type -1 where the name stores no
    // value of C to step. `operation` is 1 where the token stepped is an operator
    StickTyped stepped(const char *name, int operation, const char *step, StickTyped *before)
    {
        const StickNamed *const is = named_of(located, located_count, name);
        const int located_here = is != NULL;
        const StickTyped located_typed = located_here ? is->typed : stick_typed("", -1, 0);
        const int type = located_here ? located_typed.type : type_of(name);
        // a byte of the name read as the unsigned byte the class takes
        if ((type < 0) || (named_of(flags, flag_count, name) != NULL) || operation ||
            (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (unsigned char)name[0]) != 0))
        {
            return refuse("a step of what stores no value of C: ", name);
        }
        const int nonnegative = located_here ? located_typed.nonnegative : nonnegative_held(name);
        const StickValue read_value = value(located_here ? located_typed.text.text : name);
        if (read_value.name.text[0] == '\0')
        {
            return stick_typed("", -1, 0);
        }
        StickWord retained;
        STICK_SET(retained, "stick_retained_");
        STICK_ADD_DECIMAL(retained, steps_stored);
        steps_stored += 1u;
        bind(retained.text, read_value, type, nonnegative);
        *before = stick_typed(retained.text, type, nonnegative);
        const StickTyped one = stick_typed(number_text(1u).text, stick_type_bare("int"), 1);
        const StickTyped sum = converted(combined(*before, step, one), type);
        if (sum.type < 0)
        {
            return sum;
        }
        const StickValue value_bound = value(sum.text.text);
        if (value_bound.name.text[0] == '\0')
        {
            return stick_typed("", -1, 0);
        }
        bind(name, value_bound, type, sum.nonnegative);
        return stick_typed(name, type, sum.nonnegative);
    }

    // a unary operator over what it binds, a cast, or a primary expression. The code reads a step in its order. The
    // operation before the operand gives the value of the operand after the step. The operand before the operation
    // gives the value before it
    StickTyped unary(void)
    {
        if (token_at >= s_tokens.count)
        {
            return refuse("an expression that ends before its operand");
        }
        StickWord token;
        STICK_SET(token, token_operation(token_at));
        if (stick_same(token.text, "++") || stick_same(token.text, "--"))
        {
            token_at += 1u;
            if (token_at >= s_tokens.count)
            {
                return refuse("an expression that ends before its operand");
            }
            const StickToken name = s_tokens.token[token_at];
            token_at += 1u;
            StickTyped before;
            return stepped(name.text.text, name.operation, stick_same(token.text, "++") ? "+" : "-", &before);
        }
        StickWord next;
        STICK_SET(next, token_operation(token_at + 1u));
        if ((token.text[0] == '\0') && (stick_same(next.text, "++") || stick_same(next.text, "--")))
        {
            const StickToken name = s_tokens.token[token_at];
            token_at += 2u;
            StickTyped before;
            const StickTyped after =
                stepped(name.text.text, name.operation, stick_same(next.text, "++") ? "+" : "-", &before);
            return (after.type < 0) ? after : before;
        }
        if (stick_same(token.text, "-") || stick_same(token.text, "~") || stick_same(token.text, "+") ||
            stick_same(token.text, "!"))
        {
            token_at += 1u;
            const StickTyped operand = unary();
            if (operand.type < 0)
            {
                return operand;
            }
            const StickTyped zero = stick_typed(number_text(0u).text, stick_type_bare("int"), 1);
            if (stick_same(token.text, "!"))
            {
                return combined(operand, "==", zero);
            }
            const StickTyped value = converted(operand, stick_promoted(operand.type));
            if (stick_same(token.text, "+"))
            {
                return value;
            }
            // a negation is 0 less its operand, in the operand's type
            if (stick_same(token.text, "-"))
            {
                return combined(zero, "-", value);
            }
            // a complement's low bits are of its operand's low bits alone
            StickTyped whole = stick_typed("(", value.type, 0);
            STICK_ADD(whole.text, token.text);
            STICK_ADD(whole.text, value.text.text);
            STICK_ADD(whole.text, ")");
            if (value.low_bits != 0u)
            {
                STICK_SET(whole.low, "(");
                STICK_ADD(whole.low, token.text);
                STICK_ADD(whole.low, value.low.text);
                STICK_ADD(whole.low, ")");
            }
            whole.low_bits = value.low_bits;
            return whole;
        }
        // a cast is a type's names between parentheses, `(unsigned int)` read as the type `unsignedint` names
        unsigned int close = token_at + 1u;
        StickExpr type_name;
        type_name.text[0] = '\0';
        while (stick_same(token.text, "(") && (close < s_tokens.count) && (s_tokens.token[close].operation == 0))
        {
            STICK_ADD(type_name, s_tokens.token[close].text.text);
            close += 1u;
        }
        const int cast = ((type_name.text[0] != '\0') && token_is(close, ")")) ? stick_type_bare(type_name.text) : -1;
        if (cast >= 0)
        {
            token_at = close + 1u;
            const StickTyped operand = unary();
            return (operand.type < 0) ? operand : converted(operand, cast);
        }
        return primary();
    }

    // a parenthesized expression, a number of C, a name the kernel stores, or a bool stored as a flag
    StickTyped primary(void)
    {
        const StickToken token = s_tokens.token[token_at];
        const StickExpr shown = token_shown(token_at);
        token_at += 1u;
        if ((token.operation != 0) && stick_same(token.text.text, "("))
        {
            const StickTyped inner = conditional();
            if ((inner.type < 0) || !token_is(token_at, ")"))
            {
                return (inner.type < 0) ? inner : refuse("a parenthesis that does not close");
            }
            token_at += 1u;
            return inner;
        }
        // a byte of the token read as the unsigned byte the class takes
        if ((token.operation == 0) &&
            (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (unsigned char)token.text.text[0]) != 0))
        {
            return number_typed(token.text.text);
        }
        if (token_is(token_at, "(") || token_is(token_at, "["))
        {
            return refuse("a call or an element nothing here types: ", shown.text);
        }
        const StickNamed *const flag = named_of(flags, flag_count, shown.text);
        if (flag != NULL)
        {
            return flag->typed;
        }
        const StickNamed *const is = named_of(located, located_count, shown.text);
        if (is != NULL)
        {
            return is->typed;
        }
        const int type = type_of(shown.text);
        return (type < 0) ? refuse("a name nothing here types: ", shown.text)
                          : stick_typed(shown.text, type, nonnegative_held(shown.text));
    }

    // a number of C as the bank of immediates of cu.krs writes it, of the type that its suffix and its size give it.
    // A suffix u gives unsigned, and ll gives long long. Else it is an int where it fits one
    StickTyped number_typed(const char *token)
    {
        const char *end = NULL;
        const unsigned long long number =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = token, .end = &end, .base = 0);
        const char *const suffix = (end == NULL) ? "" : end;
        const int unsigned_one =
            EMBED_CALL(cellul.has, CatenaFinitaCfg, .src = suffix, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "u",
                       .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_TRUE);
        const int wide = EMBED_CALL(cellul.has, CatenaFinitaCfg, .src = suffix, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "ll", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_TRUE) ||
                         (number > 0xFFFFFFFFULL);
        if (number > 0xFFFFFFFFULL)
        {
            return refuse("a number wider than a word: ", token);
        }
        const int type = wide ? stick_type_bare(unsigned_one ? "unsignedlonglong" : "longlong")
                         : (unsigned_one || (number > 0x7FFFFFFFULL)) ? stick_type_bare("unsignedint")
                                                                      : stick_type_bare("int");
        // the number is a word, checked above
        StickTyped written = stick_typed(number_text((unsigned int)number).text, type, 1);
        written.zero_one = (number <= 1ULL);
        return written;
    }

    // ---- what it reads ----

    // `expression`, read and written into a register: its value. Empty where the read failed, with the reason in
    // `question`
    StickValue value(const char *expression)
    {
        const StickExpr bare = stick_bare(expression);
        const StickBound *const binding = bound_of(bare.text);
        if (binding != NULL)
        {
            return binding->value;
        }
        StickReading reading;
        if (read(bare.text, 0, &reading) == 0)
        {
            note_question("no reading of ", expression, " through cu.krs");
            return stick_value("", 0u, 0);
        }
        return written(reading);
    }

    // Form `name` over the `span_count` expressions `spans`, its first `output_count` its own registers. Each span
    // reads with a number or a folded word in place, and in a register. Of the choices that the sass.krs text
    // assembles, the code writes the one of fewest forms. 0 where none assembles, with the reason in `question`
    int form(const char *name, const StickWord *outputs, unsigned int output_count, const StickExpr *spans,
             unsigned int span_count)
    {
        const unsigned int place = stick_form(name);
        StickReading best = stick_reading(STICK_FORM, 0u, "", 0u, 0xFFFFFFFFu, 32u);
        if ((place == OPCODE_COUNT) ||
            (choose(place, outputs, output_count, spans, span_count, 32u, 0, NULL, -1, &best) == 0))
        {
            note_question("sass.krs's ", name, " assembles with no reading of its operands");
            return 0;
        }
        StickWord arguments[STICK_PLACEHOLDERS_MAX];
        const char *given[STICK_PLACEHOLDERS_MAX];
        unsigned int count = 0u;
        for (unsigned int at = 0u; (at < output_count) && (count < STICK_PLACEHOLDERS_MAX); at += 1u)
        {
            arguments[count] = outputs[at];
            count += 1u;
        }
        for (unsigned int at = 0u; (at < best.child_count) && (count < STICK_PLACEHOLDERS_MAX); at += 1u)
        {
            arguments[count] = operand(*stick_child(&best, at));
            count += 1u;
        }
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            given[at] = arguments[at].text;
        }
        return write(name, given, count);
    }

    // 1 where `bare`, a statement, reads through a statement form of cu.krs, and the code writes it
    int statement(const char *bare)
    {
        for (unsigned int at = 0u; at < s_pattern_count; at += 1u)
        {
            const StickPattern *const pattern = &s_patterns[at];
            if (pattern->statement == 0)
            {
                continue;
            }
            const unsigned int mark = s_span_top;
            const unsigned int matches = stick_matches(pattern, bare);
            const unsigned int spans = pattern->piece_count - 1u;
            if (matches == 0u)
            {
                s_span_top = mark;
                continue;
            }
            const unsigned int text_mark = s_text_top;
            StickExpr *const texts = stick_texts_taken(spans);
            for (unsigned int span = 0u; (texts != NULL) && (span < spans); span += 1u)
            {
                texts[span] = stick_span_text(bare, s_spans[mark + span]);
            }
            s_span_top = mark;
            const int written_here = (texts != NULL) && form(s_form_names[pattern->form], NULL, 0u, texts, spans);
            s_text_top = text_mark;
            return written_here;
        }
        return 0;
    }

    // 1 where `bare` reads through form `name` of cu.krs as an expression, its parameters' spans into the
    // `room` texts at `spans`
    int reads_as(const char *name, const char *bare, StickExpr *spans, unsigned int room)
    {
        for (unsigned int at = 0u; at < s_pattern_count; at += 1u)
        {
            const StickPattern *const pattern = &s_patterns[at];
            if ((pattern->statement != 0) || !stick_same(name, s_form_names[pattern->form]))
            {
                continue;
            }
            const unsigned int mark = s_span_top;
            const unsigned int matches = stick_matches(pattern, bare);
            const unsigned int count = pattern->piece_count - 1u;
            for (unsigned int span = 0u; (matches != 0u) && (span < count) && (span < room); span += 1u)
            {
                spans[span] = stick_span_text(bare, s_spans[mark + span]);
            }
            s_span_top = mark;
            if (matches != 0u)
            {
                return 1;
            }
        }
        return 0;
    }

    // ---- what it writes ----

    // form `name` written in sass.krs with the `count` arguments at `arguments`. 0 where the write failed, with the
    // reason in `question`
    int write(const char *name, const char *const *arguments, unsigned int count)
    {
        VerbaLinea *const line = stick_trace_begun();
        mmgr_line_put(line, "write ");
        mmgr_line_put(line, name);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            mmgr_line_put(line, " ");
            mmgr_line_put(line, arguments[at]);
        }
        stick_trace_written(line);
        // a scratch register the form takes is a fresh one of the kernel's, used over the form's lines alone
        const RulesetNamedScratch scratch = {stick_scratch_fresh, this};
        if (!stick_written(sass, name, arguments, count, &scratch, &text.line))
        {
            note_question("sass.krs writes no ", name);
            return 0;
        }
        forms += 1u;
        return 1;
    }

    // The word of the constant bank that form `form` moves into the register put for its first parameter. That holds
    // where its sass.krs text, with the `count` operands `arguments`, is only such moves. The result is the word of the
    // first move. Empty else
    StickWord moves(unsigned int form, const StickWord *arguments, unsigned int count) const
    {
        static TablePatternOnce s_move;
        StickWord first;
        first.text[0] = '\0';
        const char *given[STICK_PLACEHOLDERS_MAX + 1u];
        given[0] = "\x01";
        for (unsigned int at = 0u; (at < count) && (at < STICK_PLACEHOLDERS_MAX); at += 1u)
        {
            given[at + 1u] = arguments[at].text;
        }
        StickExpr lines;
        if (!stick_written_into(sass, s_form_names[form], given, count + 1u, stick_scratch_are_in, lines.text,
                                sizeof(lines.text)))
        {
            return first;
        }
        const size_t length = stick_length(lines.text);
        for (size_t at = 0u; at < length;)
        {
            const size_t end = stick_place_of(lines.text, '\n', at, length);
            StickExpr line;
            STICK_SET_N(line, &lines.text[at], end - at);
            TableMatch found;
            if (!stick_matched(&s_move, "\\s*MOV\\s+\\x01(\\.hi)?\\s*,\\s*(.+?)\\s*;\\s*", line.text, &found))
            {
                first.text[0] = '\0';
                return first;
            }
            const StickExpr word = stick_group(&found, 2u, line.text);
            if (stick_constant(word.text).text[0] == '\0')
            {
                first.text[0] = '\0';
                return first;
            }
            if (first.text[0] == '\0')
            {
                STICK_SET(first, word.text);
            }
            at = end + 1u;
        }
        return first;
    }

    // 1 where sass.krs's text for `form`, of a to and one more parameter, moves that parameter's register into to
    int copied(unsigned int form) const
    {
        static TablePatternOnce s_copy;
        StickExpr lines;
        const char *const given[2] = {"\x01", "\x02"};
        if ((sass->schema->forms[form].parameters != 2u) ||
            !stick_written_into(sass, s_form_names[form], given, 2u, stick_scratch_are_in, lines.text,
                                sizeof(lines.text)))
        {
            return 0;
        }
        TableMatch found;
        return table_search(table_once(&s_copy, "(^|\\n)\\s*MOV\\s+\\x01\\s*,\\s*\\x02\\s*;", 0), lines.text,
                            stick_length(lines.text), &found);
    }

    // The operands of the parameters of a form past its outputs, as the code writes the `count` readings at
    // `children`, into `arguments`. A number or a folded word goes in place. Else it is a register or predicate of its
    // width, with stand-ins numbered from `are_in`
    void are_ins(const StickReading *children, unsigned int count, unsigned int are_in, StickWord *arguments) const
    {
        unsigned int predicate = 1u;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const StickReading *const child = &children[at];
            StickWord *const argument = &arguments[at];
            argument->text[0] = '\0';
            if (child->leaf == STICK_NUMBER)
            {
                STICK_ADD_DECIMAL(*argument, child->number);
                continue;
            }
            if (child->place.text[0] != '\0')
            {
                *argument = child->place;
                continue;
            }
            if (child->bits == STICK_PREDICATE_BITS)
            {
                STICK_SET(*argument, (child->negated != 0) ? "!P" : "P");
                STICK_ADD_DECIMAL(*argument, predicate);
                predicate += 1u;
                continue;
            }
            are_in += ((child->bits == 64u) && ((are_in % 2u) != 0u)) ? 1u : 0u;
            STICK_SET(*argument, "R");
            STICK_ADD_DECIMAL(*argument, are_in);
            are_in += (child->bits == 64u) ? 2u : 1u;
        }
    }

    // 1 where sass.krs's text for `form` assembles with the `output_count` outputs at `outputs` and the `count`
    // readings at `children` as are_ins writes them. An output the kernel names is a register of the file of its width
    // until assign() gives it one. The answer of each write is kept in s_assembled
    int assembles(unsigned int form, const StickWord *outputs, unsigned int output_count, const StickReading *children,
                  unsigned int count) const
    {
        StickWord arguments[2u * STICK_PLACEHOLDERS_MAX];
        const char *given[2u * STICK_PLACEHOLDERS_MAX];
        unsigned int total = 0u;
        for (unsigned int at = 0u; (at < output_count) && (total < STICK_PLACEHOLDERS_MAX); at += 1u)
        {
            STICK_SET(arguments[total], stick_begins(outputs[at].text, "%w")   ? "R4"
                                        : stick_begins(outputs[at].text, "%r") ? "R3"
                                                                               : outputs[at].text);
            total += 1u;
        }
        are_ins(children, count, 8u, &arguments[total]);
        total += count;
        for (unsigned int at = 0u; at < total; at += 1u)
        {
            given[at] = arguments[at].text;
        }
        StickExpr lines;
        if (!stick_written_into(sass, s_form_names[form], given, total, stick_scratch_are_in, lines.text,
                                sizeof(lines.text)))
        {
            return 0;
        }
        unsigned int line_count = 0u;
        const size_t length = stick_length(lines.text);
        for (size_t at = stick_place_of(lines.text, '\n', 0u, length); at < length;
             at = stick_place_of(lines.text, '\n', at + 1u, length))
        {
            line_count += 1u;
        }
        for (unsigned int at = 0u; at < s_assembled_count; at += 1u)
        {
            if ((s_assembled[at].machine == machine) && stick_same(s_assembled[at].lines.text, lines.text))
            {
                return s_assembled[at].assembled;
            }
        }
        const unsigned long long room = 16ULL * (line_count + 1ULL);
        unsigned char *const code =
            (unsigned char *)platform_cell_taken(room, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        const int assembled =
            (code != NULL) && (sass_assemble_lines(machine, lines.text, SASS_CONTROL_SAFE, code, room) == line_count);
        platform_cell_released(code, platform_place_g_program_owner);
        if (s_assembled_count < STICK_ASSEMBLED_MAX)
        {
            s_assembled[s_assembled_count].machine = machine;
            s_assembled[s_assembled_count].lines = lines;
            s_assembled[s_assembled_count].assembled = assembled;
            s_assembled_count += 1u;
        }
        return assembled;
    }

    // a line of the trace: `form`, the choice `choice` and the tail
    static void choice_traced(unsigned int form, unsigned long long choice, const char *tail, unsigned int number,
                              const char *after)
    {
        VerbaLinea *const line = stick_trace_begun();
        mmgr_line_put(line, s_form_names[form]);
        mmgr_line_put(line, " choice ");
        mmgr_line_decimal(line, choice, 1u);
        mmgr_line_put(line, tail);
        mmgr_line_decimal(line, number, 1u);
        mmgr_line_put(line, after);
        stick_trace_written(line);
    }

    // Of form `form` over the `span_count` texts `spans`, with its outputs `outputs`, the reading of fewest forms whose
    // text assembles. The code stores it in `best` where it has fewer forms than `best` has.
    // Each span reads in place, as the zero register where it is the number 0, and in a register. The code tries each
    // choice.
    //
    // A span goes in registers of the width that `placeholder_bits` gives its parameter, where the caller gives
    // `placeholder_bits`. A flag goes in any width. The code reads the span at `negated`, where it is not -1, as the
    // complement of its flag.
    //
    // The form of an expression gets no outputs. A register of the width of its value, put for its to, gates it. Where
    // the text takes no register there, a predicate gates it
    int choose(unsigned int form, const StickWord *outputs, unsigned int output_count, const StickExpr *spans,
               unsigned int span_count, unsigned int bits_given, int expression, const unsigned int *placeholder_bits,
               int negated, StickReading *best)
    {
        const unsigned int mark = s_option_top;
        // each span's options from `first[at]`, `options[at]` of them, on s_options; then the children of a choice
        unsigned int first[STICK_PLACEHOLDERS_MAX];
        unsigned int options[STICK_PLACEHOLDERS_MAX];
        if ((span_count > STICK_PLACEHOLDERS_MAX) || ((s_option_top + (4u * span_count)) > STICK_OPTIONS_MAX))
        {
            s_stick_room = 1;
            return 0;
        }
        for (unsigned int at = 0u; at < span_count; at += 1u)
        {
            const StickExpr bare = stick_bare(spans[at].text);
            StickReading in_place;
            StickReading in_register;
            if ((read(bare.text, 1, &in_place) == 0) || (read(bare.text, 0, &in_register) == 0))
            {
                VerbaLinea *const line = stick_trace_begun();
                mmgr_line_put(line, s_form_names[form]);
                mmgr_line_put(line, " refused: its operand ");
                mmgr_line_put(line, spans[at].text);
                mmgr_line_put(line, " has no reading");
                stick_trace_written(line);
                s_option_top = mark;
                return 0;
            }
            first[at] = s_option_top;
            s_options[s_option_top] = in_place;
            s_option_top += 1u;
            if ((in_place.leaf == STICK_NUMBER) && (in_place.number == 0u))
            {
                StickReading zero = stick_reading(STICK_NAME, 0u, in_place.text.text, 0u, 0u, 32u);
                STICK_SET(zero.place, sass->fixed[PHYSREG_ZERO]);
                s_options[s_option_top] = zero;
                s_option_top += 1u;
            }
            s_options[s_option_top] = in_register;
            s_option_top += 1u;
            options[at] = s_option_top - first[at];
        }
        StickReading *const children = &s_options[s_option_top];
        s_option_top += span_count;
        unsigned long long choices = 1ULL;
        for (unsigned int at = 0u; at < span_count; at += 1u)
        {
            choices *= options[at];
        }
        int chosen = 0;
        for (unsigned long long choice = 0ULL; choice < choices; choice += 1ULL)
        {
            StickReading reading = stick_reading(STICK_FORM, form, "", 0u, 1u, 32u);
            unsigned int bits = 32u;
            unsigned long long rest = choice;
            int binding = 1;
            for (unsigned int at = 0u; at < span_count; at += 1u)
            {
                StickReading child = s_options[first[at] + (unsigned int)(rest % options[at])];
                rest /= options[at];
                const int count_bits = (placeholder_bits != NULL) && (placeholder_bits[at] == STICK_COUNT_BITS);
                const int fits = !((placeholder_bits != NULL) && !count_bits && (child.bits != STICK_PREDICATE_BITS) &&
                                   (stick_register_bits(child.bits) != placeholder_bits[at]));
                if (!fits && binding)
                {
                    VerbaLinea *const line = stick_trace_begun();
                    mmgr_line_put(line, s_form_names[form]);
                    mmgr_line_put(line, " choice ");
                    mmgr_line_decimal(line, choice, 1u);
                    mmgr_line_put(line, ": operand ");
                    mmgr_line_decimal(line, at, 1u);
                    mmgr_line_put(line, " wants ");
                    mmgr_line_decimal(line, placeholder_bits[at], 1u);
                    mmgr_line_put(line, " bits and has ");
                    mmgr_line_decimal(line, child.bits, 1u);
                    stick_trace_written(line);
                }
                binding = fits ? binding : 0;
                child.negated = ((int)at == negated) ? 1 : 0;
                children[at] = child;
                reading.cost += child.cost;
                // a count is no part of the width of what the form writes
                bits = (!count_bits && (child.bits > bits) && (child.bits != STICK_PREDICATE_BITS)) ? child.bits : bits;
            }
            reading.bits = (bits_given != 0u) ? bits_given : bits;
            if (!binding || (reading.cost >= best->cost))
            {
                continue;
            }
            if (expression == 0)
            {
                if (assembles(form, outputs, output_count, children, span_count) &&
                    stick_children_kept(&reading, children, span_count))
                {
                    *best = reading;
                    chosen = 1;
                }
                continue;
            }
            StickWord to;
            STICK_SET(to, (reading.bits == 64u) ? "R4" : "R2");
            StickWord flag;
            STICK_SET(flag, "P0");
            if (assembles(form, &to, 1u, children, span_count))
            {
                if (stick_children_kept(&reading, children, span_count))
                {
                    *best = reading;
                    chosen = 1;
                }
                choice_traced(form, choice, ": assembles, ", reading.cost, " forms");
            }
            else if (assembles(form, &flag, 1u, children, span_count))
            {
                reading.bits = STICK_PREDICATE_BITS;
                if (stick_children_kept(&reading, children, span_count))
                {
                    *best = reading;
                    chosen = 1;
                }
                choice_traced(form, choice, ": assembles as a flag, ", reading.cost, " forms");
            }
            else
            {
                VerbaLinea *const line = stick_trace_begun();
                mmgr_line_put(line, s_form_names[form]);
                mmgr_line_put(line, " choice ");
                mmgr_line_decimal(line, choice, 1u);
                mmgr_line_put(line, ": sass.krs's text does not assemble over it");
                stick_trace_written(line);
            }
        }
        s_option_top = mark;
        return chosen;
    }

    // `reading` as the word that it is, where the system folds the register put for the to of its form and `in_place`
    // allows it. It stays the same else
    void fold(StickReading *reading, int in_place) const
    {
        if ((in_place == 0) || ((reading->leaf != STICK_FORM) && (reading->leaf != STICK_PARAMETER)) ||
            (folded(reading->form, "register", 0u) == 0))
        {
            return;
        }
        StickWord arguments[STICK_PLACEHOLDERS_MAX];
        const unsigned int count =
            (reading->child_count < STICK_PLACEHOLDERS_MAX) ? reading->child_count : STICK_PLACEHOLDERS_MAX;
        are_ins(stick_child(reading, 0u), count, 8u, arguments);
        const StickWord word = moves(reading->form, arguments, count);
        if (word.text[0] != '\0')
        {
            reading->place = word;
            reading->cost = 0u;
        }
    }

    // The reading of `bare` of the fewest forms, with a number or a folded word in place only where `in_place` allows
    // it. 0 where there is none. `bare` reads as read_untraced() reads it, and the code writes the reading and what it
    // gives to the trace
    int read(const char *bare, int in_place, StickReading *best)
    {
        static const char *const s_leaves[] = {"form", "stored", "number in place", "number set", "parameter"};
        {
            VerbaLinea *const line = stick_trace_begun();
            mmgr_line_put(line, "read ");
            mmgr_line_put(line, bare);
            mmgr_line_put(line, (in_place != 0) ? " in place" : "");
            stick_trace_written(line);
        }
        // the code keeps no reading made during the read of a join of flags with its order exchanged. The join gets
        // no second order
        int read_given = 0;
        const StickCached *before = NULL;
        for (unsigned int at = 0u; (s_exchanging_count == 0u) && (at < s_cached_count); at += 1u)
        {
            if ((s_cached[at].in_place == in_place) && stick_same(s_cached[at].key.text, bare))
            {
                before = &s_cached[at];
                break;
            }
        }
        if (before != NULL)
        {
            read_given = before->read_given;
            *best = before->reading;
            VerbaLinea *const line = stick_trace_begun();
            mmgr_line_put(line, "(read before)");
            stick_trace_written(line);
        }
        else
        {
            s_trace_depth += 1u;
            read_given = read_untraced(bare, in_place, best);
            s_trace_depth -= 1u;
            if ((s_exchanging_count == 0u) && (s_cached_count < STICK_CACHED_MAX))
            {
                StickCached *const kept = &s_cached[s_cached_count];
                s_cached_count += 1u;
                STICK_SET(kept->key, bare);
                kept->in_place = in_place;
                kept->read_given = read_given;
                kept->reading = *best;
            }
        }
        VerbaLinea *const line = stick_trace_begun();
        if (read_given == 0)
        {
            mmgr_line_put(line, "-> no reading of ");
            mmgr_line_put(line, bare);
            stick_trace_written(line);
            return 0;
        }
        mmgr_line_put(line, "-> ");
        mmgr_line_put(line, s_leaves[best->leaf]);
        mmgr_line_put(line, (best->leaf == STICK_FORM) ? " " : "");
        mmgr_line_put(line, (best->leaf == STICK_FORM) ? s_form_names[best->form] : "");
        mmgr_line_put(line, ", ");
        mmgr_line_decimal(line, best->cost, 1u);
        mmgr_line_put(line, " forms, ");
        mmgr_line_decimal(line, best->bits, 1u);
        mmgr_line_put(line, " bits");
        stick_trace_written(line);
        return 1;
    }

    // the readings of `bare` through the expression pattern `pattern`, each kept in `best` where it has fewer forms
    void pattern_read(const StickPattern *pattern, const char *bare, StickReading *best)
    {
        const unsigned int mark = s_span_top;
        const unsigned int text_mark = s_text_top;
        const unsigned int matches = stick_matches(pattern, bare);
        const unsigned int spans = pattern->piece_count - 1u;
        const unsigned int room = (pattern->places > spans) ? pattern->places : spans;
        for (unsigned int match = 0u; match < matches; match += 1u)
        {
            const StickSpan *const spanned = &s_spans[mark + (match * spans)];
            {
                VerbaLinea *const line = stick_trace_begun();
                mmgr_line_put(line, "text of ");
                mmgr_line_put(line, s_form_names[pattern->form]);
                mmgr_line_put(line, " matches:");
                for (unsigned int span = 0u; span < spans; span += 1u)
                {
                    mmgr_line_put(line, " [");
                    (void)mmgr_line_put_n(line, &bare[spanned[span].at], spanned[span].length);
                    mmgr_line_put(line, "]");
                }
                stick_trace_written(line);
            }
            // the spans as the text gives them, then in the order of the schema, then the complement of a select
            StickExpr *const given = stick_texts_taken(spans);
            StickExpr *const ordered = stick_texts_taken(room);
            StickExpr *const complement = stick_texts_taken(room);
            if ((given == NULL) || (ordered == NULL) || (complement == NULL))
            {
                break;
            }
            for (unsigned int span = 0u; span < room; span += 1u)
            {
                ordered[span].text[0] = '\0';
            }
            for (unsigned int span = 0u; span < spans; span += 1u)
            {
                given[span] = stick_span_text(bare, spanned[span]);
                ordered[pattern->order[span]] = given[span];
            }
            // A select over a test is the select over the complement of the test, with its arms exchanged. Of
            // readings of as many forms, the code keeps the first. The reading of the complement goes first where
            // the system tests the complement
            int complemented = 0;
            if (pattern->select != 0)
            {
                const StickExpr where = stick_bare(ordered[pattern->order[0]].text);
                for (unsigned int span = 0u; span < room; span += 1u)
                {
                    complement[span] = ordered[span];
                }
                complement[pattern->order[1]] = ordered[pattern->order[2]];
                complement[pattern->order[2]] = ordered[pattern->order[1]];
                complement[pattern->order[0]] = stick_complement(where.text);
                StickReading test;
                StickReading opposite;
                const int tests = (complement[pattern->order[0]].text[0] != '\0') &&
                                  (read(where.text, 0, &test) != 0) &&
                                  (read(complement[pattern->order[0]].text, 0, &opposite) != 0) &&
                                  (test.leaf == STICK_FORM) && (opposite.leaf == STICK_FORM);
                if (tests)
                {
                    asked_kept(STICK_COMPLEMENTED, test.form, 0u, opposite.form, "");
                }
                if (tests && folded(test.form, "complement", 0u))
                {
                    choose(pattern->form, NULL, 0u, complement, spans, pattern->bits, 1, pattern->placeholder_bits, -1,
                           best);
                }
                complemented = tests;
            }
            choose(pattern->form, NULL, 0u, ordered, spans, pattern->bits, 1, pattern->placeholder_bits, -1, best);
            // a select over a flag is the select over the flag's complement with its arms exchanged
            if (pattern->select != 0)
            {
                const StickExpr chosen_arm = ordered[pattern->order[1]];
                ordered[pattern->order[1]] = ordered[pattern->order[2]];
                ordered[pattern->order[2]] = chosen_arm;
                choose(pattern->form, NULL, 0u, ordered, spans, pattern->bits, 1, pattern->placeholder_bits,
                       (int)pattern->order[0], best);
            }
            if (complemented)
            {
                choose(pattern->form, NULL, 0u, complement, spans, pattern->bits, 1, pattern->placeholder_bits, -1,
                       best);
            }
            // a power of two the system folds is the shift of the multiply's stem by its log2
            const unsigned int shift = stick_shift_of(pattern->form);
            for (unsigned int at = 0u; (spans == 2u) && (shift != OPCODE_COUNT) && (at < 2u); at += 1u)
            {
                unsigned int power = 0u;
                if (folded(pattern->form, "pow2", at + 1u) && number_of(given[at].text, &power) &&
                    (stick_log2(power) != 0u))
                {
                    StickExpr shifted[2];
                    shifted[0] = given[1u - at];
                    shifted[1] = number_text(stick_log2(power));
                    choose(shift, NULL, 0u, shifted, 2u, pattern->bits, 1, NULL, -1, best);
                }
            }
            s_text_top = text_mark;
        }
        s_span_top = mark;
        s_text_top = text_mark;
    }

    int read_untraced(const char *bare, int in_place, StickReading *best)
    {
        best->cost = 0xFFFFFFFFu;
        unsigned int number = 0u;
        const StickBound *const binding = bound_of(bare);
        if (binding != NULL)
        {
            *best = stick_reading(STICK_NAME, 0u, bare, 0u, 0u, binding->value.bits);
            return 1;
        }
        if (number_of(bare, &number) != 0)
        {
            StickExpr set;
            STICK_SET(set, "=");
            STICK_ADD(set, bare);
            *best = (in_place != 0)                ? stick_reading(STICK_NUMBER, 0u, bare, number, 0u, 32u)
                    : (bound_of(set.text) != NULL) ? stick_reading(STICK_NAME, 0u, set.text, 0u, 0u, 32u)
                                                   : stick_reading(STICK_NUMBER_SET, 0u, bare, number, 1u, 32u);
            return 1;
        }
        const StickParameter *const given = parameter_of(bare);
        if (given != NULL)
        {
            const unsigned int load = stick_form((given->bits == 64u) ? "parameter_load_wide" : "parameter_load_word");
            *best = stick_reading(STICK_PARAMETER, load, bare, 0u, 1u, given->bits);
            const StickReading offset = stick_reading(STICK_NUMBER, 0u, "", given->offset, 0u, 32u);
            (void)stick_children_kept(best, &offset, 1u);
            fold(best, in_place);
            return 1;
        }
        // a word of a value stored in a pair is the low word of the pair, the even register, and reads as that
        // register. A wide value stored as its low word alone reads as that word
        static const char *const s_words[] = {"(unsignedint)", "(int)"};
        for (unsigned int word = 0u; word < 2u; word += 1u)
        {
            const StickBound *const pair =
                stick_begins(bare, s_words[word]) ? bound_of(&bare[stick_length(s_words[word])]) : NULL;
            const int low =
                (pair != NULL) && ((pair->value.bits == STICK_PAIR_BITS) ||
                                   ((pair->value.type >= 0) && (s_types[pair->value.type].bits > STICK_REGISTER_BITS)));
            if (low)
            {
                StickValue low_word = stick_value(pair->value.name.text, STICK_REGISTER_BITS, (word == 1u) ? 1 : 0);
                low_word.type = stick_type_bare(s_words[word]);
                bound_put(bare, low_word);
                *best = stick_reading(STICK_NAME, 0u, bare, 0u, 0u, STICK_REGISTER_BITS);
                return 1;
            }
        }
        if (stick_wrapped(bare))
        {
            StickExpr inner;
            STICK_SET_N(inner, &bare[1], stick_length(bare) - 2u);
            return read(inner.text, in_place, best);
        }
        for (unsigned int at = 0u; at < s_pattern_count; at += 1u)
        {
            if (s_patterns[at].statement == 0)
            {
                pattern_read(&s_patterns[at], bare, best);
            }
        }
        // Two flags joined by && or || are valid in either order, because nothing that the reader reads writes
        // anything. A form that glues a test to another flag reads the test first. The code reads the exchanged text
        // once, because it exchanges back
        const StickExpr exchanged = stick_flags_exchanged(bare);
        int exchanging = 0;
        for (unsigned int at = 0u; at < s_exchanging_count; at += 1u)
        {
            exchanging |= stick_same(s_exchanging[at].text, bare) ? 1 : 0;
        }
        if ((best->cost == 0xFFFFFFFFu) && (exchanged.text[0] != '\0') && !exchanging)
        {
            if (s_exchanging_count >= STICK_EXCHANGING_MAX)
            {
                s_stick_room = 1;
                return 0;
            }
            s_exchanging[s_exchanging_count] = exchanged;
            s_exchanging_count += 1u;
            StickReading other;
            const int read_other = read(exchanged.text, in_place, &other);
            s_exchanging_count -= 1u;
            if (read_other != 0)
            {
                *best = other;
            }
        }
        if (best->cost == 0xFFFFFFFFu)
        {
            return 0;
        }
        STICK_SET(best->text, bare);
        fold(best, in_place);
        return 1;
    }

    // `reading`, written in sass.krs, its parameters first, into a register of its own, and stored for its text. A
    // folded reading writes nothing, and is the word that it is
    StickValue written(const StickReading &reading)
    {
        if (reading.place.text[0] != '\0')
        {
            return stick_value(reading.place.text, reading.bits, 0);
        }
        if (reading.leaf == STICK_NAME)
        {
            const StickBound *const binding = bound_of(reading.text.text);
            return (binding != NULL) ? binding->value : stick_value("", 0u, 0);
        }
        if (reading.leaf == STICK_NUMBER)
        {
            return stick_value(stick_register(sass, "immediate", reading.number).text, 32u, 0);
        }
        if (reading.leaf == STICK_NUMBER_SET)
        {
            const StickValue value = fresh(reading.bits);
            const StickWord immediate = stick_register(sass, "immediate", reading.number);
            const char *const given[2] = {value.name.text, immediate.text};
            (void)write("word_set", given, 2u);
            StickExpr set;
            STICK_SET(set, "=");
            STICK_ADD(set, reading.text.text);
            bound_put(set.text, value);
            cached_cleared();
            return value;
        }
        StickWord operands[STICK_PLACEHOLDERS_MAX];
        const unsigned int count =
            (reading.child_count < STICK_PLACEHOLDERS_MAX) ? reading.child_count : STICK_PLACEHOLDERS_MAX;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            const StickReading child = *stick_child(&reading, at);
            operands[at] = operand(child);
            const unsigned int log2 = stick_log2(child.number);
            if (((child.leaf == STICK_NUMBER) || (child.leaf == STICK_NUMBER_SET)) && (log2 != 0u) &&
                (stick_shift_of(reading.form) != OPCODE_COUNT))
            {
                asked_kept(STICK_MULTIPLIED, reading.form, at + 1u, log2, "");
            }
        }
        const StickWord word = moves(reading.form, operands, count);
        if (word.text[0] != '\0')
        {
            asked_kept(STICK_MOVED, reading.form, 0u, 0u, word.text);
        }
        // A wide made of a word whose move the system folds is the pair that the word begins. The code stores a word
        // moved into another, for assign() to read how often the code uses the moved word (assign())
        const int copies = (count == 1u) && copied(reading.form) && stick_begins(operands[0].text, "%r");
        if (copies && (reading.bits == 64u))
        {
            asked_kept(STICK_COPIED, reading.form, 0u, 0u, "");
        }
        const StickValue value = fresh(reading.bits);
        int tied = 0;
        for (unsigned int at = 0u; copies && (at < tie_count); at += 1u)
        {
            tied |= stick_same(ties[at].word.text, operands[0].text) ? 1 : 0;
        }
        if (copies && (reading.bits == 64u) && folded(reading.form, "register", 1u) && !tied)
        {
            if (tie_count < written_room)
            {
                ties[tie_count].wide = value.name;
                ties[tie_count].word = operands[0];
                tie_count += 1u;
            }
            else
            {
                s_stick_room = 1;
            }
        }
        if (copies && (reading.bits == 32u))
        {
            if (copy_count < written_room)
            {
                copies_kept(reading.form, value.name, operands[0]);
            }
            else
            {
                s_stick_room = 1;
            }
        }
        const char *given[STICK_PLACEHOLDERS_MAX + 1u];
        given[0] = value.name.text;
        for (unsigned int at = 0u; (at < count) && (at < STICK_PLACEHOLDERS_MAX); at += 1u)
        {
            given[at + 1u] = operands[at].text;
        }
        (void)write(s_form_names[reading.form], given, count + 1u);
        bound_put(reading.text.text, value);
        cached_cleared();
        return value;
    }

    // a word moved into another kept for assign()
    void copies_kept(unsigned int form, const StickWord &into, const StickWord &moved)
    {
        StickCopy *const copy = &copies[copy_count];
        copy_count += 1u;
        copy->form = form;
        copy->into = into;
        copy->moved = moved;
    }

    // `child`, written as the operand of its parent: its value, or the complement of its flag where it reads as one
    StickWord operand(const StickReading &child)
    {
        StickWord written_operand;
        STICK_SET(written_operand, (child.negated != 0) ? "!" : "");
        STICK_ADD(written_operand, written(child).name.text);
        return written_operand;
    }

    // ---- the text written ----

    // the bytes of the kernel's text, a copy the text holds until its next read [BORROWS], and their count
    const char *text_bytes(size_t *size)
    {
        const char *const bytes = platform_text_bytes(&text);
        // a kernel's text is at max what a size_t counts
        *size = (bytes != NULL) ? (size_t)platform_text_size(&text) : 0u;
        return (bytes != NULL) ? bytes : "";
    }

    // the kernel's text written anew from the `size` bytes at `bytes` of a text of the call's own
    void text_replaced(PlatformText *from)
    {
        const char *const bytes = platform_text_bytes(from);
        const unsigned long long size = platform_text_size(from);
        platform_text_reset(&text);
        // a kernel's text is at max what a size_t counts
        (void)mmgr_line_put_n(&text.line, (bytes != NULL) ? bytes : "", (size_t)size);
    }

    // each place of the kernel's text that names `name` with no digit after it written `by` in its place
    void named_replaced(const char *name, const char *by)
    {
        PlatformText out;
        platform_text_begun(&out, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        size_t length = 0u;
        const char *const bytes = text_bytes(&length);
        const size_t size = stick_length(name);
        size_t from = 0u;
        for (size_t at = stick_place(bytes, name, 0u, length); at < length;
             at = stick_place(bytes, name, at + 1u, length))
        {
            const size_t end = at + size;
            // a byte of the text read as the unsigned byte the class takes
            if ((end < length) &&
                (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (unsigned char)bytes[end]) != 0))
            {
                continue;
            }
            (void)mmgr_line_put_n(&out.line, &bytes[from], at - from);
            mmgr_line_put(&out.line, by);
            from = end;
            at = end - 1u;
        }
        (void)mmgr_line_put_n(&out.line, &bytes[from], length - from);
        text_replaced(&out);
        platform_text_ended(&out);
    }

    // The register that each name of the kernel got, and what claims and releases it. The name claims it at the line
    // that first names it, and releases it after the line that last names it. Those lines carry the numbers of the
    // text, after the code leaves out each move of a register into itself. A claim at a line left out goes to the next
    // kept line. A release at such a line goes to the kept line before it. Then each fixed register that the text
    // names, claimed at the first line that names it, and released at the last
    void register_lines_written(const StickRoot *roots, unsigned int root_count, const unsigned int *retained_at,
                                unsigned int line_count)
    {
        platform_text_reset(&register_lines);
        StickClaim *const claims =
            (StickClaim *)platform_cell_taken(((root_count != 0u) ? root_count : 1u) * sizeof(StickClaim),
                                              platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        unsigned int claim_count = 0u;
        for (unsigned int root = 0u; (claims != NULL) && (root < root_count); root += 1u)
        {
            if (roots[root].named)
            {
                claims[claim_count].first_line = roots[root].first_line;
                claims[claim_count].root = root;
                STICK_SET(claims[claim_count].name, roots[root].wide ? "%w" : "%r");
                STICK_ADD_DECIMAL(claims[claim_count].name, root);
                claim_count += 1u;
            }
        }
        (void)stick_sorted(claims, claim_count, sizeof(StickClaim), stick_claim_order);
        for (unsigned int at = 0u; at < claim_count; at += 1u)
        {
            const StickRoot *const root = &roots[claims[at].root];
            const StickWord register_name = stick_register(sass, root->wide ? "wide" : "temporary", root->given);
            mmgr_line_put(&register_lines.line, register_name.text);
            mmgr_line_put(&register_lines.line, " ");
            mmgr_line_put(&register_lines.line, claims[at].name.text);
            mmgr_line_put(&register_lines.line, " claimed ");
            mmgr_line_decimal(&register_lines.line, stick_claimed_at(retained_at, line_count, root->first_line), 1u);
            mmgr_line_put(&register_lines.line, " released ");
            mmgr_line_decimal(&register_lines.line, stick_released_at(retained_at, line_count, root->named_last), 1u);
            mmgr_line_ch(&register_lines.line, '\n');
        }
        platform_cell_released(claims, platform_place_g_program_owner);
        size_t length = 0u;
        const char *const bytes = text_bytes(&length);
        for (unsigned int fixed = 0u; fixed < sass->schema->fixed_count; fixed += 1u)
        {
            const char *const binding = sass->fixed[fixed];
            if ((binding == NULL) || (binding[0] == '\0') || stick_same(binding, "RZ") || stick_same(binding, "PT"))
            {
                continue;
            }
            unsigned int first = 0u;
            unsigned int last = 0u;
            unsigned int number = 0u;
            for (size_t at = 0u; at < length;)
            {
                const size_t end = stick_place_of(bytes, '\n', at, length);
                number += 1u;
                if (stick_fixed_named(&bytes[at], end - at, binding))
                {
                    first = (first == 0u) ? number : first;
                    last = number;
                }
                at = end + 1u;
            }
            if (first != 0u)
            {
                mmgr_line_put(&register_lines.line, binding);
                mmgr_line_put(&register_lines.line, " ");
                mmgr_line_put(&register_lines.line, sass->schema->fixed[fixed].text);
                mmgr_line_put(&register_lines.line, " claimed ");
                mmgr_line_decimal(&register_lines.line, first, 1u);
                mmgr_line_put(&register_lines.line, " released ");
                mmgr_line_decimal(&register_lines.line, last, 1u);
                mmgr_line_ch(&register_lines.line, '\n');
            }
        }
    }

    // Each register that the kernel names gets one of the file for the lines that it lives over. That is from the first
    // line that names it to the last. It goes back once the code lets it go. A word takes one register, and a wide an
    // even pair.
    //
    // Take a word moved into another and used nowhere else, its own write and the move. It is the word_used_once of
    // the form of the move. Where the system folds it, the word moved into is the moved word itself.
    //
    // A wide made of a word whose move the system folds is the pair that the word begins. The word gets the even
    // register. The code then leaves out a move of a register into itself, because it does nothing
    void assign(void)
    {
        for (unsigned int at = 0u; at < copy_count; at += 1u)
        {
            const StickCopy copy = copies[at];
            size_t length = 0u;
            if (stick_named_count(text_bytes(&length), copy.moved.text, 0, 0) != 2u)
            {
                continue;
            }
            asked_kept(STICK_COPIED_ONCE, copy.form, 0u, 0u, "");
            if (!folded(copy.form, "word_used_once", 1u))
            {
                continue;
            }
            named_replaced(copy.into.text, copy.moved.text);
            for (unsigned int tie = 0u; tie < tie_count; tie += 1u)
            {
                ties[tie].word = stick_same(ties[tie].word.text, copy.into.text) ? copy.moved : ties[tie].word;
            }
        }
        // each name's root, its register, and its first and last line, by the number the name holds
        const unsigned int root_count = next;
        StickRoot *const roots =
            (StickRoot *)platform_cell_cleared((root_count != 0u) ? root_count : 1u, sizeof(StickRoot),
                                               platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if (roots == NULL)
        {
            s_stick_room = 1;
            return;
        }
        for (unsigned int root = 0u; root < root_count; root += 1u)
        {
            roots[root].tied_to = root;
        }
        for (unsigned int tie = 0u; tie < tie_count; tie += 1u)
        {
            const unsigned int wide = stick_name_number(ties[tie].wide.text);
            const unsigned int word = stick_name_number(ties[tie].word.text);
            if ((wide < root_count) && (word < root_count))
            {
                roots[wide].tied_to = word;
                roots[word].paired = 1;
            }
        }
        // the names of each line, a name tied into another's pair counted as that other's, in the order of the text
        size_t length = 0u;
        const char *bytes = text_bytes(&length);
        unsigned int named_count = 0u;
        unsigned int line_count = 0u;
        for (size_t at = stick_place_of(bytes, '%', 0u, length); at < length;
             at = stick_place_of(bytes, '%', at + 1u, length))
        {
            named_count += 1u;
        }
        for (size_t at = stick_place_of(bytes, '\n', 0u, length); at < length;
             at = stick_place_of(bytes, '\n', at + 1u, length))
        {
            line_count += 1u;
        }
        line_count += ((length != 0u) && (bytes[length - 1u] != '\n')) ? 1u : 0u;
        StickOccurrence *const named =
            (StickOccurrence *)platform_cell_taken(((named_count != 0u) ? named_count : 1u) * sizeof(StickOccurrence),
                                                   platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if (named == NULL)
        {
            platform_cell_released(roots, platform_place_g_program_owner);
            s_stick_room = 1;
            return;
        }
        unsigned int occurrences = 0u;
        unsigned int line = 0u;
        size_t line_end = stick_place_of(bytes, '\n', 0u, length);
        for (size_t at = stick_place_of(bytes, '%', 0u, length); at < length;
             at = stick_place_of(bytes, '%', at + 1u, length))
        {
            while (at > line_end)
            {
                line += 1u;
                line_end = stick_place_of(bytes, '\n', line_end + 1u, length);
            }
            unsigned int number = 0u;
            if (!stick_name_at(bytes, length, at, &number, NULL) || (number >= root_count))
            {
                continue;
            }
            roots[number].wide = (bytes[at + 1u] == 'w') ? 1 : roots[number].wide;
            const unsigned int root = roots[number].tied_to;
            named[occurrences].line = line;
            named[occurrences].root = root;
            occurrences += 1u;
            roots[root].last_line = line;
            roots[root].named_last = line;
        }
        // Each name gets a register at the line that first names it. The register goes free after the line that last
        // names it, and never on a line that names it. A word takes one register, the lowest free one. A wide takes an
        // even pair, the lowest pair that is free. The high water mark starts at the first register past the ones that
        // kernel_open writes. It rises only where no register under it is free
        unsigned long long free_bits[STICK_FREE_WORDS];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = free_bits, .bytes = sizeof(free_bits), .val = 0);
        unsigned int high_water = 2u;
        unsigned int first = 0u;
        while (first < occurrences)
        {
            unsigned int end = first;
            while ((end < occurrences) && (named[end].line == named[first].line))
            {
                end += 1u;
            }
            for (unsigned int at = first; at < end; at += 1u)
            {
                StickRoot *const root = &roots[named[at].root];
                if (root->named)
                {
                    continue;
                }
                const int even = root->wide || root->paired;
                root->given = stick_free_taken(free_bits, even, &high_water);
                root->named = 1;
                root->first_line = named[at].line;
            }
            for (unsigned int at = first; at < end; at += 1u)
            {
                StickRoot *const root = &roots[named[at].root];
                if (root->named && (root->last_line == named[at].line))
                {
                    stick_free_given(free_bits, root->given, root->wide || root->paired);
                    root->last_line = line_count;
                }
            }
            first = end;
        }
        for (unsigned int tie = 0u; tie < tie_count; tie += 1u)
        {
            const unsigned int wide = stick_name_number(ties[tie].wide.text);
            if ((wide < root_count) && (roots[wide].tied_to < root_count))
            {
                roots[wide].given = roots[roots[wide].tied_to].given;
            }
        }
        platform_cell_released(named, platform_place_g_program_owner);
        // A register that a form names by its own number past the high water mark belongs to the form. The code moves
        // it to just past the mark, and the mark rises over it. The second register of a pair and the pair that it
        // begins stay even. The registers of the kernel then lie next to each other, and it declares the mark
        pinned_moved(&high_water);
        register_count = high_water;
        // the mark may not pass the last register the part answers a kernel's code can name
        if ((machine->register_last != SASS_MACHINE_UNANSWERED) && (high_water > (machine->register_last + 1u)))
        {
            StickExpr why;
            STICK_SET(why, "a kernel of more registers at once than the part gives code: ");
            STICK_ADD_DECIMAL(why, high_water);
            STICK_ADD(why, " past ");
            STICK_ADD_DECIMAL(why, machine->register_last + 1u);
            note_question(why.text);
        }
        names_written(roots, root_count);
        unsigned int *const retained_at =
            (unsigned int *)platform_cell_cleared((line_count != 0u) ? line_count : 1u, sizeof(unsigned int),
                                                  platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if (retained_at == NULL)
        {
            platform_cell_released(roots, platform_place_g_program_owner);
            s_stick_room = 1;
            return;
        }
        moves_itself_dropped(retained_at, line_count);
        register_lines_written(roots, root_count, retained_at, line_count);
        platform_cell_released(retained_at, platform_place_g_program_owner);
        platform_cell_released(roots, platform_place_g_program_owner);
    }

    // Each register the text names by its own number at or past `*high_water`, moved to just past the mark. They move
    // in the order of the numbers, and the mark rises over each. A pair's register stays even
    void pinned_moved(unsigned int *high_water)
    {
        static TablePatternOnce s_pinned;
        const TablePattern *const pattern = table_once(&s_pinned, "\\bR([0-9]+)(\\.hi)?\\b", 0);
        size_t length = 0u;
        const char *bytes = text_bytes(&length);
        unsigned int count = 0u;
        for (size_t at = stick_place_of(bytes, 'R', 0u, length); at < length;
             at = stick_place_of(bytes, 'R', at + 1u, length))
        {
            count += 1u;
        }
        StickPin *const pins = (StickPin *)platform_cell_taken(((count != 0u) ? count : 1u) * sizeof(StickPin),
                                                               platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if (pins == NULL)
        {
            s_stick_room = 1;
            return;
        }
        unsigned int pin_count = 0u;
        TableWalk walk;
        table_walk_begun(&walk, pattern, bytes, length);
        while ((pin_count < count) && table_walk_next(&walk))
        {
            const StickExpr digits = stick_group(&walk.match, 1u, bytes);
            // a register's number is far below 2^32
            const unsigned int number = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = digits.text,
                                                                 .end = NULL, .base = 10);
            if (number >= *high_water)
            {
                pins[pin_count].number = number;
                pins[pin_count].pair = walk.match.group[2].set ? 1 : 0;
                pins[pin_count].to = 0u;
                pin_count += 1u;
            }
        }
        table_walk_ended(&walk);
        (void)stick_sorted(pins, pin_count, sizeof(StickPin), stick_pin_order);
        unsigned int kept = 0u;
        for (unsigned int at = 0u; at < pin_count; at += 1u)
        {
            if ((kept != 0u) && (pins[kept - 1u].number == pins[at].number))
            {
                pins[kept - 1u].pair |= pins[at].pair;
                continue;
            }
            pins[kept] = pins[at];
            kept += 1u;
        }
        for (unsigned int at = 0u; at < kept; at += 1u)
        {
            *high_water += (pins[at].pair && ((*high_water % 2u) != 0u)) ? 1u : 0u;
            pins[at].to = *high_water;
            *high_water += pins[at].pair ? 2u : 1u;
        }
        PlatformText out;
        platform_text_begun(&out, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        bytes = text_bytes(&length);
        size_t last = 0u;
        table_walk_begun(&walk, pattern, bytes, length);
        while (table_walk_next(&walk))
        {
            const RegexpSpan whole = walk.match.group[0];
            const StickExpr digits = stick_group(&walk.match, 1u, bytes);
            // a register's number is far below 2^32
            const unsigned int number = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = digits.text,
                                                                 .end = NULL, .base = 10);
            const StickPin *const pin = stick_pin_of(pins, kept, number);
            // a match lies within the text, whose bytes a size_t counts
            (void)mmgr_line_put_n(&out.line, &bytes[last], (size_t)whole.from - last);
            if (pin != NULL)
            {
                mmgr_line_put(&out.line, "R");
                mmgr_line_decimal(&out.line, pin->to, 1u);
                mmgr_line_put(&out.line, walk.match.group[2].set ? ".hi" : "");
            }
            else
            {
                (void)mmgr_line_put_n(&out.line, &bytes[whole.from], (size_t)(whole.to - whole.from));
            }
            last = (size_t)whole.to;
        }
        table_walk_ended(&walk);
        (void)mmgr_line_put_n(&out.line, &bytes[last], length - last);
        text_replaced(&out);
        platform_text_ended(&out);
        platform_cell_released(pins, platform_place_g_program_owner);
    }

    // each name of the kernel, `%r` or `%w` and its number, written as the register of the file it got
    void names_written(const StickRoot *roots, unsigned int root_count)
    {
        PlatformText out;
        platform_text_begun(&out, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        size_t length = 0u;
        const char *const bytes = text_bytes(&length);
        size_t last = 0u;
        for (size_t at = stick_place_of(bytes, '%', 0u, length); at < length;
             at = stick_place_of(bytes, '%', at + 1u, length))
        {
            unsigned int number = 0u;
            size_t end = at;
            if (!stick_name_at(bytes, length, at, &number, &end))
            {
                continue;
            }
            (void)mmgr_line_put_n(&out.line, &bytes[last], at - last);
            const unsigned int given = (number < root_count) ? roots[number].given : 0u;
            mmgr_line_put(&out.line, stick_register(sass, (bytes[at + 1u] == 'w') ? "wide" : "temporary", given).text);
            last = end;
            at = end - 1u;
        }
        (void)mmgr_line_put_n(&out.line, &bytes[last], length - last);
        text_replaced(&out);
        platform_text_ended(&out);
    }

    // Each move of a register into itself left out of the text. Each line's number in the text after, counted from 1,
    // goes into `retained_at`, which holds `line_count`: 0 where the code leaves the line out
    void moves_itself_dropped(unsigned int *retained_at, unsigned int line_count)
    {
        static TablePatternOnce s_itself;
        PlatformText out;
        platform_text_begun(&out, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        size_t length = 0u;
        const char *const bytes = text_bytes(&length);
        unsigned int kept = 0u;
        unsigned int line = 0u;
        for (size_t at = 0u; at < length;)
        {
            const size_t end = stick_place_of(bytes, '\n', at, length);
            StickExpr text_line;
            STICK_SET_N(text_line, &bytes[at], end - at);
            TableMatch move;
            const int itself =
                stick_matched(&s_itself, "\\s*MOV\\s+(\\S+)\\s*,\\s*(\\S+)\\s*;\\s*", text_line.text, &move) &&
                stick_same(stick_group(&move, 1u, text_line.text).text, stick_group(&move, 2u, text_line.text).text);
            if (!itself)
            {
                (void)mmgr_line_put_n(&out.line, &bytes[at], end - at);
                mmgr_line_ch(&out.line, '\n');
                kept += 1u;
            }
            if (line < line_count)
            {
                retained_at[line] = itself ? 0u : kept;
            }
            line += 1u;
            at = end + 1u;
        }
        text_replaced(&out);
        platform_text_ended(&out);
    }

    // A word loaded and read nowhere but by its extension from a byte or a halfword. The load that extends it loads it
    // into the register that the extension wrote: global_load_<thing> for word_from_<thing>
    void fold_loads(void)
    {
        static const char *const s_narrow[] = {"byte", "signed_byte", "halfword", "signed_halfword"};
        const char *const load_given[3] = {"\x01", "\x02", "\x03"};
        StickExpr load_text;
        if (!stick_written_into(sass, "global_load_word", load_given, 3u, stick_scratch_none, load_text.text,
                                sizeof(load_text.text)))
        {
            return;
        }
        TablePattern load;
        if (!stick_line_pattern(stick_squeeze(load_text.text).text, &load))
        {
            return;
        }
        size_t length = 0u;
        const char *const bytes = text_bytes(&length);
        // the text the call holds while the lines change, and each line: a piece of it, or a line written in its place
        PlatformText held;
        platform_text_begun(&held, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        (void)mmgr_line_put_n(&held.line, bytes, length);
        const char *const whole = platform_text_bytes(&held);
        unsigned int line_count = 0u;
        for (size_t at = 0u; at < length; at = stick_place_of(whole, '\n', at, length) + 1u)
        {
            line_count += 1u;
        }
        StickLine *const lines =
            (StickLine *)platform_cell_taken(((line_count != 0u) ? line_count : 1u) * sizeof(StickLine),
                                             platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if (lines == NULL)
        {
            platform_text_ended(&held);
            table_pattern_ended(&load);
            s_stick_room = 1;
            return;
        }
        unsigned int filled = 0u;
        for (size_t at = 0u; at < length;)
        {
            const size_t end = stick_place_of(whole, '\n', at, length);
            STICK_SET_N(lines[filled], &whole[at], end - at);
            filled += 1u;
            at = end + 1u;
        }
        for (unsigned int thing = 0u; thing < 4u; thing += 1u)
        {
            StickWord extension_name;
            STICK_SET(extension_name, "word_from_");
            STICK_ADD(extension_name, s_narrow[thing]);
            const char *const extension_given[2] = {"\x01", "\x02"};
            StickExpr extension_text;
            TablePattern extension;
            if (!stick_written_into(sass, extension_name.text, extension_given, 2u, stick_scratch_none,
                                    extension_text.text, sizeof(extension_text.text)) ||
                !stick_line_pattern(stick_squeeze(extension_text.text).text, &extension))
            {
                continue;
            }
            for (unsigned int at = 0u; at < filled; at += 1u)
            {
                const StickExpr line = stick_squeeze(lines[at].text);
                TableMatch extended;
                if (!table_matched(&extension, line.text, stick_length(line.text), &extended))
                {
                    continue;
                }
                const StickExpr from = stick_group(&extended, 2u, line.text);
                const unsigned int uses = stick_named_count(whole, from.text, 1, 1);
                for (unsigned int before = 0u; (uses == 2u) && (before < at); before += 1u)
                {
                    const StickExpr earlier = stick_squeeze(lines[before].text);
                    TableMatch loaded;
                    if (!table_matched(&load, earlier.text, stick_length(earlier.text), &loaded) ||
                        !stick_same(stick_group(&loaded, 1u, earlier.text).text, from.text))
                    {
                        continue;
                    }
                    StickWord loader;
                    STICK_SET(loader, "global_load_");
                    STICK_ADD(loader, s_narrow[thing]);
                    const StickExpr into = stick_group(&extended, 1u, line.text);
                    const StickExpr address = stick_group(&loaded, 2u, earlier.text);
                    const StickExpr offset = stick_group(&loaded, 3u, earlier.text);
                    const char *const given[3] = {into.text, address.text, offset.text};
                    StickExpr written_text;
                    if (stick_written_into(sass, loader.text, given, 3u, stick_scratch_none, written_text.text,
                                           sizeof(written_text.text)))
                    {
                        STICK_SET_N(lines[before], written_text.text,
                                    stick_place_of(written_text.text, '\n', 0u, stick_length(written_text.text)));
                        lines[at].text[0] = '\0';
                        break;
                    }
                }
            }
            table_pattern_ended(&extension);
        }
        platform_text_reset(&text);
        for (unsigned int at = 0u; at < filled; at += 1u)
        {
            if (lines[at].text[0] != '\0')
            {
                mmgr_line_put(&text.line, lines[at].text);
                mmgr_line_ch(&text.line, '\n');
            }
        }
        platform_cell_released(lines, platform_place_g_program_owner);
        platform_text_ended(&held);
        table_pattern_ended(&load);
    }

    // Each operation that the system writes on either pipe, written on the FMA pipe. That holds where the .kdm of the
    // part records the lane that it moves in, and the text of the kernel is such a lane
    void balance(void)
    {
        size_t length = 0u;
        const char *const bytes = text_bytes(&length);
        PlatformText held;
        platform_text_begun(&held, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        (void)mmgr_line_put_n(&held.line, bytes, length);
        const char *const whole = platform_text_bytes(&held);
        unsigned int line_count = 0u;
        for (size_t at = 0u; at < length; at = stick_place_of(whole, '\n', at, length) + 1u)
        {
            line_count += 1u;
        }
        StickLine *const squeezed =
            (StickLine *)platform_cell_taken(((line_count != 0u) ? line_count : 1u) * sizeof(StickLine),
                                             platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if (squeezed == NULL)
        {
            platform_text_ended(&held);
            s_stick_room = 1;
            return;
        }
        unsigned int filled = 0u;
        for (size_t at = 0u; at < length;)
        {
            const size_t end = stick_place_of(whole, '\n', at, length);
            s_stick_cut |=
                stick_squeezed(&whole[at], end - at, squeezed[filled].text, sizeof(squeezed[filled].text)) ? 0 : 1;
            filled += 1u;
            at = end + 1u;
        }
        const StickLane lane = stick_pipe_lane(squeezed, filled);
        platform_text_reset(&text);
        unsigned int line = 0u;
        for (size_t at = 0u; at < length;)
        {
            const size_t end = stick_place_of(whole, '\n', at, length);
            StickExpr writing;
            char binding[SASS_TEXT];
            unsigned int least = 0u;
            unsigned int over = 0u;
            const StickWord operation = stick_pipe_choice(squeezed[line].text, &writing);
            const size_t space = stick_place_of(writing.text, ' ', 0u, stick_length(writing.text));
            const int moves_here = (operation.text[0] != '\0') &&
                                   (sass_pipe_found(operation.text, binding, &least, &over) != 0) &&
                                   stick_same_n(writing.text, space, binding) && (lane.integer >= least) &&
                                   (lane.integer >= (lane.fma + over));
            if (moves_here)
            {
                mmgr_line_ch(&text.line, '\t');
                (void)mmgr_line_put_n(&text.line, writing.text, space);
                mmgr_line_put(&text.line, " \t");
                mmgr_line_put(&text.line, (writing.text[space] != '\0') ? &writing.text[space + 1u] : "");
                mmgr_line_ch(&text.line, ';');
            }
            else
            {
                (void)mmgr_line_put_n(&text.line, &whole[at], end - at);
            }
            mmgr_line_ch(&text.line, '\n');
            line += 1u;
            at = end + 1u;
        }
        platform_cell_released(squeezed, platform_place_g_program_owner);
        platform_text_ended(&held);
    }
};

static int stick_scratch_fresh(void *context, const char *bank, VerbaLinea *name)
{
    StickKernel *const kernel = (StickKernel *)context;
    const StickValue taken = kernel->fresh(stick_same(bank, "wide")        ? 64u
                                           : stick_same(bank, "predicate") ? STICK_PREDICATE_BITS
                                                                           : 32u);
    mmgr_line_put(name, taken.name.text);
    return 1;
}

// the kernel begun: its rulesets, its machine, whether it folds, its texts, and its tables at the room of
// `statements` statements and `tokens` tokens. 1, or 0 where the pool cannot meet a cell
static int stick_kernel_begun(StickKernel *kernel, const Ruleset *cu, const Ruleset *sass, const SassMachine *machine,
                              int folding, unsigned int statements, unsigned int tokens)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = kernel, .bytes = sizeof(*kernel), .val = 0);
    kernel->cu = cu;
    kernel->sass = sass;
    kernel->machine = machine;
    kernel->folding = folding;
    platform_text_begun(&kernel->text, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    platform_text_begun(&kernel->register_lines, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner,
                        PLATFORM_TIER_MOVABLE);
    kernel->bound_room = (2u * (tokens + statements)) + 64u;
    kernel->named_room = (2u * statements) + (2u * STICK_PARAMETERS_MAX) + 16u;
    kernel->written_room = kernel->bound_room;
    kernel->bound = (StickBound *)platform_cell_taken((unsigned long long)kernel->bound_room * sizeof(StickBound),
                                                      platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    kernel->flags = (StickNamed *)platform_cell_taken((unsigned long long)kernel->named_room * sizeof(StickNamed),
                                                      platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    kernel->located = (StickNamed *)platform_cell_taken((unsigned long long)kernel->named_room * sizeof(StickNamed),
                                                        platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    kernel->nonnegatives = (StickWord *)platform_cell_taken((unsigned long long)kernel->named_room * sizeof(StickWord),
                                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    kernel->ties = (StickTie *)platform_cell_taken((unsigned long long)kernel->written_room * sizeof(StickTie),
                                                   platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    kernel->copies = (StickCopy *)platform_cell_taken((unsigned long long)kernel->written_room * sizeof(StickCopy),
                                                      platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    kernel->asked = (StickAsked *)platform_cell_taken((unsigned long long)kernel->written_room * sizeof(StickAsked),
                                                      platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    s_cached_count = 0u;
    return (kernel->bound != NULL) && (kernel->flags != NULL) && (kernel->located != NULL) &&
           (kernel->nonnegatives != NULL) && (kernel->ties != NULL) && (kernel->copies != NULL) &&
           (kernel->asked != NULL);
}

// the kernel's cells given back, in the order opposite to their take, and its texts ended
static void stick_kernel_ended(StickKernel *kernel)
{
    platform_cell_released(kernel->asked, platform_place_g_program_owner);
    platform_cell_released(kernel->copies, platform_place_g_program_owner);
    platform_cell_released(kernel->ties, platform_place_g_program_owner);
    platform_cell_released(kernel->nonnegatives, platform_place_g_program_owner);
    platform_cell_released(kernel->located, platform_place_g_program_owner);
    platform_cell_released(kernel->flags, platform_place_g_program_owner);
    platform_cell_released(kernel->bound, platform_place_g_program_owner);
    platform_text_ended(&kernel->register_lines);
    platform_text_ended(&kernel->text);
}

// ---- the stick and nvcc's listing ----

// a piece of a text the engine reads once: its first byte and its bytes
struct StickRange
{
    size_t at;
    size_t length;
};

// A kernel of the stick: its number, and its parameters' text as a piece of the stick. Then its body's statements from
// `statement_first`, `statement_count` of them, squeezed, each a piece of s_statement_text
struct StickSource
{
    StickWord number;
    StickRange parameters;
    unsigned int statement_first;
    unsigned int statement_count;
};

// The stick read once: its text, its kernels and their statements, each in one cell of the persistent tier taken
// once at its count
static char *s_stick_text = NULL;
static size_t s_stick_size = 0u;
static StickSource *s_sources = NULL;
static unsigned int s_source_count = 0u;
static StickRange *s_statements = NULL;
static unsigned int s_statement_count = 0u;
static char *s_statement_text = NULL;

// The file at `path` read whole into a cell of `tier`, terminated, its bytes into `size`. NULL where it does not read.
// The persistent tier holds a text the engine keeps, and the temporary tier a text the call gives back before it
// returns
static char *stick_file_read(const char *path, size_t *size, int tier)
{
    unsigned long long bytes = 0ULL;
    char *const text = (platform_file_size(path, &bytes) != 0)
                           ? (char *)platform_cell_taken(bytes + 1ULL, platform_place_g_program_owner, tier)
                           : NULL;
    unsigned long long read = 0ULL;
    if ((text == NULL) || !platform_file_read(path, text, bytes, &read))
    {
        platform_cell_released(text, platform_place_g_program_owner);
        *size = 0u;
        return NULL;
    }
    text[read] = '\0';
    // a file read whole is at max what a size_t counts
    *size = (size_t)read;
    return text;
}

// the opening of a kernel of the stick, `extern "C" __global__ void measuring_stick_<n>(<parameters>)`
static const char s_opening[] = "extern \"C\" __global__ void measuring_stick_";

// The kernels of the stick and their statements, counted where `filling` is 0 and written where it is 1. A body runs
// from the line after the opening's to the line `}`. Its statements end at each `;` or `}` outside every bracket
static void stick_sources_made(int filling, unsigned int *sources, unsigned int *statements, size_t *statement_bytes)
{
    *sources = 0u;
    *statements = 0u;
    *statement_bytes = 0u;
    const char *const text = s_stick_text;
    const size_t size = s_stick_size;
    const size_t opening = stick_length(s_opening);
    size_t at = 0u;
    while (at < size)
    {
        const size_t end = stick_place_of(text, '\n', at, size);
        const int opens = ((end - at) > opening) && stick_begins(&text[at], s_opening) && (text[end - 1u] == ')');
        const size_t digits = opens ? EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at + opening],
                                                 .cap = end - at - opening, .other = "0123456789")
                                    : 0u;
        const size_t open = at + opening + digits;
        if (!opens || (digits == 0u) || (text[open] != '('))
        {
            at = end + 1u;
            continue;
        }
        StickSource *const source = filling ? &s_sources[*sources] : NULL;
        if (filling)
        {
            STICK_SET_N(source->number, &text[at + opening], digits);
            source->parameters.at = open + 1u;
            source->parameters.length = end - 1u - (open + 1u);
            source->statement_first = *statements;
            source->statement_count = 0u;
        }
        *sources += 1u;
        // the line after the opening is the body's `{`, and the body runs to the line `}`
        at = stick_place_of(text, '\n', end + 1u, size) + 1u;
        size_t body_end = at;
        while (body_end < size)
        {
            const size_t line_end = stick_place_of(text, '\n', body_end, size);
            if (((line_end - body_end) == 1u) && (text[body_end] == '}'))
            {
                break;
            }
            body_end = line_end + 1u;
        }
        // a body ends with its last line's end, which the statements read as white space
        const size_t body_last = (body_end > size) ? size : body_end;
        int depth = 0;
        size_t statement = at;
        size_t walk = at + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at],
                                      .cap = (body_last > at) ? (body_last - at) : 0u, .other = "({[)}];");
        while (walk < body_last)
        {
            const char character = text[walk];
            depth += ((character == '(') || (character == '{') || (character == '[')) ? 1 : 0;
            depth -= ((character == ')') || (character == '}') || (character == ']')) ? 1 : 0;
            walk += 1u;
            if (((character == ';') || (character == '}')) && (depth == 0))
            {
                if (filling)
                {
                    StickRange *const kept = &s_statements[*statements];
                    kept->at = *statement_bytes;
                    s_stick_cut |= stick_squeezed(&text[statement], walk - statement, &s_statement_text[kept->at],
                                                  (walk - statement) + 1u)
                                       ? 0
                                       : 1;
                    kept->length = stick_length(&s_statement_text[kept->at]);
                    source->statement_count += 1u;
                }
                *statements += 1u;
                *statement_bytes += (walk - statement) + 1u;
                statement = walk;
            }
            walk += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[walk], .cap = body_last - walk,
                               .other = "({[)}];");
        }
        at = stick_place_of(text, '\n', body_end, size) + 1u;
    }
}

// The stick at `path` read once into its cells: 1, or 0 where it does not read or the pool cannot meet a cell. It is 0
// also where a statement passes the room of the engine
static int stick_sources_read(const char *path)
{
    s_stick_text = stick_file_read(path, &s_stick_size, PLATFORM_TIER_PERSISTENT);
    if (s_stick_text == NULL)
    {
        return 0;
    }
    unsigned int sources = 0u;
    unsigned int statements = 0u;
    size_t bytes = 0u;
    stick_sources_made(0, &sources, &statements, &bytes);
    s_sources = (StickSource *)platform_cell_cleared((sources != 0u) ? sources : 1u, sizeof(StickSource),
                                                     platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    s_statements = (StickRange *)platform_cell_cleared((statements != 0u) ? statements : 1u, sizeof(StickRange),
                                                       platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    s_statement_text =
        (char *)platform_cell_taken(bytes + 1u, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if ((s_sources == NULL) || (s_statements == NULL) || (s_statement_text == NULL))
    {
        return 0;
    }
    stick_sources_made(1, &s_source_count, &s_statement_count, &bytes);
    return s_stick_cut == 0;
}

// the statement at `at` of the stick, squeezed and terminated [BORROWS]
static const char *stick_statement(unsigned int at)
{
    return &s_statement_text[s_statements[at].at];
}

// a kernel of nvcc's listing: its number, and its instructions from `first`, `count` of them, in s_instructions
struct StickListed
{
    StickWord number;
    unsigned int first;
    unsigned int count;
};

// nvcc's listing read once: its kernels in the order of their numbers, and their instructions. Each is in one cell of
// the persistent tier taken once at its count. The listing's text goes back once the code reads it
static StickListed *s_listed = NULL;
static unsigned int s_listed_count = 0u;
static StickLine *s_instructions = NULL;
static unsigned int s_instruction_count = 0u;

static int stick_listed_order(const void *left, const void *right)
{
    return stick_order_text(((const StickListed *)left)->number.text, ((const StickListed *)right)->number.text);
}

// the white space of a pattern's `\s`
static const char s_white[] = " \t\r\n\v\f";

// The instruction of the `length` bytes of a line of nvcc's listing at `line`, as `/\*[0-9a-f]{4,}\*/\s+(.*?)\s*;.*`
// reads it: its first byte into `first` and its bytes into `bytes`. 1, or 0 where the line is no instruction. The
// instruction runs from past the white space to the first `;`, with the white space before that `;` cut. A `.` reads
// no carriage return. A carriage return in the instruction or after the `;` then makes the line no instruction
static int stick_listed_instruction(const char *line, size_t length, size_t *first, size_t *bytes)
{
    const size_t digits = (length > 2u) ? EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &line[2], .cap = length - 2u,
                                                     .other = "0123456789abcdef")
                                        : 0u;
    const size_t close = 2u + digits;
    if ((digits < 4u) || ((close + 2u) > length) || (line[close] != '*') || (line[close + 1u] != '/'))
    {
        return 0;
    }
    const size_t white = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &line[close + 2u], .cap = length - close - 2u,
                                    .other = s_white);
    const size_t from = close + 2u + white;
    const char *const semicolon =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &line[from], .cap = length - from, .byte = (uint8_t)';');
    if ((white == 0u) || (semicolon == NULL))
    {
        return 0;
    }
    // the `;` lies within the line, at or past `from`. The instruction ends with its last run of no white space
    const size_t stop = (size_t)(semicolon - line);
    size_t end = from;
    for (size_t walk = from; walk < stop;)
    {
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line[walk], .cap = stop - walk, .other = s_white);
        walk += run;
        end = (run != 0u) ? walk : end;
        walk += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &line[walk], .cap = stop - walk, .other = s_white);
    }
    const size_t past = stop + 1u;
    if ((EMBED_CALL(memor.chr, MemoriaCfg, .src = &line[from], .bytes = end - from, .val = '\r') != NULL) ||
        (EMBED_CALL(memor.chr, MemoriaCfg, .src = &line[past], .bytes = length - past, .val = '\r') != NULL))
    {
        return 0;
    }
    *first = from;
    *bytes = end - from;
    return 1;
}

// the bytes of the guard that begins the terminated `instruction`, as `^@!?U?P[T0-9]+\s+` reads it, or 0
static size_t stick_listed_guard(const char *instruction)
{
    if (instruction[0] != '@')
    {
        return 0u;
    }
    size_t at = 1u;
    at += (instruction[at] == '!') ? 1u : 0u;
    at += (instruction[at] == 'U') ? 1u : 0u;
    if (instruction[at] != 'P')
    {
        return 0u;
    }
    at += 1u;
    const size_t predicate = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &instruction[at],
                                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "T0123456789");
    const size_t white = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &instruction[at + predicate],
                                    .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = s_white);
    return ((predicate != 0u) && (white != 0u)) ? (at + predicate + white) : 0u;
}

// The kernels and instructions of nvcc's listing in `text`, counted where `filling` is 0 and written where it is 1. The
// code leaves out NOP and takes a guard off each instruction
static void stick_listing_made(const char *text, size_t size, int filling, unsigned int *kernels,
                               unsigned int *instructions)
{
    static TablePatternOnce s_function;
    *kernels = 0u;
    *instructions = 0u;
    for (size_t at = 0u; at < size;)
    {
        const size_t end = stick_place_of(text, '\n', at, size);
        const size_t indent = stick_spaces(&text[at], end - at);
        const char *const line = &text[at + indent];
        const size_t length = end - at - indent;
        at = end + 1u;
        if ((length > 10u) && stick_begins(line, "Function :"))
        {
            StickExpr function_line;
            STICK_SET_N(function_line, line, length);
            TableMatch found;
            if (stick_matched(&s_function, "Function : measuring_stick_([0-9]+)\\s*", function_line.text, &found))
            {
                if (filling)
                {
                    STICK_SET(s_listed[*kernels].number, stick_group(&found, 1u, function_line.text).text);
                    s_listed[*kernels].first = *instructions;
                    s_listed[*kernels].count = 0u;
                }
                *kernels += 1u;
            }
            continue;
        }
        if ((*kernels == 0u) || (length < 2u) || !stick_begins(line, "/*"))
        {
            continue;
        }
        size_t first = 0u;
        size_t bytes = 0u;
        if (!stick_listed_instruction(line, length, &first, &bytes) || stick_same_n(&line[first], bytes, "NOP"))
        {
            continue;
        }
        if (filling)
        {
            StickExpr instruction;
            STICK_SET_N(instruction, &line[first], bytes);
            STICK_SET(s_instructions[*instructions], &instruction.text[stick_listed_guard(instruction.text)]);
            s_listed[*kernels - 1u].count += 1u;
        }
        *instructions += 1u;
    }
}

// nvcc's listing at `path` read once into its cells: 1, or 0 where it does not read or the pool cannot meet a cell. It
// is 0 also where a line passes the room of the engine
static int stick_listing_read(const char *path)
{
    size_t size = 0u;
    char *const text = stick_file_read(path, &size, PLATFORM_TIER_TEMPORARY);
    if (text == NULL)
    {
        return 0;
    }
    unsigned int kernels = 0u;
    unsigned int instructions = 0u;
    stick_listing_made(text, size, 0, &kernels, &instructions);
    s_listed = (StickListed *)platform_cell_cleared((kernels != 0u) ? kernels : 1u, sizeof(StickListed),
                                                    platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    s_instructions = (StickLine *)platform_cell_cleared((instructions != 0u) ? instructions : 1u, sizeof(StickLine),
                                                        platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if ((s_listed == NULL) || (s_instructions == NULL))
    {
        platform_cell_released(text, platform_place_g_program_owner);
        return 0;
    }
    stick_listing_made(text, size, 1, &s_listed_count, &s_instruction_count);
    platform_cell_released(text, platform_place_g_program_owner);
    return (s_stick_cut == 0) && stick_sorted(s_listed, s_listed_count, sizeof(StickListed), stick_listed_order);
}

// the kernel of nvcc's listing of the number `number` [BORROWS], NULL where the listing holds none
static const StickListed *stick_listed_of(const char *number)
{
    unsigned int low = 0u;
    unsigned int high = s_listed_count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (stick_order_text(s_listed[middle].number.text, number) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < s_listed_count) && stick_same(s_listed[low].number.text, number)) ? &s_listed[low] : NULL;
}

// the operands of an instruction of nvcc's listing, its operation cut, each bare, into `operands`: their count
static unsigned int stick_operands(const char *instruction, StickExpr *operands, unsigned int room)
{
    const size_t length = stick_length(instruction);
    size_t at = stick_place_of(instruction, ' ', 0u, length);
    at = (at < length) ? (at + 1u) : length;
    unsigned int count = 0u;
    while ((at < length) && (count < room))
    {
        const size_t comma = stick_place_of(instruction, ',', at, length);
        stick_bare_n(&instruction[at], comma - at, &operands[count]);
        count += 1u;
        at = (comma < length) ? (comma + 1u) : length;
    }
    return count;
}

// the operation of an instruction of nvcc's listing, its text up to its first space
static StickWord stick_listed_operation(const char *instruction)
{
    StickWord operation;
    STICK_SET_N(operation, instruction, stick_place_of(instruction, ' ', 0u, stick_length(instruction)));
    return operation;
}

// 1 where an instruction of `listed` has an operand `operand`, or one that holds `operand` where `within` is not 0. The
// instruction has the operation `operation`, or any but a move where that is empty
static int stick_listing_contains(const StickListed *listed, const char *operation, const char *operand, int within)
{
    for (unsigned int at = 0u; at < listed->count; at += 1u)
    {
        const char *const instruction = s_instructions[listed->first + at].text;
        const StickWord named = stick_listed_operation(instruction);
        const int taken = (operation[0] == '\0') ? !stick_begins(named.text, "MOV") : stick_same(named.text, operation);
        StickExpr operands[STICK_PLACEHOLDERS_MAX];
        const unsigned int count = stick_operands(instruction, operands, STICK_PLACEHOLDERS_MAX);
        for (unsigned int one = 0u; taken && (one < count); one += 1u)
        {
            const int contains = within ? stick_has(operands[one].text, operand)
                                        : (stick_same(operands[one].text, operand) ||
                                           stick_same(stick_constant(operands[one].text).text, operand));
            if (contains)
            {
                return 1;
            }
        }
    }
    return 0;
}

// 1 where an instruction of `listed` of operation `operation` reads no word of the constant bank, as the kernel's guard
// reads the count
static int stick_listing_writes(const StickListed *listed, const char *operation)
{
    for (unsigned int at = 0u; at < listed->count; at += 1u)
    {
        const char *const instruction = s_instructions[listed->first + at].text;
        if (stick_same(stick_listed_operation(instruction).text, operation) && !stick_has(instruction, "c["))
        {
            return 1;
        }
    }
    return 0;
}

// 1 where a statement of the kernel `source` from `from` on reads `name`, and each read of it converts to a type of the
// width of one register
static int stick_narrowed(const StickSource *source, unsigned int from, const char *name)
{
    static TablePatternOnce s_cast;
    const TablePattern *const cast = table_once(&s_cast, "\\(\\s*([A-Za-z ]+?)\\s*\\)\\s*$", 0);
    const size_t size = stick_length(name);
    unsigned int reads = 0u;
    for (unsigned int at = from; at < source->statement_count; at += 1u)
    {
        const char *const statement = stick_statement(source->statement_first + at);
        const size_t length = stick_length(statement);
        for (size_t place = stick_place(statement, name, 0u, length); place < length;
             place = stick_place(statement, name, place + 1u, length))
        {
            const size_t end = place + size;
            const int before = (place != 0u) && ((EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM,
                                                             .byte = (unsigned char)statement[place - 1u]) != 0) ||
                                                 (statement[place - 1u] == '_'));
            const int after = (end < length) && ((EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM,
                                                             .byte = (unsigned char)statement[end]) != 0) ||
                                                 (statement[end] == '_'));
            if (before || after)
            {
                continue;
            }
            StickExpr prefix;
            STICK_SET_N(prefix, statement, place);
            TableMatch found;
            const int type = table_search(cast, prefix.text, stick_length(prefix.text), &found)
                                 ? stick_type(stick_group(&found, 1u, prefix.text).text)
                                 : -1;
            if ((type < 0) || (s_types[type].bits != STICK_REGISTER_BITS))
            {
                return 0;
            }
            reads += 1u;
        }
    }
    return reads != 0u;
}

// ---- one kernel read ----

// a name the kernel declares and the type of its declaration, which an assignment to the name converts its value to
struct StickDeclared
{
    StickWord name;
    int type;
};

// `name` of the kernel given `expression` converted to `type`: 1, or 0 where the kernel's question states why not
static int stick_given(StickKernel *kernel, const char *name, int type, const char *expression)
{
    const StickTyped value = kernel->converted(kernel->typed(expression), type);
    if (value.type < 0)
    {
        return 0;
    }
    if (s_types[type].bits == STICK_PREDICATE_BITS)
    {
        kernel->bind_flag(name, value);
        return 1;
    }
    const StickValue binding = kernel->value(value.text.text);
    if (binding.name.text[0] == '\0')
    {
        return 0;
    }
    kernel->bind(name, binding, type, value.nonnegative);
    return 1;
}

// The parameters of `source` read into the kernel, each at the next offset its width aligns: 1, or 0 with the
// kernel's question
static int stick_parameters_read(const StickSource *source, StickKernel *kernel)
{
    static TablePatternOnce s_parameter;
    unsigned int offset = 0u;
    const char *const text = &s_stick_text[source->parameters.at];
    const size_t length = source->parameters.length;
    size_t at = 0u;
    while (at <= length)
    {
        const char *const comma =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .byte = (uint8_t)',');
        // a found comma lies within the parameters
        const size_t end = (comma != NULL) ? (size_t)(comma - text) : length;
        StickExpr one;
        STICK_SET_N(one, &text[at], end - at);
        TableMatch found;
        if (!stick_matched(&s_parameter, "\\s*(.*?)\\s*(\\*?)\\s*([A-Za-z_][A-Za-z_0-9]*)\\s*", one.text, &found) ||
            (kernel->parameter_count >= STICK_PARAMETERS_MAX))
        {
            kernel->question_put("a parameter nothing here reads: ", one.text);
            return 0;
        }
        const StickExpr type_text = stick_group(&found, 1u, one.text);
        const int pointer = stick_same(stick_group(&found, 2u, one.text).text, "*");
        const unsigned int bits = pointer ? 64u : 32u;
        offset = (offset + ((bits / 8u) - 1u)) & ~((bits / 8u) - 1u);
        StickParameter *const parameter = &kernel->parameter[kernel->parameter_count];
        kernel->parameter_count += 1u;
        STICK_SET(parameter->name, stick_group(&found, 3u, one.text).text);
        parameter->offset = offset;
        parameter->bits = bits;
        parameter->type = pointer ? -1 : stick_type(type_text.text);
        offset += bits / 8u;
        at = end + 1u;
    }
    return 1;
}

// A load of the operand `in[...]` of the kernel into `name` of type `type`, from the statement at `at`: 1, or 0 with
// the kernel's question
static int stick_operand_read(const StickSource *source, unsigned int at, StickKernel *kernel, const char *name,
                              int type, const char *index_text)
{
    const int element = stick_type("unsigned long long");
    const int word = stick_type("unsigned int");
    // The element in[i] is an unsigned long long at its address. A value no wider than a register converts from the
    // low word of the element, the word at the address of the element. A wider value, and a bool, which tests the
    // whole element, read the pair. A wider value that each later statement converts to a word before its read reads
    // as its low word alone
    const int narrowed = stick_narrowed(source, at + 1u, name);
    const int whole =
        ((s_types[type].bits > STICK_REGISTER_BITS) && !narrowed) || (s_types[type].bits == STICK_PREDICATE_BITS);
    const unsigned int load = stick_form(whole ? "global_load_wide" : "global_load_word");
    // in[i + k], k a number, is k elements past the address of in[i] where the system folds the offset
    StickExpr index = stick_bare(index_text);
    const StickExpr index_bare = index;
    unsigned int elements = 0u;
    StickExpr spans[2];
    if (kernel->reads_as("word_add", index.text, spans, 2u) && kernel->number_of(spans[1].text, &elements))
    {
        if (kernel->folded(load, "offset", 1u))
        {
            index = spans[0];
        }
        else if (elements != 0u)
        {
            kernel->asked_kept(STICK_OFFSETS, load, 0u, 8u * elements, "");
        }
    }
    const unsigned int element_offset = stick_same(index.text, index_bare.text) ? 0u : (8u * elements);
    StickExpr address_of;
    STICK_SET(address_of, "&in[");
    STICK_ADD(address_of, index.text);
    STICK_ADD(address_of, "]");
    if (!kernel->binds(address_of.text))
    {
        const StickValue address = kernel->fresh(64u);
        StickExpr operands[3];
        operands[0] = index;
        operands[1] = kernel->number_text(8u);
        STICK_SET(operands[2], "in");
        if (!kernel->form("wide_mul_word_add", &address.name, 1u, operands, 3u))
        {
            return 0;
        }
        kernel->bind(address_of.text, address, -1);
    }
    const StickValue loaded = kernel->fresh(whole ? STICK_PAIR_BITS : STICK_REGISTER_BITS);
    StickExpr load_operands[2];
    load_operands[0] = address_of;
    load_operands[1] = kernel->number_text(element_offset);
    if (!kernel->form(s_form_names[load], &loaded.name, 1u, load_operands, 2u))
    {
        return 0;
    }
    if (!whole && (s_types[type].bits > STICK_REGISTER_BITS))
    {
        kernel->bind(name, loaded, type);
        return 1;
    }
    // A byte or a halfword is the narrowing of the loaded word, written where its read is. A bool is no narrowing. It
    // is the whole loaded value, tested against 0, and the code below reads it as a flag of that value
    if ((s_types[type].bits < STICK_REGISTER_BITS) && (s_types[type].bits != STICK_PREDICATE_BITS))
    {
        StickWord loaded_word;
        STICK_SET(loaded_word, name);
        STICK_ADD(loaded_word, "@word");
        // the word is the low word of the load, the even register where the load took a pair
        StickValue low = loaded;
        low.bits = STICK_REGISTER_BITS;
        kernel->bind(loaded_word.text, low, word);
        const StickTyped narrowed_value = kernel->converted(stick_typed(loaded_word.text, word, 0), type);
        if (narrowed_value.type < 0)
        {
            return 0;
        }
        kernel->are(name, narrowed_value);
        return 1;
    }
    const int read_as = whole ? element : word;
    kernel->bind(name, loaded, read_as);
    const StickTyped value = kernel->converted(stick_typed(name, read_as, 0), type);
    if (value.type < 0)
    {
        return 0;
    }
    if (s_types[type].bits == STICK_PREDICATE_BITS)
    {
        kernel->bind_flag(name, value);
        return 1;
    }
    const StickValue binding = kernel->value(value.text.text);
    if (binding.name.text[0] == '\0')
    {
        return 0;
    }
    kernel->bind(name, binding, type, value.nonnegative);
    return 1;
}

// the type the kernel declared `name` of among the `count` at `declared`, or -1
static int stick_declared_of(const StickDeclared *declared, unsigned int count, const char *name)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (stick_same(declared[at].name.text, name))
        {
            return declared[at].type;
        }
    }
    return -1;
}

// `name` declared of `type` among the declared, in place of the type it had before
static void stick_declared_put(StickDeclared *declared, unsigned int *count, unsigned int room, const char *name,
                               int type)
{
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (stick_same(declared[at].name.text, name))
        {
            declared[at].type = type;
            return;
        }
    }
    if (*count < room)
    {
        STICK_SET(declared[*count].name, name);
        declared[*count].type = type;
        *count += 1u;
        return;
    }
    s_stick_room = 1;
}

// One statement of `source`, at `at`, read and written into the kernel: 1, or 0 with the kernel's question
static int stick_statement_read(const StickSource *source, unsigned int at, StickKernel *kernel,
                                StickDeclared *declared, unsigned int *declared_count, unsigned int declared_room)
{
    static TablePatternOnce s_operand;
    static TablePatternOnce s_declared;
    static TablePatternOnce s_assigned;
    static TablePatternOnce s_stored;
    const char *const statement = stick_statement(source->statement_first + at);
    StickExpr text;
    STICK_SET(text, statement);
    TableMatch found;
    if (stick_matched(&s_operand, "(?:const )?(.+) ([A-Za-z_][A-Za-z_0-9]*) = \\((.+)\\)in\\[(.+)\\];", text.text,
                      &found))
    {
        const StickExpr type_name = stick_group(&found, 1u, text.text);
        const int type = stick_type(type_name.text);
        if ((type < 0) || !stick_same(type_name.text, stick_group(&found, 3u, text.text).text))
        {
            kernel->question_put("a value of type ", type_name.text);
            return 0;
        }
        const StickExpr name_text = stick_group(&found, 2u, text.text);
        stick_declared_put(declared, declared_count, declared_room, name_text.text, type);
        const StickExpr name = stick_bare(name_text.text);
        return stick_operand_read(source, at, kernel, name.text, type, stick_group(&found, 4u, text.text).text);
    }
    if (stick_matched(&s_stored, "out\\[(.+)\\] = \\(unsigned long long\\)([A-Za-z_][A-Za-z_0-9]*);", text.text,
                      &found))
    {
        StickExpr widened;
        STICK_SET(widened, "(unsigned long long)");
        STICK_ADD(widened, stick_group(&found, 2u, text.text).text);
        const StickTyped value = kernel->typed(widened.text);
        if ((value.type < 0) || (kernel->value(value.text.text).name.text[0] == '\0'))
        {
            return 0;
        }
        const StickExpr index = stick_group(&found, 1u, text.text);
        StickExpr address_of;
        STICK_SET(address_of, "&out[");
        STICK_ADD(address_of, stick_bare(index.text).text);
        STICK_ADD(address_of, "]");
        const StickValue address = kernel->fresh(64u);
        StickExpr operands[3];
        operands[0] = index;
        operands[1] = kernel->number_text(8u);
        STICK_SET(operands[2], "out");
        if (!kernel->form("wide_mul_word_add", &address.name, 1u, operands, 3u))
        {
            return 0;
        }
        kernel->bind(address_of.text, address, -1);
        StickExpr store[3];
        store[0] = address_of;
        store[1] = kernel->number_text(0u);
        store[2] = value.text;
        return kernel->form("global_store_wide", NULL, 0u, store, 3u);
    }
    if (stick_matched(&s_declared, "(?:const )?(.+) ([A-Za-z_][A-Za-z_0-9]*) = (.+);", text.text, &found))
    {
        const StickExpr type_name = stick_group(&found, 1u, text.text);
        const int type = stick_type(type_name.text);
        if (type < 0)
        {
            kernel->question_put("a value of type ", type_name.text);
            return 0;
        }
        const StickExpr name = stick_bare(stick_group(&found, 2u, text.text).text);
        stick_declared_put(declared, declared_count, declared_room, name.text, type);
        return stick_given(kernel, name.text, type, stick_group(&found, 3u, text.text).text);
    }
    // an assignment, and a compound one read as the assignment of its operator over the name and the value
    if (stick_matched(&s_assigned, "([A-Za-z_][A-Za-z_0-9]*) (\\+|-|\\*|/|%|&|\\||\\^|<<|>>)?= (.+);", text.text,
                      &found))
    {
        const StickExpr name = stick_group(&found, 1u, text.text);
        const int type = stick_declared_of(declared, *declared_count, name.text);
        if (type >= 0)
        {
            const StickExpr operation = stick_group(&found, 2u, text.text);
            StickExpr expression;
            if (operation.text[0] == '\0')
            {
                expression = stick_group(&found, 3u, text.text);
            }
            else
            {
                STICK_SET(expression, "(");
                STICK_ADD(expression, name.text);
                STICK_ADD(expression, ")");
                STICK_ADD(expression, operation.text);
                STICK_ADD(expression, "(");
                STICK_ADD(expression, stick_group(&found, 3u, text.text).text);
                STICK_ADD(expression, ")");
            }
            return stick_given(kernel, name.text, type, expression.text);
        }
    }
    if (kernel->statement(stick_bare(statement).text))
    {
        return 1;
    }
    if (kernel->question[0] == '\0')
    {
        kernel->question_put("a statement nothing here reads: ", statement);
    }
    return 0;
}

// `source` read and written into `kernel`: 1, or 0 with the question it puts in the kernel's question
static int stick_kernel_read(const StickSource *source, StickKernel *kernel)
{
    if (!stick_parameters_read(source, kernel))
    {
        return 0;
    }
    (void)kernel->write("kernel_open", NULL, 0u);
    // the type of the declaration of each name, which an assignment to the name converts its value to
    const unsigned int declared_room = source->statement_count + 1u;
    StickDeclared *const declared =
        (StickDeclared *)platform_cell_taken((unsigned long long)declared_room * sizeof(StickDeclared),
                                             platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (declared == NULL)
    {
        kernel->note_question("the pool cannot meet the kernel's declarations");
        return 0;
    }
    unsigned int declared_count = 0u;
    int read_whole = 1;
    for (unsigned int at = 0u; read_whole && (at < source->statement_count); at += 1u)
    {
        // a statement's readings take the places of the statement before
        s_node_count = 0u;
        s_cached_count = 0u;
        s_span_top = 0u;
        s_text_top = 0u;
        s_option_top = 0u;
        s_exchanging_count = 0u;
        read_whole = stick_statement_read(source, at, kernel, declared, &declared_count, declared_room);
        if ((s_stick_cut != 0) || (s_stick_room != 0) || (s_spans_full != 0) || (s_nodes_full != 0))
        {
            kernel->question[0] = '\0';
            kernel->note_question((s_stick_cut != 0) ? "a statement whose texts pass the room of the engine: "
                                                     : "a statement whose readings pass the room of the engine: ",
                                  stick_statement(source->statement_first + at));
            read_whole = 0;
        }
    }
    platform_cell_released(declared, platform_place_g_program_owner);
    if (!read_whole)
    {
        return 0;
    }
    (void)kernel->write("kernel_close", NULL, 0u);
    // a load folded into its extension writes the register lines of the extension sooner. The registers go over the
    // lines that the folded text holds
    kernel->fold_loads();
    kernel->assign();
    kernel->balance();
    return kernel->question[0] == '\0';
}

// the tokens of the statements of `source`, counted as the parser cuts them
static unsigned int stick_tokens_of(const StickSource *source)
{
    unsigned int tokens = 0u;
    for (unsigned int at = 0u; at < source->statement_count; at += 1u)
    {
        tokens +=
            stick_tokens(stick_statement(source->statement_first + at), &s_tokens) ? s_tokens.count : STICK_TOKENS_MAX;
    }
    return tokens;
}

// the flags that a read of the kernel left, cleared for the kernel after it
static void stick_flags_cleared(void)
{
    s_stick_cut = 0;
    s_stick_room = 0;
    s_spans_full = 0;
    s_nodes_full = 0;
    s_node_count = 0u;
    s_cached_count = 0u;
    s_span_top = 0u;
    s_text_top = 0u;
    s_option_top = 0u;
    s_exchanging_count = 0u;
}

// ---- the rounds ----

// the lines of sass.ksc a round drops and the folds it finds, each once, at a bound
#define STICK_FOLD_LINES_MAX 4096u

struct StickFoldLines
{
    StickExpr line[STICK_FOLD_LINES_MAX];
    unsigned int count;
};

static StickFoldLines s_openings;
static StickFoldLines s_found;
// 1 once a round found more fold lines than the bound holds
static int s_fold_lines_full = 0;

static int stick_fold_line_order(const void *left, const void *right)
{
    return stick_order_text(((const StickExpr *)left)->text, ((const StickExpr *)right)->text);
}

// the lines put in the order of their texts, each once
static void stick_fold_lines_settled(StickFoldLines *lines)
{
    s_stick_room |= stick_sorted(lines->line, lines->count, sizeof(StickExpr), stick_fold_line_order) ? 0 : 1;
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < lines->count; at += 1u)
    {
        if ((kept == 0u) || !stick_same(lines->line[kept - 1u].text, lines->line[at].text))
        {
            lines->line[kept] = lines->line[at];
            kept += 1u;
        }
    }
    lines->count = kept;
}

// The line `line` put at the end of `lines`. Where they are full, the code settles them first. Where they are full
// after that, the line has no room, and the round puts that it passes the room of the engine
static void stick_fold_line_kept(StickFoldLines *lines, const StickExpr &line)
{
    if (lines->count == STICK_FOLD_LINES_MAX)
    {
        stick_fold_lines_settled(lines);
    }
    if (lines->count == STICK_FOLD_LINES_MAX)
    {
        s_fold_lines_full = 1;
        return;
    }
    lines->line[lines->count] = line;
    lines->count += 1u;
}

// One round of the stick, with folds by what sass.ksc holds where `folding` is not 0. The code reads each kernel and
// checks it against the listing of nvcc for it. Each question that it needs goes to s_openings. Each fold that the
// listing of nvcc shows goes to s_found
static void stick_round(const Ruleset *cu, const Ruleset *sass, const SassMachine *machine, int folding)
{
    static StickKernel s_kernel;
    for (unsigned int at = 0u; at < s_source_count; at += 1u)
    {
        const StickSource *const source = &s_sources[at];
        StickKernel *const kernel = &s_kernel;
        stick_flags_cleared();
        const int begun =
            stick_kernel_begun(kernel, cu, sass, machine, folding, source->statement_count, stick_tokens_of(source));
        const StickListed *const nvcc = stick_listed_of(source->number.text);
        if (!begun || !stick_kernel_read(source, kernel) || (nvcc == NULL))
        {
            stick_kernel_ended(kernel);
            continue;
        }
        // The system folds the move of a word into a wide, and the move of a word used once. It does so where its
        // listing of the kernel moves no register into another
        static TablePatternOnce s_register;
        int moves = 0;
        for (unsigned int one = 0u; one < nvcc->count; one += 1u)
        {
            const char *const instruction = s_instructions[nvcc->first + one].text;
            StickExpr operands[STICK_PLACEHOLDERS_MAX];
            const unsigned int count = stick_operands(instruction, operands, STICK_PLACEHOLDERS_MAX);
            TableMatch found;
            moves = (stick_begins(instruction, "MOV ") && (count == 2u) &&
                     stick_matched(&s_register, "R[0-9]+", operands[1].text, &found))
                        ? 1
                        : moves;
        }
        for (unsigned int one = 0u; one < kernel->asked_count; one += 1u)
        {
            const StickAsked *const asked = &kernel->asked[one];
            const StickAskedType type = (StickAskedType)(asked->parameter >> 24u);
            const unsigned int parameter = asked->parameter & 0x00FFFFFFu;
            const unsigned int form = asked->form;
            if (type == STICK_MOVED)
            {
                stick_fold_line_kept(&s_openings, stick_fold_line(form, "register", 0u, 1));
                if (stick_listing_contains(nvcc, "", stick_constant(asked->word.text).text, 0))
                {
                    stick_fold_line_kept(&s_found, stick_fold_line(form, "register", 0u, 0));
                }
            }
            else if (type == STICK_MULTIPLIED)
            {
                stick_fold_line_kept(&s_openings, stick_fold_line(form, "pow2", parameter, 1));
                if (stick_listing_contains(nvcc, stick_operation_of(sass, stick_shift_of(form)).text,
                                           stick_hex(asked->number).text, 0))
                {
                    stick_fold_line_kept(&s_found, stick_fold_line(form, "pow2", parameter, 0));
                }
            }
            else if ((type == STICK_COPIED) || (type == STICK_COPIED_ONCE))
            {
                const char *const fold_name = (type == STICK_COPIED) ? "register" : "word_used_once";
                stick_fold_line_kept(&s_openings, stick_fold_line(form, fold_name, 1u, 1));
                if (moves == 0)
                {
                    stick_fold_line_kept(&s_found, stick_fold_line(form, fold_name, 1u, 0));
                }
            }
            else if (type == STICK_COMPLEMENTED)
            {
                // the system tests a select's test as its complement where its listing writes the complement's
                // operation and not the test's
                stick_fold_line_kept(&s_openings, stick_fold_line(form, "complement", 0u, 1));
                if (stick_listing_writes(nvcc, stick_operation_of(sass, asked->number).text) &&
                    !stick_listing_writes(nvcc, stick_operation_of(sass, form).text))
                {
                    stick_fold_line_kept(&s_found, stick_fold_line(form, "complement", 0u, 0));
                }
            }
            else if (type == STICK_OFFSETS)
            {
                stick_fold_line_kept(&s_openings, stick_fold_line(form, "offset", 1u, 1));
                StickWord offset;
                STICK_SET(offset, "+");
                STICK_ADD(offset, stick_hex(asked->number).text);
                STICK_ADD(offset, "]");
                if (stick_listing_contains(nvcc, stick_operation_of(sass, form).text, offset.text, 1))
                {
                    stick_fold_line_kept(&s_found, stick_fold_line(form, "offset", 1u, 0));
                }
            }
        }
        stick_kernel_ended(kernel);
    }
}

// sass.ksc written: what it contained that opens with each of s_openings dropped, and each of s_found taken, each in
// the order of its text. And the pipes the part's .kdm records taken, for the rounds after to write by
static void stick_folds_write(const Ruleset *sass)
{
    const size_t length = stick_length(sass->path);
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = sass->path, .cap = length, .byte = (uint8_t)'/');
    // the path's last slash lies within it
    const size_t folder_length = (slash != NULL) ? (size_t)(slash - sass->path) : 0u;
    StickExpr folder;
    STICK_SET_N(folder, sass->path, folder_length);
    StickExpr stem;
    const char *const file = &sass->path[(slash != NULL) ? (folder_length + 1u) : 0u];
    const char *const dot =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = file, .cap = stick_length(file), .byte = (uint8_t)'.');
    // the stem's dot lies within the file's name
    STICK_SET_N(stem, file, (dot != NULL) ? (size_t)(dot - file) : stick_length(file));
    sass_class_read(folder.text, stem.text);
    sass_pipe_read(s_machines, s_part);
    stick_fold_lines_settled(&s_openings);
    stick_fold_lines_settled(&s_found);
    for (unsigned int at = 0u; at < s_openings.count; at += 1u)
    {
        sass_class_drop(SASS_CHANNEL_COMPILE, SASS_CLASS_FOLDS, s_openings.line[at].text);
    }
    for (unsigned int at = 0u; at < s_found.count; at += 1u)
    {
        sass_class_take(SASS_CHANNEL_COMPILE, SASS_CLASS_FOLDS, s_found.line[at].text, 0u);
    }
    sass_class_write(folder.text, stem.text);
}

// One write of an operation the system writes on either pipe, as nvcc's listing gives it. It has the operation, its
// write on the FMA pipe, and 1 where the listing writes it there. Then the lane of its kernel
struct StickPipeSeen
{
    StickWord operation;
    StickWord written_on;
    int on_fma;
    StickLane lane;
};

static int stick_seen_order(const void *left, const void *right)
{
    return stick_order_text(((const StickPipeSeen *)left)->operation.text,
                            ((const StickPipeSeen *)right)->operation.text);
}

static int stick_count_order(const void *left, const void *right)
{
    const unsigned int one = *(const unsigned int *)left;
    const unsigned int other = *(const unsigned int *)right;
    return (one == other) ? 0 : ((one < other) ? -1 : 1);
}

// the `count` counts at `counts` in their order, each once: their count
static unsigned int stick_counts_settled(unsigned int *counts, unsigned int count)
{
    (void)stick_sorted(counts, count, sizeof(unsigned int), stick_count_order);
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((kept == 0u) || (counts[kept - 1u] != counts[at]))
        {
            counts[kept] = counts[at];
            kept += 1u;
        }
    }
    return kept;
}

// The lane that the system moves each operation in, for each operation that it writes on either pipe. The code reads
// it off the listing of nvcc, and stores it in the .kdm of the part. It takes each least count of the integer pipe
// and each count over the FMA pipe. Of those, it takes the pair under which most writes of the operation in the
// listing are on their own pipe in the listing. Where two pairs tie, it takes the smaller pair
static void stick_pipes_write(void)
{
    StickPipeSeen *const seen = (StickPipeSeen *)platform_cell_taken(
        ((s_instruction_count != 0u) ? s_instruction_count : 1u) * sizeof(StickPipeSeen),
        platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const leasts = (unsigned int *)platform_cell_taken(
        ((2u * s_listed_count) + 1u) * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const overs = (unsigned int *)platform_cell_taken(
        ((2u * s_listed_count) + 1u) * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((seen == NULL) || (leasts == NULL) || (overs == NULL))
    {
        platform_cell_released(overs, platform_place_g_program_owner);
        platform_cell_released(leasts, platform_place_g_program_owner);
        platform_cell_released(seen, platform_place_g_program_owner);
        return;
    }
    unsigned int seen_count = 0u;
    // a bound worth a try is a count that a lane has, and one past it
    unsigned int least_count = 0u;
    unsigned int over_count = 0u;
    leasts[least_count] = 0u;
    least_count += 1u;
    overs[over_count] = 0u;
    over_count += 1u;
    for (unsigned int kernel = 0u; kernel < s_listed_count; kernel += 1u)
    {
        const StickListed *const listed = &s_listed[kernel];
        const StickLane lane = stick_pipe_lane(&s_instructions[listed->first], listed->count);
        const unsigned int difference = (lane.integer > lane.fma) ? (lane.integer - lane.fma) : 0u;
        leasts[least_count] = lane.integer;
        least_count += 1u;
        overs[over_count] = difference;
        over_count += 1u;
        for (unsigned int one = 0u; one < listed->count; one += 1u)
        {
            const char *const instruction = s_instructions[listed->first + one].text;
            StickExpr writing;
            const StickWord operation = stick_pipe_choice(instruction, &writing);
            if (operation.text[0] == '\0')
            {
                continue;
            }
            StickPipeSeen *const kept = &seen[seen_count];
            seen_count += 1u;
            kept->operation = operation;
            STICK_SET_N(kept->written_on, writing.text,
                        stick_place_of(writing.text, ' ', 0u, stick_length(writing.text)));
            kept->on_fma = stick_begins(instruction, "IMAD") ? 1 : 0;
            kept->lane = lane;
        }
    }
    // each count past a lane's, one past it
    const unsigned int lane_leasts = least_count;
    for (unsigned int at = 1u; at < lane_leasts; at += 1u)
    {
        leasts[least_count] = leasts[at] + 1u;
        least_count += 1u;
    }
    const unsigned int lane_overs = over_count;
    for (unsigned int at = 1u; at < lane_overs; at += 1u)
    {
        overs[over_count] = overs[at] + 1u;
        over_count += 1u;
    }
    least_count = stick_counts_settled(leasts, least_count);
    over_count = stick_counts_settled(overs, over_count);
    (void)stick_sorted(seen, seen_count, sizeof(StickPipeSeen), stick_seen_order);
    sass_pipe_read(s_machines, s_part);
    unsigned int run = 0u;
    while (run < seen_count)
    {
        unsigned int next = run + 1u;
        while ((next < seen_count) && stick_same(seen[next].operation.text, seen[run].operation.text))
        {
            next += 1u;
        }
        unsigned int best = 0u;
        unsigned int chosen_least = 0u;
        unsigned int chosen_over = 0u;
        for (unsigned int least = 0u; least < least_count; least += 1u)
        {
            for (unsigned int over = 0u; over < over_count; over += 1u)
            {
                unsigned int agree = 0u;
                for (unsigned int one = run; one < next; one += 1u)
                {
                    const StickLane lane = seen[one].lane;
                    const int moves = (lane.integer >= leasts[least]) && (lane.integer >= (lane.fma + overs[over]));
                    agree += (moves == seen[one].on_fma) ? 1u : 0u;
                }
                if (agree > best)
                {
                    best = agree;
                    chosen_least = leasts[least];
                    chosen_over = overs[over];
                }
            }
        }
        // the write of an operation as the last of its writes gives it
        const StickWord written_on = seen[next - 1u].written_on;
        sass_pipe_take(seen[run].operation.text, written_on.text, chosen_least, chosen_over);
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  pipe: ");
        mmgr_line_put(&engine_note.line, seen[run].operation.text);
        mmgr_line_put(&engine_note.line, " written ");
        mmgr_line_put(&engine_note.line, written_on.text);
        mmgr_line_put(&engine_note.line, " from ");
        mmgr_line_decimal(&engine_note.line, chosen_least, 1u);
        mmgr_line_put(&engine_note.line, " on the integer pipe and ");
        mmgr_line_decimal(&engine_note.line, chosen_over, 1u);
        mmgr_line_put(&engine_note.line, " over the FMA pipe, ");
        mmgr_line_decimal(&engine_note.line, best, 1u);
        mmgr_line_put(&engine_note.line, " of ");
        mmgr_line_decimal(&engine_note.line, next - run, 1u);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
        run = next;
    }
    sass_pipe_write(s_machines, s_part);
    platform_cell_released(overs, platform_place_g_program_owner);
    platform_cell_released(leasts, platform_place_g_program_owner);
    platform_cell_released(seen, platform_place_g_program_owner);
}

// a line on the engine's note: `head`, then `text`, then `tail`
static void stick_noted(const char *head, const char *text, const char *tail)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, head);
    mmgr_line_put(&engine_note.line, text);
    mmgr_line_put(&engine_note.line, tail);
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
}

// The last round's kernel `source` written out, and its line in `table`. Its <number>.sass, .bin and .registers go to
// the run's writer, each a file it writes as the run ends (qry_member_filed). The trace goes to the run as
// <number>.trace. The answer is the count of its files not filed, and 1 in `answered` where it answered
static unsigned int stick_kernel_filed(const StickSource *source, const Ruleset *cu, const Ruleset *sass,
                                       const SassMachine *machine, const char *out, int traced, PlatformText *table,
                                       int *answered)
{
    static StickKernel s_kernel;
    StickKernel *const kernel = &s_kernel;
    *answered = 0;
    stick_flags_cleared();
    s_tracing = traced;
    if (s_tracing)
    {
        platform_text_begun(&s_trace, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    }
    const int begun =
        stick_kernel_begun(kernel, cu, sass, machine, 1, source->statement_count, stick_tokens_of(source));
    if (!begun)
    {
        kernel->note_question("the pool cannot meet the kernel's tables");
    }
    const int read_whole = begun && stick_kernel_read(source, kernel);
    if (s_tracing)
    {
        mmgr_line_put(&s_trace.line, read_whole ? "answered\n" : "question ");
        mmgr_line_put(&s_trace.line, read_whole ? "" : kernel->question);
        mmgr_line_put(&s_trace.line, read_whole ? "" : "\n");
        StickWord trace_name;
        STICK_SET(trace_name, source->number.text);
        STICK_ADD(trace_name, ".trace");
        const char *const trace_bytes = platform_text_bytes(&s_trace);
        (void)qry_member_append(trace_name.text, (trace_bytes != NULL) ? trace_bytes : "", platform_text_size(&s_trace),
                                QRY_ORDINARY);
        platform_text_ended(&s_trace);
        s_tracing = 0;
    }
    if (!read_whole)
    {
        mmgr_line_put(&table->line, source->number.text);
        mmgr_line_put(&table->line, "\tquestion\t0\t0\t");
        mmgr_line_put(&table->line, kernel->question);
        mmgr_line_put(&table->line, "\n");
        stick_kernel_ended(kernel);
        return 0u;
    }
    StickExpr chain;
    STICK_SET(chain, out);
    STICK_ADD(chain, "/");
    STICK_ADD(chain, source->number.text);
    unsigned int unfiled = 0u;
    size_t length = 0u;
    const char *const bytes = kernel->text_bytes(&length);
    StickExpr path;
    STICK_SET(path, chain.text);
    STICK_ADD(path, ".sass");
    unfiled += !qry_member_filed(path.text, bytes, length) ? 1u : 0u;
    unsigned int count = 0u;
    for (size_t at = 0u; at < length;)
    {
        const size_t end = stick_place_of(bytes, '\n', at, length);
        StickExpr line;
        s_stick_cut |= stick_squeezed(&bytes[at], end - at, line.text, sizeof(line.text)) ? 0 : 1;
        count += ((line.text[0] != '\0') && !stick_ends(line.text, ":")) ? 1u : 0u;
        at = end + 1u;
    }
    const unsigned long long room = 16ULL * count;
    unsigned char *const code = (unsigned char *)platform_cell_taken(
        (room != 0ULL) ? room : 1ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((code == NULL) || (sass_assemble_lines(machine, bytes, SASS_CONTROL_SAFE, code, room) != count))
    {
        platform_cell_released(code, platform_place_g_program_owner);
        mmgr_line_put(&table->line, source->number.text);
        mmgr_line_put(&table->line, "\tquestion\t");
        mmgr_line_decimal(&table->line, kernel->forms, 0u);
        mmgr_line_put(&table->line, "\t");
        mmgr_line_decimal(&table->line, count, 0u);
        mmgr_line_put(&table->line, "\tthe assembler refuses a line of the kernel\n");
        stick_kernel_ended(kernel);
        return unfiled;
    }
    STICK_SET(path, chain.text);
    STICK_ADD(path, ".bin");
    unfiled += !qry_member_filed(path.text, code, room) ? 1u : 0u;
    platform_cell_released(code, platform_place_g_program_owner);
    // the count of registers the kernel names, then each register it uses and the lines that claim and release it
    PlatformText registers;
    platform_text_begun(&registers, STICK_KERNEL_TEXT_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    mmgr_line_decimal(&registers.line, kernel->register_count, 1u);
    mmgr_line_ch(&registers.line, '\n');
    const char *const register_bytes = platform_text_bytes(&kernel->register_lines);
    // the register lines are at max what a size_t counts
    (void)mmgr_line_put_n(&registers.line, (register_bytes != NULL) ? register_bytes : "",
                          (size_t)platform_text_size(&kernel->register_lines));
    STICK_SET(path, chain.text);
    STICK_ADD(path, ".registers");
    const char *const registers_bytes = platform_text_bytes(&registers);
    unfiled +=
        !qry_member_filed(path.text, (registers_bytes != NULL) ? registers_bytes : "", platform_text_size(&registers))
            ? 1u
            : 0u;
    platform_text_ended(&registers);
    mmgr_line_put(&table->line, source->number.text);
    mmgr_line_put(&table->line, "\tanswered\t");
    mmgr_line_decimal(&table->line, kernel->forms, 0u);
    mmgr_line_put(&table->line, "\t");
    mmgr_line_decimal(&table->line, count, 0u);
    mmgr_line_put(&table->line, "\t\n");
    *answered = 1;
    stick_kernel_ended(kernel);
    return unfiled;
}

int program_main(int argc, char **argv)
{
    if (argc != 4)
    {
        stick_noted("measuring_stick_engine <stick .cu> <nvcc listing> <out directory>", "", "");
        return 2;
    }
    // the run that gets each file that the engine leaves, and the trace of each kernel
    QryShared *const run = qry_member_run();
    if (run == NULL)
    {
        stick_noted("measuring_stick_engine: no query writer runs to take its files: ", QRY_ENVIRONMENT,
                    " names no .qry a writer has open");
        return 2;
    }
    const Ruleset *const cu = cu_target().ruleset(1);
    const Ruleset *const sass = sass_target().ruleset(1);
    if ((cu == NULL) || (sass == NULL))
    {
        return 1;
    }
    static SassMachine s_machine;
    s_part = (sass->part != NULL) ? sass->part : "";
    if (s_part[0] == '\0')
    {
        stick_noted("sass.krs names no part, and no machine file is read", "", "");
        return 1;
    }
    const char *const slash = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = sass->path,
                                         .cap = stick_length(sass->path), .byte = (uint8_t)'/');
    // the path's last slash lies within it
    stick_set_n(s_machines, sizeof(s_machines), sass->path, (slash != NULL) ? (size_t)(slash - sass->path) : 0u);
    StickExpr machine_path;
    STICK_SET(machine_path, s_machines);
    STICK_ADD(machine_path, "/");
    STICK_ADD(machine_path, s_part);
    STICK_ADD(machine_path, ".khw");
    if (sass_machine_read(&s_machine, machine_path.text) == 0)
    {
        stick_noted("the machine file ", machine_path.text, " is not read");
        return 1;
    }
    stick_parameter_names_read(sass);
    if (!stick_patterns_read(cu, sass) || !stick_sources_read(argv[1]) || !stick_listing_read(argv[2]))
    {
        stick_noted("measuring_stick_engine: the stick, nvcc's listing or the patterns are not read: ", argv[1], "");
        return 1;
    }
    for (unsigned int round = 0u; round < STICK_ROUNDS; round += 1u)
    {
        const unsigned int before = s_found.count;
        stick_round(cu, sass, &s_machine, (round == 0u) ? 0 : 1);
        stick_folds_write(sass);
        sass_target().folds_read();
        if ((round != 0u) && (s_found.count == before))
        {
            break;
        }
    }
    if (s_fold_lines_full != 0)
    {
        stick_noted("measuring_stick_engine: a round found more fold lines than the engine holds", "", "");
        return 1;
    }
    stick_pipes_write();
    const char *const out = argv[3];
    // engine.tsv, the line of each kernel, and the <number>.sass, .bin and .registers of each kernel. Each goes to the
    // writer of the run, as a file that it writes as the run ends (qry_member_filed)
    PlatformText table;
    platform_text_begun(&table, QRY_WRITER_RING_BYTES / 2ULL, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    mmgr_line_put(&table.line, "number\tanswer\tforms\tinstructions\tnote\n");
    unsigned int answered = 0u;
    unsigned int unfiled = 0u;
    for (unsigned int at = 0u; at < s_source_count; at += 1u)
    {
        int answered_here = 0;
        unfiled += stick_kernel_filed(&s_sources[at], cu, sass, &s_machine, out, 1, &table, &answered_here);
        answered += answered_here ? 1u : 0u;
    }
    StickExpr table_path;
    STICK_SET(table_path, out);
    STICK_ADD(table_path, "/engine.tsv");
    const char *const table_bytes = platform_text_bytes(&table);
    const int filed = mmgr_line_ok(&table.line) && (table_bytes != NULL) &&
                      qry_member_filed(table_path.text, table_bytes, platform_text_size(&table));
    platform_text_ended(&table);
    if (!filed || (unfiled != 0u))
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "measuring_stick_engine: engine.tsv or ");
        mmgr_line_decimal(&engine_note.line, unfiled, 1u);
        mmgr_line_put(&engine_note.line, " of the kernels' files under ");
        mmgr_line_put(&engine_note.line, out);
        mmgr_line_put(&engine_note.line, " were not written");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
        return 1;
    }
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, "measuring stick through cu.krs and sass.krs: ");
    mmgr_line_decimal(&engine_note.line, s_source_count, 1u);
    mmgr_line_put(&engine_note.line, " kernels, ");
    mmgr_line_decimal(&engine_note.line, answered, 1u);
    mmgr_line_put(&engine_note.line, " answered, ");
    mmgr_line_decimal(&engine_note.line, s_source_count - answered, 1u);
    mmgr_line_put(&engine_note.line, " put a question; ");
    mmgr_line_decimal(&engine_note.line, s_found.count, 1u);
    mmgr_line_put(&engine_note.line, " folds found on the compile channel, ");
    mmgr_line_decimal(&engine_note.line, sass->folds.count, 1u);
    mmgr_line_put(&engine_note.line, " stored in sass.ksc");
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
    return 0;
}
