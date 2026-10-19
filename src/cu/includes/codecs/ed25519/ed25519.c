// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ed25519.c: Ed25519 as RFC 8032 states it. A field element of GF(2^255 - 19) is sixteen limbs of 16 bits in signed
// 64-bit words, the limbs carried as the curve's arithmetic needs; a point is its extended coordinates X, Y, Z and T.
// A signed word shifted right keeps its sign, as every compiler the tree builds with shifts it
#include "ed25519.h"

#include "../sha512/sha512.h"

#include <string.h>

// the limbs of a field element, and the bytes of a scalar's reduction
#define ED25519_LIMBS 16u
#define ED25519_WIDE 64u

// A field element, sixteen limbs of 16 bits
typedef long long Ed25519Field[ED25519_LIMBS];

static const Ed25519Field s_zero = {0};
static const Ed25519Field s_one = {1};
// the curve's d, 2d, the base point's x and y, and the square root of -1, each mod 2^255 - 19 (section 5.1)
static const Ed25519Field s_d = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                                 0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
static const Ed25519Field s_d2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
                                  0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};
static const Ed25519Field s_base_x = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
                                      0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};
static const Ed25519Field s_base_y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                                      0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};
static const Ed25519Field s_root_minus_one = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                                              0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};
// the group's order L, low byte first (section 5.1)
static const long long s_order[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                                      0xa2, 0xde, 0xf9, 0xde, 0x14, 0,    0,    0,    0,    0,    0,
                                      0,    0,    0,    0,    0,    0,    0,    0,    0,    0x10};

static void ed25519_set(Ed25519Field out, const Ed25519Field in)
{
    memcpy(out, in, sizeof(Ed25519Field));
}

// each limb carried into the next, the last's carry folded back as 38 times it, 2^256 being 38 mod 2^255 - 19
static void ed25519_carried(Ed25519Field field)
{
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        field[at] += 65536ll;
        const long long carry = field[at] >> 16;
        if (at < (ED25519_LIMBS - 1u))
        {
            field[at + 1u] += carry - 1ll;
        }
        else
        {
            field[0] += 38ll * (carry - 1ll);
        }
        field[at] -= carry * 65536ll;
    }
}

// `one` and `other` swapped where `swap` is 1 and left where it is 0, the same work either way
static void ed25519_swapped(Ed25519Field one, Ed25519Field other, long long swap)
{
    const long long mask = ~(swap - 1ll);
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        const long long flip = mask & (one[at] ^ other[at]);
        one[at] ^= flip;
        other[at] ^= flip;
    }
}

// a field element reduced whole and written as 32 bytes, low byte first
static void ed25519_packed(unsigned char out[32], const Ed25519Field in)
{
    Ed25519Field held;
    Ed25519Field less;
    ed25519_set(held, in);
    ed25519_carried(held);
    ed25519_carried(held);
    ed25519_carried(held);
    for (unsigned int pass = 0u; pass < 2u; pass += 1u)
    {
        less[0] = held[0] - 0xffedll;
        for (unsigned int at = 1u; at < (ED25519_LIMBS - 1u); at += 1u)
        {
            less[at] = held[at] - 0xffffll - ((less[at - 1u] >> 16) & 1ll);
            less[at - 1u] &= 0xffffll;
        }
        less[15] = held[15] - 0x7fffll - ((less[14] >> 16) & 1ll);
        const long long borrowed = (less[15] >> 16) & 1ll;
        less[14] &= 0xffffll;
        ed25519_swapped(held, less, 1ll - borrowed);
    }
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        // a reduced limb holds 16 bits, its two bytes kept apart
        out[2u * at] = (unsigned char)(held[at] & 0xffll);
        out[(2u * at) + 1u] = (unsigned char)((held[at] >> 8) & 0xffll);
    }
}

// whether two field elements differ once reduced: 1, or 0
static int ed25519_differ(const Ed25519Field one, const Ed25519Field other)
{
    unsigned char packed_one[32];
    unsigned char packed_other[32];
    ed25519_packed(packed_one, one);
    ed25519_packed(packed_other, other);
    return memcmp(packed_one, packed_other, sizeof(packed_one)) != 0;
}

// the low bit of a field element reduced
static int ed25519_parity(const Ed25519Field in)
{
    unsigned char packed[32];
    ed25519_packed(packed, in);
    return packed[0] & 1;
}

// 32 bytes, low byte first, as a field element, the top bit let go
static void ed25519_unpacked(Ed25519Field out, const unsigned char in[32])
{
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        out[at] = (long long)in[2u * at] + ((long long)in[(2u * at) + 1u] << 8);
    }
    out[15] &= 0x7fffll;
}

static void ed25519_add(Ed25519Field out, const Ed25519Field one, const Ed25519Field other)
{
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        out[at] = one[at] + other[at];
    }
}

static void ed25519_sub(Ed25519Field out, const Ed25519Field one, const Ed25519Field other)
{
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        out[at] = one[at] - other[at];
    }
}

static void ed25519_mul(Ed25519Field out, const Ed25519Field one, const Ed25519Field other)
{
    long long wide[(2u * ED25519_LIMBS) - 1u] = {0};
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        for (unsigned int by = 0u; by < ED25519_LIMBS; by += 1u)
        {
            wide[at + by] += one[at] * other[by];
        }
    }
    for (unsigned int at = 0u; at < (ED25519_LIMBS - 1u); at += 1u)
    {
        wide[at] += 38ll * wide[at + ED25519_LIMBS];
    }
    for (unsigned int at = 0u; at < ED25519_LIMBS; at += 1u)
    {
        out[at] = wide[at];
    }
    ed25519_carried(out);
    ed25519_carried(out);
}

static void ed25519_square(Ed25519Field out, const Ed25519Field in)
{
    ed25519_mul(out, in, in);
}

// the inverse, `in` raised to 2^255 - 21
static void ed25519_inverse(Ed25519Field out, const Ed25519Field in)
{
    Ed25519Field held;
    ed25519_set(held, in);
    for (int bit = 253; bit >= 0; bit -= 1)
    {
        ed25519_square(held, held);
        if ((bit != 2) && (bit != 4))
        {
            ed25519_mul(held, held, in);
        }
    }
    ed25519_set(out, held);
}

// `in` raised to 2^252 - 3, the step of a square root
static void ed25519_root_step(Ed25519Field out, const Ed25519Field in)
{
    Ed25519Field held;
    ed25519_set(held, in);
    for (int bit = 250; bit >= 0; bit -= 1)
    {
        ed25519_square(held, held);
        if (bit != 1)
        {
            ed25519_mul(held, held, in);
        }
    }
    ed25519_set(out, held);
}

// the point `one` made the sum of it and `other`, both in extended coordinates
static void ed25519_point_added(Ed25519Field one[4], Ed25519Field other[4])
{
    Ed25519Field a;
    Ed25519Field b;
    Ed25519Field c;
    Ed25519Field d;
    Ed25519Field t;
    Ed25519Field e;
    Ed25519Field f;
    Ed25519Field g;
    Ed25519Field h;
    ed25519_sub(a, one[1], one[0]);
    ed25519_sub(t, other[1], other[0]);
    ed25519_mul(a, a, t);
    ed25519_add(b, one[0], one[1]);
    ed25519_add(t, other[0], other[1]);
    ed25519_mul(b, b, t);
    ed25519_mul(c, one[3], other[3]);
    ed25519_mul(c, c, s_d2);
    ed25519_mul(d, one[2], other[2]);
    ed25519_add(d, d, d);
    ed25519_sub(e, b, a);
    ed25519_sub(f, d, c);
    ed25519_add(g, d, c);
    ed25519_add(h, b, a);
    ed25519_mul(one[0], e, f);
    ed25519_mul(one[1], h, g);
    ed25519_mul(one[2], g, f);
    ed25519_mul(one[3], e, h);
}

static void ed25519_points_swapped(Ed25519Field one[4], Ed25519Field other[4], long long swap)
{
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        ed25519_swapped(one[at], other[at], swap);
    }
}

// a point written as 32 bytes: its y, and the low bit of its x as the top bit
static void ed25519_point_packed(unsigned char out[32], Ed25519Field point[4])
{
    Ed25519Field inverse;
    Ed25519Field x;
    Ed25519Field y;
    ed25519_inverse(inverse, point[2]);
    ed25519_mul(x, point[0], inverse);
    ed25519_mul(y, point[1], inverse);
    ed25519_packed(out, y);
    // the parity is one bit, which the top of a byte holds
    out[31] ^= (unsigned char)(ed25519_parity(x) << 7);
}

// `point` made `scalar` times `base`, the bits taken from the top down with the same work for each
static void ed25519_scaled(Ed25519Field point[4], Ed25519Field base[4], const unsigned char scalar[32])
{
    ed25519_set(point[0], s_zero);
    ed25519_set(point[1], s_one);
    ed25519_set(point[2], s_one);
    ed25519_set(point[3], s_zero);
    for (int bit = 255; bit >= 0; bit -= 1)
    {
        const long long set = (scalar[bit / 8] >> (bit & 7)) & 1;
        ed25519_points_swapped(point, base, set);
        ed25519_point_added(base, point);
        ed25519_point_added(point, point);
        ed25519_points_swapped(point, base, set);
    }
}

// `point` made `scalar` times the base point
static void ed25519_scaled_base(Ed25519Field point[4], const unsigned char scalar[32])
{
    Ed25519Field base[4];
    ed25519_set(base[0], s_base_x);
    ed25519_set(base[1], s_base_y);
    ed25519_set(base[2], s_one);
    ed25519_mul(base[3], s_base_x, s_base_y);
    ed25519_scaled(point, base, scalar);
}

// 64 limbs of a scalar reduced mod L into 32 bytes
static void ed25519_reduced_wide(unsigned char out[32], long long wide[ED25519_WIDE])
{
    for (int at = 63; at >= 32; at -= 1)
    {
        long long carry = 0ll;
        int by = at - 32;
        for (; by < (at - 12); by += 1)
        {
            wide[by] += carry - (16ll * wide[at] * s_order[by - (at - 32)]);
            carry = (wide[by] + 128ll) >> 8;
            wide[by] -= carry * 256ll;
        }
        wide[by] += carry;
        wide[at] = 0ll;
    }
    long long carry = 0ll;
    for (unsigned int by = 0u; by < 32u; by += 1u)
    {
        wide[by] += carry - ((wide[31] >> 4) * s_order[by]);
        carry = wide[by] >> 8;
        wide[by] &= 255ll;
    }
    for (unsigned int by = 0u; by < 32u; by += 1u)
    {
        wide[by] -= carry * s_order[by];
    }
    for (unsigned int at = 0u; at < 32u; at += 1u)
    {
        wide[at + 1u] += wide[at] >> 8;
        // a reduced limb is one byte
        out[at] = (unsigned char)(wide[at] & 255ll);
    }
}

// 64 bytes, low byte first, reduced mod L into their first 32
static void ed25519_reduced(unsigned char bytes[64])
{
    long long wide[ED25519_WIDE];
    for (unsigned int at = 0u; at < ED25519_WIDE; at += 1u)
    {
        wide[at] = bytes[at];
    }
    ed25519_reduced_wide(bytes, wide);
}

// a seed's expanded secret, its scalar clamped as section 5.1.5 clamps it, and its prefix after it
static void ed25519_expanded(const unsigned char seed[ED25519_SEED_BYTES], unsigned char expanded[SHA512_BYTES])
{
    sha512(seed, ED25519_SEED_BYTES, expanded);
    expanded[0] &= 248u;
    expanded[31] &= 127u;
    expanded[31] |= 64u;
}

// the digest of three parts in turn reduced mod L into its first 32 bytes
static void ed25519_hashed(const void *first, size_t first_bytes, const void *second, size_t second_bytes,
                           const void *third, size_t third_bytes, unsigned char digest[SHA512_BYTES])
{
    Sha512 sha;
    sha512_begun(&sha);
    sha512_taken(&sha, first, first_bytes);
    sha512_taken(&sha, second, second_bytes);
    sha512_taken(&sha, third, third_bytes);
    sha512_ended(&sha, digest);
    ed25519_reduced(digest);
}

void ed25519_public_of(const unsigned char seed[ED25519_SEED_BYTES], unsigned char public_key[ED25519_PUBLIC_BYTES])
{
    unsigned char expanded[SHA512_BYTES];
    Ed25519Field point[4];
    ed25519_expanded(seed, expanded);
    ed25519_scaled_base(point, expanded);
    ed25519_point_packed(public_key, point);
}

void ed25519_signed(const unsigned char seed[ED25519_SEED_BYTES], const unsigned char public_key[ED25519_PUBLIC_BYTES],
                    const void *message, size_t bytes, unsigned char signature[ED25519_SIGNATURE_BYTES])
{
    unsigned char expanded[SHA512_BYTES];
    unsigned char nonce[SHA512_BYTES];
    unsigned char challenge[SHA512_BYTES];
    Ed25519Field point[4];
    ed25519_expanded(seed, expanded);
    ed25519_hashed(expanded + 32u, 32u, message, bytes, "", 0u, nonce);
    ed25519_scaled_base(point, nonce);
    ed25519_point_packed(signature, point);
    ed25519_hashed(signature, 32u, public_key, ED25519_PUBLIC_BYTES, message, bytes, challenge);
    long long wide[ED25519_WIDE] = {0};
    for (unsigned int at = 0u; at < 32u; at += 1u)
    {
        wide[at] = nonce[at];
    }
    for (unsigned int at = 0u; at < 32u; at += 1u)
    {
        for (unsigned int by = 0u; by < 32u; by += 1u)
        {
            wide[at + by] += (long long)challenge[at] * (long long)expanded[by];
        }
    }
    ed25519_reduced_wide(signature + 32u, wide);
}

// the point a public key names, negated, into `point`: 1, or 0 where the key names no point
static int ed25519_point_negated(Ed25519Field point[4], const unsigned char public_key[ED25519_PUBLIC_BYTES])
{
    Ed25519Field t;
    Ed25519Field check;
    Ed25519Field numerator;
    Ed25519Field denominator;
    Ed25519Field square;
    Ed25519Field fourth;
    Ed25519Field sixth;
    ed25519_set(point[2], s_one);
    ed25519_unpacked(point[1], public_key);
    ed25519_square(numerator, point[1]);
    ed25519_mul(denominator, numerator, s_d);
    ed25519_sub(numerator, numerator, point[2]);
    ed25519_add(denominator, point[2], denominator);
    ed25519_square(square, denominator);
    ed25519_square(fourth, square);
    ed25519_mul(sixth, fourth, square);
    ed25519_mul(t, sixth, numerator);
    ed25519_mul(t, t, denominator);
    ed25519_root_step(t, t);
    ed25519_mul(t, t, numerator);
    ed25519_mul(t, t, denominator);
    ed25519_mul(t, t, denominator);
    ed25519_mul(point[0], t, denominator);
    ed25519_square(check, point[0]);
    ed25519_mul(check, check, denominator);
    if (ed25519_differ(check, numerator))
    {
        ed25519_mul(point[0], point[0], s_root_minus_one);
    }
    ed25519_square(check, point[0]);
    ed25519_mul(check, check, denominator);
    if (ed25519_differ(check, numerator))
    {
        return 0;
    }
    if (ed25519_parity(point[0]) == (public_key[31] >> 7))
    {
        ed25519_sub(point[0], s_zero, point[0]);
    }
    ed25519_mul(point[3], point[0], point[1]);
    return 1;
}

// whether the 32 bytes of a scalar, low byte first, are below L
static int ed25519_below_order(const unsigned char scalar[32])
{
    for (int at = 31; at >= 0; at -= 1)
    {
        if (scalar[at] != s_order[at])
        {
            return scalar[at] < s_order[at];
        }
    }
    return 0;
}

int ed25519_holds(const unsigned char public_key[ED25519_PUBLIC_BYTES], const void *message, size_t bytes,
                  const unsigned char signature[ED25519_SIGNATURE_BYTES])
{
    Ed25519Field negated[4];
    Ed25519Field point[4];
    unsigned char challenge[SHA512_BYTES];
    unsigned char again[32];
    if (!ed25519_below_order(signature + 32u) || !ed25519_point_negated(negated, public_key))
    {
        return 0;
    }
    ed25519_hashed(signature, 32u, public_key, ED25519_PUBLIC_BYTES, message, bytes, challenge);
    ed25519_scaled(point, negated, challenge);
    Ed25519Field base[4];
    ed25519_scaled_base(base, signature + 32u);
    ed25519_point_added(point, base);
    ed25519_point_packed(again, point);
    return memcmp(again, signature, 32u) == 0;
}
