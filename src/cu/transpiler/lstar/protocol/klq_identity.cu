// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_identity.cu: the identities a language's questions carry, read off the measuring stick and written to Lstar.klq
//
//   klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder>
//   klq_identity read <folder> <Lstar.klq> <ruleset>...
//   klq_identity known <nvcc listing> <manifest> <folder> <candidate>...
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
#include "target_internal.h"

#include <stdio.h>

#include <algorithm>
#include <fstream>
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
    // written, its spaces one and its numbers decimal. The target of a control transfer is a position in the chain and
    // is written as its distance in links from the link itself
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
    const int transfers = std::any_of(std::begin(s_control_or_wait), std::end(s_control_or_wait),
                                      [&](const char *control) { return operation_name == control; });
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
            const std::vector<std::string> first = links_past(questions[one], questions[other]);
            const std::vector<std::string> second = links_past(questions[other], questions[one]);
            if ((first.size() != 1u) || (second.size() != 1u))
            {
                continue;
            }
            const Link &left = questions[one].links.at(first[0]);
            const Link &right = questions[other].links.at(second[0]);
            if ((left.registers != right.registers) || (left.kind == right.kind))
            {
                continue;
            }
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
        static const std::regex s_parameter("\\{[a-z_]+\\}(\\\\\\.hi)?");
        pattern = std::regex_replace(escaped, s_parameter, "[^,]+");
        links.push_back(std::regex("^" + pattern + "$"));
    }
    return links;
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
    // the forms of each ruleset given, each name with the links its text writes
    std::vector<std::pair<std::string, std::vector<std::regex>>> forms;
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
            return 1;
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
            forms.push_back(std::make_pair(name, form_links(text)));
        }
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

int main(int count, char **words)
{
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
    return 1;
}
