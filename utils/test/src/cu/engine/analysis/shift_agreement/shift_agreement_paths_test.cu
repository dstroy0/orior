// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shift_agreement_paths_test.cu: the path of the shift agreement on the device, with the engine linked into the
// program. The test draws seeded pairs of bit volumes, each later volume the earlier one shifted and with some bits
// flipped. Each pair runs through shift_agreement_run and through shift_agreement_host. The second pair starts from the
// later volume of the first, so the device takes its kept transform. The lag, the padding, the agreement and the counts
// of the device must equal those of the host, and their signum must equal the signum of the device path
#include "../../../../../../../src/cu/engine/analysis/shift_agreement/shift_agreement.h"
#include "../../../../../../../src/cu/engine/engine.h"
#include "../../../../../../../src/cu/engine/runtime/obsignatio/obsignatio.h"
#include "../../../../../../../src/platform/platform.h"

#include "memoria_operor/memoria_operor.h"

// the cases of the test, the most voxels of a case and the most padded entries of its counts
#define AGREEMENT_TEST_CASES 4u
#define AGREEMENT_TEST_VOXELS_MAX 8192u
#define AGREEMENT_TEST_COUNTS_MAX 65536u

// the pool of the test, for its volumes and its counts
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 27u;

// the seeded words that the volumes draw from
static unsigned int s_seed = 0xA54FF53Au;

static unsigned int agreement_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// the count of the checks, and one check, printed as it passes or fails
static unsigned int s_checks = 0u;

static unsigned int agreement_test_check(int held, const char *what)
{
    s_checks += 1u;
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// One case: its extent and its two volumes, a bit a voxel
typedef struct
{
    unsigned int axes;
    unsigned int extents[SHIFT_AGREEMENT_AXES];
    unsigned int voxels;
    unsigned long long before[AGREEMENT_TEST_VOXELS_MAX / 64u];
    unsigned long long after[AGREEMENT_TEST_VOXELS_MAX / 64u];
} AgreementTestCase;

// The answers of one path of one case: the lag, the padding, the agreement and the counts
typedef struct
{
    long returned;
    int lag[SHIFT_AGREEMENT_AXES];
    unsigned int padded[SHIFT_AGREEMENT_AXES];
    unsigned int agreement;
    unsigned int counts[AGREEMENT_TEST_COUNTS_MAX];
} AgreementTestAnswer;

// the bit of a volume at a voxel
static unsigned int agreement_test_bit(const unsigned long long *volume, unsigned int voxel)
{
    // Explicit cast takes one bit as an unsigned int
    return (unsigned int)((volume[voxel / 64u] >> (voxel % 64u)) & 1ULL);
}

// A volume of `axes` axes at `extents`. About 1 voxel of 3 holds a 1
static void agreement_test_drawn_volume(AgreementTestCase *one, unsigned int axes, const unsigned int *extents)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = one, .bytes = sizeof(*one), .val = 0);
    one->axes = axes;
    one->voxels = 1u;
    for (unsigned int axis = 0u; axis < axes; axis++)
    {
        one->extents[axis] = extents[axis];
        one->voxels *= extents[axis];
    }
    for (unsigned int voxel = 0u; voxel < one->voxels; voxel++)
    {
        one->before[voxel / 64u] |= ((agreement_test_drawn() % 3u) == 0u) ? (1ULL << (voxel % 64u)) : 0ULL;
    }
}

// The later volume: the earlier volume shifted by `shift`, with about 1 bit of 50 flipped. A voxel whose source falls
// outside the volume holds a 1 in about 1 of 3
static void agreement_test_shifted(AgreementTestCase *one, const int *shift)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = one->after, .bytes = sizeof(one->after), .val = 0);
    for (unsigned int voxel = 0u; voxel < one->voxels; voxel++)
    {
        unsigned int rest = voxel;
        unsigned int source = 0u;
        unsigned int stride = 1u;
        int inside = 1;
        for (unsigned int axis = one->axes; axis > 0u; axis--)
        {
            // Explicit casts take a coordinate and an extent below 2^31, which an int holds
            const int coordinate = (int)(rest % one->extents[axis - 1u]) - shift[axis - 1u];
            rest /= one->extents[axis - 1u];
            inside = inside && (coordinate >= 0) && (coordinate < (int)one->extents[axis - 1u]);
            // Explicit cast takes a coordinate inside the extent, and so not negative, as an unsigned int
            source += inside ? ((unsigned int)coordinate * stride) : 0u;
            stride *= one->extents[axis - 1u];
        }
        unsigned int bit =
            inside ? agreement_test_bit(one->before, source) : (((agreement_test_drawn() % 3u) == 0u) ? 1u : 0u);
        bit ^= ((agreement_test_drawn() % 50u) == 0u) ? 1u : 0u;
        one->after[voxel / 64u] |= (unsigned long long)bit << (voxel % 64u);
    }
}

// the request of a case, its answers written to `answer`
static void agreement_test_request(AgreementTestCase *one, AgreementTestAnswer *answer, ShiftAgreementRequest *request)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = answer, .bytes = sizeof(*answer), .val = 0);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request, .bytes = sizeof(*request), .val = 0);
    request->axes = one->axes;
    for (unsigned int axis = 0u; axis < one->axes; axis++)
    {
        request->extents[axis] = one->extents[axis];
        request->weights[axis] = 1u;
    }
    request->before = one->before;
    request->after = one->after;
    request->counts = answer->counts;
}

// the answers of a request copied into its answer
static void agreement_test_kept(const ShiftAgreementRequest *request, long returned, AgreementTestAnswer *answer)
{
    answer->returned = returned;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = answer->lag, .src = request->lag, .bytes = sizeof(answer->lag));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = answer->padded, .src = request->padded, .bytes = sizeof(answer->padded));
    answer->agreement = request->agreement;
}

// the padded entries of the counts of an answer, the product of its padding
static unsigned long long agreement_test_entries(const AgreementTestAnswer *answer, unsigned int axes)
{
    unsigned long long entries = 1ULL;
    for (unsigned int axis = 0u; axis < axes; axis++)
    {
        entries *= answer->padded[axis];
    }
    return entries;
}

// the first 8 bytes of the signum of `count` bytes as a word, little end first, or 0 where the signum errors
static unsigned long long agreement_test_signum(const unsigned char *bytes, unsigned long long count)
{
    unsigned char signum[OBSIGNATIO_SIGNUM_BYTES];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const ObsignatioSignumRequest request = {bytes, count, NULL, OBSIGNATIO_MODE_HASH, signum, OBSIGNATIO_SIGNUM_BYTES,
                                             &error};
    if ((count == 0ULL) || (obsignatio_signum(&request) != 0L))
    {
        return 0ULL;
    }
    unsigned long long word = 0ULL;
    for (unsigned int at = 0u; at < 8u; at++)
    {
        word |= (unsigned long long)signum[at] << (8u * at);
    }
    return word;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    // the first word of the signum of the answers of the device over every case, the answer of the device path
    static const unsigned long long SIGNUM = 0xd03ac1812905f1adULL;
    static const char *const NAMES[AGREEMENT_TEST_CASES] = {
        "three axes", "three axes from the later volume of the first, the kept transform", "two axes", "one axis"};
    static const unsigned int THREE[3] = {6u, 24u, 30u};
    static const unsigned int TWO[2] = {40u, 50u};
    static const unsigned int ONE[1] = {300u};
    static const int SHIFTS[AGREEMENT_TEST_CASES][3] = {{1, -3, 2}, {0, 2, -1}, {-4, 5, 0}, {7, 0, 0}};
    const unsigned int place = platform_place_g_program_owner;
    AgreementTestCase *const one = (AgreementTestCase *)platform_cell_taken(sizeof(AgreementTestCase), place, 1);
    AgreementTestAnswer *const answers =
        (AgreementTestAnswer *)platform_cell_taken(2u * sizeof(AgreementTestAnswer), place, 1);
    unsigned char *const signed_bytes =
        (unsigned char *)platform_cell_taken(AGREEMENT_TEST_CASES * sizeof(AgreementTestAnswer), place, 1);
    if ((one == NULL) || (answers == NULL) || (signed_bytes == NULL))
    {
        return 1;
    }
    unsigned long long signed_count = 0ULL;
    unsigned int failed = 0u;
    for (unsigned int at = 0u; at < AGREEMENT_TEST_CASES; at++)
    {
        if (at == 1u)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = one->before, .src = one->after, .bytes = sizeof(one->before));
        }
        else
        {
            const unsigned int axes = (at == 0u) ? 3u : ((at == 2u) ? 2u : 1u);
            agreement_test_drawn_volume(one, axes, (at == 0u) ? THREE : ((at == 2u) ? TWO : ONE));
        }
        agreement_test_shifted(one, SHIFTS[at]);
        ShiftAgreementRequest request;
        agreement_test_request(one, &answers[0], &request);
        agreement_test_kept(&request, shift_agreement_host(&request), &answers[0]);
        agreement_test_request(one, &answers[1], &request);
        agreement_test_kept(&request, shift_agreement_run(&request), &answers[1]);
        const unsigned long long entries = agreement_test_entries(&answers[0], one->axes);
        const int sized = (answers[0].returned == 0L) && (entries <= AGREEMENT_TEST_COUNTS_MAX);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, NAMES[at]);
            mmgr_line_put(output_line, ": agreement ");
            mmgr_line_decimal(output_line, answers[0].agreement, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, one->voxels, 1u);
            mmgr_line_put(output_line, " voxels\n");
            platform_line_out_ended();
        }
        failed += agreement_test_check(sized, "the host answers the case");
        failed += agreement_test_check(
            sized && (answers[1].returned == 0L) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = answers[1].lag, .other = answers[0].lag,
                            .bytes = sizeof(answers[0].lag)) == 0) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = answers[1].padded, .other = answers[0].padded,
                            .bytes = sizeof(answers[0].padded)) == 0) &&
                (answers[1].agreement == answers[0].agreement) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = answers[1].counts, .other = answers[0].counts,
                            .bytes = (size_t)entries * sizeof(unsigned int)) == 0),
            "the device gives the lag, the padding, the agreement and the counts of the host");
        int found = 1;
        for (unsigned int axis = 0u; axis < one->axes; axis++)
        {
            found = found && (answers[0].lag[axis] == SHIFTS[at][axis]);
        }
        failed += agreement_test_check(found, "the lag is the shift of the case");
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &signed_bytes[signed_count], .src = &answers[1],
                   .bytes = sizeof(AgreementTestAnswer));
        signed_count += sizeof(AgreementTestAnswer);
    }
    const unsigned long long word = agreement_test_signum(signed_bytes, signed_count);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the answers of the device: signum 0x");
        mmgr_line_hex(output_line, word, 16u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    failed += agreement_test_check((word != 0ULL) && (word == SIGNUM),
                                   "the answers of the device are the answer of the device path");
    platform_cell_released(signed_bytes, place);
    platform_cell_released(answers, place);
    platform_cell_released(one, place);
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  shift agreement paths test: ");
    mmgr_line_decimal(output_line, s_checks, 1u);
    mmgr_line_put(output_line, " checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
