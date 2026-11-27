// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sha256_check.c: sha256 checked against NIST's published SHA-256 vectors (CAVP, the SHAVS files vendored under
// utils/test/src/cu/transpiler/qasm/vectors). Each message file gives a length, a message and its digest, the byte
// files a length in whole bytes and the bit files any count of bits; the Monte file gives a seed and the digest of
// every hundred-and-first link of a chain, each link the digest of the three before it positioned end to end (SHAVS
// section 6.4).
//
//     sha256_check <message file>... --monte <monte file>
//
// One line a file with its counts, and a last line with the total. Exit 0 where every vector passes, 1 where any does
// not, 2 where a file did not read.
#include "../../../../../includes/codecs/sha256/sha256.h"
#include "../../../../../includes/platform/platform.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

// the longest line a vector file contains, a long message's hex with its name
#define CHECK_LINE_BYTES 1048576u
// the links a Monte checkpoint is the last of, and the checkpoints the file contains
#define CHECK_MONTE_LINKS 1000u
#define CHECK_MONTE_POINTS 100u

static char s_line[CHECK_LINE_BYTES];
static unsigned char s_message[CHECK_LINE_BYTES / 2u];

// the hex digits at `text` up to the first character that is not one, into `bytes`: the bytes written
static size_t check_hex(const char *text, unsigned char *bytes, size_t room)
{
    size_t written = 0u;
    while ((written < room) && (text[0] != '\0') && (text[1] != '\0'))
    {
        unsigned int value = 0u;
        if (sscanf(text, "%2x", &value) != 1)
        {
            break;
        }
        bytes[written] = (unsigned char)value;
        written += 1u;
        text += 2;
    }
    return written;
}

// the text after `name = ` where `line` begins with it, or NULL
static const char *check_value(const char *line, const char *name)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if ((!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = name,
                     .ci = EMBED_FALSE)) ||
        (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = &line[length], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = " = ", .ci = EMBED_FALSE)))
    {
        return NULL;
    }
    return &line[length + 3u];
}

// every Len, Msg and MD triple of the file at `path` checked: the vectors that passed through `passed` and those that
// did not through `failed`. 1, or 0 where the file did not read
static int check_messages(const char *path, unsigned int *passed, unsigned int *failed)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    // a byte file counts its length in bits too, every one a multiple of 8
    unsigned long long bits = 0ull;
    size_t message_bytes = 0u;
    int have_length = 0;
    int have_message = 0;
    while (fgets(s_line, sizeof(s_line), file) != NULL)
    {
        s_line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = s_line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                          .other = "\r\n")] = '\0';
        const char *value = check_value(s_line, "Len");
        if (value != NULL)
        {
            bits = strtoull(value, NULL, 10);
            have_length = 1;
            have_message = 0;
            continue;
        }
        value = check_value(s_line, "Msg");
        if ((value != NULL) && have_length)
        {
            message_bytes = check_hex(value, s_message, sizeof(s_message));
            have_message = 1;
            continue;
        }
        value = check_value(s_line, "MD");
        if ((value != NULL) && have_message)
        {
            unsigned char expected[SHA256_BYTES];
            unsigned char digest[SHA256_BYTES];
            const int read = check_hex(value, expected, sizeof(expected)) == SHA256_BYTES;
            const int fits = (bits + 7ull) / 8ull <= (unsigned long long)message_bytes;
            if (fits)
            {
                sha256_bits(s_message, bits, digest);
            }
            if (read && fits &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = digest, .other = expected, .bytes = SHA256_BYTES) == 0))
            {
                *passed += 1u;
            }
            else
            {
                *failed += 1u;
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  ");
                    mmgr_line_put(output_line, path);
                    mmgr_line_put(output_line, ": Len = ");
                    mmgr_line_decimal(output_line, bits, 1u);
                    mmgr_line_put(output_line, " does not pass");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
            }
            have_length = 0;
            have_message = 0;
        }
    }
    fclose(file);
    return 1;
}

// the Monte file at `path` checked: the seed, then each checkpoint's digest the last of a chain of links each the
// digest of the three before it, the chain begun again from that digest. 1, or 0 where the file did not read
static int check_monte(const char *path, unsigned int *passed, unsigned int *failed)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    unsigned char seed[SHA256_BYTES];
    int have_seed = 0;
    unsigned int point = 0u;
    while (fgets(s_line, sizeof(s_line), file) != NULL)
    {
        s_line[EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = s_line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                          .other = "\r\n")] = '\0';
        const char *value = check_value(s_line, "Seed");
        if (value != NULL)
        {
            have_seed = check_hex(value, seed, sizeof(seed)) == SHA256_BYTES;
            continue;
        }
        value = check_value(s_line, "MD");
        if ((value == NULL) || !have_seed || (point >= CHECK_MONTE_POINTS))
        {
            continue;
        }
        // the three links before the next, positioned end to end, the oldest first
        unsigned char links[3u * SHA256_BYTES];
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &links[0], .src = seed, .bytes = SHA256_BYTES);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &links[SHA256_BYTES], .src = seed, .bytes = SHA256_BYTES);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &links[2u * SHA256_BYTES], .src = seed, .bytes = SHA256_BYTES);
        unsigned char next[SHA256_BYTES];
        for (unsigned int link = 0u; link < CHECK_MONTE_LINKS; link += 1u)
        {
            sha256(links, sizeof(links), next);
            EMBED_CALL(memor.move_down, MemoriaCfg, .dst = &links[0], .src = &links[SHA256_BYTES],
                       .bytes = 2u * SHA256_BYTES);
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &links[2u * SHA256_BYTES], .src = next, .bytes = SHA256_BYTES);
        }
        unsigned char expected[SHA256_BYTES];
        if ((check_hex(value, expected, sizeof(expected)) == SHA256_BYTES) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = next, .other = expected, .bytes = SHA256_BYTES) == 0))
        {
            *passed += 1u;
        }
        else
        {
            *failed += 1u;
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put(output_line, path);
                mmgr_line_put(output_line, ": COUNT = ");
                mmgr_line_decimal(output_line, point, 1u);
                mmgr_line_put(output_line, " does not pass");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = seed, .src = next, .bytes = SHA256_BYTES);
        point += 1u;
    }
    fclose(file);
    return 1;
}

int program_main(int count, char **words)
{
    unsigned int passed_total = 0u;
    unsigned int failed_total = 0u;
    int monte = 0;
    for (int at = 1; at < count; at += 1)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "--monte", .ci = EMBED_FALSE))
        {
            monte = 1;
            continue;
        }
        unsigned int passed = 0u;
        unsigned int failed = 0u;
        const int read = monte ? check_monte(words[at], &passed, &failed) : check_messages(words[at], &passed, &failed);
        if (!read)
        {
            fprintf(stderr, "  %s did not read\n", words[at]);
            return 2;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, words[at]);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, passed, 1u);
            mmgr_line_put(output_line, " passed, ");
            mmgr_line_decimal(output_line, failed, 1u);
            mmgr_line_put(output_line, " failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        passed_total += passed;
        failed_total += failed;
        monte = 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "sha256 check: ");
        mmgr_line_decimal(output_line, passed_total + failed_total, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, failed_total, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return ((failed_total == 0u) && (passed_total != 0u)) ? 0 : 1;
}
