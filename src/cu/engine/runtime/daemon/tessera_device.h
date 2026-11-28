// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef TESSERA_DEVICE_H
#define TESSERA_DEVICE_H

// The tessera of the device: a ledger arranged in the memory of the device, whose decisions the device makes. One
// thread runs a list of calls in order. Each is the decision of tessera_ledger_core.h, the same as the ledger of the
// host daemon makes. A call that the arrays of the ledger cannot take stops the run with nothing changed.
//
// The host grows the arrays on the device, with a copy of what they allocate, and the run goes on from that call. The
// host daemon stays: it builds with no CUDA toolchain, and makes no CUDA context. The build makes this one only where
// nvcc is.

#include "tessera_ledger.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // the ledger on the device, and the copy that the host has of it as the last run left it. Its arrays point into the
    // memory of the device
    typedef struct
    {
        TesseraLedger *ledger;
        TesseraLedger host_ledger;
    } TesseraDevice;

    // a ledger arranged on the device with `capacity` jobs, tracked peaks and deadlines to start; 0 where the device
    // errors on it
    int tessera_device_open(TesseraDevice *device, unsigned long long capacity);

    void tessera_device_close(TesseraDevice *device);

    // `count` calls made in order on the device, each answered in `answers`. The return is 0 where the device gives an
    // error on the memory or a launch, or the arrays do not grow
    int tessera_device_calls(TesseraDevice *device, const TesseraCall *calls, unsigned long long count,
                             TesseraAnswer *answers);

    // the device's ledger read back into `copy`, a host ledger whose arrays are its counts, released by
    // tessera_ledger_close; 0 where a copy errors
    int tessera_device_read(const TesseraDevice *device, TesseraLedger *copy);

#ifdef __cplusplus
}
#endif

#endif
