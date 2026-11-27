// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_cfg.h: a cfg read whole, its members found by a dotted path, decimal strings read as exact rationals
#ifndef RUN_CFG_H
#define RUN_CFG_H

#include "sim_rational.h"

#include "cfg_json.h"

// the room of a text member as the cfg reads it, with its end
#define RUN_CFG_TEXT_ROOM 4096u

// A cfg read once: its text in one cell of the process, `size` bytes and a 0 after them, and its tokens in one cell at
// the count the text can hold, `token_room` of them. The process keeps both for the life of the run. A decimal value is
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

// the token a dotted path names from the root, "fluid.viscosity": its index, or 0 where a part is missing
unsigned int run_cfg_find(const RunCfg *cfg, const char *path);

// the decimal string at `path` read exactly: 1, or 0 where it is missing or does not read
int run_cfg_rational(const RunCfg *cfg, const char *path, SimRational *value);

// the whole number at `path`: 1, or 0 where it is missing or does not read
int run_cfg_count(const RunCfg *cfg, const char *path, unsigned long long *value);

// The array of decimal strings at `path` read exactly into one cell of the persistent tier sized at its count, into
// `values` and `count`: 1, or 0 where it is missing, empty or an element does not read, and the cell is then given
// back. The process reads its cfg once and keeps the cell for the run
int run_cfg_rationals(const RunCfg *cfg, const char *path, SimRational **values, unsigned int *count);

// the text member at `path` into `value`, which has room for `room` bytes with its end: 1, or 0 where it is absent,
// empty or longer than the room
int run_cfg_text(const RunCfg *cfg, const char *path, char *value, size_t room);

// the path of the text member `member`, a path from the folder that the cfg at `cfg_path` is in, into `path`, which has
// room for `room` bytes with its end: 1, or 0 where the member does not read or the path passes the room
int run_cfg_beside(const char *cfg_path, const RunCfg *cfg, const char *member, char *path, size_t room);

// one line naming the members a cfg lacks or could not read
void run_cfg_missing(VerbaLinea *line, const char *members);

// 1 where a value in this module outgrew the build's width
int run_cfg_short(void);

#endif
