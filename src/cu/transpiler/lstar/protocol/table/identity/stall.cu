// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stall.cu: the stall walk: the soonest a result is read, queried of the part link by link
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int stall_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// one question the walk puts. The pair it probes, a chain of ours, and its code with every stall the longest. Then the
// place of the writer whose stall the walk turns. The instruction after it reads what it wrote
struct StallProbe
{
    StallPair pair;
    IdentityName number;
    const IdentityCode *code;
    unsigned long long writer;
};

// a line on the output stream: the mode's name, then `first`, then `second`
static void stall_said(const char *first, const char *second)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity stall: ");
    mmgr_line_put(output_line, first);
    mmgr_line_put(output_line, second);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// what every round of the walk goes with: the host's answers, the cases' places, the question, and the queries
// counted
struct StallRound
{
    const HostAnswers *host;
    const unsigned int *places;
    RunQuestion *question;
    unsigned long long *queries;
};

// The `total` questions at `codes` put together, STALL_CARRIED_MAX to a process. None waits on another's answer.
// Each one's mark goes into `marks`, which holds `total`. `=` is alike with the host's answers to the chain the number
// of `numbers` at its place names on every case. `x` is apart on one, and `!` refused
static void marks_put(const StallRound *round, const IdentityCode *codes, unsigned long long total,
                      const IdentityName *numbers, char *marks)
{
    PlatformArray questions;
    platform_array_begun(&questions, sizeof(RunQuestion), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long first = 0ULL; first < total; first += STALL_CARRIED_MAX)
    {
        const unsigned long long count =
            ((total - first) < STALL_CARRIED_MAX) ? (total - first) : (unsigned long long)STALL_CARRIED_MAX;
        cuts_queried(&codes[first], count, NULL, NULL, round->question, round->queries, 255u, &questions);
        for (unsigned long long at = 0ULL; at < count; at += 1ULL)
        {
            const RunQuestion *const asked = (const RunQuestion *)platform_array_at(&questions, at);
            char mark = ((asked == NULL) || (asked->outcome != RUN_ANSWERED)) ? '!' : '=';
            const HostCases *const cases = host_cases_of(round->host, numbers[first + at].text);
            for (unsigned int place = 0u; (mark == '=') && (cases != NULL) && (place < round->question->cases);
                 place += 1u)
            {
                const char *const host = host_case(cases, round->places[place]);
                mark = (bracket_contains(host, asked->answered[place]) == 0) ? 'x' : mark;
            }
            marks[first + at] = mark;
        }
    }
    platform_array_ended(&questions);
}

StallPair stall_pair_key(const char *writer, const char *reader)
{
    StallPair key;
    VerbaLinea key_line = mmgr_line_over(key.text, sizeof(key.text));
    mmgr_line_put(&key_line, writer);
    mmgr_line_ch(&key_line, '\t');
    mmgr_line_put(&key_line, reader);
    (void)mmgr_line_finish(&key_line);
    return key;
}

// the writer's part of a pair's key, onto `writer`, and the reader's onto `reader`
static void stall_pair_split(const StallPair *key, VerbaLinea *writer, VerbaLinea *reader)
{
    const char *const text = key->text;
    const char *const tab =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
    // the tab lies within the key
    const size_t at = (tab != NULL)
                          ? (size_t)(tab - text)
                          : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    (void)mmgr_line_put_n(writer, text, at);
    mmgr_line_put(reader, (tab != NULL) ? (tab + 1) : "");
}

// 1 where the piece begins with the terminated `head`
static int stall_starts(const char *text, const char *head)
{
    return EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = head,
                      .ci = EMBED_FALSE);
}

// the bytes of one row the stall writes to the .ksc, its terminator among them
#define STALL_ROW_BYTES 256u

struct StallRow
{
    char text[STALL_ROW_BYTES];
};

// the places of the cases the stall goes over
static unsigned int s_stall_places[RUN_CASES_MAX];

// Every chain of ours the host computes with every stall the longest, into `codes`, `numbers` and `links`, each a
// PlatformArray of its own on the temporary tier. Its code, its number, and its links, a PlatformArray of Link
static void stall_chains_read(const char *engine, const HostAnswers *host, StallField stall, unsigned long long longest,
                              PlatformArray *codes, PlatformArray *numbers, PlatformArray *links)
{
    for (unsigned long long entry_at = 0ULL; entry_at < host->count; entry_at += 1ULL)
    {
        const IdentityName number = host->chain[entry_at].number;
        IdentityCode code;
        identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
        PlatformArray chain_links;
        platform_array_begun(&chain_links, sizeof(Link), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if (!chain_code_read(engine, number.text, &code, &chain_links))
        {
            platform_array_ended(&code);
            platform_array_ended(&chain_links);
            continue;
        }
        for (unsigned long long index = 0ULL; index < (code.count / 16ULL); index += 1ULL)
        {
            stall_bits_write(&code, index, stall, longest);
        }
        (void)platform_array_appended(numbers, &number);
        (void)platform_array_appended(codes, &code);
        (void)platform_array_appended(links, &chain_links);
    }
}

// The probes of each pair, a writer's operation and its reader's, in the chains the part answers alike. They go onto
// `probes`, a PlatformArray of StallProbe, ordered by pair and within a pair in the order found. The answer is the
// count of chains not alike at the longest stalls
static unsigned int stall_probes_found(const PlatformArray *codes, const PlatformArray *numbers,
                                       const PlatformArray *links, const char *marks, StallField barrier,
                                       unsigned long long none, PlatformArray *probes)
{
    unsigned int unanswered = 0u;
    for (unsigned long long chain = 0ULL; chain < codes->count; chain += 1ULL)
    {
        const IdentityName *const number = (const IdentityName *)platform_array_at(numbers, chain);
        if (marks[chain] != '=')
        {
            unanswered += 1u;
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, number->text);
            mmgr_line_put(output_line, ": not alike at the longest stalls, and no probe");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
            continue;
        }
        const IdentityCode *const code = (const IdentityCode *)platform_array_at(codes, chain);
        const PlatformArray *const chain_links = (const PlatformArray *)platform_array_at(links, chain);
        const Link *const link = (const Link *)chain_links->cell;
        for (unsigned long long index = 0ULL; (index + 1ULL) < chain_links->count; index += 1ULL)
        {
            const Link *const writer_link = &link[index];
            const unsigned long long place = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                        .src = writer_link->position.text, .end = NULL, .base = 16) /
                                             16ULL;
            if (stall_bits(code, place, barrier) != none)
            {
                continue;
            }
            StallRegisters written;
            StallRegisters read;
            stall_written(writer_link, &written);
            stall_read(&link[index + 1ULL], &read);
            int reads = 0;
            for (unsigned int at = 0u; at < written.count; at += 1u)
            {
                reads |= stall_register_held(&read, written.name[at].text) ? 1 : 0;
            }
            if (!reads)
            {
                continue;
            }
            StallOperands writer;
            StallOperands reader;
            stall_operation(writer_link, &writer);
            stall_operation(&link[index + 1ULL], &reader);
            StallProbe probe;
            probe.pair = stall_pair_key(writer.operation.text, reader.operation.text);
            probe.number = *number;
            probe.code = code;
            probe.writer = place;
            (void)platform_array_appended(probes, &probe);
        }
    }
    (void)identity_sorted((StallProbe *)probes->cell, probes->count,
                          [](const StallProbe &one, const StallProbe &other) {
                              return identity_order_text(one.pair.text, other.pair.text) < 0;
                          });
    return unanswered;
}

// the end of the run of one pair's probes that begins at `at`
static unsigned long long stall_pair_end(const StallProbe *probes, unsigned long long count, unsigned long long at)
{
    unsigned long long end = at;
    while ((end < count) && identity_same(probes[end].pair.text, probes[at].pair.text))
    {
        end += 1ULL;
    }
    return end;
}

// 1 where the first `taken` of `numbers` hold `number`
static int stall_number_taken(const IdentityName *numbers, unsigned long long taken, const char *number)
{
    for (unsigned long long at = 0ULL; at < taken; at += 1ULL)
    {
        if (identity_same(numbers[at].text, number))
        {
            return 1;
        }
    }
    return 0;
}

// a stall query and its code put after those of `queries` and `codes`
static void stall_query_added(PlatformArray *queries, PlatformArray *codes, const StallPair *pair,
                              const IdentityName *number, unsigned long long step, const IdentityCode *code)
{
    StallQuery query;
    query.pair = *pair;
    query.number = *number;
    query.step = step;
    (void)platform_array_appended(queries, &query);
    (void)platform_array_appended(codes, code);
}

// The address of a load written last by the other words that write a pair, each probe of an `IMAD.WIDE.U32` that a
// load reads. Every stall of the last link, onto `queries` and `codes`
static void stall_wide_ways(const char *engine, const char *machine_path, const Ruleset *sass, const StallProbe *probe,
                            const char *reader, StallField stall, unsigned long long longest, PlatformArray *queries,
                            PlatformArray *codes)
{
    static TablePatternOnce s_wide_written;
    static TablePatternOnce s_first_word;
    const TablePattern *const wide_written =
        table_once(&s_wide_written,
                   "^\\s*IMAD\\.WIDE\\.U32\\s+(R[0-9]+),\\s*(R[0-9]+),\\s*(R[0-9]+),\\s*"
                   "c\\[0x0\\]\\[(0x[0-9a-fA-F]+)\\+([0-9]+)\\];?\\s*$",
                   1);
    const TablePattern *const first_word = table_once(&s_first_word, "^\\s*(\\S+)", 0);
    char chain_path[PLATFORM_PATH_BYTES];
    VerbaLinea chain_line = mmgr_line_over(chain_path, sizeof(chain_path));
    mmgr_line_put(&chain_line, engine);
    mmgr_line_ch(&chain_line, '/');
    mmgr_line_put(&chain_line, probe->number.text);
    mmgr_line_put(&chain_line, ".sass");
    (void)mmgr_line_finish(&chain_line);
    size_t chain_size = 0u;
    char *const chain_bytes = identity_file_read(chain_path, &chain_size);
    const CarrierPiece chain_text = {(chain_bytes != NULL) ? chain_bytes : "", chain_size};
    // the chain's lines, one cell taken at their count
    unsigned long long line_count = 0ULL;
    size_t cut = 0u;
    CarrierPiece piece;
    while (carrier_piece_next(chain_text, '\n', &cut, &piece))
    {
        line_count += 1ULL;
    }
    CarrierPiece *const lines =
        (CarrierPiece *)platform_cell_taken(((line_count != 0ULL) ? line_count : 1ULL) * sizeof(CarrierPiece),
                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (lines == NULL)
    {
        platform_cell_released(chain_bytes, platform_place_g_program_owner);
        return;
    }
    cut = 0u;
    for (unsigned long long at = 0ULL; carrier_piece_next(chain_text, '\n', &cut, &piece); at += 1ULL)
    {
        lines[at] = piece;
    }
    // the line of the probe's writer, its instructions counted past the labels
    unsigned long long line_at = line_count;
    unsigned long long instructions = 0ULL;
    for (unsigned long long at = 0ULL; at < line_count; at += 1ULL)
    {
        const CarrierPiece trimmed_line = trimmed(lines[at]);
        if ((trimmed_line.length == 0u) || (trimmed_line.text[trimmed_line.length - 1u] == ':'))
        {
            continue;
        }
        if (instructions == probe->writer)
        {
            line_at = at;
            break;
        }
        instructions += 1ULL;
    }
    TableMatch found;
    if ((line_at == line_count) || !table_matched(wide_written, lines[line_at].text, lines[line_at].length, &found))
    {
        platform_cell_released(lines, platform_place_g_program_owner);
        platform_cell_released(chain_bytes, platform_place_g_program_owner);
        return;
    }
    const CarrierPiece writer_piece = lines[line_at];
    IdentityName group[6];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = group, .bytes = sizeof(group), .val = 0);
    for (unsigned int at = 1u; at <= 5u; at += 1u)
    {
        size_t length = 0u;
        const char *const bytes = table_group(&found, at, writer_piece.text, &length);
        (void)IDENTITY_SET(group[at], bytes, length);
    }
    const unsigned int base = (chain_registers(engine, probe->number.text) + 1u) & ~1u;
    const IdentityName entry = ruleset_register_name(sass, "file", base);
    const IdentityName offset = ruleset_register_name(sass, "file", base + 2u);
    IdentityName low;
    VerbaLinea low_line = mmgr_line_over(low.text, sizeof(low.text));
    mmgr_line_put(&low_line, "c[0x0][");
    mmgr_line_put(&low_line, group[4].text);
    mmgr_line_put(&low_line, "+");
    mmgr_line_put(&low_line, group[5].text);
    mmgr_line_put(&low_line, "]");
    (void)mmgr_line_finish(&low_line);
    IdentityName high;
    VerbaLinea high_line = mmgr_line_over(high.text, sizeof(high.text));
    mmgr_line_put(&high_line, "c[0x0][");
    mmgr_line_put(&high_line, group[4].text);
    mmgr_line_put(&high_line, "+");
    mmgr_line_decimal(
        &high_line,
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = group[5].text, .end = NULL, .base = 10) + 4ULL, 1u);
    mmgr_line_put(&high_line, "]");
    (void)mmgr_line_finish(&high_line);
    IdentityName entry_high;
    VerbaLinea entry_high_line = mmgr_line_over(entry_high.text, sizeof(entry_high.text));
    mmgr_line_put(&entry_high_line, entry.text);
    mmgr_line_put(&entry_high_line, ".hi");
    (void)mmgr_line_finish(&entry_high_line);
    PlatformText ways[2];
    identity_text_begun(&ways[0]);
    identity_text_begun(&ways[1]);
    mmgr_line_put(&ways[1].line, "\tIMAD.WIDE.U32 \t");
    mmgr_line_put(&ways[1].line, entry.text);
    mmgr_line_put(&ways[1].line, ", ");
    mmgr_line_put(&ways[1].line, group[2].text);
    mmgr_line_put(&ways[1].line, ", ");
    mmgr_line_put(&ways[1].line, group[3].text);
    mmgr_line_put(&ways[1].line, ", ");
    mmgr_line_put(&ways[1].line, low.text);
    mmgr_line_put(&ways[1].line, ";\n");
    // each word appended to its own probe's text, as the ruleset writes it
    const char *const pack_arguments[3] = {entry.text, low.text, high.text};
    const char *const mul_arguments[3] = {offset.text, group[2].text, group[3].text};
    const char *const add_arguments[3] = {group[1].text, entry.text, offset.text};
    const char *const move_arguments[3] = {group[1].text, entry.text, entry_high.text};
    const struct
    {
        const char *form;
        const char *const *given;
        PlatformText *onto;
    } s_words[4] = {{"wide_pack", pack_arguments, &ways[0]},
                    {"wide_mul_word", mul_arguments, &ways[0]},
                    {"wide_add", add_arguments, &ways[0]},
                    {"wide_pack", move_arguments, &ways[1]}};
    int all_ways = 1;
    for (unsigned int word = 0u; all_ways && (word < 4u); word += 1u)
    {
        all_ways = ruleset_form_put(sass, s_words[word].form, s_words[word].given, 3u, NULL, s_words[word].onto);
    }
    for (unsigned int way_at = 0u; all_ways && (way_at < 2u); way_at += 1u)
    {
        PlatformText before;
        PlatformText whole;
        identity_text_begun(&before);
        identity_text_begun(&whole);
        for (unsigned long long at = 0ULL; at < line_at; at += 1ULL)
        {
            (void)mmgr_line_put_n(&before.line, lines[at].text, lines[at].length);
            mmgr_line_ch(&before.line, '\n');
        }
        identity_text_put(&before, &ways[way_at]);
        identity_text_put(&whole, &before);
        for (unsigned long long at = line_at + 1ULL; at < line_count; at += 1ULL)
        {
            (void)mmgr_line_put_n(&whole.line, lines[at].text, lines[at].length);
            mmgr_line_ch(&whole.line, '\n');
        }
        const char *const before_bytes = platform_text_bytes(&before);
        const char *const whole_bytes = platform_text_bytes(&whole);
        const char *const texts[2] = {(before_bytes != NULL) ? before_bytes : "",
                                      (whole_bytes != NULL) ? whole_bytes : ""};
        PlatformArray ways_code;
        platform_array_begun(&ways_code, sizeof(IdentityCode), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        vendor_assembled(machine_path, texts, 2ULL, &ways_code);
        platform_text_ended(&whole);
        platform_text_ended(&before);
        const IdentityCode *const written_code = (const IdentityCode *)platform_array_at(&ways_code, 0ULL);
        IdentityCode *const code = (IdentityCode *)platform_array_at(&ways_code, 1ULL);
        // a code's instructions are far below 2^32
        const unsigned int written_count = (written_code != NULL) ? (unsigned int)(written_code->count / 16ULL) : 0u;
        const unsigned int count = (code != NULL) ? (unsigned int)(code->count / 16ULL) : 0u;
        // the way's last line, and the operation its first word names
        const unsigned long long way_size = platform_text_size(&ways[way_at]);
        const char *const way_bytes = platform_text_bytes(&ways[way_at]);
        const CarrierPiece way_text = {(way_bytes != NULL) ? way_bytes : "", (size_t)way_size};
        CarrierPiece last_line = {way_text.text, 0u};
        int any_line = 0;
        size_t way_cut = 0u;
        CarrierPiece way_line;
        while (carrier_piece_next(way_text, '\n', &way_cut, &way_line))
        {
            last_line = way_line;
            any_line = 1;
        }
        TableMatch last;
        if ((written_count == 0u) || (count == 0u) || !any_line ||
            !table_search(first_word, last_line.text, last_line.length, &last))
        {
            identity_codes_ended(&ways_code);
            continue;
        }
        size_t operation_length = 0u;
        const char *const operation = table_group(&last, 1u, last_line.text, &operation_length);
        IdentityName operation_text;
        (void)IDENTITY_SET(operation_text, operation, operation_length);
        const StallPair way_pair = stall_pair_key(operation_text.text, reader);
        for (unsigned int index = 0u; index < count; index += 1u)
        {
            stall_bits_write(code, index, stall, longest);
        }
        for (unsigned long long step = 0ULL; step <= longest; step += 1ULL)
        {
            IdentityCode turned;
            identity_code_begun(&turned, PLATFORM_TIER_TEMPORARY);
            (void)identity_code_copied(&turned, code);
            stall_bits_write(&turned, written_count - 1u, stall, step);
            stall_query_added(queries, codes, &way_pair, &probe->number, step, &turned);
        }
        identity_codes_ended(&ways_code);
    }
    platform_text_ended(&ways[0]);
    platform_text_ended(&ways[1]);
    platform_cell_released(lines, platform_place_g_program_owner);
    platform_cell_released(chain_bytes, platform_place_g_program_owner);
}

// What came back at every stall of each pair, onto `table` and into `rows`, a PlatformArray of StallRow. The marks at
// each stall over every query of the pair, and the chains it went over. The pairs go in the order of their keys, and
// `answered` holds each pair's key, one a pair, in that order. The answer is the count of pairs
static unsigned long long stall_marks_written(const StallQuery *queries, unsigned long long query_count,
                                              const char *marks, unsigned long long longest, RunWritten *table,
                                              PlatformArray *rows, PlatformArray *answered)
{
    // the queries' places ordered by pair, a run of one pair in the order put
    unsigned long long *const order = (unsigned long long *)platform_cell_taken(
        ((query_count != 0ULL) ? query_count : 1ULL) * sizeof(unsigned long long), platform_place_g_program_owner,
        PLATFORM_TIER_TEMPORARY);
    IdentityName *const over =
        (IdentityName *)platform_cell_taken(((query_count != 0ULL) ? query_count : 1ULL) * sizeof(IdentityName),
                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    char *const pair_marks =
        (char *)platform_cell_taken(longest + 2ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((order == NULL) || (over == NULL) || (pair_marks == NULL))
    {
        platform_cell_released(pair_marks, platform_place_g_program_owner);
        platform_cell_released(over, platform_place_g_program_owner);
        platform_cell_released(order, platform_place_g_program_owner);
        return 0ULL;
    }
    for (unsigned long long at = 0ULL; at < query_count; at += 1ULL)
    {
        order[at] = at;
    }
    (void)identity_sorted(order, query_count, [queries](unsigned long long one, unsigned long long other) {
        return identity_order_text(queries[one].pair.text, queries[other].pair.text) < 0;
    });
    unsigned long long pairs = 0ULL;
    for (unsigned long long at = 0ULL; at < query_count;)
    {
        const StallPair *const pair = &queries[order[at]].pair;
        unsigned long long end = at;
        for (unsigned long long step = 0ULL; step <= longest; step += 1ULL)
        {
            pair_marks[step] = '=';
        }
        pair_marks[longest + 1ULL] = '\0';
        unsigned long long over_count = 0ULL;
        while ((end < query_count) && identity_same(queries[order[end]].pair.text, pair->text))
        {
            const StallQuery *const each = &queries[order[end]];
            over[over_count] = each->number;
            over_count += 1ULL;
            const char mark = marks[order[end]];
            char *const pair_mark = &pair_marks[each->step];
            *pair_mark = (*pair_mark == '!') ? '!' : (mark != '=') ? mark : *pair_mark;
            end += 1ULL;
        }
        over_count = identity_names_settled(over, over_count);
        (void)platform_array_appended(answered, pair);
        pairs += 1ULL;
        // the soonest is the least stall from which every longer one answers alike
        unsigned long long soonest = longest + 1ULL;
        while ((soonest > 0ULL) && (pair_marks[soonest - 1ULL] == '='))
        {
            soonest -= 1ULL;
        }
        IdentityName writer;
        IdentityName reader;
        VerbaLinea writer_line = mmgr_line_over(writer.text, sizeof(writer.text));
        VerbaLinea reader_line = mmgr_line_over(reader.text, sizeof(reader.text));
        stall_pair_split(pair, &writer_line, &reader_line);
        (void)mmgr_line_finish(&writer_line);
        (void)mmgr_line_finish(&reader_line);
        VerbaLinea *const line = &table->text.line;
        mmgr_line_put(line, writer.text);
        mmgr_line_put(line, " ");
        mmgr_line_put(line, reader.text);
        mmgr_line_put(line, " soonest ");
        mmgr_line_decimal(line, soonest, 1u);
        mmgr_line_put(line, ", stalls 0 to ");
        mmgr_line_decimal(line, longest, 1u);
        mmgr_line_put(line, ": ");
        mmgr_line_put(line, pair_marks);
        mmgr_line_put(line, ", over");
        for (unsigned long long each = 0ULL; each < over_count; each += 1ULL)
        {
            mmgr_line_ch(line, ' ');
            mmgr_line_put(line, over[each].text);
        }
        mmgr_line_ch(line, '\n');
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, writer.text);
            mmgr_line_put(output_line, " then ");
            mmgr_line_put(output_line, reader.text);
            mmgr_line_put(output_line, ": soonest ");
            mmgr_line_decimal(output_line, soonest, 1u);
            mmgr_line_put(output_line, ", stalls 0 to ");
            mmgr_line_decimal(output_line, longest, 1u);
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, pair_marks);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (soonest <= longest)
        {
            StallRow row;
            VerbaLinea row_line = mmgr_line_over(row.text, sizeof(row.text));
            mmgr_line_put(&row_line, "run answers ");
            mmgr_line_hex(&row_line, soonest, 8u);
            mmgr_line_put(&row_line, " stall ");
            mmgr_line_put(&row_line, writer.text);
            mmgr_line_put(&row_line, " ");
            mmgr_line_put(&row_line, reader.text);
            (void)mmgr_line_finish(&row_line);
            (void)platform_array_appended(rows, &row);
        }
        at = end;
    }
    platform_cell_released(pair_marks, platform_place_g_program_owner);
    platform_cell_released(over, platform_place_g_program_owner);
    platform_cell_released(order, platform_place_g_program_owner);
    return pairs;
}

// Every pair the .ksc answers that this round did not put, among the `count` keys at `answered` in their order, keeps
// its answer: its row goes onto `rows`
static void stall_rows_kept(const char *ksc, const StallPair *answered, unsigned long long count, PlatformArray *rows)
{
    KscFile entry = {};
    static KscRow s_row;
    ksc_read(&entry, ksc);
    for (unsigned int at = 0u; at < entry.lines; at += 1u)
    {
        if (!ksc_row_read(entry.line[at], "run", &s_row))
        {
            continue;
        }
        const CarrierPiece question = carrier_piece_of(s_row.question);
        size_t word_at = 0u;
        CarrierPiece type = {question.text, 0u};
        CarrierPiece writer = {question.text, 0u};
        CarrierPiece reader = {question.text, 0u};
        (void)(identity_word_next(question, &word_at, &type) && identity_word_next(question, &word_at, &writer) &&
               identity_word_next(question, &word_at, &reader));
        if (!identity_piece_is(type, "stall") || (reader.length == 0u))
        {
            continue;
        }
        StallPair key;
        VerbaLinea key_line = mmgr_line_over(key.text, sizeof(key.text));
        (void)mmgr_line_put_n(&key_line, writer.text, writer.length);
        mmgr_line_ch(&key_line, '\t');
        (void)mmgr_line_put_n(&key_line, reader.text, reader.length);
        (void)mmgr_line_finish(&key_line);
        const unsigned long long found = identity_lower(
            answered, count, [&key](const StallPair &one) { return identity_order_text(one.text, key.text) < 0; });
        if ((found < count) && identity_same(answered[found].text, key.text))
        {
            continue;
        }
        StallRow row;
        (void)IDENTITY_SET_TEXT(row, entry.line[at]);
        (void)platform_array_appended(rows, &row);
    }
    ksc_free(&entry);
}

// The soonest a reader reads each operation's result, queried of the part. The walk puts every chain of ours the host
// also computes with every stall the longest, all in one round. A chain the part answers alike with the host there is
// a probe wherever the very next instruction reads a register one writes. Every stall of each pair's writer below the
// longest then goes on each of its probes, at max three, all in one round. None waits on another's answer.
//
// A stall is truthy where every probe of its pair answers alike. The soonest is the least stall from which every
// longer one is truthy. No shorter stall answers as no worse than a longer one. At 0 a pair answers alike where the
// part refuses it at 1. A reader reads an instruction that sets a write barrier behind the barrier and not behind its
// stall, and the walk does not query it.
//
// `IMAD.WIDE.U32` writes a load's address in every chain of ours. The walk writes again each probe of a pair whose
// reader loads through the pair the multiply writes. Each other word the ruleset writes a pair with then writes the
// address last. These are wide_add, the base read from the constant bank by wide_pack and the offset by wide_mul_word,
// added. Then wide_pack, the multiply's pair moved into the address's.
//
// The walk queries each at every stall of the word's last link, the one the load reads after. Every other stall is the
// longest, in the same round. The walk probes each pair that no chain of ours contains and an arrangement of the .kdm
// beside the machine file does. It probes the pair in that arrangement, written at its operator's link as the cost puts
// it (arrangement_probes). A round goes STALL_CARRIED_MAX questions to a process.
//
// Each pair's soonest goes to the .ksc as the part's answer on the run channel, `run answers <stall> stall <writer>
// <reader>`, in place of those it contained. The run's blob stall.txt contains every pair with its probes and what came
// back at every stall: `=` alike on every probe, `x` apart on one, `!` refused on one
int identity_stall(const char *engine, const char *answers, const char *ksc, const char *folder,
                   const char *machine_path, const char *const *carrier)
{
    StallFields fields;
    stall_fields(ksc, &fields);
    const StallField *const stall_field = field_of(&fields, "stall");
    const StallField *const barrier_field = field_of(&fields, "write_barrier");
    if ((stall_field == NULL) || (barrier_field == NULL))
    {
        stall_said(ksc, " gives no stall field or no write barrier field");
        stall_fields_ended(&fields);
        return 1;
    }
    const StallField stall = *stall_field;
    const StallField barrier = *barrier_field;
    stall_fields_ended(&fields);
    const unsigned long long longest = (1ULL << stall.bits) - 1ULL;
    const unsigned long long none = (1ULL << barrier.bits) - 1ULL;
    HostAnswers host;
    if (!host_answers_read(answers, 1, &host))
    {
        stall_said(answers, ": the pool cannot meet the host's answers");
        return 1;
    }
    if (!run_channel_open(carrier, 60000000ULL))
    {
        host_answers_ended(&host);
        return 1;
    }
    record_open(ksc, "stall");
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    stall_cases(&s_question, s_stall_places);
    unsigned long long queries = 0ULL;
    const StallRound round = {&host, s_stall_places, &s_question, &queries};
    // every chain of ours with every stall the longest, put in one round
    PlatformArray chain_codes;
    PlatformArray chain_numbers;
    PlatformArray chain_links;
    platform_array_begun(&chain_codes, sizeof(IdentityCode), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    platform_array_begun(&chain_numbers, sizeof(IdentityName), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    platform_array_begun(&chain_links, sizeof(PlatformArray), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    stall_chains_read(engine, &host, stall, longest, &chain_codes, &chain_numbers, &chain_links);
    char *const chain_marks = (char *)platform_cell_taken((chain_codes.count != 0ULL) ? chain_codes.count : 1ULL,
                                                          platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (chain_marks == NULL)
    {
        stall_said(ksc, ": the pool cannot meet the chains' marks");
        run_channel_close();
        host_answers_ended(&host);
        return 1;
    }
    marks_put(&round, (const IdentityCode *)chain_codes.cell, chain_codes.count,
              (const IdentityName *)chain_numbers.cell, chain_marks);
    // the probes of each pair, a writer's operation and its reader's, in the chains the part answers alike
    PlatformArray probes;
    platform_array_begun(&probes, sizeof(StallProbe), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    const unsigned int unanswered =
        stall_probes_found(&chain_codes, &chain_numbers, &chain_links, chain_marks, barrier, none, &probes);
    platform_cell_released(chain_marks, platform_place_g_program_owner);
    const StallProbe *const probe = (const StallProbe *)probes.cell;
    unsigned long long pair_count = 0ULL;
    for (unsigned long long at = 0ULL; at < probes.count; at = stall_pair_end(probe, probes.count, at))
    {
        pair_count += 1ULL;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity stall: ");
        mmgr_line_decimal(output_line, chain_codes.count, 1u);
        mmgr_line_put(output_line, " chains, ");
        mmgr_line_decimal(output_line, unanswered, 1u);
        mmgr_line_put(output_line, " not alike at the longest stalls, ");
        mmgr_line_decimal(output_line, pair_count, 1u);
        mmgr_line_put(output_line, " pairs, carried by ");
        mmgr_line_put(output_line, run_channel_carrier());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // the probes the walk goes over for a pair at max, each from a chain of its own
    const unsigned long long max = 3ULL;
    RunWritten table;
    run_written_begun(&table, identity_path_joined(folder, "/stall.txt").text, 0);
    // every stall below the longest of each pair's writer, on each of its probes
    PlatformArray stall_queries;
    PlatformArray stall_codes;
    platform_array_begun(&stall_queries, sizeof(StallQuery), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    platform_array_begun(&stall_codes, sizeof(IdentityCode), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    // the load's address written last by the other words that write a pair, every stall of the last link, in the round
    const Ruleset *const sass = identity_ruleset();
    for (int wide = 0; wide < 2; wide += 1)
    {
        for (unsigned long long at = 0ULL; at < probes.count;)
        {
            const unsigned long long end = stall_pair_end(probe, probes.count, at);
            IdentityName writer;
            IdentityName reader;
            VerbaLinea writer_line = mmgr_line_over(writer.text, sizeof(writer.text));
            VerbaLinea reader_line = mmgr_line_over(reader.text, sizeof(reader.text));
            stall_pair_split(&probe[at].pair, &writer_line, &reader_line);
            (void)mmgr_line_finish(&writer_line);
            (void)mmgr_line_finish(&reader_line);
            const int loads = identity_same(writer.text, "IMAD.WIDE.U32") && stall_starts(reader.text, "LDG");
            IdentityName numbers[3];
            unsigned long long taken = 0ULL;
            for (unsigned long long probe_at = at; (probe_at < end) && ((wide == 0) || ((sass != NULL) && loads));
                 probe_at += 1ULL)
            {
                const StallProbe *const one = &probe[probe_at];
                if ((taken >= max) || stall_number_taken(numbers, taken, one->number.text))
                {
                    continue;
                }
                numbers[taken] = one->number;
                taken += 1ULL;
                if (wide != 0)
                {
                    stall_wide_ways(engine, machine_path, sass, one, reader.text, stall, longest, &stall_queries,
                                    &stall_codes);
                    continue;
                }
                for (unsigned long long step = 0ULL; step < longest; step += 1ULL)
                {
                    IdentityCode turned;
                    identity_code_begun(&turned, PLATFORM_TIER_TEMPORARY);
                    (void)identity_code_copied(&turned, one->code);
                    stall_bits_write(&turned, one->writer, stall, step);
                    stall_query_added(&stall_queries, &stall_codes, &one->pair, &one->number, step, &turned);
                }
            }
            at = end;
        }
    }
    // the pairs the .kdm's arrangements contain and no chain of ours does, the rotates among them, in the same round
    if (sass != NULL)
    {
        PlatformArray gated;
        platform_array_begun(&gated, sizeof(StallPair), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        for (unsigned long long at = 0ULL; at < probes.count; at = stall_pair_end(probe, probes.count, at))
        {
            (void)platform_array_appended(&gated, &probe[at].pair);
        }
        char kdm[PLATFORM_PATH_BYTES];
        VerbaLinea kdm_line = mmgr_line_over(kdm, sizeof(kdm));
        const size_t machine_length =
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = machine_path, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        const int typed = (machine_length >= 4u) &&
                          EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = &machine_path[machine_length - 4u],
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".khw", .ci = EMBED_FALSE);
        (void)mmgr_line_put_n(&kdm_line, machine_path, typed ? (machine_length - 4u) : machine_length);
        mmgr_line_put(&kdm_line, ".kdm");
        (void)mmgr_line_finish(&kdm_line);
        arrangement_probes(engine, &host, kdm, sass, machine_path, stall, longest, (const StallPair *)gated.cell,
                           gated.count, max, &stall_queries, &stall_codes);
        platform_array_ended(&gated);
    }
    platform_array_ended(&probes);
    // the one round, and what came back at every stall of each pair
    const StallQuery *const query = (const StallQuery *)stall_queries.cell;
    IdentityName *const stall_numbers = (IdentityName *)platform_cell_taken(
        ((stall_queries.count != 0ULL) ? stall_queries.count : 1ULL) * sizeof(IdentityName),
        platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    char *const marks = (char *)platform_cell_taken((stall_queries.count != 0ULL) ? stall_queries.count : 1ULL,
                                                    platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((stall_numbers == NULL) || (marks == NULL))
    {
        platform_cell_released(marks, platform_place_g_program_owner);
        platform_cell_released(stall_numbers, platform_place_g_program_owner);
        stall_said(ksc, ": the pool cannot meet the round's marks");
        run_channel_close();
        host_answers_ended(&host);
        return 1;
    }
    for (unsigned long long at = 0ULL; at < stall_queries.count; at += 1ULL)
    {
        stall_numbers[at] = query[at].number;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity stall: ");
        mmgr_line_decimal(output_line, stall_codes.count, 1u);
        mmgr_line_put(output_line, " questions, every stall of every pair, put in one round");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    marks_put(&round, (const IdentityCode *)stall_codes.cell, stall_codes.count, stall_numbers, marks);
    identity_codes_ended(&stall_codes);
    platform_cell_released(stall_numbers, platform_place_g_program_owner);
    PlatformArray rows;
    PlatformArray answered;
    platform_array_begun(&rows, sizeof(StallRow), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    platform_array_begun(&answered, sizeof(StallPair), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    (void)stall_marks_written(query, stall_queries.count, marks, longest, &table, &rows, &answered);
    platform_cell_released(marks, platform_place_g_program_owner);
    platform_array_ended(&stall_queries);
    stall_rows_kept(ksc, (const StallPair *)answered.cell, answered.count, &rows);
    platform_array_ended(&answered);
    // the rows in the order of their text from ` stall ` on
    (void)identity_sorted((StallRow *)rows.cell, rows.count, [](const StallRow &left, const StallRow &right) {
        const char *const left_from =
            EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = left.text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = " stall ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        const char *const right_from =
            EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = right.text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = " stall ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        const char *const left_key = (left_from != NULL) ? left_from : left.text;
        const char *const right_key = (right_from != NULL) ? right_from : right.text;
        return stall_text_order(left_key, right_key, PLATFORM_TEXT_TO_TERMINATOR) < 0;
    });
    // the chains' codes and links given back once nothing reads them
    for (unsigned long long at = 0ULL; at < chain_links.count; at += 1ULL)
    {
        platform_array_ended((PlatformArray *)platform_array_at(&chain_links, at));
    }
    platform_array_ended(&chain_links);
    identity_codes_ended(&chain_codes);
    platform_array_ended(&chain_numbers);
    host_answers_ended(&host);
    const int closed = run_closed(&table);
    run_written_ended(&table);
    if (!closed)
    {
        stall_said(folder, "/stall.txt could not be written");
        run_channel_close();
        platform_array_ended(&rows);
        return 1;
    }
    run_channel_close();
    const char **const texts =
        (const char **)platform_cell_taken(((rows.count != 0ULL) ? rows.count : 1ULL) * sizeof(const char *),
                                           platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long at = 0ULL; (texts != NULL) && (at < rows.count); at += 1ULL)
    {
        texts[at] = ((const StallRow *)platform_array_at(&rows, at))->text;
    }
    // the rows of a record are far below 2^32
    const int ksc_written = (texts != NULL) && ksc_answers_write(ksc, "stall", texts, (unsigned int)rows.count);
    platform_cell_released(texts, platform_place_g_program_owner);
    platform_array_ended(&rows);
    if (!ksc_written)
    {
        stall_said(ksc, " could not be written");
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity stall: ");
    mmgr_line_decimal(output_line, pair_count, 1u);
    mmgr_line_put(output_line, " pairs over ");
    mmgr_line_decimal(output_line, queries, 1u);
    mmgr_line_put(output_line, " queries, written to ");
    mmgr_line_put(output_line, ksc);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}
