// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// question.c: the question: the safety word laid on the code, a refusal's cause received and reported, the fault
// recorded, the account given, one question carried, and the verdict loaded
#include "cubin_run_internal.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>
#include <string.h>

// a part of a driver's error name and the cause a conference reads it as (overseer.h)
typedef struct
{
    const char *part;
    const char *cause;
} CubinRunCauseName;

// One question carried: its code at `code_path` put in the container of `pattern_size` bytes in s_pattern, its kernel
// `kernel`, declaring `registers`, over the cases at `cases_path`, launched as `shape` specifies where `shaped`, timed
// over `launches` where that is not 0, and its line written to `answers_path`. 0 where a line was written, 1 where the
// line is the driver's or the part's refusal of a launch, after which the context answers no other question, and 2
// where the inputs, the machine or the driver were not reached
// one word of an instruction in `code` at `byte`, read and written back
static unsigned long long cubin_run_word(const unsigned char *code, unsigned int byte)
{
    unsigned long long value = 0ull;
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
        code[byte + at] = (unsigned char)((value >> (8u * at)) & 0xffull);
    }
}

// the bits `first` to `last` of the instruction `low` and `high` make set to `value`
static void cubin_run_bits_set(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last,
                               unsigned long long value)
{
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        unsigned long long *const word = (bit < 64u) ? low : high;
        const unsigned long long one = (value >> (bit - first)) & 1ull;
        *word = (*word & ~(1ull << (bit % 64u))) | (one << (bit % 64u));
    }
}

// The safety word applied to `code` before the part sees it (P9): every instruction made to wait on all six barriers,
// and the probe at `kernel_place`, where the code contains one, made to stall the longest and use no barrier; a form
// the part has not named then waits on everything, and a barrier nothing releases leaves the wait after it standing.
// The protocol sends the code as the part accepted it and names the probe; the scheduler's own word is the carrier's to
// set
static void cubin_run_safe_word(unsigned char *code, unsigned long long code_size, unsigned int probe_place)
{
    const unsigned int places = (unsigned int)(code_size / 16ull);
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

// The driver's errors by the cause each reads as: the instruction's own encoding where the part would not take the
// code or run it, an address the code reached, a register past what the kernel allots, a launch that did not return,
// and an exception that names none of those
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
        if (strstr(name, s_cause_names[at].part) != NULL)
        {
            return s_cause_names[at].cause;
        }
    }
    return NULL;
}

// The cause the Xid critical errors the part stamped read as. The driver's own text of one first, where the system's
// log records it: a register out of range a register, an illegal instruction the instruction, and an address out of
// range or a fault of the memory map an address. Then its number: a fault of the part's memory map (31) an address, a
// channel that stopped (8) or a context switch past its time (109) a launch that did not return, and an exception of
// the graphics engine (13) or of its class (69) an exception. The first of those, in that order, that the errors
// carry; NULL where they carry none
static const char *cubin_run_listener_cause(void)
{
    static const char *const s_driver_messages[] = {"Out Of Range Register", "Illegal Instruction",
                                                    "Out Of Range Address", "MMU Fault"};
    static const char *const s_driver_causes[] = {"register", "instruction", "address", "address"};
    for (size_t message = 0u; message < (sizeof(s_driver_messages) / sizeof(s_driver_messages[0])); message += 1u)
    {
        for (unsigned int place = 0u; (place < s_xid_count) && (place < CUBIN_RUN_XID_MOST); place += 1u)
        {
            if (strstr(s_xid_text[place], s_driver_messages[message]) != NULL)
            {
                return s_driver_causes[message];
            }
        }
    }
    static const unsigned long long s_listener_events[] = {31ull, 8ull, 109ull, 13ull, 69ull};
    static const char *const s_listener_causes[] = {"address", "time", "time", "exception", "exception"};
    for (size_t event = 0u; event < (sizeof(s_listener_events) / sizeof(s_listener_events[0])); event += 1u)
    {
        for (unsigned int place = 0u; (place < s_xid_count) && (place < CUBIN_RUN_XID_MOST); place += 1u)
        {
            if (s_xid_data[place] == s_listener_events[event])
            {
                return s_listener_causes[event];
            }
        }
    }
    return NULL;
}

// The probe instruction `low` `high` of a launch that faulted the part added to the run's CUBIN_RUN_FAULTS with the
// cause the BAR0 driver received, handed critical so that it is on disk before the carrier goes on, and blocked from
// the part in this process at once
static void cubin_run_faulted_stored(unsigned long long low, unsigned long long high, const char *cause)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ull];
    VerbaLinea text = {s_text_bytes, sizeof(s_text_bytes), 0ull};
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
    snprintf(line, sizeof(line), "%016llx %016llx\n", low, high);
    cubin_safe_verdict_added(line, strlen(line));
}

// The account `call` contains written in the answer `answers` too, and handed as a panic of the call where `panicked`
// and the run's writer runs; let go either way
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

// A launch the part refused, its accounts written in its answer `answers` after the refusal, where the conference the
// refusal calls reads them (overseer.h): the proctor's, the carrier's own, of the launch it carried; the BAR0 driver's,
// the part's fault telemetry's, of the Xid critical errors the part stamped against it; and the gate's, of the code it
// passed, its verdict, the gated list and the probe instruction against it. The proctor and the BAR0 driver each end in
// the cause it reads, where it reads one. Where the carrier lost its mind, a launch that faulted the part or hung past
// its bound, each account is also handed as a panic of the call (QRY_PANIC_CALL) to the run, opening the line every
// party of the run sees. Nothing waits on the disk: the carrier goes straight on off the part
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
        for (unsigned int place = 0u; (place < s_xid_count) && (place < CUBIN_RUN_XID_MOST); place += 1u)
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
        mmgr_line_put(call, (s_xid_count > CUBIN_RUN_XID_MOST) ? " ..." : "");
        mmgr_line_put(call, (listener_cause != NULL) ? "; cause " : "");
        mmgr_line_put(call, (listener_cause != NULL) ? listener_cause : "");
        mmgr_line_put(call, "\n");
    }
    cubin_run_account_given(answers, call, panicked);
    // the probe instruction as the gate read it, the safety word laid on, where the code contains a probe
    const int placed = (((unsigned long long)probe_place * 16ull) + 16ull) <= code_size;
    const unsigned long long low = placed ? cubin_run_word(s_code, probe_place * 16u) : 0ull;
    const unsigned long long high = placed ? cubin_run_word(s_code, (probe_place * 16u) + 8u) : 0ull;
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
    const int cased = (code_size != 0ull) && cubin_run_cases(cases_path);
    s_clocks.input += cubin_run_host_now() - input_began;
    if ((pattern_size == 0ull) || !cased)
    {
        fprintf(stderr, "the container, the code or the cases did not read\n");
        return 2;
    }
    // a question that names no shape is given blocks of CUBIN_RUN_THREADS, as many as give every case a thread. A
    // launch gives every case a thread, and gives a case each to no more threads than it sizes them for
    shape.blocks = shaped ? shape.blocks : ((s_case_count + CUBIN_RUN_THREADS - 1u) / CUBIN_RUN_THREADS);
    const unsigned long long threads = (unsigned long long)shape.threads * shape.blocks;
    if ((threads < s_case_count) || (threads > CUBIN_RUN_THREADS_MOST))
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
    // the scheduler's own word set here, before the gate and the part: the protocol sent the code as the part
    // accepted it and named the probe
    const unsigned long long gate_began = cubin_run_host_now();
    cubin_run_safe_word(s_code, code_size, probe_place);
    CubinWrite written;
    memset(&written, 0, sizeof(written));
    written.pattern = s_pattern;
    written.pattern_size = pattern_size;
    written.kernel = kernel;
    written.code = s_code;
    written.code_size = code_size;
    written.registers = registers;
    written.exit_count = cubin_exits_find(s_code, code_size, sass_exit_encoding(&s_machine), s_exits, CUBIN_RUN_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ull;
    if (!cubin_write(&written, s_container, sizeof(s_container), &size))
    {
        mmgr_line_put(answers, "skipped not written\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // read on the host before the driver sees it: code that breaks a rule of cubin_safe is answered without the part,
    // its register fields bound by the count this question's kernel allots (rule 6)
    unsigned long long at = 0ull;
    const unsigned int verdict = cubin_safe_image(&s_machine, s_container, size, registers, &at);
    const unsigned long long row_began = cubin_run_host_now();
    s_clocks.gate += row_began - gate_began;
    cubin_run_verdict_row(query_key, verdict, at, s_dry);
    s_clocks.handed += cubin_run_host_now() - row_began;
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
    // a dry run stops here, before the driver: the question is answered as one blocked from the part
    if (s_dry)
    {
        mmgr_line_put(answers, "skipped dry, the part taken out\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // the safety airlock: a live run reaches the part only with the vendor's verdict in hand (--gated). Without it this
    // code was never read for a fault, and an unread encoding can fault the part and crash the machine: it is blocked
    // from the part here, the cross must read it first
    if (!cubin_safe_verdict_ready())
    {
        mmgr_line_put(answers, "skipped no vendor verdict, the cross must read this code before the part\n");
        return cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    }
    // a probe instruction the vendor's reader blocked from the part was already caught by cubin_safe_image above, where
    // the verdict is loaded (cubin_safe_verdict_gated): the gate blocks the whole code, not the probe alone
    if (!s_driver_opened)
    {
        memset(&s_driver, 0, sizeof(s_driver));
        if (!cubin_run_driver(&s_driver))
        {
            fprintf(stderr, "the driver was not reached\n");
            return 2;
        }
        // the part's fault telemetry opened beside the driver, for rule 7 to read after each launch; a run goes on
        // under rule 6 alone where it is not there, the warning printed
        cubin_run_telemetry_open();
        s_driver_opened = 1;
    }
    unsigned long long nanoseconds = 0ull;
    const CubinRunStatus status = cubin_run_launch(&s_driver, kernel, &shape, launches, &nanoseconds);
    if (status != 0)
    {
        const char *name = NULL;
        s_driver.error_name(status, &name);
        char refused[160];
        snprintf(refused, sizeof(refused), "refused %s%s", (name != NULL) ? name : "CUDA_ERROR_LAUNCH_FAILED",
                 s_launch_faulted ? " the part faulted" : "");
        mmgr_line_put(answers, refused);
        mmgr_line_put(answers, "\n");
        const unsigned int outcome = s_launch_timed_out ? CUBIN_RUN_TIMED_OUT
                                     : s_launch_faulted ? CUBIN_RUN_FAULTED
                                                        : CUBIN_RUN_REFUSED;
        cubin_run_launch_row(query_key, outcome, launches, 0ull, 0ull, 0ull, refused);
        // the carrier losing its mind, a launch that faulted the part or hung past its bound, is a panic: the call
        // opened with every party's account and the answer handed with them, nothing waiting on the disk. A refusal
        // that left the part whole is an answer like any other. Either way the accounts are written in the answer, for
        // the conference the refusal calls
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
    // the part's own count of the kernel's registers, beside the count the question declared: read off the part, the
    // register boundary in hand without a query that walks up to it
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
        mmgr_line_decimal(answers, (unsigned long long)(s_timed_most), 0u);
        mmgr_line_put(answers, "\n");
        if (cubin_run_clock_offset() != 0u)
        {
            mmgr_line_put(answers, "clock address ");
            mmgr_line_hex(answers, (unsigned long long)(cubin_run_clock_offset()), 0u);
            mmgr_line_put(answers, "\n");
        }
    }
    const unsigned long long handed_began = cubin_run_host_now();
    cubin_run_launch_row(query_key, CUBIN_RUN_ANSWERED, launches, (launches != 0u) ? nanoseconds : 0ull,
                         (launches != 0u) ? s_timed_least : 0ull, (launches != 0u) ? s_timed_most : 0ull, "");
    const int filed = cubin_run_member_written(answers, answers_path, QRY_ORDINARY, 0);
    s_clocks.handed += cubin_run_host_now() - handed_began;
    return filed;
}

// the gated list named `name` loaded into the gate, the blob of it in the run, and with it every encoding that
// faulted the part, the run's CUBIN_RUN_FAULTS where it contains one. 1, or 0 where either did not read
int cubin_run_verdict_load(const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_member_blob(name, &bytes, &size, NULL) || !cubin_safe_verdict_read((const char *)bytes, size))
    {
        return 0;
    }
    return !qry_member_blob(CUBIN_RUN_FAULTS, &bytes, &size, NULL) ||
           cubin_safe_verdict_added((const char *)bytes, size);
}
