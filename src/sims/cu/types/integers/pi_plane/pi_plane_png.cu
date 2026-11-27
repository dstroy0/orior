// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_plane_png.cu: PNG pictures
#include "../../../../../../includes/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "pi_plane_internal.h"

// the bytes of a PNG file as the calls below write it: its place in the file's cell and the bytes there
typedef struct
{
    unsigned char *byte;
    size_t count;
} PlaneFile;

// each byte is a step of the CRC
static void plane_crc(unsigned int *crc, const unsigned char *bytes, size_t count)
{
    for (size_t byte = 0u; byte < count; byte += 1u)
    {
        *crc ^= bytes[byte];
        for (unsigned int bit = 0u; bit < 8u; bit += 1u)
        {
            *crc = (*crc >> 1u) ^ (0xEDB88320u & (0u - (*crc & 1u)));
        }
    }
}

static void plane_put_byte(PlaneFile *out, unsigned char byte)
{
    out->byte[out->count] = byte;
    out->count += 1u;
}

static void plane_put_bytes(PlaneFile *out, const unsigned char *bytes, size_t count)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &out->byte[out->count], .src = bytes, .bytes = count);
    out->count += count;
}

// big endian, as PNG writes every word
static void plane_put_word(PlaneFile *out, unsigned int word)
{
    for (unsigned int byte = 0u; byte < 4u; byte += 1u)
    {
        // one byte of the word, taken from the bottom after the shift
        plane_put_byte(out, (unsigned char)(word >> (24u - (8u * byte))));
    }
}

// A chunk: its length, its type, its data and the CRC of the type and the data. The `data` bytes of the data already
// lie in the file past the length and the type. The call writes the length and the type before them
static void plane_chunk(PlaneFile *out, size_t start, const char *type, size_t data)
{
    PlaneFile head = {&out->byte[start], 0u};
    // a chunk contains at max one picture, far below 2^31 bytes
    plane_put_word(&head, (unsigned int)data);
    // the chunk type is four ASCII characters
    plane_put_bytes(&head, (const unsigned char *)type, 4u);
    out->count = start + 8u + data;
    unsigned int crc = 0xFFFFFFFFu;
    plane_crc(&crc, &out->byte[start + 4u], 4u + data);
    plane_put_word(out, crc ^ 0xFFFFFFFFu);
}

// An 8-bit PNG, gray for one channel and color for three, its image data in stored deflate blocks. The call encodes
// nothing. The file goes in one cell of the temporary tier at its size, which the extents give
int plane_png(const char *path, const unsigned char *pixels, unsigned long long width, unsigned long long height,
              unsigned int channels)
{
    const unsigned long long stride = width * channels;
    const size_t raw = (size_t)(height * (stride + 1ULL));
    const size_t blocks = (raw == 0u) ? 1u : ((raw + 65534u) / 65535u);
    const size_t stream = 2u + (5u * blocks) + raw + 4u;
    const size_t bytes = 8u + (12u + 13u) + (12u + stream) + 12u;
    PlaneFile file = {(unsigned char *)plane_taken(bytes, 1u, 1), 0u};
    static const unsigned char s_signature[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
    plane_put_bytes(&file, s_signature, sizeof(s_signature));
    const size_t header_start = file.count;
    file.count += 8u;
    // the extents are below 2^16. They fit the header's words
    plane_put_word(&file, (unsigned int)width);
    plane_put_word(&file, (unsigned int)height);
    plane_put_byte(&file, 8u);
    plane_put_byte(&file, (channels == 3u) ? 2u : 0u);
    plane_put_byte(&file, 0u);
    plane_put_byte(&file, 0u);
    plane_put_byte(&file, 0u);
    plane_chunk(&file, header_start, "IHDR", 13u);
    const size_t data_start = file.count;
    file.count += 8u;
    plane_put_byte(&file, 0x78u);
    plane_put_byte(&file, 0x01u);
    // the raw rows, each after its filter byte 0, cut into stored blocks of at max 65535 bytes
    size_t at = 0u;
    unsigned int low = 1u;
    unsigned int high = 0u;
    do
    {
        const size_t take = ((raw - at) < 65535u) ? (raw - at) : 65535u;
        plane_put_byte(&file, (at + take == raw) ? 1u : 0u);
        // a stored block's length and its complement, little endian
        plane_put_byte(&file, (unsigned char)(take & 0xFFu));
        plane_put_byte(&file, (unsigned char)(take >> 8u));
        plane_put_byte(&file, (unsigned char)(~take & 0xFFu));
        plane_put_byte(&file, (unsigned char)((~take >> 8u) & 0xFFu));
        // each raw byte is a step of the block and of the Adler sum
        for (size_t place = at; place < at + take; place += 1u)
        {
            const size_t column = place % (size_t)(stride + 1ULL);
            const size_t row = place / (size_t)(stride + 1ULL);
            const unsigned char byte = (column == 0u) ? 0u : pixels[(row * (size_t)stride) + column - 1u];
            plane_put_byte(&file, byte);
            low = (low + byte) % 65521u;
            high = (high + low) % 65521u;
        }
        at += take;
    } while (at < raw);
    plane_put_word(&file, (high << 16u) | low);
    plane_chunk(&file, data_start, "IDAT", stream);
    const size_t end_start = file.count;
    file.count += 8u;
    plane_chunk(&file, end_start, "IEND", 0u);
    const int written = platform_file_written(path, file.byte, (unsigned long long)file.count);
    plane_given(file.byte);
    return written;
}

// the shape drawn with each bit a square of pixels, a one white, a zero black, and no bit gray
int plane_picture(const PlaneExtent &extent, const unsigned char *bits, const char *path)
{
    const unsigned long long longer = (extent.height > extent.width) ? extent.height : extent.width;
    const unsigned long long fit = PLANE_PICTURE_MAX / longer;
    const unsigned long long scale = (fit > 1ULL) ? fit : 1ULL;
    const unsigned long long width = extent.width * scale;
    const unsigned long long height = extent.height * scale;
    unsigned char *const pixels = (unsigned char *)plane_taken(width * height, 1u, 1);
    for (unsigned long long row = 0ULL; row < height; row += 1ULL)
    {
        for (unsigned long long column = 0ULL; column < width; column += 1ULL)
        {
            const long long cell = extent.cell[(size_t)(((row / scale) * extent.width) + (column / scale))];
            const unsigned char shade = (cell < 0LL) ? (unsigned char)PLANE_EMPTY_SHADE
                                                     : ((bits[cell] != 0u) ? (unsigned char)255u : (unsigned char)0u);
            pixels[(size_t)((row * width) + column)] = shade;
        }
    }
    const int written = plane_png(path, pixels, width, height, 1u);
    plane_given(pixels);
    return written;
}

// The shape drawn sparse: each bit a dot with a gap around it, and only the ones lit. The line's bits show in red
// whatever their value. The pixels no bit lands on are a little lighter than the ground
int plane_sparse_picture(const PlaneExtent &extent, const unsigned char *bits, const PlaneLine &found, const char *path)
{
    const unsigned long long cell = 7ULL;
    const unsigned long long dot = 5ULL;
    unsigned char *const marked = (unsigned char *)plane_taken(extent.cell_count, 1u, 1);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = marked, .bytes = extent.cell_count, .val = 0u);
    // the extents are below 2^16. They fit a signed word
    const long long height = (long long)extent.height;
    const long long width = (long long)extent.width;
    for (long long index = 0LL; index < height * width; index += 1LL)
    {
        if (extent.cell[(size_t)index] != found.start)
        {
            continue;
        }
        for (unsigned long long walked = 0ULL; walked < found.length; walked += 1ULL)
        {
            // the walk is at max the line's length, below 2^15
            const long long row = (index / width) + ((long long)walked * found.step.row);
            const long long column = (index % width) + ((long long)walked * found.step.column);
            if ((row >= 0LL) && (row < height) && (column >= 0LL) && (column < width))
            {
                // the row and column are inside the shape. The index is never negative
                marked[(size_t)((row * width) + column)] = 1u;
            }
        }
    }
    const unsigned long long picture_width = extent.width * cell;
    const unsigned long long picture_height = extent.height * cell;
    unsigned char *const pixels = (unsigned char *)plane_taken(picture_width * picture_height * 3ULL, 1u, 1);
    for (unsigned long long row = 0ULL; row < picture_height; row += 1ULL)
    {
        for (unsigned long long column = 0ULL; column < picture_width; column += 1ULL)
        {
            const size_t index = (size_t)(((row / cell) * extent.width) + (column / cell));
            const unsigned long long inside_row = row % cell;
            const unsigned long long inside_column = column % cell;
            const int in_dot =
                (inside_row >= 1ULL) && (inside_row <= dot) && (inside_column >= 1ULL) && (inside_column <= dot);
            unsigned char red = 0u;
            unsigned char green = 0u;
            unsigned char blue = 0u;
            if (extent.cell[index] < 0LL)
            {
                red = 24u;
                green = 24u;
                blue = 24u;
            }
            else if (in_dot && (marked[index] != 0u))
            {
                red = 255u;
                green = (bits[extent.cell[index]] != 0u) ? 96u : 0u;
                blue = green;
            }
            else if (in_dot && (bits[extent.cell[index]] != 0u))
            {
                red = 255u;
                green = 255u;
                blue = 255u;
            }
            const size_t at = (size_t)(((row * picture_width) + column) * 3ULL);
            pixels[at] = red;
            pixels[at + 1u] = green;
            pixels[at + 2u] = blue;
        }
    }
    const int written = plane_png(path, pixels, picture_width, picture_height, 3u);
    plane_given(pixels);
    plane_given(marked);
    return written;
}
