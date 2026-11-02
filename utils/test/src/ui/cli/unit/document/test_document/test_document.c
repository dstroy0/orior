// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_document.c: a text is split as the page splits a file; edits are written from the last to the first; the marks show whether the commit contains the file
// and carry each change's lines; a text is stored until it is released; find returns each match with case and without, a pattern's by the regular expressions the
// colorer reads; a branch is colored from its view out, each line as the line above it leaves it, and again after an edit; its folds, uses and brackets read off its
// grammar's roles; texts colored at once by walkers on threads of their own read as one walked alone; and a paged text is colored the same way through the pages
#include "ui/cli/src/editor/document.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/editor/highlight.h"
#include "ui/cli/src/editor/pages.h"
#include "ui/cli/src/plugins/plugins.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the cells the pool is allocated in for a search in little room, and the most of them
#define DOCUMENT_TAKEN_CELL (8ull << 20u)
#define DOCUMENT_TAKEN_MOST 64u

// the lines of the branch, a comment opened on the first and closed on the one before the last
#define DOCUMENT_BRANCH_LINES 401u

// the lines of the text an edit moves the state of every line after, and the room the text is written in
#define DOCUMENT_ONWARD_LINES 40000ull
#define DOCUMENT_ONWARD_ROOM (4ull << 20u)

// the lines of the texts colored at once, a comment opened on the first and closed on the middle one, and how many texts
#define DOCUMENT_AT_ONCE_LINES 3000u
#define DOCUMENT_AT_ONCE_TEXTS 4u

// a grammar whose comments run over lines: `""` is its name 0, `comment` 1 and `identifier` 2
static const char s_branch_grammar[] = "{\"tokenizer\": {\"root\": [[\"\\\\/\\\\*\", \"comment\", \"@comment\"], [\"[a-z]+\", \"identifier\"], "
                                       "[\"\\\\s+\", \"\"]], \"comment\": [[\"\\\\*\\\\/\", \"comment\", \"@pop\"], [\"[^*]+\", \"comment\"], "
                                       "[\"\\\\*\", \"comment\"]]}}";

// a grammar whose brackets open and close regions and whose `;` ends a statement
static const char s_bracket_grammar[] = "{\"tokenizer\": {\"root\": [[\"\\\\{\", \"delimiter.bracket.region.open\"], "
                                        "[\"\\\\}\", \"delimiter.bracket.region.close\"], [\";\", \"delimiter.statement\"], [\"[a-z]+\", \"identifier\"], "
                                        "[\"\\\\s+\", \"\"]]}}";

// a grammar whose `:` at a line's end opens a region its indent runs, a mark of no letters closing it
static const char s_indent_grammar[] = "{\"tokenizer\": {\"root\": [[\":(?=\\\\s*$)\", \"operator.region.open\", \"@block\"], "
                                       "[\"[a-z]+\", \"identifier\"], [\"\\\\s+\", \"\"]], "
                                       "\"block\": [[\":(?=\\\\s*$)\", \"operator.region.open\", \"@block\"], [\"[a-z]+\", \"identifier\"], "
                                       "[\"\\\\s+\", \"\"]]}, \"indent\": {\"states\": {\"block\": \"operator.region.close\"}, \"tab\": 8, "
                                       "\"skip\": \"^\\\\s*$\", \"begins\": [\"root\", \"block\"]}}";

static const char *const s_at_once_keys[DOCUMENT_AT_ONCE_TEXTS] = {"at_once_0", "at_once_1", "at_once_2", "at_once_3"};

static unsigned short s_units[1u << 12u];
static unsigned long long s_start[256];
static unsigned long long s_length[256];
static CliLines s_lines;
static char s_out[1u << 14u];
static char s_why[256];
static char s_branch[4u * DOCUMENT_BRANCH_LINES];
static char s_runs_of[DOCUMENT_BRANCH_LINES][64];
static unsigned long long s_seq;
static char s_folder[CLI_PATH_MAX_LEN];
static char s_folds[1024];
static int s_begun;

// the colors walkers send from threads of their own counted, under a lock of the suite's own
static unsigned int s_counted;
static char s_at_once[4u * DOCUMENT_AT_ONCE_LINES];
static char s_printed[DOCUMENT_AT_ONCE_TEXTS + 1u][1u << 16u];

// what each step of the live sequences recorded
static int s_branch_ran;
static int s_branch_step[12];
static int s_at_once_ran;
static int s_at_once_during;
static int s_at_once_ended;
static unsigned int s_at_once_counted;
static int s_at_once_after;
static int s_paged_ran;
static int s_paged_step[12];

// `text` split into s_lines as the page splits a file
static const CliLines *lines_of(const char *text)
{
    s_lines = (CliLines){s_units, sizeof(s_units) / sizeof(s_units[0]), 0ull, s_start, s_length, 256u, 0u, 0};
    EMBED_CALL(cli_document_text_size.lines_get, CliDocumentTextSizeCfg, .text = text, .size = CLI_UNITY_LENGTH(text), .lines_out = &s_lines);
    return &s_lines;
}

// what is sought, as the find bar sets it
static CliDocSought sought_of(const char *query, int regex, int match_case, int word)
{
    const CliDocSought sought = {query, regex, match_case, word};
    return sought;
}

// the matches of what is sought in the lines, asserted against `wanted`
static void assert_found(const CliLines *lines, CliDocSought sought, unsigned long long most, const char *wanted)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = lines, .sought = &sought, .most = most, .out = &out, .err_msg = &err_msg));
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// the matches of `query` in the text stored under `key`, asserted against `wanted`
static void assert_stored_found(const char *key, const char *query, const char *wanted)
{
    const CliDocSought sought = sought_of(query, 0, 1, 0);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_document_out_sought.find, CliDocumentOutSoughtCfg, .key = key, .sought = &sought, .most = 100ull, .out = &out, .err_msg = &err_msg);
    CLI_TEST_ASSERT_TEXT(wanted, s_out, out.at);
}

// the text `edits` stored and edited, its shape in `shape`, asserted accepted
static void edits_written(CliShape *shape)
{
    static const CliEdit s_first[2] = {{{0u, 0u}, {0u, 5u}, "1st", 3u}, {{1u, 7u}, {2u, 2u}, "joined\nand ", 11u}};
    static const CliEdit s_second[1] = {{{2u, 0u}, {2u, 0u}, "", 0u}};
    const CliEdit *const changes[2] = {s_first, s_second};
    const unsigned int counts[2] = {2u, 1u};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "edits", .text = "first line\nsecond line\nthird", .size = 28ull, .shape = shape));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "edits", .changes = changes, .edits = counts, .count_u32 = 2u, .shape = shape, .err_msg = &err_msg));
}

// the marks of the text under `key`, written compact into s_out: 1, or 0 where it has none
static int marks_written(const char *key, int sent, const char *head)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    const int marked = EMBED_CALL(cli_document_out_key_head_given.marks, CliDocumentOutKeyHeadGivenCfg, .key = key, .head_free = sent, .head = head, .head_size = (head != NULL) ? CLI_UNITY_LENGTH(head) : 0ull, .out = &out);
    (void)mmgr_line_finish(&out);
    return marked;
}

// the branch's text written into s_branch: its bytes
static unsigned long long branch_text_written(void)
{
    VerbaLinea text = {s_branch, sizeof(s_branch), 0ull};
    mmgr_line_put(&text, "/*\n");
    for (unsigned int line = 1u; line < (DOCUMENT_BRANCH_LINES - 2u); line += 1u)
    {
        mmgr_line_put(&text, "b\n");
    }
    mmgr_line_put(&text, "*/\nc");
    return text.at;
}

// whether a line was sent `runs` last
static int line_sent(unsigned int line, const char *runs)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = s_runs_of[line], .other = runs, .cap = CLI_CELLS_POOL});
}

// every line's recorded runs cleared
static void runs_cleared(void)
{
    memor.set(&(MemoriaCfg){.dst = s_runs_of, .val = 0, .bytes = sizeof(s_runs_of)});
}

// the lines 349 to 351 of the branch `key` read for a call's signature, inside the comment once the walk has reached them: 1 where the lines joined as a JSON string
// are read, and the state the first is entered in the comment's and not root
static int signature_lines_read(const char *key)
{
    char state[CLI_NAME_MAX_LEN];
    VerbaLinea text = {s_out, sizeof(s_out), 0ull};
    const int written = EMBED_CALL(cli_document_out_key.text_write, CliDocumentOutKeyCfg, .key = key, .first = 349ull, .last = 351ull, .out = &text, .state = state, .room = sizeof(state));
    (void)mmgr_line_finish(&text);
    return written && cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "\"b\\nb\\nb\"", .cap = CLI_CELLS_POOL}) && (state[0] != '\0') && !cellul.eq(&(CatenaFinitaCfg){.src = state, .other = "root", .cap = CLI_CELLS_POOL});
}

// the number the JSON node `node` writes, 0 where it writes none
static unsigned long long number_of(const CliJson *json, unsigned int node)
{
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = node, .kind = CFG_JSON_NUMBER) && (at < json->node[node].length); at += 1ull)
    {
        // a digit's value is below 10
        value = (value * 10ull) + (unsigned long long)(json->text[json->node[node].text + at] - '0');
    }
    return value;
}

// a branch's colors, `[{"from", "runs"}]` at `colors` in `json`, each line's runs stored as the last it was sent
static void colors_read(const CliJson *json, unsigned int colors)
{
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = colors, .kind = CFG_JSON_ARRAY) ? json->node[colors].first : CLI_JSON_NONE;
    for (unsigned int range = first; range != CLI_JSON_NONE; range = json->node[range].next)
    {
        unsigned long long line = number_of(json, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = range, .name = "from"));
        const unsigned int runs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = range, .name = "runs");
        const unsigned int run = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = runs, .kind = CFG_JSON_ARRAY) ? json->node[runs].first : CLI_JSON_NONE;
        for (unsigned int one = run; (one != CLI_JSON_NONE) && (line < DOCUMENT_BRANCH_LINES); one = json->node[one].next)
        {
            VerbaLinea out = {s_runs_of[line], sizeof(s_runs_of[line]), 0ull};
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = one, .out = &out, .indent = 0u, .sorted = 0);
            (void)mmgr_line_finish(&out);
            line += 1ull;
        }
    }
}

// colors sent as the walk works them out, `{"key", "seq", "colors"}`, read into the runs each line was sent last, and folds sent, `{"key", "seq", "folds"}`, stored as the
// folds sent last
static void colors_recorded(const CliDocColoredCfg *args)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(args->body, args->size, &root);
    s_seq = number_of(json, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "seq"));
    colors_read(json, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "colors"));
    const unsigned int folds = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "folds");
    if (folds != CLI_JSON_NONE)
    {
        VerbaLinea out = {s_folds, sizeof(s_folds), 0ull};
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = folds, .out = &out, .indent = 0u, .sorted = 0);
        (void)mmgr_line_finish(&out);
    }
}

// a branch viewed far from its first line, walked and edited, what each step answered recorded, once for the process
static void branch_ran(void)
{
    static const CliEdit s_opened[1] = {{{0u, 0u}, {0u, 2u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_opened};
    const unsigned int counts[1] = {1u};
    CliShape shape;
    unsigned int root = CLI_JSON_NONE;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    if (s_branch_ran)
    {
        return;
    }
    s_branch_ran = 1;
    runs_cleared();
    const unsigned long long size = branch_text_written();
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "branch", .text = s_branch, .size = size, .shape = &shape);
    s_branch_step[0] = EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "branch", .grammar = "branch", .named = 1);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    s_branch_step[1] = EMBED_CALL(cli_document_out_key_first.view, CliDocumentOutKeyFirstCfg, .key = "branch", .first = 350ull, .last = 360ull, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &root);
    colors_read(json, root);
    s_branch_step[2] = line_sent(350u, "[0,2]") && (s_runs_of[49][0] == '\0') && (s_runs_of[50][0] != '\0');
    cli_document_walk();
    s_branch_step[3] = line_sent(350u, "[0,1]") && line_sent(0u, "[0,1]") && line_sent(DOCUMENT_BRANCH_LINES - 1u, "[0,2]");
    s_branch_step[4] = signature_lines_read("branch");
    (void)EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "branch", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    cli_document_walk();
    s_branch_step[5] = line_sent(0u, "[0,2]") && line_sent(350u, "[0,2]") && (s_seq == 1ull);
    s_branch_step[6] = EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = "branch", .from = 100ull, .to = 110ull);
    runs_cleared();
    s_branch_step[7] = EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = "branch", .from = 100ull, .to = 120ull) && (s_runs_of[99][0] == '\0') && (s_runs_of[105][0] == '\0') && line_sent(115u, "[0,2]") && (s_runs_of[120][0] == '\0');
    runs_cleared();
    cli_document_walk();
    s_branch_step[8] = s_runs_of[115][0] == '\0';
    VerbaLinea printed = {s_out, sizeof(s_out), 0ull};
    const int written = EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = "branch", .first = 0ull, .last = 2ull, .mirrored = 0, .out = &printed);
    (void)mmgr_line_finish(&printed);
    s_branch_step[9] = written && cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "[{\"from\":0,\"runs\":[[0,2],[0,2],[0,2]]}]", .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "branch");
}

// the grammar `grammar` stored under `key` and the text `text` colored by it whole, the folds sent recorded
static void folds_walked(const char *key, const char *grammar, const char *text)
{
    unsigned int root = CLI_JSON_NONE;
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    const CliJson *const json = cli_unity_parsed(grammar, CLI_UNITY_LENGTH(grammar), &root);
    (void)EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = key, .json = json, .def = root, .err_msg = &err_msg);
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = key, .text = text, .size = CLI_UNITY_LENGTH(text), .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = key, .grammar = key, .named = 1);
    s_folds[0] = '\0';
    cli_document_walk();
}

// `text` colored under `key` by the grammar of the plugin `plugin`, the folds sent recorded
static void plugin_text_walked(const char *key, const char *plugin, const char *text, unsigned long long size)
{
    char grammar[CLI_NAME_MAX_LEN];
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = plugin, .key = grammar, .err_msg = &err_msg));
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = key, .text = text, .size = size, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = key, .grammar = grammar, .named = 1);
    s_folds[0] = '\0';
    cli_document_walk();
}

// the place among the names of the grammar under `key` of the name `name`, CLI_JSON_NONE where it has none
static unsigned int name_place(const char *key, const char *name)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    unsigned int root = CLI_JSON_NONE;
    (void)EMBED_CALL(cli_highlight_key_err_msg.names_write, CliHighlightKeyErrMsgCfg, .key = key, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &root);
    unsigned int place = 0u;
    for (unsigned int one = json->node[root].first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long size = 0ull;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = one, .length_pointer = &size);
        if ((text != NULL) && (size == CLI_UNITY_LENGTH(name)) && (memor.cmp(&(MemoriaCfg){.src = text, .other = name, .bytes = size}) == 0))
        {
            return place;
        }
        place += 1u;
    }
    return CLI_JSON_NONE;
}

// whether the line `line` of the colored text under `key` has a run at `col` named `name` among the names of the grammar under `grammar`
static int run_named(const char *key, const char *grammar, unsigned int line, unsigned int col, const char *name)
{
    const unsigned int place = name_place(grammar, name);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    unsigned int root = CLI_JSON_NONE;
    (void)EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = key, .first = line, .last = line, .mirrored = 0, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &root);
    const unsigned int range = json->node[root].first;
    const unsigned int runs = (range != CLI_JSON_NONE) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = range, .name = "runs") : CLI_JSON_NONE;
    const unsigned int flat = (runs != CLI_JSON_NONE) ? json->node[runs].first : CLI_JSON_NONE;
    unsigned int at = (flat != CLI_JSON_NONE) ? json->node[flat].first : CLI_JSON_NONE;
    while ((at != CLI_JSON_NONE) && (json->node[at].next != CLI_JSON_NONE))
    {
        const unsigned int named = json->node[at].next;
        if ((number_of(json, at) == col) && (number_of(json, named) == place))
        {
            return 1;
        }
        at = json->node[named].next;
    }
    return 0;
}

// whether the run at `col` of the runs `runs`, compact `[column, name, ...]`, has the name `wanted` among the names a branch colored by the grammar under `grammar` has
static int run_has_name(const char *runs, const char *grammar, unsigned int col, const char *wanted)
{
    unsigned int root = CLI_JSON_NONE;
    const CliJson *json = cli_unity_parsed(runs, CLI_UNITY_LENGTH(runs), &root);
    unsigned long long place = CLI_JSON_NONE;
    for (unsigned int at = (root != CLI_JSON_NONE) ? json->node[root].first : CLI_JSON_NONE; (at != CLI_JSON_NONE) && (json->node[at].next != CLI_JSON_NONE); at = json->node[json->node[at].next].next)
    {
        place = (number_of(json, at) == col) ? number_of(json, json->node[at].next) : place;
    }
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_document_out_grammar.names_write, CliDocumentOutGrammarCfg, .grammar = grammar, .out = &out);
    json = cli_unity_parsed(s_out, out.at, &root);
    unsigned long long at_place = 0ull;
    for (unsigned int one = json->node[root].first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long size = 0ull;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = one, .length_pointer = &size);
        if ((at_place == place) && (text != NULL))
        {
            return (size == CLI_UNITY_LENGTH(wanted)) && (memor.cmp(&(MemoriaCfg){.src = text, .other = wanted, .bytes = size}) == 0);
        }
        at_place += 1ull;
    }
    return 0;
}

// whether the run at `col` of the line `line` of the text under `key`, as a print of it reads it, has the name `wanted` among the names a branch colored by the grammar
// under `grammar` has
static int printed_has_name(const char *key, const char *grammar, unsigned int line, unsigned int col, const char *wanted)
{
    char compact[256];
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    unsigned int root = CLI_JSON_NONE;
    (void)EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = key, .first = line, .last = line, .mirrored = 0, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &root);
    const unsigned int range = (root != CLI_JSON_NONE) ? json->node[root].first : CLI_JSON_NONE;
    const unsigned int runs = (range != CLI_JSON_NONE) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = range, .name = "runs") : CLI_JSON_NONE;
    const unsigned int flat = (runs != CLI_JSON_NONE) ? json->node[runs].first : CLI_JSON_NONE;
    VerbaLinea written = {compact, sizeof(compact), 0ull};
    if (flat != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = flat, .out = &written, .indent = 0u, .sorted = 0);
    }
    (void)mmgr_line_finish(&written);
    return run_has_name(compact, grammar, col, wanted);
}

// the JSON text of nested brackets colored, the plugin's grammar key in `key`
static void brackets_walked(char *key)
{
    static const char s_json[] = "[\n  {\n    \"a\": [1]\n  }\n]";
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "json", .key = key, .err_msg = &err_msg));
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "depth", .text = s_json, .size = sizeof(s_json) - 1u, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "depth", .grammar = key, .named = 1);
    cli_document_walk();
}

// the colors walkers send counted
static void colors_counted(const CliDocColoredCfg *args)
{
    (void)args;
    s_counted += 1u;
}

// the text written into s_at_once: its bytes
static unsigned long long at_once_text_written(void)
{
    const unsigned int middle = DOCUMENT_AT_ONCE_LINES / 2u;
    VerbaLinea text = {s_at_once, sizeof(s_at_once), 0ull};
    mmgr_line_put(&text, "/*\n");
    for (unsigned int line = 1u; line < (DOCUMENT_AT_ONCE_LINES - 1u); line += 1u)
    {
        mmgr_line_put(&text, (line == middle) ? "*/\n" : ((line < middle) ? "b\n" : "c\n"));
    }
    mmgr_line_put(&text, "c");
    return text.at;
}

// every line of the text under `key` printed into `out`, `room` bytes: 1 where it was printed whole
static int text_printed(const char *key, char *out, unsigned long long room)
{
    VerbaLinea printed = {out, room, 0ull};
    const int written = EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = key, .first = 0ull, .last = DOCUMENT_AT_ONCE_LINES - 1u, .mirrored = 0, .out = &printed);
    return written && (mmgr_line_finish(&printed) != 0ull);
}

// the walkers waited for until none walks, ten seconds at the most: 1 where none walks
static int walkers_ended(void)
{
    static CliWake s_wake = CLI_WAKE_INIT;
    const unsigned long long until = cli_clock_milliseconds() + 10000ull;
    while ((cli_document_walkers() != 0u) && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_wake, .milliseconds_u64 = 5ull);
    }
    return cli_document_walkers() == 0u;
}

// whether each text's print is the same as the one walked alone, `released` freeing each after
static int prints_match(int released)
{
    int same = 1;
    for (unsigned int text = 0u; text < DOCUMENT_AT_ONCE_TEXTS; text += 1u)
    {
        same = same && text_printed(s_at_once_keys[text], s_printed[text + 1u], sizeof(s_printed[0])) && cellul.eq(&(CatenaFinitaCfg){.src = s_printed[text + 1u], .other = s_printed[0], .cap = CLI_CELLS_POOL});
        if (released)
        {
            EMBED_CALL(cli_document.release, CliDocumentCfg, .key = s_at_once_keys[text]);
        }
    }
    return same;
}

// texts colored at once by walkers on threads of their own while views and prints of them run beside them and one of them is edited, what each answered recorded, once for
// the process
static void at_once_ran(void)
{
    // views of each text as the walkers color them, near its middle, its end and its first lines
    static const unsigned long long s_views[3] = {1600ull, 2900ull, 10ull};
    // the first text's first line written again over itself as the walkers color it
    static const CliEdit s_again[1] = {{{0u, 0u}, {0u, 2u}, "/*", 2u}};
    const CliEdit *const changes[1] = {s_again};
    const unsigned int counts[1] = {1u};
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    if (s_at_once_ran)
    {
        return;
    }
    s_at_once_ran = 1;
    const unsigned long long size = at_once_text_written();
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = colors_recorded, .threaded = 0);
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "alone", .text = s_at_once, .size = size, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "alone", .grammar = "branch", .named = 1);
    cli_document_walk();
    const int alone = text_printed("alone", s_printed[0], sizeof(s_printed[0]));
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "alone");
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = colors_counted, .threaded = 1);
    for (unsigned int text = 0u; text < DOCUMENT_AT_ONCE_TEXTS; text += 1u)
    {
        (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = s_at_once_keys[text], .text = s_at_once, .size = size, .shape = &shape);
        (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = s_at_once_keys[text], .grammar = "branch", .named = 1);
    }
    for (unsigned int view = 0u; view < 3u; view += 1u)
    {
        for (unsigned int text = 0u; text < DOCUMENT_AT_ONCE_TEXTS; text += 1u)
        {
            (void)EMBED_CALL(cli_document_out_key_first.view, CliDocumentOutKeyFirstCfg, .key = s_at_once_keys[text], .first = s_views[view], .last = s_views[view] + 40ull, .out = NULL);
        }
    }
    (void)EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = s_at_once_keys[0], .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    s_at_once_during = alone && prints_match(0);
    s_at_once_ended = walkers_ended();
    s_at_once_counted = s_counted;
    s_at_once_after = prints_match(1);
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = colors_recorded, .threaded = 0);
}

// a paged text viewed far from its first line, read, walked and edited, what each step answered recorded, once for the process
static void paged_ran(void)
{
    static const char s_read_begins[] = "[{\"from\":361,\"runs\":[[0,2]";
    static const CliEdit s_opened[1] = {{{0u, 0u}, {0u, 2u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_opened};
    const unsigned int counts[1] = {1u};
    char path[CLI_PATH_MAX_LEN];
    unsigned long long lines = 0ull;
    int whole = 0;
    unsigned int root = CLI_JSON_NONE;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    if (s_paged_ran)
    {
        return;
    }
    s_paged_ran = 1;
    cli_unity_folder("document", s_folder);
    const unsigned long long size = branch_text_written();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_folder, .relative = "paged.c", .joined = path);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = s_branch, .size = size);
    s_paged_step[0] = EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = "paged", .path = path, .out = &out, .err_msg = &err_msg);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while ((!EMBED_CALL(cli_pages.count, CliPagesCfg, .key = "paged", .lines = &lines, .whole = &whole) || !whole) && (cli_clock_milliseconds() < until))
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 1u);
    }
    runs_cleared();
    s_paged_step[1] = EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = "paged", .from = 350ull, .to = 361ull) && EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "paged", .grammar = "branch", .named = 1);
    out.at = 0ull;
    const int viewed = EMBED_CALL(cli_document_out_key_first.view, CliDocumentOutKeyFirstCfg, .key = "paged", .first = 350ull, .last = 360ull, .out = &out);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &root);
    colors_read(json, root);
    s_paged_step[2] = viewed && line_sent(350u, "[0,2]") && (s_runs_of[349][0] == '\0') && (s_runs_of[361][0] == '\0');
    runs_cleared();
    cli_document_walk();
    s_paged_step[3] = (s_runs_of[350][0] == '\0') && (s_runs_of[0][0] == '\0');
    out.at = 0ull;
    const int read = EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = "paged", .first = 361ull, .last = 370ull, .mirrored = 1, .out = &out);
    (void)mmgr_line_finish(&out);
    s_paged_step[4] = read && (memor.cmp(&(MemoriaCfg){.src = s_out, .other = s_read_begins, .bytes = sizeof(s_read_begins) - 1ull}) == 0);
    runs_cleared();
    const int cached = EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = "paged", .from = 350ull, .to = 371ull);
    cli_document_walk();
    s_paged_step[5] = cached && (s_runs_of[361][0] == '\0') && (s_runs_of[370][0] == '\0');
    const int cached_all = EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = "paged", .from = 0ull, .to = DOCUMENT_BRANCH_LINES);
    cli_document_walk();
    s_paged_step[6] = cached_all && line_sent(0u, "[0,1]") && line_sent(300u, "[0,1]") && line_sent(DOCUMENT_BRANCH_LINES - 1u, "[0,2]");
    s_paged_step[7] = signature_lines_read("paged");
    out.at = 0ull;
    const int edited = EMBED_CALL(cli_document_out.paged_edit, CliDocumentOutCfg, .key = "paged", .changes = changes, .edits = counts, .count_u32 = 1u, .out = &out, .err_msg = &err_msg);
    cli_document_walk();
    s_paged_step[8] = edited && line_sent(0u, "[0,2]") && line_sent(350u, "[0,2]") && (s_seq == 1ull);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "paged");
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = "paged");
}

void setUp(void)
{
    unsigned int root = CLI_JSON_NONE;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    cli_unity_begun();
    if (!s_begun)
    {
        s_begun = 1;
        const CliJson *const json = cli_unity_parsed(s_branch_grammar, sizeof(s_branch_grammar) - 1u, &root);
        TEST_ASSERT_TRUE(EMBED_CALL(cli_highlight_key_json.store, CliHighlightKeyJsonCfg, .key = "branch", .json = json, .def = root, .err_msg = &err_msg));
        EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = colors_recorded, .threaded = 0);
    }
}

void tearDown(void)
{
}

void test_a_text_is_split_as_the_page_splits_a_file_a_char_past_the_basic_plane_two_units(void)
{
    const CliLines *const lines = lines_of("one\r\ntwo\nthree\r\r\n\xF0\x9F\x98\x80");
    TEST_ASSERT_EQUAL_UINT(4u, lines->lines);
    TEST_ASSERT_EQUAL_UINT64(6ull, lines->length[2]);
    TEST_ASSERT_EQUAL_UINT16('\r', lines->unit[lines->start[2] + 5u]);
    TEST_ASSERT_EQUAL_UINT64(2ull, lines->length[3]);
}

void test_a_large_text_accepts_letters_one_edit_at_a_time_each_where_its_edit_writes_it(void)
{
    static const CliEdit s_letter[1] = {{{50000u, 0u}, {50000u, 0u}, "x", 1u}};
    const CliEdit *const changes[1] = {s_letter};
    const unsigned int counts[1] = {1u};
    // 100000 lines of 12 letters, in a cell released once the text is stored
    const unsigned long long lines = 100000ull;
    const unsigned long long size = lines * 13ull;
    CliShape shape;
    unsigned long long pool = 0ull;
    unsigned long long before = 0ull;
    unsigned long long after = 0ull;
    unsigned long long temporary = 0ull;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    char *const text = g_cli_cells->persistent_buf_alloc(size, &g_checks_cells);
    TEST_ASSERT_NOT_NULL(text);
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        text[at] = ((at % 13ull) == 12ull) ? '\n' : 'a';
    }
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "large", .text = text, .size = size, .shape = &shape);
    g_cli_cells->persistent_buf_release(text, &g_checks_cells);
    EMBED_CALL(cli_cells_table.report, CliCellsCfg, .pool = &pool, .persistent = &before, .temporary = &temporary);
    int edited = 1;
    for (unsigned int at = 0u; at < 20u; at += 1u)
    {
        edited = edited && EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "large", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    }
    EMBED_CALL(cli_cells_table.report, CliCellsCfg, .pool = &pool, .persistent = &after, .temporary = &temporary);
    TEST_ASSERT_TRUE(edited);
    TEST_ASSERT_EQUAL_UINT64(lines + 1ull, shape.lines);
    TEST_ASSERT_EQUAL_UINT64((lines * 12ull) + 20ull, shape.units);
    assert_stored_found("large", "xxxxxxxxxxxxxxxxxxxxa", "[[50000,0,21]]");
    // no edit uses a room of the text: the cellblock's peak rises by less than one
    TEST_ASSERT_TRUE((before == 0ull) || ((after - before) < (size * 2ull)));
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "large");
}

void test_edits_are_written_from_the_last_to_the_first_with_the_texts_shape_after_them(void)
{
    CliShape shape;
    edits_written(&shape);
    TEST_ASSERT_EQUAL_UINT64(3ull, shape.lines);
    TEST_ASSERT_EQUAL_UINT64(8ull + 13ull + 7ull, shape.units);
    assert_stored_found("edits", "1st line", "[[0,0,8]]");
    assert_stored_found("edits", "second joined", "[[1,0,13]]");
    assert_stored_found("edits", "and ird", "[[2,0,7]]");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "edits");
}

void test_the_census_stored_with_the_text_counts_its_units_bytes_as_the_edits_leave_them(void)
{
    CliShape shape;
    CliCensus census;
    edits_written(&shape);
    // 28 units of ASCII, each a letter low and a zero high, four of them `n`
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document.census, CliDocumentCfg, .key = "edits", .census = &census));
    TEST_ASSERT_EQUAL_UINT64(56ull, census.total);
    TEST_ASSERT_EQUAL_UINT64(28ull, census.occurrences[0]);
    TEST_ASSERT_EQUAL_UINT64(4ull, census.occurrences['n']);
    TEST_ASSERT_EQUAL_UINT64(1ull, census.occurrences['1']);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "edits");
}

void test_an_edit_past_the_text_is_refused_and_the_text_released(void)
{
    static const CliEdit s_past[1] = {{{9u, 0u}, {9u, 0u}, "x", 1u}};
    const CliEdit *const past[1] = {s_past};
    const unsigned int one[1] = {1u};
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    edits_written(&shape);
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "edits", .changes = past, .edits = one, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = "edits", .shape = &shape));
}

void test_no_marks_are_written_before_a_commits_text_is_sent(void)
{
    CliShape shape;
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "marks", .text = "one\nTWO\nthree", .size = 13ull, .shape = &shape);
    const int marked = marks_written("marks", 0, NULL);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "marks");
    TEST_ASSERT_FALSE(marked);
}

void test_the_marks_show_the_commit_contains_the_file_and_carry_the_lines_it_contained(void)
{
    CliShape shape;
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "marks", .text = "one\nTWO\nthree", .size = 13ull, .shape = &shape);
    TEST_ASSERT_TRUE(marks_written("marks", 1, "one\ntwo\nthree"));
    CLI_TEST_ASSERT_CONTAINS("\"head\":true", s_out);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document.has_head, CliDocumentCfg, .key = "marks"));
    CLI_TEST_ASSERT_CONTAINS("\"changed\":[1]", s_out);
    CLI_TEST_ASSERT_CONTAINS("\"thenLines\":[\"two\"]", s_out);
    TEST_ASSERT_TRUE(marks_written("marks", 0, NULL));
    CLI_TEST_ASSERT_CONTAINS("\"changed\":[1]", s_out);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "marks");
}

void test_a_commit_that_contains_no_such_file_is_marked_so(void)
{
    CliShape shape;
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "marks", .text = "one\nTWO\nthree", .size = 13ull, .shape = &shape);
    TEST_ASSERT_TRUE(marks_written("marks", 1, NULL));
    TEST_ASSERT_EQUAL_STRING("{\"head\":false,\"changes\":null}", s_out);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "marks");
}

void test_a_text_is_stored_with_its_shape_until_it_is_released(void)
{
    CliShape shape;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "stored", .text = "one\ntwo\nthree", .size = 13ull, .shape = &shape));
    TEST_ASSERT_EQUAL_UINT64(3ull, shape.lines);
    TEST_ASSERT_EQUAL_UINT64(11ull, shape.units);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "stored");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = "stored", .shape = &shape));
}

void test_find_with_case(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), sought_of("line", 0, 1, 0), 100ull, "[[0,9,13],[1,2,6]]");
}

void test_find_without_case(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), sought_of("line", 0, 0, 0), 100ull, "[[0,0,4],[0,9,13],[1,2,6],[1,7,11]]");
}

void test_find_whole_words(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), sought_of("line", 0, 0, 1), 100ull, "[[0,0,4],[0,9,13],[1,7,11]]");
}

void test_find_folds_the_long_s_and_the_kelvin_sign(void)
{
    assert_found(lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), sought_of("star king", 0, 0, 0), 100ull, "[[2,0,9]]");
}

void test_the_line_that_reaches_the_most_is_searched_whole(void)
{
    unsigned int found = CLI_JSON_NONE;
    const CliDocSought most = sought_of("line", 0, 0, 0);
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = lines_of("Line one line\nnoline LINE\n\xC5\xBFtar KING"), .sought = &most, .most = 1ull, .out = &out, .err_msg = &err_msg);
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &found);
    TEST_ASSERT_EQUAL_UINT(2u, json->node[found].count);
}

void test_find_without_case_finds_a_letter_outside_ascii_in_either_case(void)
{
    assert_found(lines_of("Caf\xC3\xA9 caf\xC3\x89\nnone"), sought_of("\xC3\xA9", 0, 0, 0), 100ull, "[[0,3,4],[0,8,9]]");
}

void test_whole_words_beside_text_outside_ascii_a_mark_ends_a_word_a_letter_does_not(void)
{
    assert_found(lines_of("a line\xD9\x8B here, \xC3\xA9line"), sought_of("line", 0, 1, 1), 100ull, "[[0,2,6]]");
}

void test_a_pattern_is_found_by_the_regular_expressions_the_colorer_reads(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                          "ab"),
                 sought_of("x_\\d+", 1, 1, 0), 100ull, "[[0,0,3],[1,0,4]]");
}

void test_a_patterns_cols_are_in_utf16_units_past_a_letter_outside_the_basic_plane(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                          "ab"),
                 sought_of("[a-z]b", 1, 1, 0), 100ull, "[[1,10,12]]");
}

void test_a_pattern_that_does_not_read_is_refused(void)
{
    const CliDocSought sought = sought_of("(", 1, 1, 0);
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_document_lines_out.find_in, CliDocumentLinesOutCfg, .lines = lines_of("x_1 = 10"), .sought = &sought, .most = 100ull, .out = &out, .err_msg = &err_msg));
}

void test_a_finds_cols_are_in_utf16_units(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3\xF0\x9F\x98\x80"
                          "ab"),
                 sought_of("ab", 0, 1, 0), 100ull, "[[1,10,12]]");
}

void test_a_find_without_case_finds_an_underscored_prefix(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3"), sought_of("X_", 0, 0, 0), 100ull, "[[0,0,2],[1,0,2]]");
}

void test_nothing_found_runs_over_a_lines_end(void)
{
    assert_found(lines_of("x_1 = 10\nx_22 = 3"), sought_of("10\nx", 0, 1, 0), 100ull, "[]");
}

void test_a_small_text_is_searched_while_the_pool_has_little_room(void)
{
    // the pool allocated in cells of DOCUMENT_TAKEN_CELL until it refuses, one of them released, the search still answers
    static void *s_taken[DOCUMENT_TAKEN_MOST];
    CliShape shape;
    unsigned int taken = 0u;
    while (taken < DOCUMENT_TAKEN_MOST)
    {
        s_taken[taken] = g_cli_cells->persistent_buf_alloc(DOCUMENT_TAKEN_CELL, &g_checks_cells);
        if (s_taken[taken] == NULL)
        {
            break;
        }
        taken += 1u;
    }
    if (taken != 0u)
    {
        taken -= 1u;
        g_cli_cells->persistent_buf_release(s_taken[taken], &g_checks_cells);
    }
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "little", .text = "one line\ntwo lines", .size = 18ull, .shape = &shape);
    assert_stored_found("little", "line", "[[0,4,8],[1,4,8]]");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "little");
    for (unsigned int at = 0u; at < taken; at += 1u)
    {
        g_cli_cells->persistent_buf_release(s_taken[at], &g_checks_cells);
    }
}

void test_a_stored_text_is_found_and_its_joined_lines_made_again_after_an_edit(void)
{
    static const CliEdit s_split[1] = {{{0u, 3u}, {1u, 0u}, "\n", 1u}};
    const CliEdit *const changes[1] = {s_split};
    const unsigned int counts[1] = {1u};
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "joined", .text = "one two\nthree", .size = 13ull, .shape = &shape);
    assert_stored_found("joined", "three", "[[1,0,5]]");
    (void)EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "joined", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    assert_stored_found("joined", "three", "[[1,0,5]]");
    assert_stored_found("joined", "two", "[]");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "joined");
}

void test_a_branch_is_colored_by_a_grammar(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[0]);
    TEST_ASSERT_TRUE(s_branch_step[1]);
}

void test_a_view_is_colored_at_once_from_root_a_lead_above_it_the_comment_above_unseen(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[2]);
}

void test_the_walk_out_to_the_first_line_colors_the_view_again_as_the_comment_above_leaves_it(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[3]);
}

void test_the_lines_read_for_a_calls_signature_have_the_state_the_first_is_entered_in(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[4]);
}

void test_an_edit_removing_the_comment_colors_the_lines_after_it_with_the_change_it_follows(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[5]);
}

void test_a_text_read_whole_names_the_lines_the_page_stores(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[6]);
}

void test_only_the_lines_the_page_comes_to_store_are_sent_again(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[7]);
}

void test_the_walk_colors_none_of_them_again(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[8]);
}

void test_a_prints_lines_are_colored_and_written_whatever_lines_the_page_stores(void)
{
    branch_ran();
    TEST_ASSERT_TRUE(s_branch_step[9]);
}

void test_a_long_lines_runs_are_sent_as_far_as_a_view_draws_and_the_walk_colors_neither_again(void)
{
    static const char s_text[] = "abc def ghi\nxy";
    CliShape shape;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "columns", .text = s_text, .size = sizeof(s_text) - 1u, .shape = &shape);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "columns", .grammar = "branch", .named = 1));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = "columns", .columns = 5ull));
    runs_cleared();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = "columns", .first = 0ull, .last = 1ull, .mirrored = 1, .out = &out));
    (void)mmgr_line_finish(&out);
    TEST_ASSERT_EQUAL_STRING("[{\"from\":0,\"runs\":[[0,2,3,0,4,2],[0,2]]}]", s_out);
    cli_document_walk();
    TEST_ASSERT_EQUAL_CHAR('\0', s_runs_of[0][0]);
    TEST_ASSERT_EQUAL_CHAR('\0', s_runs_of[1][0]);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "columns");
}

void test_a_view_that_comes_to_draw_every_letter_has_the_lines_colored_again_whole(void)
{
    static const char s_text[] = "abc def ghi\nxy";
    CliShape shape;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "columns", .text = s_text, .size = sizeof(s_text) - 1u, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "columns", .grammar = "branch", .named = 1);
    (void)EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = "columns", .columns = 5ull);
    runs_cleared();
    (void)EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = "columns", .first = 0ull, .last = 1ull, .mirrored = 1, .out = &out);
    cli_document_walk();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = "columns", .columns = 0ull));
    cli_document_walk();
    TEST_ASSERT_TRUE(line_sent(0u, "[0,2,3,0,4,2,7,0,8,2]"));
    TEST_ASSERT_TRUE(line_sent(1u, "[0,2]"));
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "columns");
}

void test_a_comment_a_line_opens_past_the_letters_the_view_draws_carries_to_the_next_line(void)
{
    static const char s_opened[] = "abcdef /*\nxy";
    CliShape shape;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "opened", .text = s_opened, .size = sizeof(s_opened) - 1u, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "opened", .grammar = "branch", .named = 1);
    (void)EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = "opened", .columns = 5ull);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = "opened", .first = 0ull, .last = 1ull, .mirrored = 1, .out = &out));
    (void)mmgr_line_finish(&out);
    TEST_ASSERT_EQUAL_STRING("[{\"from\":0,\"runs\":[[0,2],[0,1]]}]", s_out);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "opened");
}

void test_a_line_of_more_than_20000_letters_is_colored_to_its_end_and_its_comment_carries_on(void)
{
    static char s_long[21007];
    CliShape shape;
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    memor.set(&(MemoriaCfg){.dst = s_long, .val = 'a', .bytes = 21000u});
    memor.cpy(&(MemoriaCfg){.dst = s_long + 21000u, .src = " /*\nxy", .bytes = 7u});
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "long", .text = s_long, .size = sizeof(s_long) - 1u, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "long", .grammar = "branch", .named = 1);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = "long", .first = 0ull, .last = 1ull, .mirrored = 0, .out = &out));
    (void)mmgr_line_finish(&out);
    TEST_ASSERT_EQUAL_STRING("[{\"from\":0,\"runs\":[[0,2,21000,0,21001,1],[0,1]]}]", s_out);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "long");
}

void test_a_brackets_folds_read_off_its_regions(void)
{
    folds_walked("brackets", s_bracket_grammar, "f {\n  a;\n  g {\n    b;\n  }\n} x\nh { c; }\nk {\n  d;\n};");
    TEST_ASSERT_EQUAL_STRING("[[0,4,0],[2,4,1],[7,9,1]]", s_folds);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "brackets");
}

void test_a_line_added_above_folds_sends_them_again_moved_once(void)
{
    static const CliEdit s_above[1] = {{{0u, 0u}, {0u, 0u}, "y;\n", 3u}};
    const CliEdit *const changes[1] = {s_above};
    const unsigned int counts[1] = {1u};
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    folds_walked("brackets", s_bracket_grammar, "f {\n  a;\n  g {\n    b;\n  }\n} x\nh { c; }\nk {\n  d;\n};");
    s_folds[0] = '\0';
    (void)EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "brackets", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    cli_document_walk();
    TEST_ASSERT_EQUAL_STRING("[[1,5,0],[3,5,1],[8,10,1]]", s_folds);
    s_folds[0] = '\0';
    cli_document_walk();
    TEST_ASSERT_EQUAL_STRING("", s_folds);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "brackets");
}

void test_a_marks_folds_end_on_the_last_line_that_has_a_leaf(void)
{
    folds_walked("indents", s_indent_grammar, "def:\n    a\n\n    b\n\nc\ne:\n    f");
    TEST_ASSERT_EQUAL_STRING("[[0,3,0],[6,7,0]]", s_folds);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "indents");
}

void test_a_plain_text_folds_each_line_over_the_deeper_lines_tabs_and_spaces_each_one(void)
{
    static const char s_plain[] = "a\n\tb\n\t\tc\n\n\td\ne\n  f\n  g";
    plugin_text_walked("plain", "plaintext", s_plain, sizeof(s_plain) - 1u);
    TEST_ASSERT_EQUAL_STRING("[[0,4,0],[1,2,0],[5,7,0]]", s_folds);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "plain");
}

void test_a_javascript_function_folds_its_body_its_closing_brace_shown(void)
{
    static const char s_script[] = "function f() {\n  return 1;\n}\n";
    plugin_text_walked("script", "javascript", s_script, sizeof(s_script) - 1u);
    TEST_ASSERT_EQUAL_STRING("[[0,2,1]]", s_folds);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "script");
}

void test_a_tables_row_folds_the_rows_indented_under_it(void)
{
    static const char s_table[] = "a\tx\n\tb\ty\tz\n\t\tc\td\ne\tf\n";
    plugin_text_walked("table", "tsv", s_table, sizeof(s_table) - 1u);
    TEST_ASSERT_EQUAL_STRING("[[0,2,0],[1,2,0]]", s_folds);
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "table");
}

void test_a_use_has_its_declarations_name_as_the_walk_reads_scopes(void)
{
    static const char s_code[] = "int f(int count)\n{\n    int total = 0;\n    total = count + other;\n"
                                 "    view->total = 1;\n}\nint g(void)\n{\n    total = 2;\n}\n";
    char key[CLI_NAME_MAX_LEN];
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "c", .key = key, .err_msg = &err_msg));
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "uses", .text = s_code, .size = sizeof(s_code) - 1u, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "uses", .grammar = key, .named = 1);
    cli_document_walk();
    const int local = run_named("uses", key, 3u, 4u, "identifier.local");
    const int parameter = run_named("uses", key, 3u, 12u, "identifier.parameter");
    const int undeclared = run_named("uses", key, 3u, 20u, "identifier");
    const int through_arrow = run_named("uses", key, 4u, 10u, "identifier");
    const int past_region = run_named("uses", key, 8u, 4u, "identifier");
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "uses");
    TEST_ASSERT_TRUE(local);
    TEST_ASSERT_TRUE(parameter);
    TEST_ASSERT_TRUE(undeclared);
    TEST_ASSERT_TRUE(through_arrow);
    TEST_ASSERT_TRUE(past_region);
}

void test_each_bracket_is_named_by_the_depth_its_pair_is_at(void)
{
    char key[CLI_NAME_MAX_LEN];
    brackets_walked(key);
    TEST_ASSERT_TRUE(printed_has_name("depth", key, 0u, 0u, "delimiter.bracket.depth.open.0"));
    TEST_ASSERT_TRUE(printed_has_name("depth", key, 1u, 2u, "delimiter.bracket.depth.open.1"));
    TEST_ASSERT_TRUE(printed_has_name("depth", key, 2u, 9u, "delimiter.bracket.depth.open.2"));
    TEST_ASSERT_TRUE(printed_has_name("depth", key, 2u, 11u, "delimiter.bracket.depth.close.2"));
    TEST_ASSERT_TRUE(printed_has_name("depth", key, 3u, 2u, "delimiter.bracket.depth.close.1"));
    TEST_ASSERT_TRUE(printed_has_name("depth", key, 4u, 0u, "delimiter.bracket.depth.close.0"));
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "depth");
}

void test_a_bracket_opened_above_the_pages_lines_colors_them_again_at_their_new_depths(void)
{
    static const CliEdit s_opened[1] = {{{0u, 0u}, {0u, 0u}, "[", 1u}};
    const CliEdit *const changes[1] = {s_opened};
    const unsigned int counts[1] = {1u};
    char key[CLI_NAME_MAX_LEN];
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    brackets_walked(key);
    (void)EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = "depth", .from = 2ull, .to = 5ull);
    runs_cleared();
    (void)EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "depth", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    cli_document_walk();
    TEST_ASSERT_TRUE(run_has_name(s_runs_of[2], key, 9u, "delimiter.bracket.depth.open.3"));
    TEST_ASSERT_TRUE(run_has_name(s_runs_of[4], key, 0u, "delimiter.bracket.depth.close.1"));
    TEST_ASSERT_TRUE(printed_has_name("depth", key, 0u, 1u, "delimiter.bracket.depth.open.1"));
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "depth");
}

void test_an_edit_that_moves_the_state_of_every_line_after_it_is_walked_many_lines_to_a_step(void)
{
    static const CliEdit s_quoted[1] = {{{1000u, 0u}, {1000u, 0u}, "\"\"\"", 3u}};
    const CliEdit *const changes[1] = {s_quoted};
    const unsigned int counts[1] = {1u};
    char key[CLI_NAME_MAX_LEN];
    CliShape shape;
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = "python", .key = key, .err_msg = &err_msg));
    char *const text = g_cli_cells->persistent_buf_alloc(DOCUMENT_ONWARD_ROOM, &g_checks_cells);
    TEST_ASSERT_NOT_NULL(text);
    VerbaLinea out = {text, DOCUMENT_ONWARD_ROOM, 0ull};
    for (unsigned long long line = 0ull; line < DOCUMENT_ONWARD_LINES; line += 1ull)
    {
        mmgr_line_put(&out, "x_");
        mmgr_line_decimal(&out, line, 1u);
        mmgr_line_put(&out, " = ");
        mmgr_line_decimal(&out, line, 1u);
        mmgr_line_put(&out, " * 2  # line ");
        mmgr_line_decimal(&out, line, 1u);
        mmgr_line_ch(&out, '\n');
    }
    (void)EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = "onward", .text = text, .size = out.at, .shape = &shape);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = "onward", .grammar = key, .named = 1);
    (void)EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = "onward", .columns = 120ull);
    const unsigned long long began = cli_clock_milliseconds();
    cli_document_walk();
    const unsigned long long walked = cli_clock_milliseconds();
    (void)EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = "onward", .changes = changes, .edits = counts, .count_u32 = 1u, .shape = &shape, .err_msg = &err_msg);
    cli_document_walk();
    const unsigned long long walked_again = cli_clock_milliseconds();
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = "onward");
    g_cli_cells->persistent_buf_release(text, &g_checks_cells);
    // no slower than its first walk of the whole text twice over, a little past it for a walk too short to time
    TEST_ASSERT_LESS_OR_EQUAL_UINT64((2ull * (walked - began)) + 100ull, walked_again - walked);
}

void test_texts_colored_at_once_print_as_one_walked_alone_views_and_an_edit_beside_them(void)
{
    at_once_ran();
    TEST_ASSERT_TRUE(s_at_once_during);
}

void test_the_walkers_send_their_colors_and_end_once_every_text_is_colored(void)
{
    at_once_ran();
    TEST_ASSERT_TRUE(s_at_once_ended);
    TEST_ASSERT_NOT_EQUAL(0u, s_at_once_counted);
}

void test_each_text_the_walkers_walked_prints_as_the_one_walked_alone(void)
{
    at_once_ran();
    TEST_ASSERT_TRUE(s_at_once_after);
}

void test_the_branchs_file_opens_as_a_paged_text_colored_by_a_grammar(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[0]);
    TEST_ASSERT_TRUE(s_paged_step[1]);
}

void test_a_paged_view_is_colored_at_once_only_the_lines_the_page_stores_sent(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[2]);
}

void test_the_paged_walk_colors_the_lines_the_page_stores_alone_the_comment_above_unread(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[3]);
}

void test_a_reads_lines_are_colored_and_written_as_the_views_lines_above_leave_them(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[4]);
}

void test_the_walk_sends_a_reads_lines_no_second_time(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[5]);
}

void test_the_lines_the_page_comes_to_store_are_sent_as_the_walk_colors_them_again(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[6]);
}

void test_a_paged_texts_lines_are_read_for_a_calls_signature_through_the_pages(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[7]);
}

void test_an_edit_the_pages_write_colors_the_lines_after_it_with_the_change_it_follows(void)
{
    paged_ran();
    TEST_ASSERT_TRUE(s_paged_step[8]);
}
