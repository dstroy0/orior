// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file isotope_table.c
 * @brief Writes isotope_table.h beside it from NIST's table of every isotope.
 *
 * Usage: isotope_table NIST_TEXT OUTPUT_HEADER
 *
 * NIST_TEXT is the "all elements, all isotopes" plain-text output of NIST's Atomic Weights and
 * Isotopic Compositions (physics.nist.gov/cgi-bin/Compositions/stand_alone.pl, ascii2, isotype=all).
 * Every isotope NIST lists is written, with no element left out. A relative atomic mass enters as
 * an exact integer count of femtodaltons, 10^-15 Da. Its parenthesized uncertainty enters in the
 * same unit, and a '#' after it, NIST's mark for an estimated value, is kept as a flag. An isotopic
 * composition enters as an exact count of 10^-10 parts with its parenthesized uncertainty at the same
 * unit, or -1 for both where NIST gives none. A standard atomic weight written as an interval
 * [low,high], IUPAC's range over normal terrestrial materials, enters as its two ends in femtodaltons;
 * any other form leaves both -1. A value written past those places is refused and the table is not
 * written: nothing is rounded.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Decimal places a mass is held at: femtodaltons. */
#define TABLE_MASS_PLACES 15u

/** Decimal places a composition is held at. */
#define TABLE_COMPOSITION_PLACES 10u

/** Longest line of NIST's text this reads. */
#define TABLE_LINE_BYTES 512u

/** Most isotopes the table holds; NIST lists about 3,400. */
#define TABLE_ISOTOPES_MOST 8192u

typedef struct
{
    unsigned int atomic_number;
    char symbol[8];
    unsigned int mass_number;
    long long mass;
    long long mass_uncertainty;
    int mass_estimated;
    long long composition;
    long long composition_uncertainty;
    long long weight_low;
    long long weight_high;
} TableIsotope;

static TableIsotope s_isotope[TABLE_ISOTOPES_MOST];

/**
 * Reads a decimal "digits.digits(uncertainty)" or "digits.digits" as an exact integer at places.
 *
 * @return 1 where it was read exactly, 0 where the text is malformed or holds more places
 */
static int table_decimal_read(const char *text, unsigned int places, long long *value, long long *uncertainty,
                              int *estimated)
{
    long long whole = 0ll;
    unsigned int fraction_places = 0u;
    int seen_point = 0;
    int seen_digit = 0;
    const char *at = text;
    for (; (*at >= '0' && *at <= '9') || *at == '.'; at += 1)
    {
        if (*at == '.')
        {
            if (seen_point)
            {
                return 0;
            }
            seen_point = 1;
            continue;
        }
        if (seen_point)
        {
            fraction_places += 1u;
        }
        if (whole > (9000000000000000000ll - 9ll) / 10ll)
        {
            return 0;
        }
        whole = (whole * 10ll) + (long long)(*at - '0');
        seen_digit = 1;
    }
    if (!seen_digit || fraction_places > places)
    {
        return 0;
    }
    long long unit_scale = 1ll;
    for (unsigned int place = fraction_places; place < places; place += 1u)
    {
        if (whole > 900000000000000000ll)
        {
            return 0;
        }
        whole *= 10ll;
        unit_scale *= 10ll;
    }
    *value = whole;
    long long spread = 0ll;
    int flagged = 0;
    if (*at == '(')
    {
        at += 1;
        for (; *at >= '0' && *at <= '9'; at += 1)
        {
            spread = (spread * 10ll) + (long long)(*at - '0');
        }
        if (*at == '#')
        {
            flagged = 1;
            at += 1;
        }
        if (*at != ')')
        {
            return 0;
        }
        at += 1;
    }
    if (*at == '#')
    {
        flagged = 1;
        at += 1;
    }
    while (*at == ' ' || *at == '\r' || *at == '\n' || *at == '\t')
    {
        at += 1;
    }
    if (*at != '\0')
    {
        return 0;
    }
    // The uncertainty counts units of the last written place, which is unit_scale units at places.
    if (uncertainty != NULL)
    {
        *uncertainty = spread * unit_scale;
    }
    if (estimated != NULL)
    {
        *estimated = flagged;
    }
    return 1;
}

/**
 * Reads a standard atomic weight "[low,high]" as its two ends at femtodaltons. A single bracketed
 * mass number such as "[227]", or a "value(uncertainty)", is not an interval and leaves both -1.
 *
 * @return 1 where the text was read, as an interval or as no interval; 0 where an interval's end is
 *         malformed or holds more places
 */
static int table_interval_read(const char *text, long long *low, long long *high)
{
    *low = -1ll;
    *high = -1ll;
    if (*text != '[')
    {
        return 1;
    }
    const char *const comma = strchr(text, ',');
    const char *const close = strchr(text, ']');
    if (comma == NULL || close == NULL || close < comma)
    {
        return 1;
    }
    char end[64];
    const size_t low_length = (size_t)(comma - (text + 1));
    const size_t high_length = (size_t)(close - (comma + 1));
    if (low_length == 0u || low_length >= sizeof(end) || high_length == 0u || high_length >= sizeof(end))
    {
        return 0;
    }
    memcpy(end, text + 1, low_length);
    end[low_length] = '\0';
    if (!table_decimal_read(end, TABLE_MASS_PLACES, low, NULL, NULL))
    {
        return 0;
    }
    memcpy(end, comma + 1, high_length);
    end[high_length] = '\0';
    return table_decimal_read(end, TABLE_MASS_PLACES, high, NULL, NULL);
}

static const char *table_field(const char *line, const char *name)
{
    const size_t length = strlen(name);
    if (strncmp(line, name, length) != 0)
    {
        return NULL;
    }
    return line + length;
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 3)
    {
        fprintf(stderr, "usage: %s NIST_TEXT OUTPUT_HEADER\n", arguments[0]);
        return 2;
    }
    FILE *const input = fopen(arguments[1], "rb");
    if (input == NULL)
    {
        fprintf(stderr, "cannot open %s\n", arguments[1]);
        return 1;
    }
    char line[TABLE_LINE_BYTES];
    unsigned int count = 0u;
    unsigned int line_number = 0u;
    TableIsotope current;
    memset(&current, 0, sizeof(current));
    int refused = 0;
    while (fgets(line, (int)sizeof(line), input) != NULL)
    {
        line_number += 1u;
        const char *value = NULL;
        if ((value = table_field(line, "Atomic Number = ")) != NULL)
        {
            memset(&current, 0, sizeof(current));
            current.atomic_number = (unsigned int)strtoul(value, NULL, 10);
            current.composition = -1ll;
            current.composition_uncertainty = -1ll;
            current.weight_low = -1ll;
            current.weight_high = -1ll;
        }
        else if ((value = table_field(line, "Atomic Symbol = ")) != NULL)
        {
            size_t length = strcspn(value, " \r\n");
            if (length == 0u || length >= sizeof(current.symbol))
            {
                fprintf(stderr, "line %u: symbol refused\n", line_number);
                refused = 1;
                break;
            }
            memcpy(current.symbol, value, length);
            current.symbol[length] = '\0';
        }
        else if ((value = table_field(line, "Mass Number = ")) != NULL)
        {
            current.mass_number = (unsigned int)strtoul(value, NULL, 10);
        }
        else if ((value = table_field(line, "Relative Atomic Mass = ")) != NULL)
        {
            if (!table_decimal_read(value, TABLE_MASS_PLACES, &current.mass, &current.mass_uncertainty,
                                    &current.mass_estimated))
            {
                fprintf(stderr, "line %u: mass refused: %s", line_number, line);
                refused = 1;
                break;
            }
        }
        else if ((value = table_field(line, "Isotopic Composition = ")) != NULL)
        {
            const char *start = value;
            while (*start == ' ')
            {
                start += 1;
            }
            if (*start != '\r' && *start != '\n' && *start != '\0')
            {
                if (!table_decimal_read(start, TABLE_COMPOSITION_PLACES, &current.composition,
                                        &current.composition_uncertainty, NULL))
                {
                    fprintf(stderr, "line %u: composition refused: %s", line_number, line);
                    refused = 1;
                    break;
                }
            }
        }
        else if ((value = table_field(line, "Standard Atomic Weight = ")) != NULL)
        {
            if (count >= TABLE_ISOTOPES_MOST || current.symbol[0] == '\0' || current.mass == 0ll)
            {
                fprintf(stderr, "line %u: isotope incomplete or table full\n", line_number);
                refused = 1;
                break;
            }
            if (!table_interval_read(value, &current.weight_low, &current.weight_high))
            {
                fprintf(stderr, "line %u: atomic weight refused: %s", line_number, line);
                refused = 1;
                break;
            }
            s_isotope[count] = current;
            count += 1u;
        }
    }
    fclose(input);
    if (refused || count == 0u)
    {
        return 1;
    }
    FILE *const output = fopen(arguments[2], "wb");
    if (output == NULL)
    {
        fprintf(stderr, "cannot write %s\n", arguments[2]);
        return 1;
    }
    fprintf(output, "// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational\n");
    fprintf(output, "/**\n * @file isotope_table.h\n * @brief Every isotope NIST lists, generated by tools/isotope_table.c. Do not edit.\n *\n");
    fprintf(output, " * Source: NIST, Atomic Weights and Isotopic Compositions for All Elements (ascii2, isotype=all),\n");
    fprintf(output, " * data/nist_isotopes.txt. Masses in femtodaltons, 10^-15 Da; compositions and their uncertainties\n");
    fprintf(output, " * in 10^-10 parts, -1 where NIST gives none; the standard atomic weight interval's ends in\n");
    fprintf(output, " * femtodaltons, -1 where NIST writes no interval. Every value is the decimal NIST wrote, held exactly.\n */\n");
    fprintf(output, "#ifndef ISOTOPE_TABLE_H\n#define ISOTOPE_TABLE_H\n\n");
    fprintf(output, "#define CASMI_ISOTOPE_COUNT %uu\n\n", count);
    fprintf(output, "static const CasmiIsotope casmi_isotope[CASMI_ISOTOPE_COUNT] = {\n");
    for (unsigned int isotope = 0u; isotope < count; isotope += 1u)
    {
        const TableIsotope *const entry = &s_isotope[isotope];
        fprintf(output, "    {%uu, \"%s\", %uu, %lldll, %lldll, %d, %lldll, %lldll, %lldll, %lldll},\n",
                entry->atomic_number, entry->symbol, entry->mass_number, entry->mass, entry->mass_uncertainty,
                entry->mass_estimated, entry->composition, entry->composition_uncertainty, entry->weight_low,
                entry->weight_high);
    }
    fprintf(output, "};\n\n#endif\n");
    fclose(output);
    printf("wrote %u isotopes to %s\n", count, arguments[2]);
    return 0;
}
