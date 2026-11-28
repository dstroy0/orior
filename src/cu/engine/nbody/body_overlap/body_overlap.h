// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef BODY_OVERLAP_H
#define BODY_OVERLAP_H

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(BODY_OVERLAP_BUILD_DLL) && BODY_OVERLAP_BUILD_DLL && defined(_WIN32)
#define BODY_OVERLAP_EXPORT __declspec(dllexport)
#else
#define BODY_OVERLAP_EXPORT
#endif

#define BODY_OVERLAP_ERROR (-1L)

#define BODY_OVERLAP_CAPACITY_LIMIT 0x7FFFFFFFu

#define BODY_OVERLAP_AXES 8u

    typedef struct
    {
        const unsigned int *labels_before;
        const unsigned long long *positive_before;
        const unsigned int *labels_after;
        const unsigned long long *positive_after;
        unsigned int axes;
        unsigned int extents[BODY_OVERLAP_AXES];
        int lag[BODY_OVERLAP_AXES];
        unsigned int voxels;
        unsigned int capacity;
        unsigned int *peaks_before;
        unsigned int *peaks_after;
        unsigned int *counts;
        const unsigned int *lag_peaks;
        const int *lag_steps;
        unsigned int lag_count;
        // where not NULL, the runs that the device sweep counted. They are the stretches of voxels of each chunk
        // whose two labels stay the same, which body_overlap_run stores on the device as pairs and lengths
        unsigned long long *runs;
    } BodyOverlapRequest;

    BODY_OVERLAP_EXPORT long body_overlap_host(const BodyOverlapRequest *args);

    BODY_OVERLAP_EXPORT long body_overlap_run(const BodyOverlapRequest *args);

    BODY_OVERLAP_EXPORT long body_overlap_run_on_device(const BodyOverlapRequest *args);

    // the device bytes that body_overlap_run allocates over `voxels` with room for `runs` runs. They are the labels and
    // sign words of both frames, the count and offset of each chunk, and the pairs and lengths of the runs. Each
    // allocation is in whole pages of `page` bytes
    BODY_OVERLAP_EXPORT unsigned long long body_overlap_bytes(unsigned long long voxels, unsigned long long runs,
                                                              unsigned long long page);

    // frees the buffers body_overlap_run and body_overlap_run_on_device allocate between calls
    BODY_OVERLAP_EXPORT void body_overlap_release(void);

#ifdef __cplusplus
}
#endif

#endif
