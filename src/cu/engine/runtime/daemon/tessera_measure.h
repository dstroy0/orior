// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef TESSERA_MEASURE_H
#define TESSERA_MEASURE_H

#include "tessera.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct TesseraMeasure TesseraMeasure;

    typedef struct TesseraProcess TesseraProcess;

    // Every GPU that NVML lists on the system, each its UUID's sixteen bytes into `devices`, `capacity` of them at max.
    // The answer is their count, 0 where NVML lists none or does not load
    unsigned int tessera_measure_devices(unsigned char (*devices)[TESSERA_DEVICE_BYTES], unsigned int capacity);

    TesseraMeasure *tessera_measure_open(const unsigned char device[TESSERA_DEVICE_BYTES], unsigned long long luid);

    // the adapter's LUID that the process counters of Windows name the device by, which the first job on it reports
    void tessera_measure_luid_set(TesseraMeasure *measure, unsigned long long luid);

    void tessera_measure_close(TesseraMeasure *measure);

    // 1 where the daemon measures no pid from outside (WSL): each job's process reports its own bytes instead
    int tessera_measure_reported(const TesseraMeasure *measure);

    // 1 for the host's processors: the capacity is the processors jobs are given, and the daemon counts their use from
    // what its running jobs report
    int tessera_measure_host(const TesseraMeasure *measure);

    int tessera_measure_device(TesseraMeasure *measure, unsigned long long *capacity, unsigned long long *in_use);

    int tessera_measure_process(TesseraMeasure *measure, unsigned long long pid, unsigned long long *used);

    TesseraProcess *tessera_process_open(unsigned long long pid);

    int tessera_process_lives(const TesseraProcess *process);

    void tessera_process_release(TesseraProcess *process);

#ifdef __cplusplus
}
#endif

#endif
