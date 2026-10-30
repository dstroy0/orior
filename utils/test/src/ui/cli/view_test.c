// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view_test.c: the editor view's check: a view opened on a stored text holds the cursor and the selection; the keys
// move the cursor as the page's editor moves it, by letters, words, lines, the line's ends and pages, Shift extending
// the selection; text typed replaces the selection; Backspace, Delete, Enter and Tab edit as the page's editor edits;
// undo and redo step the text back and forth, typing joined into one step; the pointer places the cursor and drags a
// selection, a double click takes the word; the view follows the cursor and the frame carries the rows in view, the
// cursor, the anchor and the lines' runs
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/cli/src/editor/pages.h"
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/plugins/plugins.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "cli_check.h"

static const char s_key[] = "check:view";
static const char s_paged[] = "check:paged";
static char s_out[1u << 16u];

static CliViewInput view_key(const char *name)
{
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    input.kind = CLI_VIEW_KEY;
    input.name = name;
    return input;
}

static CliViewInput view_text(const char *text)
{
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    input.kind = CLI_VIEW_TEXT;
    input.text = text;
    input.length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return input;
}

static CliViewInput view_pointer(unsigned int pointer, unsigned long long row, unsigned long long column,
                                 unsigned int clicks, int shift)
{
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    input.kind = CLI_VIEW_POINTER;
    input.pointer = pointer;
    input.row = row;
    input.column = column;
    input.clicks = clicks;
    input.shift = shift;
    return input;
}

// the input given to the view: whether it was taken
static int given(const CliViewInput *input)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int taken = cli_view_input(s_key, input, &err_msg);
    if (!taken)
    {
        (void)mmgr_line_finish(&err_msg);
        cli_check(0, why);
    }
    return taken;
}

static int key(const char *name)
{
    const CliViewInput input = view_key(name);
    return given(&input);
}

static int typed(const char *text)
{
    const CliViewInput input = view_text(text);
    return given(&input);
}

// whether the cursor stands at line, col and the anchor at anchor_line, anchor_col
static int at(unsigned long long line, unsigned long long col, unsigned long long anchor_line,
              unsigned long long anchor_col)
{
    CliPlace cursor;
    CliPlace anchor;
    return cli_view_cursor(s_key, &cursor, &anchor) && (cursor.line == line) && (cursor.col == col) &&
           (anchor.line == anchor_line) && (anchor.col == anchor_col);
}

// whether the text's lines from `first` to before `last` read as `wanted`, joined by "\n"
static int reads(unsigned long long first, unsigned long long last, const char *wanted)
{
    unsigned long long size = 0ull;
    char *const lines = cli_document_lines_taken(s_key, first, last - 1ull, &size, NULL, 0ull, &g_checks_cells);
    const int same = (lines != NULL) &&
                     (size == cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL})) &&
                     (memor.cmp(&(MemoriaCfg){.src = lines, .other = wanted, .bytes = size}) == 0);
    cli_cell_release(lines, &g_checks_cells);
    return same;
}

static unsigned long long lines_now(void)
{
    CliShape shape;
    return cli_document_shape(s_key, &shape) ? shape.lines : 0ull;
}

// a key given to the paged view, and whether its cursor stands at line, col with the anchor there too
static int paged_key(const char *name)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const CliViewInput input = view_key(name);
    const int taken = cli_view_input(s_paged, &input, &err_msg);
    if (!taken)
    {
        (void)mmgr_line_finish(&err_msg);
        cli_check(0, why);
    }
    return taken;
}

static int paged_at(unsigned long long line, unsigned long long col)
{
    CliPlace cursor;
    CliPlace anchor;
    return cli_view_cursor(s_paged, &cursor, &anchor) && (cursor.line == line) && (cursor.col == col) &&
           (anchor.line == line) && (anchor.col == col);
}

// whether the frame written last contains `wanted`
static int frame_has(const char *wanted)
{
    unsigned long long at = 0ull;
    return cli_text_find(s_out, cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = sizeof(s_out)}), wanted,
                         cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL}), &at);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    CliShape shape;
    static const char s_text[] = "int main\n{\n    return 0;\n}\n";
    cli_check(cli_document_store(s_key, s_text, sizeof(s_text) - 1u, &shape), "the text is stored");
    cli_check(!cli_view_opened("check:none", ""), "a view opens on no text that is not stored");
    cli_check(cli_view_opened(s_key, "check/view.c"), "a view opens on the stored text");
    // the text colored by the C plugin's grammar, as the branch of a file is, its regions giving it folds
    char id[CLI_NAME_MAX_LEN];
    char grammar[CLI_NAME_MAX_LEN];
    char why_text[256];
    VerbaLinea why = {why_text, sizeof(why_text), 0ull};
    CliLanguages languages;
    cli_plugins_languages(&languages);
    cli_check(cli_plugins_language_for(&languages, "check/view.c", id), "the path opens as the C plugin's language");
    cli_plugins_languages_given(&languages);
    cli_check(cli_plugins_grammar_stored(id, grammar, &why) && cli_document_colored_by(s_key, grammar, 1),
              "the text is colored by the language's grammar");
    cli_document_walked();
    CliViewInput size = view_key("");
    size.kind = CLI_VIEW_SIZE;
    size.rows = 3u;
    size.columns = 80u;
    cli_check(given(&size), "the view is sized three rows");
    cli_check(at(0ull, 0ull, 0ull, 0ull), "the cursor begins at the text's start");

    cli_check(key("Right") && key("Right") && key("Right") && at(0ull, 3ull, 0ull, 3ull), "Right moves a letter");
    cli_check(key("End") && at(0ull, 8ull, 0ull, 8ull), "End moves to the line's end");
    cli_check(key("Right") && at(1ull, 0ull, 1ull, 0ull), "Right at the end moves to the next line's start");
    cli_check(key("Left") && at(0ull, 8ull, 0ull, 8ull), "Left at the start moves to the line before's end");
    cli_check(key("Down") && at(1ull, 1ull, 1ull, 1ull), "Down keeps the column as far as the line reaches");
    cli_check(key("Down") && at(2ull, 8ull, 2ull, 8ull), "Down again finds the column kept");
    cli_check(key("Home") && at(2ull, 4ull, 2ull, 4ull), "Home moves to the line's first letter");
    cli_check(key("Home") && at(2ull, 0ull, 2ull, 0ull), "Home again moves to the line's start");
    cli_check(key("Mod+Right") && at(2ull, 10ull, 2ull, 10ull), "Mod+Right moves past the word");
    cli_check(key("Mod+Left") && at(2ull, 4ull, 2ull, 4ull), "Mod+Left moves to the word's start");
    cli_check(key("Shift+End") && at(2ull, 13ull, 2ull, 4ull), "Shift+End extends the selection to the end");
    cli_check(key("Left") && at(2ull, 4ull, 2ull, 4ull), "Left with a selection goes to its start");
    cli_check(key("Mod+End") && at(4ull, 0ull, 4ull, 0ull), "Mod+End moves to the text's end");
    cli_check(key("Mod+Home") && at(0ull, 0ull, 0ull, 0ull), "Mod+Home moves to the text's start");
    cli_check(key("PageDown") && at(2ull, 0ull, 2ull, 0ull), "PageDown moves the rows less one");
    cli_check(key("Down") && key("Down") && key("Down") && at(4ull, 0ull, 4ull, 0ull), "Down stops at the last line");

    cli_check(key("Mod+Home") && key("End") &&
                  given(&(CliViewInput){CLI_VIEW_TEXT, NULL, "(void)", 6ull, 0u, 0ull, 0ull, 0u, 0, 0ull, 0u, 0u}) &&
                  reads(0ull, 1ull, "int main(void)") && at(0ull, 14ull, 0ull, 14ull),
              "text typed is written at the cursor, which follows it");
    cli_check(key("Backspace") && reads(0ull, 1ull, "int main(void") && at(0ull, 13ull, 0ull, 13ull),
              "Backspace takes the letter before the cursor");
    cli_check(key("Mod+Z") && reads(0ull, 1ull, "int main(void)") && at(0ull, 14ull, 0ull, 14ull),
              "undo puts the letter back and the cursor after it");
    cli_check(key("Mod+Z") && reads(0ull, 1ull, "int main") && at(0ull, 8ull, 0ull, 8ull),
              "undo takes the typing out as one step");
    cli_check(key("Mod+Y") && reads(0ull, 1ull, "int main(void)"), "redo puts the typing back");
    cli_check(key("Enter") && reads(0ull, 2ull, "int main(void)\n") && (lines_now() == 6ull) &&
                  at(1ull, 0ull, 1ull, 0ull),
              "Enter breaks the line");
    cli_check(key("Backspace") && (lines_now() == 5ull) && at(0ull, 14ull, 0ull, 14ull),
              "Backspace at a line's start joins it to the one before");
    cli_check(key("Delete") && reads(0ull, 1ull, "int main(void){") && at(0ull, 14ull, 0ull, 14ull),
              "Delete at a line's end joins the next to it");
    cli_check(key("Mod+Z") && (lines_now() == 5ull) && reads(0ull, 1ull, "int main(void)"), "undo parts them again");
    cli_check(key("Down") && key("Down") && key("Home") && key("Enter") && reads(2ull, 4ull, "    \n    return 0;") &&
                  at(3ull, 4ull, 3ull, 4ull),
              "Enter keeps the line's indentation");
    cli_check(key("Mod+Z") && (lines_now() == 5ull), "undo takes the break out");
    cli_check(key("Home") && key("Tab") && reads(2ull, 3ull, "        return 0;") && at(2ull, 4ull, 2ull, 4ull),
              "Tab indents by a step");
    cli_check(key("Backspace") && reads(2ull, 3ull, "    return 0;") && at(2ull, 0ull, 2ull, 0ull),
              "Backspace in the indentation takes a step");
    cli_check(key("Shift+Right") && key("Shift+Right") && at(2ull, 2ull, 2ull, 0ull), "Shift+Right extends");
    cli_check(key("Escape") && at(2ull, 2ull, 2ull, 2ull), "Escape collapses the selection");
    cli_check(key("Shift+Left") && key("Shift+Left") &&
                  given(&(CliViewInput){CLI_VIEW_TEXT, NULL, "\t", 1ull, 0u, 0ull, 0ull, 0u, 0, 0ull, 0u, 0u}) &&
                  reads(2ull, 3ull, "\t  return 0;") && at(2ull, 1ull, 2ull, 1ull),
              "text typed replaces the selection");
    cli_check(key("Mod+Z") && reads(2ull, 3ull, "    return 0;") && at(2ull, 0ull, 2ull, 2ull),
              "undo puts the selection back as it was");
    cli_check(key("Mod+A") && at(4ull, 0ull, 0ull, 0ull), "Mod+A selects the whole text");
    cli_check(key("Delete") && (lines_now() == 1ull) && reads(0ull, 1ull, ""), "Delete takes the selection out");
    cli_check(key("Mod+Z") && (lines_now() == 5ull) && reads(0ull, 1ull, "int main(void)"), "undo puts the text back");

    CliViewInput to_top = view_key("");
    to_top.kind = CLI_VIEW_SCROLL;
    to_top.top = 0ull;
    cli_check(given(&to_top), "the view scrolls to the first line");
    const CliViewInput down = view_pointer(CLI_VIEW_POINTER_DOWN, 2ull, 6ull, 1u, 0);
    cli_check(given(&down) && at(2ull, 6ull, 2ull, 6ull), "the pointer pressed places the cursor at its cell");
    const CliViewInput dragged = view_pointer(CLI_VIEW_POINTER_MOVED, 2ull, 10ull, 1u, 0);
    cli_check(given(&dragged) && at(2ull, 10ull, 2ull, 6ull), "the pointer dragged extends the selection");
    const CliViewInput twice = view_pointer(CLI_VIEW_POINTER_DOWN, 2ull, 6ull, 2u, 0);
    cli_check(given(&twice) && at(2ull, 10ull, 2ull, 4ull), "a double press takes the word");
    const CliViewInput past = view_pointer(CLI_VIEW_POINTER_DOWN, 0ull, 40ull, 1u, 0);
    cli_check(given(&past) && at(0ull, 14ull, 0ull, 14ull),
              "the pointer past a line's end places the cursor at its end");

    CliViewInput scroll = view_key("");
    scroll.kind = CLI_VIEW_SCROLL;
    scroll.top = 2ull;
    cli_check(given(&scroll), "the view scrolls to the third line");
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_frame_written(s_key, &out), "the frame is written");
    (void)mmgr_line_finish(&out);
    cli_check(frame_has("\"top\":2,"), "the frame carries the view's top");
    cli_check(frame_has("\"from\":2,\"rows\":[\"    return 0;\",\"}\",\"\"]"), "the frame carries the rows in view");
    cli_check(frame_has("\"cursor\":{\"line\":0,\"col\":14}"), "the frame carries the cursor");
    cli_check(frame_has("\"indent\":{\"tabs\":"), "the frame carries the indentation, tabs or spaces and the step");
    VerbaLinea mapped = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_map_written(s_key, 0ull, 4ull, &mapped) && (mmgr_line_finish(&mapped) != 0ull),
              "the map of the lines is written");
    cli_check(frame_has("{\"from\":0,\"bars\":[[[0,3,"),
              "the map's first line begins with a bar as wide as its first word");
    cli_check(frame_has("],[4,4,"), "the second word's bar starts at its cell");
    cli_check(frame_has("],[[0,1,"), "a line of one letter is one bar");
    cli_check(frame_has("],[[4,6,"), "an indented line's first bar starts past the indentation");
    cli_check(frame_has(",[]]}"), "an empty line has no bar");

    // the words of a second text: the word a place touches, the prefix before the cursor, the file's words as
    // completion items, and a completion accepted written over the prefix
    static const char s_key2[] = "check:words";
    static const char s_text2[] = "alpha beta\nalp gamma\n";
    CliShape shape2;
    cli_check(cli_document_store(s_key2, s_text2, sizeof(s_text2) - 1u, &shape2) &&
                  cli_view_opened(s_key2, "check/words.txt"),
              "a second text is stored and viewed");
    const CliViewInput sized2 = {CLI_VIEW_SIZE, NULL, NULL, 0ull, 0u, 0ull, 0ull, 0u, 0, 0ull, 10u, 40u, 0};
    const CliViewInput pressed2 = {
        CLI_VIEW_POINTER, NULL, NULL, 0ull, CLI_VIEW_POINTER_DOWN, 1ull, 3ull, 1u, 0, 0ull, 0u, 0u, 0};
    const CliViewInput let_go2 = {
        CLI_VIEW_POINTER, NULL, NULL, 0ull, CLI_VIEW_POINTER_UP, 1ull, 3ull, 1u, 0, 0ull, 0u, 0u, 0};
    cli_check(cli_view_input(s_key2, &sized2, &why) && cli_view_input(s_key2, &pressed2, &why) &&
                  cli_view_input(s_key2, &let_go2, &why),
              "a press puts the cursor after alp");
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    char word[64];
    cli_check(cli_view_word_at(s_key2, 0ull, 7ull, &from, &to, word, sizeof(word)) && (from == 6ull) && (to == 10ull) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "beta", .cap = sizeof(word)}),
              "the word at a place is the word the place touches");
    cli_check(cli_view_word_at(s_key2, 0ull, 10ull, &from, &to, word, sizeof(word)) && (from == 6ull),
              "a place at a word's end touches it");
    cli_check(cli_view_word_at(s_key2, 0ull, 5ull, &from, &to, word, sizeof(word)) && (from == 0ull) && (to == 5ull),
              "a place after a word's last letter touches it");
    cli_check(!cli_view_word_at(s_key2, 2ull, 0ull, &from, &to, word, sizeof(word)), "an empty line has no word");
    unsigned long long placed_line = 0ull;
    unsigned long long placed_col = 0ull;
    cli_check(cli_view_place_at(s_key2, 1ull, 7ull, &placed_line, &placed_col) && (placed_line == 1ull) &&
                  (placed_col == 7ull),
              "a row and a cell from the view's top place a line and a column");
    cli_check(cli_view_place_at(s_key2, 0ull, 40ull, &placed_line, &placed_col) && (placed_line == 0ull) &&
                  (placed_col == 10ull),
              "a cell past the line's end places its end");
    cli_check(cli_view_prefix(s_key2, &from, word, sizeof(word)) && (from == 0ull) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "alp", .cap = sizeof(word)}),
              "the prefix is the word before the cursor");
    VerbaLinea words = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_words_written(s_key2, &words) && (mmgr_line_finish(&words) != 0ull) &&
                  frame_has("{\"label\":\"alpha\",\"kind\":\"word\",\"insert\":\"alpha\"}") &&
                  frame_has("\"label\":\"gamma\"") && !frame_has("\"label\":\"alp\""),
              "the file's words are items, the word at the cursor left out");
    const CliViewInput completed = {CLI_VIEW_COMPLETE, NULL, "alphabet", 8ull, 0u, 0ull, 3ull, 0u, 0, 0ull, 0u, 0u, 0};
    cli_check(cli_view_input(s_key2, &completed, &why), "a completion accepted is written");
    VerbaLinea frame2 = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_frame_written(s_key2, &frame2) && (mmgr_line_finish(&frame2) != 0ull) &&
                  frame_has("\"alphabet gamma\"") && frame_has("\"cursor\":{\"line\":1,\"col\":8}"),
              "the completion's text replaces the prefix, the cursor after it");
    cli_check(cli_view_changed_taken(s_key2) && !cli_view_changed_taken(s_key2), "the change is reported once");
    const CliViewInput placed = {CLI_VIEW_PLACE, NULL, NULL, 0ull, 0u, 0ull, 7ull, 0u, 0, 0ull, 0u, 0u, 0};
    CliPlace placed_cursor;
    CliPlace placed_anchor;
    cli_check(cli_view_input(s_key2, &placed, &why) && cli_view_cursor(s_key2, &placed_cursor, &placed_anchor) &&
                  (placed_cursor.line == 0ull) && (placed_cursor.col == 7ull) && (placed_anchor.col == 7ull),
              "a place input puts the cursor at the line and unit");
    const CliViewInput placed_far = {CLI_VIEW_PLACE, NULL, NULL, 0ull, 0u, 9ull, 90ull, 0u, 0, 0ull, 0u, 0u, 0};
    cli_check(cli_view_input(s_key2, &placed_far, &why) && cli_view_cursor(s_key2, &placed_cursor, &placed_anchor) &&
                  (placed_cursor.line == 2ull) && (placed_cursor.col == 0ull),
              "a place past the text is cut to its last line and that line's end");
    char full[CLI_PATH_MAX_LEN];
    cli_check(!cli_view_served(s_key2, full) && cli_view_served_set(s_key2, "/tree/check/words.txt") &&
                  cli_view_served(s_key2, full) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = full, .other = "/tree/check/words.txt", .cap = sizeof(full)}),
              "a view served records the server's path");
    cli_check(cli_view_path(s_key2, full) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = full, .other = "check/words.txt", .cap = sizeof(full)}),
              "the view's path is the file's");
    cli_view_closed(s_key2);
    cli_document_release(s_key2);
    VerbaLinea same = {s_out, sizeof(s_out), 0ull};
    cli_check(given(&size) && cli_view_frame_written(s_key, &same) && (mmgr_line_finish(&same) != 0ull) &&
                  frame_has("\"top\":2,"),
              "a size the same as the view's leaves the top where it stands");
    CliViewInput taller = view_key("");
    taller.kind = CLI_VIEW_SIZE;
    taller.rows = 4u;
    taller.columns = 80u;
    VerbaLinea resized = {s_out, sizeof(s_out), 0ull};
    cli_check(given(&taller) && cli_view_frame_written(s_key, &resized) && (mmgr_line_finish(&resized) != 0ull) &&
                  frame_has("\"top\":0,"),
              "a size changed moves the top so the cursor stands in view");
    cli_check(given(&size) && given(&scroll), "the view is sized three rows again and scrolled to the third line");
    VerbaLinea again = {s_out, sizeof(s_out), 0ull};
    cli_check(key("Down") && cli_view_frame_written(s_key, &again) && (mmgr_line_finish(&again) != 0ull) &&
                  frame_has("\"top\":1,"),
              "the view follows the cursor as it moves out of view");
    VerbaLinea paired = {s_out, sizeof(s_out), 0ull};
    cli_check(key("Mod+Home") && key("End") && cli_view_frame_written(s_key, &paired) &&
                  (mmgr_line_finish(&paired) != 0ull) &&
                  frame_has("\"pair\":[{\"line\":0,\"col\":13},{\"line\":0,\"col\":8}]"),
              "the frame carries the bracket pair at the cursor, the bracket that ends there first");
    paired = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(key("Home") && cli_view_frame_written(s_key, &paired) && (mmgr_line_finish(&paired) != 0ull) &&
                  frame_has("\"pair\":null"),
              "the frame carries no pair where no bracket stands at the cursor");

    // folds: the braces' region, opened on the second line, folded hides the lines to its closer, which shows after
    // it; the cursor and the rows step over the lines hidden; an edit above moves the fold; the gutter pressed toggles
    // it
    cli_document_walked();
    cli_check(key("Down") && key("Mod+Shift+[") && at(1ull, 1ull, 1ull, 1ull),
              "Mod+Shift+[ folds the region opened on the cursor's line, the cursor at the line's end");
    VerbaLinea folded = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_frame_written(s_key, &folded) && (mmgr_line_finish(&folded) != 0ull) &&
                  frame_has("\"rows\":[\"int main(void)\",\"{\",\"\"]") && frame_has("\"numbers\":[0,1,4]") &&
                  frame_has("\"folds\":[[1,3,1,1,\"}\"]]"),
              "the frame's rows step over the lines the fold hides, the fold carried closed with its closer's text");
    cli_check(key("Down") && at(4ull, 0ull, 4ull, 0ull), "Down steps over the lines hidden");
    cli_check(key("Up") && at(1ull, 1ull, 1ull, 1ull), "Up steps back over them");
    cli_check(key("Right") && at(4ull, 0ull, 4ull, 0ull),
              "Right at the folded line's end goes to the line after the fold");
    cli_check(key("Left") && at(1ull, 1ull, 1ull, 1ull), "Left from there goes back to the folded line's end");
    cli_check(key("Mod+Home") &&
                  given(&(CliViewInput){CLI_VIEW_TEXT, NULL, "x\n", 2ull, 0u, 0ull, 0ull, 0u, 0, 0ull, 0u, 0u, 0}) &&
                  at(1ull, 0ull, 1ull, 0ull),
              "a line put above the fold");
    cli_document_walked();
    folded = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_frame_written(s_key, &folded) && (mmgr_line_finish(&folded) != 0ull) &&
                  frame_has("\"numbers\":[0,1,2]") && frame_has("\"folds\":[[2,4,1,1,\"}\"]]"),
              "the fold moves down with the line put above it and stays closed");
    cli_check(key("Mod+Z") && at(0ull, 0ull, 0ull, 0ull), "undo takes the line out");
    cli_document_walked();
    const CliViewInput gutter = {
        CLI_VIEW_POINTER, NULL, NULL, 0ull, CLI_VIEW_POINTER_DOWN, 1ull, 0ull, 1u, 0, 0ull, 0u, 0u, 1};
    folded = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(given(&gutter) && at(0ull, 0ull, 0ull, 0ull) && cli_view_frame_written(s_key, &folded) &&
                  (mmgr_line_finish(&folded) != 0ull) && frame_has("\"numbers\":[0,1,2]") &&
                  frame_has("\"folds\":[[1,3,1,0,\"}\"]]"),
              "the gutter pressed at the fold's row opens it, the cursor where it was");
    folded = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(given(&gutter) && cli_view_frame_written(s_key, &folded) && (mmgr_line_finish(&folded) != 0ull) &&
                  frame_has("\"numbers\":[0,1,4]"),
              "the gutter pressed again closes it");
    folded = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(key("Down") && key("Mod+Shift+]") && cli_view_frame_written(s_key, &folded) &&
                  (mmgr_line_finish(&folded) != 0ull) && frame_has("\"numbers\":[0,1,2]"),
              "Mod+Shift+] opens the fold at the cursor's line");

    // the language's pairs and quotes, as its plugin names them, and its indentation after an opener
    cli_check(key("Mod+End") && typed("(") && reads(4ull, 5ull, "()") && at(4ull, 1ull, 4ull, 1ull),
              "an opener typed brings its closer, the cursor between them");
    cli_check(typed(")") && reads(4ull, 5ull, "()") && at(4ull, 2ull, 4ull, 2ull),
              "a closer typed before itself steps over it");
    cli_check(typed("\"") && reads(4ull, 5ull, "()\"\"") && at(4ull, 3ull, 4ull, 3ull),
              "a quote typed brings its mate");
    cli_check(typed("\"") && reads(4ull, 5ull, "()\"\"") && at(4ull, 4ull, 4ull, 4ull),
              "a quote typed before its mate steps over it");
    cli_check(key("Left") && key("Backspace") && reads(4ull, 5ull, "()") && at(4ull, 2ull, 4ull, 2ull),
              "Backspace between a pair takes both out");
    cli_check(key("Left") && key("Backspace") && reads(4ull, 5ull, "") && at(4ull, 0ull, 4ull, 0ull),
              "Backspace between a bracket pair takes both out");
    cli_check(key("Shift+Home") && key("Mod+Home") && key("End") && key("Shift+Left") && key("Shift+Left") &&
                  key("Shift+Left") && key("Shift+Left") && key("Shift+Left") && key("Shift+Left") && typed("[") &&
                  reads(0ull, 1ull, "int main[(void)]") && at(0ull, 15ull, 0ull, 9ull),
              "an opener typed around a selection wraps it, the selection kept inside");
    cli_check(key("Mod+Z") && reads(0ull, 1ull, "int main(void)"), "undo takes the wrap out");
    cli_check(key("Mod+End") && typed("{") && reads(4ull, 5ull, "{}") && key("Enter") &&
                  reads(4ull, 7ull, "{\n    \n}") && at(5ull, 4ull, 5ull, 4ull),
              "Enter between a pair puts the closer on a line of its own, the cursor a step in between");
    cli_check(typed("}") && reads(5ull, 6ull, "}") && at(5ull, 1ull, 5ull, 1ull),
              "a closing bracket typed on a blank lead takes the lead back a step");
    cli_check(key("Mod+End") && key("Enter") && typed("f") && typed("(") && reads(7ull, 8ull, "f()") && key("Delete") &&
                  reads(7ull, 8ull, "f(") && key("Enter") && reads(8ull, 9ull, "    ") && at(8ull, 4ull, 8ull, 4ull),
              "Enter after an opener the line leaves open indents the new line a step");
    cli_check(typed("x") && key("Enter") && reads(9ull, 10ull, "    ") && at(9ull, 4ull, 9ull, 4ull),
              "Enter after a plain line keeps its indentation");

    // find: the bar opened by Mod+F takes the keys as its query, the frame carries the query, its options and the
    // matches among the rows, Enter selects the next match, Escape closes the bar
    VerbaLinea found = {s_out, sizeof(s_out), 0ull};
    cli_check(key("Mod+F") && cli_view_frame_written(s_key, &found) && (mmgr_line_finish(&found) != 0ull) &&
                  frame_has("\"find\":{\"open\":1,\"focus\":1,\"query\":\"\",\"at\":0,\"regex\":0,\"case\":0,\"word\":"
                            "0,\"count\":0,\"current\":null}"),
              "Mod+F opens find, its query empty and the keys its own");
    found = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(typed("r") && typed("e") && typed("t") && typed("u") && typed("r") && typed("n") &&
                  reads(9ull, 10ull, "    ") && cli_view_frame_written(s_key, &found) &&
                  (mmgr_line_finish(&found) != 0ull) && frame_has("\"query\":\"return\",\"at\":6,") &&
                  frame_has("\"count\":1,\"current\":0}"),
              "letters typed go to the query, not the text, and the text is searched");
    found = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(key("Enter") && at(2ull, 10ull, 2ull, 4ull) && cli_view_frame_written(s_key, &found) &&
                  (mmgr_line_finish(&found) != 0ull) && frame_has("\"matches\":[[2,4,10]]"),
              "Enter selects the next match and the frame carries the matches among its rows");
    found = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(key("Backspace") && key("Alt+W") && cli_view_frame_written(s_key, &found) &&
                  (mmgr_line_finish(&found) != 0ull) && frame_has("\"query\":\"retur\",\"at\":5,") &&
                  frame_has("\"word\":1,\"count\":0,\"current\":null}"),
              "Backspace edits the query and Alt+W seeks whole words");
    found = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(key("Escape") && cli_view_frame_written(s_key, &found) && (mmgr_line_finish(&found) != 0ull) &&
                  frame_has("\"find\":null,\"matches\":[]"),
              "Escape closes find");
    cli_check(key("Mod+Home") && key("Mod+Shift+Right") && at(0ull, 3ull, 0ull, 0ull) && key("Mod+F") && key("Alt+W") &&
                  key("Enter") && at(0ull, 3ull, 0ull, 0ull) && key("Escape") && key("End") && key("F3") &&
                  at(0ull, 3ull, 0ull, 0ull),
              "Mod+F takes a selection on one line as the query, and F3 seeks it again with find closed");

    cli_view_closed(s_key);
    cli_check(!cli_view_cursor(s_key, NULL, NULL), "the view closed is gone");
    cli_document_release(s_key);

    // a paged text: a file of three hundred numbered lines opened in the pages and a branch over it
    char folder[CLI_PATH_MAX_LEN];
    cli_check_folder("view", folder);
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea named = {path, sizeof(path), 0ull};
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/numbered.txt");
    (void)mmgr_line_finish(&named);
    static char s_numbered[300 * 16];
    VerbaLinea numbered = {s_numbered, sizeof(s_numbered), 0ull};
    for (unsigned long long line = 0ull; line < 300ull; line += 1ull)
    {
        mmgr_line_put(&numbered, "line ");
        mmgr_line_decimal(&numbered, line, 1u);
        mmgr_line_put(&numbered, " here\n");
    }
    cli_check(query_file_written(path, s_numbered, numbered.at), "the numbered file is written");
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    VerbaLinea opened = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_pages_open(s_paged, path, &opened, &err_msg), "the file opens in the pages");
    cli_check(cli_document_paged(s_paged, 0ull, 40ull), "a branch stands over the pages");
    unsigned long long lines = 0ull;
    int whole = 0;
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((!cli_pages_counted(s_paged, &lines, &whole) || !whole) && (cli_clock_milliseconds() < until))
    {
        query_paused(1u);
    }
    cli_check(whole && (lines == 301ull), "the pages have read every line");
    cli_check(cli_view_opened(s_paged, path), "a view opens on the paged text");
    size.rows = 10u;
    cli_check(cli_view_input(s_paged, &size, &err_msg), "the paged view is sized ten rows");
    cli_check(paged_key("Mod+End") && paged_at(300ull, 0ull), "Mod+End moves to the paged text's last line");
    cli_check(paged_key("Up") && paged_key("End") && paged_at(299ull, 13ull),
              "End finds the line's end through the pages");
    cli_check(paged_key("Mod+Left") && paged_at(299ull, 9ull), "Mod+Left moves a word through the pages");
    VerbaLinea frame = {s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_frame_written(s_paged, &frame) && (mmgr_line_finish(&frame) != 0ull) &&
                  frame_has("\"from\":291,\"rows\":[\"line 291 here\""),
              "the paged frame's rows come through the pages from the view's top");
    cli_check(frame_has("\"cursor\":{\"line\":299,\"col\":9}"), "the paged frame carries the cursor");
    const CliViewInput typed = view_text("there ");
    cli_check(cli_view_input(s_paged, &typed, &err_msg) && paged_at(299ull, 15ull), "text typed into the paged text");
    frame = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_frame_written(s_paged, &frame) && (mmgr_line_finish(&frame) != 0ull) &&
                  frame_has("\"line 299 there here\""),
              "the paged frame shows the line written");
    cli_check(paged_key("Mod+Z") && paged_at(299ull, 9ull), "undo takes the typing out of the paged text");
    frame = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(cli_view_frame_written(s_paged, &frame) && (mmgr_line_finish(&frame) != 0ull) &&
                  frame_has("\"line 299 here\""),
              "the paged frame shows the line as it was");
    static const char *const s_letters[3] = {"a", "b", "c"};
    int lettered = 1;
    for (unsigned int each = 0u; each < 3u; each += 1u)
    {
        const CliViewInput letter = view_text(s_letters[each]);
        lettered = lettered && cli_view_input(s_paged, &letter, &err_msg);
    }
    frame = (VerbaLinea){s_out, sizeof(s_out), 0ull};
    cli_check(lettered && paged_at(299ull, 12ull) && cli_view_frame_written(s_paged, &frame) &&
                  (mmgr_line_finish(&frame) != 0ull) && frame_has("\"line 299 abchere\""),
              "letters typed one at a time into the paged text each land after the last");
    cli_check(paged_key("Mod+Z") && paged_at(299ull, 9ull), "undo takes the three letters out as one step");
    cli_check(paged_key("Enter") && paged_at(300ull, 0ull) && cli_pages_counted(s_paged, &lines, &whole) &&
                  (lines == 302ull),
              "Enter breaks a paged line");
    cli_check(paged_key("Backspace") && paged_at(299ull, 9ull) && cli_pages_counted(s_paged, &lines, &whole) &&
                  (lines == 301ull),
              "Backspace joins it again");
    cli_view_closed(s_paged);
    cli_document_release(s_paged);
    cli_pages_release(s_paged);
    return cli_checked();
}
