// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view_tidy.h: a view's text made ready for its file as one step, which one undo takes back whole: the spaces and tabs
// at its lines' ends taken off and a line end put after its last line; its text replaced by another over the lines that
// differ, the lines alike at either end kept; and the language's formatter run over it and its answer written back that
// way (view_tidy.c)
#ifndef VIEW_TIDY_H
#define VIEW_TIDY_H
#include "view.h"

typedef struct CliViewTidyCfg CliViewTidyCfg;
typedef struct CliViewReplaceCfg CliViewReplaceCfg;
typedef struct CliViewFormatCfg CliViewFormatCfg;

// the spaces and tabs at the end of each line of the view under `key` taken off where `trim` is 1, and a line end put
// after its last line where `final_newline` is 1 and the text ends without one, as one step: 1, with nothing changed
// where nothing stands to change or the view's text is paged, or 0 with why written into `err_msg`
int cli_view_tidy(const CliViewTidyCfg *args);
// the text of the view under `key` replaced by `text`, `length` bytes, as one step over the lines that differ: the lines
// alike at the texts' starts and at their ends stand as they are, and the cursor goes to the first line that differs: 1,
// with nothing changed where the texts read the same, or 0 with why written into `err_msg`
int cli_view_replace(const CliViewReplaceCfg *args);
// the text of the view under `key`, the file at the whole path `full` in `language`, run through the language's
// formatter and its answer written back as cli_view_replace writes it, its carriage returns before newlines taken out:
// 1, or 0 with why written into `err_msg` and the text as it was
int cli_view_format(const CliViewFormatCfg *args);
struct CliViewTidyCfg
{
    const char *key;
    int trim;
    int final_newline;
    VerbaLinea *err_msg;
};
struct CliViewReplaceCfg
{
    const char *key;
    const char *text;
    unsigned long long length;
    VerbaLinea *err_msg;
};
struct CliViewFormatCfg
{
    const char *key;
    const char *full;
    const char *language;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*tidy)(const CliViewTidyCfg *args);
} CliViewTidyNs;
EMBED_TABLE_LAYOUT(CliViewTidyNs, tidy);
EMBED_TABLE_STORAGE CliViewTidyNs cli_view_tidy_table EMBED_UNUSED = {
    .tidy = cli_view_tidy,
};

typedef struct
{
    int (*replace)(const CliViewReplaceCfg *args);
} CliViewReplaceNs;
EMBED_TABLE_LAYOUT(CliViewReplaceNs, replace);
EMBED_TABLE_STORAGE CliViewReplaceNs cli_view_replace_table EMBED_UNUSED = {
    .replace = cli_view_replace,
};

typedef struct
{
    int (*format)(const CliViewFormatCfg *args);
} CliViewFormatNs;
EMBED_TABLE_LAYOUT(CliViewFormatNs, format);
EMBED_TABLE_STORAGE CliViewFormatNs cli_view_format_table EMBED_UNUSED = {
    .format = cli_view_format,
};

#endif
