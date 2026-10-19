// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_approvals.c: the ksg program's approvals, as ksa.h lays them out: an approver's keys made from the system's
// randomness and kept as k-files, the secret kept from every reader but its owner; a seal approved and the approval
// put in the list beside it through `<file>.ksa.part`, made durable and put in its place whole; and every approval in
// a list held to its seal, with the approvers a check asks for found among those that hold
#include "ksg_tool.h"

#include "../qry/writer/qry_writer_internal.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// The approvals of one seal as the list beside it holds them
typedef struct
{
    unsigned long long count;
    KsaApproval approval[KSA_APPROVALS_MOST];
} KsgList;

static KsgList s_list;

// eight bytes, low byte first, at `out`
static void ksg_word_put(unsigned long long value, unsigned char *out)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        // a word shifted down to its low byte keeps only that byte
        out[at] = (unsigned char)(value >> (8u * at));
    }
}

static unsigned long long ksg_word_got(const unsigned char *bytes)
{
    unsigned long long word = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        word |= (unsigned long long)bytes[at] << (8u * at);
    }
    return word;
}

// the k-file head of `kind` into `head`
static void ksg_head_of(const char *kind, unsigned char *head)
{
    ksg_head(head);
    memcpy(head + 8u, kind, 4u);
}

// `size` bytes of the system's randomness into `bytes`: 1, or 0 where it gave none
static int ksg_random(unsigned char *bytes, unsigned long long size)
{
#if defined(_WIN32)
    // a seed is a few dozen bytes, which a ULONG counts
    return BCryptGenRandom(NULL, bytes, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    const int source = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (source < 0)
    {
        return 0;
    }
    unsigned long long got = 0ull;
    while (got < size)
    {
        const ssize_t read_now = read(source, bytes + got, (size_t)(size - got));
        if (read_now <= 0)
        {
            (void)close(source);
            return 0;
        }
        // a count read is positive here
        got += (unsigned long long)read_now;
    }
    return close(source) == 0;
#endif
}

// `count` pieces written as the file `path` through `<path>.part`, made durable and put in its place, kept from every
// reader but its owner where `secret` is 1: 1, or 0 where it was not
static int ksg_written(const char *path, const unsigned char *const *pieces, const unsigned long long *sizes,
                       unsigned int count, int secret)
{
    static char s_part[KSG_PATH_LONGEST];
    if (!ksg_path_with(path, ".part", s_part))
    {
        return 0;
    }
    const long long file = qry_writer_file_made(s_part);
    if (file < 0)
    {
        return 0;
    }
#if !defined(_WIN32)
    int whole = !secret || (chmod(s_part, S_IRUSR | S_IWUSR) == 0);
#else
    (void)secret;
    int whole = 1;
#endif
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        whole = qry_writer_file_put(file, pieces[at], sizes[at]);
    }
    whole = qry_writer_file_closed(file) && whole && qry_writer_file_placed(s_part, path);
    if (!whole)
    {
        (void)remove(s_part);
    }
    return whole;
}

// a key k-file of `kind` read into its scheme and its bytes, `bytes` of them most: the scheme, or NULL with why said
static const KsaScheme *ksg_key_read(const char *path, const char *kind, int secret, unsigned char *bytes)
{
    unsigned char head[KREP_HEAD_BYTES];
    unsigned char kept[KREP_HEAD_BYTES];
    char tag[KSA_TAG_BYTES];
    ksg_head_of(kind, head);
    FILE *const file = fopen(path, "rb");
    const int opened = (file != NULL) && (fread(kept, 1u, sizeof(kept), file) == sizeof(kept)) &&
                       (memcmp(kept, head, sizeof(head)) == 0) && (fread(tag, 1u, sizeof(tag), file) == sizeof(tag));
    const KsaScheme *const scheme = opened ? ksa_scheme_tagged(tag) : NULL;
    const unsigned int count = (scheme == NULL) ? 0u : (secret ? scheme->secret_bytes : scheme->key_bytes);
    const int read = (scheme != NULL) && (fread(bytes, 1u, count, file) == count);
    if (file != NULL)
    {
        (void)fclose(file);
    }
    if (!read)
    {
        ksg_said(path);
        ksg_said(secret ? ": no secret key of a known scheme\n" : ": no public key of a known scheme\n");
        return NULL;
    }
    return scheme;
}

int ksg_keys_made(const char *name, const char *secret_path, const char *public_path)
{
    const KsaScheme *const scheme = ksa_scheme_named(name);
    if (scheme == NULL)
    {
        ksg_said(name);
        ksg_said(" is no scheme; the schemes are: ");
        ksg_said(ksa_scheme_names());
        ksg_said("\n");
        return 0;
    }
    unsigned char secret[KSA_SECRET_MOST];
    unsigned char key[KSA_KEY_MOST];
    if (!ksg_random(secret, scheme->secret_bytes))
    {
        ksg_said("the system gave no randomness for the key\n");
        return 0;
    }
    scheme->public_of(secret, key);
    unsigned char secret_head[KREP_HEAD_BYTES];
    unsigned char public_head[KREP_HEAD_BYTES];
    ksg_head_of(KSA_SECRET_KIND, secret_head);
    ksg_head_of(KSA_PUBLIC_KIND, public_head);
    const unsigned char *const secret_pieces[3] = {secret_head, (const unsigned char *)scheme->tag, secret};
    const unsigned long long secret_sizes[3] = {KREP_HEAD_BYTES, KSA_TAG_BYTES, scheme->secret_bytes};
    const unsigned char *const public_pieces[3] = {public_head, (const unsigned char *)scheme->tag, key};
    const unsigned long long public_sizes[3] = {KREP_HEAD_BYTES, KSA_TAG_BYTES, scheme->key_bytes};
    const int made = ksg_written(secret_path, secret_pieces, secret_sizes, 3u, 1) &&
                     ksg_written(public_path, public_pieces, public_sizes, 3u, 0);
    memset(secret, 0, sizeof(secret));
    if (!made)
    {
        ksg_said("the keys could not be written\n");
        return 0;
    }
    ksg_said(scheme->name);
    ksg_said(" keys made, the public key ");
    ksg_said_hex(key, scheme->key_bytes);
    ksg_said("\n");
    return 1;
}

int ksg_public_read(const char *public_path, KsaApproval *key)
{
    memset(key, 0, sizeof(*key));
    const KsaScheme *const scheme = ksg_key_read(public_path, KSA_PUBLIC_KIND, 0, key->key);
    if (scheme == NULL)
    {
        return 0;
    }
    memcpy(key->tag, scheme->tag, KSA_TAG_BYTES);
    key->key_bytes = scheme->key_bytes;
    return 1;
}

// the list beside `path` read into s_list, none where there is no list: 1, or 0 where one is there and not whole
static int ksg_list_read(const char *path)
{
    static char s_listed[KSG_PATH_LONGEST];
    s_list.count = 0ull;
    FILE *const file = ksg_path_with(path, KSA_SUFFIX, s_listed) ? fopen(s_listed, "rb") : NULL;
    if (file == NULL)
    {
        return 1;
    }
    unsigned char head[KREP_HEAD_BYTES];
    unsigned char kept[KREP_HEAD_BYTES];
    unsigned char word[8];
    ksg_head_of(KSA_KIND, head);
    int whole = (fread(kept, 1u, sizeof(kept), file) == sizeof(kept)) && (memcmp(kept, head, sizeof(head)) == 0) &&
                (fread(word, 1u, sizeof(word), file) == sizeof(word));
    const unsigned long long count = whole ? ksg_word_got(word) : 0ull;
    whole = whole && (count <= KSA_APPROVALS_MOST);
    for (unsigned long long at = 0ull; whole && (at < count); at += 1ull)
    {
        KsaApproval *const approval = &s_list.approval[at];
        unsigned char sizes[16];
        whole = (fread(approval->tag, 1u, KSA_TAG_BYTES, file) == KSA_TAG_BYTES) &&
                (fread(sizes, 1u, sizeof(sizes), file) == sizeof(sizes));
        const unsigned long long key_bytes = whole ? ksg_word_got(sizes) : 0ull;
        const unsigned long long signature_bytes = whole ? ksg_word_got(sizes + 8u) : 0ull;
        whole = whole && (key_bytes <= KSA_KEY_MOST) && (signature_bytes <= KSA_SIGNATURE_MOST) &&
                (fread(approval->key, 1u, (size_t)key_bytes, file) == key_bytes) &&
                (fread(approval->signature, 1u, (size_t)signature_bytes, file) == signature_bytes);
        // each count was held at most KSA_KEY_MOST or KSA_SIGNATURE_MOST just above
        approval->key_bytes = (unsigned int)key_bytes;
        approval->signature_bytes = (unsigned int)signature_bytes;
    }
    (void)fclose(file);
    s_list.count = whole ? count : 0ull;
    return whole;
}

// s_list written as the list beside `path`: 1, or 0 where it was not
static int ksg_list_written(const char *path)
{
    static char s_listed[KSG_PATH_LONGEST];
    static unsigned char s_bytes[KREP_HEAD_BYTES + 8u +
                                 (KSA_APPROVALS_MOST * (KSA_TAG_BYTES + 16u + KSA_KEY_MOST + KSA_SIGNATURE_MOST))];
    if (!ksg_path_with(path, KSA_SUFFIX, s_listed))
    {
        return 0;
    }
    unsigned long long at = 0ull;
    ksg_head_of(KSA_KIND, s_bytes);
    at += KREP_HEAD_BYTES;
    ksg_word_put(s_list.count, s_bytes + at);
    at += 8u;
    for (unsigned long long one = 0ull; one < s_list.count; one += 1ull)
    {
        const KsaApproval *const approval = &s_list.approval[one];
        memcpy(s_bytes + at, approval->tag, KSA_TAG_BYTES);
        at += KSA_TAG_BYTES;
        ksg_word_put(approval->key_bytes, s_bytes + at);
        ksg_word_put(approval->signature_bytes, s_bytes + at + 8u);
        at += 16u;
        memcpy(s_bytes + at, approval->key, approval->key_bytes);
        at += approval->key_bytes;
        memcpy(s_bytes + at, approval->signature, approval->signature_bytes);
        at += approval->signature_bytes;
    }
    const unsigned char *const pieces[1] = {s_bytes};
    const unsigned long long sizes[1] = {at};
    return ksg_written(s_listed, pieces, sizes, 1u, 0);
}

// whether two approvals are by the same key
static int ksg_same_key(const KsaApproval *one, const KsaApproval *other)
{
    return (memcmp(one->tag, other->tag, KSA_TAG_BYTES) == 0) && (one->key_bytes == other->key_bytes) &&
           (memcmp(one->key, other->key, one->key_bytes) == 0);
}

int ksg_approval_added(const char *secret_path, const char *path, const unsigned char *trailer)
{
    unsigned char secret[KSA_SECRET_MOST];
    const KsaScheme *const scheme = ksg_key_read(secret_path, KSA_SECRET_KIND, 1, secret);
    if (scheme == NULL)
    {
        return 0;
    }
    KsaApproval approval;
    ksa_approved(scheme, secret, trailer, &approval);
    memset(secret, 0, sizeof(secret));
    if (!ksg_list_read(path))
    {
        ksg_said(path);
        ksg_said(": the list of approvals beside it is not whole\n");
        return 0;
    }
    unsigned long long at = 0ull;
    while ((at < s_list.count) && !ksg_same_key(&s_list.approval[at], &approval))
    {
        at += 1ull;
    }
    if (at == KSA_APPROVALS_MOST)
    {
        ksg_said(path);
        ksg_said(": its list holds ");
        ksg_said_number(KSA_APPROVALS_MOST);
        ksg_said(" approvals already\n");
        return 0;
    }
    s_list.approval[at] = approval;
    s_list.count += (at == s_list.count) ? 1ull : 0ull;
    if (!ksg_list_written(path))
    {
        ksg_said(path);
        ksg_said(": its list of approvals could not be written\n");
        return 0;
    }
    ksg_said("  approved by ");
    ksg_said(scheme->name);
    ksg_said(" ");
    ksg_said_hex(approval.key, approval.key_bytes);
    ksg_said("\n");
    return 1;
}

int ksg_approvals_checked(const char *path, const unsigned char *trailer, const KsaApproval *required,
                          unsigned int count)
{
    static int s_holds[KSA_APPROVALS_MOST];
    if (!ksg_list_read(path))
    {
        ksg_said("  the list of approvals beside it is not whole\n");
        return 0;
    }
    int whole = 1;
    for (unsigned long long at = 0ull; at < s_list.count; at += 1ull)
    {
        const KsaApproval *const approval = &s_list.approval[at];
        const KsaScheme *const scheme = ksa_scheme_tagged(approval->tag);
        s_holds[at] = (scheme != NULL) && ksa_holds(approval, trailer);
        whole = whole && ((scheme == NULL) || s_holds[at]);
        ksg_said("  approved by ");
        ksg_said((scheme != NULL) ? scheme->name : "a scheme not known");
        ksg_said(" ");
        ksg_said_hex(approval->key, approval->key_bytes);
        ksg_said((scheme == NULL) ? ": not held\n" : (s_holds[at] ? ": holds\n" : ": does not hold\n"));
        ksg_said_out();
    }
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        unsigned long long at = 0ull;
        while ((at < s_list.count) && !(s_holds[at] && ksg_same_key(&s_list.approval[at], &required[one])))
        {
            at += 1ull;
        }
        if (at == s_list.count)
        {
            whole = 0;
            ksg_said("  no approval holds by the approver ");
            ksg_said_hex(required[one].key, required[one].key_bytes);
            ksg_said("\n");
            ksg_said_out();
        }
    }
    if (s_list.count == 0ull)
    {
        ksg_said("  no approvals\n");
    }
    return whole;
}
