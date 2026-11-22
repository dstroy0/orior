// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the target_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef TARGET_INTERNAL_H
#define TARGET_INTERNAL_H

#include "../../../../includes/file_defs/krs/ruleset_flat.h"
#include "../../../../includes/file_defs/krs/ruleset_reader.h"
#include "target.h"

#include <stddef.h>

// the folder of the rulesets into `folder`, `bytes` of it: 1 where the whole folder fit
int ruleset_folder(char *folder, size_t bytes);

// the place of `word` among `count` names, or `count` where it is none of them
unsigned int ruleset_find(const RulesetName *names, unsigned int count, const char *word);

// the place of the `length` characters at `word` among `count` names, or `count` where they are none of them
unsigned int ruleset_find_span(const RulesetName *names, unsigned int count, const char *word, size_t length);

#endif
