// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest_test.c: the completion's list on its one cell: the list forced open on the word before the cursor lists the
// file's words that answer it, the one typed left out, the rows read with their labels, the row chosen moved by the
// keys, the prefix typed on narrowing the list, a row accepted written over the prefix, and Escape closing the list
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/plugins/plugins.h"
#include "../../../../../ui/cli/src/servers/servers.h"
#include "../cli/cli_check.h"

// whether the row `row` of the list shown reads `label`
static int row_reads(unsigned int row, const char *label)
{
    CliCompletionRow read = {"", 0ULL, "", "", 0ULL, 0u};
    return cli_servers_panels_completion_row(row, &read) &&
           (read.label_length == cellul.len(&(CatenaFinitaCfg){.src = label, .cap = 64ULL})) &&
           cellul.eq(&(CatenaFinitaCfg){.src = read.label, .other = label, .cap = read.label_length + 1ULL});
}

// the cursor of the view `key` put at `line`, `col`
static void cursor_put(const char *key, unsigned long long line, unsigned long long col)
{
    const CliViewInput put = {.type = CLI_VIEW_POSITION, .row = line, .column = col};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &put, .err_msg = &err_msg);
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    static const char s_key[] = "check:suggest";
    static const char s_text[] = "int alpha; int beta; int alphabet;\nint albatross; int gamma;\nalp";
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    char grammar[CLI_NAME_MAX_LEN];
    cli_check(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "c", .key = grammar,
                         .err_msg = &why),
              "the C grammar is read");
    CliShape shape;
    cli_check(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_key, .text = s_text,
                         .size = sizeof(s_text) - 1ULL, .shape = &shape) &&
                  EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = s_key,
                             .grammar = grammar, .named = 0),
              "the text is stored and colored by the C grammar");
    cli_document_walk();
    cli_check(EMBED_CALL(cli_view_table.open, CliViewCfg, .key = s_key, .path = "src/words.c"),
              "a view is opened on the text");
    cli_view_show(0u);
    const CliViewInput sized = {.type = CLI_VIEW_SIZE, .rows = 10u, .columns = 80u};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &sized, .err_msg = &why);
    cli_servers_panels_begin(".");
    cursor_put(s_key, 2ULL, 3ULL);
    CliCompletionPanel panel;
    cli_check(!cli_servers_panels_completion(&panel), "no list stands before one is asked for");
    cli_check(cli_servers_panels_key("Mod+Space"), "the list is forced open by its key");
    cli_check(cli_servers_panels_completion(&panel) && (panel.count == 2u) && (panel.rows == 2u) &&
                  (panel.line == 2ULL) && (panel.from == 0ULL) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = panel.prefix, .other = "alp", .cap = 4ULL}),
              "the words that start with the prefix answer it, the word typed left out");
    cli_check(row_reads(0u, "alpha") && row_reads(1u, "alphabet"), "the rows read the shorter label first");
    CliCompletionRow read = {"", 0ULL, "", "", 0ULL, 0u};
    cli_check(cli_servers_panels_completion_row(0u, &read) && (read.marked == 3u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = read.type, .other = "word", .cap = 5ULL}),
              "a row marks the prefix's bytes and names its type");
    cli_check(!cli_servers_panels_completion_row(2u, &read), "no row stands past the list");
    cli_check(cli_servers_panels_key("Down") && cli_servers_panels_completion(&panel) && (panel.chosen == 1u),
              "Down moves the row chosen");
    cli_check(cli_servers_panels_key("Up") && cli_servers_panels_completion(&panel) && (panel.chosen == 0u),
              "Up moves it back");
    cli_check(!cli_servers_panels_key("F9"), "a key the list has no use for is not taken");
    // the prefix typed on: the view's text grows by the letters and the list narrows to the words that still answer
    const CliViewInput typed = {.type = CLI_VIEW_TEXT, .text = "hab", .length = 3ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &typed, .err_msg = &why);
    cli_servers_panels_text("hab", 3ULL);
    cli_check(cli_servers_panels_key("Mod+Space") && cli_servers_panels_completion(&panel) && (panel.count == 1u) &&
                  row_reads(0u, "alphabet"),
              "the list narrows to the word the longer prefix answers");
    cli_check(cli_servers_panels_key("Enter"), "Enter accepts the row chosen");
    cli_check(!cli_servers_panels_completion(&panel), "the list closes as a row is accepted");
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = s_key, .size_pointer = &size,
                                  .owner = g_cli_servers_owner);
    cli_check((text != NULL) && (size == sizeof(s_text) - 1ULL + 5ULL) &&
                  (memor.cmp(&(MemoriaCfg){.src = text + size - 8ULL, .other = "alphabet", .bytes = 8ULL}) == 0),
              "the accepted word stands over the prefix in the text");
    g_cli_guard->persistent_buf_release(text, g_cli_servers_owner);
    // the list opened at a word's end lists the longer words that start with it, and Escape closes it
    cursor_put(s_key, 0ULL, 9ULL);
    cli_check(cli_servers_panels_key("Mod+Space") && cli_servers_panels_completion(&panel) && (panel.count == 1u) &&
                  row_reads(0u, "alphabet"),
              "the list opened at a word's end lists the longer word that starts with it");
    cli_check(cli_servers_panels_key("Escape") && !cli_servers_panels_completion(&panel), "Escape closes the list");
    cli_servers_panels_end();
    return cli_on_check();
}
