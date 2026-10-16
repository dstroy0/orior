// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// slot.c: rule 14, the instruction at the slot held before the part where it writes a register the kernel holds fixed
// around the slot, the record's address or the index's, read through the vendor's own instruction set and the part's
// answers in the .ksc
#include "cubin_safe_internal.h"

#include "../tables/read.h"

static unsigned long long cubin_slot_word(const unsigned char *bytes)
{
    unsigned long long value = 0ull;
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        value |= (unsigned long long)bytes[byte] << (8u * byte);
    }
    return value;
}

// the register the operand `operand` of `class_` names in the encoding `low` and `high`, or CUBIN_REGISTER_ZERO where
// the class writes no field from that operand
static unsigned int cubin_slot_operand(const TablesClass *class_, const char *operand, unsigned long long low,
                                       unsigned long long high)
{
    const TablesField *const field = tables_field_by_operand(class_, operand);
    if (field == NULL)
    {
        return CUBIN_REGISTER_ZERO;
    }
    const unsigned long long value = tables_field_read(field, low, high);
    // a register field holds eight bits, and RZ is the largest number it holds
    return (value <= (unsigned long long)CUBIN_REGISTER_ZERO) ? (unsigned int)value : CUBIN_REGISTER_ZERO;
}

// how many registers the record's address spans: the class of each instruction after the slot that reads the record's
// register as its address operand Ra says, two where its `e` names a 64-bit address, one otherwise; one where no
// instruction after the slot reads it
static unsigned int cubin_record_span(const struct TablesSetTag *tables, const unsigned char *code,
                                      unsigned long long code_size, unsigned long long slot_at, unsigned int record)
{
    unsigned int span = 1u;
    for (unsigned long long place = slot_at + 16ull; (place + 16ull) <= code_size; place += 16ull)
    {
        const unsigned long long low = cubin_slot_word(&code[place]);
        const unsigned long long high = cubin_slot_word(&code[place + 8u]);
        const TablesClass *const class_ = tables_class_of(tables, low, high);
        if ((class_ == NULL) || (cubin_slot_operand(class_, "Ra", low, high) != record))
        {
            continue;
        }
        const TablesField *const extended = tables_field_by_operand(class_, "e");
        if ((extended != NULL) && (tables_field_read(extended, low, high) != 0ull))
        {
            span = 2u;
        }
    }
    return span;
}

int cubin_slot_writes_fixed(const SassMachine *machine, const struct TablesSetTag *tables, const unsigned char *code,
                            unsigned long long code_size, unsigned long long place)
{
    if ((machine->slot_place == SASS_MACHINE_UNANSWERED) || (machine->opening_least == SASS_MACHINE_UNANSWERED) ||
        (machine->slot_place < machine->opening_least))
    {
        return 0;
    }
    const unsigned long long slot_at = (unsigned long long)(machine->slot_place - machine->opening_least) * 16ull;
    if ((place != slot_at) || ((slot_at + 16ull) > code_size))
    {
        return 0;
    }
    const unsigned long long low = cubin_slot_word(&code[place]);
    const unsigned long long high = cubin_slot_word(&code[place + 8u]);
    const TablesClass *const class_ = tables_class_of(tables, low, high);
    if (class_ == NULL)
    {
        return 0;
    }
    const unsigned int destination = cubin_slot_operand(class_, "Rd", low, high);
    if (destination == CUBIN_REGISTER_ZERO)
    {
        return 0;
    }
    if ((machine->fixed_index != SASS_MACHINE_UNANSWERED) && (destination == machine->fixed_index))
    {
        return 1;
    }
    if (machine->fixed_record == SASS_MACHINE_UNANSWERED)
    {
        return 0;
    }
    const unsigned int span = cubin_record_span(tables, code, code_size, slot_at, machine->fixed_record);
    return (destination >= machine->fixed_record) && (destination < (machine->fixed_record + span));
}
