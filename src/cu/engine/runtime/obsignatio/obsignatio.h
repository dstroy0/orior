// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef OBSIGNATIO_H
#define OBSIGNATIO_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define OBSIGNATIO_ERROR (-1L)

#define OBSIGNATIO_SIGNUM_BYTES 32u

#define OBSIGNATIO_KEY_BYTES 32u

#define OBSIGNATIO_MODE_HASH 0u

#define OBSIGNATIO_MODE_KEYED 16u

#define OBSIGNATIO_MODE_CONTEXT 32u

#define OBSIGNATIO_MODE_MATERIAL 64u

#define OBSIGNATIO_EXTENT_AXES 4u

    typedef struct
    {
        const unsigned char *bytes;
        unsigned long long count;
        const unsigned char *key;
        unsigned int mode;
        unsigned char *out;
        unsigned long long out_bytes;
        EngineError *error;
    } ObsignatioSignumRequest;

    long obsignatio_signum(const ObsignatioSignumRequest *request);

    typedef struct
    {
        const unsigned char *device_bytes;
        unsigned long long messages;
        unsigned long long length;
        unsigned long long stride;
        const unsigned char *key;
        unsigned int mode;
        unsigned char *device_signa;
        EngineError *error;
    } ObsignatioManyRequest;

    long obsignatio_many(const ObsignatioManyRequest *request);

    typedef struct
    {
        const unsigned int *device_limbs;
        const unsigned long long *device_offsets;
        unsigned long long messages;
        unsigned long long bits;
        const unsigned char *key;
        unsigned int mode;
        unsigned char *device_signa;
        EngineError *error;
    } ObsignatioBitsRequest;

    long obsignatio_bits(const ObsignatioBitsRequest *request);

    typedef enum
    {
        OBSIGNATIO_LEVEL_ROW = 0,
        OBSIGNATIO_LEVEL_PLANE = 1,
        OBSIGNATIO_LEVEL_VOLUME = 2,
        OBSIGNATIO_LEVEL_LANES = 3,
        OBSIGNATIO_LEVEL_CHUNK = 4,
        OBSIGNATIO_LEVEL_STREAM = 5,
        OBSIGNATIO_LEVEL_SIDE = 6,
        OBSIGNATIO_LEVEL_MEMBERS = 7,
        OBSIGNATIO_LEVEL_SAMPLE = 8,
        OBSIGNATIO_LEVEL_SET = 9,
        OBSIGNATIO_LEVEL_FILE = 10,
        OBSIGNATIO_LEVEL_UNIVERSAL = 11,
        OBSIGNATIO_LEVEL_SIDE_STORED = 12,
        OBSIGNATIO_LEVEL_SIDE_INFLATED = 13,
        OBSIGNATIO_LEVELS = 14
    } ObsignatioLevel;

    const char *obsignatio_level_context(ObsignatioLevel level);

    long obsignatio_level_key(ObsignatioLevel level, unsigned char *key, EngineError *error);

    typedef struct
    {
        const unsigned char *bytes;
        unsigned long long count;
        unsigned char *signum;
        EngineError *error;
    } ObsignatioSealRequest;

    long obsignatio_seal(const ObsignatioSealRequest *request);

    long obsignatio_seal_verify(const ObsignatioSealRequest *request);

    unsigned long long obsignatio_lanes_nodes(const unsigned long long *extent);

    typedef struct
    {
        const unsigned short *device_lanes;
        const unsigned long long *extent;
        unsigned char *device_nodes;
        EngineError *error;
    } ObsignatioLanesRequest;

    long obsignatio_lanes(const ObsignatioLanesRequest *request);

    // The nodes of the lanes as one job of the device (tessera_job.h). The lanes on the host cross in. One cooperative
    // launch seals the rows, the planes, the volumes and the lanes, as obsignatio_lanes seals them. The nodes cross
    // out into `nodes`, a room of obsignatio_lanes_nodes(extent) signa on the host. A part with CUDA alone has it.
    typedef struct
    {
        const unsigned short *lanes;
        const unsigned long long *extent;
        unsigned char *nodes;
        EngineError *error;
    } ObsignatioLanesJobRequest;

    long obsignatio_lanes_job(const ObsignatioLanesJobRequest *request);

    // The signa of the messages of a stream of bits as one job of the device. The words of the stream and the offsets
    // on the host cross in. One cooperative launch checks the offsets and seals each message, as obsignatio_bits
    // does. The signa cross out into `signa`, a room of `messages` signa on the host. A part with CUDA alone has it.
    typedef struct
    {
        const unsigned int *limbs;
        const unsigned long long *offsets;
        unsigned long long messages;
        unsigned long long bits;
        const unsigned char *key;
        unsigned int mode;
        unsigned char *signa;
        EngineError *error;
    } ObsignatioBitsJobRequest;

    long obsignatio_bits_job(const ObsignatioBitsJobRequest *request);

#ifdef __cplusplus
}
#endif

#endif
