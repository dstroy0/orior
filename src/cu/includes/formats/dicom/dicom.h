// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef DICOM_H
#define DICOM_H

#include "../../../engine/engine_config.h"
#include "../../../types/integers/exact_integer.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // mantissa times ten to the exponent, exactly as the text wrote it
    typedef struct
    {
        AnchorExactInteger mantissa;
        long long exponent;
    } DicomDecimal;

    int dicom_zip_has_member(const EngineIngestTools *tools, const char *path, const char *member);

    long dicom_describe(const EngineDescribeRequest *request);

    long long dicom_read(const EngineArrayRead *request);

    // the `want` decimals of element (group, tag) in one leaf of a crystal's side bytes; `found` is 0 where the
    // leaf does not hold it. Returns 0 only on an error
    int dicom_side_decimal_list(const EngineSideBytes *side, unsigned long long leaf, unsigned int group,
                                unsigned int tag, unsigned int want, DicomDecimal *out, unsigned int *found,
                                EngineError *error);

#ifdef __cplusplus
}
#endif

#endif
