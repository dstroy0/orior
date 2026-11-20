// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_writing.h: a file written whole, and sealed as it is written where it is of a type ksg seals: the file and its
// seal each written beside its place under a name of the writer's, made durable, and put in its place, the file first
// and then its seal, so a write that fails leaves the file and the seal it would have replaced as they were, and one
// cut between the two leaves a file its old seal does not match (ksg_writing.c)
#ifndef KSG_WRITING_H
#define KSG_WRITING_H

#include "ksg.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A file in its write:
    //   - its handle and the handle of its seal, -1 where it has no seal.
    //   - the seal in its make.
    //   - the place of the file and the place of its write. Then the place of its seal, and the place of the write of
    //     the seal.
    //   - whether each write went.
    //   - the root, once the seal is whole.
    typedef struct
    {
        long long file;
        long long seal;
        KsgSeal sealing;
        char path[KSG_PATH_MAX_LEN];
        char part[KSG_PATH_MAX_LEN];
        char seal_path[KSG_PATH_MAX_LEN];
        char seal_part[KSG_PATH_MAX_LEN];
        int whole;
        unsigned char root[KSG_SIGNUM_BYTES];
    } KsgWriting;

    // the file `path` begun, with its write at its path with `beside` after it. Where the file is of a type that ksg
    // seals, its seal begins the same way. The result is 1, or 0 where a make failed
    int ksg_writing_begun(KsgWriting *writing, const char *path, const char *beside);
    // `size` bytes written after those before, and sealed where the file is, the shape of a line's flush: 1, or 0
    // where they did not go
    int ksg_writing_put(void *context, const char *bytes, unsigned long long size);
    // the file and its seal made durable and put in their places, where `whole` is 1 and each write went. Else the call
    // takes them away. The result is 1 where they went into their places, else 0
    int ksg_writing_ended(KsgWriting *writing, int whole);

#ifdef __cplusplus
}
#endif

#endif
