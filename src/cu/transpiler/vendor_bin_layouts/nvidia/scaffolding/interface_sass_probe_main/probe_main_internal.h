// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// probe_main_internal.h: what the modules of the sass probe's entry share: its includes, the bounds of a category and an example, the probe, machine, listings and texts held, and the functions one module reads from another (interface_sass_probe.h)
#ifndef PROBE_MAIN_INTERNAL_H
#define PROBE_MAIN_INTERNAL_H

#include "../interface_sass_probe.h"

#include <stdlib.h>
#include <string.h>

// the longest category and example a bit's reading is given
#define SASS_CATEGORY 32u

#define SASS_EXAMPLE 160u

extern SassProbe s_sass_probe;
extern SassMachine s_sass_machine;
extern SassListing s_sass_kernel;
extern SassListing s_sass_resident;
extern SassListing s_sass_form;

int sass_list(SassProbe *probe, const char *name, SassListing *listing);
void sass_operations_take(SassProbe *probe, const SassListing *listing);
void sass_form_print(const char *name, const SassListing *form, const SassListing *kernel_listing);
int sass_fields(SassProbe *probe, const SassOperation *operation);
int sass_cubin_same(SassProbe *probe, const SassMachine *machine, const char *name, unsigned int *held);
int sass_lane_written(SassProbe *probe);
int sass_loop_main(SassProbe *probe, const char *path);
int sass_questions_compile(SassProbe *probe);
int sass_asks_main(SassProbe *probe, const char *path);

#endif
