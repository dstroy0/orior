// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// latex_internal.h: what the LaTeX sources share: the room a make reads into under its lock, the names the engine
// writes read as the tree's files, and what a make leaves read: its log's errors, its pages, and its SyncTeX boxes
#ifndef LATEX_INTERNAL_H
#define LATEX_INTERNAL_H
typedef struct CliLatexTextCtx CliLatexTextCtx;
typedef struct CliLatexNamesRootCtx CliLatexNamesRootCtx;
typedef struct CliLatexNamesCtx CliLatexNamesCtx;
typedef struct CliLatexTextLengthCtx CliLatexTextLengthCtx;
typedef struct CliLatexNamesLogCtx CliLatexNamesLogCtx;
typedef struct CliLatexLengthCtx CliLatexLengthCtx;
typedef struct CliLatexTextLengthAtCtx CliLatexTextLengthAtCtx;
typedef struct CliLatexTextLengthValueCtx CliLatexTextLengthValueCtx;
typedef struct CliLatexSizeCtx CliLatexSizeCtx;
typedef struct CliLatexTextLengthValueI64sCtx CliLatexTextLengthValueI64sCtx;
typedef struct CliLatexFileCtx CliLatexFileCtx;
typedef struct CliLatexCtx CliLatexCtx;

#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "embed_dispatch_layout.h"
#include "latex.h"

// the most bytes of the SVG drawn and of the SyncTeX file unzipped, the most boxes open at once, the most lines marked
// in them, the most files a SyncTeX file names and the bytes their names take
#define CLI_LATEX_SVG (1u << 22u)
#define CLI_LATEX_SYNC (1u << 22u)
#define CLI_LATEX_OPEN 4096u
#define CLI_LATEX_MARKS (1u << 16u)
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
int cli_latex_lock(void);
void cli_latex_release(void);

// The file of the tree a name the engine writes stands for: the text made for `path`, by its name or its full path in
// the make's folder, a file under the tree by its path there, and none for any other
typedef struct
{
    char job[CLI_PATH_MAX_LEN];
    char made[CLI_PATH_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    const char *path;
} CliTexNames;

// whether a byte is whitespace, as ASCII has it; the next line of `length` bytes of `text` from `at`, split at each
// "\n", a "\r" before it taken off: 1 with its start and size and `at` moved past it, or 0 where none is left; the
// digits from `at` of `length` bytes, their count; and a whole text read as Rust reads an f64, and as a count no
// greater than `most` or a signed 64-bit number, a sign before it as Rust takes one: 1, or 0 where it reads as none
// (latex_text.c)
int cli_tex_white(const CliLatexCtx *args);
int cli_tex_line(const CliLatexTextCtx *args);
unsigned long long cli_tex_digits(const CliLatexTextLengthAtCtx *args);
int cli_tex_double(const CliLatexTextLengthValueCtx *args);
int cli_tex_count(const CliLatexTextLengthCtx *args);
int cli_tex_sign(const CliLatexTextLengthValueI64sCtx *args);

// the names a make in `dir` of the job `job` for the file `path` of the tree `root` reads; and the file of the tree
// `length` bytes of a name stand for into `file`, sized for CLI_PATH_MAX_LEN: 1, or 0 where they stand for none
void cli_tex_names_init(const CliLatexNamesRootCtx *args);
int cli_tex_names_get(const CliLatexLengthCtx *args);

// the errors a log names, as `file:line: text` with -file-line-error, the first CLI_LATEX_ERRORS of them, written as
// JSON objects one after another, a comma before each but the first: their count; and the SVG pdftocairo draws, which
// is UTF-8, written as `"defs": ..., "pages": [...]`, every id in it written with `tex-` before it so that none meets
// an id of the window's: 1, or 0 where it is larger than the room fits (latex_read.c)
unsigned int cli_tex_errors_write(const CliLatexNamesLogCtx *args);
int cli_tex_pages_write(const CliLatexSizeCtx *args);

// the boxes of text a SyncTeX file reports each page contains, each with the lines of a file of the tree that set it,
// written as a JSON array (latex_sync.c)
void cli_tex_boxes_write(const CliLatexNamesCtx *args);

// the name a file is made under into `job`, sized for CLI_PATH_MAX_LEN: its own where that is a plain one, of ASCII
// letters, digits, - and _, which preserves \jobname and the bibliography made beside it, and `preview` for any other
// (latex.c)
void cli_latex_job_get(const CliLatexFileCtx *args);

struct CliLatexTextCtx
{
    const char *text;
    unsigned long long length;
    unsigned long long *at;
    unsigned long long *start;
    unsigned long long *size_pointer;
};

struct CliLatexNamesRootCtx
{
    CliTexNames *names_out;
    const char *root;
    const char *dir;
    const char *job;
    const char *path;
};

struct CliLatexNamesCtx
{
    const CliTexNames *names;
    unsigned long long size;
    VerbaLinea *out;
    const char *sync;
};

struct CliLatexTextLengthCtx
{
    const char *text;
    unsigned long long length;
    unsigned long long most;
    unsigned long long *value_u64s;
};

struct CliLatexNamesLogCtx
{
    const CliTexNames *names;
    const char *log;
    unsigned long long size;
    VerbaLinea *out;
};

struct CliLatexLengthCtx
{
    unsigned long long length;
    const CliTexNames *names;
    const char *name;
    char *file;
};

struct CliLatexTextLengthAtCtx
{
    const char *text;
    unsigned long long length;
    unsigned long long at_value;
};

struct CliLatexTextLengthValueCtx
{
    const char *text;
    unsigned long long length;
    double *value;
};

struct CliLatexSizeCtx
{
    unsigned long long size;
    VerbaLinea *out;
    const char *svg;
};

struct CliLatexTextLengthValueI64sCtx
{
    const char *text;
    unsigned long long length;
    long long *value_i64s;
};

struct CliLatexFileCtx
{
    const char *file_read;
    char *job_out;
};

struct CliLatexCtx
{
    char letter;
};

typedef struct
{
    int (*tex_white)(const CliLatexCtx *args);
} CliLatexNsTable;
EMBED_TABLE_LAYOUT(CliLatexNsTable, tex_white);
EMBED_TABLE_STORAGE CliLatexNsTable cli_latex_internal EMBED_UNUSED = {
    .tex_white = cli_tex_white,
};

typedef struct
{
    int (*tex_line)(const CliLatexTextCtx *args);
} CliLatexTextNs;
EMBED_TABLE_LAYOUT(CliLatexTextNs, tex_line);
EMBED_TABLE_STORAGE CliLatexTextNs cli_latex_text_internal EMBED_UNUSED = {
    .tex_line = cli_tex_line,
};

typedef struct
{
    unsigned long long (*tex_digits)(const CliLatexTextLengthAtCtx *args);
} CliLatexTextLengthAtNs;
EMBED_TABLE_LAYOUT(CliLatexTextLengthAtNs, tex_digits);
EMBED_TABLE_STORAGE CliLatexTextLengthAtNs cli_latex_text_length_at_internal EMBED_UNUSED = {
    .tex_digits = cli_tex_digits,
};

typedef struct
{
    int (*tex_double)(const CliLatexTextLengthValueCtx *args);
} CliLatexTextLengthValueNs;
EMBED_TABLE_LAYOUT(CliLatexTextLengthValueNs, tex_double);
EMBED_TABLE_STORAGE CliLatexTextLengthValueNs cli_latex_text_length_value_internal EMBED_UNUSED = {
    .tex_double = cli_tex_double,
};

typedef struct
{
    int (*tex_count)(const CliLatexTextLengthCtx *args);
} CliLatexTextLengthNs;
EMBED_TABLE_LAYOUT(CliLatexTextLengthNs, tex_count);
EMBED_TABLE_STORAGE CliLatexTextLengthNs cli_latex_text_length_internal EMBED_UNUSED = {
    .tex_count = cli_tex_count,
};

typedef struct
{
    int (*tex_sign)(const CliLatexTextLengthValueI64sCtx *args);
} CliLatexTextLengthValueI64sNs;
EMBED_TABLE_LAYOUT(CliLatexTextLengthValueI64sNs, tex_sign);
EMBED_TABLE_STORAGE CliLatexTextLengthValueI64sNs cli_latex_text_length_value_i_64s_internal EMBED_UNUSED = {
    .tex_sign = cli_tex_sign,
};

typedef struct
{
    void (*tex_names_init)(const CliLatexNamesRootCtx *args);
} CliLatexNamesRootNs;
EMBED_TABLE_LAYOUT(CliLatexNamesRootNs, tex_names_init);
EMBED_TABLE_STORAGE CliLatexNamesRootNs cli_latex_names_root_internal EMBED_UNUSED = {
    .tex_names_init = cli_tex_names_init,
};

typedef struct
{
    int (*tex_names_get)(const CliLatexLengthCtx *args);
} CliLatexLengthNs;
EMBED_TABLE_LAYOUT(CliLatexLengthNs, tex_names_get);
EMBED_TABLE_STORAGE CliLatexLengthNs cli_latex_length_internal EMBED_UNUSED = {
    .tex_names_get = cli_tex_names_get,
};

typedef struct
{
    unsigned int (*tex_errors_write)(const CliLatexNamesLogCtx *args);
} CliLatexNamesLogNs;
EMBED_TABLE_LAYOUT(CliLatexNamesLogNs, tex_errors_write);
EMBED_TABLE_STORAGE CliLatexNamesLogNs cli_latex_names_log_internal EMBED_UNUSED = {
    .tex_errors_write = cli_tex_errors_write,
};

typedef struct
{
    int (*tex_pages_write)(const CliLatexSizeCtx *args);
} CliLatexSizeNs;
EMBED_TABLE_LAYOUT(CliLatexSizeNs, tex_pages_write);
EMBED_TABLE_STORAGE CliLatexSizeNs cli_latex_size_internal EMBED_UNUSED = {
    .tex_pages_write = cli_tex_pages_write,
};

typedef struct
{
    void (*tex_boxes_write)(const CliLatexNamesCtx *args);
} CliLatexNamesNs;
EMBED_TABLE_LAYOUT(CliLatexNamesNs, tex_boxes_write);
EMBED_TABLE_STORAGE CliLatexNamesNs cli_latex_names_internal EMBED_UNUSED = {
    .tex_boxes_write = cli_tex_boxes_write,
};

typedef struct
{
    void (*job_get)(const CliLatexFileCtx *args);
} CliLatexFileNs;
EMBED_TABLE_LAYOUT(CliLatexFileNs, job_get);
EMBED_TABLE_STORAGE CliLatexFileNs cli_latex_file_internal EMBED_UNUSED = {
    .job_get = cli_latex_job_get,
};

#endif
