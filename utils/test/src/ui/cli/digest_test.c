// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// digest_test.c: the digests' check: SHA-1, MD5 and PBKDF2 give their standards' examples, RFC 4231's examples give
// their signatures, and the standard's examples give their SHA-256 digests
#include "../../../../../ui/cli/src/text/digest.h"
#include "cli_check.h"

static char s_hex[256];
static unsigned char s_million[1000000u];

// `size` bytes as lowercase hex into s_hex
static const char *digest_hex(const unsigned char *bytes, unsigned long long size)
{
    static const char s_digits[] = "0123456789abcdef";
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        s_hex[at * 2ull] = s_digits[bytes[at] >> 4u];
        s_hex[at * 2ull + 1ull] = s_digits[bytes[at] & 0x0fu];
    }
    s_hex[size * 2ull] = '\0';
    return s_hex;
}

// the SHA-1 of a text checked against `wanted`
static void digest_sha1(const char *text, const char *wanted)
{
    unsigned char digest[20];
    cli_sha1_bytes(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), digest);
    cli_check_text(digest_hex(digest, sizeof(digest)), 40ull, wanted, "SHA-1 gives its standard's example");
}

// the MD5 of a text checked against `wanted`
static void digest_md5(const char *text, const char *wanted)
{
    CliLine line = {s_hex, sizeof(s_hex), 0ull};
    cli_md5_text(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &line);
    cli_check_text(s_hex, line.at, wanted, "MD5 gives its standard's example");
}

// the HMAC-SHA-256 of `message` with `key_size` bytes of `key` checked against `wanted`
static void digest_hmac(const void *key, unsigned long long key_size, const char *message, const char *wanted)
{
    CliLine line = {s_hex, sizeof(s_hex), 0ull};
    cli_hmac_sha256_text(key, key_size, message, cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}),
                         &line);
    cli_check_text(s_hex, line.at, wanted, "RFC 4231's example gives its signature");
}

// the SHA-256 of `size` bytes checked against `wanted`
static void digest_sha256(const void *bytes, unsigned long long size, const char *wanted)
{
    CliLine line = {s_hex, sizeof(s_hex), 0ull};
    cli_sha256_text(bytes, size, &line);
    cli_check_text(s_hex, line.at, wanted, "the standard's example gives its digest");
}

int main(void)
{
    digest_sha1("", "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    digest_sha1("abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
    digest_sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    digest_md5("", "d41d8cd98f00b204e9800998ecf8427e");
    digest_md5("abc", "900150983cd24fb0d6963f7d28e17f72");
    digest_md5("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
    digest_md5("12345678901234567890123456789012345678901234567890123456789012345678901234567890",
               "57edf4a22be3c955ac49da2e2107b67a");
    unsigned char derived[32];
    cli_pbkdf2_sha256("passwd", 6ull, "salt", 4ull, 1u, derived);
    cli_check_text(digest_hex(derived, sizeof(derived)), 64ull,
                   "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc", "PBKDF2 gives its example");
    cli_pbkdf2_sha256("Password", 8ull, "NaCl", 4ull, 80000u, derived);
    cli_check_text(digest_hex(derived, sizeof(derived)), 64ull,
                   "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56", "and in many rounds");
    unsigned char key[131];
    memor.set(&(MemoriaCfg){.dst = key, .val = 0x0b, .bytes = 20u});
    digest_hmac(key, 20ull, "Hi There", "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    digest_hmac("Jefe", 4ull, "what do ya want for nothing?",
                "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    memor.set(&(MemoriaCfg){.dst = key, .val = 0xaa, .bytes = sizeof(key)});
    digest_hmac(key, sizeof(key), "Test Using Larger Than Block-Size Key - Hash Key First",
                "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    digest_sha256("", 0ull, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    digest_sha256("abc", 3ull, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    digest_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56ull,
                  "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    memor.set(&(MemoriaCfg){.dst = s_million, .val = 'a', .bytes = sizeof(s_million)});
    digest_sha256(s_million, sizeof(s_million), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    return cli_checked();
}
