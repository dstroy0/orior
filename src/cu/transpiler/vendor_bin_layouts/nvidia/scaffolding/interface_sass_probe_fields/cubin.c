// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin.c: the container: a file read and written, a word read and written, the kernel, a splice, a place found, the container written, the keys, a flip held safe, the first field and the operand at a place
#include "fields_internal.h"

SassMachine s_machine;
unsigned char s_pattern[FIELDS_CUBIN_BYTES];
static unsigned char s_cubin[FIELDS_CUBIN_BYTES];
unsigned char s_code[FIELDS_CODE_BYTES];
char s_kernel_text[FIELDS_TEXT_BYTES];
char s_asking[FIELDS_TEXT_BYTES];
static unsigned int s_exits[FIELDS_EXITS];
char s_kernel[128];
unsigned long long s_pattern_size;

// each operation key, the low word's bits the sweep puts every value of: whether the machine file holds a form under
// it, and whether any form it holds there transfers control or waits
static unsigned char s_key_known[SASS_OPERATION_MASK + 1u];

static unsigned char s_key_control[SASS_OPERATION_MASK + 1u];

// `path` read whole into `bytes`, which holds `room`: the bytes read, 0 where it was not read
unsigned long long fields_file_read(const char *path, unsigned char *bytes, unsigned long long room)
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

static int fields_file_write(const char *path, const unsigned char *bytes, unsigned long long size)
{
    FILE *const file = fopen(path, "wb");
    const int written = (file != NULL) && (fwrite(bytes, 1u, (size_t)size, file) == size);
    const int closed = (file != NULL) && (fclose(file) == 0);
    return written && closed;
}

// the eight bytes at `bytes` read as one word, lowest first, and written back
unsigned long long fields_word_read(const unsigned char *bytes)
{
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        word |= (unsigned long long)bytes[at] << (8u * at);
    }
    return word;
}

static void fields_word_write(unsigned char *bytes, unsigned long long word)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        bytes[at] = (unsigned char)(word >> (8u * at));
    }
}

// the kernel the kernel opens with, `.text.<kernel>:`, into `kernel`: 1, or 0 where it opens otherwise
int fields_kernel(const char *kernel_text, char *kernel, size_t room)
{
    if (strncmp(kernel_text, ".text.", 6u) != 0)
    {
        return 0;
    }
    const size_t length = strcspn(kernel_text + 6, ":\r\n");
    return (kernel_text[6u + length] == ':') && (snprintf(kernel, room, "%.*s", (int)length, kernel_text + 6) < (int)room);
}

// the kernel with its line that begins `IADD3 ` replaced by `lines`, into `text`: 1, or 0 where the kernel holds no such
// line or `text` will not hold the whole
int fields_splice(const char *kernel_text, const char *lines, char *text, size_t room)
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

// the instruction of `code`, `count` of them, whose operation bits are `low` and `high`'s: its number, or `count`
// where none is or more than one is. The scheduler's bits, 105 and above, are left out of the match
unsigned int fields_find(const unsigned char *code, unsigned int count, unsigned long long low,
                                unsigned long long high)
{
    const unsigned long long mask = (1ull << (FIELDS_OPERATION_BITS - 64u)) - 1ull;
    unsigned int found = count;
    unsigned int matches = 0u;
    for (unsigned int number = 0u; number < count; number += 1u)
    {
        const unsigned long long one = fields_word_read(&code[16u * number]);
        const unsigned long long two = fields_word_read(&code[(16u * number) + 8u]);
        if ((one == low) && ((two & mask) == (high & mask)))
        {
            found = number;
            matches += 1u;
        }
    }
    return (matches == 1u) ? found : count;
}

// `code` of `count` instructions, with the instruction at `place` turned at bit `bit` where `bit` is under 128, laid
// into a cubin and written to `path`: 1, or 0 with the reason printed. A bit of 128 writes the code as it stands
int fields_cubin(const unsigned char *code, unsigned int count, unsigned int place, unsigned int bit,
                        const char *path)
{
    static unsigned char s_turned[FIELDS_CODE_BYTES];
    const unsigned long long code_size = 16ull * count;
    memcpy(s_turned, code, (size_t)code_size);
    if (bit < 128u)
    {
        const unsigned int at = (16u * place) + ((bit < 64u) ? 0u : 8u);
        unsigned long long word = fields_word_read(&s_turned[at]);
        word ^= 1ull << (bit % 64u);
        fields_word_write(&s_turned[at], word);
    }
    CubinWrite written;
    memset(&written, 0, sizeof(written));
    written.pattern = s_pattern;
    written.pattern_size = s_pattern_size;
    written.kernel = s_kernel;
    written.code = s_turned;
    written.code_size = code_size;
    written.registers = FIELDS_REGISTERS;
    written.exit_count = cubin_exits_find(s_turned, code_size, sass_exit_encoding(&s_machine), s_exits, FIELDS_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ull;
    if (!cubin_write(&written, s_cubin, sizeof(s_cubin), &size) || !fields_file_write(path, s_cubin, size))
    {
        printf("  %s was not written\n", path);
        return 0;
    }
    return 1;
}

// the key table filled from the machine file's forms: a key is known where any form sits under it, and control where
// any form under it transfers control or waits. The sweep put every key to the disassembler from each of several
// carriers, and the file holds every operation a key named there. The counts go to the standard output
void fields_keys(void)
{
    unsigned int known = 0u;
    unsigned int control = 0u;
    for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
    {
        const unsigned long long key = s_machine.form[at].low & SASS_OPERATION_MASK;
        known += (s_key_known[key] == 0u) ? 1u : 0u;
        s_key_known[key] = 1u;
        // a turned bit that writes a control transfer or a wait is run as it is and not refused: its key is held off
        // the part
        if (sass_operation_control_or_wait(s_machine.form[at].operation) && (s_key_control[key] == 0u))
        {
            s_key_control[key] = 1u;
            control += 1u;
        }
    }
    printf("operation keys: %u known, %u holding a control or wait form\n", known, control);
}

// 1 where the instruction at `place` in s_code, turned at `bit`, is safe to run on the part. The operation key names
// the operation, and the turned encoding is run only where the machine file holds forms under its key and no control
// transfer or wait among them: such an instruction falls through whatever its other bits hold, and cannot loop or
// stall the part. A turn past the key leaves the key, and the instruction's own operation, as they were. `name` takes
// the operation the turned encoding reads back as, or the key where no form reads it back. A `bit` of 128 turns nothing
int fields_flip_safe(unsigned int place, unsigned int bit, char *name, size_t room)
{
    unsigned long long low = fields_word_read(&s_code[16u * place]);
    unsigned long long high = fields_word_read(&s_code[(16u * place) + 8u]);
    if (bit < 64u)
    {
        low ^= 1ull << bit;
    }
    else if (bit < 128u)
    {
        high ^= 1ull << (bit - 64u);
    }
    const unsigned long long key = low & SASS_OPERATION_MASK;
    char text[256];
    if (sass_encoding_read(&s_machine, low, high, 0ull, text, sizeof(text)))
    {
        SassInstructionParts parts;
        sass_instruction_read(text, &parts);
        snprintf(name, room, "%s", parts.operation);
    }
    else
    {
        snprintf(name, room, "key_0x%03llx", key);
    }
    return (s_key_known[key] != 0u) && (s_key_control[key] == 0u);
}

// the first instruction of the file at `path` that is a form to probe, into `instruction`: 1, or 0 where the file
// holds none. The kernel's own IADD3 and blank lines are passed over, as the write pass passes them
int fields_first(const char *path, char *instruction, size_t room)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    char line[512];
    int found = 0;
    while (!found && (fgets(line, sizeof(line), file) != NULL))
    {
        line[strcspn(line, "\r\n")] = '\0';
        const char *const walk = line + strspn(line, " \t");
        if ((*walk != '\0') && (strncmp(walk, "IADD3 R8", 8u) != 0))
        {
            found = snprintf(instruction, room, "%s", walk) < (int)room;
        }
    }
    fclose(file);
    return found;
}

// the operand whose run covers operation bit `bit` in `form`, or the operand count where none does
unsigned int fields_operand_at(const SassForm *form, unsigned int bit)
{
    for (unsigned int at = 0u; at < form->runs; at += 1u)
    {
        if ((bit >= form->run[at].first) && (bit <= form->run[at].last))
        {
            return form->run[at].operand;
        }
    }
    return form->operands;
}
