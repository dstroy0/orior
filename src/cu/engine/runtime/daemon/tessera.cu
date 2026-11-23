// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The device's tessera (tessera_device.h): the ledger's decisions made on the device by one thread, from the source
// the host daemon's ledger makes them by (tessera_ledger_core.h).
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "tessera_device.h"
#include "tessera_ledger_core.h"

#include <cuda_runtime.h>

// the calls from `first` made in order by one thread, each answered, until one the ledger's arrays cannot take:
// `stopped` is that call, or `count` where every call was made
__global__ void tessera_device_run(TesseraLedger *ledger, const TesseraCall *calls, unsigned long long first,
                                   unsigned long long count, TesseraAnswer *answers, unsigned long long *stopped)
{
    unsigned long long at = first;
    while (at < count)
    {
        tessera_core_call(ledger, &calls[at], &answers[at]);
        if (answers[at].full != 0)
        {
            break;
        }
        at += 1ULL;
    }
    *stopped = at;
}

// `items` on the device grown to contain `wanted` of `size` bytes each, its capacity doubled until it does, and the
// `count` it contained copied into the grown array
static int tessera_device_grow(void **items, unsigned long long *capacity, unsigned long long count,
                               unsigned long long wanted, size_t size)
{
    if (wanted <= *capacity)
    {
        return 1;
    }
    unsigned long long grown_capacity = (*capacity == 0ULL) ? 1ULL : *capacity;
    while (grown_capacity < wanted)
    {
        grown_capacity *= 2ULL;
    }
    void *grown = NULL;
    if (cudaMalloc(&grown, (size_t)grown_capacity * size) != cudaSuccess)
    {
        return 0;
    }
    if ((count != 0ULL) && (cudaMemcpy(grown, *items, (size_t)count * size, cudaMemcpyDeviceToDevice) != cudaSuccess))
    {
        cudaFree(grown);
        return 0;
    }
    cudaFree(*items);
    *items = grown;
    *capacity = grown_capacity;
    return 1;
}

// each array of the device's ledger, stored on the host in `ledger`, grown to the max a call of `type` can add
static int tessera_device_reserve(TesseraLedger *ledger, TesseraCallType type)
{
    const TesseraCapacity needed = tessera_core_capacity(ledger, type);
    return tessera_device_grow((void **)&ledger->jobs, &ledger->job_capacity, ledger->job_count, needed.jobs,
                               sizeof(TesseraJob)) &&
           tessera_device_grow((void **)&ledger->history, &ledger->history_capacity, ledger->history_count,
                               needed.history, sizeof(TesseraHistory)) &&
           tessera_device_grow((void **)&ledger->heap, &ledger->heap_capacity, ledger->heap_count, needed.heap,
                               sizeof(TesseraDeadline));
}

int tessera_device_open(TesseraDevice *device, unsigned long long capacity)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = device, .bytes = sizeof(*device), .val = 0);
    TesseraLedger *const host_ledger = &device->host_ledger;
    host_ledger->next_identity = 1ULL;
    const unsigned long long first_capacity = (capacity == 0ULL) ? 1ULL : capacity;
    const int ok =
        tessera_device_grow((void **)&host_ledger->jobs, &host_ledger->job_capacity, 0ULL, first_capacity,
                            sizeof(TesseraJob)) &&
        tessera_device_grow((void **)&host_ledger->history, &host_ledger->history_capacity, 0ULL, first_capacity,
                            sizeof(TesseraHistory)) &&
        tessera_device_grow((void **)&host_ledger->heap, &host_ledger->heap_capacity, 0ULL, first_capacity,
                            sizeof(TesseraDeadline)) &&
        (cudaMalloc((void **)&device->ledger, sizeof(TesseraLedger)) == cudaSuccess) &&
        (cudaMemcpy(device->ledger, host_ledger, sizeof(TesseraLedger), cudaMemcpyHostToDevice) == cudaSuccess);
    if (!ok)
    {
        tessera_device_close(device);
    }
    return ok;
}

void tessera_device_close(TesseraDevice *device)
{
    cudaFree(device->host_ledger.jobs);
    cudaFree(device->host_ledger.history);
    cudaFree(device->host_ledger.heap);
    cudaFree(device->ledger);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = device, .bytes = sizeof(*device), .val = 0);
}

int tessera_device_calls(TesseraDevice *device, const TesseraCall *calls, unsigned long long count,
                         TesseraAnswer *answers)
{
    if (count == 0ULL)
    {
        return 1;
    }
    TesseraCall *device_calls = NULL;
    TesseraAnswer *device_answers = NULL;
    unsigned long long *stopped = NULL;
    int ok =
        (cudaMalloc((void **)&device_calls, (size_t)count * sizeof(TesseraCall)) == cudaSuccess) &&
        (cudaMalloc((void **)&device_answers, (size_t)count * sizeof(TesseraAnswer)) == cudaSuccess) &&
        (cudaMalloc((void **)&stopped, sizeof(unsigned long long)) == cudaSuccess) &&
        (cudaMemcpy(device_calls, calls, (size_t)count * sizeof(TesseraCall), cudaMemcpyHostToDevice) == cudaSuccess);
    unsigned long long first = 0ULL;
    while (ok && (first < count))
    {
        tessera_device_run<<<1, 1>>>(device->ledger, device_calls, first, count, device_answers, stopped);
        unsigned long long reached = count;
        ok = (cudaGetLastError() == cudaSuccess) &&
             (cudaMemcpy(&reached, stopped, sizeof(reached), cudaMemcpyDeviceToHost) == cudaSuccess) &&
             (cudaMemcpy(&device->host_ledger, device->ledger, sizeof(TesseraLedger), cudaMemcpyDeviceToHost) ==
              cudaSuccess);
        if (ok && (reached < count))
        {
            // the call at `reached` could add more than the ledger's arrays contain: each is grown to the max it can
            // add, and the run goes on from that call, which the ledger then takes
            ok = tessera_device_reserve(&device->host_ledger, calls[reached].type) &&
                 (cudaMemcpy(device->ledger, &device->host_ledger, sizeof(TesseraLedger), cudaMemcpyHostToDevice) ==
                  cudaSuccess);
        }
        first = reached;
    }
    ok = ok && (cudaMemcpy(answers, device_answers, (size_t)count * sizeof(TesseraAnswer), cudaMemcpyDeviceToHost) ==
                cudaSuccess);
    cudaFree(device_calls);
    cudaFree(device_answers);
    cudaFree(stopped);
    return ok;
}

int tessera_device_read(const TesseraDevice *device, TesseraLedger *copy)
{
    TesseraLedger host_ledger;
    if (cudaMemcpy(&host_ledger, device->ledger, sizeof(host_ledger), cudaMemcpyDeviceToHost) != cudaSuccess)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = copy, .bytes = sizeof(*copy), .val = 0);
        return 0;
    }
    *copy = host_ledger;
    // one more of each than it contains, and an empty array is not a null pointer
    copy->job_capacity = host_ledger.job_count + 1ULL;
    copy->history_capacity = host_ledger.history_count + 1ULL;
    copy->heap_capacity = host_ledger.heap_count + 1ULL;
    copy->jobs = (TesseraJob *)platform_cell_taken((size_t)copy->job_capacity * sizeof(TesseraJob),
                                                   platform_place_g_program_owner, 0);
    copy->history = (TesseraHistory *)platform_cell_taken((size_t)copy->history_capacity * sizeof(TesseraHistory),
                                                          platform_place_g_program_owner, 0);
    copy->heap = (TesseraDeadline *)platform_cell_taken((size_t)copy->heap_capacity * sizeof(TesseraDeadline),
                                                        platform_place_g_program_owner, 0);
    const int read =
        (copy->jobs != NULL) && (copy->history != NULL) && (copy->heap != NULL) &&
        (cudaMemcpy(copy->jobs, host_ledger.jobs, (size_t)host_ledger.job_count * sizeof(TesseraJob),
                    cudaMemcpyDeviceToHost) == cudaSuccess) &&
        (cudaMemcpy(copy->history, host_ledger.history, (size_t)host_ledger.history_count * sizeof(TesseraHistory),
                    cudaMemcpyDeviceToHost) == cudaSuccess) &&
        (cudaMemcpy(copy->heap, host_ledger.heap, (size_t)host_ledger.heap_count * sizeof(TesseraDeadline),
                    cudaMemcpyDeviceToHost) == cudaSuccess);
    if (!read)
    {
        platform_cell_released(copy->jobs, platform_place_g_program_owner);
        platform_cell_released(copy->history, platform_place_g_program_owner);
        platform_cell_released(copy->heap, platform_place_g_program_owner);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = copy, .bytes = sizeof(*copy), .val = 0);
    }
    return read;
}
