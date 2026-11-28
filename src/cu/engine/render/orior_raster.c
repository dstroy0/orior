// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_raster.c: the raster on the host
#include "orior_raster_internal.h"
#include "../../../../includes/platform/platform.h"

/**
 * @brief Gray value for an alignment that died at `level`, or passed every probe.
 *
 * @param[in] level       The probe index that rejected the alignment, or `probe_count` where none did.
 * @param[in] probe_count How many probes the call evaluated.
 * @param[in] matched     Not zero where the full compare confirmed an occurrence.
 * @return                The pixel value.
 *
 * @note Exact integer arithmetic. The host and device ramps cannot drift apart by a rounding.
 */
static uint8_t raster_value(size_t level, size_t probe_count, int matched)
{
    if (matched != 0)
    {
        return (uint8_t)ORIOR_RASTER_MATCH;
    }
    (void)probe_count;

    const size_t scaled = 1u + (level * ORIOR_RASTER_STEP);
    return (uint8_t)((scaled > ORIOR_RASTER_CEILING) ? ORIOR_RASTER_CEILING : scaled);
}

/**
 * @brief The level at which one alignment died, and whether it matched.
 *
 * @param[out] matched Set non-zero where the full compare confirmed an occurrence [BORROWS].
 * @return             Probe index that rejected it, or `probe_count` where none did.
 */
static size_t raster_death_level(const uint8_t *corpus, const uint8_t *needle, size_t needle_len,
                                 const OriorRasterProbe *probes, size_t probe_count, size_t at, int *matched)
{
    *matched = 0;

    for (size_t index = 0u; index < probe_count; index += 1u)
    {
        for (size_t step = 0u; step < probes[index].length; step += 1u)
        {
            const size_t offset = probes[index].origin + (step * probes[index].step);
            if (corpus[at + offset] != needle[offset])
            {
                return index;
            }
        }
    }

    /* Survived every probe. The full compare decides an occurrence, and a survivor that fails it
     * is a false positive that the probe set could not refute cheaply. Both outcomes are worth a
     * look, and they take different values. */
    for (size_t step = 0u; step < needle_len; step += 1u)
    {
        if (corpus[at + step] != needle[step])
        {
            return probe_count;
        }
    }
    *matched = 1;
    return probe_count;
}

/** @brief Counts what the object under examination holds. */
void raster_census(OriorRasterCensus *census, const uint8_t *corpus, size_t corpus_len)
{
    for (size_t symbol = 0u; symbol < 256u; symbol += 1u)
    {
        census->occurrences[symbol] = 0u;
    }
    for (size_t at = 0u; at < corpus_len; at += 1u)
    {
        census->occurrences[corpus[at]] += 1u;
    }
    census->total = (uint64_t)corpus_len;
}

size_t orior_raster_cell(const OriorRasterConfig *config, size_t at, size_t alignments)
{
    const size_t cells = config->width * config->height;

    /* Corpus order maps onto a linear cell index by integer division first. Every layout below is
     * then a permutation of that index. That keeps each transform a bijection, and keeps the count of
     * alignments that reach a cell independent of the layout chosen. */
    const size_t linear = (at * cells) / alignments;
    const size_t row = linear / config->width;
    const size_t column = linear % config->width;

    switch (config->layout)
    {
    case ORIOR_LAYOUT_SERPENTINE: {
        const size_t flipped = ((row % 2u) == 0u) ? column : (config->width - 1u - column);
        return (row * config->width) + flipped;
    }
    case ORIOR_LAYOUT_COLUMNS: {
        /* Transposed through the shorter side so the index stays inside the raster on a
         * rectangle. Reading down a column puts corpus neighbors a row apart. */
        const size_t turned_row = linear % config->height;
        const size_t turned_column = linear / config->height;
        if (turned_column >= config->width)
        {
            return linear;
        }
        return (turned_row * config->width) + turned_column;
    }
    case ORIOR_LAYOUT_DIAGONAL: {
        const size_t shifted = (column + row) % config->width;
        return (row * config->width) + shifted;
    }
    case ORIOR_LAYOUT_ROWS:
    default: {
        return linear;
    }
    }
}

uint8_t orior_raster_sample(const OriorRasterConfig *config, const uint8_t *corpus, const uint8_t *needle,
                            size_t needle_len, const OriorRasterProbe *probes, size_t probe_count, size_t at,
                            const uint64_t *occurrences, uint64_t total)
{
    const uint8_t gain = (config->gain == 0u) ? 1u : config->gain;

    switch (config->channel)
    {
    case ORIOR_CHANNEL_BYTE: {
        return corpus[at];
    }
    case ORIOR_CHANNEL_RARITY: {
        /* Rarity as the steering term defines it, scaled into eight bits by exact integer
         * division. A symbol the field never produces reaches the top of the ramp. */
        if (total == 0u)
        {
            return 1u;
        }
        const uint64_t missing = total - occurrences[corpus[at]];
        const uint64_t scaled = (missing * 254u) / total;
        return (uint8_t)(1u + scaled);
    }
    case ORIOR_CHANNEL_PROVEN: {
        /* A refuted alignment holds no occurrence, by proof. A survivor has no decision yet: the
         * probes could not refute it, and only the full compare decides. Proven takes the higher
         * value, so that a minimum reduction behaves as the conjunction that this channel needs. A
         * cell stays proven only while the probes refuted every alignment under it. */
        int matched = 0;
        const size_t level = raster_death_level(corpus, needle, needle_len, probes, probe_count, at, &matched);
        return (level < probe_count) ? (uint8_t)ORIOR_RASTER_PROVEN : (uint8_t)ORIOR_RASTER_UNDETERMINED;
    }
    case ORIOR_CHANNEL_SURVIVED: {
        int matched = 0;
        const size_t level = raster_death_level(corpus, needle, needle_len, probes, probe_count, at, &matched);
        return (level >= probe_count) ? (uint8_t)ORIOR_RASTER_MATCH : 1u;
    }
    case ORIOR_CHANNEL_DEATH_LEVEL:
    default: {
        int matched = 0;
        const size_t level = raster_death_level(corpus, needle, needle_len, probes, probe_count, at, &matched);
        return raster_value(level * (size_t)gain, probe_count, matched);
    }
    }
}

void raster_reduce(uint8_t *values, uint8_t *filled, size_t cell, uint8_t value, OriorRasterReduce reduce)
{
    // The first arrival in a cell fills it, and every later arrival meets the reduction. Zero is a
    // value that a cell holds, and not the mark of an empty one. A zero byte under ORIOR_REDUCE_MIN
    // stays, as the identity seed of the device keeps it
    if (filled[cell] == 0u)
    {
        filled[cell] = 1u;
        values[cell] = value;
        return;
    }
    if (reduce == ORIOR_REDUCE_MAX)
    {
        if (value > values[cell])
        {
            values[cell] = value;
        }
    }
    else if (value < values[cell])
    {
        values[cell] = value;
    }
}

int orior_raster_host(uint8_t *pixels, const OriorRasterConfig *config, const uint8_t *corpus, size_t corpus_len,
                      const uint8_t *needle, size_t needle_len, const OriorRasterProbe *probes, size_t probe_count)
{
    if ((pixels == NULL) || (config == NULL) || (corpus == NULL) || (needle == NULL) || (config->width == 0u) ||
        (config->height == 0u) || (needle_len == 0u) || (needle_len > corpus_len))
    {
        return 0;
    }
    if ((probes == NULL) && (probe_count != 0u))
    {
        return 0;
    }

    const size_t cells = config->width * config->height;
    // which cells an alignment reached; a cell nothing reaches writes ORIOR_RASTER_EMPTY
    uint8_t *const filled = (uint8_t *)platform_cell_cleared((unsigned long long)(cells), (unsigned long long)(1u),
                                                             (unsigned int)platform_place_g_engine_owner, 0);
    if (filled == NULL)
    {
        return 0;
    }
    for (size_t cell = 0u; cell < cells; cell += 1u)
    {
        pixels[cell] = (uint8_t)ORIOR_RASTER_EMPTY;
    }

    OriorRasterCensus census;
    raster_census(&census, corpus, corpus_len);

    const size_t alignments = (corpus_len - needle_len) + 1u;
    for (size_t at = 0u; at < alignments; at += 1u)
    {
        const uint8_t value = orior_raster_sample(config, corpus, needle, needle_len, probes, probe_count, at,
                                                  census.occurrences, census.total);
        raster_reduce(pixels, filled, orior_raster_cell(config, at, alignments), value, config->reduce);
    }
    platform_cell_released(filled, (unsigned int)platform_place_g_engine_owner);
    return 1;
}

const char *orior_raster_layout_name(OriorRasterLayout layout)
{
    switch (layout)
    {
    case ORIOR_LAYOUT_ROWS: {
        return "rows";
    }
    case ORIOR_LAYOUT_SERPENTINE: {
        return "serpentine";
    }
    case ORIOR_LAYOUT_COLUMNS: {
        return "columns";
    }
    case ORIOR_LAYOUT_DIAGONAL: {
        return "diagonal";
    }
    default: {
        return "unknown";
    }
    }
}

const char *orior_raster_channel_name(OriorRasterChannel channel)
{
    switch (channel)
    {
    case ORIOR_CHANNEL_PROVEN: {
        return "proven";
    }
    case ORIOR_CHANNEL_DEATH_LEVEL: {
        return "death-level";
    }
    case ORIOR_CHANNEL_SURVIVED: {
        return "survived";
    }
    case ORIOR_CHANNEL_RARITY: {
        return "rarity";
    }
    case ORIOR_CHANNEL_BYTE: {
        return "byte";
    }
    default: {
        return "unknown";
    }
    }
}

int orior_raster_render(uint8_t *pixels, const OriorRasterConfig *config, const uint8_t *corpus, size_t corpus_len,
                        const uint8_t *needle, size_t needle_len, const OriorRasterProbe *probes, size_t probe_count)
{
    /* DEVICE FIRST WHERE THERE IS ONE. Both arms make the same bytes. This is a performance choice
     * and never a correctness one. A machine that carries a device must use it, with no request from
     * the caller. */
    if (orior_raster_device_available() != 0)
    {
        if (orior_raster_device(pixels, config, corpus, corpus_len, needle, needle_len, probes, probe_count) != 0)
        {
            return 1;
        }
    }
    return orior_raster_host(pixels, config, corpus, corpus_len, needle, needle_len, probes, probe_count);
}
