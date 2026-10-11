// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file casmi_config.h
 * @brief The one configuration every CASMI module reaches: bounds, gates, entry shape and shared types.
 *
 * The byte decoders are orior's own, src/cu/includes/codecs, and their request type comes through
 * engine_config.h.
 */
#ifndef CASMI_CONFIG_H
#define CASMI_CONFIG_H

#include "engine_config.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The most leaf columns a parquet schema may carry. The competition files carry 12 and 18. */
#define CASMI_PARQUET_LEAVES_MOST 64u

/** The most row groups a parquet file may carry. The training file carries 21. */
#define CASMI_PARQUET_ROW_GROUPS_MOST 256u

/** The longest dotted column path, terminator included: "ms2_normalized_intensities.list.element". */
#define CASMI_PARQUET_PATH_BYTES 128u

/** The most bytes a parquet footer may occupy. The training file's is 50,891. */
#define CASMI_PARQUET_FOOTER_BYTES_MOST (16ull * 1024ull * 1024ull)

/** The deepest a Thrift value may nest before the footer is refused and not walked. */
#define CASMI_THRIFT_DEPTH_MOST 16u

/** The deepest a parquet schema may nest. A list column is three deep: field, list, element. */
#define CASMI_PARQUET_SCHEMA_DEPTH_MOST 16u

/**
 * The most steps one CASMI record program may hold, the size of each program builder's step array.
 * The engine caps no step count: its binding resource is ENGINE_RECORD_LIMBS_MAX limbs of live
 * registers, and its imprint errors on a program past them.
 */
#define CASMI_RECORD_STEPS_MOST 1024u

static_assert(CASMI_PARQUET_PATH_BYTES >= 64u, "CASMI_PARQUET_PATH_BYTES: the list columns' dotted paths need 41 bytes");

// Every byte count here is an unsigned long long and passes to memcpy, fread and malloc as a size_t
// with no cast; the two must be one width.
static_assert(sizeof(size_t) == sizeof(unsigned long long), "size_t: expected 64 bits, the width of every CASMI byte count");

// __has_attribute is itself a compiler feature; it is tested once here and nowhere else.
#if defined(__has_attribute)
#define CASMI_HAS_ATTRIBUTE(attribute_) __has_attribute(attribute_)
#else
#define CASMI_HAS_ATTRIBUTE(attribute_) 0
#endif

#if CASMI_HAS_ATTRIBUTE(always_inline)
#define CASMI_ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define CASMI_ALWAYS_INLINE static inline
#endif

// A file past 2 GiB needs a 64-bit seek. Windows keeps long at 32 bits and names the 64-bit seek
// _fseeki64; every other target this builds for is LP64, where fseek's long is already 64 bits.
#if defined(_WIN32)
#define CASMI_HAVE_WINDOWS_SEEK 1
#else
#define CASMI_HAVE_WINDOWS_SEEK 0
static_assert(LONG_MAX == LLONG_MAX, "long: expected 64 bits, so fseek reaches every byte of a large parquet file");
#endif

/**
 * Call a backend with its argument struct built at the call as a compound literal.
 *
 * Every unnamed member is zero and each argument is evaluated once.
 */
#define CASMI_CALL(backend_, arguments_, ...) backend_(&(arguments_){__VA_ARGS__})

/**
 * Emit a public entry that takes one const argument struct and hands it to its file-scope backend.
 * The entry tests nothing; every check belongs to the backend.
 */
#define CASMI_ENTRY(entry_, arguments_, backend_) \
    long long entry_(const arguments_ *args)      \
    {                                             \
        return backend_(args);                    \
    }

#ifdef __cplusplus
}
#endif

#endif
