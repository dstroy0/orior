// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_seal.c: a file's seal made as its bytes are written, as ksg.h positions it out: each line a leaf passed on as it
// ends, the leaves joined two by two as they come, at max KSG_DEPTH nodes waiting, and the root over the size, the max
// bytes of a leaf and the top node
#include "../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "ksg.h"
#include "memoria_operor/memoria_operor.h"

#include "../../../src/cu/engine/runtime/obsignatio/obsignatio_internal.h"

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
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = both, .src = left, .bytes = KSG_SIGNUM_BYTES);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = both + KSG_SIGNUM_BYTES, .src = right, .bytes = KSG_SIGNUM_BYTES);
    return ksg_signed(seal->node_key, both, sizeof(both), out);
}

// `value` as eight bytes, low byte first, at `out`
static void ksg_word(unsigned long long value, unsigned char *out)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // a word shifted down to its low byte retains only that byte
        out[at] = (unsigned char)(value >> (8u * at));
    }
}

// the line carried sealed as a leaf, passed on and joined with those before it, the line emptied: 1, or 0 where it
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
    seal->leaves += 1ULL;
    for (unsigned long long count = seal->leaves; (count & 1ULL) == 0ULL; count >>= 1u)
    {
        seal->depth -= 1u;
        if (!ksg_joined(seal, seal->waiting[seal->depth], node, node))
        {
            seal->whole = 0;
            return 0;
        }
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = seal->waiting[seal->depth], .src = node, .bytes = KSG_SIGNUM_BYTES);
    seal->depth += 1u;
    return 1;
}

int ksg_seal_begun(KsgSeal *seal, KsgLeaf leaf, void *context)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = seal, .bytes = sizeof(*seal), .val = 0);
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
    for (unsigned long long at = 0ULL; seal->whole && (at < size); at += 1ULL)
    {
        seal->line[seal->carried] = from[at];
        seal->carried += 1u;
        const int line_ended = (from[at] == '\n');
        if (line_ended || (seal->carried == KSG_LINE_MAX))
        {
            (void)ksg_line_sealed(seal);
            seal->lines += line_ended ? 1ULL : 0ULL;
        }
    }
    seal->size += size;
    return seal->whole;
}

int ksg_seal_ended(KsgSeal *seal, unsigned char *root, unsigned char *trailer)
{
    // the bytes after the last line feed are a last line, and a file of no bytes is one line of none
    if (((seal->carried > 0u) || (seal->leaves == 0ULL)) && !ksg_line_sealed(seal))
    {
        return 0;
    }
    unsigned char top[KSG_SIGNUM_BYTES];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = top, .src = seal->waiting[seal->depth - 1u], .bytes = KSG_SIGNUM_BYTES);
    for (unsigned int at = seal->depth - 1u; at > 0u; at -= 1u)
    {
        if (!ksg_joined(seal, seal->waiting[at - 1u], top, top))
        {
            return 0;
        }
    }
    unsigned char rooted[16u + KSG_SIGNUM_BYTES];
    ksg_word(seal->size, rooted);
    ksg_word(KSG_LINE_MAX, rooted + 8u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = rooted + 16u, .src = top, .bytes = KSG_SIGNUM_BYTES);
    if (!ksg_signed(seal->file_key, rooted, sizeof(rooted), root))
    {
        return 0;
    }
    ksg_word(seal->size, trailer);
    ksg_word(KSG_LINE_MAX, trailer + 8u);
    ksg_word(seal->leaves, trailer + 16u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = trailer + 24u, .src = root, .bytes = KSG_SIGNUM_BYTES);
    return 1;
}

int ksg_type_sealed(const char *path)
{
    const char *const dot =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    if ((dot == NULL) || ((slash != NULL) && (slash > dot)) || ((back != NULL) && (back > dot)))
    {
        return 0;
    }
    if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = dot, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".g",
                    .ci = EMBED_FALSE)) ||
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = dot, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".gsm",
                    .ci = EMBED_FALSE)))
    {
        return 1;
    }
    static const char *const s_own[] = {".ksg", ".ksa", ".ksk", ".kpk"};
    for (unsigned int at = 0u; at < (sizeof(s_own) / sizeof(s_own[0])); at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = dot, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = s_own[at],
                       .ci = EMBED_FALSE))
        {
            return 0;
        }
    }
    const size_t characters =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = dot + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    int lower = (dot[1] == 'k') && (characters >= 3u) && (characters <= 6u);
    size_t at = 1u;
    while (lower && (at <= characters))
    {
        lower = (dot[at] >= 'a') && (dot[at] <= 'z');
        at += 1u;
    }
    return lower;
}

void ksg_head(unsigned char *head)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = head, .src = KREP_MAGIC, .bytes = 8u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = head + 8u, .src = KSG_TYPE, .bytes = 4u);
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        // the version is one 32-bit limb, low byte first, each byte shifted down to its own
        head[12u + at] = (unsigned char)(KREP_VERSION >> (8u * at));
    }
}
