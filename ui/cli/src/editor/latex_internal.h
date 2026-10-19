// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_internal.h: what the LaTeX sources share: the room a make reads into under its lock, the names the engine
// writes read as the tree's files, and what a make leaves read: its log's errors, its pages, and its SyncTeX boxes
#ifndef LATEX_INTERNAL_H
#define LATEX_INTERNAL_H

#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "latex.h"

// the most bytes of the SVG drawn and of the SyncTeX file unzipped, the most boxes open at once, the most lines marked
// in them, the most files a SyncTeX file names and the bytes their names take
#define CLI_LATEX_SVG (1u << 25u)
#define CLI_LATEX_SYNC (1u << 25u)
#define CLI_LATEX_OPEN 4096u
#define CLI_LATEX_MARKS (1u << 18u)
#define CLI_LATEX_INPUTS 8192u
#define CLI_LATEX_INPUT_TEXT (1u << 20u)

// A box open in a SyncTeX file: its point and its width, height and depth in the file's own units, and the first of
// the marks in the room that are its
typedef struct
{
    long long x;
    long long y;
    long long size[3];
    unsigned int marks;
} CliTexOpen;

// A line marked in an open box: its file's tag, the point across where its part of the box starts, and the line
typedef struct
{
    unsigned int tag;
    long long x;
    unsigned long long line;
} CliTexMark;

// A file a SyncTeX file names by its tag: whether it is a file of the tree, and where in the room its path starts
typedef struct
{
    unsigned int tag;
    int named;
    unsigned int at;
} CliTexInput;

// The room a make reads into: the toolchains, the PATH's folders, the tool found, the PATH and TEXINPUTS a program
// runs with, the SVG with its ids made the preview's own, the SyncTeX file unzipped, and its boxes, marks and files
typedef struct
{
    CliFound found;
    char path[CLI_PATH_TEXT];
    char inputs[CLI_PATH_TEXT];
    char svg[CLI_LATEX_SVG];
    unsigned char sync[CLI_LATEX_SYNC];
    CliTexOpen open[CLI_LATEX_OPEN];
    CliTexMark marks[CLI_LATEX_MARKS];
    CliTexInput files[CLI_LATEX_INPUTS];
    char names[CLI_LATEX_INPUT_TEXT];
} CliLatexRoom;

// the room, a cell taken with the lock makes are read under and given back with it; and the lock taken with the room:
// 1, or 0 with the lock given back where the cells cannot meet it; and the room given back with the lock (latex.c)
extern CliLatexRoom *g_cli_latex;
extern CliLock g_cli_latex_lock;
int cli_latex_held(void);
void cli_latex_released(void);

// The file of the tree a name the engine writes stands for: the text made for `path`, by its name or its full path in
// the make's folder, a file under the tree by its path there, and none for any other
typedef struct
{
    char job[CLI_PATH_LONGEST];
    char made[CLI_PATH_LONGEST];
    char root[CLI_PATH_LONGEST];
    const char *path;
} CliTexNames;

// whether a byte is whitespace, as ASCII has it; the next line of `length` bytes of `text` from `at`, split at each
// "\n", a "\r" before it taken off: 1 with its start and size and `at` moved past it, or 0 where none is left; the
// digits from `at` of `length` bytes, their count; and a whole text read as Rust reads an f64, and as a count no
// greater than `most` or a signed 64-bit number, a sign before it as Rust takes one: 1, or 0 where it reads as none
// (latex_text.c)
int cli_tex_white(char letter);
int cli_tex_line(const char *text, unsigned long long length, unsigned long long *at, unsigned long long *start,
                 unsigned long long *size);
unsigned long long cli_tex_digits(const char *text, unsigned long long length, unsigned long long at);
int cli_tex_double(const char *text, unsigned long long length, double *value);
int cli_tex_count(const char *text, unsigned long long length, unsigned long long most, unsigned long long *value);
int cli_tex_signed(const char *text, unsigned long long length, long long *value);

// the names a make in `dir` of the job `job` for the file `path` of the tree `root` reads; and the file of the tree
// `length` bytes of a name stand for into `file`, which holds CLI_PATH_LONGEST: 1, or 0 where they stand for none
void cli_tex_names_made(CliTexNames *names, const char *root, const char *dir, const char *job, const char *path);
int cli_tex_names_of(const CliTexNames *names, const char *name, unsigned long long length, char *file);

// the errors a log names, as `file:line: text` with -file-line-error, the first CLI_LATEX_ERRORS of them, written as
// JSON objects one after another, a comma before each but the first: their count; and the SVG pdftocairo draws, which
// is UTF-8, written as `"defs": ..., "pages": [...]`, every id in it written with `tex-` before it so that none meets
// an id of the window's: 1, or 0 where it is larger than the room holds (latex_read.c)
unsigned int cli_tex_errors_written(const char *log, unsigned long long size, const CliTexNames *names, CliLine *out);
int cli_tex_pages_written(const char *svg, unsigned long long size, CliLine *out);

// the boxes of text a SyncTeX file says each page holds, each with the lines of a file of the tree that set it, written
// as a JSON array (latex_sync.c)
void cli_tex_boxes_written(const char *sync, unsigned long long size, const CliTexNames *names, CliLine *out);

// the name a file is made under into `job`, which holds CLI_PATH_LONGEST: its own where that is a plain one, of ASCII
// letters, digits, - and _, which keeps \jobname and the bibliography made beside it, and `preview` for any other
// (latex.c)
void cli_latex_job_of(const char *file, char *job);

#endif
