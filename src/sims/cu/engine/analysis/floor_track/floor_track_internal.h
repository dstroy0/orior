// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the floor_track_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef FLOOR_TRACK_INTERNAL_H
#define FLOOR_TRACK_INTERNAL_H

// Floor 2 matched across frames. The floor 2 of every frame is that of the engine: the `16^3` corner of the crystal
// lowered as its own tower. The sim checks it against the two-level lift of the host. The sim arranges it once as 16
// bit planes. The signature of a body is its patch of floor-2 values in frame t: its own cell alone, or the 27 cells
// around it.
//
// The sim searches the later frame whole, and reads no value. It takes the range `[v - d, v + d]` of each patch cell on
// the planes by ands and ors alone, and shifts it to the patch center. It ands the masks of the patch, and a word stops
// after it is empty. The expected answer is the own path of each body.
//
// A control moves every body exactly one floor-2 cell each frame, with no noise and no ramp. Floor 2 translates
// exactly, and a tolerance of 0 must find every body. The camera law then moves bodies a voxel each frame under shot
// and read noise. From frame t to `t + 1`, that is a quarter of a floor-2 cell, and the decimated floor is not
// shift-invariant under it. From t to `t + 4`, the own time step of floor 2, it is a whole cell again.
//
// The 27 cells read the samples within 10 of the cell of the body. A body that another body reaches there does not
// translate alone. Thus the sim runs the bodies of the camera law again with no noise. It also sets three of them apart
// along z, with the noise and without it. There, every step is clear, and a tolerance of 0 must find every one.
//
// The sim counts the hits, the false candidates and the plane words read at every tolerance. It checks every candidate
// set against the scan of the host.
#include "sim_camera.h"

#include "../../../../../cu/engine/analysis/tower/tower.h"

#define TRACK_KEY 0x545241434BULL

#define TRACK_CONTROL_PURPOSE 0x434F4E54ULL

#define TRACK_FOUNDER_PURPOSE 0x464F554EULL

#define TRACK_SIDE 64ULL

#define TRACK_FLOOR 2u

#define TRACK_FLOOR_SIDE (TRACK_SIDE >> TRACK_FLOOR)

// a floor-2 cell covers this many samples along each axis
#define TRACK_CELL (1LL << TRACK_FLOOR)

// a floor-2 value reads the samples within 6 of its center: two levels of the 5/3 lowpass, 2 + 2 * 2
#define TRACK_SUPPORT 6LL

// the patch centers whose 27 cells, along one axis, every lifting formula takes in its interior form
#define TRACK_CLEAR_FIRST 3LL

#define TRACK_CLEAR_LAST 13LL

#define TRACK_SAMPLES (TRACK_SIDE * TRACK_SIDE * TRACK_SIDE)

#define TRACK_FLOOR_VALUES (TRACK_FLOOR_SIDE * TRACK_FLOOR_SIDE * TRACK_FLOOR_SIDE)

#define TRACK_BITS 16u

#define TRACK_WORD_BITS 32ULL

#define TRACK_WORDS (TRACK_FLOOR_VALUES / TRACK_WORD_BITS)

#define TRACK_VALUE_MAX 65535u

#define TRACK_PATCH_MAX 27u

#define TRACK_PATCHES 2u

#define TRACK_DELTAS 6u

#define TRACK_CONTROL_FRAMES 3ULL

#define TRACK_CAMERA_FRAMES 9ULL

#define TRACK_FRAMES_MAX TRACK_CAMERA_FRAMES

#define TRACK_CONTROL_BODIES 8u

#define TRACK_CAMERA_BODIES 10u

#define TRACK_BODIES_MAX TRACK_CAMERA_BODIES

// the sparse scene: three of the bodies of the camera law, set 22 apart along z. There, the max that a footprint and a
// body reach toward each other is `13 + 3`
#define TRACK_SPARSE_BODIES 3u

#define TRACK_SPARSE_FIRST_Z 10LL

#define TRACK_SPARSE_Z_STEP 22LL

#define TRACK_QUERIES_MAX (TRACK_BODIES_MAX * (TRACK_FRAMES_MAX - 1ULL) * TRACK_PATCHES * TRACK_DELTAS)

#define TRACK_THREADS 256ULL

static_assert(TRACK_WORDS <= 1024ULL, "one block handles a query, one thread a word");

static_assert((TRACK_FLOOR_VALUES % TRACK_THREADS) == 0ULL, "the plane kernel's blocks are whole warps");

// one query: the frame searched, and each patch cell's step from the center and its range of values
typedef struct
{
    unsigned int frame;
    unsigned int count;
    int step[TRACK_PATCH_MAX][SIM_AXES];
    unsigned int low[TRACK_PATCH_MAX];
    unsigned int high[TRACK_PATCH_MAX];
} TrackQuery;

// what the sim grades a query by: the setting that it belongs to, and the cell of the body in each of the two frames.
// Then whether the step is clear of other bodies and of the edges
typedef struct
{
    unsigned int patch;
    unsigned int delta;
    unsigned long long from;
    unsigned long long truth;
    int clear;
} TrackTruth;

// one setting's counts over its queries
typedef struct
{
    unsigned long long queries;
    unsigned long long hits;
    unsigned long long candidates;
    unsigned long long candidates_max;
    unsigned long long gated;
    unsigned long long alone;
    unsigned long long reads;
    unsigned long long reads_max;
    unsigned long long clear;
    unsigned long long clear_hits;
} TrackSetting;

// the host buffers one scene needs
typedef struct
{
    unsigned short *lanes;
    int *crystal;
    int *corner;
    unsigned short *floors;
    long long *work;
    long long *before;
    long long *floor_host;
    unsigned int *planes;
    TrackQuery *queries;
    TrackTruth *truths;
    unsigned int *candidates;
    unsigned int *reads;
    unsigned int *expected;
} TrackHost;

// the device buffers one scene needs
typedef struct
{
    unsigned short *lanes;
    unsigned short *floors;
    unsigned int *planes;
    TrackQuery *queries;
    unsigned int *candidates;
    unsigned int *reads;
} TrackDevice;

// what the sim puts on the device at once: the frames of the longest scene, and the two kept coefficient arrays of the
// tower. Then floor 2 and its planes for every frame, the queries, and the candidate word and reads of each query for
// every word
#define TRACK_DECLARED                                                                                                 \
    ((TRACK_FRAMES_MAX * TRACK_SAMPLES * sizeof(unsigned short)) + (2ULL * TRACK_SAMPLES * sizeof(int)) +              \
     (TRACK_FRAMES_MAX * TRACK_FLOOR_VALUES * sizeof(unsigned short)) +                                                \
     (TRACK_FRAMES_MAX * TRACK_BITS * TRACK_WORDS * sizeof(unsigned int)) + (TRACK_QUERIES_MAX * sizeof(TrackQuery)) + \
     (2ULL * TRACK_QUERIES_MAX * TRACK_WORDS * sizeof(unsigned int)))

void track_floor_host(const unsigned short *lanes, long long *work, long long *before, long long *floor_values);

int track_floor_engine(const unsigned short *device_lanes, int *crystal, int *corner, unsigned short *floor_values);

__global__ void track_planes_kernel(const unsigned short *floor_values, unsigned int *planes);

__global__ void track_query_kernel(const unsigned int *planes, const TrackQuery *queries, unsigned int *candidates,
                                   unsigned int *reads);

unsigned long long track_body_cell(const SimBody *body, unsigned long long frame);

void track_control_bodies(SimBody *body, unsigned long long frames);

void track_scan(const TrackQuery *query, const unsigned short *floor_values, unsigned int *expected);

int track_gated(unsigned long long from, unsigned long long position);

int track_clear(const SimScene *scene, unsigned int index, unsigned long long frame, unsigned long long stride,
                unsigned long long from, unsigned long long truth);

#endif
