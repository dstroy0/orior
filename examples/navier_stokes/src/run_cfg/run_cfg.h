// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_cfg.h: a cfg read whole, its members found by a dotted path, decimal strings read as exact rationals
#ifndef RUN_CFG_H
#define RUN_CFG_H

#include "sim_rational.h"

#include "cfg_json.h"

#include <string>
#include <vector>

// the room of a text member as the cfg reads it, with its end
#define RUN_CFG_TEXT_ROOM 4096u

// A cfg that the process reads once. Its text is in one cell of the process: `size` bytes and a 0 after them. Its
// tokens are in one cell, `token_room` of them, the count that the text can hold. The process keeps both for the life
// of the run. A decimal value is
// a JSON string, "0.0010016", read exactly as a rational; a count is a JSON number.
typedef struct
{
    char *text;
    size_t size;
    CfgJsonToken *tokens;
    unsigned int token_room;
} RunCfg;

// the cfg at `path` read and parsed: 1, or 0 with the reason written to `line`
int run_cfg_open(const char *path, RunCfg *cfg, VerbaLinea *line);

// the token a dotted path names from the root, "fluid.viscosity": its index, or 0 where a part is absent
unsigned int run_cfg_find(const RunCfg *cfg, const char *path);

// the decimal string at `path` read exactly: 1, or 0 where it is absent or does not read
int run_cfg_rational(const RunCfg *cfg, const char *path, SimRational *value);

// the whole number at `path`: 1, or 0 where it is absent or does not read
int run_cfg_count(const RunCfg *cfg, const char *path, unsigned long long *value);

// the array of decimal strings at `path` read exactly: 1, or 0 where it is absent, empty or an element does not read
int run_cfg_rationals(const RunCfg *cfg, const char *path, std::vector<SimRational> *values);

// the array of whole numbers at `path`: 1, or 0 where it is absent, empty or an element does not read
int run_cfg_counts(const RunCfg *cfg, const char *path, std::vector<unsigned long long> *values);

// the text member at `path` into `value`, which has room for `room` bytes with its end: 1, or 0 where it is absent,
// empty or longer than the room
int run_cfg_text(const RunCfg *cfg, const char *path, char *value, size_t room);

// the path of the text member `member` into `path`. The member gives a path from the folder of the cfg at `cfg_path`.
// The buffer `path` has room for `room` bytes with its end. The return is 1, or 0 where the member does not read or
// the path passes the room.
int run_cfg_beside(const char *cfg_path, const RunCfg *cfg, const char *member, char *path, size_t room);

// one line naming the members a cfg lacks or could not read
void run_cfg_missing(VerbaLinea *line, const char *members);

// 1 where a value in this module outgrew the build's width
int run_cfg_short(void);

#endif
