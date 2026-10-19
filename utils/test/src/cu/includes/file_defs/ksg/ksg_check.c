// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_check.c: a file's seal held to what ksg.h says it is. The root the seal folds as the lines come is the root of
// the leaves made again whole and joined as RFC 6962 joins a Merkle tree's, at the largest power of two below their
// count; the same bytes handed in pieces of every size seal alike; one line changed changes that leaf and the root and
// no other leaf; a file of no bytes is one leaf; a line longer than KSG_LINE_MOST is a leaf for each KSG_LINE_MOST of
// it; and the trailer holds the size, KSG_LINE_MOST, the count of leaves and the root. Exit 0 where every one held
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio_internal.h"
#include "../../../../../../../src/cu/includes/file_defs/ksg/ksg.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <string.h>

// the most leaves a check makes, and the most bytes of a text it seals
#define KSG_CHECK_LEAVES 4096u
#define KSG_CHECK_BYTES (1u << 20u)

// The leaves one seal handed on, in order
typedef struct
{
    unsigned char leaf[KSG_CHECK_LEAVES][KSG_SIGNUM_BYTES];
    unsigned int count;
} KsgCheckLeaves;

static KsgCheckLeaves s_one;
static KsgCheckLeaves s_other;
static char s_text[KSG_CHECK_BYTES];
static unsigned int s_failed;
static unsigned int s_held;

// one check said: held, or failed, by `what`
static void ksg_check(int held, const char *what)
{
    printf("%s%s\n", held ? "  ok   " : "  FAIL ", what);
    s_held += held ? 1u : 0u;
    s_failed += held ? 0u : 1u;
}

// a leaf kept in the leaves handed as context
static int ksg_kept(void *context, const unsigned char *leaf, unsigned long long line)
{
    (void)line;
    KsgCheckLeaves *const leaves = context;
    if (leaves->count == KSG_CHECK_LEAVES)
    {
        return 0;
    }
    memcpy(leaves->leaf[leaves->count], leaf, KSG_SIGNUM_BYTES);
    leaves->count += 1u;
    return 1;
}

// `size` bytes of `text` sealed handed `piece` bytes at a time into `leaves`, its root into `root` and its trailer into
// `trailer`: 1, or 0 where the seal did not end whole
static int ksg_sealed(const char *text, unsigned long long size, unsigned long long piece, KsgCheckLeaves *leaves,
                      unsigned char *root, unsigned char *trailer)
{
    static KsgSeal s_seal;
    leaves->count = 0u;
    int whole = ksg_seal_begun(&s_seal, ksg_kept, leaves);
    for (unsigned long long at = 0ull; whole && (at < size); at += piece)
    {
        const unsigned long long left = size - at;
        whole = ksg_seal_bytes(&s_seal, text + at, (left < piece) ? left : piece);
    }
    return whole && ksg_seal_ended(&s_seal, root, trailer);
}

// the node of leaves `from` to `to`, RFC 6962's way: one leaf is itself, and more are the node of those below the
// largest power of two under their count joined with the node of the rest
static void ksg_tree(const unsigned char *node_key, const KsgCheckLeaves *leaves, unsigned int from, unsigned int to,
                     unsigned char *out)
{
    if ((to - from) == 1u)
    {
        memcpy(out, leaves->leaf[from], KSG_SIGNUM_BYTES);
        return;
    }
    unsigned int split = 1u;
    while ((split * 2u) < (to - from))
    {
        split *= 2u;
    }
    unsigned char both[2u * KSG_SIGNUM_BYTES];
    ksg_tree(node_key, leaves, from, from + split, both);
    ksg_tree(node_key, leaves, from + split, to, both + KSG_SIGNUM_BYTES);
    obsignatio_host_digest(node_key, OBSIGNATIO_MODE_KEYED, both, sizeof(both), out, KSG_SIGNUM_BYTES);
}

// the root of `leaves` over `size` bytes made again whole, into `root`
static void ksg_root_again(const KsgCheckLeaves *leaves, unsigned long long size, unsigned char *root)
{
    unsigned char node_key[KSG_SIGNUM_BYTES];
    unsigned char file_key[KSG_SIGNUM_BYTES];
    (void)obsignatio_host_level_key(OBSIGNATIO_LEVEL_MEMBERS, node_key);
    (void)obsignatio_host_level_key(OBSIGNATIO_LEVEL_FILE, file_key);
    unsigned char rooted[16u + KSG_SIGNUM_BYTES];
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // each word shifted down to one byte keeps only that byte
        rooted[at] = (unsigned char)(size >> (8u * at));
        rooted[8u + at] = (unsigned char)((unsigned long long)KSG_LINE_MOST >> (8u * at));
    }
    ksg_tree(node_key, leaves, 0u, leaves->count, rooted + 16u);
    obsignatio_host_digest(file_key, OBSIGNATIO_MODE_KEYED, rooted, sizeof(rooted), root, KSG_SIGNUM_BYTES);
}

// a text of `lines` lines, each its number and letters after it, a line of `long_line` bytes among them where it is
// not 0: its size
static unsigned long long ksg_text(unsigned int lines, unsigned int long_line)
{
    const size_t room = sizeof(s_text);
    size_t text = 0u;
    for (unsigned int line = 0u; line < lines; line += 1u)
    {
        text = verba_textus.put(&(VerbaTextusCfg){.out = s_text, .cap = room, .at = text, .text = "rel "});
        text = verba_numerus.u64(&(VerbaNumerusCfg){.out = s_text, .cap = room, .at = text, .val = line});
        text = verba_textus.put(&(VerbaTextusCfg){
            .out = s_text, .cap = room, .at = text, .text = (line % 3u) == 0u ? " lt r1 r2" : " ge r3 r4"});
        for (unsigned int letter = 0u; (line == 7u) && (letter < long_line); letter += 1u)
        {
            text = verba_littera.ch(
                &(VerbaLitteraCfg){.out = s_text, .cap = room, .at = text, .ch = (char)('a' + (letter % 26u))});
        }
        text = verba_littera.ch(&(VerbaLitteraCfg){.out = s_text, .cap = room, .at = text, .ch = '\n'});
    }
    return text;
}

int main(void)
{
    unsigned char root[KSG_SIGNUM_BYTES];
    unsigned char again[KSG_SIGNUM_BYTES];
    unsigned char other[KSG_SIGNUM_BYTES];
    unsigned char trailer[KSG_TRAILER_BYTES];
    const unsigned long long size = ksg_text(1000u, 0u);
    ksg_check(ksg_sealed(s_text, size, size, &s_one, root, trailer) && (s_one.count == 1000u), "a leaf for each line");
    for (unsigned int leaves = 1u; leaves <= 1000u; leaves += (leaves < 40u) ? 1u : 97u)
    {
        const unsigned long long part = ksg_text(leaves, 0u);
        int held = ksg_sealed(s_text, part, part, &s_one, root, trailer);
        ksg_root_again(&s_one, part, again);
        held = held && (memcmp(root, again, KSG_SIGNUM_BYTES) == 0);
        ksg_check(held, (leaves == 1u) ? "the root folded as the lines come is RFC 6962's root of the leaves"
                                       : "and for another count of lines");
    }
    int alike = ksg_sealed(s_text, size, size, &s_one, root, trailer);
    for (unsigned long long piece = 1ull; alike && (piece <= 4096ull); piece *= 3ull)
    {
        alike = ksg_sealed(s_text, size, piece, &s_other, other, trailer) && (s_other.count == s_one.count) &&
                (memcmp(s_other.leaf, s_one.leaf, (size_t)s_one.count * KSG_SIGNUM_BYTES) == 0) &&
                (memcmp(root, other, KSG_SIGNUM_BYTES) == 0);
    }
    ksg_check(alike, "the same bytes handed in pieces of every size seal alike");
    ksg_text(1000u, 0u);
    char *const relation = strstr(s_text, "rel 412 ge");
    relation[8] = 'l';
    relation[9] = 't';
    unsigned int differ = 0u;
    unsigned int where = 0u;
    const int changed = ksg_sealed(s_text, size, size, &s_other, other, trailer);
    for (unsigned int at = 0u; changed && (at < s_one.count); at += 1u)
    {
        if (memcmp(s_one.leaf[at], s_other.leaf[at], KSG_SIGNUM_BYTES) != 0)
        {
            differ += 1u;
            where = at;
        }
    }
    ksg_check(changed && (differ == 1u) && (where == 412u) && (memcmp(root, other, KSG_SIGNUM_BYTES) != 0),
              "one relation changed on line 412 changes that leaf and the root and no other leaf");
    ksg_check(ksg_sealed(s_text, 0ull, 1ull, &s_one, root, trailer) && (s_one.count == 1u),
              "a file of no bytes is one leaf");
    const unsigned long long long_size = ksg_text(10u, 3000u);
    ksg_check(ksg_sealed(s_text, long_size, long_size, &s_one, root, trailer) && (s_one.count == 12u),
              "a line of 3000 bytes and more is a leaf for each KSG_LINE_MOST of it");
    unsigned long long words[3] = {0ull, 0ull, 0ull};
    for (unsigned int word = 0u; word < 3u; word += 1u)
    {
        for (unsigned int at = 0u; at < 8u; at += 1u)
        {
            words[word] |= (unsigned long long)trailer[(word * 8u) + at] << (8u * at);
        }
    }
    ksg_check((words[0] == long_size) && (words[1] == KSG_LINE_MOST) && (words[2] == 12u) &&
                  (memcmp(trailer + 24u, root, KSG_SIGNUM_BYTES) == 0),
              "the trailer holds the size, KSG_LINE_MOST, the count of leaves and the root");
    printf("ksg check: %u held, %u failed\n", s_held, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
