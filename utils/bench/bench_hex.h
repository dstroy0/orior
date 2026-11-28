// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_hex.h
 * @brief The bytes of a text of hex digits, in a record of a fixed bound.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note Each two digits give one byte, the first digit the high half. A last digit with no pair drops out.
 * @warning A text past BENCH_HEX_BYTES_MAX bytes writes its reason to the error stream and ends the program. A
 *          character that is not a hex digit does the same.
 */

#ifndef BENCH_HEX_H
#define BENCH_HEX_H

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief The bytes of a text at max. */
#define BENCH_HEX_BYTES_MAX 1024u

/** @brief The bytes of a text, `count` of them. */
typedef struct
{
    uint8_t at[BENCH_HEX_BYTES_MAX];
    size_t count;
} BenchHexBytes;

/**
 * @brief Returns the value of one hex digit.
 *
 * @param[in] hex   The text, for the message of a refusal [BORROWS].
 * @param[in] digit The digit.
 * @return          Its value, 0 to 15.
 */
static inline uint8_t bench_hex_digit(const char *hex, char digit)
{
    if (!EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_HEX, .byte = (uint8_t)digit))
    {
        fprintf(stderr, "bench_hex: '%c' in \"%s\" is not a hex digit\n", digit, hex);
        exit(1);
    }
    const uint8_t folded = EMBED_CALL(ascii.lower, AsciiCfg, .type = MMGR_ASCII_HEX, .byte = (uint8_t)digit);
    // Explicit cast narrows the value of a digit, below 16, to a byte
    return (uint8_t)((folded <= (uint8_t)'9') ? (folded - (uint8_t)'0') : (folded - (uint8_t)'a' + 10u));
}

/**
 * @brief Returns the bytes that a text of hex digits gives.
 *
 * @param[in] hex The text, ended by a zero byte [BORROWS].
 * @return        Its bytes and their count.
 */
static inline BenchHexBytes bench_hex_bytes(const char *hex)
{
    BenchHexBytes bytes;
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = hex, .cap = ~(size_t)0);
    if ((length / 2u) > BENCH_HEX_BYTES_MAX)
    {
        fprintf(stderr, "bench_hex: a text of %zu digits, past the %u bytes it holds at max\n", length,
                BENCH_HEX_BYTES_MAX);
        exit(1);
    }
    bytes.count = length / 2u;
    for (size_t at = 0u; at < bytes.count; at += 1u)
    {
        // Explicit cast joins two digits, each below 16, to one byte
        bytes.at[at] = (uint8_t)((bench_hex_digit(hex, hex[2u * at]) << 4u) | bench_hex_digit(hex, hex[2u * at + 1u]));
    }
    return bytes;
}

#endif
