// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_record.h: a member's query record, its .kqr: every ask put to it and what came back, then the paths read off
// those asks (src/cu/types/file_defs/kqr/kqr.oracle.tsv)
#ifndef QUERY_RECORD_H
#define QUERY_RECORD_H

#include <string>
#include <unordered_map>
#include <vector>

// one untimed ask and what came back: its identity, the hash of its code, the registers, threads, blocks and launches
// it was put with, its count of cases and the hash of its cases, seven words apart by one space; answers, illegal,
// nothing, timed_out or censored, timed_out where the watchdog ended it and censored where the gate held it off the
// member to protect the whole; and the word of every case where it answered, or the refusal where it did not
struct QueryRecordAsk
{
    std::string identity;
    std::string answer;
    std::vector<unsigned long long> words;
    std::string refusal;
};

// one timed ask, a sample of its own: its identity as an ask's, what came back, and the nanoseconds its launches took
// together where it answered, or the refusal where it did not
struct QueryRecordSample
{
    std::string identity;
    std::string answer;
    unsigned long long nanoseconds;
    std::string refusal;
};

// a path read off the asks: a pair of the bridge and its verdict as the bridge writes it beneath the pair
struct QueryRecordPath
{
    std::string first;
    std::string second;
    std::string verdict;
};

// one line of the record's order: a cycle, its seed or one of its rounds, `words` what the line says after its kind;
// or an ask or a sample, `at` its place in the record's asks or samples
struct QueryRecordLine
{
    std::string kind;
    size_t at;
    std::string words;
};

// a member's record: the member, its asks, each found by its identity, and its samples, all in the order they were put
// with the cycles, seeds and rounds that put them, and its paths
struct QueryRecord
{
    std::string member;
    std::vector<QueryRecordAsk> asks;
    std::unordered_map<std::string, size_t> asked;
    std::vector<QueryRecordSample> samples;
    std::vector<QueryRecordLine> order;
    std::vector<QueryRecordPath> paths;
};

// the record at `path` read into `record`: 1, an empty record of `member` where there is no such file, or 0 and the
// line it stopped at and why in `error` where a line is none the record's oracle gives
int query_record_read(const std::string &path, const std::string &member, QueryRecord *record, std::string *error);

// `record` written to `path`, its asks and samples in the order they were put, with the cycles, seeds and rounds that
// put them, and then its paths: 1, or 0 and why in `error` where it could not be written
int query_record_write(const std::string &path, const QueryRecord &record, std::string *error);

// the ask of `identity` the record holds, NULL where it holds none
const QueryRecordAsk *query_record_find(const QueryRecord &record, const std::string &identity);

// `ask` held by the record after every ask before it, where the record holds no ask of its identity, and in place of
// the one it holds where that one timed out: a later answer resolves it
void query_record_keep(QueryRecord *record, const QueryRecordAsk &ask);

// `sample` held by the record after every ask and sample before it
void query_record_sample(QueryRecord *record, const QueryRecordSample &sample);

// a line `kind words`, a cycle, a seed or a round, held by the record after every line before it
void query_record_mark(QueryRecord *record, const std::string &kind, const std::string &words);

// the count of cycles the record holds
unsigned int query_record_cycles(const QueryRecord &record);

#endif
