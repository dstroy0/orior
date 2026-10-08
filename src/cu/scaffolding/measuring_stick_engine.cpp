// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_engine: each kernel of the measuring stick read form by form through cu.krs, and each form written at
// once in sass.krs, the two rulesets of one schema (cu_target.h). The kernel is assembled against the part's machine
// file and its encodings written for nvdisasm to read back.
//
//     measuring_stick_engine <stick .cu> <nvcc listing> <out directory>
//
// An expression is read against every form cu.krs gives as `{to} = <text>;`, and a statement against every form cu.krs
// gives as a statement of its own: the text is the pattern, and each parameter past a form's to takes a value the kernel
// holds, a number as cu.krs's bank of immediates writes one, a parameter of the kernel, or an expression read the same
// way. Of the readings that fit, a form is kept only where sass.krs's text for it assembles with the operands it is
// given, its to a register or, where the text writes a predicate there, a predicate; of those the reading of the fewest
// forms is taken. An expression read once is held in its register and read again from there.
//
// The frame is the stick's one arity: an operand (T)in[i] is the word at in + 8 . i, and out[thread] =
// (unsigned long long)r is the wide stored at out + 8 . thread, its conversion written out as C converts a signed word.
// Each of its forms is gated and ranked as an expression's is. A statement nothing here reads, and a value other than a
// 32-bit word, are each a question the kernel puts, and its line in <out>/engine.tsv says which.
//
// The folds are the system's own, held in sass.ksc beside sass.krs, each found on the compile channel by holding a
// kernel as written against nvcc's listing of it:
//
//   - register put for to: a form whose sass.krs text moves one word of the constant bank into its to writes nothing,
//     and the word stands as the operand of each form that reads it. Found where nvcc's listing reads the word as the
//     operand of an instruction other than a move.
//   - pow2 put for a parameter: a multiply by a power of two is written as the shift of the multiply's stem, by the
//     power's log2. Found where nvcc's listing holds the shift's operation with that log2.
//   - offset put for address: in[i + k] is loaded from the address of in[i], k elements on in the load's offset, and
//     operands that differ only in k share the address. Found where nvcc's listing loads at that offset.
//
// The stick is read in rounds. The first writes every form unfolded; each later one folds by what sass.ksc holds, which
// can show a fold the round before hid. After each round sass.ksc is written: what it held of each question this run
// asked dropped, and every fold this run found taken. The last round is written out: each kernel as <out>/NNNN.sass and
// its encodings <out>/NNNN.bin. Nothing goes to a device.
extern "C"
{
#include "../transpiler/vendor_bin_layouts/nvidia/sass_assemble.h"
#include "../transpiler/vendor_bin_layouts/nvidia/sass_machine.h"
#include "interface_sass_probe.h"
}

#include "cu_target.h"
#include "machine_ir_types.h"
#include "../types/file_defs/readers/ruleset_core_words.h"
#include "../types/file_defs/readers/ruleset_reader.h"
#include "sass_target.h"
#include "target.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

#if defined(__GNUC__) && !defined(__ELF__)
extern "C" const char __ehdr_start = 0;
#endif

// the most rounds the stick is read in
#define STICK_ROUNDS 4u

// the part the stick is assembled for, and the folder its machine file and its .kdm are in
#define STICK_MACHINES "src/cu/transpiler/lstar/coherence"
#define STICK_PART "sm_86"

// the width a value held in a predicate is given
#define STICK_PREDICATE_BITS 1u

// The part's registers: one holds STICK_REGISTER_BITS, and a value wider than one is held in an even register and
// the one past it, its low word in the even register. A value narrower than a register is held in the low bits of one
#define STICK_REGISTER_BITS 32u
#define STICK_PAIR_BITS 64u
// the width of a shift's count, `bits` in the rulesets: one register, a value held in a pair read as its low word. C
// leaves a count past the width undefined, and the low word holds every count under it
#define STICK_COUNT_BITS 0u

// the schema's forms by their place in it, as machine_ir_types.h names them
#define STICK_FORM_NAME(name_, text_, parameters_) text_,

static const char *const s_form_names[OPCODE_COUNT] = {OPCODES(STICK_FORM_NAME)};

// The types of C a kernel's values are read in, each its width and whether it is signed. A bool is held as a flag, in
// a predicate, or as a segment, the word 0 or 1
static const struct
{
    const char *type_name;
    unsigned int bits;
    int is_signed;
} s_types[] = {
    {"signed char", 8u, 1},
    {"unsigned char", 8u, 0},
    {"short", 16u, 1},
    {"unsigned short", 16u, 0},
    {"int", 32u, 1},
    {"unsigned int", 32u, 0},
    {"long long", 64u, 1},
    {"unsigned long long", 64u, 0},
    {"bool", STICK_PREDICATE_BITS, 0},
};

// the width of the registers a value of `bits` is held in: a predicate's, one register's, or a pair's
static unsigned int stick_held_bits(unsigned int bits)
{
    return (bits == STICK_PREDICATE_BITS) ? bits
                                          : ((bits <= STICK_REGISTER_BITS) ? STICK_REGISTER_BITS : STICK_PAIR_BITS);
}

// the place in s_types of the type `bare` names with its white space taken out, or -1
static int stick_type_bare(const std::string &bare)
{
    for (int type = 0; type < (int)(sizeof(s_types) / sizeof(s_types[0])); type += 1)
    {
        std::string type_name = s_types[type].type_name;
        type_name.erase(std::remove(type_name.begin(), type_name.end(), ' '), type_name.end());
        if (bare == type_name)
        {
            return type;
        }
    }
    return -1;
}

// the cast to type `type` with its white space taken out
static std::string stick_cast(int type)
{
    std::string type_name = s_types[type].type_name;
    type_name.erase(std::remove(type_name.begin(), type_name.end(), ' '), type_name.end());
    return "(" + type_name + ")";
}

// the type C promotes a value of type `type` to: int for a type narrower than int, else its own
static int stick_promoted(int type)
{
    return (s_types[type].bits < 32u) ? stick_type_bare("int") : type;
}

// the type C's usual arithmetic conversions give two promoted types: the wider of two of one signedness, else the
// unsigned one where it is at least as wide, else the signed one, which then holds every value of the other
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
static int stick_precedence(const std::string &token)
{
    static const char *const s_levels[][4] = {
        {"||"},       {"&&"},     {"|"},          {"^"}, {"&"}, {"==", "!="}, {"<", "<=", ">", ">="},
        {"<<", ">>"}, {"+", "-"}, {"*", "/", "%"}};
    for (int level = 0; level < (int)(sizeof(s_levels) / sizeof(s_levels[0])); level += 1)
    {
        for (const char *const one : s_levels[level])
        {
            if ((one != NULL) && (token == one))
            {
                return level + 1;
            }
        }
    }
    return 0;
}

// 1 where `token` compares two values
static int stick_compares(const std::string &token)
{
    return (token == "<") || (token == "<=") || (token == ">") || (token == ">=") || (token == "==") || (token == "!=");
}

// `operation`, an operator of C, as the reader holds it among the tokens: between backticks. No name or number reads
// as one
static std::string stick_operator(const std::string &operation)
{
    return "`" + operation + "`";
}

// the operator of C token `token` holds, or empty where it holds a name or a number
static std::string stick_operation(const std::string &token)
{
    return ((token.size() > 2u) && (token[0] == '`')) ? token.substr(1u, token.size() - 2u) : std::string();
}

// `expression` cut into C's tokens as it is written, its white space parting them: names, a member of one taken with
// it, numbers with their suffixes, and each operator, the longest that stands at its place, held as stick_operator()
// gives it. `a - -b` is cut as `a`, `-`, `-`, `b` and `a--b` as `a`, `--`, `b`, as C cuts them
static std::vector<std::string> stick_tokens(const std::string &expression)
{
    static const char *const s_pairs[] = {"<<", ">>", "<=", ">=", "==", "!=", "&&", "||", "++", "--"};
    std::vector<std::string> tokens;
    size_t at = 0u;
    while (at < expression.size())
    {
        const char letter = expression[at];
        if (isspace((unsigned char)letter) != 0)
        {
            at += 1u;
            continue;
        }
        size_t end = at + 1u;
        if ((isalnum((unsigned char)letter) != 0) || (letter == '_'))
        {
            while ((end < expression.size()) && ((isalnum((unsigned char)expression[end]) != 0) ||
                                                 (expression[end] == '_') || (expression[end] == '.')))
            {
                end += 1u;
            }
            tokens.push_back(expression.substr(at, end - at));
            at = end;
            continue;
        }
        for (const char *const pair : s_pairs)
        {
            end = (expression.compare(at, 2u, pair) == 0) ? (at + 2u) : end;
        }
        tokens.push_back(stick_operator(expression.substr(at, end - at)));
        at = end;
    }
    return tokens;
}

// A form cu.krs writes: its place, its text cut at its parameters with white space taken out, and the width its casts
// give the value. An expression's form is `{to} = <text>;`, its pieces the text after `=` cut at the parameters past
// to; a statement's is its whole text cut at every parameter. `order` gives each parameter the text names, in the
// text's order, its place among the parameters past to as the schema orders them, and `slot_bits`, in the schema's
// order, the width of the registers each is held in. `select` is 1 where the text is C's conditional,
// `({where}) ? chosen : otherwise`
struct StickPattern
{
    unsigned int form;
    std::vector<std::string> pieces;
    unsigned int bits;
    int statement;
    std::vector<unsigned int> order;
    std::vector<unsigned int> slot_bits;
    int select;
};

// The trace of the kernel being read, where STICK_TRACE is set: every reading tried, nested as it is tried, every
// form whose text matches and every choice of its operands with why it fails, what each reading gives, every line
// written and every question put. NULL where nothing is traced
static FILE *s_trace = NULL;
static unsigned int s_trace_depth = 0u;

// one line of the trace, indented by the depth of the reading it is of
static void stick_trace(const char *format, ...)
{
    if (s_trace == NULL)
    {
        return;
    }
    fprintf(s_trace, "%*s", (int)(2u * s_trace_depth), "");
    va_list arguments;
    va_start(arguments, format);
    vfprintf(s_trace, format, arguments);
    va_end(arguments);
    fputc('\n', s_trace);
}

// the depth of the trace one deeper for as long as it is held
struct StickTraceDeeper
{
    StickTraceDeeper()
    {
        s_trace_depth += 1u;
    }
    ~StickTraceDeeper()
    {
        s_trace_depth -= 1u;
    }
};

// what a reading is: a form over the readings of its parameters, a value the kernel holds, a number written in
// place, a number set into a register, or a parameter of the kernel
enum StickLeaf
{
    STICK_FORM = 0,
    STICK_NAME = 1,
    STICK_NUMBER = 2,
    STICK_NUMBER_SET = 3,
    STICK_PARAMETER = 4
};

// an expression as read: what it is, its form, its text, its number, its count of forms and its width; the readings
// of its parameters; where it is folded, the word it stands as; and whether a flag is read as its complement
struct StickReading
{
    StickLeaf leaf;
    unsigned int form;
    std::string text;
    unsigned int number;
    unsigned int cost;
    unsigned int bits;
    std::vector<StickReading> children;
    std::string place;
    int negated = 0;
};

// a register the kernel holds a value in, as sass.krs writes it, or the word a folded value stands as; its width,
// whether its type is signed, and its type's place in s_types
struct StickValue
{
    std::string name;
    unsigned int bits;
    int is_signed;
    int type = -1;
};

// a parameter of the kernel: its offset in the parameter bank, its width and its type's place in s_types
struct StickParameter
{
    unsigned int offset;
    unsigned int bits;
    int type = -1;
};

// An expression written out with every conversion C makes in it, the place in s_types of the type it gives, and 1
// where its value is never negative, a number or a flag's select of 1 and 0, so that widening it signed or unsigned
// gives one value. Where `low_bits` is not 0, `low` is a text whose low `low_bits` bits are the value's own and which
// leaves out conversions those bits do not need: a narrowing reads no more of its operand than that. `zero_one` is 1
// where the value is a word that holds 0 or 1 alone. A flag tested off such a word keeps it in `word`, of type
// `word_type`: the flag read back as a word is that word, [select(p, 1, 0) != 0] = p
struct StickTyped
{
    std::string text;
    int type;
    int nonnegative = 0;
    std::string low;
    unsigned int low_bits = 0u;
    int zero_one = 0;
    std::string word;
    int word_type = -1;
};

// what a round asks of nvcc's listing about one kernel: each form that moved a word of the constant bank, and the
// word; each multiply by a power of two, its parameter and the log2; each load's form and offset into the elements;
// and each test a select read over, with the test of its complement
struct StickAsked
{
    std::vector<std::pair<unsigned int, std::string>> moved;
    std::vector<std::pair<std::pair<unsigned int, unsigned int>, unsigned int>> multiplied;
    std::vector<std::pair<unsigned int, unsigned int>> offsets;
    std::vector<unsigned int> copied;
    std::vector<unsigned int> copied_once;
    std::vector<std::pair<unsigned int, unsigned int>> complemented;
};

// `text` with every space, tab and line's end taken out
static std::string stick_bare(const std::string &text)
{
    std::string bare;
    for (const char letter : text)
    {
        if ((letter != ' ') && (letter != '\t') && (letter != '\n') && (letter != '\r'))
        {
            bare += letter;
        }
    }
    return bare;
}

// 1 where `text` is an expression other than `name` itself that reads `name`: the name standing in it with no letter,
// digit, `_` or `.` either side of it
static int stick_reads_name(const std::string &text, const std::string &name)
{
    if (text == name)
    {
        return 0;
    }
    const auto joins = [](char letter)
    { return (isalnum((unsigned char)letter) != 0) || (letter == '_') || (letter == '.'); };
    for (size_t at = text.find(name); at != std::string::npos; at = text.find(name, at + 1u))
    {
        const size_t end = at + name.size();
        if (((at == 0u) || !joins(text[at - 1u])) && ((end == text.size()) || !joins(text[end])))
        {
            return 1;
        }
    }
    return 0;
}

// `text` with every run of white space cut to one space and none at either end
static std::string stick_squeeze(const std::string &text)
{
    std::string squeezed;
    for (const char letter : text)
    {
        const int space = (letter == ' ') || (letter == '\t') || (letter == '\n') || (letter == '\r');
        if (space == 0)
        {
            squeezed += letter;
        }
        else if (!squeezed.empty() && (squeezed.back() != ' '))
        {
            squeezed += ' ';
        }
    }
    while (!squeezed.empty() && (squeezed.back() == ' '))
    {
        squeezed.pop_back();
    }
    return squeezed;
}

// 1 where `text` is not empty and its parentheses and brackets close in order
static int stick_balanced(const std::string &text)
{
    int depth = 0;
    for (const char letter : text)
    {
        depth += ((letter == '(') || (letter == '[')) ? 1 : 0;
        depth -= ((letter == ')') || (letter == ']')) ? 1 : 0;
        if (depth < 0)
        {
            return 0;
        }
    }
    return !text.empty() && (depth == 0);
}

// `bare`, two flags C joins by one && or one || outside every parenthesis, written with the two exchanged; empty where
// `bare` joins no two flags so
static std::string stick_flags_exchanged(const std::string &bare)
{
    int depth = 0;
    size_t joined = std::string::npos;
    for (size_t at = 0u; (at + 1u) < bare.size(); at += 1u)
    {
        depth += ((bare[at] == '(') || (bare[at] == '[')) ? 1 : 0;
        depth -= ((bare[at] == ')') || (bare[at] == ']')) ? 1 : 0;
        const int join = (depth == 0) && (((bare[at] == '&') && (bare[at + 1u] == '&')) ||
                                          ((bare[at] == '|') && (bare[at + 1u] == '|')));
        if (join != 0)
        {
            if (joined != std::string::npos)
            {
                return std::string();
            }
            joined = at;
            at += 1u;
        }
    }
    if (joined == std::string::npos)
    {
        return std::string();
    }
    return bare.substr(joined + 2u) + bare.substr(joined, 2u) + bare.substr(0u, joined);
}

// `bare`, a comparison of C, written as its complement: its outermost comparison's operator exchanged for the one C
// gives the opposite answer, == for !=, < for >=, > for <=. Empty where `bare` is no comparison
static std::string stick_complement(const std::string &bare)
{
    std::string inner = bare;
    while ((inner.size() > 2u) && (inner.front() == '(') && (inner.back() == ')') &&
           stick_balanced(inner.substr(1u, inner.size() - 2u)))
    {
        inner = inner.substr(1u, inner.size() - 2u);
    }
    static const char *const s_pairs[][2] = {{"==", "!="}, {"!=", "=="}, {"<=", ">"}, {">=", "<"}, {"<", ">="},
                                             {">", "<="}};
    int depth = 0;
    for (size_t at = 0u; at < inner.size(); at += 1u)
    {
        depth += ((inner[at] == '(') || (inner[at] == '[')) ? 1 : 0;
        depth -= ((inner[at] == ')') || (inner[at] == ']')) ? 1 : 0;
        const int shift = ((inner[at] == '<') || (inner[at] == '>')) &&
                          ((((at + 1u) < inner.size()) && (inner[at + 1u] == inner[at])) ||
                           ((at > 0u) && (inner[at - 1u] == inner[at])));
        if ((depth != 0) || (shift != 0))
        {
            continue;
        }
        for (const auto &pair : s_pairs)
        {
            const size_t size = strlen(pair[0]);
            if (inner.compare(at, size, pair[0]) == 0)
            {
                return "(" + inner.substr(0u, at) + pair[1] + inner.substr(at + size) + ")";
            }
        }
    }
    return std::string();
}

// The pipe an operation issues to on the part, as NVIDIA's profiler names them: 1 the integer pipe, 2 the FMA pipe,
// 0 any other, a load, a store, a branch, the double pipe or the transcendental unit
static int stick_pipe_of(const std::string &operation)
{
    static const char *const s_fma[] = {"IMAD", "IMUL", "FFMA", "FMUL", "FADD", "HFMA2", "HADD2", "HMUL2", "I2FP",
                                        "F2FP"};
    static const char *const s_integer[] = {"IADD3", "LOP3", "SHF", "SEL", "MOV", "ISETP", "FSETP", "PRMT",
                                            "IMNMX", "LEA", "FSEL", "PLOP3", "IABS", "FMNMX", "P2R", "R2P",
                                            "CS2R", "SGXT", "BMSK", "IADD"};
    for (const char *const one : s_fma)
    {
        if (operation.compare(0u, strlen(one), one) == 0)
        {
            return 2;
        }
    }
    for (const char *const one : s_integer)
    {
        if (operation.compare(0u, strlen(one), one) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// Where `instruction`, squeezed, is one the system writes on either pipe, the operation it is written with on the
// integer pipe, and its writing on the FMA pipe in `writing`: a move of a word of the constant bank, IMAD.MOV.U32 over
// two zero registers; a left shift by a number, IMAD.SHL.U32 by the power of two; or an add of two words and a carry,
// IMAD.X of the first by 1 added to the second. Empty where it is none of these
static std::string stick_pipe_choice(const std::string &instruction, std::string *writing)
{
    std::smatch found;
    const std::string text = (!instruction.empty() && (instruction.back() == ';'))
                                 ? stick_squeeze(instruction.substr(0u, instruction.size() - 1u))
                                 : instruction;
    // each pattern compiled once, the first time a line is put to it
    static const std::string word = "(R[0-9]+(?:\\.hi)?)";
    static const std::regex carried_add("^IADD3\\.X " + word + ", " + word + ", " + word + ", RZ, (P[0-6]), !PT$");
    static const std::regex carried_product("^IMAD\\.X " + word + ", " + word + ", 0x1, " + word + ", (P[0-6])$");
    static const std::regex moved("^MOV (R[0-9]+), (c\\[.+\\])$");
    static const std::regex moved_product("^IMAD\\.MOV\\.U32 (R[0-9]+), RZ, RZ, (c\\[.+\\])$");
    static const std::regex shifted("^SHF\\.L\\.U32 (R[0-9]+), (R[0-9]+), (0x[0-9a-f]+|[0-9]+), RZ$");
    static const std::regex shifted_product("^IMAD\\.SHL\\.U32 (R[0-9]+), (R[0-9]+), (0x[0-9a-f]+|[0-9]+), RZ$");
    if (std::regex_match(text, found, carried_add))
    {
        *writing = "IMAD.X " + std::string(found[1]) + ", " + std::string(found[2]) + ", 0x1, " +
                   std::string(found[3]) + ", " + std::string(found[4]);
        return "IADD3.X";
    }
    if (std::regex_match(text, found, carried_product))
    {
        *writing = text;
        return "IADD3.X";
    }
    if (std::regex_match(text, found, moved) || std::regex_match(text, found, moved_product))
    {
        *writing = "IMAD.MOV.U32 " + std::string(found[1]) + ", RZ, RZ, " + std::string(found[2]);
        return "MOV";
    }
    if (std::regex_match(text, found, shifted))
    {
        const unsigned long bits = strtoul(std::string(found[3]).c_str(), NULL, 0);
        *writing = "IMAD.SHL.U32 " + std::string(found[1]) + ", " + std::string(found[2]) + ", " +
                   std::to_string(1ul << bits) + ", RZ";
        return "SHF.L.U32";
    }
    if (std::regex_match(text, found, shifted_product))
    {
        *writing = text;
        return "SHF.L.U32";
    }
    return std::string();
}

// the operations of `instructions`, each squeezed, on the integer pipe and on the FMA pipe, those the system writes on
// either left out
static std::pair<unsigned int, unsigned int> stick_pipe_lane(const std::vector<std::string> &instructions)
{
    std::pair<unsigned int, unsigned int> lane(0u, 0u);
    for (const std::string &instruction : instructions)
    {
        std::string writing;
        if (instruction.empty() || (instruction.back() == ':') || !stick_pipe_choice(instruction, &writing).empty())
        {
            continue;
        }
        const int pipe = stick_pipe_of(instruction.substr(0u, instruction.find(' ')));
        lane.first += (pipe == 1) ? 1u : 0u;
        lane.second += (pipe == 2) ? 1u : 0u;
    }
    return lane;
}

// `squeezed`, a line of sass.krs's text written with the stand-ins \x01 to \x09 for its parameters, as a pattern that
// matches the line written with any operands, each parameter's operand a group of its own
static std::regex stick_line_pattern(const std::string &squeezed)
{
    std::string pattern = "^";
    for (const char letter : squeezed)
    {
        if ((letter >= '\x01') && (letter <= '\x09'))
        {
            pattern += "([^,\\[\\]\\s;]+)";
        }
        else if (strchr(".^$|()[]{}*+?\\", letter) != NULL)
        {
            pattern += std::string("\\") + letter;
        }
        else
        {
            pattern += letter;
        }
    }
    return std::regex(pattern + "$");
}

// the log2 of `number` where it is a power of two of 2 or more, else 0
static unsigned int stick_log2(unsigned int number)
{
    if ((number < 2u) || ((number & (number - 1u)) != 0u))
    {
        return 0u;
    }
    unsigned int log2 = 0u;
    while ((number >> log2) != 1u)
    {
        log2 += 1u;
    }
    return log2;
}

// `number` written in hexadecimal as nvcc's listing writes an immediate
static std::string stick_hex(unsigned int number)
{
    char written[16];
    snprintf(written, sizeof(written), "0x%x", number);
    return written;
}

// a word of the constant bank as nvcc's listing writes it, `c[bank][offset]` with each sum taken: empty where `text`
// is no such word
static std::string stick_constant(const std::string &text)
{
    static const std::regex word("^c\\[([0-9a-fA-Fx]+)\\]\\[([0-9a-fA-Fx+]+)\\]$");
    std::smatch found;
    const std::string bare = stick_bare(text);
    if (!std::regex_match(bare, found, word))
    {
        return std::string();
    }
    unsigned long long offset = 0ull;
    const std::string terms = found[2];
    size_t at = 0u;
    while (at < terms.size())
    {
        const size_t plus = terms.find('+', at);
        const std::string term = terms.substr(at, (plus == std::string::npos) ? std::string::npos : (plus - at));
        offset += strtoull(term.c_str(), NULL, 0);
        at = (plus == std::string::npos) ? terms.size() : (plus + 1u);
    }
    char written[64];
    snprintf(written, sizeof(written), "c[0x%llx][0x%llx]", strtoull(std::string(found[1]).c_str(), NULL, 0),
             offset);
    return written;
}

// a register a form's scratch is written with where its text is only looked at or assembled, and never run: one no
// stand-in names
static std::string stick_scratch_stand_in(const std::string &bank)
{
    return (bank == "wide") ? std::string("R200") : ((bank == "predicate") ? std::string("P5") : std::string("R202"));
}

// The width of the registers sass.krs's text for `form` takes each parameter in, by the parameter's place: a pair
// where the text writes the parameter's high word or reads it as a 64-bit address, STICK_COUNT_BITS for a shift's
// count, else one register
static std::vector<unsigned int> stick_parameter_bits(const Ruleset *sass, unsigned int form)
{
    const std::vector<std::string> names = ruleset_parameters(sass, s_form_names[form]);
    const unsigned int count = sass->schema->forms[form].parameters;
    std::vector<std::string> markers;
    for (unsigned int parameter = 0u; parameter < count; parameter += 1u)
    {
        markers.push_back(std::string("\x01") + (char)('A' + parameter) + "\x01");
    }
    std::vector<unsigned int> bits(count, STICK_REGISTER_BITS);
    std::string text;
    if (ruleset_opcode(sass, s_form_names[form], markers, stick_scratch_stand_in, text) == 0)
    {
        return bits;
    }
    for (unsigned int parameter = 0u; parameter < count; parameter += 1u)
    {
        const int pair = (text.find(markers[parameter] + ".hi") != std::string::npos) ||
                         (text.find(markers[parameter] + ".64") != std::string::npos);
        const int count = (parameter < names.size()) && (names[parameter] == "bits");
        bits[parameter] = (pair != 0) ? STICK_PAIR_BITS : ((count != 0) ? STICK_COUNT_BITS : STICK_REGISTER_BITS);
    }
    return bits;
}

// the width of the registers of the C type, written bare, of the cast that ends `piece`; 0 where `piece` ends
// in no cast to a register's own type, unsigned int or unsigned long long
static unsigned int stick_own_cast(const std::string &piece)
{
    static const char *const s_own[] = {"(unsignedint)", "(unsignedlonglong)"};
    for (unsigned int own = 0u; own < 2u; own += 1u)
    {
        const size_t size = strlen(s_own[own]);
        if ((piece.size() >= size) && (piece.compare(piece.size() - size, size, s_own[own]) == 0))
        {
            return (own == 0u) ? STICK_REGISTER_BITS : STICK_PAIR_BITS;
        }
    }
    return 0u;
}

// Every form cu.krs gives as an expression or as a statement of its own, each with a letter of its own text. A cast
// to a register's own type just before a parameter converts nothing where the parameter is held in a register of that
// width, and the form is also read with the cast left out, the parameter then held at the cast's width
static std::vector<StickPattern> stick_patterns(const Ruleset *rules, const Ruleset *sass)
{
    std::vector<StickPattern> patterns;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        const InstrTemplate &text = rules->forms[form];
        // a sign, a byte of -1, 0 or 1, is no type of C, and a form over one reads no text of C: its text narrows
        // to a signed char only what is a sign already
        const std::string name = s_form_names[form];
        const int sign = (name.compare(0u, 5u, "sign_") == 0) || (name.compare(0u, 10u, "test_sign_") == 0);
        if ((rules->form_given[form] != (unsigned char)RULESET_CORE_GIVEN) || text.slots.empty() || (sign != 0))
        {
            continue;
        }
        std::vector<std::string> pieces;
        for (const std::string &piece : text.pieces)
        {
            pieces.push_back(stick_bare(piece));
        }
        const int expression = (text.slots[0] == 0u) && pieces[0].empty() && (pieces[1].compare(0u, 1u, "=") == 0) &&
                               !pieces.back().empty() && (pieces.back().back() == ';');
        if (expression != 0)
        {
            pieces.erase(pieces.begin());
            pieces.front().erase(0u, 1u);
            pieces.back().pop_back();
            // parentheses around the whole expression are not part of it, as the reader takes them off an expression
            std::string whole;
            for (size_t piece = 0u; piece < pieces.size(); piece += 1u)
            {
                whole += pieces[piece] + ((piece + 1u) < pieces.size() ? "x" : "");
            }
            if ((whole.size() > 2u) && (whole.front() == '(') && (whole.back() == ')') &&
                stick_balanced(whole.substr(1u, whole.size() - 2u)) && (pieces.front().size() > 0u) &&
                (pieces.back().size() > 0u))
            {
                pieces.front().erase(0u, 1u);
                pieces.back().pop_back();
            }
        }
        unsigned int bits = 0u;
        for (const std::string &piece : pieces)
        {
            bits = ((piece.find("(unsignedlonglong)") != std::string::npos) ||
                    (piece.find("(longlong)") != std::string::npos))
                       ? 64u
                       : bits;
        }
        const std::vector<unsigned int> parameter_bits = stick_parameter_bits(sass, form);
        const size_t first = (expression != 0) ? 1u : 0u;
        const size_t slots = text.slots.size() - first;
        const int select = (expression != 0) && (slots == 3u) && (pieces[0] == "(") &&
                           (pieces[1].compare(0u, 2u, ")?") == 0) && (pieces[2].compare(0u, 1u, ":") == 0);
        std::vector<unsigned int> order;
        std::vector<size_t> casts;
        // a parameter the text leaves out still has its place: the widths are held by the parameter's place
        size_t places = slots;
        for (size_t slot = 0u; slot < slots; slot += 1u)
        {
            order.push_back(text.slots[slot + first] - (unsigned int)first);
            places = std::max(places, (size_t)order.back() + 1u);
            if (stick_own_cast(pieces[slot]) != 0u)
            {
                casts.push_back(slot);
            }
        }
        for (size_t variant = 0u; variant < ((size_t)1u << casts.size()); variant += 1u)
        {
            std::vector<std::string> cut = pieces;
            std::vector<unsigned int> slot_bits(places, STICK_REGISTER_BITS);
            for (size_t slot = 0u; slot < slots; slot += 1u)
            {
                slot_bits[order[slot]] = parameter_bits[text.slots[slot + first]];
            }
            for (size_t cast = 0u; cast < casts.size(); cast += 1u)
            {
                if (((variant >> cast) & 1u) != 0u)
                {
                    std::string &piece = cut[casts[cast]];
                    const unsigned int held = stick_own_cast(piece);
                    unsigned int &bits_held = slot_bits[order[casts[cast]]];
                    piece.erase(piece.rfind('('));
                    bits_held = (bits_held == STICK_PAIR_BITS) ? STICK_PAIR_BITS : held;
                }
            }
            // a text of nothing but parameters reads nothing of its own, and would read an expression as itself; an
            // expression's text is one statement
            int lettered = 0;
            for (const std::string &piece : cut)
            {
                lettered = piece.empty() ? lettered : 1;
                lettered = ((expression != 0) &&
                            ((piece.find(';') != std::string::npos) || (piece.find('{') != std::string::npos)))
                               ? -1
                               : lettered;
                if (lettered < 0)
                {
                    break;
                }
            }
            if (lettered > 0)
            {
                patterns.push_back({form, cut, bits, (expression != 0) ? 0 : 1, order, slot_bits, select});
            }
        }
    }
    return patterns;
}

// every way `pieces` from `piece` on matches `text` from `at` on, each parameter taking a balanced span, into `found`
static void stick_match(const std::vector<std::string> &pieces, size_t piece, const std::string &text, size_t at,
                        std::vector<std::string> &bound, std::vector<std::vector<std::string>> &found)
{
    if (text.compare(at, pieces[piece].size(), pieces[piece]) != 0)
    {
        return;
    }
    at += pieces[piece].size();
    if (piece == (pieces.size() - 1u))
    {
        if (at == text.size())
        {
            found.push_back(bound);
        }
        return;
    }
    for (size_t end = at + 1u; end <= text.size(); end += 1u)
    {
        const std::string span = text.substr(at, end - at);
        if (stick_balanced(span))
        {
            bound.push_back(span);
            stick_match(pieces, piece + 1u, text, end, bound, found);
            bound.pop_back();
        }
    }
}

// the place of form `name` in the schema, or OPCODE_COUNT
static unsigned int stick_form(const std::string &name)
{
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        if (name == s_form_names[form])
        {
            return form;
        }
    }
    return OPCODE_COUNT;
}

// the shift of a multiply's stem: its name with mul written shl, or OPCODE_COUNT where the schema has none
static unsigned int stick_shift_of(unsigned int form)
{
    std::string name = s_form_names[form];
    const size_t at = name.find("mul");
    return (at == std::string::npos) ? OPCODE_COUNT : stick_form(name.replace(at, 3u, "shl"));
}

// the operation sass.krs's text for `form` opens with, its operands stand-ins
static std::string stick_operation(const Ruleset *sass, unsigned int form)
{
    std::vector<std::string> arguments(sass->schema->forms[form].parameters, std::string("R2"));
    std::string text;
    if (ruleset_opcode(sass, s_form_names[form], arguments, stick_scratch_stand_in, text) == 0)
    {
        return std::string();
    }
    const std::string squeezed = stick_squeeze(text);
    return squeezed.substr(0u, squeezed.find(' '));
}

// the line sass.ksc gives a fold of `kind` put for parameter `parameter` of form `form`, and the line's opening
static std::string stick_fold_line(const Ruleset *sass, unsigned int form, const char *kind, unsigned int parameter,
                                   int opening)
{
    const std::vector<std::string> names = ruleset_parameters(sass, s_form_names[form]);
    const std::string line = std::string(s_form_names[form]) + ": the system folds the " + kind + " put for ";
    return (opening != 0) ? line : (line + ((parameter < names.size()) ? names[parameter] : std::string()));
}

// every writing put to a machine's assembler, and 1 where it assembled: a writing is put once, its answer read here
// every time after
static std::map<std::pair<const SassMachine *, std::string>, int> s_stick_assembled;

// a kernel read and written: the rulesets, the machine file a form is gated on, whether it folds, the values and
// parameters it holds, the text written so far, and what it asks of nvcc's listing
class StickKernel
{
  public:
    StickKernel(const Ruleset *cu, const Ruleset *sass, const SassMachine *machine,
                const std::vector<StickPattern> *patterns, int folding)
        : forms(0u), cu(cu), sass(sass), machine(machine), patterns(patterns), folding(folding), next(0u),
          predicates(0u)
    {
    }

    std::map<std::string, StickParameter> parameters;
    std::string text;
    std::string question;
    unsigned int forms;
    StickAsked asked;
    // the count of registers the kernel names, its high water mark once assign() has given them
    unsigned int registers_held = 0u;
    // each register the kernel holds, one a line: the register, what holds it, a name of the kernel's or a fixed
    // register by its name, and the lines of the text that claim it and release it, written by assign()
    std::string held_lines;

    // `expression` read and written into a register, its value; empty where it is not read, `question` then saying why
    StickValue value(const std::string &expression)
    {
        const std::string bare = stick_bare(expression);
        const auto held = held_values.find(bare);
        if (held != held_values.end())
        {
            return held->second;
        }
        StickReading reading;
        if (read(bare, 0, &reading) == 0)
        {
            ask("no reading of " + expression + " through cu.krs");
            return StickValue{};
        }
        return written(reading);
    }

    // Form `name` over the expressions `spans`, its first `outputs` its own registers: each span read with a number
    // or a folded word in place and in a register, and of the choices sass.krs's text assembles, the one of fewest
    // forms written. 0 where none assembles, `question` then saying why
    int form(const std::string &name, const std::vector<std::string> &outputs, const std::vector<std::string> &spans)
    {
        const unsigned int place = stick_form(name);
        StickReading best;
        best.cost = 0xFFFFFFFFu;
        if ((place == OPCODE_COUNT) || (choose(place, outputs, spans, 32u, 0, {}, -1, &best) == 0))
        {
            ask("sass.krs's " + name + " assembles with no reading of its operands");
            return 0;
        }
        std::vector<std::string> arguments = outputs;
        for (const StickReading &child : best.children)
        {
            arguments.push_back(operand(child));
        }
        return write(name, arguments);
    }

    // 1 where `bare`, a statement, is read through a statement form of cu.krs and written
    int statement(const std::string &bare)
    {
        for (const StickPattern &pattern : *patterns)
        {
            std::vector<std::string> bound;
            std::vector<std::vector<std::string>> found;
            if (pattern.statement != 0)
            {
                stick_match(pattern.pieces, 0u, bare, 0u, bound, found);
            }
            if (!found.empty())
            {
                return form(s_form_names[pattern.form], std::vector<std::string>(), found[0]);
            }
        }
        return 0;
    }

    // `name` held in `value` as a value of type `type`, or as a word of no type of C where `type` is -1, never
    // negative where `nonnegative` is set. A name held anew is read from `value` alone, whatever stood for it before,
    // and every expression held that reads the name is let go: its value is of what the name held when it was read
    void hold(const std::string &name, StickValue value, int type, int nonnegative = 0)
    {
        value.is_signed = (type >= 0) ? s_types[type].is_signed : 0;
        value.type = type;
        for (auto held = held_values.begin(); held != held_values.end();)
        {
            held = stick_reads_name(held->first, name) ? held_values.erase(held) : std::next(held);
        }
        held_values[name] = value;
        readings.clear();
        standing.erase(name);
        flags.erase(name);
        nonnegatives.erase(name);
        if (nonnegative != 0)
        {
            nonnegatives.insert(name);
        }
    }

    // `name`, a bool, held as the flag `flag` reads as, read where the name is read, with the word of 0 and 1 it is
    // tested off where it is one
    void hold_flag(const std::string &name, const StickTyped &flag)
    {
        StickTyped held{"(" + flag.text + ")", flag.type};
        held.word = flag.word;
        held.word_type = flag.word_type;
        flags[name] = held;
    }

    // `name` read as `value`, written where the name is read and only as much of it as is read there
    void stand(const std::string &name, const StickTyped &value)
    {
        standing[name] = value;
    }

    // `expression` typed, and written out with every conversion C makes in it, for the reader to read through
    // cu.krs; type -1 where it is not typed, `question` then saying why
    StickTyped typed(const std::string &expression)
    {
        tokens = stick_tokens(expression);
        token_at = 0u;
        const StickTyped whole = conditional();
        if ((whole.type >= 0) && (token_at != tokens.size()))
        {
            return refuse("a token nothing here types: " + tokens[token_at]);
        }
        return whole;
    }

    // `value` converted to type `to` as C converts it. A bool is a flag, and its segment the select of 1 and 0 over
    // it; a value converted to a bool is the flag of its differing from 0. A value of a register's width converted to
    // another of that width is the same register; a word converted to a pair is widened as its sign says; a pair
    // converted to a word is its low word
    StickTyped converted(const StickTyped &value, int to)
    {
        if ((value.type < 0) || (value.type == to))
        {
            return value;
        }
        const int flag = stick_type_bare("bool");
        if (to == flag)
        {
            StickTyped tested = combined(value, "!=", StickTyped{number_text(0u), stick_type_bare("int"), 1});
            tested.word = (value.zero_one != 0) ? value.text : std::string();
            tested.word_type = (value.zero_one != 0) ? value.type : -1;
            return tested;
        }
        if ((value.type == flag) && !value.word.empty())
        {
            StickTyped word{value.word, value.word_type, 1};
            word.zero_one = 1;
            return converted(word, to);
        }
        if (value.type == flag)
        {
            StickTyped selected{"((" + value.text + ")?" + number_text(1u) + ":" + number_text(0u) + ")",
                                stick_type_bare("unsignedint"), 1};
            selected.zero_one = 1;
            return converted(selected, to);
        }
        const unsigned int from_bits = s_types[value.type].bits;
        const unsigned int to_bits = s_types[to].bits;
        // A byte or a halfword is held in a word extended as its sign says, the narrowing of the word's low bits
        if (to_bits < STICK_REGISTER_BITS)
        {
            const int whole = (value.low_bits < to_bits);
            std::string low = (whole != 0) ? value.text : value.low;
            low = ((whole != 0) && (from_bits > STICK_REGISTER_BITS)) ? ("(unsignedint)" + low) : low;
            StickTyped narrowed{stick_cast(to) + low, to, (s_types[to].is_signed == 0) ? 1 : 0};
            narrowed.low = low;
            narrowed.low_bits = to_bits;
            narrowed.zero_one = value.zero_one;
            return narrowed;
        }
        if (stick_held_bits(from_bits) == stick_held_bits(to_bits))
        {
            StickTyped same{value.text, to, value.nonnegative};
            same.low = value.low;
            same.low_bits = value.low_bits;
            same.zero_one = value.zero_one;
            return same;
        }
        if (to_bits > from_bits)
        {
            const int sign = (s_types[value.type].is_signed != 0) && (value.nonnegative == 0);
            StickTyped widened{((sign != 0) ? "(unsignedlonglong)(longlong)(int)" : "(unsignedlonglong)") + value.text,
                               to, value.nonnegative};
            widened.zero_one = value.zero_one;
            return widened;
        }
        StickTyped cut{stick_cast(to) + value.text, to};
        cut.zero_one = value.zero_one;
        return cut;
    }

    // 1 where `name` is held
    int holds(const std::string &name) const
    {
        return held_values.find(name) != held_values.end();
    }

    // A fresh register of `bits`: a predicate's one of the predicates, and a word's or a wide's one the kernel names
    // until assign() gives it a register of the file
    StickValue fresh(unsigned int bits)
    {
        if (bits == STICK_PREDICATE_BITS)
        {
            predicates += 1u;
            return StickValue{ruleset_register(sass, "predicate", predicates - 1u), bits, 0};
        }
        next += 1u;
        return StickValue{std::string((bits == 64u) ? "%w" : "%r") + std::to_string(next - 1u), bits, 0};
    }

    // Each register the kernel names given one of the file for the lines it lives over, from the first that names it
    // to the last, and given again once it is let go: a word one register, and a wide an even pair. A word moved into
    // another and used nowhere else, its own writing and the move, is the word_used_once of the move's form, and where
    // the system folds it the word moved into is the moved word itself. A wide made of a word whose move the system
    // folds is the pair that word begins, the word given the even register. A move of a register into itself is then
    // left out as the nothing it does
    void assign(void)
    {
        for (const auto &copy : pending_copies)
        {
            const std::string &into = copy.second.first;
            const std::string &moved = copy.second.second;
            const std::regex moved_named(moved + "(?![0-9])");
            const std::ptrdiff_t uses =
                std::distance(std::sregex_iterator(text.begin(), text.end(), moved_named), std::sregex_iterator());
            if (uses != 2)
            {
                continue;
            }
            asked.copied_once.push_back(copy.first);
            if (folded(copy.first, "word_used_once", 1u) == 0)
            {
                continue;
            }
            text = std::regex_replace(text, std::regex(into + "(?![0-9])"), moved);
            for (auto &tie : ties)
            {
                tie.second = (tie.second == into) ? moved : tie.second;
            }
        }
        std::map<std::string, std::string> pair_of;
        for (const auto &tie : ties)
        {
            pair_of[tie.second] = tie.first;
        }
        // each name's first and last line, a name tied into another's pair counted as that other's
        static const std::regex named("%[rw][0-9]+");
        const auto root_of = [&](const std::string &name)
        {
            const auto tie = ties.find(name);
            return (tie != ties.end()) ? tie->second : name;
        };
        std::vector<std::vector<std::string>> line_names;
        std::map<std::string, size_t> last_line;
        for (size_t line_at = 0u; line_at < text.size();)
        {
            const size_t line_end = std::min(text.find('\n', line_at), text.size());
            const std::string line = text.substr(line_at, line_end - line_at);
            line_names.emplace_back();
            for (std::sregex_iterator found(line.begin(), line.end(), named), end; found != end; ++found)
            {
                const std::string root = root_of(found->str());
                line_names.back().push_back(root);
                last_line[root] = line_names.size() - 1u;
            }
            line_at = line_end + 1u;
        }
        // Each name given a register at the line that first names it and the register let go after the line that
        // last names it, never on a line that names it: a word one register, the lowest free, and a wide an even pair,
        // the lowest both of which are free. The high water mark, from the first past the ones kernel_open writes,
        // rises only where no register under it is free
        std::map<std::string, unsigned int> given;
        std::map<std::string, size_t> first_line;
        // each name's last line as the text names it, before the walk marks a name let go by moving it past every line
        const std::map<std::string, size_t> named_last = last_line;
        std::set<unsigned int> free;
        unsigned int high_water = 2u;
        for (size_t line = 0u; line < line_names.size(); line += 1u)
        {
            for (const std::string &root : line_names[line])
            {
                if (given.find(root) != given.end())
                {
                    continue;
                }
                const int even = (root[1] == 'w') || (pair_of.find(root) != pair_of.end());
                unsigned int number = high_water;
                const auto pair_free = std::find_if(free.begin(), free.end(), [&](unsigned int one)
                                                    { return ((one % 2u) == 0u) && (free.count(one + 1u) != 0u); });
                if ((even == 0) && !free.empty())
                {
                    number = *free.begin();
                    free.erase(free.begin());
                }
                else if ((even != 0) && (pair_free != free.end()))
                {
                    number = *pair_free;
                    free.erase(number);
                    free.erase(number + 1u);
                }
                else
                {
                    if ((even != 0) && ((high_water % 2u) != 0u))
                    {
                        free.insert(high_water);
                        number = high_water + 1u;
                    }
                    high_water = number + ((even != 0) ? 2u : 1u);
                }
                given[root] = number;
                first_line[root] = line;
            }
            for (const std::string &root : line_names[line])
            {
                if ((last_line[root] == line) && (given.find(root) != given.end()))
                {
                    const int even = (root[1] == 'w') || (pair_of.find(root) != pair_of.end());
                    free.insert(given[root]);
                    if (even != 0)
                    {
                        free.insert(given[root] + 1u);
                    }
                    last_line[root] = line_names.size();
                }
            }
        }
        for (const auto &tie : ties)
        {
            given[tie.first] = given[tie.second];
        }
        // A register a form names by its own number past the high water mark is the form's own, and is moved to just
        // past the mark, the second register of a pair and the pair it begins kept even, the mark rising over it: the
        // kernel's registers lie next to each other, and it declares the mark. A number the text names before the
        // kernel's names are given is a form's, since every register of the kernel's own is named by %r or %w
        static const std::regex pinned("\\bR([0-9]+)(\\.hi)?\\b");
        std::map<unsigned int, int> pinned_pairs;
        for (std::sregex_iterator found(text.begin(), text.end(), pinned), end; found != end; ++found)
        {
            const unsigned int number = (unsigned int)std::stoul((*found)[1].str());
            if (number >= high_water)
            {
                pinned_pairs[number] = pinned_pairs[number] || (*found)[2].matched;
            }
        }
        std::map<unsigned int, unsigned int> moved_to;
        for (const auto &pin : pinned_pairs)
        {
            high_water += ((pin.second != 0) && ((high_water % 2u) != 0u)) ? 1u : 0u;
            moved_to[pin.first] = high_water;
            high_water += (pin.second != 0) ? 2u : 1u;
        }
        std::string moved_text;
        size_t moved_last = 0u;
        for (std::sregex_iterator found(text.begin(), text.end(), pinned), end; found != end; ++found)
        {
            const unsigned int number = (unsigned int)std::stoul((*found)[1].str());
            moved_text += text.substr(moved_last, (size_t)found->position() - moved_last);
            moved_text += (moved_to.count(number) != 0u) ? ("R" + std::to_string(moved_to[number]) + (*found)[2].str())
                                                         : found->str();
            moved_last = (size_t)found->position() + found->str().size();
        }
        text = moved_text + text.substr(moved_last);
        registers_held = high_water;
        // the mark may not pass the last register the part answers a kernel's code can name
        if ((machine->register_last != SASS_MACHINE_UNANSWERED) && (high_water > (machine->register_last + 1u)))
        {
            ask("a kernel of more registers at once than the part gives code: " + std::to_string(high_water) +
                " past " + std::to_string(machine->register_last + 1u));
        }
        std::string written;
        size_t last = 0u;
        for (std::sregex_iterator found(text.begin(), text.end(), named), end; found != end; ++found)
        {
            written += text.substr(last, (size_t)found->position() - last);
            written += ruleset_register(sass, (found->str()[1] == 'w') ? "wide" : "temporary", given[found->str()]);
            last = (size_t)found->position() + found->str().size();
        }
        written += text.substr(last);
        static const std::regex itself("^\\s*MOV\\s+(\\S+)\\s*,\\s*(\\S+)\\s*;\\s*$");
        std::smatch move;
        text.clear();
        // each line of the text before a move of a register into itself is left out, by its number in the text after,
        // counted from 1, and 0 where it is left out
        std::vector<size_t> kept_as;
        size_t line_at = 0u;
        while (line_at < written.size())
        {
            const size_t line_end = written.find('\n', line_at);
            const std::string line =
                written.substr(line_at, (line_end == std::string::npos) ? std::string::npos : (line_end - line_at));
            const int kept = !std::regex_match(line, move, itself) || (move[1] != move[2]);
            if (kept)
            {
                text += line + "\n";
            }
            kept_as.push_back(kept ? (size_t)std::count(text.begin(), text.end(), '\n') : 0u);
            line_at = (line_end == std::string::npos) ? written.size() : (line_end + 1u);
        }
        held_written(given, first_line, named_last, kept_as);
    }

    // The register each name of the kernel was given, claimed at the line that first names it and released after the
    // line that last names it, those lines numbered as the text holds them once a move of a register into itself is
    // left out: a claim at a line left out is at the next line kept, and a release at one at the line kept before it.
    // Then each fixed register the text names, claimed at the first line naming it and released at the last
    void held_written(const std::map<std::string, unsigned int> &given, const std::map<std::string, size_t> &first_line,
                      const std::map<std::string, size_t> &last_line, const std::vector<size_t> &kept_as)
    {
        const auto claimed_at = [&](size_t line) {
            while ((line < kept_as.size()) && (kept_as[line] == 0u))
            {
                line += 1u;
            }
            return (line < kept_as.size()) ? kept_as[line] : 0u;
        };
        const auto released_at = [&](size_t line) {
            size_t at = (line < kept_as.size()) ? (line + 1u) : kept_as.size();
            while ((at != 0u) && (kept_as[at - 1u] == 0u))
            {
                at -= 1u;
            }
            return (at != 0u) ? kept_as[at - 1u] : 0u;
        };
        std::vector<std::pair<size_t, std::string>> order;
        for (const auto &first : first_line)
        {
            order.push_back(std::make_pair(first.second, first.first));
        }
        std::sort(order.begin(), order.end());
        held_lines.clear();
        for (const auto &each : order)
        {
            const std::string &name = each.second;
            const auto last = last_line.find(name);
            const std::string bank = (name[1] == 'w') ? "wide" : "temporary";
            held_lines += ruleset_register(sass, bank, given.at(name)) + " " + name + " claimed " +
                          std::to_string(claimed_at(each.first)) + " released " +
                          std::to_string(released_at((last != last_line.end()) ? last->second : each.first)) + "\n";
        }
        for (unsigned int fixed = 0u; fixed < sass->schema->fixed_count; fixed += 1u)
        {
            const std::string held = sass->fixed[fixed];
            if (held.empty() || (held == "RZ") || (held == "PT"))
            {
                continue;
            }
            const std::regex named_fixed("(^|[^A-Za-z0-9_])" + held + "([^0-9]|$)");
            size_t first = 0u;
            size_t last = 0u;
            size_t number = 0u;
            for (size_t at = 0u; at < text.size(); at += 1u)
            {
                const size_t end = std::min(text.find('\n', at), text.size());
                number += 1u;
                if (std::regex_search(text.substr(at, end - at), named_fixed))
                {
                    first = (first == 0u) ? number : first;
                    last = number;
                }
                at = end;
            }
            if (first != 0u)
            {
                held_lines += held + " " + sass->schema->fixed[fixed].text + " claimed " + std::to_string(first) +
                              " released " + std::to_string(last) + "\n";
            }
        }
    }

    // A word loaded and read nowhere but by its extension from a byte or a halfword is loaded by the load that extends
    // it, global_load_<thing> for word_from_<thing>, into the register the extension wrote
    void fold_loads(void)
    {
        static const char *const s_narrow[] = {"byte", "signed_byte", "halfword", "signed_halfword"};
        const auto none = [](const std::string &) { return std::string(); };
        std::string load_text;
        if (ruleset_opcode(sass, "global_load_word", {"\x01", "\x02", "\x03"}, none, load_text) == 0)
        {
            return;
        }
        const std::regex load = stick_line_pattern(stick_squeeze(load_text));
        std::vector<std::string> lines;
        for (size_t at = 0u; at < text.size(); at += 1u)
        {
            const size_t end = text.find('\n', at);
            lines.push_back(text.substr(at, end - at));
            at = (end == std::string::npos) ? text.size() : end;
        }
        for (const char *const thing : s_narrow)
        {
            std::string extension_text;
            if (ruleset_opcode(sass, std::string("word_from_") + thing, {"\x01", "\x02"}, none, extension_text) == 0)
            {
                continue;
            }
            const std::regex extension = stick_line_pattern(stick_squeeze(extension_text));
            for (size_t at = 0u; at < lines.size(); at += 1u)
            {
                std::smatch extended;
                const std::string line = stick_squeeze(lines[at]);
                if (!std::regex_match(line, extended, extension))
                {
                    continue;
                }
                const std::string from = extended[2];
                const std::regex named("(^|[^0-9A-Za-z_.])" + from + "(?![0-9]|\\.hi)");
                const std::ptrdiff_t uses =
                    std::distance(std::sregex_iterator(text.begin(), text.end(), named), std::sregex_iterator());
                for (size_t before = 0u; (uses == 2) && (before < at); before += 1u)
                {
                    std::smatch loaded;
                    const std::string earlier = stick_squeeze(lines[before]);
                    std::string written;
                    if (std::regex_match(earlier, loaded, load) && (loaded[1] == from) &&
                        (ruleset_opcode(sass, std::string("global_load_") + thing,
                                        {extended[1], loaded[2], loaded[3]}, none, written) != 0))
                    {
                        lines[before] = written.substr(0u, written.find('\n'));
                        lines[at].clear();
                        break;
                    }
                }
            }
        }
        text.clear();
        for (const std::string &line : lines)
        {
            text += line.empty() ? std::string() : (line + "\n");
        }
    }

    // Each operation the system writes on either pipe written on the FMA pipe, where the part's .kdm holds the lane it
    // moves in and the kernel's text is such a lane
    void balance(void)
    {
        std::vector<std::string> lines;
        for (size_t at = 0u; at < text.size(); at += 1u)
        {
            const size_t end = text.find('\n', at);
            lines.push_back(text.substr(at, end - at));
            at = (end == std::string::npos) ? text.size() : end;
        }
        std::vector<std::string> squeezed;
        for (const std::string &line : lines)
        {
            squeezed.push_back(stick_squeeze(line));
        }
        const std::pair<unsigned int, unsigned int> lane = stick_pipe_lane(squeezed);
        text.clear();
        for (size_t at = 0u; at < lines.size(); at += 1u)
        {
            std::string writing;
            char held[SASS_TEXT];
            unsigned int least = 0u;
            unsigned int over = 0u;
            const std::string operation = stick_pipe_choice(squeezed[at], &writing);
            const size_t space = writing.find(' ');
            const int moves = !operation.empty() && (sass_pipe_held(operation.c_str(), held, &least, &over) != 0) &&
                              (writing.compare(0u, space, held) == 0) && (lane.first >= least) &&
                              (lane.first >= (lane.second + over));
            text += (moves != 0) ? ("\t" + writing.substr(0u, space) + " \t" + writing.substr(space + 1u) + ";")
                                 : lines[at];
            text += "\n";
        }
    }

    // form `name` written in sass.krs with `arguments`; 0 where it is not, `question` then saying why
    int write(const std::string &name, const std::vector<std::string> &arguments)
    {
        std::string listed;
        for (const std::string &argument : arguments)
        {
            listed += " " + argument;
        }
        stick_trace("write %s%s", name.c_str(), listed.c_str());
        // a scratch register the form takes is a fresh one of the kernel's, held over the form's lines alone
        const auto taken = [this](const std::string &bank) {
            return fresh((bank == "wide") ? 64u : ((bank == "predicate") ? STICK_PREDICATE_BITS : 32u)).name;
        };
        if (ruleset_opcode(sass, name, arguments, taken, text) == 0)
        {
            ask("sass.krs writes no " + name);
            return 0;
        }
        forms += 1u;
        return 1;
    }

    // `why` kept as the kernel's question where it has none yet
    void ask(const std::string &why)
    {
        stick_trace("question %s", why.c_str());
        question = question.empty() ? why : question;
    }

    // 1 where this kernel folds and sass.ksc holds a fold of `kind` put for form `form`'s parameter `parameter`
    int folded(unsigned int form, const char *kind, unsigned int parameter) const
    {
        for (const RulesetFold &fold : sass->folds)
        {
            if ((folding != 0) && (fold.form == form) && (fold.kind == kind) && (fold.parameter == parameter))
            {
                return 1;
            }
        }
        return 0;
    }

    // `number` as cu.krs's bank of immediates writes it
    std::string number_text(unsigned int number) const
    {
        const InstrTemplate &bank = cu->banks[REGCLASS_IMMEDIATE];
        return stick_bare(bank.pieces[0]) + std::to_string(number) + stick_bare(bank.pieces.back());
    }

    // 1 where `bare` is a number as cu.krs's bank of immediates writes one, its value in `number`
    int number_of(const std::string &bare, unsigned int *number) const
    {
        const InstrTemplate &bank = cu->banks[REGCLASS_IMMEDIATE];
        const std::string before = stick_bare(bank.pieces[0]);
        const std::string after = stick_bare(bank.pieces.back());
        if ((bare.size() <= (before.size() + after.size())) || (bare.compare(0u, before.size(), before) != 0) ||
            (bare.compare(bare.size() - after.size(), after.size(), after) != 0))
        {
            return 0;
        }
        const std::string digits = bare.substr(before.size(), bare.size() - before.size() - after.size());
        char *end = NULL;
        const unsigned long long value = strtoull(digits.c_str(), &end, 0);
        if ((end == NULL) || (*end != '\0') || (value > 0xFFFFFFFFull))
        {
            return 0;
        }
        *number = (unsigned int)value;
        return 1;
    }

    // 1 where `bare` reads through form `name` of cu.krs as an expression, its parameters' spans in `spans`
    int reads_as(const std::string &name, const std::string &bare, std::vector<std::string> *spans) const
    {
        for (const StickPattern &pattern : *patterns)
        {
            std::vector<std::string> bound;
            std::vector<std::vector<std::string>> found;
            if ((pattern.statement == 0) && (name == s_form_names[pattern.form]))
            {
                stick_match(pattern.pieces, 0u, bare, 0u, bound, found);
            }
            if (!found.empty())
            {
                *spans = found[0];
                return 1;
            }
        }
        return 0;
    }

  private:
    const Ruleset *cu;
    const Ruleset *sass;
    const SassMachine *machine;
    const std::vector<StickPattern> *patterns;
    int folding;
    unsigned int next;
    unsigned int predicates;
    std::map<std::string, StickValue> held_values;
    // how many values a `++` or a `--` has kept under a name of their own, the value its name held before the step
    unsigned int steps_kept = 0u;
    // the names held whose value is never negative
    std::set<std::string> nonnegatives;
    // the joins of two flags read exchanged, each read in that order once
    std::set<std::string> exchanging;
    // Each reading made since the kernel last held a value, by whether it was read in place and its text, and
    // whether it read. A reading is of the values held, the kernel's parameters and the rulesets alone, and a text
    // read again while nothing new is held reads the same: an expression's parts are read once each, however
    // many of its readings hold them
    std::map<std::string, std::pair<int, StickReading>> readings;
    // each bool held as a flag, by its name, and the expression it is the flag of
    std::map<std::string, StickTyped> flags;
    // each name read as an expression not yet written, by the name
    std::map<std::string, StickTyped> standing;
    // the tokens of the expression typed() reads, and the next it takes
    std::vector<std::string> tokens;
    size_t token_at = 0u;

    // `why` kept as the kernel's question, and the expression left untyped
    StickTyped refuse(const std::string &why)
    {
        ask(why);
        return StickTyped{"", -1};
    }

    // The type of `name`: a value or a parameter the kernel holds, or a built-in variable cu.krs reads through a form
    // of no parameters, of the type of a register of the form's width; -1 where it has none
    int type_of(const std::string &name)
    {
        const auto held = held_values.find(name);
        if ((held != held_values.end()) && (held->second.type >= 0))
        {
            return held->second.type;
        }
        const auto parameter = parameters.find(name);
        if (parameter != parameters.end())
        {
            return parameter->second.type;
        }
        StickReading reading;
        if ((read(name, 0, &reading) != 0) && (reading.leaf == STICK_FORM) && reading.children.empty())
        {
            return stick_type_bare((reading.bits == 64u) ? "unsignedlonglong" : "unsignedint");
        }
        return -1;
    }

    // C's conditional, `where ? chosen : otherwise`, or what binds tighter
    StickTyped conditional(void)
    {
        const StickTyped where = binary(1);
        if ((where.type < 0) || (token_at >= tokens.size()) || (tokens[token_at] != stick_operator("?")))
        {
            return where;
        }
        token_at += 1u;
        const StickTyped chosen = conditional();
        if ((chosen.type < 0) || (token_at >= tokens.size()) || (tokens[token_at] != stick_operator(":")))
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
            return StickTyped{"", -1};
        }
        // a select between two words of 0 and 1 holds 0 or 1
        StickTyped selected{"((" + flag.text + ")?" + left.text + ":" + right.text + ")", common};
        selected.zero_one = (left.zero_one != 0) && (right.zero_one != 0);
        return selected;
    }

    // the binary operators that bind at `lowest` or tighter, each left to right over what binds tighter still
    StickTyped binary(int lowest)
    {
        StickTyped left = unary();
        while ((left.type >= 0) && (token_at < tokens.size()))
        {
            const std::string token = stick_operation(tokens[token_at]);
            const int precedence = stick_precedence(token);
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
            left = combined(left, token, right);
        }
        return left;
    }

    // `left` and `right` under binary operator `token`, each converted as C converts it. The logical operators take
    // two flags and give one. A shift takes each side promoted and gives the left's type; every other operator takes
    // both sides in their common type, and a comparison gives a flag. An ordering, a quotient, a remainder and a
    // right shift are of their operands' type, and a signed one is written with its type's cast before each operand,
    // which the signed forms read. Equality reads no sign and takes no cast
    StickTyped combined(const StickTyped &left, const std::string &token, const StickTyped &right)
    {
        const int flag = stick_type_bare("bool");
        if ((token == "&&") || (token == "||"))
        {
            const StickTyped first = converted(left, flag);
            const StickTyped second = converted(right, flag);
            if ((first.type < 0) || (second.type < 0))
            {
                return StickTyped{"", -1};
            }
            return StickTyped{"(" + first.text + token + second.text + ")", flag};
        }
        const int shift = (token == "<<") || (token == ">>");
        const int common = (shift != 0) ? stick_promoted(left.type)
                                        : stick_common(stick_promoted(left.type), stick_promoted(right.type));
        StickTyped first = converted(left, common);
        StickTyped second = converted(right, (shift != 0) ? stick_promoted(right.type) : common);
        if ((first.type < 0) || (second.type < 0))
        {
            return StickTyped{"", -1};
        }
        // two values neither of which is negative order alike signed and unsigned, and are ordered unsigned
        const int orders = (stick_compares(token) != 0) && (token != "==") && (token != "!=") &&
                           ((first.nonnegative == 0) || (second.nonnegative == 0));
        // a quotient and a remainder of two values neither of which is negative, and a right shift of a value that is
        // not, come out alike signed and unsigned, and are written unsigned
        const int divides =
            ((token == "/") || (token == "%")) && ((first.nonnegative == 0) || (second.nonnegative == 0));
        const int shifts = (token == ">>") && (first.nonnegative == 0);
        const int signs = ((orders != 0) || (divides != 0) || (shifts != 0)) && (s_types[common].is_signed != 0);
        if (signs != 0)
        {
            first.text = stick_cast(common) + first.text;
            second.text = (shift != 0) ? second.text : (stick_cast(common) + second.text);
        }
        StickTyped whole{"(" + first.text + token + second.text + ")", (stick_compares(token) != 0) ? flag : common};
        // the low bits of a sum, a difference, a product, a bitwise operation and a left shift are of their operands'
        // low bits alone, the count of a shift taken whole
        const int low_alone = (token == "+") || (token == "-") || (token == "*") || (token == "&") ||
                              (token == "|") || (token == "^") || (token == "<<");
        if ((low_alone != 0) && ((first.low_bits != 0u) || ((token != "<<") && (second.low_bits != 0u))))
        {
            const unsigned int width = stick_held_bits(s_types[common].bits);
            const unsigned int left_bits = (first.low_bits != 0u) ? first.low_bits : width;
            const unsigned int right_bits = ((token != "<<") && (second.low_bits != 0u)) ? second.low_bits : width;
            const std::string &left_low = (first.low_bits != 0u) ? first.low : first.text;
            const std::string &right_low = ((token != "<<") && (second.low_bits != 0u)) ? second.low : second.text;
            whole.low = "(" + left_low + token + right_low + ")";
            whole.low_bits = (token == "<<") ? left_bits : std::min(left_bits, right_bits);
        }
        return whole;
    }

    // `name` stepped by one, `step` the `+` or the `-` of the step: the name read once, its value and 1 under `step`
    // as C combines them, converted back to the name's type, and held as the name's value from here on. `++` and `--`
    // are each an assignment that gives a value, `a += 1` and `a -= 1`: the add primitive of the name's own context
    // and nothing past it. The value read is kept under a name of its own and given through `before`; type -1 where
    // the name holds no value of C to step
    StickTyped stepped(const std::string &name, const std::string &step, StickTyped *before)
    {
        const auto stands = standing.find(name);
        const int type = (stands != standing.end()) ? stands->second.type : type_of(name);
        if ((type < 0) || (flags.count(name) != 0u) || !stick_operation(name).empty() ||
            (isdigit((unsigned char)name[0]) != 0))
        {
            return refuse("a step of what holds no value of C: " + name);
        }
        const int nonnegative =
            (stands != standing.end()) ? stands->second.nonnegative : ((nonnegatives.count(name) != 0u) ? 1 : 0);
        const StickValue read_value = value((stands != standing.end()) ? stands->second.text : name);
        if (read_value.name.empty())
        {
            return StickTyped{"", -1};
        }
        const std::string kept = "stick_kept_" + std::to_string(steps_kept);
        steps_kept += 1u;
        hold(kept, read_value, type, nonnegative);
        *before = StickTyped{kept, type, nonnegative};
        const StickTyped one{number_text(1u), stick_type_bare("int"), 1};
        const StickTyped sum = converted(combined(*before, step, one), type);
        if (sum.type < 0)
        {
            return sum;
        }
        const StickValue value_held = value(sum.text);
        if (value_held.name.empty())
        {
            return StickTyped{"", -1};
        }
        hold(name, value_held, type, sum.nonnegative);
        return StickTyped{name, type, sum.nonnegative};
    }

    // a unary operator over what it binds, a cast, or a primary expression. A step is read in its order: the
    // operation before the operand gives the operand's value after the step, the operand before the operation the
    // value before it
    StickTyped unary(void)
    {
        if (token_at >= tokens.size())
        {
            return refuse("an expression that ends before its operand");
        }
        const std::string token = stick_operation(tokens[token_at]);
        if ((token == "++") || (token == "--"))
        {
            token_at += 1u;
            if (token_at >= tokens.size())
            {
                return refuse("an expression that ends before its operand");
            }
            const std::string name = tokens[token_at];
            token_at += 1u;
            StickTyped before;
            return stepped(name, token.substr(0u, 1u), &before);
        }
        const std::string next = ((token_at + 1u) < tokens.size()) ? stick_operation(tokens[token_at + 1u]) : "";
        if (token.empty() && ((next == "++") || (next == "--")))
        {
            const std::string name = tokens[token_at];
            token_at += 2u;
            StickTyped before;
            const StickTyped after = stepped(name, next.substr(0u, 1u), &before);
            return (after.type < 0) ? after : before;
        }
        if ((token == "-") || (token == "~") || (token == "+") || (token == "!"))
        {
            token_at += 1u;
            const StickTyped operand = unary();
            if (operand.type < 0)
            {
                return operand;
            }
            if (token == "!")
            {
                return combined(operand, "==", StickTyped{number_text(0u), stick_type_bare("int"), 1});
            }
            const StickTyped value = converted(operand, stick_promoted(operand.type));
            if (token == "+")
            {
                return value;
            }
            // a negation is 0 less its operand, in the operand's type
            if (token == "-")
            {
                return combined(StickTyped{number_text(0u), stick_type_bare("int"), 1}, "-", value);
            }
            // a complement's low bits are of its operand's low bits alone
            StickTyped whole{"(" + token + value.text + ")", value.type};
            whole.low = (value.low_bits != 0u) ? ("(" + token + value.low + ")") : std::string();
            whole.low_bits = value.low_bits;
            return whole;
        }
        // a cast is a type's names between parentheses, `(unsigned int)` read as the type `unsignedint` names
        size_t close = token_at + 1u;
        std::string type_name;
        while ((token == "(") && (close < tokens.size()) && stick_operation(tokens[close]).empty())
        {
            type_name += tokens[close];
            close += 1u;
        }
        const int cast = (!type_name.empty() && (close < tokens.size()) && (tokens[close] == stick_operator(")")))
                             ? stick_type_bare(type_name)
                             : -1;
        if (cast >= 0)
        {
            token_at = close + 1u;
            const StickTyped operand = unary();
            return (operand.type < 0) ? operand : converted(operand, cast);
        }
        return primary();
    }

    // a parenthesized expression, a number of C, a name the kernel holds, or a bool held as a flag
    StickTyped primary(void)
    {
        const std::string token = tokens[token_at];
        token_at += 1u;
        if (token == stick_operator("("))
        {
            const StickTyped inner = conditional();
            if ((inner.type < 0) || (token_at >= tokens.size()) || (tokens[token_at] != stick_operator(")")))
            {
                return (inner.type < 0) ? inner : refuse("a parenthesis that does not close");
            }
            token_at += 1u;
            return inner;
        }
        if (isdigit((unsigned char)token[0]) != 0)
        {
            return number_typed(token);
        }
        if ((token_at < tokens.size()) &&
            ((tokens[token_at] == stick_operator("(")) || (tokens[token_at] == stick_operator("["))))
        {
            return refuse("a call or an element nothing here types: " + token);
        }
        const auto flag = flags.find(token);
        if (flag != flags.end())
        {
            return flag->second;
        }
        const auto stands = standing.find(token);
        if (stands != standing.end())
        {
            return stands->second;
        }
        const int type = type_of(token);
        return (type < 0) ? refuse("a name nothing here types: " + token)
                          : StickTyped{token, type, (nonnegatives.count(token) != 0u) ? 1 : 0};
    }

    // a number of C as cu.krs's bank of immediates writes it, of the type its suffix and its size give it: unsigned
    // for a suffix u, long long for ll, and an int otherwise where it fits one
    StickTyped number_typed(const std::string &token)
    {
        char *end = NULL;
        const unsigned long long number = strtoull(token.c_str(), &end, 0);
        std::string suffix = (end == NULL) ? std::string() : std::string(end);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       [](unsigned char letter) { return (char)tolower(letter); });
        const int unsigned_one = (suffix.find('u') != std::string::npos);
        const int wide = (suffix.find("ll") != std::string::npos) || (number > 0xFFFFFFFFull);
        if (number > 0xFFFFFFFFull)
        {
            return refuse("a number wider than a word: " + token);
        }
        const int type = (wide != 0) ? stick_type_bare((unsigned_one != 0) ? "unsignedlonglong" : "longlong")
                         : ((unsigned_one != 0) || (number > 0x7FFFFFFFull)) ? stick_type_bare("unsignedint")
                                                                             : stick_type_bare("int");
        StickTyped written{number_text((unsigned int)number), type, 1};
        written.zero_one = (number <= 1ull);
        return written;
    }
    // each wide that is the pair a word begins, by the wide's name, and the words so held
    std::map<std::string, std::string> ties;
    std::set<std::string> tied_words;
    // each word moved into another: the form that moves it, the word it is moved into, and the word moved
    std::vector<std::pair<unsigned int, std::pair<std::string, std::string>>> pending_copies;

    // The word of the constant bank form `form` moves into the register put for its first parameter, where its
    // sass.krs text, its operands `arguments`, is nothing but such moves: the first move's word. Empty otherwise
    std::string moves(unsigned int form, const std::vector<std::string> &arguments) const
    {
        std::vector<std::string> given = {"\x01"};
        given.insert(given.end(), arguments.begin(), arguments.end());
        std::string lines;
        if (ruleset_opcode(sass, s_form_names[form], given, stick_scratch_stand_in, lines) == 0)
        {
            return std::string();
        }
        const std::regex move("^\\s*MOV\\s+\x01(\\.hi)?\\s*,\\s*(.+?)\\s*;\\s*$");
        std::smatch found;
        std::string first;
        size_t at = 0u;
        while (at < lines.size())
        {
            const size_t end = lines.find('\n', at);
            const std::string line = lines.substr(at, end - at);
            if (!std::regex_match(line, found, move) || stick_constant(found[2]).empty())
            {
                return std::string();
            }
            first = first.empty() ? std::string(found[2]) : first;
            at = (end == std::string::npos) ? lines.size() : (end + 1u);
        }
        return first;
    }

    // 1 where sass.krs's text for `form`, of a to and one more parameter, moves that parameter's register into to
    int copied(unsigned int form) const
    {
        std::string lines;
        if ((sass->schema->forms[form].parameters != 2u) ||
            (ruleset_opcode(sass, s_form_names[form], {"\x01", "\x02"}, stick_scratch_stand_in, lines) == 0))
        {
            return 0;
        }
        return std::regex_search(lines, std::regex("(^|\\n)\\s*MOV\\s+\x01\\s*,\\s*\x02\\s*;"));
    }

    // the operands of a form's parameters past its outputs, as `children` are written: a number or a folded word in
    // place, and a register or predicate of its width otherwise, stand-ins numbered from `stand_in`
    std::vector<std::string> stand_ins(const std::vector<StickReading> &children, unsigned int stand_in) const
    {
        std::vector<std::string> arguments;
        unsigned int predicate = 1u;
        for (const StickReading &child : children)
        {
            if (child.leaf == STICK_NUMBER)
            {
                arguments.push_back(std::to_string(child.number));
                continue;
            }
            if (!child.place.empty())
            {
                arguments.push_back(child.place);
                continue;
            }
            if (child.bits == STICK_PREDICATE_BITS)
            {
                arguments.push_back(((child.negated != 0) ? "!P" : "P") + std::to_string(predicate));
                predicate += 1u;
                continue;
            }
            stand_in += ((child.bits == 64u) && ((stand_in % 2u) != 0u)) ? 1u : 0u;
            arguments.push_back("R" + std::to_string(stand_in));
            stand_in += (child.bits == 64u) ? 2u : 1u;
        }
        return arguments;
    }

    // 1 where sass.krs's text for `form` assembles with `outputs` and `children` as stand_ins writes them
    int assembles(unsigned int form, const std::vector<std::string> &outputs,
                  const std::vector<StickReading> &children) const
    {
        // an output the kernel names is a register of the file of its width until assign() gives it one
        std::vector<std::string> arguments;
        for (const std::string &output : outputs)
        {
            arguments.push_back((output.compare(0u, 2u, "%w") == 0)   ? std::string("R4")
                                : (output.compare(0u, 2u, "%r") == 0) ? std::string("R3")
                                                                      : output);
        }
        const std::vector<std::string> operands = stand_ins(children, 8u);
        arguments.insert(arguments.end(), operands.begin(), operands.end());
        std::string lines;
        if (ruleset_opcode(sass, s_form_names[form], arguments, stick_scratch_stand_in, lines) == 0)
        {
            return 0;
        }
        unsigned int count = 0u;
        for (const char letter : lines)
        {
            count += (letter == '\n') ? 1u : 0u;
        }
        const auto asked = s_stick_assembled.find(std::make_pair(machine, lines));
        if (asked != s_stick_assembled.end())
        {
            return asked->second;
        }
        std::vector<unsigned char> code(16u * (count + 1u));
        const int assembled =
            sass_assemble_lines(machine, lines.c_str(), SASS_CONTROL_SAFE, code.data(), code.size()) == count;
        s_stick_assembled.emplace(std::make_pair(machine, lines), assembled);
        return assembled;
    }

    // Of form `form` over `spans`, its outputs `outputs`, the reading of fewest forms whose text assembles, kept in
    // `best` where it has fewer forms than `best` holds: each span read in place, as the zero register where it is
    // the number 0, and in a register, every choice tried. A span is held in registers of the width `slot_bits` gives
    // its parameter where `slot_bits` is given, a flag in any width. The span at `negated`, where it is not -1, is read
    // as the complement of its flag. An expression's form is given no outputs, and is gated with a register of its
    // value's width put for its to, or a predicate where the text takes no register there
    int choose(unsigned int form, const std::vector<std::string> &outputs, const std::vector<std::string> &spans,
               unsigned int bits_given, int expression, const std::vector<unsigned int> &slot_bits, int negated,
               StickReading *best)
    {
        std::vector<std::vector<StickReading>> options(spans.size());
        for (size_t at = 0u; at < spans.size(); at += 1u)
        {
            StickReading in_place;
            StickReading in_register;
            if ((read(stick_bare(spans[at]), 1, &in_place) == 0) || (read(stick_bare(spans[at]), 0, &in_register) == 0))
            {
                stick_trace("%s refused: its operand %s has no reading", s_form_names[form], spans[at].c_str());
                return 0;
            }
            options[at].push_back(in_place);
            if ((in_place.leaf == STICK_NUMBER) && (in_place.number == 0u))
            {
                options[at].push_back(
                    StickReading{STICK_NAME, 0u, in_place.text, 0u, 0u, 32u, {}, sass->fixed[PHYSREG_ZERO]});
            }
            options[at].push_back(in_register);
        }
        size_t choices = 1u;
        for (const std::vector<StickReading> &option : options)
        {
            choices *= option.size();
        }
        int chosen = 0;
        for (size_t choice = 0u; choice < choices; choice += 1u)
        {
            StickReading reading = {STICK_FORM, form, "", 0u, 1u, 32u, {}, ""};
            unsigned int bits = 32u;
            size_t rest = choice;
            int held = 1;
            for (size_t at = 0u; at < spans.size(); at += 1u)
            {
                StickReading child = options[at][rest % options[at].size()];
                rest /= options[at].size();
                const int count = (at < slot_bits.size()) && (slot_bits[at] == STICK_COUNT_BITS);
                const int fits = !((at < slot_bits.size()) && (count == 0) && (child.bits != STICK_PREDICATE_BITS) &&
                                   (stick_held_bits(child.bits) != slot_bits[at]));
                if ((fits == 0) && (held != 0))
                {
                    stick_trace("%s choice %zu: operand %zu wants %u bits and holds %u", s_form_names[form], choice, at,
                                slot_bits[at], child.bits);
                }
                held = (fits == 0) ? 0 : held;
                child.negated = ((int)at == negated) ? 1 : 0;
                reading.children.push_back(child);
                reading.cost += child.cost;
                // a count is no part of the width of what the form writes
                bits = ((count == 0) && (child.bits > bits) && (child.bits != STICK_PREDICATE_BITS)) ? child.bits
                                                                                                       : bits;
            }
            reading.bits = (bits_given != 0u) ? bits_given : bits;
            if ((held == 0) || (reading.cost >= best->cost))
            {
                continue;
            }
            if (expression == 0)
            {
                if (assembles(form, outputs, reading.children) != 0)
                {
                    *best = reading;
                    chosen = 1;
                }
                continue;
            }
            if (assembles(form, {(reading.bits == 64u) ? "R4" : "R2"}, reading.children) != 0)
            {
                *best = reading;
                chosen = 1;
                stick_trace("%s choice %zu: assembles, %u forms", s_form_names[form], choice, reading.cost);
            }
            else if (assembles(form, {"P0"}, reading.children) != 0)
            {
                reading.bits = STICK_PREDICATE_BITS;
                *best = reading;
                chosen = 1;
                stick_trace("%s choice %zu: assembles as a flag, %u forms", s_form_names[form], choice, reading.cost);
            }
            else
            {
                stick_trace("%s choice %zu: sass.krs's text does not assemble over it", s_form_names[form], choice);
            }
        }
        return chosen;
    }

    // `reading` as the word it stands as where the system folds the register put for its form's to and `in_place`
    // lets it stand: unchanged otherwise
    void fold(StickReading *reading, int in_place) const
    {
        if ((in_place == 0) || ((reading->leaf != STICK_FORM) && (reading->leaf != STICK_PARAMETER)) ||
            (folded(reading->form, "register", 0u) == 0))
        {
            return;
        }
        const std::string word = moves(reading->form, stand_ins(reading->children, 8u));
        if (!word.empty())
        {
            reading->place = word;
            reading->cost = 0u;
        }
    }

    // the reading of `bare` of the fewest forms, a number or a folded word in place only where `in_place` allows it;
    // 0 where there is none
    // `bare` read as read_untraced() reads it, the reading and what it gives written to the trace
    int read(const std::string &bare, int in_place, StickReading *best)
    {
        static const char *const s_leaves[] = {"form", "held", "number in place", "number set", "parameter"};
        stick_trace("read %s%s", bare.c_str(), (in_place != 0) ? " in place" : "");
        // a reading made while a join of flags is read exchanged is not kept: the join is refused its second order
        const std::string key = std::string((in_place != 0) ? "1" : "0") + bare;
        const auto before = exchanging.empty() ? readings.find(key) : readings.end();
        int read_given = 0;
        if (before != readings.end())
        {
            read_given = before->second.first;
            *best = before->second.second;
            stick_trace("(read before)");
        }
        else
        {
            {
                const StickTraceDeeper deeper;
                read_given = read_untraced(bare, in_place, best);
            }
            if (exchanging.empty())
            {
                readings[key] = std::make_pair(read_given, *best);
            }
        }
        if (read_given == 0)
        {
            stick_trace("-> no reading of %s", bare.c_str());
            return 0;
        }
        stick_trace("-> %s%s%s, %u forms, %u bits", s_leaves[best->leaf],
                    (best->leaf == STICK_FORM) ? " " : "", (best->leaf == STICK_FORM) ? s_form_names[best->form] : "",
                    best->cost, best->bits);
        return 1;
    }

    int read_untraced(const std::string &bare, int in_place, StickReading *best)
    {
        best->cost = 0xFFFFFFFFu;
        unsigned int number = 0u;
        const auto held = held_values.find(bare);
        if (held != held_values.end())
        {
            *best = StickReading{STICK_NAME, 0u, bare, 0u, 0u, held->second.bits, {}, ""};
            return 1;
        }
        if (number_of(bare, &number) != 0)
        {
            const auto set = held_values.find("=" + bare);
            *best = (in_place != 0)              ? StickReading{STICK_NUMBER, 0u, bare, number, 0u, 32u, {}, ""}
                    : (set != held_values.end()) ? StickReading{STICK_NAME, 0u, "=" + bare, 0u, 0u, 32u, {}, ""}
                                                 : StickReading{STICK_NUMBER_SET, 0u, bare, number, 1u, 32u, {}, ""};
            return 1;
        }
        const auto parameter = parameters.find(bare);
        if (parameter != parameters.end())
        {
            const unsigned int load =
                stick_form((parameter->second.bits == 64u) ? "parameter_load_wide" : "parameter_load_word");
            *best = StickReading{STICK_PARAMETER, load, bare, 0u, 1u, parameter->second.bits, {}, ""};
            best->children.push_back(StickReading{STICK_NUMBER, 0u, "", parameter->second.offset, 0u, 32u, {}, ""});
            fold(best, in_place);
            return 1;
        }
        // a word of a value held in a pair is the pair's low word, the even register, and is read as that register; a
        // wide value held as its low word alone is read as that word
        static const char *const s_words[] = {"(unsignedint)", "(int)"};
        for (unsigned int word = 0u; word < 2u; word += 1u)
        {
            const size_t size = strlen(s_words[word]);
            const auto pair =
                (bare.compare(0u, size, s_words[word]) == 0) ? held_values.find(bare.substr(size)) : held_values.end();
            const int low = (pair != held_values.end()) &&
                            ((pair->second.bits == STICK_PAIR_BITS) ||
                             ((pair->second.type >= 0) && (s_types[pair->second.type].bits > STICK_REGISTER_BITS)));
            if (low != 0)
            {
                held_values[bare] = StickValue{pair->second.name, STICK_REGISTER_BITS, (word == 1u) ? 1 : 0,
                                               stick_type_bare(s_words[word])};
                *best = StickReading{STICK_NAME, 0u, bare, 0u, 0u, STICK_REGISTER_BITS, {}, ""};
                return 1;
            }
        }
        if ((bare.size() > 2u) && (bare.front() == '(') && (bare.back() == ')') &&
            stick_balanced(bare.substr(1u, bare.size() - 2u)))
        {
            return read(bare.substr(1u, bare.size() - 2u), in_place, best);
        }
        for (const StickPattern &pattern : *patterns)
        {
            std::vector<std::string> bound;
            std::vector<std::vector<std::string>> found;
            if (pattern.statement == 0)
            {
                stick_match(pattern.pieces, 0u, bare, 0u, bound, found);
            }
            for (const std::vector<std::string> &spans : found)
            {
                std::string listed;
                for (const std::string &span : spans)
                {
                    listed += " [" + span + "]";
                }
                stick_trace("text of %s matches:%s", s_form_names[pattern.form], listed.c_str());
                std::vector<std::string> ordered(spans.size());
                for (size_t slot = 0u; slot < spans.size(); slot += 1u)
                {
                    ordered[pattern.order[slot]] = spans[slot];
                }
                // A select over a test is the select over the test's complement with its arms exchanged. Of
                // readings of as many forms the first taken is kept, and the complement's goes first where the
                // system tests the complement
                std::vector<std::string> complement;
                if (pattern.select != 0)
                {
                    const std::string where = stick_bare(ordered[pattern.order[0]]);
                    complement = ordered;
                    std::swap(complement[pattern.order[1]], complement[pattern.order[2]]);
                    complement[pattern.order[0]] = stick_complement(where);
                    StickReading test;
                    StickReading opposite;
                    const int tests = !complement[pattern.order[0]].empty() && (read(where, 0, &test) != 0) &&
                                      (read(complement[pattern.order[0]], 0, &opposite) != 0) &&
                                      (test.leaf == STICK_FORM) && (opposite.leaf == STICK_FORM);
                    if (tests != 0)
                    {
                        asked.complemented.push_back(std::make_pair(test.form, opposite.form));
                    }
                    if ((tests != 0) && (folded(test.form, "complement", 0u) != 0))
                    {
                        choose(pattern.form, std::vector<std::string>(), complement, pattern.bits, 1,
                               pattern.slot_bits, -1, best);
                    }
                    complement = (tests != 0) ? complement : std::vector<std::string>();
                }
                choose(pattern.form, std::vector<std::string>(), ordered, pattern.bits, 1, pattern.slot_bits, -1, best);
                // a select over a flag is the select over the flag's complement with its arms exchanged
                if (pattern.select != 0)
                {
                    std::swap(ordered[pattern.order[1]], ordered[pattern.order[2]]);
                    choose(pattern.form, std::vector<std::string>(), ordered, pattern.bits, 1, pattern.slot_bits,
                           (int)pattern.order[0], best);
                }
                if (!complement.empty())
                {
                    choose(pattern.form, std::vector<std::string>(), complement, pattern.bits, 1, pattern.slot_bits,
                           -1, best);
                }
                // a power of two the system folds is the shift of the multiply's stem by its log2
                const unsigned int shift = stick_shift_of(pattern.form);
                for (unsigned int at = 0u; (spans.size() == 2u) && (shift != OPCODE_COUNT) && (at < 2u); at += 1u)
                {
                    unsigned int power = 0u;
                    if ((folded(pattern.form, "pow2", at + 1u) != 0) && (number_of(spans[at], &power) != 0) &&
                        (stick_log2(power) != 0u))
                    {
                        choose(shift, std::vector<std::string>(), {spans[1u - at], number_text(stick_log2(power))},
                               pattern.bits, 1, {}, -1, best);
                    }
                }
            }
        }
        // Two flags joined by && or || hold in either order, nothing the reader reads writing anything, and a form
        // that glues a test to another flag reads the test first. The exchanged text is read once: it exchanges back
        const std::string exchanged = stick_flags_exchanged(bare);
        if ((best->cost == 0xFFFFFFFFu) && !exchanged.empty() && (exchanging.count(bare) == 0u))
        {
            exchanging.insert(exchanged);
            StickReading other;
            const int read_other = read(exchanged, in_place, &other);
            exchanging.erase(exchanged);
            if (read_other != 0)
            {
                *best = other;
            }
        }
        if (best->cost == 0xFFFFFFFFu)
        {
            return 0;
        }
        best->text = bare;
        fold(best, in_place);
        return 1;
    }

    // `reading` written in sass.krs, its parameters first, into a register of its own, held for its text: a folded
    // reading writes nothing and is the word it stands as
    StickValue written(const StickReading &reading)
    {
        if (!reading.place.empty())
        {
            return StickValue{reading.place, reading.bits, 0};
        }
        if (reading.leaf == STICK_NAME)
        {
            return held_values[reading.text];
        }
        if (reading.leaf == STICK_NUMBER)
        {
            return StickValue{ruleset_register(sass, "immediate", reading.number), 32u, 0};
        }
        if (reading.leaf == STICK_NUMBER_SET)
        {
            const StickValue value = fresh(reading.bits);
            write("word_set", {value.name, ruleset_register(sass, "immediate", reading.number)});
            held_values["=" + reading.text] = value;
            readings.clear();
            return value;
        }
        std::vector<std::string> operands;
        for (size_t at = 0u; at < reading.children.size(); at += 1u)
        {
            const StickReading &child = reading.children[at];
            operands.push_back(operand(child));
            const unsigned int log2 = stick_log2(child.number);
            if (((child.leaf == STICK_NUMBER) || (child.leaf == STICK_NUMBER_SET)) && (log2 != 0u) &&
                (stick_shift_of(reading.form) != OPCODE_COUNT))
            {
                asked.multiplied.push_back(std::make_pair(std::make_pair(reading.form, (unsigned int)at + 1u), log2));
            }
        }
        const std::string word = moves(reading.form, operands);
        if (!word.empty())
        {
            asked.moved.push_back(std::make_pair(reading.form, word));
        }
        // A wide made of a word whose move the system folds is the pair the word begins; a word moved into another is
        // held for assign() to read how often the moved word is used (assign())
        const int copies =
            (operands.size() == 1u) && (copied(reading.form) != 0) && (operands[0].compare(0u, 2u, "%r") == 0);
        if ((copies != 0) && (reading.bits == 64u))
        {
            asked.copied.push_back(reading.form);
        }
        const StickValue value = fresh(reading.bits);
        if ((copies != 0) && (reading.bits == 64u) && (folded(reading.form, "register", 1u) != 0) &&
            (tied_words.find(operands[0]) == tied_words.end()))
        {
            ties[value.name] = operands[0];
            tied_words.insert(operands[0]);
        }
        if ((copies != 0) && (reading.bits == 32u))
        {
            pending_copies.push_back(std::make_pair(reading.form, std::make_pair(value.name, operands[0])));
        }
        std::vector<std::string> arguments = {value.name};
        arguments.insert(arguments.end(), operands.begin(), operands.end());
        write(s_form_names[reading.form], arguments);
        held_values[reading.text] = value;
        readings.clear();
        return value;
    }

    // `child` written as its parent's operand: its value, or the complement of its flag where it is read as one
    std::string operand(const StickReading &child)
    {
        return std::string((child.negated != 0) ? "!" : "") + written(child).name;
    }
};

// the type `text` names, or -1
static int stick_type(const std::string &text)
{
    return stick_type_bare(stick_bare(text));
}

// a kernel of the stick: its number, its parameters' text and its body's statements
struct StickSource
{
    std::string number;
    std::string parameters;
    std::vector<std::string> statements;
};

static std::vector<StickSource> stick_sources(const char *path)
{
    std::vector<StickSource> sources;
    std::ifstream file(path);
    std::string line;
    const std::regex opening("^extern \"C\" __global__ void measuring_stick_([0-9]+)\\((.*)\\)$");
    std::smatch found;
    while (std::getline(file, line))
    {
        if (!std::regex_match(line, found, opening))
        {
            continue;
        }
        StickSource source = {found[1], found[2], {}};
        std::string body;
        std::getline(file, line);
        while (std::getline(file, line) && (line != "}"))
        {
            body += line + "\n";
        }
        int depth = 0;
        std::string statement;
        for (const char letter : body)
        {
            depth += ((letter == '(') || (letter == '{') || (letter == '[')) ? 1 : 0;
            depth -= ((letter == ')') || (letter == '}') || (letter == ']')) ? 1 : 0;
            statement += letter;
            if (((letter == ';') || (letter == '}')) && (depth == 0))
            {
                source.statements.push_back(stick_squeeze(statement));
                statement.clear();
            }
        }
        sources.push_back(source);
    }
    return sources;
}

// each kernel's instructions in nvcc's listing, by the kernel's number, NOP left out
static std::map<std::string, std::vector<std::string>> stick_listing(const char *path)
{
    std::map<std::string, std::vector<std::string>> kernels;
    std::ifstream file(path);
    std::string line;
    const std::regex function("^\\s*Function : measuring_stick_([0-9]+)\\s*$");
    const std::regex instruction("^\\s*/\\*[0-9a-f]{4,}\\*/\\s+(.*?)\\s*;.*$");
    std::smatch found;
    std::vector<std::string> *kernel = NULL;
    while (std::getline(file, line))
    {
        if (std::regex_match(line, found, function))
        {
            kernel = &kernels[found[1]];
            continue;
        }
        if ((kernel != NULL) && std::regex_match(line, found, instruction) && (found[1] != "NOP"))
        {
            kernel->push_back(std::regex_replace(std::string(found[1]), std::regex("^@!?U?P[T0-9]+\\s+"), ""));
        }
    }
    return kernels;
}

// the operands of an instruction of nvcc's listing, its operation cut
static std::vector<std::string> stick_operands(const std::string &instruction)
{
    std::vector<std::string> operands;
    const size_t space = instruction.find(' ');
    size_t at = (space == std::string::npos) ? instruction.size() : (space + 1u);
    while (at < instruction.size())
    {
        const size_t comma = instruction.find(',', at);
        operands.push_back(stick_bare(instruction.substr(at, comma - at)));
        at = (comma == std::string::npos) ? instruction.size() : (comma + 1u);
    }
    return operands;
}

// 1 where an instruction of `listing` whose operation is `operation`, or any but a move where it is empty, has an
// operand `operand` or, where `within` is set, one holding `operand`
static int stick_listing_holds(const std::vector<std::string> &listing, const std::string &operation,
                               const std::string &operand, int within)
{
    for (const std::string &instruction : listing)
    {
        const std::string named = instruction.substr(0u, instruction.find(' '));
        const int taken = operation.empty() ? (named.compare(0u, 3u, "MOV") != 0) : (named == operation);
        for (const std::string &one : stick_operands(instruction))
        {
            const int holds = (within != 0) ? (one.find(operand) != std::string::npos)
                                            : ((one == operand) || (stick_constant(one) == operand));
            if ((taken != 0) && (holds != 0))
            {
                return 1;
            }
        }
    }
    return 0;
}

// 1 where `name` is read in a statement of `statements` from `from` on, and every read of it is converted to a type
// of one register's width
static int stick_narrowed(const std::vector<std::string> &statements, size_t from, const std::string &name)
{
    const std::regex read("(^|[^A-Za-z_0-9])" + name + "(?![A-Za-z_0-9])");
    const std::regex cast("\\(\\s*([A-Za-z ]+?)\\s*\\)\\s*$");
    unsigned int reads = 0u;
    for (size_t at = from; at < statements.size(); at += 1u)
    {
        const std::string &statement = statements[at];
        for (std::sregex_iterator one(statement.begin(), statement.end(), read), end; one != end; ++one)
        {
            const std::string before = statement.substr(0u, (size_t)one->position() + one->str(1).size());
            std::smatch found;
            const int type = std::regex_search(before, found, cast) ? stick_type_bare(stick_bare(found[1])) : -1;
            if ((type < 0) || (s_types[type].bits != STICK_REGISTER_BITS))
            {
                return 0;
            }
            reads += 1u;
        }
    }
    return reads != 0u;
}

// 1 where an instruction of `listing` of operation `operation` reads no word of the constant bank, as the kernel's
// guard reads the count
static int stick_listing_writes(const std::vector<std::string> &listing, const std::string &operation)
{
    for (const std::string &instruction : listing)
    {
        if ((instruction.substr(0u, instruction.find(' ')) == operation) &&
            (instruction.find("c[") == std::string::npos))
        {
            return 1;
        }
    }
    return 0;
}

// `source` read and written into `kernel`: empty where it is, else the question it puts
static std::string stick_kernel(const StickSource &source, StickKernel *kernel)
{
    // the parameters, each at the next offset its width aligns
    unsigned int offset = 0u;
    const std::regex parameter("^\\s*(.*?)\\s*(\\*?)\\s*([A-Za-z_][A-Za-z_0-9]*)\\s*$");
    std::smatch found;
    size_t at = 0u;
    while (at <= source.parameters.size())
    {
        const size_t comma = source.parameters.find(',', at);
        const std::string one =
            source.parameters.substr(at, (comma == std::string::npos) ? std::string::npos : (comma - at));
        if (!std::regex_match(one, found, parameter))
        {
            return "a parameter nothing here reads: " + one;
        }
        const unsigned int bits = (found[2] == "*") ? 64u : 32u;
        offset = (offset + ((bits / 8u) - 1u)) & ~((bits / 8u) - 1u);
        kernel->parameters[found[3]] = StickParameter{offset, bits, (found[2] == "*") ? -1 : stick_type(found[1])};
        offset += bits / 8u;
        at = (comma == std::string::npos) ? (source.parameters.size() + 1u) : (comma + 1u);
    }
    kernel->write("kernel_open", {});
    const std::regex operand("^(?:const )?(.+) ([A-Za-z_][A-Za-z_0-9]*) = \\((.+)\\)in\\[(.+)\\];$");
    const std::regex declared("^(?:const )?(.+) ([A-Za-z_][A-Za-z_0-9]*) = (.+);$");
    const std::regex assigned("^([A-Za-z_][A-Za-z_0-9]*) (\\+|-|\\*|/|%|&|\\||\\^|<<|>>)?= (.+);$");
    const std::regex stored("^out\\[(.+)\\] = \\(unsigned long long\\)([A-Za-z_][A-Za-z_0-9]*);$");
    const int element = stick_type("unsigned long long");
    const int word = stick_type("unsigned int");
    // the type each name was declared with, which an assignment to the name converts its value to
    std::map<std::string, int> declared_types;
    // `name` given `expression` converted to `type`: 1, or 0 where the kernel's question says why not
    const auto given = [&](const std::string &name, int type, const std::string &expression) -> int {
        const StickTyped value = kernel->converted(kernel->typed(expression), type);
        if (value.type < 0)
        {
            return 0;
        }
        if (s_types[type].bits == STICK_PREDICATE_BITS)
        {
            kernel->hold_flag(name, value);
            return 1;
        }
        const StickValue held = kernel->value(value.text);
        if (held.name.empty())
        {
            return 0;
        }
        kernel->hold(name, held, type, value.nonnegative);
        return 1;
    };
    for (size_t statement_at = 0u; statement_at < source.statements.size(); statement_at += 1u)
    {
        const std::string &statement = source.statements[statement_at];
        if (std::regex_match(statement, found, operand))
        {
            const int type = stick_type(found[1]);
            if ((type < 0) || (found[1] != found[3]))
            {
                return "a value of type " + std::string(found[1]);
            }
            declared_types[found[2]] = type;
            // The element in[i] is an unsigned long long at its address. A value no wider than a register is
            // converted from the element's low word, the word at the element's address; a wider value, and
            // a bool, which tests the whole element, read the pair. A wider value every later statement converts
            // to a word before it reads it is read as its low word alone
            const int narrowed = stick_narrowed(source.statements, statement_at + 1u, found[2]);
            const int whole =
                ((s_types[type].bits > STICK_REGISTER_BITS) && (narrowed == 0)) ||
                (s_types[type].bits == STICK_PREDICATE_BITS);
            const unsigned int load = stick_form((whole != 0) ? "global_load_wide" : "global_load_word");
            // in[i + k], k a number, is k elements past the address of in[i] where the system folds the offset
            std::string index = stick_bare(found[4]);
            unsigned int elements = 0u;
            std::vector<std::string> spans;
            if (kernel->reads_as("word_add", index, &spans) && kernel->number_of(spans[1], &elements))
            {
                if (kernel->folded(load, "offset", 1u) != 0)
                {
                    index = spans[0];
                }
                else if (elements != 0u)
                {
                    kernel->asked.offsets.push_back(std::make_pair(load, 8u * elements));
                }
            }
            const unsigned int element_offset = (index == stick_bare(found[4])) ? 0u : (8u * elements);
            const std::string address_of = "&in[" + index + "]";
            if (kernel->holds(address_of) == 0)
            {
                const StickValue address = kernel->fresh(64u);
                if (kernel->form("wide_mul_word_add", {address.name}, {index, kernel->number_text(8u), "in"}) ==
                    0)
                {
                    return kernel->question;
                }
                kernel->hold(address_of, address, -1);
            }
            const StickValue loaded = kernel->fresh((whole != 0) ? STICK_PAIR_BITS : STICK_REGISTER_BITS);
            if (kernel->form(s_form_names[load], {loaded.name}, {address_of, kernel->number_text(element_offset)}) == 0)
            {
                return kernel->question;
            }
            const std::string name = stick_bare(found[2]);
            if ((whole == 0) && (s_types[type].bits > STICK_REGISTER_BITS))
            {
                kernel->hold(name, loaded, type);
                continue;
            }
            // A byte or a halfword is the narrowing of the word loaded, written where it is read. A bool is no
            // narrowing: it is the whole value loaded tested against 0, and is read below as a flag of that value
            if ((s_types[type].bits < STICK_REGISTER_BITS) && (s_types[type].bits != STICK_PREDICATE_BITS))
            {
                const std::string loaded_word = name + "@word";
                // the word is the low word of what was loaded, the even register where a pair was loaded
                StickValue low = loaded;
                low.bits = STICK_REGISTER_BITS;
                kernel->hold(loaded_word, low, word);
                const StickTyped narrowed = kernel->converted(StickTyped{loaded_word, word}, type);
                if (narrowed.type < 0)
                {
                    return kernel->question;
                }
                kernel->stand(name, narrowed);
                continue;
            }
            const int read_as = (whole != 0) ? element : word;
            kernel->hold(name, loaded, read_as);
            const StickTyped value = kernel->converted(StickTyped{name, read_as}, type);
            if (value.type < 0)
            {
                return kernel->question;
            }
            if (s_types[type].bits == STICK_PREDICATE_BITS)
            {
                kernel->hold_flag(name, value);
                continue;
            }
            const StickValue held = kernel->value(value.text);
            if (held.name.empty())
            {
                return kernel->question;
            }
            kernel->hold(name, held, type, value.nonnegative);
            continue;
        }
        if (std::regex_match(statement, found, stored))
        {
            const StickTyped value = kernel->typed("(unsigned long long)" + std::string(found[2]));
            if ((value.type < 0) || kernel->value(value.text).name.empty())
            {
                return kernel->question;
            }
            const std::string converted = value.text;
            const std::string address_of = "&out[" + stick_bare(found[1]) + "]";
            const StickValue address = kernel->fresh(64u);
            if (kernel->form("wide_mul_word_add", {address.name}, {found[1], kernel->number_text(8u), "out"}) == 0)
            {
                return kernel->question;
            }
            kernel->hold(address_of, address, -1);
            if (kernel->form("global_store_wide", {}, {address_of, kernel->number_text(0u), converted}) == 0)
            {
                return kernel->question;
            }
            continue;
        }
        if (std::regex_match(statement, found, declared))
        {
            const int type = stick_type(found[1]);
            if (type < 0)
            {
                return "a value of type " + std::string(found[1]);
            }
            declared_types[stick_bare(found[2])] = type;
            if (given(stick_bare(found[2]), type, found[3]) == 0)
            {
                return kernel->question;
            }
            continue;
        }
        // an assignment, and a compound one read as the assignment of its operator over the name and the value
        if (std::regex_match(statement, found, assigned) && (declared_types.count(found[1]) != 0u))
        {
            const std::string name = found[1];
            const std::string expression =
                (found[2].length() == 0u)
                    ? std::string(found[3])
                    : ("(" + name + ")" + std::string(found[2]) + "(" + std::string(found[3]) + ")");
            if (given(name, declared_types[name], expression) == 0)
            {
                return kernel->question;
            }
            continue;
        }
        if (kernel->statement(stick_bare(statement)) != 0)
        {
            continue;
        }
        return kernel->question.empty() ? ("a statement nothing here reads: " + statement) : kernel->question;
    }
    kernel->write("kernel_close", {});
    // a load folded into its extension writes the extension's register lines sooner, and registers are given over
    // the lines the folded text holds
    kernel->fold_loads();
    kernel->assign();
    kernel->balance();
    return kernel->question;
}

// One round of the stick, folding by what sass.ksc holds where `folding` is set: each kernel read and held against
// nvcc's listing of it, each question it asks added to `openings` and each fold nvcc's listing shows to `found`
static void stick_round(const std::vector<StickSource> &sources,
                        const std::map<std::string, std::vector<std::string>> &listing, const Ruleset *cu,
                        const Ruleset *sass, const SassMachine *machine, const std::vector<StickPattern> &patterns,
                        int folding, std::set<std::string> *openings, std::set<std::string> *found)
{
    for (const StickSource &source : sources)
    {
        StickKernel kernel(cu, sass, machine, &patterns, folding);
        const auto nvcc = listing.find(source.number);
        if (!stick_kernel(source, &kernel).empty() || (nvcc == listing.end()))
        {
            continue;
        }
        for (const auto &move : kernel.asked.moved)
        {
            openings->insert(stick_fold_line(sass, move.first, "register", 0u, 1));
            if (stick_listing_holds(nvcc->second, std::string(), stick_constant(move.second), 0) != 0)
            {
                found->insert(stick_fold_line(sass, move.first, "register", 0u, 0));
            }
        }
        for (const auto &multiply : kernel.asked.multiplied)
        {
            const unsigned int form = multiply.first.first;
            openings->insert(stick_fold_line(sass, form, "pow2", multiply.first.second, 1));
            if (stick_listing_holds(nvcc->second, stick_operation(sass, stick_shift_of(form)),
                                    stick_hex(multiply.second), 0) != 0)
            {
                found->insert(stick_fold_line(sass, form, "pow2", multiply.first.second, 0));
            }
        }
        // The system folds a word's move into a wide, and the move of a word used once, where its listing of the
        // kernel moves no register into another
        int moves = 0;
        for (const std::string &instruction : nvcc->second)
        {
            const std::vector<std::string> operands = stick_operands(instruction);
            moves = ((instruction.compare(0u, 4u, "MOV ") == 0) && (operands.size() == 2u) &&
                     std::regex_match(operands[1], std::regex("^R[0-9]+$")))
                        ? 1
                        : moves;
        }
        for (const unsigned int form : kernel.asked.copied)
        {
            openings->insert(stick_fold_line(sass, form, "register", 1u, 1));
            if (moves == 0)
            {
                found->insert(stick_fold_line(sass, form, "register", 1u, 0));
            }
        }
        for (const unsigned int form : kernel.asked.copied_once)
        {
            openings->insert(stick_fold_line(sass, form, "word_used_once", 1u, 1));
            if (moves == 0)
            {
                found->insert(stick_fold_line(sass, form, "word_used_once", 1u, 0));
            }
        }
        // the system tests a select's test as its complement where its listing writes the complement's operation and
        // not the test's
        for (const auto &complement : kernel.asked.complemented)
        {
            openings->insert(stick_fold_line(sass, complement.first, "complement", 0u, 1));
            if ((stick_listing_writes(nvcc->second, stick_operation(sass, complement.second)) != 0) &&
                (stick_listing_writes(nvcc->second, stick_operation(sass, complement.first)) == 0))
            {
                found->insert(stick_fold_line(sass, complement.first, "complement", 0u, 0));
            }
        }
        for (const auto &offset : kernel.asked.offsets)
        {
            openings->insert(stick_fold_line(sass, offset.first, "offset", 1u, 1));
            if (stick_listing_holds(nvcc->second, stick_operation(sass, offset.first),
                                    "+" + stick_hex(offset.second) + "]", 1) != 0)
            {
                found->insert(stick_fold_line(sass, offset.first, "offset", 1u, 0));
            }
        }
    }
}

// sass.ksc written: what it held that opens with each of `openings` dropped, and each of `found` taken; and the pipes
// the part's .kdm holds taken, for the rounds after to write by
static void stick_folds_write(const Ruleset *sass, const std::set<std::string> &openings,
                              const std::set<std::string> &found)
{
    const size_t slash = sass->path.find_last_of('/');
    const std::string rulesets = sass->path.substr(0u, slash);
    const std::string file = sass->path.substr(slash + 1u);
    const std::string stem = file.substr(0u, file.find_last_of('.'));
    sass_class_read(rulesets.c_str(), stem.c_str());
    sass_pipe_read(STICK_MACHINES, STICK_PART);
    for (const std::string &opening : openings)
    {
        sass_class_drop(SASS_CHANNEL_COMPILE, SASS_CLASS_FOLDS, opening.c_str());
    }
    for (const std::string &fold : found)
    {
        sass_class_take(SASS_CHANNEL_COMPILE, SASS_CLASS_FOLDS, fold.c_str(), 0u);
    }
    sass_class_write(rulesets.c_str(), stem.c_str());
}

// The lane the system moves each operation it writes on either pipe in, read off nvcc's listing and held in the part's
// .kdm: of every least count of the integer pipe and every count over the FMA pipe, the pair under which the most of
// the listing's writings of the operation are on the pipe the listing wrote them on, the smaller pair where two hold as
// many
static void stick_pipes_write(const std::map<std::string, std::vector<std::string>> &listing)
{
    std::map<std::string, std::vector<std::pair<int, std::pair<unsigned int, unsigned int>>>> seen;
    std::map<std::string, std::string> written_on;
    // a bound worth trying is a count some lane has, and one past it
    std::set<unsigned int> leasts = {0u};
    std::set<unsigned int> overs = {0u};
    for (const auto &kernel : listing)
    {
        const std::pair<unsigned int, unsigned int> lane = stick_pipe_lane(kernel.second);
        const unsigned int difference = (lane.first > lane.second) ? (lane.first - lane.second) : 0u;
        leasts.insert({lane.first, lane.first + 1u});
        overs.insert({difference, difference + 1u});
        for (const std::string &instruction : kernel.second)
        {
            std::string writing;
            const std::string operation = stick_pipe_choice(instruction, &writing);
            if (!operation.empty())
            {
                seen[operation].push_back(std::make_pair((instruction.compare(0u, 4u, "IMAD") == 0) ? 1 : 0, lane));
                written_on[operation] = writing.substr(0u, writing.find(' '));
            }
        }
    }
    sass_pipe_read(STICK_MACHINES, STICK_PART);
    for (const auto &operation : seen)
    {
        unsigned int best = 0u;
        std::pair<unsigned int, unsigned int> chosen(0u, 0u);
        for (const unsigned int least : leasts)
        {
            for (const unsigned int over : overs)
            {
                unsigned int agree = 0u;
                for (const auto &one : operation.second)
                {
                    const int moves = (one.second.first >= least) && (one.second.first >= (one.second.second + over));
                    agree += (moves == one.first) ? 1u : 0u;
                }
                if (agree > best)
                {
                    best = agree;
                    chosen = std::make_pair(least, over);
                }
            }
        }
        sass_pipe_take(operation.first.c_str(), written_on[operation.first].c_str(), chosen.first, chosen.second);
        fprintf(stderr, "  pipe: %s written %s from %u on the integer pipe and %u over the FMA pipe, %u of %zu\n",
                operation.first.c_str(), written_on[operation.first].c_str(), chosen.first, chosen.second, best,
                operation.second.size());
    }
    sass_pipe_write(STICK_MACHINES, STICK_PART);
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "measuring_stick_engine <stick .cu> <nvcc listing> <out directory>\n");
        return 2;
    }
    const Ruleset *const cu = cu_target().ruleset(1);
    const Ruleset *const sass = sass_target().ruleset(1);
    if ((cu == NULL) || (sass == NULL))
    {
        return 1;
    }
    static SassMachine machine;
    if (sass_machine_read(&machine, STICK_MACHINES "/" STICK_PART) == 0)
    {
        fprintf(stderr, "the machine file is not read\n");
        return 1;
    }
    const std::vector<StickPattern> patterns = stick_patterns(cu, sass);
    const std::vector<StickSource> sources = stick_sources(argv[1]);
    const std::map<std::string, std::vector<std::string>> listing = stick_listing(argv[2]);
    std::set<std::string> openings;
    std::set<std::string> found;
    for (unsigned int round = 0u; round < STICK_ROUNDS; round += 1u)
    {
        const size_t before = found.size();
        stick_round(sources, listing, cu, sass, &machine, patterns, (round == 0u) ? 0 : 1, &openings, &found);
        stick_folds_write(sass, openings, found);
        sass_target().folds_read();
        if ((round != 0u) && (found.size() == before))
        {
            break;
        }
    }
    stick_pipes_write(listing);
    const std::string out = argv[3];
    FILE *const table = fopen((out + "/engine.tsv").c_str(), "wb");
    if (table == NULL)
    {
        return 1;
    }
    fprintf(table, "number\tanswer\tforms\tinstructions\tnote\n");
    unsigned int answered = 0u;
    for (const StickSource &source : sources)
    {
        StickKernel kernel(cu, sass, &machine, &patterns, 1);
        // STICK_TRACE set writes <out>/<number>.trace beside the kernel's text
        s_trace = (getenv("STICK_TRACE") != NULL) ? fopen((out + "/" + source.number + ".trace").c_str(), "wb") : NULL;
        const std::string question = stick_kernel(source, &kernel);
        if (s_trace != NULL)
        {
            fprintf(s_trace, "%s\n", question.empty() ? "answered" : ("question " + question).c_str());
            fclose(s_trace);
            s_trace = NULL;
        }
        if (!question.empty())
        {
            fprintf(table, "%s\tquestion\t0\t0\t%s\n", source.number.c_str(), question.c_str());
            continue;
        }
        std::ofstream(out + "/" + source.number + ".sass", std::ios::binary) << kernel.text;
        unsigned int count = 0u;
        for (size_t at = 0u; at < kernel.text.size(); at += 1u)
        {
            const size_t end = kernel.text.find('\n', at);
            const std::string line = stick_squeeze(kernel.text.substr(at, end - at));
            count += (!line.empty() && (line.back() != ':')) ? 1u : 0u;
            at = end;
        }
        std::vector<unsigned char> code((size_t)count * 16u);
        if (sass_assemble_lines(&machine, kernel.text.c_str(), SASS_CONTROL_SAFE, code.data(), code.size()) != count)
        {
            fprintf(table, "%s\tquestion\t%u\t%u\tthe assembler refuses a line of the kernel\n",
                    source.number.c_str(), kernel.forms, count);
            continue;
        }
        std::ofstream(out + "/" + source.number + ".bin", std::ios::binary)
            .write((const char *)code.data(), (std::streamsize)code.size());
        // the count of registers the kernel names, then each register it holds and the lines that claim and release
        // it
        std::ofstream(out + "/" + source.number + ".registers", std::ios::binary) << kernel.registers_held << "\n"
                                                                                  << kernel.held_lines;
        fprintf(table, "%s\tanswered\t%u\t%u\t\n", source.number.c_str(), kernel.forms, count);
        answered += 1u;
    }
    fclose(table);
    fprintf(stderr,
            "measuring stick through cu.krs and sass.krs: %zu kernels, %u answered, %zu put a question; %zu folds "
            "found on the compile channel, %zu held in sass.ksc\n",
            sources.size(), answered, sources.size() - answered, found.size(), sass->folds.size());
    return 0;
}
