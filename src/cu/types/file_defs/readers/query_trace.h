// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_trace.h: the flags of the query's trace, each choice a put makes at a link written as the flags that hold
// there, ORed into one word, and read back by name (src/cu/types/file_defs/klq/query_trace.tsv)
#ifndef QUERY_TRACE_H
#define QUERY_TRACE_H

#include <map>
#include <string>
#include <vector>

// the trace's flags as its table defines them: each flag's word by its name, and each flag's name and gloss in the
// table's order
struct QueryTrace
{
    std::map<std::string, unsigned long long> flags;
    std::vector<std::pair<unsigned long long, std::string>> named;
    std::vector<std::string> glosses;
};

// the table at src/cu/types/file_defs/klq/query_trace.tsv in the tree this file was built from
std::string query_trace_path(void);

// the table at `path` read into `trace`: 1, or 0 and why in `error` where it could not be read or a line of it gives no
// flag of one bit, or one two names share
int query_trace_read(const std::string &path, QueryTrace *trace, std::string *error);

// the word of the flag `name`, 0 where the table gives no such flag
unsigned long long query_trace_flag(const QueryTrace &trace, const std::string &name);

// the names of the flags `word` holds, in the table's order, a space between each
std::string query_trace_names(const QueryTrace &trace, unsigned long long word);

#endif
