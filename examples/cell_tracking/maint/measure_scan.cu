#include "../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

#define BLOCK 64ull
#define K_BITS 5ull
#define ESCAPE 24ull

#define GOLDEN_TURN 0x9E3779B97F4A7C15ull

typedef int Integer;

static unsigned int zigzag(long long value)
{
    return (value >= 0) ? (unsigned int)(2 * value) : (unsigned int)(-2 * value - 1);
}

static long long unzigzag(unsigned int value)
{
    return ((value & 1u) == 0u) ? (long long)(value >> 1u) : -(long long)((value + 1u) >> 1u);
}

static long long floor_shift(long long value, unsigned int shift)
{
    return (value >= 0) ? (value >> shift) : -(((-value) + (1ll << shift) - 1) >> shift);
}

static unsigned long long rice_bits(const unsigned int *values, unsigned long long count)
{
    unsigned long long total = 0ull;
    for (unsigned long long at = 0ull; at < count; at += BLOCK)
    {
        const unsigned long long take = ((count - at) < BLOCK) ? (count - at) : BLOCK;
        unsigned long long sum = 0ull;
        for (unsigned long long i = 0ull; i < take; i += 1ull)
        {
            sum += values[at + i];
        }
        unsigned int guess = 0u;
        while ((guess < 32u) && (((sum / take) >> guess) != 0ull))
        {
            guess += 1u;
        }
        const unsigned int low = (guess > 2u) ? guess - 2u : 0u, high = ((guess + 1u) < 31u) ? guess + 1u : 31u;
        unsigned long long best = ~0ull;
        for (unsigned int k = low; k <= high; k += 1u)
        {
            unsigned long long bits = K_BITS;
            for (unsigned long long i = 0ull; i < take; i += 1ull)
            {
                const unsigned long long q = values[at + i] >> k;
                bits += (q < ESCAPE) ? (q + 1ull + k) : (ESCAPE + 32ull);
            }
            best = (bits < best) ? bits : best;
        }
        total += best;
    }
    return total;
}

static void lift_forward(Integer *line, unsigned long long n, Integer *scratch)
{
    if (n < 2ull)
    {
        return;
    }
    const unsigned long long lows = (n + 1ull) / 2ull, highs = n / 2ull;
    Integer *const high = scratch;
    for (unsigned long long i = 0ull; i < highs; i += 1ull)
    {
        const long long left = line[2ull * i];
        const long long right = ((2ull * i + 2ull) < n) ? line[2ull * i + 2ull] : left;
        high[i] = (Integer)(line[2ull * i + 1ull] - floor_shift(left + right, 1u));
    }
    Integer *const low = scratch + highs;
    for (unsigned long long i = 0ull; i < lows; i += 1ull)
    {
        const long long before = (i > 0ull) ? high[i - 1ull] : high[0];
        const long long after = (i < highs) ? high[i] : before;
        low[i] = (Integer)(line[2ull * i] + floor_shift(before + after + 2, 2u));
    }
    for (unsigned long long i = 0ull; i < lows; i += 1ull)
    {
        line[i] = low[i];
    }
    for (unsigned long long i = 0ull; i < highs; i += 1ull)
    {
        line[lows + i] = high[i];
    }
}

static void lift_axis(Integer *data, const unsigned long long *stride, const unsigned long long *extent,
                      unsigned int axis)
{
    // a line of the axis and the lift's room, cells of the temporary tier that the call gives back
    Integer *const line =
        (Integer *)platform_cell_taken(extent[axis] * sizeof(Integer), platform_place_g_program_owner, 1);
    Integer *const scratch = (Integer *)platform_cell_taken((extent[axis] * 2ull + 2ull) * sizeof(Integer),
                                                            platform_place_g_program_owner, 1);
    if ((line == NULL) || (scratch == NULL))
    {
        platform_cell_released(line, platform_place_g_program_owner);
        platform_cell_released(scratch, platform_place_g_program_owner);
        return;
    }
    unsigned long long other[3], other_stride[3];
    unsigned int at = 0u;
    for (unsigned int a = 0u; a < 4u; a += 1u)
    {
        if (a != axis)
        {
            other[at] = extent[a];
            other_stride[at] = stride[a];
            at += 1u;
        }
    }
    for (unsigned long long i = 0ull; i < other[0]; i += 1ull)
    {
        for (unsigned long long j = 0ull; j < other[1]; j += 1ull)
        {
            for (unsigned long long k = 0ull; k < other[2]; k += 1ull)
            {
                const unsigned long long base = i * other_stride[0] + j * other_stride[1] + k * other_stride[2];
                for (unsigned long long p = 0ull; p < extent[axis]; p += 1ull)
                {
                    line[p] = data[base + p * stride[axis]];
                }
                lift_forward(line, extent[axis], scratch);
                for (unsigned long long p = 0ull; p < extent[axis]; p += 1ull)
                {
                    data[base + p * stride[axis]] = line[p];
                }
            }
        }
    }
    platform_cell_released(line, platform_place_g_program_owner);
    platform_cell_released(scratch, platform_place_g_program_owner);
}

struct Rectangle
{
    unsigned long long y0, y1, x0, x1;
};
struct Place
{
    long long dy, dx;
    unsigned long long offset;
};

static long long radius_of(const Place &place)
{
    return place.dy * place.dy + place.dx * place.dx;
}

// the floors an extent of 2^32 halves through to 1, and more: the bound of the table of floors
#define SCAN_FLOORS 64u

// shells by radius, each swept by angle from the positive half: below 0, 0 or above 0, a total order on distinct places
static int scan_angle_order(const void *left, const void *right)
{
    const Place &one = *(const Place *)left;
    const Place &other = *(const Place *)right;
    if (radius_of(one) != radius_of(other))
    {
        return (radius_of(one) < radius_of(other)) ? -1 : 1;
    }
    const int half_one = ((one.dy < 0) || ((one.dy == 0) && (one.dx < 0))) ? 1 : 0;
    const int half_other = ((other.dy < 0) || ((other.dy == 0) && (other.dx < 0))) ? 1 : 0;
    if (half_one != half_other)
    {
        return (half_one < half_other) ? -1 : 1;
    }
    const long long cross = one.dx * other.dy - one.dy * other.dx;
    return (cross > 0) ? -1 : ((cross < 0) ? 1 : 0);
}

// shells by radius, each by dy then dx: below 0, 0 or above 0
static int scan_shell_order(const void *left, const void *right)
{
    const Place &one = *(const Place *)left;
    const Place &other = *(const Place *)right;
    if (radius_of(one) != radius_of(other))
    {
        return (radius_of(one) < radius_of(other)) ? -1 : 1;
    }
    if (one.dy != other.dy)
    {
        return (one.dy < other.dy) ? -1 : 1;
    }
    return (one.dx < other.dx) ? -1 : ((one.dx > other.dx) ? 1 : 0);
}

// the places of a shell in the golden order of their ranks: below 0, 0 or above 0
static int scan_golden_order(const void *left, const void *right)
{
    const unsigned long long one = (*(const unsigned long long *)left + 1ull) * GOLDEN_TURN;
    const unsigned long long other = (*(const unsigned long long *)right + 1ull) * GOLDEN_TURN;
    return (one < other) ? -1 : ((one > other) ? 1 : 0);
}

// a cell of the temporary tier of `count` elements of `size` bytes, NULL where the pool cannot meet it
static void *scan_cell(unsigned long long count, unsigned long long size)
{
    return platform_cell_taken((count != 0ull) ? (count * size) : size, platform_place_g_program_owner, 1);
}

int program_main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: measure_scan <file.stack>\n");
        return 1;
    }
    FILE *const stack = fopen(argv[1], "rb");
    unsigned int header[5];
    if ((stack == NULL) || (fread(header, 4, 5, stack) != 5))
    {
        fprintf(stderr, "bad stack\n");
        return 1;
    }
    const unsigned long long extent[4] = {header[0], header[1], header[2], header[3]};
    const unsigned long long voxels = extent[1] * extent[2] * extent[3];
    const unsigned long long lanes = extent[0] * voxels;
    // the stack's lanes, its integers and their naturals, each a cell of the persistent tier: the program reads the
    // stack once and keeps it to its end
    unsigned short *const raw =
        (unsigned short *)platform_cell_taken(lanes * sizeof(unsigned short), platform_place_g_program_owner, 0);
    Integer *const data = (Integer *)platform_cell_taken(lanes * sizeof(Integer), platform_place_g_program_owner, 0);
    unsigned int *const naturals =
        (unsigned int *)platform_cell_taken(lanes * sizeof(unsigned int), platform_place_g_program_owner, 0);
    if ((raw == NULL) || (data == NULL) || (naturals == NULL))
    {
        fprintf(stderr, "the pool could not meet the stack's lanes\n");
        return 1;
    }
    if (fread(raw, 2, lanes, stack) != lanes)
    {
        fprintf(stderr, "short read\n");
        return 1;
    }
    fclose(stack);
    const unsigned long long raw_bytes = lanes * 2ull;
    const unsigned long long stride[4] = {voxels, extent[2] * extent[3], extent[3], 1ull};

    for (unsigned long long i = 0ull; i < lanes; i += 1ull)
    {
        data[i] = raw[i];
    }
    // each floor's extents, the last all 1, in a table at a bound: an extent halves at each floor
    static unsigned long long s_box[SCAN_FLOORS + 1u][4];
    unsigned int floors = 0u;
    unsigned long long sub[4] = {extent[0], extent[1], extent[2], extent[3]};
    while ((floors < SCAN_FLOORS) && ((sub[0] > 1ull) || (sub[1] > 1ull) || (sub[2] > 1ull) || (sub[3] > 1ull)))
    {
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            s_box[floors][axis] = sub[axis];
        }
        floors += 1u;
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            if (sub[axis] > 1ull)
            {
                lift_axis(data, stride, sub, axis);
            }
        }
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            sub[axis] = (sub[axis] + 1ull) / 2ull;
        }
    }
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        s_box[floors][axis] = 1ull;
    }
    for (unsigned long long i = 0ull; i < lanes; i += 1ull)
    {
        naturals[i] = zigzag(data[i]);
    }
    const unsigned long long base_bytes = (rice_bits(naturals, lanes) + 7ull) / 8ull;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "the residue as the .iapx writes it", 44u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_columns(output_line, base_bytes, 12u);
        mmgr_line_put(output_line, " bytes, ");
        mmgr_line_decimal_columns(output_line, 100ull * base_bytes / raw_bytes, 3u);
        mmgr_line_put(output_line, ".");
        mmgr_line_decimal(output_line, (1000ull * base_bytes / raw_bytes) % 10ull, 1u);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, " of raw");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_line_flushed();

    // the rectangles of every floor and the last, in a table at a bound: three to a floor and one more
    static Rectangle s_rectangles[(3u * SCAN_FLOORS) + 1u];
    unsigned int rectangle_count = 0u;
    for (unsigned int level = 0u; level < floors; level += 1u)
    {
        const unsigned long long ey = s_box[level][2], ex = s_box[level][3], iy = s_box[level + 1u][2],
                                 ix = s_box[level + 1u][3];
        const Rectangle pieces[3] = {{0ull, iy, ix, ex}, {iy, ey, 0ull, ix}, {iy, ey, ix, ex}};
        for (unsigned int each = 0u; each < 3u; each += 1u)
        {
            if ((pieces[each].y1 > pieces[each].y0) && (pieces[each].x1 > pieces[each].x0))
            {
                s_rectangles[rectangle_count] = pieces[each];
                rectangle_count += 1u;
            }
        }
    }
    s_rectangles[rectangle_count] = Rectangle{0ull, s_box[floors][2], 0ull, s_box[floors][3]};
    rectangle_count += 1u;
    // every rectangle's offsets one after another, the largest rectangle's area, and their sum
    unsigned long long area_most = 0ull;
    unsigned long long area_all = 0ull;
    for (unsigned int index = 0u; index < rectangle_count; index += 1u)
    {
        const unsigned long long area =
            (s_rectangles[index].y1 - s_rectangles[index].y0) * (s_rectangles[index].x1 - s_rectangles[index].x0);
        area_most = (area > area_most) ? area : area_most;
        area_all += area;
    }
    // the scan's offsets, a rectangle's places, the room their sorts write and their golden order, the stream and its
    // decode, cells of the temporary tier at the counts the rectangles fix, given back as the program ends
    unsigned long long *const offsets = (unsigned long long *)scan_cell(area_all, sizeof(unsigned long long));
    Place *const places = (Place *)scan_cell(area_most, sizeof(Place));
    Place *const places_room = (Place *)scan_cell(area_most, sizeof(Place));
    Place *const golden = (Place *)scan_cell(area_most, sizeof(Place));
    unsigned long long *const ranked = (unsigned long long *)scan_cell(area_most, sizeof(unsigned long long));
    unsigned long long *const ranked_room = (unsigned long long *)scan_cell(area_most, sizeof(unsigned long long));
    unsigned int *const stream = (unsigned int *)scan_cell(extent[0] * extent[1] * area_all, sizeof(unsigned int));
    Integer *const undone = (Integer *)scan_cell(lanes, sizeof(Integer));
    if ((offsets == NULL) || (places == NULL) || (places_room == NULL) || (golden == NULL) || (ranked == NULL) ||
        (ranked_room == NULL) || (stream == NULL) || (undone == NULL))
    {
        fprintf(stderr, "the pool could not meet the cells of the scans\n");
        return 1;
    }

    const char *const names[3] = {"each floor's rectangles row by row", "shells, each swept by angle",
                                  "shells, each in the golden order"};
    for (int order = 0; order < 3; order += 1)
    {
        unsigned long long written = 0ull;
        for (unsigned int index = 0u; index < rectangle_count; index += 1u)
        {
            const Rectangle &piece = s_rectangles[index];
            unsigned long long count = 0ull;
            for (unsigned long long y = piece.y0; y < piece.y1; y += 1ull)
            {
                for (unsigned long long x = piece.x0; x < piece.x1; x += 1ull)
                {
                    places[count] =
                        Place{(long long)(2ull * y) - (long long)(piece.y0 + piece.y1 - 1ull),
                              (long long)(2ull * x) - (long long)(piece.x0 + piece.x1 - 1ull), y * extent[3] + x};
                    count += 1ull;
                }
            }
            if (order == 1)
            {
                (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = places, .bytes = sizeof(Place), .scratch = places_room,
                                 .count = count, .order = scan_angle_order);
            }
            else if (order == 2)
            {
                (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = places, .bytes = sizeof(Place), .scratch = places_room,
                                 .count = count, .order = scan_shell_order);
                unsigned long long golden_count = 0ull;
                for (unsigned long long start = 0ull; start < count;)
                {
                    unsigned long long end = start;
                    while ((end < count) && (radius_of(places[end]) == radius_of(places[start])))
                    {
                        end += 1ull;
                    }
                    for (unsigned long long at = 0ull; at < (end - start); at += 1ull)
                    {
                        ranked[at] = at;
                    }
                    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = ranked, .bytes = sizeof(unsigned long long),
                                     .scratch = ranked_room, .count = end - start, .order = scan_golden_order);
                    for (unsigned long long at = 0ull; at < (end - start); at += 1ull)
                    {
                        golden[golden_count] = places[start + ranked[at]];
                        golden_count += 1ull;
                    }
                    start = end;
                }
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = places, .src = golden,
                           .bytes = (size_t)(count * sizeof(Place)));
            }
            for (unsigned long long at = 0ull; at < count; at += 1ull)
            {
                offsets[written] = places[at].offset;
                written += 1ull;
            }
        }
        unsigned long long streamed = 0ull;
        for (unsigned long long plane = 0ull; plane < extent[0] * extent[1]; plane += 1ull)
        {
            for (unsigned long long at = 0ull; at < written; at += 1ull)
            {
                stream[streamed] = naturals[plane * stride[1] + offsets[at]];
                streamed += 1ull;
            }
        }
        const unsigned long long bytes = (rice_bits(stream, streamed) + 7ull) / 8ull;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = undone, .bytes = (size_t)(lanes * sizeof(Integer)), .val = 0);
        unsigned long long cursor = 0ull;
        for (unsigned long long plane = 0ull; plane < extent[0] * extent[1]; plane += 1ull)
        {
            for (unsigned long long at = 0ull; at < written; at += 1ull)
            {
                undone[plane * stride[1] + offsets[at]] = (Integer)unzigzag(stream[cursor]);
                cursor += 1ull;
            }
        }
        unsigned long long differ = (cursor == lanes) ? 0ull : 1ull;
        for (unsigned long long i = 0ull; i < lanes; i += 1ull)
        {
            differ += (undone[i] != data[i]) ? 1ull : 0ull;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, names[order], 44u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, bytes, 12u);
            mmgr_line_put(output_line, " bytes, ");
            mmgr_line_decimal_columns(output_line, 100ull * bytes / raw_bytes, 3u);
            mmgr_line_put(output_line, ".");
            mmgr_line_decimal(output_line, (1000ull * bytes / raw_bytes) % 10ull, 1u);
            mmgr_line_ch(output_line, '%');
            mmgr_line_put(output_line, " of raw, ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)((long long)bytes - (long long)base_bytes);
                if (linea_value >= 0)
                {
                    mmgr_line_ch(&linea_piece_line, '+');
                }
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_right(output_line, linea_piece, 0u);
            }
            mmgr_line_put(output_line, " bytes; decoded back: ");
            mmgr_line_decimal(output_line, differ, 1u);
            mmgr_line_put(output_line, " differ");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
    }
    void *const cells[11] = {offsets, places, places_room, golden, ranked,  ranked_room,
                             stream,  undone, raw,         data,   naturals};
    for (unsigned int each = 0u; each < 11u; each += 1u)
    {
        platform_cell_released(cells[each], platform_place_g_program_owner);
    }
    return 0;
}
