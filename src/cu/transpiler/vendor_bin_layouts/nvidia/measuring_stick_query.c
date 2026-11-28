// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// measuring_stick_query.c: the questions of the query, read against the own disassembler of the vendor. The input is
// the object that the carrier popped out under --diff-output-against-vendor. The gate passed some questions that it
// must block before a series reaches the part. Such a question has a probe instruction that the vendor reads as one of
// these:
//   - an illegal instruction;
//   - a control transfer that the operation key of our gate could not see;
//   - an instruction that names a register at or past the count that its kernel allots;
//   - an instruction that writes a register that the code after the probe addresses memory through;
//   - an access to memory of its own.
//
// Our gate judges by operation key and by the fields that a form learned. It cannot know that a whole encoding is
// illegal while it did not learn the operation that the encoding probes. It also cannot know where a register sits in a
// form whose fields it did not query. The reader of the vendor prints both.
//
// A register past the count faults the part at once, with an out-of-range register warp exception. A run of them takes
// the display and the machine down. A probe that overwrites an address that the kernel stores through faults the part
// with an illegal address. So does a probe that reads or writes memory through registers that hold no address from the
// question. Nothing here runs on the part.
//
//     orior_measuring_stick_query <question list> <nvdisasm> <architecture> <report> <gated> [<refusals> [<read>]]
//
// Every name but that of the disassembler is a blob of the run, in the .qry that QRY names (qry_member.h). The blobs
// are the list, the code of each question, and the container that the carrier popped for it as `<answers stem>.cubin`.
// The list has one question to a line as `<code> <registers> <cases> <answers> <probe> [<launches>]` (run_channel.h).
// Every container is the kernel that the system accepted, with the code of the question in it. The program reads the
// code of each, as the part would get it, out of the code section of the container.
//
// The vendor reads a raw run of encodings (nvdisasm --binary <architecture>) only from the path of a file. Thus the
// program passes each run to the writer of the run as a stream of the .qry (qry_streamed). It points the vendor at that
// stream. The writer is the only thing that writes a file, and everything that the vendor prints of a run is a blob of
// the run.
//
// The program reads each distinct encoding of a probe instruction once. It reads them in as many runs as the host has
// processors, each on its own reader thread, with the frames and tails. The vendor stops at the first instruction that
// it calls illegal and names its address. The program then puts that probe to a NOP and reads the run again. A long run
// reads in two parts, to the stop and past it. This continues until every run
// reads clean, and a clean run gives the operation of every legal probe at once.
//
// A stop that names no address splits the run in two, down to the one probe that it stops at (frames.c,
// msq_range_read). For each probe that the vendor reads whole, the program checks every register that it names against
// the `<registers>` of the question, with RZ aside. A register read as a pair (.64, .WIDE) reaches its second word, and
// one of .128 its fourth. The program reads the code past each probe to its first EXIT with no guard, every distinct
// one together as one raw run. It finds the registers that its memory operands read an address from. The gate blocks a
// probe that writes one of them, and a probe with a memory operand of its own.
//
// The program also reads each distinct frame of a question once and whole. That is its code with its probe put to a
// NOP, or its whole code where it holds no probe. It puts each instruction of it that the vendor calls
// illegal to a NOP, and reads the frame again. The gate blocks each such instruction, and each that the vendor reads
// with a register at or past the `<registers>` of the question. The gate blocks a whole code where it blocks any
// instruction of it (cubin_safe.c). A frame that the vendor stops in at no instruction that it names blocks its
// questions by no encoding.
//
// The report, passed as `<report>`, lists every blocked question. `<gated>` is the feedback that the carrier reads
// (--gated), with a line `<low> <high>` for each blocked encoding. The carrier blocks every code that holds it from the
// part.
//
// `<refusals>`, where the caller gives it, is the feedback that the teacher reads before the carry of the round
// (run_channel.h). It has a line `<answers stem> <registers> <why>` for each refusal. `<registers>` is the count of
// registers that clears a refusal for a register past the count that its kernel allots, at max MSQ_ALLOTTED_MAX. It is
// 0 where no count clears it.
//
// The vendor names no operation, and nothing that it reports enters a form. The gated list is only the exact words not
// to query. A count that clears a refusal changes how the program puts a question, and not what the question requires.
//
// The exit is 1 where the gate blocks any question. It is 2 where the program did not reach a blob, a tool or the run.
// It is also 2 where it read a frame at no stop that the vendor named. In each case the round does not carry
#include "../../../../../includes/parsers/line_read.h"
#include "../../../../../includes/platform/platform.h"
#include "../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "measuring_stick_query/msq_internal.h"
#include "memoria_operor/memoria_operor.h"

int program_main(int count, char **words)
{
    if ((count < 6) || (count > 8))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_measuring_stick_query <question list> <nvdisasm> <architecture> "
                                             "<report> <gated> [<refusals>[<read>]]");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
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
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_measuring_stick_query: no query writer runs: ");
            mmgr_line_put(&engine_note.line, QRY_ENVIRONMENT);
            mmgr_line_put(&engine_note.line, " names no .qry a writer has open");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const unsigned char *list = NULL;
    unsigned long long list_size = 0ULL;
    if (!qry_member_blob(list_name, &list, &list_size, NULL))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_measuring_stick_query: the run's buffer contains no ");
            mmgr_line_put(&engine_note.line, list_name);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    // the program reads a question with a probe by its probe instruction, among those of every other question. It reads
    // one with no probe, the own question of the kernel, whole, once for each code
    size_t lines = 0u;
    for (unsigned long long at = 0ULL; at < list_size; at += 1ULL)
    {
        lines += (list[at] == '\n') ? 1u : 0u;
    }
    MsqQuestion *const questions =
        (MsqQuestion *)platform_cell_cleared(lines + 1u, sizeof(MsqQuestion), platform_place_g_program_owner, 0);
    if (questions == NULL)
    {
        return 2;
    }
    size_t question_count = 0u;
    for (unsigned long long at = 0ULL; at < list_size;)
    {
        const unsigned char *const end = (const unsigned char *)EMBED_CALL(
            memor.chr, MemoriaCfg, .src = list + at, .bytes = (size_t)(list_size - at), .val = '\n');
        const size_t length = (end != NULL) ? (size_t)(end - (list + at)) : (size_t)(list_size - at);
        char line[1024];
        {
            VerbaLinea buffer_line = mmgr_line_over(line, sizeof(line));
            mmgr_line_put_max(&buffer_line, (const char *)(list + at), (size_t)((int)length));
            (void)mmgr_line_finish(&buffer_line);
        }
        at += (unsigned long long)length + 1ULL;
        char code_name[256];
        char cases_name[256];
        char answers_name[256];
        unsigned int registers = 0u;
        unsigned long long probe_place = 0ULL;
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
        {
            VerbaLinea buffer_line = mmgr_line_over(question->code_name, sizeof(question->code_name));
            mmgr_line_put(&buffer_line, code_name);
            (void)mmgr_line_finish(&buffer_line);
        }
        question->place = probe_place;
        question->registers = registers;
        question->placed =
            (probe_place < (question->code.size / 16ULL)) && (((probe_place * 16ULL) + 16ULL) <= question->code.size);
        if (question->placed)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = question->word,
                       .src = question->code.bytes + (probe_place * 16ULL), .bytes = 16u);
        }
        question_count += 1u;
    }
    // the distinct probe encodings, whole codes, codes before and past a probe, and frames, each read once. The frame
    // of a slotted question is the code before its probe, a NOP, and the code past it, as the two searches find them
    MsqDistinct place_words;
    MsqDistinct whole_codes;
    MsqDistinct tails;
    MsqDistinct prefixes;
    MsqDistinct pieces;
    size_t *const piece_of =
        (size_t *)platform_cell_cleared(question_count + 1u, sizeof(size_t), platform_place_g_program_owner, 0);
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
            const MsqBytes word = {question->word, 16ULL};
            question->word_index = msq_distinct_index(&place_words, &word);
            const MsqBytes tail = {question->code.bytes + ((question->place + 1ULL) * 16ULL),
                                   question->code.size - ((question->place + 1ULL) * 16ULL)};
            question->tail_index = msq_distinct_index(&tails, &tail);
            const MsqBytes prefix = {question->code.bytes, question->place * 16ULL};
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
    // The reads of the distinct probe encodings, and every frame read whole. A question with no probe has its whole
    // code as its frame. A slotted one has the code before its probe, a NOP and the code past it. The code past a probe
    // is the same in every question of one kernel. The program reads the encodings, the frames and the tails together
    // as one accumulated run at one start of the vendor (msq_accumulated_read). It sets aside a piece that the vendor
    // stops at, and reads it alone
    MsqReading *const readings = (MsqReading *)platform_cell_cleared(place_words.count + 1u, sizeof(MsqReading),
                                                                     platform_place_g_program_owner, 0);
    MsqFrame *const whole_frames =
        (MsqFrame *)platform_cell_cleared(whole_codes.count + 1u, sizeof(MsqFrame), platform_place_g_program_owner, 0);
    MsqFrame *const piece_frames =
        (MsqFrame *)platform_cell_cleared(pieces.count + 1u, sizeof(MsqFrame), platform_place_g_program_owner, 0);
    MsqRegisters *const addressed = (MsqRegisters *)platform_cell_cleared(tails.count + 1u, sizeof(MsqRegisters),
                                                                          platform_place_g_program_owner, 0);
    const size_t frame_count = whole_codes.count + pieces.count;
    MsqBytes *const framed =
        (MsqBytes *)platform_cell_cleared(frame_count + 1u, sizeof(MsqBytes), platform_place_g_program_owner, 0);
    MsqFrame *const frame_read =
        (MsqFrame *)platform_cell_cleared(frame_count + 1u, sizeof(MsqFrame), platform_place_g_program_owner, 0);
    unsigned char **const piece_bytes = (unsigned char **)platform_cell_cleared(
        pieces.count + 1u, sizeof(unsigned char *), platform_place_g_program_owner, 0);
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
    // distinct tails. The program sets aside a piece that the vendor stops at, and reads the run again without it until
    // it reads clean
    const size_t piece_count = place_words.count + frame_count + tails.count;
    MsqPiece *const run_pieces =
        (MsqPiece *)platform_cell_cleared(piece_count + 1u, sizeof(MsqPiece), platform_place_g_program_owner, 0);
    size_t *const aside =
        (size_t *)platform_cell_cleared(piece_count + 1u, sizeof(size_t), platform_place_g_program_owner, 0);
    whole = whole && (run_pieces != NULL) && (aside != NULL);
    size_t placed = 0u;
    for (size_t word = 0u; whole && (word < place_words.count); word += 1u)
    {
        run_pieces[placed] = (MsqPiece){MSQ_PIECE_WORD, word, place_words.items[word], 0ULL, MSQ_PIECE_READ};
        placed += 1u;
    }
    for (size_t frame = 0u; whole && (frame < frame_count); frame += 1u)
    {
        run_pieces[placed] = (MsqPiece){MSQ_PIECE_FRAME, frame, framed[frame], 0ULL, MSQ_PIECE_READ};
        placed += 1u;
    }
    for (size_t tail = 0u; whole && (tail < tails.count); tail += 1u)
    {
        run_pieces[placed] = (MsqPiece){MSQ_PIECE_TAIL, tail, tails.items[tail], 0ULL, MSQ_PIECE_READ};
        placed += 1u;
    }
    // the disassembler of the vendor stops at a piece with an instruction that its own instruction set reads as no
    // class. The program sets that piece aside before the run and reads it alone. Thus the run reads clean at one start
    for (size_t piece = 0u; whole && msq_tables_read() && (piece < piece_count); piece += 1u)
    {
        run_pieces[piece].state = msq_vendor_reads(&run_pieces[piece].bytes) ? MSQ_PIECE_READ : MSQ_PIECE_UNREAD;
    }
    MsqReader reader;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &reader, .bytes = sizeof(reader), .val = 0);
    reader.output_room = 8ULL << 20u;
    reader.output =
        whole ? (char *)platform_cell_taken((size_t)reader.output_room, platform_place_g_program_owner, 0) : NULL;
    // the accumulated run's listing, which every reading and frame of it points into until the cross ends
    MsqListing accumulated;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &accumulated, .bytes = sizeof(accumulated), .val = 0);
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
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &pool, .bytes = sizeof(pool), .val = 0);
    pool.pieces = run_pieces;
    pool.aside = aside;
    pool.aside_count = aside_count;
    pool.readings = readings;
    pool.frame_read = frame_read;
    pool.addressed = addressed;
    const unsigned int processors = msq_processors();
    const unsigned int threads = (aside_count < processors) ? (unsigned int)aside_count : processors;
    pthread_t *const started =
        (pthread_t *)platform_cell_cleared((size_t)threads + 1u, sizeof(pthread_t), platform_place_g_program_owner, 0);
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
    platform_cell_released(started, platform_place_g_program_owner);
    whole = whole && (started != NULL) && !atomic_load(&pool.failed);
    const unsigned long long accumulated_calls = reader.calls;
    platform_cell_released(reader.output, platform_place_g_program_owner);
    platform_cell_released(reader.raw, platform_place_g_program_owner);
    platform_cell_released(run_pieces, platform_place_g_program_owner);
    platform_cell_released(aside, platform_place_g_program_owner);
    for (size_t code = 0u; code < whole_codes.count; code += 1u)
    {
        whole_frames[code] = frame_read[code];
    }
    for (size_t piece = 0u; piece < pieces.count; piece += 1u)
    {
        piece_frames[piece] = frame_read[whole_codes.count + piece];
        platform_cell_released(piece_bytes[piece], platform_place_g_program_owner);
    }
    platform_cell_released(piece_bytes, platform_place_g_program_owner);
    platform_cell_released(framed, platform_place_g_program_owner);
    platform_cell_released(frame_read, platform_place_g_program_owner);
    if (!whole)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_measuring_stick_query: the vendor's disassembler ");
            mmgr_line_put(&engine_note.line, s_nvdisasm);
            mmgr_line_put(&engine_note.line, " could not be run, or there was no memory: ");
            mmgr_line_put(&engine_note.line, (pool.why != NULL) ? pool.why : "the work could not be queued");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    // the blocked questions, each with the reason, the gated list and the report passed to the run. The gate blocks a
    // question by its probe where the vendor reads the probe as below. It blocks it by its frame where the vendor calls
    // an instruction of it illegal. It also blocks it where the vendor reads an instruction with a register at or past
    // the count that its kernel allots. The gate blocks the encoding of each such instruction, and every code that
    // holds it. A frame that the vendor stopped in at no instruction that it named blocks its questions by no encoding,
    // and its round does not carry
    MsqVerdicts verdicts;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &verdicts, .bytes = sizeof(verdicts), .val = 0);
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
            {
                VerbaLinea buffer_line = mmgr_line_over(why, sizeof(why));
                mmgr_line_put(&buffer_line, "the vendor reads an illegal instruction at byte ");
                mmgr_line_decimal(&buffer_line, frame->illegal[at], 1u);
                mmgr_line_put(&buffer_line, " of the code");
                (void)mmgr_line_finish(&buffer_line);
            }
            msq_gate_reported(&verdicts, question, question->code.bytes + frame->illegal[at], why, 0u);
            gated_question = 1;
        }
        for (size_t line = 0u; line < frame->listing.count; line += 1u)
        {
            const MsqLine *const read_line = &frame->listing.lines[line];
            unsigned long long frame_reached = 0ULL;
            const int in_code = (read_line->offset + 16ULL) <= question->code.size;
            if (in_code && msq_register_reached(read_line->body, question->registers, &frame_reached))
            {
                char why[200];
                {
                    VerbaLinea buffer_line = mmgr_line_over(why, sizeof(why));
                    mmgr_line_put(&buffer_line, "the vendor reads R");
                    mmgr_line_decimal(&buffer_line, frame_reached, 1u);
                    mmgr_line_put(&buffer_line, " at byte ");
                    mmgr_line_decimal(&buffer_line, read_line->offset, 1u);
                    mmgr_line_put(&buffer_line, " of the code, past the ");
                    mmgr_line_decimal(&buffer_line, question->registers, 1u);
                    mmgr_line_put(&buffer_line, " registers its kernel allots");
                    (void)mmgr_line_finish(&buffer_line);
                }
                msq_gate_reported(&verdicts, question, question->code.bytes + read_line->offset, why,
                                  msq_clearing(frame_reached));
                gated_question = 1;
            }
            else if (in_code && !question->placed && !msq_control_read(read_line->body))
            {
                // an instruction of a whole question read clear. It is legal and within the registers that its kernel
                // allots, and no control transfer but the EXIT at the end of a kernel. The program reports it to the
                // teacher with its encoding. The gate calls a conference on it where it blocks the unknown key of the
                // instruction (overseer.h)
                msq_clear_reported(&verdicts, question, read_line->body, question->code.bytes + read_line->offset);
            }
        }
        gated_count += gated_question ? 1u : 0u;
        if (!question->placed)
        {
            continue;
        }
        const MsqReading *const reading = &readings[question->word_index];
        const MsqType type = reading->type;
        const char *const body = ((type == MSQ_READ) && (reading->body != NULL)) ? reading->body : "";
        char why[256] = "";
        unsigned int clearing = 0u;
        unsigned long long reached = 0ULL;
        MsqRegisters written;
        msq_registers_written(body, &written);
        long long overwritten = -1;
        if (question->placed)
        {
            const MsqRegisters *const read_from = &addressed[question->tail_index];
            for (unsigned int number = 0u; (overwritten < 0) && (number < MSQ_REGISTERS); number += 1u)
            {
                if ((written.bits[number / 64u] & read_from->bits[number / 64u] & (1ULL << (number % 64u))) != 0ULL)
                {
                    overwritten = (long long)number;
                }
            }
        }
        int control = 0;
        for (size_t at = 0u; (type == MSQ_READ) && (at < (sizeof(s_control) / sizeof(s_control[0]))); at += 1u)
        {
            control |= (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = reading->operation,
                                   .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = s_control[at], .ci = EMBED_FALSE));
        }
        if (type == MSQ_ILLEGAL)
        {
            VerbaLinea buffer_line = mmgr_line_over(why, sizeof(why));
            mmgr_line_put(&buffer_line, s_illegal_why);
            (void)mmgr_line_finish(&buffer_line);
        }
        else if (control)
        {
            VerbaLinea buffer_line = mmgr_line_over(why, sizeof(why));
            mmgr_line_put(&buffer_line, "the vendor reads ");
            mmgr_line_put(&buffer_line, reading->operation);
            mmgr_line_put(&buffer_line, " at the probe, a control transfer");
            (void)mmgr_line_finish(&buffer_line);
        }
        else if (msq_register_reached(body, question->registers, &reached))
        {
            {
                VerbaLinea buffer_line = mmgr_line_over(why, sizeof(why));
                mmgr_line_put(&buffer_line, "the vendor reads R");
                mmgr_line_decimal(&buffer_line, reached, 1u);
                mmgr_line_put(&buffer_line, " at the probe, past the ");
                mmgr_line_decimal(&buffer_line, question->registers, 1u);
                mmgr_line_put(&buffer_line, " registers its kernel allots");
                (void)mmgr_line_finish(&buffer_line);
            }
            clearing = msq_clearing(reached);
        }
        else if (overwritten >= 0)
        {
            VerbaLinea buffer_line = mmgr_line_over(why, sizeof(why));
            mmgr_line_put(&buffer_line, "the vendor reads R");
            mmgr_line_decimal_signed(&buffer_line, overwritten);
            mmgr_line_put(&buffer_line, " written at the probe, a register the code after it addresses memory through");
            (void)mmgr_line_finish(&buffer_line);
        }
        else if ((body[0] != '\0') && msq_memory(body))
        {
            VerbaLinea buffer_line = mmgr_line_over(why, sizeof(why));
            mmgr_line_put(
                &buffer_line,
                "the vendor reads a memory access at the probe, through registers the question sets no address in");
            (void)mmgr_line_finish(&buffer_line);
        }
        if ((why[0] == '\0') && (type == MSQ_READ))
        {
            // the probe read clear: a legal instruction that is no control transfer and names no register past the
            // count. It overwrites no register that the code addresses memory through, and reaches no memory. The
            // program reports it to the teacher with its encoding. Its conference admits it where the gate blocks its
            // unknown key (overseer.h)
            msq_clear_reported(&verdicts, question, body, question->word);
        }
        if (why[0] == '\0')
        {
            continue;
        }
        gated_count += gated_question ? 0u : 1u;
        // the blocked encoding, the question's own code at its probe
        const unsigned char *code = NULL;
        unsigned long long code_size = 0ULL;
        const int coded = qry_member_blob(question->code_name, &code, &code_size, NULL) &&
                          (question->place < (code_size / 16ULL)) && (((question->place * 16ULL) + 16ULL) <= code_size);
        msq_gate_reported(&verdicts, question, coded ? (code + (question->place * 16ULL)) : NULL, why, clearing);
    }
    const unsigned long long readings_count = accumulated_calls + atomic_load(&pool.calls);
    char head[256];
    VerbaLinea formed_line_1 = mmgr_line_over(head, sizeof(head));
    mmgr_line_put(&formed_line_1, "questions read: ");
    mmgr_line_decimal(&formed_line_1, question_count, 1u);
    mmgr_line_put(&formed_line_1, " in ");
    mmgr_line_decimal(&formed_line_1, readings_count, 1u);
    mmgr_line_put(&formed_line_1, " readings; blocked from the part: ");
    mmgr_line_decimal(&formed_line_1, gated_count, 1u);
    mmgr_line_put(&formed_line_1, "\n");
    (void)mmgr_line_finish(&formed_line_1);
    const int head_length = (int)formed_line_1.at;
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
    // the refusals that the teacher reads, passed even where there are none. Thus the teacher knows that the program
    // read this round
    if (gated_list_name != NULL)
    {
        qry_member_append(gated_list_name, verdicts.listed, verdicts.listed_size, QRY_ORDINARY);
    }
    // the probes read clear, passed even where there are none. Thus the teacher knows that the program read the round
    if (read_name != NULL)
    {
        qry_member_append(read_name, verdicts.read, verdicts.read_size, QRY_ORDINARY);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  questions read: ");
        mmgr_line_decimal(output_line, question_count, 1u);
        mmgr_line_put(output_line, " in ");
        mmgr_line_decimal(output_line, readings_count, 1u);
        mmgr_line_put(output_line, " readings; the vendor blocks ");
        mmgr_line_decimal(output_line, gated_count, 1u);
        mmgr_line_put(output_line, " from the part");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the readings: ");
        mmgr_line_decimal(output_line, place_words.count, 1u);
        mmgr_line_put(output_line, " distinct probe encodings, ");
        mmgr_line_decimal(output_line, frame_count, 1u);
        mmgr_line_put(output_line, " frames and ");
        mmgr_line_decimal(output_line, tails.count, 1u);
        mmgr_line_put(output_line, " tails in one accumulated run read at ");
        mmgr_line_decimal(output_line, accumulated_calls, 1u);
        mmgr_line_put(output_line, " starts, ");
        mmgr_line_decimal(output_line, aside_count, 1u);
        mmgr_line_put(output_line, " set aside and read alone; their streams ");
        mmgr_line_fixed(output_line, (double)((double)atomic_load(&s_streamed_nanoseconds) / 1000000000.0), 2u, 0u,
                        EMBED_FALSE);
        mmgr_line_put(output_line, " s and the vendor ");
        mmgr_line_fixed(output_line, (double)((double)atomic_load(&s_vendor_nanoseconds) / 1000000000.0), 2u, 0u,
                        EMBED_FALSE);
        mmgr_line_put(output_line, " s, each summed over the readers");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the gated list is ");
        mmgr_line_put(output_line, gated_name);
        mmgr_line_put(output_line, " and the report ");
        mmgr_line_put(output_line, report_name);
        mmgr_line_put(output_line, ", blobs of ");
        mmgr_line_put(output_line, qry_member_path());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (verdicts.report_size != 0u)
    {
        VerbaLinea *const output_line = platform_line_out();
        (void)mmgr_line_put_n(output_line, (const char *)(verdicts.report),
                              (size_t)(1u) * (size_t)(verdicts.report_size));
        platform_line_out_ended();
    }
    for (size_t code = 0u; code < whole_codes.count; code += 1u)
    {
        msq_frame_free(&whole_frames[code]);
    }
    for (size_t piece = 0u; piece < pieces.count; piece += 1u)
    {
        msq_frame_free(&piece_frames[piece]);
    }
    platform_cell_released(whole_frames, platform_place_g_program_owner);
    platform_cell_released(piece_frames, platform_place_g_program_owner);
    platform_cell_released(piece_of, platform_place_g_program_owner);
    msq_listing_free(&accumulated);
    msq_distinct_free(&place_words);
    msq_distinct_free(&whole_codes);
    msq_distinct_free(&tails);
    msq_distinct_free(&prefixes);
    msq_distinct_free(&pieces);
    // a frame that the vendor named no stop in is no question that it read, and the round does not carry
    if (unread != 0u)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "orior_measuring_stick_query: ");
            mmgr_line_decimal(&engine_note.line, unread, 1u);
            mmgr_line_put(&engine_note.line, " questions' codes the vendor stopped in at no instruction it named");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    return (gated_count != 0u) ? 1 : 0;
}
