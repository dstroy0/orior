// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ed25519_check.c: SHA-512 held to FIPS 180-4's examples, and Ed25519 to RFC 8032's test vectors (section 7.1): each
// seed's public key, each signature made byte for byte, and each held; a message changed, a signature changed, and a
// signature with the group's order added to its S each refused. Exit 0 where every one held
#include "../../../../../../../src/cu/engine/runtime/scriptura/scriptura.h"
#include "../../../../../../../src/cu/includes/codecs/ed25519/ed25519.h"
#include "../../../../../../../src/cu/includes/codecs/sha512/sha512.h"

#include <string.h>

// One of RFC 8032's vectors, each as hex: the seed, the public key, the message and the signature
typedef struct
{
    const char *seed;
    const char *public_key;
    const char *message;
    const char *signature;
} Ed25519Vector;

static const Ed25519Vector s_vectors[] = {
    {"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
     "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
     "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe246"
     "5"
     "5141438e7a100b"},
    {"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
     "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
     "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb"
     "0"
     "0d291612bb0c00"},
    {"c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
     "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
     "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc"
     "0"
     "27beceea1ec40a"},
};

static char s_said[512];
static unsigned int s_held;
static unsigned int s_failed;

// one check said: held, or failed, by `what`
static void ed25519_check(int held, const char *what)
{
    ScripturaLine line = {s_said, sizeof(s_said), 0ull, NULL, NULL, 0ull};
    scriptura_text(&line, held ? "  ok   " : "  FAIL ");
    scriptura_text(&line, what);
    scriptura_character(&line, '\n');
    (void)scriptura_write(&line, stdout);
    s_held += held ? 1u : 0u;
    s_failed += held ? 0u : 1u;
}

// one hex digit's value
static unsigned int ed25519_digit(char digit)
{
    return (digit <= '9') ? (unsigned int)(digit - '0') : (unsigned int)(digit - 'a' + 10);
}

// the hex `text` as bytes into `bytes`: how many
static size_t ed25519_bytes_of(const char *text, unsigned char *bytes)
{
    const size_t count = strlen(text) / 2u;
    for (size_t at = 0u; at < count; at += 1u)
    {
        // two hex digits make one byte
        bytes[at] = (unsigned char)((ed25519_digit(text[2u * at]) << 4u) | ed25519_digit(text[(2u * at) + 1u]));
    }
    return count;
}

// whether the digest of `text` is the hex `wanted`
static int ed25519_sha_holds(const char *text, const char *wanted)
{
    unsigned char digest[SHA512_BYTES];
    unsigned char expected[SHA512_BYTES];
    sha512(text, strlen(text), digest);
    (void)ed25519_bytes_of(wanted, expected);
    return memcmp(digest, expected, SHA512_BYTES) == 0;
}

int main(void)
{
    ed25519_check(ed25519_sha_holds("",
                                    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0"
                                    "ff8318d2877eec2f63b931bd47417a81a538327af927da3e"),
                  "SHA-512 of no bytes");
    ed25519_check(ed25519_sha_holds("abc", "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a27"
                                           "4fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"),
                  "SHA-512 of abc, FIPS 180-4's one block");
    ed25519_check(
        ed25519_sha_holds("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqkl"
                          "mnopqrlmnopqrsmnopqrstnopqrstu",
                          "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b"
                          "99dec4b5433ac7d329eeb6dd26545e96e55b874be909"),
        "SHA-512 of FIPS 180-4's two blocks");
    unsigned char seed[ED25519_SEED_BYTES];
    unsigned char public_key[ED25519_PUBLIC_BYTES];
    unsigned char made[ED25519_PUBLIC_BYTES];
    unsigned char message[64];
    unsigned char signature[ED25519_SIGNATURE_BYTES];
    unsigned char signed_now[ED25519_SIGNATURE_BYTES];
    for (unsigned int at = 0u; at < (sizeof(s_vectors) / sizeof(s_vectors[0])); at += 1u)
    {
        (void)ed25519_bytes_of(s_vectors[at].seed, seed);
        (void)ed25519_bytes_of(s_vectors[at].public_key, public_key);
        const size_t length = ed25519_bytes_of(s_vectors[at].message, message);
        (void)ed25519_bytes_of(s_vectors[at].signature, signature);
        ed25519_public_of(seed, made);
        ed25519_check(memcmp(made, public_key, sizeof(made)) == 0, "the seed's public key is RFC 8032's");
        ed25519_signed(seed, public_key, message, length, signed_now);
        ed25519_check(memcmp(signed_now, signature, sizeof(signature)) == 0,
                      "the signature is RFC 8032's byte for byte");
        ed25519_check(ed25519_holds(public_key, message, length, signature), "and it holds");
        message[0] ^= 1u;
        ed25519_check(!ed25519_holds(public_key, message, (length > 0u) ? length : 1u, signature),
                      "a message changed is refused");
        message[0] ^= 1u;
        signature[10] ^= 4u;
        ed25519_check(!ed25519_holds(public_key, message, length, signature), "a signature changed is refused");
        signature[10] ^= 4u;
    }
    // S with the group's order L added, the same signature in a second form, is refused
    static const unsigned char s_order[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                                              0xa2, 0xde, 0xf9, 0xde, 0x14, 0,    0,    0,    0,    0,    0,
                                              0,    0,    0,    0,    0,    0,    0,    0,    0,    0x10};
    unsigned int carry = 0u;
    for (unsigned int at = 0u; at < 32u; at += 1u)
    {
        const unsigned int sum = signature[32u + at] + s_order[at] + carry;
        // the sum's low byte is kept, its high bit carried
        signature[32u + at] = (unsigned char)(sum & 0xffu);
        carry = sum >> 8u;
    }
    const size_t length = ed25519_bytes_of(s_vectors[2].message, message);
    ed25519_check(!ed25519_holds(public_key, message, length, signature),
                  "a signature with L added to its S is refused, no second form of it holding");
    ScripturaLine line = {s_said, sizeof(s_said), 0ull, NULL, NULL, 0ull};
    scriptura_text(&line, "ed25519 check: ");
    scriptura_decimal(&line, s_held, 1u);
    scriptura_text(&line, " held, ");
    scriptura_decimal(&line, s_failed, 1u);
    scriptura_text(&line, " failed\n");
    (void)scriptura_write(&line, stdout);
    return (s_failed == 0u) ? 0 : 1;
}
