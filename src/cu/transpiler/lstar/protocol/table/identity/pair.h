// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair.h: what the modules of the pair share: the work of one pair pass, and a link of a carrier a put logs its
// product at
#ifndef IDENTITY_PAIR_H
#define IDENTITY_PAIR_H

#include "identity_internal.h"

// each pair's question and places, held with the pair and reached from its own thread, since a pair waits on its
// round with both
extern thread_local RunQuestion *t_question;
extern thread_local std::vector<unsigned int> *t_places;

struct PairWork;

// A link of a carrier a form stands in at, as the put reads it: the pair's two forms, the first's parameters, text
// and the parameters the second reads, whether the two unify and the parameters bound to literals, the values the
// forms write of their own and the cases the pair is put over, the question and places the pair's thread holds, the
// product so far, the carrier's chain and the link found in it, the operands the link gives by name, the carrier's
// flow and the link's position, the stand-in written there, the question's address, the carrier's delta before the
// form, whether a lesson holds the stand-in, and whether the form reads in a shape no link read before
struct PutLink
{
    const std::string &from;
    const std::string &to;
    const std::vector<std::string> &from_names;
    const std::string &from_text;
    const std::set<std::string> &read_by_to;
    int unified;
    const std::map<char, std::string> &fixed;
    const std::set<unsigned long long> &qualifiers;
    const std::vector<std::set<unsigned long long>> &pair_cased;
    RunQuestion &question;
    std::vector<unsigned int> &places;
    ConceptProduct &product;
    const std::pair<const std::string, std::string> &chain;
    const std::smatch &found;
    std::map<std::string, std::string> &given;
    const std::pair<std::vector<CarrierLink>, long> &flow;
    long link_at;
    const char *link_address;
    const std::string &standing;
    const char *address;
    long delta;
    int stand_in_held;
    int shape_new;
    // the put beneath its ask in the log and the product asked at the link (pair_product.cu): 1 where a put added a
    // row that came back one way
    int logged(PairWork &work) const;
};

// The work of one pair pass: the host's answers to the carriers' cases, the ruleset, the part's machine, the
// carriers, the seed the order of the links of one magnitude is drawn from, the bridge's lines and its keys, the
// schema's forms in its order, the values the cases are made of, and the counts of conferences held, lessons taught
// and asks put
struct PairWork
{
    std::map<std::string, std::vector<std::string>> host;
    const Ruleset *sass = NULL;
    SassMachine *machine = NULL;
    Carriers carriers;
    unsigned long seed = 1ul;
    std::vector<std::string> bridge;
    std::vector<std::string> keys;
    std::vector<std::set<unsigned long long>> cased;
    unsigned int conferences = 0u;
    unsigned int lessons_taught = 0u;
    unsigned long long asks = 0ull;
    // every choice a put makes at a link, written to the log as `choice <flags> <link>` (pair.cu)
    void chosen(const char *const *names, unsigned int count, const std::string &link) const;
    void chose(const char *name, const std::string &link) const;
    // the conference a refused put calls (pair.cu): the overseer's guidance, OVERSEER_HANDS_OFF where no party called
    // a conference of `asked`
    unsigned int conferred(const std::string &from, const std::string &to, const std::string &statement,
                           const std::string &standing, const std::string &carrier_name, unsigned int scratch,
                           const RunQuestion &asked, const std::string &closed_here, const std::string &link_address);
    // `from` stood in for by `to` at each of its links in each carrier (pair_put.cu): 1 with its verdict, 0 where the
    // part answered none
    int put(const std::string &from, const std::string &to, std::string *verdict);
    // the lower witness of a pair whose puts asked nothing (pair_witness.cu): 1 with the verdict where the part
    // answered both forms at a link of a third, 0 otherwise
    int witness_lower(const std::string &first, const std::string &second, std::string *verdict);
};

#endif
