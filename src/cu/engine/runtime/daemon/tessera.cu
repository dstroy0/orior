// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The device's tessera (tessera_device.h): the ledger's decisions made on the device by one thread, from the source
// the host daemon's ledger makes them by (tessera_ledger_core.h).
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "tessera_device.h"
#include "tessera_job.h"
#include "tessera_ledger_core.h"

#include <cuda_runtime.h>

// the owner that the cells of the device's ledger in the pool of the device count to
#define TESSERA_DEVICE_PLACE ((unsigned int)platform_place_g_engine_owner)

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
    // the grown array, a persistent cell of the pool of the device that the device holds until it closes
    void *grown = NULL;
    if (!tessera_job_persistent_taken(grown_capacity * size, TESSERA_DEVICE_PLACE, &grown))
    {
        return 0;
    }
    if (!tessera_job_copied(grown, *items, count * size))
    {
        tessera_job_given(grown, TESSERA_DEVICE_PLACE);
        return 0;
    }
    tessera_job_given(*items, TESSERA_DEVICE_PLACE);
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
        tessera_job_persistent_taken(sizeof(TesseraLedger), TESSERA_DEVICE_PLACE, (void **)&device->ledger) &&
        tessera_job_crossed_into(device->ledger, host_ledger, sizeof(TesseraLedger));
    if (!ok)
    {
        tessera_device_close(device);
    }
    return ok;
}

void tessera_device_close(TesseraDevice *device)
{
    tessera_job_given(device->ledger, TESSERA_DEVICE_PLACE);
    tessera_job_given(device->host_ledger.heap, TESSERA_DEVICE_PLACE);
    tessera_job_given(device->host_ledger.history, TESSERA_DEVICE_PLACE);
    tessera_job_given(device->host_ledger.jobs, TESSERA_DEVICE_PLACE);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = device, .bytes = sizeof(*device), .val = 0);
}

int tessera_device_calls(TesseraDevice *device, const TesseraCall *calls, unsigned long long count,
                         TesseraAnswer *answers)
{
    if (count == 0ULL)
    {
        return 1;
    }
    // The calls cross in. The answers and the call that stopped are temporary cells of the pool of the device. The call
    // gives them back last first before it returns
    TesseraJobCells cells;
    tessera_job_cells_begun(&cells, TESSERA_DEVICE_PLACE);
    void *cell[3] = {NULL, NULL, NULL};
    int ok = tessera_job_cells_in(&cells, calls, count * sizeof(TesseraCall), &cell[0]) &&
             tessera_job_cells_room(&cells, count * sizeof(TesseraAnswer), &cell[1]) &&
             tessera_job_cells_room(&cells, sizeof(unsigned long long), &cell[2]);
    // Explicit casts read each cell as the records that it holds
    const TesseraCall *const device_calls = (const TesseraCall *)cell[0];
    TesseraAnswer *const device_answers = (TesseraAnswer *)cell[1];
    unsigned long long *const stopped = (unsigned long long *)cell[2];
    unsigned long long first = 0ULL;
    while (ok && (first < count))
    {
        tessera_device_run<<<1, 1>>>(device->ledger, device_calls, first, count, device_answers, stopped);
        unsigned long long reached = count;
        ok = (cudaGetLastError() == cudaSuccess) && tessera_job_crossed_out(&reached, stopped, sizeof(reached)) &&
             tessera_job_crossed_out(&device->host_ledger, device->ledger, sizeof(TesseraLedger));
        if (ok && (reached < count))
        {
            // the call at `reached` could add more than the ledger's arrays contain: each is grown to the max it can
            // add, and the run goes on from that call, which the ledger then takes
            ok = tessera_device_reserve(&device->host_ledger, calls[reached].type) &&
                 tessera_job_crossed_into(device->ledger, &device->host_ledger, sizeof(TesseraLedger));
        }
        first = reached;
    }
    ok = ok && tessera_job_crossed_out(answers, device_answers, count * sizeof(TesseraAnswer));
    tessera_job_cells_given(&cells);
    return ok;
}

int tessera_device_read(const TesseraDevice *device, TesseraLedger *copy)
{
    TesseraLedger host_ledger;
    if (!tessera_job_crossed_out(&host_ledger, device->ledger, sizeof(host_ledger)))
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
        tessera_job_crossed_out(copy->jobs, host_ledger.jobs, host_ledger.job_count * sizeof(TesseraJob)) &&
        tessera_job_crossed_out(copy->history, host_ledger.history,
                                host_ledger.history_count * sizeof(TesseraHistory)) &&
        tessera_job_crossed_out(copy->heap, host_ledger.heap, host_ledger.heap_count * sizeof(TesseraDeadline));
    if (!read)
    {
        platform_cell_released(copy->jobs, platform_place_g_program_owner);
        platform_cell_released(copy->history, platform_place_g_program_owner);
        platform_cell_released(copy->heap, platform_place_g_program_owner);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = copy, .bytes = sizeof(*copy), .val = 0);
    }
    return read;
}
