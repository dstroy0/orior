// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SHIFT_AGREEMENT_H
#define SHIFT_AGREEMENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(SHIFT_AGREEMENT_BUILD_DLL) && SHIFT_AGREEMENT_BUILD_DLL && defined(_WIN32)
#define SHIFT_AGREEMENT_EXPORT __declspec(dllexport)
#else
#define SHIFT_AGREEMENT_EXPORT
#endif

#define SHIFT_AGREEMENT_ERROR (-1L)

#define SHIFT_AGREEMENT_AXES 8u

#define SHIFT_AGREEMENT_PRIME 998244353u

#define SHIFT_AGREEMENT_LONGEST_AXIS (1u << 23u)

    typedef struct
    {
        unsigned int axes;
        unsigned int extents[SHIFT_AGREEMENT_AXES];
        unsigned int weights[SHIFT_AGREEMENT_AXES];
        const unsigned long long *before;
        const unsigned long long *after;
        int lag[SHIFT_AGREEMENT_AXES];
        unsigned int agreement;
        unsigned int padded[SHIFT_AGREEMENT_AXES];
        unsigned int *counts;
    } ShiftAgreementRequest;

    SHIFT_AGREEMENT_EXPORT long shift_agreement_host(ShiftAgreementRequest *args);

    SHIFT_AGREEMENT_EXPORT long shift_agreement_run(ShiftAgreementRequest *args);

    // The bytes that shift_agreement_run keeps on the device after it returns, for `axes` extents. That is one pool of
    // the before and after words and the four transform volumes. Beside it are the negation and root tables, rounded up
    // to the page together. The process keeps the root tables from the lengths of earlier runs across runs, and the
    // call does not count them. The return is 0 for extents that the run refuses with an error.
    SHIFT_AGREEMENT_EXPORT unsigned long long shift_agreement_reserve_bytes(unsigned int axes,
                                                                            const unsigned int *extents);

#ifdef __cplusplus
}
#endif

#endif
