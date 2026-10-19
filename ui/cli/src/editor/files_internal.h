// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_internal.h: what the files sources share: git's view of the tree, held under its lock while it is read
#ifndef FILES_INTERNAL_H
#define FILES_INTERNAL_H

#include "files.h"

// A view of the tree: every file git tracks or would track, and every folder holding one, or for a tree git cannot
// read every file walked from its top and no folders; whether git read the tree; and the milliseconds the clock that
// only goes forward read as the view was read
typedef struct
{
    CliFiles files;
    CliFiles dirs;
    int git;
    unsigned long long at;
} CliSeen;

// the view of the tree at `root`, as kept, read again behind it once it is older than four seconds, or read now where
// none is kept: the view, held in place until cli_seen_let gives it back, or NULL where no view is free to be read for
// it or the cells cannot meet it (files_seen.c)
const CliSeen *cli_seen_held(const char *root);
void cli_seen_let(const CliSeen *seen);
// the tree's files, less `.git` and `target`, walked from its top: each file's path from the top handed to `each`, at
// most CLI_FILES_MOST of them (files_seen.c)
void cli_files_walked(const char *root, void (*each)(void *context, const char *path, unsigned long long length),
                      void *context);

// The files of a held view a reader keeps: the view, held in place while they are kept, and the places in its files of
// those kept, `count` of them, a cell of the cellblock counted to the files
typedef struct
{
    const CliSeen *seen;
    unsigned int *place;
    unsigned int count;
} CliPicked;

// a cell of `bytes` of the cellblock counted to the files, or NULL where it cannot meet them, the files' report told
// where their mark rose a step past the one last told; and a cell given back, NULL given back as nothing. A list made
// for `count` paths of `bytes` together, each held to the list's limits, none made for no paths: 1, or 0 where the
// cellblock cannot meet its cells, the list left empty; a path put after the files: 1, or 0 where they are full; the
// paths sorted by their bytes, each kept once, the sort's spare a cell taken and given back: 1, or 0 where it could not
// be taken; and a list's cells given back and the list emptied. The files of the view of the tree at `root` that
// `keeps` keeps picked, the view held: 1, or 0 where no view could be held or the cellblock cannot meet their places,
// nothing held; the path of the one picked at `index`; and the view let go and the places given back (files_cells.c)
void *cli_files_cell_taken(unsigned long long bytes);
void cli_files_cell_given(void *cell);
int cli_files_made(CliFiles *files, unsigned long long count, unsigned long long bytes);
int cli_files_put(CliFiles *files, const char *path, unsigned long long length);
int cli_files_sorted(CliFiles *files);
void cli_files_emptied(CliFiles *files);
int cli_files_picked(CliPicked *picked, const char *root, int (*keeps)(void *context, const char *path), void *context);
const char *cli_picked_at(const CliPicked *picked, unsigned int index);
void cli_picked_let(CliPicked *picked);

// the most lines a search returns, the most letters of a found line kept, and the most bytes they take
#define CLI_HITS_MOST 2000u
#define CLI_HIT_TEXT 240u
#define CLI_HIT_BYTES (CLI_HIT_TEXT * 4u)

// What a search looks for: the query and its bytes, whether it is all ASCII, its letters in lower case, and how it is
// read
typedef struct
{
    const char *query;
    unsigned long long length;
    int ascii;
    const unsigned int *lower;
    unsigned int lower_count;
    CliSearching how;
} CliSought;

// a line a search found handed on: its line and column counted from 1, and the bytes of its text a hit keeps
typedef void (*CliHitPut)(void *context, unsigned long long line, unsigned long long col, const unsigned char *text,
                          unsigned long long size);

// of `size` bytes of a line, the bytes a hit keeps: those before its trailing whitespace, at most CLI_HIT_TEXT letters
// of them; whether a letter joins a word, for a search of whole words; and each line of a file's `size` bytes that
// holds the query, the first place on it the query stands, at most CLI_HITS_MOST of them, handed to `put`. Text in
// ASCII without case, and any text with case, is found by the sift over the bytes; text outside ASCII without case is
// found line by line in its lower case (files_hits.c)
unsigned long long cli_hit_cut(const unsigned char *text, unsigned long long size);
int cli_hit_wordy(unsigned int letter);
void cli_files_hits_in(const unsigned char *bytes, unsigned long long size, const CliSought *sought, CliHitPut put,
                       void *context);

// what a search by git came to: git cannot search the tree, which is then searched for the query as text; the lines it
// found written; or why it could not read the expression said
#define CLI_GIT_SEARCH_NONE 0
#define CLI_GIT_SEARCH_FOUND 1
#define CLI_GIT_SEARCH_FAILED 2

// a found line written as the window reads it, `{"path", "line", "col", "text"}`, a comma before it where it is not the
// first; and a regular expression searched by git over the files `specs` name, less those `patterns` or `set` hide, at
// most CLI_HITS_MOST lines written as a JSON array (files_find.c)
void cli_hit_written(CliLine *out, int first, const char *path, unsigned long long line, unsigned long long col,
                     const unsigned char *text, unsigned long long size);
int cli_search_by_git(const char *root, const char *query, CliSearching how, const CliWords *specs,
                      const CliPatterns *patterns, const CliPatterns *set, CliLine *out, CliLine *said);

// a file or folder of a paste's walk handed on: where it is, where it goes, and whether it is a folder: 1, or 0 with
// why said
typedef int (*CliPasteEach)(void *context, const char *source, const char *target, int folder, CliLine *said);

// the last part of `path`, its trailing slashes left off, into `name`, which holds CLI_PATH_LONGEST: 1, or 0 where it
// has none; whether `path` is `folder` or inside it, a slash either way; the path for `name` in `folder`, numbered
// where the folder holds that name already, `notes (2).txt` and on, into `path`, which holds CLI_PATH_LONGEST; `source`
// and all it holds walked, each handed to `each` with where it goes under `target`, a folder before what it holds and
// its entries in the order the system lists them: 1, or 0 with why said where `each` gave 0 or a folder could not be
// read; and a pair pasted written as `{"from", "to"}`, `from` null where it is not given (files_paste.c)
int cli_paste_name(const char *path, char *name);
int cli_paste_under(const char *folder, const char *path);
void cli_paste_free(const char *folder, const char *name, char *path);
int cli_paste_walked(const char *source, const char *target, CliPasteEach each, void *context, CliLine *said);
void cli_pasted_written(CliLine *out, int first, const char *from, const char *to);

// whether a letter is whitespace, as Unicode reads it; whether a word starts at `at` of `text`: the text's start, after
// a space, a slash, a separator in a name, a dot, a colon or an at, or an upper case letter after one that is not, a
// text with `bytes` 1 being the bytes of a path all ASCII, whose only spaces are a space and a tab; and the match of
// `wanted`, lower case, in `shown` and its lower case `lower`, `count` letters, letter for letter, a letter past `from`
// scoring more, `latest` holding CLI_FUZZY_LETTERS for its own use: 1 with its score and its places, or 0 where it
// does not match (files_fuzzy.c)
int cli_fuzzy_white(unsigned int letter);
int cli_fuzzy_starts_word(const unsigned int *text, unsigned int at, int bytes);
int cli_fuzzy_letters(const unsigned int *wanted, unsigned int wanted_count, const unsigned int *shown,
                      const unsigned int *lower, unsigned int count, unsigned int from, int bytes, unsigned int *latest,
                      double *score, unsigned int *hits, unsigned int *hit_count);

#endif
