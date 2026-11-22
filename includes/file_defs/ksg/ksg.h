// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg.h: the seal of a file, the Merkle DAG of its bytes as its writer wrote them, stored beside it as `<file>.ksg`.
//
// Each line of the file is a leaf: its bytes up to and with its line feed. A line longer than KSG_LINE_MAX bytes is a
// leaf for each KSG_LINE_MAX of it in turn. The bytes after the last line feed are a last line. A file of no bytes is
// one line of none.
//
// A leaf is the signum of its bytes, keyed for OBSIGNATIO_LEVEL_ROW. Two nodes side by side are a node: the signum of
// the 32 bytes of the left and then of the right, keyed for OBSIGNATIO_LEVEL_MEMBERS.
//
// Each leaf joins with the leaf before it wherever the count of leaves so far is even. The nodes that stay then join
// from the last to the first.
//
// The root is the signum keyed for OBSIGNATIO_LEVEL_FILE of the size of the file and KSG_LINE_MAX, then the top node.
// The size and KSG_LINE_MAX are eight bytes each, low byte first.
//
// A change in one line gives another leaf at that line and another root. The seal then names the line that differs.
//
//   - the k-file head (krep_head.h), type KSG_TYPE.
//   - each leaf in turn, KSG_SIGNUM_BYTES each.
//   - the trailer: the size of the file, KSG_LINE_MAX and the count of leaves, eight bytes each, low byte first. Then
//     the root.
//
// The leaves come before the counts. A writer passes each leaf on as it writes the line that the leaf seals, and keeps
// none (ksg_seal.c)
#ifndef KSG_H
#define KSG_H

#include "../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../krep/krep_head.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define KSG_TYPE "KSG\0"
#define KSG_SUFFIX ".ksg"
#define KSG_SIGNUM_BYTES 32u
// the most bytes of one leaf, one chunk of obsignatio. A leaf is the compressions of one chunk. A seal carries at max one
// chunk between the pieces that it gets
#define KSG_LINE_MAX 1024u
// the most nodes that wait for a join, one for each bit of a count of leaves
#define KSG_DEPTH 64u
#define KSG_TRAILER_BYTES ((3u * 8u) + KSG_SIGNUM_BYTES)
// the most bytes of a path that a seal sits beside
#define KSG_PATH_MAX_LEN 4096u

    _Static_assert(KSG_SIGNUM_BYTES == OBSIGNATIO_SIGNUM_BYTES, "a ksg node is one obsignatio signum");
    _Static_assert(KSG_SIGNUM_BYTES == OBSIGNATIO_KEY_BYTES, "a ksg key is one obsignatio key");

    // where each leaf goes as the seal makes it, in the order of the lines of the file. It goes with the number of its
    // line, the first 0. The result is 1, or 0 where it did not go
    typedef int (*KsgLeaf)(void *context, const unsigned char *leaf, unsigned long long line);

    // A seal in its make. It holds the three keys, and where each leaf goes. It holds the line that it carries until its
    // line feed or KSG_LINE_MAX bytes of it. It holds the nodes that wait for a join, and how many.
    // It holds the leaves, the sealed bytes and the sealed line feeds so far, and the number of the line that it carries.
    // It holds 0 once the make of a leaf failed, or a leaf did not go
    typedef struct
    {
        unsigned char row_key[KSG_SIGNUM_BYTES];
        unsigned char node_key[KSG_SIGNUM_BYTES];
        unsigned char file_key[KSG_SIGNUM_BYTES];
        KsgLeaf leaf;
        void *context;
        unsigned char line[KSG_LINE_MAX];
        unsigned int carried;
        unsigned char waiting[KSG_DEPTH][KSG_SIGNUM_BYTES];
        unsigned int depth;
        unsigned long long leaves;
        unsigned long long size;
        unsigned long long lines;
        int whole;
    } KsgSeal;

    // a seal begun, with each leaf passed to `leaf` with `context`, and its keys made. The result is 1, or 0 where the
    // make of a key failed
    int ksg_seal_begun(KsgSeal *seal, KsgLeaf leaf, void *context);
    // `size` bytes of the file, after the bytes sealed before, sealed. The result is 1, or 0 where the make of a leaf
    // failed or a leaf did not go
    int ksg_seal_bytes(KsgSeal *seal, const void *bytes, unsigned long long size);
    // the carried line, sealed as the last, or one line of none where the file had no bytes. The root goes into `root`,
    // which has room for KSG_SIGNUM_BYTES. The trailer goes into `trailer`, which has room for KSG_TRAILER_BYTES. The
    // result is 1, or 0 where the make of a leaf or a node failed, or a leaf did not go
    int ksg_seal_ended(KsgSeal *seal, unsigned char *root, unsigned char *trailer);
    // the k-file head of a seal into `head`, which has room for KREP_HEAD_BYTES
    void ksg_head(unsigned char *head);
    // whether the file at `path` is of a type that ksg seals. That is a `.g`, a `.gsm`, or a k-file, `.k` and two to five
    // characters after it. The own files of a seal, `.ksg` and `.ksa`, and the keys of an approver, `.ksk` and `.kpk`, are
    // not
    int ksg_type_sealed(const char *path);

#ifdef __cplusplus
}
#endif

#endif
