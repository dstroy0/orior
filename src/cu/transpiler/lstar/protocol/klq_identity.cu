// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_identity.cu: the identities a language's questions carry, read off the measuring stick and written to Lstar.klq
//
//   klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder> [<ours folder>]
//   klq_identity read <folder> <Lstar.klq> <ruleset>...
//   klq_identity known <nvcc listing> <manifest> <folder> <candidate>...
//   klq_identity permute <nvcc listing> <manifest> <folder>
//   klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...
//   klq_identity stall <ours folder> <host answers> <ksc> <folder> -- <carrier>...
//   klq_identity register <ours folder> <host answers> <ksc> <folder> -- <carrier>...
//   klq_identity curve <ours folder> <host answers> <ksc> <folder> <task>... -- <carrier>...
//   klq_identity queue <ours folder> <host answers> <ksc> <folder> <manifest> -- <carrier>...
//   klq_identity text_identity <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <manifest> -- <carrier>...
//   klq_identity pair <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <machine> -- <carrier>...
//
// Two of the stick's questions whose nvcc listings are the same but for one link a side are a slice, and the two
// links are the slice's identity: each an instruction with its registers read as their kind and its literals as
// written. The rest of the two listings is the same register for register, and the two links name the same registers:
// nothing but the two links sets the questions apart. An identity is carried by every slice whose links are its own,
// and each of them is information about it. Members the same are the same identity, and the carriers of one identity
// are told apart by address, the position of each member in the chain: question by question, then link by link within
// a question. Where two carriers contend, the one whose most basal member sits first comes first, and an identity's
// address is its first carrier's.
//
// A chain reads in its grammar and holds its whole intent, and a member's place in that grammar is its context: for each
// register its link names, the nearest link before it and the nearest after it in its own chain that name the same
// register, with the slot each names it in. No link's meaning is read, only the names links share. The carriers of one
// identity are gathered by context, each context the same members in one place of the grammar, addressed by its most
// basal carrier and given a verdict of its own.
//
// A question's chain as a whole is an identity too, and so is each window of it: each held at the first address it
// occurs at, each primitive a link that first occurs where it stands, and each chain a signature of the identities it
// is read as, its primitives and its operands, with a number its contents in their order give it.
//
// slice writes <folder>/slices.txt, each slice a line, <folder>/chains.txt, the primitives and the chains, and
// <folder>/host_questions.cpp, every question whose operands and result are integers that a slice holds, that is a task
// of register pressure, or that <ours folder> holds the engine's chain for, compiled for the host as C++ with the frame
// every kernel shares. The host
// computes each over every case and writes <folder>/host_answers.txt; read then gives each identity its verdict: the
// first case the two questions of one of its slices answer apart closes it as two operations, and an identity alike on
// every case asked stays open with its count of cases. A case the host's C would trap on or leave undefined is asked of
// nothing. Each identity is written to Lstar.klq after its keys, with the forms of the given rulesets whose texts write
// its links, and with the rule a link breaks where asking the part as written would end it
#include "../interface/interface.h"
#include "carrier_flow.h"
#include "code_generator.h"
#include "concept_product.h"
#include "query_record.h"
#include "run_channel.h"
#include "target_internal.h"
extern "C"
{
#include "../../vendor_bin_layouts/nvidia/sass_assemble.h"
}

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// the values each operand is put through: zero, the small numbers, and the edges of every width
static const unsigned long long s_values[] = {
    0x0ull,          0x1ull,          0x2ull,          0x3ull,          0x7full,
    0x80ull,         0xffull,         0x100ull,        0x7fffull,       0x8000ull,
    0xffffull,       0x7fffffffull,   0x80000000ull,   0xffffffffull,   0x100000000ull,
    0x7fffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull};
#define IDENTITY_VALUES (sizeof(s_values) / sizeof(s_values[0]))
// the third operand, a test's other predicate where a question reads one
#define IDENTITY_THIRDS 2u
#define IDENTITY_CASES (IDENTITY_VALUES * IDENTITY_VALUES * IDENTITY_THIRDS)

// the operations that transfer control or wait, which cubin_safe.h's rule 2 keeps off the part: a question holding
// one is not put to the part as written
static const char *const s_control_or_wait[] = {"BRA",   "BRX",  "JMP",      "JMX",   "CALL", "RET",
                                                "EXIT",  "BSSY", "BSYNC",    "BREAK", "BMOV", "WARPSYNC",
                                                "YIELD", "BAR",  "DEPBAR",   "NANOSLEEP", "BPT", "RTT",
                                                "KILL",  "RPCMOV", "RETIRE", "PMTRIG"};

// 1 where the link, as written or read as its kind, transfers control or waits, which cubin_safe.h's rule 2 keeps off
// the part, EXIT alone excepted
static int rule_broken(const std::string &link)
{
    const size_t guarded = (link[0] == '@') ? link.find(' ') + 1u : 0u;
    const std::string operation = link.substr(guarded, link.find_first_of(". ", guarded) - guarded);
    return (operation != "EXIT") && std::any_of(std::begin(s_control_or_wait), std::end(s_control_or_wait),
                                                [&](const char *control) { return operation == control; });
}

// a link of a listing: as nvcc wrote it, its registers as written, with its registers read as their kind, and its
// position in the chain, the address nvcc wrote beside it, and each register it names with the slot it names it in: g
// for the guard, and each operand by its count from 0
struct Link
{
    std::string exact;
    std::string registers;
    std::string kind;
    std::string position;
    std::vector<std::pair<std::string, std::string>> slots;
    std::string marked;
    std::vector<std::string> named;
    std::vector<std::string> literals;
    std::vector<std::string> targets;
    std::vector<std::string> distances;
};

// one question of the stick: its number, its category and C, its links as nvcc wrote them, each with its count, and
// its chain, every link in the order nvcc wrote it
struct Question
{
    std::string number;
    std::string category;
    std::string text;
    std::map<std::string, unsigned int> exact;
    std::map<std::string, Link> links;
    std::vector<Link> chain;
};

// the text with each hexadecimal number written in decimal
static std::string numbers_decimal(const std::string &text)
{
    std::string decimal;
    static const std::regex s_hex("0x([0-9a-fA-F]+)");
    std::sregex_iterator walk(text.begin(), text.end(), s_hex);
    size_t at = 0u;
    for (; walk != std::sregex_iterator(); ++walk)
    {
        decimal += text.substr(at, (size_t)walk->position() - at);
        decimal += std::to_string(std::stoull((*walk)[1].str(), nullptr, 16));
        at = (size_t)(walk->position() + walk->length());
    }
    decimal += text.substr(at);
    return decimal;
}

// an operand with its registers read as their kind and its numbers in decimal
static std::string operand_kind(std::string text)
{
    static const std::regex s_reuse("\\.reuse");
    static const std::regex s_uniform("\\bUR[0-9]+\\b");
    static const std::regex s_register("\\bR[0-9]+\\b");
    static const std::regex s_predicate("(^|[^A-Z_])P[0-6]\\b");
    text = std::regex_replace(text, s_reuse, "");
    text = std::regex_replace(text, s_uniform, "UR");
    text = std::regex_replace(text, s_register, "R");
    text = std::regex_replace(text, s_predicate, "$1P");
    return numbers_decimal(text);
}

// the registers an instruction names, in order
static std::string registers_named(const std::string &text)
{
    static const std::regex s_named("\\b(UR[0-9]+|R[0-9]+|P[0-6])\\b");
    std::string named;
    for (std::sregex_iterator walk(text.begin(), text.end(), s_named); walk != std::sregex_iterator(); ++walk)
    {
        named += walk->str() + " ";
    }
    return named;
}

// each register `text` names, put down in `slot`
static void slots_named(const std::string &text, const std::string &slot,
                        std::vector<std::pair<std::string, std::string>> *slots)
{
    static const std::regex s_named("\\b(UR[0-9]+|R[0-9]+|P[0-6])\\b");
    for (std::sregex_iterator walk(text.begin(), text.end(), s_named); walk != std::sregex_iterator(); ++walk)
    {
        slots->push_back(std::make_pair(slot, walk->str()));
    }
}

static std::string trimmed(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    const size_t last = text.find_last_not_of(" \t\r\n;");
    return (first == std::string::npos) ? std::string() : text.substr(first, last - first + 1u);
}

// the line of a listing as a link: 1, 0 where it is no instruction, or -1 where it is the branch to itself past the
// last exit, after which nothing of the chain is reached
static int link_read(const std::string &line, Link *link)
{
    static const std::regex s_address("/\\*([0-9a-f]{4,})\\*/");
    static const std::regex s_comment("/\\*.*?\\*/");
    std::smatch address;
    const int addressed = std::regex_search(line, address, s_address);
    std::string text = trimmed(std::regex_replace(line, s_comment, ""));
    static const std::regex s_instruction("^(@!?[A-Z0-9]+\\s+)?[A-Z][A-Z0-9_.]*(\\s.*)?$");
    if (text.empty() || !std::regex_match(text, s_instruction))
    {
        return 0;
    }
    static const std::regex s_self("^BRA\\s+0x([0-9a-f]+)$");
    std::smatch self;
    if (addressed && std::regex_match(text, self, s_self) &&
        (std::stoull(self[1].str(), nullptr, 16) == std::stoull(address[1].str(), nullptr, 16)))
    {
        return -1;
    }
    const std::string written = text;
    link->slots.clear();
    std::string guard;
    if (text[0] == '@')
    {
        const size_t space = text.find_first_of(" \t");
        guard = operand_kind(text.substr(0u, space)) + " ";
        slots_named(text.substr(0u, space), "g", &link->slots);
        text = trimmed(text.substr(space));
    }
    const size_t space = text.find_first_of(" \t");
    std::string kind = guard + text.substr(0u, space);
    if (space != std::string::npos)
    {
        std::stringstream operands(text.substr(space));
        std::string piece;
        std::string joined;
        unsigned int slot = 0u;
        while (std::getline(operands, piece, ','))
        {
            joined += (joined.empty() ? "" : ", ") + operand_kind(trimmed(piece));
            slots_named(piece, std::to_string(slot), &link->slots);
            slot += 1u;
        }
        kind += " " + joined;
    }
    text = written;
    // the link with each register a mark and each literal operand another, the registers and the literals in the order
    // written, its spaces one and its numbers decimal. The target of an operation that aims at an address of the code is
    // a position in the chain and is written as its distance in links from the link itself: a barrier's number, a mask
    // or a count is a literal
    static const std::regex s_reuse("\\.reuse");
    static const std::regex s_spaces("\\s+");
    static const std::regex s_named("\\b(UR[0-9]+|R[0-9]+|P[0-6])\\b");
    static const std::regex s_literal("^-?(0x[0-9a-f]+|[0-9]+(\\.[0-9]+)?(e[+-][0-9]+)?|[+-]?INF|[+-]?QNAN)$");
    static const std::regex s_target("^0x[0-9a-f]+$");
    const std::string plain = std::regex_replace(std::regex_replace(written, s_reuse, ""), s_spaces, " ");
    link->marked.clear();
    link->named.clear();
    link->literals.clear();
    link->targets.clear();
    link->distances.clear();
    const size_t operation_first = (plain[0] == '@') ? plain.find(' ') + 1u : 0u;
    const size_t operation_last = plain.find(' ', operation_first);
    const std::string operation_name = plain.substr(operation_first, plain.find_first_of(". ", operation_first) - operation_first);
    static const char *const s_aimed[] = {"BRA", "JMP", "CALL", "BSSY"};
    const int transfers = std::any_of(std::begin(s_aimed), std::end(s_aimed),
                                      [&](const char *aimed) { return operation_name == aimed; });
    std::vector<std::string> pieces;
    if (operation_last != std::string::npos)
    {
        std::stringstream operands(plain.substr(operation_last + 1u));
        std::string piece;
        while (std::getline(operands, piece, ','))
        {
            pieces.push_back(trimmed(piece));
        }
    }
    const std::string head = plain.substr(0u, operation_last);
    std::string rebuilt;
    for (size_t at = 0u; at < pieces.size(); at += 1u)
    {
        std::string piece = pieces[at];
        if (transfers && addressed && std::regex_match(piece, s_target))
        {
            const long long distance =
                ((long long)std::stoull(piece, nullptr, 16) - (long long)std::stoull(address[1].str(), nullptr, 16)) / 16;
            link->targets.push_back(piece);
            link->distances.push_back("." + std::string((distance < 0) ? "" : "+") + std::to_string(distance));
            piece = "\x03";
        }
        else if (std::regex_match(piece, s_literal))
        {
            link->literals.push_back(numbers_decimal(piece));
            piece = "\x02";
        }
        rebuilt += ((at == 0u) ? " " : ", ") + piece;
    }
    const std::string shaped = head + rebuilt;
    size_t at = 0u;
    for (std::sregex_iterator walk(shaped.begin(), shaped.end(), s_named); walk != std::sregex_iterator(); ++walk)
    {
        link->marked += shaped.substr(at, (size_t)walk->position() - at) + "\x01";
        link->named.push_back(walk->str());
        at = (size_t)(walk->position() + walk->length());
    }
    link->marked = numbers_decimal(link->marked + shaped.substr(at));
    link->exact = text;
    link->registers = registers_named(text);
    link->kind = kind;
    link->position = addressed ? address[1].str() : std::string();
    return 1;
}

static int questions_read(const char *listing, const char *manifest, std::vector<Question> *questions)
{
    std::map<std::string, std::pair<std::string, std::string>> named;
    std::ifstream table(manifest);
    std::string row;
    while (std::getline(table, row))
    {
        const size_t first = row.find('\t');
        const size_t second = row.find('\t', first + 1u);
        if ((first == 4u) && (second != std::string::npos))
        {
            named[row.substr(0u, 4u)] = std::make_pair(row.substr(5u, second - 5u), trimmed(row.substr(second + 1u)));
        }
    }
    std::ifstream file(listing);
    if (!file || named.empty())
    {
        printf("  %s or %s could not be read\n", listing, manifest);
        return 0;
    }
    static const std::regex s_function("Function : measuring_stick_([0-9]+)");
    std::string line;
    int ended = 1;
    while (std::getline(file, line))
    {
        std::smatch function;
        if (std::regex_search(line, function, s_function))
        {
            Question question;
            question.number = function[1].str();
            question.category = named[question.number].first;
            question.text = named[question.number].second;
            questions->push_back(question);
            ended = 0;
            continue;
        }
        Link link;
        const int read = ended ? 0 : link_read(line, &link);
        ended = ended || (read < 0);
        if (read > 0)
        {
            // a link written more than once in a listing is held at its most basal position
            questions->back().exact[link.exact] += 1u;
            questions->back().links.insert(std::make_pair(link.exact, link));
            questions->back().chain.push_back(link);
        }
    }
    // the chain runs question by question in the order of their numbers
    std::sort(questions->begin(), questions->end(),
              [](const Question &one, const Question &other) { return one.number < other.number; });
    return 1;
}

// the context of the link at `position` in the question's chain: for each register it names, the nearest link before
// it and the nearest after it that name the same register, each written as which side, the slot the link names it in,
// the slot the other names it in, and the other's link. Nothing but the names the links share is read
static std::string member_context(const Question &question, const std::string &position)
{
    size_t at = 0u;
    while ((at < question.chain.size()) && (question.chain[at].position != position))
    {
        at += 1u;
    }
    if (at == question.chain.size())
    {
        return std::string();
    }
    std::set<std::string> entries;
    const Link &link = question.chain[at];
    for (const auto &slot : link.slots)
    {
        for (int side = 0; side < 2; side += 1)
        {
            const int step = (side == 0) ? -1 : 1;
            for (long walk = (long)at + step; (walk >= 0) && (walk < (long)question.chain.size()); walk += step)
            {
                const Link &other = question.chain[(size_t)walk];
                int named = 0;
                for (const auto &other_slot : other.slots)
                {
                    if (other_slot.second == slot.second)
                    {
                        entries.insert(std::string((side == 0) ? "before " : "after ") + slot.first + " " +
                                       other_slot.first + " " + other.kind);
                        named = 1;
                    }
                }
                if (named)
                {
                    break;
                }
            }
        }
    }
    std::string context;
    for (const std::string &entry : entries)
    {
        context += (context.empty() ? "" : ";") + entry;
    }
    return context;
}

// the links of `one` past those of `other`, counted
static std::vector<std::string> links_past(const Question &one, const Question &other)
{
    std::vector<std::string> past;
    for (const auto &held : one.exact)
    {
        const auto found = other.exact.find(held.first);
        const unsigned int there = (found == other.exact.end()) ? 0u : found->second;
        for (unsigned int extra = there; extra < held.second; extra += 1u)
        {
            past.push_back(held.first);
        }
    }
    return past;
}

// 1 where the question's operands and result are integers, which the host computes as every system does
static int question_integer(const Question &question)
{
    static const std::set<std::string> s_categories = {"operator", "compound", "unary", "conversion", "conditional",
                                                       "statement", "test", "pressure", "text_identity"};
    return s_categories.count(question.category) && (question.text.find("float") == std::string::npos) &&
           (question.text.find("double") == std::string::npos) && (question.text.find("__") == std::string::npos);
}

// the kernel `number` of the stick's text, from its signature to the brace that closes it
static std::string kernel_text(const std::string &stick, const std::string &number)
{
    const std::string signature = "extern \"C\" __global__ void measuring_stick_" + number + "(";
    const size_t first = stick.find(signature);
    if (first == std::string::npos)
    {
        return std::string();
    }
    const size_t last = stick.find("\n}\n", first);
    return stick.substr(first, last + 3u - first);
}

// the type a kernel's first operand is cast to, as its text declares it, const or not: a compound assignment writes it
static std::string first_type(const std::string &kernel)
{
    static const std::regex s_first("\\b(?:const )?([a-z][a-z ]*) a = \\(");
    std::smatch found;
    return std::regex_search(kernel, found, s_first) ? found[1].str() : std::string("int");
}

static int host_questions_write(const std::string &path, const std::string &stick, const std::set<std::string> &numbers)
{
    FILE *const file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 0;
    }
    fprintf(file, "// the stick's integer questions as the host computes them, written by klq_identity\n");
    fprintf(file, "#include <limits>\n#include <stdio.h>\n\n");
    fprintf(file, "struct HostIndex\n{\n    unsigned int x, y, z;\n};\n");
    fprintf(file, "static const HostIndex threadIdx = {0u, 0u, 0u};\nstatic const HostIndex blockIdx = {0u, 0u, 0u};\n");
    fprintf(file, "static const HostIndex blockDim = {1u, 1u, 1u};\n#define __global__\n\n");
    std::vector<std::string> written;
    for (const std::string &number : numbers)
    {
        const std::string kernel = kernel_text(stick, number);
        if (kernel.empty())
        {
            continue;
        }
        fprintf(file, "%s\n", kernel.c_str());
        const std::string type = first_type(kernel);
        const int divides = (kernel.find(" / b") != std::string::npos) || (kernel.find(" % b") != std::string::npos) ||
                            (kernel.find("/=") != std::string::npos) || (kernel.find("%=") != std::string::npos) ||
                            (kernel.find(" / ") != std::string::npos);
        const int shifts = (kernel.find("<<") != std::string::npos) || (kernel.find(">>") != std::string::npos);
        // 1 where the host's C would trap on the case or leave it undefined: a divisor of zero, the least value over -1,
        // or a shift by the width or past it
        fprintf(file, "static int refused_%s(const unsigned long long *in)\n{\n", number.c_str());
        fprintf(file, "    const %s a = (%s)in[0];\n    const %s b = (%s)in[1];\n    (void)a;\n    (void)b;\n", type.c_str(),
                type.c_str(), type.c_str(), type.c_str());
        if (divides)
        {
            fprintf(file, "    if ((b == 0) || (std::numeric_limits<%s>::is_signed && (b == (%s)-1) && "
                          "(a == std::numeric_limits<%s>::min())))\n    {\n        return 1;\n    }\n",
                    type.c_str(), type.c_str(), type.c_str());
        }
        if (shifts)
        {
            fprintf(file, "    if (((unsigned long long)b >= (unsigned long long)(sizeof(a + 0) * 8u)) || ((long long)b < 0))\n"
                          "    {\n        return 1;\n    }\n");
        }
        fprintf(file, "    return 0;\n}\n\n");
        written.push_back(number);
    }
    fprintf(file, "typedef void (*HostKernel)(const unsigned long long *, unsigned long long *, unsigned int);\n");
    fprintf(file, "typedef int (*HostRefused)(const unsigned long long *);\n");
    fprintf(file, "static const struct\n{\n    const char *number;\n    HostKernel kernel;\n    HostRefused refused;\n} "
                  "s_questions[] = {\n");
    for (const std::string &number : written)
    {
        fprintf(file, "    {\"%s\", measuring_stick_%s, refused_%s},\n", number.c_str(), number.c_str(), number.c_str());
    }
    fprintf(file, "};\n\nstatic const unsigned long long s_values[] = {");
    for (unsigned int at = 0u; at < IDENTITY_VALUES; at += 1u)
    {
        fprintf(file, "%s0x%llxull", (at == 0u) ? "" : ", ", s_values[at]);
    }
    fprintf(file, "};\n\n");
    fprintf(file, "// each question a line: its number, then its answer to every case in order, - where the case is refused\n");
    fprintf(file, "int main(void)\n{\n");
    fprintf(file, "    for (unsigned int at = 0u; at < sizeof(s_questions) / sizeof(s_questions[0]); at += 1u)\n    {\n");
    fprintf(file, "        printf(\"%%s\", s_questions[at].number);\n");
    fprintf(file, "        for (unsigned int third = 0u; third < %uu; third += 1u)\n        {\n", IDENTITY_THIRDS);
    fprintf(file, "            for (unsigned int left = 0u; left < %uu; left += 1u)\n            {\n", (unsigned int)IDENTITY_VALUES);
    fprintf(file, "                for (unsigned int right = 0u; right < %uu; right += 1u)\n                {\n",
            (unsigned int)IDENTITY_VALUES);
    fprintf(file, "                    const unsigned long long in[4] = {s_values[left], s_values[right], third, 0ull};\n");
    fprintf(file, "                    unsigned long long out[2] = {0ull, 0ull};\n");
    fprintf(file, "                    if (s_questions[at].refused(in))\n                    {\n");
    fprintf(file, "                        printf(\" -\");\n                        continue;\n                    }\n");
    fprintf(file, "                    s_questions[at].kernel(in, out, 1u);\n");
    fprintf(file, "                    printf(\" %%llx\", out[0]);\n                }\n            }\n        }\n");
    fprintf(file, "        printf(\"\\n\");\n    }\n    return 0;\n}\n");
    fclose(file);
    printf("  %s: %u questions the host computes\n", path.c_str(), (unsigned int)written.size());
    return 1;
}

// the first occurrence of a chain identity, the question and the link it starts at, and how often it occurs
struct Occurrence
{
    unsigned int question;
    unsigned int link;
    unsigned int count;
};

// the link written with each register by its class and the order the window first names it in, r for R, p for P, ur
// for UR, each literal operand as a part of its own, k, in the order the window first writes its value, and each target
// of a control transfer as a part of its own, t, in the order the window first aims at it: two windows the same but for
// the registers they were given, the values they hold and where they sit write the same text
static std::string link_render(const Link &link, std::map<std::string, std::string> *names, unsigned int counts[5])
{
    std::string text;
    size_t mark = 0u;
    size_t name = 0u;
    size_t literal = 0u;
    size_t target = 0u;
    for (size_t next = link.marked.find_first_of("\x01\x02\x03", mark); next != std::string::npos;
         next = link.marked.find_first_of("\x01\x02\x03", mark))
    {
        text += link.marked.substr(mark, next - mark);
        const char part = link.marked[next];
        std::string key;
        unsigned int kind = 0u;
        if (part == '\x01')
        {
            key = link.named[name];
            kind = (key[0] == 'U') ? 2u : ((key[0] == 'P') ? 1u : 0u);
            name += 1u;
        }
        else if (part == '\x02')
        {
            key = "\x02" + link.literals[literal];
            kind = 3u;
            literal += 1u;
        }
        else
        {
            key = "\x03" + link.targets[target];
            kind = 4u;
            target += 1u;
        }
        auto found = names->find(key);
        if (found == names->end())
        {
            static const char *const s_classes[5] = {"r", "p", "ur", "k", "t"};
            found = names->insert(std::make_pair(key, s_classes[kind] + std::to_string(counts[kind]))).first;
            counts[kind] += 1u;
        }
        text += found->second;
        mark = next + 1u;
    }
    return text + link.marked.substr(mark);
}

// the window of `length` links from `first` of the question's chain, rendered link by link
static std::string window_render(const Question &question, size_t first, size_t length)
{
    std::map<std::string, std::string> names;
    unsigned int counts[5] = {0u, 0u, 0u, 0u, 0u};
    std::string text;
    for (size_t at = first; at < first + length; at += 1u)
    {
        text += link_render(question.chain[at], &names, counts) + "\n";
    }
    return text;
}

// the identity of each window from `first` of the question's chain, one for each length, each the hash of the window
// rendered: the window and its every extension are read in one walk
static std::vector<unsigned long long> window_identities(const Question &question, size_t first)
{
    std::vector<unsigned long long> identities;
    std::map<std::string, std::string> names;
    unsigned int counts[5] = {0u, 0u, 0u, 0u, 0u};
    unsigned long long hash = 14695981039346656037ull;
    for (size_t at = first; at < question.chain.size(); at += 1u)
    {
        const std::string text = link_render(question.chain[at], &names, counts) + "\n";
        for (const char letter : text)
        {
            hash = (hash ^ (unsigned char)letter) * 1099511628211ull;
        }
        identities.push_back(hash);
    }
    return identities;
}

static std::string address_text(const std::vector<Question> &questions, unsigned int question, unsigned int link)
{
    return questions[question].number + ":" + questions[question].chain[link].position;
}

// The chains of every question, each window of each an identity held at its first occurrence, the lowest address it
// occurs at: every later occurrence reaffirms it at a new address, with the same constituents. A link whose identity
// first occurs where it stands is a primitive. Each question's chain is read from its first link as the constituents
// that came before it, at each link the longest window whose identity first occurs at a lower address, or the link
// alone where nothing does, and its signature is those constituents in order, each an identity at its address, with
// the primitive of each link and the literal operands in that order: identity:address with its primitives.
//
// <folder>/chains.txt holds the primitives in the order of their addresses, each with its count and its link, then for
// each question its chain: the identity of the whole at its address and its count, the constituents, each an address
// and a length, the primitives, and the operands
// every window of every question's chain, each identity at its first occurrence with its count
static std::unordered_map<unsigned long long, Occurrence> windows_held(const std::vector<Question> &questions)
{
    std::unordered_map<unsigned long long, Occurrence> held;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        for (unsigned int link = 0u; link < questions[question].chain.size(); link += 1u)
        {
            for (const unsigned long long identity : window_identities(questions[question], link))
            {
                const auto found = held.insert(std::make_pair(identity, Occurrence{question, link, 0u})).first;
                found->second.count += 1u;
            }
        }
    }
    return held;
}

static int chains_write(const std::vector<Question> &questions, const std::string &folder)
{
    const std::unordered_map<unsigned long long, Occurrence> held = windows_held(questions);
    const std::string path = folder + "/chains.txt";
    FILE *const file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 0;
    }
    std::string primitives;
    std::string chains;
    unsigned int primitive_count = 0u;
    unsigned int constituent_count = 0u;
    unsigned int reaffirmed = 0u;
    std::set<unsigned long long> constituents;
    std::set<unsigned long long> wholes;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        const Question &asked = questions[question];
        if (asked.chain.empty())
        {
            continue;
        }
        const unsigned long long whole_identity = window_identities(asked, 0u).back();
        const Occurrence &whole = held.at(whole_identity);
        wholes.insert(whole_identity);
        reaffirmed += ((whole.question != question) || (whole.link != 0u)) ? 1u : 0u;
        std::string read;
        std::string primitive_row;
        std::string operand_row;
        for (unsigned int link = 0u; link < asked.chain.size();)
        {
            const std::vector<unsigned long long> identities = window_identities(asked, link);
            unsigned int length = 0u;
            for (unsigned int longest = (unsigned int)identities.size(); (longest > 0u) && (length == 0u); longest -= 1u)
            {
                const Occurrence &first = held.at(identities[longest - 1u]);
                const int lower = (first.question < question) || ((first.question == question) && (first.link < link));
                // the constituents of a reaffirmation are the same, link for link, as those of its first occurrence
                if (lower && (window_render(questions[first.question], first.link, longest) ==
                              window_render(asked, link, longest)))
                {
                    read += " " + address_text(questions, first.question, first.link) + "/" + std::to_string(longest);
                    constituents.insert(identities[longest - 1u]);
                    length = longest;
                }
            }
            if (length == 0u)
            {
                primitives += "primitive " + address_text(questions, question, link) + " " +
                              std::to_string(held.at(identities[0]).count) + " " + window_render(asked, link, 1u);
                read += " " + address_text(questions, question, link) + "/1";
                primitive_count += 1u;
                length = 1u;
            }
            constituent_count += 1u;
            for (unsigned int within = link; within < link + length; within += 1u)
            {
                const Occurrence &primitive = held.at(window_identities(asked, within)[0]);
                primitive_row += " " + address_text(questions, primitive.question, primitive.link);
                // the literal operands and the distances of the targets, in the order the link writes them
                const Link &written = asked.chain[within];
                size_t literal = 0u;
                size_t target = 0u;
                for (const char part : written.marked)
                {
                    if (part == '\x02')
                    {
                        operand_row += " " + written.literals[literal];
                        literal += 1u;
                    }
                    else if (part == '\x03')
                    {
                        operand_row += " " + written.distances[target];
                        target += 1u;
                    }
                }
            }
            link += length;
        }
        // the number of the whole is the hash of its contents in their order, the same wherever and in whichever stick it
        // is read
        char number[17];
        snprintf(number, sizeof(number), "%016llx", whole_identity);
        chains += "chain " + address_text(questions, question, 0u) + " " +
                  address_text(questions, whole.question, whole.link) + " " + std::to_string(whole.count) + " " + number +
                  "\n";
        chains += "constituents" + read + "\nprimitives" + primitive_row + "\noperands" + operand_row + "\n";
    }
    fprintf(file, "%s%s", primitives.c_str(), chains.c_str());
    fclose(file);
    printf("  %s: %u chains, %u whole identities, %u chains reaffirming a lower address whole, %u primitives, %u "
           "constituents read, %u identities among them\n",
           path.c_str(), (unsigned int)questions.size(), (unsigned int)wholes.size(), reaffirmed, primitive_count,
           constituent_count, (unsigned int)constituents.size());
    return 1;
}

// 1 where questions `one` and `other` are a slice: their listings the same but for one link a side, the two links
// naming the same registers and read as different kinds, given through `left` and `right`
static int slice_found(const Question &one, const Question &other, const Link **left, const Link **right)
{
    const std::vector<std::string> first = links_past(one, other);
    const std::vector<std::string> second = links_past(other, one);
    if ((first.size() != 1u) || (second.size() != 1u))
    {
        return 0;
    }
    *left = &one.links.at(first[0]);
    *right = &other.links.at(second[0]);
    return ((*left)->registers == (*right)->registers) && ((*left)->kind != (*right)->kind);
}

static int identity_slice(const char *listing, const char *manifest, const char *stick_path, const std::string &folder,
                          const std::string &ours)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    std::ifstream stick_file(stick_path);
    std::stringstream stick;
    stick << stick_file.rdbuf();
    const std::string path = folder + "/slices.txt";
    FILE *const file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    std::set<std::string> asked;
    unsigned int slices = 0u;
    std::set<std::string> identities;
    for (size_t one = 0u; one < questions.size(); one += 1u)
    {
        for (size_t other = one + 1u; other < questions.size(); other += 1u)
        {
            const Link *found_left = nullptr;
            const Link *found_right = nullptr;
            if (!slice_found(questions[one], questions[other], &found_left, &found_right))
            {
                continue;
            }
            const Link &left = *found_left;
            const Link &right = *found_right;
            // each slice a line: its two members, each a question and the position of its link in that question's chain,
            // then the identity, the two links in order, the first member writing the first, then each member's context
            const int swapped = right.kind < left.kind;
            const Question &lead = swapped ? questions[other] : questions[one];
            const Question &rite = swapped ? questions[one] : questions[other];
            const Link &lead_link = swapped ? right : left;
            const Link &rite_link = swapped ? left : right;
            const std::string identity = lead_link.kind + "\t" + rite_link.kind;
            const std::string lead_context = member_context(lead, lead_link.position);
            const std::string rite_context = member_context(rite, rite_link.position);
            fprintf(file, "%s:%s\t%s:%s\t%s\t%s\t%s\n", lead.number.c_str(), lead_link.position.c_str(),
                    rite.number.c_str(), rite_link.position.c_str(), identity.c_str(), lead_context.c_str(),
                    rite_context.c_str());
            identities.insert(identity);
            slices += 1u;
            if (question_integer(lead) && question_integer(rite))
            {
                asked.insert(lead.number);
                asked.insert(rite.number);
            }
        }
    }
    fclose(file);
    // the host computes each integer question the run channel puts to the part, a slice's member or not: a task of
    // register pressure, which slices with nothing, and every question the engine writes a chain for in `ours`
    for (const Question &question : questions)
    {
        const int written = !ours.empty() && std::filesystem::exists(ours + "/" + question.number + ".bin");
        if (question_integer(question) && ((question.category == "pressure") || (written != 0)))
        {
            asked.insert(question.number);
        }
    }
    printf("  %s: %u questions, %u slices, %u identities\n", path.c_str(), (unsigned int)questions.size(), slices,
           (unsigned int)identities.size());
    if (!chains_write(questions, folder))
    {
        return 1;
    }
    return host_questions_write(folder + "/host_questions.cpp", stick.str(), asked) ? 0 : 1;
}

// a language's files read and held against each other and its schema
struct HeldSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// a form's text as the links it writes, each parameter a pattern any operand matches
static std::vector<std::regex> form_links(const std::string &text)
{
    std::vector<std::regex> links;
    std::string written;
    for (size_t at = 0u; at < text.size(); at += 1u)
    {
        if ((text[at] == '\\') && ((at + 1u) < text.size()))
        {
            written += (text[at + 1u] == 'n') ? '\n' : ((text[at + 1u] == 't') ? '\t' : text[at + 1u]);
            at += 1u;
            continue;
        }
        written += text[at];
    }
    std::stringstream lines(written);
    std::string line;
    while (std::getline(lines, line))
    {
        Link link;
        if (link_read(line, &link) != 1)
        {
            continue;
        }
        std::string pattern;
        static const std::regex s_special("[.^$|()\\[\\]*+?\\\\]");
        const std::string escaped = std::regex_replace(link.kind, s_special, "\\$&");
        // an offset added to an address is not written where it is 0: the parameter after a + is there or not
        static const std::regex s_offset("\\\\\\+\\{[a-z_]+\\}");
        static const std::regex s_parameter("\\{[a-z_]+\\}(\\\\\\.hi)?");
        pattern = std::regex_replace(std::regex_replace(escaped, s_offset, "(\\+[^,\\]]+)?"), s_parameter, "[^,]+");
        links.push_back(std::regex("^" + pattern + "$"));
    }
    return links;
}

// the forms of each ruleset given, each name with the links its text writes; 0 where a ruleset could not be read
static int forms_read(int count, char **files, std::vector<std::pair<std::string, std::vector<std::regex>>> *forms)
{
    for (int given = 0; given < count; given += 1)
    {
        HeldSet set;
        const RulesetSchema *const schema = code_generator(files[given]).schema();
        std::string paths[RULESET_PATHS_MAX];
        const unsigned int paths_count = ruleset_paths(schema, ruleset_folder() + "/" + schema->file, paths);
        RulesetFlat flat;
        ruleset_flat_schema(schema, &flat);
        std::string error;
        if (ruleset_relations_read(paths, paths_count, &flat.schema, &set.memory, &set.relations, &error) == 0)
        {
            printf("  %s could not be opened\n", error.c_str());
            return 0;
        }
        for (unsigned int at = 0u; at < set.relations.entry_count; at += 1u)
        {
            const RulesetCoreEntry *const entry = &set.relations.entries[at];
            if (entry->kind != RULESET_CORE_ENTRY_FORM)
            {
                continue;
            }
            const std::string name((const char *)&set.relations.text[entry->name.first], entry->name.length);
            const std::string text((const char *)&set.relations.text[entry->text.first], entry->text.length);
            forms->push_back(std::make_pair(name, form_links(text)));
        }
        // a pipe row chooses between an operation and its writing on the other pipe: a link of either was placed by
        // the row's choice, whichever way it went
        for (unsigned int at = 0u; at < paths_count; at += 1u)
        {
            std::ifstream file(paths[at]);
            std::string line;
            while (std::getline(file, line))
            {
                std::stringstream words(line);
                std::string head;
                std::string operation;
                std::string writing;
                if ((words >> head >> operation >> writing) && (head == "pipe"))
                {
                    static const std::regex s_special("[.^$|()\\[\\]*+?\\\\]");
                    const std::string either = std::regex_replace(operation, s_special, "\\$&") + "|" +
                                               std::regex_replace(writing, s_special, "\\$&");
                    forms->push_back(std::make_pair("pipe:" + operation + ":" + writing,
                                                    std::vector<std::regex>{std::regex("^(@!?P )?(" + either + ")( .*)?$")}));
                }
            }
        }
    }
    return 1;
}

// the names of the forms whose texts write `kind`, joined by spaces, or - where none does
static std::string forms_writing(const std::vector<std::pair<std::string, std::vector<std::regex>>> &forms,
                                 const std::string &kind)
{
    std::string names;
    for (const auto &form : forms)
    {
        if (std::any_of(form.second.begin(), form.second.end(),
                        [&](const std::regex &one) { return std::regex_match(kind, one); }))
        {
            names += (names.empty() ? "" : " ") + form.first;
        }
    }
    return names.empty() ? std::string("-") : names;
}

// a member of a slice: its question and the position of its link in that question's chain
struct Member
{
    std::string question;
    std::string position;
};

// a slice carrying an identity: the member writing its first link and the member writing its second, each with its
// context
struct Carrier
{
    Member first;
    Member second;
    std::string first_context;
    std::string second_context;
};

// the entries of a context, split
static std::set<std::string> context_entries(const std::string &context)
{
    std::set<std::string> entries;
    std::stringstream split(context);
    std::string entry;
    while (std::getline(split, entry, ';'))
    {
        entries.insert(entry);
    }
    return entries;
}

// a verdict over the carriers given, written after them: the first case one carrier's two questions answer apart, or
// the count of cases alike. 1 where it closed, 0 where it stayed open over cases asked, -1 where nothing was asked
static int carriers_verdict(FILE *file, const std::vector<const Carrier *> &carriers,
                            const std::map<std::string, std::vector<std::string>> &answers,
                            std::string (*text)(const Carrier &))
{
    unsigned int alike = 0u;
    for (const Carrier *const slice : carriers)
    {
        const auto lead = answers.find(slice->first.question);
        const auto rite = answers.find(slice->second.question);
        if ((lead == answers.end()) || (rite == answers.end()))
        {
            continue;
        }
        for (unsigned int at = 0u; at < IDENTITY_CASES; at += 1u)
        {
            const std::string &one = lead->second[at];
            const std::string &other = rite->second[at];
            if ((one == "-") || (other == "-"))
            {
                continue;
            }
            if (one != other)
            {
                const unsigned int third = at / (IDENTITY_VALUES * IDENTITY_VALUES);
                const unsigned int left = (at / IDENTITY_VALUES) % IDENTITY_VALUES;
                const unsigned int right = at % IDENTITY_VALUES;
                fprintf(file, "closed %s %llx,%llx,%x->%s,%s\n", text(*slice).c_str(), s_values[left], s_values[right],
                        third, one.c_str(), other.c_str());
                return 1;
            }
            alike += 1u;
        }
    }
    fprintf(file, "open %u\n", alike);
    return (alike != 0u) ? 0 : -1;
}

static Member member_read(const std::string &text)
{
    const size_t colon = text.find(':');
    return Member{text.substr(0u, colon), text.substr(colon + 1u)};
}

// 1 where `one` sits before `other` in the chain: by question, then by position in it
static int member_before(const Member &one, const Member &other)
{
    if (one.question != other.question)
    {
        return std::stoul(one.question) < std::stoul(other.question);
    }
    return std::stoull(one.position, nullptr, 16) < std::stoull(other.position, nullptr, 16);
}

// 1 where `one` sits before `other`: contention defers to the most basal member of each, then to the other member
static bool carrier_before(const Carrier &one, const Carrier &other)
{
    const int one_swapped = member_before(one.second, one.first);
    const int other_swapped = member_before(other.second, other.first);
    const Member &one_basal = one_swapped ? one.second : one.first;
    const Member &other_basal = other_swapped ? other.second : other.first;
    if (member_before(one_basal, other_basal) || member_before(other_basal, one_basal))
    {
        return member_before(one_basal, other_basal);
    }
    return member_before(one_swapped ? one.first : one.second, other_swapped ? other.first : other.second);
}

static std::string carrier_text(const Carrier &carrier)
{
    return carrier.first.question + ":" + carrier.first.position + " " + carrier.second.question + ":" +
           carrier.second.position;
}

static int identity_read(const std::string &folder, const std::string &klq, int count, char **files)
{
    std::map<std::string, std::vector<std::string>> answers;
    std::ifstream answered(folder + "/host_answers.txt");
    std::string line;
    while (std::getline(answered, line))
    {
        std::stringstream words(line);
        std::string number;
        words >> number;
        std::string word;
        while (words >> word)
        {
            answers[number].push_back(word);
        }
    }
    // members the same make the identity the same, and the carriers of one identity are told apart by address: the
    // position of each in the chain, the most basal member first
    std::map<std::string, std::vector<Carrier>> carried;
    std::ifstream sliced(folder + "/slices.txt");
    while (std::getline(sliced, line))
    {
        std::vector<std::string> cells;
        std::stringstream split(line);
        std::string cell;
        while (std::getline(split, cell, '\t'))
        {
            cells.push_back(cell);
        }
        cells.resize(6u);
        carried[cells[2] + "\t" + cells[3]].push_back(
            Carrier{member_read(cells[0]), member_read(cells[1]), cells[4], cells[5]});
    }
    if (carried.empty())
    {
        printf("  %s/slices.txt holds no slice\n", folder.c_str());
        return 1;
    }
    std::vector<std::pair<std::string, std::vector<Carrier>>> addressed;
    // the most basal member writes an identity's first link
    for (auto &identity : carried)
    {
        std::sort(identity.second.begin(), identity.second.end(), carrier_before);
        if (member_before(identity.second[0].second, identity.second[0].first))
        {
            const size_t tab = identity.first.find('\t');
            for (Carrier &carrier : identity.second)
            {
                std::swap(carrier.first, carrier.second);
                std::swap(carrier.first_context, carrier.second_context);
            }
            addressed.push_back(std::make_pair(identity.first.substr(tab + 1u) + "\t" + identity.first.substr(0u, tab),
                                               identity.second));
            continue;
        }
        addressed.push_back(identity);
    }
    std::sort(addressed.begin(), addressed.end(),
              [](const std::pair<std::string, std::vector<Carrier>> &one,
                 const std::pair<std::string, std::vector<Carrier>> &other)
              { return carrier_before(one.second[0], other.second[0]); });
    std::vector<std::pair<std::string, std::vector<std::regex>>> forms;
    if (!forms_read(count, files, &forms))
    {
        return 1;
    }
    std::ifstream held(klq);
    std::stringstream kept;
    while (std::getline(held, line) && (line.rfind("slice_identity ", 0u) != 0u))
    {
        kept << line << "\n";
    }
    FILE *const file = fopen(klq.c_str(), "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", klq.c_str());
        return 1;
    }
    fprintf(file, "%s", kept.str().c_str());
    unsigned int number = 0u;
    unsigned int contexts = 0u;
    unsigned int closed = 0u;
    unsigned int open = 0u;
    unsigned int unasked = 0u;
    unsigned int many = 0u;
    unsigned int split = 0u;
    unsigned int flagged = 0u;
    unsigned int formed = 0u;
    for (const auto &identity : addressed)
    {
        const size_t tab = identity.first.find('\t');
        const std::string sides[2] = {identity.first.substr(0u, tab), identity.first.substr(tab + 1u)};
        // an identity's address is its most basal carrier's
        fprintf(file, "slice_identity %s\nfirst %s\nsecond %s\n", carrier_text(identity.second[0]).c_str(), sides[0].c_str(),
                sides[1].c_str());
        number += 1u;
        for (const std::string &side : sides)
        {
            if (rule_broken(side))
            {
                fprintf(file, "rule 2 %s: its form transfers control or waits\n", side.c_str());
                flagged += 1u;
            }
        }
        for (unsigned int at = 0u; at < 2u; at += 1u)
        {
            for (const auto &form : forms)
            {
                const auto &links = form.second;
                if (std::any_of(links.begin(), links.end(),
                                [&](const std::regex &one) { return std::regex_match(sides[at], one); }))
                {
                    fprintf(file, "form %s %s\n", (at == 0u) ? "first" : "second", form.first.c_str());
                    formed += 1u;
                }
            }
        }
        // the carriers of one identity, gathered by context in the order of their most basal carrier: each context the
        // same members in one place of the grammar, with a verdict of its own
        std::vector<std::pair<std::string, std::vector<const Carrier *>>> placed;
        for (const Carrier &slice : identity.second)
        {
            const std::string key = slice.first_context + "\t" + slice.second_context;
            auto found = std::find_if(placed.begin(), placed.end(),
                                      [&](const std::pair<std::string, std::vector<const Carrier *>> &one)
                                      { return one.first == key; });
            if (found == placed.end())
            {
                placed.push_back(std::make_pair(key, std::vector<const Carrier *>()));
                found = placed.end() - 1;
            }
            found->second.push_back(&slice);
        }
        many += (placed.size() > 1u) ? 1u : 0u;
        int verdicts[2] = {0, 0};
        for (const auto &place : placed)
        {
            const Carrier &basal = *place.second[0];
            fprintf(file, "context %s\n", carrier_text(basal).c_str());
            // an entry both members hold is written once, and one a member alone holds is written under its side
            const std::set<std::string> first_entries = context_entries(basal.first_context);
            const std::set<std::string> second_entries = context_entries(basal.second_context);
            for (const std::string &entry : first_entries)
            {
                fprintf(file, "%s%s\n", second_entries.count(entry) ? "" : "first ", entry.c_str());
            }
            for (const std::string &entry : second_entries)
            {
                if (!first_entries.count(entry))
                {
                    fprintf(file, "second %s\n", entry.c_str());
                }
            }
            for (const Carrier *const slice : place.second)
            {
                fprintf(file, "address %s\n", carrier_text(*slice).c_str());
            }
            const int verdict = carriers_verdict(file, place.second, answers, carrier_text);
            contexts += 1u;
            closed += (verdict == 1) ? 1u : 0u;
            open += (verdict == 0) ? 1u : 0u;
            unasked += (verdict == -1) ? 1u : 0u;
            verdicts[0] |= (verdict == 1);
            verdicts[1] |= (verdict == 0);
        }
        split += (verdicts[0] && verdicts[1]) ? 1u : 0u;
    }
    // the chains after the identities, as slice wrote them
    std::ifstream chained(folder + "/chains.txt");
    while (std::getline(chained, line))
    {
        fprintf(file, "%s\n", line.c_str());
    }
    fclose(file);
    printf("  %s: %u identities in %u contexts, %u identities in more than one, %u closed in one context and open in "
           "another\n",
           klq.c_str(), number, contexts, many, split);
    printf("  contexts: %u closed, %u open over the cases asked, %u asked of nothing; %u links a rule keeps off the part, "
           "%u forms written beside them\n",
           closed, open, unasked, flagged, formed);
    return 0;
}

// the files `given` names, a folder standing for every .dis file in it by name: a folder of the engine's writing of the
// stick holds more files than one command line can name
static std::vector<std::string> candidate_paths(const std::vector<std::string> &given)
{
    std::vector<std::string> paths;
    for (const std::string &path : given)
    {
        if (!std::filesystem::is_directory(path))
        {
            paths.push_back(path);
            continue;
        }
        std::vector<std::string> held;
        for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(path))
        {
            if (entry.path().extension() == ".dis")
            {
                held.push_back(entry.path().generic_string());
            }
        }
        std::sort(held.begin(), held.end());
        paths.insert(paths.end(), held.begin(), held.end());
    }
    return paths;
}

// the chains of a candidate file: one for each function its listing names, or the whole file as one chain named for
// its stem where it names none
static void candidates_read(const std::string &path, std::vector<Question> *candidates)
{
    std::ifstream file(path);
    static const std::regex s_function("Function : ([A-Za-z0-9_]+)");
    const size_t slash = path.find_last_of("/\\");
    const std::string stem = path.substr((slash == std::string::npos) ? 0u : slash + 1u);
    const size_t first = candidates->size();
    std::string line;
    int ended = 0;
    while (std::getline(file, line))
    {
        std::smatch function;
        if (std::regex_search(line, function, s_function))
        {
            Question candidate;
            candidate.number = function[1].str();
            candidates->push_back(candidate);
            ended = 0;
            continue;
        }
        Link link;
        const int read = ended ? 0 : link_read(line, &link);
        ended = ended || (read < 0);
        if (read > 0)
        {
            if (candidates->size() == first)
            {
                Question candidate;
                candidate.number = stem.substr(0u, stem.find('.'));
                candidates->push_back(candidate);
            }
            candidates->back().chain.push_back(link);
        }
    }
}

// Each candidate chain sifted through the identities nvcc's chains hold, before anything of it is put to the part:
// its whole known at an address, or read as the longest known windows from each link, with every link whose primitive
// nvcc never writes and every two links side by side nvcc never writes so, each known apart. A part unknown that rule 2
// keeps off the part is marked with the rule. <folder>/known.txt holds a candidate a group of lines
static int identity_known(const char *listing, const char *manifest, const std::string &folder,
                          const std::vector<std::string> &paths)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    const std::unordered_map<unsigned long long, Occurrence> held = windows_held(questions);
    std::vector<Question> candidates;
    for (const std::string &path : paths)
    {
        candidates_read(path, &candidates);
    }
    const std::string path = folder + "/known.txt";
    FILE *const file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    // the first occurrence of a window where nvcc's chains hold it, its constituents the same link for link
    const auto known = [&](const Question &candidate, unsigned int link, unsigned int length,
                           unsigned long long identity) -> const Occurrence *
    {
        const auto found = held.find(identity);
        if ((found == held.end()) || (window_render(questions[found->second.question], found->second.link, length) !=
                                      window_render(candidate, link, length)))
        {
            return nullptr;
        }
        return &found->second;
    };
    unsigned int wholes = 0u;
    unsigned int permutations = 0u;
    unsigned int strangers = 0u;
    unsigned int unknown_primitives = 0u;
    unsigned int unknown_pairs = 0u;
    unsigned int flagged = 0u;
    for (const Question &candidate : candidates)
    {
        if (candidate.chain.empty())
        {
            continue;
        }
        const unsigned int links = (unsigned int)candidate.chain.size();
        const std::vector<unsigned long long> from_first = window_identities(candidate, 0u);
        const Occurrence *const whole = known(candidate, 0u, links, from_first.back());
        char number[17];
        snprintf(number, sizeof(number), "%016llx", from_first.back());
        fprintf(file, "candidate %s %s %s\n", candidate.number.c_str(), number,
                (whole == nullptr) ? "unknown" : ("known " + address_text(questions, whole->question, whole->link)).c_str());
        if (whole != nullptr)
        {
            wholes += 1u;
            continue;
        }
        std::string read;
        for (unsigned int link = 0u; link < links;)
        {
            const std::vector<unsigned long long> identities = window_identities(candidate, link);
            unsigned int length = 0u;
            for (unsigned int longest = (unsigned int)identities.size(); (longest > 0u) && (length == 0u); longest -= 1u)
            {
                const Occurrence *const first = known(candidate, link, longest, identities[longest - 1u]);
                if (first != nullptr)
                {
                    read += " " + address_text(questions, first->question, first->link) + "/" + std::to_string(longest);
                    length = longest;
                }
            }
            if (length == 0u)
            {
                read += " ?" + candidate.chain[link].position;
                length = 1u;
            }
            link += length;
        }
        fprintf(file, "constituents%s\n", read.c_str());
        unsigned int unknown_here = 0u;
        for (unsigned int link = 0u; link < links; link += 1u)
        {
            const std::vector<unsigned long long> identities = window_identities(candidate, link);
            const int alone = known(candidate, link, 1u, identities[0]) != nullptr;
            const int paired = ((link + 1u) < links) ? (known(candidate, link, 2u, identities[1]) != nullptr) : 1;
            const int next_alone =
                ((link + 1u) < links) ? (known(candidate, link + 1u, 1u, window_identities(candidate, link + 1u)[0]) != nullptr) : 0;
            const Link &written = candidate.chain[link];
            const std::string rule = rule_broken(written.exact) ? " rule 2: its form transfers control or waits" : "";
            if (!alone)
            {
                fprintf(file, "unknown %s %s%s\n", written.position.c_str(), window_render(candidate, link, 1u).c_str(),
                        rule.c_str());
                unknown_primitives += 1u;
                unknown_here += 1u;
                flagged += rule.empty() ? 0u : 1u;
            }
            // two links each known alone and never written side by side
            if (alone && next_alone && !paired)
            {
                std::string pair = window_render(candidate, link, 2u);
                std::replace(pair.begin(), pair.end(), '\n', ';');
                fprintf(file, "unpaired %s %s\n", written.position.c_str(), pair.c_str());
                unknown_pairs += 1u;
                unknown_here += 1u;
            }
        }
        if (unknown_here == 0u)
        {
            permutations += 1u;
        }
        else
        {
            strangers += 1u;
        }
    }
    fclose(file);
    printf("  %s: %u candidates, %u known whole, %u not known whole but every link and every two side by side known, "
           "%u with a part nvcc never writes\n",
           path.c_str(), (unsigned int)candidates.size(), wholes, permutations, strangers);
    printf("  parts unknown: %u links, %u of them kept off the part by rule 2, %u pairs side by side\n",
           unknown_primitives, flagged, unknown_pairs);
    return 0;
}

// the root of `kind` among the categories, each kind joined to the kinds it was found in a slice with
static std::string category_root(std::map<std::string, std::string> *parent, const std::string &kind)
{
    std::string root = kind;
    while ((*parent)[root] != root)
    {
        root = (*parent)[root];
    }
    for (std::string walk = kind; walk != root;)
    {
        const std::string next = (*parent)[walk];
        (*parent)[walk] = root;
        walk = next;
    }
    return root;
}

// a question the permutations put: the window it is asked in at its first occurrence, how often it arises, the link
// put in and the link it stands for
struct Permuted
{
    unsigned int question;
    unsigned int link;
    unsigned int count;
    std::string window;
    std::string from;
    std::string to;
};

// The parts of nvcc's chains put together by their categories, no part's meaning read: two links a slice sets apart
// stand for each other, and each link joined to another so, at any remove, is of its category. Each link of each chain
// is stood for by every other of its category naming as many registers, the registers of the link it stands for given
// to it, and asked in the window of the link before it and the link after: a window nvcc writes somewhere is known, and
// one it never writes is a question, held at the first address it arises at with its count. A link put in that rule 2
// keeps off the part is marked with the rule. <folder>/permutations.txt holds the categories, each with its members at
// the first address each occurs at, then the questions in the order of their addresses
static int identity_permute(const char *listing, const char *manifest, const std::string &folder)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    const std::unordered_map<unsigned long long, Occurrence> held = windows_held(questions);
    // the first occurrence of every kind of link, and each kind its own category before any slice joins it
    std::map<std::string, std::pair<unsigned int, unsigned int>> first_kind;
    std::map<std::string, std::string> parent;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        for (unsigned int link = 0u; link < questions[question].chain.size(); link += 1u)
        {
            const std::string &kind = questions[question].chain[link].kind;
            first_kind.insert(std::make_pair(kind, std::make_pair(question, link)));
            parent.insert(std::make_pair(kind, kind));
        }
    }
    for (size_t one = 0u; one < questions.size(); one += 1u)
    {
        for (size_t other = one + 1u; other < questions.size(); other += 1u)
        {
            const Link *left = nullptr;
            const Link *right = nullptr;
            if (slice_found(questions[one], questions[other], &left, &right))
            {
                const std::string left_root = category_root(&parent, left->kind);
                const std::string right_root = category_root(&parent, right->kind);
                parent[left_root] = right_root;
            }
        }
    }
    // each category's members in the order of their first occurrence
    std::vector<std::pair<std::pair<unsigned int, unsigned int>, std::string>> ordered;
    for (const auto &kind : first_kind)
    {
        ordered.push_back(std::make_pair(kind.second, kind.first));
    }
    std::sort(ordered.begin(), ordered.end());
    std::map<std::string, std::vector<std::string>> categories;
    std::vector<std::string> roots;
    for (const auto &kind : ordered)
    {
        const std::string root = category_root(&parent, kind.second);
        if (categories[root].empty())
        {
            roots.push_back(root);
        }
        categories[root].push_back(kind.second);
    }
    std::unordered_map<unsigned long long, Permuted> asked;
    std::vector<unsigned long long> asked_order;
    unsigned long long known = 0ull;
    unsigned long long put = 0ull;
    for (unsigned int question = 0u; question < questions.size(); question += 1u)
    {
        const std::vector<Link> &chain = questions[question].chain;
        for (unsigned int link = 0u; link < chain.size(); link += 1u)
        {
            const std::vector<std::string> &members = categories[category_root(&parent, chain[link].kind)];
            for (const std::string &member : members)
            {
                const std::pair<unsigned int, unsigned int> &at = first_kind.at(member);
                const Link &source = questions[at.first].chain[at.second];
                if ((member == chain[link].kind) || (source.named.size() != chain[link].named.size()))
                {
                    continue;
                }
                Link substitute = source;
                substitute.named = chain[link].named;
                substitute.position = chain[link].position;
                Question window;
                window.number = questions[question].number;
                for (unsigned int within = (link == 0u) ? 0u : link - 1u;
                     within < std::min((unsigned int)chain.size(), link + 2u); within += 1u)
                {
                    window.chain.push_back((within == link) ? substitute : chain[within]);
                }
                const unsigned int length = (unsigned int)window.chain.size();
                const unsigned long long identity = window_identities(window, 0u).back();
                const std::string rendered = window_render(window, 0u, length);
                put += 1ull;
                const auto found = held.find(identity);
                if ((found != held.end()) &&
                    (window_render(questions[found->second.question], found->second.link, length) == rendered))
                {
                    known += 1ull;
                    continue;
                }
                const auto inserted = asked.insert(std::make_pair(
                    identity, Permuted{question, link, 0u, rendered, chain[link].kind, member}));
                if (inserted.second)
                {
                    asked_order.push_back(identity);
                }
                inserted.first->second.count += 1u;
            }
        }
    }
    const std::string path = folder + "/permutations.txt";
    FILE *const file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    unsigned int joined = 0u;
    for (const std::string &root : roots)
    {
        const std::vector<std::string> &members = categories[root];
        if (members.size() < 2u)
        {
            continue;
        }
        joined += 1u;
        const std::pair<unsigned int, unsigned int> &first = first_kind.at(members[0]);
        fprintf(file, "category %s %u\n", address_text(questions, first.first, first.second).c_str(),
                (unsigned int)members.size());
        for (const std::string &member : members)
        {
            const std::pair<unsigned int, unsigned int> &at = first_kind.at(member);
            fprintf(file, "member %s %s\n", address_text(questions, at.first, at.second).c_str(), member.c_str());
        }
    }
    unsigned int flagged = 0u;
    for (const unsigned long long identity : asked_order)
    {
        const Permuted &question = asked.at(identity);
        std::string window = question.window;
        window.pop_back();
        std::replace(window.begin(), window.end(), '\n', ';');
        fprintf(file, "question %s %u\nfrom %s\nto %s\nwindow %s\n",
                address_text(questions, question.question, question.link).c_str(), question.count,
                question.from.c_str(), question.to.c_str(), window.c_str());
        if (rule_broken(question.to))
        {
            fprintf(file, "rule 2 %s: its form transfers control or waits\n", question.to.c_str());
            flagged += 1u;
        }
    }
    fclose(file);
    printf("  %s: %u kinds of link in %u categories of more than one, %llu windows put, %llu of them nvcc writes, "
           "%u questions, %u of them kept off the part by rule 2\n",
           path.c_str(), (unsigned int)first_kind.size(), joined, put, known, (unsigned int)asked_order.size(), flagged);
    return 0;
}

// what one form's writing came to over the questions nvcc makes in fewer steps: the questions, and its links of ours
// alone there, of a kind nvcc writes nothing of in the question
struct FormFolded
{
    std::set<std::string> questions;
    unsigned int alone;
};

// The answer and ours, question by question. nvcc's chain for a question of the stick is the answer; ours is the
// disassembly of what the transpiler wrote for it, derived from the rulesets alone, and the rulesets are what the L*
// query gave. A question is one product, and the only reason to fold is to make the same product in fewer steps: two
// chains of as many steps are two arrangements of it and neither is broken, whatever order or pipe each takes. Where
// nvcc makes it in fewer steps there is a fold the query has not learned, and where ours takes fewer, ours folds
// further.
//
// The two chains are aligned link by link on their kinds, the longest run in common kept in order, and where they part
// each link of ours is moved (nvcc writes its kind elsewhere in the question) or ours alone (nvcc writes nothing of its
// kind there), each with the forms of the rulesets whose texts write it, and each link of nvcc's with nothing of ours
// is written with its address. <folder>/broken.txt holds a question a group of lines, its verdict and the steps of
// each, then each form whose links stand alone in a question nvcc makes in fewer steps, with its count of questions
// and of links, the most questions first
static int identity_broken(const char *listing, const char *manifest, const std::string &folder,
                           const std::vector<std::string> &ours_paths, int count, char **files)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    std::vector<std::pair<std::string, std::vector<std::regex>>> forms;
    if (!forms_read(count, files, &forms))
    {
        return 1;
    }
    std::map<std::string, size_t> numbered;
    for (size_t at = 0u; at < questions.size(); at += 1u)
    {
        numbered[questions[at].number] = at;
    }
    std::vector<Question> ours;
    for (const std::string &path : ours_paths)
    {
        candidates_read(path, &ours);
    }
    const std::string path = folder + "/broken.txt";
    FILE *const file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        printf("  %s could not be written\n", path.c_str());
        return 1;
    }
    std::map<std::string, FormFolded> folded;
    unsigned int alike = 0u;
    unsigned int same = 0u;
    unsigned int theirs_fewer = 0u;
    unsigned int ours_fewer = 0u;
    unsigned int steps_to_learn = 0u;
    unsigned int unanswered = 0u;
    for (const Question &mine : ours)
    {
        const auto found = numbered.find(mine.number);
        if ((found == numbered.end()) || mine.chain.empty())
        {
            unanswered += 1u;
            continue;
        }
        const Question &answer = questions[found->second];
        const size_t rows = mine.chain.size();
        const size_t columns = answer.chain.size();
        // the longest run of kinds the two chains hold in common, in order
        std::vector<std::vector<unsigned int>> common(rows + 1u, std::vector<unsigned int>(columns + 1u, 0u));
        for (size_t row = rows; row-- > 0u;)
        {
            for (size_t column = columns; column-- > 0u;)
            {
                common[row][column] = (mine.chain[row].kind == answer.chain[column].kind)
                                          ? common[row + 1u][column + 1u] + 1u
                                          : std::max(common[row + 1u][column], common[row][column + 1u]);
            }
        }
        // the links each chain holds alone, and the stretches they part in, each running from one link in common to the
        // next
        std::vector<size_t> mine_only;
        std::vector<size_t> answer_only;
        std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>> stretches(1u);
        for (size_t row = 0u, column = 0u; (row < rows) || (column < columns);)
        {
            if ((row < rows) && (column < columns) && (mine.chain[row].kind == answer.chain[column].kind))
            {
                if (!stretches.back().first.empty() || !stretches.back().second.empty())
                {
                    stretches.emplace_back();
                }
                row += 1u;
                column += 1u;
            }
            else if ((column < columns) && ((row == rows) || (common[row][column + 1u] >= common[row + 1u][column])))
            {
                answer_only.push_back(column);
                stretches.back().second.push_back(column);
                column += 1u;
            }
            else
            {
                mine_only.push_back(row);
                stretches.back().first.push_back(row);
                row += 1u;
            }
        }
        // a link of ours alone whose kind nvcc writes alone elsewhere in the question is moved, and the two stand for
        // each other; what is left of a stretch is its own, and a stretch where ours holds more of its own links than
        // nvcc's is where a fold takes steps away
        std::vector<int> moved(rows, 0);
        std::vector<int> paired(columns, 0);
        std::map<std::string, std::vector<size_t>> answer_kinds;
        for (const size_t column : answer_only)
        {
            answer_kinds[answer.chain[column].kind].push_back(column);
        }
        for (const size_t row : mine_only)
        {
            std::vector<size_t> &held = answer_kinds[mine.chain[row].kind];
            if (!held.empty())
            {
                moved[row] = 1;
                paired[held.front()] = 1;
                held.erase(held.begin());
            }
        }
        std::vector<int> folding(rows, 0);
        for (const auto &stretch : stretches)
        {
            const size_t mine_own = (size_t)std::count_if(stretch.first.begin(), stretch.first.end(),
                                                          [&](size_t row) { return !moved[row]; });
            const size_t answer_own = (size_t)std::count_if(stretch.second.begin(), stretch.second.end(),
                                                            [&](size_t column) { return !paired[column]; });
            for (const size_t row : stretch.first)
            {
                folding[row] = !moved[row] && (mine_own > answer_own);
            }
        }
        const char *verdict = "same";
        if (mine_only.empty() && answer_only.empty())
        {
            verdict = "alike";
            alike += 1u;
        }
        else if (columns < rows)
        {
            verdict = "fewer theirs";
            theirs_fewer += 1u;
            steps_to_learn += (unsigned int)(rows - columns);
        }
        else if (rows < columns)
        {
            verdict = "fewer ours";
            ours_fewer += 1u;
        }
        else
        {
            same += 1u;
        }
        fprintf(file, "question %s %s %u %u %u\n", mine.number.c_str(), verdict, (unsigned int)rows,
                (unsigned int)columns, common[0][0]);
        for (const size_t row : mine_only)
        {
            const Link &link = mine.chain[row];
            const std::string names = forms_writing(forms, link.kind);
            fprintf(file, "%s %s %s form %s%s\n", moved[row] ? "moved" : "ours", link.position.c_str(),
                    link.kind.c_str(), names.c_str(), folding[row] ? " fold" : "");
            // only a link of a stretch where ours holds more of its own links than nvcc's, in a question nvcc makes in
            // fewer steps, is a step a fold takes away
            if (!folding[row] || (columns >= rows))
            {
                continue;
            }
            std::stringstream split(names);
            std::string name;
            while (split >> name)
            {
                FormFolded &form = folded[name];
                form.questions.insert(mine.number);
                form.alone += 1u;
            }
        }
        for (const size_t column : answer_only)
        {
            fprintf(file, "theirs %s:%s %s\n", answer.number.c_str(), answer.chain[column].position.c_str(),
                    answer.chain[column].kind.c_str());
        }
    }
    std::vector<std::pair<std::string, FormFolded>> ranked(folded.begin(), folded.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const std::pair<std::string, FormFolded> &one, const std::pair<std::string, FormFolded> &other)
              {
                  return (one.second.questions.size() != other.second.questions.size())
                             ? (one.second.questions.size() > other.second.questions.size())
                             : (one.first < other.first);
              });
    for (const auto &form : ranked)
    {
        fprintf(file, "fold %s %u %u\n", form.first.c_str(), (unsigned int)form.second.questions.size(),
                form.second.alone);
    }
    fclose(file);
    printf("  %s: %u of ours, %u alike link for link, %u in as many steps, %u nvcc makes in fewer (%u steps to "
           "learn), %u ours makes in fewer, %u with no answer\n",
           path.c_str(), (unsigned int)ours.size(), alike, same, theirs_fewer, steps_to_learn, ours_fewer, unanswered);
    for (size_t at = 0u; (at < ranked.size()) && (at < 8u); at += 1u)
    {
        printf("    %s: %u questions, %u links alone\n", ranked[at].first.c_str(),
               (unsigned int)ranked[at].second.questions.size(), ranked[at].second.alone);
    }
    return 0;
}


// a field of an instruction as the system's .ksc gives it: its first bit and how many bits it takes
struct StallField
{
    unsigned int first;
    unsigned int bits;
};

// one question the walk puts: a chain of ours, its code with every stall the longest, and the place of the writer
// whose stall is turned, the instruction after it reading what it wrote
struct StallProbe
{
    std::string number;
    std::vector<unsigned char> code;
    size_t writer;
};

// the instructions' bits `field` takes at instruction `index` of `code`, sixteen bytes an instruction
static unsigned long long stall_bits(const std::vector<unsigned char> &code, size_t index, const StallField &field)
{
    unsigned long long value = 0ull;
    for (unsigned int bit = 0u; bit < field.bits; bit += 1u)
    {
        const unsigned int at = field.first + bit;
        value |= (unsigned long long)((code[(index * 16u) + (at / 8u)] >> (at % 8u)) & 1u) << bit;
    }
    return value;
}

// `value` written into the bits `field` takes at instruction `index` of `code`
static void stall_bits_write(std::vector<unsigned char> *code, size_t index, const StallField &field,
                             unsigned long long value)
{
    for (unsigned int bit = 0u; bit < field.bits; bit += 1u)
    {
        const unsigned int at = field.first + bit;
        unsigned char *const byte = &(*code)[(index * 16u) + (at / 8u)];
        const unsigned char mask = (unsigned char)(1u << (at % 8u));
        *byte = (unsigned char)(((value >> bit) & 1ull) ? (*byte | mask) : (*byte & ~mask));
    }
}

// the registers an operand names, a pair read whole where it is written .64, as a wide destination is
static std::set<std::string> stall_registers(const std::string &operand, int wide)
{
    static const std::regex s_named("\\b(R|P|UR)([0-9]+)\\b");
    std::set<std::string> named;
    for (std::sregex_iterator walk(operand.begin(), operand.end(), s_named); walk != std::sregex_iterator(); ++walk)
    {
        named.insert(walk->str());
        if (wide || (operand.find(".64") != std::string::npos))
        {
            named.insert((*walk)[1].str() + std::to_string(std::stoul((*walk)[2].str()) + 1ul));
        }
    }
    return named;
}

// a link's operation without its guard, and its operands split
static std::string stall_operation(const Link &link, std::vector<std::string> *operands)
{
    std::string text = link.exact;
    if (text[0] == '@')
    {
        text = trimmed(text.substr(text.find_first_of(" \t")));
    }
    const size_t space = text.find_first_of(" \t");
    operands->clear();
    if (space != std::string::npos)
    {
        std::stringstream pieces(text.substr(space));
        std::string piece;
        while (std::getline(pieces, piece, ','))
        {
            operands->push_back(trimmed(piece));
        }
    }
    return text.substr(0u, space);
}

// What the link writes: the registers its first operand names, a pair where the operation's result is wide. An address
// in that place writes nothing, and neither does a link that writes no register
static std::set<std::string> stall_written(const Link &link)
{
    std::vector<std::string> operands;
    const std::string operation = stall_operation(link, &operands);
    if (operands.empty() || (operands[0].find('[') != std::string::npos) || (operands[0] == "PT") ||
        (operands[0] == "RZ"))
    {
        return std::set<std::string>();
    }
    const int wide = (operation.find(".WIDE") != std::string::npos) || (operation.find(".64") != std::string::npos);
    return stall_registers(operands[0], wide);
}

// what the link reads: its guard, and every operand but a first it writes, a register a wide operation reads as data
// read as its pair
static std::set<std::string> stall_read(const Link &link)
{
    std::vector<std::string> operands;
    const std::string operation = stall_operation(link, &operands);
    const int wide = (operation.find(".64") != std::string::npos);
    std::set<std::string> read;
    if (link.exact[0] == '@')
    {
        read = stall_registers(link.exact.substr(0u, link.exact.find_first_of(" \t")), 0);
    }
    const size_t first = (stall_written(link).empty()) ? 0u : 1u;
    for (size_t at = first; at < operands.size(); at += 1u)
    {
        const int data = wide && (operands[at].find('[') == std::string::npos);
        const std::set<std::string> named = stall_registers(operands[at], data);
        read.insert(named.begin(), named.end());
    }
    return read;
}

// the fields the system's .ksc gives, by name
static std::map<std::string, StallField> stall_fields(const char *ksc)
{
    std::map<std::string, StallField> fields;
    std::ifstream file(ksc);
    std::string line;
    while (std::getline(file, line))
    {
        char name[64];
        StallField field = {0u, 0u};
        if (sscanf(line.c_str(), "field %63s %u %u", name, &field.first, &field.bits) == 3)
        {
            fields[name] = field;
        }
    }
    return fields;
}

// The cases a pair's product is put over where the cases a question holds leave it short of whole: each of the three
// operands put through the values every case is put through and `qualifiers`, the values the pair's forms write of
// their own, every combination of them, cut into runs the run channel holds. The run `run` of them into `question`,
// and the count of runs. The host computes none of them, and a product asks nothing of the host
static unsigned int whole_cases(RunQuestion *question, const std::set<unsigned long long> &qualifiers, unsigned int run)
{
    std::vector<unsigned long long> values(s_values, s_values + IDENTITY_VALUES);
    for (const unsigned long long value : qualifiers)
    {
        if (std::find(values.begin(), values.end(), value) == values.end())
        {
            values.push_back(value);
        }
    }
    const unsigned long long each = values.size();
    const unsigned long long count = each * each * each;
    const unsigned long long first = (unsigned long long)run * RUN_CASES_MOST;
    question->cases =
        (unsigned int)std::min<unsigned long long>(RUN_CASES_MOST, (count > first) ? (count - first) : 0u);
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const unsigned long long at = first + place;
        const unsigned long long left = values[(at / (each * each)) % each];
        const unsigned long long right = values[(at / each) % each];
        const unsigned long long third = values[at % each];
        const unsigned int words[RUN_IN_WORDS] = {(unsigned int)(left & 0xffffffffull),
                                                  (unsigned int)(left >> 32u),
                                                  (unsigned int)(right & 0xffffffffull),
                                                  (unsigned int)(right >> 32u),
                                                  (unsigned int)(third & 0xffffffffull),
                                                  (unsigned int)(third >> 32u),
                                                  0u,
                                                  0u};
        memcpy(question->word[place], words, sizeof(words));
    }
    return (unsigned int)((count + RUN_CASES_MOST - 1u) / RUN_CASES_MOST);
}

// The cases a question is put over: every case the host computes, spread evenly through them only where the run
// channel holds fewer. The words of each into `question`, and the host's place of each into `places`
static void stall_cases(RunQuestion *question, std::vector<unsigned int> *places)
{
    places->clear();
    const unsigned int count = (IDENTITY_CASES < RUN_CASES_MOST) ? IDENTITY_CASES : RUN_CASES_MOST;
    question->cases = count;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        const unsigned int at = (unsigned int)(((unsigned long long)place * IDENTITY_CASES) / count);
        const unsigned long long left = s_values[(at / IDENTITY_VALUES) % IDENTITY_VALUES];
        const unsigned long long right = s_values[at % IDENTITY_VALUES];
        const unsigned int words[RUN_IN_WORDS] = {(unsigned int)(left & 0xffffffffull), (unsigned int)(left >> 32u),
                                                  (unsigned int)(right & 0xffffffffull), (unsigned int)(right >> 32u),
                                                  (unsigned int)(at / (IDENTITY_VALUES * IDENTITY_VALUES)), 0u, 0u, 0u};
        memcpy(question->word[place], words, sizeof(words));
        places->push_back(at);
    }
}

// a 64-bit FNV-1a hash of `size` bytes at `bytes`, carried on from `hash`
static unsigned long long record_hash(const void *bytes, size_t size, unsigned long long hash)
{
    const unsigned char *const at = (const unsigned char *)bytes;
    for (size_t place = 0u; place < size; place += 1u)
    {
        hash = (hash ^ at[place]) * 0x100000001b3ull;
    }
    return hash;
}

// The record of the run channel's asks, the part's .ksc rows
//
//     run <answer> <word> ask <code> <registers> <threads> <blocks> <cases> <host> [<refusal>]
//
// each ask by the hashes of its code, of its cases and of the host's answers to them, and the registers and the shape
// it is put with, which together fix what it comes to. The word is the host's place of the first case apart, or
// ffffffff where every case is alike, and 0 where the part refused it or left nothing. An ask the record holds is not
// put again. A timed ask is put every time, each time a sample of its own, and an ask the gate holds is the host's
// and recorded nowhere. The record is read when a mode opens the run channel and written back when it ends
struct AskRecorded
{
    std::string answer;
    unsigned int word;
    std::string refusal;
};
static std::map<std::string, AskRecorded> s_record;
static std::string s_record_ksc;

// The part's query record, its .kqr beside its .ksc and of the same stem, held in memory for the run: every untimed
// ask a cycle put and what came back, by the question's identity, every timed ask a sample of its own, and the paths
// the last cycle read off them. Whatever asks an untimed ask again, in this cycle or a later one, is answered from it,
// and the part is asked it once; an ask that timed out is asked again until an answer resolves it, and an ask the gate
// censored is censored again without a process. It is read where R is read and written back where R is, and its path is
// empty where it could not be read, so that nothing writes over a record no mode read
static QueryRecord s_query_record;
static std::string s_query_record_path;

// The lines that place what a cycle puts in the record's order, its cycle and mode, its seed and its rounds, each
// written before the first new ask or sample that follows it and nowhere where none follows: a cycle the record
// answers whole adds no line to it
static std::vector<std::pair<std::string, std::string>> s_marks_pending;

// `kind words` held to be written before the next new ask or sample, a round in place of a round still pending
static void mark_pending(const std::string &kind, const std::string &words)
{
    if ((kind == "round") && !s_marks_pending.empty() && (s_marks_pending.back().first == "round"))
    {
        s_marks_pending.back().second = words;
        return;
    }
    s_marks_pending.push_back(std::make_pair(kind, words));
}

// the lines pending written to the record, a cycle numbered after every cycle it holds
static void marks_written(void)
{
    for (const auto &mark : s_marks_pending)
    {
        const std::string words = (mark.first == "cycle")
                                      ? (std::to_string(query_record_cycles(s_query_record) + 1u) + " " + mark.second)
                                      : mark.second;
        query_record_mark(&s_query_record, mark.first, words);
    }
    s_marks_pending.clear();
}
static int ksc_answers_write(const char *ksc, const std::string &asked, const std::vector<std::string> &rows);

// the record written back to the .ksc it was read from, every ask a row in the order of its keys
static void record_close(void)
{
    if (s_record_ksc.empty())
    {
        return;
    }
    std::vector<std::string> rows;
    for (const auto &recorded : s_record)
    {
        char word[16];
        snprintf(word, sizeof(word), "%08x", recorded.second.word);
        rows.push_back("run " + recorded.second.answer + " " + word + " ask " + recorded.first +
                       (recorded.second.refusal.empty() ? "" : (" " + recorded.second.refusal)));
    }
    if (!ksc_answers_write(s_record_ksc.c_str(), "ask", rows))
    {
        printf("  the record could not be written to %s\n", s_record_ksc.c_str());
    }
    s_record_ksc.clear();
    std::string error;
    if (!s_query_record_path.empty() && !query_record_write(s_query_record_path, s_query_record, &error))
    {
        printf("  %s\n", error.c_str());
    }
    s_query_record_path.clear();
}

// the record read from `ksc`, written back to it when the program ends, and the query record beside it, a cycle of
// mode `mode` pending in its order
static void record_open(const char *ksc, const char *mode)
{
    s_marks_pending.clear();
    mark_pending("cycle", mode);
    s_record.clear();
    s_record_ksc = ksc;
    std::ifstream in(ksc, std::ios::binary);
    std::string line;
    while (std::getline(in, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        std::stringstream words(line);
        std::string channel;
        std::string answer;
        std::string word;
        std::string asked;
        words >> channel >> answer >> word >> asked;
        if ((channel != "run") || (asked != "ask"))
        {
            continue;
        }
        std::string key;
        std::string part;
        for (unsigned int at = 0u; (at < 6u) && (words >> part); at += 1u)
        {
            key += ((at == 0u) ? "" : " ") + part;
        }
        std::string refusal;
        std::getline(words, refusal);
        refusal = (!refusal.empty() && (refusal[0] == ' ')) ? refusal.substr(1u) : refusal;
        s_record[key] = AskRecorded{answer, (unsigned int)std::stoul(word, nullptr, 16), refusal};
    }
    const std::string classified = ksc;
    const size_t stem_at = classified.find_last_of("/\\") + 1u;
    const size_t suffix_at = classified.find_last_of('.');
    std::string error;
    s_query_record_path = classified.substr(0u, suffix_at) + ".kqr";
    if (!query_record_read(s_query_record_path, classified.substr(stem_at, suffix_at - stem_at), &s_query_record,
                           &error))
    {
        printf("  %s: the query record is not read, and nothing is written to it\n", error.c_str());
        s_query_record = QueryRecord();
        s_query_record_path.clear();
    }
    static int s_registered = 0;
    if (s_registered == 0)
    {
        atexit(record_close);
        s_registered = 1;
    }
}

// The trace of every ask, where KLQ_TRACE names a file: the ask's number, the code's size, the registers, the shape
// and the launches it is put with and its count of cases, then what came back: alike, apart with the first case
// apart and both answers, the carrier's refusal, or nothing. NULL where nothing is traced
static thread_local FILE *s_ask_trace = NULL;

// The log beside the trace, KLQ_TRACE's name and .log, that klq_decoder reads: the cases as `cases <case>...`,
// written again where an ask is put over other cases, and each ask as `ask <number>` and what came back of every case
// in the order of the cases, `=` alike, `x` apart and `-` where the host gives no bracket, or `recorded` or `refused`
// where the part answered it nothing here. A mode writes what it put after the ask. NULL where nothing is traced. A
// pair put in a thread of its own writes its trace and its log to files of its own, each added whole to these once
// the pair is put, so that every put stands in one piece
static thread_local FILE *s_ask_log = NULL;
static FILE *s_trace_whole = NULL;
static FILE *s_log_whole = NULL;

// the index of the pair this thread puts, and -1 on the thread that carries the asks, and 1 where the stand-in the
// pair puts now is dangerous
static thread_local int t_pair = -1;
static thread_local int t_dangerous = 0;

// the trace and its log opened where KLQ_TRACE names them
static void ask_files_opened(void)
{
    if ((getenv("KLQ_TRACE") == NULL) || (s_ask_trace != NULL))
    {
        return;
    }
    s_ask_trace = fopen(getenv("KLQ_TRACE"), "wb");
    s_ask_log = fopen((std::string(getenv("KLQ_TRACE")) + ".log").c_str(), "wb");
    s_trace_whole = s_ask_trace;
    s_log_whole = s_ask_log;
}

// `file` flushed where every ask is to reach the disk as it is written: on the thread that carries the asks, and not
// in a pair's own files, which are added whole once the pair is put
static void ask_flushed(FILE *file)
{
    if (t_pair < 0)
    {
        fflush(file);
    }
}

static std::string case_text(unsigned int at);

// the host's place of the first case the last ask answered apart at, from the part or from the record
static thread_local unsigned int s_apart_at = 0xffffffffu;

// One ask a pair's thread hands to the round, 1 where it is dangerous: its stand-in reads or writes memory through an
// address, or sends control elsewhere, and it is carried in a process of its own
struct CarriedAsk
{
    RunQuestion *question;
    int dangerous;
};

// The round: each pair put in a thread of its own runs while it is its turn, one at a time in the order of the bridge,
// until it asks or is put. Every ask a pair hands over waits for the round, and once every pair left has asked, the
// round's asks are carried together: the mundane in one process, each dangerous one in a process of its own. No ask
// waits on another ask of the same round, since a pair asks its next only once its last is answered. `s_turn` is the
// pair that runs, and -1 the thread that carries the asks
static std::mutex s_turn_lock;
static std::condition_variable s_turn_changed;
static int s_turn = -1;
static std::vector<std::vector<CarriedAsk>> s_round_asks;

// what the rounds asked: the asks handed over, those answered from what was held, and the processes that carried
// the rest
static unsigned long long s_round_handed = 0ull;
static unsigned long long s_round_held = 0ull;
static unsigned long long s_round_processes = 0ull;

// a question's identity: a hash of its code, its registers, its shape and launches, and a hash of its cases
static std::string question_identity(const RunQuestion *question)
{
    unsigned long long code = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < question->code_size; at += 1ull)
    {
        code = (code ^ question->code[at]) * 0x100000001b3ull;
    }
    unsigned long long cases = 0xcbf29ce484222325ull;
    const unsigned char *const words = (const unsigned char *)question->word;
    const size_t bytes = (size_t)question->cases * sizeof(question->word[0]);
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        cases = (cases ^ words[at]) * 0x100000001b3ull;
    }
    char identity[96];
    snprintf(identity, sizeof(identity), "%016llx %u %u %u %u %u %016llx", code, question->registers, question->threads,
             question->blocks, question->launches, question->cases, cases);
    return identity;
}

// `asked` answered from `held`
static void answer_given(RunQuestion *asked, const QueryRecordAsk &held)
{
    asked->outcome = (held.answer == "answers")    ? (unsigned int)RUN_ANSWERED
                     : (held.answer == "illegal")  ? (unsigned int)RUN_ILLEGAL
                     : (held.answer == "censored") ? (unsigned int)RUN_HELD
                                                   : (unsigned int)RUN_NOTHING;
    snprintf(asked->refused, sizeof(asked->refused), "%s", held.refusal.c_str());
    for (size_t place = 0u; (place < held.words.size()) && (place < RUN_CASES_MOST); place += 1u)
    {
        asked->answered[place] = held.words[place];
    }
}

// what came back of `question` as the query record writes it: answers, illegal, nothing, timed_out where the
// watchdog ended it before it came back, or censored where the gate held it off the part to protect the whole; empty
// where the channel never carried it
static std::string answer_recorded(const RunQuestion *question)
{
    if (question->outcome == RUN_ANSWERED)
    {
        return "answers";
    }
    if (question->outcome == RUN_ILLEGAL)
    {
        return "illegal";
    }
    if (question->outcome == RUN_HELD)
    {
        return "censored";
    }
    if (question->outcome != RUN_NOTHING)
    {
        return std::string();
    }
    return (strstr(question->refused, interface_ending_name(INTERFACE_ENDING_OUT_OF_TIME)) != NULL) ? "timed_out"
                                                                                                    : "nothing";
}

// The asks `carried` answered. A timed ask is put every time and kept as a sample of its own, with its cost. An
// untimed one is answered from the query record where its identity was asked before and did not time out, and the
// rest are carried, one ask of an identity the round holds twice: the mundane together, and alone each dangerous one
// and each one of a shape of its own, which a process of many does not take. Each answer the part gave, and each ask
// the gate censored, kept in the record after by its identity, an answer in place of the ask that timed out
static void asks_answered(const std::vector<CarriedAsk> &carried)
{
    std::vector<RunQuestion *> mundane;
    std::vector<RunQuestion *> alone;
    std::vector<RunQuestion *> timed;
    std::vector<std::string> identities(carried.size());
    std::map<std::string, RunQuestion *> first_of;
    std::vector<std::string> firsts;
    std::map<RunQuestion *, unsigned int> waiting;
    std::vector<std::pair<RunQuestion *, RunQuestion *>> copies;
    for (size_t at = 0u; at < carried.size(); at += 1u)
    {
        RunQuestion *const question = carried[at].question;
        identities[at] = question_identity(question);
        s_round_handed += 1ull;
        if (question->launches != 0u)
        {
            timed.push_back(question);
            continue;
        }
        const QueryRecordAsk *const held = query_record_find(s_query_record, identities[at]);
        if ((held != NULL) && (held->answer != "timed_out"))
        {
            answer_given(question, *held);
            s_round_held += 1ull;
            continue;
        }
        const auto first = first_of.find(identities[at]);
        if (first != first_of.end())
        {
            copies.push_back(std::make_pair(question, first->second));
            waiting[first->second] += 1u;
            continue;
        }
        first_of[identities[at]] = question;
        firsts.push_back(identities[at]);
        const int shaped = (question->threads != 0u) || (question->blocks != 0u);
        ((carried[at].dangerous != 0) || shaped ? alone : mundane).push_back(question);
        waiting[question] = 1u;
    }
    // the ask that settles the most entries first: the one the most pairs of the round wait on, in the order handed
    // where two settle as many
    const auto settles_more = [&waiting](RunQuestion *left, RunQuestion *right) {
        return waiting[left] > waiting[right];
    };
    std::stable_sort(mundane.begin(), mundane.end(), settles_more);
    std::stable_sort(alone.begin(), alone.end(), settles_more);
    if (!mundane.empty())
    {
        run_channel_ask_many(mundane.data(), (unsigned int)mundane.size());
        s_round_processes += 1ull;
    }
    for (RunQuestion *const question : alone)
    {
        if ((question->threads != 0u) || (question->blocks != 0u))
        {
            run_channel_ask(question);
        }
        else
        {
            RunQuestion *const one[1] = {question};
            run_channel_ask_many(one, 1u);
        }
        s_round_processes += 1ull;
    }
    for (RunQuestion *const question : timed)
    {
        run_channel_ask(question);
        s_round_processes += 1ull;
        const std::string answer = answer_recorded(question);
        if (!answer.empty())
        {
            marks_written();
            query_record_sample(&s_query_record,
                                QueryRecordSample{question_identity(question), answer, question->nanoseconds,
                                                  (answer == "answers") ? std::string() : question->refused});
        }
    }
    for (const std::string &identity : firsts)
    {
        RunQuestion *const question = first_of[identity];
        QueryRecordAsk held;
        held.identity = identity;
        held.answer = answer_recorded(question);
        if (held.answer.empty())
        {
            continue;
        }
        if (held.answer == "answers")
        {
            held.words.assign(question->answered, question->answered + question->cases);
        }
        else
        {
            held.refusal = question->refused;
        }
        if (query_record_find(s_query_record, identity) == NULL)
        {
            marks_written();
        }
        query_record_keep(&s_query_record, held);
    }
    for (const auto &copy : copies)
    {
        copy.first->outcome = copy.second->outcome;
        snprintf(copy.first->refused, sizeof(copy.first->refused), "%s", copy.second->refused);
        memcpy(copy.first->answered, copy.second->answered,
               (size_t)copy.first->cases * sizeof(copy.first->answered[0]));
    }
}

// The asks `asked`, `count` of them, each dangerous where `dangerous` says, answered through the query record: on a
// pair's own thread handed to the round and waited on, and on the thread that carries the asks answered at once. A
// timed ask, the only one a mode means to ask again, is put every time
static void asks_carried(RunQuestion *const *asked, const int *dangerous, unsigned int count)
{
    std::vector<CarriedAsk> carried;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        carried.push_back(CarriedAsk{asked[at], (dangerous != NULL) ? dangerous[at] : 0});
    }
    if (t_pair < 0)
    {
        asks_answered(carried);
        return;
    }
    std::unique_lock<std::mutex> lock(s_turn_lock);
    s_round_asks[(size_t)t_pair] = carried;
    s_turn = -1;
    s_turn_changed.notify_all();
    const int pair = t_pair;
    s_turn_changed.wait(lock, [pair]() { return s_turn == pair; });
}

// The host's answer to a case as a bracket [down, up] around the exact value at the result's width, and whether the
// part's answer `answered` falls in it: 1 where it does, 0 where it does not, and -1 where the host gives no bracket.
// An integer case is a bracket of one word, `down`, and holds where the part answers that word. A floating case is a
// bracket of two ends, `down:up`, the two values of the result's type nearest the exact one, and holds where the part
// answers either end, the end it takes being its rounding. A case the host's C leaves undefined is `-` and gates
// nothing
static int bracket_holds(const std::string &host, unsigned long long answered)
{
    if (host == "-")
    {
        return -1;
    }
    const size_t colon = host.find(':');
    const unsigned long long down = std::stoull(host.substr(0u, colon), nullptr, 16);
    const unsigned long long up = (colon == std::string::npos) ? down : std::stoull(host.substr(colon + 1u), nullptr, 16);
    return ((answered == down) || (answered == up)) ? 1 : 0;
}

// 1 where the part answers `code`, put as `question` says of its registers, shape and launches, alike with the host
// on every case it computes, 0 where it answers apart, refuses it or the gate holds it, each counted in `asks`
static int question_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                          const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks)
{
    static int s_opened = 0;
    if (s_opened == 0)
    {
        ask_files_opened();
    }
    s_opened = 1;
    question->code = code.data();
    question->code_size = code.size();
    *asks += 1ull;
    // the ask's key in the record, and what the record holds of it
    unsigned long long hosted = 0xcbf29ce484222325ull;
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        hosted = record_hash(host[places[place]].c_str(), host[places[place]].size() + 1u, hosted);
    }
    char keyed[160];
    snprintf(keyed, sizeof(keyed), "%016llx %u %u %u %016llx %016llx",
             record_hash(code.data(), code.size(), 0xcbf29ce484222325ull), question->registers, question->threads,
             question->blocks, record_hash(question->word, sizeof(question->word[0]) * question->cases, 0xcbf29ce484222325ull),
             hosted);
    const std::string key = keyed;
    // R holds an ask's verdict and its first case apart, and the query record and the log what came back of every
    // case: with the log kept, an ask is read from the query record or put to the part, and R written as ever
    const int recordable = !s_record_ksc.empty() && (question->launches == 0u);
    const auto recorded = (recordable && (s_ask_log == NULL)) ? s_record.find(key) : s_record.end();
    if (recorded != s_record.end())
    {
        const AskRecorded &held = recorded->second;
        question->outcome = (held.answer == "answers") ? RUN_ANSWERED : (held.answer == "illegal") ? RUN_ILLEGAL : RUN_NOTHING;
        snprintf(question->refused, sizeof(question->refused), "%s", held.refusal.c_str());
        const int alike = (held.answer == "answers") && (held.word == 0xffffffffu);
        s_apart_at = (held.answer == "answers") ? held.word : 0xffffffffu;
        if (s_ask_trace != NULL)
        {
            fprintf(s_ask_trace, "ask %llu: recorded %s %08x %s\n", *asks, held.answer.c_str(), held.word, key.c_str());
            ask_flushed(s_ask_trace);
        }
        if (s_ask_log != NULL)
        {
            fprintf(s_ask_log, "ask %llu recorded\n", *asks);
        }
        return alike;
    }
    if (s_ask_trace != NULL)
    {
        fprintf(s_ask_trace, "ask %llu: %llu bytes, %u registers, %u threads in %u blocks, %u launches, %u cases: ",
                *asks, question->code_size, question->registers, question->threads, question->blocks,
                question->launches, question->cases);
    }
    RunQuestion *const asked[1] = {question};
    asks_carried(asked, &t_dangerous, 1u);
    if (question->outcome != RUN_ANSWERED)
    {
        if (s_ask_trace != NULL)
        {
            fprintf(s_ask_trace, "outcome %u, %s\n", question->outcome, question->refused);
            ask_flushed(s_ask_trace);
        }
        if (s_ask_log != NULL)
        {
            fprintf(s_ask_log, "ask %llu refused\n", *asks);
        }
        if (recordable && ((question->outcome == RUN_ILLEGAL) || (question->outcome == RUN_NOTHING)))
        {
            s_record[key] = AskRecorded{(question->outcome == RUN_ILLEGAL) ? "illegal" : "nothing", 0u,
                                        question->refused};
        }
        return 0;
    }
    if (s_ask_log != NULL)
    {
        // the places this thread's log last wrote as its cases, by a hash of them and their count, a count past every
        // question's where it wrote none
        static thread_local unsigned long long t_logged_hash = 0ull;
        static thread_local unsigned int t_logged_count = 0xffffffffu;
        const unsigned long long asked_hash =
            record_hash(places.data(), sizeof(places[0]) * question->cases, 0xcbf29ce484222325ull);
        if ((asked_hash != t_logged_hash) || (question->cases != t_logged_count))
        {
            fprintf(s_ask_log, "cases");
            for (unsigned int place = 0u; place < question->cases; place += 1u)
            {
                fprintf(s_ask_log, " %s", case_text(places[place]).c_str());
            }
            fprintf(s_ask_log, "\n");
            t_logged_hash = asked_hash;
            t_logged_count = question->cases;
        }
        std::string came_back;
        for (unsigned int place = 0u; place < question->cases; place += 1u)
        {
            const int holds = bracket_holds(host[places[place]], question->answered[place]);
            came_back += (holds < 0) ? '-' : (holds != 0) ? '=' : 'x';
        }
        fprintf(s_ask_log, "ask %llu %s\n", *asks, came_back.c_str());
    }
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const std::string &expected = host[places[place]];
        if (bracket_holds(expected, question->answered[place]) == 0)
        {
            if (s_ask_trace != NULL)
            {
                fprintf(s_ask_trace, "apart at case %u, the host %s and the part %llx\n", places[place],
                        expected.c_str(), question->answered[place]);
                ask_flushed(s_ask_trace);
            }
            if (recordable)
            {
                s_record[key] = AskRecorded{"answers", places[place], ""};
            }
            s_apart_at = places[place];
            return 0;
        }
    }
    if (recordable)
    {
        s_record[key] = AskRecorded{"answers", 0xffffffffu, ""};
    }
    if (s_ask_trace != NULL)
    {
        fprintf(s_ask_trace, "alike%s\n",
                (question->launches != 0u) ? (", " + std::to_string(question->nanoseconds) + " ns").c_str() : "");
        ask_flushed(s_ask_trace);
    }
    return 1;
}

// 1 where the part answers `code` alike with the host on every case it computes, in a container declaring every
// register its file holds (sm_86.kdm), 0 where it answers apart, refuses it or the gate holds it, each counted in
// `asks`
static int stall_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                       const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks)
{
    question->registers = 255u;
    return question_alike(code, host, places, question, asks);
}

// each chain's answers the host computes, one a case over every case, by the chain's number
static std::map<std::string, std::vector<std::string>> host_answers_read(const char *answers)
{
    std::map<std::string, std::vector<std::string>> host;
    std::ifstream host_file(answers);
    std::string line;
    while (std::getline(host_file, line))
    {
        std::stringstream words(line);
        std::string number;
        words >> number;
        std::vector<std::string> case_answers;
        std::string answer;
        while (words >> answer)
        {
            case_answers.push_back(answer);
        }
        if (case_answers.size() == IDENTITY_CASES)
        {
            host[number] = case_answers;
        }
    }
    return host;
}

// chain `number` of ours, read from `engine`: its code, sixteen bytes an instruction, and the links its listing gives.
// 0 where either is missing or the listing gives more links than the code holds instructions
static int chain_code_read(const char *engine, const std::string &number, std::vector<unsigned char> *code,
                           std::vector<Link> *links)
{
    const std::string base = std::string(engine) + "/" + number;
    std::ifstream listing(base + ".dis");
    std::ifstream binary(base + ".bin", std::ios::binary);
    if (!listing || !binary)
    {
        return 0;
    }
    code->assign((std::istreambuf_iterator<char>(binary)), std::istreambuf_iterator<char>());
    links->clear();
    std::string line;
    while (std::getline(listing, line))
    {
        Link link;
        const int read = link_read(line, &link);
        if (read < 0)
        {
            break;
        }
        if ((read > 0) && !link.position.empty())
        {
            links->push_back(link);
        }
    }
    return !links->empty() && ((code->size() % 16u) == 0u) && ((links->size() * 16u) <= code->size());
}

// the registers chain `number` of ours names, its high water mark as `engine` gives it, or 0 where it gives none. The
// part refuses a container declaring the mark alone where the code names a register within the part's own past the
// last it gives code, and the count a container declares is walked up from the mark until the part answers
static unsigned int chain_registers(const char *engine, const std::string &number)
{
    std::ifstream file(std::string(engine) + "/" + number + ".registers");
    unsigned int registers = 0u;
    return (file >> registers) ? registers : 0u;
}

// The .ksc rewritten with `rows` in place of every row of the run channel it held to a question beginning with the
// words `asked`, whatever came back, the rows put after its last row of the run channel and its counts of the run
// channel taken again. 1, or 0 where the .ksc could not be written
static int ksc_answers_write(const char *ksc, const std::string &asked, const std::vector<std::string> &rows)
{
    std::ifstream in(ksc, std::ios::binary);
    std::vector<std::string> kept;
    std::string line;
    while (std::getline(in, line))
    {
        const std::string plain = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        if (plain.compare(0u, 4u, "run ") == 0)
        {
            std::stringstream words(plain);
            std::string channel;
            std::string answered;
            std::string word;
            std::string question;
            words >> channel >> answered >> word;
            std::getline(words, question);
            question = (!question.empty() && (question[0] == ' ')) ? question.substr(1u) : question;
            // a row is the question's where its question begins with `asked`, whole words of it
            if ((question.compare(0u, asked.size(), asked) == 0) &&
                ((question.size() == asked.size()) || (question[asked.size()] == ' ')))
            {
                continue;
            }
        }
        kept.push_back(plain);
    }
    in.close();
    size_t after = kept.size();
    for (size_t at = 0u; at < kept.size(); at += 1u)
    {
        after = (kept[at].compare(0u, 4u, "run ") == 0) ? (at + 1u) : after;
    }
    kept.insert(kept.begin() + (std::ptrdiff_t)after, rows.begin(), rows.end());
    std::map<std::string, unsigned int> counted;
    for (const std::string &kept_line : kept)
    {
        std::stringstream words(kept_line);
        std::string channel;
        std::string answered;
        words >> channel >> answered;
        counted[answered] += (channel == "run") ? 1u : 0u;
    }
    FILE *const out = fopen(ksc, "wb");
    if (out == NULL)
    {
        return 0;
    }
    for (const std::string &kept_line : kept)
    {
        std::stringstream words(kept_line);
        std::string count_word;
        std::string channel;
        std::string answered;
        words >> count_word >> channel >> answered;
        if ((count_word == "count") && (channel == "run"))
        {
            fprintf(out, "count run      %-8s %u\n", answered.c_str(), counted[answered]);
            continue;
        }
        fprintf(out, "%s\n", kept_line.c_str());
    }
    fclose(out);
    return 1;
}

// The soonest each operation's result is read, asked of the part and walked down. Every chain of ours the host also
// computes is a probe where an instruction that writes a register is read by the very next: with every stall the
// longest, the chain answers alike with the host, and each stall the walk puts at the writer is truthy while every
// probe of its pair answers alike and falsy once one answers apart. The walk starts at the longest, which is truthy,
// and steps down one at a time until a stall is falsy; the soonest is the last truthy one, since a stall longer than
// a truthy one is truthy too. An instruction that sets a write barrier is read behind the barrier and not behind its
// stall, and is not walked. Each pair's soonest is written to the .ksc as the part's answer on the run channel,
// `run answers <stall> stall <writer> <reader>`, in place of those it held, and <folder>/stall.txt holds every pair
// with its probes and its asks
static int identity_stall(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                          const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("write_barrier") == 0u))
    {
        printf("klq_identity stall: %s gives no stall field or no write barrier field\n", ksc);
        return 1;
    }
    const StallField stall = fields["stall"];
    const StallField barrier = fields["write_barrier"];
    const unsigned long long longest = (1ull << stall.bits) - 1ull;
    const unsigned long long none = (1ull << barrier.bits) - 1ull;
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    if (!run_channel_open(carrier, folder.c_str(), 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "stall");
    static RunQuestion s_question;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    // the probes of each pair, a writer's operation and its reader's
    std::map<std::pair<std::string, std::string>, std::vector<StallProbe>> pairs;
    unsigned int chains = 0u;
    unsigned int unanswered = 0u;
    for (const auto &held : host)
    {
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if (!chain_code_read(engine, held.first, &code, &links))
        {
            continue;
        }
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            stall_bits_write(&code, index, stall, longest);
        }
        // a chain is a probe only where, with every stall the longest, the part answers it alike with the host
        chains += 1u;
        if (!stall_alike(code, held.second, places, &s_question, &asks))
        {
            unanswered += 1u;
            printf("  %s: not alike at the longest stalls (%s), and no probe\n", held.first.c_str(), s_question.refused);
            continue;
        }
        for (size_t index = 0u; (index + 1u) < links.size(); index += 1u)
        {
            const size_t place = (size_t)std::stoul(links[index].position, nullptr, 16) / 16u;
            if (stall_bits(code, place, barrier) != none)
            {
                continue;
            }
            const std::set<std::string> written = stall_written(links[index]);
            const std::set<std::string> read = stall_read(links[index + 1u]);
            const int reads = std::any_of(written.begin(), written.end(),
                                          [&](const std::string &name) { return read.count(name) != 0u; });
            if (!reads)
            {
                continue;
            }
            std::vector<std::string> operands;
            const std::pair<std::string, std::string> pair(stall_operation(links[index], &operands),
                                                           stall_operation(links[index + 1u], &operands));
            pairs[pair].push_back(StallProbe{held.first, code, place});
        }
    }
    printf("klq_identity stall: %u chains, %u not alike at the longest stalls, %zu pairs, carried by %s\n", chains,
           unanswered, pairs.size(), run_channel_carrier());
    // the probes a pair is walked over at most, each from a chain of its own
    const size_t most = 3u;
    std::vector<std::string> rows;
    FILE *const table = fopen((folder + "/stall.txt").c_str(), "wb");
    if (table == NULL)
    {
        printf("klq_identity stall: %s/stall.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    for (const auto &pair : pairs)
    {
        std::vector<const StallProbe *> probes;
        std::set<std::string> numbers;
        for (const StallProbe &probe : pair.second)
        {
            if ((probes.size() < most) && numbers.insert(probe.number).second)
            {
                probes.push_back(&probe);
            }
        }
        unsigned long long soonest = longest;
        const unsigned long long before = asks;
        for (unsigned long long step = longest; step-- > 0ull;)
        {
            int truthy = 1;
            for (const StallProbe *probe : probes)
            {
                std::vector<unsigned char> turned = probe->code;
                stall_bits_write(&turned, probe->writer, stall, step);
                truthy = truthy && stall_alike(turned, host[probe->number], places, &s_question, &asks);
                if (!truthy)
                {
                    break;
                }
            }
            if (!truthy)
            {
                break;
            }
            soonest = step;
        }
        std::string over;
        for (const StallProbe *probe : probes)
        {
            over += " " + probe->number;
        }
        fprintf(table, "%s %s soonest %llu, %llu asks, over%s\n", pair.first.first.c_str(), pair.first.second.c_str(),
                soonest, asks - before, over.c_str());
        printf("  %s then %s: soonest %llu (%llu asks)\n", pair.first.first.c_str(), pair.first.second.c_str(), soonest,
               asks - before);
        char row[256];
        snprintf(row, sizeof(row), "run answers %08llx stall %s %s", soonest, pair.first.first.c_str(),
                 pair.first.second.c_str());
        rows.push_back(row);
    }
    fclose(table);
    run_channel_close();
    if (!ksc_answers_write(ksc, "stall", rows))
    {
        printf("klq_identity stall: %s could not be written\n", ksc);
        return 1;
    }
    printf("klq_identity stall: %zu pairs walked over %llu asks, written to %s\n", pairs.size(), asks, ksc);
    return 0;
}

// one question the register walk puts: a chain of ours, its code with every stall the longest, the register it
// writes that the walk renames, and every number its register fields hold
// The one walk over numbers a relation holds at on one side of a bound and not past it: the last register, the fewest
// registers and each knee are read by it. `holds` answers 1 where the relation holds at a number, 0 where it does not,
// and -1 at a number that is not the relation's to answer, which the walk passes over. Stepping: from `from` by `step`
// as far as `end`, the first number the relation holds at, or `end` + `step` where it holds at none
static long long walk_step(const std::function<int(long long)> &holds, long long from, long long step, long long end)
{
    for (long long number = from; number != (end + step); number += step)
    {
        if (holds(number) == 1)
        {
            return number;
        }
    }
    return end + step;
}

// Halving: `truthy` a number the relation holds at and `falsy` one it does not, on either side of it, each moved to
// the middle the relation gives the same answer as until the two are adjacent. The truthy end, the last number the
// relation holds at before the bound
static long long walk_halved(const std::function<int(long long)> &holds, long long truthy, long long falsy)
{
    while (((truthy - falsy) > 1) || ((falsy - truthy) > 1))
    {
        const long long middle = truthy + ((falsy - truthy) / 2);
        if (holds(middle) == 1)
        {
            truthy = middle;
        }
        else
        {
            falsy = middle;
        }
    }
    return truthy;
}

struct RegisterProbe
{
    std::string number;
    std::vector<unsigned char> code;
    unsigned long long renamed;
    std::set<unsigned long long> named;
};

// `code` with every register field that holds `from` set to `to`
static std::vector<unsigned char> register_renamed(const std::vector<unsigned char> &code,
                                                   const std::vector<StallField> &registers, unsigned long long from,
                                                   unsigned long long to)
{
    std::vector<unsigned char> turned = code;
    for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
    {
        for (const StallField &field : registers)
        {
            if (stall_bits(code, index, field) == from)
            {
                stall_bits_write(&turned, index, field, to);
            }
        }
    }
    return turned;
}

// The last register a question's code can name, asked of the part and walked down. A chain of ours the host also
// computes is a probe where, with every stall the longest, it answers alike with the host, and still does with a
// register it writes and then reads renamed, in every register field, to the lowest number those fields do not
// hold. The walk
// starts at the highest number a register field holds and steps down one at a time, skipping a number a probe holds:
// a number is truthy where every probe answers alike with its register renamed to it, and falsy where one answers
// apart, is refused, or is held by the gate. The last register is the first truthy number, and every number under
// it is the code's to name. It is written to the .ksc as the part's answer on the run channel,
// `run answers <last> register last`, and <folder>/register.txt holds every number asked with its verdict
static int identity_register(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                             const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("rd") == 0u) || (fields.count("ra") == 0u) ||
        (fields.count("rb") == 0u))
    {
        printf("klq_identity register: %s gives no stall field or not every register field\n", ksc);
        return 1;
    }
    const StallField stall = fields["stall"];
    const std::vector<StallField> registers = {fields["rd"], fields["ra"], fields["rb"]};
    const unsigned long long longest = (1ull << stall.bits) - 1ull;
    const unsigned long long highest = (1ull << fields["rd"].bits) - 1ull;
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    if (!run_channel_open(carrier, folder.c_str(), 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "register");
    static RunQuestion s_question;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    // the probes the walk is put over at most, each from a chain of its own
    const size_t most = 3u;
    std::vector<RegisterProbe> probes;
    for (const auto &held : host)
    {
        if (probes.size() >= most)
        {
            break;
        }
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if (!chain_code_read(engine, held.first, &code, &links))
        {
            continue;
        }
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            stall_bits_write(&code, index, stall, longest);
        }
        if (!stall_alike(code, held.second, places, &s_question, &asks))
        {
            continue;
        }
        RegisterProbe probe{held.first, code, highest, {}};
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            for (const StallField &field : registers)
            {
                probe.named.insert(stall_bits(code, index, field));
            }
        }
        unsigned long long lowest = 0ull;
        while ((lowest < highest) && (probe.named.count(lowest) != 0u))
        {
            lowest += 1ull;
        }
        // the registers the chain writes and reads after, in the order it writes them, each tried until one renamed
        // to the lowest number the chain does not hold still answers alike: a register written and never read again
        // asks whether a number is written, and not whether it holds what is written
        for (size_t index = 0u; (index < (code.size() / 16u)) && (probe.renamed == highest); index += 1u)
        {
            const unsigned long long written = stall_bits(code, index, registers[0]);
            int read_after = 0;
            for (size_t after = index + 1u; after < (code.size() / 16u); after += 1u)
            {
                read_after = read_after || (stall_bits(code, after, registers[1]) == written) ||
                             (stall_bits(code, after, registers[2]) == written);
            }
            if ((written == highest) || (lowest == highest) || !read_after ||
                !stall_alike(register_renamed(code, registers, written, lowest), held.second, places, &s_question,
                             &asks))
            {
                continue;
            }
            probe.renamed = written;
        }
        if (probe.renamed != highest)
        {
            printf("  probe %s: R%llu renamed\n", held.first.c_str(), probe.renamed);
            probes.push_back(probe);
        }
    }
    if (probes.empty())
    {
        printf("klq_identity register: no chain answers alike with a register renamed, and no probe\n");
        run_channel_close();
        return 1;
    }
    FILE *const table = fopen((folder + "/register.txt").c_str(), "wb");
    if (table == NULL)
    {
        printf("klq_identity register: %s/register.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    // a number is the walk's to ask where no probe holds it
    const auto renamed_alike = [&](long long number) -> int {
        const int held_by_probe = std::any_of(probes.begin(), probes.end(), [&](const RegisterProbe &probe) {
            return probe.named.count((unsigned long long)number) != 0u;
        });
        if (held_by_probe)
        {
            return -1;
        }
        int truthy = 1;
        for (const RegisterProbe &probe : probes)
        {
            truthy = truthy && stall_alike(register_renamed(probe.code, registers, probe.renamed,
                                                            (unsigned long long)number),
                                           host[probe.number], places, &s_question, &asks);
        }
        fprintf(table, "R%lld %s%s%s\n", number, truthy ? "truthy" : "falsy", truthy ? "" : ": ",
                truthy ? "" : s_question.refused);
        printf("  R%lld: %s\n", number, truthy ? "truthy" : "falsy");
        return truthy;
    };
    const long long last = walk_step(renamed_alike, (long long)highest, -1, 0);
    fclose(table);
    run_channel_close();
    if (last < 0)
    {
        printf("klq_identity register: no number answers alike, and no last register\n");
        return 1;
    }
    char row[64];
    snprintf(row, sizeof(row), "run answers %08llx register last", (unsigned long long)last);
    if (!ksc_answers_write(ksc, "register", {row}))
    {
        printf("klq_identity register: %s could not be written\n", ksc);
        return 1;
    }
    printf("klq_identity register: the last register R%lld, over %llu asks, written to %s\n", last, asks, ksc);
    return 0;
}

// the threads every point of a curve launches in all, and the launches its time is taken over; the asks in a row the
// band of the fewest registers' times holds without widening before it is taken as sustained, and the most asks one
// band or one point's bounces is given
#define CURVE_THREADS (1u << 20u)
#define CURVE_LAUNCHES 100u
#define CURVE_SUSTAIN 5u
#define CURVE_ASKS_MOST 64u

// the times a launch of one point takes over every ask of it: the least and the most
struct CurveBand
{
    unsigned long long low;
    unsigned long long high;
};

// `band` widened to hold `time`: 1 where it had to widen, 0 where it held `time` already
static int curve_band_widened(CurveBand *band, unsigned long long time)
{
    const int widened = (time < band->low) || (time > band->high);
    band->low = std::min(band->low, time);
    band->high = std::max(band->high, time);
    return widened;
}

// one point of a curve asked of the part: `code` declaring `registers`, in blocks of `threads`, as many blocks as
// CURVE_THREADS takes, timed over CURVE_LAUNCHES. 1 where it answers alike with the host, its time a launch into
// `nanoseconds`; 0 where it answers apart or the part refuses it. Every point asked is a line of `table`
static int curve_point(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                       const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks,
                       const std::string &task, unsigned int threads, unsigned int registers,
                       unsigned long long *nanoseconds, FILE *table)
{
    question->registers = registers;
    question->threads = threads;
    question->blocks = CURVE_THREADS / threads;
    question->launches = CURVE_LAUNCHES;
    const int alike = question_alike(code, host, places, question, asks);
    *nanoseconds = alike ? (question->nanoseconds / CURVE_LAUNCHES) : 0ull;
    fprintf(table, "%s threads %u blocks %u registers %u: %s %llu ns a launch%s%s\n", task.c_str(), threads,
            question->blocks, registers, alike ? "alike" : "apart", *nanoseconds, alike ? "" : ", ",
            alike ? "" : question->refused);
    return alike;
}

// The cost of registers to a task, asked of the part by timing it. A task is a chain of ours the host also computes,
// with every stall the longest. For each count of threads a block holds, from 1 and doubling, the task is launched
// over CURVE_THREADS threads in all, and its curve is its time against the registers its container declares: from the
// fewest it answers alike with to the most the register field names. The fewest is asked in blocks of one thread,
// walked up from the registers the chain names, or where it gives none from one past the highest number its
// register fields hold, until it answers alike and halved down from there, and is the task's at every count of
// threads. The fewest's band is the least and the most of its times, asked
// again until it holds CURVE_SUSTAIN asks in a row without widening. A count of registers is truthy where the task
// answers alike and its time a launch keeps within the band, and falsy where it answers apart, the part refuses the
// launch, or its time is past the band twice running with the fewest bounced between and holding to the band; where
// the bounced fewest strays past the band itself, the band widens and the count is asked again. Declaring more
// registers takes residency and never gives it: every count past a falsy one is falsy, and the knee, the most
// registers truthy, is found by halving. The counts of threads end where the part answers no count of registers. Each
// knee is written to the .ksc as the part's answer on the run channel, `run answers <registers> curve <task>
// <threads>`, and each task's count of threads whose fewest registers run soonest as `run answers <threads> curve
// <task> threads`; <folder>/curve.txt holds every point asked
static int identity_curve(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                          const std::vector<std::string> &tasks, const char *const *carrier)
{
    std::map<std::string, StallField> fields = stall_fields(ksc);
    if ((fields.count("stall") == 0u) || (fields.count("rd") == 0u) || (fields.count("ra") == 0u) ||
        (fields.count("rb") == 0u))
    {
        printf("klq_identity curve: %s gives no stall field or not every register field\n", ksc);
        return 1;
    }
    const StallField stall = fields["stall"];
    const std::vector<StallField> registers = {fields["rd"], fields["ra"], fields["rb"]};
    const unsigned long long longest = (1ull << stall.bits) - 1ull;
    const unsigned int highest = (unsigned int)((1ull << fields["rd"].bits) - 1ull);
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    if (!run_channel_open(carrier, folder.c_str(), 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "curve");
    FILE *const table = fopen((folder + "/curve.txt").c_str(), "wb");
    if (table == NULL)
    {
        printf("klq_identity curve: %s/curve.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    static RunQuestion s_question;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    std::vector<std::string> rows;
    for (const std::string &task : tasks)
    {
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if ((host.count(task) == 0u) || !chain_code_read(engine, task, &code, &links))
        {
            printf("  %s: no chain of ours the host computes\n", task.c_str());
            continue;
        }
        unsigned int named = 0u;
        for (size_t index = 0u; index < (code.size() / 16u); index += 1u)
        {
            stall_bits_write(&code, index, stall, longest);
            for (const StallField &field : registers)
            {
                const unsigned int number = (unsigned int)stall_bits(code, index, field);
                named = ((number != highest) && (number > named)) ? number : named;
            }
        }
        unsigned int soonest_threads = 0u;
        unsigned long long soonest = 0ull;
        // the walk for the fewest starts at the registers the chain names where the engine gives them
        const unsigned int mark = chain_registers(engine, task);
        unsigned int fewest = (mark != 0u) ? mark : (named + 1u);
        for (unsigned int threads = 1u; threads <= CURVE_THREADS; threads *= 2u)
        {
            // the fewest registers the task answers alike with, walked up in blocks of one thread until the part says
            // yes; the registers code names are its own whatever its blocks hold, and a count of threads the part
            // refuses at the fewest it refuses at every count, since more registers take more of it
            std::map<long long, unsigned long long> times;
            const auto declaring_alike = [&](long long count) -> int {
                unsigned long long time = 0ull;
                const int alike = curve_point(code, host[task], places, &s_question, &asks, task, threads,
                                              (unsigned int)count, &time, table);
                times[count] = time;
                return alike;
            };
            int answered = 0;
            if (threads == 1u)
            {
                // a register field holds an immediate's bits as well as a register's number: the count it answers at
                // is a bound, and the fewest is found under it by halving. Code that answers alike declaring some
                // count answers alike declaring more, in a block of one thread that every count fits
                const long long found = walk_step(declaring_alike, (long long)fewest, 1, (long long)highest);
                answered = (found <= (long long)highest);
                fewest = answered ? (unsigned int)walk_halved(declaring_alike, found, 0) : fewest;
            }
            else
            {
                answered = declaring_alike((long long)fewest);
            }
            const unsigned long long first = times[(long long)fewest];
            if (!answered)
            {
                printf("  %s: blocks of %u threads answer alike at no count of registers\n", task.c_str(), threads);
                break;
            }
            // the band of the fewest registers' times, asked again until it holds CURVE_SUSTAIN asks in a row without
            // widening
            CurveBand band{first, first};
            unsigned int held = 0u;
            for (unsigned int asked = 0u; (held < CURVE_SUSTAIN) && (asked < CURVE_ASKS_MOST); asked += 1u)
            {
                unsigned long long again = 0ull;
                if (curve_point(code, host[task], places, &s_question, &asks, task, threads, fewest, &again, table))
                {
                    held = curve_band_widened(&band, again) ? 0u : (held + 1u);
                }
            }
            // a point inside the band is truthy, and one that answers apart or is refused is falsy. A point past the
            // band is bounced off the fewest: where the fewest strays past the band too, the band widens and the point
            // is asked again against it; where the fewest holds, a point past the band twice running is falsy
            const auto within_band = [&](long long count) -> int {
                unsigned int past = 0u;
                for (unsigned int asked = 0u; asked < CURVE_ASKS_MOST; asked += 1u)
                {
                    unsigned long long time = 0ull;
                    if (!curve_point(code, host[task], places, &s_question, &asks, task, threads, (unsigned int)count,
                                     &time, table))
                    {
                        return 0;
                    }
                    if (time <= band.high)
                    {
                        return 1;
                    }
                    past += 1u;
                    if (past == 2u)
                    {
                        return 0;
                    }
                    unsigned long long bounced = 0ull;
                    if (curve_point(code, host[task], places, &s_question, &asks, task, threads, fewest, &bounced,
                                    table) &&
                        curve_band_widened(&band, bounced))
                    {
                        past = 0u;
                    }
                }
                return 0;
            };
            const unsigned int truthy = (unsigned int)walk_halved(within_band, (long long)fewest, (long long)highest + 1);
            printf("  %s, blocks of %u threads: %llu to %llu ns a launch at %u registers, the knee at %u\n",
                   task.c_str(), threads, band.low, band.high, fewest, truthy);
            char row[128];
            snprintf(row, sizeof(row), "run answers %08x curve %s %u", truthy, task.c_str(), threads);
            rows.push_back(row);
            if ((soonest_threads == 0u) || (band.low < soonest))
            {
                soonest_threads = threads;
                soonest = band.low;
            }
        }
        if (soonest_threads != 0u)
        {
            printf("  %s: soonest in blocks of %u threads, %llu ns a launch\n", task.c_str(), soonest_threads, soonest);
            char row[128];
            snprintf(row, sizeof(row), "run answers %08x curve %s threads", soonest_threads, task.c_str());
            rows.push_back(row);
        }
    }
    fclose(table);
    run_channel_close();
    // each task's rows in place of that task's alone, every other task's curve kept
    for (const std::string &task : tasks)
    {
        const std::string asked = "curve " + task;
        std::vector<std::string> own;
        std::copy_if(rows.begin(), rows.end(), std::back_inserter(own), [&](const std::string &row) {
            return row.find(" " + asked + " ") != std::string::npos;
        });
        if (!ksc_answers_write(ksc, asked, own))
        {
            printf("klq_identity curve: %s could not be written\n", ksc);
            return 1;
        }
    }
    printf("klq_identity curve: %zu tasks over %llu asks, %zu answers written to %s\n", tasks.size(), asks,
           rows.size(), ksc);
    return 0;
}

// The queue over the stick's questions (P13, the scheduler). Every question of the manifest is an entry of its
// category, and R gives each what it reads: 1 where the part answers it as the host does on every case the host
// computes it on, 0 where it answers one case otherwise or refuses it, and gray where nothing has settled it. A
// gray entry carries what keeps it gray: the engine's note where the engine writes no chain for it, that the host
// computes no answer to it, or that the gate holds it off the part. A gray entry with a chain and a host answer is
// put through the run channel, every case at once in a container declaring every register its file holds, and its
// answer is written to R. A question R has settled is never put again. Each ask settles one entry and no other:
// the questions of the stick share no case, and the order the entries are put in settles no more of them than another
// order would. <folder>/queue.txt holds every entry with its category, what it reads and what keeps it gray, and each
// category's entries are counted on the standard output as a row of P12's table
static int identity_queue(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                          const char *manifest, const char *const *carrier)
{
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    // the engine's answer to each question and its note where it writes no chain
    std::map<std::string, std::string> notes;
    std::ifstream written(std::string(engine) + "/engine.tsv", std::ios::binary);
    std::string line;
    while (std::getline(written, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        std::vector<std::string> cells;
        std::stringstream row(line);
        std::string cell;
        while (std::getline(row, cell, '\t'))
        {
            cells.push_back(cell);
        }
        if ((cells.size() >= 2u) && (cells[1] != "answered") && (cells[0] != "number"))
        {
            notes[cells[0]] = (cells.size() >= 5u) ? cells[4] : cells[1];
        }
    }
    std::vector<std::pair<std::string, std::string>> entries;
    std::ifstream listed(manifest, std::ios::binary);
    while (std::getline(listed, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        const size_t first_tab = line.find('\t');
        const size_t second_tab = line.find('\t', first_tab + 1u);
        if ((first_tab == std::string::npos) || (second_tab == std::string::npos) || (line.compare(0u, 6u, "number") == 0))
        {
            continue;
        }
        entries.emplace_back(line.substr(0u, first_tab), line.substr(first_tab + 1u, second_tab - first_tab - 1u));
    }
    if (entries.empty())
    {
        printf("klq_identity queue: %s lists no question\n", manifest);
        return 1;
    }
    if (!run_channel_open(carrier, folder.c_str(), 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "queue");
    FILE *const table = fopen((folder + "/queue.txt").c_str(), "wb");
    if (table == NULL)
    {
        printf("klq_identity queue: %s/queue.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    static RunQuestion s_question;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    // each category's entries counted by what came of them, in the order the manifest first names the category
    std::vector<std::string> categories;
    std::map<std::string, std::map<std::string, unsigned int>> counted;
    for (const auto &entry : entries)
    {
        const std::string &number = entry.first;
        const std::string &category = entry.second;
        if (counted.count(category) == 0u)
        {
            categories.push_back(category);
        }
        counted[category]["questions"] += 1u;
        std::string reads = "gray";
        std::string reason;
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if ((notes.count(number) != 0u) || !chain_code_read(engine, number, &code, &links))
        {
            reason = (notes.count(number) != 0u) ? notes[number] : "the engine writes no chain";
        }
        else if (host.count(number) == 0u)
        {
            counted[category]["written"] += 1u;
            reason = "the host computes no answer";
        }
        else
        {
            counted[category]["written"] += 1u;
            counted[category]["held"] += 1u;
            const int alike = stall_alike(code, host[number], places, &s_question, &asks);
            reads = alike ? "1" : (s_question.outcome == RUN_HELD) ? "gray" : "0";
            reason = alike ? "alike"
                     : (s_question.outcome == RUN_ANSWERED) ? "apart"
                     : (s_question.outcome == RUN_HELD)     ? std::string("the gate holds it: ") + s_question.refused
                                                            : std::string("refused: ") + s_question.refused;
            counted[category]["alike"] += alike ? 1u : 0u;
        }
        counted[category][reads] += 1u;
        fprintf(table, "%s\t%s\t%s\t%s\n", number.c_str(), category.c_str(), reads.c_str(), reason.c_str());
    }
    fclose(table);
    run_channel_close();
    printf("| category | questions | written | held | alike | 1 | 0 | gray |\n");
    std::map<std::string, unsigned int> all;
    for (const std::string &category : categories)
    {
        std::map<std::string, unsigned int> &row = counted[category];
        printf("| %s | %u | %u | %u | %u | %u | %u | %u |\n", category.c_str(), row["questions"], row["written"],
               row["held"], row["alike"], row["1"], row["0"], row["gray"]);
        for (const char *const column : {"questions", "written", "held", "alike", "1", "0", "gray"})
        {
            all[column] += row[column];
        }
    }
    printf("| all | %u | %u | %u | %u | %u | %u | %u |\n", all["questions"], all["written"], all["held"], all["alike"],
           all["1"], all["0"], all["gray"]);
    printf("klq_identity queue: %llu asks, %u read 1, %u read 0, %u gray\n", asks, all["1"], all["0"], all["gray"]);
    return (all["0"] == 0u) ? 0 : 1;
}

// a case of the host, its operands and its third, as an identity's verdict writes it
static std::string case_text(unsigned int at)
{
    const unsigned int third = at / (IDENTITY_VALUES * IDENTITY_VALUES);
    const unsigned int left = (at / IDENTITY_VALUES) % IDENTITY_VALUES;
    const unsigned int right = at % IDENTITY_VALUES;
    char written[96];
    snprintf(written, sizeof(written), "%llx,%llx,%x", s_values[left], s_values[right], third);
    return written;
}

// The bridge's identities between texts held on the part (P13, the identities). Each side of
// `text_identity <text> = <text>` is a question of the sides' stick, its chain the engine's and its answers the host's,
// found by its text in the sides' manifest. An identity closes at the first case the host answers its two sides apart
// on, the texts then two meanings, and at the first case the part answers a side apart from the host on, the sides
// then not held to each other there. Where the host answers the two sides alike on every case it computes them on and
// the part answers each as the host does, the identity is open with its count of cases, every case both sides are
// computed on. A side with no chain, no host answer, or that the part refuses or the gate holds is not asked, and its
// identity is open with 0. Each identity's verdict is written beneath it in the bridge in place of the one it held
static int identity_text(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                         const char *klq, const char *manifest, const char *const *carrier)
{
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    std::map<std::string, std::string> numbered;
    std::ifstream listed(manifest, std::ios::binary);
    std::string line;
    while (std::getline(listed, line))
    {
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        const size_t first_tab = line.find('\t');
        const size_t second_tab = line.find('\t', first_tab + 1u);
        if ((first_tab != std::string::npos) && (second_tab != std::string::npos))
        {
            numbered[line.substr(second_tab + 1u)] = line.substr(0u, first_tab);
        }
    }
    std::vector<std::string> bridge;
    std::ifstream held(klq, std::ios::binary);
    while (std::getline(held, line))
    {
        bridge.push_back((!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line);
    }
    held.close();
    if (!run_channel_open(carrier, folder.c_str(), 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "text_identity");
    static RunQuestion s_question;
    std::vector<unsigned int> places;
    stall_cases(&s_question, &places);
    unsigned long long asks = 0ull;
    // a side put to the part: 1 where it answers as the host does on every case, 0 where it answers one apart, its
    // place in s_apart_at, and -1 where it is not asked
    const auto side_alike = [&](const std::string &number) -> int {
        std::vector<unsigned char> code;
        std::vector<Link> links;
        if ((host.count(number) == 0u) || !chain_code_read(engine, number, &code, &links))
        {
            return -1;
        }
        const int alike = stall_alike(code, host[number], places, &s_question, &asks);
        return alike ? 1 : (s_question.outcome == RUN_ANSWERED) ? 0 : -1;
    };
    std::vector<std::string> written;
    unsigned int open = 0u;
    unsigned int closed = 0u;
    unsigned int unasked = 0u;
    // every verdict line the bridge held beneath an identity between texts, however many, gives way to the one written
    int beneath = 0;
    for (size_t at = 0u; at < bridge.size(); at += 1u)
    {
        const std::string &entry = bridge[at];
        const int verdict_line = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        beneath = (entry.rfind("text_identity ", 0u) == 0u) || (beneath && verdict_line);
        if (verdict_line && beneath)
        {
            continue;
        }
        written.push_back(entry);
        if (entry.rfind("text_identity ", 0u) != 0u)
        {
            continue;
        }
        const std::string sides = entry.substr(14u);
        const size_t equals = sides.find(" = ");
        const std::string one = (equals == std::string::npos) ? sides : sides.substr(0u, equals);
        const std::string other = (equals == std::string::npos) ? std::string() : sides.substr(equals + 3u);
        const std::string first = (numbered.count(one) != 0u) ? numbered[one] : std::string();
        const std::string second = (numbered.count(other) != 0u) ? numbered[other] : std::string();
        std::string verdict = "open 0";
        if (!first.empty() && !second.empty() && (host.count(first) != 0u) && (host.count(second) != 0u))
        {
            unsigned int alike = 0u;
            std::string apart;
            for (const unsigned int place : places)
            {
                const std::string &left = host[first][place];
                const std::string &right = host[second][place];
                if ((left == "-") || (right == "-"))
                {
                    continue;
                }
                if (left != right)
                {
                    apart = "closed " + case_text(place) + "->" + left + "," + right;
                    break;
                }
                alike += 1u;
            }
            const int first_alike = apart.empty() ? side_alike(first) : -1;
            const unsigned int first_apart = s_apart_at;
            const int second_alike = (apart.empty() && (first_alike >= 0)) ? side_alike(second) : -1;
            const unsigned int second_apart = s_apart_at;
            if (!apart.empty())
            {
                verdict = apart;
            }
            else if ((first_alike == 0) || (second_alike == 0))
            {
                const unsigned int place = (first_alike == 0) ? first_apart : second_apart;
                verdict = "closed " + case_text(place) + "->" + host[first][place] + ", the part apart on " +
                          ((first_alike == 0) ? one : other);
            }
            else if ((first_alike == 1) && (second_alike == 1))
            {
                verdict = "open " + std::to_string(alike);
            }
        }
        open += (verdict.rfind("open ", 0u) == 0u) && (verdict != "open 0") ? 1u : 0u;
        closed += (verdict.rfind("closed ", 0u) == 0u) ? 1u : 0u;
        unasked += (verdict == "open 0") ? 1u : 0u;
        if (s_ask_trace != NULL)
        {
            fprintf(s_ask_trace, "text_identity %s [%s %s]: %s\n", sides.c_str(), first.c_str(), second.c_str(),
                    verdict.c_str());
            ask_flushed(s_ask_trace);
        }
        written.push_back(verdict);
    }
    run_channel_close();
    FILE *const file = fopen(klq, "wb");
    if (file == NULL)
    {
        printf("klq_identity text_identity: %s could not be written\n", klq);
        return 1;
    }
    for (const std::string &kept : written)
    {
        fprintf(file, "%s\n", kept.c_str());
    }
    fclose(file);
    printf("klq_identity text_identity: %llu asks, %u open, %u closed, %u not asked, written to %s\n", asks, open,
           closed, unasked, klq);
    return 0;
}

// The way an ask enters the part, its vector: the registers it declares, the lanes it is put over, a case a lane, and
// the operands of a case its code loads. An ask is put at the least magnitude, the registers its chain names as
// `mark`, the engine's high water mark. Where the part refuses it there and answers it declaring every register its
// file holds, the fewest it answers at is walked between the two, and the ask stands at the fewest; where the part
// refuses it at both, the refusal is not the registers', and it stands refused. 1 where the part answers it alike with
// the host on every case it computes, 0 otherwise, its registers in `question`
static int vector_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
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
static std::string carrier_cut_at_read(const std::vector<CarrierLink> &links, long cases_defined,
                                       const std::string &text, long link_at, long form_lines, const std::string &read,
                                       const std::string &standing = std::string(), int wide = 0)
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
static std::string read_shape(const std::vector<CarrierLink> &links, const std::vector<std::string> &lines,
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
static std::vector<std::string> form_tokens(const std::string &text)
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
// part of a pair stands against the same part of a marker, and against `RZ`, whose every part is zero. 1 where every
// other token is the same: `to` is `from` with the parameters `fixed` names bound, one structure
static int form_unified(const std::string &to_text, const std::string &from_text, std::map<char, std::string> *bound,
                        std::map<char, std::string> *fixed)
{
    const std::vector<std::string> to_tokens = form_tokens(to_text);
    const std::vector<std::string> from_tokens = form_tokens(from_text);
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
            const int parts_stand = from_marker ? (to_part == from_part) : (to_part.empty() || (from_token == "RZ"));
            if (!parts_stand || ((bound->count(to_token[1]) != 0u) && ((*bound)[to_token[1]] != standing)))
            {
                return 0;
            }
            (*bound)[to_token[1]] = standing;
            continue;
        }
        if (from_marker)
        {
            if ((!from_part.empty() && (to_token != "RZ")) ||
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
static int form_launched(const std::string &text)
{
    static const std::regex s_launch("SR_(TID|CTAID|NTID|NCTAID)|c\\[0x0\\]\\[0x(0|4|8|c|10|14)\\]");
    return std::regex_search(text, s_launch) ? 1 : 0;
}

// the conditions the comparisons of the form `text` test, each the word after a `SETP`'s first `.`
static std::set<std::string> form_conditions(const std::string &text)
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
static int form_controls(const std::string &text)
{
    int controls = 0;
    for (const std::vector<std::string> &instruction : form_instructions(text))
    {
        controls |= opcode_controls(instruction[0]);
    }
    return controls;
}

// 1 where the form `text` reads or writes memory through an address, or sends control elsewhere: a stand-in of it is
// carried in a process of its own. A bank of constants, `c[...]`, is read at a place no case moves, and is no address
static int form_dangerous(const std::string &text)
{
    for (const std::vector<std::string> &instruction : form_instructions(text))
    {
        if (opcode_controls(instruction[0]))
        {
            return 1;
        }
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

// 1 where the form `text` computes a value from its operands
static int form_operates(const std::string &text)
{
    int operates = 0;
    for (const std::vector<std::string> &instruction : form_instructions(text))
    {
        operates |= opcode_operates(instruction[0]);
    }
    return operates;
}

// 1 where every instruction of the form `text` gives one value with its first two sources taken either way round
static int form_commutes(const std::string &text)
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
static int form_verbs(const std::string &text)
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
static int form_const_loaded(const std::string &first, const std::string &second)
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
static int form_switches(const std::string &text)
{
    const std::vector<std::string> tokens = form_tokens(text);
    return !tokens.empty() && (std::find(tokens.begin(), tokens.end(), ";") == tokens.end());
}

// 1 where a parameter of the form `text` is named at its `.hi`: the form works a wide value in its pieces, the low
// word and the high
static int form_ranged(const std::string &text)
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

// The cuts `codes` put together over the cases `question` holds, each dangerous where `dangerous` says: each container
// declaring `mark` registers, and every register its file holds where the part refuses that, those put together in
// the round after. Each cut's question into `asked`, in the order of `codes`, its outcome, answers and refusal its own,
// and a cut with no code left unasked. Each ask counted in `asks`. The answers are values the host does not compute,
// held in the log alone and never in R
static void cuts_asked(const std::vector<std::vector<unsigned char>> &codes, const std::vector<int> &dangerous,
                       const RunQuestion *question, unsigned long long *asks, unsigned int mark,
                       std::vector<RunQuestion> *asked)
{
    const unsigned int most = 255u;
    asked->assign(codes.size(), RunQuestion());
    std::vector<RunQuestion *> carried;
    std::vector<int> carried_dangerous;
    for (size_t at = 0u; at < codes.size(); at += 1u)
    {
        if (codes[at].empty())
        {
            continue;
        }
        RunQuestion *const cut = &(*asked)[at];
        cut->code = codes[at].data();
        cut->code_size = codes[at].size();
        cut->registers = ((mark != 0u) && (mark < most)) ? mark : most;
        cut->cases = question->cases;
        memcpy(cut->word, question->word, (size_t)question->cases * sizeof(question->word[0]));
        carried.push_back(cut);
        carried_dangerous.push_back(dangerous[at]);
    }
    *asks += carried.size();
    if (!carried.empty())
    {
        asks_carried(carried.data(), carried_dangerous.data(), (unsigned int)carried.size());
    }
    std::vector<RunQuestion *> again;
    std::vector<int> again_dangerous;
    for (size_t at = 0u; at < carried.size(); at += 1u)
    {
        if ((carried[at]->outcome != RUN_ANSWERED) && (carried[at]->registers != most))
        {
            carried[at]->registers = most;
            again.push_back(carried[at]);
            again_dangerous.push_back(carried_dangerous[at]);
        }
    }
    *asks += again.size();
    if (!again.empty())
    {
        asks_carried(again.data(), again_dangerous.data(), (unsigned int)again.size());
    }
}

// The bridge's pairs put to the part, `pair <form> <form>` in Lstar.klq: two forms whose sameness the text leaves
// open. A pair is put in a carrier, a chain of ours the host computes that holds one form's text as sass.krs writes it:
// the form's arguments are read off the chain's lines, the other form is written in its place with the arguments of
// its parameters' names, and the chain is assembled as the engine assembles it and put to the part over every case
// the host computes the carrier on. The pair is open with its count of cases where the part answers the changed chain
// as the host answers the carrier on every one, and closed at the carrier's address, the question and the position of
// the link, and the first case it answers apart. Each form is tried in the other's place, the first in the second's
// first; a pair neither form can stand in for in any carrier, the second naming a parameter the first does not or
// neither's text standing in a chain, and a pair the part refuses in every carrier, is open with 0. Each verdict is
// written beneath its pair in the bridge in place of the one it held. Where the log is kept, each put is followed by
// the values the form reads there on every case, the carrier cut at the form, for klq_decoder to read its product off
static int identity_pair(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                         const char *klq, const char *machine_path, const char *const *carrier)
{
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    const Ruleset *const sass = code_generator("sass.krs").ruleset(1);
    static SassMachine s_machine;
    if ((sass == NULL) || (sass_machine_read(&s_machine, machine_path) == 0))
    {
        printf("klq_identity pair: sass.krs or the machine file %s is not read\n", machine_path);
        return 1;
    }
    // every carrier's text, by its question
    std::map<std::string, std::string> texts;
    for (const auto &held : host)
    {
        std::ifstream chain(std::string(engine) + "/" + held.first + ".sass", std::ios::binary);
        if (chain)
        {
            texts[held.first] = std::string((std::istreambuf_iterator<char>(chain)), std::istreambuf_iterator<char>());
        }
    }
    // every carrier's links, read for what each writes, reads and loads, and the link writing the register its cases
    // are loaded through
    std::map<std::string, std::pair<std::vector<CarrierLink>, long>> flows;
    for (const auto &chain : texts)
    {
        long cases_defined = -2;
        const std::vector<CarrierLink> links = carrier_read(chain.second, &cases_defined);
        flows[chain.first] = std::make_pair(links, cases_defined);
    }
    // each carrier's vector at its least: the registers its chain names, then the operands of a case it loads
    std::map<std::string, std::pair<unsigned int, unsigned int>> magnitudes;
    for (const auto &flow : flows)
    {
        magnitudes[flow.first] =
            std::make_pair(chain_registers(engine, flow.first),
                           (unsigned int)carrier_operands_loaded(flow.second.first, flow.second.second).size());
    }
    // the seed the order of the links of one magnitude is drawn from, with each put's two forms, KLQ_SEED where it is
    // given
    const unsigned long seed = (getenv("KLQ_SEED") != NULL) ? std::stoul(getenv("KLQ_SEED")) : 1ul;
    std::vector<std::string> bridge;
    std::string line;
    std::ifstream held_bridge(klq, std::ios::binary);
    while (std::getline(held_bridge, line))
    {
        bridge.push_back((!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line);
    }
    held_bridge.close();
    if (!run_channel_open(carrier, folder.c_str(), 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "pair");
    mark_pending("seed", std::to_string(seed));
    // the asks the query record held before this cycle, read with R
    const size_t recorded_before = s_query_record.asks.size();
    // each pair's question and places, held with the pair and reached from its own thread, since a pair waits on its
    // round with both
    static thread_local RunQuestion *t_question = NULL;
    static thread_local std::vector<unsigned int> *t_places = NULL;
    const std::unique_ptr<RunQuestion> stalled(new RunQuestion());
    std::vector<unsigned int> stalled_places;
    stall_cases(stalled.get(), &stalled_places);
    // the values the cases are made of: the three operands of every case, as the log's cases write them
    std::vector<unsigned long long> operands;
    for (unsigned int place = 0u; place < stalled->cases; place += 1u)
    {
        for (unsigned int operand = 0u; operand < 3u; operand += 1u)
        {
            operands.push_back(((unsigned long long)stalled->word[place][2u * operand + 1u] << 32u) |
                               stalled->word[place][2u * operand]);
        }
    }
    const std::vector<std::set<unsigned long long>> cased = concept_values(operands);
    unsigned long long asks = 0ull;
    const auto none = [](const std::string &) { return std::string(); };
    // form `form` as sass.krs writes it with each parameter a marker, and its parameters' names; empty where it gives
    // no form of the name
    const auto marked = [&](const std::string &form, std::vector<std::string> *names) -> std::string {
        *names = ruleset_parameters(sass, form);
        std::vector<std::string> markers;
        for (size_t at = 0u; at < names->size(); at += 1u)
        {
            markers.push_back(std::string("\x01") + (char)('A' + at) + "\x01");
        }
        std::string written;
        return (ruleset_opcode(sass, form, markers, none, written) != 0) ? written : std::string();
    };
    // `from` stood in for by `to` at each of its links in each carrier, until the part answers one apart: 1 with its
    // verdict, closed at the first link apart or open with the cases alike at every link the part answered, 0 where the
    // part answered none
    const auto put = [&](const std::string &from, const std::string &to, std::string *verdict) -> int {
        RunQuestion &question = *t_question;
        std::vector<unsigned int> &places = *t_places;
        std::vector<std::string> from_names;
        std::vector<std::string> to_names;
        const std::string from_text = marked(from, &from_names);
        const std::string to_text = marked(to, &to_names);
        // the stand-in read against the form token by token: what the form writes in place of each of the stand-in's
        // parameters, and each of the form's parameters the stand-in writes a literal in place of
        std::map<char, std::string> bound;
        std::map<char, std::string> fixed;
        const int unified = form_unified(to_text, from_text, &bound, &fixed);
        // a const load and a plain one: the first link alike is the part's answer, and the question stays open for
        // the chain to settle, or for a lower identity to witness where the rest of the chain fails
        const int const_loaded = form_const_loaded(from_text, to_text);
        // the form's parameters the stand-in reads: by name, or in place of one of its own
        std::set<std::string> read_by_to(to_names.begin(), to_names.end());
        for (const auto &each : bound)
        {
            if (unified && (each.second[0] == '\x01') && ((size_t)(each.second[1] - 'A') < from_names.size()))
            {
                read_by_to.insert(from_names[(size_t)(each.second[1] - 'A')]);
            }
        }
        // the values the two forms write of their own, as the ruleset gives them, and the values the pair's product is
        // whole over with them
        std::set<unsigned long long> qualifiers = carrier_literals(from_text);
        for (const unsigned long long value : carrier_literals(to_text))
        {
            qualifiers.insert(value);
        }
        const std::vector<std::set<unsigned long long>> pair_cased = concept_values_qualified(cased, qualifiers);
        if (from_text.empty() || to_text.empty())
        {
            return 0;
        }
        // the pattern of the form's lines, each marker an operand, the same marker the same operand
        std::string pattern;
        std::map<char, unsigned int> group;
        for (size_t at = 0u; at < from_text.size(); at += 1u)
        {
            if ((from_text[at] == '\x01') && ((at + 2u) < from_text.size()) && (from_text[at + 2u] == '\x01'))
            {
                const char marker = from_text[at + 1u];
                if (group.count(marker) == 0u)
                {
                    const unsigned int next = (unsigned int)group.size() + 1u;
                    group[marker] = next;
                    pattern += "([^,;\\s]+?)";
                }
                else
                {
                    pattern += "\\" + std::to_string(group[marker]);
                }
                at += 2u;
                continue;
            }
            const char letter = from_text[at];
            const int special = (letter != '\0') && (strchr("\\^$.|?*+()[]{}", letter) != NULL);
            pattern += special ? (std::string("\\") + letter) : std::string(1u, letter);
        }
        const std::regex lines(pattern);
        // each link the form stands at, in each carrier, with its vector: the carrier's delta before the form, the
        // links between a case's load and the form, then the carrier's magnitude
        struct FoundLink
        {
            const decltype(texts)::value_type *chain;
            std::smatch found;
            std::pair<long, std::pair<unsigned int, unsigned int>> vector;
        };
        std::vector<FoundLink> found_links;
        for (const auto &chain : texts)
        {
            const std::sregex_iterator end;
            for (std::sregex_iterator link_found(chain.second.begin(), chain.second.end(), lines); link_found != end;
                 ++link_found)
            {
                const auto &flow = flows[chain.first];
                const long link_at = (long)std::count(chain.second.begin(),
                                                      chain.second.begin() + link_found->position(0), '\n');
                const long delta = carrier_form_delta(flow.first, flow.second, link_found->str(0), link_at);
                found_links.push_back(FoundLink{&chain, *link_found,
                                                std::make_pair((delta < 0) ? LONG_MAX : delta, magnitudes[chain.first])});
            }
        }
        // the links in the order of their vectors, the least first: a form the cases reach as they were loaded answers
        // of the form alone. Links of one vector are tried in an order drawn from the seed and this put's two forms
        // alone, so that no other pair, put or settled, moves the order of this one
        std::stable_sort(found_links.begin(), found_links.end(),
                         [](const FoundLink &left, const FoundLink &right) { return left.vector < right.vector; });
        const std::string drawn_for = from + " " + to;
        const unsigned long long drawn_hash = record_hash(drawn_for.c_str(), drawn_for.size(), seed);
        std::seed_seq drawn_seed{(unsigned int)(drawn_hash & 0xffffffffull), (unsigned int)(drawn_hash >> 32u),
                                 (unsigned int)seed};
        std::mt19937 drawn(drawn_seed);
        for (size_t first = 0u; first < found_links.size();)
        {
            size_t last = first;
            while ((last < found_links.size()) && (found_links[last].vector == found_links[first].vector))
            {
                last += 1u;
            }
            std::shuffle(found_links.begin() + (long)first, found_links.begin() + (long)last, drawn);
            first = last;
        }
        // the pair's product over every put, the shapes of what the form read at each, and its verdict where a put
        // closed it
        unsigned int alike_cases = 0u;
        int answered = 0;
        ConceptProduct product;
        std::set<std::string> shapes;
        std::string closed;
        long band = LONG_MIN;
        int band_truthy = 0;
        for (const FoundLink &found_link : found_links)
        {
            const auto &chain = *found_link.chain;
            const std::smatch &found = found_link.found;
            std::map<std::string, std::string> given;
            for (const auto &each : group)
            {
                const size_t parameter = (size_t)(each.first - 'A');
                if (parameter < from_names.size())
                {
                    given[from_names[parameter]] = found[each.second].str();
                }
            }
            // a carrier is one that reads what the form writes: its first parameter, the register it writes, named
            // again after it, a pair's high word read where the pair is named. A link nothing reads answers alike
            // whatever stands in for it
            const std::string written = (group.count('A') != 0u) ? found[group['A']].str() : std::string();
            const std::string written_register = written.substr(0u, written.find('.'));
            const std::string after = chain.second.substr((size_t)(found.position(0) + found.length(0)));
            if (written_register.empty() ||
                !std::regex_search(after, std::regex("(^|[^A-Za-z0-9_])" + written_register + "([^0-9]|$)")))
            {
                continue;
            }
            // a link of the carrier's own, the thread's index, the case's index, the bound or an address, moves the case
            // every thread reads: a form is put only where its link carries the cases (carrier_flow.h)
            const auto &flow = flows[chain.first];
            const long link_at = (long)std::count(chain.second.begin(), chain.second.begin() + found.position(0), '\n');
            if (!carrier_form_carries(flow.first, flow.second, found.str(0), link_at))
            {
                continue;
            }
            // each of the stand-in's parameters given by name, or where a name is not given, by what the form writes
            // in its place, token by token: an argument of the form, or a literal the form writes there
            std::vector<std::string> arguments;
            for (size_t parameter = 0u; parameter < to_names.size(); parameter += 1u)
            {
                const std::string &name = to_names[parameter];
                const auto aligned = bound.find((char)('A' + parameter));
                if (given.count(name) != 0u)
                {
                    arguments.push_back(given[name]);
                }
                else if (unified && (aligned != bound.end()) && (aligned->second[0] == '\x01') &&
                         ((size_t)(aligned->second[1] - 'A') < from_names.size()) &&
                         (given.count(from_names[(size_t)(aligned->second[1] - 'A')]) != 0u))
                {
                    arguments.push_back(given[from_names[(size_t)(aligned->second[1] - 'A')]]);
                }
                else if (unified && (aligned != bound.end()) && (aligned->second[0] != '\x01'))
                {
                    arguments.push_back(aligned->second);
                }
                else
                {
                    return 0;
                }
            }
            // a stand-in the ruleset cannot write is a malformed question: the put exits, settled
            std::string standing;
            if (ruleset_opcode(sass, to, arguments, none, standing) == 0)
            {
                return 0;
            }
            // a link the stand-in writes as it stood is no ask
            if (standing == found.str(0))
            {
                continue;
            }
            t_dangerous = form_dangerous(standing);
            const std::string before = chain.second.substr(0u, (size_t)found.position(0));
            const std::string changed = before + standing + chain.second.substr((size_t)(found.position(0) + found.length(0)));
            std::vector<unsigned char> code(16u * 4096u);
            const unsigned int count = sass_assemble_lines(&s_machine, changed.c_str(), SASS_CONTROL_SAFE, code.data(), code.size());
            if (count == 0u)
            {
                continue;
            }
            code.resize(16u * count);
            const unsigned int link = (unsigned int)std::count(before.begin(), before.end(), '\n');
            char address[32];
            snprintf(address, sizeof(address), "%s:%04x", chain.first.c_str(), 16u * link);
            const std::vector<std::string> &expected = host[chain.first];
            // Past the first case apart a link adds to the product alone, and only where the form reads in a shape no
            // link read before it: a link of a shape read before holds the same rows, and is no branch of its own. The
            // cycle loops while the product is not whole and a branch is left, and ends at a whole product or with
            // every branch put or steered off. Its verdict stands, and it asks nothing
            const int past_closed = !closed.empty();
            // The steer: the links of one delta are one band, the nearest first. A band is truthy where a put in it
            // adds a row that came back one way, signal, and falsy where its puts add none or only rows that came back
            // both ways, noise. A truthy band steers on to the next; past the first case apart, a falsy band steers off
            // every farther one, whose links stand farther from the cases and read them through more links
            if (found_link.vector.first != band)
            {
                if (past_closed && (band != LONG_MIN) && !band_truthy)
                {
                    if (s_ask_log != NULL)
                    {
                        fprintf(s_ask_log, "steered off past delta %ld\n", band);
                    }
                    break;
                }
                band = found_link.vector.first;
                band_truthy = 0;
            }
            const int shape_new =
                shapes.insert(read_shape(flow.first, carrier_pieces(chain.second, '\n'), flow.second, found.str(0),
                                         link_at))
                    .second;
            if (past_closed && !shape_new)
            {
                continue;
            }
            const int alike =
                past_closed ? 0 : vector_alike(code, expected, places, &question, &asks, magnitudes[chain.first].first);
            const unsigned int put_outcome = past_closed ? (unsigned int)RUN_HELD : question.outcome;
            if ((s_ask_trace != NULL) && !past_closed)
            {
                fprintf(s_ask_trace, "pair %s in place of %s at %s: %s\n", to.c_str(), from.c_str(), address,
                        alike                           ? "alike"
                        : (put_outcome == RUN_ANSWERED) ? "apart"
                                                        : question.refused);
                ask_flushed(s_ask_trace);
            }
            // the put beneath its ask in the log: the link as it stood, the link standing in its place, each on one
            // line of the log, and the operands only the form it stood as names
            if (s_ask_log != NULL)
            {
                std::string from_line = found.str(0);
                std::string to_line = standing;
                std::replace(from_line.begin(), from_line.end(), '\n', ' ');
                std::replace(to_line.begin(), to_line.end(), '\n', ' ');
                fprintf(s_ask_log, "pair %s in place of %s at %s\nfrom %s\nto %s\nonly", to.c_str(), from.c_str(),
                        address, from_line.c_str(), to_line.c_str());
                for (const std::string &name : from_names)
                {
                    if ((given.count(name) != 0u) && (read_by_to.count(name) == 0u))
                    {
                        fprintf(s_ask_log, " %s", given[name].c_str());
                    }
                }
                // each of the form's parameters the stand-in writes a literal in place of: one structure, bound
                if (unified && !fixed.empty())
                {
                    fprintf(s_ask_log, "\nbinds");
                    for (const auto &each : fixed)
                    {
                        const size_t parameter = (size_t)(each.first - 'A');
                        fprintf(s_ask_log, " %s=%s",
                                (parameter < from_names.size()) ? from_names[parameter].c_str() : "?",
                                each.second.c_str());
                    }
                }
                fprintf(s_ask_log, "\nvector registers %u lanes %u operands %u delta %ld seed %lu\n",
                        question.registers, question.cases, magnitudes[chain.first].second, found_link.vector.first,
                        seed);
                // what the form's product is made of: each register it reads that a case reaches, its value on every
                // case as the part holds it at the form, the carrier cut there and the value stored in its place
                const std::string form_found = found.str(0);
                const long form_lines = (long)std::count(form_found.begin(), form_found.end(), '\n') +
                                        ((!form_found.empty() && (form_found.back() == '\n')) ? 0 : 1);
                // the product asked over the cases the question holds: what the form reads, and what it and its
                // stand-in write
                // what the form reads, in the order it names its parameters: a parameter the chain binds to a literal
                // is read as that literal on every case, so that every carrier of the form reads it as one set of
                // values. A parameter inside `[` and `]` says where the form reads and is no value it reads. With none
                // bound, the registers in the order the form names them
                const std::vector<std::string> registers_read =
                    carrier_form_reads(flow.first, flow.second, form_found, link_at);
                std::vector<std::string> form_reads;
                std::map<std::string, unsigned long long> literal_reads;
                int addressed = 0;
                for (const std::string &token : form_tokens(from_text))
                {
                    addressed += (token == "[") ? 1 : (token == "]") ? -1 : 0;
                    const size_t parameter = (token.size() >= 3u) ? (size_t)(token[1] - 'A') : from_names.size();
                    if ((token[0] != '\x01') || (addressed > 0) || (parameter >= from_names.size()) ||
                        (given.count(from_names[parameter]) == 0u))
                    {
                        continue;
                    }
                    const std::string part = token.substr(3u);
                    const std::string value = given[from_names[parameter]];
                    const std::string named = value + part;
                    std::string read;
                    if (std::find(registers_read.begin(), registers_read.end(), named) != registers_read.end())
                    {
                        read = named;
                    }
                    else if ((value == "RZ") || (value == "PT") || (value == "!PT") ||
                             std::regex_match(value, std::regex("-?(0x[0-9a-fA-F]+|[0-9]+)")))
                    {
                        // a flag bound to a literal is read as a flag, `P` before its name
                        const int flag = (value == "PT") || (value == "!PT");
                        read = (flag ? "P=" : "=") + from_names[parameter] + part;
                        literal_reads[read] = (value == "PT") ? 1ull
                                              : ((value == "RZ") || (value == "!PT"))
                                                  ? 0ull
                                                  : (unsigned long long)std::stoll(value, nullptr, 0);
                    }
                    if (!read.empty() && (std::find(form_reads.begin(), form_reads.end(), read) == form_reads.end()))
                    {
                        form_reads.push_back(read);
                    }
                }
                for (const std::string &read : registers_read)
                {
                    if (std::find(form_reads.begin(), form_reads.end(), read) == form_reads.end())
                    {
                        form_reads.push_back(read);
                    }
                }
                const std::vector<std::string> &reads_named = literal_reads.empty() ? registers_read : form_reads;
                const auto product_asked = [&](ConceptReads &put_reads, std::vector<unsigned long long> *put_writes) {
                    // the cuts the product asks, put together: one before the form for each value it reads that a
                    // register holds, and one after it for each side's product. Each is the carrier, the value stored
                    // in place of its answer and nothing between, and a cut the part refuses costs no other cut its
                    // answer
                    std::vector<std::vector<unsigned char>> cut_codes;
                    std::vector<int> cut_dangerous;
                    for (const std::string &read : reads_named)
                    {
                        if ((literal_reads.count(read) != 0u) || (read[0] == '['))
                        {
                            continue;
                        }
                        const std::string cut =
                            carrier_cut_at_read(flow.first, flow.second, chain.second, link_at, form_lines, read);
                        std::vector<unsigned char> cut_code(16u * 4096u);
                        const unsigned int cut_count =
                            cut.empty() ? 0u
                                        : sass_assemble_lines(&s_machine, cut.c_str(), SASS_CONTROL_SAFE,
                                                              cut_code.data(), cut_code.size());
                        cut_code.resize(16u * cut_count);
                        if (s_ask_trace != NULL)
                        {
                            fprintf(s_ask_trace, "reads %s, the carrier cut at %s, %u links:\n%s", read.c_str(),
                                    address, cut_count, cut.c_str());
                            ask_flushed(s_ask_trace);
                        }
                        cut_codes.push_back(cut_code);
                        cut_dangerous.push_back(0);
                    }
                    // the form's product and its stand-in's, each the value it writes on every case as the part holds
                    // it at the form, the carrier cut just after it: the links after the form are a delta of their own
                    const std::string sides[2] = {form_found, standing};
                    const char *const side_names[2] = {"from", "to"};
                    for (unsigned int side = 0u; side < 2u; side += 1u)
                    {
                        int wide = 0;
                        const std::string product = carrier_form_product(sides[side], &wide);
                        const std::string cut =
                            product.empty() ? std::string()
                                            : carrier_cut_at_read(flow.first, flow.second, chain.second, link_at,
                                                                  form_lines, product, sides[side], wide);
                        std::vector<unsigned char> cut_code(16u * 4096u);
                        const unsigned int cut_count =
                            cut.empty() ? 0u
                                        : sass_assemble_lines(&s_machine, cut.c_str(), SASS_CONTROL_SAFE,
                                                              cut_code.data(), cut_code.size());
                        cut_code.resize(16u * cut_count);
                        if (s_ask_trace != NULL)
                        {
                            fprintf(s_ask_trace, "writes %s %s, the carrier cut after %s, %u links:\n%s",
                                    side_names[side], product.c_str(), address, cut_count, cut.c_str());
                            ask_flushed(s_ask_trace);
                        }
                        cut_codes.push_back(cut_code);
                        cut_dangerous.push_back((side == 1u) ? t_dangerous : 0);
                    }
                    std::vector<RunQuestion> cut_answers;
                    cuts_asked(cut_codes, cut_dangerous, &question, &asks, magnitudes[chain.first].first, &cut_answers);
                    size_t cut_at = 0u;
                    for (const std::string &read : reads_named)
                    {
                        // a parameter the chain binds to a literal is that literal on every case, and asks nothing
                        if (literal_reads.count(read) != 0u)
                        {
                            fprintf(s_ask_log, "reads %s", read.c_str());
                            put_reads.push_back(std::make_pair(read, std::vector<unsigned long long>()));
                            for (unsigned int place = 0u; place < question.cases; place += 1u)
                            {
                                fprintf(s_ask_log, " %llx", literal_reads[read]);
                                put_reads.back().second.push_back(literal_reads[read]);
                            }
                            fprintf(s_ask_log, "\n");
                            continue;
                        }
                        // an operand of a case the form loads itself is read as the question holds it, and asks nothing
                        if (read[0] == '[')
                        {
                            const unsigned int operand = (unsigned int)std::stoul(read.substr(6u));
                            fprintf(s_ask_log, "reads [%u]", operand);
                            put_reads.push_back(
                                std::make_pair("[" + std::to_string(operand) + "]", std::vector<unsigned long long>()));
                            for (unsigned int place = 0u; place < question.cases; place += 1u)
                            {
                                const unsigned int *const words = question.word[place];
                                const unsigned long long value =
                                    ((2u * operand + 1u) < RUN_IN_WORDS)
                                        ? (((unsigned long long)words[2u * operand + 1u] << 32u) | words[2u * operand])
                                        : 0ull;
                                fprintf(s_ask_log, " %llx", value);
                                put_reads.back().second.push_back(value);
                            }
                            fprintf(s_ask_log, "\n");
                            continue;
                        }
                        const RunQuestion &cut = cut_answers[cut_at];
                        const int unwritten = cut_codes[cut_at].empty();
                        cut_at += 1u;
                        fprintf(s_ask_log, "reads %s", read.c_str());
                        put_reads.push_back(std::make_pair(read, std::vector<unsigned long long>()));
                        if (unwritten || (cut.outcome != RUN_ANSWERED))
                        {
                            fprintf(s_ask_log, " refused %s\n", unwritten ? "unwritten" : cut.refused);
                            continue;
                        }
                        for (unsigned int place = 0u; place < question.cases; place += 1u)
                        {
                            fprintf(s_ask_log, " %llx", cut.answered[place]);
                            put_reads.back().second.push_back(cut.answered[place]);
                        }
                        fprintf(s_ask_log, "\n");
                    }
                    for (unsigned int side = 0u; side < 2u; side += 1u)
                    {
                        const RunQuestion &cut = cut_answers[cut_at];
                        const int unwritten = cut_codes[cut_at].empty();
                        cut_at += 1u;
                        fprintf(s_ask_log, "writes %s", side_names[side]);
                        if (unwritten || (cut.outcome != RUN_ANSWERED))
                        {
                            fprintf(s_ask_log, " refused %s\n", unwritten ? "unwritten" : cut.refused);
                            continue;
                        }
                        for (unsigned int place = 0u; place < question.cases; place += 1u)
                        {
                            fprintf(s_ask_log, " %llx", cut.answered[place]);
                            put_writes[side].push_back(cut.answered[place]);
                        }
                        fprintf(s_ask_log, "\n");
                    }
                    ask_flushed(s_ask_log);
                };
                // the values the pair's forms write of their own, each put to the product with every case value
                fprintf(s_ask_log, "qualifies");
                for (const unsigned long long value : qualifiers)
                {
                    fprintf(s_ask_log, " %llx", value);
                }
                fprintf(s_ask_log, "\n");
                const size_t signal_before = concept_signal(product);
                ConceptReads put_reads;
                std::vector<unsigned long long> put_writes[2];
                product_asked(put_reads, put_writes);
                concept_product_held(put_reads, put_writes[0], put_writes[1], pair_cased, &product);
                // a product the question's cases leave short of whole is put over every combination of the values the
                // cases are made of, at each put of a shape no link read before whose every read the part answered, run
                // by run until the product is whole or every run is put
                int read_whole = !put_reads.empty();
                for (const auto &each : put_reads)
                {
                    read_whole &= !each.second.empty() ? 1 : 0;
                }
                if (shape_new && read_whole && !concept_whole(product, pair_cased))
                {
                    const unsigned int runs = whole_cases(&question, qualifiers, 0u);
                    for (unsigned int run = 0u; (run < runs) && !concept_whole(product, pair_cased); run += 1u)
                    {
                        whole_cases(&question, qualifiers, run);
                        fprintf(s_ask_log, "pair %s in place of %s at %s\nqualified", to.c_str(), from.c_str(),
                                address);
                        for (const unsigned long long value : qualifiers)
                        {
                            fprintf(s_ask_log, " %llx", value);
                        }
                        fprintf(s_ask_log, "\n");
                        ConceptReads whole_reads;
                        std::vector<unsigned long long> whole_writes[2];
                        product_asked(whole_reads, whole_writes);
                        concept_product_held(whole_reads, whole_writes[0], whole_writes[1], pair_cased, &product);
                    }
                    stall_cases(&question, &places);
                }
                band_truthy |= (concept_signal(product) > signal_before) ? 1 : 0;
            }
            if (past_closed && concept_whole(product, pair_cased))
            {
                *verdict = closed;
                return 1;
            }
            if (alike)
            {
                alike_cases += (unsigned int)std::count_if(places.begin(), places.end(),
                                                           [&](unsigned int place) { return expected[place] != "-"; });
                answered = 1;
                if (const_loaded)
                {
                    break;
                }
                continue;
            }
            if (put_outcome == RUN_ANSWERED)
            {
                // the verdict is the first case apart; with the log kept, the pair is put on until its product is whole
                closed = closed.empty() ? ("closed " + std::string(address) + " " + case_text(s_apart_at) + "->" +
                                           expected[s_apart_at])
                                        : closed;
                if ((s_ask_log == NULL) || concept_whole(product, pair_cased))
                {
                    *verdict = closed;
                    return 1;
                }
            }
        }
        if (!closed.empty())
        {
            *verdict = closed;
            return 1;
        }
        *verdict = "open " + std::to_string(alike_cases);
        return answered;
    };
    std::vector<std::string> written;
    unsigned int open = 0u;
    unsigned int closed = 0u;
    unsigned int unasked = 0u;
    int beneath = 0;
    ask_files_opened();
    // what the texts of a pair's two forms say of it before any ask: one structure, one form the other with a
    // parameter bound; both reading what the launch gives; or two operations, one meaning written twice
    const auto pair_facts = [&](const std::string &first, const std::string &second) {
        std::vector<std::string> first_names;
        std::vector<std::string> second_names;
        const std::string first_text = marked(first, &first_names);
        const std::string second_text = marked(second, &second_names);
        std::map<char, std::string> bound;
        std::map<char, std::string> fixed;
        const int structural = (form_unified(first_text, second_text, &bound, &fixed) && !fixed.empty()) ||
                               (form_unified(second_text, first_text, &bound, &fixed) && !fixed.empty());
        const int launched = !first_text.empty() && !second_text.empty() && form_launched(first_text) &&
                             form_launched(second_text);
        const std::vector<std::string> first_tokens = form_tokens(first_text);
        const std::vector<std::string> second_tokens = form_tokens(second_text);
        const int two_operations = !first_tokens.empty() && !second_tokens.empty() &&
                            (first_tokens[0].substr(0u, first_tokens[0].find('.')) !=
                             second_tokens[0].substr(0u, second_tokens[0].find('.')));
        // the categories the texts read the pair into: both forms comparisons, every condition tested an equality or
        // some an order; either form reading where its lane sits, working a wide value in pieces, deciding which
        // form runs next, or holding no instruction
        const std::set<std::string> first_conditions = form_conditions(first_text);
        const std::set<std::string> second_conditions = form_conditions(second_text);
        std::set<std::string> conditions = first_conditions;
        conditions.insert(second_conditions.begin(), second_conditions.end());
        const int compared = !first_conditions.empty() && !second_conditions.empty();
        int ordered = 0;
        int equal = compared;
        for (const std::string &condition : conditions)
        {
            ordered |= ((condition == "LT") || (condition == "LE") || (condition == "GT") || (condition == "GE")) ? 1
                                                                                                                  : 0;
            equal &= ((condition == "EQ") || (condition == "NE")) ? 1 : 0;
        }
        const int placed = form_launched(first_text) || form_launched(second_text);
        const int ranged = form_ranged(first_text) || form_ranged(second_text);
        const int controlled = form_controls(first_text) || form_controls(second_text);
        const int switched = form_switches(first_text) || form_switches(second_text);
        // either form an operation, at any width; either form's every instruction one value with its first two
        // sources either way round; either form a doing word that computes no value
        const int operated = form_operates(first_text) || form_operates(second_text);
        const int commuted = form_commutes(first_text) || form_commutes(second_text);
        const int verbed = form_verbs(first_text) || form_verbs(second_text);
        fprintf(s_ask_log, "facts %s %s%s%s%s%s%s%s%s%s%s%s%s%s%s\n", first.c_str(), second.c_str(),
                structural ? " structural" : "", launched ? " pragmatic" : "", two_operations ? " syntactic" : "",
                compared ? " comparison" : "", equal ? " equality" : "", (compared && ordered) ? " order" : "",
                operated ? " operation" : "", commuted ? " commutative" : "", verbed ? " verb" : "",
                ranged ? " range" : "", placed ? " vector" : "", controlled ? " control" : "",
                switched ? " switch" : "");
    };
    // every pair of the bridge, in its order, the question and places its put asks with, the hash of what its put
    // reads, and the verdict it came to
    struct PairPut
    {
        std::string first;
        std::string second;
        std::string verdict;
        int put_whole;
        std::unique_ptr<RunQuestion> question;
        std::vector<unsigned int> places;
        std::string reads;
    };
    // what every put reads of the engine: each chain's text and the host's answers to its cases, by its question
    unsigned long long engine_read = 0xcbf29ce484222325ull;
    for (const auto &chain : texts)
    {
        engine_read = record_hash(chain.first.c_str(), chain.first.size() + 1u, engine_read);
        engine_read = record_hash(chain.second.c_str(), chain.second.size() + 1u, engine_read);
        for (const std::string &answer : host[chain.first])
        {
            engine_read = record_hash(answer.c_str(), answer.size() + 1u, engine_read);
        }
    }
    // The scheduler: the record's gray entries are the queue. A pair closed on the record's path, whose reads are as
    // they were, is settled: its verdict is the record's, and it is put no more, since a put runs again only where what
    // it reads has changed. Every other pair, open, with no path, or reading what changed, is gray, and the rounds put
    // the gray pairs alone
    std::map<std::string, const QueryRecordPath *> recorded_paths;
    for (const QueryRecordPath &path : s_query_record.paths)
    {
        recorded_paths[path.first + " " + path.second] = &path;
    }
    std::vector<PairPut> pairs;
    size_t settled = 0u;
    for (const std::string &entry : bridge)
    {
        if (entry.rfind("pair ", 0u) == 0u)
        {
            std::stringstream words(entry);
            std::string kind;
            PairPut pair{std::string(),
                         std::string(),
                         "open 0",
                         0,
                         std::unique_ptr<RunQuestion>(new RunQuestion()),
                         std::vector<unsigned int>(),
                         std::string()};
            words >> kind >> pair.first >> pair.second;
            std::vector<std::string> names;
            const std::string first_text = marked(pair.first, &names);
            const std::string second_text = marked(pair.second, &names);
            unsigned long long reads = record_hash(first_text.c_str(), first_text.size() + 1u, engine_read);
            reads = record_hash(second_text.c_str(), second_text.size() + 1u, reads);
            char hashed[17];
            snprintf(hashed, sizeof(hashed), "%016llx", reads);
            pair.reads = hashed;
            const auto path = recorded_paths.find(pair.first + " " + pair.second);
            if ((path != recorded_paths.end()) && (path->second->reads == pair.reads) &&
                (path->second->verdict.rfind("closed ", 0u) == 0u))
            {
                pair.verdict = path->second->verdict;
                pair.put_whole = 1;
                settled += 1u;
            }
            pairs.push_back(std::move(pair));
        }
    }
    // Each pair put in a thread of its own, run only in its turn: its facts, its put, and the stand-in the other way
    // round where the form's own way answered nothing. Its trace and its log are files of its own beside the trace,
    // added whole to the trace and the log once it is put
    s_round_asks.assign(pairs.size(), std::vector<CarriedAsk>());
    const std::string traced = (getenv("KLQ_TRACE") != NULL) ? std::string(getenv("KLQ_TRACE")) : std::string();
    const auto file_added = [](FILE *whole, const std::string &path) {
        FILE *const part = fopen(path.c_str(), "rb");
        if ((whole != NULL) && (part != NULL))
        {
            char moved[65536];
            size_t read = fread(moved, 1u, sizeof(moved), part);
            while (read != 0u)
            {
                fwrite(moved, 1u, read, whole);
                read = fread(moved, 1u, sizeof(moved), part);
            }
            fflush(whole);
        }
        if (part != NULL)
        {
            fclose(part);
        }
        remove(path.c_str());
    };
    std::vector<std::thread> threads;
    for (size_t at = 0u; at < pairs.size(); at += 1u)
    {
        if (pairs[at].put_whole)
        {
            continue;
        }
        threads.emplace_back([&, at]() {
            {
                std::unique_lock<std::mutex> lock(s_turn_lock);
                s_turn_changed.wait(lock, [at]() { return s_turn == (int)at; });
            }
            t_pair = (int)at;
            const std::string trace_path = traced + ".pair" + std::to_string(at);
            const std::string log_path = traced + ".log.pair" + std::to_string(at);
            if (s_log_whole != NULL)
            {
                s_ask_trace = fopen(trace_path.c_str(), "wb");
                s_ask_log = fopen(log_path.c_str(), "wb");
            }
            PairPut &pair = pairs[at];
            t_question = pair.question.get();
            t_places = &pair.places;
            stall_cases(t_question, t_places);
            if (s_ask_log != NULL)
            {
                pair_facts(pair.first, pair.second);
            }
            if (!put(pair.first, pair.second, &pair.verdict))
            {
                put(pair.second, pair.first, &pair.verdict);
            }
            if (s_log_whole != NULL)
            {
                if (s_ask_trace != NULL)
                {
                    fclose(s_ask_trace);
                }
                if (s_ask_log != NULL)
                {
                    fclose(s_ask_log);
                }
                s_ask_trace = NULL;
                s_ask_log = NULL;
                file_added(s_trace_whole, trace_path);
                file_added(s_log_whole, log_path);
            }
            std::unique_lock<std::mutex> lock(s_turn_lock);
            pair.put_whole = 1;
            s_turn = -1;
            s_turn_changed.notify_all();
        });
    }
    // The rounds: each pair not yet put runs in its turn until it asks or is put, and the asks the round was handed
    // are answered together. The rounds loop while a pair is left to put, and end once every pair is put
    unsigned int round = 0u;
    size_t left = pairs.size() - settled;
    printf("klq_identity pair: %zu pairs settled on the record, %zu gray in the queue\n", settled, left);
    while (left != 0u)
    {
        const std::chrono::steady_clock::time_point round_began = std::chrono::steady_clock::now();
        const unsigned long long handed_before = s_round_handed;
        const unsigned long long held_before = s_round_held;
        const unsigned long long processes_before = s_round_processes;
        round += 1u;
        mark_pending("round", std::to_string(round));
        std::vector<CarriedAsk> carried;
        unsigned int asking = 0u;
        for (size_t at = 0u; at < pairs.size(); at += 1u)
        {
            std::unique_lock<std::mutex> lock(s_turn_lock);
            if (pairs[at].put_whole)
            {
                continue;
            }
            s_turn = (int)at;
            s_turn_changed.notify_all();
            s_turn_changed.wait(lock, []() { return s_turn == -1; });
            asking += s_round_asks[at].empty() ? 0u : 1u;
            carried.insert(carried.end(), s_round_asks[at].begin(), s_round_asks[at].end());
            s_round_asks[at].clear();
        }
        asks_answered(carried);
        left = (size_t)std::count_if(pairs.begin(), pairs.end(), [](const PairPut &pair) { return !pair.put_whole; });
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - round_began).count();
        printf("klq_identity round %u: %u pairs asked, %llu asks handed, %llu answered from what was held, %llu "
               "processes, %zu of %zu pairs put, %.1f seconds\n",
               round, asking, s_round_handed - handed_before, s_round_held - held_before,
               s_round_processes - processes_before, pairs.size() - left, pairs.size(), seconds);
        fflush(stdout);
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    // the bridge written with each pair's verdict beneath it, in place of the verdict it held
    size_t paired = 0u;
    for (const std::string &entry : bridge)
    {
        const int verdict_line = (entry.rfind("open ", 0u) == 0u) || (entry.rfind("closed ", 0u) == 0u);
        beneath = (entry.rfind("pair ", 0u) == 0u) || (beneath && verdict_line);
        if (verdict_line && beneath)
        {
            continue;
        }
        written.push_back(entry);
        if (entry.rfind("pair ", 0u) != 0u)
        {
            continue;
        }
        const PairPut &pair = pairs[paired];
        paired += 1u;
        open += ((pair.verdict.rfind("open ", 0u) == 0u) && (pair.verdict != "open 0")) ? 1u : 0u;
        closed += (pair.verdict.rfind("closed ", 0u) == 0u) ? 1u : 0u;
        unasked += (pair.verdict == "open 0") ? 1u : 0u;
        printf("  %s %s: %s\n", pair.first.c_str(), pair.second.c_str(), pair.verdict.c_str());
        written.push_back(pair.verdict);
    }
    run_channel_close();
    FILE *const file = fopen(klq, "wb");
    if (file == NULL)
    {
        printf("klq_identity pair: %s could not be written\n", klq);
        return 1;
    }
    for (const std::string &kept : written)
    {
        fprintf(file, "%s\n", kept.c_str());
    }
    fclose(file);
    printf("klq_identity pair: %llu asks, %u open, %u closed, %u not asked, written to %s\n", asks, open, closed,
           unasked, klq);
    // the path this cycle read off the asks, each pair and its verdict, an open pair the question a further pass takes
    // up, written to the query record with every ask it held and every ask this cycle put once R is written back
    s_query_record.paths.clear();
    for (const PairPut &pair : pairs)
    {
        s_query_record.paths.push_back(QueryRecordPath{pair.first, pair.second, pair.reads, pair.verdict});
    }
    printf("klq_identity pair: %zu asks held in the query record, %zu of them put this cycle\n",
           s_query_record.asks.size(), s_query_record.asks.size() - recorded_before);
    return 0;
}

int main(int count, char **words)
{
    if ((count >= 10) && (std::string(words[1]) == "pair") && (std::string(words[8]) == "--"))
    {
        std::vector<const char *> carrier(words + 9, words + count);
        carrier.push_back(NULL);
        return identity_pair(words[2], words[3], words[4], words[5], words[6], words[7], carrier.data());
    }
    if ((count >= 10) && (std::string(words[1]) == "text_identity") && (std::string(words[8]) == "--"))
    {
        std::vector<const char *> carrier(words + 9, words + count);
        carrier.push_back(NULL);
        return identity_text(words[2], words[3], words[4], words[5], words[6], words[7], carrier.data());
    }
    if ((count >= 9) && (std::string(words[1]) == "queue") && (std::string(words[7]) == "--"))
    {
        std::vector<const char *> carrier(words + 8, words + count);
        carrier.push_back(NULL);
        return identity_queue(words[2], words[3], words[4], words[5], words[6], carrier.data());
    }
    // the carrier's words follow a lone --
    if ((count >= 8) && (std::string(words[1]) == "stall") && (std::string(words[6]) == "--"))
    {
        std::vector<const char *> carrier(words + 7, words + count);
        carrier.push_back(NULL);
        return identity_stall(words[2], words[3], words[4], words[5], carrier.data());
    }
    if ((count >= 8) && (std::string(words[1]) == "register") && (std::string(words[6]) == "--"))
    {
        std::vector<const char *> carrier(words + 7, words + count);
        carrier.push_back(NULL);
        return identity_register(words[2], words[3], words[4], words[5], carrier.data());
    }
    // the tasks are given up to a lone --, the carrier's words after it
    if ((count >= 9) && (std::string(words[1]) == "curve"))
    {
        std::vector<std::string> tasks;
        int at = 6;
        for (; (at < count) && (std::string(words[at]) != "--"); at += 1)
        {
            tasks.push_back(words[at]);
        }
        if (!tasks.empty() && ((at + 1) < count))
        {
            std::vector<const char *> carrier(words + at + 1, words + count);
            carrier.push_back(NULL);
            return identity_curve(words[2], words[3], words[4], words[5], tasks, carrier.data());
        }
    }
    // the paths of ours are given up to a lone --, the rulesets after it
    if ((count >= 6) && (std::string(words[1]) == "broken"))
    {
        std::vector<std::string> ours_paths;
        int at = 5;
        for (; (at < count) && (std::string(words[at]) != "--"); at += 1)
        {
            ours_paths.push_back(words[at]);
        }
        at += (at < count) ? 1 : 0;
        return identity_broken(words[2], words[3], words[4], candidate_paths(ours_paths), count - at, words + at);
    }
    if ((count == 5) && (std::string(words[1]) == "permute"))
    {
        return identity_permute(words[2], words[3], words[4]);
    }
    if ((count >= 5) && (std::string(words[1]) == "known"))
    {
        return identity_known(words[2], words[3], words[4],
                              candidate_paths(std::vector<std::string>(words + 5, words + count)));
    }
    if (((count == 6) || (count == 7)) && (std::string(words[1]) == "slice"))
    {
        return identity_slice(words[2], words[3], words[4], words[5], (count == 7) ? words[6] : "");
    }
    if ((count >= 4) && (std::string(words[1]) == "read"))
    {
        return identity_read(words[2], words[3], count - 4, words + 4);
    }
    printf("klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder> [<ours folder>]\n");
    printf("klq_identity read <folder> <Lstar.klq> <ruleset>...\n");
    printf("klq_identity known <nvcc listing> <manifest> <folder> <candidate>...\n");
    printf("klq_identity permute <nvcc listing> <manifest> <folder>\n");
    printf("klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...\n");
    printf("klq_identity stall <ours folder> <host answers> <ksc> <folder> -- <carrier>...\n");
    printf("klq_identity register <ours folder> <host answers> <ksc> <folder> -- <carrier>...\n");
    printf("klq_identity queue <ours folder> <host answers> <ksc> <folder> <manifest> -- <carrier>...\n");
    printf("klq_identity pair <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <machine> -- <carrier>...\n");
    printf("klq_identity text_identity <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <manifest> -- "
           "<carrier>...\n");
    printf("klq_identity curve <ours folder> <host answers> <ksc> <folder> <task>... -- <carrier>...\n");
    return 1;
}
