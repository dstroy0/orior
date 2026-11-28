// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.h: the vendor's instruction set read: each CLASS with its opcode, its fields by name, each a run of bits or
// several, and its constant-bank operand where it contains one
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

    // one field of the ENCODING of a class, `BITS_<n>_<hi>_<lo>..._<name>=<operand>`. It has its runs in the order that
    // the line gives them, the high run first, and what goes in it. That is an operand of the FORMAT by name, or a
    // value that the class fixes, `*7`
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

    // one CLASS: its name, its opcode as OPCODES gives it, and its fields. Where the FORMAT holds a `C:` operand, it
    // has the bank and offset fields of its constant-bank operand by name, empty where the FORMAT holds none. It has
    // the width in bits of the offset operand as the FORMAT gives it, `SImm(17)`, with its sign bit counted, 0 where
    // the FORMAT gives none
    typedef struct
    {
        char name[TABLES_NAME + 16u];
        unsigned long long opcode;
        unsigned int fields;
        TablesField field[TABLES_CLASS_FIELDS];
        char bank[TABLES_NAME];
        char offset[TABLES_NAME];
        unsigned int offset_bits;
        // the size operand where the FORMAT holds one, `/SZ_U8_S8_U16_S16_32_64("32"):sz`. It has its name, and the
        // bytes that each value of its field reads, in the order that the FORMAT lists the choices. It is empty where
        // the FORMAT holds none
        char size[TABLES_NAME];
        unsigned int sizes;
        unsigned int size_bytes[TABLES_SIZES];
        // the size operand's list as the FORMAT names it, `SZ_U8_S8_U16_S16_32_64`, which a condition's constant
        // names a choice under, `SZ_U8_S8_U16_S16_32_64@"64"`; empty where the class contains no size operand
        char size_list[TABLES_NAME];
        // the CONDITIONS of the class on its registers, under OOR_REG_ERROR and MISALIGNED_REG_ERROR. Each is an
        // expression over the operands of the class that is true for a legal encoding. The reader stores it as the
        // text gives it, with no closing colon, and does not store one longer than its room
        char condition[TABLES_CONDITIONS][TABLES_CONDITION_TEXT];
        unsigned int conditions;
    } TablesClass;

    typedef struct TablesSetTag
    {
        TablesClass *class_;
        unsigned int count;
        unsigned int room;
    } TablesSet;

    // Every CLASS of the instruction set `text`, `size` long, read into `set`. The return is 1, or 0 where there was no
    // memory, with the set that holds what the reader read before
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

    // The bytes a read of `class_` takes in the encoding `low` and `high`: the choice its size operand's field stores,
    // as the FORMAT lists the choices. Four where the class names no size operand or the field stores a choice past
    // the list
    unsigned int tables_class_bytes(const TablesClass *class_, unsigned long long low, unsigned long long high);

    // The byte offset into the constant bank that the encoding `low` and `high` of `class_` reads. It is the value of
    // the offset field, scaled by the bits that the field leaves below the operand magnitude. The operand is `SImm(n)`
    // with its sign
    unsigned long long tables_class_offset(const TablesClass *class_, unsigned long long low, unsigned long long high);

    // Whether the encoding `low` and `high` meets every register condition of `class_`. The return is 1 where each is
    // true, or is one that this reader cannot judge. It is 0 where one fails, with the text of that one through
    // `failed` where `failed` is not NULL.
    //
    // The reader judges a condition over the fields of the class by operand. It also uses the own constants of the
    // registers: RZ, R<n>, URZ, UR<n>, PT and UPT. It also uses the register counts %MAX_REG_COUNT and
    // %MAX_UNIFORM_REG_COUNT, and the choices of the size operand under its list. It does not judge one that names a
    // thing outside these
    int tables_class_legal(const TablesClass *class_, unsigned long long low, unsigned long long high,
                           const char **failed);
    // The encoding `low` and `high`, described in one line into `text`, sized for `room`. It has the name of its class,
    // or `-` where the opcode of no class reads it. Where the class reads the constant bank, it has the bank, the byte
    // offset and the bytes read. The return is 1 where a class reads it, 0 where none does
    int tables_class_described(const TablesSet *set, unsigned long long low, unsigned long long high, char *text,
                               size_t room);

#ifdef __cplusplus
}
#endif

#endif
