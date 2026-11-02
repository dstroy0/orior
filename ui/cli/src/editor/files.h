// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files.h: the editor's reads and writes, every one inside the tree (files_seen.c, files_cells.c, files_list.c,
// files_fuzzy.c, files_ranked.c, files_search.c, files_hits.c, files_find.c, files_read.c, files_write.c,
// files_paste.c, files_bring.c).
//
// A folder's list contains every entry in it but git's own folder, and marks the ones the tree's ignore files leave
// out: what a build writes, what a cache stores and what a tool stores for itself. A search finds only what git tracks
// or would track. A tree git cannot read is listed whole, less `.git` and `target`, and nothing in it is marked
#ifndef FILES_H
#define FILES_H

#include "../text/patterns.h"

// the most paths the tree's list contains, and the most bytes of them
#define CLI_FILES_MOST 50000u

#define CLI_FILES_TEXT (1u << 23u)

// A list of paths: each path where it starts in the text, ended by a 0, `count` of them in room for `most`; and the
// bytes the text uses, in room for `text_most`. Its starts and text are cells of the cellblock, counted to the files,
// taken at the sizes known as it is made
typedef struct
{
    unsigned int *start;
    unsigned int count;
    unsigned int most;
    char *text;
    unsigned long long used;
    unsigned long long text_most;
} CliFiles;

// every file of the tree at `root`, at most CLI_FILES_MOST of them, sorted, laid flat in the temporary tier for the
// length of one call, until cli_files_let gives it back; or NULL where the cellblock cannot meet it (files_seen.c)
const CliFiles *cli_files_view(const char *root);
void cli_files_let(const CliFiles *files);
// the files of the folder at `root`, less `.git` and `target`, walked from its top: each file's path from the top
// handed to `each`, at most CLI_FILES_MOST of them (files_seen.c)
void cli_files_walked(const char *root, void (*each)(void *context, const char *path, unsigned long long length),
                      void *context);
// each path the tree lists off its trunk handed to `each`, git's where git reads the tree, its word into `*git`: 1
// where the listing is whole; and a folder's alone, by the same means. Whether a path runs through a folder a walk does
// not go into, `git` naming whether git decides the rest. Which of `paths`, `count` of them, git ignores, 1 into
// `ignored` for each (files_seen.c)
int cli_files_listed(const char *root, void (*each)(void *context, const char *path, unsigned long long length),
                     void *context, int *git);
int cli_files_folder_listed(const char *root, const char *folder, int git,
                            void (*each)(void *context, const char *path, unsigned long long length), void *context);
int cli_files_path_skipped(const char *path, int git);
void cli_files_ignored_read(const char *root, const char *const *paths, unsigned int count, unsigned char *ignored);
// the name of every file of the tree, and every ending, `.lean` from a name's last dot, each once, sorted, laid flat
// for one call and given back by cli_files_let; or NULL as there (files_seen.c)
const CliFiles *cli_files_kinds_view(const char *root);
// the files' own report, `<pool's bytes> <their mark> 0`, the most the views and lists occupied of the cellblock at
// once, written into `out`; the path at `index`; and whether `path` is among the files, which are sorted
// (files_cells.c)
void cli_files_cells_usage_written(VerbaLinea *out);
const char *cli_files_at(const CliFiles *files, unsigned int index);
int cli_files_contains(const CliFiles *files, const char *path);
// the tree listed behind the window, as a tree opens; and whether git tracks the path or would: a file it lists, a
// file in a folder containing one it lists or at the tree's top, or a file in a folder the tree does not contain, as
// one just made, that git does not ignore. Every path is, in a tree git cannot read (files_seen.c, files_tracked.c)
void cli_files_warm(const char *root);
int cli_files_tracked(const char *root, const char *path);

// a folder's entries, less what the explorer's patterns hide, written as the window reads them, `[{"name", "path",
// "dir", "ignored", "archive", "raw"}]`, folders first, then by name, case aside; where a pattern includes only what it
// names, a folder shows while it contains a file included or a pattern includes it. 1, or 0 with why given
// (files_list.c)
int cli_files_list(const char *root, const char *dir, VerbaLinea *out, VerbaLinea *err_msg);

// the most letters a text a match reads contains
#define CLI_FUZZY_LETTERS 4096u

// the places in `text` that match `query`, counted in letters, into `hits`, sized for CLI_FUZZY_LETTERS, their count
// into `count`, and the match's score into `score`: 1, or 0 where it does not match. The query's letters match in
// order anywhere in the text, case aside, in no more pieces than half its letters or three. A letter scores more at the
// start of a word or past `from`, where the text's name starts; the letters of `query` a match looks for, in lower
// case, its spaces left out; a text's letters as a match reads them, as they are and in lower case; and the match of
// letters already read, `wanted` in a text's `shown` and `lower`, `count` letters each, `latest` sized for
// CLI_FUZZY_LETTERS for the match's own use (files_fuzzy.c)
int cli_files_fuzzy(const char *query, const char *text, unsigned long long from, double *score, unsigned int *hits,
                    unsigned int *count);
unsigned int cli_files_wanted(const char *query, unsigned int *letters, unsigned int room);
unsigned int cli_files_letters(const char *text, unsigned int *shown, unsigned int *lower, unsigned int room);
int cli_files_fuzzy_matches(const unsigned int *wanted, unsigned int wanted_count, const unsigned int *shown,
                            const unsigned int *lower, unsigned int count, unsigned int *latest, double *score,
                            unsigned int *hits, unsigned int *hit_count);

// the tree listed behind the window for the quick open, where it is not listed; whether it is not listed yet; and the
// tree's files that answer
// `query`, at most `most` of them, the best first, written as `{"found": [{"path", "score", "hits"}], "reading"}`:
// until the files are read, those of `recent` alone answer; a file in `recent` scores more, and one named as typed
// more again; with `fuzzy` off a file answers only where its path contains the query as typed, case aside
// (files_ranked.c)
void cli_files_read_behind(const char *root);
int cli_files_unread(const char *root);
void cli_files_ranked(const char *root, const char *query, const char *const *recent, unsigned int recents,
                      unsigned int most, int fuzzy, VerbaLinea *out);
// the same of the `count` paths of `files`, as though they were the tree's files read
void cli_files_ranked_in(const char *const *files, unsigned int count, const char *query, const char *const *recent,
                         unsigned int recents, unsigned int most, int fuzzy, VerbaLinea *out);

// How a search reads its query: case as given or not, whole words only or not, and a regular expression or the text
typedef struct
{
    int match_case;
    int word;
    int regex;
} CliSearching;

// every line in the tree's files that contains the query, at most 2000 of them, less the files the search's patterns
// hide, and only in the files `set` names where it names any, written as `[{"path", "line", "col", "text"}]`: 1, or
// 0 with why given; and every listed file whose path contains each word of the query, case aside, at most 500 of them,
// less the files the explorer's patterns hide, written as a JSON array of paths (files_search.c, files_find.c)
int cli_files_search(const char *root, const char *query, CliSearching how, const CliPatterns *set, VerbaLinea *out,
                     VerbaLinea *err_msg);
void cli_files_find(const char *root, const char *query, VerbaLinea *out);

// a file read for the editor, written as `{"path", "size", "text", "bytes", "windowed"}`: its text where it is text,
// its first bytes where it is not, and a larger text windowed, for the page to store as pages; and the whole lines
// from about `start` to about `end`, written as `{"start", "end", "size", "line", "lines", "text"}`. Each 1, or 0
// with why given (files_read.c)
int cli_files_read(const char *root, const char *file, VerbaLinea *out, VerbaLinea *err_msg);
int cli_files_slice(const char *root, const char *file, unsigned long long start, unsigned long long end,
                    VerbaLinea *out, VerbaLinea *err_msg);
// an empty file made at `file`, and a folder at `dir`, with each folder above that is not there yet, one there already
// left as it is; and `length` bytes of `text` written beside the file and moved over it, a failed write leaving it as
// it was, a file within an archive saved back into it. Each 1, or 0 with why given (files_write.c)
int cli_files_create_file(const char *root, const char *file, VerbaLinea *err_msg);
int cli_files_create_folder(const char *root, const char *dir, VerbaLinea *err_msg);
int cli_files_write(const char *root, const char *file, const char *text, unsigned long long length,
                    VerbaLinea *err_msg);
// the tree's way of writing a file, which every writer of one takes: the text is written beside the file at `path`,
// as `.name.saving` in its folder, the path into `beside`, sized for CLI_PATH_MAX_LEN, and moved over the file once
// whole, the file left as it was where the move fails and the text beside removed; `file` names the file in what is
// given. Each 1, or 0 with why given (files_write.c)
int cli_files_saving_beside(const char *path, const char *file, char *beside, VerbaLinea *err_msg);
int cli_files_saved(const char *beside, const char *path, const char *file, VerbaLinea *err_msg);

// what is reported as a file is brought: the tree's path of it, the bytes copied and its size
typedef void (*CliProgressFn)(void *context, const char *path, unsigned long long done, unsigned long long size);

// a call made of the tree's side on another machine: its name and its JSON arguments, the answer into `out`: 1, or 0
// with why given
typedef int (*CliCall)(void *context, const char *name, const char *arguments, VerbaLinea *out, VerbaLinea *err_msg);

// each of `from`, a file or a folder and all it contains, copied or moved into the folder `into` of the tree, a name
// the folder contains already numbered, `notes (2).txt` and on, written as `[{"from", "to"}]`; the same copied as a
// paste copies it, each file reported as it is copied; the tree's path for `name` in `into`, numbered where it contains
// it already; `size` bytes of `data` written at `at` of the file at the tree's `path` as it is brought a part at a
// time, the last moving it over the file; and each of `from`, on this machine, copied into `into` of a tree on another
// machine through `call`. Each 1, or 0 with why given (files_paste.c, files_bring.c)
int cli_files_paste(const char *root, const char *into, const char *const *from, unsigned int count, int moving,
                    VerbaLinea *out, VerbaLinea *err_msg);
int cli_files_bring(const char *root, const char *into, const char *const *from, unsigned int count,
                    CliProgressFn progress, void *context, VerbaLinea *out, VerbaLinea *err_msg);
int cli_files_free_in(const char *root, const char *into, const char *name, char *path, VerbaLinea *err_msg);
int cli_files_write_part(const char *root, const char *path, unsigned long long at, const void *data,
                         unsigned long long size, int last, VerbaLinea *err_msg);
int cli_files_bring_over(CliCall call, void *call_context, const char *into, const char *const *from,
                         unsigned int count, CliProgressFn progress, void *context, VerbaLinea *out,
                         VerbaLinea *err_msg);

#endif
