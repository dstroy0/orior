// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "../../engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define COMPRESSION_ERROR (-1L)

    typedef struct
    {
        const int *device_coefficients;
        unsigned long long count;
        unsigned int *device_scratch;
        unsigned long long *chunks;
        unsigned long long *bits;
        const unsigned long long **offsets;
        const unsigned int **stream;
        EngineError *error;
    } CompressionEncodeRequest;

    long compression_encode(const CompressionEncodeRequest *request);

    typedef struct
    {
        const unsigned long long *offsets;
        unsigned long long chunks;
        const unsigned int *stream;
        unsigned long long bits;
        unsigned long long count;
        int *device_coefficients;
        EngineError *error;
    } CompressionDecodeRequest;

    long compression_decode(const CompressionDecodeRequest *request);

    // An encode as one job of the device (tessera_job.h). The values `coefficients` on the host cross in. One
    // cooperative launch zigzags, measures, scans and writes them on the device. The offsets and the stream cross out
    // into the host cells that compression keeps until its next encode, as compression_encode gives them.
    typedef struct
    {
        const int *coefficients;
        unsigned long long count;
        unsigned long long *chunks;
        unsigned long long *bits;
        const unsigned long long **offsets;
        const unsigned int **stream;
        EngineError *error;
    } CompressionEncodeJobRequest;

    long compression_encode_job(const CompressionEncodeJobRequest *request);

    // A decode as one job of the device. The offsets and the stream on the host cross in, and one cooperative launch
    // reads the values on the device. The values cross out into `coefficients`, a room of `count` ints on the host.
    typedef struct
    {
        const unsigned long long *offsets;
        unsigned long long chunks;
        const unsigned int *stream;
        unsigned long long bits;
        unsigned long long count;
        int *coefficients;
        EngineError *error;
    } CompressionDecodeJobRequest;

    long compression_decode_job(const CompressionDecodeJobRequest *request);

    unsigned long long compression_chunks(unsigned long long count);

    // The device bytes that compression takes for `count` values before it measures them, rounded to the page. That is
    // its chunk pool: the bits and the offset of each chunk, and the scratch of the scan. 0 for a count that errors.
    // The values set the size of the stream, which is a pool of its own. Its bytes are the limbs of the stream and one
    // more, rounded to the page.
    unsigned long long compression_reserve_bytes(unsigned long long count);

    // frees the chunk pool and the stream compression retains between calls
    void compression_resident_release(void);

#ifdef __cplusplus
}
#endif

#endif
