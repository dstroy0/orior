// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg.c: files sealed, their seals approved, and both checked, as ksg.h and ksa.h position them out.
//
//     ksg seal <file>...                     each file's seal written beside it as <file>.ksg, through
//                                            <file>.ksg.part made durable and put in its place whole
//     ksg keys <scheme> <secret> <public>    an approver's key pair of the scheme made, the secret protected from every
//                                            reader but its owner
//     ksg approve <secret> <file>...         each file verified against its seal first, and the seal then approved with
//     the
//                                            secret, the approval put in the list beside it as <file>.ksa
//     ksg check [--approver <public>]... <file>...
//                                            each file sealed again and verified against its seal: every line whose
//                                            leaf differs named, whether the root, the size and the count of leaves
//                                            match, every approval in its list verified against the seal, and each
//                                            approver named found among the approvals that are valid
//
// None changes a file it seals, approves or checks. Exit 0 where every seal, key and approval was written or verified,
// 1 where one was not, 2 where the words are none of the above
#include "ksg_tool.h"

#include "../qry/writer/qry_writer_internal.h"
#include "verba_scribo/verba_scribo.h"

#include <stdio.h>
#include <string.h>

// the bytes of a file read at once, and the max lines that differ named for one file
#define KSG_READ_BYTES (256u << 10u)
#define KSG_NAMED_MAX 64u

// A check under way: the seal's file, the leaves it read so far, and the lines that differ so far
typedef struct
{
    FILE *seal;
    unsigned long long leaves;
    unsigned long long differ;
} KsgChecking;

static unsigned char s_read[KSG_READ_BYTES];
static KsgSeal s_seal;
static KsaApproval s_required[KSG_APPROVERS_MAX];

// the program's report and the bytes it contains so far
static char s_output[1u << 16u];
static size_t s_output_at;

void ksg_print_flush(void)
{
    (void)fwrite(s_output, 1u, s_output_at, stdout);
    s_output_at = 0u;
}

void ksg_print(const char *text)
{
    size_t next =
        verba_textus.put(&(VerbaTextusCfg){.out = s_output, .cap = sizeof(s_output), .at = s_output_at, .text = text});
    if (next == sizeof(s_output))
    {
        // a text longer than the whole report is written as far as it fits
        ksg_print_flush();
        next =
            verba_textus.put_clip(&(VerbaTextusCfg){.out = s_output, .cap = sizeof(s_output), .at = 0u, .text = text});
    }
    s_output_at = next;
}

void ksg_print_number(unsigned long long value)
{
    char digits[24];
    verba_finis.finish(&(VerbaFinisCfg){
        .out = digits,
        .cap = sizeof(digits),
        .at = verba_numerus.u64(&(VerbaNumerusCfg){.out = digits, .cap = sizeof(digits), .at = 0u, .val = value})});
    ksg_print(digits);
}

void ksg_print_hex(const unsigned char *bytes, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char digits[3];
        verba_finis.finish(
            &(VerbaFinisCfg){.out = digits,
                             .cap = sizeof(digits),
                             .at = verba_numerus.hex(&(VerbaNumerusCfg){
                                 .out = digits, .cap = sizeof(digits), .at = 0u, .val = bytes[at], .min = 2u})});
        ksg_print(digits);
    }
}

int ksg_path_with(const char *path, const char *suffix, char *out)
{
    size_t at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = KSG_PATH_MAX_LEN, .at = 0u, .text = path});
    at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = KSG_PATH_MAX_LEN, .at = at, .text = suffix});
    return verba_finis.finish(&(VerbaFinisCfg){.out = out, .cap = KSG_PATH_MAX_LEN, .at = at}) != 0u;
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

// the seal beside `path` opened, its head checked against ksg's and its trailer read into `stored_trailer`: the file,
// or NULL
static FILE *ksg_seal_opened(const char *path, unsigned char *stored_trailer)
{
    static char s_sealed[KSG_PATH_MAX_LEN];
    FILE *const seal = ksg_path_with(path, KSG_SUFFIX, s_sealed) ? fopen(s_sealed, "rb") : NULL;
    unsigned char head[KREP_HEAD_BYTES];
    unsigned char stored_head[KREP_HEAD_BYTES];
    ksg_head(head);
    const int opened = (seal != NULL) && (fseek(seal, -(long)KSG_TRAILER_BYTES, SEEK_END) == 0) &&
                       (fread(stored_trailer, 1u, KSG_TRAILER_BYTES, seal) == KSG_TRAILER_BYTES) &&
                       (fseek(seal, 0L, SEEK_SET) == 0) &&
                       (fread(stored_head, 1u, sizeof(stored_head), seal) == sizeof(stored_head)) &&
                       (memcmp(stored_head, head, sizeof(head)) == 0);
    if (!opened && (seal != NULL))
    {
        (void)fclose(seal);
    }
    return opened ? seal : NULL;
}

int ksg_trailer_read(const char *path, unsigned char *trailer)
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

// `path`'s seal written beside it: 1, or 0 where it was not, why reported
static int ksg_sealed(const char *path)
{
    static char s_part[KSG_PATH_MAX_LEN];
    static char s_placed[KSG_PATH_MAX_LEN];
    long long file = (ksg_path_with(path, KSG_SUFFIX ".part", s_part) && ksg_path_with(path, KSG_SUFFIX, s_placed))
                         ? qry_writer_file_made(s_part)
                         : -1;
    if (file < 0)
    {
        ksg_print(path);
        ksg_print(": its seal could not be made\n");
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
    ksg_print(path);
    if (!whole)
    {
        (void)remove(s_part);
        ksg_print(": it could not be read or its seal written\n");
        return 0;
    }
    ksg_print(": sealed, ");
    ksg_print_number(s_seal.leaves);
    ksg_print(" leaves, root ");
    ksg_print_hex(root, KSG_SIGNUM_BYTES);
    ksg_print("\n");
    return 1;
}

// each leaf checked against the next of the seal's file, a line whose leaf differs named
static int ksg_leaf_check(void *context, const unsigned char *leaf, unsigned long long line)
{
    KsgChecking *const checking = context;
    unsigned char stored_trailer[KSG_SIGNUM_BYTES];
    const int read = fread(stored_trailer, 1u, sizeof(stored_trailer), checking->seal) == sizeof(stored_trailer);
    checking->leaves += 1ull;
    if (read && (memcmp(stored_trailer, leaf, KSG_SIGNUM_BYTES) == 0))
    {
        return 1;
    }
    checking->differ += 1ull;
    if (checking->differ <= KSG_NAMED_MAX)
    {
        ksg_print("  line ");
        ksg_print_number(line + 1ull);
        ksg_print(read ? " differs\n" : " has no leaf in the seal\n");
        ksg_print_flush();
    }
    return 1;
}

// `path` sealed again and verified against its seal, and its approvals verified against it with the `count` approvers
// of `required` found among them: 1 where every leaf, the root, the size, the count and the approvals matched, why
// reported
static int ksg_checked(const char *path, const KsaApproval *required, unsigned int count)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    unsigned char stored_trailer[KSG_TRAILER_BYTES];
    FILE *const seal = ksg_seal_opened(path, stored_trailer);
    ksg_print(path);
    if (seal == NULL)
    {
        ksg_print(": no seal of ksg's type beside it\n");
        return 0;
    }
    ksg_print(":\n");
    ksg_print_flush();
    KsgChecking checking = {seal, 0ull, 0ull};
    unsigned char root[KSG_SIGNUM_BYTES];
    const int sealed = ksg_seal_begun(&s_seal, ksg_leaf_check, &checking) && ksg_file_sealed(path) &&
                       ksg_seal_ended(&s_seal, root, trailer);
    (void)fclose(seal);
    const int size_matches = ksg_word_read(trailer) == ksg_word_read(stored_trailer);
    const int count_matches = (ksg_word_read(trailer + 16u) == ksg_word_read(stored_trailer + 16u)) &&
                              (ksg_word_read(stored_trailer + 8u) == KSG_LINE_MAX);
    const int root_matches = memcmp(trailer + 24u, stored_trailer + 24u, KSG_SIGNUM_BYTES) == 0;
    const int seal_matches = sealed && (checking.differ == 0ull) && size_matches && count_matches && root_matches;
    if (checking.differ > KSG_NAMED_MAX)
    {
        ksg_print("  and ");
        ksg_print_number(checking.differ - KSG_NAMED_MAX);
        ksg_print(" lines more differ\n");
    }
    ksg_print(sealed ? "" : "  it could not be read\n");
    ksg_print(size_matches ? "" : "  its size differs from the seal's\n");
    ksg_print(count_matches ? "" : "  its count of leaves differs from the seal's\n");
    ksg_print(root_matches ? "  the root matches: " : "  the root does not match: sealed ");
    ksg_print_hex(stored_trailer + 24u, KSG_SIGNUM_BYTES);
    if (!root_matches)
    {
        ksg_print(", now ");
        ksg_print_hex(trailer + 24u, KSG_SIGNUM_BYTES);
    }
    ksg_print("\n");
    ksg_print_flush();
    // the approvals are verified against the seal as it is stored, and a seal written again over a changed file
    // validates none
    const int approved = ksg_approvals_checked(path, stored_trailer, required, count);
    return seal_matches && approved;
}

// `path` verified against its seal and the seal approved with the secret at `secret`: 1, or 0 where the file did not
// match or the approval was not added, why reported
static int ksg_approved(const char *secret, const char *path)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    if (!ksg_checked(path, NULL, 0u))
    {
        ksg_print_flush();
        ksg_print("  not approved: the file does not match its seal\n");
        return 0;
    }
    ksg_print_flush();
    return ksg_trailer_read(path, trailer) && ksg_approval_added(secret, path, trailer);
}

// the usage printed
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
        ksg_print_flush();
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
        if ((required == KSG_APPROVERS_MAX) || !ksg_public_read(words[first + 1], &s_required[required]))
        {
            ksg_print_flush();
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
        ksg_print_flush();
    }
    return whole ? 0 : 1;
}
