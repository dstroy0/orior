// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_ring_report.cu: rings, paths and printing
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "pi_plane_internal.h"

// the path of a picture of the rings, from the directory and its file name
static void plane_ring_path(char *path, const char *directory, const char *file)
{
    plane_path(path, directory, "/", file, "");
}

// The rings mod floor(pi 2^k), drawn. The sim reads each bit against the bit at its angle one ring in, against keyed
// shuffles of the rings' bits
int plane_rings(SimResults *results, const unsigned char *bits, unsigned int bits_count, unsigned int draws,
                unsigned int blind, unsigned int blind_first, int balanced, unsigned int funnels, const char *directory)
{
    VerbaLinea *const line = &results->line;
    PlaneRings rings;
    plane_ring_sizes(bits, bits_count, PLANE_BITS_MAX, &rings);
    const unsigned long long total = plane_ring_total(&rings);
    if ((rings.count < 2u) || (total + PLANE_GUARD + 64ULL > ORIOR_EXACT_BITS))
    {
        mmgr_line_put(line, "  the rings need ");
        mmgr_line_decimal(line, total, 1u);
        mmgr_line_put(line, " bits and a wider exact integer\n");
        sim_check(results, 0, "the rings fit the exact width");
        return 0;
    }
    // the rings' bits: the first of pi's bits where they reach the total, else pi's bits read again at the total
    unsigned char *const ring_bits = (unsigned char *)plane_taken(total, 1u, 1);
    int ok = 1;
    if ((unsigned long long)bits_count >= total)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = ring_bits, .src = bits, .bytes = (size_t)total);
    }
    else
    {
        // the total is at max PLANE_BITS_MAX. An unsigned int stores it
        ok = plane_bits(results, (unsigned int)total, ring_bits);
    }
    mmgr_line_put(line, "  the rings: ring k contains floor(pi 2^k) bits,");
    for (size_t ring = 0u; ring < rings.count; ring += 1u)
    {
        mmgr_line_ch(line, ' ');
        mmgr_line_decimal(line, rings.size[ring], 1u);
    }
    mmgr_line_put(line, ", ");
    mmgr_line_decimal(line, total, 1u);
    mmgr_line_put(line, " bits in all\n");
    const PlaneTurn turn = plane_turn(ring_bits);
    unsigned char *const drawn = (unsigned char *)plane_taken(total, 1u, 1);
    plane_shuffle(ring_bits, (size_t)total, 0u, drawn);
    const size_t center = (rings.count < PLANE_RING_CENTER) ? rings.count : PLANE_RING_CENTER;
    char path[PLATFORM_PATH_BYTES];
    if (ok)
    {
        plane_ring_path(path, directory, "pi_rings_unrolled.png");
        ok = plane_ring_unrolled(&rings, ring_bits, path);
    }
    if (ok)
    {
        plane_ring_path(path, directory, "shuffled_rings_unrolled.png");
        ok = plane_ring_unrolled(&rings, drawn, path);
    }
    if (ok)
    {
        plane_ring_path(path, directory, "pi_rings.png");
        ok = plane_ring_circle(&rings, ring_bits, turn, rings.count, 1LL, path);
    }
    if (ok)
    {
        plane_ring_path(path, directory, "shuffled_rings.png");
        ok = plane_ring_circle(&rings, drawn, turn, rings.count, 1LL, path);
    }
    if (ok)
    {
        plane_ring_path(path, directory, "pi_rings_center.png");
        ok = plane_ring_circle(&rings, ring_bits, turn, center, PLANE_RING_CENTER_SCALE, path);
    }
    if (ok)
    {
        plane_ring_path(path, directory, "shuffled_rings_center.png");
        ok = plane_ring_circle(&rings, drawn, turn, center, PLANE_RING_CENTER_SCALE, path);
    }
    sim_check(results, ok, "the rings are drawn for pi and for one shuffle of their bits");
    if (!ok)
    {
        plane_given(drawn);
        plane_given(ring_bits);
        return 0;
    }
    const PlaneRingMeasurement measurement = plane_ring_read(&rings, ring_bits);
    unsigned long long *const agree_band = (unsigned long long *)plane_taken(draws, sizeof(unsigned long long), 1);
    unsigned long long *const run_band = (unsigned long long *)plane_taken(draws, sizeof(unsigned long long), 1);
    for (unsigned int draw = 0u; draw < draws; draw += 1u)
    {
        plane_shuffle(ring_bits, (size_t)total, draw, drawn);
        const PlaneRingMeasurement drawn_measurement = plane_ring_read(&rings, drawn);
        agree_band[draw] = drawn_measurement.agree;
        run_band[draw] = drawn_measurement.run;
    }
    mmgr_line_put(line, "  each bit against the bit at its angle one ring in: ");
    mmgr_line_decimal(line, measurement.agree, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, measurement.pairs, 1u);
    mmgr_line_put(line, " agree");
    plane_band_print(line, agree_band, draws, measurement.agree);
    mmgr_line_put(line, "  the longest chain of equal bits running outward: ");
    mmgr_line_decimal(line, measurement.run, 1u);
    mmgr_line_put(line, " rings, ending on ring ");
    mmgr_line_decimal(line, measurement.run_ring, 1u);
    mmgr_line_put(line, " at place ");
    mmgr_line_decimal(line, measurement.run_place, 1u);
    plane_band_print(line, run_band, draws, measurement.run);
    mmgr_line_put(line, "  the pictures pi_rings.png (true radius 2^(k - 1)), pi_rings_center.png (rings 0 to ");
    mmgr_line_decimal(line, center - 1u, 1u);
    mmgr_line_put(line, " at twice the size) and pi_rings_unrolled.png (ring k as row k), each with its shuffle\n");
    sim_flush(results);
    plane_given(run_band);
    plane_given(agree_band);

    // the spirals, on the bits the pictures showed and on pi's next block of as many bits, which no picture showed
    plane_ring_spiral_read(results, &rings, ring_bits, (size_t)total, draws, "the rings' bits");
    plane_given(drawn);
    plane_given(ring_bits);
    if (2ULL * total + PLANE_GUARD + 64ULL > ORIOR_EXACT_BITS)
    {
        mmgr_line_put(line, "  the next block needs an exact width of ");
        mmgr_line_decimal(line, (2ULL * total) + PLANE_GUARD + 64ULL, 1u);
        mmgr_line_put(line, " bits, not read\n");
        sim_flush(results);
        return 1;
    }
    unsigned char *const both = (unsigned char *)plane_taken(2ULL * total, 1u, 1);
    // twice the total is at max 2 PLANE_BITS_MAX, below 2^17
    const int more = plane_bits(results, (unsigned int)(2ULL * total), both);
    sim_check(results, more, "pi's next block of the rings' size is certified");
    if (more)
    {
        const unsigned char *const next = &both[total];
        plane_ring_path(path, directory, "pi_rings_unrolled_next.png");
        sim_check(results, plane_ring_unrolled(&rings, next, path), "the next block is drawn unrolled");
        plane_ring_spiral_read(results, &rings, next, (size_t)total, draws, "pi's next block, unseen");
    }
    plane_given(both);
    if (more && (blind > 0u))
    {
        plane_blind(results, &rings, 2ULL * total, blind_first, blind, balanced, directory);
    }
    if (more && (funnels > 0u))
    {
        plane_funnels(results, &rings, 2ULL * total, funnels, draws, directory);
    }
    return 1;
}

// The bits under the cells a drawn path covers on a shape, the cells read as "row column" lines. The call reads the
// file once into one cell of the temporary tier. The cells under the path go in one cell of the persistent tier at
// the count of its lines, which `under` names. The answer is the count of the cells [RETURNS OWNERSHIP]
size_t plane_path_cells(const PlaneExtent &extent, const char *path, long long **under)
{
    *under = NULL;
    unsigned long long size = 0ULL;
    if (platform_file_size(path, &size) == 0)
    {
        return 0u;
    }
    char *const text = (char *)plane_taken(size + 1ULL, 1u, 1);
    unsigned long long read = 0ULL;
    if (platform_file_read(path, text, size, &read) == 0)
    {
        plane_given(text);
        return 0u;
    }
    text[read] = '\0';
    // the call counts the lines first, then takes the cell of the cells under the path at that count
    size_t lines = 1u;
    const char *newline_at = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = (size_t)read, .byte = '\n');
    while (newline_at != NULL)
    {
        lines += 1u;
        const size_t past = (size_t)(newline_at - text) + 1u;
        newline_at =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &text[past], .cap = (size_t)read - past, .byte = '\n');
    }
    long long *const cells = (long long *)plane_taken(lines, sizeof(long long), 0);
    size_t count = 0u;
    // A line a cell, its row and its column, each signed. The read ends at the first line that holds no two numbers,
    // and a line of white space alone is no line
    const char *at = text;
    const char *const end_of_text = &text[read];
    while (at < end_of_text)
    {
        const char *const newline =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = at, .cap = (size_t)(end_of_text - at), .byte = '\n');
        const char *const line_end = (newline != NULL) ? newline : end_of_text;
        const char *walk = at;
        long long pair[2] = {0LL, 0LL};
        int numbers = 0;
        while (numbers < 2)
        {
            walk += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = walk, .cap = (size_t)(line_end - walk),
                               .other = " \t\r\n");
            const int negative = ((walk < line_end) && (*walk == '-')) ? 1 : 0;
            const char *const digits = walk + negative;
            const size_t digit_count = (digits < line_end)
                                           ? EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = digits,
                                                        .cap = (size_t)(line_end - digits), .other = "0123456789")
                                           : 0u;
            if (digit_count == 0u)
            {
                break;
            }
            const char *end = digits;
            const embed_word magnitude = EMBED_CALL(cellul.to_ulong, TransfiguroCfg, .src = digits, .end = &end);
            // Explicit casts carry the magnitude to the signed width of the cell, as %lld reads it
            pair[numbers] = (negative != 0) ? -(long long)magnitude : (long long)magnitude;
            walk = end;
            numbers += 1;
        }
        const int blank = (numbers == 0) && (walk == line_end);
        if (!blank && (numbers != 2))
        {
            break;
        }
        if (!blank)
        {
            const long long cell = plane_cell_at(extent, pair[0], pair[1]);
            if (cell >= 0LL)
            {
                cells[count] = cell;
                count += 1u;
            }
        }
        at = (newline != NULL) ? (newline + 1) : end_of_text;
    }
    plane_given(text);
    *under = cells;
    return count;
}

unsigned long long plane_ones_under(const long long *under, size_t count, const unsigned char *bits)
{
    unsigned long long ones = 0ULL;
    for (size_t at = 0u; at < count; at += 1u)
    {
        ones += (unsigned long long)bits[under[at]];
    }
    return ones;
}

// a named text argument, or NULL where the request names none
const char *plane_text(int count, char **arguments, const char *name)
{
    for (int argument = 1; argument + 1 < count; argument += 1)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = arguments[argument], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = name, .ci = EMBED_FALSE))
        {
            return arguments[argument + 1];
        }
    }
    return NULL;
}

// The directory of the pictures: the --out argument, or the program's own directory, or "." where its path names
// none. The program's directory is a text the call keeps for the run
const char *plane_directory(int count, char **arguments)
{
    const char *const out = plane_text(count, arguments, "--out");
    if (out != NULL)
    {
        return out;
    }
    static char s_directory[PLATFORM_PATH_BYTES];
    const char *const program = arguments[0];
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = program, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const char *const slash = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = program, .cap = length, .byte = '/');
    const char *const backslash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = program, .cap = length, .byte = '\\');
    const char *const last = ((slash != NULL) && ((backslash == NULL) || (slash > backslash))) ? slash : backslash;
    if (last == NULL)
    {
        return ".";
    }
    const size_t kept = (size_t)(last - program);
    if (kept >= sizeof(s_directory))
    {
        plane_refused("the program's directory passes its room");
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_directory, .src = program, .bytes = kept);
    s_directory[kept] = '\0';
    return s_directory;
}

// The path directory + stem + name + tail into the PLATFORM_PATH_BYTES at `path`. The run ends where it passes them
void plane_path(char *path, const char *directory, const char *stem, const char *name, const char *tail)
{
    VerbaLinea line = mmgr_line_over(path, PLATFORM_PATH_BYTES);
    mmgr_line_put(&line, directory);
    mmgr_line_put(&line, stem);
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, tail);
    const int fit = mmgr_line_ok(&line);
    (void)mmgr_line_finish(&line);
    if (fit == 0)
    {
        plane_refused("a picture's path passes its room");
    }
}

// a named whole-number argument, or the fallback where the request names none
unsigned int plane_number(int count, char **arguments, const char *name, unsigned int fallback)
{
    const char *const text = plane_text(count, arguments, name);
    if (text == NULL)
    {
        return fallback;
    }
    // a request past its max errors in its reader. The conversion's range does not matter here
    return (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = text, .end = NULL, .base = 10);
}

void plane_line_print(VerbaLinea *line, const PlaneLine &found)
{
    mmgr_line_decimal(line, found.length, 1u);
    mmgr_line_put(line, " bits along (");
    mmgr_line_decimal_signed(line, found.step.row);
    mmgr_line_put(line, ", ");
    mmgr_line_decimal_signed(line, found.step.column);
    mmgr_line_put(line, ") from bit ");
    mmgr_line_decimal_signed(line, found.start);
}

// -1, 0 or 1 as the left word is below, equal to or above the right
static int plane_word_order(const void *left, const void *right)
{
    const unsigned long long one = *(const unsigned long long *)left;
    const unsigned long long other = *(const unsigned long long *)right;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

// The draws' least, median and max, and how many reach pi's measurement. The call sorts a copy of the draws on the
// temporary tier
void plane_band_print(VerbaLinea *line, const unsigned long long *band, size_t count, unsigned long long measurement)
{
    unsigned long long *const sorted = (unsigned long long *)plane_taken(count, sizeof(unsigned long long), 1);
    unsigned long long *const room = (unsigned long long *)plane_taken(count, sizeof(unsigned long long), 1);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = sorted, .src = band, .bytes = count * sizeof(unsigned long long));
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = sorted, .bytes = sizeof(unsigned long long), .scratch = room,
                     .count = count, .order = plane_word_order);
    unsigned long long reached = 0ULL;
    for (size_t at = 0u; at < count; at += 1u)
    {
        reached += (sorted[at] >= measurement) ? 1ULL : 0ULL;
    }
    mmgr_line_put(line, "; the draws: least ");
    mmgr_line_decimal(line, sorted[0], 1u);
    mmgr_line_put(line, ", median ");
    mmgr_line_decimal(line, sorted[count / 2u], 1u);
    mmgr_line_put(line, ", max ");
    mmgr_line_decimal(line, sorted[count - 1u], 1u);
    mmgr_line_put(line, "; ");
    mmgr_line_decimal(line, reached, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, count, 1u);
    mmgr_line_put(line, " reach pi's\n");
    plane_given(room);
    plane_given(sorted);
}
