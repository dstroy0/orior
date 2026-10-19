// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages_test.c: the paged text's check: lines are read as the page splits them from anywhere in the file, and edits are
// written over the file's lines and saved through a file beside it
#include "../../../../../ui/cli/src/editor/pages.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static char s_out[1u << 20u];
static char s_folder[CLI_PATH_LONGEST];
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
        CliLine said = {why, sizeof(why), 0ull};
        CliLine out = {s_out, sizeof(s_out), 0ull};
        (void)cli_pages_shape(key, &out, &said);
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
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_pages_read(key, from, to, &out, &said);
    unsigned int lines = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &lines);
    char joined[1024];
    CliLine line = {joined, sizeof(joined), 0ull};
    for (unsigned int at = 0u; cli_json_item(json, lines, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const text = cli_json_string(json, cli_json_item(json, lines, at), NULL);
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, (text != NULL) ? text : "");
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
    CliLine text = {s_text, sizeof(s_text), 0ull};
    for (unsigned int n = 0u; n < 5000u; n += 1u)
    {
        cli_line_text(&text, "line ");
        cli_line_decimal(&text, n, 1u);
        cli_line_text(&text, "\r\n");
    }
    char path[CLI_PATH_LONGEST];
    pages_file("read.txt", s_text, text.at, path);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open("read", path, &out, &said), "the file opens as a paged text");
    cli_check(pages_indexed("read").lines == 5001ull, "a file ending in a newline has an empty line last");
    pages_read_checked("read", 0ull, 2ull, "line 0|line 1", "the first lines read");
    pages_read_checked("read", 2047ull, 2050ull, "line 2047|line 2048|line 2049", "lines in the middle read");
    pages_read_checked("read", 4999ull, 5001ull, "line 4999|", "the last lines read, the empty one last");
    cli_pages_release("read");
}

// edits are written over the file's lines and saved through a file beside it
static void pages_edit(void)
{
    CliLine text = {s_text, sizeof(s_text), 0ull};
    for (unsigned int n = 0u; n < 3000u; n += 1u)
    {
        cli_line_text(&text, "row ");
        cli_line_decimal(&text, n, 1u);
        cli_line_character(&text, '\n');
    }
    char path[CLI_PATH_LONGEST];
    pages_file("edit.txt", s_text, text.at, path);
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    (void)cli_pages_open("edit", path, &out, &said);
    (void)pages_indexed("edit");
    static const CliEdit s_edits[2] = {{{10ull, 0ull}, {10ull, 3ull}, "ROW", 3ull},
                                       {{2000ull, 4ull}, {2001ull, 4ull}, "joined\nand ", 11ull}};
    const CliEdit *const changes[1] = {s_edits};
    const unsigned int counts[1] = {2u};
    out.at = 0ull;
    cli_check(cli_pages_edit("edit", changes, counts, 1u, &out, &said) && (pages_shape_of(out.at).lines == 3001ull),
              "the edits are written, the text one line longer");
    char shadow[CLI_PATH_LONGEST];
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
              "the lines written, and only those, are kept in the shadow beside the file");
    pages_read_checked("edit", 9ull, 12ull, "row 9|ROW 10|row 11", "an edit within a line is read");
    pages_read_checked("edit", 2000ull, 2002ull, "row joined|and 2001", "an edit that joins lines is read");
    out.at = 0ull;
    (void)cli_pages_changed("edit", &out, &said);
    cli_check_text(s_out, out.at, "[[10,11],[2000,2002]]", "the lines written since the file was read are marked");
    static const CliEdit s_taken[1] = {{{5ull, 0ull}, {7ull, 0ull}, "", 0ull}};
    const CliEdit *const taken[1] = {s_taken};
    const unsigned int taken_counts[1] = {1u};
    out.at = 0ull;
    (void)cli_pages_edit("edit", taken, taken_counts, 1u, &out, &said);
    pages_read_checked("edit", 4ull, 7ull, "row 4|row 7|row 8", "lines taken out move the rest up");
    const CliDocSought sought = {"ROW", 0, 1, 0, 0};
    out.at = 0ull;
    (void)cli_pages_find("edit", &sought, 10ull, &out, &said);
    cli_check_text(s_out, out.at, "[[8,0,3]]", "find reads the text as it stands");
    out.at = 0ull;
    cli_check(cli_pages_save("edit", &out, &said), "the text is saved");
    const PagesShape saved = pages_shape_of(out.at);
    cli_check((saved.lines == 2999ull) && saved.indexed, "the saved file is read afresh");
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text) - 1u);
    s_text[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    static const char s_head[] = "row 0\nrow 1\nrow 2\nrow 3\nrow 4\nrow 7\nrow 8\nrow 9\nROW 10\n";
    cli_check(cli_text_same(s_text, s_head, sizeof(s_head) - 1u), "the file holds the edits");
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
    (void)cli_pages_changed("edit", &out, &said);
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
    CliLine said = {why, sizeof(why), 0ull};
    int opened = 1;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char name[CLI_NAME_LONGEST];
        CliLine named = {name, sizeof(name), 0ull};
        cli_line_text(&named, s_keys[at]);
        cli_line_text(&named, ".txt");
        char path[CLI_PATH_LONGEST];
        pages_file(name, s_keys[at], cellul.len(&(CatenaFinitaCfg){.src = s_keys[at], .cap = CLI_CELLS_POOL}), path);
        CliLine out = {s_out, sizeof(s_out), 0ull};
        opened = cli_pages_open(s_keys[at], path, &out, &said) && opened;
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

int main(void)
{
    cli_check_folder("pages", s_folder);
    pages_read();
    pages_edit();
    pages_many();
    return cli_checked();
}
