// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_internal.h: what the ranker's parts share: the spectra as the ranker stores them, the record machine's loaded
// programs, and the sweep that runs one over host records on the device
#ifndef CASMI_RANK_INTERNAL_H
#define CASMI_RANK_INTERNAL_H

#include "../../../../../includes/exact_types/integers/exact_integer.h"
#include "../../../../../includes/exact_types/integers/exact_record/exact_record.h"
#include "../../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../../src/cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../src/cu/engine/analysis/keymath/keymath.h"
#include "sim.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>

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

// the max device bytes a piece of a sweep occupies: the records its lanes read, its index and the records it writes
#define RANK_SWEEP_BYTES (1ull << 30u)

// the files a set reads at max: the train file, and in test the test file after it
#define RANK_FILES 2u

// the outputs a loaded program lists at max
#define RANK_OUTPUTS_MAX 4u

// the values of a block of a double column: 2^27 of them, 1 GB, inside the bytes that one cell of the pool holds
#define RANK_BLOCK_SHIFT 27u
#define RANK_BLOCK_VALUES (1ull << RANK_BLOCK_SHIFT)

// the blocks a double column holds at max, 2^33 values
#define RANK_BLOCKS_MAX 64u

// One double column as the files store it: each value's 64 bits, `values` of them, value v at bit v % RANK_BLOCK_VALUES
// of block v / RANK_BLOCK_VALUES, and row r's values from row_start[r] up to row_start[r + 1], `rows` rows. The cells
// are on the persistent tier, at the bounds of the files' footers, `values_room` and `rows_room` + 1
typedef struct
{
    unsigned long long *block[RANK_BLOCKS_MAX];
    unsigned int blocks;
    unsigned long long values;
    unsigned long long values_room;
    unsigned long long *row_start;
    unsigned long long rows;
    unsigned long long rows_room;
} RankColumn;

// The distinct strings of one text column, `count` of them in the order of their first row. String s is the terminated
// text at bytes + offset[s], and offset[s + 1] - offset[s] - 1 bytes long. order holds the ids in the order of their
// bytes. The cells are on the persistent tier
typedef struct
{
    char *bytes;
    unsigned long long *offset;
    unsigned int *order;
    unsigned int count;
} RankStrings;

// value `at` of a double column, its stored 64 bits
static inline unsigned long long rank_value(const RankColumn *column, unsigned long long at)
{
    return column->block[at >> RANK_BLOCK_SHIFT][at & (RANK_BLOCK_VALUES - 1ull)];
}

// The spectra of every file read: each spectrum's text ids, one a text column and RANK_ABSENT where the row has none,
// and the three double columns; each text column's distinct strings over every row group of every file. The spectra
// of file f end at file_spectra[f]
typedef struct
{
    unsigned long long spectra;
    unsigned long long file_spectra[RANK_FILES];
    unsigned int *text[RANK_TEXTS];
    RankStrings strings[RANK_TEXTS];
    RankColumn precursor;
    RankColumn mz;
    RankColumn intensity;
    unsigned int row_groups;
} RankSet;

// a program loaded on the record machine, its outputs, `output_count` of them, and its members' limbs
typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
    unsigned int members;
    unsigned int limbs[ENGINE_RECORD_MEMBERS_MAX];
    unsigned int outputs[RANK_OUTPUTS_MAX];
    unsigned int output_count;
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

// The `files` parquet files at `paths` read into the set in order, every row group of each in order. The first pass
// reads each footer and bounds the cells, and the second fills them. 0 where a file or a row group did not read, or a
// file holds no spectrum, the reason printed
int rank_set_load(SimResults *results, const char *const *paths, unsigned int files, RankSet *out);

// string `id` of text column `text`, terminated
const char *rank_string(const RankSet *set, unsigned int text, unsigned int id);

// the bytes of string `id` of text column `text`, its terminator not counted
unsigned long long rank_string_length(const RankSet *set, unsigned int text, unsigned int id);

// a text's id in the set's strings of that column, RANK_ABSENT where the set holds no such string
unsigned int rank_string_find(const RankSet *set, unsigned int text, const char *wanted);

// A cell of `count` elements of `size` bytes, each byte 0, on the temporary tier where `ephemeral` is 1 and else on the
// persistent tier. NULL where the pool cannot meet it, and rank_cells_refused then answers 1
void *rank_cell(unsigned long long count, unsigned long long size, int ephemeral);

// A host cell on the persistent tier of `count` elements of `size` bytes at least. Where *room is less, the cell grows
// to twice *room or to `count`, the larger, and its first `held` elements stay. The bytes past them are not 0. 1, or 0
// where the pool cannot meet it, and the cell then stays as it was
int rank_host_room(void **cell, unsigned long long *room, unsigned long long count, unsigned long long held,
                   unsigned long long size);

// 1 where the pool refused a cell of the ranker, the reason printed, and 0 where it refused none
int rank_cells_refused(SimResults *results);

// a cell of the ranker given back. Nothing goes back where the cell is NULL
void rank_cell_released(void *cell);

// a built program loaded with `members` members of the limbs given; 0 where it errored, the error printed
int rank_machine_load(SimResults *results, const char *name, ExactRecordProgram *program, const unsigned int *outputs,
                      unsigned int output_count, unsigned int members, const unsigned int *limbs, RankMachine *machine);

void rank_machine_release(RankMachine *machine);

RankField rank_output(const RankMachine *machine, unsigned int output);

// a device buffer of at least `bytes`, grown where it has fewer; 0 where it did not allocate
int rank_device_room(void **buffer, size_t *room, size_t bytes);

// The program run on the device over `lanes` lanes: each member's `bodies` records copied from host memory, the index
// where it is set, and the records written back to `out`, a piece of lanes at a time; 0 where a copy or the run errored
int rank_sweep(SimResults *results, const char *name, const RankMachine *machine, const unsigned int *const *members,
               const unsigned long long *bodies, const unsigned int *index, unsigned long long lanes, unsigned int *out,
               unsigned long long *microseconds);

// the match's buffers on the device, each grown to the max a chunk requested: the envelopes, a query's peaks, a
// chunk's reference peaks and its spectra's first lanes, first peaks and peak counts, and its lanes' index, records,
// order and retained records
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
    unsigned int *matched;
    size_t envelope_room;
    size_t query_room;
    size_t reference_room;
    size_t lane_first_room;
    size_t atom_first_room;
    size_t peaks_room;
    size_t index_room;
    size_t out_room;
    size_t order_room;
    size_t matched_room;
} RankMatchDevice;

// One chunk of the match: the query's peaks against each of the chunk's spectra in turn, spectrum s's lanes from
// lane_first[s] up to lane_first[s + 1], query peak q against its peak p at lane lane_first[s] + q peaks[s] + p, its
// peaks from atom_first[s] in reference_atoms; `matched` the program's output that is 1 where a lane's weight is above
// 0
typedef struct
{
    const RankMachine *machine;
    RankField matched;
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

// The chunk run on the device: its index built there, the program run over every lane, and only the retained lanes'
// numbers and records brought back, least lane first, *matched_count of them. The caller's cells grow to them through
// rank_host_room. 0 where a copy, a run or a cell failed
int rank_match_chunk(SimResults *results, RankMatchDevice *device, const RankMatchChunk *chunk,
                     unsigned int **matched_lanes, unsigned long long *lanes_room, unsigned int **matched_records,
                     unsigned long long *records_room, unsigned long long *matched_count,
                     unsigned long long *microseconds);

// a field of a record read as two's complement into an exact integer
void rank_field_read(const unsigned int *record, RankField field, OriorExactInteger *value);

// the sign of a field: -1, 0 or 1
int rank_field_sign(const unsigned int *record, RankField field);

// an exact integer written into a record's field, two's complement
void rank_field_put(unsigned int *record, unsigned int offset, unsigned int bits, const OriorExactInteger *value);

// a value below 2^bits written into a record's field that contains 0
void rank_put_unsigned(unsigned int *record, unsigned int offset, unsigned int bits, unsigned long long value);

// `bits` bits of `limbs` from bit 0 or'ed into a record at `offset`, the field there 0
void rank_bits_copy(unsigned int *record, unsigned int offset, const unsigned int *limbs, unsigned int bits);
unsigned int rank_bits(const OriorExactInteger *value);

void rank_exact_of(unsigned long long value, OriorExactInteger *out);

#endif
