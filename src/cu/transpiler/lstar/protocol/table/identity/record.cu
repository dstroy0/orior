// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.cu: the fields of the part's .ksc the walks read, the stall and the registers, the cases a question is put over, and the query record opened and closed
#include "identity_internal.h"

// the instructions' bits `field` takes at instruction `index` of `code`, sixteen bytes an instruction
unsigned long long stall_bits(const std::vector<unsigned char> &code, size_t index, const StallField &field)
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
void stall_bits_write(std::vector<unsigned char> *code, size_t index, const StallField &field,
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
std::string stall_operation(const Link &link, std::vector<std::string> *operands)
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
std::set<std::string> stall_written(const Link &link)
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
std::set<std::string> stall_read(const Link &link)
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
std::map<std::string, StallField> stall_fields(const char *ksc)
{
    std::map<std::string, StallField> fields;
    KscFile file = {};
    ksc_read(&file, ksc);
    for (unsigned int at = 0u; at < file.lines; at += 1u)
    {
        char name[64];
        StallField field = {0u, 0u};
        if (sscanf(file.line[at], "field %63s %u %u", name, &field.first, &field.bits) == 3)
        {
            fields[name] = field;
        }
    }
    ksc_free(&file);
    return fields;
}

// The cases a pair's product is put over where the cases a question holds leave it short of whole: each of the three
// operands put through the values every case is put through and `qualifiers`, the values the pair's forms write of
// their own, every combination of them, cut into runs the run channel holds. The run `run` of them into `question`,
// and the count of runs. The host computes none of them, and a product asks nothing of the host
unsigned int whole_cases(RunQuestion *question, const std::set<unsigned long long> &qualifiers, unsigned int run)
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
void stall_cases(RunQuestion *question, std::vector<unsigned int> *places)
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
unsigned long long record_hash(const void *bytes, size_t size, unsigned long long hash)
{
    const unsigned char *const at = (const unsigned char *)bytes;
    for (size_t place = 0u; place < size; place += 1u)
    {
        hash = (hash ^ at[place]) * 0x100000001b3ull;
    }
    return hash;
}

std::map<std::string, AskRecorded> s_record;

std::string s_record_ksc;

// The part's query record, its .kqr beside its .ksc and of the same stem, held in memory for the run: every untimed
// ask a cycle put and what came back, by the question's identity, every timed ask a sample of its own, and the paths
// the last cycle read off them. Whatever asks an untimed ask again, in this cycle or a later one, is answered from it,
// and the part is asked it once; an ask that timed out is asked again until an answer resolves it, and an ask the gate
// censored or the part answered illegal, timed or not, is censored without a process every time after, the part's
// answer kept as it gave it. It is read where R is read and written back where R is, and its path is
// empty where it could not be read, so that nothing writes over a record no mode read. What a cycle puts is placed in
// the record's order by its cycle and mode, its seed and its rounds (kqr_mark), each written before the first new ask
// or sample that follows it and nowhere where none follows: a cycle the record answers whole adds no line to it
KqrRecord s_query_record;

// the flags of the trace's choices, read from their table (src/cu/types/file_defs/klq/query_trace.tsv) when the pair
// mode opens
QueryTrace s_query_trace;

static std::string s_query_record_path;

int ksc_answers_written(const char *ksc, const std::string &asked, const std::vector<std::string> &rows,
                               QryWho who);

void cuts_asked(const std::vector<std::vector<unsigned char>> &codes, const std::vector<int> &dangerous,
                       const std::vector<unsigned int> &slots, const RunQuestion *question, unsigned long long *asks,
                       unsigned int mark, std::vector<RunQuestion> *asked);

// the record written back to the .ksc it was read from, every ask a row in the order of its keys
void record_close(void)
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
    if (!s_query_record_path.empty() && !kqr_write(&s_query_record, s_query_record_path.c_str()))
    {
        printf("  %s could not be written through the run's writer\n", s_query_record_path.c_str());
    }
    s_query_record_path.clear();
}

// the record read from `ksc`, written back to it when the program ends, and the query record beside it, a cycle of
// mode `mode` pending in its order; and the channel told how far a container's code reaches, as `ksc` holds it
void record_open(const char *ksc, const char *mode)
{
    s_record.clear();
    s_record_ksc = ksc;
    // how far a container's code reaches, for a register hold the cross gives the channel (run_channel.h)
    if (!run_channel_registers_past(ksc))
    {
        printf("  %s holds no answer of how far a container's code reaches: a register hold stays held\n", ksc);
    }
    KscFile held = {};
    static KscRow s_row;
    ksc_read(&held, ksc);
    for (unsigned int at = 0u; at < held.lines; at += 1u)
    {
        if (!ksc_row_read(held.line[at], "run", &s_row) || (strncmp(s_row.question, "ask ", 4u) != 0))
        {
            continue;
        }
        const std::string answer = s_row.answer;
        const std::string word = s_row.word;
        std::stringstream words(s_row.question + 4);
        std::string key;
        std::string part;
        for (unsigned int place = 0u; (place < 6u) && (words >> part); place += 1u)
        {
            key += ((place == 0u) ? "" : " ") + part;
        }
        std::string refusal;
        std::getline(words, refusal);
        refusal = (!refusal.empty() && (refusal[0] == ' ')) ? refusal.substr(1u) : refusal;
        // the word is the host's place of the first case apart, at most eight hex digits
        s_record[key] = AskRecorded{answer, (unsigned int)s_row.value, refusal};
    }
    ksc_free(&held);
    const std::string classified = ksc;
    const size_t stem_at = classified.find_last_of("/\\") + 1u;
    const size_t suffix_at = classified.find_last_of('.');
    char error[1280];
    s_query_record_path = classified.substr(0u, suffix_at) + ".kqr";
    if (!kqr_read(&s_query_record, s_query_record_path.c_str(), classified.substr(stem_at, suffix_at - stem_at).c_str(),
                  error, sizeof(error)))
    {
        printf("  %s: the query record is not read, and nothing is written to it\n", error);
        s_query_record_path.clear();
    }
    else
    {
        // the record a view of the run's table of asks, written as its rows put it together
        kqr_tabled(&s_query_record, qry_run(), s_query_record_path.c_str());
    }
    kqr_mark(&s_query_record, KQR_CYCLE_RECORD, mode);
    static int s_registered = 0;
    if (s_registered == 0)
    {
        atexit(record_close);
        s_registered = 1;
    }
}
