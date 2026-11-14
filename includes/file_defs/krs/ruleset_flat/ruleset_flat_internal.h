// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_flat_internal.h: what the modules of the flat ruleset reader share: its includes, the state of the read, and
// the functions one module reads from another (ruleset_flat.h)
#ifndef RULESET_FLAT_INTERNAL_H
#define RULESET_FLAT_INTERNAL_H

#include "../ruleset_flat.h"

#include "../../klang/klang.h"
#include "../../qry/writer/qry_member.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ruleset_append(const std::string &path, std::string *text);

#endif
