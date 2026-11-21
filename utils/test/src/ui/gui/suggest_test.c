// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// suggest_test.c: the completion's list on its one cell: the list forced open on the word before the cursor lists the
// names of the file's branch in the tree that answer it, the one typed left out, the rows read with their labels, the
// row chosen moved by the keys, the prefix typed on narrowing the list, a row accepted written over the prefix, and
// Escape closing the list
#include "../../../../../ui/cli/src/code/git.h"
#include "../../../../../ui/cli/src/code/symbols.h"
#include "../../../../../ui/cli/src/editor/document.h"
#include "../../../../../ui/cli/src/editor/view.h"
#include "../../../../../ui/cli/src/plugins/plugins.h"
#include "../../../../../ui/cli/src/servers/servers.h"
#include "../cli/cli_check.h"

// whether the row `row` of the list displayed reads `label`
static int row_reads(unsigned int row, const char *label)
{
    CliCompletionRow read = {"", 0ULL, "", "", 0ULL, 0u};
    return cli_servers_panels_completion_row(&(CliServersPanelsCfg){.row = row, .out = &read}) &&
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

// The text an accepted row leaves: the bytes it takes, and whether the accepted word ends it
typedef struct
{
    unsigned long long size;
    int accepted;
} TextAccepted;

// the text the document gives compared with the bytes it takes and the accepted word at its end
static void text_accepted(const CliEachTextLengthCfg *args)
{
    TextAccepted *const accepted = (TextAccepted *)args->context;
    accepted->accepted =
        (args->length == accepted->size) &&
        (memor.cmp(&(MemoriaCfg){.src = args->text + args->length - 8ULL, .other = "alphabet", .bytes = 8ULL}) == 0);
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    static const char s_key[] = "check:suggest";
    static const char s_text[] = "int alpha(void);\nint beta(void);\nint alphabet(void);\nint albatross(void);\nalp";
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ULL};
    char grammar[CLI_NAME_MAX_LEN];
    cli_check(
        EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "c", .key = grammar, .err_msg = &why),
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
    cli_view_show(&(CliViewAtCfg){.at = 0u});
    const CliViewInput sized = {.type = CLI_VIEW_SIZE, .rows = 10u, .columns = 80u};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &sized, .err_msg = &why);
    // the file on disk in a tree of its own, listed and walked. The tree's branch for the file gives the names the
    // list offers, and the line under edit gives the rest
    char tree[CLI_PATH_MAX_LEN];
    cli_check_folder("build/harness/suggest_tree", tree);
    char src[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = tree, .relative = "src", .joined = src);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src);
    char file[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = tree, .relative = "src/words.c", .joined = file);
    cli_check(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file,
                         .bytes = s_text, .size = sizeof(s_text) - 1ULL),
              "the file is written into its tree");
    // the tree lists the files its repository tracks
    char out_text[1024];
    VerbaLinea out = {out_text, sizeof(out_text), 0ULL};
    const char *const init[2] = {"init", "-q"};
    const char *const add[2] = {"add", "-A"};
    cli_check(EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = tree, .out = &out,
                         .count = 2u, .err_msg = &why, .request = init) &&
                  EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = tree, .out = &out,
                             .count = 2u, .err_msg = &why, .request = add),
              "the tree's repository tracks the file");
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = tree);
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = tree);
    CliWake pause = CLI_WAKE_INIT;
    const unsigned long long last_ms = cli_clock_milliseconds() + 10000ULL;
    while (cli_tree_unwalked() && (cli_clock_milliseconds() < last_ms))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &pause, .milliseconds_u64 = 5ULL);
    }
    cli_check(!cli_tree_unwalked(), "the tree is walked");
    cli_servers_panels_begin(&(CliServersPanelsCfg){.root = tree});
    cursor_put(s_key, 4ULL, 3ULL);
    CliCompletionPanel panel;
    cli_check(!cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}),
              "no list is before one is asked for");
    cli_check(cli_servers_panels_key(&(CliServersPanelsCfg){.name = "Mod+Space"}),
              "the list is forced open by its key");
    cli_check(cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}) && (panel.count == 2u) &&
                  (panel.rows == 2u) && (panel.line == 4ULL) && (panel.from == 0ULL) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = panel.prefix, .other = "alp", .cap = 4ULL}),
              "the words that start with the prefix answer it, the word typed left out");
    cli_check(row_reads(0u, "alpha") && row_reads(1u, "alphabet"), "the rows read the shorter label first");
    CliCompletionRow read = {"", 0ULL, "", "", 0ULL, 0u};
    cli_check(cli_servers_panels_completion_row(&(CliServersPanelsCfg){.row = 0u, .out = &read}) &&
                  (read.marked == 3u) && cellul.eq(&(CatenaFinitaCfg){.src = read.type, .other = "word", .cap = 5ULL}),
              "a row marks the prefix's bytes and names its type");
    cli_check(!cli_servers_panels_completion_row(&(CliServersPanelsCfg){.row = 2u, .out = &read}),
              "no row is past the list");
    cli_check(cli_servers_panels_key(&(CliServersPanelsCfg){.name = "Down"}) &&
                  cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}) && (panel.chosen == 1u),
              "Down moves the row chosen");
    cli_check(cli_servers_panels_key(&(CliServersPanelsCfg){.name = "Up"}) &&
                  cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}) && (panel.chosen == 0u),
              "Up moves it back");
    cli_check(!cli_servers_panels_key(&(CliServersPanelsCfg){.name = "F9"}),
              "a key the list has no use for is not taken");
    // the prefix typed on: the view's text grows by the characters and the list narrows to the words that still answer
    const CliViewInput typed = {.type = CLI_VIEW_TEXT, .text = "hab", .length = 3ULL};
    (void)EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = s_key, .input = &typed, .err_msg = &why);
    cli_servers_panels_text(&(CliServersPanelsCfg){.text = "hab", .length = 3ULL});
    cli_check(cli_servers_panels_key(&(CliServersPanelsCfg){.name = "Mod+Space"}) &&
                  cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}) && (panel.count == 1u) &&
                  row_reads(0u, "alphabet"),
              "the list narrows to the word the longer prefix answers");
    cli_check(cli_servers_panels_key(&(CliServersPanelsCfg){.name = "Enter"}), "Enter accepts the row chosen");
    cli_check(!cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}),
              "the list closes as a row is accepted");
    TextAccepted accepted = {sizeof(s_text) - 1ULL + 5ULL, 0};
    (void)EMBED_CALL(cli_document_key.text_each, CliDocumentKeyCfg, .key = s_key, .each = text_accepted,
                     .context = &accepted);
    cli_check(accepted.accepted, "the accepted word is over the prefix in the text");
    // the list opened at a word's end lists the longer words that start with it, and Escape closes it
    cursor_put(s_key, 0ULL, 9ULL);
    cli_check(cli_servers_panels_key(&(CliServersPanelsCfg){.name = "Mod+Space"}) &&
                  cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}) && (panel.count == 1u) &&
                  row_reads(0u, "alphabet"),
              "the list opened at a word's end lists the longer word that starts with it");
    cli_check(cli_servers_panels_key(&(CliServersPanelsCfg){.name = "Escape"}) &&
                  !cli_servers_panels_completion(&(CliServersPanelsCfg){.panel = &panel}),
              "Escape closes the list");
    cli_servers_panels_end();
    return cli_on_check();
}
