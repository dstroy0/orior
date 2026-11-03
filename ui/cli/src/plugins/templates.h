// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// templates.h: templates, the reader's own starts for a project, each a folder under `templates/` in orior's own
// folder. A project begun from one is a copy of it in a new folder, `{{name}}` written as the project's name in every
// file's name and in every file's text. A tree saved as a template is a copy of the files the tree's patterns show,
// past git's own folder (templates.c)
#ifndef TEMPLATES_H
#define TEMPLATES_H
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliTemplatesTemplateNameCfg CliTemplatesTemplateNameCfg;
typedef struct CliTemplatesNameCfg CliTemplatesNameCfg;
typedef struct CliTemplatesCfg CliTemplatesCfg;
typedef struct CliTemplatesOutCfg CliTemplatesOutCfg;

// the folder that contains the reader's templates into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0 where orior_cli
// has no folder of its own; the reader's templates, by name, in order, written as a JSON array of names; a project
// named `name` written in `parent` from the template `name_of_template`, its folder into `made`; and the tree at `root`
// saved as the template `name`, the template's folder into `made`. Each of the last two 1, or 0 with why given
int cli_templates_folder(const CliTemplatesCfg *args);
int cli_templates_create(const CliTemplatesTemplateNameCfg *args);
int cli_templates_save(const CliTemplatesNameCfg *args);
// the cells the templates' names and a template file's pieces occupy
extern CellOwner g_cli_templates_cells;

struct CliTemplatesTemplateNameCfg
{
    const char *template_name;
    const char *parent;
    const char *name;
    char *made;
    VerbaLinea *err_msg;
};

struct CliTemplatesNameCfg
{
    const char *name;
    char *made;
    VerbaLinea *err_msg;
    const char *root;
};

struct CliTemplatesCfg
{
    char *folder;
};

struct CliTemplatesOutCfg
{
    VerbaLinea *out;
};

typedef struct
{
    int (*folder)(const CliTemplatesCfg *args);
} CliTemplatesNs;
EMBED_TABLE_LAYOUT(CliTemplatesNs, folder);
EMBED_TABLE_STORAGE CliTemplatesNs cli_templates_table EMBED_UNUSED = {
    .folder = cli_templates_folder,
};

typedef struct
{
    int (*create)(const CliTemplatesTemplateNameCfg *args);
} CliTemplatesTemplateNameNs;
EMBED_TABLE_LAYOUT(CliTemplatesTemplateNameNs, create);
EMBED_TABLE_STORAGE CliTemplatesTemplateNameNs cli_templates_template_name EMBED_UNUSED = {
    .create = cli_templates_create,
};

typedef struct
{
    int (*save)(const CliTemplatesNameCfg *args);
} CliTemplatesNameNs;
EMBED_TABLE_LAYOUT(CliTemplatesNameNs, save);
EMBED_TABLE_STORAGE CliTemplatesNameNs cli_templates_name EMBED_UNUSED = {
    .save = cli_templates_save,
};

#endif
