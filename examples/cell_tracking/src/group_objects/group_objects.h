// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef GROUP_OBJECTS_H
#define GROUP_OBJECTS_H

#include "track.h"

int group_objects(TreeFrame *frame, const TreeFrame *previous, const TreeFrame *next, unsigned int height,
                  unsigned int width, const TreeRules *rules);

#endif
