// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// compression_job_test.cu: the encode as one job of the device (compression_encode_job) held to compression_encode.
// The chunks, the bits, each offset and each word of the stream must agree. The decode as one job
// (compression_decode_job) must give back the values that went in. The counts go below one chunk, across several
// chunks with a part chunk at the end, and with values large enough to escape. A count of 0 must error both ways
#include "../../../../../../../src/cu/engine/analysis/compression/compression.h"
#include "../../../../../../../src/platform/platform.h"

#include <cuda_runtime.h>

#include <cstring>

// the most values of a case, the words of its stream at max, and the values of a chunk
#define JOB_TEST_VALUES_MAX (5u * 4096u + 77u)
#define JOB_TEST_LIMBS_MAX (JOB_TEST_VALUES_MAX * 2u + 64u)
#define JOB_TEST_OFFSETS_MAX 64u

// the seeded words that each case draws its values from
static unsigned int s_seed = 0x9E3779B9u;

static unsigned int job_test_drawn(void)
{
    s_seed ^= s_seed << 13u;
    s_seed ^= s_seed >> 17u;
    s_seed ^= s_seed << 5u;
    return s_seed;
}

// one check, printed as it passes or fails
static unsigned int job_test_check(int held, const char *what)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, (held != 0) ? "  ok   " : "  FAIL ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return (held != 0) ? 0u : 1u;
}

// The answer of one encode: its chunks, its bits, and copies of its offsets and of its stream
struct JobTestCode
{
    unsigned long long chunks;
    unsigned long long bits;
    unsigned long long *offsets;
    unsigned int *stream;
};

// the answer copied out of the host cells of compression, which its next encode writes over
static int job_test_kept(const unsigned long long *offsets, const unsigned int *stream, JobTestCode *code)
{
    const unsigned long long limbs = (code->bits + 31ULL) / 32ULL;
    if ((code->chunks > JOB_TEST_OFFSETS_MAX) || (limbs > JOB_TEST_LIMBS_MAX))
    {
        return 0;
    }
    memcpy(code->offsets, offsets, (size_t)code->chunks * sizeof(unsigned long long));
    memcpy(code->stream, stream, (size_t)limbs * sizeof(unsigned int));
    return 1;
}

// the values encoded by compression_encode from the memory of the device, the answer copied into `code`
static long job_test_encoded(const int *values, unsigned long long count, JobTestCode *code)
{
    void *device = NULL;
    void *scratch = NULL;
    long answer = -1L;
    const size_t bytes = (size_t)((count != 0ULL) ? count : 1ULL) * sizeof(int);
    if ((cudaMalloc(&device, bytes) == cudaSuccess) && (cudaMalloc(&scratch, bytes) == cudaSuccess) &&
        (cudaMemcpy(device, values, (size_t)count * sizeof(int), cudaMemcpyHostToDevice) == cudaSuccess))
    {
        EngineError error;
        memset(&error, 0, sizeof(error));
        const unsigned long long *offsets = NULL;
        const unsigned int *stream = NULL;
        CompressionEncodeRequest encode;
        memset(&encode, 0, sizeof(encode));
        // Explicit casts read the memory of the device as the values and the scratch words
        encode.device_coefficients = (const int *)device;
        encode.count = count;
        encode.device_scratch = (unsigned int *)scratch;
        encode.chunks = &code->chunks;
        encode.bits = &code->bits;
        encode.offsets = &offsets;
        encode.stream = &stream;
        encode.error = &error;
        answer = compression_encode(&encode);
        if ((answer == 0L) && (job_test_kept(offsets, stream, code) == 0))
        {
            answer = -1L;
        }
    }
    cudaFree(scratch);
    cudaFree(device);
    return answer;
}

// the values encoded by compression_encode_job, the answer copied into `code`
static long job_test_encoded_job(const int *values, unsigned long long count, JobTestCode *code)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    const unsigned long long *offsets = NULL;
    const unsigned int *stream = NULL;
    CompressionEncodeJobRequest encode;
    memset(&encode, 0, sizeof(encode));
    encode.coefficients = values;
    encode.count = count;
    encode.chunks = &code->chunks;
    encode.bits = &code->bits;
    encode.offsets = &offsets;
    encode.stream = &stream;
    encode.error = &error;
    long answer = compression_encode_job(&encode);
    if ((answer == 0L) && (job_test_kept(offsets, stream, code) == 0))
    {
        answer = -1L;
    }
    return answer;
}

// the stream of `code` decoded by compression_decode_job into `values`
static long job_test_decoded_job(const JobTestCode *code, unsigned long long count, int *values)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    CompressionDecodeJobRequest decode;
    memset(&decode, 0, sizeof(decode));
    decode.offsets = code->offsets;
    decode.chunks = code->chunks;
    decode.stream = code->stream;
    decode.bits = code->bits;
    decode.count = count;
    decode.coefficients = values;
    decode.error = &error;
    return compression_decode_job(&decode);
}

// One case: the values encoded both ways and compared word for word, then decoded by the job and compared to the
// values that went in
static unsigned int job_test_case(const char *what, const int *values, unsigned long long count, JobTestCode *encoded,
                                  JobTestCode *encoded_job, int *decoded)
{
    int agree = (job_test_encoded(values, count, encoded) == 0L) &&
                (job_test_encoded_job(values, count, encoded_job) == 0L) && (encoded->chunks == encoded_job->chunks) &&
                (encoded->bits == encoded_job->bits);
    for (unsigned long long at = 0ULL; (agree != 0) && (at < encoded->chunks); at++)
    {
        agree = encoded->offsets[at] == encoded_job->offsets[at];
    }
    const unsigned long long limbs = (encoded->bits + 31ULL) / 32ULL;
    for (unsigned long long at = 0ULL; (agree != 0) && (at < limbs); at++)
    {
        agree = encoded->stream[at] == encoded_job->stream[at];
    }
    unsigned int failed = job_test_check(agree, what);
    agree = job_test_decoded_job(encoded_job, count, decoded) == 0L;
    for (unsigned long long at = 0ULL; (agree != 0) && (at < count); at++)
    {
        agree = decoded[at] == values[at];
    }
    failed += job_test_check(agree, "the decode job gives back the values that went in");
    return failed;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const unsigned int place = platform_place_g_program_owner;
    int *const small = (int *)platform_cell_taken(JOB_TEST_VALUES_MAX * sizeof(int), place, 1);
    int *const wide = (int *)platform_cell_taken(JOB_TEST_VALUES_MAX * sizeof(int), place, 1);
    int *const decoded = (int *)platform_cell_taken(JOB_TEST_VALUES_MAX * sizeof(int), place, 1);
    JobTestCode encoded;
    JobTestCode encoded_job;
    encoded.offsets =
        (unsigned long long *)platform_cell_taken(JOB_TEST_OFFSETS_MAX * sizeof(unsigned long long), place, 1);
    encoded.stream = (unsigned int *)platform_cell_taken(JOB_TEST_LIMBS_MAX * sizeof(unsigned int), place, 1);
    encoded_job.offsets =
        (unsigned long long *)platform_cell_taken(JOB_TEST_OFFSETS_MAX * sizeof(unsigned long long), place, 1);
    encoded_job.stream = (unsigned int *)platform_cell_taken(JOB_TEST_LIMBS_MAX * sizeof(unsigned int), place, 1);
    if ((small == NULL) || (wide == NULL) || (decoded == NULL) || (encoded.offsets == NULL) ||
        (encoded.stream == NULL) || (encoded_job.offsets == NULL) || (encoded_job.stream == NULL))
    {
        return 1;
    }
    for (unsigned int at = 0u; at < JOB_TEST_VALUES_MAX; at++)
    {
        // Explicit casts keep the low bits of the drawn words. The small values are of either sign. The wide values
        // are 28 bits of either sign, which pass the escape
        small[at] = (int)(job_test_drawn() % 64u) - 32;
        wide[at] = (int)(job_test_drawn() & 0x0FFFFFFFu) - 0x08000000;
    }
    for (unsigned int at = 0u; at < JOB_TEST_VALUES_MAX; at += 7u)
    {
        small[at] = wide[at];
    }

    unsigned int failed = 0u;
    failed += job_test_case("fewer values than a chunk encode as compression_encode encodes them", small, 1000ULL,
                            &encoded, &encoded_job, decoded);
    failed += job_test_case("values across five chunks and a part chunk encode as compression_encode encodes them",
                            small, JOB_TEST_VALUES_MAX, &encoded, &encoded_job, decoded);
    failed += job_test_case("values past the escape encode as compression_encode encodes them", wide,
                            JOB_TEST_VALUES_MAX, &encoded, &encoded_job, decoded);
    failed += job_test_check((job_test_encoded(small, 0ULL, &encoded) != 0L) &&
                                 (job_test_encoded_job(small, 0ULL, &encoded_job) != 0L),
                             "a count of 0 errors both ways");

    platform_cell_released(encoded_job.stream, place);
    platform_cell_released(encoded_job.offsets, place);
    platform_cell_released(encoded.stream, place);
    platform_cell_released(encoded.offsets, place);
    platform_cell_released(decoded, place);
    platform_cell_released(wide, place);
    platform_cell_released(small, place);
    compression_resident_release();
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  compression job test: 7 checks, ");
    mmgr_line_decimal(output_line, failed, 1u);
    mmgr_line_put(output_line, " failed\n");
    platform_line_out_ended();
    return (failed == 0u) ? 0 : 1;
}
