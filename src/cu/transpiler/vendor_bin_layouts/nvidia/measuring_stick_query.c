// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_query.c: the query's questions read against the vendor's own disassembler, the object the carrier
// popped out under --diff-output-against-vendor. A question whose probe instruction the vendor calls illegal, reads as
// a control transfer our gate's operation key could not, reads as naming a register at or past the count its kernel
// allots, reads as writing a register the code after the probe addresses memory through, or reads as a memory access of
// its own, is one the gate passed that must be blocked before a series reaches the part. Our gate judges by operation
// key and by the fields a form has learned: it cannot know a whole encoding illegal while the operation it probes is
// still unlearned, nor where a register sits in a form whose fields are not yet queried. The vendor's reader prints
// both. A register past the count faults the part at once, an out-of-range register warp exception, and a run of them
// takes the display and the machine down with it. A probe that overwrites an address the kernel stores through, or that
// reads or writes memory through registers the question set no address in, faults it with an illegal address. Nothing
// here runs on the part.
//
//     orior_measuring_stick_query <question list> <nvdisasm> <architecture> <report> <gated> [<refusals> [<read>]]
//
// Every name but the disassembler's is a blob of the run, the .qry QRY names (qry_member.h): the list, a line
// a question, `<code> <registers> <cases> <answers> <probe> [<launches>]` (run_channel.h), each question's code, and
// the container the carrier popped for it as <answers stem>.cubin. Every container is the kernel the system accepted
// with the question's code in it; the code of each, as the part would be passed it, is read out of the container's code
// section.
//
// The vendor reads a raw run of encodings (nvdisasm --binary <architecture>) only from a file's path: each run is
// passed to the run's writer as a stream of the .qry (qry_streamed), and the vendor is pointed at that stream. The
// writer is the only thing that writes a file, and everything the vendor prints of a run is a blob of the run.
//
// The probe instructions are read each distinct encoding once, in as many runs as the host has processors, each run
// read by a reader thread of its own and the frames and tails read beside them. The vendor stops at the first
// instruction it calls illegal and names its address: that probe is put to a NOP and the run read again, a long run as
// two, the part to the stop and the part past it, until every run reads clean, and a clean run gives every legal
// probe's operation at once. A stop that names no address splits the run in two, down to the one probe it stops at
// (frames.c, msq_range_read). Each probe the vendor reads whole has every register it names checked against the
// question's <registers>, RZ aside, a register read as a pair (.64, .WIDE) reaching its second word and one of .128 its
// fourth. The code past each probe is read to its first EXIT with no guard, every distinct one together as one raw run,
// for the registers its memory operands read an address from; a probe that writes one of them is blocked, as is a probe
// with a memory operand of its own.
//
// Every question's frame is read whole too: its code with its probe put to a NOP, or its whole code where it contains
// no probe, each distinct frame once. Each instruction of it the vendor calls illegal is put to a NOP and the frame
// read again; each such instruction, and each the vendor reads naming a register at or past the question's <registers>,
// is blocked. The gate blocks a whole code where any instruction of it is blocked (cubin_safe.c). A frame the vendor
// stops in at no instruction it names blocks its questions by no encoding.
//
// The report, passed as <report>, lists every question blocked; <gated> is the feedback the carrier reads (--gated): a
// line
// `<low> <high>` an encoding blocked, for the carrier to block every code that contains it from the part. <refusals>,
// where it is given, is the feedback the teacher reads before the round is carried (run_channel.h): a line a refusal,
// `<answers stem> <registers> <why>`, the count of registers that clears it where it is a register past the count its
// kernel allots and that count is at max MSQ_ALLOTTED_MAX, 0 where no count clears it. The vendor names no
// operation and nothing it reports enters a form: the gated list is only the exact words not to query, and a count that
// clears a refusal changes how a question is put and not what it requires. Exit 1 where any
// question is blocked, 2 where a blob, a tool or the run was not reached, or a frame was read at no stop it
// named: the round is then not carried
#include "measuring_stick_query/msq_internal.h"
#include "../../../../../includes/parsers/line_read.h"

int main(int count, char **words)
{
    if ((count < 6) || (count > 8))
    {
        fprintf(stderr,
                "orior_measuring_stick_query <question list> <nvdisasm> <architecture> <report> <gated> [<refusals>"
                "[<read>]]\n");
        return 2;
    }
    const char *const list_name = words[1];
    s_nvdisasm = words[2];
    s_architecture = words[3];
    const char *const report_name = words[4];
    const char *const gated_name = words[5];
    const char *const gated_list_name = (count >= 7) ? words[6] : NULL;
    const char *const read_name = (count == 8) ? words[7] : NULL;
    s_run = qry_member_run();
    if (s_run == NULL)
    {
        fprintf(stderr, "orior_measuring_stick_query: no query writer runs: %s names no .qry a writer has open\n",
                QRY_ENVIRONMENT);
        return 2;
    }
    const unsigned char *list = NULL;
    unsigned long long list_size = 0ull;
    if (!qry_member_blob(list_name, &list, &list_size, NULL))
    {
        fprintf(stderr, "orior_measuring_stick_query: the run's buffer contains no %s\n", list_name);
        return 2;
    }
    // a question with a probe is read by its probe instruction, among every other's; one with none, the kernel's own,
    // is read whole, once a code
    size_t lines = 0u;
    for (unsigned long long at = 0ull; at < list_size; at += 1ull)
    {
        lines += (list[at] == '\n') ? 1u : 0u;
    }
    MsqQuestion *const questions = (MsqQuestion *)calloc(lines + 1u, sizeof(MsqQuestion));
    if (questions == NULL)
    {
        return 2;
    }
    size_t question_count = 0u;
    for (unsigned long long at = 0ull; at < list_size;)
    {
        const unsigned char *const end = (const unsigned char *)memchr(list + at, '\n', (size_t)(list_size - at));
        const size_t length = (end != NULL) ? (size_t)(end - (list + at)) : (size_t)(list_size - at);
        char line[1024];
        snprintf(line, sizeof(line), "%.*s", (int)length, (const char *)(list + at));
        at += (unsigned long long)length + 1ull;
        char code_name[256];
        char cases_name[256];
        char answers_name[256];
        unsigned int registers = 0u;
        unsigned long long probe_place = 0ull;
        const char *read = line;
        if (!line_word(&read, code_name, sizeof(code_name)) || !line_unsigned(&read, &registers) ||
            !line_word(&read, cases_name, sizeof(cases_name)) ||
            !line_word(&read, answers_name, sizeof(answers_name)) || !line_number(&read, 0, &probe_place))
        {
            continue;
        }
        char container_name[300];
        msq_extension_put(answers_name, ".cubin", container_name, sizeof(container_name));
        MsqBytes container;
        MsqQuestion *const question = &questions[question_count];
        if (!qry_member_blob(container_name, &container.bytes, &container.size, NULL) ||
            !msq_container_code(&container, &question->code))
        {
            continue;
        }
        msq_stem(answers_name, question->stem, sizeof(question->stem));
        snprintf(question->code_name, sizeof(question->code_name), "%s", code_name);
        question->place = probe_place;
        question->registers = registers;
        question->placed =
            (probe_place < (question->code.size / 16ull)) && (((probe_place * 16ull) + 16ull) <= question->code.size);
        if (question->placed)
        {
            memcpy(question->word, question->code.bytes + (probe_place * 16ull), 16u);
        }
        question_count += 1u;
    }
    // the distinct probe encodings, whole codes, codes before and past a probe, and frames, each read once: a slotted
    // question's frame is the code before its probe, a NOP, and the code past it, found by the two
    MsqDistinct place_words;
    MsqDistinct whole_codes;
    MsqDistinct tails;
    MsqDistinct prefixes;
    MsqDistinct pieces;
    size_t *const piece_of = (size_t *)calloc(question_count + 1u, sizeof(size_t));
    if (!msq_distinct_made(&place_words, question_count) || !msq_distinct_made(&whole_codes, question_count) ||
        !msq_distinct_made(&tails, question_count) || !msq_distinct_made(&prefixes, question_count) ||
        !msq_distinct_made(&pieces, question_count) || (piece_of == NULL))
    {
        return 2;
    }
    for (size_t place = 0u; place < question_count; place += 1u)
    {
        MsqQuestion *const question = &questions[place];
        if (question->placed)
        {
            const MsqBytes word = {question->word, 16ull};
            question->word_index = msq_distinct_index(&place_words, &word);
            const MsqBytes tail = {question->code.bytes + ((question->place + 1ull) * 16ull),
                                   question->code.size - ((question->place + 1ull) * 16ull)};
            question->tail_index = msq_distinct_index(&tails, &tail);
            const MsqBytes prefix = {question->code.bytes, question->place * 16ull};
            question->prefix_index = msq_distinct_index(&prefixes, &prefix);
            question->piece_key[0] = question->prefix_index;
            question->piece_key[1] = question->tail_index;
            const MsqBytes key = {(const unsigned char *)question->piece_key, sizeof(question->piece_key)};
            const size_t before = pieces.count;
            question->piece_index = msq_distinct_index(&pieces, &key);
            // the question a frame is first found by gives its bytes
            if (pieces.count != before)
            {
                piece_of[question->piece_index] = place;
            }
        }
        else
        {
            question->whole_index = msq_distinct_index(&whole_codes, &question->code);
        }
    }
    // The readings of the distinct probe encodings, and every frame read whole: a question with no probe by its whole
    // code, a slotted one by the code before its probe, a NOP and the code past it, the code past a probe being the
    // same in every question of one kernel. The encodings, the frames and the tails are read together as one
    // accumulated run at one start of the vendor (msq_accumulated_read), and a piece it stops at is set aside and read
    // alone
    MsqReading *const readings = (MsqReading *)calloc(place_words.count + 1u, sizeof(MsqReading));
    MsqFrame *const whole_frames = (MsqFrame *)calloc(whole_codes.count + 1u, sizeof(MsqFrame));
    MsqFrame *const piece_frames = (MsqFrame *)calloc(pieces.count + 1u, sizeof(MsqFrame));
    MsqRegisters *const addressed = (MsqRegisters *)calloc(tails.count + 1u, sizeof(MsqRegisters));
    const size_t frame_count = whole_codes.count + pieces.count;
    MsqBytes *const framed = (MsqBytes *)calloc(frame_count + 1u, sizeof(MsqBytes));
    MsqFrame *const frame_read = (MsqFrame *)calloc(frame_count + 1u, sizeof(MsqFrame));
    unsigned char **const piece_bytes = (unsigned char **)calloc(pieces.count + 1u, sizeof(unsigned char *));
    int whole = (readings != NULL) && (whole_frames != NULL) && (piece_frames != NULL) && (addressed != NULL) &&
                (framed != NULL) && (frame_read != NULL) && (piece_bytes != NULL);
    for (size_t code = 0u; whole && (code < whole_codes.count); code += 1u)
    {
        framed[code] = whole_codes.items[code];
    }
    for (size_t piece = 0u; whole && (piece < pieces.count); piece += 1u)
    {
        const MsqQuestion *const first = &questions[piece_of[piece]];
        const MsqBytes *const prefix = &prefixes.items[first->prefix_index];
        const MsqBytes *const tail = &tails.items[first->tail_index];
        size_t joined_size = 0u;
        size_t joined_room = 0u;
        // a code is at max a container's, which a size_t counts
        whole = msq_added(&piece_bytes[piece], &joined_size, &joined_room, prefix->bytes, (size_t)prefix->size) &&
                msq_added(&piece_bytes[piece], &joined_size, &joined_room, s_nop, sizeof(s_nop)) &&
                msq_added(&piece_bytes[piece], &joined_size, &joined_room, tail->bytes, (size_t)tail->size);
        framed[whole_codes.count + piece].bytes = piece_bytes[piece];
        framed[whole_codes.count + piece].size = (unsigned long long)joined_size;
    }
    // every piece in one accumulated run, read at one start: the distinct probe encodings, then the frames, then the
    // distinct tails; a piece the vendor stops at set aside, and the run read again without it until it reads clean
    const size_t piece_count = place_words.count + frame_count + tails.count;
    MsqPiece *const run_pieces = (MsqPiece *)calloc(piece_count + 1u, sizeof(MsqPiece));
    size_t *const aside = (size_t *)calloc(piece_count + 1u, sizeof(size_t));
    whole = whole && (run_pieces != NULL) && (aside != NULL);
    size_t placed = 0u;
    for (size_t word = 0u; whole && (word < place_words.count); word += 1u)
    {
        run_pieces[placed] = (MsqPiece){MSQ_PIECE_WORD, word, place_words.items[word], 0ull, MSQ_PIECE_READ};
        placed += 1u;
    }
    for (size_t frame = 0u; whole && (frame < frame_count); frame += 1u)
    {
        run_pieces[placed] = (MsqPiece){MSQ_PIECE_FRAME, frame, framed[frame], 0ull, MSQ_PIECE_READ};
        placed += 1u;
    }
    for (size_t tail = 0u; whole && (tail < tails.count); tail += 1u)
    {
        run_pieces[placed] = (MsqPiece){MSQ_PIECE_TAIL, tail, tails.items[tail], 0ull, MSQ_PIECE_READ};
        placed += 1u;
    }
    // a piece containing an instruction the vendor's own instruction set reads as no class is one its disassembler
    // stops at: set aside before the run, to be read alone, so that the run is read clean at one start
    for (size_t piece = 0u; whole && msq_tables_read() && (piece < piece_count); piece += 1u)
    {
        run_pieces[piece].state =
            msq_vendor_reads(&run_pieces[piece].bytes) ? MSQ_PIECE_READ : MSQ_PIECE_UNREAD;
    }
    MsqReader reader;
    memset(&reader, 0, sizeof(reader));
    reader.output_room = 8ull << 20u;
    reader.output = whole ? (char *)malloc((size_t)reader.output_room) : NULL;
    // the accumulated run's listing, which every reading and frame of it points into until the cross ends
    MsqListing accumulated;
    memset(&accumulated, 0, sizeof(accumulated));
    whole = whole && (reader.output != NULL) && msq_accumulated_read(&reader, run_pieces, piece_count, &accumulated);
    if (whole)
    {
        msq_pieces_cut(&accumulated, run_pieces, piece_count, readings, frame_read, addressed);
    }
    size_t aside_count = 0u;
    for (size_t piece = 0u; whole && (piece < piece_count); piece += 1u)
    {
        if (run_pieces[piece].state != MSQ_PIECE_READ)
        {
            aside[aside_count] = piece;
            aside_count += 1u;
        }
    }
    // the pieces set aside, each read alone, on a reader thread for each up to the host's processors
    MsqPool pool;
    memset(&pool, 0, sizeof(pool));
    pool.pieces = run_pieces;
    pool.aside = aside;
    pool.aside_count = aside_count;
    pool.readings = readings;
    pool.frame_read = frame_read;
    pool.addressed = addressed;
    const unsigned int processors = msq_processors();
    const unsigned int threads = (aside_count < processors) ? (unsigned int)aside_count : processors;
    pthread_t *const started = (pthread_t *)calloc((size_t)threads + 1u, sizeof(pthread_t));
    unsigned int readers = 0u;
    for (unsigned int thread = 0u; whole && (started != NULL) && (thread < threads); thread += 1u)
    {
        if (pthread_create(&started[thread], NULL, msq_pool_worker, &pool) != 0)
        {
            pool.why = "a reader thread could not be started";
            atomic_store(&pool.failed, 1);
            break;
        }
        readers += 1u;
    }
    for (unsigned int thread = 0u; thread < readers; thread += 1u)
    {
        pthread_join(started[thread], NULL);
    }
    free(started);
    whole = whole && (started != NULL) && !atomic_load(&pool.failed);
    const unsigned long long accumulated_calls = reader.calls;
    free(reader.output);
    free(reader.raw);
    free(run_pieces);
    free(aside);
    for (size_t code = 0u; code < whole_codes.count; code += 1u)
    {
        whole_frames[code] = frame_read[code];
    }
    for (size_t piece = 0u; piece < pieces.count; piece += 1u)
    {
        piece_frames[piece] = frame_read[whole_codes.count + piece];
        free(piece_bytes[piece]);
    }
    free(piece_bytes);
    free(framed);
    free(frame_read);
    if (!whole)
    {
        fprintf(stderr, "orior_measuring_stick_query: the vendor's disassembler %s could not be run, or there was no memory: %s\n",
                s_nvdisasm, (pool.why != NULL) ? pool.why : "the work could not be queued");
        return 2;
    }
    // the questions blocked, each with why, the gated list and the report passed to the run. A question is blocked
    // by its probe where the vendor reads it as below, and by its frame where the vendor calls an instruction of it
    // illegal or reads one naming a register at or past the count its kernel allots: each such instruction's encoding
    // is blocked, and the gate blocks every code that contains it. A frame the vendor stopped in at no instruction it
    // named blocks its questions by no encoding, and the round they came in is not carried
    MsqVerdicts verdicts;
    memset(&verdicts, 0, sizeof(verdicts));
    size_t gated_count = 0u;
    size_t unread = 0u;
    for (size_t place = 0u; place < question_count; place += 1u)
    {
        const MsqQuestion *const question = &questions[place];
        const MsqFrame *const frame =
            question->placed ? &piece_frames[question->piece_index] : &whole_frames[question->whole_index];
        int gated_question = 0;
        if (!frame->readable)
        {
            msq_gate_reported(&verdicts, question, NULL,
                              "the vendor stopped in its code at no instruction it named, and the round is not carried",
                              0u);
            unread += 1u;
            gated_count += 1u;
            continue;
        }
        for (size_t at = 0u; at < frame->illegal_count; at += 1u)
        {
            char why[160];
            snprintf(why, sizeof(why), "the vendor reads an illegal instruction at byte %llu of the code",
                     frame->illegal[at]);
            msq_gate_reported(&verdicts, question, question->code.bytes + frame->illegal[at], why, 0u);
            gated_question = 1;
        }
        for (size_t line = 0u; line < frame->listing.count; line += 1u)
        {
            const MsqLine *const read_line = &frame->listing.lines[line];
            unsigned long long frame_reached = 0ull;
            const int in_code = (read_line->offset + 16ull) <= question->code.size;
            if (in_code && msq_register_reached(read_line->body, question->registers, &frame_reached))
            {
                char why[200];
                snprintf(why, sizeof(why),
                         "the vendor reads R%llu at byte %llu of the code, past the %u registers its "
                         "kernel allots",
                         frame_reached, read_line->offset, question->registers);
                msq_gate_reported(&verdicts, question, question->code.bytes + read_line->offset, why,
                                  msq_clearing(frame_reached));
                gated_question = 1;
            }
            else if (in_code && !question->placed && !msq_control_read(read_line->body))
            {
                // an instruction of a whole question read clear: legal, within the registers its kernel allots and no
                // control transfer but the EXIT a kernel ends in; reported to the teacher with its encoding, for the
                // conference the gate calls where it blocks the instruction's key unknown (overseer.h)
                msq_clear_reported(&verdicts, question, read_line->body, question->code.bytes + read_line->offset);
            }
        }
        gated_count += gated_question ? 1u : 0u;
        if (!question->placed)
        {
            continue;
        }
        const MsqReading *const reading = &readings[question->word_index];
        const MsqKind type = reading->type;
        const char *const body = ((type == MSQ_READ) && (reading->body != NULL)) ? reading->body : "";
        char why[256] = "";
        unsigned int clearing = 0u;
        unsigned long long reached = 0ull;
        MsqRegisters written;
        msq_registers_written(body, &written);
        long long overwritten = -1;
        if (question->placed)
        {
            const MsqRegisters *const read_from = &addressed[question->tail_index];
            for (unsigned int number = 0u; (overwritten < 0) && (number < MSQ_REGISTERS); number += 1u)
            {
                if ((written.bits[number / 64u] & read_from->bits[number / 64u] & (1ull << (number % 64u))) != 0ull)
                {
                    overwritten = (long long)number;
                }
            }
        }
        int control = 0;
        for (size_t at = 0u; (type == MSQ_READ) && (at < (sizeof(s_control) / sizeof(s_control[0]))); at += 1u)
        {
            control |= (strcmp(reading->operation, s_control[at]) == 0);
        }
        if (type == MSQ_ILLEGAL)
        {
            snprintf(why, sizeof(why), "%s", s_illegal_why);
        }
        else if (control)
        {
            snprintf(why, sizeof(why), "the vendor reads %s at the probe, a control transfer", reading->operation);
        }
        else if (msq_register_reached(body, question->registers, &reached))
        {
            snprintf(why, sizeof(why), "the vendor reads R%llu at the probe, past the %u registers its kernel allots",
                     reached, question->registers);
            clearing = msq_clearing(reached);
        }
        else if (overwritten >= 0)
        {
            snprintf(
                why, sizeof(why),
                "the vendor reads R%lld written at the probe, a register the code after it addresses memory through",
                overwritten);
        }
        else if ((body[0] != '\0') && msq_memory(body))
        {
            snprintf(
                why, sizeof(why),
                "the vendor reads a memory access at the probe, through registers the question sets no address in");
        }
        if ((why[0] == '\0') && (type == MSQ_READ))
        {
            // the probe read clear: a legal instruction that is no control transfer, names no register past the count,
            // overwrites no register the code addresses memory through and reaches no memory; reported to the teacher
            // with its encoding, whose conference admits it where the gate blocks its key unknown (overseer.h)
            msq_clear_reported(&verdicts, question, body, question->word);
        }
        if (why[0] == '\0')
        {
            continue;
        }
        gated_count += gated_question ? 0u : 1u;
        // the blocked encoding, the question's own code at its probe
        const unsigned char *code = NULL;
        unsigned long long code_size = 0ull;
        const int coded = qry_member_blob(question->code_name, &code, &code_size, NULL) &&
                          (question->place < (code_size / 16ull)) && (((question->place * 16ull) + 16ull) <= code_size);
        msq_gate_reported(&verdicts, question, coded ? (code + (question->place * 16ull)) : NULL, why, clearing);
    }
    const unsigned long long readings_count = accumulated_calls + atomic_load(&pool.calls);
    char head[256];
    const int head_length =
        snprintf(head, sizeof(head), "questions read: %zu in %llu readings; blocked from the part: %zu\n",
                 question_count, readings_count, gated_count);
    unsigned char *whole_report = NULL;
    size_t whole_report_size = 0u;
    size_t whole_report_room = 0u;
    msq_added(&whole_report, &whole_report_size, &whole_report_room, head, (size_t)head_length);
    if (verdicts.report_size != 0u)
    {
        msq_added(&whole_report, &whole_report_size, &whole_report_room, verdicts.report, verdicts.report_size);
    }
    qry_member_append(gated_name, verdicts.gated, verdicts.gated_size, QRY_ORDINARY);
    qry_member_append(report_name, whole_report, whole_report_size, QRY_ORDINARY);
    // the refusals the teacher reads, passed even where there are none, so that it knows this round was read
    if (gated_list_name != NULL)
    {
        qry_member_append(gated_list_name, verdicts.listed, verdicts.listed_size, QRY_ORDINARY);
    }
    // the probes read clear, passed even where there are none, so that the teacher knows the round was read
    if (read_name != NULL)
    {
        qry_member_append(read_name, verdicts.read, verdicts.read_size, QRY_ORDINARY);
    }
    printf("  questions read: %zu in %llu readings; the vendor blocks %zu from the part\n", question_count,
           readings_count, gated_count);
    printf("  the readings: %zu distinct probe encodings, %zu frames and %zu tails in one accumulated run read at %llu "
           "starts, %zu set aside and read alone; their streams %.2f s and the vendor %.2f s, each summed over the "
           "readers\n",
           place_words.count, frame_count, tails.count, accumulated_calls, aside_count,
           (double)atomic_load(&s_streamed_nanoseconds) / 1000000000.0,
           (double)atomic_load(&s_vendor_nanoseconds) / 1000000000.0);
    printf("  the gated list is %s and the report %s, blobs of %s\n", gated_name, report_name, qry_member_path());
    if (verdicts.report_size != 0u)
    {
        fwrite(verdicts.report, 1u, verdicts.report_size, stdout);
    }
    for (size_t code = 0u; code < whole_codes.count; code += 1u)
    {
        msq_frame_free(&whole_frames[code]);
    }
    for (size_t piece = 0u; piece < pieces.count; piece += 1u)
    {
        msq_frame_free(&piece_frames[piece]);
    }
    free(whole_frames);
    free(piece_frames);
    free(piece_of);
    msq_listing_free(&accumulated);
    msq_distinct_free(&place_words);
    msq_distinct_free(&whole_codes);
    msq_distinct_free(&tails);
    msq_distinct_free(&prefixes);
    msq_distinct_free(&pieces);
    // a frame the vendor named no stop in is no question it read: the round is not carried
    if (unread != 0u)
    {
        fprintf(stderr,
                "orior_measuring_stick_query: %zu questions' codes the vendor stopped in at no instruction it named\n",
                unread);
        return 2;
    }
    return (gated_count != 0u) ? 1 : 0;
}
