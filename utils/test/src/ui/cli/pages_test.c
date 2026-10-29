// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_test.c: the paged text's check: lines are read as the page splits them from anywhere in the file; a view is
// placed the lines it takes, a wrapping one by its rows and a step by the bytes it reads; and edits are written over the
// file's lines and saved through a file beside it
#include "../../../../../ui/cli/src/editor/pages.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static char s_out[1u << 20u];
static char s_folder[CLI_PATH_MAX_LEN];
static char s_text[1u << 17u];

// A shape: its lines, and whether its index is whole
typedef struct
{
    unsigned long long lines;
    int indexed;
} PagesShape;

// a whole number member of a node of the shared tree, 0 where it has none
static unsigned long long pages_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = cli_json_member(json, node, name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; cli_json_is(json, number, CFG_JSON_NUMBER) && (at < json->node[number].length);
         at += 1ull)
    {
        value = value * 10ull + (unsigned long long)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

// a shape written as `{"lines", "indexed", "size"}` read
static PagesShape pages_shape_of(unsigned long long size)
{
    unsigned int shape = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, size, &shape);
    const PagesShape read = {pages_whole(json, shape, "lines"),
                             cli_json_is(json, cli_json_member(json, shape, "indexed"), CFG_JSON_TRUE)};
    return read;
}

// the shape of `key` once its index is whole, waited for up to five seconds
static PagesShape pages_indexed(const char *key)
{
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    for (;;)
    {
        char why[256];
        VerbaLinea err_msg = {why, sizeof(why), 0ull};
        VerbaLinea out = {s_out, sizeof(s_out), 0ull};
        (void)cli_pages_shape(key, &out, &err_msg);
        const PagesShape shape = pages_shape_of(out.at);
        if (shape.indexed || (cli_clock_milliseconds() > until))
        {
            return shape;
        }
    }
}

// the lines of `key` from `from` to before `to`, joined by bars, checked against `wanted`
static void pages_read_checked(const char *key, unsigned long long from, unsigned long long to, const char *wanted,
                               const char *what)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)cli_pages_read(key, from, to, NULL, &out, NULL, &err_msg);
    unsigned int lines = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &lines);
    char joined[1024];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, lines, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const text = cli_json_string(json, cli_json_item(json, lines, at), NULL);
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, (text != NULL) ? text : "");
    }
    cli_check_text(joined, line.at, wanted, what);
}

// the file `name` of the folder written with `size` bytes of `text`, its path into `path`
static void pages_file(const char *name, const char *text, unsigned long long size, char *path)
{
    cli_full(s_folder, name, path);
    (void)query_file_written(path, text, size);
}

// lines are read as the page splits them from anywhere in the file
static void pages_read(void)
{
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    for (unsigned int n = 0u; n < 5000u; n += 1u)
    {
        mmgr_line_put(&text, "line ");
        mmgr_line_decimal(&text, n, 1u);
        mmgr_line_put(&text, "\r\n");
    }
    char path[CLI_PATH_MAX_LEN];
    pages_file("read.txt", s_text, text.at, path);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("read", path, &out, &err_msg), "the file opens as a paged text");
    cli_check(pages_indexed("read").lines == 5001ull, "a file ending in a newline has an empty line last");
    pages_read_checked("read", 0ull, 2ull, "line 0|line 1", "the first lines read");
    pages_read_checked("read", 2047ull, 2050ull, "line 2047|line 2048|line 2049", "lines in the middle read");
    pages_read_checked("read", 4999ull, 5001ull, "line 4999|", "the last lines read, the empty one last");
    const CliDocSought sought = {"line 0", 0, 1, 0};
    out.at = 0ull;
    (void)cli_pages_find("read", &sought, 10ull, &out, &err_msg);
    cli_check_text(s_out, out.at, "[[0,0,6]]", "find reaches the first line of a block of letters of one byte each");
    cli_pages_release("read");
}

// a file of long lines: 1000 bytes each, the first naming its number as "long N"
static char s_long[1u << 19u];

// the lines of `key` from `from` to before `to` read, each checked to be the long line of its number, whole
static void pages_long_checked(const char *key, unsigned long long from, unsigned long long to, const char *what)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)cli_pages_read(key, from, to, NULL, &out, NULL, &err_msg);
    unsigned int lines = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &lines);
    int whole = 1;
    unsigned int count = 0u;
    for (unsigned int at = 0u; cli_json_item(json, lines, at) != CLI_JSON_NONE; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(json, cli_json_item(json, lines, at), &length);
        char named[32];
        VerbaLinea name = {named, sizeof(named), 0ull};
        mmgr_line_put(&name, "long ");
        mmgr_line_decimal(&name, from + at, 1u);
        whole = whole && (text != NULL) && (length == 1000ull) &&
                (memor.cmp(&(MemoriaCfg){.src = text, .other = named, .bytes = name.at}) == 0) && (text[name.at] == 'x');
        count += 1u;
    }
    cli_check(whole && (count == (to - from)), what);
}

// a file of long lines is read from anywhere in it: the index records a start every CLI_PAGE_SPAN bytes as well as
// every CLI_PAGE_STEP lines, and a reading begins at the start nearest before its first line
static void pages_long(void)
{
    VerbaLinea text = {s_long, sizeof(s_long), 0ull};
    for (unsigned int n = 0u; n < 400u; n += 1u)
    {
        const unsigned long long begun = text.at;
        mmgr_line_put(&text, "long ");
        mmgr_line_decimal(&text, n, 1u);
        while ((text.at - begun) < 1000ull)
        {
            mmgr_line_ch(&text, 'x');
        }
        mmgr_line_ch(&text, '\n');
    }
    char path[CLI_PATH_MAX_LEN];
    pages_file("long.txt", s_long, text.at, path);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("long", path, &out, &err_msg), "a file of long lines opens as a paged text");
    cli_check(pages_indexed("long").lines == 401ull, "its lines are counted");
    pages_long_checked("long", 0ull, 2ull, "its first lines read whole");
    pages_long_checked("long", 64ull, 68ull, "lines either side of the first start recorded by its bytes read whole");
    pages_long_checked("long", 200ull, 240ull, "a view's lines in the middle read whole");
    pages_long_checked("long", 390ull, 400ull, "its last lines read whole");
    cli_pages_release("long");
}

// the lines a view of the long file is answered, from `from` to before `to` cut at `cut` letters, each checked to be
// the long line of its number cut there, and its length in the page's units 1000
static void pages_cut_checked(const char *key, const CliPagesPlaced *placed, const char *what)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out) / 2u, 0ull};
    VerbaLinea lengths = {s_out + (sizeof(s_out) / 2u), sizeof(s_out) / 2u, 0ull};
    unsigned long long made = placed->cut;
    (void)cli_pages_read(key, placed->from, placed->to, &made, &out, &lengths, &err_msg);
    unsigned int lines = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_out, out.at, &lines);
    int cut = 1;
    unsigned int count = 0u;
    for (unsigned int at = 0u; cli_json_item(json, lines, at) != CLI_JSON_NONE; at += 1u)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(json, cli_json_item(json, lines, at), &length);
        char named[32];
        VerbaLinea name = {named, sizeof(named), 0ull};
        mmgr_line_put(&name, "long ");
        mmgr_line_decimal(&name, placed->from + at, 1u);
        // the file ends in a newline, its last line, the 401st, empty
        const int last = (placed->from + at) == 400ull;
        cut = cut && (text != NULL) &&
              (last ? (length == 0ull)
                    : ((length == ((placed->cut != 0ull) ? placed->cut : 1000ull)) &&
                       (memor.cmp(&(MemoriaCfg){.src = text, .other = named, .bytes = name.at}) == 0)));
        count += 1u;
    }
    unsigned int counted = CLI_JSON_NONE;
    json = cli_check_parsed(s_out + (sizeof(s_out) / 2u), lengths.at, &counted);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int length = cli_json_item(json, counted, at);
        const int last = (placed->from + at) == 400ull;
        cut = cut && cli_json_is(json, length, CFG_JSON_NUMBER) &&
              (last ? (json->text[json->node[length].text] == '0')
                    : ((json->node[length].length == 4u) && (json->text[json->node[length].text] == '1')));
    }
    cli_check(cut && (made == placed->cut) && (count == (placed->to - placed->from)), what);
}

// lines no longer than a cut are read whole, and the read says none was cut
static void pages_short_uncut(void)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    char path[CLI_PATH_MAX_LEN];
    cli_full(s_folder, "read.txt", path);
    (void)cli_pages_open("short", path, &out, &err_msg);
    (void)pages_indexed("short");
    unsigned long long made = 64ull;
    out.at = 0ull;
    (void)cli_pages_read("short", 10ull, 13ull, &made, &out, NULL, &err_msg);
    cli_check_text(s_out, out.at, "[\"line 10\",\"line 11\",\"line 12\"]", "lines shorter than the cut are read whole");
    cli_check(made == 0ull, "a read of lines no longer than its cut tells none was cut");
    cli_pages_release("short");
}

// A view the page reports is placed here: as it moves, the lines past the page's on the side it goes, a step of
// them, once it comes near their end, cut at the letters it reaches, and where it jumped away from them its rows where
// it will stand a report later; as it rests, its rows whole, then the lines it keeps whole a step at a time; and
// nothing where the page holds what it needs
static void pages_viewed(void)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(s_folder, "long.txt", path);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("viewed", path, &out, &err_msg), "the file of long lines opens to be viewed");
    (void)pages_indexed("viewed");
    CliPagesView view = {100ull, 20ull, 64ull, 90ull, 130ull, 0ull, 0ull, 0ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.cut == 0ull) && (placed.from <= 100ull) &&
                  (placed.to >= 120ull) && (placed.to - placed.from < 60ull),
              "a view first reported rests: its rows are answered whole, and few more");
    pages_cut_checked("viewed", &placed, "the rows of a view at rest are read whole");
    view.held_from = placed.from;
    view.held_to = placed.to;
    view.top = 110ull;
    const unsigned long long held_end = view.held_to;
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.cut == 64ull) && (placed.from == held_end) &&
                  (placed.to == 401ull) && (placed.keep_from <= view.held_from) && (placed.keep_to >= placed.to),
              "a view that moved 10 lines past the page's lines is answered a step of the lines after them, as far as "
              "the file goes here, cut at its reach");
    pages_cut_checked("viewed", &placed, "the lines of a moving view are read cut at its reach, their lengths whole");
    view.held_to = placed.to;
    view.held_cut = placed.cut;
    view.top = 115ull;
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.from == placed.to) &&
                  (placed.keep_from <= view.held_from) && (placed.keep_to >= view.held_to),
              "a view the page's lines hold far from their end is answered nothing, and lets none go");
    view.held_to = 200ull;
    view.top = 120ull;
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.from == 200ull) && (placed.to == 401ull),
              "a view moving down within NEAR lines of the page's last line is answered the step after it");
    view.held_to = 148ull;
    view.top = 165ull;
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.cut == 64ull) && (placed.from > 165ull) &&
                  (placed.from <= 191ull) && (placed.to >= 249ull) && (placed.to - placed.from < 60ull),
              "a view that moved 45 lines away from the page's lines, more than its rows, is answered the rows 45 "
              "lines on alone, with EDGE and a quarter of the 45 lines on either side of them");
    view.held_from = placed.from;
    view.held_to = placed.to;
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.cut == 0ull) && (placed.from <= 165ull) &&
                  (placed.to >= 185ull),
              "a view reported where it was reported last rests: its rows are answered whole");
    view.held_from = placed.from;
    view.held_to = placed.to;
    view.held_cut = 0ull;
    view.map_from = 60ull;
    view.map_to = 300ull;
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.cut == 0ull) && (placed.from == view.held_to) &&
                  (placed.to > placed.from) && (placed.keep_from <= 60ull) && (placed.keep_to >= 300ull),
              "a view at rest whose lines are whole is answered the lines below them it keeps, a step");
    view.held_from = 300ull;
    view.held_to = 400ull;
    view.top = 350ull;
    (void)cli_pages_viewed("viewed", &view, &placed);
    view.top = 230ull;
    cli_check(cli_pages_viewed("viewed", &view, &placed) && (placed.from <= 72ull) && (placed.to >= 168ull),
              "a view that moved 120 lines up is answered the rows 120 lines on, with EDGE and a quarter of the 120 "
              "lines on either side of them, a step of its cut lines holding them all");
    cli_pages_release("viewed");
}

// A view that wraps its lines at 100 letters, each line of the file of long lines ten rows: placed the lines its rows
// take, as many about its top as fill the rows placed on either side, at rest and as it moves; and a view the page holds
// those lines for at rest answered the step below them, never those lines again
static void pages_wrapped(void)
{
    char path[CLI_PATH_MAX_LEN];
    cli_full(s_folder, "long.txt", path);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("wrapped", path, &out, &err_msg), "the file of long lines opens to be viewed wrapped");
    (void)pages_indexed("wrapped");
    CliPagesView view = {100ull, 20ull, 0ull, 90ull, 130ull, 0ull, 0ull, 0ull, 100ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    cli_check(cli_pages_viewed("wrapped", &view, &placed) && (placed.from == 99ull) && (placed.to == 103ull),
              "a wrapping view at rest is placed the lines its rows take: the line above its top, and three from it");
    view.held_from = placed.from;
    view.held_to = placed.to;
    cli_check(cli_pages_viewed("wrapped", &view, &placed) && (placed.from == 103ull) && (placed.to > placed.from),
              "and once the page holds them, the step below them");
    view.top = 200ull;
    cli_check(cli_pages_viewed("wrapped", &view, &placed) && (placed.from >= 290ull) && (placed.from <= 299ull) &&
                  (placed.to >= 302ull) && (placed.to <= 310ull),
              "a wrapping view that moved 100 lines is placed the lines its rows take where it will stand a report "
              "later, a few each side");
    cli_pages_release("wrapped");
}

// a file of short lines and then long ones, its lines' bytes on average far less than those of the long ones
static char s_mixed[1u << 21u];

// A step toward the lines a view at rest keeps is read as far as CLI_PAGE_REST_BYTES of its lines' bytes reach, one
// line at the least, however much shorter the file's lines are on average
static void pages_step_bytes(void)
{
    VerbaLinea text = {s_mixed, sizeof(s_mixed), 0ull};
    for (unsigned int n = 0u; n < 2000u; n += 1u)
    {
        mmgr_line_put(&text, "s ");
        mmgr_line_decimal(&text, n, 1u);
        mmgr_line_ch(&text, '\n');
    }
    for (unsigned int n = 0u; n < 60u; n += 1u)
    {
        for (unsigned int at = 0u; at < 20000u; at += 1u)
        {
            mmgr_line_ch(&text, 'x');
        }
        mmgr_line_ch(&text, '\n');
    }
    char path[CLI_PATH_MAX_LEN];
    pages_file("mixed.txt", s_mixed, text.at, path);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("mixed", path, &out, &err_msg), "a file of short lines and then long ones opens");
    (void)pages_indexed("mixed");
    CliPagesView view = {1980ull, 20ull, 0ull, 1960ull, 2050ull, 1972ull, 2008ull, 0ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    cli_check(cli_pages_viewed("mixed", &view, &placed) && (placed.from == 2008ull) &&
                  ((placed.to - placed.from) >= 1ull) && ((placed.to - placed.from) <= 7ull),
              "a step into the long lines below a view at rest is as many of them as 128 KB reach");
    view.top = 2010ull;
    view.map_from = 2000ull;
    view.map_to = 2040ull;
    view.held_from = 0ull;
    view.held_to = 0ull;
    (void)cli_pages_viewed("mixed", &view, &placed);
    cli_check(cli_pages_viewed("mixed", &view, &placed) && (placed.from == 2002ull) &&
                  ((placed.to - placed.from) >= 1ull) && ((placed.to - placed.from) <= 7ull),
              "a view at rest among the long lines is answered its rows whole from EDGE above its top, as many as "
              "128 KB reach");
    view.held_from = placed.from;
    view.held_to = placed.to;
    const unsigned long long whole_to = placed.to;
    cli_check(cli_pages_viewed("mixed", &view, &placed) && (placed.from == whole_to) && (placed.to > placed.from),
              "and once the page holds those whole, the rows after them");
    cli_pages_release("mixed");
}

// a file of many short lines, "row N", for a view moving thousands of them a report
static char s_rows[1u << 18u];

// A view moving far through a file of short lines: the lines about its rows where it will stand no more than a step of
// them on either side, however far it went
static void pages_spread(void)
{
    VerbaLinea text = {s_rows, sizeof(s_rows), 0ull};
    for (unsigned int n = 0u; n < 20000u; n += 1u)
    {
        mmgr_line_put(&text, "row ");
        mmgr_line_decimal(&text, n, 1u);
        mmgr_line_ch(&text, '\n');
    }
    char path[CLI_PATH_MAX_LEN];
    pages_file("rows.txt", s_rows, text.at, path);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("rows", path, &out, &err_msg), "the file of short lines opens to be viewed");
    (void)pages_indexed("rows");
    CliPagesView view = {19000ull, 20ull, 64ull, 19000ull, 19020ull, 0ull, 0ull, 0ull};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    (void)cli_pages_viewed("rows", &view, &placed);
    view.top = 14000ull;
    cli_check(cli_pages_viewed("rows", &view, &placed) && (placed.from <= 7992ull) && (placed.to >= 10028ull) &&
                  ((placed.to - placed.from) <= 2036ull),
              "a view that moved 5000 lines up through short lines is answered the rows 5000 lines on, with EDGE and "
              "a step of 1000 lines on either side of them, no more");
    cli_pages_release("rows");
}

// edits are written over the file's lines and saved through a file beside it
static void pages_edit(void)
{
    VerbaLinea text = {s_text, sizeof(s_text), 0ull};
    for (unsigned int n = 0u; n < 3000u; n += 1u)
    {
        mmgr_line_put(&text, "row ");
        mmgr_line_decimal(&text, n, 1u);
        mmgr_line_ch(&text, '\n');
    }
    char path[CLI_PATH_MAX_LEN];
    pages_file("edit.txt", s_text, text.at, path);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    const int opened = cli_pages_open("edit", path, &out, &err_msg);
    (void)mmgr_line_finish(&err_msg);
    cli_check(opened, opened ? "the file opens for its edits" : why);
    (void)pages_indexed("edit");
    static const CliEdit s_edits[2] = {{{10ull, 0ull}, {10ull, 3ull}, "ROW", 3ull},
                                       {{2000ull, 4ull}, {2001ull, 4ull}, "joined\nand ", 11ull}};
    const CliEdit *const changes[1] = {s_edits};
    const unsigned int counts[1] = {2u};
    out.at = 0ull;
    const int edited = cli_pages_edit("edit", changes, counts, 1u, &out, &err_msg);
    (void)mmgr_line_finish(&err_msg);
    cli_check(edited && (pages_shape_of(out.at).lines == 3001ull),
              edited ? "the edits are written, the text one line longer"
                     : ((why[0] != '\0') ? why : "the edit was refused with no reason given"));
    char shadow[CLI_PATH_MAX_LEN];
    cli_full(s_folder, ".edit.txt.~", shadow);
    const unsigned long long shadowed = qry_member_file_read(shadow, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(shadowed == QRY_MEMBER_UNREAD) ? 0u : shadowed] = '\0';
    cli_check((cellul.find(&(CatenaFinitaCfg){
                   .src = s_text, .cap = CLI_CELLS_POOL, .other = "ROW 10\n", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_text,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "row joined\nand 2001\n",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_text, .cap = CLI_CELLS_POOL, .other = "row 9", .other_cap = CLI_CELLS_POOL}) == NULL),
              "the lines written, and only those, are stored in the shadow beside the file");
    pages_read_checked("edit", 9ull, 12ull, "row 9|ROW 10|row 11", "an edit within a line is read");
    pages_read_checked("edit", 2000ull, 2002ull, "row joined|and 2001", "an edit that joins lines is read");
    out.at = 0ull;
    (void)cli_pages_changed("edit", &out, &err_msg);
    cli_check_text(s_out, out.at, "[[10,11],[2000,2002]]", "the lines written since the file was read are marked");
    static const CliEdit s_taken[1] = {{{5ull, 0ull}, {7ull, 0ull}, "", 0ull}};
    const CliEdit *const taken[1] = {s_taken};
    const unsigned int taken_counts[1] = {1u};
    out.at = 0ull;
    (void)cli_pages_edit("edit", taken, taken_counts, 1u, &out, &err_msg);
    pages_read_checked("edit", 4ull, 7ull, "row 4|row 7|row 8", "lines taken out move the rest up");
    const CliDocSought sought = {"ROW", 0, 1, 0};
    out.at = 0ull;
    (void)cli_pages_find("edit", &sought, 10ull, &out, &err_msg);
    cli_check_text(s_out, out.at, "[[8,0,3]]", "find reads the text as it stands");
    out.at = 0ull;
    cli_check(cli_pages_save("edit", &out, &err_msg), "the text is saved");
    const PagesShape saved = pages_shape_of(out.at);
    cli_check((saved.lines == 2999ull) && saved.indexed, "the saved file is read afresh");
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    static const char s_head[] = "row 0\nrow 1\nrow 2\nrow 3\nrow 4\nrow 7\nrow 8\nrow 9\nROW 10\n";
    cli_check(cli_text_same(s_text, s_head, sizeof(s_head) - 1u), "the file contains the edits");
    cli_check(
        cellul.find(&(CatenaFinitaCfg){
            .src = s_text, .cap = CLI_CELLS_POOL, .other = "row joined\nand 2001\n", .other_cap = CLI_CELLS_POOL}) !=
            NULL,
        "the joined lines are saved");
    cli_check(
        (size != QRY_MEMBER_UNREAD) && (size >= 9u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_text + size - 9u, .other = "row 2999\n", .cap = CLI_CELLS_POOL})),
        "the file ends as it did");
    out.at = 0ull;
    (void)cli_pages_changed("edit", &out, &err_msg);
    cli_check_text(s_out, out.at, "[]", "a saved text is the file's again");
    cli_pages_release("edit");
    cli_check(!query_is_file(shadow), "the shadow is removed once the text is let go");
}

// as many texts are open at once as the cells meet, each let go on its own
static void pages_many(void)
{
    static const char *const s_keys[] = {"one", "two", "three", "four", "five", "six", "seven", "eight"};
    const unsigned int count = sizeof(s_keys) / sizeof(s_keys[0]);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    int opened = 1;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char name[CLI_NAME_MAX_LEN];
        VerbaLinea named = {name, sizeof(name), 0ull};
        mmgr_line_put(&named, s_keys[at]);
        mmgr_line_put(&named, ".txt");
        (void)mmgr_line_finish(&named);
        char path[CLI_PATH_MAX_LEN];
        pages_file(name, s_keys[at], cellul.len(&(CatenaFinitaCfg){.src = s_keys[at], .cap = CLI_CELLS_POOL}), path);
        VerbaLinea out = {s_out, sizeof(s_out), 0ull};
        opened = cli_pages_open(s_keys[at], path, &out, &err_msg) && opened;
    }
    cli_check(opened, "eight texts are open at once");
    pages_read_checked("eight", 0ull, 1ull, "eight", "the last text opened is read");
    cli_pages_release("three");
    pages_read_checked("seven", 0ull, 1ull, "seven", "a text is read after another is let go");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_pages_release(s_keys[at]);
    }
}

// a block of lines wider than a reading covers is read a part at a time: find reaches the last line and the save
// writes every byte
static void pages_wide(void)
{
    static char s_wide[32u * 8192u];
    memor.set(&(MemoriaCfg){.dst = s_wide, .val = 'w', .bytes = sizeof(s_wide)});
    for (unsigned int line = 1u; line <= 32u; line += 1u)
    {
        s_wide[(line * 8192u) - 1u] = '\n';
    }
    static const char s_last[] = "the last line\n";
    char path[CLI_PATH_MAX_LEN];
    cli_full(s_folder, "wide.txt", path);
    int written = cli_file_appended(path, "", 0ull, 1);
    for (unsigned int part = 0u; written && (part < 282u); part += 1u)
    {
        written = cli_file_appended(path, s_wide, sizeof(s_wide), 0);
    }
    written = written && cli_file_appended(path, s_last, sizeof(s_last) - 1u, 0);
    const unsigned long long whole = (282ull * sizeof(s_wide)) + sizeof(s_last) - 1u;
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(written && cli_pages_open("wide", path, &out, &err_msg), "a file of wide lines opens as a paged text");
    cli_check(pages_indexed("wide").lines == 9026ull, "and is indexed whole");
    const CliDocSought sought = {"the last", 0, 1, 0};
    out.at = 0ull;
    err_msg.at = 0ull;
    (void)cli_pages_find("wide", &sought, 10ull, &out, &err_msg);
    cli_check_text(s_out, out.at, "[[9024,0,8]]", "find reaches the line past a block wider than a reading");
    out.at = 0ull;
    err_msg.at = 0ull;
    cli_check(cli_pages_save("wide", &out, &err_msg), "the wide text is saved");
    unsigned long long size = 0ull;
    cli_check(cli_file_size(path, &size) && (size == whole), "the save writes every byte of every line");
    cli_pages_release("wide");
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_folder("pages", s_folder);
    pages_read();
    pages_long();
    pages_viewed();
    pages_wrapped();
    pages_step_bytes();
    pages_spread();
    pages_short_uncut();
    pages_edit();
    pages_many();
    pages_wide();
    return cli_checked();
}
