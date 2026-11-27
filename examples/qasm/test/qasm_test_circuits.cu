// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qasm_test_circuits.cu: RXX, the inverse Fourier transform, measurement, fixtures and main
#include "../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "qasm_test_internal.h"

// exact gates alone: E = 0 and the peak's probability is 1 exactly
static void exact_only(void)
{
    char reason[QASM_REASON_CAPACITY];
    QasmCircuit circuit;
    const char *const text = "OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[4];\ncreg c[4];\nx q[0];\ncx q[0],q[1];\n"
                             "swap q[1],q[2];\ns q[2];\ny q[3];\nccx q[2],q[3],q[0];\ncz q[2],q[3];\nmeasure q -> c;\n";
    QasmOutcome outcome;
    const int ran = read_text("exact.qasm", text, &circuit, reason, sizeof(reason)) && run(&circuit, 0, &outcome, NULL);
    char bitstring[QASM_CLBITS_MAX + 1u];
    qasm_bitstring(&circuit, ran ? outcome.peak : 0ull, bitstring, sizeof(bitstring));
    const unsigned int zero[QASM_WIDE_LIMBS] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    check(ran && (circuit.rounded_gates == 0u) &&
              (EMBED_CALL(memor.cmp, MemoriaCfg, .src = circuit.bound, .other = zero, .bytes = sizeof(zero)) == 0) &&
              units_are_power(outcome.peak_units, 2u * QASM_FRACTION_BITS) && (outcome.proved != 0u) &&
              (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bitstring, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                          .other = "1100", .ci = EMBED_FALSE)),
          "exact gates only: E = 0, p(%s) = 1 exactly, proved", bitstring);
    if (ran)
    {
        qasm_release(&circuit);
    }
}

// rxx(2 pi / 3) on |00>: cos(pi/3)|00> - i sin(pi/3)|11>. P(11) = 3/4 and p(00) = 1/4
static void rxx_split(void)
{
    char reason[QASM_REASON_CAPACITY];
    QasmCircuit circuit;
    const char *const text = "OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[2];\ncreg c[2];\nrxx(2*pi/3) q[0],q[1];\n"
                             "measure q -> c;\n";
    QasmOutcome outcome;
    const int ran = read_text("rxx.qasm", text, &circuit, reason, sizeof(reason)) && run(&circuit, 0, &outcome, NULL);
    char peak[QASM_CLBITS_MAX + 1u];
    char second[QASM_CLBITS_MAX + 1u];
    qasm_bitstring(&circuit, ran ? outcome.peak : 0ull, peak, sizeof(peak));
    qasm_bitstring(&circuit, ran ? outcome.runner_up : 0ull, second, sizeof(second));
    check(ran &&
              (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = peak, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "11",
                          .ci = EMBED_FALSE)) &&
              (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = second, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "00",
                          .ci = EMBED_FALSE)) &&
              (outcome.proved != 0u) &&
              within(outcome.peak_units, 3u, (2u * QASM_FRACTION_BITS) - 2u, outcome.slack_units) &&
              within(outcome.runner_up_units, 1u, (2u * QASM_FRACTION_BITS) - 2u, outcome.slack_units),
          "rxx(2 pi/3): p(%s) = 3/4 and p(%s) = 1/4 within the slack, proved", peak, second);
    if (ran)
    {
        qasm_release(&circuit);
    }
}

// the inverse quantum Fourier transform of the Fourier state of k returns k
static void inverse_fourier(void)
{
    const unsigned int n = 12u;
    const unsigned int k = 2741u;
    const unsigned int size = 1u << n;
    char *const text = (char *)platform_cell_taken(TEXT_CAPACITY, platform_place_g_program_owner, 0);
    Text built = {text, 0u};
    text_add(&built, "%sqreg q[%u];\ncreg c[%u];\n", HEADER, n, n);
    // (|0> + e^{2 pi i k 2^j / N}|1>) / sqrt 2 on qubit j is the Fourier state of k, bit j of x on qubit j
    for (unsigned int j = 0u; j < n; j += 1u)
    {
        text_add(&built, "h q[%u];\nu1(2*pi*%u/%u) q[%u];\n", j, (k << j) % size, size, j);
    }
    // the inverse of Qiskit's QFT: the swaps, then for each j its controlled phases and its h
    for (unsigned int i = 0u; i < (n / 2u); i += 1u)
    {
        text_add(&built, "swap q[%u],q[%u];\n", i, n - 1u - i);
    }
    for (unsigned int j = 0u; j < n; j += 1u)
    {
        for (unsigned int m = 0u; m < j; m += 1u)
        {
            text_add(&built, "cp(-pi/%u) q[%u],q[%u];\n", 1u << (j - m), j, m);
        }
        text_add(&built, "h q[%u];\n", j);
    }
    text_add(&built, "measure q -> c;\n");
    char reason[QASM_REASON_CAPACITY];
    QasmCircuit circuit;
    QasmOutcome outcome;
    const int read = read_text("fourier.qasm", text, &circuit, reason, sizeof(reason));
    const int ran = read && run(&circuit, 0, &outcome, NULL);
    char bitstring[QASM_CLBITS_MAX + 1u];
    char wanted[QASM_CLBITS_MAX + 1u];
    qasm_bitstring(&circuit, ran ? outcome.peak : 0ull, bitstring, sizeof(bitstring));
    qasm_bitstring(&circuit, k, wanted, sizeof(wanted));
    check(ran &&
              (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bitstring, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                          .other = wanted, .ci = EMBED_FALSE)) &&
              (outcome.proved != 0u) && within(outcome.peak_units, 1u, 2u * QASM_FRACTION_BITS, outcome.slack_units),
          "inverse QFT over %u qubits returns k = %u as %s (%u gates, %u rounded), p = 1 within the slack, proved", n,
          k, bitstring, read ? circuit.gate_count : 0u, read ? circuit.rounded_gates : 0u);
    if (!read)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, reason);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (read)
    {
        qasm_release(&circuit);
    }
    platform_cell_released(text, platform_place_g_program_owner);
}

// the measure's clbits, measuring part of the register, and no measure at all
static void measurement(void)
{
    static const struct
    {
        const char *text;
        const char *expected;
        const char *what;
    } CASES[] = {
        {"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[3];\ncreg c[3];\nx q[0];\nmeasure q[0] -> c[2];\n"
         "measure q[1] -> c[0];\nmeasure q[2] -> c[1];\n",
         "100", "q[0] measured into c[2]"},
        {"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[2];\ncreg c[1];\nh q[0];\nx q[1];\nmeasure q[1] -> c[0];\n",
         "1", "q[1] alone measured, q[0] summed over"},
        {"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[2];\nx q[1];\n", "10",
         "no measure: every qubit into its own clbit"},
        {"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg a[2];\nqreg b[2];\ncreg c[4];\nx a;\ncx a, b;\n"
         "measure a[0] -> c[0];\nmeasure a[1] -> c[1];\nmeasure b[0] -> c[2];\nmeasure b[1] -> c[3];\n",
         "1111", "two registers broadcast together"},
        {"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg a[2];\nqreg b[1];\ncreg c[2];\ncreg d[1];\nx b[0];\n"
         "measure a -> c;\nmeasure b -> d;\n",
         "100", "registers flattened in declaration order"},
    };
    char reason[QASM_REASON_CAPACITY];
    for (size_t at = 0u; at < (sizeof(CASES) / sizeof(CASES[0])); at += 1u)
    {
        QasmCircuit circuit;
        QasmOutcome outcome;
        const int read = read_text("measure.qasm", CASES[at].text, &circuit, reason, sizeof(reason));
        const int ran = read && run(&circuit, 0, &outcome, NULL);
        char bitstring[QASM_CLBITS_MAX + 1u];
        qasm_bitstring(&circuit, ran ? outcome.peak : 0ull, bitstring, sizeof(bitstring));
        check(ran &&
                  (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bitstring, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                              .other = CASES[at].expected, .ci = EMBED_FALSE)) &&
                  (outcome.proved != 0u) &&
                  within(outcome.peak_units, 1u, 2u * QASM_FRACTION_BITS, outcome.slack_units),
              "%s: %s (wanted %s), p = 1 within the slack, proved", CASES[at].what, bitstring, CASES[at].expected);
        if (!read)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, reason);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (read)
        {
            qasm_release(&circuit);
        }
    }
}

// the fixtures, read by path
static void fixtures(const char *directory)
{
    static const struct
    {
        const char *file;
        const char *expected;
        unsigned int proved;
    } FIXTURES[] = {
        {"bernstein_vazirani.qasm", "101101001101", 1u},
        {"bell.qasm", NULL, 0u},
    };
    for (size_t at = 0u; at < (sizeof(FIXTURES) / sizeof(FIXTURES[0])); at += 1u)
    {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", directory, FIXTURES[at].file);
        char reason[QASM_REASON_CAPACITY];
        reason[0] = '\0';
        EngineError error;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        QasmCircuit circuit;
        const QasmReadRequest request = {path, NULL, 0u, reason, sizeof(reason), &error};
        const int read = qasm_read(&request, &circuit) != QASM_ERROR;
        QasmOutcome outcome;
        const int ran = read && run(&circuit, 0, &outcome, NULL);
        char bitstring[QASM_CLBITS_MAX + 1u];
        qasm_bitstring(&circuit, ran ? outcome.peak : 0ull, bitstring, sizeof(bitstring));
        check(ran && (outcome.proved == FIXTURES[at].proved) &&
                  ((FIXTURES[at].expected == NULL) ||
                   (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bitstring, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                               .other = FIXTURES[at].expected, .ci = EMBED_FALSE))),
              "fixture %s: %s, %s", FIXTURES[at].file, bitstring, (ran && outcome.proved) ? "proved" : "not proved");
        if (!read)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, reason);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (read)
        {
            qasm_release(&circuit);
        }
    }
}

// a random circuit on the device and on the host: the same state word for word
static unsigned long long g_seed = 0x9E3779B97F4A7C15ull;

static unsigned int next_random(unsigned int below)
{
    g_seed = (g_seed * 6364136223846793005ull) + 1442695040888963407ull;
    return (unsigned int)((g_seed >> 33u) % below);
}

static void random_angle(char *angle, size_t capacity)
{
    const unsigned int form = next_random(3u);
    if (form == 0u)
    {
        snprintf(angle, capacity, "%d*pi/%u", (int)next_random(17u) - 8, 1u + next_random(12u));
    }
    else if (form == 1u)
    {
        snprintf(angle, capacity, "0.%06u", next_random(1000000u));
    }
    else
    {
        snprintf(angle, capacity, "-%u.%03u + pi/%u", next_random(4u), next_random(1000u), 1u + next_random(7u));
    }
}

static void device_against_host(void)
{
    static const char *const ONE[] = {"h", "x", "y", "z", "s", "sdg", "t", "tdg", "sx", "sxdg"};
    static const char *const ONE_ANGLE[] = {"rx", "ry", "rz", "u1", "p"};
    static const char *const TWO[] = {"cx", "cz", "cy", "ch", "swap", "csx"};
    static const char *const TWO_ANGLE[] = {"crx", "cry", "crz", "cu1", "cp", "rzz", "rxx", "ryy"};
    const unsigned int n = 8u;
    const unsigned int gates = 200u;
    char *const text = (char *)platform_cell_taken(TEXT_CAPACITY, platform_place_g_program_owner, 0);
    Text built = {text, 0u};
    text_add(&built, "%sqreg q[%u];\ncreg c[%u];\n", HEADER, n, n);
    for (unsigned int gate = 0u; gate < gates; gate += 1u)
    {
        const unsigned int a = next_random(n);
        const unsigned int b = (a + 1u + next_random(n - 1u)) % n;
        unsigned int c = next_random(n);
        while ((c == a) || (c == b))
        {
            c = (c + 1u) % n;
        }
        char one[64];
        char two[64];
        char three[64];
        random_angle(one, sizeof(one));
        random_angle(two, sizeof(two));
        random_angle(three, sizeof(three));
        const unsigned int type = next_random(8u);
        if (type == 0u)
        {
            text_add(&built, "%s q[%u];\n", ONE[next_random(10u)], a);
        }
        else if (type == 1u)
        {
            text_add(&built, "%s(%s) q[%u];\n", ONE_ANGLE[next_random(5u)], one, a);
        }
        else if (type == 2u)
        {
            text_add(&built, "u3(%s,%s,%s) q[%u];\n", one, two, three, a);
        }
        else if (type == 3u)
        {
            text_add(&built, "u2(%s,%s) q[%u];\n", one, two, a);
        }
        else if (type == 4u)
        {
            text_add(&built, "%s q[%u],q[%u];\n", TWO[next_random(6u)], a, b);
        }
        else if (type == 5u)
        {
            text_add(&built, "%s(%s) q[%u],q[%u];\n", TWO_ANGLE[next_random(8u)], one, a, b);
        }
        else if (type == 6u)
        {
            text_add(&built, "cu3(%s,%s,%s) q[%u],q[%u];\n", one, two, three, a, b);
        }
        else
        {
            text_add(&built, "%s q[%u],q[%u],q[%u];\n", (next_random(2u) == 0u) ? "ccx" : "cswap", a, b, c);
        }
    }
    text_add(&built, "measure q -> c;\n");
    char reason[QASM_REASON_CAPACITY];
    QasmCircuit circuit;
    const int read = read_text("random.qasm", text, &circuit, reason, sizeof(reason));
    const unsigned long long lanes = 1ull << n;
    unsigned int *const device_state = (unsigned int *)platform_cell_cleared(
        (size_t)(lanes * QASM_STATE_LIMBS), sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const host_state = (unsigned int *)platform_cell_cleared(
        (size_t)(lanes * QASM_STATE_LIMBS), sizeof(unsigned int), platform_place_g_program_owner, 0);
    QasmOutcome device;
    QasmOutcome host;
    const int ran = read && run(&circuit, 0, &device, device_state) && run(&circuit, 1, &host, host_state);
    check(ran &&
              (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device_state, .other = host_state,
                          .bytes = (size_t)(lanes * QASM_STATE_LIMBS * sizeof(unsigned int))) == 0) &&
              (device.peak == host.peak) && (device.runner_up == host.runner_up) &&
              (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device.peak_units, .other = host.peak_units,
                          .bytes = sizeof(device.peak_units)) == 0) &&
              (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device.slack_units, .other = host.slack_units,
                          .bytes = sizeof(device.slack_units)) == 0) &&
              (device.proved == host.proved),
          "random circuit, %u qubits, %u gates (%u rounded): device and host agree word for word", n,
          read ? circuit.gate_count : 0u, read ? circuit.rounded_gates : 0u);
    if (!read)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, reason);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (read)
    {
        qasm_release(&circuit);
    }
    platform_cell_released(device_state, platform_place_g_program_owner);
    platform_cell_released(host_state, platform_place_g_program_owner);
    platform_cell_released(text, platform_place_g_program_owner);
}

int program_main(int count, char **arguments)
{
    const char *const directory = (count > 1) ? arguments[1] : ".";
    // the job reserves the widest run here: 13 qubits
    QasmCircuit widest;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &widest, .bytes = sizeof(widest), .val = 0);
    widest.qubits = 13u;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    QasmJob *job = NULL;
    static const unsigned char named[] = "qasm_test";
    if (qasm_job_submit(named, sizeof(named) - 1u, qasm_device_bytes(&widest), &job, &error) == QASM_ERROR)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  FAIL the device's tessera daemon did not admit the test (error type ");
            mmgr_line_decimal_signed(output_line, (int)error.type);
            mmgr_line_put(output_line, ", module ");
            mmgr_line_decimal_signed(output_line, (int)error.module);
            mmgr_line_put(output_line, ", site ");
            mmgr_line_decimal(output_line, error.site, 1u);
            mmgr_line_put(output_line, ")");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    errors();
    bound_formula();
    known_answers();
    ties();
    exact_only();
    rxx_split();
    inverse_fourier();
    measurement();
    fixtures(directory);
    device_against_host();
    EngineError released;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &released, .bytes = sizeof(released), .val = 0);
    qasm_job_release(job, &released);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "qasm_test: ");
        mmgr_line_decimal(output_line, g_passed, 1u);
        mmgr_line_put(output_line, " passed, ");
        mmgr_line_decimal(output_line, g_failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (g_failed == 0u) ? 0 : 1;
}
