// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_internal.h: what the ranker's parts share: the set as the ranker holds it, the record machine's loaded
// programs, and the sweep that runs one over host records on the device
#ifndef CASMI_RANK_INTERNAL_H
#define CASMI_RANK_INTERNAL_H

#include "../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "../../../../../src/cu/types/integers/exact_integer.h"
#include "../../../../../src/cu/types/integers/exact_record/exact_record.h"
#include "sim.h"

#include <string>
#include <unordered_map>
#include <vector>

// the text columns the ranker reads, in the order the ingest seals them
#define RANK_TEXTS 7u
#define RANK_LIBRARY 0u
#define RANK_MODE 1u
#define RANK_ADDUCT 2u
#define RANK_FORMULA 3u
#define RANK_KEY 4u
#define RANK_SMILES 5u
#define RANK_MOLECULE 6u

// a value's form and a row's term, as the ingest writes them
#define RANK_FORM_COUNT 0u
#define RANK_FORM_DECIMAL 1u
#define RANK_FORM_DIVIDED 2u
#define RANK_DIVISORS 2u
#define RANK_FORM_KEPT (RANK_FORM_DIVIDED + RANK_DIVISORS)
#define RANK_TERM_FORMS 16u
#define RANK_ROW_EACH 15u

// an absent entry of a list of record numbers
#define RANK_ABSENT 0xFFFFFFFFu

// One double column as the ranker holds it: each value's integer on its row's unit, a kept value's stored bits in its
// place, and its form, each row's term, and row r's values from row_start[r] up to row_start[r + 1]
typedef struct
{
    std::vector<unsigned long long> unit;
    std::vector<unsigned char> form;
    std::vector<unsigned short> term;
    std::vector<unsigned long long> row_start;
} RankColumn;

// The set: each spectrum's text ids, one a text column and RANK_ABSENT where the row has none, its base's stored bits
// and whether it has one, and the three double columns; each text column's distinct strings over every row group of
// every set read into it, and each string's id
typedef struct
{
    unsigned long long spectra;
    std::vector<unsigned int> text[RANK_TEXTS];
    std::vector<std::string> strings[RANK_TEXTS];
    std::unordered_map<std::string, unsigned int> table[RANK_TEXTS];
    std::vector<unsigned long long> base;
    std::vector<unsigned char> base_held;
    RankColumn precursor;
    RankColumn mz;
    RankColumn intensity;
    unsigned int row_groups;
} RankSet;

// a program loaded on the record machine, its outputs and its members' limbs
typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
    unsigned int members;
    unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX];
    std::vector<unsigned int> outputs;
    int loaded;
} RankMachine;

// one output's place in a loaded program's records
typedef struct
{
    unsigned int offset;
    unsigned int bits;
} RankField;

void rank_line_decimal(SimResults *results, const char *before, unsigned long long value);

void rank_line_end(SimResults *results);

void rank_error_line(SimResults *results, const char *what, const EngineError *error);

// a set's crystals read into the ranker's form, every row group in order, after the spectra of any set read before it
int rank_set_load(SimResults *results, const char *set, RankSet *out);

// a built program loaded with `members` members of the limbs given; 0 where it errored, the error printed
int rank_machine_load(SimResults *results, const char *name, ExactRecordProgram *program, const unsigned int *outputs,
                      unsigned int output_count, unsigned int members, const unsigned int *limbs, RankMachine *machine);

void rank_machine_release(RankMachine *machine);

RankField rank_output(const RankMachine *machine, unsigned int output);

// The program run on the device over `lanes` lanes: each member's `bodies` records copied from host memory, the index
// where it is set, and the records written back to `out`; 0 where a copy or the run errored
int rank_sweep(SimResults *results, const char *name, const RankMachine *machine, const unsigned int *const *members,
               const unsigned long long *bodies, const unsigned int *index, unsigned long long lanes, unsigned int *out,
               unsigned long long *microseconds);

// a field of a record read as two's complement into an exact integer
void rank_field_read(const unsigned int *record, RankField field, AnchorExactInteger *value);

// the sign of a field: -1, 0 or 1
int rank_field_sign(const unsigned int *record, RankField field);

// an exact integer written into a record's field, two's complement
void rank_field_put(unsigned int *record, unsigned int offset, unsigned int bits, const AnchorExactInteger *value);

// a value below 2^bits written into a record's field that holds 0
void rank_put_unsigned(unsigned int *record, unsigned int offset, unsigned int bits, unsigned long long value);

// `bits` bits of `limbs` from bit 0 or'ed into a record at `offset`, the field there 0
void rank_bits_copy(unsigned int *record, unsigned int offset, const unsigned int *limbs, unsigned int bits);

unsigned int rank_bits(const AnchorExactInteger *value);

void rank_exact_of(unsigned long long value, AnchorExactInteger *out);

#endif
