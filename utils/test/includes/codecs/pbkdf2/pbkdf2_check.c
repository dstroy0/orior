// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file pbkdf2_check.c
 * @brief Holds the HMAC-SHA-512 and PBKDF2 of includes/codecs/pbkdf2 to published vectors from outside the tree.
 *
 * @note Every expected value comes from a file that pbkdf2_check.sh fetches and checks against MANIFEST.tsv. No
 *       expected value is in this source. The four sources:
 *         RFC 4231 section 4, the HMAC-SHA-512 rows of test cases 1 to 7.
 *         Wycheproof hmac_sha512_test.json, valid and invalid tags at several tag sizes.
 *         Wycheproof pbkdf2_hmacsha512_test.json.
 *         The BIP-39 vectors of trezor/python-mnemonic, english, passphrase TREZOR, 2048 iterations.
 * @note BIP-39 normalizes the mnemonic and the passphrase to NFKD. For ASCII text, NFKD is the identity. Thus the
 *       english vectors need no normalization, and the check reads only those.
 * @note Each source must yield at least one case, or the check fails. A parser that reads nothing would
 *       otherwise pass.
 *
 * Usage: pbkdf2_check RFC4231 HMAC_JSON PBKDF2_JSON BIP39_JSON
 */
#include "../../../../../includes/codecs/pbkdf2/pbkdf2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Bytes of the largest value that one line of a source carries, after hex decode. */
#define LONGEST_VALUE_BYTES 8192u

/** @brief Bytes of a BIP-39 seed. */
#define BIP39_SEED_BYTES 64u

/** @brief Iterations of PBKDF2 in BIP-39. */
#define BIP39_ITERATIONS 2048u

/** @brief Count of the RFC 4231 test cases. */
#define RFC4231_CASES 7u

/** @brief Counts of checks and failures across all sources. */
typedef struct
{
    unsigned long run;
    unsigned long failed;
} Tally;

/**
 * @brief Reads a whole file into a buffer, with a zero at the end.
 *
 * @param[in] path The file [BORROWS].
 * @return         The text, or NULL where the read fails [OWNS].
 */
static char *file_read(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }
    const long size = ftell(file);
    if ((size < 0) || (fseek(file, 0, SEEK_SET) != 0))
    {
        fclose(file);
        return NULL;
    }
    // A long that ftell returns non-negative fits a size_t on every target of this tree.
    char *const text = (char *)malloc((size_t)size + 1u);
    if (text == NULL)
    {
        fclose(file);
        return NULL;
    }
    const size_t got = fread(text, 1u, (size_t)size, file);
    fclose(file);
    text[got] = '\0';
    return text;
}

/**
 * @brief Gives the value of one hex digit.
 *
 * @param[in] digit The character.
 * @return          The value 0 to 15, or -1 where the character is not a hex digit.
 */
static int hex_digit(char digit)
{
    if ((digit >= '0') && (digit <= '9'))
    {
        return digit - '0';
    }
    if ((digit >= 'a') && (digit <= 'f'))
    {
        return (digit - 'a') + 10;
    }
    if ((digit >= 'A') && (digit <= 'F'))
    {
        return (digit - 'A') + 10;
    }
    return -1;
}

/**
 * @brief Appends the bytes of a run of hex digits to a buffer.
 *
 * @param[in]     hex    The digits [BORROWS].
 * @param[in]     digits How many digits, an even count.
 * @param[in,out] out    The buffer [BORROWS].
 * @param[in,out] length Bytes in the buffer [BORROWS].
 * @return               1 on success, 0 where a digit is bad, the count is odd, or the buffer is full.
 */
static int hex_append(const char *hex, size_t digits, unsigned char *out, size_t *length)
{
    if ((digits % 2u) != 0u)
    {
        return 0;
    }
    for (size_t at = 0u; at < digits; at += 2u)
    {
        const int high = hex_digit(hex[at]);
        const int low = hex_digit(hex[at + 1u]);
        if ((high < 0) || (low < 0) || (*length >= LONGEST_VALUE_BYTES))
        {
            return 0;
        }
        // Two hex digits make one value from 0 to 255, which a byte represents.
        out[*length] = (unsigned char)((high << 4) | low);
        *length += 1u;
    }
    return 1;
}

/**
 * @brief Records one check and prints a failure.
 *
 * @param[in,out] tally   The counts [BORROWS].
 * @param[in]     source  The name of the source [BORROWS].
 * @param[in]     label   The name of the case [BORROWS].
 * @param[in]     passed  Nonzero where the case passed.
 */
static void record(Tally *tally, const char *source, const char *label, int passed)
{
    tally->run += 1u;
    if (!passed)
    {
        tally->failed += 1u;
        printf("  [FAIL] %s %s\n", source, label);
    }
}

/**
 * @brief Finds the quoted string value of a named field on one JSON line.
 *
 * @param[in]  line   The line [BORROWS].
 * @param[in]  name   The field name, without quotes [BORROWS].
 * @param[out] start  Receives the first character of the value [BORROWS].
 * @param[out] length Receives the length of the value [BORROWS].
 * @return            1 where the line carries the field with a string value, else 0.
 * @note The JSON of the vector files puts one field on each line. The values here are hex or plain ASCII, and
 *       carry no escaped quote.
 */
static int json_string_field(const char *line, const char *name, const char **start, size_t *length)
{
    char key[64];
    snprintf(key, sizeof(key), "\"%s\"", name);
    const char *at = strstr(line, key);
    if (at == NULL)
    {
        return 0;
    }
    at = strchr(at + strlen(key), ':');
    if (at == NULL)
    {
        return 0;
    }
    at = strchr(at, '"');
    if (at == NULL)
    {
        return 0;
    }
    const char *const end = strchr(at + 1, '"');
    if (end == NULL)
    {
        return 0;
    }
    *start = at + 1;
    // The pointer difference is non-negative, because end follows at.
    *length = (size_t)(end - (at + 1));
    return 1;
}

/**
 * @brief Finds the number value of a named field on one JSON line.
 *
 * @param[in]  line  The line [BORROWS].
 * @param[in]  name  The field name, without quotes [BORROWS].
 * @param[out] value Receives the value [BORROWS].
 * @return           1 where the line carries the field, else 0.
 */
static int json_number_field(const char *line, const char *name, unsigned long *value)
{
    char key[64];
    snprintf(key, sizeof(key), "\"%s\"", name);
    const char *at = strstr(line, key);
    if (at == NULL)
    {
        return 0;
    }
    at = strchr(at + strlen(key), ':');
    if (at == NULL)
    {
        return 0;
    }
    *value = strtoul(at + 1, NULL, 10);
    return 1;
}

/**
 * @brief Gives the next line of a text, and moves the cursor past it.
 *
 * @param[in,out] cursor The position in the text, moved past the line [BORROWS].
 * @return               The line with its end replaced by a zero, or NULL at the end of the text.
 */
static char *next_line(char **cursor)
{
    char *const line = *cursor;
    if (*line == '\0')
    {
        return NULL;
    }
    char *end = strchr(line, '\n');
    if (end == NULL)
    {
        *cursor = line + strlen(line);
        return line;
    }
    *end = '\0';
    if ((end > line) && (end[-1] == '\r'))
    {
        end[-1] = '\0';
    }
    *cursor = end + 1;
    return line;
}

/** @brief The field of an RFC 4231 test case that a line feeds. */
typedef enum
{
    RFC_FIELD_NONE,
    RFC_FIELD_KEY,
    RFC_FIELD_DATA,
    RFC_FIELD_TAG
} RfcField;

/** @brief The fields of one RFC 4231 test case under assembly. */
typedef struct
{
    unsigned char key[LONGEST_VALUE_BYTES];
    size_t key_bytes;
    unsigned char data[LONGEST_VALUE_BYTES];
    size_t data_bytes;
    unsigned char tag[LONGEST_VALUE_BYTES];
    size_t tag_bytes;
} RfcCase;

/**
 * @brief Checks one assembled RFC 4231 case.
 *
 * @param[in,out] tally  The counts [BORROWS].
 * @param[in]     test   The case [BORROWS].
 * @param[in]     number The number of the case.
 * @note Test case 5 prints its tag truncated to 128 bits. The check compares the leading bytes that the RFC prints.
 */
static void rfc4231_case_check(Tally *tally, const RfcCase *test, unsigned number)
{
    unsigned char tag[HMAC_SHA512_BYTES];
    hmac_sha512(test->key, test->key_bytes, test->data, test->data_bytes, tag);
    char label[32];
    snprintf(label, sizeof(label), "test case %u", number);
    const int passed = (test->tag_bytes > 0u) && (test->tag_bytes <= HMAC_SHA512_BYTES) &&
                       (memcmp(tag, test->tag, test->tag_bytes) == 0);
    record(tally, "RFC 4231", label, passed);
}

/**
 * @brief Reads the HMAC-SHA-512 rows of RFC 4231 section 4 and checks each case.
 *
 * @param[in,out] tally The counts [BORROWS].
 * @param[in]     text  The text of the RFC [BORROWS].
 * @return              How many cases the check found.
 * @note A field line starts with Key, Data or HMAC-SHA-512 after three spaces. Test case 3 prints Key without its
 *       equals sign, and the reader accepts that. A continuation line starts with eighteen spaces. A word in
 *       parentheses after the hex is a comment of the RFC, and the reader skips it.
 */
static unsigned rfc4231_check(Tally *tally, char *text)
{
    static RfcCase test;
    unsigned found = 0u;
    unsigned number = 0u;
    RfcField field = RFC_FIELD_NONE;
    char *cursor = text;
    char *line;
    while ((line = next_line(&cursor)) != NULL)
    {
        unsigned case_number = 0u;
        if (sscanf(line, "4.%*u.  Test Case %u", &case_number) == 1)
        {
            if (number != 0u)
            {
                rfc4231_case_check(tally, &test, number);
                found += 1u;
            }
            memset(&test, 0, sizeof(test));
            number = case_number;
            field = RFC_FIELD_NONE;
            continue;
        }
        if (number == 0u)
        {
            continue;
        }
        if (strncmp(line, "5.  Security Considerations", 27u) == 0)
        {
            break;
        }

        const char *hex = NULL;
        if (strncmp(line, "   Key", 6u) == 0)
        {
            field = RFC_FIELD_KEY;
            hex = line + 6;
        }
        else if (strncmp(line, "   Data", 7u) == 0)
        {
            field = RFC_FIELD_DATA;
            hex = line + 7;
        }
        else if (strncmp(line, "   HMAC-SHA-512", 15u) == 0)
        {
            field = RFC_FIELD_TAG;
            hex = line + 15;
        }
        else if (strncmp(line, "   HMAC-SHA-", 12u) == 0)
        {
            field = RFC_FIELD_NONE;
        }
        else if ((strncmp(line, "                  ", 18u) == 0) && (hex_digit(line[18]) >= 0))
        {
            hex = line + 18;
        }
        else if (line[0] != '\0')
        {
            field = RFC_FIELD_NONE;
        }
        if ((hex == NULL) || (field == RFC_FIELD_NONE))
        {
            continue;
        }

        while ((*hex == ' ') || (*hex == '='))
        {
            hex += 1;
        }
        size_t digits = 0u;
        while (hex_digit(hex[digits]) >= 0)
        {
            digits += 1u;
        }
        unsigned char *const target = (field == RFC_FIELD_KEY)    ? test.key
                                      : (field == RFC_FIELD_DATA) ? test.data
                                                                  : test.tag;
        size_t *const length = (field == RFC_FIELD_KEY)    ? &test.key_bytes
                               : (field == RFC_FIELD_DATA) ? &test.data_bytes
                                                           : &test.tag_bytes;
        if (!hex_append(hex, digits, target, length))
        {
            record(tally, "RFC 4231", "a field that does not decode", 0);
        }
    }
    if (number != 0u)
    {
        rfc4231_case_check(tally, &test, number);
        found += 1u;
    }
    return found;
}

/**
 * @brief Reads the Wycheproof HMAC-SHA-512 vectors and checks each case.
 *
 * @param[in,out] tally The counts [BORROWS].
 * @param[in]     text  The JSON text [BORROWS].
 * @return              How many cases the check found.
 * @note A valid case must match its tag, after truncation of the computed tag to the tagSize of its group. An
 *       invalid case must not match.
 */
static unsigned wycheproof_hmac_check(Tally *tally, char *text)
{
    static unsigned char key[LONGEST_VALUE_BYTES];
    static unsigned char message[LONGEST_VALUE_BYTES];
    static unsigned char expected[LONGEST_VALUE_BYTES];
    size_t key_bytes = 0u;
    size_t message_bytes = 0u;
    size_t expected_bytes = 0u;
    unsigned long tag_bits = 0u;
    unsigned long case_id = 0u;
    unsigned found = 0u;
    int decoded = 1;

    char *cursor = text;
    char *line;
    while ((line = next_line(&cursor)) != NULL)
    {
        const char *value = NULL;
        size_t length = 0u;
        unsigned long number = 0u;
        if (json_number_field(line, "tagSize", &number))
        {
            tag_bits = number;
        }
        else if (json_number_field(line, "tcId", &number))
        {
            case_id = number;
            key_bytes = 0u;
            message_bytes = 0u;
            expected_bytes = 0u;
            decoded = 1;
        }
        else if (json_string_field(line, "key", &value, &length))
        {
            decoded = hex_append(value, length, key, &key_bytes) && decoded;
        }
        else if (json_string_field(line, "msg", &value, &length))
        {
            decoded = hex_append(value, length, message, &message_bytes) && decoded;
        }
        else if (json_string_field(line, "tag", &value, &length))
        {
            decoded = hex_append(value, length, expected, &expected_bytes) && decoded;
        }
        else if (json_string_field(line, "result", &value, &length))
        {
            unsigned char tag[HMAC_SHA512_BYTES];
            hmac_sha512(key, key_bytes, message, message_bytes, tag);
            const size_t tag_bytes = tag_bits / 8u;
            const int same = (tag_bytes <= HMAC_SHA512_BYTES) && (expected_bytes == tag_bytes) &&
                             (memcmp(tag, expected, tag_bytes) == 0);
            const int valid = (length == 5u) && (strncmp(value, "valid", 5u) == 0);
            char label[48];
            snprintf(label, sizeof(label), "tcId %lu (%s)", case_id, valid ? "valid" : "invalid");
            record(tally, "Wycheproof HMAC-SHA512", label, decoded && (valid ? same : !same));
            found += 1u;
        }
    }
    return found;
}

/**
 * @brief Reads the Wycheproof PBKDF2-HMAC-SHA512 vectors and checks each case.
 *
 * @param[in,out] tally The counts [BORROWS].
 * @param[in]     text  The JSON text [BORROWS].
 * @return              How many cases the check found.
 */
static unsigned wycheproof_pbkdf2_check(Tally *tally, char *text)
{
    static unsigned char password[LONGEST_VALUE_BYTES];
    static unsigned char salt[LONGEST_VALUE_BYTES];
    static unsigned char expected[LONGEST_VALUE_BYTES];
    static unsigned char derived[LONGEST_VALUE_BYTES];
    size_t password_bytes = 0u;
    size_t salt_bytes = 0u;
    size_t expected_bytes = 0u;
    unsigned long iterations = 0u;
    unsigned long derived_bytes = 0u;
    unsigned long case_id = 0u;
    unsigned found = 0u;
    int decoded = 1;

    char *cursor = text;
    char *line;
    while ((line = next_line(&cursor)) != NULL)
    {
        const char *value = NULL;
        size_t length = 0u;
        unsigned long number = 0u;
        if (json_number_field(line, "tcId", &number))
        {
            case_id = number;
            password_bytes = 0u;
            salt_bytes = 0u;
            expected_bytes = 0u;
            decoded = 1;
        }
        else if (json_number_field(line, "iterationCount", &number))
        {
            iterations = number;
        }
        else if (json_number_field(line, "dkLen", &number))
        {
            derived_bytes = number;
        }
        else if (json_string_field(line, "password", &value, &length))
        {
            decoded = hex_append(value, length, password, &password_bytes) && decoded;
        }
        else if (json_string_field(line, "salt", &value, &length))
        {
            decoded = hex_append(value, length, salt, &salt_bytes) && decoded;
        }
        else if (json_string_field(line, "dk", &value, &length))
        {
            decoded = hex_append(value, length, expected, &expected_bytes) && decoded;
        }
        else if (json_string_field(line, "result", &value, &length))
        {
            const int valid = (length == 5u) && (strncmp(value, "valid", 5u) == 0);
            const int fits = derived_bytes <= LONGEST_VALUE_BYTES;
            const int made = fits && pbkdf2_hmac_sha512(password, password_bytes, salt, salt_bytes, iterations, derived,
                                                        derived_bytes);
            const int same =
                made && (expected_bytes == derived_bytes) && (memcmp(derived, expected, expected_bytes) == 0);
            char label[48];
            snprintf(label, sizeof(label), "tcId %lu (%s)", case_id, valid ? "valid" : "invalid");
            record(tally, "Wycheproof PBKDF2-HMAC-SHA512", label, decoded && (valid ? same : !same));
            found += 1u;
        }
    }
    return found;
}

/**
 * @brief Reads the english BIP-39 vectors of trezor/python-mnemonic and checks each seed.
 *
 * @param[in,out] tally The counts [BORROWS].
 * @param[in]     text  The JSON text [BORROWS].
 * @return              How many cases the check found.
 * @note Each vector is four strings: entropy, mnemonic, seed and extended key. The seed is PBKDF2-HMAC-SHA512 of the
 *       mnemonic, with the salt "mnemonic" followed by the passphrase TREZOR, at 2048 iterations, as BIP-39 states.
 */
static unsigned trezor_bip39_check(Tally *tally, char *text)
{
    static const char salt[] = "mnemonicTREZOR";
    static unsigned char expected[LONGEST_VALUE_BYTES];
    char mnemonic[1024];
    int inside = 0;
    unsigned position = 0u;
    unsigned found = 0u;

    char *cursor = text;
    char *line;
    while ((line = next_line(&cursor)) != NULL)
    {
        if (strstr(line, "\"english\"") != NULL)
        {
            inside = 1;
            position = 0u;
            continue;
        }
        if (!inside)
        {
            continue;
        }
        // The next language key ends the english vectors.
        if ((strchr(line, ':') != NULL) && (strstr(line, "\"english\"") == NULL))
        {
            break;
        }
        const char *const open = strchr(line, '"');
        if (open == NULL)
        {
            continue;
        }
        const char *const close = strchr(open + 1, '"');
        if (close == NULL)
        {
            continue;
        }
        // The pointer difference is non-negative, because close follows open.
        const size_t length = (size_t)(close - (open + 1));

        if (position == 1u)
        {
            const size_t kept = (length < (sizeof(mnemonic) - 1u)) ? length : (sizeof(mnemonic) - 1u);
            memcpy(mnemonic, open + 1, kept);
            mnemonic[kept] = '\0';
        }
        else if (position == 2u)
        {
            size_t expected_bytes = 0u;
            const int decoded = hex_append(open + 1, length, expected, &expected_bytes);
            unsigned char seed[BIP39_SEED_BYTES];
            const int made = pbkdf2_hmac_sha512(mnemonic, strlen(mnemonic), salt, strlen(salt), BIP39_ITERATIONS, seed,
                                                sizeof(seed));
            const int same =
                decoded && made && (expected_bytes == sizeof(seed)) && (memcmp(seed, expected, sizeof(seed)) == 0);
            char label[48];
            snprintf(label, sizeof(label), "english vector %u", found + 1u);
            record(tally, "Trezor BIP-39", label, same);
            found += 1u;
        }
        position = (position + 1u) % 4u;
    }
    return found;
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 5)
    {
        printf("usage: pbkdf2_check RFC4231 HMAC_JSON PBKDF2_JSON BIP39_JSON\n");
        return 2;
    }

    char *texts[4];
    for (int which = 0; which < 4; which += 1)
    {
        texts[which] = file_read(arguments[which + 1]);
        if (texts[which] == NULL)
        {
            printf("  cannot read %s\n", arguments[which + 1]);
            return 1;
        }
    }

    Tally tally = {0u, 0u};
    const unsigned rfc = rfc4231_check(&tally, texts[0]);
    const unsigned hmac = wycheproof_hmac_check(&tally, texts[1]);
    const unsigned pbkdf2 = wycheproof_pbkdf2_check(&tally, texts[2]);
    const unsigned bip39 = trezor_bip39_check(&tally, texts[3]);

    printf("  RFC 4231 HMAC-SHA-512:            %u cases\n", rfc);
    printf("  Wycheproof HMAC-SHA512:           %u cases\n", hmac);
    printf("  Wycheproof PBKDF2-HMAC-SHA512:    %u cases\n", pbkdf2);
    printf("  Trezor BIP-39, english:           %u cases\n", bip39);

    // A source that yields no case means that its reader failed, and not that the code passed.
    const int complete = (rfc == RFC4231_CASES) && (hmac > 0u) && (pbkdf2 > 0u) && (bip39 > 0u);
    if (!complete)
    {
        printf("  FAIL: a source yielded too few cases. RFC 4231 must yield %u.\n", RFC4231_CASES);
    }
    printf("  pbkdf2 check: %lu checks, %lu failed\n", tally.run, tally.failed);

    for (int which = 0; which < 4; which += 1)
    {
        free(texts[which]);
    }
    return (complete && (tally.failed == 0u)) ? 0 : 1;
}
