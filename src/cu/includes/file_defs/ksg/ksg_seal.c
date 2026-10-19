// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_seal.c: a file's seal made as its bytes are written, as ksg.h lays it out: each line a leaf handed on as it ends,
// the leaves joined two by two as they come, at most KSG_DEPTH nodes waiting, and the root over the size, the most
// bytes of a leaf and the top node
#include "ksg.h"

#include "../../../engine/runtime/obsignatio/obsignatio_internal.h"

#include <string.h>

// `size` bytes signed with `key`, as obsignatio's keyed signum on the host, into `out`: 1
static int ksg_signed(const unsigned char *key, const void *bytes, unsigned long long size, unsigned char *out)
{
    obsignatio_host_digest(key, OBSIGNATIO_MODE_KEYED, (const unsigned char *)bytes, size, out, KSG_SIGNUM_BYTES);
    return 1;
}

// two nodes joined, the left's bytes then the right's signed with the node key, into `out`
static int ksg_joined(const KsgSeal *seal, const unsigned char *left, const unsigned char *right, unsigned char *out)
{
    unsigned char both[2u * KSG_SIGNUM_BYTES];
    memcpy(both, left, KSG_SIGNUM_BYTES);
    memcpy(both + KSG_SIGNUM_BYTES, right, KSG_SIGNUM_BYTES);
    return ksg_signed(seal->node_key, both, sizeof(both), out);
}

// `value` as eight bytes, low byte first, at `out`
static void ksg_word(unsigned long long value, unsigned char *out)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // a word shifted down to its low byte keeps only that byte
        out[at] = (unsigned char)(value >> (8u * at));
    }
}

// the line carried sealed as a leaf, handed on and joined with those before it, the line emptied: 1, or 0 where it
// could not be
static int ksg_line_sealed(KsgSeal *seal)
{
    unsigned char node[KSG_SIGNUM_BYTES];
    if (!seal->whole || !ksg_signed(seal->row_key, seal->line, seal->carried, node) ||
        !seal->leaf(seal->context, node, seal->lines))
    {
        seal->whole = 0;
        return 0;
    }
    seal->carried = 0u;
    seal->leaves += 1ull;
    for (unsigned long long count = seal->leaves; (count & 1ull) == 0ull; count >>= 1u)
    {
        seal->depth -= 1u;
        if (!ksg_joined(seal, seal->waiting[seal->depth], node, node))
        {
            seal->whole = 0;
            return 0;
        }
    }
    memcpy(seal->waiting[seal->depth], node, KSG_SIGNUM_BYTES);
    seal->depth += 1u;
    return 1;
}

int ksg_seal_begun(KsgSeal *seal, KsgLeaf leaf, void *context)
{
    memset(seal, 0, sizeof(*seal));
    seal->leaf = leaf;
    seal->context = context;
    seal->whole = obsignatio_host_level_key(OBSIGNATIO_LEVEL_ROW, seal->row_key) &&
                  obsignatio_host_level_key(OBSIGNATIO_LEVEL_MEMBERS, seal->node_key) &&
                  obsignatio_host_level_key(OBSIGNATIO_LEVEL_FILE, seal->file_key);
    return seal->whole;
}

int ksg_seal_bytes(KsgSeal *seal, const void *bytes, unsigned long long size)
{
    const unsigned char *const from = (const unsigned char *)bytes;
    for (unsigned long long at = 0ull; seal->whole && (at < size); at += 1ull)
    {
        seal->line[seal->carried] = from[at];
        seal->carried += 1u;
        const int line_ended = (from[at] == '\n');
        if (line_ended || (seal->carried == KSG_LINE_MOST))
        {
            (void)ksg_line_sealed(seal);
            seal->lines += line_ended ? 1ull : 0ull;
        }
    }
    seal->size += size;
    return seal->whole;
}

int ksg_seal_ended(KsgSeal *seal, unsigned char *root, unsigned char *trailer)
{
    // the bytes after the last line feed are a last line, and a file of no bytes is one line of none
    if (((seal->carried > 0u) || (seal->leaves == 0ull)) && !ksg_line_sealed(seal))
    {
        return 0;
    }
    unsigned char top[KSG_SIGNUM_BYTES];
    memcpy(top, seal->waiting[seal->depth - 1u], KSG_SIGNUM_BYTES);
    for (unsigned int at = seal->depth - 1u; at > 0u; at -= 1u)
    {
        if (!ksg_joined(seal, seal->waiting[at - 1u], top, top))
        {
            return 0;
        }
    }
    unsigned char rooted[16u + KSG_SIGNUM_BYTES];
    ksg_word(seal->size, rooted);
    ksg_word(KSG_LINE_MOST, rooted + 8u);
    memcpy(rooted + 16u, top, KSG_SIGNUM_BYTES);
    if (!ksg_signed(seal->file_key, rooted, sizeof(rooted), root))
    {
        return 0;
    }
    ksg_word(seal->size, trailer);
    ksg_word(KSG_LINE_MOST, trailer + 8u);
    ksg_word(seal->leaves, trailer + 16u);
    memcpy(trailer + 24u, root, KSG_SIGNUM_BYTES);
    return 1;
}

int ksg_kind_sealed(const char *path)
{
    const char *const dot = strrchr(path, '.');
    const char *const slash = strrchr(path, '/');
    const char *const back = strrchr(path, '\\');
    if ((dot == NULL) || ((slash != NULL) && (slash > dot)) || ((back != NULL) && (back > dot)))
    {
        return 0;
    }
    if ((strcmp(dot, ".g") == 0) || (strcmp(dot, ".gsm") == 0))
    {
        return 1;
    }
    static const char *const s_own[] = {".ksg", ".ksa", ".ksk", ".kpk"};
    for (unsigned int at = 0u; at < (sizeof(s_own) / sizeof(s_own[0])); at += 1u)
    {
        if (strcmp(dot, s_own[at]) == 0)
        {
            return 0;
        }
    }
    const size_t letters = strlen(dot + 1);
    int lower = (dot[1] == 'k') && (letters >= 3u) && (letters <= 6u);
    for (size_t at = 1u; lower && (at <= letters); at += 1u)
    {
        lower = (dot[at] >= 'a') && (dot[at] <= 'z');
    }
    return lower;
}

void ksg_head(unsigned char *head)
{
    memcpy(head, KREP_MAGIC, 8u);
    memcpy(head + 8u, KSG_KIND, 4u);
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        // the version is one 32-bit limb, low byte first, each byte shifted down to its own
        head[12u + at] = (unsigned char)(KREP_VERSION >> (8u * at));
    }
}
