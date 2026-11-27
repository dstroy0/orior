// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// text.cu: the queue of the stick's manifest read against the record, and the text identity each side of a slice is put
// through
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"

// the next line of a file read in binary from `*at` into `line`, a carriage return at its end taken off
static int queue_line_next(CarrierPiece text, size_t *at, CarrierPiece *line)
{
    if (!carrier_piece_next(text, '\n', at, line))
    {
        return 0;
    }
    if ((line->length != 0u) && (line->text[line->length - 1u] == '\r'))
    {
        line->length -= 1u;
    }
    return 1;
}

// 1 where the piece begins with the terminated `head`
static int piece_starts(CarrierPiece piece, const char *head)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = head, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (piece.length >= length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = piece.text, .other = head, .bytes = length) == 0);
}

// one category's entries counted by what came of them
struct QueueCounts
{
    unsigned int questions;
    unsigned int written;
    unsigned int gated;
    unsigned int alike;
    unsigned int one;
    unsigned int zero;
    unsigned int gray;
};

// a row of P12's table: the category's name, then its counts
static void queue_row_said(const char *category, const QueueCounts *row)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "| ");
    mmgr_line_put(output_line, category);
    mmgr_line_put(output_line, " | ");
    mmgr_line_decimal(output_line, row->questions, 1u);
    mmgr_line_put(output_line, " | ");
    mmgr_line_decimal(output_line, row->written, 1u);
    mmgr_line_put(output_line, " | ");
    mmgr_line_decimal(output_line, row->gated, 1u);
    mmgr_line_put(output_line, " | ");
    mmgr_line_decimal(output_line, row->alike, 1u);
    mmgr_line_put(output_line, " | ");
    mmgr_line_decimal(output_line, row->one, 1u);
    mmgr_line_put(output_line, " | ");
    mmgr_line_decimal(output_line, row->zero, 1u);
    mmgr_line_put(output_line, " | ");
    mmgr_line_decimal(output_line, row->gray, 1u);
    mmgr_line_put(output_line, " |");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// a key and its value, each a piece of a text a mode holds read
struct PieceEntry
{
    CarrierPiece key;
    CarrierPiece value;
};

// The `count` entries at `entries` ordered by key, a key given twice at its last value. The answer is the count kept
static unsigned long long piece_entries_settled(PieceEntry *entries, unsigned long long count)
{
    (void)identity_sorted(entries, count, [](const PieceEntry &one, const PieceEntry &other) {
        return identity_piece_order(one.key, other.key) < 0;
    });
    unsigned long long kept = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if ((kept != 0ULL) && identity_piece_same(entries[kept - 1ULL].key, entries[at].key))
        {
            entries[kept - 1ULL] = entries[at];
            continue;
        }
        entries[kept] = entries[at];
        kept += 1ULL;
    }
    return kept;
}

// the value of `key` among the `count` sorted entries at `entries`, or NULL where none holds it
static const PieceEntry *piece_entry_of(const PieceEntry *entries, unsigned long long count, CarrierPiece key)
{
    const unsigned long long at =
        identity_lower(entries, count, [key](const PieceEntry &one) { return identity_piece_order(one.key, key) < 0; });
    return ((at < count) && identity_piece_same(entries[at].key, key)) ? &entries[at] : NULL;
}

// the lines of `text` counted, for the entries a cell holds
static unsigned long long text_line_count(CarrierPiece text)
{
    unsigned long long lines = 1ULL;
    for (size_t at = 0u; at < text.length; at += 1u)
    {
        lines += (text.text[at] == '\n') ? 1ULL : 0ULL;
    }
    return lines;
}

// The engine's note of each question it writes no chain for, from its engine.tsv, into `notes`, which holds the text's
// lines, each a piece of `text`. The answer is the count, ordered by number
static unsigned long long queue_notes_read(CarrierPiece text, PieceEntry *notes)
{
    unsigned long long count = 0ULL;
    size_t cut = 0u;
    CarrierPiece line;
    while (queue_line_next(text, &cut, &line))
    {
        CarrierPiece cells[5];
        unsigned int cell_count = 0u;
        size_t cell_cut = 0u;
        CarrierPiece cell;
        while (carrier_piece_next(line, '\t', &cell_cut, &cell))
        {
            if (cell_count < 5u)
            {
                cells[cell_count] = cell;
            }
            cell_count += 1u;
        }
        if ((cell_count >= 2u) && !identity_piece_is(cells[1], "answered") && !identity_piece_is(cells[0], "number"))
        {
            const PieceEntry note = {cells[0], (cell_count >= 5u) ? cells[4] : cells[1]};
            notes[count] = note;
            count += 1ULL;
        }
    }
    return piece_entries_settled(notes, count);
}

// the manifest's row `line` cut at its first two tabs: its number, its category and the rest. 0 where it holds two not
static int manifest_cells(CarrierPiece line, CarrierPiece *number, CarrierPiece *category, CarrierPiece *rest)
{
    const char *const first_tab =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line.text, .cap = line.length, .byte = '\t');
    // each tab lies within the line
    const size_t first_at = (first_tab != NULL) ? (size_t)(first_tab - line.text) : line.length;
    const char *const second_tab = (first_tab != NULL) ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = first_tab + 1,
                                                                    .cap = line.length - first_at - 1u, .byte = '\t')
                                                       : NULL;
    if ((first_tab == NULL) || (second_tab == NULL))
    {
        return 0;
    }
    const size_t second_at = (size_t)(second_tab - line.text);
    number->text = line.text;
    number->length = first_at;
    category->text = first_tab + 1;
    category->length = second_at - first_at - 1u;
    *rest = carrier_after(line, second_at + 1u);
    return 1;
}

// one category of the queue, in the order the manifest first names it, and its counts
struct QueueCategory
{
    CarrierPiece category;
    QueueCounts counts;
};

// the places of the cases the queue and the text identities go over
static unsigned int s_text_places[RUN_CASES_MAX];

// what the queue holds while it reads: the engine's notes text, the manifest's text, the notes and the categories
struct QueueHeld
{
    char *notes_text;
    char *manifest_text;
    PieceEntry *notes;
    QueueCategory *categories;
};

static void queue_held_ended(QueueHeld *held)
{
    platform_cell_released(held->categories, platform_place_g_program_owner);
    platform_cell_released(held->notes, platform_place_g_program_owner);
    platform_cell_released(held->manifest_text, platform_place_g_program_owner);
    platform_cell_released(held->notes_text, platform_place_g_program_owner);
}

// The queue over the stick's questions (P13, the scheduler). Every question of the manifest is an entry of its
// category, and R gives each what it reads. 1 where the part answers it as the host does on every case the host
// computes it on. 0 where it answers one case otherwise or refuses it, and gray where nothing settled it. A gray entry
// carries what keeps it gray. The engine's note where the engine writes no chain for it, that the host computes no
// answer to it, or that the gate blocks it.
//
// A gray entry with a chain and a host answer goes through the run channel, every case at once. Its container declares
// every register its file uses, and its answer goes to R. The queue never puts again a question R settled. Each query
// settles one entry and no other: the questions of the stick share no case. The order the queue puts the entries in
// settles no more of them than another order would. The run's blob queue.txt contains every entry with its category,
// what it reads and what keeps it gray.
//
// The standard output counts each category's entries as a row of P12's table
int identity_queue(const char *engine, const char *answers, const char *ksc, const char *folder, const char *manifest,
                   const char *const *carrier)
{
    HostAnswers host;
    if (!host_answers_read(answers, 1, &host))
    {
        identity_path_said(answers, ": the pool cannot meet the host's answers");
        return 1;
    }
    QueueHeld held;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &held, .bytes = sizeof(held), .val = 0);
    // the engine's answer to each question and its note where it writes no chain
    const IdentityPath notes_path = identity_path_joined(engine, "/engine.tsv");
    size_t notes_size = 0u;
    held.notes_text = identity_file_read(notes_path.text, &notes_size);
    const CarrierPiece notes_text = {(held.notes_text != NULL) ? held.notes_text : "", notes_size};
    size_t manifest_size = 0u;
    held.manifest_text = identity_file_read(manifest, &manifest_size);
    const CarrierPiece entries = {(held.manifest_text != NULL) ? held.manifest_text : "", manifest_size};
    const unsigned long long entry_lines = text_line_count(entries);
    held.notes = (PieceEntry *)platform_cell_taken(text_line_count(notes_text) * sizeof(PieceEntry),
                                                   platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    held.categories = (QueueCategory *)platform_cell_cleared(entry_lines, sizeof(QueueCategory),
                                                             platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((held.notes == NULL) || (held.categories == NULL))
    {
        identity_path_said(manifest, ": the pool cannot meet the queue's cells");
        queue_held_ended(&held);
        host_answers_ended(&host);
        return 1;
    }
    const unsigned long long note_count = queue_notes_read(notes_text, held.notes);
    unsigned long long entry_count = 0ULL;
    {
        size_t cut = 0u;
        CarrierPiece line;
        while (queue_line_next(entries, &cut, &line))
        {
            CarrierPiece number;
            CarrierPiece category;
            CarrierPiece rest;
            entry_count +=
                (manifest_cells(line, &number, &category, &rest) && !piece_starts(line, "number")) ? 1ULL : 0ULL;
        }
    }
    if (entry_count == 0ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity queue: ");
        mmgr_line_put(output_line, manifest);
        mmgr_line_put(output_line, " lists no question");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        queue_held_ended(&held);
        host_answers_ended(&host);
        return 1;
    }
    if (!run_channel_open(carrier, 60000000ULL))
    {
        queue_held_ended(&held);
        host_answers_ended(&host);
        return 1;
    }
    record_open(ksc, "queue");
    RunWritten table;
    run_written_begun(&table, identity_path_joined(folder, "/queue.txt").text, 0);
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    stall_cases(&s_question, s_text_places);
    unsigned long long queries = 0ULL;
    // each category's entries counted by what came of them, in the order the manifest first names the category
    unsigned long long category_count = 0ULL;
    IdentityCode code;
    identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
    PlatformArray links;
    platform_array_begun(&links, sizeof(Link), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    size_t cut = 0u;
    CarrierPiece line;
    while (queue_line_next(entries, &cut, &line))
    {
        CarrierPiece number_piece;
        CarrierPiece category;
        CarrierPiece rest;
        if (!manifest_cells(line, &number_piece, &category, &rest) || piece_starts(line, "number"))
        {
            continue;
        }
        unsigned long long found = category_count;
        for (unsigned long long at = 0ULL; (found == category_count) && (at < category_count); at += 1ULL)
        {
            found = identity_piece_same(held.categories[at].category, category) ? at : found;
        }
        if (found == category_count)
        {
            held.categories[category_count].category = category;
            category_count += 1ULL;
        }
        QueueCounts *const row = &held.categories[found].counts;
        row->questions += 1u;
        IdentityName number;
        (void)IDENTITY_SET(number, number_piece.text, number_piece.length);
        const char *reads = "gray";
        char reason[256];
        VerbaLinea reason_line = mmgr_line_over(reason, sizeof(reason));
        const PieceEntry *const note = piece_entry_of(held.notes, note_count, number_piece);
        const HostCases *const cases = host_cases_of(&host, number.text);
        if ((note != NULL) || !chain_code_read(engine, number.text, &code, &links))
        {
            if (note != NULL)
            {
                (void)mmgr_line_put_n(&reason_line, note->value.text, note->value.length);
            }
            else
            {
                mmgr_line_put(&reason_line, "the engine writes no chain");
            }
        }
        else if (cases == NULL)
        {
            row->written += 1u;
            mmgr_line_put(&reason_line, "the host computes no answer");
        }
        else
        {
            row->written += 1u;
            row->gated += 1u;
            const int alike = stall_alike(&code, cases, s_text_places, &s_question, &queries);
            reads = alike ? "1" : (s_question.outcome == RUN_GATED) ? "gray" : "0";
            if (alike)
            {
                mmgr_line_put(&reason_line, "alike");
            }
            else if (s_question.outcome == RUN_ANSWERED)
            {
                mmgr_line_put(&reason_line, "apart");
            }
            else
            {
                mmgr_line_put(&reason_line, (s_question.outcome == RUN_GATED) ? "the gate blocks it: " : "refused: ");
                mmgr_line_put(&reason_line, s_question.refused);
            }
            row->alike += alike ? 1u : 0u;
        }
        (void)mmgr_line_finish(&reason_line);
        row->one += (reads[0] == '1') ? 1u : 0u;
        row->zero += (reads[0] == '0') ? 1u : 0u;
        row->gray += (reads[0] == 'g') ? 1u : 0u;
        VerbaLinea *const out = &table.text.line;
        mmgr_line_put(out, number.text);
        mmgr_line_put(out, "\t");
        (void)mmgr_line_put_n(out, category.text, category.length);
        mmgr_line_put(out, "\t");
        mmgr_line_put(out, reads);
        mmgr_line_put(out, "\t");
        mmgr_line_put(out, reason);
        mmgr_line_ch(out, '\n');
    }
    platform_array_ended(&code);
    platform_array_ended(&links);
    host_answers_ended(&host);
    const int closed = run_closed(&table);
    run_written_ended(&table);
    if (!closed)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity queue: ");
        mmgr_line_put(output_line, folder);
        mmgr_line_put(output_line, "/queue.txt could not be written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        run_channel_close();
        queue_held_ended(&held);
        return 1;
    }
    run_channel_close();
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "| category | questions | written | gated | alike | 1 | 0 | gray |");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    QueueCounts all = {0u, 0u, 0u, 0u, 0u, 0u, 0u};
    for (unsigned long long at = 0ULL; at < category_count; at += 1ULL)
    {
        const QueueCategory *const each = &held.categories[at];
        IdentityName category;
        (void)IDENTITY_SET(category, each->category.text, each->category.length);
        queue_row_said(category.text, &each->counts);
        all.questions += each->counts.questions;
        all.written += each->counts.written;
        all.gated += each->counts.gated;
        all.alike += each->counts.alike;
        all.one += each->counts.one;
        all.zero += each->counts.zero;
        all.gray += each->counts.gray;
    }
    queue_held_ended(&held);
    queue_row_said("all", &all);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity queue: ");
    mmgr_line_decimal(output_line, queries, 1u);
    mmgr_line_put(output_line, " queries, ");
    mmgr_line_decimal(output_line, all.one, 1u);
    mmgr_line_put(output_line, " read 1, ");
    mmgr_line_decimal(output_line, all.zero, 1u);
    mmgr_line_put(output_line, " read 0, ");
    mmgr_line_decimal(output_line, all.gray, 1u);
    mmgr_line_put(output_line, " gray");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (all.zero == 0u) ? 0 : 1;
}

void case_text(unsigned int at, VerbaLinea *line)
{
    const unsigned int third = at / (IDENTITY_VALUES * IDENTITY_VALUES);
    const unsigned int left = (at / IDENTITY_VALUES) % IDENTITY_VALUES;
    const unsigned int right = at % IDENTITY_VALUES;
    mmgr_line_hex(line, s_values[left], 1u);
    mmgr_line_put(line, ",");
    mmgr_line_hex(line, s_values[right], 1u);
    mmgr_line_put(line, ",");
    mmgr_line_hex(line, third, 1u);
}

// what a side put to the part reads from. The engine, the host's answers, the question, the queries counted, and the
// code and links read into
struct TextSides
{
    const char *engine;
    const HostAnswers *host;
    RunQuestion *question;
    unsigned long long *queries;
    IdentityCode *code;
    PlatformArray *links;
};

// a side put to the part. 1 where it answers as the host does on every case, 0 where it answers one apart, its place
// in s_apart_at, and -1 where it went unqueried
static int side_alike(const TextSides *sides, const char *number)
{
    const HostCases *const cases = host_cases_of(sides->host, number);
    if ((cases == NULL) || !chain_code_read(sides->engine, number, sides->code, sides->links))
    {
        return -1;
    }
    const int alike = stall_alike(sides->code, cases, s_text_places, sides->question, sides->queries);
    return alike ? 1 : (sides->question->outcome == RUN_ANSWERED) ? 0 : -1;
}

// The verdict of the identity between the sides `one` and `other`, numbered `first` and `second`, onto `verdict_text`.
// The host's answers read against each other, then each side put to the part
static void text_verdict(const TextSides *sides, const HostCases *first_cases, const HostCases *second_cases,
                         const char *first, const char *second, CarrierPiece one, CarrierPiece other,
                         unsigned int cases, VerbaLinea *verdict_text)
{
    unsigned int alike = 0u;
    char apart[CARRIER_TEXT_MAX];
    VerbaLinea apart_line = mmgr_line_over(apart, sizeof(apart));
    int is_apart = 0;
    for (unsigned int at = 0u; at < cases; at += 1u)
    {
        const unsigned int place = s_text_places[at];
        const char *const left = host_case(first_cases, place);
        const char *const right = host_case(second_cases, place);
        if (identity_same(left, "-") || identity_same(right, "-"))
        {
            continue;
        }
        if (!identity_same(left, right))
        {
            mmgr_line_put(&apart_line, "closed ");
            case_text(place, &apart_line);
            mmgr_line_put(&apart_line, "->");
            mmgr_line_put(&apart_line, left);
            mmgr_line_put(&apart_line, ",");
            mmgr_line_put(&apart_line, right);
            is_apart = 1;
            break;
        }
        alike += 1u;
    }
    (void)mmgr_line_finish(&apart_line);
    const int first_alike = !is_apart ? side_alike(sides, first) : -1;
    const unsigned int first_apart = s_apart_at;
    const int second_alike = (!is_apart && (first_alike >= 0)) ? side_alike(sides, second) : -1;
    const unsigned int second_apart = s_apart_at;
    if (is_apart)
    {
        verdict_text->at = 0ULL;
        mmgr_line_put(verdict_text, apart);
    }
    else if ((first_alike == 0) || (second_alike == 0))
    {
        const unsigned int place = (first_alike == 0) ? first_apart : second_apart;
        const CarrierPiece side = (first_alike == 0) ? one : other;
        verdict_text->at = 0ULL;
        mmgr_line_put(verdict_text, "closed ");
        case_text(place, verdict_text);
        mmgr_line_put(verdict_text, "->");
        mmgr_line_put(verdict_text, host_case(first_cases, place));
        mmgr_line_put(verdict_text, ", the part apart on ");
        (void)mmgr_line_put_n(verdict_text, side.text, side.length);
    }
    else if ((first_alike == 1) && (second_alike == 1))
    {
        verdict_text->at = 0ULL;
        mmgr_line_put(verdict_text, "open ");
        mmgr_line_decimal(verdict_text, alike, 1u);
    }
}

// The bridge's identities between texts checked on the part (P13, the identities). Each side of
// `text_identity <text> = <text>` is a question of the sides' stick, found by its text in the sides' manifest. Its
// chain is the engine's and its answers the host's. An identity closes at the first case the host answers its two
// sides apart on, the texts then two meanings. It closes at the first case the part answers a side apart from the host
// on, the sides then not checked against each other there.
//
// Where the host answers the two sides alike on every case, and the part answers each as the host does, the identity
// is open. Its count of cases is every case the host computes both sides on. The check queries no side with no chain,
// no host answer, or that the part refuses or the gate blocks. Its identity is open with 0.
// Each identity's verdict goes beneath it in the bridge in place of the one it recorded
int identity_text(const char *engine, const char *answers, const char *ksc, const char *klq, const char *manifest,
                  const char *const *carrier)
{
    HostAnswers host;
    if (!host_answers_read(answers, 1, &host))
    {
        identity_path_said(answers, ": the pool cannot meet the host's answers");
        return 1;
    }
    // each side's text by its number, pieces of the manifest's text, a side given twice at its last number
    size_t manifest_size = 0u;
    char *const listed = identity_file_read(manifest, &manifest_size);
    const CarrierPiece listed_text = {(listed != NULL) ? listed : "", manifest_size};
    PieceEntry *const numbered = (PieceEntry *)platform_cell_taken(
        text_line_count(listed_text) * sizeof(PieceEntry), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned long long numbered_count = 0ULL;
    {
        size_t cut = 0u;
        CarrierPiece line;
        while ((numbered != NULL) && queue_line_next(listed_text, &cut, &line))
        {
            CarrierPiece number;
            CarrierPiece category;
            CarrierPiece side;
            if (manifest_cells(line, &number, &category, &side))
            {
                const PieceEntry entry = {side, number};
                numbered[numbered_count] = entry;
                numbered_count += 1ULL;
            }
        }
        numbered_count = piece_entries_settled(numbered, numbered_count);
    }
    size_t bridge_size = 0u;
    char *const bridge_text = run_file_read(klq, &bridge_size);
    const CarrierPiece bridge = {(bridge_text != NULL) ? bridge_text : "", bridge_size};
    if ((numbered == NULL) || !run_channel_open(carrier, 60000000ULL))
    {
        platform_cell_released(bridge_text, platform_place_g_program_owner);
        platform_cell_released(numbered, platform_place_g_program_owner);
        platform_cell_released(listed, platform_place_g_program_owner);
        host_answers_ended(&host);
        return 1;
    }
    record_open(ksc, "text_identity");
    // a chain of ours contains no probe place, and the cross reads it whole (measuring_stick_query.c)
    static RunQuestion s_question;
    s_question.place = RUN_PLACE_NONE;
    stall_cases(&s_question, s_text_places);
    unsigned long long queries = 0ULL;
    IdentityCode code;
    identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
    PlatformArray links;
    platform_array_begun(&links, sizeof(Link), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    const TextSides sides_read = {engine, &host, &s_question, &queries, &code, &links};
    RunWritten file;
    run_written_begun(&file, klq, 1);
    VerbaLinea *const out = &file.text.line;
    unsigned int open = 0u;
    unsigned int closed = 0u;
    unsigned int unqueried = 0u;
    // every verdict line the bridge recorded beneath an identity between texts, however many, gives way to the one
    // written
    int beneath = 0;
    size_t cut = 0u;
    CarrierPiece entry;
    while (queue_line_next(bridge, &cut, &entry))
    {
        const int verdict_line = piece_starts(entry, "open ") || piece_starts(entry, "closed ");
        const int identity = piece_starts(entry, "text_identity ");
        beneath = identity || (beneath && verdict_line);
        if (verdict_line && beneath)
        {
            continue;
        }
        (void)mmgr_line_put_n(out, entry.text, entry.length);
        mmgr_line_ch(out, '\n');
        if (!identity)
        {
            continue;
        }
        const CarrierPiece sides = carrier_after(entry, 14u);
        const char *const equals = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = sides.text, .cap = sides.length,
                                              .other = " = ", .other_cap = sides.length, .ci = EMBED_FALSE);
        // the equals lies within the sides
        const size_t equals_at = (equals != NULL) ? (size_t)(equals - sides.text) : sides.length;
        const CarrierPiece one = {sides.text, equals_at};
        const CarrierPiece other =
            (equals != NULL) ? carrier_after(sides, equals_at + 3u) : carrier_after(sides, sides.length);
        const PieceEntry *const first_entry = piece_entry_of(numbered, numbered_count, one);
        const PieceEntry *const second_entry = piece_entry_of(numbered, numbered_count, other);
        IdentityName first;
        IdentityName second;
        first.text[0] = '\0';
        second.text[0] = '\0';
        if (first_entry != NULL)
        {
            (void)IDENTITY_SET(first, first_entry->value.text, first_entry->value.length);
        }
        if (second_entry != NULL)
        {
            (void)IDENTITY_SET(second, second_entry->value.text, second_entry->value.length);
        }
        char verdict[CARRIER_TEXT_MAX];
        VerbaLinea verdict_text = mmgr_line_over(verdict, sizeof(verdict));
        mmgr_line_put(&verdict_text, "open 0");
        const HostCases *const first_cases = host_cases_of(&host, first.text);
        const HostCases *const second_cases = host_cases_of(&host, second.text);
        if ((first.text[0] != '\0') && (second.text[0] != '\0') && (first_cases != NULL) && (second_cases != NULL))
        {
            text_verdict(&sides_read, first_cases, second_cases, first.text, second.text, one, other, s_question.cases,
                         &verdict_text);
        }
        (void)mmgr_line_finish(&verdict_text);
        const int unqueried_here = EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = verdict,
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "open 0", .ci = EMBED_FALSE);
        open += (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = verdict, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "open ", .ci = EMBED_FALSE) &&
                 !unqueried_here)
                    ? 1u
                    : 0u;
        closed += EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = verdict, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "closed ", .ci = EMBED_FALSE)
                      ? 1u
                      : 0u;
        unqueried += unqueried_here ? 1u : 0u;
        if (s_trace_log != NULL)
        {
            VerbaLinea *const line = query_log_line(s_trace_log);
            mmgr_line_put(line, "text_identity ");
            (void)mmgr_line_put_n(line, sides.text, sides.length);
            mmgr_line_put(line, " [");
            mmgr_line_put(line, first.text);
            mmgr_line_put(line, " ");
            mmgr_line_put(line, second.text);
            mmgr_line_put(line, "]: ");
            mmgr_line_put(line, verdict);
            mmgr_line_ch(line, '\n');
            query_log_flushed(s_trace_log);
        }
        mmgr_line_put(out, verdict);
        mmgr_line_ch(out, '\n');
    }
    platform_cell_released(bridge_text, platform_place_g_program_owner);
    platform_cell_released(numbered, platform_place_g_program_owner);
    platform_cell_released(listed, platform_place_g_program_owner);
    platform_array_ended(&code);
    platform_array_ended(&links);
    host_answers_ended(&host);
    run_channel_close();
    const int written = run_closed(&file);
    run_written_ended(&file);
    if (!written)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity text_identity: ");
        mmgr_line_put(output_line, klq);
        mmgr_line_put(output_line, " could not be written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_identity text_identity: ");
    mmgr_line_decimal(output_line, queries, 1u);
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
    return 0;
}
