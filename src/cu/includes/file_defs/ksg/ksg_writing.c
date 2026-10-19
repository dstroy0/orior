// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_writing.c: a file written whole and sealed as it is written, as ksg_writing.h says, through the run writer's
// file calls, which make each file durable and put it in its place
#include "ksg_writing.h"

#include "../qry/writer/qry_writer_internal.h"

#include <string.h>

// `path` with `suffix` after it into `out`, which holds KSG_PATH_LONGEST: 1, or 0 where it does not fit
static int ksg_named(const char *path, const char *suffix, char *out)
{
    const size_t length = strlen(path);
    const size_t more = strlen(suffix);
    if ((length + more) >= KSG_PATH_LONGEST)
    {
        return 0;
    }
    memcpy(out, path, length);
    memcpy(out + length, suffix, more + 1u);
    return 1;
}

// each leaf written after those before it in the seal being written
static int ksg_writing_leaf(void *context, const unsigned char *leaf, unsigned long long line)
{
    (void)line;
    const KsgWriting *const writing = context;
    return qry_writer_file_put(writing->seal, leaf, KSG_SIGNUM_BYTES);
}

int ksg_writing_begun(KsgWriting *writing, const char *path, const char *beside)
{
    writing->file = -1;
    writing->seal = -1;
    writing->whole = 0;
    const int sealed = ksg_kind_sealed(path);
    if (!ksg_named(path, "", writing->path) || !ksg_named(path, beside, writing->part) ||
        (sealed && (!ksg_named(path, KSG_SUFFIX, writing->seal_path) ||
                    !ksg_named(writing->seal_path, beside, writing->seal_part))))
    {
        return 0;
    }
    writing->file = qry_writer_file_made(writing->part);
    if (writing->file < 0)
    {
        return 0;
    }
    writing->whole = 1;
    if (!sealed)
    {
        return 1;
    }
    unsigned char head[KREP_HEAD_BYTES];
    ksg_head(head);
    writing->seal = qry_writer_file_made(writing->seal_part);
    writing->whole = (writing->seal >= 0) && qry_writer_file_put(writing->seal, head, sizeof(head)) &&
                     ksg_seal_begun(&writing->sealing, ksg_writing_leaf, writing);
    return writing->whole;
}

int ksg_writing_put(void *context, const char *bytes, unsigned long long size)
{
    KsgWriting *const writing = context;
    writing->whole = writing->whole && qry_writer_file_put(writing->file, (const unsigned char *)bytes, size) &&
                     ((writing->seal < 0) || ksg_seal_bytes(&writing->sealing, bytes, size));
    return writing->whole;
}

int ksg_writing_ended(KsgWriting *writing, int whole)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    int kept = whole && writing->whole;
    if (writing->seal >= 0)
    {
        kept = kept && ksg_seal_ended(&writing->sealing, writing->root, trailer) &&
               qry_writer_file_put(writing->seal, trailer, sizeof(trailer));
        kept = qry_writer_file_closed(writing->seal) && kept;
    }
    if (writing->file >= 0)
    {
        kept = qry_writer_file_closed(writing->file) && kept;
    }
    if (!kept)
    {
        (void)qry_writer_file_removed(writing->part);
        if (writing->seal >= 0)
        {
            (void)qry_writer_file_removed(writing->seal_part);
        }
        return 0;
    }
    // the file goes first: a write cut between the two leaves the new file beside a seal of the old, which fails
    kept = qry_writer_file_placed(writing->part, writing->path);
    if ((writing->seal >= 0) && !kept)
    {
        (void)qry_writer_file_removed(writing->seal_part);
    }
    return kept && ((writing->seal < 0) || qry_writer_file_placed(writing->seal_part, writing->seal_path));
}
