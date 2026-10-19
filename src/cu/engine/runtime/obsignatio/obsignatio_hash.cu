// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// obsignatio_hash.cu: the hash, chunks and their kernel
#include "obsignatio_internal.h"

extern "C" long obsignatio_signum(const ObsignatioSignumRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return OBSIGNATIO_ERROR;
    }
    EngineError *const error = request->error;
    if (OBSIGNATIO_CHECK(((request->bytes != NULL) || (request->count == 0ull)) &&
                             ((request->out != NULL) || (request->out_bytes == 0ull)) &&
                             obsignatio_mode_valid(request->mode, request->key),
                         request, error) == 0)
    {
        return OBSIGNATIO_ERROR;
    }
    obsignatio_host_digest(request->key, request->mode, request->bytes, request->count, request->out,
                           request->out_bytes);
    return 0L;
}
#if (defined(__CUDACC__))
__global__ void obsignatio_chunk_kernel(const unsigned char *bytes, unsigned long long messages,
                                        unsigned long long length, unsigned long long stride, unsigned long long chunks,
                                        ObsignatioKey key, unsigned int mode, unsigned int *chaining,
                                        unsigned char *signa)
{
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    for (unsigned long long index = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
         index < (messages * chunks); index += jump)
    {
        const unsigned long long message = index / chunks;
        const unsigned long long chunk = index % chunks;
        const unsigned long long start = chunk << OBSIGNATIO_CHUNK_SHIFT;
        const unsigned long long remain = length - start;
        // the take is at most one chunk's 1024 bytes. It fits an unsigned int
        const unsigned int take = (remain < OBSIGNATIO_CHUNK_BYTES) ? (unsigned int)remain : OBSIGNATIO_CHUNK_BYTES;
        const ObsignatioSource source = {&bytes[message * stride], NULL, 0ull, 0ull, length};
        ObsignatioNode node;
        obsignatio_chunk(key.words, mode, &source, start, take, chunk, &node);
        if (chunks == 1ull)
        {
            obsignatio_root(&node, &signa[message * OBSIGNATIO_SIGNUM_BYTES], OBSIGNATIO_SIGNUM_BYTES);
        }
        else
        {
            obsignatio_chaining(&node, &chaining[index * OBSIGNATIO_CHAINING_WORDS]);
        }
    }
}
#endif
