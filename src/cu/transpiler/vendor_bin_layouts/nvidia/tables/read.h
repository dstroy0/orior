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

#ifdef __cplusplus
}
#endif

#endif
