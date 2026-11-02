// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// callback_cfg.h: the Cfg of each callback signature whose members are the language's own types and the line writer's
// line. A function pointer of the signature and every function named as one take the same args, so any of them can
// stand for any other.
#ifndef CALLBACK_CFG_H
#define CALLBACK_CFG_H

#include <stddef.h>
#include <stdint.h>

#include "../../../../src/cu/includes/mmgr/src/verba_scribo/verba_linea.h"
#include "embed_dispatch_layout.h"

typedef struct
{
    const char *path;
    const unsigned char *bytes;
    unsigned long long size;
    void *context;
} BenchEachCfg;

typedef struct
{
    unsigned int at;
} CliAtCfg;

typedef struct
{
    void *context;
    const char *name;
    const char *arguments;
    VerbaLinea *out;
    VerbaLinea *err_msg;
} CliCallCfg;

typedef struct
{
    void *context;
    const char *channel;
    const char *from;
    const char *body;
    unsigned long long size;
} CliCommsRecvCfg;

typedef struct
{
    const char *one;
    unsigned long long one_length;
    const char *other;
    unsigned long long other_length;
} CliCompareCfg;

typedef struct
{
    void *context;
    unsigned long long session;
    const char *event;
    const char *body;
    unsigned long long size;
} CliDebugEventCfg;

typedef struct
{
    void *context;
    const char *path;
    VerbaLinea *out;
} CliDebugPathWrittenCfg;

typedef struct
{
    void *context;
    const char *name;
    unsigned long long length;
    const char *kind;
    unsigned int line;
} CliDeclaredCfg;

typedef struct
{
    void *context;
    unsigned long long at;
} CliEachAtCfg;

typedef struct
{
    void *context;
    const char *batch;
    unsigned long long size;
} CliEachBatchSizeCfg;

typedef struct
{
    void *context;
    const char *bytes;
    unsigned long long size;
} CliEachBytesSizeCfg;

typedef struct
{
    void *context;
    const char *event;
    const char *json;
    unsigned long long size;
} CliEachEventJsonSizeCfg;

typedef struct
{
    void *context;
    const char *folder;
    unsigned int depth;
} CliEachFolderDepthCfg;

typedef struct
{
    void *context;
    const char *from;
    const char *dash;
    const char *to;
} CliEachFromDashToCfg;

typedef struct
{
    void *context;
    const char *group;
} CliEachGroupCfg;

typedef struct
{
    void *context;
    const char *id;
    const char *folder;
} CliEachIdFolderCfg;

typedef struct
{
    void *context;
    const char *item;
    unsigned long long size;
} CliEachItemSizeCfg;

typedef struct
{
    void *context;
    const char *json;
    unsigned long long size;
} CliEachJsonSizeCfg;

typedef struct
{
    void *context;
    const char *key;
    const char *adjacent;
    const char *text;
    unsigned long long size;
} CliEachKeyAdjacentTextSizeCfg;

typedef struct
{
    const void *context;
    const char *kind;
} CliEachKindCfg;

typedef struct
{
    unsigned int letter;
} CliEachLetterCfg;

typedef struct
{
    void *context;
    const char *line;
    int ended;
} CliEachLineEndedCfg;

typedef struct
{
    void *context;
    char *line;
    unsigned long long capacity;
} CliEachLineCapacityCfg;

typedef struct
{
    void *context;
    const char *line;
    unsigned long long size;
} CliEachLineSizeCfg;

typedef struct
{
    void *context;
    const char *module;
    unsigned long long module_length;
    const char *importers;
    unsigned long long importers_length;
} CliEachModuleImportersCfg;

typedef struct
{
    void *context;
    const char *name;
} CliEachNameCfg;

typedef struct
{
    void *context;
    const char *name;
    int is_folder;
} CliEachNameIsFolderCfg;

typedef struct
{
    void *context;
    const char *name;
    unsigned long long length;
    VerbaLinea *out;
} CliEachNameLengthOutCfg;

typedef struct
{
    void *context;
    const char *name;
    unsigned long long name_size;
    const char *message;
    unsigned long long message_size;
    const char *path;
    long long line;
} CliEachNameMessagePathLineCfg;

typedef struct
{
    void *context;
    const char *name;
    unsigned long long size;
} CliEachNameSizeCfg;

typedef struct
{
    void *context;
    const char *name;
    const char *value;
} CliEachNameValueCfg;

typedef struct
{
    void *context;
    const char *notice;
} CliEachNoticeCfg;

typedef struct
{
    void *context;
    const char *path;
} CliEachPathCfg;

typedef struct
{
    void *context;
    const char *path;
    unsigned int kind;
} CliEachPathKindCfg;

typedef struct
{
    void *context;
    const char *path;
    unsigned long long length;
} CliEachPathLengthCfg;

typedef struct
{
    void *context;
    const char *path;
    const char *text;
    unsigned long long length;
} CliEachPathTextLengthCfg;

typedef struct
{
    void *context;
    unsigned int pid;
    const char *name;
    const char *command;
} CliEachPidNameCommandCfg;

typedef struct
{
    unsigned int place;
} CliEachPositionCfg;

typedef struct
{
    void *context;
    const char *displayed;
} CliEachDisplayedCfg;

typedef struct
{
    void *context;
    unsigned int step;
    unsigned int line;
    const char *value;
} CliEachStepLineValueCfg;

typedef struct
{
    void *context;
    unsigned int target;
} CliEachTargetCfg;

typedef struct
{
    void *context;
    const char *word;
    unsigned long long size;
} CliEachWordSizeCfg;

typedef struct
{
    void *context;
    unsigned long long written;
} CliEachWrittenCfg;

typedef struct
{
    void *context;
    unsigned long long first;
    unsigned long long last;
    int displayed;
} CliFoldEachCfg;

typedef struct
{
    void *context;
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
} CliFound3Cfg;

typedef struct
{
    void *context;
    unsigned long long word;
    unsigned long long word_size;
    unsigned long long rest;
    unsigned long long remainder_end;
} CliGradleStatementCfg;

typedef struct
{
    void *context;
    unsigned int line;
    unsigned int col;
    const char *name;
    unsigned long long length;
} CliHintedCfg;

typedef struct
{
    void *context;
    unsigned long long line;
    unsigned long long col;
    const unsigned char *text;
    unsigned long long size;
} CliHitAppendCfg;

typedef struct
{
    const char *id;
    unsigned long long id_length;
    long long when;
    const char *made;
    unsigned long long created_length;
} CliIdWhenCreatedCfg;

typedef struct
{
    void *context;
    const char *path;
    const char *finding;
    unsigned long long size;
} CliInspectFoundCfg;

typedef struct
{
    void *context;
    const char *path;
    const char *class_name;
    const char *attribute;
} CliInspectSuppliedCfg;

typedef struct
{
    const void *items;
    unsigned int at;
} CliItemsAtCfg;

typedef struct
{
    void *context;
    const char *message;
    unsigned long long size;
} CliKernelRecvCfg;

typedef struct
{
    void *context;
    const char *state;
    const char *detail;
} CliLinkStatedCfg;

typedef struct
{
    void *context;
    const char *method;
    const char *params;
    unsigned long long size;
    VerbaLinea *answer;
} CliLspRecvCfg;

typedef struct
{
    void *context;
    unsigned int place;
    const char *name;
} CliNameEachCfg;

typedef struct
{
    const char *name;
} CliNameCfg;

typedef struct
{
    void *context;
    const char *path;
    unsigned long long *size;
} CliOpenTextCfg;

typedef struct
{
    void *context;
    const char *source;
    const char *target;
    int folder;
    VerbaLinea *err_msg;
} CliPasteEachCfg;

typedef struct
{
    void *context;
    const char *path;
    unsigned long long done;
    unsigned long long size;
} CliProgressCfg;

typedef struct
{
    void *context;
    const char *written;
    unsigned long long size;
    unsigned long long from;
    unsigned long long to;
} CliSconsScriptCfg;

typedef struct
{
    void *context;
    unsigned long long from;
    unsigned long long to;
} CliSconsSpanCfg;

typedef struct
{
    const void *job;
    unsigned long long size;
} CliServersJobCfg;

typedef struct
{
    void *sink;
    const void *bytes;
    unsigned long long size;
} CliSinkBytesSizeCfg;

typedef struct
{
    void *context;
    const char *path;
    unsigned int line;
    const char *text;
    unsigned long long length;
} CliSliceLineCfg;

typedef struct
{
    void *context;
    unsigned int first;
    unsigned int last;
    const unsigned int *opener;
    unsigned int openers;
} CliSliceWindowCfg;

typedef struct
{
    void *source;
    char *piece;
    unsigned long long capacity;
} CliSourcePieceCapacityCfg;

typedef struct
{
    void *context;
    const char *path;
    unsigned int line;
    unsigned int from;
    unsigned int to;
    const char *text;
    unsigned long long length;
} CliUsageCfg;

typedef struct
{
    void *context;
    int displayed;
} GuiEachDisplayedCfg;

typedef struct
{
    void *context;
    unsigned int width;
    unsigned int height;
    int scale;
    unsigned int frame_ms;
} GuiEachWidthHeightScaleCfg;

typedef struct
{
    void *context;
    unsigned long long ticket;
    const char *name;
    int ok;
    const char *text;
    unsigned long long size;
} GuiInterfaceEachCfg;

typedef struct
{
    unsigned long long size;
} QuerySizeCfg;

#endif
