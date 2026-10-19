// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.h: paged texts: files too large for the page to hold, held here as the file on the disk and the lines written
// over it, the page holding only the lines near its view.
//
// A file opened paged is read for an index behind the page: the byte each 1024th line starts at, and how many lines
// there are, which grows as the reading goes and is whole once it ends. A line is read from the disk from the nearest
// line the index holds before it. The lines are split as the page splits a file it reads: at each "\n", a "\r" before
// it taken off, and a file that ends in "\n" has an empty line last.
//
// The text is a list of pieces, each a run of the file's lines or of lines written here, and an edit the page makes
// takes the lines it reaches out of their pieces and puts its own in their place: the file on the disk is read and
// never written until the text is saved. A save writes every piece in turn to a file beside the file, moves it over the
// file, and reads the file afresh. The lines written since the file was read are the changes the gutter marks
// (pages.c, pages_edit.c)
#ifndef PAGES_H
#define PAGES_H

#include "document.h"

// the file at `path` opened as the paged text `key`, its index read on a thread of its own; its shape as it stands
// written as `{"lines", "indexed", "size"}`: 1, or 0 with why said. The shape of the text under `key`; its lines from
// `from` to before `to`, once the index has read as far as `to` or the whole file, as far as the text holds them,
// written as a JSON array of strings; and the text let go
int cli_pages_open(const char *key, const char *path, CliLine *out, CliLine *said);
int cli_pages_shape(const char *key, CliLine *out, CliLine *said);
int cli_pages_read(const char *key, unsigned long long from, unsigned long long to, CliLine *out, CliLine *said);
void cli_pages_release(const char *key);

// the page's changes written, each as its document writes it, `edits[at]` edits of `changes[at]`, `count` changes,
// its edits placed against the text as it stood before any of them, written from the last to the first, the text's
// shape written after; the lines written since the file was read, each run written as `[from, to]`; the text's matches
// for `sought`, each `[line, from col, to col]`, read 16384 lines at a time until `most` are found; and the text saved
// to its file through a file beside it, the file read afresh, its index whole before the save answers, its shape
// written. Each 1, or 0 with why said (pages_edit.c)
int cli_pages_edit(const char *key, const CliEdit *const *changes, const unsigned int *edits, unsigned int count,
                   CliLine *out, CliLine *said);
int cli_pages_changed(const char *key, CliLine *out, CliLine *said);
int cli_pages_find(const char *key, const CliDocSought *sought, unsigned long long most, CliLine *out, CliLine *said);
int cli_pages_save(const char *key, CliLine *out, CliLine *said);

#endif
