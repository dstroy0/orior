// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef RESIDUAL_SURVEY_H
#define RESIDUAL_SURVEY_H

#include <stdio.h>

extern unsigned int g_survey;

void survey_residual(const unsigned int *residual, size_t voxels);

void survey_report(void);

#endif
