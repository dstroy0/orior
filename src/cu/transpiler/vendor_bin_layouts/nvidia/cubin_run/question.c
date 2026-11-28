// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// question.c: the question: the safety word positioned on the code, a refusal's cause received and reported, the fault
// recorded, the account given, one question carried, and the verdict loaded
#include "../../../../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "cubin_run_internal.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_linea.h"

// a part of a driver's error name and the cause a conference reads it as (overseer.h)
typedef struct
{
    const char *part;
    const char *cause;
} CubinRunCauseName;

// one word of an instruction in `code` at `byte`, read and written back
static unsigned long long cubin_run_word(const unsigned char *code, unsigned int byte)
{
    unsigned long long value = 0ULL;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        value |= (unsigned long long)code[byte + at] << (8u * at);
    }
    return value;
}

static void cubin_run_word_set(unsigned char *code, unsigned int byte, unsigned long long value)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        code[byte + at] = (unsigned char)((value >> (8u * at)) & 0xffULL);
    }
}

// the bits `first` to `last` of the instruction `low` and `high` make set to `value`
static void cubin_run_bits_set(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last,
                               unsigned long long value)
{
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        unsigned long long *const word = (bit < 64u) ? low : high;
        const unsigned long long one = (value >> (bit - first)) & 1ULL;
        *word = (*word & ~(1ULL << (bit % 64u))) | (one << (bit % 64u));
    }
}

// The safety word, applied to `code` before the part sees it (P9). Every instruction waits on all six barriers. The
// probe at `probe_place`, where the code holds one, stalls the longest and uses no barrier. A form that the part did
// not name then waits on all, and the wait after a barrier that nothing releases never ends. The protocol sends the
// code as the part accepted it, and names the probe. The carrier sets the own word of the scheduler
static void cubin_run_safe_word(unsigned char *code, unsigned long long code_size, unsigned int probe_place)
{
    const unsigned int places = (unsigned int)(code_size / 16ULL);
    for (unsigned int place = 0u; place < places; place += 1u)
    {
        unsigned long long low = cubin_run_word(code, place * 16u);
        unsigned long long high = cubin_run_word(code, (place * 16u) + 8u);
        cubin_run_bits_set(&low, &high, SASS_WAIT_FIRST, SASS_WAIT_FIRST + 5u, SASS_WAIT_EVERY);
        if (place == probe_place)
        {
            cubin_run_bits_set(&low, &high, SASS_STALL_FIRST, SASS_STALL_FIRST + 3u, SASS_STALL_MAX_LEN);
            cubin_run_bits_set(&low, &high, SASS_WRITE_BARRIER_FIRST, SASS_WRITE_BARRIER_FIRST + 2u, SASS_BARRIER_NONE);
            cubin_run_bits_set(&low, &high, SASS_READ_BARRIER_FIRST, SASS_READ_BARRIER_FIRST + 2u, SASS_BARRIER_NONE);
        }
        cubin_run_word_set(code, place * 16u, low);
        cubin_run_word_set(code, (place * 16u) + 8u, high);
    }
}

// The errors of the driver, by the cause that each reads as. The cause is the own encoding of the instruction where
// the part did not take the code or run it. It is an address that the code reached, or a register past what the kernel
// allots. It is a launch that did not return, or an exception that names none of those
static const CubinRunCauseName s_cause_names[] = {{"ILLEGAL_INSTRUCTION", "instruction"},
                                                  {"INVALID_PC", "instruction"},
                                                  {"INVALID_IMAGE", "instruction"},
                                                  {"INVALID_SOURCE", "instruction"},
                                                  {"NO_BINARY_FOR_GPU", "instruction"},
                                                  {"INVALID_PTX", "instruction"},
                                                  {"ADDRESS", "address"},
                                                  {"OUT_OF_RESOURCES", "register"},
                                                  {"TIMEOUT", "time"},
                                                  {"LAUNCH_FAILED", "exception"},
                                                  {"HARDWARE_STACK_ERROR", "exception"},
                                                  {"ASSERT", "exception"}};

// the cause the driver's error `name` reads as, NULL where it reads none
static const char *cubin_run_cause_named(const char *name)
{
    for (size_t at = 0u; (name != NULL) && (at < (sizeof(s_cause_names) / sizeof(s_cause_names[0]))); at += 1u)
    {
        if (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = s_cause_names[at].part, .other_cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .ci = EMBED_FALSE) != NULL)
        {
            return s_cause_names[at].cause;
        }
    }
    return NULL;
}

// The cause that the Xid critical errors that the part stamped read as. First comes the own text of the driver for
// one, where the log of the system records it. A register out of range reads as a register, and an illegal
// instruction as the instruction. An address out of range, or a fault of the memory map, reads as an address.
//
// Then comes its number. A fault of the memory map of the part (31) reads as an address. A channel that stopped (8),
// or a context switch past its time (109), reads as a launch that did not return. An exception of the graphics engine
// (13) or of its class (69) reads as an exception. The return is the first of those, in that order, that the errors
// carry, or NULL where they carry none
static const char *cubin_run_listener_cause(void)
{
    static const char *const s_driver_messages[] = {"Out Of Range Register", "Illegal Instruction",
                                                    "Out Of Range Address", "MMU Fault"};
    static const char *const s_driver_causes[] = {"register", "instruction", "address", "address"};
    for (size_t message = 0u; message < (sizeof(s_driver_messages) / sizeof(s_driver_messages[0])); message += 1u)
    {
        for (unsigned int place = 0u; (place < s_xid_count) && (place < CUBIN_RUN_XID_MAX); place += 1u)
        {
            if (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = s_xid_text[place], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = s_driver_messages[message], .other_cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .ci = EMBED_FALSE) != NULL)
            {
                return s_driver_causes[message];
            }
        }
    }
    static const unsigned long long s_listener_events[] = {31ULL, 8ULL, 109ULL, 13ULL, 69ULL};
    static const char *const s_listener_causes[] = {"address", "time", "time", "exception", "exception"};
    for (size_t event = 0u; event < (sizeof(s_listener_events) / sizeof(s_listener_events[0])); event += 1u)
    {
        for (unsigned int place = 0u; (place < s_xid_count) && (place < CUBIN_RUN_XID_MAX); place += 1u)
        {
            if (s_xid_data[place] == s_listener_events[event])
            {
                return s_listener_causes[event];
            }
        }
    }
    return NULL;
}

// The probe instruction `low` `high` of a launch that faulted the part, added to CUBIN_RUN_FAULTS of the run with the
// cause that the BAR0 driver received. The call passes it as critical, so that it is on disk before the carrier goes
// on. It also blocks it from the part in this process at once
static void cubin_run_faulted_stored(unsigned long long low, unsigned long long high, const char *cause)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    VerbaLinea text = {s_text_bytes, sizeof(s_text_bytes), 0ULL};
    if (qry_member_blob(CUBIN_RUN_FAULTS, &bytes, &size, NULL))
    {
        mmgr_line_put_n(&text, (const char *)bytes, size);
    }
    mmgr_line_hex(&text, (unsigned long long)(low), 16u);
    mmgr_line_put(&text, " ");
    mmgr_line_hex(&text, (unsigned long long)(high), 16u);
    mmgr_line_put(&text, " ");
    mmgr_line_put(&text, (cause != NULL) ? cause : "-");
    mmgr_line_put(&text, "\n");
    if (mmgr_line_ok(&text))
    {
        qry_member_append(CUBIN_RUN_FAULTS, text.out, text.at, QRY_CRITICAL);
    }
    char line[48];
    {
        VerbaLinea buffer_line = mmgr_line_over(line, sizeof(line));
        mmgr_line_hex(&buffer_line, low, 16u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_hex(&buffer_line, high, 16u);
        mmgr_line_put(&buffer_line, "\n");
        (void)mmgr_line_finish(&buffer_line);
    }
    cubin_safe_verdict_added(line,
                             EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR));
}

// The account `call` contains written in the answer `answers` too, and passed as a panic of the call where `panicked`
// and the run's writer runs. Let go either way
static void cubin_run_account_given(VerbaLinea *answers, VerbaLinea *call, int panicked)
{
    // an account is a line, far below what an int counts
    mmgr_line_put_n(answers, call->out, call->at);
    if (panicked)
    {
        cubin_run_member_written(call, QRY_PANIC_CALL, QRY_PANIC, 0);
    }
    call->at = 0u;
}

// A launch that the part refused. The call writes its accounts in its answer `answers` after the refusal, where the
// conference that the refusal calls reads them (overseer.h). The accounts are these:
//
// - the account of the proctor, the own account of the carrier, of the launch that it carried
// - the account of the BAR0 driver, the fault telemetry of the part, of the Xid critical errors that the part stamped
//   against it
// - the account of the gate, of the code that it passed, its verdict, the gated list and the probe instruction against
//   it
//
// The proctor and the BAR0 driver each end in the cause that they read, where they read one. The carrier can lose its
// mind: a launch faulted the part, or hung past its bound. Then the call also passes each account as a panic of the
// call (QRY_PANIC_CALL) to the run. That opens the line that every party of the run sees. Nothing waits on the disk,
// and the carrier goes on from the part at once
void cubin_run_refusal_written(VerbaLinea *answers, int panicked, const char *code_path, unsigned long long code_size,
                               unsigned int registers, unsigned int probe_place, unsigned int launches,
                               const CubinRunShape *shape, const char *status_name, CubinRunStatus status,
                               unsigned int verdict, unsigned long long at)
{
    VerbaLinea *const call = &s_call_line;
    call->at = 0u;
    const char *const proctor_cause = cubin_run_cause_named(status_name);
    mmgr_line_put(call, "proctor: the launch of ");
    mmgr_line_put(call, code_path);
    mmgr_line_put(call, ", ");
    mmgr_line_decimal(call, (unsigned long long)(code_size), 0u);
    mmgr_line_put(call, " bytes of code, ");
    mmgr_line_decimal(call, (unsigned long long)(registers), 0u);
    mmgr_line_put(call, " registers, ");
    mmgr_line_decimal(call, (unsigned long long)(shape->threads), 0u);
    mmgr_line_put(call, " threads in ");
    mmgr_line_decimal(call, (unsigned long long)(shape->blocks), 0u);
    mmgr_line_put(call, " blocks over ");
    mmgr_line_decimal(call, (unsigned long long)(s_case_count), 0u);
    mmgr_line_put(call, " cases, ");
    mmgr_line_decimal(call, (unsigned long long)(launches), 0u);
    mmgr_line_put(call, " launches timed after it, ");
    mmgr_line_put(call, s_launch_faulted     ? "faulted the part"
                        : s_launch_timed_out ? "hung past its bound"
                                             : "was refused, the part left whole");
    mmgr_line_put(call, ": the driver's status ");
    mmgr_line_put(call, (status_name != NULL) ? status_name : "with no name");
    mmgr_line_put(call, " (");
    mmgr_line_decimal_signed(call, (long long)(status));
    mmgr_line_put(call, ")");
    mmgr_line_put(call, (proctor_cause != NULL) ? "; cause " : "");
    mmgr_line_put(call, (proctor_cause != NULL) ? proctor_cause : "");
    mmgr_line_put(call, "\n");
    cubin_run_account_given(answers, call, panicked);
    const char *const listener_cause = cubin_run_listener_cause();
    if (!s_telemetry_ready && !s_events_ready)
    {
        mmgr_line_put(call, "listener: the part's fault telemetry was not reached this run, and received nothing\n");
    }
    else if (s_xid_count == 0u)
    {
        mmgr_line_put(call, "listener: the part stamped no Xid critical error against the launch\n");
    }
    else
    {
        mmgr_line_put(call, "listener: the part stamped ");
        mmgr_line_decimal(call, (unsigned long long)(s_xid_count), 0u);
        mmgr_line_put(call, " Xid critical errors against the launch, Xid");
        for (unsigned int place = 0u; (place < s_xid_count) && (place < CUBIN_RUN_XID_MAX); place += 1u)
        {
            mmgr_line_put(call, " ");
            mmgr_line_decimal(call, (unsigned long long)(s_xid_data[place]), 0u);
            if (s_xid_text[place][0] != '\0')
            {
                mmgr_line_put(call, " (");
                mmgr_line_put(call, s_xid_text[place]);
                mmgr_line_put(call, ")");
            }
        }
        mmgr_line_put(call, (s_xid_count > CUBIN_RUN_XID_MAX) ? " ..." : "");
        mmgr_line_put(call, (listener_cause != NULL) ? "; cause " : "");
        mmgr_line_put(call, (listener_cause != NULL) ? listener_cause : "");
        mmgr_line_put(call, "\n");
    }
    cubin_run_account_given(answers, call, panicked);
    // the probe instruction as the gate read it, the safety word positioned on, where the code contains a probe
    const int placed = (((unsigned long long)probe_place * 16ULL) + 16ULL) <= code_size;
    const unsigned long long low = placed ? cubin_run_word(s_code, probe_place * 16u) : 0ULL;
    const unsigned long long high = placed ? cubin_run_word(s_code, (probe_place * 16u) + 8u) : 0ULL;
    mmgr_line_put(call, "gate: the code read ");
    mmgr_line_put(call, cubin_safe_name(verdict));
    if (verdict != CUBIN_SAFE)
    {
        mmgr_line_put(call, " at byte ");
        mmgr_line_decimal(call, (unsigned long long)(at), 0u);
    }
    mmgr_line_put(call, ", its register fields bound by ");
    mmgr_line_decimal(call, (unsigned long long)(registers), 0u);
    mmgr_line_put(call, "; the gated list ");
    mmgr_line_put(call, cubin_safe_verdict_ready() ? "loaded" : "not loaded");
    if (placed)
    {
        mmgr_line_put(call, "; the probe ");
        mmgr_line_decimal(call, (unsigned long long)(probe_place), 0u);
        mmgr_line_put(call, ", ");
        mmgr_line_hex(call, (unsigned long long)(low), 16u);
        mmgr_line_put(call, " ");
        mmgr_line_hex(call, (unsigned long long)(high), 16u);
        mmgr_line_put(call, ", ");
        mmgr_line_put(call, cubin_safe_verdict_gated(low, high) ? "in the gated list" : "not in the gated list");
    }
    else
    {
        mmgr_line_put(call, "; the code contains no probe");
    }
    mmgr_line_put(call, "\n");
    cubin_run_account_given(answers, call, panicked);
    if (s_launch_faulted && placed)
    {
        cubin_run_faulted_stored(low, high, listener_cause);
    }
}

// One question carried. Its code at `code_path` goes in the container of `pattern_size` bytes in s_pattern. Its kernel
// is `kernel`, which declares `registers`, over the cases at `cases_path`. The launch is as `shape` specifies where
// `shaped` is not 0, and the call times it over `launches` where that is not 0. Its line goes to `answers_path`.
//
// The return is 0 where the call wrote a line. It is 1 where the line is the refusal of a launch by the driver or the
// part, and the context then answers no other question. It is 2 where the call did not reach the inputs, the machine
// or the driver
int cubin_run_question(const char *kernel, unsigned long long pattern_size, const char *code_path,
                       unsigned int registers, const char *cases_path, const char *answers_path,
                       unsigned int probe_place, unsigned int launches, CubinRunShape shape, int shaped)
{
    VerbaLinea *const answers = &s_answers_line;
    answers->at = 0u;
    s_part_registers = CUBIN_RUN_REGISTERS_UNREAD;
    const unsigned long long input_began = cubin_run_host_now();
    const unsigned long long query_key = cubin_run_query_key(code_path);
    const unsigned long long code_size = cubin_run_input(code_path, s_code, sizeof(s_code));
    const int cased = (code_size != 0ULL) && cubin_run_cases(cases_path);
    s_clocks.input += cubin_run_host_now() - input_began;
    if ((pattern_size == 0ULL) || !cased)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "the container, the code or the cases did not read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    // a question that names no shape gets blocks of CUBIN_RUN_THREADS, enough to give every case a thread. A launch
    // gives every case a thread, and gives a case each to no more threads than it sizes them for
    shape.blocks = shaped ? shape.blocks : ((s_case_count + CUBIN_RUN_THREADS - 1u) / CUBIN_RUN_THREADS);
    const unsigned long long threads = (unsigned long long)shape.threads * shape.blocks;
    if ((threads < s_case_count) || (threads > CUBIN_RUN_THREADS_MAX))
    {
        mmgr_line_put(answers, "skipped a launch of ");
        mmgr_line_decimal(answers, (unsigned long long)(shape.threads), 0u);
        mmgr_line_put(answers, " threads in ");
        mmgr_line_decimal(answers, (unsigned long long)(shape.blocks), 0u);
        mmgr_line_put(answers, " blocks for ");
        mmgr_line_decimal(answers, (unsigned long long)(s_case_count), 0u);
        mmgr_line_put(answers, " cases\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // the call sets the own word of the scheduler here, before the gate and the part. The protocol sent the code as
    // the part accepted it, and named the probe
    const unsigned long long gate_began = cubin_run_host_now();
    cubin_run_safe_word(s_code, code_size, probe_place);
    CubinWrite written;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &written, .bytes = sizeof(written), .val = 0);
    written.pattern = s_pattern;
    written.pattern_size = pattern_size;
    written.kernel = kernel;
    written.code = s_code;
    written.code_size = code_size;
    written.registers = registers;
    written.exit_count = cubin_exits_find(s_code, code_size, sass_exit_encoding(&s_machine), s_exits, CUBIN_RUN_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ULL;
    if (!cubin_write(&written, s_container, sizeof(s_container), &size))
    {
        mmgr_line_put(answers, "skipped not written\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // the host reads the code before the driver sees it. The call answers code that breaks a rule of cubin_safe
    // without the part. The count that the kernel of this question allots bounds its register fields (rule 6)
    unsigned long long at = 0ULL;
    const unsigned int verdict = cubin_safe_image(&s_machine, s_container, size, registers, &at);
    const unsigned long long row_began = cubin_run_host_now();
    s_clocks.gate += row_began - gate_began;
    cubin_run_verdict_row(query_key, verdict, at, s_dry);
    s_clocks.passed += cubin_run_host_now() - row_began;
    if (s_dry)
    {
        if (s_facets & CUBIN_FACET_VENDOR)
        {
            cubin_run_container_out(answers_path, s_container, size);
        }
        cubin_run_dry_written(code_size, registers, launches, &shape, verdict, at);
    }
    if (verdict != CUBIN_SAFE)
    {
        mmgr_line_put(answers, "skipped ");
        mmgr_line_decimal(answers, (unsigned long long)(verdict), 0u);
        mmgr_line_put(answers, " ");
        mmgr_line_put(answers, cubin_safe_name(verdict));
        mmgr_line_put(answers, " at byte ");
        mmgr_line_decimal(answers, (unsigned long long)(at), 0u);
        mmgr_line_put(answers, "\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // a dry run stops here, before the driver. The call answers the question as one that the gate blocked from the
    // part
    if (s_dry)
    {
        mmgr_line_put(answers, "skipped dry, the part taken out\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // the safety airlock: a live run reaches the part only with the verdict of the vendor in hand (--gated). Without
    // it, nothing read this code for a fault, and an unread encoding can fault the part and crash the machine. The call
    // blocks it from the part here, and the cross must read it first
    if (!cubin_safe_verdict_ready())
    {
        mmgr_line_put(answers, "skipped no vendor verdict, the cross must read this code before the part\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // cubin_safe_image above already caught a probe instruction that the reader of the vendor blocked from the part,
    // where the gate has the verdict (cubin_safe_verdict_gated). The gate blocks the whole code, not the probe alone
    if (!s_driver_opened)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &s_driver, .bytes = sizeof(s_driver), .val = 0);
        if (!cubin_run_driver(&s_driver))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "the driver was not reached");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 2;
        }
        // the fault telemetry of the part, opened beside the driver, for rule 8 to read after each launch. Where it is
        // not there, a run goes on under rule 7 alone, and prints the warning
        cubin_run_telemetry_open();
        s_driver_opened = 1;
    }
    unsigned long long nanoseconds = 0ULL;
    const CubinRunStatus status = cubin_run_launch(&s_driver, kernel, &shape, launches, &nanoseconds);
    if (status != 0)
    {
        const char *name = NULL;
        s_driver.error_name(status, &name);
        char refused[160];
        {
            VerbaLinea buffer_line = mmgr_line_over(refused, sizeof(refused));
            mmgr_line_put(&buffer_line, "refused ");
            mmgr_line_put(&buffer_line, (name != NULL) ? name : "CUDA_ERROR_LAUNCH_FAILED");
            mmgr_line_put(&buffer_line, s_launch_faulted ? " the part faulted" : "");
            (void)mmgr_line_finish(&buffer_line);
        }
        mmgr_line_put(answers, refused);
        mmgr_line_put(answers, "\n");
        const unsigned int outcome = s_launch_timed_out ? CUBIN_RUN_TIMED_OUT
                                     : s_launch_faulted ? CUBIN_RUN_FAULTED
                                                        : CUBIN_RUN_REFUSED;
        cubin_run_launch_row(query_key, outcome, launches, 0ULL, 0ULL, 0ULL, refused);
        // the carrier can lose its mind: a launch faulted the part, or hung past its bound. That is a panic. The
        // program opens the call with the account of every party, and passes the answer with them. Nothing waits on
        // the disk. A refusal that left the part whole is an answer as any other. In both cases the call writes the
        // accounts in the answer, for the conference that the refusal calls
        const int panicked = s_launch_faulted || s_launch_timed_out;
        cubin_run_refusal_written(answers, panicked, code_path, code_size, registers, probe_place, launches, &shape,
                                  name, status, verdict, at);
        return cubin_run_member_written(answers, answers_path, panicked ? QRY_PANIC : QRY_ORDINARY, 1);
    }
    mmgr_line_put(answers, "answered");
    for (unsigned int place = 0u; place < s_case_count; place += 1u)
    {
        mmgr_line_put(answers, " ");
        mmgr_line_hex(answers,
                      (unsigned long long)(((unsigned long long)s_answers[place][1] << 32u) | s_answers[place][0]), 0u);
    }
    mmgr_line_put(answers, "\n");
    // the own count of the registers of the kernel from the part, beside the count that the question declared. The
    // call reads it from the part, and has the register boundary in hand with no query that walks up to it
    if (s_part_registers != CUBIN_RUN_REGISTERS_UNREAD)
    {
        mmgr_line_put(answers, "registers ");
        mmgr_line_decimal(answers, (unsigned long long)(registers), 0u);
        mmgr_line_put(answers, " ");
        mmgr_line_decimal(answers, (unsigned long long)(s_part_registers), 0u);
        mmgr_line_put(answers, "\n");
    }
    if (launches != 0u)
    {
        mmgr_line_put(answers, "timed ");
        mmgr_line_decimal(answers, (unsigned long long)(launches), 0u);
        mmgr_line_put(answers, " ");
        mmgr_line_decimal(answers, (unsigned long long)(nanoseconds), 0u);
        mmgr_line_put(answers, " ");
        mmgr_line_decimal(answers, (unsigned long long)(s_timed_least), 0u);
        mmgr_line_put(answers, " ");
        mmgr_line_decimal(answers, (unsigned long long)(s_timed_max), 0u);
        mmgr_line_put(answers, "\n");
        if (cubin_run_clock_offset() != 0u)
        {
            mmgr_line_put(answers, "clock address ");
            mmgr_line_hex(answers, (unsigned long long)(cubin_run_clock_offset()), 0u);
            mmgr_line_put(answers, "\n");
        }
    }
    const unsigned long long passed_began = cubin_run_host_now();
    cubin_run_launch_row(query_key, CUBIN_RUN_ANSWERED, launches, (launches != 0u) ? nanoseconds : 0ULL,
                         (launches != 0u) ? s_timed_least : 0ULL, (launches != 0u) ? s_timed_max : 0ULL, "");
    const int filed = cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    s_clocks.passed += cubin_run_host_now() - passed_began;
    return filed;
}

// the gated list named `name`, loaded into the gate from its blob in the run. With it comes every encoding that
// faulted the part, from CUBIN_RUN_FAULTS of the run where the run holds one. The return is 1, or 0 where either did
// not read
int cubin_run_verdict_load(const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_blob(name, &bytes, &size, NULL) || !cubin_safe_verdict_read((const char *)bytes, size))
    {
        return 0;
    }
    return !qry_member_blob(CUBIN_RUN_FAULTS, &bytes, &size, NULL) ||
           cubin_safe_verdict_added((const char *)bytes, size);
}
