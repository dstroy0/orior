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
#include "../../../../../../src/c/transpiler/cubin/sass_assemble.h"
#include "../../../../../../src/c/types/file_defs/krs/sass_machine.h"
#include "../../../c/transpiler/interface/interface_sass_probe.h"
}

#include "cu_target.h"
#include "machine_ir_types.h"
#include "ruleset_core_words.h"
#include "ruleset_reader.h"
#include "sass_target.h"
#include "target.h"

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

// the width a value held in a predicate is given
#define STICK_PREDICATE_BITS 1u

// the schema's forms by their place in it, as machine_ir_types.h names them
#define STICK_FORM_NAME(name_, text_, parameters_) text_,

static const char *const s_form_names[OPCODE_COUNT] = {OPCODES(STICK_FORM_NAME)};

// the integer types of C a kernel's values are read in, each its width and whether it is signed
static const struct
{
    const char *spelling;
    unsigned int bits;
    int is_signed;
} s_types[] = {
    {"signed char", 8u, 1},  {"unsigned char", 8u, 0},  {"short", 16u, 1},     {"unsigned short", 16u, 0},
    {"int", 32u, 1},         {"unsigned int", 32u, 0},  {"long long", 64u, 1}, {"unsigned long long", 64u, 0},
};

// A form cu.krs writes: its place, its text cut at its parameters with white space taken out, and the width its casts
// give the value. An expression's form is `{to} = <text>;`, its pieces the text after `=` cut at the parameters past
// to; a statement's is its whole text cut at every parameter
struct StickPattern
{
    unsigned int form;
    std::vector<std::string> pieces;
    unsigned int bits;
    int statement;
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
// of its parameters; and, where it is folded, the word it stands as
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
};

// a register the kernel holds a value in, as sass.krs writes it, or the word a folded value stands as; its width and
// whether its type is signed
struct StickValue
{
    std::string name;
    unsigned int bits;
    int is_signed;
};

// a parameter of the kernel: its offset in the parameter bank and its width
struct StickParameter
{
    unsigned int offset;
    unsigned int bits;
};

// what a round asks of nvcc's listing about one kernel: each form that moved a word of the constant bank, and the
// word; each multiply by a power of two, its parameter and the log2; and each load's offset into the elements
struct StickAsked
{
    std::vector<std::pair<unsigned int, std::string>> moved;
    std::vector<std::pair<std::pair<unsigned int, unsigned int>, unsigned int>> multiplied;
    std::vector<unsigned int> offsets;
    std::vector<unsigned int> copied;
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
    const std::regex word("^c\\[([0-9a-fA-Fx]+)\\]\\[([0-9a-fA-Fx+]+)\\]$");
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

// every form cu.krs gives as an expression or as a statement of its own, each with a letter of its own text
static std::vector<StickPattern> stick_patterns(const Ruleset *rules)
{
    std::vector<StickPattern> patterns;
    for (unsigned int form = 0u; form < OPCODE_COUNT; form += 1u)
    {
        const InstrTemplate &text = rules->forms[form];
        if ((rules->form_given[form] != (unsigned char)RULESET_CORE_GIVEN) || text.slots.empty())
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
        // a text of nothing but parameters reads nothing of its own, and would read an expression as itself; an
        // expression's text is one statement
        int lettered = 0;
        unsigned int bits = 0u;
        for (const std::string &piece : pieces)
        {
            lettered = piece.empty() ? lettered : 1;
            lettered = ((expression != 0) && ((piece.find(';') != std::string::npos) ||
                                              (piece.find('{') != std::string::npos)))
                           ? -1
                           : lettered;
            bits = ((piece.find("(unsignedlonglong)") != std::string::npos) ||
                    (piece.find("(longlong)") != std::string::npos))
                       ? 64u
                       : bits;
            if (lettered < 0)
            {
                break;
            }
        }
        if (lettered > 0)
        {
            patterns.push_back({form, pieces, bits, (expression != 0) ? 0 : 1});
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

// the shift of a multiply's stem: its name with multiply written shift_left, or OPCODE_COUNT where the schema has none
static unsigned int stick_shift_of(unsigned int form)
{
    std::string name = s_form_names[form];
    const size_t at = name.find("multiply");
    return (at == std::string::npos) ? OPCODE_COUNT : stick_form(name.replace(at, 8u, "shift_left"));
}

// the operation sass.krs's text for `form` opens with, its operands stand-ins
static std::string stick_operation(const Ruleset *sass, unsigned int form)
{
    std::vector<std::string> arguments(sass->schema->forms[form].parameters, std::string("R2"));
    std::string text;
    const auto none = [](const std::string &) { return std::string(); };
    if (ruleset_opcode(sass, s_form_names[form], arguments, none, text) == 0)
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

// a kernel read and written: the rulesets, the machine file a form is gated on, whether it folds, the values and
// parameters it holds, the text written so far, and what it asks of nvcc's listing
class StickKernel
{
  public:
    StickKernel(const Ruleset *cu, const Ruleset *sass, const SassMachine *machine,
                const std::vector<StickPattern> *patterns, int folding)
        : forms(0u), cu(cu), sass(sass), machine(machine), patterns(patterns), folding(folding), next(2u),
          predicates(0u)
    {
    }

    std::map<std::string, StickParameter> parameters;
    std::string text;
    std::string question;
    unsigned int forms;
    StickAsked asked;

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
        if ((place == OPCODE_COUNT) || (choose(place, outputs, spans, 32u, 0, &best) == 0))
        {
            ask("sass.krs's " + name + " assembles with no reading of its operands");
            return 0;
        }
        std::vector<std::string> arguments = outputs;
        for (const StickReading &child : best.children)
        {
            arguments.push_back(written(child).name);
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

    // `name` held in `value` with the signedness of its type
    void hold(const std::string &name, StickValue value, int is_signed)
    {
        value.is_signed = is_signed;
        held_values[name] = value;
    }

    // 1 where `name` is held
    int holds(const std::string &name) const
    {
        return held_values.find(name) != held_values.end();
    }

    // a fresh register of `bits`, a wide one an even pair and a predicate's one of the predicates
    StickValue fresh(unsigned int bits)
    {
        if (bits == STICK_PREDICATE_BITS)
        {
            predicates += 1u;
            return StickValue{ruleset_register(sass, "predicate", predicates - 1u), bits, 0};
        }
        next += ((bits == 64u) && ((next % 2u) != 0u)) ? 1u : 0u;
        const StickValue value = {ruleset_register(sass, (bits == 64u) ? "wide" : "temporary", next), bits, 0};
        next += (bits == 64u) ? 2u : 1u;
        return value;
    }

    // form `name` written in sass.krs with `arguments`, a move of a register into itself left out as the nothing it
    // does; 0 where it is not, `question` then saying why
    int write(const std::string &name, const std::vector<std::string> &arguments)
    {
        const auto none = [](const std::string &) { return std::string(); };
        std::string lines;
        if (ruleset_opcode(sass, name, arguments, none, lines) == 0)
        {
            ask("sass.krs writes no " + name);
            return 0;
        }
        const std::regex itself("^\\s*MOV\\s+(\\S+)\\s*,\\s*(\\S+)\\s*;\\s*$");
        std::smatch found;
        size_t at = 0u;
        while (at < lines.size())
        {
            const size_t end = lines.find('\n', at);
            const std::string line = lines.substr(at, (end == std::string::npos) ? std::string::npos : (end - at));
            if (!std::regex_match(line, found, itself) || (found[1] != found[2]))
            {
                text += line + "\n";
            }
            at = (end == std::string::npos) ? lines.size() : (end + 1u);
        }
        forms += 1u;
        return 1;
    }

    // `why` kept as the kernel's question where it has none yet
    void ask(const std::string &why)
    {
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

    // The word of the constant bank form `form` moves into the register put for its first parameter, where its
    // sass.krs text, its operands `arguments`, is nothing but such moves: the first move's word. Empty otherwise
    std::string moves(unsigned int form, const std::vector<std::string> &arguments) const
    {
        std::vector<std::string> given = {"\x01"};
        given.insert(given.end(), arguments.begin(), arguments.end());
        std::string lines;
        const auto none = [](const std::string &) { return std::string(); };
        if (ruleset_opcode(sass, s_form_names[form], given, none, lines) == 0)
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
        const auto none = [](const std::string &) { return std::string(); };
        if ((sass->schema->forms[form].parameters != 2u) ||
            (ruleset_opcode(sass, s_form_names[form], {"\x01", "\x02"}, none, lines) == 0))
        {
            return 0;
        }
        return std::regex_search(lines, std::regex("(^|\\n)\\s*MOV\\s+\x01\\s*,\\s*\x02\\s*;"));
    }

    // the number of the register sass.krs's bank of temporaries writes as `name`, or all ones where it writes none
    unsigned int register_number(const std::string &name) const
    {
        const InstrTemplate &bank = sass->banks[REGCLASS_TEMPORARY];
        const std::string before = bank.pieces[0];
        const std::string after = bank.pieces.back();
        if ((name.size() <= (before.size() + after.size())) || (name.compare(0u, before.size(), before) != 0) ||
            (name.compare(name.size() - after.size(), after.size(), after) != 0))
        {
            return 0xFFFFFFFFu;
        }
        const std::string digits = name.substr(before.size(), name.size() - before.size() - after.size());
        char *end = NULL;
        const unsigned long number = strtoul(digits.c_str(), &end, 10);
        return ((end == NULL) || (*end != '\0') || digits.empty()) ? 0xFFFFFFFFu : (unsigned int)number;
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
                arguments.push_back("P" + std::to_string(predicate));
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
        std::vector<std::string> arguments = outputs;
        const std::vector<std::string> operands = stand_ins(children, 8u);
        arguments.insert(arguments.end(), operands.begin(), operands.end());
        std::string lines;
        const auto none = [](const std::string &) { return std::string(); };
        if (ruleset_opcode(sass, s_form_names[form], arguments, none, lines) == 0)
        {
            return 0;
        }
        unsigned int count = 0u;
        for (const char letter : lines)
        {
            count += (letter == '\n') ? 1u : 0u;
        }
        unsigned char code[256];
        return (count <= 16u) &&
               (sass_assemble_lines(machine, lines.c_str(), SASS_CONTROL_SAFE, code, sizeof(code)) == count);
    }

    // Of form `form` over `spans`, its outputs `outputs`, the reading of fewest forms whose text assembles, kept in
    // `best` where it has fewer forms than `best` holds: each span read in place and in a register, every choice tried.
    // An expression's form is given no outputs, and is gated with a register of its value's width put for its to, or
    // a predicate where the text takes no register there
    int choose(unsigned int form, const std::vector<std::string> &outputs, const std::vector<std::string> &spans,
               unsigned int bits_given, int expression, StickReading *best)
    {
        std::vector<StickReading> in_place(spans.size());
        std::vector<StickReading> in_register(spans.size());
        for (size_t at = 0u; at < spans.size(); at += 1u)
        {
            if ((read(stick_bare(spans[at]), 1, &in_place[at]) == 0) ||
                (read(stick_bare(spans[at]), 0, &in_register[at]) == 0))
            {
                return 0;
            }
        }
        int chosen = 0;
        const size_t choices = (size_t)1u << spans.size();
        for (size_t choice = 0u; choice < choices; choice += 1u)
        {
            StickReading reading = {STICK_FORM, form, "", 0u, 1u, 32u, {}, ""};
            unsigned int bits = 32u;
            for (size_t at = 0u; at < spans.size(); at += 1u)
            {
                const StickReading &child = (((choice >> at) & 1u) != 0u) ? in_register[at] : in_place[at];
                reading.children.push_back(child);
                reading.cost += child.cost;
                bits = ((child.bits > bits) && (child.bits != STICK_PREDICATE_BITS)) ? child.bits : bits;
            }
            reading.bits = (bits_given != 0u) ? bits_given : bits;
            if (reading.cost >= best->cost)
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
            }
            else if (assembles(form, {"P0"}, reading.children) != 0)
            {
                reading.bits = STICK_PREDICATE_BITS;
                *best = reading;
                chosen = 1;
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
    int read(const std::string &bare, int in_place, StickReading *best)
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
            const unsigned int load = stick_form((parameter->second.bits == 64u) ? "parameter_load_wide"
                                                                                 : "parameter_load");
            *best = StickReading{STICK_PARAMETER, load, bare, 0u, 1u, parameter->second.bits, {}, ""};
            best->children.push_back(StickReading{STICK_NUMBER, 0u, "", parameter->second.offset, 0u, 32u, {}, ""});
            fold(best, in_place);
            return 1;
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
                choose(pattern.form, std::vector<std::string>(), spans, pattern.bits, 1, best);
                // a power of two the system folds is the shift of the multiply's stem by its log2
                const unsigned int shift = stick_shift_of(pattern.form);
                for (unsigned int at = 0u; (spans.size() == 2u) && (shift != OPCODE_COUNT) && (at < 2u); at += 1u)
                {
                    unsigned int power = 0u;
                    if ((folded(pattern.form, "pow2", at + 1u) != 0) && (number_of(spans[at], &power) != 0) &&
                        (stick_log2(power) != 0u))
                    {
                        choose(shift, std::vector<std::string>(), {spans[1u - at], number_text(stick_log2(power))},
                               pattern.bits, 1, best);
                    }
                }
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
            return value;
        }
        std::vector<std::string> operands;
        for (size_t at = 0u; at < reading.children.size(); at += 1u)
        {
            const StickReading &child = reading.children[at];
            operands.push_back(written(child).name);
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
        // a wide made of a word whose move the system folds is the pair whose low register the word is already in,
        // where that register begins a pair and the one above it is free
        const int copies = (reading.bits == 64u) && (operands.size() == 1u) && (copied(reading.form) != 0);
        if (copies != 0)
        {
            asked.copied.push_back(reading.form);
        }
        StickValue value;
        const unsigned int low = register_number(operands.empty() ? std::string() : operands[0]);
        if ((copies != 0) && (folded(reading.form, "register", 1u) != 0) && (low != 0xFFFFFFFFu) &&
            ((low % 2u) == 0u) && ((low + 1u) == next))
        {
            value = StickValue{ruleset_register(sass, "wide", low), 64u, 0};
            next = low + 2u;
        }
        else
        {
            value = fresh(reading.bits);
        }
        std::vector<std::string> arguments = {value.name};
        arguments.insert(arguments.end(), operands.begin(), operands.end());
        write(s_form_names[reading.form], arguments);
        held_values[reading.text] = value;
        return value;
    }
};

// the integer type `text` names, or -1
static int stick_type(const std::string &text)
{
    for (int type = 0; type < (int)(sizeof(s_types) / sizeof(s_types[0])); type += 1)
    {
        if (text == s_types[type].spelling)
        {
            return type;
        }
    }
    return -1;
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
        kernel->parameters[found[3]] = StickParameter{offset, bits};
        offset += bits / 8u;
        at = (comma == std::string::npos) ? (source.parameters.size() + 1u) : (comma + 1u);
    }
    kernel->write("kernel_open", {});
    const std::regex operand("^const (.+) ([A-Za-z_][A-Za-z_0-9]*) = \\((.+)\\)in\\[(.+)\\];$");
    const std::regex declared("^const (.+) ([A-Za-z_][A-Za-z_0-9]*) = (.+);$");
    const std::regex stored("^out\\[(.+)\\] = \\(unsigned long long\\)([A-Za-z_][A-Za-z_0-9]*);$");
    const unsigned int load = stick_form("global_load_word");
    std::map<std::string, int> types;
    for (const std::string &statement : source.statements)
    {
        if (std::regex_match(statement, found, operand))
        {
            const int type = stick_type(found[1]);
            if ((type < 0) || (found[1] != found[3]) || (s_types[type].bits != 32u))
            {
                return "a value of type " + std::string(found[1]);
            }
            // in[i + k], k a number, is k elements past the address of in[i] where the system folds the offset
            std::string index = stick_bare(found[4]);
            unsigned int elements = 0u;
            std::vector<std::string> spans;
            if (kernel->reads_as("add_alone", index, &spans) && kernel->number_of(spans[1], &elements))
            {
                if (kernel->folded(load, "offset", 1u) != 0)
                {
                    index = spans[0];
                }
                else if (elements != 0u)
                {
                    kernel->asked.offsets.push_back(8u * elements);
                }
            }
            const unsigned int element_offset = (index == stick_bare(found[4])) ? 0u : (8u * elements);
            const std::string address_of = "&in[" + index + "]";
            if (kernel->holds(address_of) == 0)
            {
                const StickValue address = kernel->fresh(64u);
                if (kernel->form("wide_multiply_word_add", {address.name}, {index, kernel->number_text(8u), "in"}) ==
                    0)
                {
                    return kernel->question;
                }
                kernel->hold(address_of, address, 0);
            }
            const StickValue loaded = kernel->fresh(32u);
            if (kernel->form("global_load_word", {loaded.name}, {address_of, kernel->number_text(element_offset)}) ==
                0)
            {
                return kernel->question;
            }
            kernel->hold(stick_bare(found[2]), loaded, s_types[type].is_signed);
            types[found[2]] = type;
            continue;
        }
        if (std::regex_match(statement, found, stored))
        {
            const auto type = types.find(found[2]);
            if (type == types.end())
            {
                return "the store reads a value it does not hold: " + statement;
            }
            // C converts a signed word to unsigned long long as its value, through long long
            const std::string converted = (s_types[type->second].is_signed != 0)
                                              ? ("(unsigned long long)(long long)(int)" + std::string(found[2]))
                                              : ("(unsigned long long)" + std::string(found[2]));
            if (kernel->value(converted).name.empty())
            {
                return kernel->question;
            }
            const std::string address_of = "&out[" + stick_bare(found[1]) + "]";
            const StickValue address = kernel->fresh(64u);
            if (kernel->form("wide_multiply_word_add", {address.name}, {found[1], kernel->number_text(8u), "out"}) == 0)
            {
                return kernel->question;
            }
            kernel->hold(address_of, address, 0);
            if (kernel->form("global_store_wide", {}, {address_of, kernel->number_text(0u), converted}) == 0)
            {
                return kernel->question;
            }
            continue;
        }
        if (std::regex_match(statement, found, declared))
        {
            const int type = stick_type(found[1]);
            if ((type < 0) || (s_types[type].bits != 32u))
            {
                return "a value of type " + std::string(found[1]);
            }
            const StickValue value = kernel->value(found[3]);
            if (value.name.empty())
            {
                return kernel->question;
            }
            kernel->hold(stick_bare(found[2]), value, s_types[type].is_signed);
            types[found[2]] = type;
            continue;
        }
        if (kernel->statement(stick_bare(statement)) != 0)
        {
            continue;
        }
        return kernel->question.empty() ? ("a statement nothing here reads: " + statement) : kernel->question;
    }
    kernel->write("kernel_close", {});
    return kernel->question;
}

// One round of the stick, folding by what sass.ksc holds where `folding` is set: each kernel read and held against
// nvcc's listing of it, each question it asks added to `openings` and each fold nvcc's listing shows to `found`
static void stick_round(const std::vector<StickSource> &sources,
                        const std::map<std::string, std::vector<std::string>> &listing, const Ruleset *cu,
                        const Ruleset *sass, const SassMachine *machine, const std::vector<StickPattern> &patterns,
                        int folding, std::set<std::string> *openings, std::set<std::string> *found)
{
    const unsigned int load = stick_form("global_load_word");
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
        // the system folds a word's move into a wide where its listing of the kernel moves no register into another
        for (const unsigned int form : kernel.asked.copied)
        {
            openings->insert(stick_fold_line(sass, form, "register", 1u, 1));
            int moves = 0;
            for (const std::string &instruction : nvcc->second)
            {
                const std::vector<std::string> operands = stick_operands(instruction);
                moves = ((instruction.compare(0u, 4u, "MOV ") == 0) && (operands.size() == 2u) &&
                         std::regex_match(operands[1], std::regex("^R[0-9]+$")))
                            ? 1
                            : moves;
            }
            if (moves == 0)
            {
                found->insert(stick_fold_line(sass, form, "register", 1u, 0));
            }
        }
        for (const unsigned int offset : kernel.asked.offsets)
        {
            openings->insert(stick_fold_line(sass, load, "offset", 1u, 1));
            if (stick_listing_holds(nvcc->second, stick_operation(sass, load), "+" + stick_hex(offset) + "]", 1) != 0)
            {
                found->insert(stick_fold_line(sass, load, "offset", 1u, 0));
            }
        }
    }
}

// sass.ksc written: what it held that opens with each of `openings` dropped, and each of `found` taken
static void stick_folds_write(const Ruleset *sass, const std::set<std::string> &openings,
                              const std::set<std::string> &found)
{
    const size_t slash = sass->path.find_last_of('/');
    const std::string rulesets = sass->path.substr(0u, slash);
    const std::string file = sass->path.substr(slash + 1u);
    const std::string stem = file.substr(0u, file.find_last_of('.'));
    sass_class_read(rulesets.c_str(), stem.c_str());
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
    if (sass_machine_read(&machine, "src/c/transpiler/cubin/machines/sm_86") == 0)
    {
        fprintf(stderr, "the machine file is not read\n");
        return 1;
    }
    const std::vector<StickPattern> patterns = stick_patterns(cu);
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
        const std::string question = stick_kernel(source, &kernel);
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
