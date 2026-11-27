// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.cu: a run's record (record.h)
#include "record.h"

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

void record_form(FILE *file, const char *name, const TermForm &form, const TermBook *book)
{
    fprintf(file, "form %s %zu\n", name, term_form_terms(form));
    term_form_write(file, form, book->names);
}

void record_text(FILE *file, const char *text)
{
    fputs(text, file);
    fputc('\n', file);
}

int record_close(FILE *file)
{
    const int written = ferror(file) == 0;
    return (fclose(file) == 0) && written;
}

int record_short(void)
{
    return g_sim_rational_wide != 0;
}
