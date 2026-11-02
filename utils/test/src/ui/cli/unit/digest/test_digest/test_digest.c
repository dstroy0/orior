// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_digest.c: SHA-1, MD5, SHA-256, HMAC-SHA-256 and PBKDF2 against their standards' test vectors
#include "ui/cli/src/text/digest.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_hex[256];
static unsigned char s_million[1000000u];

// `size` bytes as lowercase hex in s_hex
static const char *hex_of(const unsigned char *bytes, unsigned long long size)
{
    static const char s_digits[] = "0123456789abcdef";
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        s_hex[at * 2ull] = s_digits[bytes[at] >> 4u];
        s_hex[(at * 2ull) + 1ull] = s_digits[bytes[at] & 0x0fu];
    }
    s_hex[size * 2ull] = '\0';
    return s_hex;
}

static void assert_sha1(const char *wanted, const char *text)
{
    unsigned char digest[20];
    EMBED_CALL(cli_digest_bytes_size.sha1_bytes, CliDigestBytesSizeCfg, .bytes = text, .size = CLI_UNITY_LENGTH(text), .digest = digest);
    TEST_ASSERT_EQUAL_STRING(wanted, hex_of(digest, sizeof(digest)));
}

static void assert_md5(const char *wanted, const char *text)
{
    VerbaLinea line = {s_hex, sizeof(s_hex), 0ull};
    EMBED_CALL(cli_digest_bytes.md5_text, CliDigestBytesCfg, .bytes = text, .size = CLI_UNITY_LENGTH(text), .text = &line);
    CLI_TEST_ASSERT_TEXT(wanted, s_hex, line.at);
}

static void assert_hmac(const char *wanted, const void *key, unsigned long long key_size, const char *message)
{
    VerbaLinea line = {s_hex, sizeof(s_hex), 0ull};
    EMBED_CALL(cli_digest_text.hmac_sha256_text, CliDigestTextCfg, .key = key, .key_size = key_size, .message = message, .message_size = CLI_UNITY_LENGTH(message), .text = &line);
    CLI_TEST_ASSERT_TEXT(wanted, s_hex, line.at);
}

static void assert_sha256(const char *wanted, const void *bytes, unsigned long long size)
{
    VerbaLinea line = {s_hex, sizeof(s_hex), 0ull};
    EMBED_CALL(cli_digest_bytes.sha256_text, CliDigestBytesCfg, .bytes = bytes, .size = size, .text = &line);
    CLI_TEST_ASSERT_TEXT(wanted, s_hex, line.at);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_sha1_of_the_empty_string(void)
{
    assert_sha1("da39a3ee5e6b4b0d3255bfef95601890afd80709", "");
}

void test_sha1_of_abc(void)
{
    assert_sha1("a9993e364706816aba3e25717850c26c9cd0d89d", "abc");
}

void test_sha1_of_two_blocks(void)
{
    assert_sha1("84983e441c3bd26ebaae4aa1f95129e5e54670f1", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
}

void test_md5_of_the_empty_string(void)
{
    assert_md5("d41d8cd98f00b204e9800998ecf8427e", "");
}

void test_md5_of_abc(void)
{
    assert_md5("900150983cd24fb0d6963f7d28e17f72", "abc");
}

void test_md5_of_message_digest(void)
{
    assert_md5("f96b697d7cb7938d525a2f31aaf161d0", "message digest");
}

void test_md5_of_eighty_digits(void)
{
    assert_md5("57edf4a22be3c955ac49da2e2107b67a", "12345678901234567890123456789012345678901234567890123456789012345678901234567890");
}

void test_pbkdf2_with_one_round(void)
{
    unsigned char derived[32];
    EMBED_CALL(cli_digest_password.pbkdf2_sha256, CliDigestPasswordCfg, .password = "passwd", .password_size = 6ull, .salt = "salt", .salt_size = 4ull, .rounds = 1u, .derived = derived);
    TEST_ASSERT_EQUAL_STRING("55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc", hex_of(derived, sizeof(derived)));
}

void test_pbkdf2_with_many_rounds(void)
{
    unsigned char derived[32];
    EMBED_CALL(cli_digest_password.pbkdf2_sha256, CliDigestPasswordCfg, .password = "Password", .password_size = 8ull, .salt = "NaCl", .salt_size = 4ull, .rounds = 80000u, .derived = derived);
    TEST_ASSERT_EQUAL_STRING("4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56", hex_of(derived, sizeof(derived)));
}

void test_rfc4231_case_1(void)
{
    unsigned char key[20];
    memor.set(&(MemoriaCfg){.dst = key, .val = 0x0b, .bytes = sizeof(key)});
    assert_hmac("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", key, sizeof(key), "Hi There");
}

void test_rfc4231_case_2(void)
{
    assert_hmac("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "Jefe", 4ull, "what do ya want for nothing?");
}

void test_rfc4231_key_larger_than_a_block(void)
{
    unsigned char key[131];
    memor.set(&(MemoriaCfg){.dst = key, .val = 0xaa, .bytes = sizeof(key)});
    assert_hmac("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54", key, sizeof(key), "Test Using Larger Than Block-Size Key - Hash Key First");
}

void test_sha256_of_the_empty_string(void)
{
    assert_sha256("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "", 0ull);
}

void test_sha256_of_abc(void)
{
    assert_sha256("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc", 3ull);
}

void test_sha256_of_two_blocks(void)
{
    assert_sha256("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56ull);
}

void test_sha256_of_a_million_a(void)
{
    memor.set(&(MemoriaCfg){.dst = s_million, .val = 'a', .bytes = sizeof(s_million)});
    assert_sha256("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", s_million, sizeof(s_million));
}
