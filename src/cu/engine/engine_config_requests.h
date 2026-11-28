// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_config_requests.h: residual, bodies, streams, signum, programs and the seal (engine_config.h includes the
// parts in order)
#ifndef ENGINE_CONFIG_REQUESTS_H
#define ENGINE_CONFIG_REQUESTS_H

#include "engine_config_key.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ENGINE_RESIDUAL_LIMBS 9u

// the spacings a spaced pair takes, 2^j for every j below this: each one an unsigned int represents
#define ENGINE_SPACINGS 32u

    typedef struct
    {
        unsigned long long moments[6];
        unsigned long long sums[3];
        unsigned int level[ENGINE_RESIDUAL_LIMBS];
        unsigned int peak;
        unsigned int mass;
        unsigned int touches;
        unsigned int code;
        unsigned int sample;
        unsigned int frame;
        unsigned int id;
        unsigned int state;
        unsigned int parent;
        int forward;
        int backward;
        int velocity[3];
    } EngineBody;

    typedef struct
    {
        unsigned int leaf_count;
        unsigned int *peaks;
        unsigned int *sizes;
        unsigned long long *sums;
        unsigned long long *moments;
        unsigned int *touches;
        unsigned int *joined;
        unsigned int joined_count;
    } EngineLeaves;

    typedef struct
    {
        unsigned int voxels;
        const unsigned int *labels;
        const unsigned int *peaks;
        unsigned int leaf_count;
        int *leaf_at_peak;
        unsigned int *start;
        unsigned int *grouped;
    } EngineGroupRequest;

    // A background order is even on every axis. A smooth order can be odd. The window of an order o starts
    // `floor((o + 1) / 2)` before the voxel. On an axis whose smooth order is odd, both terms and the residual sit half
    // a voxel before the voxel of the lane index. `offset_halves` gets that place for each axis in half voxels, -1 on
    // such an axis and 0 on the others. A request with an odd smooth order and no `offset_halves` gives an error, and
    // the call never loses the offset.
    typedef struct
    {
        const unsigned short *volume;
        unsigned int depth;
        unsigned int height;
        unsigned int width;
        unsigned int smooth_orders[ENGINE_AXES];
        unsigned int background_orders[ENGINE_AXES];
        unsigned int unit_sweep;
        int *offset_halves;
        EngineError *error;
        // the comb: a moving sum of `comb[axis]` voxels along each axis, taken on both terms alike before the smooth.
        // It sends a period of `comb[axis]` along the axis to a constant, with every harmonic of it. The kernel of the
        // residual still sums to zero. 0 and 1 leave the axis as it is. A comb of n moves the centers of both terms by
        // `n - 1` half voxels, as a smooth order of `n - 1` does. Thus `s + n - 1` sets the place that an odd smooth
        // order gives.
        unsigned int comb[ENGINE_AXES];
        // the spaced pairs. Both terms take `smooth_spaced[axis][j]` pairs [1, 2, 1], whose taps are `2^j` voxels
        // apart, after the smooth order. The term of the background takes `background_spaced[axis][j]` more after its
        // order. The spacings go up in order.
        //
        // A pair is symmetric about the voxel and moves no center. It adds 2 bits and the variance of an order
        // `2 · 4^j`. Thus a smooth of wide variance costs bits in the count of its pairs.
        unsigned int smooth_spaced[ENGINE_AXES][ENGINE_SPACINGS];
        unsigned int background_spaced[ENGINE_AXES][ENGINE_SPACINGS];
        // the limbs that hold a residual lane, from 1 to ENGINE_RESIDUAL_LIMBS. 0 stores it in ENGINE_RESIDUAL_LIMBS.
        // The call engine_residual_limbs gives the fewest that hold the residual of the request
        unsigned int limbs;
    } EngineResidualRequest;

#define ENGINE_RESIDUAL_BY_UNIT_SWEEP 0u

#define ENGINE_RESIDUAL_BY_KEY 1u

#define ENGINE_RESIDUAL_BOTH_PROVED 2u

#define ENGINE_COEFFICIENT_LIMIT (1LL << 30)

    typedef struct
    {
        unsigned long long extent[4];
        unsigned long long chunks;
        unsigned long long bits;
        unsigned long long lane_offset;
        const unsigned long long *offsets;
        const unsigned int *stream;
    } EngineStream;

#define ENGINE_SIGNUM_BYTES 32u

    typedef struct
    {
        unsigned char bytes[ENGINE_SIGNUM_BYTES];
    } EngineSignum;

    // A program resident on the device keeps a block: what it is, what the scheduler sends it, where it is and how it
    // left. The program runs on its own, and checks in to its block as it goes. It yields before its time to live runs
    // out, and leaves in the block all that a launch needs to resume it. The scheduler writes only the command.
    //
    // The program writes the rest while a launch refers to the block, and the checksum seals the block when none does.
    // Every word is 64 bits, a device address among them. The host and the device read one layout.
    typedef enum
    {
        ENGINE_PROGRAM_RUN = 0,
        ENGINE_PROGRAM_YIELD = 1,
        ENGINE_PROGRAM_STOP = 2
    } EngineProgramCommand;

    // where a program is: it runs, it yielded for a resume, it waits for its grant, or it is at one of its exits. The
    // exits of a question are true, false, malformed (it built no lattice) and answered (the answer table held it). The
    // exit of a sweep is complete, with its records written. The grant of a program that waits is more than the
    // scheduler has free, and it runs after the space frees
    typedef enum
    {
        ENGINE_PROGRAM_PLACED = 0,
        ENGINE_PROGRAM_RUNNING = 1,
        ENGINE_PROGRAM_YIELDED = 2,
        ENGINE_PROGRAM_DONE = 3,
        ENGINE_PROGRAM_TRUE = 4,
        ENGINE_PROGRAM_FALSE = 5,
        ENGINE_PROGRAM_MALFORMED = 6,
        ENGINE_PROGRAM_ANSWERED = 7,
        ENGINE_PROGRAM_STOPPED = 8,
        ENGINE_PROGRAM_FAULT = 9,
        ENGINE_PROGRAM_WAITING = 10
    } EngineProgramState;

// the blocks that one program reads from and that read from it, at max
#define ENGINE_PROGRAM_LINKS 4u

    typedef struct
    {
        // what it is: its signum, the run that the scheduler arranged it for, and the launch that runs it
        EngineSignum signature;
        unsigned long long generation;
        unsigned long long owner;
        // what the scheduler sends it (EngineProgramCommand), and the grant that limits it. The grant has registers
        // for each thread, threads for each launch, and the device bytes of the local frame across them. It also has
        // the shared memory that each thread block uses its registers in. The scheduler measures it exactly, from the
        // widths of the program
        unsigned long long command;
        unsigned long long grant_registers;
        unsigned long long grant_threads;
        unsigned long long grant_bytes;
        unsigned long long grant_shared;
        // where it is (EngineProgramState): the next lane that it runs (execaddr), and the step that it left in a lane.
        // That step is evacaddr, 0 where it leaves between lanes. Then the limbs of the register map, and the registers
        // that it saved there
        unsigned long long state;
        unsigned long long offset;
        unsigned long long step;
        unsigned long long span;
        unsigned long long saved;
        // its clock, in nanoseconds of the device timer. It has the time that a launch runs before it yields, and the
        // check-in that the scheduler limits it to. It has the start of this launch, the time across every launch of
        // the run, and the own time of this launch
        unsigned long long ttl;
        unsigned long long wdt;
        unsigned long long launch_time;
        unsigned long long runtime;
        unsigned long long exectime;
        // its progress: check-ins in order, the last one's time, the launches the run took, its lanes and the errored
        unsigned long long checkin;
        unsigned long long checkin_time;
        unsigned long long launches;
        unsigned long long lanes;
        unsigned long long error;
        // how it failed: the engine module and the site, as EngineError records them
        unsigned long long error_module;
        unsigned long long error_site;
        // its wires: the blocks that it reads, the blocks that read it, and the words that each link makes and uses.
        // Then its result, its length in words, and the block that launched it
        unsigned long long inputs[ENGINE_PROGRAM_LINKS];
        unsigned long long outputs[ENGINE_PROGRAM_LINKS];
        unsigned long long produced[ENGINE_PROGRAM_LINKS];
        unsigned long long consumed[ENGINE_PROGRAM_LINKS];
        unsigned long long result;
        unsigned long long result_words;
        unsigned long long parent;
        // CRC-64/XZ over every word above
        unsigned long long checksum;
    } EngineProgramBlock;

    typedef enum
    {
        ENGINE_SEAL_SAMPLE = 0,
        ENGINE_SEAL_STREAM = 1,
        ENGINE_SEAL_SIDE = 2,
        ENGINE_SEAL_MEMBERS = 3,
        ENGINE_SEAL_SIDE_STORED = 4,
        ENGINE_SEAL_SIDE_INFLATED = 5,
        ENGINE_SEAL_ROOTS = 6
    } EngineSealRoot;

    typedef struct
    {
        EngineSignum roots[ENGINE_SEAL_ROOTS];
        EngineSignum *lane_nodes;
        unsigned long long lane_count;
        EngineSignum *chunk_leaves;
        unsigned long long chunk_count;
    } EngineSeal;

#define ENGINE_HISTORY_WINDOW 11u

#define ENGINE_HISTORY_BITS 16u

#define ENGINE_HISTORY_WINDOWS_MAX 16u

    typedef struct
    {
        unsigned long long extent[4];
        unsigned long long windows;
        EngineSignum sample;
        unsigned long long payload_crc;
        unsigned long long cloud_crc;
        unsigned long long *cloud;
        unsigned long long *history;
    } EngineHistory;

#define ENGINE_BODY_WORDS 11u

#define ENGINE_BODY_PEAK 0u

#define ENGINE_BODY_MASS 1u

#define ENGINE_BODY_SUMS 2u

#define ENGINE_BODY_MOMENTS 5u

    typedef struct
    {
        unsigned long long extent[4];
        unsigned long long frames;
        unsigned long long bodies;
        unsigned long long crc;
        unsigned long long *frame_start;
        unsigned long long *words;
    } EngineBodyTable;

#define ENGINE_BYTES_ERROR (-1LL)

    typedef struct
    {
        const unsigned char *in;
        unsigned long long in_bytes;
        unsigned char *out;
        unsigned long long out_capacity;
    } EngineBytesRequest;

    typedef long long (*EngineBytesDecode)(const EngineBytesRequest *request);

    typedef enum
    {
        ENGINE_CODEC_RAW = 0,
        ENGINE_CODEC_ZSTD = 1,
        ENGINE_CODEC_ZLIB = 2,
        ENGINE_CODEC_GZIP = 3,
        ENGINE_CODEC_DEFLATE = 4,
        ENGINE_CODEC_LZ4 = 5,
        ENGINE_CODEC_LZ4_FRAME = 6,
        ENGINE_CODEC_SNAPPY = 7,
        ENGINE_CODEC_BLOSCLZ = 8,
        ENGINE_CODEC_BLOSC = 9,
        ENGINE_CODEC_LZ4_SIZED = 10,
        ENGINE_CODECS = 11
    } EngineCodec;

    typedef struct
    {
        const char *path;
        unsigned long long offset;
        unsigned long long bytes;
        unsigned char *out;
    } EngineFileRange;

    typedef long long (*EngineFileRead)(const EngineFileRange *range);

    typedef long long (*EngineFileSize)(const char *path);

    // one item of `items` independent ones: 1 where it is complete, 0 where it failed
    typedef int (*EngineEachWork)(void *context, unsigned long long item);

    // runs `work` on every item, on as many workers as the process has processors. $TESSERA_RUN_PROCESSORS gives the
    // count, or 1 where the environment does not set it. It returns 1 where every item is complete. After an item
    // fails, the call starts no item after it
    typedef int (*EngineEach)(unsigned long long items, EngineEachWork work, void *context);

    // `each` can be NULL, and a module that gets none runs its items one at a time
    typedef struct
    {
        EngineBytesDecode decode[ENGINE_CODECS];
        EngineFileRead read;
        EngineFileSize size;
        EngineEach each;
    } EngineIngestTools;

#define ENGINE_ARRAY_RANK 8u

    typedef enum
    {
        ENGINE_ELEMENT_UNSIGNED = 0,
        ENGINE_ELEMENT_SIGNED = 1,
        ENGINE_ELEMENT_FLOAT = 2
    } EngineElementType;

    typedef struct
    {
        unsigned int rank;
        unsigned long long sizes[ENGINE_ARRAY_RANK];
        char axes[ENGINE_ARRAY_RANK];
        unsigned int element_bytes;
        EngineElementType element_type;
    } EngineArrayExtent;

    typedef struct
    {
        const char *path;
        const char *member;
        const EngineIngestTools *tools;
        EngineArrayExtent *extent;
        EngineError *error;
    } EngineDescribeRequest;

    typedef struct
    {
        unsigned long long leaves;
        unsigned long long *pixel_at;
        unsigned long long *byte_start;
        unsigned char *bytes;
        unsigned long long *name_start;
        char *names;
        unsigned long long *member_crc;
        unsigned long long *member_bytes;
        unsigned long long *pixel_stored;
        unsigned long long lane_offset;
    } EngineSideBytes;

    typedef struct
    {
        EngineSideBytes side;
        unsigned char *packed;
        unsigned long long packed_bytes;
    } EngineSideSection;

    typedef struct
    {
        const char *path;
        const char *member;
        const EngineIngestTools *tools;
        const EngineArrayExtent *extent;
        unsigned long long first;
        unsigned long long end;
        unsigned char *out;
        unsigned long long out_capacity;
        EngineSideBytes *side;
        EngineError *error;
    } EngineArrayRead;

#ifdef __cplusplus
}
#endif

#endif
