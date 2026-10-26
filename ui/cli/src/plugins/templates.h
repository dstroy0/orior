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

// the folder that contains the reader's templates into `folder`, sized for CLI_PATH_MAX_LEN: 1, or 0 where orior_cli
// has no folder of its own; the reader's templates, by name, in order, written as a JSON array of names; a project
// named `name` begun in `parent` from the template `name_of_template`, its folder into `made`; and the tree at `root`
// saved as the template `name`, the template's folder into `made`. Each of the last two 1, or 0 with why given
int cli_templates_folder(char *folder);
void cli_templates_list(VerbaLinea *out);
int cli_templates_create(const char *template_name, const char *parent, const char *name, char *made,
                         VerbaLinea *err_msg);
int cli_templates_save(const char *root, const char *name, char *made, VerbaLinea *err_msg);
// the cells the templates' names and a template file's pieces occupy
extern CellOwner g_cli_templates_cells;

#endif
