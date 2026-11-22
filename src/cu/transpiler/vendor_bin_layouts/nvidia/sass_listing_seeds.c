// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sass_listing_seeds.c: the instructions a listing of NVIDIA's compiler writes, one of each operation, as the seeds a
// discover pass queries their relation of (khw_write --seeds). The listing prints each instruction's two 64-bit words
// as
// `/* 0x<low> */` and `/* 0x<high> */`; the first instruction seen of each operation key, the low word's bits 0 to
// 11, is printed as `<low> <high>` in hex, one a line. With `--modifiers` an instruction is one of its operation and
// its high word's bits below the scheduler's and above the fourth register's, 72 to 104, so that each modifier the
// compiler wrote is a seed of its own. Which instructions to query is the choice here and no fact of the part: a form
// enters the machine file only where the part answers for it (P8, Q17).
//
//     orior_sass_listing_seeds <listing.sass> [--modifiers]
//
// Exit 0 with the seeds on the standard output; 1 where the listing did not read; 2 where the words are not these
#include <stdio.h>
#include "../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_linea.h"
#include <stdlib.h>
#include <string.h>

// the operation key, the low word's bits 0 to 11
#define SEED_KEY_BITS 12u
// the modifiers an instruction carries past its fourth register field and below the scheduler's word: the high word's
// bits 72 to 104, bits 8 to 40 of the word
#define SEED_MODIFIER_FIRST 8u
#define SEED_MODIFIER_LAST 40u

// a seed seen: what distinguishes it from the others, and the instruction it was first seen as
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

// the instruction `low` and `high` recorded as a seed where none of its key and modifiers is seen yet: 1, or 0 where
// there was no memory for it
static int seed_accepted(unsigned long long low, unsigned long long high, int modifiers)
{
    const unsigned long long key = low & ((1ULL << SEED_KEY_BITS) - 1ULL);
    const unsigned long long masked =
        modifiers ? ((high >> SEED_MODIFIER_FIRST) & ((1ULL << (SEED_MODIFIER_LAST - SEED_MODIFIER_FIRST + 1u)) - 1ULL))
                  : 0ULL;
    if (seed_seen(key, masked))
    {
        return 1;
    }
    if (s_seeds == s_room)
    {
        const unsigned int room = (s_room != 0u) ? (s_room * 2u) : 256u;
        Seed *const grown = (Seed *)platform_cell_grown(s_seed, (size_t)s_room * sizeof(Seed),
                                                        (size_t)room * sizeof(Seed), platform_place_g_program_owner, 0);
        if (grown == NULL)
        {
            return 0;
        }
        s_seed = grown;
        s_room = room;
    }
    s_seed[s_seeds] = (Seed){key, masked, low, high};
    s_seeds += 1u;
    return 1;
}

int program_main(int count, char **word)
{
    const int modifiers = (count == 3) && (mmgr_text_equal(word[2], "--modifiers", PLATFORM_TEXT_TO_TERMINATOR));
    if ((count < 2) || (count > 3) || ((count == 3) && !modifiers))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_sass_listing_seeds <listing.sass> [--modifiers]");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    PlatformFile *const listing = platform_file_open(word[1], "rb");
    if (listing == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_sass_listing_seeds: ");
            mmgr_line_put(output_line, word[1]);
            mmgr_line_put(output_line, " did not open");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    // each instruction's words come as a pair of `/* 0x<16 hex digits> */`, the low word first
    char line[4096];
    unsigned long long words[2] = {0ULL, 0ULL};
    unsigned int helds = 0u;
    int accepted = 1;
    while (accepted && (platform_file_text_read(line, sizeof(line), listing) != NULL))
    {
        for (const char *at = mmgr_text_find(line, "/* 0x", PLATFORM_TEXT_TO_TERMINATOR); at != NULL;
             at = mmgr_text_find(at + 5, "/* 0x", PLATFORM_TEXT_TO_TERMINATOR))
        {
            char *end = NULL;
            const unsigned long long value = strtoull(at + 5, &end, 16);
            if ((end == NULL) || ((end - (at + 5)) != 16) ||
                (!mmgr_text_starts(end, " */", PLATFORM_TEXT_TO_TERMINATOR)))
            {
                continue;
            }
            words[helds] = value;
            helds += 1u;
            if (helds == 2u)
            {
                accepted = seed_accepted(words[0], words[1], modifiers);
                helds = 0u;
            }
        }
    }
    platform_file_close(listing);
    if (!accepted)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_sass_listing_seeds: no memory for the seeds");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    for (unsigned int at = 0u; at < s_seeds; at += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_hex(output_line, s_seed[at].low, 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_hex(output_line, s_seed[at].high, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(s_seed, platform_place_g_program_owner);
    return 0;
}
