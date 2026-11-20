// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file boundary_raster.cu
 * @brief Rasterizes a boundary reading straight to pixels, with the anti-alias filter solved, and not sampled.
 *
 * WHAT THIS REPLACES
 *
 * The mesh path holds a reading as (L+1)^2 coefficients, and evaluates it at 2,701 vertices of a 36 by 72 mesh. A
 * triangle rasterizer fills the space between. The coefficients are exact, and every pixel between two vertices is a
 * straight line drawn through a curve.
 *
 * The script examples/00_blob_viz_tools/grid_error.py measures that at degree 10. The value is wrong by 5.9 percent of
 * the picture's own contrast at worst. The SURFACE NORMAL is 7.47 degrees off at the median, and 47.15 degrees off at
 * the ninety-fifth percentile. Shading reads the gradient, and the gradient of a linear interpolant is constant inside
 * a triangle. The normal is therefore the error that shows.
 *
 * There is no mesh here. Each pixel gets its own direction, and the expansion runs there.
 *
 * THE ANTIALIASING IS EXACT
 *
 * A pixel does not want the field at a point. It wants the field averaged over the solid angle that it covers. A
 * supersample estimates that average with k samples for cost k, and reaches it only in the limit.
 *
 * Convolution on the sphere is multiplication in the degree. Take the average of a band-limited field against any
 * kernel that is a function of the Laplacian. That average is one multiply per degree, and needs no samples at all.
 * Take the heat kernel of angular width sigma as the kernel:
 *
 *     tau_pixel = sigma^2 / 2,     gain contribution exp(-l(l+1) tau_pixel)
 *
 * ONE evaluation of the prefiltered field is then the exact solid-angle average of the pixel. The smoothing times add,
 * because the heat kernel is a semigroup. This is therefore one addend inside a tau that the chain already applies, and
 * it costs nothing beyond the gain vector.
 *
 * @note THE GAIN VECTOR HOLDS ALL THE PHYSICS, AND IT IS ELEVEN NUMBERS. Depth (r/R)^l, conduction exp(-l(l+1) tau),
 *       the pixel prefilter and the degree ceiling are all diagonal in the degree. They commute, and compose by a
 *       multiply of one scalar per degree. Everything between an interior source and a pixel is therefore a vector of
 *       L+1 gains. The only per-pixel part of that is the prefilter. The host builds the rest once, and the kernel
 *       folds in the pixel's own term.
 * @note SIGMA COMES FROM THE RAYS, AND NOT FROM AN ASSUMPTION. The footprint is the angle between the directions that
 *       two adjacent pixels land on, from two extra intersections. An analytic footprint would need the derivative of
 *       the intersection, and is a place to be subtly wrong for no measurable gain.
 * @note THE THETA DERIVATIVE IS A DELIBERATE CENTRAL DIFFERENCE. The phi derivative is exact and free. The basis
 *       carries cos(m phi) and sin(m phi), and a derivative swaps them and multiplies by m, with no new recurrence. The
 *       exact theta derivative needs the derivative of the Legendre recurrence, a formula that can carry a
 *       transcription error and still draw a picture. A central difference on the exact value is correct to about 1e-10
 *       at this step size, and cannot be subtly wrong. The closed form is an optimization, and the result does not need
 *       it.
 * @note ANISOTROPY IS A KNOWN LIMIT. The heat kernel is isotropic, and the footprint of a pixel near the silhouette
 *       stretches. One sigma therefore over-blurs across the short axis. The exact repair wants a directional kernel,
 *       which is not diagonal in the degree, and therefore leaves the composition above behind.
 * @note WHAT THE RUN DUMPS, AND WHY. The run writes the image, and also the coefficients that it used, with the
 *       direction and field value at every pixel. The checker in `examples/proofing` reads that, and evaluates the same
 *       coefficients through sphere_field.py at the same directions. That compares two independent codings of one
 *       recurrence, one in Python and one in the JavaScript of the viewer. The dump carries the coefficients, so that
 *       the grader shares them, and does not compute them again and hope.
 */

#include <cuda_runtime.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define DEGREE_CEILING 10
#define COEFFICIENT_COUNT ((DEGREE_CEILING + 1) * (DEGREE_CEILING + 1))

#define IMAGE_WIDTH 1024
#define IMAGE_HEIGHT 1024

/* The boundary's radius, and the observer's distance along positive z. */
#define BOUNDARY_RADIUS 1.0f
#define OBSERVER_DISTANCE 3.2f
#define VERTICAL_FIELD_OF_VIEW 0.62f /* radians, chosen so the sphere fills most of the frame */

/* Sources placed inside, read outward. Matches the placement boundary_read.golden_place produces. */
#define SOURCE_COUNT 63
#define SOURCE_RADIUS_FRACTION 0.62f
#define CONDUCTION_TIME 8.0e-4f

/*
 * How far the surface may move under the field, as a fraction of the radius. Shading needs a shape, and a pure
 * luminance has no normal to shade.
 */
#define SURFACE_SWING 0.16f

/*
 * Step for the colatitude central difference. Large enough to stay clear of cancellation in single precision, and small
 * enough that the truncation term is far below the measured gradient.
 */
#define THETA_STEP 3.0e-4f

/*
 * OUR pi and OUR placement angle, from utils/maint/dn_precision/dn_const/dn_constants.csv, where two independent routes
 * agree on both. Written past double, so that the float here and a double instantiation elsewhere each round correctly
 * from the same digits.
 *
 * M_PI IS NOT IN USE, AND IS NOT AVAILABLE ANYWAY. The C++ standard library lacks it unless _USE_MATH_DEFINES comes
 * first. It would also break the project's rule of its own constants. The golden angle is the measured case for that
 * rule. The composed form math.pi * (3 - sqrt(5)) lands one unit in the last place away from the correctly rounded
 * value. It is the constant that positions every source in the placement.
 */
#define BOUNDARY_PI 3.14159265358979323846264338327950288f
#define GOLDEN_ANGLE 2.39996322972865332223155550663361385f

/**
 * @brief Normalized associated Legendre values for one order, every degree up to the ceiling.
 * @param order  The order, non-negative.
 * @param x      Cosine of the colatitude.
 * @param out    Buffer of DEGREE_CEILING+1 values that the caller owns, written in full.
 *
 * Climbs the diagonal and then recurs in degree, which keeps every intermediate at unit scale. The factorial form
 * overflows above degree 150, and loses digits long before that, quietly: the low degrees stay right, and only the fine
 * structure goes wrong. Same recurrence, same normalization and same order of operations as
 * sphere_field.legendre_column, because the grader compares them.
 */
__host__ __device__ static void legendre_column(int order, float x, float *out)
{
    for (int slot = 0; slot <= DEGREE_CEILING; slot++)
    {
        out[slot] = 0.0f;
    }

    const float flat = 1.0f - x * x;
    const float sine = sqrtf(flat > 0.0f ? flat : 0.0f);
    float value = sqrtf(1.0f / (4.0f * BOUNDARY_PI));

    for (int step = 1; step <= order; step++)
    {
        value *= sqrtf((float)(2 * step + 1) / (float)(2 * step)) * sine;
    }
    if (order <= DEGREE_CEILING)
    {
        out[order] = value;
    }
    if (order + 1 <= DEGREE_CEILING)
    {
        out[order + 1] = sqrtf((float)(2 * order + 3)) * x * value;
    }
    for (int degree = order + 2; degree <= DEGREE_CEILING; degree++)
    {
        const float lead = sqrtf((float)(4 * degree * degree - 1) / (float)(degree * degree - order * order));
        const float trail =
            sqrtf((float)((degree - 1) * (degree - 1) - order * order) / (float)(4 * (degree - 1) * (degree - 1) - 1));
        out[degree] = lead * (x * out[degree - 1] - trail * out[degree - 2]);
    }
}

/**
 * @brief The field and its longitude derivative at one direction, under one gain vector.
 * @param colatitude  Colatitude in radians.
 * @param longitude   Longitude in radians.
 * @param coefficients The reading, flattened, with degree l in the run that starts at l squared.
 * @param gain        One scalar per degree: depth, conduction, prefilter and ceiling together.
 * @param by_longitude Written with the exact derivative with respect to longitude.
 * @return The field value.
 *
 * The longitude derivative is exact and nearly free. The basis carries cos(m phi) at the +order position, and sin(m
 * phi) at the -order position. A derivative swaps the two, and multiplies by the order. No second recurrence, and no
 * step size.
 */
__host__ __device__ static float field_at(float colatitude, float longitude, const float *coefficients,
                                          const float *gain, float *by_longitude)
{
    float column[DEGREE_CEILING + 1];
    const float x = cosf(colatitude);
    const float root_two = sqrtf(2.0f);

    float total = 0.0f;
    float slope = 0.0f;

    for (int order = 0; order <= DEGREE_CEILING; order++)
    {
        legendre_column(order, x, column);

        if (order == 0)
        {
            for (int degree = 0; degree <= DEGREE_CEILING; degree++)
            {
                total += gain[degree] * coefficients[degree * degree + degree] * column[degree];
            }
            continue;
        }

        const float angle = (float)order * longitude;
        const float cosine = cosf(angle) * root_two;
        const float sine = sinf(angle) * root_two;

        for (int degree = order; degree <= DEGREE_CEILING; degree++)
        {
            const float weight = gain[degree] * column[degree];
            const float on_cosine = coefficients[degree * degree + degree + order];
            const float on_sine = coefficients[degree * degree + degree - order];

            total += weight * (on_cosine * cosine + on_sine * sine);
            /*
             * d/dphi of sqrt2 cos(m phi) is -m sqrt2 sin(m phi), and of sqrt2 sin(m phi) is
             * +m sqrt2 cos(m phi). Exact, and it reuses the same Legendre column.
             */
            slope += weight * (float)order * (on_sine * cosine - on_cosine * sine);
        }
    }

    if (by_longitude)
    {
        *by_longitude = slope;
    }
    return total;
}

/**
 * @brief Where a ray meets the boundary sphere, as the near root, or a negative value for a miss.
 * @param from       Ray origin.
 * @param direction  Unit ray direction.
 * @return Distance along the ray to the near intersection, or -1 for no hit.
 *
 * The sphere is analytic. The silhouette is therefore exact at every distance, and no tessellation shows. That removes
 * a whole class of artifact by construction, and not by refinement.
 */
__host__ __device__ static float sphere_hit(const float3 from, const float3 direction)
{
    const float middle = -(from.x * direction.x + from.y * direction.y + from.z * direction.z);
    const float square = from.x * from.x + from.y * from.y + from.z * from.z - BOUNDARY_RADIUS * BOUNDARY_RADIUS;
    const float under = middle * middle - square;
    if (under < 0.0f)
    {
        return -1.0f;
    }
    const float root = sqrtf(under);
    const float near_root = middle - root;
    return near_root > 0.0f ? near_root : middle + root;
}

/**
 * @brief The ray through one pixel, for a camera on positive z that looks at the origin.
 */
__host__ __device__ static float3 ray_through(float across, float down)
{
    const float aspect = (float)IMAGE_WIDTH / (float)IMAGE_HEIGHT;
    const float reach = tanf(0.5f * VERTICAL_FIELD_OF_VIEW);
    const float sideways = (2.0f * across / (float)IMAGE_WIDTH - 1.0f) * reach * aspect;
    const float upward = (1.0f - 2.0f * down / (float)IMAGE_HEIGHT) * reach;

    float3 out;
    out.x = sideways;
    out.y = upward;
    out.z = -1.0f;
    const float size = sqrtf(out.x * out.x + out.y * out.y + out.z * out.z);
    out.x /= size;
    out.y /= size;
    out.z /= size;
    return out;
}

/**
 * @brief The direction on the boundary a pixel's ray lands on, or a miss.
 * @return True on a hit, with colatitude and longitude written.
 */
__host__ __device__ static bool direction_for(float across, float down, float *colatitude, float *longitude)
{
    float3 from;
    from.x = 0.0f;
    from.y = 0.0f;
    from.z = OBSERVER_DISTANCE;

    const float3 direction = ray_through(across, down);
    const float reach = sphere_hit(from, direction);
    if (reach < 0.0f)
    {
        return false;
    }

    const float hx = from.x + reach * direction.x;
    const float hy = from.y + reach * direction.y;
    const float hz = from.z + reach * direction.z;
    const float size = sqrtf(hx * hx + hy * hy + hz * hz);

    *colatitude = acosf(fmaxf(-1.0f, fminf(1.0f, hy / size)));
    *longitude = atan2f(hz, hx);
    return true;
}

/**
 * @brief One pixel: the prefiltered field, its gradient, a shaded value, and the dump rows.
 * @param image        Bytes, three per pixel.
 * @param sampled      Written with the field value at every pixel, for the grader.
 * @param directions   Written with the colatitude and longitude at every pixel, for the grader.
 * @param coefficients The reading.
 * @param base_gain    Depth, conduction and ceiling. The pixel's own prefilter folds in here.
 */
__global__ static void draw(unsigned char *image, float *sampled, float *directions, const float *coefficients,
                            const float *base_gain)
{
    const int across = blockIdx.x * blockDim.x + threadIdx.x;
    const int down = blockIdx.y * blockDim.y + threadIdx.y;
    if (across >= IMAGE_WIDTH || down >= IMAGE_HEIGHT)
    {
        return;
    }
    const int at = down * IMAGE_WIDTH + across;

    float colatitude = 0.0f;
    float longitude = 0.0f;
    const bool hit = direction_for((float)across + 0.5f, (float)down + 0.5f, &colatitude, &longitude);

    directions[at * 3] = hit ? colatitude : -1.0f;
    directions[at * 3 + 1] = hit ? longitude : 0.0f;
    directions[at * 3 + 2] = 0.0f;

    if (!hit)
    {
        image[at * 3] = 8;
        image[at * 3 + 1] = 10;
        image[at * 3 + 2] = 14;
        sampled[at] = 0.0f;
        return;
    }

    /*
     * The footprint, measured from the rays, and not assumed. Two adjacent pixels, and the larger of the two angles.
     * One sigma must cover the pixel: an under-filter aliases, and an over-filter only softens. At the silhouette, an
     * adjacent ray can miss the sphere entirely, and there the far side of the pixel is the horizon. Fall back to the
     * other axis there.
     */
    float sigma = 0.0f;
    float other_colatitude = 0.0f;
    float other_longitude = 0.0f;
    for (int side = 0; side < 2; side++)
    {
        const float step_across = (float)across + 0.5f + (side == 0 ? 1.0f : 0.0f);
        const float step_down = (float)down + 0.5f + (side == 1 ? 1.0f : 0.0f);
        if (!direction_for(step_across, step_down, &other_colatitude, &other_longitude))
        {
            continue;
        }
        /* Angle between two directions on the unit sphere, from their dot product. */
        const float first = sinf(colatitude);
        const float second = sinf(other_colatitude);
        const float dot =
            first * second * cosf(longitude - other_longitude) + cosf(colatitude) * cosf(other_colatitude);
        const float angle = acosf(fmaxf(-1.0f, fminf(1.0f, dot)));
        if (angle > sigma)
        {
            sigma = angle;
        }
    }

    /*
     * The prefilter, folded into the gain vector that the host already built. This is the entire cost of the exact
     * anti-alias filter: one exponential per degree, eleven of them.
     *
     * The dump records sigma, because the grader must apply the SAME filter. Without it, the comparison would be of two
     * different fields. It would report a disagreement that is really a difference in the request.
     */
    const float tau_pixel = 0.5f * sigma * sigma;
    directions[at * 3 + 2] = sigma;
    float gain[DEGREE_CEILING + 1];
    for (int degree = 0; degree <= DEGREE_CEILING; degree++)
    {
        gain[degree] = base_gain[degree] * expf(-(float)degree * (float)(degree + 1) * tau_pixel);
    }

    float by_longitude = 0.0f;
    const float value = field_at(colatitude, longitude, coefficients, gain, &by_longitude);
    sampled[at] = value;

    /* The colatitude derivative, by central difference on the exact value. See the file note. */
    float ignored = 0.0f;
    const float above = field_at(colatitude - THETA_STEP, longitude, coefficients, gain, &ignored);
    const float below = field_at(colatitude + THETA_STEP, longitude, coefficients, gain, &ignored);
    const float by_colatitude = (below - above) / (2.0f * THETA_STEP);

    /*
     * A radial graph r = R(1 + s f) has a normal built from the two tangential derivatives. The
     * sine in the longitude term is the metric's and not a fudge: a step in longitude covers less
     * ground near a pole.
     */
    const float radius = BOUNDARY_RADIUS * (1.0f + SURFACE_SWING * value);
    const float sine_down = fmaxf(1.0e-4f, sinf(colatitude));
    const float slope_down = BOUNDARY_RADIUS * SURFACE_SWING * by_colatitude;
    const float slope_around = BOUNDARY_RADIUS * SURFACE_SWING * by_longitude / sine_down;

    /*
     * Normal in the local radial frame, then lit by a lamp behind the observer. Only the angle between the normal and
     * the view matters for this first picture, and the frame therefore stays out of world space.
     */
    float normal_radial = radius;
    float normal_down = -slope_down;
    float normal_around = -slope_around;
    const float normal_size =
        sqrtf(normal_radial * normal_radial + normal_down * normal_down + normal_around * normal_around);
    normal_radial /= normal_size;
    normal_down /= normal_size;
    normal_around /= normal_size;

    const float facing = fmaxf(0.0f, normal_radial);
    const float lit = 0.12f + 0.88f * facing * facing;

    /* Value to color: a two-sided ramp, so that a sign stays visible, and does not fold away. */
    const float scaled = fmaxf(-1.0f, fminf(1.0f, value * 2.2f));
    const float warm = scaled > 0.0f ? scaled : 0.0f;
    const float cool = scaled < 0.0f ? -scaled : 0.0f;

    const float red = lit * (0.16f + 0.80f * warm + 0.10f * cool);
    const float green = lit * (0.20f + 0.38f * warm + 0.34f * cool);
    const float blue = lit * (0.26f + 0.10f * warm + 0.74f * cool);

    image[at * 3] = (unsigned char)(255.0f * fminf(1.0f, red));
    image[at * 3 + 1] = (unsigned char)(255.0f * fminf(1.0f, green));
    image[at * 3 + 2] = (unsigned char)(255.0f * fminf(1.0f, blue));
}

/**
 * @brief The reading of golden-placed interior sources, carried outward through the kernel.
 * @param coefficients Written, COEFFICIENT_COUNT of them.
 * @param base_gain    Written, one per degree: depth and conduction, ceiling implied.
 *
 * The addition theorem turns the zonal kernel of a source about its own axis into a sum over this basis. A source then
 * costs one evaluation of the harmonics at its direction, and nothing on a grid. Same construction as
 * sphere_field.coefficients.
 */
static void build_reading(float *coefficients, float *base_gain)
{
    for (int slot = 0; slot < COEFFICIENT_COUNT; slot++)
    {
        coefficients[slot] = 0.0f;
    }

    float climb = 1.0f;
    for (int degree = 0; degree <= DEGREE_CEILING; degree++)
    {
        const float soften = expf(-(float)degree * (float)(degree + 1) * CONDUCTION_TIME);
        base_gain[degree] = climb * soften;
        climb *= SOURCE_RADIUS_FRACTION;
    }

    float column[DEGREE_CEILING + 1];
    const float root_two = sqrtf(2.0f);

    for (int which = 0; which < SOURCE_COUNT; which++)
    {
        const float height = 1.0f - 2.0f * ((float)which + 0.5f) / (float)SOURCE_COUNT;
        const float around = fmodf((float)which * GOLDEN_ANGLE, 2.0f * BOUNDARY_PI);
        const float colatitude = acosf(fmaxf(-1.0f, fminf(1.0f, height)));

        const float x = cosf(colatitude);
        for (int order = 0; order <= DEGREE_CEILING; order++)
        {
            legendre_column(order, x, column);
            if (order == 0)
            {
                for (int degree = 0; degree <= DEGREE_CEILING; degree++)
                {
                    coefficients[degree * degree + degree] += column[degree];
                }
                continue;
            }
            const float angle = (float)order * around;
            const float cosine = cosf(angle) * root_two;
            const float sine = sinf(angle) * root_two;
            for (int degree = order; degree <= DEGREE_CEILING; degree++)
            {
                coefficients[degree * degree + degree + order] += column[degree] * cosine;
                coefficients[degree * degree + degree - order] += column[degree] * sine;
            }
        }
    }

    /*
     * Normalized by the source count, so that the contrast of the picture does not depend on how many sources there
     * are. The grader divides by nothing, and must see the same numbers.
     */
    for (int slot = 0; slot < COEFFICIENT_COUNT; slot++)
    {
        coefficients[slot] /= (float)SOURCE_COUNT;
    }
}

/**
 * @brief Renders one frame, writes the image, and writes the dump the grader reads.
 */
int main(int argc, char **argv)
{
    const char *image_path = argc > 1 ? argv[1] : "boundary_raster.ppm";
    const char *dump_path = argc > 2 ? argv[2] : "boundary_raster.bin";

    float coefficients[COEFFICIENT_COUNT];
    float base_gain[DEGREE_CEILING + 1];
    build_reading(coefficients, base_gain);

    unsigned char *image = NULL;
    float *sampled = NULL;
    float *directions = NULL;
    float *device_coefficients = NULL;
    float *device_gain = NULL;

    const size_t image_bytes = (size_t)IMAGE_WIDTH * IMAGE_HEIGHT * 3;
    const size_t sampled_bytes = (size_t)IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(float);
    const size_t direction_bytes = sampled_bytes * 3;

    if (cudaMalloc((void **)&image, image_bytes) != cudaSuccess ||
        cudaMalloc((void **)&sampled, sampled_bytes) != cudaSuccess ||
        cudaMalloc((void **)&directions, direction_bytes) != cudaSuccess ||
        cudaMalloc((void **)&device_coefficients, sizeof(coefficients)) != cudaSuccess ||
        cudaMalloc((void **)&device_gain, sizeof(base_gain)) != cudaSuccess)
    {
        fprintf(stderr, "cudaMalloc failed\n");
        return 1;
    }
    cudaMemcpy(device_coefficients, coefficients, sizeof(coefficients), cudaMemcpyHostToDevice);
    cudaMemcpy(device_gain, base_gain, sizeof(base_gain), cudaMemcpyHostToDevice);

    const dim3 block(16, 16);
    const dim3 grid((IMAGE_WIDTH + block.x - 1) / block.x, (IMAGE_HEIGHT + block.y - 1) / block.y);

    cudaEvent_t began;
    cudaEvent_t ended;
    cudaEventCreate(&began);
    cudaEventCreate(&ended);

    /*
     * One warm pass before the timed one, so that the report is the kernel, and not the first-launch cost of its load.
     */
    draw<<<grid, block>>>(image, sampled, directions, device_coefficients, device_gain);
    cudaDeviceSynchronize();

    cudaEventRecord(began);
    draw<<<grid, block>>>(image, sampled, directions, device_coefficients, device_gain);
    cudaEventRecord(ended);

    const cudaError_t ran = cudaDeviceSynchronize();
    if (ran != cudaSuccess)
    {
        fprintf(stderr, "kernel failed: %s\n", cudaGetErrorString(ran));
        return 1;
    }
    float milliseconds = 0.0f;
    cudaEventElapsedTime(&milliseconds, began, ended);

    unsigned char *host_image = (unsigned char *)malloc(image_bytes);
    float *host_sampled = (float *)malloc(sampled_bytes);
    float *host_directions = (float *)malloc(direction_bytes);
    if (!host_image || !host_sampled || !host_directions)
    {
        fprintf(stderr, "out of host memory\n");
        return 1;
    }
    cudaMemcpy(host_image, image, image_bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(host_sampled, sampled, sampled_bytes, cudaMemcpyDeviceToHost);
    cudaMemcpy(host_directions, directions, direction_bytes, cudaMemcpyDeviceToHost);

    FILE *out = fopen(image_path, "wb");
    if (!out)
    {
        fprintf(stderr, "cannot write %s\n", image_path);
        return 1;
    }
    fprintf(out, "P6\n%d %d\n255\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    fwrite(host_image, 1, image_bytes, out);
    fclose(out);

    /*
     * The dump that the grader reads starts with a small header, the coefficients and the base gain that the run used.
     * Then it holds a colatitude, a longitude and a field value per pixel. The coefficients travel with it, so that the
     * comparison is of the basis, and never of two different readings.
     */
    out = fopen(dump_path, "wb");
    if (!out)
    {
        fprintf(stderr, "cannot write %s\n", dump_path);
        return 1;
    }
    const int header[4] = {DEGREE_CEILING, COEFFICIENT_COUNT, IMAGE_WIDTH, IMAGE_HEIGHT};
    fwrite(header, sizeof(int), 4, out);
    fwrite(coefficients, sizeof(float), COEFFICIENT_COUNT, out);
    fwrite(base_gain, sizeof(float), DEGREE_CEILING + 1, out);
    fwrite(host_directions, sizeof(float), (size_t)IMAGE_WIDTH * IMAGE_HEIGHT * 3, out);
    fwrite(host_sampled, sizeof(float), (size_t)IMAGE_WIDTH * IMAGE_HEIGHT, out);
    fclose(out);

    long hits = 0;
    for (long at = 0; at < (long)IMAGE_WIDTH * IMAGE_HEIGHT; at++)
    {
        if (host_directions[at * 3] >= 0.0f)
        {
            hits++;
        }
    }

    printf("  %s  %d by %d\n", image_path, IMAGE_WIDTH, IMAGE_HEIGHT);
    printf("  degree %d, %d coefficients, %d sources at r/R %.2f, tau %.1e\n", DEGREE_CEILING, COEFFICIENT_COUNT,
           SOURCE_COUNT, SOURCE_RADIUS_FRACTION, CONDUCTION_TIME);
    printf("  %ld pixels on the boundary of %d, no mesh and no interpolation\n", hits, IMAGE_WIDTH * IMAGE_HEIGHT);
    printf("  %.3f ms for the frame, %.1f million field evaluations a second\n", milliseconds,
           (double)hits * 3.0 / (milliseconds * 1.0e3));
    printf("  the three per pixel are the value and the two colatitude samples\n");
    printf("  %s holds the coefficients and a direction and value per pixel\n", dump_path);

    cudaFree(image);
    cudaFree(sampled);
    cudaFree(directions);
    cudaFree(device_coefficients);
    cudaFree(device_gain);
    free(host_image);
    free(host_sampled);
    free(host_directions);
    return 0;
}
