// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair.cu: the pair: each pair of the bridge put to the part through the carriers, its verdict written beneath it
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "pair.h"

thread_local RunQuestion *t_question = NULL;
thread_local unsigned int *t_places = NULL;

// Every choice a put makes at a link, written to the log as `choice <flags> <link>`. The flags of the trace's table set
// there, ORed into one word (query_trace.tsv). Why the put passed over a link, how the part answered it, or why the put
// ended at it. The flags an open pair's links carry are the questions a further pass takes up
void PairWork::chosen(const char *const *names, unsigned int count, const char *link) const
{
    if (s_query_log == NULL)
    {
        return;
    }
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        word |= query_trace_flag(&s_query_trace, names[at]);
    }
    VerbaLinea *const line = query_log_line(s_query_log);
    mmgr_line_put(line, "choice 0x");
    mmgr_line_hex(line, word, 1u);
    mmgr_line_put(line, " ");
    mmgr_line_put(line, link);
    mmgr_line_ch(line, '\n');
}

// one choice written, by its flag's name
void PairWork::chose(const char *name, const char *link) const
{
    const char *const names[1] = {name};
    chosen(names, 1u, link);
}

// 1 where the piece begins with the terminated `head`
static int piece_starts(CarrierPiece piece, const char *head)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = head, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (piece.length >= length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = piece.text, .other = head, .bytes = length) == 0);
}

// The conference a refused put calls, convened on the pair's thread once the teacher called it (overseer.h). The
// learner passes its statement: what it put, the carrier, what the stand-in does and the stand-in as written. The
// coherence sets hand what the put queried the pair as: the sets the bridge reads it into and what its texts show of
// it. The counterexample passes the first case apart the pair contains, this run's or the bridge's. The OverseerRuling
// goes to the trace and its choice to the log, and its guidance comes back: OVERSEER_HANDS_OFF where no party called a
// conference of `statement`
unsigned int PairWork::conferred(const char *from, const char *to, const char *statement, const char *located,
                                 const char *carrier_name, unsigned int scratch, const RunQuestion *question,
                                 const char *closed_here, const char *link_address)
{
    QryShared *const run = qry_member_run();
    char identity[96];
    question_identity(question, identity, sizeof(identity));
    char joined[OVERSEER_STANDING_MAX_LEN];
    overseer_located_joined(located, joined);
    PlatformText said;
    identity_text_begun(&said);
    mmgr_line_put(&said.line, statement);
    mmgr_line_put(&said.line, "; carrier ");
    mmgr_line_put(&said.line, carrier_name);
    mmgr_line_put(&said.line, form_addresses(located) ? "; stand-in addresses" : "");
    mmgr_line_put(&said.line, form_controls(located) ? "; stand-in controls" : "");
    mmgr_line_put(&said.line, (scratch != 0u) ? "; stand-in scratch" : "");
    mmgr_line_put(&said.line, "; located ");
    mmgr_line_put(&said.line, joined);
    overseer_account(run, identity, "learner", platform_text_bytes(&said));
    PlatformArray beneath;
    identity_pieces_begun(&beneath);
    pair_beneath(bridge, bridge_count, from, to, &beneath);
    PlatformText sets;
    identity_text_begun(&sets);
    PlatformText counterexample;
    identity_text_begun(&counterexample);
    mmgr_line_put(&counterexample.line, closed_here);
    for (unsigned long long at = 0ULL; at < beneath.count; at += 1ULL)
    {
        const CarrierPiece line = identity_piece_at(&beneath, at);
        const int closed = piece_starts(line, "closed ");
        const int open = piece_starts(line, "open ");
        if (closed && (platform_text_size(&counterexample) == 0ULL))
        {
            (void)mmgr_line_put_n(&counterexample.line, line.text, line.length);
        }
        else if (!open && !closed)
        {
            mmgr_line_put(&sets.line, (platform_text_size(&sets) == 0ULL) ? "" : " ");
            (void)mmgr_line_put_n(&sets.line, line.text, line.length);
        }
    }
    platform_array_ended(&beneath);
    const PairCategories categories = pair_categories(sass, from, to);
    platform_text_reset(&said);
    mmgr_line_put(&said.line, "the pair ");
    mmgr_line_put(&said.line, from);
    mmgr_line_put(&said.line, " ");
    mmgr_line_put(&said.line, to);
    mmgr_line_put(&said.line, "; learning ");
    if (platform_text_size(&sets) == 0ULL)
    {
        mmgr_line_put(&said.line, "no set an answer reads it into yet");
    }
    else
    {
        identity_text_put(&said, &sets);
    }
    if (categories.text[0] != '\0')
    {
        mmgr_line_put(&said.line, ", its texts reading it");
        mmgr_line_put(&said.line, categories.text);
    }
    overseer_account(run, identity, "coherence", platform_text_bytes(&said));
    platform_text_reset(&said);
    if (platform_text_size(&counterexample) == 0ULL)
    {
        mmgr_line_put(&said.line, "the pair contains no case apart yet");
    }
    else
    {
        mmgr_line_put(&said.line, "the pair contains its first case apart; counterexample ");
        identity_text_put(&said, &counterexample);
    }
    overseer_account(run, identity, "counterexample", platform_text_bytes(&said));
    platform_text_ended(&counterexample);
    platform_text_ended(&sets);
    platform_text_ended(&said);
    OverseerRuling ruling;
    if (!overseer_convened(run, identity, t_other_way, &ruling))
    {
        return OVERSEER_HANDS_OFF;
    }
    conferences += 1u;
    lessons_taught += ruling.taught ? 1u : 0u;
    chose("conference", link_address);
    if (s_trace_log != NULL)
    {
        VerbaLinea *const line = query_log_line(s_trace_log);
        mmgr_line_put(line, "conference ");
        mmgr_line_put(line, identity);
        mmgr_line_put(line, ": ");
        mmgr_line_put(line, ruling.text);
        mmgr_line_ch(line, '\n');
        query_log_flushed(s_trace_log);
    }
    return ruling.guidance;
}

// the bytes of the hash of what a pair's put reads, its terminator among them: sixteen hexadecimal digits
#define PAIR_READS_BYTES 17u

// every pair of the bridge, in its order. The question and places its put queries with, the hash of what its put reads,
// and the verdict it came to
struct PairPut
{
    IdentityName first;
    IdentityName second;
    PairVerdict verdict;
    int put_whole;
    RunQuestion *question;
    unsigned int places[RUN_CASES_MAX];
    char reads[PAIR_READS_BYTES];
};

// what a pair's thread reads: the work, every pair, and the place of its own
struct PairThread
{
    PairWork *work;
    PairPut *pairs;
    unsigned int at;
};

// what the texts of a pair's two forms show of it before any query, written to the log
static void pair_facts(const PairWork *work, const char *first, const char *second)
{
    const PairCategories categories = pair_categories(work->sass, first, second);
    VerbaLinea *const line = query_log_line(s_query_log);
    mmgr_line_put(line, "facts ");
    mmgr_line_put(line, first);
    mmgr_line_put(line, " ");
    mmgr_line_put(line, second);
    mmgr_line_put(line, categories.text);
    mmgr_line_ch(line, '\n');
}

// One pair put in its thread, run only in its turn. Its facts, its put, and the stand-in the other way round where the
// form's own way answered nothing. Its trace and its log are logs of its own, stored in memory. They pass whole to the
// query's writer once its put ends, after what the thread that carries the queries wrote before it
static void pair_thread_run(void *context)
{
    PairThread *const thread = (PairThread *)context;
    PairWork *const work = thread->work;
    // a pair's place is far below 2^31
    const int at = (int)thread->at;
    identity_turn_awaited(at);
    t_pair = at;
    QueryLog pair_trace = {s_trace_whole.name};
    QueryLog pair_log = {s_log_whole.name};
    if (s_query_run != NULL)
    {
        s_trace_log = &pair_trace;
        s_query_log = &pair_log;
    }
    PairPut *const pair = &thread->pairs[at];
    t_question = pair->question;
    t_places = pair->places;
    stall_cases(t_question, t_places);
    if (s_query_log != NULL)
    {
        pair_facts(work, pair->first.text, pair->second.text);
    }
    // the form's own way first, then the other way round where it answered nothing or a conference turned it. Last,
    // the lower witness where neither answered
    t_turned = 0;
    t_other_way = 0;
    PairVerdict verdict = pair->verdict;
    const int one_way = work->put(pair->first.text, pair->second.text, &verdict);
    int either_way = one_way;
    if (!one_way || t_turned)
    {
        t_other_way = 1;
        PairVerdict other_verdict;
        other_verdict.text[0] = '\0';
        const int other_way = work->put(pair->second.text, pair->first.text, &other_verdict);
        verdict = one_way ? pair_verdicts_joined(verdict.text, other_verdict.text) : other_verdict;
        either_way = one_way || other_way;
    }
    if (!either_way)
    {
        (void)work->witness_lower(pair->first.text, pair->second.text, &verdict);
    }
    // a pair that no put and no witness wrote a verdict of, its forms unread in the ruleset, is open with 0
    if (verdict.text[0] == '\0')
    {
        (void)IDENTITY_SET_TEXT(verdict, "open 0");
    }
    pair->verdict = verdict;
    if (s_query_log != NULL)
    {
        s_trace_log = NULL;
        s_query_log = NULL;
        // the thread that carries the queries waits out this pair's turn: what it wrote before the pair goes first
        query_log_passed(&s_trace_whole);
        query_log_passed(&s_log_whole);
        query_log_passed(&pair_trace);
        query_log_passed(&pair_log);
    }
    if (pair_trace.begun)
    {
        platform_text_ended(&pair_trace.bytes);
    }
    if (pair_log.begun)
    {
        platform_text_ended(&pair_log.bytes);
    }
    pair->put_whole = 1;
    identity_turn_given(-1);
}

// the number `value` in decimal as a terminated text in `text`
static void decimal_text(unsigned long long value, char *text, size_t capacity)
{
    VerbaLinea line = mmgr_line_over(text, capacity);
    mmgr_line_decimal(&line, value, 1u);
    (void)mmgr_line_finish(&line);
}

// 1 where the terminated `text` begins with the terminated `head`
static int text_starts(const char *text, const char *head)
{
    return EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = head,
                      .ci = EMBED_FALSE);
}

// The bridge's lines and its keys, the schema's forms in its order, into `work`, each a piece of the bridge's text,
// which the work keeps read. 1, or 0 where the pool cannot meet a cell
static int bridge_read(const char *klq, PairWork *work)
{
    size_t size = 0u;
    work->bridge_text = run_file_read(klq, &size);
    const CarrierPiece text = {(work->bridge_text != NULL) ? work->bridge_text : "", size};
    unsigned long long lines = 1ULL;
    for (size_t at = 0u; at < size; at += 1u)
    {
        lines += (text.text[at] == '\n') ? 1ULL : 0ULL;
    }
    work->bridge = (CarrierPiece *)platform_cell_taken(lines * sizeof(CarrierPiece), platform_place_g_program_owner,
                                                       PLATFORM_TIER_PERSISTENT);
    work->keys = (CarrierPiece *)platform_cell_taken(lines * sizeof(CarrierPiece), platform_place_g_program_owner,
                                                     PLATFORM_TIER_PERSISTENT);
    if ((work->bridge == NULL) || (work->keys == NULL))
    {
        return 0;
    }
    size_t cut = 0u;
    CarrierPiece line;
    while (carrier_piece_next(text, '\n', &cut, &line))
    {
        if ((line.length != 0u) && (line.text[line.length - 1u] == '\r'))
        {
            line.length -= 1u;
        }
        work->bridge[work->bridge_count] = line;
        work->bridge_count += 1ULL;
        if (piece_starts(line, "key "))
        {
            work->keys[work->key_count] = carrier_after(line, 4u);
            work->key_count += 1ULL;
        }
    }
    return 1;
}

// the work's cells given back
static void pair_work_ended(PairWork *work)
{
    platform_cell_released(work->keys, platform_place_g_program_owner);
    platform_cell_released(work->bridge, platform_place_g_program_owner);
    platform_cell_released(work->bridge_text, platform_place_g_program_owner);
    carriers_ended(&work->carriers);
    host_answers_ended(&work->host);
}

// what every put reads of the engine: each chain's text and the host's answers to its cases, by its question
static unsigned long long engine_read_hashed(const PairWork *work)
{
    unsigned long long engine_read = 0xcbf29ce484222325ULL;
    for (unsigned long long at = 0ULL; at < work->carriers.count; at += 1ULL)
    {
        const CarrierChain *const chain = &work->carriers.chain[at];
        const char *const key = chain->number.text;
        engine_read = record_hash(
            key, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
            engine_read);
        const char *const text = chain->text;
        engine_read = record_hash(
            text, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
            engine_read);
        const HostCases *const cases = host_cases_of(&work->host, key);
        for (unsigned int answer = 0u; (cases != NULL) && (answer < cases->count); answer += 1u)
        {
            const char *const answer_text = cases->answer[answer];
            engine_read = record_hash(
                answer_text,
                EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = answer_text, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
                engine_read);
        }
    }
    return engine_read;
}

// a path the record holds, by its two forms apart by a space
struct RecordedPath
{
    char key[sizeof(((KqrPath *)0)->first) + sizeof(((KqrPath *)0)->second) + 2u];
    KqrPath path;
};

// The paths the query record holds, into a cell of the temporary tier the caller gives back, ordered by key. A key
// given twice keeps its last path. NULL where the pool cannot meet the cell
static RecordedPath *recorded_paths_read(unsigned long long *count)
{
    *count = 0ULL;
    static KqrPath s_recorded;
    unsigned long long paths = 0ULL;
    while (kqr_path_read(&s_query_record, (unsigned int)paths, &s_recorded))
    {
        paths += 1ULL;
    }
    RecordedPath *const recorded = (RecordedPath *)platform_cell_cleared(
        (paths != 0ULL) ? paths : 1ULL, sizeof(RecordedPath), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (recorded == NULL)
    {
        return NULL;
    }
    for (unsigned long long at = 0ULL; at < paths; at += 1ULL)
    {
        // the record's paths are far below 2^32
        (void)kqr_path_read(&s_query_record, (unsigned int)at, &recorded[at].path);
        VerbaLinea key_line = mmgr_line_over(recorded[at].key, sizeof(recorded[at].key));
        mmgr_line_put(&key_line, recorded[at].path.first);
        mmgr_line_ch(&key_line, ' ');
        mmgr_line_put(&key_line, recorded[at].path.second);
        (void)mmgr_line_finish(&key_line);
    }
    (void)identity_sorted(recorded, paths, [](const RecordedPath &one, const RecordedPath &other) {
        return identity_order_text(one.key, other.key) < 0;
    });
    for (unsigned long long at = 0ULL; at < paths; at += 1ULL)
    {
        if ((*count != 0ULL) && identity_same(recorded[*count - 1ULL].key, recorded[at].key))
        {
            recorded[*count - 1ULL] = recorded[at];
            continue;
        }
        recorded[*count] = recorded[at];
        *count += 1ULL;
    }
    return recorded;
}

// the path of the key `key` among the `count` sorted paths, or NULL where none holds it
static const KqrPath *recorded_path_of(const RecordedPath *recorded, unsigned long long count, const char *key)
{
    const unsigned long long at = identity_lower(
        recorded, count, [key](const RecordedPath &one) { return identity_order_text(one.key, key) < 0; });
    return ((at < count) && identity_same(recorded[at].key, key)) ? &recorded[at].path : NULL;
}

// The pairs of the bridge, `pair <form> <form>`, into `pairs`, each with its question and the hash of what its put
// reads. Then the record's verdict where the record settled it. The answer is the count the record settled
static unsigned long long pairs_read(PairWork *work, unsigned long long engine_read, PairPut *pairs,
                                     RunQuestion *questions)
{
    unsigned long long recorded_count = 0ULL;
    RecordedPath *const recorded = recorded_paths_read(&recorded_count);
    unsigned long long settled = 0ULL;
    unsigned long long filled = 0ULL;
    FormNames names;
    for (unsigned long long at = 0ULL; at < work->bridge_count; at += 1ULL)
    {
        const CarrierPiece words = work->bridge[at];
        if (!piece_starts(words, "pair "))
        {
            continue;
        }
        size_t word_at = 0u;
        CarrierPiece type = {words.text, 0u};
        CarrierPiece first = {words.text, 0u};
        CarrierPiece second = {words.text, 0u};
        (void)(identity_word_next(words, &word_at, &type) && identity_word_next(words, &word_at, &first) &&
               identity_word_next(words, &word_at, &second));
        PairPut *const pair = &pairs[filled];
        filled += 1ULL;
        (void)IDENTITY_SET(pair->first, first.text, first.length);
        (void)IDENTITY_SET(pair->second, second.text, second.length);
        (void)IDENTITY_SET_TEXT(pair->verdict, "open 0");
        pair->question = &questions[filled - 1ULL];
        PlatformText first_text;
        PlatformText second_text;
        identity_text_begun(&first_text);
        identity_text_begun(&second_text);
        (void)marked(work->sass, pair->first.text, &names, &first_text);
        (void)marked(work->sass, pair->second.text, &names, &second_text);
        const char *const first_bytes = platform_text_bytes(&first_text);
        const char *const first_marked = (first_bytes != NULL) ? first_bytes : "";
        unsigned long long reads = record_hash(
            first_marked,
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = first_marked, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
            engine_read);
        const char *const second_bytes = platform_text_bytes(&second_text);
        const char *const second_marked = (second_bytes != NULL) ? second_bytes : "";
        reads = record_hash(
            second_marked,
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = second_marked, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
            reads);
        platform_text_ended(&second_text);
        platform_text_ended(&first_text);
        VerbaLinea hashed_line = mmgr_line_over(pair->reads, sizeof(pair->reads));
        mmgr_line_hex(&hashed_line, reads, 16u);
        (void)mmgr_line_finish(&hashed_line);
        char key[CARRIER_TEXT_MAX * 2u];
        VerbaLinea key_line = mmgr_line_over(key, sizeof(key));
        mmgr_line_put(&key_line, pair->first.text);
        mmgr_line_ch(&key_line, ' ');
        mmgr_line_put(&key_line, pair->second.text);
        (void)mmgr_line_finish(&key_line);
        const KqrPath *const path = (recorded != NULL) ? recorded_path_of(recorded, recorded_count, key) : NULL;
        if ((path != NULL) && path->read &&
            (EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = pair->reads, .end = NULL, .base = 16) ==
             path->reads) &&
            text_starts(path->verdict, "closed "))
        {
            (void)IDENTITY_SET_TEXT(pair->verdict, path->verdict);
            pair->put_whole = 1;
            settled += 1ULL;
        }
    }
    platform_cell_released(recorded, platform_place_g_program_owner);
    return settled;
}

// The rounds. Each pair not yet put runs in its turn until it queries or its put ends. The round answers the queries it
// took together. The rounds loop while any pair's put goes on, and end once every pair's put ends
static void pairs_rounds(PairPut *pairs, unsigned long long pair_count, unsigned long long left)
{
    unsigned int round = 0u;
    PlatformArray carried;
    platform_array_begun(&carried, sizeof(CarriedQuery), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    while (left != 0ULL)
    {
        const unsigned long long round_began = platform_clock_ns();
        const unsigned long long passed_before = s_round_passed;
        const unsigned long long recalled_before = s_round_recalled;
        const unsigned long long processes_before = s_round_processes;
        round += 1u;
        {
            char round_text[24];
            decimal_text(round, round_text, sizeof(round_text));
            kqr_mark(&s_query_record, KQR_ROUND_RECORD, round_text);
        }
        (void)platform_array_counted(&carried, 0ULL);
        unsigned int in_flight = 0u;
        for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
        {
            if (pairs[at].put_whole)
            {
                continue;
            }
            // a pair's place is far below 2^31
            identity_turn_given((int)at);
            identity_turn_awaited(-1);
            CarriedQueries *const queued = (CarriedQueries *)platform_array_at(&s_round_queries, at);
            in_flight += (queued->count == 0ULL) ? 0u : 1u;
            for (unsigned long long query = 0ULL; query < queued->count; query += 1ULL)
            {
                (void)platform_array_appended(&carried, platform_array_at(queued, query));
            }
            (void)platform_array_counted(queued, 0ULL);
        }
        queries_answered(&carried);
        left = 0ULL;
        for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
        {
            left += pairs[at].put_whole ? 0ULL : 1ULL;
        }
        const unsigned long long elapsed = platform_clock_ns() - round_began;
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity round ");
        mmgr_line_decimal(output_line, round, 1u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, in_flight, 1u);
        mmgr_line_put(output_line, " pairs queried, ");
        mmgr_line_decimal(output_line, s_round_passed - passed_before, 1u);
        mmgr_line_put(output_line, " queries passed, ");
        mmgr_line_decimal(output_line, s_round_recalled - recalled_before, 1u);
        mmgr_line_put(output_line, " answered from what was recorded, ");
        mmgr_line_decimal(output_line, s_round_processes - processes_before, 1u);
        mmgr_line_put(output_line, " processes, ");
        mmgr_line_decimal(output_line, pair_count - left, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, pair_count, 1u);
        mmgr_line_put(output_line, " pairs put, ");
        mmgr_line_fixed(output_line, (double)elapsed / 1e9, 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " seconds");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        platform_line_flushed();
    }
    platform_array_ended(&carried);
}

// The bridge written with each pair's verdict beneath it, in place of the verdict it recorded, and each verdict counted
// into `open`, `closed` and `unqueried`. 1, or 0 where the write failed
static int bridge_written(const PairWork *work, const char *klq, const PairPut *pairs, unsigned int *open,
                          unsigned int *closed, unsigned int *unqueried)
{
    RunWritten file;
    run_written_begun(&file, klq, 1);
    VerbaLinea *const out = &file.text.line;
    int beneath = 0;
    unsigned long long paired = 0ULL;
    for (unsigned long long at = 0ULL; at < work->bridge_count; at += 1ULL)
    {
        const CarrierPiece entry = work->bridge[at];
        const int verdict_line = piece_starts(entry, "open ") || piece_starts(entry, "closed ");
        const int is_pair = piece_starts(entry, "pair ");
        beneath = is_pair || (beneath && verdict_line);
        if (verdict_line && beneath)
        {
            continue;
        }
        (void)mmgr_line_put_n(out, entry.text, entry.length);
        mmgr_line_ch(out, '\n');
        if (!is_pair)
        {
            continue;
        }
        const PairPut *const pair = &pairs[paired];
        paired += 1ULL;
        const int unqueried_here = identity_same(pair->verdict.text, "open 0");
        *open += (text_starts(pair->verdict.text, "open ") && !unqueried_here) ? 1u : 0u;
        *closed += text_starts(pair->verdict.text, "closed ") ? 1u : 0u;
        *unqueried += unqueried_here ? 1u : 0u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, pair->first.text);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, pair->second.text);
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, pair->verdict.text);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        mmgr_line_put(out, pair->verdict.text);
        mmgr_line_ch(out, '\n');
    }
    const int written = run_closed(&file);
    run_written_ended(&file);
    return written;
}

// a line on the output stream: the mode's name, then `first`, then `second`
static void pair_said(const char *first, const char *second)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity pair: ");
    mmgr_line_put(output_line, first);
    mmgr_line_put(output_line, second);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// The bridge's pairs put to the part, `pair <form> <form>` in Lstar.klq: two forms whose sameness the text leaves open.
// A pair goes in a carrier, a chain of ours the host computes that contains one form's text as the ruleset writes it.
// The put reads the form's arguments off the chain's lines, and writes the other form in its place with the arguments
// of its parameters' names. The put assembles the chain as the engine assembles it, and puts it to the part over every
// case the host computes the carrier on.
//
// The pair is open with its count of cases where the part answers the changed chain as the host answers the carrier
// on every one. It closes at the carrier's address, the question and the position of the link, and the first case it
// answers apart. The put tries each form in the other's place, the first in the second's first. A pair is open with 0
// where neither form can go in for it in any carrier. The second names a parameter the first does not, neither's text
// is in a chain, or the ruleset writes neither form. A pair the part refuses in every carrier is open with 0 as well.
//
// Each verdict goes beneath its pair in the bridge in place of the one it recorded. Where the run keeps the log, the
// values the form reads there on every case follow each put, the carrier cut at the form. `orior_klq_decoder` reads
// its product off them
int identity_pair(const char *engine, const char *answers, const char *ksc, const char *klq, const char *machine_path,
                  const char *const *carrier)
{
    static PairWork s_work;
    PairWork *const work = &s_work;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = work, .bytes = sizeof(*work), .val = 0);
    work->seed = 1UL;
    work->sass = identity_ruleset();
    work->machine = machine_path;
    if (work->sass == NULL)
    {
        pair_said("the ruleset ", IDENTITY_RULESET " names is not read");
        return 1;
    }
    if (!host_answers_read(answers, 1, &work->host) || !carriers_read(engine, &work->host, &work->carriers) ||
        !bridge_read(klq, work))
    {
        pair_said(klq, ": the pool cannot meet the pairs' cells");
        pair_work_ended(work);
        return 1;
    }
    // the seed the order of the links of one magnitude draws from, with each put's two forms, the run's IDENTITY_SEED
    // where the run gives it
    work->seed = identity_seed();
    if (!run_channel_open(carrier, 60000000ULL))
    {
        pair_work_ended(work);
        return 1;
    }
    record_open(ksc, "pair");
    char trace_path[PLATFORM_PATH_BYTES];
    char trace_why[QUERY_TRACE_GLOSS_BYTES];
    VerbaLinea trace_error = mmgr_line_over(trace_why, sizeof(trace_why));
    const int trace_read =
        query_trace_path(trace_path, sizeof(trace_path)) && query_trace_read(trace_path, &s_query_trace, &trace_error);
    (void)mmgr_line_finish(&trace_error);
    if (!trace_read)
    {
        pair_said(trace_why, "");
        run_channel_close();
        pair_work_ended(work);
        return 1;
    }
    {
        char seed_text[24];
        decimal_text(work->seed, seed_text, sizeof(seed_text));
        kqr_mark(&s_query_record, KQR_SEED_RECORD, seed_text);
    }
    // the lessons a run before this one taught, carried into this run (overseer.h)
    overseer_lessons_cleared();
    const unsigned int lessons_carried = overseer_lessons_read(qry_member_run());
    // the queries the query record contained before this cycle, read with R
    const unsigned int recorded_before = s_query_record.query_count;
    static RunQuestion s_stalled;
    static unsigned int s_stalled_places[RUN_CASES_MAX];
    stall_cases(&s_stalled, s_stalled_places);
    // the values of the cases: the three operands of every case, as the log's cases write them
    static unsigned long long s_operands[RUN_CASES_MAX * 3u];
    unsigned long long operand_count = 0ULL;
    for (unsigned int place = 0u; place < s_stalled.cases; place += 1u)
    {
        for (unsigned int operand = 0u; operand < 3u; operand += 1u)
        {
            s_operands[operand_count] = ((unsigned long long)s_stalled.word[place][2u * operand + 1u] << 32u) |
                                        s_stalled.word[place][2u * operand];
            operand_count += 1ULL;
        }
    }
    concept_values(s_operands, operand_count, &work->cased);
    unsigned int open = 0u;
    unsigned int closed = 0u;
    unsigned int unqueried = 0u;
    query_logs_opened();
    // The scheduler: the record's gray entries are the queue. A pair closed on the record's path, whose reads are as
    // they were, settles. Its verdict is the record's, and no put runs on it, since a put runs again only where what it
    // reads changed. Every other pair, open, with no path, or reading what changed, is gray, and the rounds put the
    // gray pairs alone
    unsigned long long pair_count = 0ULL;
    for (unsigned long long at = 0ULL; at < work->bridge_count; at += 1ULL)
    {
        pair_count += piece_starts(work->bridge[at], "pair ") ? 1ULL : 0ULL;
    }
    // the pairs and their questions, each in one cell for the run, so that no pointer to one moves
    PairPut *const pairs = (PairPut *)platform_cell_cleared((pair_count != 0ULL) ? pair_count : 1ULL, sizeof(PairPut),
                                                            platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    RunQuestion *const questions =
        (RunQuestion *)platform_cell_cleared((pair_count != 0ULL) ? pair_count : 1ULL, sizeof(RunQuestion),
                                             platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if ((pairs == NULL) || (questions == NULL))
    {
        platform_cell_released(pairs, platform_place_g_program_owner);
        platform_cell_released(questions, platform_place_g_program_owner);
        run_channel_close();
        pair_work_ended(work);
        return 1;
    }
    const unsigned long long settled = pairs_read(work, engine_read_hashed(work), pairs, questions);
    // Each pair put in a thread of its own, run only in its turn. Every worker and every list of the round is in place
    // before the first thread starts, and none moves while a thread runs
    platform_array_begun(&s_round_queries, sizeof(CarriedQueries), platform_place_g_program_owner,
                         PLATFORM_TIER_PERSISTENT);
    (void)platform_array_counted(&s_round_queries, pair_count);
    for (unsigned long long at = 0ULL; at < s_round_queries.count; at += 1ULL)
    {
        platform_array_begun((CarriedQueries *)platform_array_at(&s_round_queries, at), sizeof(CarriedQuery),
                             platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    }
    PlatformWorker *const workers =
        (PlatformWorker *)platform_cell_cleared((pair_count != 0ULL) ? pair_count : 1ULL, sizeof(PlatformWorker),
                                                platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    PairThread *const threads =
        (PairThread *)platform_cell_cleared((pair_count != 0ULL) ? pair_count : 1ULL, sizeof(PairThread),
                                            platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    for (unsigned long long at = 0ULL; (workers != NULL) && (threads != NULL) && (at < pair_count); at += 1ULL)
    {
        if (pairs[at].put_whole)
        {
            continue;
        }
        threads[at].work = work;
        threads[at].pairs = pairs;
        // a pair's place is far below 2^32
        threads[at].at = (unsigned int)at;
        if (!platform_worker_started(&workers[at], pair_thread_run, &threads[at]))
        {
            // a pair whose thread the system refused stays open with nothing queried
            pairs[at].put_whole = 1;
        }
    }
    unsigned long long left = 0ULL;
    for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
    {
        left += pairs[at].put_whole ? 0ULL : 1ULL;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity pair: ");
        mmgr_line_decimal(output_line, settled, 1u);
        mmgr_line_put(output_line, " pairs settled on the record, ");
        mmgr_line_decimal(output_line, left, 1u);
        mmgr_line_put(output_line, " gray in the queue");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    pairs_rounds(pairs, pair_count, left);
    for (unsigned long long at = 0ULL; (workers != NULL) && (at < pair_count); at += 1ULL)
    {
        platform_worker_joined(&workers[at]);
    }
    platform_cell_released(threads, platform_place_g_program_owner);
    platform_cell_released(workers, platform_place_g_program_owner);
    for (unsigned long long at = 0ULL; at < s_round_queries.count; at += 1ULL)
    {
        platform_array_ended((CarriedQueries *)platform_array_at(&s_round_queries, at));
    }
    platform_array_ended(&s_round_queries);
    run_channel_close();
    const int written = bridge_written(work, klq, pairs, &open, &closed, &unqueried);
    if (!written)
    {
        pair_said(klq, " could not be written");
        platform_cell_released(questions, platform_place_g_program_owner);
        platform_cell_released(pairs, platform_place_g_program_owner);
        pair_work_ended(work);
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity pair: ");
        mmgr_line_decimal(output_line, work->queries, 1u);
        mmgr_line_put(output_line, " queries, ");
        mmgr_line_decimal(output_line, open, 1u);
        mmgr_line_put(output_line, " open, ");
        mmgr_line_decimal(output_line, closed, 1u);
        mmgr_line_put(output_line, " closed, ");
        mmgr_line_decimal(output_line, unqueried, 1u);
        mmgr_line_put(output_line, " not queried, written to ");
        mmgr_line_put(output_line, klq);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // the lessons stored, those carried in and those taught, passed to the run for the next run to carry
    overseer_lessons_passed(qry_member_run());
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity pair: ");
        mmgr_line_decimal(output_line, work->conferences, 1u);
        mmgr_line_put(output_line, " conferences called by refusals, ");
        mmgr_line_decimal(output_line, work->lessons_taught, 1u);
        mmgr_line_put(output_line, " lessons taught, ");
        mmgr_line_decimal(output_line, lessons_carried, 1u);
        mmgr_line_put(output_line, " carried in");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // The path this cycle read off the queries, each pair and its verdict, an open pair the question a further pass
    // takes up. It goes to the query record with every query it contained and every query this cycle put once the run
    // writes R back
    kqr_paths_clear(&s_query_record);
    static KqrPath s_path;
    for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
    {
        const PairPut *const pair = &pairs[at];
        (void)identity_name_set(s_path.first, sizeof(s_path.first), pair->first.text, PLATFORM_TEXT_TO_TERMINATOR);
        (void)identity_name_set(s_path.second, sizeof(s_path.second), pair->second.text, PLATFORM_TEXT_TO_TERMINATOR);
        s_path.read = (pair->reads[0] == '\0') ? 0 : 1;
        s_path.reads = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = pair->reads, .end = NULL, .base = 16);
        (void)identity_name_set(s_path.verdict, sizeof(s_path.verdict), pair->verdict.text,
                                PLATFORM_TEXT_TO_TERMINATOR);
        kqr_path_add(&s_query_record, &s_path);
        pair_row_passed(pair->first.text, pair->second.text, s_path.read, s_path.reads, pair->verdict.text);
    }
    platform_cell_released(questions, platform_place_g_program_owner);
    platform_cell_released(pairs, platform_place_g_program_owner);
    pair_work_ended(work);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity pair: ");
        mmgr_line_decimal(output_line, s_query_record.query_count, 1u);
        mmgr_line_put(output_line, " queries stored in the query record, ");
        mmgr_line_decimal(output_line, s_query_record.query_count - recorded_before, 1u);
        mmgr_line_put(output_line, " of them put this cycle");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
