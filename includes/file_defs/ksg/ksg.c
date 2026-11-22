// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg.c: files sealed, their seals approved, and both checked, as ksg.h and ksa.h position them out.
//
//   - ksg seal <file>...: the seal of each file, written beside it as <file>.ksg. The write goes through
//     <file>.ksg.part, made durable and put in its place whole.
//   - ksg keys <scheme> <secret> <public>: a key pair of an approver of the scheme. Only its owner can read the
//     secret.
//   - ksg approve <secret> <file>...: each file, verified against its seal first. The secret then approves the seal,
//     and the approval goes in the list beside it as <file>.ksa.
//   - ksg check [--approver <public>]... <file>...: each file, sealed again and verified against its seal. The check
//     names each line whose leaf differs, and tells whether the root, the size and the count of leaves match. It
//     verifies each approval in its list against the seal. It finds each named approver among the valid approvals.
//
// None changes a file that it seals, approves or checks. The exit is 0 where each write or verify of a seal, key and
// approval succeeded. It is 1 where one failed, and 2 where the words are none of the above
#include "ksg_tool.h"
#include "../../../src/platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

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
    PlatformFile *seal;
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
    {
        VerbaLinea *const output_line = platform_line_out();
        (void)mmgr_line_put_n(output_line, (const char *)(s_output), (size_t)(1u) * (size_t)(s_output_at));
        platform_line_out_ended();
    }
    s_output_at = 0u;
}

void ksg_print(const char *text)
{
    size_t next =
        verba_textus.put(&(VerbaTextusCfg){.out = s_output, .cap = sizeof(s_output), .at = s_output_at, .text = text});
    if (next == sizeof(s_output))
    {
        // the code writes a text longer than the whole report as far as it fits
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
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        word |= (unsigned long long)bytes[at] << (8u * at);
    }
    return word;
}

// the seal beside `path` opened, its head checked against ksg's and its trailer read into `stored_trailer`: the file,
// or NULL
static PlatformFile *ksg_seal_opened(const char *path, unsigned char *stored_trailer)
{
    static char s_sealed[KSG_PATH_MAX_LEN];
    PlatformFile *const seal = ksg_path_with(path, KSG_SUFFIX, s_sealed) ? platform_file_open(s_sealed, "rb") : NULL;
    unsigned char head[KREP_HEAD_BYTES];
    unsigned char stored_head[KREP_HEAD_BYTES];
    ksg_head(head);
    const int opened = (seal != NULL) && (platform_file_seek(seal, -(long)KSG_TRAILER_BYTES, SEEK_END) == 0) &&
                       (platform_file_read_items(stored_trailer, 1u, KSG_TRAILER_BYTES, seal) == KSG_TRAILER_BYTES) &&
                       (platform_file_seek(seal, 0L, SEEK_SET) == 0) &&
                       (platform_file_read_items(stored_head, 1u, sizeof(stored_head), seal) == sizeof(stored_head)) &&
                       (mmgr_bytes_compare(stored_head, head, sizeof(head)) == 0);
    if (!opened && (seal != NULL))
    {
        (void)platform_file_close(seal);
    }
    return opened ? seal : NULL;
}

int ksg_trailer_read(const char *path, unsigned char *trailer)
{
    PlatformFile *const seal = ksg_seal_opened(path, trailer);
    if (seal == NULL)
    {
        return 0;
    }
    (void)platform_file_close(seal);
    return 1;
}

// `path`, sealed into the begun seal, with its bytes read a piece at a time. The result is 1, or 0 where the read or
// the seal failed
static int ksg_file_sealed(const char *path)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    int whole = 1;
    for (;;)
    {
        const size_t got = platform_file_read_items(s_read, 1u, sizeof(s_read), file);
        whole = whole && ksg_seal_bytes(&s_seal, s_read, got);
        if (got < sizeof(s_read))
        {
            whole = whole && !platform_file_failed(file);
            break;
        }
    }
    (void)platform_file_close(file);
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
    const int read =
        platform_file_read_items(stored_trailer, 1u, sizeof(stored_trailer), checking->seal) == sizeof(stored_trailer);
    checking->leaves += 1ULL;
    if (read && (mmgr_bytes_compare(stored_trailer, leaf, KSG_SIGNUM_BYTES) == 0))
    {
        return 1;
    }
    checking->differ += 1ULL;
    if (checking->differ <= KSG_NAMED_MAX)
    {
        ksg_print("  line ");
        ksg_print_number(line + 1ULL);
        ksg_print(read ? " differs\n" : " has no leaf in the seal\n");
        ksg_print_flush();
    }
    return 1;
}

// `path`, sealed again and verified against its seal. The call verifies its approvals against it, and finds the
// `count` approvers of `required` among them.
// The result is 1 where each leaf, the root, the size, the count and the approvals matched. The call reports the
// reason
static int ksg_checked(const char *path, const KsaApproval *required, unsigned int count)
{
    unsigned char trailer[KSG_TRAILER_BYTES];
    unsigned char stored_trailer[KSG_TRAILER_BYTES];
    PlatformFile *const seal = ksg_seal_opened(path, stored_trailer);
    ksg_print(path);
    if (seal == NULL)
    {
        ksg_print(": no seal of ksg's type beside it\n");
        return 0;
    }
    ksg_print(":\n");
    ksg_print_flush();
    KsgChecking checking = {seal, 0ULL, 0ULL};
    unsigned char root[KSG_SIGNUM_BYTES];
    const int sealed = ksg_seal_begun(&s_seal, ksg_leaf_check, &checking) && ksg_file_sealed(path) &&
                       ksg_seal_ended(&s_seal, root, trailer);
    (void)platform_file_close(seal);
    const int size_matches = ksg_word_read(trailer) == ksg_word_read(stored_trailer);
    const int count_matches = (ksg_word_read(trailer + 16u) == ksg_word_read(stored_trailer + 16u)) &&
                              (ksg_word_read(stored_trailer + 8u) == KSG_LINE_MAX);
    const int root_matches = mmgr_bytes_compare(trailer + 24u, stored_trailer + 24u, KSG_SIGNUM_BYTES) == 0;
    const int seal_matches = sealed && (checking.differ == 0ULL) && size_matches && count_matches && root_matches;
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
    // the call verifies the approvals against the seal as the file stores it. A seal written again over a changed file
    // validates none
    const int approved = ksg_approvals_checked(path, stored_trailer, required, count);
    return seal_matches && approved;
}

// `path`, verified against its seal, and the seal approved with the secret at `secret`. The result is 1, or 0 where
// the file did not match or the add of the approval failed. The call reports the reason
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
    static const char usage[] = "usage: ksg seal <file>...\n"
                                "       ksg keys <scheme> <secret> <public>\n"
                                "       ksg approve <secret> <file>...\n"
                                "       ksg check [--approver <public>]... <file>...\n";
    platform_terminal_write(1, usage, sizeof(usage) - 1u);
    return 2;
}

int program_main(int count, char **words)
{
    if ((count == 5) && (mmgr_text_equal(words[1], "keys", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        const int made = ksg_keys_made(words[2], words[3], words[4]);
        ksg_print_flush();
        return made ? 0 : 1;
    }
    const int sealing = (count >= 3) && (mmgr_text_equal(words[1], "seal", PLATFORM_TEXT_TO_TERMINATOR));
    const int approving = (count >= 4) && (mmgr_text_equal(words[1], "approve", PLATFORM_TEXT_TO_TERMINATOR));
    const int checking = (count >= 3) && (mmgr_text_equal(words[1], "check", PLATFORM_TEXT_TO_TERMINATOR));
    if (!sealing && !approving && !checking)
    {
        return ksg_usage();
    }
    int first = approving ? 3 : 2;
    unsigned int required = 0u;
    int whole = 1;
    while (checking && ((first + 1) < count) &&
           (mmgr_text_equal(words[first], "--approver", PLATFORM_TEXT_TO_TERMINATOR)))
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
