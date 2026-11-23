// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_funnels.cu: the blind reading and funnels
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "pi_plane_internal.h"

// the first PLANE_BLIND_RINGS rings, or every ring where there are fewer
static void plane_blind_rings(const PlaneRings *sizes, PlaneRings *rings)
{
    *rings = *sizes;
    rings->count = (sizes->count < PLANE_BLIND_RINGS) ? sizes->count : PLANE_BLIND_RINGS;
}

// Blind pairs: pi's unseen blocks, from block `first` past the seen bits. The sim arranges each in the rings unrolled
// beside a shuffle of the same block. A keyed coin decides which is which. Where balanced, a keyed deal puts pi on A
// in exactly half the pairs. Only blind_answer.txt holds the answer.
//
// The block keys the coin, and the first block keys the deal. A round from --blind-from deals its own sides, and the
// round from block 0 keeps the sides it showed
int plane_blind(SimResults *results, const PlaneRings *sizes, unsigned long long seen, unsigned int first,
                unsigned int pairs, int balanced, const char *directory)
{
    VerbaLinea *const line = &results->line;
    PlaneRings rings;
    plane_blind_rings(sizes, &rings);
    const unsigned long long block = plane_ring_total(&rings);
    const unsigned long long needed = seen + ((first + pairs) * block);
    if ((first + pairs > PLANE_BLIND_MAX) || (needed + PLANE_GUARD + 64ULL > ORIOR_EXACT_BITS))
    {
        mmgr_line_put(line, "  the blind pairs lie in the first ");
        mmgr_line_decimal(line, PLANE_BLIND_MAX, 1u);
        mmgr_line_put(line, " blocks and need an exact width of ");
        mmgr_line_decimal(line, needed + PLANE_GUARD + 64ULL, 1u);
        mmgr_line_put(line, " bits\n");
        sim_check(results, 0, "the blind pairs fit the exact width");
        return 0;
    }
    // pi on A in the first half of the pairs, then dealt by a keyed Fisher-Yates shuffle
    unsigned char sides[PLANE_BLIND_MAX] = {0u};
    for (unsigned int pair = 0u; pair < pairs / 2u; pair += 1u)
    {
        sides[pair] = 1u;
    }
    for (unsigned int pair = pairs; pair > 1u; pair -= 1u)
    {
        // the draw is below the pair count. It narrows to unsigned int exactly
        const unsigned int other = (unsigned int)sim_draw_below(
            PLANE_KEY ^ 0x42414C414E4345ULL ^ ((unsigned long long)first << 32u), pair - 1u, pair);
        const unsigned char held = sides[pair - 1u];
        sides[pair - 1u] = sides[other];
        sides[other] = held;
    }
    unsigned char *const all = (unsigned char *)plane_taken(needed, 1u, 1);
    unsigned char *const other = (unsigned char *)plane_taken(block, 1u, 1);
    // the bits needed are at max 2^18 or so, far below 2^32
    int ok = plane_bits(results, (unsigned int)needed, all);
    char path[PLATFORM_PATH_BYTES];
    plane_path(path, directory, "/blind_answer.txt", "", "");
    PlatformFile *const answer = ok ? platform_file_open(path, "w") : NULL;
    ok = ok && (answer != NULL);
    for (unsigned int pair = 0u; ok && (pair < pairs); pair += 1u)
    {
        const size_t from = (size_t)(seen + ((first + pair) * block));
        const unsigned char *const mine = &all[from];
        plane_shuffle(mine, (size_t)block, 7919u + first + pair, other);
        const int pi_first =
            balanced ? (sides[pair] != 0u) : ((sim_draw(PLANE_KEY ^ 0x424C494E44ULL, first + pair) & 1ULL) == 0ULL);
        char number[24];
        VerbaLinea number_line = mmgr_line_over(number, sizeof(number));
        mmgr_line_decimal(&number_line, pair + 1u, 1u);
        (void)mmgr_line_finish(&number_line);
        plane_path(path, directory, "/blind_", number, "_A.png");
        ok = plane_ring_unrolled(&rings, pi_first ? mine : other, path);
        plane_path(path, directory, "/blind_", number, "_B.png");
        ok = ok && plane_ring_unrolled(&rings, pi_first ? other : mine, path);
        {
            mmgr_line_put(platform_file_line(answer), "pair ");
            mmgr_line_decimal(platform_file_line(answer), pair + 1u, 1u);
            mmgr_line_put(platform_file_line(answer), ": pi is ");
            mmgr_line_put(platform_file_line(answer), pi_first ? "A" : "B");
            mmgr_line_put(platform_file_line(answer), " (pi's bits ");
            mmgr_line_decimal(platform_file_line(answer), (unsigned long long)from + 1ULL, 1u);
            mmgr_line_put(platform_file_line(answer), " to ");
            mmgr_line_decimal(platform_file_line(answer), (unsigned long long)(from + block), 1u);
            mmgr_line_put(platform_file_line(answer), ")\n");
        }
    }
    if (answer != NULL)
    {
        ok = (platform_file_close(answer) == 0) && ok;
    }
    plane_given(other);
    plane_given(all);
    sim_check(results, ok, "the blind pairs are drawn and their answer written");
    mmgr_line_put(line, "  blind: ");
    mmgr_line_decimal(line, pairs, 1u);
    mmgr_line_put(line, " pairs, blind_<n>_A.png and blind_<n>_B.png, rings 0 to ");
    mmgr_line_decimal(line, rings.count - 1u, 1u);
    mmgr_line_put(line, " unrolled; each pair is one unseen block of pi from bit ");
    mmgr_line_decimal(line, seen + (first * block) + 1ULL, 1u);
    mmgr_line_put(line, " on and a shuffle of the same block; ");
    mmgr_line_put(line, balanced ? "pi is A in exactly half the pairs, dealt by a keyed shuffle; " : "");
    mmgr_line_put(line, "which is pi is in blind_answer.txt and nowhere else\n");
    sim_flush(results);
    return ok;
}

// The funnels. A funnel's mouth is a run of two equal bits or more on rings 1 to PLANE_FUNNEL_MOUTH_LAST. Each bar
// below it is the widest run of the same bit among the cells whose parent lies under the bar above. The funnel ends on
// the ring where no such cell contains the bit. Its depth is how many bars it has below the mouth, and the dense
// rings, two bits or more to a pixel, count too. Three bars w0, w1, w2 fit one ratio as w1^2 comes near w0 w2.
#define PLANE_FUNNEL_MOUTH_LAST 8u

#define PLANE_FUNNEL_FRESH_MAX 16u

// the funnels drawn over a picture are those from a mouth this many bits wide or wider that reach the last ring
#define PLANE_FUNNEL_DRAWN_MOUTH 3ULL

static const char *const s_plane_funnel_names[PLANE_FUNNEL_READINGS] = {
    "the funnels' depth, the bars below every mouth", "three bars' fit to one ratio, the mean in millionths",
    "the bits agreeing with their parent"};

// the bars of one funnel, a bar a ring at max, in a table at that bound with their count
typedef struct
{
    PlaneBar bar[PLANE_RINGS_MAX];
    size_t count;
} PlaneBars;

// the first bit of each ring
static void plane_ring_starts(const PlaneRings *rings, unsigned long long *starts)
{
    starts[0] = 0ULL;
    for (size_t ring = 1u; ring < rings->count; ring += 1u)
    {
        starts[ring] = starts[ring - 1u] + rings->size[ring - 1u];
    }
}

static void plane_bar_put(PlaneBars *bars, size_t ring, unsigned long long first, unsigned long long last)
{
    bars->bar[bars->count].ring = ring;
    bars->bar[bars->count].first = first;
    bars->bar[bars->count].last = last;
    bars->count += 1u;
}

// the bars of the funnel whose mouth is cells first to last of the ring
static void plane_funnel_trace(const PlaneRings *rings, const unsigned long long *starts, const unsigned char *bits,
                               size_t ring, unsigned long long first, unsigned long long last, PlaneBars *bars)
{
    const unsigned char bit = bits[starts[ring] + first];
    bars->count = 0u;
    plane_bar_put(bars, ring, first, last);
    while (ring + 1u < rings->count)
    {
        const unsigned long long inner = rings->size[ring];
        const unsigned long long outer = rings->size[ring + 1u];
        // The cells whose parent, floor(place c_k / c_(k + 1)), lies from first to last. There is at least one, as the
        // ring out is at least as large
        const unsigned long long from = ((first * outer) + inner - 1ULL) / inner;
        const unsigned long long to = ((((last + 1ULL) * outer) + inner - 1ULL) / inner) - 1ULL;
        const unsigned char *const cells = bits + starts[ring + 1u];
        unsigned long long best_first = 0ULL;
        unsigned long long best_length = 0ULL;
        unsigned long long run_first = from;
        for (unsigned long long place = from; place <= to; place += 1ULL)
        {
            if (cells[place] != bit)
            {
                continue;
            }
            if ((place == from) || (cells[place - 1ULL] != bit))
            {
                run_first = place;
            }
            if (place - run_first + 1ULL > best_length)
            {
                best_length = place - run_first + 1ULL;
                best_first = run_first;
            }
        }
        if (best_length == 0ULL)
        {
            return;
        }
        ring += 1u;
        first = best_first;
        last = best_first + best_length - 1ULL;
        plane_bar_put(bars, ring, first, last);
    }
}

// how near three bars come to one ratio, in millionths: the smaller of w1^2 and w0 w2 over the larger, with w = bits /
// c_k
static unsigned long long plane_funnel_fit(const PlaneRings *rings, const PlaneBar *bar)
{
    const unsigned long long wide = bar[0].last - bar[0].first + 1ULL;
    const unsigned long long middle = bar[1].last - bar[1].first + 1ULL;
    const unsigned long long narrow = bar[2].last - bar[2].first + 1ULL;
    // w1^2 against w0 w2 is middle^2 c_k c_(k + 2) against wide narrow c_(k + 1)^2; every width and size is below
    // 2^13. Each product stays under 2^52
    const unsigned long long square = middle * middle * rings->size[bar[0].ring] * rings->size[bar[2].ring];
    const unsigned long long across = wide * narrow * rings->size[bar[1].ring] * rings->size[bar[1].ring];
    unsigned long long smaller = (square < across) ? square : across;
    unsigned long long larger = (square > across) ? square : across;
    // the loop halves both until a million times the smaller fits a word
    while (larger >= (1ULL << 43u))
    {
        smaller >>= 1u;
        larger >>= 1u;
    }
    return (smaller * 1000000ULL) / larger;
}

// the last ring that holds a funnel's mouth
static size_t plane_last_mouth(const PlaneRings *rings)
{
    return ((rings->count - 1u) < PLANE_FUNNEL_MOUTH_LAST) ? (rings->count - 1u) : PLANE_FUNNEL_MOUTH_LAST;
}

// the last place of the run of equal cells from `place` on a ring of `size` cells. Each cell is a step of the run
static unsigned long long plane_run_end(const unsigned char *cells, unsigned long long place, unsigned long long size)
{
    unsigned long long end = place;
    while ((end + 1ULL < size) && (cells[end + 1ULL] == cells[place]))
    {
        end += 1ULL;
    }
    return end;
}

// Every funnel of the block: the sum of the bars below every mouth. Also the mean fit of every three bars whose first
// is two bits or wider, and the bits that agree with their parent
static PlaneFunnelMeasurement plane_funnel_read(const PlaneRings *rings, const unsigned long long *starts,
                                                const unsigned char *bits)
{
    PlaneFunnelMeasurement measurement = {{0ULL, 0ULL, plane_ring_read(rings, bits).agree}};
    unsigned long long fit = 0ULL;
    unsigned long long triples = 0ULL;
    PlaneBars bars;
    const size_t last_mouth = plane_last_mouth(rings);
    for (size_t ring = 1u; ring <= last_mouth; ring += 1u)
    {
        const unsigned char *const cells = bits + starts[ring];
        unsigned long long place = 0ULL;
        while (place < rings->size[ring])
        {
            const unsigned long long end = plane_run_end(cells, place, rings->size[ring]);
            if (end > place)
            {
                plane_funnel_trace(rings, starts, bits, ring, place, end, &bars);
                measurement.value[0] += bars.count - 1u;
                for (size_t bar = 0u; bar + 2u < bars.count; bar += 1u)
                {
                    if (bars.bar[bar].last > bars.bar[bar].first)
                    {
                        fit += plane_funnel_fit(rings, &bars.bar[bar]);
                        triples += 1ULL;
                    }
                }
            }
            place = end + 1ULL;
        }
    }
    measurement.value[1] = (triples == 0ULL) ? 0ULL : (fit / triples);
    return measurement;
}

// The rings unrolled as plane_ring_unrolled draws them. The bits of every funnel that reaches the last ring, from a
// mouth PLANE_FUNNEL_DRAWN_MOUTH bits wide or wider, show in red
static int plane_funnel_picture(const PlaneRings *rings, const unsigned char *bits, const char *path)
{
    unsigned long long starts[PLANE_RINGS_MAX];
    plane_ring_starts(rings, starts);
    const unsigned long long total = plane_ring_total(rings);
    unsigned char *const marked = (unsigned char *)plane_taken(total, 1u, 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = marked, .bytes = (size_t)total, .val = 0u);
    PlaneBars bars;
    bars.count = 0u;
    const size_t last_mouth = plane_last_mouth(rings);
    for (size_t ring = 1u; ring <= last_mouth; ring += 1u)
    {
        const unsigned char *const cells = bits + starts[ring];
        unsigned long long place = 0ULL;
        while (place < rings->size[ring])
        {
            const unsigned long long end = plane_run_end(cells, place, rings->size[ring]);
            if (end + 1ULL >= place + PLANE_FUNNEL_DRAWN_MOUTH)
            {
                plane_funnel_trace(rings, starts, bits, ring, place, end, &bars);
            }
            if ((end + 1ULL >= place + PLANE_FUNNEL_DRAWN_MOUTH) &&
                (bars.bar[bars.count - 1u].ring + 1u == rings->count))
            {
                for (size_t at = 0u; at < bars.count; at += 1u)
                {
                    const PlaneBar &bar = bars.bar[at];
                    for (unsigned long long cell = bar.first; cell <= bar.last; cell += 1ULL)
                    {
                        marked[(size_t)(starts[bar.ring] + cell)] = 1u;
                    }
                }
            }
            place = end + 1ULL;
        }
    }
    const unsigned long long width = PLANE_RING_UNROLLED_WIDTH;
    const unsigned long long height = rings->count * PLANE_RING_ROW;
    unsigned char *const pixels = (unsigned char *)plane_taken(width * height * 3ULL, 1u, 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = pixels, .bytes = (size_t)(width * height * 3ULL),
               .val = PLANE_EMPTY_SHADE);
    unsigned long long *const ones = (unsigned long long *)plane_taken(width, sizeof(unsigned long long), 1);
    unsigned long long *const counted = (unsigned long long *)plane_taken(width, sizeof(unsigned long long), 1);
    unsigned long long *const tinted = (unsigned long long *)plane_taken(width, sizeof(unsigned long long), 1);
    for (size_t ring = 0u; ring < rings->count; ring += 1u)
    {
        const unsigned long long size = rings->size[ring];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = ones, .bytes = (size_t)width * sizeof(unsigned long long), .val = 0u);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = counted, .bytes = (size_t)width * sizeof(unsigned long long),
                   .val = 0u);
        EMBED_CALL(memor.set, MemoriaCfg, .dst = tinted, .bytes = (size_t)width * sizeof(unsigned long long),
                   .val = 0u);
        for (unsigned long long place = 0ULL; place < size; place += 1ULL)
        {
            const unsigned long long from = (place * width) / size;
            const unsigned long long reach = ((place + 1ULL) * width) / size;
            const unsigned long long to = (reach > (from + 1ULL)) ? reach : (from + 1ULL);
            for (unsigned long long column = from; column < to; column += 1ULL)
            {
                ones[(size_t)column] += (unsigned long long)bits[starts[ring] + place];
                counted[(size_t)column] += 1ULL;
                tinted[(size_t)column] += (unsigned long long)marked[(size_t)(starts[ring] + place)];
            }
        }
        for (unsigned long long row = 1ULL; row + 1ULL < PLANE_RING_ROW; row += 1ULL)
        {
            for (unsigned long long column = 0ULL; column < width; column += 1ULL)
            {
                // the share of ones, scaled to 255, is at max 255, and each tint below stays within a byte
                const unsigned long long shade = (255ULL * ones[(size_t)column]) / counted[(size_t)column];
                const int red = tinted[(size_t)column] != 0ULL;
                const size_t at = (size_t)(((((ring * PLANE_RING_ROW) + row) * width) + column) * 3ULL);
                pixels[at] = (unsigned char)(red ? (110ULL + ((145ULL * shade) / 255ULL)) : shade);
                pixels[at + 1u] = (unsigned char)(red ? ((150ULL * shade) / 255ULL) : shade);
                pixels[at + 2u] = pixels[at + 1u];
            }
        }
    }
    const int written = plane_png(path, pixels, width, height, 3u);
    plane_given(tinted);
    plane_given(counted);
    plane_given(ones);
    plane_given(pixels);
    plane_given(marked);
    return written;
}

// A set of pi's blocks, each read against the same keyed shuffles of itself. The sim sums each reading over the
// blocks, pi's against the draws'. For each block, the count of the draws that reach pi's
static void plane_funnel_blocks(SimResults *results, const PlaneRings *rings, const unsigned char *all,
                                unsigned long long from, unsigned int blocks, unsigned int draws, const char *label)
{
    VerbaLinea *const line = &results->line;
    unsigned long long starts[PLANE_RINGS_MAX];
    plane_ring_starts(rings, starts);
    const unsigned long long block = plane_ring_total(rings);
    PlaneFunnelMeasurement pi_total = {{0ULL, 0ULL, 0ULL}};
    PlaneFunnelMeasurement *const drawn_total =
        (PlaneFunnelMeasurement *)plane_taken(draws, sizeof(PlaneFunnelMeasurement), 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = drawn_total, .bytes = draws * sizeof(PlaneFunnelMeasurement), .val = 0u);
    // the blocks of one set are at max PLANE_FUNNEL_FRESH_MAX
    unsigned long long reached[PLANE_FUNNEL_READINGS][PLANE_FUNNEL_FRESH_MAX] = {{0ULL}};
    unsigned char *const drawn = (unsigned char *)plane_taken(block, 1u, 1);
    for (unsigned int each = 0u; each < blocks; each += 1u)
    {
        const unsigned char *const mine = &all[(size_t)(from + (each * block))];
        const PlaneFunnelMeasurement measurement = plane_funnel_read(rings, starts, mine);
        for (unsigned int type = 0u; type < PLANE_FUNNEL_READINGS; type += 1u)
        {
            pi_total.value[type] += measurement.value[type];
        }
        for (unsigned int draw = 0u; draw < draws; draw += 1u)
        {
            plane_shuffle(mine, (size_t)block, draw, drawn);
            const PlaneFunnelMeasurement drawn_measurement = plane_funnel_read(rings, starts, drawn);
            for (unsigned int type = 0u; type < PLANE_FUNNEL_READINGS; type += 1u)
            {
                drawn_total[draw].value[type] += drawn_measurement.value[type];
                reached[type][each] += (drawn_measurement.value[type] >= measurement.value[type]) ? 1ULL : 0ULL;
            }
        }
    }
    plane_given(drawn);
    mmgr_line_put(line, "  ");
    mmgr_line_put(line, label);
    mmgr_line_put(line, ", ");
    mmgr_line_decimal(line, blocks, 1u);
    mmgr_line_put(line, " blocks from bit ");
    mmgr_line_decimal(line, from + 1ULL, 1u);
    mmgr_line_put(line, " to ");
    mmgr_line_decimal(line, from + (blocks * block), 1u);
    mmgr_line_ch(line, '\n');
    unsigned long long *const band = (unsigned long long *)plane_taken(draws, sizeof(unsigned long long), 1);
    for (unsigned int type = 0u; type < PLANE_FUNNEL_READINGS; type += 1u)
    {
        for (unsigned int draw = 0u; draw < draws; draw += 1u)
        {
            band[draw] = drawn_total[draw].value[type];
        }
        mmgr_line_put(line, "    ");
        mmgr_line_put(line, s_plane_funnel_names[type]);
        mmgr_line_put(line, ", summed over the blocks: pi's ");
        mmgr_line_decimal(line, pi_total.value[type], 1u);
        plane_band_print(line, band, draws, pi_total.value[type]);
        mmgr_line_put(line, "      each block, the draws of ");
        mmgr_line_decimal(line, draws, 1u);
        mmgr_line_put(line, " reaching pi's:");
        for (unsigned int each = 0u; each < blocks; each += 1u)
        {
            mmgr_line_ch(line, ' ');
            mmgr_line_decimal(line, reached[type][each], 1u);
        }
        mmgr_line_ch(line, '\n');
        sim_flush(results);
    }
    plane_given(band);
    plane_given(drawn_total);
}

// The funnels on the six blind blocks that the eye picked pi from, then on fresh blocks past them that no picture
// showed. The sim draws the first blind pair, pi and the shuffle that it showed beside pi, with their funnels
int plane_funnels(SimResults *results, const PlaneRings *sizes, unsigned long long seen, unsigned int fresh,
                  unsigned int draws, const char *directory)
{
    VerbaLinea *const line = &results->line;
    PlaneRings rings;
    plane_blind_rings(sizes, &rings);
    const unsigned long long block = plane_ring_total(&rings);
    const unsigned long long needed = seen + ((PLANE_BLIND_SCORED + fresh) * block);
    if ((fresh > PLANE_FUNNEL_FRESH_MAX) || (needed + PLANE_GUARD + 64ULL > ORIOR_EXACT_BITS))
    {
        mmgr_line_put(line, "  the funnels read at max ");
        mmgr_line_decimal(line, PLANE_FUNNEL_FRESH_MAX, 1u);
        mmgr_line_put(line, " fresh blocks and need an exact width of ");
        mmgr_line_decimal(line, needed + PLANE_GUARD + 64ULL, 1u);
        mmgr_line_put(line, " bits\n");
        sim_check(results, 0, "the funnels' blocks fit the exact width");
        return 0;
    }
    unsigned char *const all = (unsigned char *)plane_taken(needed, 1u, 1);
    // the bits needed are at max 2^18 or so, far below 2^32
    int ok = plane_bits(results, (unsigned int)needed, all);
    if (ok)
    {
        const unsigned char *const mine = &all[(size_t)seen];
        unsigned char *const other = (unsigned char *)plane_taken(block, 1u, 1);
        plane_shuffle(mine, (size_t)block, 7919u, other);
        char path[PLATFORM_PATH_BYTES];
        plane_path(path, directory, "/funnels_pi.png", "", "");
        ok = plane_funnel_picture(&rings, mine, path);
        plane_path(path, directory, "/funnels_shuffled.png", "", "");
        ok = ok && plane_funnel_picture(&rings, other, path);
        plane_given(other);
        sim_check(results, ok, "the first blind pair is drawn with its funnels");
    }
    if (ok)
    {
        mmgr_line_put(line, "  the funnels: mouths of two bits or more on rings 1 to ");
        mmgr_line_decimal(line, PLANE_FUNNEL_MOUTH_LAST, 1u);
        mmgr_line_put(line, ", traced to ring ");
        mmgr_line_decimal(line, rings.count - 1u, 1u);
        mmgr_line_put(line,
                      "; funnels_pi.png and funnels_shuffled.png are the first blind pair, the funnels from a mouth ");
        mmgr_line_decimal(line, PLANE_FUNNEL_DRAWN_MOUTH, 1u);
        mmgr_line_put(line, " bits wide or wider that reach the last ring in red\n");
        sim_flush(results);
        plane_funnel_blocks(results, &rings, all, seen, PLANE_BLIND_SCORED, draws, "the six blind blocks");
        if (fresh > 0u)
        {
            plane_funnel_blocks(results, &rings, all, seen + (PLANE_BLIND_SCORED * block), fresh, draws,
                                "fresh blocks, unseen");
        }
    }
    plane_given(all);
    return ok;
}
