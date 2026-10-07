// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_record.cu: a member's .kqr read, written, and asked of by an ask's identity
#include "query_record.h"

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <sstream>

// the count of words of an ask's identity
#define QUERY_RECORD_IDENTITY_WORDS 7u

// an ask's or a sample's identity and what came back read from the words after its kind into `identity` and
// `answer`: 1, or 0 where it has no identity or its answer is none the record's oracle gives
static int query_record_answer_read(std::stringstream &words, std::string *identity, std::string *answer)
{
    std::string word;
    for (unsigned int at = 0u; (at < QUERY_RECORD_IDENTITY_WORDS) && (words >> word); at += 1u)
    {
        *identity += (at == 0u) ? word : (" " + word);
    }
    words >> *answer;
    return (std::count(identity->begin(), identity->end(), ' ') == (long)(QUERY_RECORD_IDENTITY_WORDS - 1u)) &&
           ((*answer == "answers") || (*answer == "illegal") || (*answer == "nothing") || (*answer == "timed_out") ||
            (*answer == "censored"));
}

int query_record_read(const std::string &path, const std::string &member, QueryRecord *record, std::string *error)
{
    *record = QueryRecord();
    record->member = member;
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return 1;
    }
    std::string line;
    unsigned int number = 0u;
    while (std::getline(file, line))
    {
        number += 1u;
        line = (!line.empty() && (line.back() == '\r')) ? line.substr(0u, line.size() - 1u) : line;
        std::stringstream words(line);
        std::string kind;
        words >> kind;
        if (kind.empty() || (kind[0] == '#'))
        {
            continue;
        }
        if ((number == 1u) && (kind == "kqr"))
        {
            words >> record->member;
            continue;
        }
        if ((kind == "cycle") || (kind == "seed") || (kind == "round"))
        {
            std::string said;
            std::getline(words >> std::ws, said);
            query_record_mark(record, kind, said);
            continue;
        }
        if (kind == "pair")
        {
            QueryRecordPath held;
            words >> held.first >> held.second;
            std::getline(words >> std::ws, held.verdict);
            if (held.verdict.empty())
            {
                *error = path + ":" + std::to_string(number) + ": a pair with no verdict";
                return 0;
            }
            record->paths.push_back(held);
            continue;
        }
        if (kind == "sample")
        {
            QueryRecordSample held{std::string(), std::string(), 0ull, std::string()};
            if (!query_record_answer_read(words, &held.identity, &held.answer))
            {
                *error = path + ":" + std::to_string(number) + ": a sample with no identity or no answer";
                return 0;
            }
            if (held.answer == "answers")
            {
                words >> held.nanoseconds;
            }
            else
            {
                std::getline(words >> std::ws, held.refusal);
            }
            query_record_sample(record, held);
            continue;
        }
        if (kind != "ask")
        {
            *error = path + ":" + std::to_string(number) + ": " + kind + " is no line of a query record";
            return 0;
        }
        QueryRecordAsk held;
        if (!query_record_answer_read(words, &held.identity, &held.answer))
        {
            *error = path + ":" + std::to_string(number) + ": an ask with no identity or no answer";
            return 0;
        }
        if (held.answer == "answers")
        {
            std::string word;
            while (words >> word)
            {
                held.words.push_back(std::stoull(word, nullptr, 16));
            }
        }
        else
        {
            std::getline(words >> std::ws, held.refusal);
        }
        query_record_keep(record, held);
    }
    return 1;
}

int query_record_write(const std::string &path, const QueryRecord &record, std::string *error)
{
    FILE *const file = fopen(path.c_str(), "wb");
    if (file == NULL)
    {
        *error = path + " could not be opened";
        return 0;
    }
    fprintf(file, "kqr %s\n", record.member.c_str());
    for (const QueryRecordLine &line : record.order)
    {
        if (line.kind == "ask")
        {
            const QueryRecordAsk &held = record.asks[line.at];
            fprintf(file, "ask %s %s", held.identity.c_str(), held.answer.c_str());
            for (const unsigned long long word : held.words)
            {
                fprintf(file, " %llx", word);
            }
            if (!held.refusal.empty())
            {
                fprintf(file, " %s", held.refusal.c_str());
            }
        }
        else if (line.kind == "sample")
        {
            const QueryRecordSample &held = record.samples[line.at];
            fprintf(file, "sample %s %s", held.identity.c_str(), held.answer.c_str());
            if (held.answer == "answers")
            {
                fprintf(file, " %llu", held.nanoseconds);
            }
            else if (!held.refusal.empty())
            {
                fprintf(file, " %s", held.refusal.c_str());
            }
        }
        else
        {
            fprintf(file, "%s %s", line.kind.c_str(), line.words.c_str());
        }
        fprintf(file, "\n");
    }
    for (const QueryRecordPath &held : record.paths)
    {
        fprintf(file, "pair %s %s %s\n", held.first.c_str(), held.second.c_str(), held.verdict.c_str());
    }
    if (fclose(file) != 0)
    {
        *error = path + " could not be written whole";
        return 0;
    }
    return 1;
}

const QueryRecordAsk *query_record_find(const QueryRecord &record, const std::string &identity)
{
    const auto found = record.asked.find(identity);
    return (found == record.asked.end()) ? NULL : &record.asks[found->second];
}

void query_record_keep(QueryRecord *record, const QueryRecordAsk &ask)
{
    const auto held = record->asked.find(ask.identity);
    if (held != record->asked.end())
    {
        QueryRecordAsk &kept = record->asks[held->second];
        kept = (kept.answer == "timed_out") ? ask : kept;
        return;
    }
    record->asked[ask.identity] = record->asks.size();
    record->order.push_back(QueryRecordLine{"ask", record->asks.size(), std::string()});
    record->asks.push_back(ask);
}

void query_record_sample(QueryRecord *record, const QueryRecordSample &sample)
{
    record->order.push_back(QueryRecordLine{"sample", record->samples.size(), std::string()});
    record->samples.push_back(sample);
}

void query_record_mark(QueryRecord *record, const std::string &kind, const std::string &words)
{
    record->order.push_back(QueryRecordLine{kind, 0u, words});
}

unsigned int query_record_cycles(const QueryRecord &record)
{
    return (unsigned int)std::count_if(record.order.begin(), record.order.end(),
                                       [](const QueryRecordLine &line) { return line.kind == "cycle"; });
}
