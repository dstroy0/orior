// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// questions.cu: the values a case is made of, a run's blobs read as text, and the links and questions the stick's manifest lists
#include "identity_internal.h"

// the operations that transfer control or wait, which cubin_safe.h's rule 2 keeps off the part: a question holding
// one is not put to the part as written
static const char *const s_control_or_wait[] = {"BRA",   "BRX",  "JMP",      "JMX",   "CALL", "RET",
                                                "EXIT",  "BSSY", "BSYNC",    "BREAK", "BMOV", "WARPSYNC",
                                                "YIELD", "BAR",  "DEPBAR",   "NANOSLEEP", "BPT", "RTT",
                                                "KILL",  "RPCMOV", "RETIRE", "PMTRIG"};

// the latest blob of `name` in the run's buffer (qry_buffer.h) as text, empty where no run's writer runs or the run
// holds none
std::string run_blob_text(const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_latest(qry_run(), name, &bytes, &size, NULL))
    {
        return std::string();
    }
    // a blob is at most the buffer's bytes, which a size_t counts
    return std::string(reinterpret_cast<const char *>(bytes), static_cast<size_t>(size));
}

// the file at `path` as the run holds it (qry_file_read): its latest blob, or the file where the run holds none; empty
// where neither is there
std::string run_file_text(const std::string &path)
{
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_file_read(qry_run(), path.c_str(), &bytes, &size))
    {
        return std::string();
    }
    // a file read whole is at most what a size_t counts
    const std::string text(reinterpret_cast<const char *>(bytes), static_cast<size_t>(size));
    free(bytes);
    return text;
}

// `format` and what follows it added to the end of what `written` holds, as fprintf writes them
void run_print(RunWritten *written, const char *format, ...)
{
    va_list values;
    va_start(values, format);
    va_list measured;
    va_copy(measured, values);
    const int length = vsnprintf(NULL, 0u, format, measured);
    va_end(measured);
    if (length > 0)
    {
        const size_t at = written->text.size();
        written->text.resize(at + (size_t)length + 1u);
        vsnprintf(&written->text[at], (size_t)length + 1u, format, values);
        written->text.resize(at + (size_t)length);
    }
    va_end(values);
}

// `written` handed to the run's writer whole: as the current file at its path, or as a blob named by the path's own
// name. 1, or 0 where no run's writer runs or it was not handed
int run_closed(const RunWritten *written)
{
    QryBuffer *const run = qry_run();
    if (run == NULL)
    {
        printf("klq_identity: no query writer runs to write %s: %s names no .qry a writer holds open\n",
               written->path.c_str(), QRY_ENVIRONMENT);
        return 0;
    }
    if (written->filed)
    {
        return qry_filed(run, written->path.c_str(), written->text.data(), written->text.size()) != 0ull;
    }
    const size_t slash = written->path.find_last_of("/\\");
    const std::string name = (slash == std::string::npos) ? written->path : written->path.substr(slash + 1u);
    return qry_hand(run, name.c_str(), written->text.data(), written->text.size(), QRY_ORDINARY) != 0ull;
}

// 1 where the link, as written or read as its kind, transfers control or waits, which cubin_safe.h's rule 2 keeps off
// the part, EXIT alone excepted
int rule_broken(const std::string &link)
{
    const size_t guarded = (link[0] == '@') ? link.find(' ') + 1u : 0u;
    const std::string operation = link.substr(guarded, link.find_first_of(". ", guarded) - guarded);
    return (operation != "EXIT") && std::any_of(std::begin(s_control_or_wait), std::end(s_control_or_wait),
                                                [&](const char *control) { return operation == control; });
}

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

std::string trimmed(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    const size_t last = text.find_last_not_of(" \t\r\n;");
    return (first == std::string::npos) ? std::string() : text.substr(first, last - first + 1u);
}

// the line of a listing as a link: 1, 0 where it is no instruction, or -1 where it is the branch to itself past the
// last exit, after which nothing of the chain is reached
int link_read(const std::string &line, Link *link)
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

int questions_read(const char *listing, const char *manifest, std::vector<Question> *questions)
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
std::string member_context(const Question &question, const std::string &position)
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
std::vector<std::string> links_past(const Question &one, const Question &other)
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
int question_integer(const Question &question)
{
    static const std::set<std::string> s_categories = {"operator", "compound", "unary", "conversion", "conditional",
                                                       "statement", "test", "pressure", "text_identity"};
    return s_categories.count(question.category) && (question.text.find("float") == std::string::npos) &&
           (question.text.find("double") == std::string::npos) && (question.text.find("__") == std::string::npos);
}
