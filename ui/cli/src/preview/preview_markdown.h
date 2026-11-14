// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preview_markdown.h: a Markdown text laid out as the preview's items, as GitHub reads it: headings, paragraphs, lists
// and task lists, quotes, code fenced and indented, tables, rules, links, emphasis and strike, each block starting on
// the line of the text it came from. Runs are measured by a table of the faces' advances and broken at spaces to the
// layout's width; the text a run draws is copied into the text cell with its marks taken out, and a link's address
// before it. An address that runs script, javascript:, vbscript: or a data: page, is no link (preview_markdown.c)
#ifndef CLI_PREVIEW_MARKDOWN_H
#define CLI_PREVIEW_MARKDOWN_H
#include "preview.h"

typedef struct CliPreviewMarkdownCfg CliPreviewMarkdownCfg;

// the page's pixels the layout stands in from the rectangle's sides and top, and below the last block
#define CLI_PREVIEW_PAD 20
#define CLI_PREVIEW_PAD_TOP 16
#define CLI_PREVIEW_PAD_FOOT 24

// `length` bytes of `text`, its first line counted as `first_line`, laid out `width_px` page pixels wide after the
// items `items` carries, each of its movable cells taken anew at the size the old items and the new together fill, the
// old copied in and its cell released: 1, or 0 where a cell cannot be taken, the items as they were
int cli_preview_markdown_layout(const CliPreviewMarkdownCfg *args);
struct CliPreviewMarkdownCfg
{
    const char *text;
    unsigned long long length;
    unsigned int first_line;
    int width_px;
    CliPreviewItems *items;
};

typedef struct
{
    int (*layout)(const CliPreviewMarkdownCfg *args);
} CliPreviewMarkdownNs;
EMBED_TABLE_LAYOUT(CliPreviewMarkdownNs, layout);
EMBED_TABLE_STORAGE CliPreviewMarkdownNs cli_preview_markdown EMBED_UNUSED = {
    .layout = cli_preview_markdown_layout,
};

#endif
