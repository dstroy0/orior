// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files.h: the editor's reads and writes, every one inside the tree (files_seen.c, files_cells.c, files_list.c,
// files_fuzzy.c, files_ranked.c, files_search.c, files_hits.c, files_find.c, files_read.c, files_write.c,
// files_paste.c, files_bring.c).
//
// A folder's list holds every entry in it but git's own folder, and marks the ones the tree's ignore files leave out:
// what a build writes, what a cache keeps and what a tool keeps for itself. A search finds only what git tracks or
// would track. A tree git cannot read is listed whole, less `.git` and `target`, and nothing in it is marked
#ifndef FILES_H
#define FILES_H

#include "../text/patterns.h"

// the most paths the tree's list holds, and the most bytes of them
#define CLI_FILES_MOST 100000u

#define CLI_FILES_TEXT (1u << 24u)

// The tree's files as a view holds them: each path where it starts in the text, ended by a 0, sorted by their bytes,
// `count` of them in room for `most`; and the bytes the text uses, in room for `text_most`. A view's starts and text
// are cells of the cellblock, counted to the files, taken at the sizes the tree gives
typedef struct
{
    unsigned int *start;
    unsigned int count;
    unsigned int most;
    char *text;
    unsigned long long used;
    unsigned long long text_most;
} CliFiles;

// every file git tracks or would track, or for a tree git cannot read every file less `.git` and `target`, at most
// CLI_FILES_MOST of them, sorted, as the view of the tree at `root` holds them: the view, held in place until
// cli_files_let gives it back, so no view a holder reads is filled again under it; or NULL where neither of the two
// views is free to be filled for a tree read now, or the cellblock cannot meet it (files_seen.c)
const CliFiles *cli_files_held(const char *root);
void cli_files_let(const CliFiles *files);
// the name of every file of the same view, and every ending, `.lean` from a name's last dot, each once, sorted, held as
// cli_files_held holds the files and given back by cli_files_let; or NULL as there (files_seen.c)
const CliFiles *cli_files_kinds_held(const char *root);
// the files' own report, `<pool's bytes> <their mark> 0`, the most the views and lists held of the cellblock at once,
// written into `out`; the path
// at `index`; and whether `path` is among the files, which are sorted (files_cells.c)
void cli_files_cells_said(CliLine *out);
const char *cli_files_at(const CliFiles *files, unsigned int index);
int cli_files_holds(const CliFiles *files, const char *path);
// git's view of the tree read behind the window, as a tree opens; git's view let go, to be read again by the next list
// or search, as after files are made, moved or taken out; and whether git tracks the path or would: a file it lists, a
// file in a folder holding one it lists or at the tree's top, or a file in a folder the view does not hold, as one just
// made, that git does not ignore. Every path is, in a tree git cannot read (files_seen.c)
void cli_files_warm(const char *root);
void cli_files_forget_seen(void);
int cli_files_tracked(const char *root, const char *path);

// a folder's entries, less what the explorer's patterns hide, written as the window reads them, `[{"name", "path",
// "dir", "ignored", "archive", "raw"}]`, folders first, then by name, case aside; where a pattern keeps only what it
// names, a folder shows while it holds a file kept or a pattern keeps it. 1, or 0 with why said (files_list.c)
int cli_files_list(const char *root, const char *dir, CliLine *out, CliLine *said);

// the most letters a text a match reads holds
#define CLI_FUZZY_LETTERS 4096u

// the places in `text` that match `query`, counted in letters, into `hits`, which holds CLI_FUZZY_LETTERS, their count
// into `count`, and the match's score into `score`: 1, or 0 where it does not match. The query's letters match in
// order anywhere in the text, case aside, in no more pieces than half its letters or three. A letter scores more at the
// start of a word or past `from`, where the text's name starts; the letters of `query` a match looks for, in lower
// case, its spaces left out; a text's letters as a match reads them, as they are and in lower case; and the match of
// letters already read, `wanted` in a text's `shown` and `lower`, `count` letters each, `latest` holding
// CLI_FUZZY_LETTERS for the match's own use (files_fuzzy.c)
int cli_files_fuzzy(const char *query, const char *text, unsigned long long from, double *score, unsigned int *hits,
                    unsigned int *count);
unsigned int cli_files_wanted(const char *query, unsigned int *letters, unsigned int room);
unsigned int cli_files_letters(const char *text, unsigned int *shown, unsigned int *lower, unsigned int room);
int cli_files_fuzzy_held(const unsigned int *wanted, unsigned int wanted_count, const unsigned int *shown,
                         const unsigned int *lower, unsigned int count, unsigned int *latest, double *score,
                         unsigned int *hits, unsigned int *hit_count);

// the quick open's files read behind the window, where they are neither held nor being read; whether they are not
// read yet; and read again behind the window, as after the search's patterns change; and the tree's files that answer
// `query`, at most `most` of them, the best first, written as `{"found": [{"path", "score", "hits"}], "reading"}`:
// until the files are read, those of `recent` alone answer; a file in `recent` scores more, and one named as typed
// more again; with `fuzzy` off a file answers only where its path holds the query as typed, case aside
// (files_ranked.c)
void cli_files_read_behind(const char *root);
int cli_files_unread(const char *root);
void cli_files_forget_held(void);
void cli_files_ranked(const char *root, const char *query, const char *const *recent, unsigned int recents,
                      unsigned int most, int fuzzy, CliLine *out);
// the same of the `count` paths of `files`, as though they were the tree's files read
void cli_files_ranked_in(const char *const *files, unsigned int count, const char *query, const char *const *recent,
                         unsigned int recents, unsigned int most, int fuzzy, CliLine *out);

// How a search reads its query: case as given or not, whole words only or not, and a regular expression or the text
typedef struct
{
    int match_case;
    int word;
    int regex;
} CliSearching;

// every line in the tree's files that holds the query, at most 2000 of them, less the files the search's patterns
// hide, and only in the files `set` names where it names any, written as `[{"path", "line", "col", "text"}]`: 1, or
// 0 with why said; and every listed file whose path holds each word of the query, case aside, at most 500 of them,
// less the files the explorer's patterns hide, written as a JSON array of paths (files_search.c, files_find.c)
int cli_files_search(const char *root, const char *query, CliSearching how, const CliPatterns *set, CliLine *out,
                     CliLine *said);
void cli_files_find(const char *root, const char *query, CliLine *out);

// a file read for the editor, written as `{"path", "size", "text", "bytes", "windowed"}`: its text where it is text,
// its first bytes where it is not, and a larger text windowed, for the page to hold as pages; and the whole lines
// from about `start` to about `end`, written as `{"start", "end", "size", "line", "lines", "text"}`. Each 1, or 0
// with why said (files_read.c)
int cli_files_read(const char *root, const char *file, CliLine *out, CliLine *said);
int cli_files_slice(const char *root, const char *file, unsigned long long start, unsigned long long end, CliLine *out,
                    CliLine *said);
// an empty file made at `file`, and a folder at `dir`, with each folder above that is not there yet, one there already
// left as it is; and `length` bytes of `text` written beside the file and moved over it, a failed write leaving it as
// it was, a file within an archive saved back into it. Each 1, or 0 with why said (files_write.c)
int cli_files_create_file(const char *root, const char *file, CliLine *said);
int cli_files_create_folder(const char *root, const char *dir, CliLine *said);
int cli_files_write(const char *root, const char *file, const char *text, unsigned long long length, CliLine *said);
// the tree's way of writing a file, which every writer of one takes: the text is written beside the file at `path`,
// as `.name.saving` in its folder, the path into `beside`, which holds CLI_PATH_LONGEST, and moved over the file once
// whole, the file left as it was where the move fails and the text beside removed; `file` names the file in what is
// said. Each 1, or 0 with why said (files_write.c)
int cli_files_saving_beside(const char *path, const char *file, char *beside, CliLine *said);
int cli_files_saved(const char *beside, const char *path, const char *file, CliLine *said);

// what is told as a file is brought: the tree's path of it, the bytes copied and its size
typedef void (*CliTold)(void *context, const char *path, unsigned long long done, unsigned long long size);

// a call made of the tree's side on another machine: its name and its JSON arguments, the answer into `out`: 1, or 0
// with why said
typedef int (*CliCall)(void *context, const char *name, const char *arguments, CliLine *out, CliLine *said);

// each of `from`, a file or a folder and all it holds, copied or moved into the folder `into` of the tree, a name the
// folder holds already numbered, `notes (2).txt` and on, written as `[{"from", "to"}]`; the same copied as a paste
// copies it, each file told as it is copied; the tree's path for `name` in `into`, numbered where it holds it already;
// `size` bytes of `data` written at `at` of the file at the tree's `path` as it is brought a part at a time, the last
// moving it over the file; and each of `from`, on this machine, copied into `into` of a tree on another machine through
// `call`. Each 1, or 0 with why said (files_paste.c, files_bring.c)
int cli_files_paste(const char *root, const char *into, const char *const *from, unsigned int count, int moving,
                    CliLine *out, CliLine *said);
int cli_files_bring(const char *root, const char *into, const char *const *from, unsigned int count, CliTold told,
                    void *context, CliLine *out, CliLine *said);
int cli_files_free_in(const char *root, const char *into, const char *name, char *path, CliLine *said);
int cli_files_write_part(const char *root, const char *path, unsigned long long at, const void *data,
                         unsigned long long size, int last, CliLine *said);
int cli_files_bring_over(CliCall call, void *call_context, const char *into, const char *const *from,
                         unsigned int count, CliTold told, void *context, CliLine *out, CliLine *said);

#endif
