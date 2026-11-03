// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// controller_work.h: the model's work that blocks, posted to a lane and finished on the controller's thread
// (controller_work.c)
#ifndef CONTROLLER_WORK_H
#define CONTROLLER_WORK_H
#include "controller.h"

typedef struct CliControllerWorkCfg CliControllerWorkCfg;

// the lanes: files read, what reads the tree's declarations, and what waits on a language server
#define CLI_WORK_FILES 0u
#define CLI_WORK_TREE 1u
#define CLI_WORK_SERVERS 2u
#define CLI_WORK_LANES 3u

// the lanes begun: 1, or 0 where the cells cannot meet their rings; from the controller's thread alone, `run` posted to
// `lane` with `context`, run on the lane and never on the controller's thread, and `done` run on the controller's thread
// with it after, the window's busy circle turning until it has where `busy` is 1; a piece posted while the lane's ring
// has no segment free waits for one, behind those posted before it: 1, or 0 where the lane does not run or the cells
// cannot meet a piece that waits, nothing posted and nothing run; each `done` come back run and the pieces waiting posted
// as segments free: 1 where any was; and the lanes ended once every piece posted finished
int cli_controller_work_begin(void);
int cli_controller_work_post(const CliControllerWorkCfg *args);
int cli_controller_work_step(void);
void cli_controller_work_end(void);

struct CliControllerWorkCfg
{
    unsigned int lane_at;
    void (*run)(void *context);
    void (*done)(void *context);
    void *context;
    int busy;
};

typedef struct
{
    int (*post)(const CliControllerWorkCfg *args);
} CliControllerWorkNs;
EMBED_TABLE_LAYOUT(CliControllerWorkNs, post);
EMBED_TABLE_STORAGE CliControllerWorkNs cli_controller_work EMBED_UNUSED = {
    .post = cli_controller_work_post,
};

#endif
