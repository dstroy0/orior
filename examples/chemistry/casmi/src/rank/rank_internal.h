// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_internal.h: what the ranker's parts share: the spectra as the ranker holds them, the record machine's loaded
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

// the text columns the ranker reads
#define RANK_TEXTS 7u
#define RANK_LIBRARY 0u
#define RANK_MODE 1u
#define RANK_ADDUCT 2u
#define RANK_FORMULA 3u
#define RANK_KEY 4u
#define RANK_SMILES 5u
#define RANK_MOLECULE 6u

// an absent entry of a list of record numbers
#define RANK_ABSENT 0xFFFFFFFFu

// the most device bytes a piece of a sweep holds: the records its lanes read, its index and the records it writes
#define RANK_SWEEP_BYTES (1ull << 30u)

// one double column as the file stores it: each value's 64 bits, and row r's values from row_start[r] up to
// row_start[r + 1]
typedef struct
{
    std::vector<unsigned long long> bits;
    std::vector<unsigned long long> row_start;
} RankColumn;

// The spectra of every file read: each spectrum's text ids, one a text column and RANK_ABSENT where the row has none,
// and the three double columns; each text column's distinct strings over every row group of every file, and each
// string's id
typedef struct
{
    unsigned long long spectra;
    std::vector<unsigned int> text[RANK_TEXTS];
    std::vector<std::string> strings[RANK_TEXTS];
    std::unordered_map<std::string, unsigned int> table[RANK_TEXTS];
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

// a parquet file's spectra read, every row group in order, after the spectra of any file read before it; 0 where a
// row group did not read, the reason printed
int rank_file_load(SimResults *results, const char *path, RankSet *out);

// a built program loaded with `members` members of the limbs given; 0 where it errored, the error printed
int rank_machine_load(SimResults *results, const char *name, ExactRecordProgram *program, const unsigned int *outputs,
                      unsigned int output_count, unsigned int members, const unsigned int *limbs, RankMachine *machine);

void rank_machine_release(RankMachine *machine);

RankField rank_output(const RankMachine *machine, unsigned int output);

// a device buffer of at least `bytes`, grown where it holds fewer; 0 where it did not allocate
int rank_device_room(void **buffer, size_t *room, size_t bytes);

// The program run on the device over `lanes` lanes: each member's `bodies` records copied from host memory, the index
// where it is set, and the records written back to `out`, a piece of lanes at a time; 0 where a copy or the run errored
int rank_sweep(SimResults *results, const char *name, const RankMachine *machine, const unsigned int *const *members,
               const unsigned long long *bodies, const unsigned int *index, unsigned long long lanes, unsigned int *out,
               unsigned long long *microseconds);

// the match's buffers on the device, each grown to the most a chunk has asked: the envelopes, a query's peaks, a
// chunk's reference peaks and its spectra's first lanes, first peaks and peak counts, and its lanes' index, records,
// order and kept records
typedef struct
{
    unsigned int *envelope;
    unsigned int *query;
    unsigned int *reference;
    unsigned long long *lane_first;
    unsigned int *atom_first;
    unsigned int *peaks;
    unsigned int *index;
    unsigned int *out;
    unsigned int *order;
    unsigned int *kept;
    size_t envelope_room;
    size_t query_room;
    size_t reference_room;
    size_t lane_first_room;
    size_t atom_first_room;
    size_t peaks_room;
    size_t index_room;
    size_t out_room;
    size_t order_room;
    size_t kept_room;
} RankMatchDevice;

// One chunk of the match: the query's peaks against each of the chunk's spectra in turn, spectrum s's lanes from
// lane_first[s] up to lane_first[s + 1], query peak q against its peak p at lane lane_first[s] + q peaks[s] + p, its
// peaks from atom_first[s] in reference_atoms; `kept` the program's output that is 1 where a lane's weight is above 0
typedef struct
{
    const RankMachine *machine;
    RankField kept;
    const unsigned int *reference_atoms;
    unsigned long long reference_peaks;
    const unsigned long long *lane_first;
    const unsigned int *atom_first;
    const unsigned int *peaks;
    size_t spectra;
    unsigned long long query_peaks;
    unsigned long long envelopes;
    unsigned int mode;
    unsigned long long lanes;
} RankMatchChunk;

// the envelopes' records copied to the device once; 0 where a copy failed
int rank_match_open(RankMatchDevice *device, const unsigned int *envelope, size_t envelope_limbs);

void rank_match_close(RankMatchDevice *device);

// a query's peak records copied to the device, the query member of every chunk until the next
int rank_match_query(RankMatchDevice *device, const unsigned int *atoms, size_t limbs);

// The chunk run on the device: its index built there, the program run over every lane, and only the kept lanes'
// numbers and records brought back, least lane first; 0 where a copy or a run errored
int rank_match_chunk(SimResults *results, RankMatchDevice *device, const RankMatchChunk *chunk,
                     std::vector<unsigned int> *kept_lanes, std::vector<unsigned int> *kept_records,
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
