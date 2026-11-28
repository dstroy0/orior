// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.cu: the fields of the part's .ksc the walks read, the stall and the registers, the cases a question is put
// over, and the query record opened and closed
#include "../../../../../../../includes/parsers/who.h"
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "identity_internal.h"
#include "memoria_operor/memoria_operor.h"

unsigned long long stall_bits(const IdentityCode *code, unsigned long long index, StallField field)
{
    const unsigned char *const bytes = (const unsigned char *)code->cell;
    unsigned long long value = 0ULL;
    for (unsigned int bit = 0u; bit < field.bits; bit += 1u)
    {
        const unsigned int at = field.first + bit;
        value |= (unsigned long long)((bytes[(index * 16ULL) + (at / 8u)] >> (at % 8u)) & 1u) << bit;
    }
    return value;
}

void stall_bits_write(IdentityCode *code, unsigned long long index, StallField field, unsigned long long value)
{
    unsigned char *const bytes = (unsigned char *)code->cell;
    for (unsigned int bit = 0u; bit < field.bits; bit += 1u)
    {
        const unsigned int at = field.first + bit;
        unsigned char *const byte = &bytes[(index * 16ULL) + (at / 8u)];
        const unsigned char mask = (unsigned char)(1u << (at % 8u));
        *byte = (unsigned char)(((value >> bit) & 1ULL) ? (*byte | mask) : (*byte & ~mask));
    }
}

// the `length` bytes of `name` put into `registers` in the order of their names, where it does not hold them yet
static void stall_register_added(StallRegisters *registers, const char *name, size_t length)
{
    LinkRegister added;
    if (!IDENTITY_SET(added, name, length) || stall_register_held(registers, added.text) ||
        (registers->count >= STALL_REGISTERS_MAX))
    {
        return;
    }
    unsigned int at = registers->count;
    while ((at > 0u) && (identity_order_text(registers->name[at - 1u].text, added.text) > 0))
    {
        registers->name[at] = registers->name[at - 1u];
        at -= 1u;
    }
    registers->name[at] = added;
    registers->count += 1u;
}

// the registers an operand names put into `named`, a pair read whole where the link writes it .64, as it writes a wide
// destination
static void stall_registers(CarrierPiece operand, int wide, StallRegisters *named)
{
    static TablePatternOnce s_named;
    const int pair = wide || (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = operand.text, .cap = operand.length,
                                         .other = ".64", .other_cap = operand.length, .ci = EMBED_FALSE) != NULL);
    TableWalk walk;
    table_walk_begun(&walk, table_once(&s_named, "\\b(R|P|UR)([0-9]+)\\b", 0), operand.text, operand.length);
    while (table_walk_next(&walk))
    {
        size_t length = 0u;
        const char *const whole = table_walk_group(&walk, 0u, &length);
        stall_register_added(named, whole, length);
        if (pair)
        {
            size_t type_length = 0u;
            const char *const type = table_walk_group(&walk, 1u, &type_length);
            size_t digits_length = 0u;
            const char *const digits = table_walk_group(&walk, 2u, &digits_length);
            char number[32] = {0};
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = number, .src = digits,
                       .bytes = (digits_length < 31u) ? digits_length : 31u);
            char next[48];
            VerbaLinea next_line = mmgr_line_over(next, sizeof(next));
            (void)mmgr_line_put_n(&next_line, type, type_length);
            mmgr_line_decimal(
                &next_line,
                EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = number, .end = NULL, .base = 10) + 1ULL, 1u);
            const size_t next_length = mmgr_line_finish(&next_line);
            stall_register_added(named, next, next_length);
        }
    }
    table_walk_ended(&walk);
}

void stall_operation(const Link *link, StallOperands *operands)
{
    CarrierPiece text = carrier_piece_of(link->exact.text);
    if ((text.length != 0u) && (text.text[0] == '@'))
    {
        text = trimmed(carrier_after(text, carrier_space_at(text)));
    }
    const size_t space = carrier_space_at(text);
    operands->count = 0u;
    if (space != text.length)
    {
        size_t cut = 0u;
        CarrierPiece piece;
        const CarrierPiece rest = carrier_after(text, space);
        while (carrier_piece_next(rest, ',', &cut, &piece) && (operands->count < STALL_OPERANDS_MAX))
        {
            const CarrierPiece each = trimmed(piece);
            (void)IDENTITY_SET(operands->operand[operands->count], each.text, each.length);
            operands->count += 1u;
        }
    }
    (void)IDENTITY_SET(operands->operation, text.text, space);
}

void stall_written(const Link *link, StallRegisters *written)
{
    written->count = 0u;
    StallOperands operands;
    stall_operation(link, &operands);
    if (operands.count != 0u)
    {
        const char *const first = operands.operand[0].text;
        if (!identity_has(first, "[") && !identity_same(first, "PT") && !identity_same(first, "RZ"))
        {
            const char *const operation = operands.operation.text;
            const int wide = identity_has(operation, ".WIDE") || identity_has(operation, ".64");
            stall_registers(carrier_piece_of(first), wide, written);
        }
    }
}

void stall_read(const Link *link, StallRegisters *read)
{
    read->count = 0u;
    StallOperands operands;
    stall_operation(link, &operands);
    const int wide = identity_has(operands.operation.text, ".64");
    if (link->exact.text[0] == '@')
    {
        const CarrierPiece exact = carrier_piece_of(link->exact.text);
        const CarrierPiece guard = {exact.text, carrier_space_at(exact)};
        stall_registers(guard, 0, read);
    }
    StallRegisters written;
    stall_written(link, &written);
    const unsigned int first = (written.count == 0u) ? 0u : 1u;
    for (unsigned int at = first; at < operands.count; at += 1u)
    {
        const CarrierPiece each = carrier_piece_of(operands.operand[at].text);
        const int data = wide && (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = each.text, .cap = each.length,
                                             .byte = '[') == NULL);
        stall_registers(each, data, read);
    }
}

void stall_fields(const char *ksc, StallFields *fields)
{
    fields->field = NULL;
    fields->count = 0ULL;
    KscFile file = {};
    ksc_read(&file, ksc);
    fields->field = (file.lines != 0u) ? (StallFieldNamed *)platform_cell_cleared(file.lines, sizeof(StallFieldNamed),
                                                                                  platform_place_g_program_owner,
                                                                                  PLATFORM_TIER_PERSISTENT)
                                       : NULL;
    for (unsigned int at = 0u; (fields->field != NULL) && (at < file.lines); at += 1u)
    {
        StallFieldNamed *const named = &fields->field[fields->count];
        named->field.first = 0u;
        named->field.bits = 0u;
        if (ksc_field_read(file.line[at], named->name.text, sizeof(named->name.text), &named->field.first,
                           &named->field.bits))
        {
            fields->count += 1ULL;
        }
    }
    ksc_free(&file);
    // the fields by name, a name the .ksc gives twice at its last field
    if ((fields->field != NULL) &&
        !identity_sorted(fields->field, fields->count, [](const StallFieldNamed &one, const StallFieldNamed &other) {
            return identity_order_text(one.name.text, other.name.text) < 0;
        }))
    {
        stall_fields_ended(fields);
        return;
    }
    unsigned long long kept = 0ULL;
    for (unsigned long long at = 0ULL; at < fields->count; at += 1ULL)
    {
        if ((kept != 0ULL) && identity_same(fields->field[kept - 1ULL].name.text, fields->field[at].name.text))
        {
            fields->field[kept - 1ULL] = fields->field[at];
            continue;
        }
        fields->field[kept] = fields->field[at];
        kept += 1ULL;
    }
    fields->count = kept;
}

unsigned int whole_cases(RunQuestion *question, const unsigned long long *qualifiers, unsigned long long count,
                         unsigned int run)
{
    unsigned long long *const values =
        (unsigned long long *)platform_cell_taken((IDENTITY_VALUES + count) * sizeof(unsigned long long),
                                                  platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (values == NULL)
    {
        question->cases = 0u;
        return 0u;
    }
    unsigned long long each = 0ULL;
    for (unsigned int at = 0u; at < IDENTITY_VALUES; at += 1u)
    {
        values[each] = s_values[at];
        each += 1ULL;
    }
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        int held = 0;
        for (unsigned long long value = 0ULL; value < each; value += 1ULL)
        {
            held |= (values[value] == qualifiers[at]) ? 1 : 0;
        }
        if (held == 0)
        {
            values[each] = qualifiers[at];
            each += 1ULL;
        }
    }
    const unsigned long long cases = each * each * each;
    const unsigned long long first = (unsigned long long)run * RUN_CASES_MAX;
    const unsigned long long left_over = (cases > first) ? (cases - first) : 0ULL;
    // the cases of one run are at max RUN_CASES_MAX, which an unsigned int counts
    question->cases = (unsigned int)((left_over < RUN_CASES_MAX) ? left_over : RUN_CASES_MAX);
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const unsigned long long at = first + place;
        const unsigned long long left = values[(at / (each * each)) % each];
        const unsigned long long right = values[(at / each) % each];
        const unsigned long long third = values[at % each];
        // each value goes as its low word and its high word
        const unsigned int words[RUN_IN_WORDS] = {(unsigned int)(left & 0xffffffffULL),
                                                  (unsigned int)(left >> 32u),
                                                  (unsigned int)(right & 0xffffffffULL),
                                                  (unsigned int)(right >> 32u),
                                                  (unsigned int)(third & 0xffffffffULL),
                                                  (unsigned int)(third >> 32u),
                                                  0u,
                                                  0u};
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = question->word[place], .src = words, .bytes = sizeof(words));
    }
    platform_cell_released(values, platform_place_g_program_owner);
    // the runs of the cases are far below 2^32
    return (unsigned int)((cases + RUN_CASES_MAX - 1u) / RUN_CASES_MAX);
}

void stall_cases(RunQuestion *question, unsigned int *places)
{
    const unsigned int count = (IDENTITY_CASES < RUN_CASES_MAX) ? IDENTITY_CASES : RUN_CASES_MAX;
    question->cases = count;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        // a place below the count of the host's cases, which an unsigned int counts
        const unsigned int at = (unsigned int)(((unsigned long long)place * IDENTITY_CASES) / count);
        const unsigned long long left = s_values[(at / IDENTITY_VALUES) % IDENTITY_VALUES];
        const unsigned long long right = s_values[at % IDENTITY_VALUES];
        // each value goes as its low word and its high word
        const unsigned int words[RUN_IN_WORDS] = {(unsigned int)(left & 0xffffffffULL),
                                                  (unsigned int)(left >> 32u),
                                                  (unsigned int)(right & 0xffffffffULL),
                                                  (unsigned int)(right >> 32u),
                                                  (unsigned int)(at / (IDENTITY_VALUES * IDENTITY_VALUES)),
                                                  0u,
                                                  0u,
                                                  0u};
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = question->word[place], .src = words, .bytes = sizeof(words));
        places[place] = at;
    }
}

unsigned long long record_hash(const void *bytes, size_t size, unsigned long long hash)
{
    const unsigned char *const at = (const unsigned char *)bytes;
    for (size_t place = 0u; place < size; place += 1u)
    {
        hash = (hash ^ at[place]) * 0x100000001b3ULL;
    }
    return hash;
}

QueryRecords s_identity_record;

char s_record_ksc[PLATFORM_PATH_BYTES];

// The part's query record, its .kqr beside its .ksc and of the same stem, stored in memory for the run. It holds every
// untimed query a cycle put and what came back, by the question's identity. It holds every timed query as a sample of
// its own, and the paths the last cycle read off them.
//
// Whatever queries an untimed query again, in this cycle or a later one, gets its answer from it, and the part answers
// once. The run queries again a query that timed out, until an answer resolves it. The record censors a query the gate
// censored or the part answered illegal, timed or not, every time after. No process carries it, and the part's answer
// stays as it gave it.
//
// The run reads it where it reads R, and writes it back where it writes R. Its path is empty where it does not read,
// so that nothing writes over a record no mode read. What a cycle puts goes in the record's order by its cycle and
// mode, its seed and its rounds (kqr_mark). Each mark goes before the first new query or sample that follows it, and
// nowhere where none follows. A cycle the record answers whole adds no line to it
KqrRecord s_query_record;

// the flags of the trace's choices, read from their table (includes/file_defs/klq/query_trace.tsv) when the pair
// mode opens
QueryTrace s_query_trace;

static char s_query_record_path[PLATFORM_PATH_BYTES];

// the bytes of one row of the record as the .ksc writes it, its terminator among them
#define RECORD_ROW_BYTES (RECORD_KEY_BYTES + RECORD_ANSWER_BYTES + RECORD_REFUSAL_BYTES + 32u)

struct RecordRow
{
    char text[RECORD_ROW_BYTES];
};

// The record's queries ordered by key, each once, as the .ksc writes them. 1, or 0 where the pool cannot meet the
// sort
static int records_settled(QueryRecords *record)
{
    QueryRecorded *const records = (QueryRecorded *)record->records.cell;
    if (!identity_sorted(records, record->records.count, [](const QueryRecorded &one, const QueryRecorded &other) {
            return identity_order_text(one.key, other.key) < 0;
        }))
    {
        return 0;
    }
    // a key given twice keeps its last
    unsigned long long kept = 0ULL;
    for (unsigned long long at = 0ULL; at < record->records.count; at += 1ULL)
    {
        if ((kept != 0ULL) && identity_same(records[kept - 1ULL].key, records[at].key))
        {
            records[kept - 1ULL] = records[at];
            continue;
        }
        records[kept] = records[at];
        kept += 1ULL;
    }
    (void)platform_array_counted(&record->records, kept);
    record->sorted = kept;
    return 1;
}

void record_close(void)
{
    if (s_record_ksc[0] == '\0')
    {
        return;
    }
    const int settled = records_settled(&s_identity_record);
    const unsigned long long count = s_identity_record.records.count;
    RecordRow *const rows =
        settled ? (RecordRow *)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(RecordRow),
                                                   platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
                : NULL;
    const char **const texts =
        (rows != NULL) ? (const char **)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(const char *),
                                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
                       : NULL;
    const QueryRecorded *const records = (const QueryRecorded *)s_identity_record.records.cell;
    for (unsigned long long at = 0ULL; (texts != NULL) && (at < count); at += 1ULL)
    {
        const QueryRecorded *const recorded = &records[at];
        VerbaLinea row = mmgr_line_over(rows[at].text, sizeof(rows[at].text));
        mmgr_line_put(&row, "run ");
        mmgr_line_put(&row, recorded->answer);
        mmgr_line_ch(&row, ' ');
        mmgr_line_hex(&row, recorded->word, 8u);
        mmgr_line_put(&row, " query ");
        mmgr_line_put(&row, recorded->key);
        if (recorded->refusal[0] != '\0')
        {
            mmgr_line_ch(&row, ' ');
            mmgr_line_put(&row, recorded->refusal);
        }
        (void)mmgr_line_finish(&row);
        texts[at] = rows[at].text;
    }
    // the rows of a record are far below 2^32
    if ((texts == NULL) || !ksc_answers_write(s_record_ksc, "query", texts, (unsigned int)count))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the record could not be written to ");
        mmgr_line_put(output_line, s_record_ksc);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(texts, platform_place_g_program_owner);
    platform_cell_released(rows, platform_place_g_program_owner);
    s_record_ksc[0] = '\0';
    if ((s_query_record_path[0] != '\0') && !kqr_write(&s_query_record, s_query_record_path))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, s_query_record_path);
        mmgr_line_put(output_line, " could not be written through the run's writer");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    s_query_record_path[0] = '\0';
}

// the bytes that the extraction of a stream takes as spaces
#define RECORD_SPACES " \t\n\r\v\f"

// a place past the spaces of `text` from `at`, as the extraction of a stream skips them
static size_t record_spaces_past(const char *text, size_t at)
{
    return at + EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = RECORD_SPACES);
}

// The run rows of the .ksc at `ksc` that record a query, into the record, ordered by key. A key the .ksc gives twice
// keeps its last row
static void records_read(const char *ksc)
{
    if (s_identity_record.records.element == 0ULL)
    {
        platform_array_begun(&s_identity_record.records, sizeof(QueryRecorded), platform_place_g_program_owner,
                             PLATFORM_TIER_PERSISTENT);
    }
    (void)platform_array_counted(&s_identity_record.records, 0ULL);
    s_identity_record.sorted = 0ULL;
    KscFile ksc_file = {};
    static KscRow s_row;
    ksc_read(&ksc_file, ksc);
    // the cell taken once at the count of the .ksc's lines, each row a query at max
    if (!platform_array_counted(&s_identity_record.records, ksc_file.lines))
    {
        ksc_free(&ksc_file);
        identity_path_said(ksc, ": the pool cannot meet its record");
        return;
    }
    unsigned long long count = 0ULL;
    QueryRecorded *const records = (QueryRecorded *)s_identity_record.records.cell;
    for (unsigned int at = 0u; at < ksc_file.lines; at += 1u)
    {
        if (!ksc_row_read(ksc_file.line[at], "run", &s_row) ||
            (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_row.question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = "query ", .ci = EMBED_FALSE)))
        {
            continue;
        }
        // The key is the first six words after `query `, one space apart, as a stream's extraction reads them. The
        // refusal is the rest, one space at its start taken off, and nothing where fewer than six words are there
        const char *const words = s_row.question + 6;
        QueryRecorded *const kept = &records[count];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = kept, .bytes = sizeof(*kept), .val = 0);
        VerbaLinea key_line = mmgr_line_over(kept->key, sizeof(kept->key));
        size_t place = 0u;
        unsigned int read = 0u;
        for (; read < 6u; read += 1u)
        {
            place = record_spaces_past(words, place);
            if (words[place] == '\0')
            {
                break;
            }
            const size_t word_first = place;
            place += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &words[place],
                                .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = RECORD_SPACES);
            if (read != 0u)
            {
                mmgr_line_ch(&key_line, ' ');
            }
            (void)mmgr_line_put_n(&key_line, &words[word_first], place - word_first);
        }
        (void)mmgr_line_finish(&key_line);
        const char *refusal = (read == 6u) ? &words[place] : "";
        refusal = (refusal[0] == ' ') ? (refusal + 1) : refusal;
        // the refusal ends at a line's end, as std::getline ends it
        const char *const refusal_end =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = refusal, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\n');
        const size_t refusal_length = (refusal_end != NULL) ? (size_t)(refusal_end - refusal)
                                                            : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = refusal,
                                                                         .cap = PLATFORM_TEXT_TO_TERMINATOR);
        (void)identity_name_set(kept->answer, sizeof(kept->answer), s_row.answer, PLATFORM_TEXT_TO_TERMINATOR);
        // the word is the host's place of the first case apart, at max eight hex digits
        kept->word = (unsigned int)s_row.value;
        (void)identity_name_set(kept->refusal, sizeof(kept->refusal), refusal, refusal_length);
        count += 1ULL;
    }
    ksc_free(&ksc_file);
    (void)platform_array_counted(&s_identity_record.records, count);
    if (!records_settled(&s_identity_record))
    {
        (void)platform_array_counted(&s_identity_record.records, 0ULL);
        identity_path_said(ksc, ": the pool cannot meet the order of its record");
    }
}

void record_open(const char *ksc, const char *mode)
{
    (void)identity_name_set(s_record_ksc, sizeof(s_record_ksc), ksc, PLATFORM_TEXT_TO_TERMINATOR);
    // how far a container's code reaches, for a register refusal the cross gives the channel (run_channel.h)
    if (!run_channel_registers_past(ksc))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, ksc);
        mmgr_line_put(output_line,
                      " contains no answer of how far a container's code reaches: a register refusal stays refused");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    records_read(ksc);
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = ksc, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = ksc, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const last = ((back != NULL) && ((slash == NULL) || (back > slash))) ? back : slash;
    const char *const stem = (last != NULL) ? (last + 1) : ksc;
    const char *const dot =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = ksc, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    const char *const end =
        (dot != NULL) ? dot
                      : (ksc + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = ksc, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    VerbaLinea path_line = mmgr_line_over(s_query_record_path, sizeof(s_query_record_path));
    // the dot lies at or past the path's start
    (void)mmgr_line_put_n(&path_line, ksc, (size_t)(end - ksc));
    mmgr_line_put(&path_line, ".kqr");
    (void)mmgr_line_finish(&path_line);
    char classified[PLATFORM_PATH_BYTES];
    VerbaLinea classified_line = mmgr_line_over(classified, sizeof(classified));
    // the stem runs from past the last folder to the dot, and to the end where the dot lies in a folder before it
    const char *const stem_end =
        (end >= stem) ? end
                      : (ksc + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = ksc, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    (void)mmgr_line_put_n(&classified_line, stem, (size_t)(stem_end - stem));
    (void)mmgr_line_finish(&classified_line);
    char error[1280];
    if (!kqr_read(&s_query_record, s_query_record_path, classified, error, sizeof(error)))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, error);
        mmgr_line_put(output_line, ": the query record is not read, and nothing is written to it");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        s_query_record_path[0] = '\0';
    }
    else
    {
        // the record a view of the run's table of queries, written as its rows put it together
        kqr_tabled(&s_query_record, qry_member_run(), s_query_record_path);
    }
    kqr_mark(&s_query_record, KQR_CYCLE_RECORD, mode);
    static int s_registered = 0;
    if (s_registered == 0)
    {
        (void)platform_at_close(record_close);
        s_registered = 1;
    }
}
