// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_rsa.c: RSA encryption with a public key, as RFC 8017 sets it out, with OAEP padding by SHA-1: the password
// caching_sha2_password takes over a connection that is not encrypted. The numbers are 32-bit words, least first, in
// rooms of a fixed size, enough for a key of 8192 bits and the products a power of it takes
#include "../text/base64.h"
#include "../text/digest.h"
#include "mysql_internal.h"

// a number with no zero words at its top
static void big_trimmed(CliBig *big)
{
    while ((big->count > 0u) && (big->word[big->count - 1u] == 0u))
    {
        big->count -= 1u;
    }
}

// `size` bytes, big-endian, as a number: 1, or 0 where it does not fit the room
int cli_big_from_be(CliBig *big, const unsigned char *bytes, unsigned long long size)
{
    memor.set(&(MemoriaCfg){.dst = big, .val = 0, .bytes = sizeof(*big)});
    if (size > (unsigned long long)CLI_BIG_WORDS * 4ull)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        const unsigned long long place = size - 1ull - at;
        big->word[place / 4u] |= (unsigned int)bytes[at] << (8u * (unsigned int)(place % 4u));
    }
    big->count = (unsigned int)((size + 3u) / 4u);
    big_trimmed(big);
    return 1;
}

// a number as `length` bytes, big-endian, into `out`
void cli_big_to_be(const CliBig *big, unsigned char *out, unsigned long long length)
{
    memor.set(&(MemoriaCfg){.dst = out, .val = 0, .bytes = (size_t)length});
    for (unsigned int index = 0u; index < big->count; index += 1u)
    {
        for (unsigned int shift = 0u; shift < 4u; shift += 1u)
        {
            const unsigned long long at = ((unsigned long long)index * 4ull) + shift;
            if (at < length)
            {
                out[length - 1ull - at] = (unsigned char)((big->word[index] >> (8u * shift)) & 0xffu);
            }
        }
    }
}

static unsigned int big_bits(const CliBig *big)
{
    if (big->count == 0u)
    {
        return 0u;
    }
    unsigned int top = big->word[big->count - 1u];
    unsigned int bits = (big->count - 1u) * 32u;
    while (top != 0u)
    {
        bits += 1u;
        top >>= 1u;
    }
    return bits;
}

static int big_bit(const CliBig *big, unsigned int at)
{
    return ((at / 32u) < big->count) && (((big->word[at / 32u] >> (at % 32u)) & 1u) == 1u);
}

// `a` times `b` into `out`, which is neither: 1, or 0 where the product does not fit the room
int cli_big_mul(const CliBig *a, const CliBig *b, CliBig *out)
{
    memor.set(&(MemoriaCfg){.dst = out, .val = 0, .bytes = sizeof(*out)});
    if (a->count + b->count + 1u > CLI_BIG_WORDS)
    {
        return 0;
    }
    for (unsigned int i = 0u; i < a->count; i += 1u)
    {
        unsigned long long carry = 0ull;
        for (unsigned int j = 0u; j < b->count; j += 1u)
        {
            const unsigned long long sum =
                (unsigned long long)out->word[i + j] + ((unsigned long long)a->word[i] * b->word[j]) + carry;
            out->word[i + j] = (unsigned int)(sum & 0xffffffffull);
            carry = sum >> 32u;
        }
        for (unsigned int at = i + b->count; carry > 0ull; at += 1u)
        {
            const unsigned long long sum = (unsigned long long)out->word[at] + carry;
            out->word[at] = (unsigned int)(sum & 0xffffffffull);
            carry = sum >> 32u;
        }
    }
    out->count = a->count + b->count + 1u;
    big_trimmed(out);
    return 1;
}

static int big_at_least(const CliBig *a, const CliBig *b)
{
    if (a->count != b->count)
    {
        return a->count > b->count;
    }
    for (unsigned int at = a->count; at > 0u; at -= 1u)
    {
        if (a->word[at - 1u] != b->word[at - 1u])
        {
            return a->word[at - 1u] > b->word[at - 1u];
        }
    }
    return 1;
}

// `b` taken from `a`, which is at least `b`
static void big_sub(CliBig *a, const CliBig *b)
{
    long long borrow = 0ll;
    for (unsigned int index = 0u; index < a->count; index += 1u)
    {
        long long value = (long long)a->word[index] - borrow - (long long)((index < b->count) ? b->word[index] : 0u);
        borrow = 0ll;
        if (value < 0ll)
        {
            value += 4294967296ll;
            borrow = 1ll;
        }
        a->word[index] = (unsigned int)value;
    }
    big_trimmed(a);
}

// a number doubled with `bit` added
static void big_shift_in(CliBig *big, int bit)
{
    unsigned int carry = bit ? 1u : 0u;
    for (unsigned int at = 0u; at < big->count; at += 1u)
    {
        const unsigned int next = big->word[at] >> 31u;
        big->word[at] = (big->word[at] << 1u) | carry;
        carry = next;
    }
    if ((carry != 0u) && (big->count < CLI_BIG_WORDS))
    {
        big->word[big->count] = carry;
        big->count += 1u;
    }
}

// `big` modulo `modulus` into `out`, a bit at a time
void cli_big_rem(const CliBig *big, const CliBig *modulus, CliBig *out)
{
    memor.set(&(MemoriaCfg){.dst = out, .val = 0, .bytes = sizeof(*out)});
    for (unsigned int at = big_bits(big); at > 0u; at -= 1u)
    {
        big_shift_in(out, big_bit(big, at - 1u));
        if (big_at_least(out, modulus))
        {
            big_sub(out, modulus);
        }
    }
}

// `base` to the power `exponent` modulo `modulus` into `out`: 1, or 0 where a product does not fit the room
int cli_big_pow_mod(const CliBig *base, const CliBig *exponent, const CliBig *modulus, CliBig *out)
{
    static CliBig s_base;
    static CliBig s_product;
    cli_big_rem(base, modulus, &s_base);
    memor.set(&(MemoriaCfg){.dst = out, .val = 0, .bytes = sizeof(*out)});
    out->word[0] = 1u;
    out->count = 1u;
    for (unsigned int at = big_bits(exponent); at > 0u; at -= 1u)
    {
        if (!cli_big_mul(out, out, &s_product))
        {
            return 0;
        }
        cli_big_rem(&s_product, modulus, out);
        if (big_bit(exponent, at - 1u))
        {
            if (!cli_big_mul(out, &s_base, &s_product))
            {
                return 0;
            }
            cli_big_rem(&s_product, modulus, out);
        }
    }
    return 1;
}

// the most bytes of a key's modulus, a PEM's DER, and a block OAEP pads
#define CLI_RSA_BYTES 1024u
#define CLI_RSA_DER 8192u

// the DER element at `at` of `size` bytes of `der`: its tag and where its contents start and how long they are, `at`
// moved past it: 1, or 0 where it runs past the end
static int rsa_element(const unsigned char *der, unsigned long long size, unsigned long long *at, unsigned char *tag,
                       unsigned long long *start, unsigned long long *length)
{
    if (*at + 2u > size)
    {
        return 0;
    }
    *tag = der[*at];
    unsigned long long count = der[*at + 1u];
    *at += 2u;
    if ((count & 0x80u) != 0u)
    {
        const unsigned long long width = count & 0x7fu;
        count = 0ull;
        for (unsigned long long one = 0ull; one < width; one += 1ull)
        {
            if ((*at >= size) || (count > 0xffffffffull))
            {
                return 0;
            }
            count = (count << 8u) | der[*at];
            *at += 1u;
        }
    }
    if (*at + count > size)
    {
        return 0;
    }
    *start = *at;
    *length = count;
    *at += count;
    return 1;
}

// the key a PEM holds, as a SubjectPublicKeyInfo or as PKCS #1's RSAPublicKey: its modulus and exponent, and the
// modulus's length in bytes into `length`: 1, or 0 where it holds none
static int rsa_public_key(const char *pem, unsigned long long size, CliBig *modulus, CliBig *exponent,
                          unsigned long long *length)
{
    static char s_body[CLI_RSA_DER * 2u];
    static unsigned char s_der[CLI_RSA_DER];
    CliLine body = {s_body, sizeof(s_body), 0ull};
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = pem + at, .val = '\n', .bytes = (size_t)(size - at)});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - pem) : size;
        if ((end - at < 5u) || (memor.cmp(&(MemoriaCfg){.src = pem + at, .other = "-----", .bytes = 5u}) != 0))
        {
            cli_line_counted(&body, pem + at, end - at);
        }
        at = end + 1ull;
    }
    unsigned long long der_size = 0ull;
    if (!cli_line_fits(&body) || !cli_unbase64(body.out, body.at, s_der, sizeof(s_der), &der_size))
    {
        return 0;
    }
    unsigned char tag = 0u;
    unsigned long long outer = 0ull;
    unsigned long long outer_size = 0ull;
    unsigned long long at = 0ull;
    if (!rsa_element(s_der, der_size, &at, &tag, &outer, &outer_size))
    {
        return 0;
    }
    const unsigned char *const inner = s_der + outer;
    unsigned long long inner_at = 0ull;
    unsigned long long first = 0ull;
    unsigned long long first_size = 0ull;
    if (!rsa_element(inner, outer_size, &inner_at, &tag, &first, &first_size))
    {
        return 0;
    }
    const unsigned char *pair = inner;
    unsigned long long pair_size = outer_size;
    if (tag == 0x30u)
    {
        // a SubjectPublicKeyInfo: the algorithm, then the key as a bit string, its first byte the unused bits
        unsigned long long bits = 0ull;
        unsigned long long bits_size = 0ull;
        unsigned long long key_at = 0ull;
        unsigned long long key = 0ull;
        if (!rsa_element(inner, outer_size, &inner_at, &tag, &bits, &bits_size) || (bits_size < 1u) ||
            !rsa_element(inner + bits + 1u, bits_size - 1u, &key_at, &tag, &key, &pair_size))
        {
            return 0;
        }
        pair = inner + bits + 1u + key;
    }
    unsigned long long pair_at = 0ull;
    unsigned long long modulus_at = 0ull;
    unsigned long long modulus_size = 0ull;
    unsigned long long exponent_at = 0ull;
    unsigned long long exponent_size = 0ull;
    if (!rsa_element(pair, pair_size, &pair_at, &tag, &modulus_at, &modulus_size) ||
        !rsa_element(pair, pair_size, &pair_at, &tag, &exponent_at, &exponent_size))
    {
        return 0;
    }
    // the modulus without the zero a DER integer leads a high bit with
    if ((modulus_size > 0u) && (pair[modulus_at] == 0u))
    {
        modulus_at += 1u;
        modulus_size -= 1u;
    }
    *length = modulus_size;
    return (modulus_size <= CLI_RSA_BYTES) && cli_big_from_be(modulus, pair + modulus_at, modulus_size) &&
           cli_big_from_be(exponent, pair + exponent_at, exponent_size);
}

// `length` bytes of the mask MGF1 makes of `size` bytes of `seed` with SHA-1, into `out`
void cli_mysql_mgf1(const unsigned char *seed, unsigned long long size, unsigned char *out, unsigned long long length)
{
    unsigned char input[CLI_RSA_BYTES + 4u];
    unsigned char digest[20];
    memor.cpy(&(MemoriaCfg){.dst = input, .src = seed, .bytes = (size_t)size});
    for (unsigned int counter = 0u; ((unsigned long long)counter * 20ull) < length; counter += 1u)
    {
        for (unsigned int byte = 0u; byte < 4u; byte += 1u)
        {
            input[size + byte] = (unsigned char)((counter >> (24u - (8u * byte))) & 0xffu);
        }
        cli_sha1_bytes(input, size + 4u, digest);
        const unsigned long long at = (unsigned long long)counter * 20ull;
        const unsigned long long piece = ((length - at) < 20u) ? (length - at) : 20ull;
        memor.cpy(&(MemoriaCfg){.dst = out + at, .src = digest, .bytes = (size_t)piece});
    }
}

void cli_mysql_oaep(const unsigned char *message, unsigned long long size, const unsigned char *seed,
                    unsigned long long length, unsigned char *padded)
{
    unsigned char block[CLI_RSA_BYTES];
    unsigned char mask[CLI_RSA_BYTES];
    // the block: the empty label's hash, zeros, a one, then the message
    const unsigned long long block_size = length - 21u;
    memor.set(&(MemoriaCfg){.dst = block, .val = 0, .bytes = sizeof(block)});
    cli_sha1_bytes("", 0ull, block);
    block[block_size - size - 1u] = 1u;
    memor.cpy(&(MemoriaCfg){.dst = block + block_size - size, .src = message, .bytes = (size_t)size});
    cli_mysql_mgf1(seed, 20ull, mask, block_size);
    for (unsigned long long at = 0ull; at < block_size; at += 1ull)
    {
        padded[21u + at] = (unsigned char)(block[at] ^ mask[at]);
    }
    cli_mysql_mgf1(padded + 21u, block_size, mask, 20ull);
    padded[0] = 0u;
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        padded[1u + at] = (unsigned char)(seed[at] ^ mask[at]);
    }
}

int cli_mysql_rsa_sealed(const char *pem, unsigned long long pem_size, const unsigned char *message,
                         unsigned long long size, unsigned char *out, unsigned long long *out_size, CliLine *said)
{
    static CliBig s_modulus;
    static CliBig s_exponent;
    static CliBig s_padded;
    static CliBig s_sealed;
    static CliLock s_lock = CLI_LOCK_INIT;
    unsigned char padded[CLI_RSA_BYTES];
    unsigned char seed[20];
    unsigned long long length = 0ull;
    cli_lock_take(&s_lock);
    if (!rsa_public_key(pem, pem_size, &s_modulus, &s_exponent, &length))
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "the server's RSA key could not be read");
        return 0;
    }
    if ((size + 42u > length) || !cli_random_bytes(seed, sizeof(seed)))
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "the password is too long for the server's RSA key");
        return 0;
    }
    cli_mysql_oaep(message, size, seed, length, padded);
    const int sealed =
        cli_big_from_be(&s_padded, padded, length) && cli_big_pow_mod(&s_padded, &s_exponent, &s_modulus, &s_sealed);
    if (sealed)
    {
        cli_big_to_be(&s_sealed, out, length);
        *out_size = length;
    }
    cli_lock_give(&s_lock);
    if (!sealed)
    {
        cli_line_text(said, "the server's RSA key is longer than orior_cli takes");
    }
    return sealed;
}
