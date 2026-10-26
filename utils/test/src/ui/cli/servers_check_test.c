// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_check_test.c: the tree's check stores every file of a tree it is given and lists them back in order, taking
// cells sized to those files, and gives every cell back as it is let go
#include "../../../../../ui/cli/src/servers/servers_internal.h"
#include "cli_check.h"

// the files of the tree given: more than the check's room begins with, each named by its place
#define CHECK_FILES 5000u

static char s_names[CHECK_FILES][64];
static const char *s_files[CHECK_FILES];
static char s_owners[1u << 18u];

// The files the check lists back: how many, and whether each stood where it was given
typedef struct
{
    unsigned int count;
    int in_order;
} CheckListed;

// a file the check lists, checked against the one given at its place
static void check_listed(void *context, const char *file)
{
    CheckListed *const listed = context;
    listed->in_order =
        listed->in_order && (listed->count < CHECK_FILES) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = s_names[listed->count], .cap = CLI_CELLS_POOL}));
    listed->count += 1u;
}

// nothing the servers send is received
static void events_ignored(void *context, const char *event, const char *json, unsigned long long size)
{
    (void)context;
    (void)event;
    (void)json;
    (void)size;
}

// a whole number member of a node, 0 where it has none
static unsigned long long check_whole(const CliJson *json, unsigned int node, const char *name)
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

// the bytes in use now by the cells taken for the tree's check, as the cells' audit counts them
static unsigned long long check_in_use(void)
{
    VerbaLinea out = {s_owners, sizeof(s_owners), 0ull};
    cli_cells_owners_written(&out);
    unsigned int owners = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_owners, out.at, &owners);
    unsigned long long in_use = 0ull;
    for (unsigned int at = 0u; cli_json_item(json, owners, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, owners, at);
        const char *const name = cli_json_string(json, cli_json_member(json, one, "name"), NULL);
        if ((name != NULL) &&
            (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "the tree's check", .cap = CLI_CELLS_POOL})))
        {
            in_use += check_whole(json, one, "in_use");
        }
    }
    return in_use;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char root[CLI_PATH_MAX_LEN];
    cli_check_folder("servers_check", root);
    for (unsigned int at = 0u; at < CHECK_FILES; at += 1u)
    {
        VerbaLinea name = {s_names[at], sizeof(s_names[at]), 0ull};
        mmgr_line_put(&name, "notes/a_note_of_the_tree_numbered_");
        mmgr_line_decimal(&name, at, 5u);
        mmgr_line_put(&name, ".txt");
        (void)mmgr_line_finish(&name);
        s_files[at] = s_names[at];
    }
    CliServersSink sink = {events_ignored, NULL, ""};
    (void)cli_servers_check_tree(root, s_files, CHECK_FILES, &sink);
    CheckListed listed = {0u, 1};
    cli_check(cli_servers_check_tree_files(root, check_listed, &listed), "the check is on over the tree");
    cli_check((listed.count == CHECK_FILES) && listed.in_order, "every file of the tree is stored and listed in order");
    const unsigned long long in_use = check_in_use();
    cli_check(in_use < (1ull << 20u), "the check takes cells sized to the tree's files, under a mebibyte for these");
    cli_servers_check_let_go();
    cli_check(check_in_use() == 0ull, "and gives every cell back as it is let go");
    return cli_checked();
}
