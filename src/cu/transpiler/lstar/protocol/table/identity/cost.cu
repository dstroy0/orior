// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cost.cu: the cost: each arrangement put at a link of its operator's word and the part's clock read around it
#include "identity_internal.h"
#include "../../../../../../../includes/parsers/who.h"

// The link the arrangements of an operator, `own` among `rows`, are put at: of the links of the word of the web they
// produce (word_produced), in the order the pair puts links, the first whose result a later link reads, that carries
// the cases, whose operands and result are registers, whose chain names its registers, and which the chain can be cut
// at with the clock read in its place. 1; 0 with `link->word` NULL where they produce no word the ruleset writes with
// two reads, and 0 where no link is such
static int operator_link(const Ruleset *sass, Carriers &carriers, unsigned long seed, const std::string &operator_name,
                         const std::vector<CostRow> &rows, const std::vector<size_t> &own, OperatorLink *link)
{
    link->word = NULL;
    for (size_t at = 0u; (link->word == NULL) && (at < own.size()); at += 1u)
    {
        link->word = rows[own[at]].nodes.empty() ? NULL : word_produced(rows[own[at]].nodes);
    }
    std::vector<std::string> names;
    const std::string text = (link->word == NULL) ? std::string() : marked(sass, link->word->name, &names);
    if (text.empty() || (names.size() != 3u))
    {
        link->word = NULL;
        return 0;
    }
    std::map<char, unsigned int> group;
    const std::vector<FoundLink> found_links = links_of(carriers, seed, text, &group, operator_name);
    const std::regex named_register("R[0-9]+");
    for (const FoundLink &found_link : found_links)
    {
        const auto &chain = *found_link.chain;
        const std::smatch &found = found_link.found;
        std::map<std::string, std::string> given;
        for (const auto &marker : group)
        {
            const size_t parameter = (size_t)(marker.first - 'A');
            if (parameter < names.size())
            {
                given[names[parameter]] = found[marker.second].str();
            }
        }
        const std::string written = given[names[0]];
        const std::string after = chain.second.substr((size_t)(found.position(0) + found.length(0)));
        const long link_at = (long)std::count(chain.second.begin(), chain.second.begin() + found.position(0), '\n');
        const auto &flow = carriers.flows[chain.first];
        if (!std::regex_match(written, named_register) || !std::regex_match(given[names[1]], named_register) ||
            !std::regex_match(given[names[2]], named_register) ||
            !std::regex_search(after, std::regex("(^|[^A-Za-z0-9_])" + written + "([^0-9]|$)")) ||
            !carrier_form_carries(flow.first, flow.second, found.str(0), link_at) ||
            (carriers.magnitudes[chain.first].first == 0u))
        {
            continue;
        }
        const unsigned int base = carriers.magnitudes[chain.first].first;
        const std::string start = ruleset_register(sass, "file", base + 1u);
        const std::string end = ruleset_register(sass, "file", base + 2u);
        const std::string clock =
            "\tCS2R.32 \t" + start + ", SR_CLOCKLO;\n\tCS2R.32 \t" + end + ", SR_CLOCKLO;\n" +
            arrangement_written(sass, {{PRECEPT_SUB, PRECEPT_LEFT, PRECEPT_RIGHT}}, {end, start}, "", end, base + 4u);
        const std::string found_text = found.str(0);
        const long form_lines = (long)std::count(found_text.begin(), found_text.end(), '\n') +
                                ((!found_text.empty() && (found_text.back() == '\n')) ? 0 : 1);
        const std::string clock_cut =
            carrier_cut_at_read(flow.first, flow.second, chain.second, link_at, form_lines, end, clock);
        if (clock_cut.empty())
        {
            continue;
        }
        link->link = found_link;
        link->operands = {given[names[1]], given[names[2]]};
        link->to = written;
        link->base = base;
        link->link_at = link_at;
        link->form_lines = form_lines;
        link->clock_cut = clock_cut;
        return 1;
    }
    return 0;
}

// `placed` with each place `{n}` filled as the relation's tuple is: `{0}` the first word, `{1}` the second and `{2}` the
// answer, the register written `to`
static std::string placed_filled(const std::string &placed, const std::string &to, const std::string &left,
                                 const std::string &right)
{
    std::string text = placed;
    const std::pair<const char *, const std::string *> places[3] = {{"{0}", &left}, {"{1}", &right}, {"{2}", &to}};
    for (const auto &place : places)
    {
        for (size_t at = text.find(place.first); at != std::string::npos; at = text.find(place.first, at))
        {
            text.replace(at, 3u, *place.second);
            at += place.second->size();
        }
    }
    return text;
}

// The arrangements of a part's .kdm costed on the part (P6, the rank). An operator's arrangements are put at one link
// of a carrier of ours that stores the word they produce (word_web.h): the first link, in the order the pair puts
// links, whose result a later link reads, that carries the cases, and whose operands and result are registers. The
// link's operands are the arrangement's and its result the arrangement's root's. An arrangement each of whose nodes is
// the precept of a word the ruleset writes is written node by node through it, and one with a node no such word
// writes is left. Each is queried two questions over every case the host computes the carrier on: the carrier with the
// arrangement in the link's place, checked against the host's answers; and the part's clock read before and after the
// arrangement written COST_REPEATS times, each time over the last time's result, the difference stored in place of
// the carrier's answer. Each link is queried its clock read twice with nothing between, the clock's own. No question
// waits on another's answer, and every one is written before any is put: all are put in one round, in an order drawn
// from the run's IDENTITY_SEED, and the round after puts again only those the part refused, declaring every register
// its file uses (cuts_queried). The clock's reading is the words the part stores and no launch's time: each is an query
// the query record answers once it is queried, and a question the record contains is never put again. An arrangement
// the part answers apart or refuses records no cost. Its cost is the least difference any case read, less the least its
// link's clock read alone, over COST_REPEATS, in the part's clock count: written in its row of the .kdm with 1 run, the
// one reading. the run's blob cost.txt contains every arrangement with its link and what came of it. The machine file's
// forms are costed the same way where `forms` names the blob the vendor's writer placed them in
// (orior_khw_machine_write, krs): a line `<relation> <signed> <low> <high> <text>`, the text with each operand of the
// relation's tuple as its place. A form with the three places of a two-word relation is written at the first link any
// operator's arrangements are put at, COST_REPEATS times over its own result between the clock reads, and queried its
// clock alone; no host computes it, since the protocol classified it. Its cost is the least difference any case read
// less the link's clock read alone, and its spread the max difference less the least, both in the part's clock count
// over COST_REPEATS: written as the part's .ksc stores a cost, `run answers <least> cost <low> <high> <spread>`, every
// cost row the file contained before let go, since one reading is for every form
int identity_cost(const char *engine, const char *answers, const char *ksc, const std::string &folder,
                         const char *kdm, const char *machine_path, const char *forms, const char *const *carrier)
{
    std::map<std::string, std::vector<std::string>> host = host_answers_read(answers);
    const Ruleset *const sass = identity_ruleset();
    if (sass == NULL)
    {
        printf("orior_klq_identity cost: the ruleset %s names is not read\n", IDENTITY_RULESET);
        return 1;
    }
    Carriers carriers;
    carriers_read(engine, host, &carriers);
    const unsigned long seed = identity_seed();
    // the .kdm's lines, and each arrangement's row among them
    std::vector<std::string> lines;
    std::vector<CostRow> rows;
    std::map<std::string, std::vector<size_t>> operators;
    kdm_rows_read(kdm, &lines, &rows, &operators);
    // the machine file's forms a cost is queried of, as the vendor's writer placed them
    struct CostForm
    {
        std::string relation;
        unsigned long long low;
        unsigned long long high;
        std::string placed;
        unsigned long long least;
        unsigned long long max;
        std::string verdict;
    };
    std::vector<CostForm> costed_forms;
    if (forms != NULL)
    {
        // the latest blob of `forms` read where the run stores it, one row a form (forms_blob.h)
        const unsigned char *rows = NULL;
        unsigned long long rows_size = 0ull;
        if (!qry_member_blob(forms, &rows, &rows_size, NULL))
        {
            rows = NULL;
        }
        for (unsigned long long row_at = 0ull; (rows != NULL) && (row_at < (rows_size / sizeof(FormsBlobRow)));
             row_at += 1ull)
        {
            FormsBlobRow row;
            memcpy(&row, rows + (row_at * sizeof(FormsBlobRow)), sizeof(row));
            row.relation[sizeof(row.relation) - 1u] = '\0';
            row.text[sizeof(row.text) - 1u] = '\0';
            CostForm form = {row.relation, row.low, row.high, row.text, ULLONG_MAX, 0ull, std::string()};
            const int two_words = (form.placed.find("{0}") != std::string::npos) &&
                                  (form.placed.find("{1}") != std::string::npos) &&
                                  (form.placed.find("{2}") != std::string::npos) &&
                                  (form.placed.find("{3}") == std::string::npos);
            form.verdict = two_words ? std::string() : std::string("no two-word places");
            costed_forms.push_back(form);
        }
    }
    if (!run_channel_open(carrier, 60000000ull))
    {
        return 1;
    }
    record_open(ksc, "cost");
    kqr_mark(&s_query_record, KQR_SEED_RECORD, std::to_string(seed).c_str());
    RunWritten table = {(folder + "/cost.txt"), 0, std::string()};
    // an arrangement's chain contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    const std::unique_ptr<RunQuestion> question(new RunQuestion());
    question->place = RUN_PLACE_NONE;
    std::vector<unsigned int> places;
    stall_cases(question.get(), &places);
    unsigned long long queries = 0ull;
    const auto assembled = [machine_path](const std::string &text) -> std::vector<unsigned char> {
        return text.empty() ? std::vector<unsigned char>() : vendor_assembled(machine_path, text);
    };
    // every question of the job, written before any is put: its code, the registers it declares, the row it queries of,
    // or the link whose clock it reads alone where it is past every row, and whether it checks the row against the host
    // or reads its clock
    struct Planned
    {
        std::vector<unsigned char> code;
        unsigned int mark;
        size_t row;
        int host_checked;
        std::string chain;
        // the machine file's form the question reads the clock over, -1 where it is an arrangement's or a link's own
        long form;
    };
    std::vector<Planned> planned;
    std::vector<unsigned long long> clock_least;
    std::map<size_t, size_t> row_link;
    // the link the machine file's forms are put at: the first any operator's arrangements are, and its clock alone
    OperatorLink forms_link;
    int forms_linked = 0;
    size_t forms_clock_at = 0u;
    const auto own_left = [&rows](const std::vector<size_t> &own, const std::string &verdict) {
        for (const size_t at : own)
        {
            rows[at].verdict = rows[at].verdict.empty() ? verdict : rows[at].verdict;
        }
    };
    for (const auto &each : operators)
    {
        const std::vector<size_t> &own = each.second;
        OperatorLink operator_at;
        if (!operator_link(sass, carriers, seed, each.first, rows, own, &operator_at))
        {
            if (operator_at.word == NULL)
            {
                run_print(&table, "%s: its arrangements produce no word of two reads the ruleset writes\n",
                          each.first.c_str());
                own_left(own, "unwritable");
                continue;
            }
            run_print(&table, "%s: no link of %s carries the cases with registers alone and can be cut\n",
                      each.first.c_str(), operator_at.word->name);
            own_left(own, "no_link");
            continue;
        }
        const Word *const word = operator_at.word;
        const std::vector<std::string> &operands = operator_at.operands;
        const std::string &to = operator_at.to;
        const unsigned int base = operator_at.base;
        const std::string &clock_cut = operator_at.clock_cut;
        const auto &chain = *operator_at.link.chain;
        const std::smatch &found = operator_at.link.found;
        const auto &flow = carriers.flows[chain.first];
        const long link_at = operator_at.link_at;
        const std::string found_text = found.str(0);
        const long form_lines = operator_at.form_lines;
        char address[64];
        snprintf(address, sizeof(address), "%s:%04x", chain.first.c_str(), 16u * (unsigned int)link_at);
        run_print(&table, "%s at %s, %s in place of %s", each.first.c_str(), address, word->name, found_text.c_str());
        run_print(&table, "%s", (!found_text.empty() && (found_text.back() == '\n')) ? "" : "\n");
        const std::string ones = ruleset_register(sass, "file", base);
        const std::string start = ruleset_register(sass, "file", base + 1u);
        const std::string end = ruleset_register(sass, "file", base + 2u);
        const std::string last = ruleset_register(sass, "file", base + 3u);
        const unsigned int mark = base + 4u + PRECEPT_NODES;
        const std::string ones_written =
            arrangement_written(sass, {{PRECEPT_NOT, PRECEPT_ZERO, PRECEPT_NONE}}, {}, "", ones, base + 4u);
        const std::string before = chain.second.substr(0u, (size_t)found.position(0));
        const std::string past = chain.second.substr((size_t)(found.position(0) + found.length(0)));
        const size_t clock_at = clock_least.size();
        clock_least.push_back(ULLONG_MAX);
        planned.push_back(Planned{assembled(clock_cut), mark, rows.size() + clock_at, 0, chain.first, -1});
        if (!forms_linked)
        {
            forms_link = operator_at;
            forms_clock_at = clock_at;
            forms_linked = 1;
        }
        // each arrangement written at the link, and its clock read over it written COST_REPEATS times, each time
        // reading the last time's result as its first operand
        for (const size_t at : own)
        {
            CostRow &row = rows[at];
            if (!row.verdict.empty())
            {
                continue;
            }
            const std::string located = arrangement_written(sass, row.nodes, operands, ones, to, base + 4u);
            if (located.empty() || ones_written.empty())
            {
                row.verdict = "unwritable";
                continue;
            }
            std::vector<unsigned char> code = assembled(before + ones_written + located + past);
            if (code.empty())
            {
                row.verdict = "unassembled";
                continue;
            }
            std::string block = ones_written + "\tCS2R.32 \t" + start + ", SR_CLOCKLO;\n";
            for (unsigned int again = 0u; again < COST_REPEATS; again += 1u)
            {
                block += arrangement_written(sass, row.nodes, {(again == 0u) ? operands[0] : last, operands[1]}, ones,
                                             last, base + 4u);
            }
            block += "\tCS2R.32 \t" + end + ", SR_CLOCKLO;\n" +
                     arrangement_written(sass, {{PRECEPT_SUB, PRECEPT_LEFT, PRECEPT_RIGHT}}, {end, start}, "", end,
                                         base + 4u);
            std::vector<unsigned char> clocked =
                assembled(carrier_cut_at_read(flow.first, flow.second, chain.second, link_at, form_lines, end, block));
            if (clocked.empty())
            {
                row.verdict = "unassembled";
                continue;
            }
            planned.push_back(Planned{code, mark, at, 1, chain.first, -1});
            planned.push_back(Planned{clocked, mark, at, 0, chain.first, -1});
            row_link[at] = clock_at;
        }
    }
    // each form of the machine file written at the forms' link, its clock read over it written COST_REPEATS times,
    // each time reading the last time's result as its first operand
    for (size_t at = 0u; at < costed_forms.size(); at += 1u)
    {
        CostForm &form = costed_forms[at];
        if (!form.verdict.empty())
        {
            continue;
        }
        if (!forms_linked)
        {
            form.verdict = "no_link";
            continue;
        }
        const auto &chain = *forms_link.link.chain;
        const auto &flow = carriers.flows[chain.first];
        const std::string start = ruleset_register(sass, "file", forms_link.base + 1u);
        const std::string end = ruleset_register(sass, "file", forms_link.base + 2u);
        const std::string last = ruleset_register(sass, "file", forms_link.base + 3u);
        const unsigned int mark = forms_link.base + 4u + PRECEPT_NODES;
        std::string block = "\tCS2R.32 \t" + start + ", SR_CLOCKLO;\n";
        for (unsigned int again = 0u; again < COST_REPEATS; again += 1u)
        {
            block += "\t" +
                     placed_filled(form.placed, last, (again == 0u) ? forms_link.operands[0] : last,
                                   forms_link.operands[1]) +
                     ";\n";
        }
        block += "\tCS2R.32 \t" + end + ", SR_CLOCKLO;\n" +
                 arrangement_written(sass, {{PRECEPT_SUB, PRECEPT_LEFT, PRECEPT_RIGHT}}, {end, start}, "", end,
                                     forms_link.base + 4u);
        std::vector<unsigned char> clocked = assembled(carrier_cut_at_read(
            flow.first, flow.second, chain.second, forms_link.link_at, forms_link.form_lines, end, block));
        if (clocked.empty())
        {
            form.verdict = "unassembled";
            continue;
        }
        planned.push_back(Planned{clocked, mark, 0u, 0, chain.first, (long)at});
    }
    // every question in one round, in an order drawn from the seed
    std::seed_seq order_seed{(unsigned int)seed};
    std::mt19937 drawn(order_seed);
    std::shuffle(planned.begin(), planned.end(), drawn);
    std::vector<std::vector<unsigned char>> round;
    unsigned int mark = 0u;
    for (const Planned &each : planned)
    {
        round.push_back(each.code);
        mark = std::max(mark, each.mark);
    }
    // the plan passed as the teacher passes one, for the panel: the questions of the one round
    char plan[64];
    const int plan_length = snprintf(plan, sizeof(plan), "cost %zu\n", round.size());
    if ((plan_length > 0) && (qry_member_run() != NULL))
    {
        qry_member_append("plan.txt", plan, (unsigned long long)plan_length, QRY_ORDINARY);
    }
    printf("  plan: cost %zu\n", round.size());
    printf("  %zu questions written, put in one round\n", round.size());
    std::vector<RunQuestion> questions;
    cuts_queried(round, std::vector<int>(round.size(), 0), std::vector<unsigned int>(round.size(), RUN_PLACE_NONE),
                 question.get(), &queries, mark, &questions);
    // a row alike where the part answers it as the host does on every case, and the least each clock read
    for (size_t at = 0u; at < planned.size(); at += 1u)
    {
        const Planned &each = planned[at];
        const RunQuestion &answer = questions[at];
        if (each.form >= 0)
        {
            CostForm &form = costed_forms[(size_t)each.form];
            if (answer.outcome != RUN_ANSWERED)
            {
                form.verdict = std::string("refused ") + answer.refused;
                continue;
            }
            for (unsigned int place = 0u; place < question->cases; place += 1u)
            {
                const unsigned long long read = answer.answered[place] & 0xffffffffull;
                form.least = std::min(form.least, read);
                form.max = std::max(form.max, read);
            }
            continue;
        }
        if (each.host_checked)
        {
            CostRow &row = rows[each.row];
            if (answer.outcome != RUN_ANSWERED)
            {
                row.verdict = std::string("refused ") + answer.refused;
                continue;
            }
            for (unsigned int place = 0u; row.verdict.empty() && (place < question->cases); place += 1u)
            {
                if (bracket_contains(host[each.chain][places[place]], answer.answered[place]) == 0)
                {
                    row.verdict = "apart at case " + std::to_string(places[place]);
                }
            }
            row.verdict = row.verdict.empty() ? std::string("alike") : row.verdict;
            continue;
        }
        if (answer.outcome != RUN_ANSWERED)
        {
            continue;
        }
        unsigned long long least = ULLONG_MAX;
        for (unsigned int place = 0u; place < question->cases; place += 1u)
        {
            least = std::min(least, answer.answered[place] & 0xffffffffull);
        }
        unsigned long long &smallest =
            (each.row < rows.size()) ? rows[each.row].least : clock_least[each.row - rows.size()];
        smallest = std::min(smallest, least);
    }
    run_channel_close();
    // each cost an exact rational in the part's clock count
    unsigned int costed = 0u;
    std::map<std::string, unsigned int> left;
    for (CostRow &row : rows)
    {
        const auto linked = row_link.find((size_t)(&row - rows.data()));
        const unsigned long long clock = (linked == row_link.end()) ? ULLONG_MAX : clock_least[linked->second];
        if ((row.verdict != "alike") || (row.least == ULLONG_MAX) || (clock == ULLONG_MAX))
        {
            const std::string reason = row.verdict.substr(0u, row.verdict.find_first_of(" ,"));
            left[(row.verdict == "alike") ? std::string("untimed") : reason] += 1u;
            run_print(&table, "  %s: %s\n", row.fields[2].c_str(),
                      row.verdict.empty() ? "unwritable" : row.verdict.c_str());
            continue;
        }
        long long numerator = (long long)row.least - (long long)clock;
        long long denominator = COST_REPEATS;
        const long long divisor = std::gcd(numerator < 0 ? -numerator : numerator, denominator);
        numerator /= (divisor == 0) ? 1 : divisor;
        denominator /= (divisor == 0) ? 1 : divisor;
        row.fields[3] = std::to_string(numerator) + ((denominator == 1) ? "" : ("/" + std::to_string(denominator)));
        row.fields[4] = "1";
        lines[row.line] =
            row.fields[0] + "\t" + row.fields[1] + "\t" + row.fields[2] + "\t" + row.fields[3] + "\t" + row.fields[4];
        run_print(&table, "  %s: alike, %llu clocks over %u, the clock alone %llu: %s\n", row.fields[2].c_str(),
                  row.least, COST_REPEATS, clock, row.fields[3].c_str());
        costed += 1u;
    }
    // each form's cost and spread in the part's clock count, every cost row the .ksc contained let go for them
    unsigned int forms_costed = 0u;
    std::vector<std::string> cost_rows;
    const unsigned long long clock_alone = forms_linked ? clock_least[forms_clock_at] : ULLONG_MAX;
    for (CostForm &form : costed_forms)
    {
        if (!form.verdict.empty() || (form.least == ULLONG_MAX) || (clock_alone == ULLONG_MAX))
        {
            run_print(&table, "  %s: %s\n", form.placed.c_str(), form.verdict.empty() ? "untimed" : form.verdict.c_str());
            continue;
        }
        const unsigned long long least = (form.least > clock_alone) ? (form.least - clock_alone) : 0ull;
        const unsigned long long spread = form.max - form.least;
        char row[192];
        snprintf(row, sizeof(row), "run answers %016llx cost %016llx %016llx %016llx", least, form.low, form.high,
                 spread);
        cost_rows.push_back(row);
        run_print(&table, "  %s: %llu clocks over %u, the clock alone %llu, the spread %llu\n", form.placed.c_str(),
                  form.least, COST_REPEATS, clock_alone, spread);
        forms_costed += 1u;
    }
    if (!cost_rows.empty() && !ksc_answers_write(ksc, "cost", cost_rows))
    {
        printf("orior_klq_identity cost: the forms' costs could not be written to %s\n", ksc);
    }
    if (!run_closed(&table))
    {
        printf("orior_klq_identity cost: %s/cost.txt could not be written\n", folder.c_str());
        run_channel_close();
        return 1;
    }
    // the .kdm's section, every line after its first to its tail with each row costed now in place of the one before,
    // written by the cost pass; its tail as it was (kdm.h)
    KdmFile written = {};
    int written_ok = 1;
    for (const std::string &each : lines)
    {
        written_ok = written_ok && kfile_lines_line_add(&written, each.c_str());
    }
    const unsigned int tail = kdm_tail_at(&written);
    std::vector<const char *> section;
    Who who;
    int who_present = 0;
    for (unsigned int at = 1u; at < tail; at += 1u)
    {
        if (!kfile_lines_who_line(written.line[at], &who, &who_present))
        {
            section.push_back(written.line[at]);
        }
    }
    const std::string part = (!lines.empty() && (lines[0].compare(0u, 4u, "kdm ") == 0)) ? lines[0].substr(4u) : "-";
    written_ok = written_ok &&
                 kdm_section_written(&written, part.c_str(), section.data(), (unsigned int)section.size()) &&
                 kdm_write(&written, kdm);
    kdm_free(&written);
    if (!written_ok)
    {
        printf("orior_klq_identity cost: %s could not be written\n", kdm);
        return 1;
    }
    printf("orior_klq_identity cost: %zu arrangements, %u costed over %llu queries and written to %s; %zu forms, %u "
           "costed in "
           "the part's clock and written to %s",
           rows.size(), costed, queries, kdm, costed_forms.size(), forms_costed, ksc);
    for (const auto &each : left)
    {
        printf(", %u %s", each.second, each.first.c_str());
    }
    printf("\n");
    return 0;
}

// The pairs the arrangements of the .kdm at `kdm` contain and no chain of ours does, those of `excluded_pairs` left:
// each arrangement whose nodes the ruleset writes is written at its operator's link as the cost puts it
// (operator_link), and where a node's result is read by the very next node, the pair of their operations is probed
// there. At max `max` arrangements are a pair's probes, and each is written with every stall the longest and queried
// at every stall of its writer, each query into `queries` and its code into `codes`
void arrangement_probes(const char *engine, const std::map<std::string, std::vector<std::string>> &host,
                        const std::string &kdm, const Ruleset *sass, const char *machine_path, const StallField &stall,
                        unsigned long long longest, const std::set<std::pair<std::string, std::string>> &excluded_pairs,
                        size_t max, std::vector<StallQuery> *queries, std::vector<std::vector<unsigned char>> *codes)
{
    Carriers carriers;
    carriers_read(engine, host, &carriers);
    const unsigned long seed = identity_seed();
    std::vector<std::string> lines;
    std::vector<CostRow> rows;
    std::map<std::string, std::vector<size_t>> operators;
    kdm_rows_read(kdm.c_str(), &lines, &rows, &operators);
    const std::regex first_word("^\\s*(\\S+)");
    std::map<std::pair<std::string, std::string>, size_t> probed;
    for (const auto &each : operators)
    {
        OperatorLink at_link;
        if (!operator_link(sass, carriers, seed, each.first, rows, each.second, &at_link))
        {
            continue;
        }
        const auto &chain = *at_link.link.chain;
        const std::smatch &found = at_link.link.found;
        const std::string before = chain.second.substr(0u, (size_t)found.position(0));
        const std::string past = chain.second.substr((size_t)(found.position(0) + found.length(0)));
        const std::string ones = ruleset_register(sass, "file", at_link.base);
        const std::string ones_written =
            arrangement_written(sass, {{PRECEPT_NOT, PRECEPT_ZERO, PRECEPT_NONE}}, {}, "", ones, at_link.base + 4u);
        // the instructions before the arrangement's first node
        const unsigned int ahead = (unsigned int)(vendor_assembled(machine_path, before + ones_written).size() / 16u);
        for (const size_t at : each.second)
        {
            const CostRow &row = rows[at];
            const std::string located = row.verdict.empty() ? arrangement_written(sass, row.nodes, at_link.operands,
                                                                                   ones, at_link.to, at_link.base + 4u)
                                                             : std::string();
            std::vector<std::string> node_lines;
            for (const std::string &piece : carrier_pieces(located, '\n'))
            {
                if (!trimmed(piece).empty())
                {
                    node_lines.push_back(piece);
                }
            }
            if ((ahead == 0u) || ones_written.empty() || (node_lines.size() != row.nodes.size()))
            {
                continue;
            }
            for (size_t node = 1u; node < row.nodes.size(); node += 1u)
            {
                const unsigned char writer = (unsigned char)(node - 1u);
                std::smatch writer_word;
                std::smatch reader_word;
                if (((row.nodes[node].left != writer) && (row.nodes[node].right != writer)) ||
                    !std::regex_search(node_lines[node - 1u], writer_word, first_word) ||
                    !std::regex_search(node_lines[node], reader_word, first_word))
                {
                    continue;
                }
                const std::pair<std::string, std::string> pair(writer_word[1].str(), reader_word[1].str());
                if ((excluded_pairs.count(pair) != 0u) || (probed[pair] >= max))
                {
                    continue;
                }
                std::vector<unsigned char> written =
                    vendor_assembled(machine_path, before + ones_written + located + past);
                const size_t count = written.size() / 16u;
                if (count == 0u)
                {
                    continue;
                }
                for (size_t index = 0u; index < count; index += 1u)
                {
                    stall_bits_write(&written, index, stall, longest);
                }
                for (unsigned long long step = 0ull; step <= longest; step += 1ull)
                {
                    std::vector<unsigned char> turned = written;
                    stall_bits_write(&turned, ahead + node - 1u, stall, step);
                    codes->push_back(turned);
                    queries->push_back(StallQuery{pair, chain.first, step});
                }
                probed[pair] += 1u;
            }
        }
    }
}
