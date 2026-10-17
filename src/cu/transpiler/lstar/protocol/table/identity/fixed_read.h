// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed_read.h: what a register holds at the slot, read from its values over the cases, in C for the fixed mode's asks
// and for the check that holds them (The fixed registers asked, Open 6.2)
#ifndef FIXED_READ_H
#define FIXED_READ_H

#ifdef __cplusplus
extern "C"
{
#endif

    // what a register's values over the cases say it holds at the slot
    typedef enum
    {
        FIXED_NOTHING,
        FIXED_LANE_NUMBER,
        FIXED_RECORD,
        FIXED_INDEX,
        FIXED_CASE_WORD_0,
        FIXED_CASE_WORD_1
    } FixedKind;

    // The kind the values `held`, one a case and case t on thread t, are read as, against the host's thread number and
    // the case's words `first` and `second`. A store of 32 bits leaves the record's low word, and each value is read
    // at that width. The lane's number where every value is its case's number; a case word where every value is it;
    // the record where the values step 16 a case and the index where they step 32, each the low word of an address;
    // nothing where they are 0, one constant, or fewer than two
    static inline FixedKind fixed_kind(const unsigned long long *held, const unsigned int *first,
                                       const unsigned int *second, unsigned int cases)
    {
        if (cases < 2u)
        {
            return FIXED_NOTHING;
        }
        int lane = 1;
        int word_0 = 1;
        int word_1 = 1;
        int record = 1;
        int index = 1;
        for (unsigned int at = 0u; at < cases; at += 1u)
        {
            // the store is 32 bits wide: the value is the low word of what the register held
            const unsigned int value = (unsigned int)(held[at] & 0xffffffffull);
            lane = lane && (value == at);
            word_0 = word_0 && (value == first[at]);
            word_1 = word_1 && (value == second[at]);
            if ((at + 1u) < cases)
            {
                // the step between two cases' values, at the width the store leaves them
                const unsigned int step = (unsigned int)((held[at + 1u] - held[at]) & 0xffffffffull);
                record = record && (step == 16u);
                index = index && (step == 32u);
            }
        }
        return lane     ? FIXED_LANE_NUMBER
               : word_0 ? FIXED_CASE_WORD_0
               : word_1 ? FIXED_CASE_WORD_1
               : record ? FIXED_RECORD
               : index  ? FIXED_INDEX
                        : FIXED_NOTHING;
    }

    // the question a kind's row is written under, `run answers <n> <question>`, and empty where nothing is fixed
    static inline const char *fixed_kind_question(FixedKind kind)
    {
        switch (kind)
        {
        case FIXED_LANE_NUMBER:
            return "fixed lane_number";
        case FIXED_RECORD:
            return "fixed record";
        case FIXED_INDEX:
            return "fixed index";
        case FIXED_CASE_WORD_0:
            return "case word 0";
        case FIXED_CASE_WORD_1:
            return "case word 1";
        default:
            return "";
        }
    }

#ifdef __cplusplus
}
#endif

#endif
