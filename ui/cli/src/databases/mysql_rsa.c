// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// mysql_rsa.c: RSA encryption with a public key, as RFC 8017 sets it out, with OAEP padding by SHA-1: the password
// caching_sha2_password takes over a connection that is not encrypted. The numbers are 32-bit words, least first, in
// capacities of a fixed size, enough for a key of 8192 bits and the products a power of it takes
#include "../text/base64.h"
#include "../text/digest.h"
#include "../text/text.h"
#include "mysql_internal.h"

typedef struct
{
    CliBig *big;
    const CliBig *big_read;
    unsigned int at;
    const CliBig *a;
    const CliBig *b;
    CliBig *a_out;
    int bit;
    const unsigned char *der;
    unsigned long long size;
    unsigned long long *at_u64s;
    unsigned char *tag;
    unsigned long long *start;
    unsigned long long *length;
    const char *pem;
    CliBig *modulus;
    CliBig *exponent;
} MysqlRsaCtx;

// a number with no zero words at its top
EMBED_INLINE void big_trim(const MysqlRsaCtx *args)
{
    while ((args->big->count > 0u) && (args->big->word[args->big->count - 1u] == 0u))
    {
        args->big->count -= 1u;
    }
}

// `size` bytes, big-endian, as a number: 1, or 0 where it does not fit the capacity
int cli_big_from_be(const CliMysqlCtx *args)
{
    const unsigned char *bytes = args->bytes;
    memor.set(&(MemoriaCfg){.dst = args->big_out, .val = 0, .bytes = sizeof(*args->big_out)});
    if (args->size > (unsigned long long)CLI_BIG_WORDS * 4ull)
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        const unsigned long long place = args->size - 1ull - at;
        args->big_out->word[place / 4u] |= (unsigned int)bytes[at] << (8u * (unsigned int)(place % 4u));
    }
    args->big_out->count = (unsigned int)((args->size + 3u) / 4u);
    EMBED_CALL(big_trim, MysqlRsaCtx, .big = args->big_out);
    return 1;
}

// a number as `length` bytes, big-endian, into `out`
void cli_big_to_be(const CliMysqlBigOutCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->out, .val = 0, .bytes = (size_t)args->length});
    for (unsigned int index = 0u; index < args->big->count; index += 1u)
    {
        for (unsigned int shift = 0u; shift < 4u; shift += 1u)
        {
            const unsigned long long at = ((unsigned long long)index * 4ull) + shift;
            if (at < args->length)
            {
                args->out[args->length - 1ull - at] = (unsigned char)((args->big->word[index] >> (8u * shift)) & 0xffu);
            }
        }
    }
}

EMBED_INLINE unsigned int big_bits(const MysqlRsaCtx *args)
{
    if (args->big_read->count == 0u)
    {
        return 0u;
    }
    unsigned int top = args->big_read->word[args->big_read->count - 1u];
    unsigned int bits = (args->big_read->count - 1u) * 32u;
    while (top != 0u)
    {
        bits += 1u;
        top >>= 1u;
    }
    return bits;
}

EMBED_INLINE int big_bit(const MysqlRsaCtx *args)
{
    return ((args->at / 32u) < args->big_read->count) &&
           (((args->big_read->word[args->at / 32u] >> (args->at % 32u)) & 1u) == 1u);
}

// `a` times `b` into `out`, which is neither: 1, or 0 where the product does not fit the capacity
int cli_big_mul(const CliMysqlACtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->out_big_pointer, .val = 0, .bytes = sizeof(*args->out_big_pointer)});
    if (args->a->count + args->b->count + 1u > CLI_BIG_WORDS)
    {
        return 0;
    }
    for (unsigned int i = 0u; i < args->a->count; i += 1u)
    {
        unsigned long long carry = 0ull;
        for (unsigned int j = 0u; j < args->b->count; j += 1u)
        {
            const unsigned long long sum = (unsigned long long)args->out_big_pointer->word[i + j] +
                                           ((unsigned long long)args->a->word[i] * args->b->word[j]) + carry;
            args->out_big_pointer->word[i + j] = (unsigned int)(sum & 0xffffffffull);
            carry = sum >> 32u;
        }
        for (unsigned int at = i + args->b->count; carry > 0ull; at += 1u)
        {
            const unsigned long long sum = (unsigned long long)args->out_big_pointer->word[at] + carry;
            args->out_big_pointer->word[at] = (unsigned int)(sum & 0xffffffffull);
            carry = sum >> 32u;
        }
    }
    args->out_big_pointer->count = args->a->count + args->b->count + 1u;
    EMBED_CALL(big_trim, MysqlRsaCtx, .big = args->out_big_pointer);
    return 1;
}

EMBED_INLINE int big_at_min(const MysqlRsaCtx *args)
{
    if (args->a->count != args->b->count)
    {
        return args->a->count > args->b->count;
    }
    for (unsigned int at = args->a->count; at > 0u; at -= 1u)
    {
        if (args->a->word[at - 1u] != args->b->word[at - 1u])
        {
            return args->a->word[at - 1u] > args->b->word[at - 1u];
        }
    }
    return 1;
}

// `b` taken from `a`, which is at least `b`
EMBED_INLINE void big_sub(const MysqlRsaCtx *args)
{
    long long borrow = 0ll;
    for (unsigned int index = 0u; index < args->a_out->count; index += 1u)
    {
        long long value = (long long)args->a_out->word[index] - borrow -
                          (long long)((index < args->b->count) ? args->b->word[index] : 0u);
        borrow = 0ll;
        if (value < 0ll)
        {
            value += 4294967296ll;
            borrow = 1ll;
        }
        args->a_out->word[index] = (unsigned int)value;
    }
    EMBED_CALL(big_trim, MysqlRsaCtx, .big = args->a_out);
}

// a number doubled with `bit` added
EMBED_INLINE void big_shift_in(const MysqlRsaCtx *args)
{
    unsigned int carry = args->bit ? 1u : 0u;
    for (unsigned int at = 0u; at < args->big->count; at += 1u)
    {
        const unsigned int next = args->big->word[at] >> 31u;
        args->big->word[at] = (args->big->word[at] << 1u) | carry;
        carry = next;
    }
    if ((carry != 0u) && (args->big->count < CLI_BIG_WORDS))
    {
        args->big->word[args->big->count] = carry;
        args->big->count += 1u;
    }
}

// `big` modulo `modulus` into `out`, a bit at a time
void cli_big_rem(const CliMysqlBigCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->out_big_pointer, .val = 0, .bytes = sizeof(*args->out_big_pointer)});
    for (unsigned int at = EMBED_CALL(big_bits, MysqlRsaCtx, .big_read = args->big); at > 0u; at -= 1u)
    {
        EMBED_CALL(big_shift_in, MysqlRsaCtx, .big = args->out_big_pointer,
                   .bit = EMBED_CALL(big_bit, MysqlRsaCtx, .big_read = args->big, .at = at - 1u));
        if (EMBED_CALL(big_at_min, MysqlRsaCtx, .a = args->out_big_pointer, .b = args->modulus))
        {
            EMBED_CALL(big_sub, MysqlRsaCtx, .a_out = args->out_big_pointer, .b = args->modulus);
        }
    }
}

// `base` to the power `exponent` modulo `modulus` into `out`: 1, or 0 where a product does not fit the capacity
int cli_big_pow_mod(const CliMysqlOutBigCtx *args)
{
    static CliBig s_base;
    static CliBig s_product;
    EMBED_CALL(cli_mysql_big_internal.big_rem, CliMysqlBigCtx, .big = args->base, .modulus = args->modulus,
               .out_big_pointer = &s_base);
    memor.set(&(MemoriaCfg){.dst = args->out_big_pointer, .val = 0, .bytes = sizeof(*args->out_big_pointer)});
    args->out_big_pointer->word[0] = 1u;
    args->out_big_pointer->count = 1u;
    for (unsigned int at = EMBED_CALL(big_bits, MysqlRsaCtx, .big_read = args->exponent); at > 0u; at -= 1u)
    {
        if (!EMBED_CALL(cli_mysql_a_internal.big_mul, CliMysqlACtx, .a = args->out_big_pointer,
                        .b = args->out_big_pointer, .out_big_pointer = &s_product))
        {
            return 0;
        }
        EMBED_CALL(cli_mysql_big_internal.big_rem, CliMysqlBigCtx, .big = &s_product, .modulus = args->modulus,
                   .out_big_pointer = args->out_big_pointer);
        if (EMBED_CALL(big_bit, MysqlRsaCtx, .big_read = args->exponent, .at = at - 1u))
        {
            if (!EMBED_CALL(cli_mysql_a_internal.big_mul, CliMysqlACtx, .a = args->out_big_pointer, .b = &s_base,
                            .out_big_pointer = &s_product))
            {
                return 0;
            }
            EMBED_CALL(cli_mysql_big_internal.big_rem, CliMysqlBigCtx, .big = &s_product, .modulus = args->modulus,
                       .out_big_pointer = args->out_big_pointer);
        }
    }
    return 1;
}

// the most bytes of a key's modulus, a PEM's DER, and a block OAEP pads
#define CLI_RSA_BYTES 1024u
#define CLI_RSA_DER 8192u

// the DER element at `at` of `size` bytes of `der`: its tag and where its contents start and how long they are, `at`
// moved past it: 1, or 0 where it runs past the end
EMBED_INLINE int rsa_element(const MysqlRsaCtx *args)
{
    unsigned long long *at = args->at_u64s;
    unsigned char *tag = args->tag;
    unsigned long long *start = args->start;
    unsigned long long *length = args->length;
    if (*at + 2u > args->size)
    {
        return 0;
    }
    *tag = args->der[*at];
    unsigned long long count = args->der[*at + 1u];
    *at += 2u;
    if ((count & 0x80u) != 0u)
    {
        const unsigned long long width = count & 0x7fu;
        count = 0ull;
        for (unsigned long long one = 0ull; one < width; one += 1ull)
        {
            if ((*at >= args->size) || (count > 0xffffffffull))
            {
                return 0;
            }
            count = (count << 8u) | args->der[*at];
            *at += 1u;
        }
    }
    if (*at + count > args->size)
    {
        return 0;
    }
    *start = *at;
    *length = count;
    *at += count;
    return 1;
}

// the key a PEM contains, as a SubjectPublicKeyInfo or as PKCS #1's RSAPublicKey: its modulus and exponent, and the
// modulus's length in bytes into `length`: 1, or 0 where it contains none
EMBED_INLINE int rsa_public_key(const MysqlRsaCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long *length = args->length;
    static char s_body[CLI_RSA_DER * 2u];
    static unsigned char s_der[CLI_RSA_DER];
    VerbaLinea body = {s_body, sizeof(s_body), 0ull};
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->pem, .at = at, .size = size).stop;
        if ((end - at < 5u) || (memor.cmp(&(MemoriaCfg){.src = args->pem + at, .other = "-----", .bytes = 5u}) != 0))
        {
            mmgr_line_put_n(&body, args->pem + at, end - at);
        }
        at = end + 1ull;
    }
    unsigned long long der_size = 0ull;
    if (!mmgr_line_ok(&body) ||
        !EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = body.out, .length = body.at, .out_bytes = s_der,
                    .capacity = sizeof(s_der), .size_pointer = &der_size))
    {
        return 0;
    }
    unsigned char tag = 0u;
    unsigned long long outer = 0ull;
    unsigned long long outer_size = 0ull;
    unsigned long long at = 0ull;
    if (!EMBED_CALL(rsa_element, MysqlRsaCtx, .der = s_der, .size = der_size, .at_u64s = &at, .tag = &tag,
                    .start = &outer, .length = &outer_size))
    {
        return 0;
    }
    const unsigned char *const inner = s_der + outer;
    unsigned long long inner_at = 0ull;
    unsigned long long first = 0ull;
    unsigned long long first_size = 0ull;
    if (!EMBED_CALL(rsa_element, MysqlRsaCtx, .der = inner, .size = outer_size, .at_u64s = &inner_at, .tag = &tag,
                    .start = &first, .length = &first_size))
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
        if (!EMBED_CALL(rsa_element, MysqlRsaCtx, .der = inner, .size = outer_size, .at_u64s = &inner_at, .tag = &tag,
                        .start = &bits, .length = &bits_size) ||
            (bits_size < 1u) ||
            !EMBED_CALL(rsa_element, MysqlRsaCtx, .der = inner + bits + 1u, .size = bits_size - 1u, .at_u64s = &key_at,
                        .tag = &tag, .start = &key, .length = &pair_size))
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
    if (!EMBED_CALL(rsa_element, MysqlRsaCtx, .der = pair, .size = pair_size, .at_u64s = &pair_at, .tag = &tag,
                    .start = &modulus_at, .length = &modulus_size) ||
        !EMBED_CALL(rsa_element, MysqlRsaCtx, .der = pair, .size = pair_size, .at_u64s = &pair_at, .tag = &tag,
                    .start = &exponent_at, .length = &exponent_size))
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
    return (modulus_size <= CLI_RSA_BYTES) &&
           EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = args->modulus, .bytes = pair + modulus_at,
                      .size = modulus_size) &&
           EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = args->exponent,
                      .bytes = pair + exponent_at, .size = exponent_size);
}

// `length` bytes of the mask MGF1 makes of `size` bytes of `seed` with SHA-1, into `out`
void cli_mysql_mgf1(const CliMysqlSizeOutCtx *args)
{
    unsigned char input[CLI_RSA_BYTES + 4u];
    unsigned char digest[20];
    memor.cpy(&(MemoriaCfg){.dst = input, .src = args->seed, .bytes = (size_t)args->size});
    for (unsigned int counter = 0u; ((unsigned long long)counter * 20ull) < args->length; counter += 1u)
    {
        for (unsigned int byte = 0u; byte < 4u; byte += 1u)
        {
            input[args->size + byte] = (unsigned char)((counter >> (24u - (8u * byte))) & 0xffu);
        }
        EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = input, .size = args->size + 4u,
                   .digest = digest);
        const unsigned long long at = (unsigned long long)counter * 20ull;
        const unsigned long long piece = ((args->length - at) < 20u) ? (args->length - at) : 20ull;
        memor.cpy(&(MemoriaCfg){.dst = args->out + at, .src = digest, .bytes = (size_t)piece});
    }
}

void cli_mysql_oaep(const CliMysqlSizeLengthCtx *args)
{
    unsigned char block[CLI_RSA_BYTES];
    unsigned char mask[CLI_RSA_BYTES];
    // the block: the empty label's hash, zeros, a one, then the message
    const unsigned long long block_size = args->length - 21u;
    memor.set(&(MemoriaCfg){.dst = block, .val = 0, .bytes = sizeof(block)});
    EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = "", .size = 0ull, .digest = block);
    block[block_size - args->size - 1u] = 1u;
    memor.cpy(&(MemoriaCfg){.dst = block + block_size - args->size, .src = args->message, .bytes = (size_t)args->size});
    EMBED_CALL(cli_mysql_size_out_internal.mgf1, CliMysqlSizeOutCtx, .seed = args->seed, .size = 20ull, .out = mask,
               .length = block_size);
    for (unsigned long long at = 0ull; at < block_size; at += 1ull)
    {
        args->padded[21u + at] = (unsigned char)(block[at] ^ mask[at]);
    }
    EMBED_CALL(cli_mysql_size_out_internal.mgf1, CliMysqlSizeOutCtx, .seed = args->padded + 21u, .size = block_size,
               .out = mask, .length = 20ull);
    args->padded[0] = 0u;
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        args->padded[1u + at] = (unsigned char)(args->seed[at] ^ mask[at]);
    }
}

int cli_mysql_rsa_seal(const CliMysqlSizeCtx *args)
{
    unsigned long long *out_size = args->out_size;
    static CliBig s_modulus;
    static CliBig s_exponent;
    static CliBig s_padded;
    static CliBig s_sealed;
    unsigned char padded[CLI_RSA_BYTES];
    unsigned char seed[20];
    unsigned long long length = 0ull;
    if (!EMBED_CALL(rsa_public_key, MysqlRsaCtx, .pem = args->pem, .size = args->pem_size, .modulus = &s_modulus,
                    .exponent = &s_exponent, .length = &length))
    {
        mmgr_line_put(args->err_msg, "the server's RSA key could not be read");
        return 0;
    }
    if ((args->size + 42u > length) ||
        !EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = seed, .size = sizeof(seed)))
    {
        mmgr_line_put(args->err_msg, "the password is too long for the server's RSA key");
        return 0;
    }
    EMBED_CALL(cli_mysql_size_length_internal.oaep, CliMysqlSizeLengthCtx, .message = args->message, .size = args->size,
               .seed = seed, .length = length, .padded = padded);
    const int sealed = EMBED_CALL(cli_mysql_internal.big_from_be, CliMysqlCtx, .big_out = &s_padded, .bytes = padded,
                                  .size = length) &&
                       EMBED_CALL(cli_mysql_out_big_internal.big_pow_mod, CliMysqlOutBigCtx, .base = &s_padded,
                                  .exponent = &s_exponent, .modulus = &s_modulus, .out_big_pointer = &s_sealed);
    if (sealed)
    {
        EMBED_CALL(cli_mysql_big_out_internal.big_to_be, CliMysqlBigOutCtx, .big = &s_sealed, .out = args->out,
                   .length = length);
        *out_size = length;
    }
    if (!sealed)
    {
        mmgr_line_put(args->err_msg, "the server's RSA key is longer than orior_cli takes");
    }
    return sealed;
}
