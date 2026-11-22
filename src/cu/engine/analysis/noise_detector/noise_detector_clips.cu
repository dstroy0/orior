// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// noise_detector_clips.cu: spikes and clipping
#include "noise_detector_internal.h"
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"

__device__ static void noise_spikes_flush(unsigned long long *cells, unsigned int level_bin,
                                          unsigned long long run[NOISE_SPIKE_SUMS])
{
    unsigned long long *const cell = &cells[level_bin * NOISE_SPIKE_SUMS];
    for (unsigned int sum = 0u; sum < NOISE_SPIKE_SUMS; sum += 1u)
    {
        if (run[sum] != 0ULL)
        {
            atomicAdd(&cell[sum], run[sum]);
        }
        run[sum] = 0ULL;
    }
}

// One thread a voxel. Every frame's value is counted. A frame with a frame on either side is a triple, binned by the
// mean of the two beside it, and a spike or a dip where it clears both by a threshold. A voxel whose least and max
// values agree is constant: counted, its value counted apart, and its place taken into the box.
__global__ static void noise_clips_kernel(const unsigned short *lanes, unsigned long long frames,
                                          unsigned long long depth, unsigned long long height, unsigned long long width,
                                          unsigned long long *counts, unsigned int *box, unsigned long long *values,
                                          unsigned long long *constant_values, unsigned long long *spikes)
{
    __shared__ unsigned long long cells[NOISE_SPIKE_CELLS];
    __shared__ unsigned int shared_values[NOISE_SHARED_VALUES];
    for (unsigned int entry = threadIdx.x; entry < NOISE_SPIKE_CELLS; entry += blockDim.x)
    {
        cells[entry] = 0ULL;
    }
    for (unsigned int entry = threadIdx.x; entry < NOISE_SHARED_VALUES; entry += blockDim.x)
    {
        shared_values[entry] = 0u;
    }
    __syncthreads();
    const unsigned long long voxels = depth * height * width;
    const unsigned long long jump = (unsigned long long)gridDim.x * blockDim.x;
    unsigned long long at_zero = 0ULL;
    unsigned long long at_top = 0ULL;
    for (unsigned long long voxel = ((unsigned long long)blockIdx.x * blockDim.x) + threadIdx.x; voxel < voxels;
         voxel += jump)
    {
        unsigned int least = NOISE_LANE_TOP;
        unsigned int max = 0u;
        unsigned long long run[NOISE_SPIKE_SUMS];
        for (unsigned int sum = 0u; sum < NOISE_SPIKE_SUMS; sum += 1u)
        {
            run[sum] = 0ULL;
        }
        unsigned int run_bin = 0u;
        unsigned int earlier = 0u;
        unsigned int middle = 0u;
        for (unsigned long long frame = 0ULL; frame < frames; frame += 1ULL)
        {
            const unsigned int value = lanes[(frame * voxels) + voxel];
            least = (value < least) ? value : least;
            max = (value > max) ? value : max;
            at_zero += (value == 0u) ? 1ULL : 0ULL;
            at_top += (value == NOISE_LANE_TOP) ? 1ULL : 0ULL;
            if (value < NOISE_SHARED_VALUES)
            {
                atomicAdd(&shared_values[value], 1u);
            }
            else
            {
                atomicAdd(&values[value], 1ULL);
            }
            if (frame >= 2ULL)
            {
                // the middle frame against the frames either side of it, binned by their mean
                const unsigned int summed_bin = (earlier + value) >> NOISE_LEVEL_BIN_SHIFT;
                const unsigned int level_bin = (summed_bin < NOISE_LEVEL_BINS) ? summed_bin : (NOISE_LEVEL_BINS - 1u);
                if ((level_bin != run_bin) && (run[NOISE_SPIKE_TRIPLES] != 0ULL))
                {
                    noise_spikes_flush(cells, run_bin, run);
                }
                run_bin = level_bin;
                const unsigned int higher = (earlier > value) ? earlier : value;
                const unsigned int lower = (earlier < value) ? earlier : value;
                run[NOISE_SPIKE_TRIPLES] += 1ULL;
                for (unsigned int threshold = 0u; threshold < NOISE_SPIKE_THRESHOLDS; threshold += 1u)
                {
                    const unsigned int apart = NOISE_SPIKE_LEAST << threshold;
                    run[1u + (2u * threshold)] += (middle >= (higher + apart)) ? 1ULL : 0ULL;
                    run[2u + (2u * threshold)] += ((middle + apart) <= lower) ? 1ULL : 0ULL;
                }
            }
            earlier = middle;
            middle = value;
        }
        if (run[NOISE_SPIKE_TRIPLES] != 0ULL)
        {
            noise_spikes_flush(cells, run_bin, run);
        }
        if (least != max)
        {
            continue;
        }
        atomicAdd(&counts[NOISE_CLIPS_CONSTANT], 1ULL);
        if (least == 0u)
        {
            atomicAdd(&counts[NOISE_CLIPS_CONSTANT_ZERO], 1ULL);
        }
        if (least == NOISE_LANE_TOP)
        {
            atomicAdd(&counts[NOISE_CLIPS_CONSTANT_TOP], 1ULL);
        }
        atomicAdd(&constant_values[least], 1ULL);
        // each coordinate is below its extent, which the host bounds under 2^32
        const unsigned int place[ENGINE_AXES] = {(unsigned int)(voxel / (height * width)),
                                                 (unsigned int)((voxel / width) % height),
                                                 (unsigned int)(voxel % width)};
        for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
        {
            atomicMin(&box[2u * axis], place[axis]);
            atomicMax(&box[(2u * axis) + 1u], place[axis]);
        }
    }
    __syncthreads();
    for (unsigned int entry = threadIdx.x; entry < NOISE_SPIKE_CELLS; entry += blockDim.x)
    {
        if (cells[entry] != 0ULL)
        {
            atomicAdd(&spikes[entry], cells[entry]);
        }
    }
    for (unsigned int entry = threadIdx.x; entry < NOISE_SHARED_VALUES; entry += blockDim.x)
    {
        if (shared_values[entry] != 0u)
        {
            atomicAdd(&values[entry], (unsigned long long)shared_values[entry]);
        }
    }
    if (at_zero != 0ULL)
    {
        atomicAdd(&counts[NOISE_CLIPS_AT_ZERO], at_zero);
    }
    if (at_top != 0ULL)
    {
        atomicAdd(&counts[NOISE_CLIPS_AT_TOP], at_top);
    }
}

long noise_clips_sample(const unsigned short *volume, const unsigned long long extent[4], NoiseClips *clips,
                        EngineError *error)
{
    const unsigned long long frames = extent[0];
    const unsigned long long depth = extent[1];
    const unsigned long long height = extent[2];
    const unsigned long long width = extent[3];
    const unsigned long long voxels = depth * height * width;
    const unsigned long long needed = (voxels + NOISE_DETECTOR_THREADS - 1ULL) / NOISE_DETECTOR_THREADS;
    const unsigned int blocks = (unsigned int)((needed < 65536ULL) ? needed : 65536ULL);
    // the voxels each thread takes in turn
    const unsigned long long rounds = (blocks != 0u) ? ((needed + blocks - 1ULL) / blocks) : 0ULL;
    // every count is at max the sample's voxel-frames; a block's shared counts are at max its threads' voxel-frames,
    // which an unsigned int represents. Every coordinate fits an unsigned int
    const int bounded = (frames >= 1ULL) && (voxels != 0ULL) && (frames <= (~0ULL / voxels)) &&
                        (depth <= 0xFFFFFFFFULL) && (height <= 0xFFFFFFFFULL) && (width <= 0xFFFFFFFFULL) &&
                        (rounds <= ((0xFFFFFFFFULL / NOISE_DETECTOR_THREADS) / frames));
    if (!NOISE_DETECTOR_CHECK(bounded, extent, error, ENGINE_ERROR_REQUEST))
    {
        return NOISE_DETECTOR_ERROR;
    }
    const size_t lane_bytes = (size_t)(frames * voxels) * sizeof(unsigned short);
    const size_t count_bytes = (size_t)NOISE_CLIPS_COUNTS * sizeof(unsigned long long);
    const size_t box_bytes = (size_t)NOISE_CLIPS_BOX * sizeof(unsigned int);
    const size_t value_bytes = (size_t)NOISE_VALUES * sizeof(unsigned long long);
    const size_t spike_bytes = (size_t)NOISE_SPIKE_CELLS * sizeof(unsigned long long);
    // an empty box: every least at the widest coordinate and every max at 0
    const unsigned int empty_box[NOISE_CLIPS_BOX] = {0xFFFFFFFFu, 0u, 0xFFFFFFFFu, 0u, 0xFFFFFFFFu, 0u};
    unsigned short *device_lanes = NULL;
    unsigned long long *device_counts = NULL;
    unsigned int *device_box = NULL;
    unsigned long long *device_values = NULL;
    unsigned long long *device_constant_values = NULL;
    unsigned long long *device_spikes = NULL;
    int ok = NOISE_DETECTOR_STATUS_CHECK(cudaMalloc((void **)&device_lanes, lane_bytes), &device_lanes, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMalloc((void **)&device_counts, count_bytes), &device_counts, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMalloc((void **)&device_box, box_bytes), &device_box, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMalloc((void **)&device_values, value_bytes), &device_values, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMalloc((void **)&device_constant_values, value_bytes),
                                         &device_constant_values, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMalloc((void **)&device_spikes, spike_bytes), &device_spikes, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemset(device_counts, 0, count_bytes), device_counts, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemset(device_values, 0, value_bytes), device_values, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemset(device_constant_values, 0, value_bytes), device_constant_values,
                                         error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemset(device_spikes, 0, spike_bytes), device_spikes, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemcpy(device_box, empty_box, box_bytes, cudaMemcpyHostToDevice),
                                         device_box, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemcpy(device_lanes, volume, lane_bytes, cudaMemcpyHostToDevice),
                                         device_lanes, error);
    if (ok != 0)
    {
        noise_clips_kernel<<<blocks, NOISE_DETECTOR_THREADS>>>(device_lanes, frames, depth, height, width,
                                                               device_counts, device_box, device_values,
                                                               device_constant_values, device_spikes);
        ok = NOISE_DETECTOR_STATUS_CHECK(cudaGetLastError(), device_counts, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemcpy(clips->counts, device_counts, count_bytes, cudaMemcpyDeviceToHost),
                                         clips->counts, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemcpy(clips->box, device_box, box_bytes, cudaMemcpyDeviceToHost),
                                         clips->box, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemcpy(clips->values, device_values, value_bytes, cudaMemcpyDeviceToHost),
                                         clips->values, error) &&
             NOISE_DETECTOR_STATUS_CHECK(
                 cudaMemcpy(clips->constant_values, device_constant_values, value_bytes, cudaMemcpyDeviceToHost),
                 clips->constant_values, error) &&
             NOISE_DETECTOR_STATUS_CHECK(cudaMemcpy(clips->spikes, device_spikes, spike_bytes, cudaMemcpyDeviceToHost),
                                         clips->spikes, error);
    }
    cudaFree(device_lanes);
    cudaFree(device_counts);
    cudaFree(device_box);
    cudaFree(device_values);
    cudaFree(device_constant_values);
    cudaFree(device_spikes);
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}

// the least and max values the sample contains, and how many values between them it never contains
static void noise_clips_range(const NoiseClips *clips, unsigned int *least, unsigned int *max,
                              unsigned long long *empty)
{
    int found = 0;
    *least = 0u;
    *max = 0u;
    for (unsigned int value = 0u; value < NOISE_VALUES; value += 1u)
    {
        if (clips->values[value] == 0ULL)
        {
            continue;
        }
        *least = (found != 0) ? *least : value;
        *max = value;
        found = 1;
    }
    *empty = 0ULL;
    for (unsigned int value = *least; (found != 0) && (value <= *max); value += 1u)
    {
        *empty += (clips->values[value] == 0ULL) ? 1ULL : 0ULL;
    }
}

// the triples, spikes and dips pooled over the summary's level bins, means 40 to 199
void noise_spikes_pooled(const NoiseClips *clips, unsigned long long pooled[NOISE_SPIKE_SUMS])
{
    mmgr_bytes_set(pooled, 0, (size_t)NOISE_SPIKE_SUMS * sizeof(unsigned long long));
    for (unsigned int level_bin = NOISE_SUMMARY_FIRST_BIN; level_bin <= NOISE_SUMMARY_LAST_BIN; level_bin += 1u)
    {
        for (unsigned int sum = 0u; sum < NOISE_SPIKE_SUMS; sum += 1u)
        {
            pooled[sum] += clips->spikes[(level_bin * NOISE_SPIKE_SUMS) + sum];
        }
    }
}

void noise_clips_summary(const char *name, const NoiseClips *clips)
{
    unsigned int least = 0u;
    unsigned int max = 0u;
    unsigned long long empty = 0ULL;
    noise_clips_range(clips, &least, &max, &empty);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, name, 24u);
        mmgr_line_put(output_line, " values ");
        mmgr_line_decimal(output_line, least, 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, max, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, empty, 1u);
        mmgr_line_put(output_line, " never present between; at 0: ");
        mmgr_line_decimal(output_line, clips->counts[NOISE_CLIPS_AT_ZERO], 1u);
        mmgr_line_put(output_line, ", at the top: ");
        mmgr_line_decimal(output_line, clips->counts[NOISE_CLIPS_AT_TOP], 1u);
        mmgr_line_put(output_line, "; constant voxels: ");
        mmgr_line_decimal(output_line, clips->counts[NOISE_CLIPS_CONSTANT], 1u);
        mmgr_line_put(output_line, " (at 0: ");
        mmgr_line_decimal(output_line, clips->counts[NOISE_CLIPS_CONSTANT_ZERO], 1u);
        mmgr_line_put(output_line, ", at the top: ");
        mmgr_line_decimal(output_line, clips->counts[NOISE_CLIPS_CONSTANT_TOP], 1u);
        mmgr_line_put(output_line, ")");
        platform_line_out_ended();
    }
    if (clips->counts[NOISE_CLIPS_CONSTANT] != 0ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, ", within z ");
        mmgr_line_decimal(output_line, clips->box[0], 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, clips->box[1], 1u);
        mmgr_line_put(output_line, ", y ");
        mmgr_line_decimal(output_line, clips->box[2], 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, clips->box[3], 1u);
        mmgr_line_put(output_line, ", x ");
        mmgr_line_decimal(output_line, clips->box[4], 1u);
        mmgr_line_put(output_line, " to ");
        mmgr_line_decimal(output_line, clips->box[5], 1u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned long long pooled[NOISE_SPIKE_SUMS];
    noise_spikes_pooled(clips, pooled);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "", 24u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, pooled[NOISE_SPIKE_TRIPLES], 1u);
        mmgr_line_put(output_line, " triples at means 40 to 199; spikes against dips at");
        platform_line_out_ended();
    }
    for (unsigned int threshold = 0u; threshold < NOISE_SPIKE_THRESHOLDS; threshold += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, NOISE_SPIKE_LEAST << threshold, 1u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, pooled[1u + (2u * threshold)], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, pooled[2u + (2u * threshold)], 1u);
        mmgr_line_put(output_line, ";");
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_line_flushed();
}

int noise_clips_rows(PlatformFile *const tables[NOISE_CLIPS_TABLES], const char *name, const NoiseClips *clips)
{
    unsigned int least = 0u;
    unsigned int max = 0u;
    unsigned long long empty = 0ULL;
    noise_clips_range(clips, &least, &max, &empty);
    unsigned long long formed_mark_1;
    int ok =
        (formed_mark_1 = platform_file_mark(tables[NOISE_TABLE_CLIPS]),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), name),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), least, 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), max, 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), empty, 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->counts[NOISE_CLIPS_AT_ZERO], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->counts[NOISE_CLIPS_AT_TOP], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->counts[NOISE_CLIPS_CONSTANT], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->counts[NOISE_CLIPS_CONSTANT_ZERO], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->counts[NOISE_CLIPS_CONSTANT_TOP], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->box[0], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->box[1], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->box[2], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->box[3], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->box[4], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\t"),
         mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_CLIPS]), clips->box[5], 1u),
         mmgr_line_put(platform_file_line(tables[NOISE_TABLE_CLIPS]), "\n"),
         platform_file_written_since(tables[NOISE_TABLE_CLIPS], formed_mark_1)) > 0;
    for (unsigned int value = 0u; ok && (value < NOISE_VALUES); value += 1u)
    {
        if (clips->values[value] == 0ULL)
        {
            continue;
        }
        unsigned long long formed_mark_2;
        ok = (formed_mark_2 = platform_file_mark(tables[NOISE_TABLE_VALUES]),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_VALUES]), name),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_VALUES]), "\t"),
              mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_VALUES]), value, 1u),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_VALUES]), "\t"),
              mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_VALUES]), clips->values[value], 1u),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_VALUES]), "\t"),
              mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_VALUES]), clips->constant_values[value], 1u),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_VALUES]), "\n"),
              platform_file_written_since(tables[NOISE_TABLE_VALUES], formed_mark_2)) > 0;
    }
    for (unsigned int level_bin = 0u; ok && (level_bin < NOISE_LEVEL_BINS); level_bin += 1u)
    {
        const unsigned long long *const cell = &clips->spikes[level_bin * NOISE_SPIKE_SUMS];
        if (cell[NOISE_SPIKE_TRIPLES] == 0ULL)
        {
            continue;
        }
        unsigned long long formed_mark_3;
        ok = (formed_mark_3 = platform_file_mark(tables[NOISE_TABLE_SPIKES]),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_SPIKES]), name),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_SPIKES]), "\t"),
              mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_SPIKES]),
                                level_bin << (NOISE_LEVEL_BIN_SHIFT - 1u), 1u),
              mmgr_line_put(platform_file_line(tables[NOISE_TABLE_SPIKES]), "\t"),
              mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_SPIKES]), cell[NOISE_SPIKE_TRIPLES], 1u),
              platform_file_written_since(tables[NOISE_TABLE_SPIKES], formed_mark_3)) > 0;
        for (unsigned int sum = 1u; ok && (sum < NOISE_SPIKE_SUMS); sum += 1u)
        {
            unsigned long long formed_mark_4;
            ok = (formed_mark_4 = platform_file_mark(tables[NOISE_TABLE_SPIKES]),
                  mmgr_line_put(platform_file_line(tables[NOISE_TABLE_SPIKES]), "\t"),
                  mmgr_line_decimal(platform_file_line(tables[NOISE_TABLE_SPIKES]), cell[sum], 1u),
                  platform_file_written_since(tables[NOISE_TABLE_SPIKES], formed_mark_4)) > 0;
        }
        unsigned long long formed_mark_5;
        ok = ok && ((formed_mark_5 = platform_file_mark(tables[NOISE_TABLE_SPIKES]),
                     mmgr_line_put(platform_file_line(tables[NOISE_TABLE_SPIKES]), "\n"),
                     platform_file_written_since(tables[NOISE_TABLE_SPIKES], formed_mark_5)) > 0);
    }
    return ok;
}
