// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms.cu: what a form's text says: its tokens, its launches, conditions and instructions, and what its opcode controls, compares, operates on or commutes
#include "identity_internal.h"

// The way an ask enters the part, its vector: the registers it declares, the lanes it is put over, a case a lane, and
// the operands of a case its code loads. An ask is put at the least magnitude, the registers its chain names as
// `mark`, the engine's high water mark. Where the part refuses it there and answers it declaring every register its
// file holds, the fewest it answers at is walked between the two, and the ask stands at the fewest; where the part
// refuses it at both, the refusal is not the registers', and it stands refused. 1 where the part answers it alike with
// the host on every case it computes, 0 otherwise, its registers in `question`
int vector_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                        const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks,
                        unsigned int mark)
{
    const unsigned int most = 255u;
    question->registers = ((mark != 0u) && (mark < most)) ? mark : most;
    const int least_alike = question_alike(code, host, places, question, asks);
    if ((question->outcome == RUN_ANSWERED) || (question->registers == most))
    {
        return least_alike;
    }
    const unsigned int least = question->registers;
    question->registers = most;
    const int most_alike = question_alike(code, host, places, question, asks);
    if (question->outcome != RUN_ANSWERED)
    {
        return most_alike;
    }
    const auto answers_declaring = [&](long long count) -> int {
        question->registers = (unsigned int)count;
        question_alike(code, host, places, question, asks);
        return (question->outcome == RUN_ANSWERED) ? 1 : 0;
    };
    // the ask at the fewest is in R once the walk has put it, and is read from there
    question->registers = (unsigned int)walk_halved(answers_declaring, (long long)most, (long long)least);
    return question_alike(code, host, places, question, asks);
}

// The carrier `text`, its links `links`, cut at the form standing at link `link_at` over `form_lines` links, the value
// of `read` stored in place of the carrier's answer: the carrier's links before the form, `standing` in the form's
// place where it is given, the register the store reads written with the value, a flag's as 1 where it holds, the
// carrier's own links after the form, those no operand of a case reaches, and then its store and what follows. With no
// form standing, `read` is a value the form reads; with one, a value it writes. Empty where a link of the carrier's own
// after the form names the stored register, or `read` is a uniform register
std::string carrier_cut_at_read(const std::vector<CarrierLink> &links, long cases_defined,
                                       const std::string &text, long link_at, long form_lines, const std::string &read,
                                       const std::string &standing, int wide)
{
    const std::vector<std::string> lines = carrier_pieces(text, '\n');
    long stored_at = -1;
    for (size_t at = 0u; at < lines.size(); at += 1u)
    {
        stored_at = (lines[at].find("STG") != std::string::npos) ? (long)at : stored_at;
    }
    if ((stored_at < (link_at + form_lines)) || read.empty() || (read[0] == 'U'))
    {
        return std::string();
    }
    const CarrierLink store = carrier_link_read(lines[(size_t)stored_at]);
    if (store.read.empty())
    {
        return std::string();
    }
    const std::string stored = store.read.back();
    std::string cut;
    for (long at = 0; at < link_at; at += 1)
    {
        cut += lines[(size_t)at] + "\n";
    }
    cut += standing.empty() ? std::string() : (standing + ((standing.back() == '\n') ? "" : "\n"));
    cut += (read[0] == 'P') ? ("\tSEL \t" + stored + ", RZ, 1, !" + read + ";\n")
                            : ("\tMOV \t" + stored + ", " + read + ";\n");
    cut += "\tMOV \t" + stored + ".hi, " + (wide ? (read + ".hi") : std::string("RZ")) + ";\n";
    for (long at = link_at + form_lines; at < stored_at; at += 1)
    {
        if (carrier_form_carries(links, cases_defined, lines[(size_t)at], at))
        {
            continue;
        }
        // the stored register's high word written by a link of the carrier's own is the store's width, and the cut
        // writes it already
        const CarrierLink own = carrier_link_read(lines[(size_t)at]);
        const std::vector<std::string> named = carrier_registers(lines[(size_t)at], 1);
        if ((own.written == stored) && !named.empty() && (named[0] == (stored + ".hi")))
        {
            continue;
        }
        if ((own.written == stored) || (std::find(own.read.begin(), own.read.end(), stored) != own.read.end()))
        {
            return std::string();
        }
        cut += lines[(size_t)at] + "\n";
    }
    for (size_t at = (size_t)stored_at; at < lines.size(); at += 1u)
    {
        cut += lines[at] + "\n";
    }
    return cut;
}

// The shape of what the form `form_text` at link `at` of a carrier, its links `links` and its lines `lines`, reads: the
// form and the link that writes each register it reads, their registers struck out. Two links of one shape read the
// cases alike, and hold the same rows of a product
std::string read_shape(const std::vector<CarrierLink> &links, const std::vector<std::string> &lines,
                              long cases_defined, const std::string &form_text, long at)
{
    static const std::regex s_register("\\b(U?R[0-9]+|P[0-9]+)\\b");
    std::string shape = std::regex_replace(form_text, s_register, "R");
    for (const std::string &read : carrier_form_reads(links, cases_defined, form_text, at))
    {
        const std::string base = read.substr(0u, read.find('.'));
        const long defined = (read[0] == '[') ? -1 : carrier_definition(links, base, at);
        shape += "|" + ((defined < 0) ? read : std::regex_replace(lines[(size_t)defined], s_register, "R"));
    }
    return shape;
}

// A form's text, each parameter a marker, cut into its tokens: a marker one token with the part of a pair it names,
// `.hi`, a run of letters, digits, `_` and `.` one, and every other mark one, the space between passed over
std::vector<std::string> form_tokens(const std::string &text)
{
    std::vector<std::string> tokens;
    for (size_t at = 0u; at < text.size();)
    {
        const char letter = text[at];
        if ((letter == '\x01') && ((at + 2u) < text.size()))
        {
            size_t end = at + 3u;
            if ((end < text.size()) && (text[end] == '.'))
            {
                end += 1u;
                while ((end < text.size()) && (isalnum((unsigned char)text[end]) || (text[end] == '_')))
                {
                    end += 1u;
                }
            }
            tokens.push_back(text.substr(at, end - at));
            at = end;
            continue;
        }
        if (isspace((unsigned char)letter))
        {
            at += 1u;
            continue;
        }
        size_t end = at + 1u;
        if (isalnum((unsigned char)letter) || (letter == '_') || (letter == '.'))
        {
            while ((end < text.size()) &&
                   (isalnum((unsigned char)text[end]) || (text[end] == '_') || (text[end] == '.')))
            {
                end += 1u;
            }
        }
        tokens.push_back(text.substr(at, end - at));
        at = end;
    }
    return tokens;
}

// The form `to_text` read against the form `from_text`, token by token, each parameter a marker: each parameter of
// `to` bound in `bound` to what `from` writes in its place, a marker of one of `from`'s parameters or a literal, and
// each parameter of `from` that `to` writes a literal in place of bound in `fixed` to that literal. A marker naming a
// part of a pair stands against the same part of a marker, and against the fixed zero, whose every part is zero. 1
// where every other token is the same: `to` is `from` with the parameters `fixed` names bound, one structure
int form_unified(const Ruleset *sass, const std::string &to_text, const std::string &from_text,
                 std::map<char, std::string> *bound, std::map<char, std::string> *fixed)
{
    const std::vector<std::string> to_tokens = form_tokens(to_text);
    const std::vector<std::string> from_tokens = form_tokens(from_text);
    const std::string zero = ruleset_physreg(sass, "zero");
    bound->clear();
    fixed->clear();
    if (to_tokens.empty() || (to_tokens.size() != from_tokens.size()))
    {
        return 0;
    }
    for (size_t at = 0u; at < to_tokens.size(); at += 1u)
    {
        const std::string &to_token = to_tokens[at];
        const std::string &from_token = from_tokens[at];
        const int to_marker = (to_token.size() >= 3u) && (to_token[0] == '\x01');
        const int from_marker = (from_token.size() >= 3u) && (from_token[0] == '\x01');
        const std::string to_part = to_marker ? to_token.substr(3u) : std::string();
        const std::string from_part = from_marker ? from_token.substr(3u) : std::string();
        if (to_marker)
        {
            // a part of a pair stands against the same part of another, and a whole against a marker or a literal
            const std::string standing = from_marker ? from_token.substr(0u, 3u) : from_token;
            const int parts_stand = from_marker ? (to_part == from_part) : (to_part.empty() || (from_token == zero));
            if (!parts_stand || ((bound->count(to_token[1]) != 0u) && ((*bound)[to_token[1]] != standing)))
            {
                return 0;
            }
            (*bound)[to_token[1]] = standing;
            continue;
        }
        if (from_marker)
        {
            if ((!from_part.empty() && (to_token != zero)) ||
                ((fixed->count(from_token[1]) != 0u) && ((*fixed)[from_token[1]] != to_token)))
            {
                return 0;
            }
            (*fixed)[from_token[1]] = to_token;
            continue;
        }
        if (to_token != from_token)
        {
            return 0;
        }
    }
    return 1;
}

// 1 where the form `text` reads what the launch gives: a thread's or a block's number from a special register, or a
// block's or a grid's extent from the constant bank below the parameters
int form_launched(const std::string &text)
{
    static const std::regex s_launch("SR_(TID|CTAID|NTID|NCTAID)|c\\[0x0\\]\\[0x(0|4|8|c|10|14)\\]");
    return std::regex_search(text, s_launch) ? 1 : 0;
}

// the conditions the comparisons of the form `text` test, each the word after a `SETP`'s first `.`
std::set<std::string> form_conditions(const std::string &text)
{
    static const std::regex s_condition("[A-Z]*SETP\\.([A-Z]+)");
    std::set<std::string> conditions;
    for (std::sregex_iterator found(text.begin(), text.end(), s_condition), end; found != end; ++found)
    {
        conditions.insert((*found)[1].str());
    }
    return conditions;
}

// each instruction of the form `text`, its opcode with its modifiers first and then its operands, a label and a guard
// before it passed over
static std::vector<std::vector<std::string>> form_instructions(const std::string &text)
{
    std::vector<std::vector<std::string>> instructions;
    for (const std::string &line : carrier_pieces(text, ';'))
    {
        std::stringstream words(carrier_trimmed(line));
        std::string opcode;
        words >> opcode;
        while (!opcode.empty() && ((opcode.back() == ':') || (opcode[0] == '@')))
        {
            opcode.clear();
            words >> opcode;
        }
        if (opcode.empty() || !isupper((unsigned char)opcode[0]))
        {
            continue;
        }
        std::string rest;
        std::getline(words, rest);
        std::vector<std::string> instruction{opcode};
        for (const std::string &operand : carrier_pieces(rest, ','))
        {
            instruction.push_back(carrier_trimmed(operand));
        }
        instructions.push_back(instruction);
    }
    return instructions;
}

// the operation `opcode` names, its word before the first `.`
static std::string opcode_stem(const std::string &opcode)
{
    return opcode.substr(0u, opcode.find('.'));
}

// 1 where `opcode` decides which instruction runs next: an exit, a return, a branch, a call or a convergence
static int opcode_controls(const std::string &opcode)
{
    static const std::regex s_control("(EXIT|RET|BRA|BRX|JMP|JMX|CALL|BREAK|BSSY|BSYNC)");
    return std::regex_match(opcode_stem(opcode), s_control) ? 1 : 0;
}

// 1 where `opcode` tests its operands and sets a flag
static int opcode_compares(const std::string &opcode)
{
    static const std::regex s_compare("U?[A-Z]?SETP2?");
    return std::regex_match(opcode_stem(opcode), s_compare) ? 1 : 0;
}

// 1 where `opcode` computes a value from its operands at any width, a bit, a word or many words: an add, a multiply,
// a shift, a bitwise or a floating operation. `IMAD.MOV` moves a value and computes nothing
static int opcode_operates(const std::string &opcode)
{
    static const std::regex s_operation("U?(IADD3|IADD|IMAD|IMUL|IMNMX|IABS|ISCADD|LEA|LOP3|LOP|SHF|SHL|SHR|POPC|FLO|"
                                        "BREV|BMSK|SGXT|PRMT|BFE|BFI|IDP|VABSDIFF|FADD|FMUL|FFMA|FMNMX|DADD|DMUL|DFMA|"
                                        "HADD2|HMUL2|HFMA2|MUFU)");
    return std::regex_match(opcode_stem(opcode), s_operation) && (opcode.find(".MOV") == std::string::npos);
}

// 1 where `instruction` gives one value with its first two sources taken either way round: an add, a multiply, a
// least or a most, a bitwise table that reads its first two inputs alike, or an equality
static int instruction_commutes(const std::vector<std::string> &instruction)
{
    static const std::regex s_commutes("U?(IADD3|IMAD|IMUL|IMNMX|VABSDIFF|FADD|FMUL|FFMA|FMNMX|DADD|DMUL|DFMA|HADD2|HMUL2|"
                                       "HFMA2)");
    const std::string &opcode = instruction[0];
    const std::string stem = opcode_stem(opcode);
    if (opcode_compares(opcode))
    {
        const size_t dot = opcode.find('.');
        const std::string condition = (dot == std::string::npos) ? std::string()
                                                                 : opcode.substr(dot + 1u, opcode.find('.', dot + 1u) -
                                                                                               dot - 1u);
        return (condition == "EQ") || (condition == "NE");
    }
    if (std::regex_match(stem, std::regex("U?LOP3")))
    {
        // the table's bit at `a`, `b`, `c` is read off its place `4a + 2b + c`: the first two inputs read alike where
        // the bits at `a` = 1, `b` = 0 and at `a` = 0, `b` = 1 agree for each `c`
        if ((instruction.size() < 6u) || (instruction[5].rfind("0x", 0u) != 0u))
        {
            return 0;
        }
        const unsigned long table = std::stoul(instruction[5], nullptr, 16);
        return (((table >> 4u) & 1u) == ((table >> 2u) & 1u)) && (((table >> 5u) & 1u) == ((table >> 3u) & 1u));
    }
    if (std::regex_match(stem, std::regex("U?LOP")))
    {
        return (opcode.find(".AND") != std::string::npos) || (opcode.find(".OR") != std::string::npos) ||
               (opcode.find(".XOR") != std::string::npos);
    }
    return std::regex_match(stem, s_commutes) && (opcode.find(".MOV") == std::string::npos);
}

// 1 where the form `text` decides which form runs next: an exit, a return, a branch, a call or a convergence
int form_controls(const std::string &text)
{
    int controls = 0;
    for (const std::vector<std::string> &instruction : form_instructions(text))
    {
        controls |= opcode_controls(instruction[0]);
    }
    return controls;
}

// 1 where the form `text` reads or writes memory through an address. A bank of constants, `c[...]`, is read at a place
// no case moves, and is no address
int form_addresses(const std::string &text)
{
    for (const std::vector<std::string> &instruction : form_instructions(text))
    {
        for (size_t at = 1u; at < instruction.size(); at += 1u)
        {
            const size_t bracket = instruction[at].find('[');
            if ((bracket != std::string::npos) && ((bracket == 0u) || (instruction[at][bracket - 1u] != 'c')))
            {
                return 1;
            }
        }
    }
    return 0;
}

// 1 where the form `text` reads or writes memory through an address, or sends control elsewhere: a stand-in of it is
// carried in a process of its own
int form_dangerous(const std::string &text)
{
    return form_controls(text) || form_addresses(text);
}

// 1 where the form `text` computes a value from its operands
int form_operates(const std::string &text)
{
    int operates = 0;
    for (const std::vector<std::string> &instruction : form_instructions(text))
    {
        operates |= opcode_operates(instruction[0]);
    }
    return operates;
}

// 1 where every instruction of the form `text` gives one value with its first two sources taken either way round
int form_commutes(const std::string &text)
{
    const std::vector<std::vector<std::string>> instructions = form_instructions(text);
    int commutes = !instructions.empty();
    for (const std::vector<std::string> &instruction : instructions)
    {
        commutes &= instruction_commutes(instruction);
    }
    return commutes;
}

// 1 where the form `text` does a thing that computes no value, tests nothing and sends control nowhere: a load, a
// store, a move, a copy or a read of a special register
int form_verbs(const std::string &text)
{
    int verbs = 0;
    for (const std::vector<std::string> &instruction : form_instructions(text))
    {
        const std::string &opcode = instruction[0];
        verbs |= (!opcode_operates(opcode) && !opcode_compares(opcode) && !opcode_controls(opcode)) ? 1 : 0;
    }
    return verbs;
}

// 1 where one of the forms `first` and `second` is the other loaded const: the same operands, its opcode the other's
// with `.CONSTANT` after it. Whether a load may be const is read off its chain, a store to the memory it reads, and
// the part answers the two alike at every link of a chain that holds it
int form_const_loaded(const std::string &first, const std::string &second)
{
    const std::vector<std::string> first_tokens = form_tokens(first);
    const std::vector<std::string> second_tokens = form_tokens(second);
    if (first_tokens.empty() || (first_tokens.size() != second_tokens.size()) ||
        !std::equal(first_tokens.begin() + 1, first_tokens.end(), second_tokens.begin() + 1))
    {
        return 0;
    }
    return ((first_tokens[0] + ".CONSTANT") == second_tokens[0]) ||
           ((second_tokens[0] + ".CONSTANT") == first_tokens[0]);
}

// 1 where the form `text` holds no instruction, each of which ends at a `;`: a directive or a label alone, placing
// the forms a case reads
int form_switches(const std::string &text)
{
    const std::vector<std::string> tokens = form_tokens(text);
    return !tokens.empty() && (std::find(tokens.begin(), tokens.end(), ";") == tokens.end());
}

// 1 where a parameter of the form `text` is named at its `.hi`: the form works a wide value in its pieces, the low
// word and the high
int form_ranged(const std::string &text)
{
    for (const std::string &token : form_tokens(text))
    {
        if ((token.size() > 3u) && (token[0] == '\x01') && (token.compare(3u, std::string::npos, ".hi") == 0))
        {
            return 1;
        }
    }
    return 0;
}
