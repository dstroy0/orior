// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg.h: a file's seal, the Merkle DAG of its bytes as they were written, kept beside it as `<file>.ksg`. Each line of
// the file is a leaf, its bytes up to and with its line feed, a line longer than KSG_LINE_MOST bytes a leaf for each
// KSG_LINE_MOST of it in turn, the bytes after the last line feed a last line, and a file of no bytes one line of none;
// a leaf is the signum of its bytes keyed for OBSIGNATIO_LEVEL_ROW. Two nodes side by side are a node, the signum of
// the left's 32 bytes then the right's keyed for OBSIGNATIO_LEVEL_MEMBERS, each leaf joined with the one before it
// wherever the count of leaves so far is even, and those left joined from the last to the first. The root is the signum
// keyed for OBSIGNATIO_LEVEL_FILE of the file's size and KSG_LINE_MOST, eight bytes each, low byte first, then the top
// node. A file changed in one line has another leaf at that line and another root, so the seal names the line that
// differs.
//
//     the k-file head (krep_head.h), kind KSG_KIND
//     each leaf in turn, KSG_SIGNUM_BYTES each
//     the trailer: the file's size, KSG_LINE_MOST and the count of leaves, eight bytes each, low byte first, then the
//     root
//
// The leaves come before the counts so a writer hands each leaf on as the line it seals is written, holding none
// (ksg_seal.c)
#ifndef KSG_H
#define KSG_H

#include "../../../engine/runtime/obsignatio/obsignatio.h"
#include "../krep/krep_head.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define KSG_KIND "KSG\0"
#define KSG_SUFFIX ".ksg"
#define KSG_SIGNUM_BYTES 32u
// the most bytes of one leaf, one chunk of obsignatio's, so a leaf is one chunk's compressions and a seal carries at
// most one between the pieces it is handed
#define KSG_LINE_MOST 1024u
// the most nodes waiting to be joined, one for each bit of a count of leaves
#define KSG_DEPTH 64u
#define KSG_TRAILER_BYTES ((3u * 8u) + KSG_SIGNUM_BYTES)
// the most bytes of a path a seal is written beside
#define KSG_PATH_LONGEST 4096u

    _Static_assert(KSG_SIGNUM_BYTES == OBSIGNATIO_SIGNUM_BYTES, "a ksg node is one obsignatio signum");
    _Static_assert(KSG_SIGNUM_BYTES == OBSIGNATIO_KEY_BYTES, "a ksg key is one obsignatio key");

    // where each leaf goes as it is made, in the order of the file's lines, with the number of the line it is of, the
    // first 0: 1, or 0 where it did not go
    typedef int (*KsgLeaf)(void *context, const unsigned char *leaf, unsigned long long line);

    // A seal being made: the three keys, where each leaf goes, the line carried until its line feed or KSG_LINE_MOST
    // bytes of it, the nodes waiting to be joined and how many, the leaves so far, the bytes sealed so far, the line
    // feeds sealed so far, which is the number of the line carried, and 0 once a leaf could not be made or did not go
    typedef struct
    {
        unsigned char row_key[KSG_SIGNUM_BYTES];
        unsigned char node_key[KSG_SIGNUM_BYTES];
        unsigned char file_key[KSG_SIGNUM_BYTES];
        KsgLeaf leaf;
        void *context;
        unsigned char line[KSG_LINE_MOST];
        unsigned int carried;
        unsigned char waiting[KSG_DEPTH][KSG_SIGNUM_BYTES];
        unsigned int depth;
        unsigned long long leaves;
        unsigned long long size;
        unsigned long long lines;
        int whole;
    } KsgSeal;

    // a seal begun, each leaf handed to `leaf` with `context`, its keys made: 1, or 0 where a key could not be made
    int ksg_seal_begun(KsgSeal *seal, KsgLeaf leaf, void *context);
    // `size` bytes of the file, after those sealed before, sealed: 1, or 0 where a leaf could not be made or did not go
    int ksg_seal_bytes(KsgSeal *seal, const void *bytes, unsigned long long size);
    // the line carried sealed as the last, or one line of none where the file had no bytes; the root into `root`,
    // which holds KSG_SIGNUM_BYTES, and the trailer into `trailer`, which holds KSG_TRAILER_BYTES: 1, or 0 where a leaf
    // or a node could not be made or a leaf did not go
    int ksg_seal_ended(KsgSeal *seal, unsigned char *root, unsigned char *trailer);
    // the k-file head of a seal into `head`, which holds KREP_HEAD_BYTES
    void ksg_head(unsigned char *head);
    // whether the file at `path` is of a kind ksg seals: a `.g`, a `.gsm`, or a k-file, `.k` and two to five letters
    // after it, other than a seal's own files, `.ksg` and `.ksa`, and an approver's keys, `.ksk` and `.kpk`
    int ksg_kind_sealed(const char *path);

#ifdef __cplusplus
}
#endif

#endif
