// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef KREP_H
#define KREP_H

#include "../../../engine/engine_config.h"

#include "../kbo/kbo.h"
#include "../kcr/kcr.h"
#include "../kcs/kcs.h"
#include "../kip/kip.h"
#include "../knf/knf.h"
#include "../ksh/ksh.h"
#include "krep_head.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int krep_head_write(FILE *file, const char *kind);

    int krep_head_read(FILE *file, const char *kind);

    int krep_words_write(FILE *file, const unsigned long long *words, size_t count);

    int krep_words_read(FILE *file, unsigned long long *words, size_t count);

    int krep_limbs_write(FILE *file, const unsigned int *limbs, size_t count);

    int krep_limbs_read(FILE *file, unsigned int *limbs, size_t count);

    typedef struct
    {
        const char *path;
        EngineStream *stream;
        EngineSideSection *section;
        EngineSeal *seal;
        EngineError *error;
    } KrepCrystalRequest;

    unsigned long long krep_crystal_bytes(const EngineStream *stream, const EngineSideSection *section,
                                          const EngineSeal *seal);

    int krep_crystal_write(const KrepCrystalRequest *request);

    int krep_crystal_read(const KrepCrystalRequest *request);

    int krep_crystal_head(const char *path, EngineStream *stream, EngineSignum *root, EngineError *error);

    void krep_crystal_release(EngineStream *stream);

    void krep_side_release(EngineSideSection *section);

    void krep_seal_release(EngineSeal *seal);

    int krep_history_write(const char *path, const EngineHistory *history, EngineError *error);

    int krep_history_read(const char *path, EngineHistory *history, unsigned int payload, EngineError *error);

    void krep_history_release(EngineHistory *history);

    int krep_bodies_write(const char *path, const EngineBodyTable *table, EngineError *error);

    int krep_bodies_read(const char *path, EngineBodyTable *table, EngineError *error);

    void krep_bodies_release(EngineBodyTable *table);

    // a target's construction set as read (kcs.h)
    typedef struct
    {
        unsigned long long forms;
        unsigned long long name_words;
        unsigned long long crc;
        unsigned long long *words;
    } KrepFormTable;

    int krep_forms_write(const char *path, const KrepFormTable *table, EngineError *error);

    int krep_forms_read(const char *path, KrepFormTable *table, EngineError *error);

    void krep_forms_release(KrepFormTable *table);

#ifdef __cplusplus
}
#endif

#endif
