// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_test.c: the JSON schemas' check: JSON reads with its places and its faults, a file is checked against the
// schema it names, a schema is checked against its own schema, a schema not held offers its fetch, and keys and values
// complete from the schema
#include "../../../../../ui/cli/src/code/schema_internal.h"
#include "cli_check.h"

static const char s_schema[] =
    "{\n  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n  \"type\": \"object\",\n"
    "  \"required\": [\"name\", \"version\"],\n  \"additionalProperties\": false,\n"
    "  \"definitions\": {\"level\": {\"type\": \"string\", \"enum\": [\"low\", \"medium\", \"high\"], \"description\": "
    "\"How loud\"}},\n  \"properties\": {\n    \"$schema\": {\"type\": \"string\"},\n"
    "    \"name\": {\"type\": \"string\", \"minLength\": 2, \"description\": \"The name\"},\n"
    "    \"version\": {\"type\": \"string\", \"pattern\": \"^\\\\d+\\\\.\\\\d+\\\\.\\\\d+$\"},\n"
    "    \"count\": {\"type\": \"integer\", \"minimum\": 1, \"maximum\": 10},\n"
    "    \"level\": {\"$ref\": \"#/definitions/level\"},\n"
    "    \"tags\": {\"type\": \"array\", \"items\": {\"type\": \"string\"}, \"uniqueItems\": true},\n"
    "    \"old\": {\"type\": \"boolean\", \"deprecated\": true},\n"
    "    \"server\": {\"type\": \"object\", \"properties\": {\"port\": {\"type\": \"integer\"}, \"host\": {\"type\": "
    "\"string\"}}, \"required\": [\"port\"]}\n  }\n}";

static CliSchemaRead s_read;
static char s_out[1u << 18u];
static CliCompletions s_items;
static char s_dir[CLI_PATH_LONGEST];

// JSON reads with its places and its faults
static void schema_json(void)
{
    static const char s_text[] = "{\n  \"a\": [1, 2,],\n  \"b\": tru,\n  // a comment\n  \"a\": {}\n}";
    cli_schema_read(s_text, sizeof(s_text) - 1u, &s_read);
    int no_item = 0;
    int no_value = 0;
    int above = 0;
    for (unsigned int at = 0u; at < s_read.errors; at += 1u)
    {
        const char *const message = cli_schema_text(&s_read, s_read.error[at].message_at);
        no_item |=
            cellul.find(&(CatenaFinitaCfg){
                .src = message, .cap = CLI_CELLS_POOL, .other = "No item follows", .other_cap = CLI_CELLS_POOL}) !=
            NULL;
        no_value |=
            cellul.find(&(CatenaFinitaCfg){
                .src = message, .cap = CLI_CELLS_POOL, .other = "`tru` is no value", .other_cap = CLI_CELLS_POOL}) !=
            NULL;
        above |= cellul.find(&(CatenaFinitaCfg){.src = message,
                                                .cap = CLI_CELLS_POOL,
                                                .other = "\"a\" is a key of this object above it",
                                                .other_cap = CLI_CELLS_POOL}) != NULL;
    }
    cli_check(no_item && no_value && above && (s_read.errors == 3u), "JSON reads with its faults");
    const CliSchemaNode *const root = (s_read.root != CLI_SCHEMA_NONE) ? &s_read.node[s_read.root] : NULL;
    cli_check((root != NULL) && (root->kind == CLI_SCHEMA_OBJECT) && (root->count == 3u) &&
                  (s_read.member[root->first].from == 4ull),
              "and with its places");
}

// the findings of `text`, the file `name` of the tree at `root`, read into the shared tree: the array's node
static unsigned int schema_checked(const char *root, const char *name, const char *text, const CliJson **json)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_schema_check(root, path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &out);
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// the first fix of a finding
static unsigned int schema_fix(const CliJson *json, unsigned int finding)
{
    return cli_json_item(json, cli_json_member(json, cli_json_member(json, finding, "data"), "orior"), 0u);
}

// a file is checked against the schema it names
static void schema_file(void)
{
    static const char s_text[] =
        "{\n  \"$schema\": \"./schema.json\",\n  \"name\": \"x\",\n  \"version\": \"1.0\",\n  \"count\": 12,\n"
        "  \"level\": \"loud\",\n  \"tags\": [\"a\", \"a\"],\n  \"old\": true,\n  \"server\": {\"host\": 1},\n"
        "  \"nmae\": 1\n}\n";
    static const char *const s_has[9][2] = {{"schema-length", "shorter than the 2"},
                                            {"schema-pattern", "pattern"},
                                            {"schema-range", "12 is above the most the schema allows, 10"},
                                            {"schema-enum", "`\"loud\"`"},
                                            {"schema-unique", "above it"},
                                            {"schema-deprecated", "\"old\" is deprecated"},
                                            {"schema-required", "`port`"},
                                            {"schema-property", "\"nmae\" is no property"},
                                            {"schema-type", NULL}};
    char path[CLI_PATH_LONGEST];
    cli_full(s_dir, "schema.json", path);
    (void)query_file_written(path, s_schema, sizeof(s_schema) - 1u);
    const CliJson *json = NULL;
    const unsigned int found = schema_checked(s_dir, "a.json", s_text, &json);
    int has = 1;
    for (unsigned int at = 0u; at < 9u; at += 1u)
    {
        has &= cli_check_finding(found, s_has[at][0], s_has[at][1]) != CLI_JSON_NONE;
    }
    cli_check(has, "a file is checked against the schema it names");
    const unsigned int typo = schema_fix(json, cli_check_finding(found, "schema-property", NULL));
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(cli_json_item(json, cli_json_member(json, typo, "edits"), 0u), "text"),
                  .other = "\"name\"",
                  .cap = CLI_CELLS_POOL}),
              "a property near a known one is offered it");
    const unsigned int loud = schema_fix(json, cli_check_finding(found, "schema-enum", NULL));
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(loud, "title"), .other = "Change to \"low\"", .cap = CLI_CELLS_POOL}),
              "a value not allowed the nearest");
    cli_check(json->node[found].count == 9u, "and nothing more is found");
}

// a schema is checked against its own schema, and a schema not held offers its fetch
static void schema_own(void)
{
    const CliJson *json = NULL;
    unsigned int found = schema_checked("/t", "s.json",
                                        "{\n  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n  \"type\": "
                                        "\"strin\",\n  \"minLength\": -1,\n  \"required\": [\"a\", \"a\"]\n}\n",
                                        &json);
    cli_check(cli_check_finding(found, NULL, "\"strin\"") != CLI_JSON_NONE, "a type no schema knows is found");
    cli_check(cli_check_finding(found, NULL, "-1 is below the least") != CLI_JSON_NONE, "a bound below its least");
    cli_check(cli_check_finding(found, "schema-unique", NULL) != CLI_JSON_NONE, "and a name required twice");
    found = schema_checked(
        "/t", "s.json", "{\"$schema\": \"https://json-schema.org/draft/2020-12/schema\", \"prefixItems\": 3}", &json);
    cli_check(cli_check_finding(found, "schema-type", NULL) != CLI_JSON_NONE, "a 2020-12 schema is held to its draft");
    found = schema_checked("/t", "a.json", "{\"$schema\": \"https://example.com/none.json\"}", &json);
    const unsigned int missing = cli_json_item(json, found, 0u);
    const char *const line = cli_check_member_text(cli_json_member(json, schema_fix(json, missing), "run"), "line");
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
    static const char s_end[] = "' 'https://example.com/none.json'";
    cli_check(
        (json->node[found].count == 1u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(missing, "code"), .other = "schema-missing", .cap = CLI_CELLS_POOL})) &&
            (cellul.starts(
                &(CatenaFinitaCfg){.src = line, .other = "curl -fsSL --create-dirs -o '", .cap = CLI_CELLS_POOL})) &&
            (length >= sizeof(s_end) - 1u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = line + length - (sizeof(s_end) - 1u), .other = s_end, .cap = CLI_CELLS_POOL})),
        "a schema not held offers its fetch");
}

// what completes the end of `text`, the file a.json of the tree
static const CliCompletions *schema_completed(const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_dir, "a.json", path);
    unsigned int line = 0u;
    unsigned int col = 0u;
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        line += (at[0] == '\n') ? 1u : 0u;
        col = (at[0] == '\n') ? 0u : col + 1u;
    }
    cli_completions_begun(&s_items);
    cli_schema_complete(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), line, col,
                        &s_items);
    return &s_items;
}

// whether an item shows `label` and, where `insert` is not NULL, inserts it
static int schema_offers(const CliCompletions *items, const char *label, const char *insert)
{
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const item = &items->item[at];
        const int labeled =
            (item->label_size == cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_CELLS_POOL})) &&
            (memor.cmp(&(MemoriaCfg){.src = items->text + item->label_at, .other = label, .bytes = item->label_size}) ==
             0);
        if (labeled &&
            ((insert == NULL) ||
             ((item->insert_size == cellul.len(&(CatenaFinitaCfg){.src = insert, .cap = CLI_CELLS_POOL})) &&
              (memor.cmp(&(MemoriaCfg){
                   .src = items->text + item->insert_at, .other = insert, .bytes = item->insert_size}) == 0))))
        {
            return 1;
        }
    }
    return 0;
}

// the labels of the items, `|` between them, checked against `wanted`
static void schema_labels(const CliCompletions *items, const char *wanted, const char *what)
{
    char text[256];
    CliLine line = {text, sizeof(text), 0ull};
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_counted(&line, items->text + items->item[at].label_at, items->item[at].label_size);
    }
    cli_check_text(text, line.at, wanted, what);
}

// keys and values complete from the schema
static void schema_completes(void)
{
    const CliCompletions *items = schema_completed("{\n  \"$schema\": \"./schema.json\",\n  \"name\": \"x\",\n  ");
    cli_check(schema_offers(items, "version", "\"version\": \"$0\""), "a key the schema gives completes");
    cli_check(!schema_offers(items, "name", NULL), "and a key the object holds is not offered");
    items = schema_completed("{\"$schema\": \"./schema.json\", \"ser");
    cli_check(schema_offers(items, "server", "server"), "a key begun in its quotes completes inside them");
    schema_labels(schema_completed("{\"$schema\": \"./schema.json\", \"server\": {\"po"), "port|host",
                  "an inner object's keys complete");
    schema_labels(schema_completed("{\"$schema\": \"./schema.json\", \"level\": \""), "low|medium|high",
                  "a value completes from its enum");
    schema_labels(schema_completed("{\"$schema\": \"./schema.json\", \"old\": "), "true|false", "and a boolean's");
    cli_check(schema_offers(schema_completed("{\"$schema\": \"http://json-schema.org/draft-07/schema#\", \"type\": \""),
                            "object", NULL),
              "a schema completes from its own schema");
}

int main(void)
{
    cli_check_folder("schema", s_dir);
    schema_json();
    schema_file();
    schema_own();
    schema_completes();
    return cli_checked();
}
