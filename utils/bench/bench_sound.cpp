// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_sound.cpp
 * @brief The digest heard against its own nonce, at two scales, with a spectrum and a wav file.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note A histogram throws away the order in which its inputs arrive. H35 tests the arrangement of
 *       the holes across the value axis in all 4.29 billion linear directions, and finds nothing.
 *       But the value axis is not the only axis, and it is not the axis that the miner walks. **No
 *       histogram can look along the nonce axis.**
 * @note Nonce is time. The digest is the waveform. That makes this an audio problem.
 * @note The script examples/sound/1_represent/vocalization_scale.py carries the lesson. A choice of
 *       representation decides an answer, and can be wrong by four orders of magnitude. At 8 kHz
 *       per sample, the animals and the people interleave. At one symbol every 10 ms, they separate
 *       without overlap. The sample-scale statistic reads inside a single call, and never sees the
 *       arrangement of calls.
 * @note Thus two scales, the same as there. The sample scale is one digest, one symbol. The envelope
 *       scale is the root mean square over a block of nonces, spread back over a byte. It is the
 *       same envelope, applied to this signal without modification.
 * @note And a different transform. A Walsh test is linear over GF(2), and sees dyadic structure. A
 *       Fourier spectrum is linear over the integers, and sees periodic structure. The two are blind
 *       to different things. A Walsh test cannot see a comb.
 * @note Two controls, because one fails in only one direction. A splitmix64 chain must come out
 *       white. A signal with a period deliberately written into it must come out with that period
 *       visible. A spectrum tool that reports flat for everything is broken, and only the second
 *       control can catch that.
 * @warning The wav files are the purpose, and not a decoration. See the corpus audit posit in
 *          evidence/proofs/posits. A read of the output finds problems. A statistic that leaves its
 *          range does not. A spectrum plot is a statistic. To listen is to read the
 *          output.
 */

#include "../../includes/platform/platform.h"
#include "bench_load_limit.h"
#include "bench_seed.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace
{

/** @brief The padding word the standard appends after the message. */
const uint32_t PADDING_LEAD_WORD = 0x80000000u;

/** @brief Length of a block header in bits. */
const uint32_t HEADER_LENGTH_BITS = 0x00000280u;

/** @brief Length of a digest in bits. */
const uint32_t DIGEST_LENGTH_BITS = 0x00000100u;

/** @brief Samples per second the wav files claim, chosen to match the vocalization corpora. */
const uint32_t SAMPLE_RATE = 8000u;

/** @brief Where silence sits in an unsigned byte sample, as anchor_sift's envelope has it. */
const int MIDPOINT = 128;

/** @brief Which function fills the signal. */
enum Source
{
    SOURCE_SHA256D,  /**< The real thing. */
    SOURCE_SPLITMIX, /**< A good pseudorandom function, which must come out white. */
    SOURCE_COMB      /**< A deliberate period, which must come out with a peak. */
};

/** @brief The midstate and header tail for the hash of a nonce. */
struct Job
{
    Sha256State midstate;
    uint32_t merkle_root_tail;
    uint32_t ntime;
    uint32_t nbits;
};

/**
 * @brief Reverses the byte order of a word.
 *
 * @param[in] value Word to reverse.
 * @return          The reversed word.
 */
uint32_t reverse_word_bytes(uint32_t value)
{
    return ((value & 0x000000ffu) << 24) | ((value & 0x0000ff00u) << 8) | ((value & 0x00ff0000u) >> 8) |
           ((value & 0xff000000u) >> 24);
}

/**
 * @brief Advances a splitmix64 state and returns its output.
 *
 * @param[in,out] state Generator state [BORROWS].
 * @return              Sixty-four output bits.
 */
uint64_t splitmix64(uint64_t *state)
{
    *state += 0x9e3779b97f4a7c15ull;
    uint64_t mixed = *state;
    mixed = (mixed ^ (mixed >> 30)) * 0xbf58476d1ce4e5b9ull;
    mixed = (mixed ^ (mixed >> 27)) * 0x94d049bb133111ebull;
    return mixed ^ (mixed >> 31);
}

/**
 * @brief One sample of the signal: a byte read off the digest of one nonce.
 *
 * @param[in] job    Midstate and header tail [BORROWS].
 * @param[in] source Which function to evaluate.
 * @param[in] nonce  The nonce: the time index.
 * @return           One unsigned byte sample.
 * @note The function takes the last digest byte, and not the first. The protocol reads a digest
 *       little-endian, and a share threshold looks at the leading bits. A read of the other end keeps
 *       the signal away from the part of the digest that the threshold examines.
 */
void digest_for_nonce(const Job &job, Source source, uint32_t nonce, unsigned char *digest)
{
    if (source == SOURCE_SPLITMIX)
    {
        uint64_t state = (uint64_t)nonce;
        for (unsigned half = 0u; half < 4u; half += 1u)
        {
            const uint64_t drawn = splitmix64(&state);
            for (unsigned byte = 0u; byte < 8u; byte += 1u)
            {
                digest[(half * 8u) + byte] = (unsigned char)(drawn >> (56u - (byte * 8u)));
            }
        }
        return;
    }

    if (source == SOURCE_COMB)
    {
        // Pseudorandom with a period written into it, faintly, and into one byte only. If the
        // spectrum cannot find this, it cannot find anything, and a flat reading on the real signal
        // would mean nothing. The period in one byte of thirty-two also grades the unit sweep. With a
        // fixed byte, a run will miss it thirty-one times out of thirty-two.
        uint64_t state = (uint64_t)nonce;
        for (unsigned half = 0u; half < 4u; half += 1u)
        {
            const uint64_t drawn = splitmix64(&state);
            for (unsigned byte = 0u; byte < 8u; byte += 1u)
            {
                digest[(half * 8u) + byte] = (unsigned char)(drawn >> (56u - (byte * 8u)));
            }
        }
        const double period = 137.0;
        const double wave = 6.0 * sin(2.0 * 3.14159265358979 * (double)nonce / period);
        int mixed = (int)digest[19] + (int)wave;
        mixed = (mixed < 0) ? 0 : ((mixed > 255) ? 255 : mixed);
        digest[19] = (unsigned char)mixed;
        return;
    }

    uint32_t message_block[SHA256_BLOCK_WORDS];
    Sha256State first_pass = job.midstate;

    message_block[0] = job.merkle_root_tail;
    message_block[1] = job.ntime;
    message_block[2] = job.nbits;
    message_block[3] = reverse_word_bytes(nonce);
    message_block[4] = PADDING_LEAD_WORD;
    for (unsigned slot = 5u; slot < 15u; slot += 1u)
    {
        message_block[slot] = 0u;
    }
    message_block[15] = HEADER_LENGTH_BITS;
    sha256_block_compress(&first_pass, message_block);

    Sha256State second_pass;
    sha256_state_init(&second_pass);
    for (unsigned slot = 0u; slot < SHA256_STATE_WORDS; slot += 1u)
    {
        message_block[slot] = first_pass.word[slot];
    }
    message_block[8] = PADDING_LEAD_WORD;
    for (unsigned slot = 9u; slot < 15u; slot += 1u)
    {
        message_block[slot] = 0u;
    }
    message_block[15] = DIGEST_LENGTH_BITS;
    sha256_block_compress(&second_pass, message_block);

    for (unsigned word = 0u; word < SHA256_STATE_WORDS; word += 1u)
    {
        digest[(word * 4u) + 0u] = (unsigned char)(second_pass.word[word] >> 24);
        digest[(word * 4u) + 1u] = (unsigned char)(second_pass.word[word] >> 16);
        digest[(word * 4u) + 2u] = (unsigned char)(second_pass.word[word] >> 8);
        digest[(word * 4u) + 3u] = (unsigned char)second_pass.word[word];
    }
}

/**
 * @brief Root mean square deviation from the midpoint per block, spread back over a byte.
 *
 * @param[in]  samples Sample-scale signal [BORROWS].
 * @param[in]  count   Samples of the signal.
 * @param[in]  block   Samples per envelope symbol.
 * @param[out] out     The envelope, room for `count / block` symbols [BORROWS].
 * @return             The symbols of the envelope, 0 where the signal is shorter than a block.
 * @note This is the envelope of the anchor sift sound representation, transcribed and not
 *       rewritten. It includes the spread over the range that the signal actually uses. Without the
 *       spread, a quiet signal falls into a few levels, and meets a loud one that does not.
 */
size_t envelope(const unsigned char *samples, size_t count, size_t block, unsigned char *out)
{
    size_t made = 0u;
    if (count < block)
    {
        return made;
    }

    for (size_t start = 0u; (start + block) <= count; start += block)
    {
        double total = 0.0;
        for (size_t at = start; at < (start + block); at += 1u)
        {
            const double offset = (double)samples[at] - (double)MIDPOINT;
            total += offset * offset;
        }
        out[made] = (unsigned char)(int)sqrt(total / (double)block);
        made += 1u;
    }

    unsigned char low = 255u;
    unsigned char high = 0u;
    for (size_t at = 0u; at < made; at += 1u)
    {
        low = (out[at] < low) ? out[at] : low;
        high = (out[at] > high) ? out[at] : high;
    }
    if (high <= low)
    {
        return made;
    }
    for (size_t at = 0u; at < made; at += 1u)
    {
        out[at] = (unsigned char)(1 + (int)(254.0 * ((double)out[at] - (double)low) / ((double)high - (double)low)));
    }
    return made;
}

/**
 * @brief Transforms a real signal in place, radix two, decimation in time.
 *
 * @param[in,out] real      Real parts, `length` of them [BORROWS].
 * @param[in,out] imaginary Imaginary parts, the same length [BORROWS].
 * @param[in]     length    The parts, a power of two.
 * @note Written out, and not pulled in. The only alternatives on this machine would drag a
 *       dependency into a tree that has none, and the transform is forty lines. The bench checks it
 *       against a signal with a known answer, and does not trust it.
 */
void fourier_transform(double *real, double *imaginary, size_t length)
{
    for (size_t index = 1u, position = 0u; index < length; index += 1u)
    {
        size_t bit = length >> 1;
        for (; (position & bit) != 0u; bit >>= 1)
        {
            position ^= bit;
        }
        position ^= bit;
        if (index < position)
        {
            const double swap_real = real[index];
            const double swap_imaginary = imaginary[index];
            real[index] = real[position];
            imaginary[index] = imaginary[position];
            real[position] = swap_real;
            imaginary[position] = swap_imaginary;
        }
    }

    for (size_t span = 2u; span <= length; span <<= 1)
    {
        const double angle = -2.0 * 3.14159265358979323846 / (double)span;
        const double step_real = cos(angle);
        const double step_imaginary = sin(angle);

        for (size_t base = 0u; base < length; base += span)
        {
            double turn_real = 1.0;
            double turn_imaginary = 0.0;

            for (size_t at = 0u; at < (span / 2u); at += 1u)
            {
                const double upper_real =
                    real[base + at + (span / 2u)] * turn_real - imaginary[base + at + (span / 2u)] * turn_imaginary;
                const double upper_imaginary =
                    real[base + at + (span / 2u)] * turn_imaginary + imaginary[base + at + (span / 2u)] * turn_real;
                real[base + at + (span / 2u)] = real[base + at] - upper_real;
                imaginary[base + at + (span / 2u)] = imaginary[base + at] - upper_imaginary;
                real[base + at] += upper_real;
                imaginary[base + at] += upper_imaginary;

                const double next_real = turn_real * step_real - turn_imaginary * step_imaginary;
                turn_imaginary = turn_real * step_imaginary + turn_imaginary * step_real;
                turn_real = next_real;
            }
        }
    }
}

/** @brief What a spectrum reading reports back. */
struct Peak
{
    double height;  /**< Largest bin, in multiples of the mean power. */
    size_t bin;     /**< Which bin it sat in. */
    double period;  /**< The period that bin matches, in samples. */
    size_t counted; /**< How many bins the search examined. */
};

/**
 * @brief Reads the power spectrum of a byte signal and returns its largest peak.
 *
 * @param[in]  samples   The signal [BORROWS].
 * @param[in]  count     Samples of the signal.
 * @param[out] real      Room for `count` real parts, which the transform writes [BORROWS].
 * @param[out] imaginary Room for `count` imaginary parts, which the transform writes [BORROWS].
 * @return               The largest peak, relative to the mean power.
 * @note The function centers the signal first. Thus bin zero carries no weight, and cannot look like
 *       structure. For a white signal, every other bin has an exponential distribution about the mean
 *       power. Thus the largest of n of them sits near ln(n) times the mean. The height reads against
 *       that reference, and not against a threshold that anybody picked.
 */
Peak spectrum_peak(const unsigned char *samples, size_t count, double *real, double *imaginary)
{
    size_t length = 1u;
    while ((length * 2u) <= count)
    {
        length *= 2u;
    }

    EMBED_CALL(memor.set, MemoriaCfg, .dst = imaginary, .bytes = length * sizeof(double), .val = 0);

    double mean = 0.0;
    for (size_t at = 0u; at < length; at += 1u)
    {
        mean += (double)samples[at];
    }
    mean /= (double)length;

    for (size_t at = 0u; at < length; at += 1u)
    {
        real[at] = (double)samples[at] - mean;
    }

    fourier_transform(real, imaginary, length);

    // Only the first half is independent, and bin zero is zero by construction after the center step.
    const size_t usable = length / 2u;
    double total = 0.0;
    for (size_t bin = 1u; bin < usable; bin += 1u)
    {
        total += (real[bin] * real[bin]) + (imaginary[bin] * imaginary[bin]);
    }
    const double average = total / (double)(usable - 1u);

    Peak peak = {0.0, 0u, 0.0, usable - 1u};
    for (size_t bin = 1u; bin < usable; bin += 1u)
    {
        const double power = ((real[bin] * real[bin]) + (imaginary[bin] * imaginary[bin])) / average;
        if (power > peak.height)
        {
            peak.height = power;
            peak.bin = bin;
        }
    }
    peak.period = (peak.bin > 0u) ? ((double)length / (double)peak.bin) : 0.0;
    return peak;
}

/**
 * @brief Writes an eight-bit unsigned mono wav file.
 *
 * @param[in] path    Where to write [BORROWS].
 * @param[in] samples The signal [BORROWS].
 * @param[in] count   Samples of the signal.
 * @param[in] rate    Samples per second to claim.
 * @return            Nonzero on success.
 */
int write_wav(const char *path, const unsigned char *samples, size_t count, uint32_t rate)
{
    FILE *const handle = fopen(path, "wb");
    if (handle == nullptr)
    {
        return 0;
    }

    const uint32_t data_bytes = (uint32_t)count;
    const uint32_t riff_bytes = 36u + data_bytes;
    const uint32_t format_bytes = 16u;
    const uint16_t format_tag = 1u;
    const uint16_t channels = 1u;
    const uint32_t byte_rate = rate;
    const uint16_t block_align = 1u;
    const uint16_t bits = 8u;

    fwrite("RIFF", 1u, 4u, handle);
    fwrite(&riff_bytes, 4u, 1u, handle);
    fwrite("WAVEfmt ", 1u, 8u, handle);
    fwrite(&format_bytes, 4u, 1u, handle);
    fwrite(&format_tag, 2u, 1u, handle);
    fwrite(&channels, 2u, 1u, handle);
    fwrite(&rate, 4u, 1u, handle);
    fwrite(&byte_rate, 4u, 1u, handle);
    fwrite(&block_align, 2u, 1u, handle);
    fwrite(&bits, 2u, 1u, handle);
    fwrite("data", 1u, 4u, handle);
    fwrite(&data_bytes, 4u, 1u, handle);
    fwrite(samples, 1u, count, handle);
    fclose(handle);
    return 1;
}

/** @brief Names a source, for a printed report. */
const char *source_name(Source source)
{
    return (source == SOURCE_SHA256D)
               ? "SHA256d"
               : ((source == SOURCE_SPLITMIX) ? "splitmix64 control" : "comb control, period 137");
}

} // namespace

/** @brief The pool of the program: room for the tracks, the envelope and the transform of 2^22 samples, and more. */
extern "C" const unsigned long long g_program_pool_bytes = 1ull << 29u;

int program_main(int argc, char **argv)
{
    bench_lower_priority();

    const unsigned length_bits = (argc > 1) ? (unsigned)atoi(argv[1]) : 22u;
    const size_t length = (size_t)1u << length_bits;

    printf("================================================================\n");
    printf("  The digest heard against its own nonce\n");
    printf("================================================================\n");
    printf("\n  Every statistic in this workbook is a histogram, and a histogram throws away\n");
    printf("  the order its inputs arrived in. The arrangement work in H35 walked the value\n");
    printf("  axis in all 4.29 billion linear directions. The nonce axis is a different axis\n");
    printf("  and it is the one the miner walks, and nothing here has ever looked along it,\n");
    printf("  because no histogram can.\n");
    printf("\n  Nonce is time and the digest is the waveform. One sample is the last byte of\n");
    printf("  one digest, which is the end of the digest the protocol reads as least\n");
    printf("  significant and the part nothing else here has examined.\n");
    printf("\n  Two scales, after vocalization_scale.py, where reading at the wrong one was\n");
    printf("  wrong by four orders of magnitude. And a Fourier spectrum, which sees periodic\n");
    printf("  structure where every Walsh test run here sees only dyadic.\n");
    printf("\n  Samples: 2^%u nonces from zero.\n", length_bits);

    const uint8_t header[BITCOIN_HEADER_BYTES] = {
        0x01, 0x00, 0x00, 0x00, 0x81, 0xcd, 0x02, 0xab, 0x7e, 0x56, 0x9e, 0x8b, 0xcd, 0x93, 0x17, 0xe2,
        0xfe, 0x99, 0xf2, 0xde, 0x44, 0xd4, 0x9a, 0xb2, 0xb8, 0x85, 0x1b, 0xa4, 0xa3, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xe3, 0x20, 0xb6, 0xc2, 0xff, 0xfc, 0x8d, 0x75, 0x04, 0x23, 0xdb, 0x8b,
        0x1e, 0xb9, 0x42, 0xae, 0x71, 0x0e, 0x95, 0x1e, 0xd7, 0x97, 0xf7, 0xaf, 0xfc, 0x88, 0x92, 0xb0,
        0xf1, 0xfc, 0x12, 0x2b, 0xc7, 0xf5, 0xd7, 0x4d, 0xf2, 0xb9, 0x44, 0x1a, 0x42, 0xa1, 0x46, 0x95};

    Job job;
    sha256_header_midstate(&job.midstate, header);
    job.merkle_root_tail = ((uint32_t)header[64] << 24) | ((uint32_t)header[65] << 16) | ((uint32_t)header[66] << 8) |
                           (uint32_t)header[67];
    job.ntime = ((uint32_t)header[68] << 24) | ((uint32_t)header[69] << 16) | ((uint32_t)header[70] << 8) |
                (uint32_t)header[71];
    job.nbits = ((uint32_t)header[72] << 24) | ((uint32_t)header[73] << 16) | ((uint32_t)header[74] << 8) |
                (uint32_t)header[75];

    const Source sources[3] = {SOURCE_SHA256D, SOURCE_SPLITMIX, SOURCE_COMB};

    // Nothing here stays fixed where a sweep could cover it. The unit runs over all thirty-two digest
    // bytes, and the scale over a geometric ladder. A grid of two block sizes lets a period of 137
    // fall straight between its rungs. The comb reads 63,427 times the mean power at sample scale,
    // and 12.3 against an expected 12.5 at envelope 80: the same signal, reported as absent. Scale
    // and unit stay free until the answer is final.
    const size_t ladder[12] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u, 1024u, 4096u, 16384u};

    printf("\n  Sweeping the unit over all 32 digest bytes and the scale over 12 blocks,\n");
    printf("  which is 384 readings per signal instead of the 3 a fixed grid would give.\n");

    printf("\n  %-28s %8s %8s %14s %12s %12s\n", "signal", "byte", "block", "largest peak", "expected", "period");
    printf("  %-28s %8s %8s %14s %12s %12s\n", "----------------------------", "--------", "--------", "--------------",
           "------------", "------------");

    // The track of each digest byte, `length` samples each, one after another. Then the envelope of a track and the
    // parts of its transform. Each cell is on the temporary tier, and the program gives it back before it returns
    unsigned char *const tracks =
        (unsigned char *)platform_cell_taken(32ull * length, platform_place_g_program_owner, 1);
    unsigned char *const shaped_room = (unsigned char *)platform_cell_taken(length, platform_place_g_program_owner, 1);
    double *const real = (double *)platform_cell_taken(length * sizeof(double), platform_place_g_program_owner, 1);
    double *const imaginary = (double *)platform_cell_taken(length * sizeof(double), platform_place_g_program_owner, 1);
    if ((tracks == nullptr) || (shaped_room == nullptr) || (real == nullptr) || (imaginary == nullptr))
    {
        fprintf(stderr, "bench_sound: the pool cannot meet the tracks of 2^%u samples\n", length_bits);
        return 1;
    }

    for (unsigned which = 0u; which < 3u; which += 1u)
    {
        const Source source = sources[which];

        // All thirty-two bytes come from one hash. Thus a sweep of the unit costs memory, and not work.
        for (size_t at = 0u; at < length; at += 1u)
        {
            unsigned char digest[32];
            digest_for_nonce(job, source, (uint32_t)at, digest);
            for (unsigned byte = 0u; byte < 32u; byte += 1u)
            {
                tracks[(byte * length) + at] = digest[byte];
            }
        }

        double best_height = 0.0;
        unsigned best_byte = 0u;
        size_t best_block = 0u;
        double best_period = 0.0;
        double bins_examined = 0.0;

        for (unsigned byte = 0u; byte < 32u; byte += 1u)
        {
            for (unsigned rung = 0u; rung < 12u; rung += 1u)
            {
                const unsigned char *const track = &tracks[byte * length];
                const unsigned char *const shaped = (ladder[rung] == 1u) ? track : shaped_room;
                const size_t shaped_count =
                    (ladder[rung] == 1u) ? length : envelope(track, length, ladder[rung], shaped_room);
                if (shaped_count < 256u)
                {
                    continue;
                }
                const Peak peak = spectrum_peak(shaped, shaped_count, real, imaginary);
                bins_examined += (double)peak.counted;
                if (peak.height > best_height)
                {
                    best_height = peak.height;
                    best_byte = byte;
                    best_block = ladder[rung];
                    best_period = peak.period;
                }
            }
        }

        printf("  %-28s %8u %8zu %14.3f %12.3f %12.1f\n", source_name(source), best_byte, best_block, best_height,
               log(bins_examined), best_period);

        // Five seconds of the byte that the sweep settles on. Thus anyone can listen to the report.
        const size_t heard = (size_t)SAMPLE_RATE * 5u;
        if (length >= heard)
        {
            // The bench runs from the root of the tree. Thus the wav files go to build/bench, and not to
            // the root.
            (void)platform_folders_made("build/bench", 0777u);
            char path[64];
            snprintf(path, sizeof(path), "build/bench/sound_%u.wav", which);
            if (write_wav(path, &tracks[best_byte * length], heard, SAMPLE_RATE) == 0)
            {
                printf("  could not write %s\n", path);
            }
        }
    }
    platform_cell_released(imaginary, platform_place_g_program_owner);
    platform_cell_released(real, platform_place_g_program_owner);
    platform_cell_released(shaped_room, platform_place_g_program_owner);
    platform_cell_released(tracks, platform_place_g_program_owner);

    printf("\n================================================================\n");
    printf("  Reading it\n");
    printf("================================================================\n");
    printf("\n  For a white signal every spectral bin is exponentially distributed about the\n");
    printf("  mean power, so the largest of n bins sits near ln(n) times the mean. That is\n");
    printf("  the expected column, and it is derived instead of chosen.\n");
    printf("\n  The comb control must show a peak at period 137 and a height far above the\n");
    printf("  expected column. If it does not, the spectrum cannot find a period that was\n");
    printf("  deliberately written in, and a flat reading on SHA256d would mean nothing at\n");
    printf("  all. That control is the whole reason the other two rows are readable.\n");
    printf("\n  The splitmix row is the other direction: it must come out near the expected\n");
    printf("  column, because a tool that reports a peak for everything is as useless as one\n");
    printf("  that reports none.\n");
    printf("\n  In build/bench, sound_0.wav is SHA256d, sound_1.wav the pseudorandom control, and\n");
    printf("  sound_2.wav the comb. The third should be audibly different from the first two.\n");
    return 0;
}
