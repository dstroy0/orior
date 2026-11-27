// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qasm_gates.c: gate entries and the lowering of named gates
#include "../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "qasm_internal.h"

static void qasm_gate_start(QasmGate *gate, unsigned int type, unsigned int pattern, const QasmToken *token)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = gate, .bytes = sizeof(*gate), .val = 0);
    gate->type = type;
    gate->pattern = pattern;
    gate->line = token->line;
    gate->column = token->column;
}

static void qasm_gate_controls(QasmGate *gate, const unsigned int *qubits, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        gate->controls |= 1ull << qubits[at];
    }
    gate->control_count = count;
}

// the entry r e^{i phase} at G: (r cos, r sin), with r and the phase's cos and sin at G; slack grows by the product's
static int qasm_entry(QasmParser *parser, const OriorExactInteger *radius, const QasmAngle *phase, int negate,
                      long long *re, long long *im, unsigned long long *slack)
{
    OriorExactInteger c;
    OriorExactInteger s;
    unsigned long long phase_slack = 0ull;
    if (!qasm_cos_sin(parser, phase, &c, &s, &phase_slack))
    {
        return 0;
    }
    OriorExactInteger x;
    OriorExactInteger y;
    if (!qasm_fixed_multiply(parser, radius, &c, &x) || !qasm_fixed_multiply(parser, radius, &s, &y))
    {
        return 0;
    }
    // |radius|, |cos|, |sin| <= 1 within a few units: the product's error is the two errors, their product's
    // share below one unit, and one unit truncated
    const unsigned long long counted = *slack + phase_slack + 2ull;
    if (counted >= QASM_SLACK_MAX)
    {
        qasm_error(parser, &parser->tokens[parser->at], "an entry's counted error passed its capacity");
        return 0;
    }
    if (negate != 0)
    {
        x.sign = -x.sign;
        y.sign = -y.sign;
    }
    return qasm_fixed_round(parser, &x, re) && qasm_fixed_round(parser, &y, im);
}

// e^{i gamma} U3(theta, phi, lambda): [[c, -e^{i lambda} s], [e^{i phi} s, e^{i (phi + lambda)} c]] times e^{i gamma}
static int qasm_u3_entries(QasmParser *parser, const QasmAngle *theta, const QasmAngle *phi, const QasmAngle *lambda,
                           const QasmAngle *gamma, QasmGate *gate)
{
    QasmAngle half;
    OriorExactInteger c;
    OriorExactInteger s;
    unsigned long long slack = 0ull;
    if (!qasm_angle_scale(parser, theta, 1, 2, &half) || !qasm_cos_sin(parser, &half, &c, &s, &slack))
    {
        return 0;
    }
    QasmAngle gamma_lambda;
    QasmAngle gamma_phi;
    QasmAngle gamma_phi_lambda;
    if (!qasm_angle_add(parser, gamma, lambda, 0, &gamma_lambda) ||
        !qasm_angle_add(parser, gamma, phi, 0, &gamma_phi) ||
        !qasm_angle_add(parser, &gamma_phi, lambda, 0, &gamma_phi_lambda))
    {
        return 0;
    }
    return qasm_entry(parser, &c, gamma, 0, &gate->entries[0], &gate->entries[1], &slack) &&
           qasm_entry(parser, &s, &gamma_lambda, 1, &gate->entries[2], &gate->entries[3], &slack) &&
           qasm_entry(parser, &s, &gamma_phi, 0, &gate->entries[4], &gate->entries[5], &slack) &&
           qasm_entry(parser, &c, &gamma_phi_lambda, 0, &gate->entries[6], &gate->entries[7], &slack);
}

// diag(e^{i zero}, e^{i one})
static int qasm_diagonal_entries(QasmParser *parser, const QasmAngle *zero, const QasmAngle *one, QasmGate *gate)
{
    unsigned long long slack = 0ull;
    return qasm_entry(parser, &parser->one_fixed, zero, 0, &gate->entries[0], &gate->entries[1], &slack) &&
           qasm_entry(parser, &parser->one_fixed, one, 0, &gate->entries[2], &gate->entries[3], &slack);
}

// the angle is a whole number of quarter turns (a = 0 and 2b whole): its count mod 4, else -1
static int qasm_quarter_turns(QasmParser *parser, const QasmAngle *angle)
{
    if (!qasm_exact_is_zero(&angle->a.num))
    {
        return -1;
    }
    QasmFraction twice;
    QasmFraction two;
    qasm_fraction_integer(&two, 2);
    if (!qasm_fraction_multiply(parser, &angle->b, &two, &twice))
    {
        return -1;
    }
    OriorExactInteger one;
    qasm_exact_set(&one, 1ull, 0);
    if (!orior_exact_equal(&twice.den, &one))
    {
        return -1;
    }
    OriorExactInteger four;
    OriorExactInteger turns;
    OriorExactInteger remainder;
    qasm_exact_set(&four, 4ull, 0);
    if (orior_exact_divide(&twice.num, &four, &turns, &remainder) != ORIOR_EXACT_OK)
    {
        return -1;
    }
    long long quarter = (long long)qasm_exact_small(&remainder);
    quarter = (remainder.sign < 0) ? (4 - quarter) % 4 : quarter;
    return (int)quarter;
}

static const QasmBuiltin QASM_BUILTINS[] = {
    {"U", 3u, 1u},    {"u3", 3u, 1u},   {"u", 3u, 1u},   {"u2", 2u, 1u},  {"u1", 1u, 1u},    {"p", 1u, 1u},
    {"CX", 0u, 2u},   {"cx", 0u, 2u},   {"id", 0u, 1u},  {"u0", 1u, 1u},  {"x", 0u, 1u},     {"y", 0u, 1u},
    {"z", 0u, 1u},    {"h", 0u, 1u},    {"s", 0u, 1u},   {"sdg", 0u, 1u}, {"t", 0u, 1u},     {"tdg", 0u, 1u},
    {"rx", 1u, 1u},   {"ry", 1u, 1u},   {"rz", 1u, 1u},  {"sx", 0u, 1u},  {"sxdg", 0u, 1u},  {"cz", 0u, 2u},
    {"cy", 0u, 2u},   {"swap", 0u, 2u}, {"ch", 0u, 2u},  {"ccx", 0u, 3u}, {"cswap", 0u, 3u}, {"crx", 1u, 2u},
    {"cry", 1u, 2u},  {"crz", 1u, 2u},  {"cu1", 1u, 2u}, {"cp", 1u, 2u},  {"cu3", 3u, 2u},   {"cu", 4u, 2u},
    {"csx", 0u, 2u},  {"rzz", 1u, 2u},  {"rxx", 1u, 2u}, {"ryy", 1u, 2u}, {"c3x", 0u, 4u},   {"c4x", 0u, 5u},
    {"iden", 0u, 1u},
};

const QasmBuiltin *qasm_builtin(const char *name)
{
    for (unsigned int at = 0u; at < (sizeof(QASM_BUILTINS) / sizeof(QASM_BUILTINS[0])); at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = QASM_BUILTINS[at].name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = name, .ci = EMBED_FALSE))
        {
            return &QASM_BUILTINS[at];
        }
    }
    return NULL;
}

// a pair gate with its entries computed from U3 plus a phase, under the given controls
static int qasm_lower_u3(QasmParser *parser, const QasmAngle *theta, const QasmAngle *phi, const QasmAngle *lambda,
                         const QasmAngle *gamma, const unsigned int *controls, unsigned int control_count,
                         unsigned int target, const QasmToken *token)
{
    QasmGate gate;
    qasm_gate_start(&gate, QASM_GATE_PAIR, 0u, token);
    gate.target = target;
    qasm_gate_controls(&gate, controls, control_count);
    return qasm_u3_entries(parser, theta, phi, lambda, gamma, &gate) && qasm_push_gate(parser, &gate);
}

// diag(1, e^{i lambda}) under controls: exact where lambda is whole quarter turns
static int qasm_lower_phase(QasmParser *parser, const QasmAngle *lambda, const unsigned int *controls,
                            unsigned int control_count, unsigned int target, const QasmToken *token)
{
    QasmGate gate;
    const int quarters = qasm_quarter_turns(parser, lambda);
    if (parser->failed != 0)
    {
        return 0;
    }
    if (quarters >= 0)
    {
        if (quarters == 0)
        {
            return 1;
        }
        qasm_gate_start(&gate, QASM_GATE_PERMUTE, QASM_PERMUTE_PHASE, token);
        gate.target = target;
        gate.phase = (unsigned int)quarters;
        qasm_gate_controls(&gate, controls, control_count);
        return qasm_push_gate(parser, &gate);
    }
    QasmAngle zero;
    qasm_angle_rational(&zero, 0, 1, 0, 1);
    qasm_gate_start(&gate, QASM_GATE_DIAGONAL, QASM_DIAGONAL_BIT, token);
    gate.target = target;
    qasm_gate_controls(&gate, controls, control_count);
    return qasm_diagonal_entries(parser, &zero, lambda, &gate) && qasm_push_gate(parser, &gate);
}

// a gate whose entries are exact multiples of 2^-(F+1): sx and sxdg, (1 +- i) / 2
static int qasm_lower_sx(QasmParser *parser, int dagger, const unsigned int *controls, unsigned int control_count,
                         unsigned int target, const QasmToken *token)
{
    QasmGate gate;
    qasm_gate_start(&gate, QASM_GATE_PAIR, 0u, token);
    gate.target = target;
    qasm_gate_controls(&gate, controls, control_count);
    const long long half = 1ll << (QASM_FRACTION_BITS - 1u);
    const long long sign = (dagger != 0) ? -1 : 1;
    const long long entries[8] = {half, sign * half, half, -sign * half, half, -sign * half, half, sign * half};
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = gate.entries, .src = entries, .bytes = sizeof(entries));
    gate.exact_entries = 1u;
    return qasm_push_gate(parser, &gate);
}

static int qasm_lower_permute(QasmParser *parser, unsigned int pattern, unsigned int phase,
                              const unsigned int *controls, unsigned int control_count, unsigned int target,
                              unsigned int second, const QasmToken *token)
{
    QasmGate gate;
    qasm_gate_start(&gate, QASM_GATE_PERMUTE, pattern, token);
    gate.target = target;
    gate.second = second;
    gate.phase = phase;
    qasm_gate_controls(&gate, controls, control_count);
    return qasm_push_gate(parser, &gate);
}

int qasm_lower_gate(QasmParser *parser, const char *name, const QasmAngle *angles, const unsigned int *qubits,
                    const QasmToken *token)
{
    QasmAngle zero;
    QasmAngle half_pi;
    QasmAngle negative_half_pi;
    qasm_angle_rational(&zero, 0, 1, 0, 1);
    qasm_angle_rational(&half_pi, 0, 1, 1, 2);
    qasm_angle_rational(&negative_half_pi, 0, 1, -1, 2);
    const unsigned int q0 = qubits[0];
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "U",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "u3",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "u",
                    .ci = EMBED_FALSE)))
    {
        return qasm_lower_u3(parser, &angles[0], &angles[1], &angles[2], &zero, NULL, 0u, q0, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "u2",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_u3(parser, &half_pi, &angles[0], &angles[1], &zero, NULL, 0u, q0, token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "u1",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "p",
                    .ci = EMBED_FALSE)))
    {
        return qasm_lower_phase(parser, &angles[0], NULL, 0u, q0, token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "id",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "u0",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "iden",
                    .ci = EMBED_FALSE)))
    {
        return 1;
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "CX",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cx",
                    .ci = EMBED_FALSE)))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_FLIP, 0u, &qubits[0], 1u, qubits[1], 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "ccx",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_FLIP, 0u, &qubits[0], 2u, qubits[2], 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "c3x",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_FLIP, 0u, &qubits[0], 3u, qubits[3], 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "c4x",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_FLIP, 0u, &qubits[0], 4u, qubits[4], 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "x",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_FLIP, 0u, NULL, 0u, q0, 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "y",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_Y, 0u, NULL, 0u, q0, 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cy",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_Y, 0u, &qubits[0], 1u, qubits[1], 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "z",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_PHASE, 2u, NULL, 0u, q0, 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cz",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_PHASE, 2u, &qubits[0], 1u, qubits[1], 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "s",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_PHASE, 1u, NULL, 0u, q0, 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "sdg",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_PHASE, 3u, NULL, 0u, q0, 0u, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "swap",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_SWAP, 0u, NULL, 0u, qubits[0], qubits[1], token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cswap",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_permute(parser, QASM_PERMUTE_SWAP, 0u, &qubits[0], 1u, qubits[1], qubits[2], token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "t",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "tdg",
                    .ci = EMBED_FALSE)))
    {
        QasmAngle eighth;
        qasm_angle_rational(&eighth, 0, 1,
                            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                        .other = "t", .ci = EMBED_FALSE))
                                ? 1
                                : -1,
                            4);
        return qasm_lower_phase(parser, &eighth, NULL, 0u, q0, token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cu1",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cp",
                    .ci = EMBED_FALSE)))
    {
        return qasm_lower_phase(parser, &angles[0], &qubits[0], 1u, qubits[1], token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "h",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "ch",
                    .ci = EMBED_FALSE)))
    {
        // H = U3(pi/2, 0, pi)
        QasmAngle pi;
        qasm_angle_rational(&pi, 0, 1, 1, 1);
        const unsigned int controlled =
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "ch",
                        .ci = EMBED_FALSE));
        return qasm_lower_u3(parser, &half_pi, &zero, &pi, &zero, &qubits[0], controlled, qubits[controlled], token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "rx",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "crx",
                    .ci = EMBED_FALSE)))
    {
        // RX(theta) = U3(theta, -pi/2, pi/2)
        const unsigned int controlled =
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "crx",
                        .ci = EMBED_FALSE));
        return qasm_lower_u3(parser, &angles[0], &negative_half_pi, &half_pi, &zero, &qubits[0], controlled,
                             qubits[controlled], token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "ry",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cry",
                    .ci = EMBED_FALSE)))
    {
        // RY(theta) = U3(theta, 0, 0)
        const unsigned int controlled =
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cry",
                        .ci = EMBED_FALSE));
        return qasm_lower_u3(parser, &angles[0], &zero, &zero, &zero, &qubits[0], controlled, qubits[controlled],
                             token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "rz",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "crz",
                    .ci = EMBED_FALSE)))
    {
        // RZ(theta) = diag(e^{-i theta/2}, e^{i theta/2})
        const unsigned int controlled =
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "crz",
                        .ci = EMBED_FALSE));
        QasmAngle half;
        QasmAngle negative_half;
        if (!qasm_angle_scale(parser, &angles[0], 1, 2, &half) ||
            !qasm_angle_scale(parser, &angles[0], -1, 2, &negative_half))
        {
            return 0;
        }
        QasmGate gate;
        qasm_gate_start(&gate, QASM_GATE_DIAGONAL, QASM_DIAGONAL_BIT, token);
        gate.target = qubits[controlled];
        qasm_gate_controls(&gate, &qubits[0], controlled);
        return qasm_diagonal_entries(parser, &negative_half, &half, &gate) && qasm_push_gate(parser, &gate);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "rzz",
                   .ci = EMBED_FALSE))
    {
        // exp(-i theta/2 Z Z): e^{-i theta/2} at even parity, e^{i theta/2} at odd
        QasmAngle half;
        QasmAngle negative_half;
        if (!qasm_angle_scale(parser, &angles[0], 1, 2, &half) ||
            !qasm_angle_scale(parser, &angles[0], -1, 2, &negative_half))
        {
            return 0;
        }
        QasmGate gate;
        qasm_gate_start(&gate, QASM_GATE_DIAGONAL, QASM_DIAGONAL_PARITY, token);
        gate.target = qubits[0];
        gate.second = qubits[1];
        return qasm_diagonal_entries(parser, &negative_half, &half, &gate) && qasm_push_gate(parser, &gate);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "rxx",
                   .ci = EMBED_FALSE))
    {
        // X = H Z H: RXX = (H x H) RZZ (H x H)
        const unsigned int a[1] = {qubits[0]};
        const unsigned int b[1] = {qubits[1]};
        return qasm_lower_gate(parser, "h", NULL, a, token) && qasm_lower_gate(parser, "h", NULL, b, token) &&
               qasm_lower_gate(parser, "rzz", angles, qubits, token) && qasm_lower_gate(parser, "h", NULL, a, token) &&
               qasm_lower_gate(parser, "h", NULL, b, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "ryy",
                   .ci = EMBED_FALSE))
    {
        // Y = S X Sdg: RYY = (S x S) RXX (Sdg x Sdg)
        const unsigned int a[1] = {qubits[0]};
        const unsigned int b[1] = {qubits[1]};
        return qasm_lower_gate(parser, "sdg", NULL, a, token) && qasm_lower_gate(parser, "sdg", NULL, b, token) &&
               qasm_lower_gate(parser, "rxx", angles, qubits, token) && qasm_lower_gate(parser, "s", NULL, a, token) &&
               qasm_lower_gate(parser, "s", NULL, b, token);
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "sx",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "sxdg",
                    .ci = EMBED_FALSE)))
    {
        return qasm_lower_sx(parser,
                             EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                        .other = "sxdg", .ci = EMBED_FALSE),
                             NULL, 0u, q0, token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "csx",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_sx(parser, 0, &qubits[0], 1u, qubits[1], token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cu3",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_u3(parser, &angles[0], &angles[1], &angles[2], &zero, &qubits[0], 1u, qubits[1], token);
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cu",
                   .ci = EMBED_FALSE))
    {
        return qasm_lower_u3(parser, &angles[0], &angles[1], &angles[2], &angles[3], &qubits[0], 1u, qubits[1], token);
    }
    qasm_error(parser, token, "the gate '%s' has no built-in form", name);
    return 0;
}
