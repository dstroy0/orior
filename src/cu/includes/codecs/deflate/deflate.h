// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef DEFLATE_H
#define DEFLATE_H

#include "../../../engine/engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    unsigned long long deflate_raw_bound(unsigned long long in_bytes);

    long long deflate_raw_encode(const EngineBytesRequest *request);

#ifdef __cplusplus
}
#endif

#endif
