// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_writing.h: a file written whole, and sealed as it is written where it is of a kind ksg seals: the file and its
// seal each written beside its place under a name of the writer's, made durable, and put in its place, the file first
// and then its seal, so a write that fails leaves the file and the seal it would have replaced as they were, and one
// cut between the two leaves a file its old seal does not hold (ksg_writing.c)
#ifndef KSG_WRITING_H
#define KSG_WRITING_H

#include "ksg.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A file being written: its handle and its seal's, -1 where it is not sealed; the seal being made; the file's
    // place, the place it is written at, its seal's place and the place that is written at; whether every write went;
    // and the root once the seal is made
    typedef struct
    {
        long long file;
        long long seal;
        KsgSeal sealing;
        char path[KSG_PATH_LONGEST];
        char part[KSG_PATH_LONGEST];
        char seal_path[KSG_PATH_LONGEST];
        char seal_part[KSG_PATH_LONGEST];
        int whole;
        unsigned char root[KSG_SIGNUM_BYTES];
    } KsgWriting;

    // the file `path` begun, written at its path with `beside` after it, and its seal begun the same way where the
    // file is of a kind ksg seals: 1, or 0 where one could not be made
    int ksg_writing_begun(KsgWriting *writing, const char *path, const char *beside);
    // `size` bytes written after those before, and sealed where the file is, the shape of a line's flush: 1, or 0
    // where they did not go
    int ksg_writing_put(void *context, const char *bytes, unsigned long long size);
    // the file and its seal made durable and put in their places where `whole` is 1 and every write went, or taken
    // away: 1 where they were put in their places, else 0
    int ksg_writing_ended(KsgWriting *writing, int whole);

#ifdef __cplusplus
}
#endif

#endif
