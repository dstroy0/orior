// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SCHEDULE_H
#define SCHEDULE_H

extern const char *g_schedule_path;

int schedule_program(const char *set, char *const *names, unsigned int count);

#endif
