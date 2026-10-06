// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_identity.cu: the identities a language's questions carry, read off the measuring stick and written to Lstar.klq
//
//   klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder>
//   klq_identity read <folder> <Lstar.klq> <ruleset>...
//   klq_identity known <nvcc listing> <manifest> <folder> <candidate>...
//   klq_identity permute <nvcc listing> <manifest> <folder>
//   klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...
//   klq_identity stall <ours folder> <host answers> <ksc> <folder> -- <carrier>...
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
// <folder>/host_questions.cpp, every question a slice holds whose operands and result are integers, compiled for the
// host as C++ with the frame every kernel shares. The host
// computes each over every case and writes <folder>/host_answers.txt; read then gives each identity its verdict: the
// first case the two questions of one of its slices answer apart closes it as two operations, and an identity alike on
// every case asked stays open with its count of cases. A case the host's C would trap on or leave undefined is asked of
// nothing. Each identity is written to Lstar.klq after its keys, with the forms of the given rulesets whose texts write
// its links, and with the rule a link breaks where asking the part as written would end it
#include "code_generator.h"
#include "run_channel.h"
#include "target_internal.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
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
                                                       "statement", "test"};
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

// the type a kernel's first operand is cast to, as its text declares it
static std::string first_type(const std::string &kernel)
{
    static const std::regex s_first("const ([a-z ]+) a = \\(");
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

static int identity_slice(const char *listing, const char *manifest, const char *stick_path, const std::string &folder)
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
    while (std::getline(held, line) && (line.rfind("identity ", 0u) != 0u))
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
        fprintf(file, "identity %s\nfirst %s\nsecond %s\n", carrier_text(identity.second[0]).c_str(), sides[0].c_str(),
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
static int identity_known(const char *listing, const char *manifest, const std::string &folder, int count,
                          char **paths)
{
    std::vector<Question> questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    const std::unordered_map<unsigned long long, Occurrence> held = windows_held(questions);
    std::vector<Question> candidates;
    for (int at = 0; at < count; at += 1)
    {
        candidates_read(paths[at], &candidates);
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

// The cases a question is put over, spread evenly through the host's: the words of each into `question`, and the
// host's place of each into `places`
static void stall_cases(RunQuestion *question, std::vector<unsigned int> *places)
{
    places->clear();
    question->cases = RUN_CASES_MOST;
    for (unsigned int place = 0u; place < RUN_CASES_MOST; place += 1u)
    {
        const unsigned int at = (unsigned int)(((unsigned long long)place * IDENTITY_CASES) / RUN_CASES_MOST);
        const unsigned long long left = s_values[(at / IDENTITY_VALUES) % IDENTITY_VALUES];
        const unsigned long long right = s_values[at % IDENTITY_VALUES];
        const unsigned int words[RUN_IN_WORDS] = {(unsigned int)(left & 0xffffffffull), (unsigned int)(left >> 32u),
                                                  (unsigned int)(right & 0xffffffffull), (unsigned int)(right >> 32u),
                                                  (unsigned int)(at / (IDENTITY_VALUES * IDENTITY_VALUES)), 0u, 0u, 0u};
        memcpy(question->word[place], words, sizeof(words));
        places->push_back(at);
    }
}

// 1 where the part answers `code`, put as `question` says of its registers, shape and launches, alike with the host
// on every case it computes, 0 where it answers apart, refuses it or the gate holds it, each counted in `asks`
static int question_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host,
                          const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks)
{
    question->code = code.data();
    question->code_size = code.size();
    *asks += 1ull;
    if (!run_channel_ask(question))
    {
        return 0;
    }
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const std::string &expected = host[places[place]];
        if ((expected != "-") && (std::stoull(expected, nullptr, 16) != question->answered[place]))
        {
            return 0;
        }
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

// The .ksc rewritten with `rows` in place of every answer of the run channel it held to the question `asked`, the rows
// put after its last row of the run channel and its count of the run channel's answers taken again. 1, or 0 where the
// .ksc could not be written
static int ksc_answers_write(const char *ksc, const std::string &asked, const std::vector<std::string> &rows)
{
    std::ifstream in(ksc, std::ios::binary);
    std::vector<std::string> kept;
    std::string line;
    while (std::getline(in, line))
    {
        const std::string plain = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        if (plain.compare(0u, 12u, "run answers ") == 0)
        {
            std::stringstream words(plain);
            std::string channel;
            std::string answered;
            std::string word;
            std::string question;
            words >> channel >> answered >> word >> question;
            if (question == asked)
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
    unsigned int answered = 0u;
    for (const std::string &kept_line : kept)
    {
        answered += (kept_line.compare(0u, 12u, "run answers ") == 0) ? 1u : 0u;
    }
    FILE *const out = fopen(ksc, "wb");
    if (out == NULL)
    {
        return 0;
    }
    for (const std::string &kept_line : kept)
    {
        if (kept_line.compare(0u, 22u, "count run      answers") == 0)
        {
            fprintf(out, "count run      answers  %u\n", answered);
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
    unsigned long long last = highest + 1ull;
    for (unsigned long long number = highest + 1ull; number-- > 0ull;)
    {
        const int held_by_probe = std::any_of(probes.begin(), probes.end(), [&](const RegisterProbe &probe) {
            return probe.named.count(number) != 0u;
        });
        if (held_by_probe)
        {
            continue;
        }
        int truthy = 1;
        for (const RegisterProbe &probe : probes)
        {
            truthy = truthy && stall_alike(register_renamed(probe.code, registers, probe.renamed, number),
                                           host[probe.number], places, &s_question, &asks);
        }
        fprintf(table, "R%llu %s%s%s\n", number, truthy ? "truthy" : "falsy", truthy ? "" : ": ",
                truthy ? "" : s_question.refused);
        printf("  R%llu: %s\n", number, truthy ? "truthy" : "falsy");
        if (truthy)
        {
            last = number;
            break;
        }
    }
    fclose(table);
    run_channel_close();
    if (last > highest)
    {
        printf("klq_identity register: no number answers alike, and no last register\n");
        return 1;
    }
    char row[64];
    snprintf(row, sizeof(row), "run answers %08llx register last", last);
    if (!ksc_answers_write(ksc, "register", {row}))
    {
        printf("klq_identity register: %s could not be written\n", ksc);
        return 1;
    }
    printf("klq_identity register: the last register R%llu, over %llu asks, written to %s\n", last, asks, ksc);
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
// walked up from one past the highest number its register fields hold until it answers alike and halved down from
// there, and is the task's at every count of threads. The fewest's band is the least and the most of its times, asked
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
        unsigned int fewest = named + 1u;
        for (unsigned int threads = 1u; threads <= CURVE_THREADS; threads *= 2u)
        {
            // the fewest registers the task answers alike with, walked up in blocks of one thread until the part says
            // yes; the registers code names are its own whatever its blocks hold, and a count of threads the part
            // refuses at the fewest it refuses at every count, since more registers take more of it
            unsigned long long first = 0ull;
            int answered = curve_point(code, host[task], places, &s_question, &asks, task, threads, fewest, &first,
                                       table);
            while ((threads == 1u) && !answered && (fewest < highest))
            {
                fewest += 1u;
                answered = curve_point(code, host[task], places, &s_question, &asks, task, threads, fewest, &first,
                                       table);
            }
            // a register field holds an immediate's bits as well as a register's number: the count it answers at is
            // a bound, and the fewest is found under it by halving. Code that answers alike declaring some count
            // answers alike declaring more, in a block of one thread that every count fits
            unsigned int short_of = 0u;
            while ((threads == 1u) && answered && ((fewest - short_of) > 1u))
            {
                const unsigned int middle = short_of + ((fewest - short_of) / 2u);
                unsigned long long time = 0ull;
                if (curve_point(code, host[task], places, &s_question, &asks, task, threads, middle, &time, table))
                {
                    fewest = middle;
                    first = time;
                }
                else
                {
                    short_of = middle;
                }
            }
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
            unsigned int truthy = fewest;
            unsigned int falsy = highest + 1u;
            while ((falsy - truthy) > 1u)
            {
                const unsigned int middle = truthy + ((falsy - truthy) / 2u);
                // a point inside the band is truthy, and one that answers apart or is refused is falsy. A point past
                // the band is bounced off the fewest: where the fewest strays past the band too, the band widens and
                // the point is asked again against it; where the fewest holds, a point past the band twice running is
                // falsy
                int verdict = -1;
                unsigned int past = 0u;
                for (unsigned int asked = 0u; (verdict < 0) && (asked < CURVE_ASKS_MOST); asked += 1u)
                {
                    unsigned long long time = 0ull;
                    if (!curve_point(code, host[task], places, &s_question, &asks, task, threads, middle, &time,
                                     table))
                    {
                        verdict = 0;
                        continue;
                    }
                    if (time <= band.high)
                    {
                        verdict = 1;
                        continue;
                    }
                    past += 1u;
                    if (past == 2u)
                    {
                        verdict = 0;
                        continue;
                    }
                    unsigned long long bounced = 0ull;
                    if (curve_point(code, host[task], places, &s_question, &asks, task, threads, fewest, &bounced,
                                    table) &&
                        curve_band_widened(&band, bounced))
                    {
                        past = 0u;
                    }
                }
                if (verdict == 1)
                {
                    truthy = middle;
                }
                else
                {
                    falsy = middle;
                }
            }
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
    if (!ksc_answers_write(ksc, "curve", rows))
    {
        printf("klq_identity curve: %s could not be written\n", ksc);
        return 1;
    }
    printf("klq_identity curve: %zu tasks over %llu asks, %zu answers written to %s\n", tasks.size(), asks,
           rows.size(), ksc);
    return 0;
}

int main(int count, char **words)
{
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
        return identity_broken(words[2], words[3], words[4], ours_paths, count - at, words + at);
    }
    if ((count == 5) && (std::string(words[1]) == "permute"))
    {
        return identity_permute(words[2], words[3], words[4]);
    }
    if ((count >= 5) && (std::string(words[1]) == "known"))
    {
        return identity_known(words[2], words[3], words[4], count - 5, words + 5);
    }
    if ((count == 6) && (std::string(words[1]) == "slice"))
    {
        return identity_slice(words[2], words[3], words[4], words[5]);
    }
    if ((count >= 4) && (std::string(words[1]) == "read"))
    {
        return identity_read(words[2], words[3], count - 4, words + 4);
    }
    printf("klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder>\n");
    printf("klq_identity read <folder> <Lstar.klq> <ruleset>...\n");
    printf("klq_identity known <nvcc listing> <manifest> <folder> <candidate>...\n");
    printf("klq_identity permute <nvcc listing> <manifest> <folder>\n");
    printf("klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...\n");
    printf("klq_identity stall <ours folder> <host answers> <ksc> <folder> -- <carrier>...\n");
    printf("klq_identity register <ours folder> <host answers> <ksc> <folder> -- <carrier>...\n");
    printf("klq_identity curve <ours folder> <host answers> <ksc> <folder> <task>... -- <carrier>...\n");
    return 1;
}
