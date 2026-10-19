// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// review_test.c: the review's check: a change is the declarations it touches, a move is one piece and callers stand
// before what they call; a declaration added or taken out is one and pieces that call none stand apart; and a commit is
// reviewed against its parent with who made it
#include "../../../../../ui/cli/src/code/review.h"
#include "cli_check.h"

static char s_out[1u << 16u];

// each piece of a review written, `path name how from` with `from` `-` where it has none, `|` between them
static void review_pieces(const CliJson *json, unsigned int review, CliLine *line)
{
    const unsigned int pieces = cli_json_member(json, review, "pieces");
    for (unsigned int at = 0u; cli_json_item(json, pieces, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int piece = cli_json_item(json, pieces, at);
        const char *const from = cli_json_string(json, cli_json_member(json, piece, "from"), NULL);
        cli_line_text(line, (at > 0u) ? "|" : "");
        cli_line_text(line, cli_check_member_text(piece, "path"));
        cli_line_character(line, ' ');
        cli_line_text(line, cli_check_member_text(piece, "name"));
        cli_line_character(line, ' ');
        cli_line_text(line, cli_check_member_text(piece, "how"));
        cli_line_character(line, ' ');
        cli_line_text(line, (from != NULL) ? from : "-");
    }
}

// a JSON member of a node written compact into `line`
static void review_member(const CliJson *json, unsigned int node, const char *name, CliLine *line)
{
    cli_json_write(json, cli_json_member(json, node, name), line, 0u, 0);
}

// the review of `count` files given whole, read into the shared tree: its node
static unsigned int review_of(const char *const *paths, const char *const *was, const char *const *now,
                              unsigned int count, const CliJson **json)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_review_files(paths, was, now, count, &out);
    unsigned int review = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &review);
    return review;
}

// a change is the declarations it touches, a move is one piece and callers stand before what they call
static void review_change(void)
{
    static const char *const s_paths[3] = {"a.py", "b.py", "notes.md"};
    static const char *const s_was[3] = {"def main():\n    helper()\n    return 0\n\ndef helper():\n    return "
                                         "1\n\n\ndef moving():\n    x = 1\n    y = 2\n"
                                         "    return x + y\n",
                                         "def other():\n    return 3\n", "one"};
    static const char *const s_now[3] = {
        "def main():\n    helper()\n    other()\n    return 0\n\n\ndef helper():\n    return 2\n",
        "def other():\n    return moving()\n\n\ndef moving():\n    x = 1\n    y = 2\n    return x + y\n", "two"};
    const CliJson *json = NULL;
    const unsigned int review = review_of(s_paths, s_was, s_now, 3u, &json);
    char text[1024];
    CliLine line = {text, sizeof(text), 0ull};
    review_pieces(json, review, &line);
    cli_check_text(text, line.at,
                   "a.py main changed -|a.py helper changed -|b.py other changed -|b.py moving moved a.py",
                   "a change is the declarations it touches, a move one piece");
    line.at = 0ull;
    review_member(json, review, "other", &line);
    cli_check_text(text, line.at, "[\"notes.md\"]", "a file of no language read here is named apart");
    const unsigned int pieces = cli_json_member(json, review, "pieces");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, pieces, 3u), "was"),
                                           .other = "def moving():\n    x = 1\n    y = 2\n    return x + y",
                                           .cap = CLI_CELLS_POOL}),
              "the blank lines at a declaration's end left off");
    line.at = 0ull;
    review_member(json, cli_json_item(json, pieces, 0u), "calls", &line);
    cli_check_text(text, line.at, "[1,2]", "main calls helper and other");
    line.at = 0ull;
    review_member(json, cli_json_item(json, pieces, 2u), "calls", &line);
    cli_check_text(text, line.at, "[3]", "other calls moving");
    line.at = 0ull;
    review_member(json, review, "groups", &line);
    cli_check_text(text, line.at, "[[0,1,2,3]]", "one group, each caller before what it calls");
}

// a declaration added or taken out is one and pieces that call none stand apart
static void review_added(void)
{
    static const char *const s_paths[1] = {"c.rs"};
    static const char *const s_was[1] = {"fn gone() {\n}\n\nfn kept() {\n}\n"};
    static const char *const s_now[1] = {"fn kept() {\n}\n\nfn fresh() {\n    let a = 1;\n}\n"};
    const CliJson *json = NULL;
    unsigned int review = review_of(s_paths, s_was, s_now, 1u, &json);
    char text[1024];
    CliLine line = {text, sizeof(text), 0ull};
    review_pieces(json, review, &line);
    cli_check_text(text, line.at, "c.rs gone removed -|c.rs fresh added -", "a declaration added or taken out is one");
    line.at = 0ull;
    review_member(json, review, "groups", &line);
    cli_check_text(text, line.at, "[[0],[1]]", "pieces that call none stand apart");
    static const char *const s_moved_paths[2] = {"d.py", "e.py"};
    static const char *const s_moved_was[2] = {"def f():\n    a = 1\n    b = 2\n    c = 3\n", NULL};
    static const char *const s_moved_now[2] = {NULL, "def f():\n    a = 1\n    b = 2\n    c = 4\n"};
    review = review_of(s_moved_paths, s_moved_was, s_moved_now, 2u, &json);
    line.at = 0ull;
    review_pieces(json, review, &line);
    cli_check_text(text, line.at, "e.py f moved d.py", "two of three lines kept is a move");
}

// git run in `root` with `count` words after it, what it wrote into `out` where it is not NULL
static void review_git(const char *root, const char *const *words, unsigned int count, CliLine *out)
{
    const char *line[8] = {"git"};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line[at + 1u] = words[at];
    }
    line[count + 1u] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_ran(line, root, NULL, 0u, ran, &said) && (ran->code == 0), words[0]);
    for (unsigned long long at = 0ull; (out != NULL) && (at < ran->out_size) && (ran->out[at] > ' '); at += 1ull)
    {
        cli_line_character(out, ran->out[at]);
    }
    cli_ran_given(ran);
}

// a commit is reviewed against its parent with who made it
static void review_commit(void)
{
    static const char *const s_init[2] = {"init", "-q"};
    static const char *const s_name[3] = {"config", "user.name", "Tester"};
    static const char *const s_email[3] = {"config", "user.email", "tester@example.com"};
    static const char *const s_add[2] = {"add", "."};
    static const char *const s_first[4] = {"commit", "-q", "-m", "first"};
    static const char *const s_second[4] = {"commit", "-q", "-am", "second"};
    static const char *const s_head[2] = {"rev-parse", "HEAD"};
    static const char s_one[] = "def one():\n    return 1\n";
    static const char s_two[] = "def one():\n    return two()\n\n\ndef two():\n    return 2\n";
    static const char s_three[] = "def one():\n    return 3\n";
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("review_commit", root);
    cli_full(root, "a.py", path);
    review_git(root, s_init, 2u, NULL);
    review_git(root, s_name, 3u, NULL);
    review_git(root, s_email, 3u, NULL);
    (void)query_file_written(path, s_one, sizeof(s_one) - 1u);
    review_git(root, s_add, 2u, NULL);
    review_git(root, s_first, 4u, NULL);
    (void)query_file_written(path, s_two, sizeof(s_two) - 1u);
    review_git(root, s_second, 4u, NULL);
    char id[128];
    CliLine id_line = {id, sizeof(id), 0ull};
    review_git(root, s_head, 2u, &id_line);
    (void)cli_line_closed(&id_line);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_review(root, id, &out, &said), "a commit is reviewed");
    unsigned int review = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(s_out, out.at, &review);
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(review, "by"), .other = "Tester", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(review, "subject"), .other = "second", .cap = CLI_CELLS_POOL})),
              "with who made it and its subject");
    char text[1024];
    CliLine line = {text, sizeof(text), 0ull};
    review_pieces(json, review, &line);
    cli_check_text(text, line.at, "a.py one changed -|a.py two added -", "against its parent");
    line.at = 0ull;
    review_member(json, review, "groups", &line);
    cli_check_text(text, line.at, "[[0,1]]", "the caller before what it calls");
    (void)query_file_written(path, s_three, sizeof(s_three) - 1u);
    out.at = 0ull;
    (void)cli_review(root, NULL, &out, &said);
    json = cli_check_parsed(s_out, out.at, &review);
    cli_check(cli_json_is(json, cli_json_member(json, review, "by"), CFG_JSON_NULL), "the tree's change has no maker");
    line.at = 0ull;
    review_pieces(json, review, &line);
    cli_check_text(text, line.at, "a.py one changed -|a.py two removed -", "and is the tree against its last commit");
}

int main(void)
{
    review_change();
    review_added();
    review_commit();
    return cli_checked();
}
