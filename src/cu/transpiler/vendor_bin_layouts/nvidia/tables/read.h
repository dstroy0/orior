// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.h: the vendor's instruction set read: each CLASS with its opcode, its fields by name, each a run of bits or
// several, and its constant-bank operand where it holds one
#ifndef TABLES_READ_H
#define TABLES_READ_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define TABLES_NAME 48u
#define TABLES_FIELD_RUNS 4u
#define TABLES_CLASS_FIELDS 48u
#define TABLES_SIZES 8u
#define TABLES_CONDITIONS 16u
#define TABLES_CONDITION_TEXT 320u

    // one run of bits of a field, `first` the low bit and `last` the high, of the 128
    typedef struct
    {
        unsigned int first;
        unsigned int last;
    } TablesBits;

    // one field of a class's ENCODING, `BITS_<n>_<hi>_<lo>..._<name>=<operand>`: its runs in the order the line
    // gives them, the high run first, and what is written in it: an operand of the FORMAT by name, or a value
    // fixed by the class, `*7`
    typedef struct
    {
        char name[TABLES_NAME];
        char operand[TABLES_NAME];
        unsigned int runs;
        TablesBits run[TABLES_FIELD_RUNS];
        unsigned int width;
        int fixed;
        unsigned long long value;
    } TablesField;

    // one CLASS: its name, its opcode as OPCODES gives it, its fields, and its constant-bank operand's bank and
    // offset fields by name where the FORMAT holds a `C:` operand, empty where it holds none, with the offset
    // operand's width in bits as the FORMAT gives it, `SImm(17)`, its sign bit counted, 0 where it gives none
    typedef struct
    {
        char name[TABLES_NAME + 16u];
        unsigned long long opcode;
        unsigned int fields;
        TablesField field[TABLES_CLASS_FIELDS];
        char bank[TABLES_NAME];
        char offset[TABLES_NAME];
        unsigned int offset_bits;
        // the size operand where the FORMAT holds one, `/SZ_U8_S8_U16_S16_32_64("32"):sz`: its name, and the bytes
        // each value of its field reads, in the order the FORMAT lists the choices; empty where it holds none
        char size[TABLES_NAME];
        unsigned int sizes;
        unsigned int size_bytes[TABLES_SIZES];
        // the register operand a memory address is read from where the FORMAT holds one in brackets,
        // `[ NonZeroRegister:Ra + SImm(24/0)*:Ra_offset ]`, by name; empty where the class reads no memory address,
        // and where its bracket is the constant bank's
        char address[TABLES_NAME];
        // the size operand's list as the FORMAT names it, `SZ_U8_S8_U16_S16_32_64`, which a condition's constant
        // names a choice under, `SZ_U8_S8_U16_S16_32_64@"64"`; empty where the class holds no size operand
        char size_list[TABLES_NAME];
        // the class's CONDITIONS on its registers, under OOR_REG_ERROR and MISALIGNED_REG_ERROR: each an
        // expression over the class's operands that holds for a legal encoding, kept as the text gives it without
        // its closing colon; one longer than its room is not kept
        char condition[TABLES_CONDITIONS][TABLES_CONDITION_TEXT];
        unsigned int conditions;
    } TablesClass;

    typedef struct TablesSetTag
    {
        TablesClass *class_;
        unsigned int count;
        unsigned int room;
    } TablesSet;

    // Every CLASS of the instruction set `text`, `size` long, read into `set`: 1, or 0 where there was no memory,
    // the set holding what was read before
    int tables_set_read(TablesSet *set, const unsigned char *text, size_t size);

    void tables_set_freed(TablesSet *set);

    // The field of `class_` named `name`, or NULL
    const TablesField *tables_field_named(const TablesClass *class_, const char *name);

    // The field of `class_` written from the FORMAT's operand `operand`, the first where several are, or NULL
    const TablesField *tables_field_by_operand(const TablesClass *class_, const char *operand);

    // The value of `field` read off the encoding `low` and `high`, its runs taken high to low
    unsigned long long tables_field_read(const TablesField *field, unsigned long long low, unsigned long long high);

    // `value` written into `field` of the encoding `low` and `high`, its runs taken high to low
    void tables_field_write(const TablesField *field, unsigned long long *low, unsigned long long *high,
                            unsigned long long value);

    // The first class of `set` whose opcode field, read off the encoding `low` and `high`, is its opcode; NULL
    // where none is
    const TablesClass *tables_class_of(const TablesSet *set, unsigned long long low, unsigned long long high);

    // The bytes a read of `class_` takes in the encoding `low` and `high`: the choice its size operand's field holds,
    // as the FORMAT lists the choices; four where the class names no size operand or the field holds a choice past
    // the list
    unsigned int tables_class_bytes(const TablesClass *class_, unsigned long long low, unsigned long long high);

    // The byte offset into the constant bank the encoding `low` and `high` of `class_` reads: the offset field's
    // value scaled by the bits the field leaves below the operand's magnitude (the operand `SImm(n)` with its sign)
    unsigned long long tables_class_offset(const TablesClass *class_, unsigned long long low, unsigned long long high);

    // Whether the encoding `low` and `high` holds to every register condition of `class_`: 1 where each holds or is
    // one this reader cannot judge, 0 where one fails, that one's text given through `failed` where it is not NULL.
    // A condition is judged over the class's fields by operand, the registers' own constants, RZ, R<n>, URZ, UR<n>,
    // PT and UPT, the register counts %MAX_REG_COUNT and %MAX_UNIFORM_REG_COUNT, and the size operand's choices
    // under its list; one naming anything else is not judged
    int tables_class_legal(const TablesClass *class_, unsigned long long low, unsigned long long high,
                           const char **failed);
    // The encoding `low` and `high` described in one line into `text`, which holds `room`: its class's name, or `-`
    // where no class's opcode reads it, and, where the class reads the constant bank, the bank, the byte offset and
    // the bytes read. 1 where a class reads it, 0 where none does
    int tables_class_described(const TablesSet *set, unsigned long long low, unsigned long long high, char *text,
                               size_t room);

#ifdef __cplusplus
}
#endif

#endif
