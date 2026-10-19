// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg.c: files sealed, their seals approved, and both checked, as ksg.h and ksa.h lay them out.
//
//     ksg seal <file>...                     each file's seal written beside it as <file>.ksg, through
//                                            <file>.ksg.part made durable and put in its place whole
//     ksg keys <scheme> <secret> <public>    an approver's key pair of the scheme made, the secret kept from every
//                                            reader but its owner
//     ksg approve <secret> <file>...         each file held to its seal first, and the seal then approved with the
//                                            secret, the approval put in the list beside it as <file>.ksa
//     ksg check [--approver <public>]... <file>...
//                                            each file sealed again and held to its seal: every line whose leaf differs
//                                            named, whether the root, the size and the count of leaves hold, every
//                                            approval in its list held to the seal, and each approver named found
//                                            among the approvals that hold
//
// None changes a file it seals, approves or checks. Exit 0 where every seal, key and approval was written or held, 1
// where one was not, 2 where the words are none of the above
#include "ksg_tool.h"

#include "../qry/writer/qry_writer_internal.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <string.h>

// the bytes of a file read at once, and the most lines that differ named for one file
#define KSG_READ_BYTES (256u << 10u)
#define KSG_NAMED_MOST 64u

// A check under way: the seal's file, the leaves it held so far, and the lines that differ so far
typedef struct
{
    FILE *seal;
    unsigned long long leaves;
    unsigned long long differ;
} KsgChecking;

static unsigned char s_read[KSG_READ_BYTES];
static KsgSeal s_seal;
static KsaApproval s_required[KSG_APPROVERS_MOST];

// the program's saying and the bytes it holds so far
static char s_said[1u << 16u];
static size_t s_said_at;

void ksg_said_out(void)
{
    (void)fwrite(s_said, 1u, s_said_at, stdout);
    s_said_at = 0u;
}

void ksg_said(const char *text)
{
    size_t next =
        verba_textus.put(&(VerbaTextusCfg){.out = s_said, .cap = sizeof(s_said), .at = s_said_at, .text = text});
    if (next == sizeof(s_said))
    {
        // a text longer than the whole saying is said as far as it fits
        ksg_said_out();
        next = verba_textus.put_clip(&(VerbaTextusCfg){.out = s_said, .cap = sizeof(s_said), .at = 0u, .text = text});
    }
    s_said_at = next;
}

void ksg_said_number(unsigned long long value)
{
    char digits[24];
    verba_finis.finish(&(VerbaFinisCfg){
        .out = digits,
        .cap = sizeof(digits),
        .at = verba_numerus.u64(&(VerbaNumerusCfg){.out = digits, .cap = sizeof(digits), .at = 0u, .val = value})});
    ksg_said(digits);
}

void ksg_said_hex(const unsigned char *bytes, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char digits[3];
        verba_finis.finish(
            &(VerbaFinisCfg){.out = digits,
                             .cap = sizeof(digits),
                             .at = verba_numerus.hex(&(VerbaNumerusCfg){
                                 .out = digits, .cap = sizeof(digits), .at = 0u, .val = bytes[at], .min = 2u})});
        ksg_said(digits);
    }
}

int ksg_path_with(const char *path, const char *suffix, char *out)
{
    size_t at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = KSG_PATH_LONGEST, .at = 0u, .text = path});
    at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = KSG_PATH_LONGEST, .at = at, .text = suffix});
    return verba_finis.finish(&(VerbaFinisCfg){.out = out, .cap = KSG_PATH_LONGEST, .at = at}) != 0u;
}

// eight bytes low byte first as a word
static unsigned long long ksg_word_read(const unsigned char *bytes)
{
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        word |= (unsigned long long)bytes[at] << (8u * at);
    }
    return word;
}

// the seal beside `path` opened, its head held to ksg's and its trailer read into `kept`: the file, or NULL
static FILE *ksg_seal_opened(const char *path, unsigned char *kept)
{
    static char s_sealed[KSG_PATH_LONGEST];
    FILE *const seal = ksg_path_with(path, KSG_SUFFIX, s_sealed) ? fopen(s_sealed, "rb") : NULL;
    unsigned char head[KREP_HEAD_BYTES];
    unsigned char kept_head[KREP_HEAD_BYTES];
    ksg_head(head);
    const int opened = (seal != NULL) && (fseek(seal, -(long)KSG_TRAILER_BYTES, SEEK_END) == 0) &&
                       (fread(kept, 1u, KSG_TRAILER_BYTES, seal) == KSG_TRAILER_BYTES) &&
                       (fseek(seal, 0L, SEEK_SET) == 0) &&
                       (fread(kept_head, 1u, sizeof(kept_head), seal) == sizeof(kept_head)) &&
                       (memcmp(kept_head, head, sizeof(head)) == 0);
    if (!opened && (seal != NULL))
    {
        (void)fclose(seal);
    }
    return opened ? seal : NULL;
}

int ksg_trailer_kept(const char *path, unsigned char *trailer)
{
    FILE *const seal = ksg_seal_opened(path, trailer);
    if (seal == NULL)
    {
        return 0;
    }
    (void)fclose(seal);
    return 1;
}

// `path` sealed into the seal begun, its bytes read a piece at a time: 1, or 0 where it could not be read or sealed
static int ksg_file_sealed(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    int whole = 1;
    for (;;)
    {
        const size_t got = fread(s_read, 1u, sizeof(s_read), file);
        whole = whole && ksg_seal_bytes(&s_seal, s_read, got);
        if (got < sizeof(s_read))
        {
            whole = whole && !ferror(file);
            break;
        }
    }
    (void)fclose(file);
    return whole;
}

// each leaf written after those before it in the seal's file, whose handle is the context
static int ksg_leaf_put(void *context, const unsigned char *leaf, unsigned long long line)
{
    (void)line;
    const long long *const file = context;
    return qry_writer_file_put(*file, leaf, KSG_SIGNUM_BYTES);
}

// `path`'s seal written beside it: 1, or 0 where it was not, why said
static int ksg_sealed(const char *path)
{
    static char s_part[KSG_PATH_LONGEST];
    static char s_placed[KSG_PATH_LONGEST];
    long long file = (ksg_path_with(path, KSG_SUFFIX ".part", s_part) && ksg_path_with(path, KSG_SUFFIX, s_placed))
                         ? qry_writer_file_made(s_part)
                         : -1;
    if (file < 0)
    {
        ksg_said(path);
        ksg_said(": its seal could not be made\n");
        return 0;
    }
    unsigned char head[KREP_HEAD_BYTES];
    unsigned char root[KSG_SIGNUM_BYTES];
    unsigned char trailer[KSG_TRAILER_BYTES];
    ksg_head(head);
    int whole = qry_writer_file_put(file, head, sizeof(head)) && ksg_seal_begun(&s_seal, ksg_leaf_put, &file) &&
                ksg_file_sealed(path) && ksg_seal_ended(&s_seal, root, trailer) &&
                qry_writer_file_put(file, trailer, sizeof(trailer));
    whole = qry_writer_file_closed(file) && whole && qry_writer_file_placed(s_part, s_placed);
    ksg_said(path);
    if (!whole)
    {
        (void)remove(s_part);
        ksg_said(": it could not be read or its seal written\n");
        return 0;
    }
    ksg_said(": sealed, ");
    ksg_said_number(s_seal.leaves);
    ksg_said(" leaves, root ");
    ksg_said_hex(root, KSG_SIGNUM_BYTES);
    ksg_said("\n");
    return 1;
}

// each leaf held to the next of the seal's file, a line whose leaf differs named
static int ksg_leaf_held(void *context, const unsigned char *leaf, unsigned long long line)
{
    KsgChecking *const checking = context;
    unsigned char kept[KSG_SIGNUM_BYTES];
    const int read = fread(kept, 1u, sizeof(kept), checking->seal) == sizeof(kept);
    checking->leaves += 1ull;
    if (read && (memcmp(kept, leaf, KSG_SIGNUM_BYTES) == 0))
    {
        return 1;
    }
    checking->differ += 1ull;
    if (checking->differ <= KSG_NAMED_MOST)
    {
        ksg_said("  line ");
        ksg_said_number(line + 1ull);
        ksg_said(read ? " differs\n" : " has no leaf in the seal\n");
        ksg_said_out();
    }
    return 1;
}

// `path` sealed again and held to its seal, and its approvals held to it with the `count` approvers of `required`
// found among them: 1 where every leaf, the root, the size, the count and the approvals held, why said
static int ksg_checked(const char *path, const KsaApproval *required, unsigned int count)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    unsigned char kept[KSG_TRAILER_BYTES];
    FILE *const seal = ksg_seal_opened(path, kept);
    ksg_said(path);
    if (seal == NULL)
    {
        ksg_said(": no seal of ksg's kind beside it\n");
        return 0;
    }
    ksg_said(":\n");
    ksg_said_out();
    KsgChecking checking = {seal, 0ull, 0ull};
    unsigned char root[KSG_SIGNUM_BYTES];
    const int sealed = ksg_seal_begun(&s_seal, ksg_leaf_held, &checking) && ksg_file_sealed(path) &&
                       ksg_seal_ended(&s_seal, root, trailer);
    (void)fclose(seal);
    const int size_held = ksg_word_read(trailer) == ksg_word_read(kept);
    const int count_held =
        (ksg_word_read(trailer + 16u) == ksg_word_read(kept + 16u)) && (ksg_word_read(kept + 8u) == KSG_LINE_MOST);
    const int root_held = memcmp(trailer + 24u, kept + 24u, KSG_SIGNUM_BYTES) == 0;
    const int held = sealed && (checking.differ == 0ull) && size_held && count_held && root_held;
    if (checking.differ > KSG_NAMED_MOST)
    {
        ksg_said("  and ");
        ksg_said_number(checking.differ - KSG_NAMED_MOST);
        ksg_said(" lines more differ\n");
    }
    ksg_said(sealed ? "" : "  it could not be read\n");
    ksg_said(size_held ? "" : "  its size differs from the seal's\n");
    ksg_said(count_held ? "" : "  its count of leaves differs from the seal's\n");
    ksg_said(root_held ? "  the root holds: " : "  the root does not hold: sealed ");
    ksg_said_hex(kept + 24u, KSG_SIGNUM_BYTES);
    if (!root_held)
    {
        ksg_said(", now ");
        ksg_said_hex(trailer + 24u, KSG_SIGNUM_BYTES);
    }
    ksg_said("\n");
    ksg_said_out();
    // the approvals are held to the seal as it is kept, so a seal written again over a changed file holds none
    const int approved = ksg_approvals_checked(path, kept, required, count);
    return held && approved;
}

// `path` held to its seal and the seal approved with the secret at `secret`: 1, or 0 where the file did not hold or
// the approval was not added, why said
static int ksg_approved(const char *secret, const char *path)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    if (!ksg_checked(path, NULL, 0u))
    {
        ksg_said_out();
        ksg_said("  not approved: the file does not hold to its seal\n");
        return 0;
    }
    ksg_said_out();
    return ksg_trailer_kept(path, trailer) && ksg_approval_added(secret, path, trailer);
}

// the usage said
static int ksg_usage(void)
{
    (void)fputs("usage: ksg seal <file>...\n"
                "       ksg keys <scheme> <secret> <public>\n"
                "       ksg approve <secret> <file>...\n"
                "       ksg check [--approver <public>]... <file>...\n",
                stderr);
    return 2;
}

int main(int count, char **words)
{
    if ((count == 5) && (strcmp(words[1], "keys") == 0))
    {
        const int made = ksg_keys_made(words[2], words[3], words[4]);
        ksg_said_out();
        return made ? 0 : 1;
    }
    const int sealing = (count >= 3) && (strcmp(words[1], "seal") == 0);
    const int approving = (count >= 4) && (strcmp(words[1], "approve") == 0);
    const int checking = (count >= 3) && (strcmp(words[1], "check") == 0);
    if (!sealing && !approving && !checking)
    {
        return ksg_usage();
    }
    int first = approving ? 3 : 2;
    unsigned int required = 0u;
    int whole = 1;
    while (checking && ((first + 1) < count) && (strcmp(words[first], "--approver") == 0))
    {
        if ((required == KSG_APPROVERS_MOST) || !ksg_public_read(words[first + 1], &s_required[required]))
        {
            ksg_said_out();
            return 1;
        }
        required += 1u;
        first += 2;
    }
    if (first >= count)
    {
        return ksg_usage();
    }
    for (int at = first; at < count; at += 1)
    {
        const int done = sealing    ? ksg_sealed(words[at])
                         : checking ? ksg_checked(words[at], s_required, required)
                                    : ksg_approved(words[2], words[at]);
        whole = done && whole;
        ksg_said_out();
    }
    return whole ? 0 : 1;
}
