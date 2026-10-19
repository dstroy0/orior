// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksa.c: the table of schemes an approval of a seal is made by, and an approval made and held, as ksa.h lays them out.
// Each scheme signs KSA_CONTEXT and then the seal's trailer
#include "ksa.h"

#include "../../codecs/ed25519/ed25519.h"

#include <string.h>

_Static_assert(ED25519_SEED_BYTES <= KSA_SECRET_MOST, "an Ed25519 seed fits a ksa secret");
_Static_assert(ED25519_PUBLIC_BYTES <= KSA_KEY_MOST, "an Ed25519 public key fits a ksa key");
_Static_assert(ED25519_SIGNATURE_BYTES <= KSA_SIGNATURE_MOST, "an Ed25519 signature fits a ksa signature");

// the bytes a scheme signs: KSA_CONTEXT, then the seal's trailer
#define KSA_MESSAGE_BYTES ((sizeof(KSA_CONTEXT) - 1u) + KSG_TRAILER_BYTES)

static void ksa_ed25519_public_of(const unsigned char *secret, unsigned char *key)
{
    ed25519_public_of(secret, key);
}

static void ksa_ed25519_signed(const unsigned char *secret, const unsigned char *key, const void *message, size_t bytes,
                               unsigned char *signature)
{
    ed25519_signed(secret, key, message, bytes, signature);
}

static int ksa_ed25519_holds(const unsigned char *key, const void *message, size_t bytes,
                             const unsigned char *signature)
{
    return ed25519_holds(key, message, bytes, signature);
}

// the schemes an approval is made by, one row each
static const KsaScheme s_schemes[] = {
    {{'E', 'D', '2', '5'},
     "ed25519",
     ED25519_SEED_BYTES,
     ED25519_PUBLIC_BYTES,
     ED25519_SIGNATURE_BYTES,
     ksa_ed25519_public_of,
     ksa_ed25519_signed,
     ksa_ed25519_holds},
};

#define KSA_SCHEMES (sizeof(s_schemes) / sizeof(s_schemes[0]))

// the message a scheme signs for the seal whose trailer is `trailer`, into `message`
static void ksa_message(const unsigned char *trailer, unsigned char *message)
{
    memcpy(message, KSA_CONTEXT, sizeof(KSA_CONTEXT) - 1u);
    memcpy(message + (sizeof(KSA_CONTEXT) - 1u), trailer, KSG_TRAILER_BYTES);
}

const KsaScheme *ksa_scheme_named(const char *name)
{
    for (unsigned int at = 0u; at < KSA_SCHEMES; at += 1u)
    {
        if (strcmp(s_schemes[at].name, name) == 0)
        {
            return &s_schemes[at];
        }
    }
    return NULL;
}

const KsaScheme *ksa_scheme_tagged(const char *tag)
{
    for (unsigned int at = 0u; at < KSA_SCHEMES; at += 1u)
    {
        if (memcmp(s_schemes[at].tag, tag, KSA_TAG_BYTES) == 0)
        {
            return &s_schemes[at];
        }
    }
    return NULL;
}

const char *ksa_scheme_names(void)
{
    static char s_names[256];
    size_t used = 0u;
    for (unsigned int at = 0u; at < KSA_SCHEMES; at += 1u)
    {
        const size_t length = strlen(s_schemes[at].name);
        if ((used + length + 2u) > sizeof(s_names))
        {
            break;
        }
        if (used > 0u)
        {
            s_names[used] = ' ';
            used += 1u;
        }
        memcpy(s_names + used, s_schemes[at].name, length);
        used += length;
    }
    s_names[used] = '\0';
    return s_names;
}

void ksa_approved(const KsaScheme *scheme, const unsigned char *secret, const unsigned char *trailer,
                  KsaApproval *approval)
{
    unsigned char message[KSA_MESSAGE_BYTES];
    memset(approval, 0, sizeof(*approval));
    memcpy(approval->tag, scheme->tag, KSA_TAG_BYTES);
    approval->key_bytes = scheme->key_bytes;
    approval->signature_bytes = scheme->signature_bytes;
    scheme->public_of(secret, approval->key);
    ksa_message(trailer, message);
    scheme->signed_with(secret, approval->key, message, sizeof(message), approval->signature);
}

int ksa_holds(const KsaApproval *approval, const unsigned char *trailer)
{
    const KsaScheme *const scheme = ksa_scheme_tagged(approval->tag);
    if ((scheme == NULL) || (approval->key_bytes != scheme->key_bytes) ||
        (approval->signature_bytes != scheme->signature_bytes))
    {
        return 0;
    }
    unsigned char message[KSA_MESSAGE_BYTES];
    ksa_message(trailer, message);
    return scheme->holds(approval->key, message, sizeof(message), approval->signature);
}
