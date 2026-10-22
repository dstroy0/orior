// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// decoder_internal.h: what the modules of orior_klq_decoder share: its includes, one put of the log, the metas, sets
// and categories a pair is read into, the functions over puts, and the decoding of one log
#ifndef KLQ_DECODER_INTERNAL_H
#define KLQ_DECODER_INTERNAL_H

#include "carrier_flow.h"
#include "concept_product.h"
#include "../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../includes/file_defs/klq/query_trace.h"

#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>


// one put of a form in another's place, as the log holds it
struct LoggedPut
{
    std::string from;
    std::string to;
    std::string address;
    std::string from_text;
    std::string to_text;
    std::vector<std::string> only;
    // what came back of every case, `=` alike, `x` apart and `-` unbracketed, or `recorded` or `refused`
    std::string came_back;
    // the cases the ask was put over, by their place in the log's case sets
    size_t cases_at;
    // each register the form reads that a case reaches, as the form writes it, and its value on every case as the part
    // holds it at the form, empty where the part refused the read
    std::vector<std::pair<std::string, std::vector<unsigned long long>>> reads;
    // the value the form writes and the value its stand-in writes on every case, each read just after the form
    std::vector<unsigned long long> writes_from;
    std::vector<unsigned long long> writes_to;
    // the values the pair's two forms write of their own
    std::set<unsigned long long> qualifiers;
};

// 1 where the part answered every read of `put` on every case it was put over

static const char *const s_metas[] = {"structural_coherence", "syntactic_coherence",    "pragmatic_coherence",
                                      "data_quality_coherence", "noise_coherence", "folding_coherence"};

// the sets, each written beneath a pair's verdict in the order the tree holds them, `unknown_coherence` last. The
// texts read a pair into a category, and only an answer reads it out of `unknown_coherence`
static const char *const s_sets[] = {
    "comparison_coherence",  "equality_coherence", "order_coherence",   "operation_coherence",
    "commutative_coherence", "verb_coherence",     "statement_coherence",   "qualifier_coherence",
    "modifier_coherence",    "negation_coherence", "witness_coherence", "range_coherence",
    "vector_coherence",      "control_coherence",  "switch_coherence",  "unknown_coherence"};

// the categories the texts read a pair into, before any ask
static const char *const s_categories[] = {"comparison_coherence", "equality_coherence", "order_coherence",
                                           "operation_coherence",  "commutative_coherence", "verb_coherence",
                                           "range_coherence",      "vector_coherence",   "control_coherence",
                                           "switch_coherence"};

// 1 where `name` is one of the `count` names of `names`

// the first word of a link's text up to its first `.`, its guard passed over
std::string operation_stem(const std::string &text);
// the set a put is read into off its cases apart (decoder/puts.cu)
std::string put_decoded(const LoggedPut &put, const std::vector<std::vector<std::string>> &cases,
                        const std::string &folder);
// 1 where `name` is one of the `count` names
int named_in(const char *const *names, size_t count, const std::string &name);
// the key of a pair in the maps below, its two forms in order
std::string pair_key(const std::string &first, const std::string &second);

// The decoding of one log: the puts and case sets read off it, each pair's metas, the categories its texts read
// it into, the pairs whose texts the log read, the trace and the choices each pair's puts made, the pairs the
// lower witness answered, each pair's set, qualifiers, product and identity, the bridge as the run holds it, and
// the bridge written with what each pair is read into, counted
struct Decoding
{
    std::vector<std::vector<std::vector<std::string>>> case_sets;
    std::vector<LoggedPut> puts;
    // each pair's metas, the internal nodes of the tree it hangs from, the categories its texts read it into, and the
    // pairs whose texts the log read
    std::map<std::string, std::set<std::string>> metas;
    std::map<std::string, std::set<std::string>> categories;
    std::set<std::string> faceted;
    // the choices each pair's puts made, read off the log's `choice <flags> <link>` lines by the trace's table: how
    // many of its links held each flag, and how many of every pair's
    QueryTrace trace;
    std::map<std::string, std::map<unsigned long long, unsigned int>> choices;
    std::map<unsigned long long, unsigned int> chosen;
    // the pairs the lower witness answered: a choice of a link of a third form the part answered alike or apart
    std::set<std::string> witnessed;
    // each pair's set, read off its puts apart, or a qualifier's where every put the part answered came back alike
    std::map<std::string, std::set<std::string>> decoded;
    // each pair's qualifiers, the values its forms write of their own, and the values its product is whole over
    std::map<std::string, std::set<unsigned long long>> pair_qualifiers;
    // each answered pair's product, its rows over every put the part answered and read
    std::map<std::string, ConceptProduct> products;
    // a pair no test read into a set whose whole product agrees only where it reads a value its forms write of their
    // own is a qualifier's, that value its qualifier
    std::map<std::string, std::string> identities;
    // the bridge as the run holds it, and the bridge written with each pair's lights beneath it
    std::vector<std::string> bridge;
    std::vector<std::string> written;
    // the pair the bridge's lines are beneath as it is written, and what was read beneath it
    std::string pair_held;
    std::vector<std::string> sets_held;
    std::vector<std::string> metas_held;
    std::string identity_held;
    std::string intent_held;
    // the metas, sets and concepts written, counted, and each set's pairs, signal and noise
    std::map<std::string, unsigned int> counted;
    std::map<std::string, unsigned int> concepts;
    std::map<std::string, unsigned int> named_pairs;
    std::map<std::string, size_t> set_signal;
    std::map<std::string, size_t> set_noise;
    // the log of the run at `qry` read into the puts, metas, categories and choices (decoder/log.cu): 0, or 1
    // where the log or the trace could not be read
    int log_read(const char *qry);
    // each pair's set, product and identity decoded off its puts, the carriers at `engine`
    void products_decoded(const char *engine);
    // the bridge at `klq` read from the run, each pair's concept folded (decoder/bridge.cu): 0, or 1 where it
    // could not be read
    int bridge_read(const char *klq);
    // the pair the bridge's lines are beneath closed: what it is read into written beneath it and counted
    void pair_closed();
    // the bridge folded line by line, each pair closed in its turn
    void bridge_folded();
    // the bridge written handed to the run's writer for `klq`: 0, or 1 where it was not handed
    int bridge_handed(const char *klq);
    // each pair's lights, the choices, the concepts shared and each set's rows printed
    void printed(const char *klq);
};

#endif
