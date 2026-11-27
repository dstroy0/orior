// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "qasm.h"
#include "../../../src/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

// qasm_bitstring [--host] circuit.qasm
// Reads an OpenQASM 2.0 circuit, runs it exactly in fixed point on the device (or the host), and prints the max
// likely bitstring, its probability, the runner-up, and whether the proved error bound separates them.
// Exit 0: the peak is proved. Exit 3: it is not. Exit 1: the circuit or the run errored.

#define QASM_EXIT_PROVED 0
#define QASM_EXIT_ERROR 1
#define QASM_EXIT_USAGE 2
#define QASM_EXIT_NOT_PROVED 3

#define QASM_PLACES 12u

static void qasm_print_error(const char *what, const EngineError *error)
{
    fprintf(stderr, "qasm_bitstring: %s (error type %d, module %d, site %u, status %d)\n", what, (int)error->type,
            (int)error->module, error->site, error->status);
}

int program_main(int count, char **arguments)
{
    int on_host = 0;
    const char *path = NULL;
    for (int at = 1; at < count; at += 1)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "--host", .ci = EMBED_FALSE))
        {
            on_host = 1;
        }
        else if ((path == NULL) && (arguments[at][0] != '-'))
        {
            path = arguments[at];
        }
        else
        {
            path = NULL;
            break;
        }
    }
    if (path == NULL)
    {
        fprintf(stderr, "usage: qasm_bitstring [--host] circuit.qasm\n");
        return QASM_EXIT_USAGE;
    }
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    char reason[QASM_REASON_CAPACITY];
    reason[0] = '\0';
    QasmCircuit circuit;
    const QasmReadRequest read = {path, NULL, 0u, reason, sizeof(reason), &error};
    if (qasm_read(&read, &circuit) == QASM_ERROR)
    {
        if (reason[0] != '\0')
        {
            fprintf(stderr, "%s\n", reason);
        }
        else
        {
            qasm_print_error("the circuit was not read", &error);
        }
        return QASM_EXIT_ERROR;
    }
    QasmJob *job = NULL;
    if (on_host == 0)
    {
        // the job is named by the run's path and the circuit's size
        char named[QASM_REASON_CAPACITY];
        const int length =
            snprintf(named, sizeof(named), "qasm_bitstring %s %u %u", path, circuit.qubits, circuit.gate_count);
        const unsigned long long bytes = (length > 0) ? (unsigned long long)length : 0ull;
        if (qasm_job_submit((const unsigned char *)named, bytes, qasm_device_bytes(&circuit), &job, &error) ==
            QASM_ERROR)
        {
            qasm_print_error("the device's tessera daemon did not admit the run", &error);
            qasm_release(&circuit);
            return QASM_EXIT_ERROR;
        }
    }
    QasmOutcome outcome;
    const QasmRunRequest run = {&circuit, on_host, NULL, &error};
    const long ran = qasm_run(&run, &outcome);
    if (job != NULL)
    {
        EngineError released;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &released, .bytes = sizeof(released), .val = 0);
        qasm_job_release(job, &released);
    }
    if (ran == QASM_ERROR)
    {
        qasm_print_error((error.type == ENGINE_ERROR_RESOURCE) ? "the run errored: the state does not fit"
                                                               : "the run errored",
                         &error);
        qasm_release(&circuit);
        return QASM_EXIT_ERROR;
    }
    char bitstring[QASM_CLBITS_MAX + 1u];
    char runner_up[QASM_CLBITS_MAX + 1u];
    char bound[96];
    char peak[96];
    char second[96];
    char slack[96];
    qasm_bitstring(&circuit, outcome.peak, bitstring, sizeof(bitstring));
    qasm_bitstring(&circuit, outcome.runner_up, runner_up, sizeof(runner_up));
    qasm_units_decimal(circuit.bound, QASM_PLACES, bound, sizeof(bound));
    qasm_units_decimal(outcome.peak_units, QASM_PLACES, peak, sizeof(peak));
    qasm_units_decimal(outcome.runner_up_units, QASM_PLACES, second, sizeof(second));
    qasm_units_decimal(outcome.slack_units, QASM_PLACES, slack, sizeof(slack));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "qubits     ");
        mmgr_line_decimal(output_line, circuit.qubits, 1u);
        mmgr_line_put(output_line, " (");
        mmgr_line_decimal(output_line, circuit.measured, 1u);
        mmgr_line_put(output_line, " measured");
        mmgr_line_put(output_line,
                      (circuit.measured_all_by_default != 0u) ? ", no measure: every qubit into its own clbit" : "");
        mmgr_line_put(output_line, ")");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "gates      ");
        mmgr_line_decimal(output_line, circuit.gate_count, 1u);
        mmgr_line_put(output_line, " (");
        mmgr_line_decimal(output_line, circuit.rounded_gates, 1u);
        mmgr_line_put(output_line, " rounded, ");
        mmgr_line_decimal(output_line, circuit.exact_gates, 1u);
        mmgr_line_put(output_line, " exact)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "bound E    ");
        mmgr_line_put(output_line, bound);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "bitstring  ");
        mmgr_line_put(output_line, bitstring);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "p(peak)    ");
        mmgr_line_put(output_line, peak);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "runner-up  ");
        mmgr_line_put(output_line, runner_up);
        mmgr_line_put(output_line, "  p ");
        mmgr_line_put(output_line, second);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "slack      ");
        mmgr_line_put(output_line, slack);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "run        ");
        mmgr_line_put(output_line, (on_host != 0) ? "host" : "device");
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, outcome.sweeps, 1u);
        mmgr_line_put(output_line, " sweeps over ");
        mmgr_line_decimal(output_line, outcome.lanes, 1u);
        mmgr_line_put(output_line, " lanes, ");
        mmgr_line_decimal(output_line, outcome.microseconds, 1u);
        mmgr_line_put(output_line, " us");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (outcome.proved != 0u) ? "PROVED" : "NOT PROVED");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    qasm_release(&circuit);
    return (outcome.proved != 0u) ? QASM_EXIT_PROVED : QASM_EXIT_NOT_PROVED;
}
