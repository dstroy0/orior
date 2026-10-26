// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef TESSERA_TEXT_H
#define TESSERA_TEXT_H

#include "verba_scribo/verba_linea.h"
#include <stdio.h>
#include <string.h>
#include "tessera.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int tessera_text_staged(VerbaLinea *line, const char *text);

    void tessera_bytes_hex(VerbaLinea *line, const unsigned char *bytes, unsigned int count);

#ifdef __cplusplus
}
#endif

#endif
