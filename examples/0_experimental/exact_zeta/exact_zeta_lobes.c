// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: EXP-x-027
/**
 * @file exact_zeta_lobes.c
 * @brief The two device stages of exact_zeta_lobes.cu over one cell: the curves of R, and the logarithm of the phase.
 *
 * @note The curve stage gives the curves that the remainder R of Riemann-Siegel holds at every point. The log stage
 *       gives the logarithm by which the phase of the main sum turns. Each point is one lane. The host checks each
 *       stage against the device word for word, and each output against the exact value that it stands for.
 * @note Run build_exact_zeta.sh lobes [binary], beside this file. Without a binary, it builds one through
 *       examples/0_experimental/exact_zeta_lobes.sh.
 * @note This reads no corpus. It sits in 0_experimental, and is an entry in the workbook of analytic number theory,
 *       on its rail. It claims nothing about the Riemann hypothesis.
 * @note The cell: lane l is at x = nu + l / 2^b over a cell of 2^b + 1 points. Here X = nu 2^b + l, and
 *       z = 1 - 2 p = Zt / 2^b with Zt = 2^b - 2 l. The sign is s = (-1)^(nu - 1).
 * @note The curve stage: C_n(z) is the sum over j of g_(n,j) z^j. Each g is a Taylor coefficient of C_n, an integer
 *       at the binary scale 2^E. The rule of Horner reads it as H_n = sum over j of g_(n,j) Zt^j 2^(b (J_n - 1 - j)).
 * @note The part of curve n in R x^(1/2) is s C_n(z) x^(-n). Over the common denominator X^K it is the integer
 *       T_n = s H_n 2^(b n) X^(K - n), each output at its own binary exponent.
 * @note The coefficients come from the generator of Gabcke in zeta_riemann_siegel.c, trimmed where they fall below
 *       the scale. The check holds the C_n(z) of each lane, read back from the device, against the exact rational of
 *       rs_c_at.
 * @note The log stage: A = ln(X / (nu 2^b)) = 2 artanh(l / D), D = 2 nu 2^b + l, one series a lane. Let
 *       c_k = Lambda / (2k + 1), with Lambda the least common multiple of the odd numbers below 2L.
 * @note The first L terms sum to 2 l S / (Lambda D^(2L - 1)), with S = sum over k < L of c_k l^(2k) D^(2(L - 1 - k)).
 *       The stage outputs the four integers l S, D^(2L - 1), l^(2L + 1) and D^2 - l^2.
 * @note A lies at or above 2 l S / (Lambda D^(2L - 1)), and below that plus the tail
 *       2 l^(2L + 1) / ((2L + 1) D^(2L - 1) (D^2 - l^2)). The check holds the bracket of each lane against the exact
 *       2 artanh(l / D).
 */
#include "zeta_riemann_siegel.h"
#include "../../../src/platform/platform.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/** @brief The cell, its bits, and its points. */
#define NU 2
#define B 9
#define LANES ((1 << B) + 1)

/** @brief The lanes that the host checks against the device. */
#define CHECKED 64

/** @brief The last curve. */
#define CURVES 3

/** @brief The binary scale of the coefficients. */
#define SCALE_BITS 256

/** @brief The terms of the logarithm. */
#define LOG_TERMS 48

/** @brief The most Taylor coefficients of a curve. */
#define TERMS_MAX 160

/** @brief The places of the coefficients, of the check of the curves, and of the check of the logarithm. */
#define GAMMA_DIGITS 100
#define CURVE_DIGITS 50
#define LOG_DIGITS 200

/** @brief The device program, and its two sources. */
#define LOBES_BUILD "examples/0_experimental/exact_zeta_lobes.sh"
#define LOBES_SOURCE "examples/0_experimental/exact_zeta_lobes.cu"

/**
 * @brief The text of a truth value, as Python prints it.
 *
 * @param[in] value The value.
 * @return          "True" or "False".
 */
static const char *truth(int value)
{
    return value ? "True" : "False";
}

/**
 * @brief Stops the program where an allocation fails, else returns the block.
 *
 * @param[in] block The block [OWNS].
 * @return          The same block [OWNS].
 */
static void *lobes_memory(void *block)
{
    if (block == NULL)
    {
        zeta_stop("out of memory");
    }
    return block;
}

/**
 * @brief The quotient of two integers, rounded toward zero.
 *
 * @param[in]  a      The dividend [BORROWS].
 * @param[in]  b      The divisor [BORROWS].
 * @param[out] result The quotient [BORROWS].
 */
static void toward_zero(const OriorExactInteger *a, const OriorExactInteger *b, OriorExactInteger *result)
{
    OriorExactInteger top;
    OriorExactInteger under;
    z_abs(a, &top);
    z_abs(b, &under);
    z_floor_div(&top, &under, result);
    if ((a->sign >= 0) != (b->sign > 0))
    {
        z_neg(result, result);
    }
}

/**
 * @brief The Taylor coefficients of C_0 through C_K, each at the binary scale 2^E, trimmed past the last that the
 *        scale still resolves.
 *
 * @param[out] gammas The coefficients of each curve [BORROWS]. The caller frees each.
 */
static void curve_coefficients(ZetaRow gammas[CURVES + 1])
{
    ZetaCurve curve;
    rs_curve_begin(&curve, GAMMA_DIGITS);
    OriorExactInteger decimal;
    z_ten_to(GAMMA_DIGITS, &decimal);
    OriorExactInteger power;
    z_two_to(SCALE_BITS, &power);
    for (int64_t n = 0; n <= CURVES; n += 1)
    {
        ZetaRow raw;
        rs_curve_gamma(&curve, n, TERMS_MAX, &raw);
        memset(&gammas[n], 0, sizeof(gammas[n]));
        size_t last = 0u;
        OriorExactInteger *const binary = (OriorExactInteger *)lobes_memory(
            platform_cell_taken(raw.count * sizeof(*binary), platform_place_g_program_owner, 0));
        for (size_t j = 0u; j < raw.count; j += 1u)
        {
            OriorExactInteger shifted;
            z_mul(&raw.values[j], &power, &shifted);
            toward_zero(&shifted, &decimal, &binary[j]);
            last = (binary[j].sign != 0) ? j : last;
        }
        for (size_t j = 0u; j <= last; j += 1u)
        {
            row_push(&gammas[n], &binary[j]);
        }
        platform_cell_released(binary, platform_place_g_program_owner);
        row_free(&raw);
    }
    rs_curve_free(&curve);
}

/**
 * @brief Lambda, the least common multiple of the odd numbers below 2L, and c_k = Lambda / (2k + 1) for k < L.
 *
 * @param[out] lambda The multiple [BORROWS].
 * @param[out] cs     The constants [BORROWS].
 */
static void log_constants(OriorExactInteger *lambda, OriorExactInteger cs[LOG_TERMS])
{
    z_set(lambda, 1);
    for (int64_t odd = 1; odd < (2 * LOG_TERMS); odd += 2)
    {
        // The multiple times the odd number over their divisor.
        OriorExactInteger value;
        z_set(&value, odd);
        OriorExactInteger divisor;
        z_gcd(lambda, &value, &divisor);
        z_mul(lambda, &value, lambda);
        z_div_exact(lambda, &divisor, lambda);
    }
    for (int64_t k = 0; k < LOG_TERMS; k += 1)
    {
        OriorExactInteger odd;
        z_set(&odd, (2 * k) + 1);
        z_floor_div(lambda, &odd, &cs[k]);
    }
}

/**
 * @brief Writes an integer as the device reads it: a count of limbs, the limbs of the magnitude, and the sign.
 *
 * @param[in] handle The file [BORROWS].
 * @param[in] value  The integer [BORROWS].
 * @note Zero writes no limbs.
 */
static void put_mantissa(FILE *handle, const OriorExactInteger *value)
{
    const int64_t count = (z_bit_length(value) + 31) / 32;
    fwrite(&count, sizeof(int64_t), 1u, handle);
    if (count > 0)
    {
        // The limb count is within the width.
        fwrite(value->limb, sizeof(uint32_t), (size_t)count, handle);
    }
    const int64_t sign = value->sign;
    fwrite(&sign, sizeof(int64_t), 1u, handle);
}

/**
 * @brief The path of the newest device program newer than its sources, else the one that exact_zeta_lobes.sh builds.
 *
 * @param[out] binary The path [BORROWS]. 512 bytes.
 */
static void lobes_build(char binary[512])
{
    struct stat facts;
    double newest_source = 0.0;
    if (stat(LOBES_BUILD, &facts) == 0)
    {
        newest_source = (double)facts.st_mtime;
    }
    if ((stat(LOBES_SOURCE, &facts) == 0) && ((double)facts.st_mtime > newest_source))
    {
        newest_source = (double)facts.st_mtime;
    }
    binary[0] = '\0';
    double newest = 0.0;
    DIR *const folder = opendir("build");
    if (folder != NULL)
    {
        const char *const tail = "_exact_zeta_lobes";
        struct dirent *entry;
        while ((entry = readdir(folder)) != NULL)
        {
            const size_t length = strlen(entry->d_name);
            if ((length < strlen(tail)) || (strcmp(entry->d_name + length - strlen(tail), tail) != 0))
            {
                continue;
            }
            static const char *const names[2] = {"exact_zeta_lobes.exe", "exact_zeta_lobes"};
            for (unsigned which = 0u; which < 2u; which += 1u)
            {
                char path[512];
                snprintf(path, sizeof(path), "build/%.200s/%s", entry->d_name, names[which]);
                if ((stat(path, &facts) == 0) && ((double)facts.st_mtime > newest))
                {
                    newest = (double)facts.st_mtime;
                    snprintf(binary, 512u, "%s", path);
                }
            }
        }
        closedir(folder);
    }
    if ((binary[0] != '\0') && (newest > newest_source))
    {
        return;
    }
    binary[0] = '\0';
    FILE *const made = popen("bash " LOBES_BUILD, "r");
    if (made == NULL)
    {
        zeta_stop("cannot run the build of the device program");
    }
    char line[512];
    while (fgets(line, sizeof(line), made) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0')
        {
            snprintf(binary, 512u, "%s", line);
        }
    }
    if ((pclose(made) != 0) || (binary[0] == '\0'))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the build failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        exit(1);
    }
}

/** @brief One output of a stage: its name, offset and bits. */
typedef struct
{
    char name[32];  /**< The field. */
    int64_t offset; /**< Its offset in the word. */
    int64_t bits;   /**< Its bits. */
} StageOutput;

/** @brief One stage of the device: its outputs, a word a lane, and the check of the host. */
typedef struct
{
    StageOutput outputs[16]; /**< The outputs. */
    size_t output_count;     /**< How many. */
    OriorExactInteger *rows; /**< The word of each lane [OWNS]. */
    int host;                /**< 1 where the host equals the device. */
} Stage;

/**
 * @brief Reads the stages of the output of the device.
 *
 * @param[in]  path   The output [BORROWS].
 * @param[out] curves The curve stage [BORROWS].
 * @param[out] logs   The log stage [BORROWS].
 */
static void parse(const char *path, Stage *curves, Stage *logs)
{
    FILE *const handle = fopen(path, "r");
    if (handle == NULL)
    {
        zeta_stop("the device wrote no output");
    }
    memset(curves, 0, sizeof(*curves));
    memset(logs, 0, sizeof(*logs));
    static char line[1 << 20];
    while (fgets(line, sizeof(line), handle) != NULL)
    {
        char name[64] = {0};
        long long out_limbs = 0;
        if ((sscanf(line, "stage %63s %*s %lld", name, &out_limbs) != 2))
        {
            continue;
        }
        Stage *const stage = (strcmp(name, "curves") == 0) ? curves : logs;
        stage->rows = (OriorExactInteger *)lobes_memory(
            platform_cell_cleared(LANES, sizeof(OriorExactInteger), platform_place_g_program_owner, 0));
        while (fgets(line, sizeof(line), handle) != NULL)
        {
            char field[32] = {0};
            long long offset = 0;
            long long bits = 0;
            if (sscanf(line, "output %31s %lld %lld", field, &offset, &bits) != 3)
            {
                break;
            }
            if (stage->output_count < 16u)
            {
                StageOutput *const output = &stage->outputs[stage->output_count];
                snprintf(output->name, sizeof(output->name), "%s", field);
                output->offset = offset;
                output->bits = bits;
                stage->output_count += 1u;
            }
        }
        for (int lane = 0; lane < LANES; lane += 1)
        {
            if (lane > 0)
            {
                if (fgets(line, sizeof(line), handle) == NULL)
                {
                    zeta_stop("the device output is short");
                }
            }
            OriorExactInteger *const word = &stage->rows[lane];
            orior_exact_zero(word);
            char *cursor = line;
            for (long long limb = 0; (limb < out_limbs) && (limb < (long long)ORIOR_EXACT_LIMBS); limb += 1)
            {
                word->limb[limb] = (uint32_t)strtoul(cursor, &cursor, 16);
                if (word->limb[limb] != 0u)
                {
                    word->sign = 1;
                }
            }
        }
        if (fgets(line, sizeof(line), handle) != NULL)
        {
            int host = 0;
            if (sscanf(line, "host %d", &host) == 1)
            {
                stage->host = host;
            }
        }
    }
    fclose(handle);
}

/**
 * @brief The signed field of a word.
 *
 * @param[in]  stage  The stage [BORROWS].
 * @param[in]  lane   The lane.
 * @param[in]  name   The name of the field [BORROWS].
 * @param[out] result The value [BORROWS].
 */
static void lane_field(const Stage *stage, int lane, const char *name, OriorExactInteger *result)
{
    for (size_t at = 0u; at < stage->output_count; at += 1u)
    {
        const StageOutput *const output = &stage->outputs[at];
        if (strcmp(output->name, name) != 0)
        {
            continue;
        }
        OriorExactInteger power;
        z_two_to(output->offset, &power);
        OriorExactInteger shifted;
        z_floor_div(&stage->rows[lane], &power, &shifted);
        OriorExactInteger modulus;
        z_two_to(output->bits, &modulus);
        z_mod(&shifted, &modulus, result);
        OriorExactInteger high;
        z_two_to(output->bits - 1, &high);
        if (z_cmp(result, &high) >= 0)
        {
            z_sub(result, &modulus, result);
        }
        return;
    }
    zeta_stop("the stage has no such field");
}

/**
 * @brief Whether a lane is among the samples: every eighth lane, and the last.
 *
 * @param[in] lane The lane.
 * @return         1 where it is.
 */
static int sampled(int lane)
{
    return ((lane % 8) == 0) || (lane == (LANES - 1));
}

/**
 * @brief The largest gap between the C_n of each sampled lane, read back from the device, and the exact rational.
 *
 * @param[in] stage  The curve stage [BORROWS].
 * @param[in] gammas The coefficients [BORROWS].
 * @return           The gap.
 * @note Its floor across the cell is the truncation of the device at 160 terms, widest where z reaches the ends of
 *       the cell.
 */
static ZetaRational check_curves(const Stage *stage, const ZetaRow gammas[CURVES + 1])
{
    const int sign = ((((NU - 1) % 2) + 2) % 2 == 0) ? 1 : -1;
    ZetaRational worst;
    rational_small(0, 1, &worst);
    OriorExactInteger den;
    z_set(&den, 1 << B);
    for (int lane = 0; lane < LANES; lane += 1)
    {
        if (!sampled(lane))
        {
            continue;
        }
        const int64_t big_x = ((int64_t)NU << B) + lane;
        OriorExactInteger z_num;
        z_set(&z_num, (1 << B) - (2 * lane));
        for (int64_t n = 0; n <= CURVES; n += 1)
        {
            char name[16];
            snprintf(name, sizeof(name), "curve%lld", (long long)n);
            OriorExactInteger value;
            lane_field(stage, lane, name, &value);
            z_mul_small(&value, sign, &value);
            OriorExactInteger under;
            // The coefficient count of a curve is at most 160.
            z_two_to(SCALE_BITS + (B * ((int64_t)gammas[n].count - 1)), &under);
            for (int64_t at = 0; at < (CURVES - n); at += 1)
            {
                z_mul_small(&under, big_x, &under);
            }
            ZetaRational device_value;
            rational_make(&value, &under, &device_value);
            OriorExactInteger exact_value;
            rs_c_at(n, &z_num, &den, CURVE_DIGITS, &exact_value);
            OriorExactInteger places;
            z_ten_to(CURVE_DIGITS + ZETA_GUARD, &places);
            ZetaRational exact;
            rational_make(&exact_value, &places, &exact);
            ZetaRational gap;
            rational_sub(&device_value, &exact, &gap);
            if (gap.top.sign < 0)
            {
                gap.top.sign = 1;
            }
            if (rational_cmp(&gap, &worst) > 0)
            {
                worst = gap;
            }
        }
    }
    return worst;
}

/**
 * @brief Whether the bracket of each sampled lane on A holds the exact 2 artanh(l / D), and the widest bracket.
 *
 * @param[in]  stage  The log stage [BORROWS].
 * @param[in]  lambda The multiple [BORROWS].
 * @param[out] widest The widest bracket [BORROWS].
 * @return            1 where every bracket holds.
 */
static int check_logs(const Stage *stage, const OriorExactInteger *lambda, ZetaRational *widest)
{
    OriorExactInteger scale;
    z_ten_to(LOG_DIGITS, &scale);
    OriorExactInteger slack_under;
    z_ten_to(190, &slack_under);
    OriorExactInteger one;
    z_set(&one, 1);
    ZetaRational slack;
    rational_make(&one, &slack_under, &slack);
    int held = 1;
    rational_small(0, 1, widest);
    for (int lane = 1; lane < LANES; lane += 1)
    {
        if (!sampled(lane))
        {
            continue;
        }
        OriorExactInteger numerator;
        OriorExactInteger power;
        OriorExactInteger tail_value;
        OriorExactInteger gap;
        lane_field(stage, lane, "numerator", &numerator);
        lane_field(stage, lane, "power", &power);
        lane_field(stage, lane, "tail", &tail_value);
        lane_field(stage, lane, "gap", &gap);
        OriorExactInteger top;
        OriorExactInteger under;
        z_mul_small(&numerator, 2, &top);
        z_mul(lambda, &power, &under);
        ZetaRational lower;
        rational_make(&top, &under, &lower);
        z_mul_small(&tail_value, 2, &top);
        z_mul_small(&power, (2 * LOG_TERMS) + 1, &under);
        z_mul(&under, &gap, &under);
        ZetaRational tail;
        rational_make(&top, &under, &tail);
        OriorExactInteger l;
        OriorExactInteger big_d;
        z_set(&l, lane);
        z_set(&big_d, ((int64_t)2 * NU << B) + lane);
        OriorExactInteger artanh;
        zeta_artanh(&l, &big_d, &scale, &artanh);
        z_mul_small(&artanh, 2, &artanh);
        ZetaRational exact;
        rational_make(&artanh, &scale, &exact);
        ZetaRational floor_bound;
        ZetaRational ceiling_bound;
        rational_sub(&lower, &slack, &floor_bound);
        rational_add(&lower, &tail, &ceiling_bound);
        rational_add(&ceiling_bound, &slack, &ceiling_bound);
        held = held && (rational_cmp(&floor_bound, &exact) <= 0) && (rational_cmp(&exact, &ceiling_bound) <= 0);
        if (rational_cmp(&tail, widest) > 0)
        {
            *widest = tail;
        }
    }
    return held;
}

int program_main(int argument_count, char **arguments)
{
#ifdef _WIN32
    // The report ends each line with a line feed alone, on every platform.
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    char binary[512];
    if (argument_count > 1)
    {
        snprintf(binary, sizeof(binary), "%s", arguments[1]);
    }
    else
    {
        lobes_build(binary);
    }
    ZetaRow gammas[CURVES + 1];
    curve_coefficients(gammas);
    OriorExactInteger lambda;
    static OriorExactInteger cs[LOG_TERMS];
    log_constants(&lambda, cs);
    const char *const given = "build/lobes_in.bin";
    const char *const taken = "build/lobes_out.txt";
    FILE *const handle = fopen(given, "wb");
    if (handle == NULL)
    {
        zeta_stop("cannot write the input of the device");
    }
    const int64_t words[7] = {LANES, CHECKED, NU, B, CURVES, SCALE_BITS, LOG_TERMS};
    fwrite(words, sizeof(int64_t), 7u, handle);
    for (int n = 0; n <= CURVES; n += 1)
    {
        // The coefficient count of a curve is at most 160.
        const int64_t count = (int64_t)gammas[n].count;
        fwrite(&count, sizeof(int64_t), 1u, handle);
    }
    for (int n = 0; n <= CURVES; n += 1)
    {
        for (size_t at = 0u; at < gammas[n].count; at += 1u)
        {
            put_mantissa(handle, &gammas[n].values[at]);
        }
    }
    for (int k = 0; k < LOG_TERMS; k += 1)
    {
        put_mantissa(handle, &cs[k]);
    }
    fclose(handle);
    char command[2048];
#ifdef _WIN32
    (void)_putenv("CYCLE_RECORD_RETAIN_PTX=1");
    snprintf(command, sizeof(command), "\"\"%s\" \"%s\" \"%s\" >build\\lobes_device.txt 2>&1\"", binary, given, taken);
#else
    snprintf(command, sizeof(command), "CYCLE_RECORD_RETAIN_PTX=1 \"%s\" \"%s\" \"%s\" >build/lobes_device.txt 2>&1",
             binary, given, taken);
#endif
    if (system(command) != 0)
    {
        FILE *const log = fopen("build/lobes_device.txt", "r");
        if (log != NULL)
        {
            static char text[4096];
            const size_t length = fread(text, 1u, sizeof(text) - 1u, log);
            fclose(log);
            text[length] = '\0';
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, text);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  the device run failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    Stage curves;
    Stage logs;
    parse(taken, &curves, &logs);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  cell: nu = ");
        mmgr_line_decimal_signed(output_line, NU);
        mmgr_line_put(output_line, ", b = ");
        mmgr_line_decimal_signed(output_line, B);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal_signed(output_line, LANES);
        mmgr_line_put(output_line, " points; curves C_0 .. C_");
        mmgr_line_decimal_signed(output_line, CURVES);
        mmgr_line_put(output_line, " at 2^-");
        mmgr_line_decimal_signed(output_line, SCALE_BITS);
        mmgr_line_put(output_line, "; the logarithm by ");
        mmgr_line_decimal_signed(output_line, LOG_TERMS);
        mmgr_line_put(output_line, " terms");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  coefficients a curve: ");
        platform_line_out_ended();
    }
    for (int n = 0; n <= CURVES; n += 1)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, (n > 0) ? ", " : "");
        mmgr_line_decimal(output_line, (unsigned long long)gammas[n].count, 1u);
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int host = (curves.host == 1) && (logs.host == 1);
    const ZetaRational worst = check_curves(&curves, gammas);
    OriorExactInteger tolerance_under;
    z_ten_to(18, &tolerance_under);
    OriorExactInteger one;
    z_set(&one, 1);
    ZetaRational tolerance;
    rational_make(&one, &tolerance_under, &tolerance);
    const int curves_ok = rational_cmp(&worst, &tolerance) <= 0;
    ZetaRational widest;
    const int logs_ok = check_logs(&logs, &lambda, &widest);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the curves: C_n read back from the device meets c_at's exact rational to ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&worst)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " across the cell, the residual the 160-term truncation at z = +-1, inside ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&tolerance)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, truth(curves_ok));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the logarithm: every bracket holds zz's exact 2 artanh(l / D), the widest ");
        mmgr_line_exponent(output_line, (double)(rational_to_double(&widest)), 3u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " wide: ");
        mmgr_line_put(output_line, truth(logs_ok));
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the host's records ");
        mmgr_line_put(output_line, host ? "equal" : "differ from");
        mmgr_line_put(output_line, " the device's word for word");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int good = host && curves_ok && logs_ok;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, good ? "  both stages check out" : "  a stage does not check out");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (int n = 0; n <= CURVES; n += 1)
    {
        row_free(&gammas[n]);
    }
    platform_cell_released(curves.rows, platform_place_g_program_owner);
    platform_cell_released(logs.rows, platform_place_g_program_owner);
    return good ? 0 : 1;
}
