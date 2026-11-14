// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook_model_unity.h: the notebook suites' reads of a model's movable cells: a cell, an output, a gone cell and the
// row a cell starts at read by copy, a cell written back, and whether the outputs' texts contain a text
#ifndef NOTEBOOK_MODEL_UNITY_H
#define NOTEBOOK_MODEL_UNITY_H
#include "ui/cli/src/notebooks/notebook_cells_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the cell `at` of `model`
static inline CliNotebookCell notebook_cell_at(const CliNotebookModel *model, unsigned int at)
{
    CliNotebookCell cell;
    EMBED_CALL(cli_notebook_model_internal.cell_read, CliNotebookModelCtx, .model = (CliNotebookModel *)(embed_word)model,
               .cell = at, .cell_out = &cell);
    return cell;
}

// `cell` written as the cell `at` of `model`
static inline void notebook_cell_set(CliNotebookModel *model, unsigned int at, const CliNotebookCell *cell)
{
    EMBED_CALL(cli_notebook_model_internal.cell_written, CliNotebookModelCtx, .model = model, .cell = at,
               .cell_read = cell);
}

// the output `at` of `model`
static inline CliNotebookOutput notebook_output_at(const CliNotebookModel *model, unsigned int at)
{
    CliNotebookOutput output;
    EMBED_CALL(cli_notebook_model_internal.output_read, CliNotebookModelCtx,
               .model = (CliNotebookModel *)(embed_word)model, .output = at, .output_out = &output);
    return output;
}

// the gone cell `at` of `model`
static inline CliNotebookGone notebook_gone_at(const CliNotebookModel *model, unsigned int at)
{
    CliNotebookGone gone;
    EMBED_CALL(cli_notebook_model_internal.gone_read, CliNotebookModelCtx, .model = (CliNotebookModel *)(embed_word)model,
               .at = at, .gone_out = &gone);
    return gone;
}

// the row the cell `at` of `model` starts at
static inline unsigned int notebook_first_row_at(const CliNotebookModel *model, unsigned int at)
{
    return EMBED_CALL(cli_notebook_model_internal.first_row, CliNotebookModelCtx,
                      .model = (CliNotebookModel *)(embed_word)model, .cell = at);
}

// whether the outputs' texts of `model` contain `text`
static inline int notebook_texts_contain(const CliNotebookModel *model, const char *text)
{
    char *const texts = g_cli_guard->temporary_buf_alloc(model->output_used + 1ULL, g_cli_notebook_open_owner);
    if (texts == NULL)
    {
        return 0;
    }
    if (model->output_used != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = model->text_cell, .offset = 0ULL, .dst = texts, .bytes = model->output_used});
    }
    texts[model->output_used] = '\0';
    const int found = cellul.find(&(CatenaFinitaCfg){.src = texts,
                                                     .cap = model->output_used,
                                                     .other = text,
                                                     .other_cap = CLI_UNITY_LENGTH(text)}) != NULL;
    g_cli_guard->persistent_buf_release(texts, g_cli_notebook_open_owner);
    return found;
}

#endif
