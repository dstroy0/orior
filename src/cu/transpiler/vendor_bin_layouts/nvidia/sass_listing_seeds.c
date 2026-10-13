// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sass_listing_seeds.c: the instructions a listing of NVIDIA's compiler writes, one of each operation, as the seeds a
// discover pass asks their relation of (khw_write --seeds). The listing prints each instruction's two 64-bit words as
// `/* 0x<low> */` and `/* 0x<high> */`; the first instruction seen of each operation key, the low word's bits 0 to
// 11, is printed as `<low> <high>` in hex, one a line. With `--modifiers` an instruction is one of its operation and
// its high word's bits below the scheduler's and above the fourth register's, 72 to 104, so that each modifier the
// compiler wrote is a seed of its own. Which instructions to ask is the choice here and no fact of the part: a form
// enters the machine file only where the part answers for it (P8, Q17).
//
//     sass_listing_seeds <listing.sass> [--modifiers]
//
// Exit 0 with the seeds on the standard output; 1 where the listing did not read; 2 where the words are not these
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the operation key, the low word's bits 0 to 11
#define SEED_KEY_BITS 12u
// the modifiers an instruction carries past its fourth register field and below the scheduler's word: the high word's
// bits 72 to 104, bits 8 to 40 of the word
#define SEED_MODIFIER_FIRST 8u
#define SEED_MODIFIER_LAST 40u

// a seed seen: what tells it from the others, and the instruction it was first seen as
typedef struct
{
    unsigned long long key;
    unsigned long long modifiers;
    unsigned long long low;
    unsigned long long high;
} Seed;

static Seed *s_seed;
static unsigned int s_seeds;
static unsigned int s_room;

// 1 where a seed of `key` and `modifiers` is among those seen
static int seed_seen(unsigned long long key, unsigned long long modifiers)
{
    for (unsigned int at = 0u; at < s_seeds; at += 1u)
    {
        if ((s_seed[at].key == key) && (s_seed[at].modifiers == modifiers))
        {
            return 1;
        }
    }
    return 0;
}

// the instruction `low` and `high` kept as a seed where none of its key and modifiers is seen yet: 1, or 0 where
// there was no memory for it
static int seed_kept(unsigned long long low, unsigned long long high, int modifiers)
{
    const unsigned long long key = low & ((1ull << SEED_KEY_BITS) - 1ull);
    const unsigned long long held =
        modifiers ? ((high >> SEED_MODIFIER_FIRST) & ((1ull << (SEED_MODIFIER_LAST - SEED_MODIFIER_FIRST + 1u)) - 1ull))
                  : 0ull;
    if (seed_seen(key, held))
    {
        return 1;
    }
    if (s_seeds == s_room)
    {
        const unsigned int room = (s_room != 0u) ? (s_room * 2u) : 256u;
        Seed *const grown = (Seed *)realloc(s_seed, (size_t)room * sizeof(Seed));
        if (grown == NULL)
        {
            return 0;
        }
        s_seed = grown;
        s_room = room;
    }
    s_seed[s_seeds] = (Seed){key, held, low, high};
    s_seeds += 1u;
    return 1;
}

int main(int count, char **word)
{
    const int modifiers = (count == 3) && (strcmp(word[2], "--modifiers") == 0);
    if ((count < 2) || (count > 3) || ((count == 3) && !modifiers))
    {
        printf("sass_listing_seeds <listing.sass> [--modifiers]\n");
        return 2;
    }
    FILE *const listing = fopen(word[1], "rb");
    if (listing == NULL)
    {
        printf("sass_listing_seeds: %s did not open\n", word[1]);
        return 1;
    }
    // each instruction's words come as a pair of `/* 0x<16 hex digits> */`, the low word first
    char line[4096];
    unsigned long long held[2] = {0ull, 0ull};
    unsigned int helds = 0u;
    int kept = 1;
    while (kept && (fgets(line, sizeof(line), listing) != NULL))
    {
        for (const char *at = strstr(line, "/* 0x"); at != NULL; at = strstr(at + 5, "/* 0x"))
        {
            char *end = NULL;
            const unsigned long long value = strtoull(at + 5, &end, 16);
            if ((end == NULL) || ((end - (at + 5)) != 16) || (strncmp(end, " */", 3u) != 0))
            {
                continue;
            }
            held[helds] = value;
            helds += 1u;
            if (helds == 2u)
            {
                kept = seed_kept(held[0], held[1], modifiers);
                helds = 0u;
            }
        }
    }
    fclose(listing);
    if (!kept)
    {
        printf("sass_listing_seeds: no memory for the seeds\n");
        return 1;
    }
    for (unsigned int at = 0u; at < s_seeds; at += 1u)
    {
        printf("%016llx %016llx\n", s_seed[at].low, s_seed[at].high);
    }
    free(s_seed);
    return 0;
}
