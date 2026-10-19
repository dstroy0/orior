// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// probe.c: the probe: a file read and written, bits and words read and written, the binary and the text, the kernel, a place found and written, and one question run on the part
#include "unprinted_internal.h"

SassMachine s_machine;

// the runner, the folder a cubin is written into, the kernel and the pattern cubin's size, as main was given them
const char *s_runner;

const char *s_folder;
char s_kernel[128];
unsigned long long s_pattern_size;
unsigned char s_pattern[UNPRINTED_CUBIN_BYTES];
static unsigned char s_cubin[UNPRINTED_CUBIN_BYTES];
unsigned char s_code[UNPRINTED_CODE_BYTES];
unsigned char s_turned[UNPRINTED_CODE_BYTES];
char s_kernel_text[UNPRINTED_TEXT_BYTES];
char s_asking[UNPRINTED_TEXT_BYTES];
static char s_output[UNPRINTED_OUTPUT_BYTES];
static unsigned int s_exits[UNPRINTED_EXITS];

// `path` read whole into `bytes`, which holds `room` of them: how many were read, 0 where the file was not read
unsigned long long unprinted_file_read(const char *path, unsigned char *bytes, unsigned long long room)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0ull;
    }
    const unsigned long long read = (unsigned long long)fread(bytes, 1u, (size_t)room, file);
    fclose(file);
    return read;
}

static int unprinted_file_write(const char *path, const unsigned char *bytes, unsigned long long size)
{
    FILE *const file = fopen(path, "wb");
    const int written = (file != NULL) && (fwrite(bytes, 1u, (size_t)size, file) == size);
    const int closed = (file != NULL) && (fclose(file) == 0);
    return written && closed;
}

// `bits` bits of `value` written into the encoding at `first`, the two words taken as one of 128 bits
static void unprinted_bits_write(unsigned long long *low, unsigned long long *high, unsigned int first,
                                 unsigned int bits, unsigned long long value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = first + bit;
        unsigned long long *const word = (at < 64u) ? low : high;
        const unsigned long long mask = 1ull << (at % 64u);
        *word = ((value >> bit) & 1ull) ? (*word | mask) : (*word & ~mask);
    }
}

// `bits` bits of the encoding read from `first`
unsigned long long unprinted_bits_read(unsigned long long low, unsigned long long high, unsigned int first,
                                              unsigned int bits)
{
    unsigned long long value = 0ull;
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = first + bit;
        const unsigned long long word = (at < 64u) ? low : high;
        value |= ((word >> (at % 64u)) & 1ull) << bit;
    }
    return value;
}

// the eight bytes at `bytes` as one word, the lowest first
static unsigned long long unprinted_word_read(const unsigned char *bytes)
{
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        word |= (unsigned long long)bytes[at] << (8u * at);
    }
    return word;
}

static void unprinted_word_write(unsigned char *bytes, unsigned long long word)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // each byte is the word's next eight bits, which a byte holds
        bytes[at] = (unsigned char)(word >> (8u * at));
    }
}

// `value` written as `bits` binary digits, the highest first, into `text`
void unprinted_binary(unsigned long long value, unsigned int bits, char *text)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        text[bit] = ((value >> (bits - 1u - bit)) & 1ull) ? '1' : '0';
    }
    text[bits] = '\0';
}

// the kernel's text with its line that begins `IADD3 ` replaced by `lines`, into `text`: 1, or 0 where the kernel holds
// no such line or `text` will not hold the whole
int unprinted_text(const char *kernel_text, const char *lines, char *text, size_t room)
{
    const char *const line = strstr(kernel_text, "\nIADD3 ");
    if (line == NULL)
    {
        return 0;
    }
    const char *const after = strchr(line + 1, '\n');
    const size_t before = (size_t)(line - kernel_text) + 1u;
    return snprintf(text, room, "%.*s%s%s", (int)before, kernel_text, lines, (after != NULL) ? after : "") < (int)room;
}

// the kernel the kernel's text opens with, `.text.<kernel>:`, into `kernel`: 1, or 0 where it opens otherwise
int unprinted_kernel(const char *kernel_text, char *kernel, size_t room)
{
    if (strncmp(kernel_text, ".text.", 6u) != 0)
    {
        return 0;
    }
    const size_t length = strcspn(kernel_text + 6, ":\r\n");
    return (kernel_text[6u + length] == ':') && (snprintf(kernel, room, "%.*s", (int)length, kernel_text + 6) < (int)room);
}

// every instruction of `code`, `count` of them, whose operation bits are `low` and `high`'s, their numbers in order
// into `found`, which holds `room`: how many there are
unsigned int unprinted_find(const unsigned char *code, unsigned int count, unsigned long long low,
                                   unsigned long long high, unsigned int *found, unsigned int room)
{
    unsigned int matches = 0u;
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        const unsigned long long one = unprinted_word_read(&code[16u * number]);
        const unsigned long long other = unprinted_word_read(&code[(16u * number) + 8u]);
        if ((one == low) && ((other & UNPRINTED_OPERATION_HIGH) == (high & UNPRINTED_OPERATION_HIGH)))
        {
            if (matches < room)
            {
                found[matches] = number;
            }
            matches += 1u;
        }
    }
    return matches;
}

// the encoding at place `place` of `code` with `bits` bits from `first` set to `value`, and the bits each question
// holds fixed beside it
void unprinted_place_write(unsigned char *code, unsigned int place, const UnprintedFixed *fixed,
                                  unsigned int fixed_count, unsigned int first, unsigned int bits,
                                  unsigned long long value)
{
    unsigned long long low = unprinted_word_read(&code[16u * place]);
    unsigned long long high = unprinted_word_read(&code[(16u * place) + 8u]);
    for (unsigned int number = 0u; number < fixed_count; number += 1u)
    {
        unprinted_bits_write(&low, &high, fixed[number].first, fixed[number].bits, fixed[number].value);
    }
    unprinted_bits_write(&low, &high, first, bits, value);
    unprinted_word_write(&code[16u * place], low);
    unprinted_word_write(&code[(16u * place) + 8u], high);
}

// the cubin of `code` run on the part over the case: 1 with the four words it answered in `answered`, or 0 where the
// cubin was not written or did not run, with how it ended in `ended`
int unprinted_run(const unsigned char *code, unsigned long long code_size, unsigned int *answered, char *ended,
                         size_t room)
{
    CubinWrite written;
    memset(&written, 0, sizeof(written));
    written.pattern = s_pattern;
    written.pattern_size = s_pattern_size;
    written.kernel = s_kernel;
    written.code = code;
    written.code_size = code_size;
    written.registers = UNPRINTED_REGISTERS;
    written.exit_count = cubin_exits_find(code, code_size, sass_exit_encoding(&s_machine), s_exits, UNPRINTED_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ull;
    char cubin[1024];
    snprintf(cubin, sizeof(cubin), "%s/unprinted.cubin", s_folder);
    if (!cubin_write(&written, s_cubin, sizeof(s_cubin), &size) || !unprinted_file_write(cubin, s_cubin, size))
    {
        snprintf(ended, room, "not written");
        return 0;
    }
    // the gate the carrier and the rest of the scaffolding share: a turned bit whose slot transfers control or waits
    // would loop or stall the part and take the display down with it, and is held off the part here, never handed to
    // the driver. The register count is left open (UNPRINTED_REGISTERS), since which registers the part allots is one
    // of the things this probe reads
    unsigned long long held_at = 0ull;
    if (cubin_safe_image(&s_machine, s_cubin, size, written.registers, &held_at) != CUBIN_SAFE)
    {
        snprintf(ended, room, "held off the part by cubin_safe at byte %llu", held_at);
        return 0;
    }
    char output[1024];
    snprintf(output, sizeof(output), "%s/unprinted.out", s_folder);
    // one case of eight words, none of the other seven zero, the case every question of the probe is asked over
    char *const command[] = {(char *)s_runner,   (char *)"run",      cubin,              (char *)"0000000b",
                             (char *)"00000007", (char *)"00000003", (char *)"00000005", (char *)"00000002",
                             (char *)"00000009", (char *)"00000001", (char *)"00000004", NULL};
    const InterfaceProbe probe = {command, output, UNPRINTED_LIMIT};
    InterfaceAnswer answer;
    memset(&answer, 0, sizeof(answer));
    answer.output = s_output;
    answer.output_capacity = sizeof(s_output);
    EngineError error;
    memset(&error, 0, sizeof(error));
    if (interface_probe_run(&probe, &answer, &error) != 0L)
    {
        snprintf(ended, room, "the interface failed");
        return 0;
    }
    const char *const line = strstr(s_output, "answered ");
    if ((answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull) || (line == NULL))
    {
        // the runner names the error the part raised on a line of its own, "error <number> <name>"
        const char *const raised = strstr(s_output, "error ");
        const char *const name = (raised != NULL) ? strchr(raised + 6, ' ') : NULL;
        snprintf(ended, room, "%s, code %llu%s%.*s", interface_ending_name(answer.ending), answer.code,
                 (name != NULL) ? ", " : "", (name != NULL) ? (int)strcspn(name + 1, "\r\n") : 0,
                 (name != NULL) ? name + 1 : "");
        return 0;
    }
    // the runner prints each answered word as eight hexadecimal digits, which an unsigned int holds
    const char *walk = line + 9;
    for (unsigned int word = 0u; word < UNPRINTED_COPIES; word += 1u)
    {
        char *after = NULL;
        answered[word] = (unsigned int)strtoul(walk, &after, 16);
        walk = after;
    }
    return 1;
}
