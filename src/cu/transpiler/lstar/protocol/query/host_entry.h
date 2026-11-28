// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef HOST_ENTRY_H
#define HOST_ENTRY_H

// How the program reaches a host. Two relations.
//
//   put   a word at an address
//   read  a word at an address
//
// A store and a load. Nothing is under them, and nothing is between them and the part. The program asks no system for
// permission, calls no library, and includes no header of another party. This file includes nothing at all. A
// compiler that must get an address and a word can get them anywhere.
//
// A bus is any bus because a bus is these two. A window at an address is one way to write them, and a serial line that
// clocks bits is another. The aperture of a fabric is a third. Each is a few stores and loads at the addresses that
// drive it, and the program finds each as it finds all else here.
//
// What is on the bus is not a third relation. It is these two, put to the own addresses of the bus. The program reads
// the word that a part answers with, and reads nothing where no part answers. A query to a system about what it
// carries gets the opinion of that system. That puts a third party in the loop, and its opinion is not the answer of
// the part. The bus reports what is on it because the program queries the bus.
//
// An entry is not a translator. A thing that turns a source into a coding is the part of the loop that the program
// derives. A word that went through one is the answer of that translator, and not the answer of the host.

// how wide a word is here. Every address is one word apart from the next. How many places a word truly holds is a
// thing that the program asks the part, and not a thing that this file decides
#define HOST_WORD 4u

// A read that comes back all ones is what a line gives a reader when nothing drove it: every wire floats and reads
// high. It is not a word a part reported
#define HOST_NO_ANSWER 0xffffffffu

// the two words that the program puts to an address. They share no bits. Thus an address that always reads one of
// them shows apart from an address that keeps what it gets
#define HOST_PATTERN_FIRST 0xa5a5a5a5u
#define HOST_PATTERN_THEN 0x5a5a5a5au

// what an address turned out to be, once put words and read back
typedef enum
{
    // gave back every word that it got
    HOST_RETAINS,
    // gave back the same word, whatever it got
    HOST_FIXED,
    // gave back something else: something behind it decided the answer
    HOST_LIVE,
    // gave back nothing a reader here can distinguish from nothing
    HOST_NOTHING
} HostAddressType;

// `word` put at `at`
static inline void host_put(unsigned long long at, unsigned int word)
{
    *(volatile unsigned int *)(unsigned long long)at = word;
}

// the word at `at`
static inline unsigned int host_read(unsigned long long at)
{
    return *(volatile unsigned int *)(unsigned long long)at;
}

// the type that two read-backs name: `first`, read after the put of HOST_PATTERN_FIRST, and `then`, after the put of
// HOST_PATTERN_THEN. This call reads no address, and takes the two words that the caller already read. A caller can
// give it the own values of a sizing register with no bus
static inline unsigned int host_address_classify(unsigned int first, unsigned int then)
{
    if ((first == HOST_PATTERN_FIRST) && (then == HOST_PATTERN_THEN))
    {
        return HOST_RETAINS;
    }
    if ((first == HOST_NO_ANSWER) && (then == HOST_NO_ANSWER))
    {
        return HOST_NOTHING;
    }
    if (first == then)
    {
        return HOST_FIXED;
    }
    // it moved, and not to what it got: a thing behind the address decided the answer
    return HOST_LIVE;
}

// What the address at `at` is, through `read_back`, which holds what came back last. The call puts back what was there
// first before it returns. An address that keeps a value can be a thing that the part uses
static inline unsigned int host_address_probe(unsigned long long at, unsigned int *read_back)
{
    const unsigned int saved = host_read(at);
    host_put(at, HOST_PATTERN_FIRST);
    const unsigned int first = host_read(at);
    host_put(at, HOST_PATTERN_THEN);
    const unsigned int then = host_read(at);
    host_put(at, saved);
    *read_back = then;
    return host_address_classify(first, then);
}

// the address bits that a sizing register decodes, from the word that it gives back after a put of all ones. That word
// holds the high bits that the register owns, and the register forces the low bits that it decodes to zero. The count
// of those low zero bits is the width of the region. A readback of all ones decodes nothing and has width 0. A
// readback of zero decodes every bit
static inline unsigned int host_width(unsigned int readback)
{
    unsigned int width = 0u;
    while ((width < 32u) && (((readback >> width) & 1u) == 0u))
    {
        width += 1u;
    }
    return width;
}

// the width that the sizing register at `at` names. The call puts all ones to it, reads the word that it gives back,
// and puts back what was there. Memory reads 0 here, because memory gives back the ones that it got. A sizing register
// reads the width of the region
static inline unsigned int host_width_probe(unsigned long long at)
{
    const unsigned int saved = host_read(at);
    host_put(at, HOST_NO_ANSWER);
    const unsigned int readback = host_read(at);
    host_put(at, saved);
    return host_width(readback);
}

// whether a part answers at `at`: a word comes back that is not the word a line gives when nothing drove it
static inline int host_answers(unsigned long long at)
{
    return (host_read(at) != HOST_NO_ANSWER) ? 1 : 0;
}

#endif
