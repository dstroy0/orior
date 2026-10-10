// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// rank_match.cu: the match kept on the device. A chunk's lanes, each a query peak against a reference peak, have their
// index built on the device, the match program run over them, the lanes whose kept output is 1 counted and ordered
// after the others by the record machine's sum and sort, and only those lanes' numbers and records brought back
#include "rank_internal.h"

#include <cuda_runtime.h>

#include <string.h>

#define RANK_MATCH_THREADS 256u

// Lane l of a chunk: the chunk's spectrum s whose lanes hold it, lane_first[s] <= l < lane_first[s + 1], and its
// query peak q and reference peak p, l - lane_first[s] = q peaks[s] + p. Its index is the query's peak q, the chunk's
// peak atom_first[s] + p, and the mode's envelope
__global__ static void rank_match_index_kernel(const unsigned long long *lane_first, const unsigned int *atom_first,
                                               const unsigned int *peaks, unsigned int spectra, unsigned long long lanes,
                                               unsigned int mode, unsigned int *index)
{
    const unsigned long long lane = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (lane >= lanes)
    {
        return;
    }
    unsigned int low = 0u;
    unsigned int high = spectra;
    while ((high - low) > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (lane_first[middle] <= lane)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    const unsigned long long local = lane - lane_first[low];
    index[3ull * lane] = (unsigned int)(local / peaks[low]);
    index[(3ull * lane) + 1ull] = atom_first[low] + (unsigned int)(local % peaks[low]);
    index[(3ull * lane) + 2ull] = mode;
}

// record `order[r]`'s limbs written as record r of `out`, for each of `count` records
__global__ static void rank_match_gather_kernel(const unsigned int *records, unsigned int limbs, const unsigned int *order,
                                                unsigned long long count, unsigned int *out)
{
    const unsigned long long at = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x;
    if (at >= (count * limbs))
    {
        return;
    }
    const unsigned long long record = at / limbs;
    out[at] = records[((unsigned long long)order[record] * limbs) + (at % limbs)];
}

static unsigned int rank_match_blocks(unsigned long long threads)
{
    return (unsigned int)((threads + RANK_MATCH_THREADS - 1ull) / RANK_MATCH_THREADS);
}

// a device buffer of at least `bytes`, grown where it holds fewer
static int rank_match_room(void **buffer, size_t *room, size_t bytes)
{
    if (bytes <= *room)
    {
        return 1;
    }
    cudaFree(*buffer);
    *buffer = NULL;
    *room = 0u;
    if (cudaMalloc(buffer, bytes) != cudaSuccess)
    {
        *buffer = NULL;
        return 0;
    }
    *room = bytes;
    return 1;
}

int rank_match_open(RankMatchDevice *device, const unsigned int *envelope, size_t envelope_limbs)
{
    memset(device, 0, sizeof(*device));
    const size_t bytes = envelope_limbs * sizeof(unsigned int);
    return rank_match_room((void **)&device->envelope, &device->envelope_room, bytes) &&
           (cudaMemcpy(device->envelope, envelope, bytes, cudaMemcpyHostToDevice) == cudaSuccess);
}

void rank_match_close(RankMatchDevice *device)
{
    cudaFree(device->envelope);
    cudaFree(device->query);
    cudaFree(device->reference);
    cudaFree(device->lane_first);
    cudaFree(device->atom_first);
    cudaFree(device->peaks);
    cudaFree(device->index);
    cudaFree(device->out);
    cudaFree(device->order);
    cudaFree(device->kept);
    memset(device, 0, sizeof(*device));
}

int rank_match_query(RankMatchDevice *device, const unsigned int *atoms, size_t limbs)
{
    const size_t bytes = limbs * sizeof(unsigned int);
    return rank_match_room((void **)&device->query, &device->query_room, bytes + 4u) &&
           (cudaMemcpy(device->query, atoms, bytes, cudaMemcpyHostToDevice) == cudaSuccess);
}

int rank_match_chunk(SimResults *results, RankMatchDevice *device, const RankMatchChunk *chunk,
                     std::vector<unsigned int> *kept_lanes, std::vector<unsigned int> *kept_records,
                     unsigned long long *microseconds)
{
    const RankMachine *const machine = chunk->machine;
    const unsigned int out_limbs = machine->layout.out_limbs;
    const unsigned long long lanes = chunk->lanes;
    const size_t reference_bytes = (size_t)(chunk->reference_peaks * machine->limbs[1]) * sizeof(unsigned int);
    const size_t spectra = chunk->spectra;
    int ok = rank_match_room((void **)&device->reference, &device->reference_room, reference_bytes + 4u) &&
             rank_match_room((void **)&device->lane_first, &device->lane_first_room,
                             (spectra + 1u) * sizeof(unsigned long long)) &&
             rank_match_room((void **)&device->atom_first, &device->atom_first_room, spectra * sizeof(unsigned int)) &&
             rank_match_room((void **)&device->peaks, &device->peaks_room, spectra * sizeof(unsigned int)) &&
             rank_match_room((void **)&device->index, &device->index_room, (size_t)(3ull * lanes) * sizeof(unsigned int)) &&
             rank_match_room((void **)&device->out, &device->out_room, (size_t)(lanes * out_limbs) * sizeof(unsigned int)) &&
             rank_match_room((void **)&device->order, &device->order_room, (size_t)lanes * sizeof(unsigned int)) &&
             (cudaMemcpy(device->reference, chunk->reference_atoms, reference_bytes, cudaMemcpyHostToDevice) ==
              cudaSuccess) &&
             (cudaMemcpy(device->lane_first, chunk->lane_first, (spectra + 1u) * sizeof(unsigned long long),
                         cudaMemcpyHostToDevice) == cudaSuccess) &&
             (cudaMemcpy(device->atom_first, chunk->atom_first, spectra * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
              cudaSuccess) &&
             (cudaMemcpy(device->peaks, chunk->peaks, spectra * sizeof(unsigned int), cudaMemcpyHostToDevice) ==
              cudaSuccess);
    EngineError error;
    memset(&error, 0, sizeof(error));
    const unsigned long long started = engine_clock_microseconds();
    if (ok)
    {
        rank_match_index_kernel<<<rank_match_blocks(lanes), RANK_MATCH_THREADS>>>(
            device->lane_first, device->atom_first, device->peaks, (unsigned int)spectra, lanes, chunk->mode,
            device->index);
        ok = cudaGetLastError() == cudaSuccess;
    }
    if (ok)
    {
        const CycleRecordRunRequest run = {machine->record,
                                           {device->query, device->reference, device->envelope},
                                           {chunk->query_peaks, chunk->reference_peaks, chunk->envelopes},
                                           device->index,
                                           lanes,
                                           device->out,
                                           &error};
        ok = cycle_record_run(&run) != CYCLE_ERROR;
    }
    // the kept lanes' count, then the lanes in order of their kept output, each run of equal outputs in lane order:
    // the last `kept` of the order are the kept lanes, least first
    unsigned int sums[2] = {0u, 0u};
    if (ok)
    {
        const CycleRecordSumRequest count = {device->out, lanes, lanes, out_limbs, chunk->kept.offset, chunk->kept.bits,
                                             2u,          sums,  &error};
        ok = cycle_record_sum(&count) != CYCLE_ERROR;
    }
    const unsigned long long kept = ((unsigned long long)sums[1] << 32u) | sums[0];
    kept_lanes->resize((size_t)kept);
    kept_records->resize((size_t)(kept * out_limbs));
    if (ok && (kept != 0ull))
    {
        const CycleRecordSortRequest sort = {device->out, lanes, lanes, out_limbs, chunk->kept.offset, chunk->kept.bits,
                                             device->order, &error};
        ok = (cycle_record_sort(&sort) != CYCLE_ERROR) &&
             rank_match_room((void **)&device->kept, &device->kept_room, (size_t)(kept * out_limbs) * sizeof(unsigned int));
        const unsigned int *const tail = device->order + (lanes - kept);
        if (ok)
        {
            rank_match_gather_kernel<<<rank_match_blocks(kept * out_limbs), RANK_MATCH_THREADS>>>(
                device->out, out_limbs, tail, kept, device->kept);
            ok = (cudaGetLastError() == cudaSuccess) &&
                 (cudaMemcpy(kept_lanes->data(), tail, (size_t)kept * sizeof(unsigned int), cudaMemcpyDeviceToHost) ==
                  cudaSuccess) &&
                 (cudaMemcpy(kept_records->data(), device->kept, (size_t)(kept * out_limbs) * sizeof(unsigned int),
                             cudaMemcpyDeviceToHost) == cudaSuccess);
        }
    }
    *microseconds += engine_clock_microseconds() - started;
    if (!ok)
    {
        rank_error_line(results, "  the match's chunk errored", &error);
    }
    return ok;
}
