// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file orior_raster.h
 * @brief Rasterizes the object under examination directly from engine state, on host or device.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * THE OBJECT UNDER EXAMINATION IS THE FIELD AND WHAT THE ENGINE SAW OF IT. A search over a corpus
 * produces one outcome per alignment: some probe rejected it, or every probe agreed and the full
 * compare decided it. That outcome sequence is already an image. One value per alignment, arranged
 * in corpus order, is a direct render of where the engine pruned and where it had to look harder.
 *
 * DIRECT: the raster comes from the same state that the search runs on, with no export step and no
 * second representation in between. A viewer that reads a dumped file describes what a writer put down.
 * This describes what the engine did.
 *
 * WHAT A PIXEL CARRIES. The level at which an alignment died. Zero means the first probe rejected
 * it, one means the second did, and so on. An alignment that passed every probe carries the probe
 * count, and one that also matched under the full compare carries ORIOR_RASTER_MATCH. Brighter is
 * later. A bright pixel is an alignment the probe set could not cheaply refute.
 *
 * HOST AND DEVICE PRODUCE THE SAME BYTES. That is the contract, and it is gradeable and not
 * aspirational: the raster is integer valued throughout. Agreement is exact and a difference of
 * one in one pixel is a defect. This is the same contract includes/exact_types/integers/arm.h states for the arms,
 * applied for the same reason.
 *
 * @note Downsampling takes the MINIMUM death level over the alignments mapping to a pixel. Minimum
 *       is associative and commutative. A sequential host reduction and a parallel device one
 *       reach the same value, and their orders need not agree. A rule like "first alignment
 *       wins" would make the device answer depend on scheduling.
 * @note No float anywhere. Positions map through integer division and values are counts.
 * @note Writes Netpbm P5, which needs no library and no encoder.
 */
#ifndef ORIOR_RASTER_H
#define ORIOR_RASTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
/* nvcc compiles the device rasterizer as C++, and it calls these, which the C compiler builds. */
extern "C"
{
#endif

/** @brief Value a pixel carries when its alignment passed every probe and matched. */
#define ORIOR_RASTER_MATCH 255u

/** @brief Value a pixel carries when no alignment mapped to it. */
#define ORIOR_RASTER_EMPTY 0u

    /**
     * @brief How an alignment index becomes a pixel position.
     *
     * A transform is a choice about the object's shape on the page. It changes what a reader can
     * see, and changes nothing measured. Run length shows in one, locality shows in another, and
     * periodicity that a row layout smears across a scanline is up as a column stripe.
     *
     * @note Every transform here is a bijection on the cell index computed in integer arithmetic.
     *       The device reproduces it exactly and no transform can drop or duplicate an alignment.
     */
    typedef enum
    {
        ORIOR_LAYOUT_ROWS = 0,       /**< Row major. Corpus order runs left to right, top to bottom. */
        ORIOR_LAYOUT_SERPENTINE = 1, /**< Row major with odd rows reversed. Neighbors stay adjacent
                                       *   across a row boundary. */
        ORIOR_LAYOUT_COLUMNS = 2,    /**< Column major. A period near the width is up as a stripe. */
        ORIOR_LAYOUT_DIAGONAL = 3    /**< Diagonal striping, which breaks up both axis alignments. */
    } OriorRasterLayout;

    /**
     * @brief What quantity a pixel carries.
     *
     * @note Every channel is an integer read off engine state. No channel uses floating point, and
     *       no channel scales against the image. A pixel means the same thing in two rasters taken
     *       at different sizes.
     */
    typedef enum
    {
        ORIOR_CHANNEL_DEATH_LEVEL = 0, /**< Probe index that rejected the alignment. Brighter is later. */
        ORIOR_CHANNEL_SURVIVED = 1,    /**< Binary. Bright where every probe agreed, dark otherwise. */
        ORIOR_CHANNEL_RARITY = 2,      /**< Rarity rank of the corpus byte at the alignment, from the
                                         *   census the engine steers by. */
        ORIOR_CHANNEL_BYTE = 3,        /**< The corpus byte itself, which renders the object raw. */
        ORIOR_CHANNEL_PROVEN = 4       /**< Two valued. Proven to contain no occurrence, or undetermined. */
    } OriorRasterChannel;

/** @brief Pixel value for a cell proven to contain no occurrence. */
#define ORIOR_RASTER_PROVEN 200u

/** @brief Pixel value for a cell that holds at least one alignment that the probes could not refute. */
#define ORIOR_RASTER_UNDETERMINED 60u

    /**
     * @brief How pixels resolve when several alignments map to one cell.
     *
     * @warning Both rules here are associative and commutative. That lets the device reduce in
     *          scheduler order and still agree with the host. A rule that selects by arrival, such as
     *          first or last writer, would make the device answer depend on scheduling. This enum must
     *          not get one.
     */
    typedef enum
    {
        ORIOR_REDUCE_MIN = 0, /**< Darkest wins. Earliest rejection dominates. */
        ORIOR_REDUCE_MAX = 1  /**< Brightest wins. Alignments that survive dominate. */
    } OriorRasterReduce;

    /**
     * @brief The whole input configuration for a render.
     *
     * @note One structure drives both arms. A caller that changes a field changes the host and the
     *       device render together. The grader compares them under whatever configuration it gets,
     *       and not under a fixed one.
     */
    typedef struct
    {
        size_t width;                /**< Pixels across. Non-zero. */
        size_t height;               /**< Pixel rows. Non-zero. */
        OriorRasterLayout layout;   /**< How an alignment index becomes a pixel position. */
        OriorRasterChannel channel; /**< What quantity a pixel carries. */
        OriorRasterReduce reduce;   /**< How collisions resolve. */
        uint8_t gain;                /**< Multiplier on a death level before it reaches the ramp. The code
                                      *   reads zero as one. A zeroed configuration still renders. */
    } OriorRasterConfig;

/** @brief Number of layouts, for a caller sweeping every one. */
#define ORIOR_RASTER_LAYOUTS 4u

/**
 * @brief Number of channels, for a caller sweeping every one.
 *
 * THE PROOF CHANNEL IS DIFFERENT IN KIND FROM THE OTHER FOUR AND THE DIFFERENCE IS WORTH STATING.
 * A probe set is a sound filter: it never loses a true occurrence, and it does admit alignments that
 * are not one. So the negative direction is certain and the positive is not. A cell where no
 * alignment survived contains no occurrence, and that is a proof.
 *
 * Three properties follow, and each one carries weight. It reduces as a conjunction: a cell has a
 * proof only when the probes refuted every alignment under it, and conjunction is associative and
 * commutative. It rides ORIOR_REDUCE_MIN and needs no new reduction rule. It is monotone under
 * refinement, because a new probe only removes survivors. A render never retracts a claim it
 * made earlier. And it inherits the anytime property of the planner: stop the descent anywhere,
 * render, and every proven pixel is still proven.
 *
 * That last one separates this channel from every other. A death level taken from a half-built plan
 * is a fact about the plan. A proof taken from a half-built plan is a fact about the OBJECT.
 *
 * @warning Brightness is not presence anywhere in this renderer, and here least of all.
 *          ORIOR_RASTER_PROVEN is brighter than ORIOR_RASTER_UNDETERMINED and means the opposite
 *          of an occurrence. ORIOR_RASTER_MATCH is the only value entitled to assert one.
 */
#define ORIOR_RASTER_CHANNELS 5u

    /**
     * @brief One probe as the rasterizer needs it, matching OriorProbe in orior.h.
     *
     * @note Declared here, and not by an include of the engine header. The device translation unit
     *       then compiles with no pull of the limb library that it does not use. The two layouts are identical
     *       and orior_raster_*.c asserts that at compile time.
     * @note Declared above the volume surface below, which names this type in a signature. A structure
     *       cannot name a type the compiler has not seen, and the order of this header enforces that.
     */
    typedef struct
    {
        size_t origin; /**< First position in the needle this probe reads. */
        size_t step;   /**< Distance between successive positions. */
        size_t length; /**< Positions read. */
    } OriorRasterProbe;

    /**
     * @brief How an alignment index becomes a voxel position.
     *
     * SEPARATE FROM OriorRasterLayout AND NOT AN EXTENSION OF IT. A raster layout is a bijection onto
     * a width by height sheet. A volume layout is a bijection onto a width by height by depth block.
     * A wider raster enum would change ORIOR_RASTER_LAYOUTS, which every caller that sweeps the raster
     * uses as its bound. It would also hand those callers layouts that need a depth they do not carry.
     *
     * @note The channel, the reduce rule and the gain stay as they are, and this header has no copy. Each
     *       reads one alignment and returns one value, with no knowledge of how many dimensions the
     *       destination has. Only the index to position map changes between a sheet and a block. The
     *       engine states the same about its own index set, which needs no order and no
     *       dimension.
     * @note Every transform here is a bijection computed in integer arithmetic. A device
     *       implementation reproduces it exactly and no transform can drop or duplicate an alignment.
     */
    typedef enum
    {
        ORIOR_VOLUME_SLABS = 0,   /**< Slab major. Fills a sheet, then the next sheet behind it. The
                                    *   three dimensional reading of ORIOR_LAYOUT_ROWS. */
        ORIOR_VOLUME_BOUSTRO = 1, /**< Slab major with every other row and every other slab reversed.
                                    *   Consecutive alignments stay adjacent across both boundaries. */
        ORIOR_VOLUME_MORTON = 2,  /**< Morton order, which interleaves the bits of x, y and z. It
                                    *   keeps locality on all three axes at once. A linear index set needs
                                    *   that to read as a solid and not as stacked sheets.
                                    *   Requires the extents to be powers of two. A caller that gives
                                    *   others gets an error, and not a silent remap. */
        ORIOR_VOLUME_HELIX = 3    /**< Slab major with each slab's rows shifted by its depth index.
                                    *   A feature at a fixed corpus offset winds through the block, and
                                    *   does not stack. It is a shear, and not a rotation. A true helix
                                    *   needs trigonometry, and this renderer is integer throughout. The
                                    *   host and a device then agree exactly. A shear is the bijection
                                    *   that gives the same reading inside the integers. */
    } OriorVolumeLayout;

/** @brief Number of volume layouts, for a caller sweeping every one. */
#define ORIOR_VOLUME_LAYOUTS 4u

    /**
     * @brief The whole input configuration for a volume render.
     *
     * @note Carries the raster's channel, reduce and gain by reference to the same enums, with no copy.
     *       A channel means one thing in this tree, and a second definition of it would be a future
     *       disagreement.
     * @warning `depth` of zero renders nothing and errors. A flat render is the raster's job and
     *          this entry does not quietly become one.
     */
    typedef struct
    {
        size_t width;                /**< Voxels across. Non-zero. */
        size_t height;               /**< Voxel rows. Non-zero. */
        size_t depth;                /**< Voxel slabs. Non-zero, and one errors and not flattened. */
        OriorVolumeLayout layout;   /**< How an alignment index becomes a voxel position. */
        OriorRasterChannel channel; /**< What quantity a voxel carries. Same set as the raster. */
        OriorRasterReduce reduce;   /**< How collisions resolve. Same set as the raster. */
        uint8_t gain;                /**< Multiplier on a death level. The code reads zero as one. */
    } OriorVolumeConfig;

    /**
     * @brief Voxel index an alignment lands on under a configuration's layout.
     *
     * @param[in] config    Render configuration [BORROWS].
     * @param[in] alignment Alignment index.
     * @return              Index into a width*height*depth block, or the block size where the
     *                      configuration is unusable, which a caller treats as "not placed".
     */
    size_t orior_volume_cell_for(const OriorVolumeConfig *config, size_t alignment);

    /**
     * @brief Renders the object under examination into a volume, on the host.
     *
     * @param[out] voxels     width*height*depth bytes, written whole [BORROWS].
     * @param[in]  config     Render configuration [BORROWS].
     * @param[in]  corpus     Bytes under examination [BORROWS].
     * @param[in]  corpus_len How many.
     * @param[in]  needle     Bytes that the search looks for [BORROWS].
     * @param[in]  needle_len How many.
     * @param[in]  probes     Probes in evaluation order [BORROWS].
     * @param[in]  probe_count How many probes.
     * @param[in]  census     RESERVED AND NOT READ TODAY [BORROWS]. This call builds its own census
     *                        from `corpus`, and uses that for ORIOR_CHANNEL_RARITY whatever the caller
     *                        gives here. The parameter is the integration point for a rarity source from
     *                        the caller: a reference distribution, or a census over a sampled slice.
     *                        The header keeps it for that reason, and every caller in this tree
     *                        passes NULL, which is correct. The call silently
     *                        ignores a census built over something else, and that has no symptom. The
     *                        render succeeds, and carries rarity computed from the corpus in front of
     *                        it.
     * @return                1 where the call wrote the volume, 0 where the configuration errored.
     *
     * @note THE HOST ARM. orior_volume_render is the entry a caller should use. It prefers the device
     *       and falls back here, the same way orior_raster_render does for a sheet. This arm and
     *       orior_volume_device stay public because a grader has to call one specific arm and compare
     *       it against the other. That comparison is the only check on the device copy.
     */
    int orior_volume_render_host(uint8_t *voxels, const OriorVolumeConfig *config, const uint8_t *corpus,
                                  size_t corpus_len, const uint8_t *needle, size_t needle_len,
                                  const OriorRasterProbe *probes, size_t probe_count, const void *census);

    /**
     * @brief Renders a volume on whichever arm this machine has, with the device first.
     *
     * @param[out] voxels     width*height*depth bytes, written whole [BORROWS].
     * @param[in]  config     Render configuration [BORROWS].
     * @param[in]  corpus     Bytes under examination [BORROWS].
     * @param[in]  corpus_len How many.
     * @param[in]  needle     Bytes that the search looks for [BORROWS].
     * @param[in]  needle_len How many.
     * @param[in]  probes     Probes in evaluation order [BORROWS].
     * @param[in]  probe_count How many probes.
     * @param[in]  census     RESERVED AND NOT READ, as in orior_volume_render_host [BORROWS].
     * @return                1 on success, 0 where both arms errored.
     *
     * THE ENTRY A CALLER SHOULD USE, matching orior_raster_render for the sheet. Both arms produce the
     * same bytes. The choice between them is a performance decision, and never a correctness one. This
     * queries the device first and falls back to the host. A render happens if either arm can do it.
     */
    int orior_volume_render(uint8_t *voxels, const OriorVolumeConfig *config, const uint8_t *corpus,
                             size_t corpus_len, const uint8_t *needle, size_t needle_len,
                             const OriorRasterProbe *probes, size_t probe_count, const void *census);

    /**
     * @brief Renders the object under examination into a volume, on the device.
     *
     * @return 1 where the call wrote the volume. 0 where it rejected an argument, the layout errored on the
     *         configuration, no device answered, or the device work failed. Produces the same bytes
     *         orior_volume_render_host produces for the same arguments, which bench_raster grades voxel
     *         for voxel. A difference is a defect in one of the two and never a tradeoff.
     * @note Always returns 0 in a build compiled without the device renderer. A caller written
     *       against both arms links and runs either way.
     */
    int orior_volume_device(uint8_t *voxels, const OriorVolumeConfig *config, const uint8_t *corpus,
                             size_t corpus_len, const uint8_t *needle, size_t needle_len,
                             const OriorRasterProbe *probes, size_t probe_count, const void *census);

    /**
     * @brief Whether a usable CUDA device is present for the device volume renderer.
     *
     * @return 1 where a device is present and this build carries the device volume renderer, 0
     *         otherwise. Always 0 in a build compiled without it. A caller written against both arms
     *         links and runs either way, and queries this before a call to orior_volume_device.
     */
    int orior_volume_device_available(void);

    /**
     * @brief Writes a volume as a raw byte block beside a text header naming its extents.
     *
     * @param[in] path   Destination for the bytes [BORROWS].
     * @param[in] voxels width*height*depth bytes [BORROWS].
     * @param[in] config Render configuration, read for the extents [BORROWS].
     * @return           1 on success, 0 where the write of the file failed.
     *
     * @note Netpbm has no volume format. This writes the block raw and states its extent in a sidecar
     *       and does not invent a container. A generated file states that a generator made it, and names
     *       that generator, as the sidecar does.
     */
    int orior_volume_write_raw(const char *path, const uint8_t *voxels, const OriorVolumeConfig *config);

    /** @brief Name of a volume layout, for a caller that prints a row. Never null. */
    const char *orior_volume_layout_name(OriorVolumeLayout layout);

    /**
     * @brief Rasterizes the object under examination on the host.
     *
     * @param[out] pixels     Raster, `width` bytes per row, `height` rows [BORROWS].
     * @param[in]  width      Pixels across. Must be non-zero.
     * @param[in]  height     Pixel rows. Must be non-zero.
     * @param[in]  corpus     Bytes under examination [BORROWS].
     * @param[in]  corpus_len How many.
     * @param[in]  needle     Bytes that the search looks for [BORROWS].
     * @param[in]  needle_len How many.
     * @param[in]  probes     Probe set in evaluation order [BORROWS].
     * @param[in]  probe_count How many probes.
     * @return                1 on success, 0 where it rejects an argument.
     *
     * @note The call examines every alignment. The raster is a rendering of the search and not a sample of
     *       it. A pixel is never guessed from its neighbors.
     * @note Rejects and writes nothing where a pointer is null, where `width` or `height` is zero,
     *       where `needle_len` is zero, or where `needle_len` exceeds `corpus_len`.
     */
    int orior_raster_host(uint8_t *pixels, const OriorRasterConfig *config, const uint8_t *corpus, size_t corpus_len,
                           const uint8_t *needle, size_t needle_len, const OriorRasterProbe *probes,
                           size_t probe_count);

    /**
     * @brief Pixel index an alignment lands on under a configuration's layout.
     *
     * @param[in] config     Render configuration [BORROWS].
     * @param[in] at         Alignment index.
     * @param[in] alignments How many alignments the object has.
     * @return               Cell index inside `width * height`.
     * @note Exposed because the device rasterizer calls the same function. That retains one
     *       transform and not two that agree until somebody edits one.
     */
    size_t orior_raster_cell(const OriorRasterConfig *config, size_t at, size_t alignments);

    /**
     * @brief Value one alignment contributes under a configuration's channel.
     *
     * @param[in] occurrences Symbol counts over the object, 256 entries [BORROWS].
     * @param[in] total       Bytes counted into `occurrences`.
     * @return                The pixel value before reduction.
     * @note Shared with the device for the same reason orior_raster_cell is.
     */
    uint8_t orior_raster_sample(const OriorRasterConfig *config, const uint8_t *corpus, const uint8_t *needle,
                                 size_t needle_len, const OriorRasterProbe *probes, size_t probe_count, size_t at,
                                 const uint64_t *occurrences, uint64_t total);

    /** @brief Name of a layout, for a caller that prints a row. Never null. */
    const char *orior_raster_layout_name(OriorRasterLayout layout);

    /** @brief Name of a channel, for a caller that prints a row. Never null. */
    const char *orior_raster_channel_name(OriorRasterChannel channel);

    /**
     * @brief Writes a raster as Netpbm P5 grayscale.
     *
     * @param[in] path   Where to write [BORROWS].
     * @param[in] pixels Raster [BORROWS].
     * @param[in] width  Pixels across.
     * @param[in] height Pixel rows.
     * @return           1 on success, 0 where the write of the file failed.
     *
     * @note P5 because the format is a short text header and then the bytes. No encoding applies. A
     *       reader who compares two rasters compares the same bytes that the rasterizer produced.
     */
    int orior_raster_write_pgm(const char *path, const uint8_t *pixels, size_t width, size_t height);

    /**
     * @brief Renders on whichever arm this machine has, with the device first.
     *
     * @param[out] pixels     Raster, `config->width` bytes per row [BORROWS].
     * @param[in]  config     Render configuration [BORROWS].
     * @param[in]  corpus     Bytes under examination [BORROWS].
     * @param[in]  corpus_len How many.
     * @param[in]  needle     Bytes that the search looks for [BORROWS].
     * @param[in]  needle_len How many.
     * @param[in]  probes     Probe set in evaluation order [BORROWS].
     * @param[in]  probe_count How many probes.
     * @return                1 on success, 0 where both arms errored.
     *
     * THE ENTRY A CALLER SHOULD USE. A machine with a device should render on it with no request from the
     * caller, and the two arms produce the same bytes. The choice between them is a performance
     * decision, and never a correctness one. This queries the device first and falls back to the host.
     *
     * @note Falls back, and does not fail, where the device errors. A render always happens if
     *       either arm can do it.
     * @note orior_raster_host and orior_raster_device stay public because a grader has to be able to
     *       call one specific arm and compare. A caller that does not care should not have to.
     */
    int orior_raster_render(uint8_t *pixels, const OriorRasterConfig *config, const uint8_t *corpus,
                             size_t corpus_len, const uint8_t *needle, size_t needle_len,
                             const OriorRasterProbe *probes, size_t probe_count);

    /**
     * @brief Whether a usable CUDA device is present for the device rasterizer.
     *
     * @return 1 where a device is present, 0 otherwise.
     * @note Always returns 0 in a build compiled without the device rasterizer. A caller written
     *       against both arms links and runs either way.
     */
    int orior_raster_device_available(void);

    /**
     * @brief Rasterizes the object under examination on the device.
     *
     * @param[out] pixels     Raster, `width` bytes per row, `height` rows [BORROWS].
     * @param[in]  width      Pixels across. Must be non-zero.
     * @param[in]  height     Pixel rows. Must be non-zero.
     * @param[in]  corpus     Bytes under examination [BORROWS].
     * @param[in]  corpus_len How many.
     * @param[in]  needle     Bytes that the search looks for [BORROWS].
     * @param[in]  needle_len How many.
     * @param[in]  probes     Probe set in evaluation order [BORROWS].
     * @param[in]  probe_count How many probes.
     * @return                1 on success, 0 where no device is present or it rejects an argument.
     *
     * @note Produces the same bytes orior_raster_host produces for the same arguments. A difference is
     *       a defect in one of them, and bench_raster grades exactly that.
     * @note One thread per alignment, which reduces into the raster with atomicMin. The minimum rule
     *       lets that reduction run in any order and still agree with the host.
     */
    int orior_raster_device(uint8_t *pixels, const OriorRasterConfig *config, const uint8_t *corpus,
                             size_t corpus_len, const uint8_t *needle, size_t needle_len,
                             const OriorRasterProbe *probes, size_t probe_count);

#ifdef __cplusplus
}
#endif

#endif /* ORIOR_RASTER_H */
