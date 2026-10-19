// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef ZSTD_H
#define ZSTD_H

#include "../../../engine/engine_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    long long zstd_decode(const EngineBytesRequest *request);

#ifdef __cplusplus
}
#endif

#endif
