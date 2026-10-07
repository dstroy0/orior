// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_record.cu: a member's .kqr read, written, and asked of by an ask's identity
#include "query_record.h"

#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <sstream>

// the count of words of an ask's identity
#define QUERY_RECORD_IDENTITY_WORDS 7u

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
        if (kind != "ask")
        {
            *error = path + ":" + std::to_string(number) + ": " + kind + " is no line of a query record";
            return 0;
        }
        QueryRecordAsk held;
        std::string word;
        for (unsigned int at = 0u; (at < QUERY_RECORD_IDENTITY_WORDS) && (words >> word); at += 1u)
        {
            held.identity += (at == 0u) ? word : (" " + word);
        }
        words >> held.answer;
        if (held.answer == "answers")
        {
            while (words >> word)
            {
                held.words.push_back(std::stoull(word, nullptr, 16));
            }
        }
        else
        {
            std::getline(words >> std::ws, held.refusal);
        }
        if (std::count(held.identity.begin(), held.identity.end(), ' ') != (long)(QUERY_RECORD_IDENTITY_WORDS - 1u) ||
            ((held.answer != "answers") && (held.answer != "illegal") && (held.answer != "nothing")))
        {
            *error = path + ":" + std::to_string(number) + ": an ask with no identity or no answer";
            return 0;
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
    for (const QueryRecordAsk &held : record.asks)
    {
        fprintf(file, "ask %s %s", held.identity.c_str(), held.answer.c_str());
        for (const unsigned long long word : held.words)
        {
            fprintf(file, " %llx", word);
        }
        if (!held.refusal.empty())
        {
            fprintf(file, " %s", held.refusal.c_str());
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
    if (record->asked.count(ask.identity) != 0u)
    {
        return;
    }
    record->asked[ask.identity] = record->asks.size();
    record->asks.push_back(ask);
}
