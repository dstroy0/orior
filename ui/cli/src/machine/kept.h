// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept.h: what the window keeps between runs, as files in orior's own folder, laid out as the README's "orior's own
// folder" says (kept.c, kept_keep.c):
//
// - `settings.json`, the reader's settings, those the settings list names, each under its key, and the keys of a
//   settings file's own that settings.c reads, which the page is not given;
// - `themes/`, each theme of the reader's in a file of its own, named for the theme, the themes read back in the order
//   of their files' names;
// - `state.json`, what the window keeps of itself: its panes, its sessions, the files opened last, the places left in
//   them, bookmarks, breakpoints and watches;
// - `backups/`, the text of each tab with changes not saved, a file each.
//
// The page keeps each entry as text under a key starting `orior.`; here the key goes without it. An entry whose text
// is JSON other than a string is written as JSON, and any other as a string, and each reads back as the text it was. A
// key the reader takes out of a file is gone at the next start, and the window's default stands for it
#ifndef KEPT_H
#define KEPT_H

#include "../text/json_tree.h"

// whether the entry `key`, without `orior.`, is a setting or the list of themes, those a taking of another machine's
// settings changes
int cli_kept_is_carried(const char *key);
// an entry as the page keeps it, from the value written: a string's text, else the value as compact JSON, after `out`
void cli_kept_read_back(const CliJson *json, unsigned int value, CliLine *out);
// every entry kept in `folder`, by its key without `orior.`, written as a JSON object of texts in the order of the
// keys into `out`: 1, or 0 where the window has kept nothing there yet, which is while there is no `state.json`
int cli_kept_read(const char *folder, CliLine *out);
// the changes in `folder` kept, given as a JSON object of each key without `orior.` and its new text, or null where
// it is gone, `length` bytes of `changes`; `state.json` made where it is not there yet: 1, or 0 with why said
// (kept_keep.c)
int cli_kept_keep(const char *folder, const char *changes, unsigned long long length, CliLine *said);

#endif
