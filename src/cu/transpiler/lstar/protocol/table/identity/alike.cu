// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// alike.cu: a question put and read alike or apart with the host, the host's questions written and its answers read,
// and a chain's code read from the engine
#include "../../../../../../../includes/parsers/who.h"
#include "../../../../../../platform/platform.h"
#include "identity_internal.h"

// the head of a query's line of the trace: the query's number, the code's size, the registers, the shape, the launches
// and the cases, then `tail`
static void query_trace_head(const RunQuestion *question, unsigned long long queries, const char *tail)
{
    VerbaLinea *const line = query_log_line(s_trace_log);
    mmgr_line_put(line, "query ");
    mmgr_line_decimal(line, queries, 1u);
    mmgr_line_put(line, ": ");
    mmgr_line_decimal(line, question->code_size, 1u);
    mmgr_line_put(line, " bytes, ");
    mmgr_line_decimal(line, question->registers, 1u);
    mmgr_line_put(line, " registers, ");
    mmgr_line_decimal(line, question->threads, 1u);
    mmgr_line_put(line, " threads in ");
    mmgr_line_decimal(line, question->blocks, 1u);
    mmgr_line_put(line, " blocks, ");
    mmgr_line_decimal(line, question->launches, 1u);
    mmgr_line_put(line, " launches, ");
    mmgr_line_decimal(line, question->cases, 1u);
    mmgr_line_put(line, tail);
}

// the line `query <number> <word>` on the query's log
static void query_log_said(unsigned long long queries, const char *word)
{
    VerbaLinea *const line = query_log_line(s_query_log);
    mmgr_line_put(line, "query ");
    mmgr_line_decimal(line, queries, 1u);
    mmgr_line_put(line, word);
    mmgr_line_ch(line, '\n');
}

// the answer the record stores of the terminated `key`, in place of what it stored before
static void record_stored(const char *key, const char *answer, unsigned int word, const char *refusal)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    QueryRecorded *kept = query_recorded(&s_identity_record, key, length);
    if (kept == NULL)
    {
        QueryRecorded added;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &added, .bytes = sizeof(added), .val = 0);
        (void)identity_name_set(added.key, sizeof(added.key), key, length);
        if (!platform_array_appended(&s_identity_record.records, &added))
        {
            return;
        }
        kept = (QueryRecorded *)platform_array_at(&s_identity_record.records, s_identity_record.records.count - 1ULL);
    }
    (void)identity_name_set(kept->answer, sizeof(kept->answer), answer, PLATFORM_TEXT_TO_TERMINATOR);
    kept->word = word;
    (void)identity_name_set(kept->refusal, sizeof(kept->refusal), refusal, PLATFORM_TEXT_TO_TERMINATOR);
}

int question_alike(const IdentityCode *code, const HostCases *host, const unsigned int *places, RunQuestion *question,
                   unsigned long long *queries)
{
    static int s_opened = 0;
    if (s_opened == 0)
    {
        query_logs_opened();
    }
    s_opened = 1;
    question->code = (const unsigned char *)code->cell;
    question->code_size = code->count;
    *queries += 1ULL;
    // the query's key in the record, and what the record contains of it
    unsigned long long hosted = 0xcbf29ce484222325ULL;
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const char *const answer = host_case(host, places[place]);
        hosted = record_hash(
            answer, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = answer, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u,
            hosted);
    }
    char keyed[160];
    VerbaLinea key_line = mmgr_line_over(keyed, sizeof(keyed));
    mmgr_line_hex(&key_line, record_hash(question->code, question->code_size, 0xcbf29ce484222325ULL), 16u);
    mmgr_line_put(&key_line, " ");
    mmgr_line_decimal(&key_line, question->registers, 1u);
    mmgr_line_put(&key_line, " ");
    mmgr_line_decimal(&key_line, question->threads, 1u);
    mmgr_line_put(&key_line, " ");
    mmgr_line_decimal(&key_line, question->blocks, 1u);
    mmgr_line_put(&key_line, " ");
    mmgr_line_hex(&key_line,
                  record_hash(question->word, sizeof(question->word[0]) * question->cases, 0xcbf29ce484222325ULL), 16u);
    mmgr_line_put(&key_line, " ");
    mmgr_line_hex(&key_line, hosted, 16u);
    const size_t key_length = mmgr_line_finish(&key_line);
    // R stores a query's verdict and its first case apart, and the query record and the log what came back of every
    // case. Where the run keeps the log, the query record answers a query or the run puts it to the part. The run
    // writes R as ever
    const int recordable = (s_record_ksc[0] != '\0') && (question->launches == 0u);
    const QueryRecorded *const recorded =
        (recordable && (s_query_log == NULL)) ? query_recorded(&s_identity_record, keyed, key_length) : NULL;
    if (recorded != NULL)
    {
        const QueryRecorded stored = *recorded;
        const int answers = identity_same(stored.answer, "answers");
        // the record censors a query the part answered illegal, its answer stored in R as the part gave it
        question->outcome = answers ? RUN_ANSWERED : identity_same(stored.answer, "illegal") ? RUN_GATED : RUN_NOTHING;
        VerbaLinea refused_line = mmgr_line_over(question->refused, sizeof(question->refused));
        mmgr_line_put(&refused_line, stored.refusal);
        (void)mmgr_line_finish(&refused_line);
        const int alike = answers && (stored.word == 0xffffffffu);
        s_apart_at = answers ? stored.word : 0xffffffffu;
        if (s_trace_log != NULL)
        {
            VerbaLinea *const line = query_log_line(s_trace_log);
            mmgr_line_put(line, "query ");
            mmgr_line_decimal(line, *queries, 1u);
            mmgr_line_put(line, ": recorded ");
            mmgr_line_put(line, stored.answer);
            mmgr_line_put(line, " ");
            mmgr_line_hex(line, stored.word, 8u);
            mmgr_line_put(line, " ");
            (void)mmgr_line_put_n(line, keyed, key_length);
            mmgr_line_ch(line, '\n');
            query_log_flushed(s_trace_log);
        }
        if (s_query_log != NULL)
        {
            query_log_said(*queries, " recorded");
        }
        return alike;
    }
    const char *const key = keyed;
    if (s_trace_log != NULL)
    {
        query_trace_head(question, *queries, " cases: ");
    }
    RunQuestion *const single[1] = {question};
    queries_carried(single, &t_dangerous, 1u);
    if (question->outcome != RUN_ANSWERED)
    {
        if (s_trace_log != NULL)
        {
            VerbaLinea *const line = query_log_line(s_trace_log);
            mmgr_line_put(line, "outcome ");
            mmgr_line_decimal(line, question->outcome, 1u);
            mmgr_line_put(line, ", ");
            mmgr_line_put(line, question->refused);
            mmgr_line_ch(line, '\n');
            query_log_flushed(s_trace_log);
        }
        if (s_query_log != NULL)
        {
            query_log_said(*queries, " refused");
        }
        if (recordable && ((question->outcome == RUN_ILLEGAL) || (question->outcome == RUN_NOTHING)))
        {
            record_stored(key, (question->outcome == RUN_ILLEGAL) ? "illegal" : "nothing", 0u, question->refused);
        }
        return 0;
    }
    if (s_query_log != NULL)
    {
        // the places this thread's log last wrote as its cases, by a hash of them and their count. A count past every
        // question's where it wrote none
        static thread_local unsigned long long t_logged_hash = 0ULL;
        static thread_local unsigned int t_logged_count = 0xffffffffu;
        const unsigned long long query_hash =
            record_hash(places, sizeof(unsigned int) * question->cases, 0xcbf29ce484222325ULL);
        if ((query_hash != t_logged_hash) || (question->cases != t_logged_count))
        {
            VerbaLinea *const line = query_log_line(s_query_log);
            mmgr_line_put(line, "cases");
            for (unsigned int place = 0u; place < question->cases; place += 1u)
            {
                mmgr_line_put(line, " ");
                case_text(places[place], line);
            }
            mmgr_line_ch(line, '\n');
            t_logged_hash = query_hash;
            t_logged_count = question->cases;
        }
        VerbaLinea *const line = query_log_line(s_query_log);
        mmgr_line_put(line, "query ");
        mmgr_line_decimal(line, *queries, 1u);
        mmgr_line_put(line, " ");
        for (unsigned int place = 0u; place < question->cases; place += 1u)
        {
            const int in_bracket = bracket_contains(host_case(host, places[place]), question->answered[place]);
            mmgr_line_ch(line, (in_bracket < 0) ? '-' : (in_bracket != 0) ? '=' : 'x');
        }
        mmgr_line_ch(line, '\n');
    }
    for (unsigned int place = 0u; place < question->cases; place += 1u)
    {
        const unsigned int at = places[place];
        const char *const expected = host_case(host, at);
        if (bracket_contains(expected, question->answered[place]) == 0)
        {
            if (s_trace_log != NULL)
            {
                VerbaLinea *const line = query_log_line(s_trace_log);
                mmgr_line_put(line, "apart at case ");
                mmgr_line_decimal(line, at, 1u);
                mmgr_line_put(line, ", the host ");
                mmgr_line_put(line, expected);
                mmgr_line_put(line, " and the part ");
                mmgr_line_hex(line, question->answered[place], 1u);
                mmgr_line_ch(line, '\n');
                query_log_flushed(s_trace_log);
            }
            if (recordable)
            {
                record_stored(key, "answers", at, "");
            }
            s_apart_at = at;
            return 0;
        }
    }
    if (recordable)
    {
        record_stored(key, "answers", 0xffffffffu, "");
    }
    if (s_trace_log != NULL)
    {
        VerbaLinea *const line = query_log_line(s_trace_log);
        mmgr_line_put(line, "alike");
        if (question->launches != 0u)
        {
            mmgr_line_put(line, ", ");
            mmgr_line_decimal(line, question->nanoseconds, 1u);
            mmgr_line_put(line, " ns");
        }
        mmgr_line_ch(line, '\n');
        query_log_flushed(s_trace_log);
    }
    return 1;
}

int question_read(const IdentityCode *code, RunQuestion *question, unsigned long long *queries)
{
    query_logs_opened();
    question->code = (const unsigned char *)code->cell;
    question->code_size = code->count;
    *queries += 1ULL;
    if (s_trace_log != NULL)
    {
        query_trace_head(question, *queries, " cases, read: ");
    }
    RunQuestion *const single[1] = {question};
    queries_carried(single, &t_dangerous, 1u);
    const int answered = question->outcome == RUN_ANSWERED;
    if (s_trace_log != NULL)
    {
        VerbaLinea *const line = query_log_line(s_trace_log);
        mmgr_line_put(line, answered ? "answered" : question->refused);
        mmgr_line_ch(line, '\n');
        query_log_flushed(s_trace_log);
    }
    if (s_query_log != NULL)
    {
        query_log_said(*queries, answered ? " read" : " refused");
    }
    return answered;
}

int stall_alike(const IdentityCode *code, const HostCases *host, const unsigned int *places, RunQuestion *question,
                unsigned long long *queries)
{
    question->registers = 255u;
    return question_alike(code, host, places, question, queries);
}

// The answers of one line of the host's blob from `at`, each a piece of the line, into `chain`, after the `count` it
// holds already, at max IDENTITY_CASES. Each answer's end is kept in `ends`, for the terminator the caller writes after
// the walk. The answer is the count of words the line holds from `at`
static unsigned long long host_line_read(CarrierPiece line, size_t at, HostCases *chain, char **ends)
{
    unsigned long long words = 0ULL;
    CarrierPiece answer;
    while (identity_word_next(line, &at, &answer))
    {
        if (chain->count < IDENTITY_CASES)
        {
            chain->answer[chain->count] = answer.text;
            // the word lies in the line, which lies in the blob the caller holds and writes
            ends[chain->count] = (char *)&answer.text[answer.length];
            chain->count += 1u;
        }
        words += 1ULL;
    }
    return words;
}

int host_answers_read(const char *answers, int whole, HostAnswers *host)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = host, .bytes = sizeof(*host), .val = 0);
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_blob(answers, &bytes, &size, NULL) || (size == 0ULL))
    {
        return 1;
    }
    // the blob kept whole on the persistent tier, each answer terminated in place
    host->text = (char *)platform_cell_taken(size + 1ULL, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    unsigned long long lines = 1ULL;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        lines += (bytes[at] == '\n') ? 1ULL : 0ULL;
    }
    host->chain = (host->text != NULL)
                      ? (HostCases *)platform_cell_cleared(lines, sizeof(HostCases), platform_place_g_program_owner,
                                                           PLATFORM_TIER_PERSISTENT)
                      : NULL;
    char **const ends = (char **)platform_cell_taken(IDENTITY_CASES * sizeof(char *), platform_place_g_program_owner,
                                                     PLATFORM_TIER_TEMPORARY);
    if ((host->chain == NULL) || (ends == NULL))
    {
        platform_cell_released(ends, platform_place_g_program_owner);
        host_answers_ended(host);
        return 0;
    }
    // a blob is at max the buffer's bytes, which a size_t counts
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = host->text, .src = bytes, .bytes = (size_t)size);
    host->text[size] = '\0';
    const CarrierPiece text = {host->text, (size_t)size};
    size_t cut = 0u;
    CarrierPiece line;
    while (carrier_piece_next(text, '\n', &cut, &line))
    {
        size_t at = 0u;
        CarrierPiece number;
        HostCases *const chain = &host->chain[host->count];
        if (!identity_word_next(line, &at, &number) || !IDENTITY_SET(chain->number, number.text, number.length))
        {
            continue;
        }
        chain->count = 0u;
        const unsigned long long words = host_line_read(line, at, chain, ends);
        if (whole && (words != IDENTITY_CASES))
        {
            continue;
        }
        for (unsigned int each = 0u; each < chain->count; each += 1u)
        {
            *ends[each] = '\0';
        }
        host->count += 1ULL;
    }
    platform_cell_released(ends, platform_place_g_program_owner);
    // the chains by number. A number given twice keeps its last line where the read is whole, and its lines' answers
    // in order where it is not
    if (!identity_sorted(host->chain, host->count, [](const HostCases &one, const HostCases &other) {
            return identity_order_text(one.number.text, other.number.text) < 0;
        }))
    {
        host_answers_ended(host);
        return 0;
    }
    unsigned long long kept = 0ULL;
    for (unsigned long long at = 0ULL; at < host->count; at += 1ULL)
    {
        HostCases *const later = &host->chain[at];
        HostCases *const earlier = (kept != 0ULL) ? &host->chain[kept - 1ULL] : NULL;
        if ((earlier == NULL) || !identity_same(earlier->number.text, later->number.text))
        {
            host->chain[kept] = *later;
            kept += 1ULL;
            continue;
        }
        if (whole)
        {
            *earlier = *later;
            continue;
        }
        for (unsigned int each = 0u; (each < later->count) && (earlier->count < IDENTITY_CASES); each += 1u)
        {
            earlier->answer[earlier->count] = later->answer[each];
            earlier->count += 1u;
        }
    }
    host->count = kept;
    return 1;
}

// the path of `engine`'s file of chain `number` with `suffix`, into `path`
static void chain_path(const char *engine, const char *number, const char *suffix, char *path)
{
    VerbaLinea path_line = mmgr_line_over(path, PLATFORM_PATH_BYTES);
    mmgr_line_put(&path_line, engine);
    mmgr_line_ch(&path_line, '/');
    mmgr_line_put(&path_line, number);
    mmgr_line_put(&path_line, suffix);
    (void)mmgr_line_finish(&path_line);
}

int chain_code_read(const char *engine, const char *number, IdentityCode *code, PlatformArray *links)
{
    char listing_path[PLATFORM_PATH_BYTES];
    char binary_path[PLATFORM_PATH_BYTES];
    chain_path(engine, number, ".dis", listing_path);
    chain_path(engine, number, ".bin", binary_path);
    unsigned long long binary_size = 0ULL;
    size_t listing_size = 0u;
    char *const listing = identity_file_read(listing_path, &listing_size);
    if ((listing == NULL) || !platform_file_size(binary_path, &binary_size))
    {
        platform_cell_released(listing, platform_place_g_program_owner);
        return 0;
    }
    unsigned long long read = 0ULL;
    if (!platform_array_counted(code, binary_size) ||
        ((binary_size != 0ULL) && !platform_file_read(binary_path, code->cell, binary_size, &read)))
    {
        (void)platform_array_counted(code, 0ULL);
    }
    (void)platform_array_counted(code, read);
    // the links' cell taken once at the listing's lines, a link a line at max
    unsigned long long lines = 1ULL;
    for (size_t at = 0u; at < listing_size; at += 1u)
    {
        lines += (listing[at] == '\n') ? 1ULL : 0ULL;
    }
    unsigned long long count = 0ULL;
    if (platform_array_counted(links, 0ULL) && platform_array_counted(links, lines))
    {
        const CarrierPiece text = {listing, listing_size};
        size_t cut = 0u;
        CarrierPiece line;
        while (identity_line_next(text, &cut, &line))
        {
            Link *const link = (Link *)platform_array_at(links, count);
            const int link_found = link_read(line, link);
            if (link_found < 0)
            {
                break;
            }
            count += ((link_found > 0) && (link->position.text[0] != '\0')) ? 1ULL : 0ULL;
        }
    }
    (void)platform_array_counted(links, count);
    platform_cell_released(listing, platform_place_g_program_owner);
    const unsigned long long bytes = code->count;
    return (count != 0ULL) && ((bytes % 16ULL) == 0ULL) && ((count * 16ULL) <= bytes);
}

unsigned int chain_registers(const char *engine, const char *number)
{
    char path[PLATFORM_PATH_BYTES];
    chain_path(engine, number, ".registers", path);
    size_t size = 0u;
    char *const file = identity_file_read(path, &size);
    if (file == NULL)
    {
        return 0u;
    }
    size_t at = 0u;
    while ((at < size) && identity_space(file[at]))
    {
        at += 1u;
    }
    const char *end = NULL;
    const unsigned long long registers =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &file[at], .end = &end, .base = 10);
    // a count that read no digit, or passes an unsigned int, fails as the extraction fails
    const int read = (end != NULL) && (end != &file[at]) && (registers <= 0xffffffffULL);
    platform_cell_released(file, platform_place_g_program_owner);
    // a count within an unsigned int, as the test above bounds it
    return read ? (unsigned int)registers : 0u;
}

int ksc_answers_written(const char *ksc, const char *queried, const char *const *rows, unsigned int count, Who who)
{
    KscFile file = {};
    KfileLines removed = {};
    const int written = ksc_read(&file, ksc) &&
                        ksc_rows_replaced_traced_by(&file, "run", queried, rows, count, who, &removed) &&
                        ksc_write_by(&file, ksc, who);
    ksc_free(&file);
    if (written && (removed.lines != 0u))
    {
        char krf[1200];
        krf_path_beside(ksc, krf, sizeof(krf));
        if (!krf_removed_by(krf, "ksc", "replaced", removed.line, removed.lines, who))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_decimal(output_line, removed.lines, 1u);
            mmgr_line_put(output_line, " rows let go from ");
            mmgr_line_put(output_line, ksc);
            mmgr_line_put(output_line, " could not be recorded in ");
            mmgr_line_put(output_line, krf);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    kfile_lines_free(&removed);
    return written;
}

// The kernel `number` of the stick's text, from its signature to the brace that closes it, or to the end where no
// brace closes it. An empty piece where the stick has no such kernel
static CarrierPiece kernel_text(CarrierPiece stick, const char *number)
{
    char signature[160];
    VerbaLinea signature_line = mmgr_line_over(signature, sizeof(signature));
    mmgr_line_put(&signature_line, "extern \"C\" __global__ void measuring_stick_");
    mmgr_line_put(&signature_line, number);
    mmgr_line_ch(&signature_line, '(');
    (void)mmgr_line_finish(&signature_line);
    const char *const first = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = stick.text, .cap = stick.length,
                                         .other = signature, .other_cap = stick.length, .ci = EMBED_FALSE);
    if (first == NULL)
    {
        const CarrierPiece none = {stick.text, 0u};
        return none;
    }
    // the kernel's first byte lies within the stick
    const size_t from = (size_t)(first - stick.text);
    const char *const last = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = first, .cap = stick.length - from,
                                        .other = "\n}\n", .other_cap = stick.length - from, .ci = EMBED_FALSE);
    const size_t length = (last != NULL) ? ((size_t)(last - first) + 3u) : (stick.length - from);
    const CarrierPiece kernel = {first, length};
    return kernel;
}

// the type a kernel's first operand is cast to, as its text declares it, const or not, into `type`: a compound
// assignment writes it
static void first_type(CarrierPiece kernel, char *type, size_t capacity)
{
    static TablePatternOnce s_first;
    TableMatch found;
    VerbaLinea type_line = mmgr_line_over(type, capacity);
    if (table_search(table_once(&s_first, "\\b(?:const )?([a-z][a-z ]*) a = \\(", 0), kernel.text, kernel.length,
                     &found))
    {
        size_t length = 0u;
        const char *const group = table_group(&found, 1u, kernel.text, &length);
        (void)mmgr_line_put_n(&type_line, group, length);
    }
    else
    {
        mmgr_line_put(&type_line, "int");
    }
    (void)mmgr_line_finish(&type_line);
}

// 1 where the `length` bytes at `text` hold `needle`
static int kernel_has(CarrierPiece kernel, const char *needle)
{
    return EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = kernel.text, .cap = kernel.length, .other = needle,
                      .other_cap = kernel.length, .ci = EMBED_FALSE) != NULL;
}

// the lines of the host's program before its questions, and those that read them
static const char *const s_host_head =
    "// the stick's integer questions as the host computes them, written by orior_klq_identity\n"
    "#include <limits>\n#include <stdio.h>\n\n"
    "struct HostIndex\n{\n    unsigned int x, y, z;\n};\n"
    "static const HostIndex threadIdx = {0u, 0u, 0u};\nstatic const HostIndex blockIdx = {0u, 0u, 0u};\n"
    "static const HostIndex blockDim = {1u, 1u, 1u};\n#define __global__\n\n";

// one question's kernel and the test of the cases the host's C refuses, onto `line`
static void host_question_put(VerbaLinea *line, CarrierPiece kernel, const char *number)
{
    (void)mmgr_line_put_n(line, kernel.text, kernel.length);
    mmgr_line_ch(line, '\n');
    char type[CARRIER_TEXT_MAX];
    first_type(kernel, type, sizeof(type));
    const int divides = kernel_has(kernel, " / b") || kernel_has(kernel, " % b") || kernel_has(kernel, "/=") ||
                        kernel_has(kernel, "%=") || kernel_has(kernel, " / ");
    const int shifts = kernel_has(kernel, "<<") || kernel_has(kernel, ">>");
    // 1 where the host's C would trap on the case or leave it undefined. A divisor of zero, the least value over
    // -1, or a shift by the width or past it
    mmgr_line_put(line, "static int refused_");
    mmgr_line_put(line, number);
    mmgr_line_put(line, "(const unsigned long long *in)\n{\n");
    mmgr_line_put(line, "    const ");
    mmgr_line_put(line, type);
    mmgr_line_put(line, " a = (");
    mmgr_line_put(line, type);
    mmgr_line_put(line, ")in[0];\n    const ");
    mmgr_line_put(line, type);
    mmgr_line_put(line, " b = (");
    mmgr_line_put(line, type);
    mmgr_line_put(line, ")in[1];\n    (void)a;\n    (void)b;\n");
    if (divides)
    {
        mmgr_line_put(line, "    if ((b == 0) || (std::numeric_limits<");
        mmgr_line_put(line, type);
        mmgr_line_put(line, ">::is_signed && (b == (");
        mmgr_line_put(line, type);
        mmgr_line_put(line, ")-1) && (a == std::numeric_limits<");
        mmgr_line_put(line, type);
        mmgr_line_put(line, ">::min())))\n    {\n        return 1;\n    }\n");
    }
    if (shifts)
    {
        mmgr_line_put(line, "    if (((unsigned long long)b >= (unsigned long long)(sizeof(a + 0) * "
                            "8u)) || ((long long)b < 0))\n    {\n        return 1;\n    }\n");
    }
    mmgr_line_put(line, "    return 0;\n}\n\n");
}

// the host program's table of its questions, its values and its main, onto `line`
static void host_main_put(VerbaLinea *line, const char *const *written, unsigned long long count)
{
    mmgr_line_put(line,
                  "typedef void (*HostKernel)(const unsigned long long *, unsigned long long *, unsigned int);\n");
    mmgr_line_put(line, "typedef int (*HostRefused)(const unsigned long long *);\n");
    mmgr_line_put(line, "static const struct\n{\n    const char *number;\n    HostKernel kernel;\n    "
                        "HostRefused refused;\n} s_questions[] = {\n");
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        const char *const number = written[at];
        mmgr_line_put(line, "    {\"");
        mmgr_line_put(line, number);
        mmgr_line_put(line, "\", measuring_stick_");
        mmgr_line_put(line, number);
        mmgr_line_put(line, ", refused_");
        mmgr_line_put(line, number);
        mmgr_line_put(line, "},\n");
    }
    mmgr_line_put(line, "};\n\nstatic const unsigned long long s_values[] = {");
    for (unsigned int at = 0u; at < IDENTITY_VALUES; at += 1u)
    {
        mmgr_line_put(line, (at == 0u) ? "" : ", ");
        mmgr_line_put(line, "0x");
        mmgr_line_hex(line, s_values[at], 1u);
        mmgr_line_put(line, "ull");
    }
    mmgr_line_put(line, "};\n\n");
    mmgr_line_put(
        line,
        "// each question a line: its number, then its answer to every case in order, - where the case is refused\n");
    mmgr_line_put(line, "int main(void)\n{\n");
    mmgr_line_put(
        line, "    for (unsigned int at = 0u; at < sizeof(s_questions) / sizeof(s_questions[0]); at += 1u)\n    {\n");
    mmgr_line_put(line, "        printf(\"");
    mmgr_line_ch(line, '%');
    mmgr_line_put(line, "s\", s_questions[at].number);\n");
    mmgr_line_put(line, "        for (unsigned int third = 0u; third < ");
    mmgr_line_decimal(line, IDENTITY_THIRDS, 1u);
    mmgr_line_put(line, "u; third += 1u)\n        {\n");
    mmgr_line_put(line, "            for (unsigned int left = 0u; left < ");
    mmgr_line_decimal(line, (unsigned int)IDENTITY_VALUES, 1u);
    mmgr_line_put(line, "u; left += 1u)\n            {\n");
    mmgr_line_put(line, "                for (unsigned int right = 0u; right < ");
    mmgr_line_decimal(line, (unsigned int)IDENTITY_VALUES, 1u);
    mmgr_line_put(line, "u; right += 1u)\n                {\n");
    mmgr_line_put(
        line, "                    const unsigned long long in[4] = {s_values[left], s_values[right], third, 0ULL};\n");
    mmgr_line_put(line, "                    unsigned long long out[2] = {0ULL, 0ULL};\n");
    mmgr_line_put(line, "                    if (s_questions[at].refused(in))\n                    {\n");
    mmgr_line_put(
        line, "                        printf(\" -\");\n                        continue;\n                    }\n");
    mmgr_line_put(line, "                    s_questions[at].kernel(in, out, 1u);\n");
    mmgr_line_put(line, "                    printf(\" ");
    mmgr_line_ch(line, '%');
    mmgr_line_put(line, "llx\", out[0]);\n                }\n            }\n        }\n");
    mmgr_line_put(line, "        printf(\"\\n\");\n    }\n    return 0;\n}\n");
}

int host_questions_write(const char *path, CarrierPiece stick, const IdentityName *numbers, unsigned long long count)
{
    // the numbers whose kernels the stick holds, at max every number given
    const char **const written =
        (const char **)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(const char *),
                                           platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (written == NULL)
    {
        identity_path_said(path, ": the pool cannot meet its questions");
        return 0;
    }
    RunWritten file;
    run_written_begun(&file, path, 0);
    VerbaLinea *const line = &file.text.line;
    mmgr_line_put(line, s_host_head);
    unsigned long long written_count = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        const CarrierPiece kernel = kernel_text(stick, numbers[at].text);
        if (kernel.length == 0u)
        {
            continue;
        }
        host_question_put(line, kernel, numbers[at].text);
        written[written_count] = numbers[at].text;
        written_count += 1ULL;
    }
    host_main_put(line, written, written_count);
    platform_cell_released(written, platform_place_g_program_owner);
    const int closed = run_closed(&file);
    run_written_ended(&file);
    if (!closed)
    {
        identity_path_said(path, " could not be written");
        return 0;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, path);
    mmgr_line_put(output_line, ": ");
    mmgr_line_decimal(output_line, written_count, 1u);
    mmgr_line_put(output_line, " questions the host computes");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 1;
}
