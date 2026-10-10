// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// parquet_thrift.c: the Thrift compact protocol the footer and every page header are written in, and growing bytes
#include "parquet_internal.h"

// the longest a varint may run: ten groups of seven bits cover sixty-four, and the last group's bits that land
// inside sixty-four, 64 - 7 x 9 = 1
#define PARQUET_VARINT_BYTES_MOST 10u
#define PARQUET_VARINT_LAST_GROUP_BITS (64u - (7u * (PARQUET_VARINT_BYTES_MOST - 1u)))

_Static_assert((7u * PARQUET_VARINT_BYTES_MOST) >= 64u,
               "PARQUET_VARINT_BYTES_MOST: its seven-bit groups must cover a 64-bit value");
_Static_assert((7u * (PARQUET_VARINT_BYTES_MOST - 1u)) < 64u,
               "PARQUET_VARINT_BYTES_MOST: the last group's shift must stay below the 64-bit width");

// a Thrift field id is an i16
#define PARQUET_FIELD_ID_LEAST (-32768ll)
#define PARQUET_FIELD_ID_MOST 32767ll

// one open container of a Thrift value being skipped: a struct's fields, or a list's or map's values
typedef struct
{
    unsigned int container_kind;
    unsigned long long values_left;
    unsigned int key_kind;
    unsigned int value_kind;
    long long field_id;
} ParquetThriftFrame;

unsigned long long parquet_little(const unsigned char *bytes, unsigned int count)
{
    unsigned long long value = 0ull;
    for (unsigned int byte = 0u; byte < count; byte += 1u)
    {
        value |= (unsigned long long)bytes[byte] << (8u * byte);
    }
    return value;
}

unsigned int parquet_byte_read(ParquetCursor *cursor)
{
    if ((cursor->broken != 0u) || (cursor->position >= cursor->byte_count))
    {
        cursor->broken = 1u;
        return 0u;
    }
    const unsigned int byte = cursor->bytes[cursor->position];
    cursor->position += 1ull;
    return byte;
}

// an unsigned LEB128 varint of at most sixty-four bits. One that runs past PARQUET_VARINT_BYTES_MOST bytes, or whose
// last group carries a bit at or past PARQUET_VARINT_LAST_GROUP_BITS, encodes more than 2^64 - 1 and breaks the
// cursor
unsigned long long parquet_varint_read(ParquetCursor *cursor)
{
    unsigned long long value = 0ull;
    for (unsigned int group = 0u; group < PARQUET_VARINT_BYTES_MOST; group += 1u)
    {
        const unsigned int byte = parquet_byte_read(cursor);
        const unsigned int group_bits = byte & 0x7Fu;
        if ((group == (PARQUET_VARINT_BYTES_MOST - 1u)) && ((group_bits >> PARQUET_VARINT_LAST_GROUP_BITS) != 0u))
        {
            cursor->broken = 1u;
            return 0ull;
        }
        // widened to 64 bits before the shift, which reaches 63, and the last group was held to the bits below 64
        value |= (unsigned long long)group_bits << (7u * group);
        if ((byte & 0x80u) == 0u)
        {
            return value;
        }
    }
    cursor->broken = 1u;
    return 0ull;
}

// a zigzag varint: the signed form every Thrift compact integer takes
static long long parquet_zigzag_read(ParquetCursor *cursor)
{
    const unsigned long long stored = parquet_varint_read(cursor);
    // the bits past the sign bit are below 2^63 and convert exactly; the low bit alone decides the sign, and set,
    // the value is -value_bits - 1, which reaches -2^63 at most
    const long long value_bits = (long long)(stored >> 1u);
    return ((stored & 1ull) != 0ull) ? (-value_bits - 1ll) : value_bits;
}

// a zigzag integer a count, size or offset field holds, refused where negative
unsigned long long parquet_nonnegative_read(ParquetCursor *cursor)
{
    const long long value = parquet_zigzag_read(cursor);
    if (value < 0ll)
    {
        cursor->broken = 1u;
        return 0ull;
    }
    // negative values were refused above, and the conversion is exact
    return (unsigned long long)value;
}

// a length-prefixed byte string, returned in place with its length through *length
const unsigned char *parquet_binary_read(ParquetCursor *cursor, unsigned long long *length)
{
    *length = parquet_varint_read(cursor);
    if ((cursor->broken != 0u) || (*length > (cursor->byte_count - cursor->position)))
    {
        cursor->broken = 1u;
        *length = 0ull;
        return cursor->bytes;
    }
    const unsigned char *const start = cursor->bytes + cursor->position;
    cursor->position += *length;
    return start;
}

// a list or set header: the element count, with the element type through *element_kind
unsigned long long parquet_list_header_read(ParquetCursor *cursor, unsigned int *element_kind)
{
    const unsigned int header = parquet_byte_read(cursor);
    *element_kind = header & 0x0Fu;
    const unsigned long long short_count = header >> 4u;
    return (short_count == 15ull) ? parquet_varint_read(cursor) : short_count;
}

// the next field of a struct: its id through *field_id and its type as the return, where PARQUET_THRIFT_STOP ends the
// struct. A short header carries the id as a delta from the last and a long one carries it whole. An id outside an
// i16 breaks the cursor, leaves *field_id as it was and ends the struct
unsigned int parquet_field_read(ParquetCursor *cursor, long long *field_id)
{
    const unsigned int header = parquet_byte_read(cursor);
    if (header == PARQUET_THRIFT_STOP)
    {
        return PARQUET_THRIFT_STOP;
    }
    const unsigned int delta = header >> 4u;
    // a delta is at most 15 and widens exactly; every caller starts *field_id at zero and only this function writes
    // it; the last id is an i16 and the sum does not overflow
    const long long next_id = (delta == 0u) ? parquet_zigzag_read(cursor) : (*field_id + (long long)delta);
    if ((next_id < PARQUET_FIELD_ID_LEAST) || (next_id > PARQUET_FIELD_ID_MOST))
    {
        cursor->broken = 1u;
        return PARQUET_THRIFT_STOP;
    }
    *field_id = next_id;
    return header & 0x0Fu;
}

// skip one value that holds no other value. A kind that is not a scalar breaks the cursor
static void parquet_scalar_skip(ParquetCursor *cursor, unsigned int kind)
{
    switch (kind)
    {
    case PARQUET_THRIFT_TRUE:
    case PARQUET_THRIFT_FALSE:
        // a struct field's boolean is carried in its type code and takes no byte
        break;
    case PARQUET_THRIFT_BYTE:
        parquet_byte_read(cursor);
        break;
    case PARQUET_THRIFT_I16:
    case PARQUET_THRIFT_I32:
    case PARQUET_THRIFT_I64:
        parquet_varint_read(cursor);
        break;
    case PARQUET_THRIFT_DOUBLE:
        cursor->broken |= ((cursor->byte_count - cursor->position) < 8ull) ? 1u : 0u;
        cursor->position += (cursor->broken != 0u) ? 0ull : 8ull;
        break;
    case PARQUET_THRIFT_BINARY: {
        unsigned long long length = 0ull;
        parquet_binary_read(cursor, &length);
        break;
    }
    default:
        cursor->broken = 1u;
        break;
    }
}

// skip one value of any kind, nested containers included, without recursion: each open struct, list or map is a
// frame, and a value nested deeper than PARQUET_THRIFT_DEPTH_MOST breaks the cursor
void parquet_value_skip(ParquetCursor *cursor, unsigned int kind)
{
    ParquetThriftFrame frame[PARQUET_THRIFT_DEPTH_MOST];
    unsigned int depth = 0u;
    unsigned int next_kind = kind;
    while (cursor->broken == 0u)
    {
        const int opens_container = (next_kind == PARQUET_THRIFT_STRUCT) || (next_kind == PARQUET_THRIFT_LIST) ||
                                    (next_kind == PARQUET_THRIFT_SET) || (next_kind == PARQUET_THRIFT_MAP);
        if (!opens_container)
        {
            parquet_scalar_skip(cursor, next_kind);
        }
        else if (depth == PARQUET_THRIFT_DEPTH_MOST)
        {
            cursor->broken = 1u;
            return;
        }
        else
        {
            ParquetThriftFrame *const opened = &frame[depth];
            opened->container_kind = (next_kind == PARQUET_THRIFT_SET) ? PARQUET_THRIFT_LIST : next_kind;
            opened->values_left = 0ull;
            opened->key_kind = 0u;
            opened->value_kind = 0u;
            opened->field_id = 0ll;
            if (opened->container_kind == PARQUET_THRIFT_LIST)
            {
                opened->values_left = parquet_list_header_read(cursor, &opened->value_kind);
                opened->key_kind = opened->value_kind;
            }
            else if (opened->container_kind == PARQUET_THRIFT_MAP)
            {
                const unsigned long long entry_count = parquet_varint_read(cursor);
                const unsigned int map_kinds = (entry_count != 0ull) ? parquet_byte_read(cursor) : 0u;
                // every entry takes at least one byte: more entries than bytes left is refused, and the doubling
                // that counts keys and values apart does not wrap
                cursor->broken |= (entry_count > (cursor->byte_count - cursor->position)) ? 1u : 0u;
                opened->values_left = (cursor->broken != 0u) ? 0ull : (2ull * entry_count);
                opened->key_kind = map_kinds >> 4u;
                opened->value_kind = map_kinds & 0x0Fu;
            }
            depth += 1u;
        }
        // the next value to skip is the innermost open container's next one; each container that has none left
        // closes, and when none is open the whole value is skipped
        next_kind = PARQUET_THRIFT_STOP;
        while ((depth != 0u) && (next_kind == PARQUET_THRIFT_STOP) && (cursor->broken == 0u))
        {
            ParquetThriftFrame *const innermost = &frame[depth - 1u];
            if (innermost->container_kind == PARQUET_THRIFT_STRUCT)
            {
                next_kind = parquet_field_read(cursor, &innermost->field_id);
            }
            else if (innermost->values_left != 0ull)
            {
                innermost->values_left -= 1ull;
                // a map counts down from an even total, and an odd count left means a key comes next
                next_kind = ((innermost->values_left & 1ull) != 0ull) ? innermost->key_kind : innermost->value_kind;
                // a boolean inside a list or map is one byte, not a type code carrying the value
                next_kind = ((next_kind == PARQUET_THRIFT_TRUE) || (next_kind == PARQUET_THRIFT_FALSE))
                                ? PARQUET_THRIFT_BYTE
                                : next_kind;
                cursor->broken |= (next_kind == PARQUET_THRIFT_STOP) ? 1u : 0u;
            }
            if ((next_kind == PARQUET_THRIFT_STOP) && (cursor->broken == 0u))
            {
                depth -= 1u;
            }
        }
        if (next_kind == PARQUET_THRIFT_STOP)
        {
            return;
        }
    }
}

// `count` bytes from `from` after the ones held, the room doubled until they fit. 0 where the room does not grow
int parquet_bytes_append(ParquetBytes *bytes, const unsigned char *from, unsigned long long count)
{
    if (count > (bytes->room - bytes->count))
    {
        unsigned long long room = (bytes->room != 0ull) ? bytes->room : 64ull;
        while (count > (room - bytes->count))
        {
            room *= 2ull;
        }
        unsigned char *const grown = (unsigned char *)realloc(bytes->bytes, (size_t)room);
        if (grown == NULL)
        {
            return 0;
        }
        bytes->bytes = grown;
        bytes->room = room;
    }
    if (count != 0ull)
    {
        memcpy(bytes->bytes + bytes->count, from, (size_t)count);
    }
    bytes->count += count;
    return 1;
}
