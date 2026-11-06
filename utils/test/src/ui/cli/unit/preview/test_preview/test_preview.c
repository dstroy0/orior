// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_preview.c: the preview's tables against fixed texts: a path's ending answers its kind; the mode reads its word,
// turns off where it stands, and is saved and read back; a Markdown text lays its headings, paragraphs, lists, tasks,
// quotes, code, tables, rules, links, emphasis and strike as items sorted by top, its cells the size of what they
// carry; an address that runs script is no link; and over a view bound: the first item a top starts at, the line at a
// top and the top of a line answering each other, the wheel leading the view and the follow leading nothing back, the
// editor's scroll moving the top, the cursor's line marking its block, a press following a link, and a step laying the
// text again only where it changed
#include "ui/cli/src/editor/document.h"
#include "ui/cli/src/editor/view.h"
#include "ui/cli/src/preview/preview.h"
#include "ui/cli/src/preview/preview_markdown.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the key and the path of the view the preview is bound to, the rectangle it is laid in, and the clock the steps read
#define PREVIEW_KEY "window:notes/readme.md"
#define PREVIEW_PATH "notes/readme.md"
#define PREVIEW_WIDTH 600
#define PREVIEW_HEIGHT 400

static unsigned long long s_now_ms = 1000ULL;
static int s_opened;

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

// the item of `items` that is a run of text reading `text` whole: its row, or NULL where none is
static const CliPreviewItem *run_reading(const CliPreviewItems *items, const char *text)
{
    const unsigned long long length = CLI_UNITY_LENGTH(text);
    for (unsigned int at = 0u; at < items->items; at += 1u)
    {
        const CliPreviewItem *const item = &items->item[at];
        if ((item->kind == CLI_PREVIEW_ITEM_TEXT) && (item->length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = items->text + item->text, .other = text, .bytes = length}) == 0))
        {
            return item;
        }
    }
    return NULL;
}

// how many items of `items` are of the kind `kind` with the role `role` and the stroke `stroke`
static unsigned int items_of(const CliPreviewItems *items, unsigned int kind, unsigned int role, unsigned int stroke)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < items->items; at += 1u)
    {
        count += ((items->item[at].kind == kind) && (items->item[at].role == role) &&
                  (items->item[at].stroke == stroke))
                     ? 1u
                     : 0u;
    }
    return count;
}

// `text` laid out into `items`, PREVIEW_WIDTH wide
static void laid(const char *text, CliPreviewItems *items)
{
    *items = (CliPreviewItems){0};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_preview_markdown.layout, CliPreviewMarkdownCfg, .text = text,
                                        .length = CLI_UNITY_LENGTH(text), .width_px = PREVIEW_WIDTH, .items = items));
}

// the cells of `items` released
static void released(CliPreviewItems *items)
{
    g_cli_guard->persistent_buf_release(items->item, g_cli_preview_owner);
    g_cli_guard->persistent_buf_release(items->text, g_cli_preview_owner);
    *items = (CliPreviewItems){0};
}

// the view the preview is bound to showing `text`, the mode beside, and a step a look later laying it
static int bound(const char *text)
{
    CliShape shape;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .text = text,
                                        .size = CLI_UNITY_LENGTH(text), .key = PREVIEW_KEY, .shape = &shape));
    if (!s_opened)
    {
        TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_view_table.open, CliViewCfg, .key = PREVIEW_KEY, .path = PREVIEW_PATH));
        s_opened = 1;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const CliViewInput sized = {.kind = CLI_VIEW_SIZE, .rows = 10u, .columns = 80u};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = PREVIEW_KEY, .input = &sized,
                     .err_msg = &err_msg);
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "beside");
    s_now_ms += CLI_PREVIEW_LOOK_MS;
    return EMBED_CALL(cli_preview.step, CliPreviewCfg, .key = PREVIEW_KEY, .now_ms = s_now_ms,
                      .width_px = PREVIEW_WIDTH, .height_px = PREVIEW_HEIGHT, .scale = 100);
}

// the view's input `input` taken
static void view_input(const CliViewInput *input)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = PREVIEW_KEY,
                                        .input = input, .err_msg = &err_msg));
}

// a step a look later
static int stepped(void)
{
    s_now_ms += CLI_PREVIEW_LOOK_MS;
    return EMBED_CALL(cli_preview.step, CliPreviewCfg, .key = PREVIEW_KEY, .now_ms = s_now_ms,
                      .width_px = PREVIEW_WIDTH, .height_px = PREVIEW_HEIGHT, .scale = 100);
}

// the bound items' run reading `text`: its row, or past the last
static unsigned int bound_run(const char *text)
{
    const unsigned long long length = CLI_UNITY_LENGTH(text);
    const unsigned int count = EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *read = NULL;
        const CliPreviewItem *const item =
            EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = at, .text_out = &read);
        if ((item->kind == CLI_PREVIEW_ITEM_TEXT) && (item->length == length) &&
            (memor.cmp(&(MemoriaCfg){.src = read, .other = text, .bytes = length}) == 0))
        {
            return at;
        }
    }
    return count;
}

// the bound block item of the source line `line`: its row, or past the last
static unsigned int bound_block(unsigned int line)
{
    const unsigned int count = EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliPreviewItem *const item = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = at);
        if ((item->kind == CLI_PREVIEW_ITEM_BLOCK) && (item->line == line))
        {
            return at;
        }
    }
    return count;
}

// sixty paragraphs, one on every second line
static const char *paragraphs(void)
{
    static char s_text[2048];
    VerbaLinea text = {s_text, sizeof(s_text), 0ULL};
    for (unsigned int at = 0u; at < 60u; at += 1u)
    {
        mmgr_line_put(&text, "Line ");
        mmgr_line_decimal(&text, at, 1u);
        mmgr_line_put(&text, "\n\n");
    }
    (void)mmgr_line_finish(&text);
    return s_text;
}

void test_kind_answers_each_ending_and_none_for_others(void)
{
    static const char *const s_paths[] = {"a.md",  "b/c.MARKDOWN", "d.mdown", "e.mkd",   "f.mdx",  "g.html",
                                          "h.htm", "i.xhtml",      "j.css",   "k.svg",   "l.mmd",  "m.mermaid",
                                          "n.tex", "o.latex",      "p.ltx",   "q.http",  "r.rest"};
    static const unsigned int s_kinds[] = {
        CLI_PREVIEW_MARKDOWN, CLI_PREVIEW_MARKDOWN, CLI_PREVIEW_MARKDOWN, CLI_PREVIEW_MARKDOWN, CLI_PREVIEW_MARKDOWN,
        CLI_PREVIEW_HTML,     CLI_PREVIEW_HTML,     CLI_PREVIEW_HTML,     CLI_PREVIEW_CSS,      CLI_PREVIEW_SVG,
        CLI_PREVIEW_MERMAID,  CLI_PREVIEW_MERMAID,  CLI_PREVIEW_LATEX,    CLI_PREVIEW_LATEX,    CLI_PREVIEW_LATEX,
        CLI_PREVIEW_REQUESTS, CLI_PREVIEW_REQUESTS};
    for (unsigned int at = 0u; at < sizeof(s_paths) / sizeof(s_paths[0]); at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(s_kinds[at], EMBED_CALL(cli_preview.kind, CliPreviewCfg, .path = s_paths[at]));
    }
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_NONE, EMBED_CALL(cli_preview.kind, CliPreviewCfg, .path = "main.c"));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_NONE, EMBED_CALL(cli_preview.kind, CliPreviewCfg, .path = "md"));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_NONE, EMBED_CALL(cli_preview.kind, CliPreviewCfg, .path = "notes.md/readme"));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_NONE, EMBED_CALL(cli_preview.kind, CliPreviewCfg, .path = NULL));
}

void test_mode_reads_its_word_turns_off_where_it_stands_and_is_saved(void)
{
    char home[CLI_PATH_MAX_LEN];
    cli_unity_folder("preview_home", home);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME",
                                        .value = home));
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "alone");
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_ALONE, EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = 0ULL));
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .mode = CLI_PREVIEW_BESIDE);
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_BESIDE, EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = 0ULL));
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .mode = CLI_PREVIEW_BESIDE);
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_OFF, EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = 0ULL));
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .mode = CLI_PREVIEW_BESIDE, .word = "alone");
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_ALONE, EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = 0ULL));
    CliStateEntries *const entries =
        cli_state_entries_alloc(&(CliStateEntriesCfg){.folder = home, .owner = g_cli_preview_owner});
    TEST_ASSERT_NOT_NULL(entries);
    CLI_TEST_ASSERT_TEXT("alone", cli_state_entry_value(entries, CLI_PREVIEW_MODE_KEY), 5ULL);
    EMBED_CALL(cli_preview.begin, CliPreviewCfg, .entries = NULL);
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_OFF, EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = 0ULL));
    EMBED_CALL(cli_preview.begin, CliPreviewCfg, .entries = entries);
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_ALONE, EMBED_CALL(cli_preview.mode_get, CliPreviewCfg, .now_ms = 0ULL));
    g_cli_guard->persistent_buf_release(entries, g_cli_preview_owner);
}

void test_markdown_lays_every_block_as_items_sorted_by_top(void)
{
    static const char s_text[] = "# Title\n"
                                 "\n"
                                 "A *word* and **strong** and ~~gone~~ with [a link](other.md) here.\n"
                                 "\n"
                                 "- one\n"
                                 "- two\n"
                                 "\n"
                                 "1. first\n"
                                 "2. second\n"
                                 "\n"
                                 "- [x] done\n"
                                 "- [ ] open\n"
                                 "\n"
                                 "> quoted\n"
                                 "\n"
                                 "```c\n"
                                 "fenced\n"
                                 "```\n"
                                 "\n"
                                 "    indented\n"
                                 "\n"
                                 "| a | b |\n"
                                 "|---|---|\n"
                                 "| 1 | 2 |\n"
                                 "\n"
                                 "---\n";
    CliPreviewItems items;
    laid(s_text, &items);
    const CliPreviewItem *const title = run_reading(&items, "Title");
    TEST_ASSERT_NOT_NULL(title);
    TEST_ASSERT_EQUAL_UINT(26u, title->size);
    TEST_ASSERT_TRUE((title->flags & CLI_PREVIEW_BOLD) != 0u);
    TEST_ASSERT_EQUAL_INT(1, items.block[0].heading);
    TEST_ASSERT_EQUAL_UINT(0u, items.block[0].line);
    TEST_ASSERT_TRUE((run_reading(&items, "word")->flags & CLI_PREVIEW_ITALIC) != 0u);
    TEST_ASSERT_TRUE((run_reading(&items, "strong")->flags & CLI_PREVIEW_BOLD) != 0u);
    TEST_ASSERT_TRUE((run_reading(&items, "gone")->flags & CLI_PREVIEW_STRIKE) != 0u);
    const CliPreviewItem *const link = run_reading(&items, "a link");
    TEST_ASSERT_NOT_NULL(link);
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_ROLE_LINK, link->role);
    TEST_ASSERT_TRUE((link->flags & CLI_PREVIEW_UNDERLINE) != 0u);
    CLI_TEST_ASSERT_TEXT("other.md", items.text + link->link, link->link_length);
    TEST_ASSERT_EQUAL_UINT(2u, run_reading(&items, "word")->line);
    TEST_ASSERT_EQUAL_UINT(0u, run_reading(&items, "here.")->flags);
    // the bullets two discs, the numbers runs, the tasks a box each, the checked one lit with its tick
    TEST_ASSERT_EQUAL_UINT(2u, items_of(&items, CLI_PREVIEW_ITEM_BOX, CLI_PREVIEW_ROLE_TEXT, CLI_PREVIEW_ROLE_NONE));
    TEST_ASSERT_NOT_NULL(run_reading(&items, "1."));
    TEST_ASSERT_NOT_NULL(run_reading(&items, "2."));
    TEST_ASSERT_EQUAL_UINT(1u, items_of(&items, CLI_PREVIEW_ITEM_BOX, CLI_PREVIEW_ROLE_SIGNAL, CLI_PREVIEW_ROLE_LINE));
    TEST_ASSERT_EQUAL_UINT(2u, items_of(&items, CLI_PREVIEW_ITEM_SEGMENT, CLI_PREVIEW_ROLE_TEXT, CLI_PREVIEW_ROLE_NONE));
    TEST_ASSERT_NOT_NULL(run_reading(&items, "done"));
    TEST_ASSERT_NOT_NULL(run_reading(&items, "open"));
    // the quote's bar and its text quieter
    TEST_ASSERT_EQUAL_UINT(1u, items_of(&items, CLI_PREVIEW_ITEM_BOX, CLI_PREVIEW_ROLE_LINE, CLI_PREVIEW_ROLE_NONE));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_ROLE_QUIET, run_reading(&items, "quoted")->role);
    // the code, fenced and indented, in the code face on its ground
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_FACE_CODE, run_reading(&items, "fenced")->face);
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_FACE_CODE, run_reading(&items, "indented")->face);
    TEST_ASSERT_EQUAL_UINT(16u, run_reading(&items, "fenced")->line);
    TEST_ASSERT_TRUE(items_of(&items, CLI_PREVIEW_ITEM_BOX, CLI_PREVIEW_ROLE_CODE_GROUND, CLI_PREVIEW_ROLE_NONE) >= 3u);
    // the table: its head bold, its cells on borders
    TEST_ASSERT_TRUE((run_reading(&items, "a")->flags & CLI_PREVIEW_BOLD) != 0u);
    TEST_ASSERT_EQUAL_UINT(0u, run_reading(&items, "1")->flags & CLI_PREVIEW_BOLD);
    // the table's four cells' borders and the open task's box
    TEST_ASSERT_EQUAL_UINT(5u, items_of(&items, CLI_PREVIEW_ITEM_BOX, CLI_PREVIEW_ROLE_NONE, CLI_PREVIEW_ROLE_LINE));
    // the rules: two under the title, one at the end
    TEST_ASSERT_EQUAL_UINT(2u, items_of(&items, CLI_PREVIEW_ITEM_RULE, CLI_PREVIEW_ROLE_LINE, CLI_PREVIEW_ROLE_NONE));
    // every block in the source's order, by line and by top, its item first, and every item at or under its block
    int top = 0;
    for (unsigned int at = 0u; at < items.blocks; at += 1u)
    {
        TEST_ASSERT_TRUE(items.block[at].top >= top);
        TEST_ASSERT_TRUE((at == 0u) || (items.block[at].line >= items.block[at - 1u].line));
        TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_ITEM_BLOCK, items.item[items.block[at].item].kind);
        top = items.block[at].top;
    }
    int block_top = 0;
    for (unsigned int at = 0u; at < items.items; at += 1u)
    {
        block_top = (items.item[at].kind == CLI_PREVIEW_ITEM_BLOCK) ? items.item[at].y : block_top;
        TEST_ASSERT_TRUE(items.item[at].y >= block_top);
    }
    TEST_ASSERT_TRUE(items.height > top);
    released(&items);
}

void test_markdown_cells_are_the_size_of_what_they_carry(void)
{
    static const char s_text[] = "One [two](a.md) three\n\nfour [five](b.md)\n";
    CliPreviewItems items;
    laid(s_text, &items);
    unsigned int bytes = 0u;
    unsigned int link = 0xffffffffu;
    for (unsigned int at = 0u; at < items.items; at += 1u)
    {
        const CliPreviewItem *const item = &items.item[at];
        bytes += (item->kind == CLI_PREVIEW_ITEM_TEXT) ? item->length : 0u;
        if ((item->link_length != 0u) && (item->link != link))
        {
            bytes += item->link_length;
            link = item->link;
        }
    }
    TEST_ASSERT_EQUAL_UINT(items.text_bytes, bytes);
    TEST_ASSERT_EQUAL_UINT(2u, items.blocks);
    TEST_ASSERT_EQUAL_PTR((void *)(items.item + items.items), (void *)items.block);
    CliPreviewItems more = items;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_preview_markdown.layout, CliPreviewMarkdownCfg, .text = "six\n",
                                        .length = 4ULL, .first_line = 4u, .width_px = PREVIEW_WIDTH, .items = &more));
    TEST_ASSERT_EQUAL_UINT(items.items + 2u, more.items);
    TEST_ASSERT_EQUAL_UINT(3u, more.blocks);
    TEST_ASSERT_EQUAL_UINT(4u, more.block[2].line);
    TEST_ASSERT_TRUE(more.block[2].top >= items.height);
    TEST_ASSERT_NOT_NULL(run_reading(&more, "two"));
    TEST_ASSERT_NOT_NULL(run_reading(&more, "six"));
    released(&more);
}

void test_an_address_that_runs_script_is_no_link(void)
{
    static const char s_text[] = "[x](javascript:alert) [y]( JavaScript:alert) [z](vbscript:do) [w](data:text/html,a) "
                                 "[safe](https://orior.test)\n";
    CliPreviewItems items;
    laid(s_text, &items);
    unsigned int links = 0u;
    for (unsigned int at = 0u; at < items.items; at += 1u)
    {
        links += (items.item[at].link_length != 0u) ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT(1u, links);
    // the four labels whose addresses run script stand as one run of plain text
    TEST_ASSERT_NOT_NULL(run_reading(&items, "x y z w"));
    TEST_ASSERT_EQUAL_UINT(0u, run_reading(&items, "x y z w")->link_length);
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_ROLE_LINK, run_reading(&items, "safe")->role);
    released(&items);
}

void test_first_starts_at_the_outermost_block_at_a_top(void)
{
    TEST_ASSERT_EQUAL_INT(1, bound("Intro\n\n> one\n>\n> two\n\nOutro\n"));
    TEST_ASSERT_EQUAL_UINT(0u, EMBED_CALL(cli_preview_item.first, CliPreviewItemCfg, .top_px = 0));
    const unsigned int quote = bound_block(2u);
    const CliPreviewItem *const quoted = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = quote);
    const unsigned int inner = bound_block(4u);
    const CliPreviewItem *const deep = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = inner);
    TEST_ASSERT_TRUE(deep->y > quoted->y);
    TEST_ASSERT_EQUAL_UINT(quote, EMBED_CALL(cli_preview_item.first, CliPreviewItemCfg, .top_px = deep->y + 1));
    const unsigned int outro = bound_block(6u);
    const CliPreviewItem *const last = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = outro);
    TEST_ASSERT_EQUAL_UINT(outro, EMBED_CALL(cli_preview_item.first, CliPreviewItemCfg, .top_px = last->y));
    TEST_ASSERT_EQUAL_INT(EMBED_CALL(cli_preview_item.extent, CliPreviewItemCfg, .at = 0u),
                          last->y + last->height + CLI_PREVIEW_PAD_FOOT);
    TEST_ASSERT_NULL(EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg,
                                .at = EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u)));
}

void test_line_at_and_top_of_answer_each_other_between_two_blocks(void)
{
    (void)bound(paragraphs());
    const int second = EMBED_CALL(cli_preview_scroll.top_of, CliPreviewScrollCfg, .line = 2ULL);
    const int third = EMBED_CALL(cli_preview_scroll.top_of, CliPreviewScrollCfg, .line = 4ULL);
    TEST_ASSERT_TRUE(third > second);
    TEST_ASSERT_EQUAL_UINT64(2ULL, EMBED_CALL(cli_preview_scroll.line_at, CliPreviewScrollCfg, .top_px = second));
    const int between = EMBED_CALL(cli_preview_scroll.top_of, CliPreviewScrollCfg, .line = 3ULL, .line_fraction = 0u);
    TEST_ASSERT_EQUAL_INT(second + ((third - second) / 2), between);
    unsigned int fraction = 9u;
    TEST_ASSERT_EQUAL_UINT64(3ULL, EMBED_CALL(cli_preview_scroll.line_at, CliPreviewScrollCfg, .top_px = between,
                                              .fraction_out = &fraction));
    TEST_ASSERT_TRUE(fraction < 4096u);
    const int quarter =
        EMBED_CALL(cli_preview_scroll.top_of, CliPreviewScrollCfg, .line = 2ULL, .line_fraction = 32768u);
    TEST_ASSERT_EQUAL_UINT64(2ULL, EMBED_CALL(cli_preview_scroll.line_at, CliPreviewScrollCfg, .top_px = quarter,
                                              .fraction_out = &fraction));
    TEST_ASSERT_TRUE((fraction > 28000u) && (fraction < 37000u));
}

void test_the_wheel_leads_the_view_and_the_follow_leads_nothing_back(void)
{
    (void)bound(paragraphs());
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_preview_scroll.by, CliPreviewScrollCfg, .delta_px = -40));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_preview_scroll.by, CliPreviewScrollCfg, .delta_px = 340));
    const int top = EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0);
    TEST_ASSERT_EQUAL_INT(340, top);
    const unsigned long long line = EMBED_CALL(cli_preview_scroll.line_at, CliPreviewScrollCfg, .top_px = top);
    TEST_ASSERT_EQUAL_UINT64(line, cli_view_top(PREVIEW_KEY));
    (void)stepped();
    TEST_ASSERT_EQUAL_INT(top, EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0));
    TEST_ASSERT_EQUAL_UINT64(line, cli_view_top(PREVIEW_KEY));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_preview_scroll.by, CliPreviewScrollCfg, .delta_px = 1000000));
    TEST_ASSERT_EQUAL_INT(EMBED_CALL(cli_preview_item.extent, CliPreviewItemCfg, .at = 0u) - PREVIEW_HEIGHT,
                          EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_preview_scroll.by, CliPreviewScrollCfg, .delta_px = -1000000));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0));
}

void test_a_scroll_by_the_editor_moves_the_top_and_none_goes_back(void)
{
    (void)bound(paragraphs());
    const CliViewInput scrolled = {.kind = CLI_VIEW_SCROLL, .top = 40ULL};
    view_input(&scrolled);
    TEST_ASSERT_EQUAL_INT(1, stepped());
    TEST_ASSERT_EQUAL_INT(EMBED_CALL(cli_preview_scroll.top_of, CliPreviewScrollCfg, .line = 40ULL),
                          EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0));
    TEST_ASSERT_EQUAL_UINT64(40ULL, cli_view_top(PREVIEW_KEY));
    TEST_ASSERT_EQUAL_INT(0, stepped());
    TEST_ASSERT_EQUAL_UINT64(40ULL, cli_view_top(PREVIEW_KEY));
}

void test_the_cursor_line_marks_its_block(void)
{
    (void)bound(paragraphs());
    const CliViewInput placed = {.kind = CLI_VIEW_POSITION, .row = 5ULL, .column = 0ULL};
    view_input(&placed);
    TEST_ASSERT_EQUAL_INT(1, stepped());
    const CliPreviewItem *const fourth = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = bound_block(4u));
    const CliPreviewItem *const fifth = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = bound_block(6u));
    TEST_ASSERT_TRUE((fourth->flags & CLI_PREVIEW_HERE) != 0u);
    TEST_ASSERT_EQUAL_UINT(0u, fifth->flags & CLI_PREVIEW_HERE);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_preview_mark.mark, CliPreviewMarkCfg, .line = 4ULL));
    const CliViewInput moved = {.kind = CLI_VIEW_POSITION, .row = 6ULL, .column = 0ULL};
    view_input(&moved);
    TEST_ASSERT_EQUAL_INT(1, stepped());
    TEST_ASSERT_EQUAL_UINT(0u, fourth->flags & CLI_PREVIEW_HERE);
    TEST_ASSERT_TRUE((fifth->flags & CLI_PREVIEW_HERE) != 0u);
}

void test_a_press_follows_its_link_and_elsewhere_nothing(void)
{
    (void)bound("[next](sub/other.md#part) and [up](../top.md) and [here](#heading-two)\n\n## Heading Two\n");
    CliPreviewOpen open;
    const CliPreviewItem *const next =
        EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = bound_run("next"));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_OPEN_FILE, EMBED_CALL(cli_preview_mark.press, CliPreviewMarkCfg,
                                                             .x = next->x + 1, .y = next->y + 1, .open_out = &open));
    CLI_TEST_ASSERT_TEXT("notes/sub/other.md", open.path, CLI_UNITY_LENGTH(open.path));
    TEST_ASSERT_EQUAL_INT(0, open.placed);
    const CliPreviewItem *const up = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = bound_run("up"));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_OPEN_FILE, EMBED_CALL(cli_preview_mark.press, CliPreviewMarkCfg, .x = up->x + 1,
                                                             .y = up->y + 1, .open_out = &open));
    CLI_TEST_ASSERT_TEXT("top.md", open.path, CLI_UNITY_LENGTH(open.path));
    const CliPreviewItem *const here = EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = bound_run("here"));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_OPEN_PLACE, EMBED_CALL(cli_preview_mark.press, CliPreviewMarkCfg,
                                                              .x = here->x + 1, .y = here->y + 1, .open_out = &open));
    TEST_ASSERT_EQUAL_INT(open.top_px, EMBED_CALL(cli_preview_scroll.top, CliPreviewScrollCfg, .delta_px = 0));
    TEST_ASSERT_EQUAL_UINT(CLI_PREVIEW_OPEN_NONE,
                           EMBED_CALL(cli_preview_mark.press, CliPreviewMarkCfg, .x = 1, .y = 1, .open_out = &open));
}

void test_a_step_lays_the_text_again_only_where_it_changed(void)
{
    TEST_ASSERT_EQUAL_INT(1, bound("First\n"));
    TEST_ASSERT_EQUAL_INT(0, stepped());
    TEST_ASSERT_NOT_NULL(EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = bound_run("First")));
    CliShape shape;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .text = "Second\n", .size = 7ULL,
                                        .key = PREVIEW_KEY, .shape = &shape));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_preview.step, CliPreviewCfg, .key = PREVIEW_KEY, .now_ms = s_now_ms + 1ULL,
                                        .width_px = PREVIEW_WIDTH, .height_px = PREVIEW_HEIGHT, .scale = 100));
    TEST_ASSERT_EQUAL_INT(1, stepped());
    TEST_ASSERT_EQUAL_UINT(EMBED_CALL(cli_preview_item.count, CliPreviewItemCfg, .at = 0u), bound_run("First"));
    TEST_ASSERT_NOT_NULL(EMBED_CALL(cli_preview_item.item, CliPreviewItemCfg, .at = bound_run("Second")));
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "off");
    TEST_ASSERT_EQUAL_UINT64(0ULL, EMBED_CALL(cli_preview.deadline_ms, CliPreviewCfg, .now_ms = s_now_ms));
    EMBED_CALL(cli_preview.mode_set, CliPreviewCfg, .word = "beside");
    TEST_ASSERT_EQUAL_UINT64(s_now_ms + CLI_PREVIEW_LOOK_MS,
                             EMBED_CALL(cli_preview.deadline_ms, CliPreviewCfg, .now_ms = s_now_ms));
}
