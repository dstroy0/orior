// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksa.h: the approvals of a file's seal, stored beside it as `<file>.ksa`: a list, each approval one approver's
// signature, by one of the schemes the table of schemes names, over KSA_CONTEXT and then the seal's trailer, which
// contains the file's size, the max bytes of a leaf, the count of leaves and the root. An approver's keys are stored
// as k-files too, the secret `KSA_SECRET_KIND` and the public `KSA_PUBLIC_KIND`, each the scheme's tag then its bytes.
//
//     the k-file head (krep_head.h), type KSA_KIND
//     the count of approvals, one word, low byte first
//     each approval in turn: the scheme's tag, 4 bytes; the bytes of its public key and of its signature, one word
//     each, low byte first; the public key; the signature
//
// A scheme is a row of the table in ksa.c: its tag, its name, its sizes and its three calls. A scheme added is a row
// added, and a list that contains an approval by a scheme a reader does not know is still read, that approval named and
// not verified (ksa.c)
#ifndef KSA_H
#define KSA_H

#include "ksg.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define KSA_KIND "KSA\0"
#define KSA_SUFFIX ".ksa"
#define KSA_SECRET_KIND "KSK\0"
#define KSA_PUBLIC_KIND "KPK\0"
// the bytes every signature is over before the seal's trailer, its zero not among them
#define KSA_CONTEXT "orior ksg approval 1"
#define KSA_TAG_BYTES 4u
// the max approvals of one seal, and the max bytes of any scheme's secret, public key and signature
#define KSA_APPROVALS_MAX 64u
#define KSA_SECRET_MAX 64u
#define KSA_KEY_MAX 64u
#define KSA_SIGNATURE_MAX 128u

    // One scheme: its tag and name, the bytes of its secret, its public key and its signature, its public key made
    // from a secret, a message signed, and a signature verified against its public key and message: 1, or 0
    typedef struct
    {
        char tag[KSA_TAG_BYTES];
        const char *name;
        unsigned int secret_bytes;
        unsigned int key_bytes;
        unsigned int signature_bytes;
        void (*public_of)(const unsigned char *secret, unsigned char *key);
        void (*signed_with)(const unsigned char *secret, const unsigned char *key, const void *message, size_t bytes,
                            unsigned char *signature);
        int (*verify)(const unsigned char *key, const void *message, size_t bytes, const unsigned char *signature);
    } KsaScheme;

    // One approval: its scheme's tag, the bytes of its key and its signature, and them
    typedef struct
    {
        char tag[KSA_TAG_BYTES];
        unsigned int key_bytes;
        unsigned int signature_bytes;
        unsigned char key[KSA_KEY_MAX];
        unsigned char signature[KSA_SIGNATURE_MAX];
    } KsaApproval;

    // the scheme of `name`, or of the 4 bytes of `tag`: it, or NULL where the table has none
    const KsaScheme *ksa_scheme_named(const char *name);
    const KsaScheme *ksa_scheme_tagged(const char *tag);
    // the schemes' names, one after another with a space between, for a reader to be informed
    const char *ksa_scheme_names(void);
    // the trailer of a seal approved with `secret` by `scheme` into `approval`
    void ksa_approved(const KsaScheme *scheme, const unsigned char *secret, const unsigned char *trailer,
                      KsaApproval *approval);
    // whether `approval` is valid for the seal whose trailer is `trailer`: 1, or 0 where it does not or its scheme is
    // not one the table names
    int ksa_verify(const KsaApproval *approval, const unsigned char *trailer);

#ifdef __cplusplus
}
#endif

#endif
