// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.cu: a run's record (record.h)
#include "record.h"

#include "verba_scribo/verba_linea.h"

FILE *record_open(const char *cfg_path, const RunCfg *cfg, const char *member)
{
    char path[PLATFORM_PATH_BYTES];
    if (run_cfg_beside(cfg_path, cfg, member, path, sizeof(path)) == 0)
    {
        return NULL;
    }
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        return NULL;
    }
    fwrite(cfg->text, 1u, cfg->size, file);
    if ((cfg->size == 0u) || (cfg->text[cfg->size - 1u] != '\n'))
    {
        fputc('\n', file);
    }
    return file;
}

// each limb of `value` in hex from the highest, after its sign and "0x"; "0" where the value is 0
static void record_limbs(VerbaLinea *line, const OriorExactInteger *value)
{
    // the limbs a value uses are far inside what an unsigned int counts
    const unsigned int used = (unsigned int)((sim_exact_bits(value) + 31ull) / 32ull);
    if (used == 0u)
    {
        mmgr_line_ch(line, '0');
        return;
    }
    mmgr_line_put(line, (value->sign < 0) ? "-0x" : "0x");
    for (unsigned int limb = used; limb > 0u; limb -= 1u)
    {
        mmgr_line_hex(line, value->limb[limb - 1u], 8u);
    }
}

void record_rational(VerbaLinea *line, SimRational value)
{
    long long numerator = 0ll;
    long long denominator = 0ll;
    if (!sim_rational_small(&value.numerator, &numerator) || !sim_rational_small(&value.denominator, &denominator))
    {
        mmgr_line_ch(line, '(');
        record_limbs(line, &value.numerator);
        mmgr_line_ch(line, '/');
        record_limbs(line, &value.denominator);
        mmgr_line_ch(line, ')');
    }
    else
    {
        mmgr_line_decimal_signed(line, numerator);
        if (denominator != 1ll)
        {
            mmgr_line_ch(line, '/');
            mmgr_line_decimal_signed(line, denominator);
        }
    }
}

// 1 where a line of the record passed its bound or its bytes could not be read
static int s_record_line_refused;

void record_line_begun(PlatformText *line)
{
    platform_text_begun(line, RECORD_LINE_MAX, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
}

void record_line_ended(FILE *file, PlatformText *line)
{
    const char *const bytes = mmgr_line_ok(&line->line) ? platform_text_bytes(line) : NULL;
    if (bytes != NULL)
    {
        record_text(file, bytes);
    }
    s_record_line_refused |= (bytes == NULL) ? 1 : 0;
    platform_text_ended(line);
}

void record_text(FILE *file, const char *text)
{
    fputs(text, file);
    fputc('\n', file);
}

int record_close(FILE *file)
{
    const int written = (ferror(file) == 0) && !s_record_line_refused;
    return (fclose(file) == 0) && written;
}

int record_short(void)
{
    return g_sim_rational_wide != 0;
}
