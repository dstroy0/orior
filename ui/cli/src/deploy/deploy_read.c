// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_read.c: the deployments deploy.json names, read as serde reads them; the places a deployment sends to; and a
// shell line run at one
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

// a page of `room` bytes taken for `pool`, NULL with the pool marked refused where it could not be
static CliDeployPage *read_page(CliDeployPool *pool, unsigned long long room)
{
    CliDeployPage *const page = cli_cell_taken(sizeof(CliDeployPage) + room, "a deployment's pool");
    if (page == NULL)
    {
        pool->refused = 1;
        return NULL;
    }
    page->size = room;
    page->used = 0ull;
    return page;
}

void *cli_deploy_placed(CliDeployPool *pool, unsigned long long size)
{
    CliDeployPage *const newest = pool->page;
    if (size > CLI_DEPLOY_PAGE)
    {
        // a page of its own goes behind the newest, which keeps the bytes it has left
        CliDeployPage *const own = read_page(pool, size);
        if (own == NULL)
        {
            return NULL;
        }
        own->used = size;
        own->before = (newest != NULL) ? newest->before : NULL;
        if (newest != NULL)
        {
            newest->before = own;
        }
        else
        {
            pool->page = own;
        }
        return own->bytes;
    }
    const unsigned long long at = (newest != NULL) ? ((newest->used + 7u) & ~7ull) : 0ull;
    if ((newest != NULL) && (at <= newest->size) && (size <= newest->size - at))
    {
        newest->used = at + size;
        return newest->bytes + at;
    }
    CliDeployPage *const page = read_page(pool, CLI_DEPLOY_PAGE);
    if (page == NULL)
    {
        return NULL;
    }
    page->before = newest;
    page->used = size;
    pool->page = page;
    return page->bytes;
}

const char *cli_deploy_kept(CliDeployPool *pool, const char *text, unsigned long long size)
{
    char *const kept = cli_deploy_placed(pool, size + 1u);
    if (kept == NULL)
    {
        return "";
    }
    memor.cpy(&(MemoriaCfg){.dst = kept, .src = text, .bytes = (size_t)size});
    kept[size] = '\0';
    return kept;
}

int cli_deploy_order(const char *left, const char *right)
{
    const size_t left_length = cellul.len(&(CatenaFinitaCfg){.src = left, .cap = CLI_CELLS_POOL});
    const size_t right_length = cellul.len(&(CatenaFinitaCfg){.src = right, .cap = CLI_CELLS_POOL});
    // the shorter text's terminator is compared too, so a text sorts before a longer one that begins with it
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const embed_iword order = memor.cmp(&(MemoriaCfg){.src = left, .other = right, .bytes = shorter + 1u});
    return (order > 0) - (order < 0);
}

void cli_deploy_pool_given(CliDeployPool *pool)
{
    while (pool->page != NULL)
    {
        CliDeployPage *const before = pool->page->before;
        cli_cell_given(pool->page);
        pool->page = before;
    }
    pool->refused = 0;
}

// the name serde gives a value's kind
static const char *read_kind(const CliJson *json, unsigned int node)
{
    static const char *const kinds[7] = {"map", "sequence", "string", "integer", "boolean", "boolean", "null"};
    return kinds[json->node[node].kind];
}

// that a value is not the kind wanted said, as serde says it
static int read_invalid(const CliJson *json, unsigned int node, const char *wanted, CliLine *said)
{
    cli_line_text(said, "deploy.json: invalid type: ");
    cli_line_text(said, read_kind(json, node));
    cli_line_text(said, ", expected ");
    cli_line_text(said, wanted);
    return 0;
}

// that a field is missing said, as serde says it
static int read_missing(const char *field, CliLine *said)
{
    cli_line_text(said, "deploy.json: missing field `");
    cli_line_text(said, field);
    cli_line_character(said, '`');
    return 0;
}

// the texts of the list `field` of `entry` into `*texts`, placed in `pool` at their count, that count into `count`:
// 1, or 0 with why said
static int read_texts(const CliJson *json, unsigned int entry, const char *field, CliDeployPool *pool,
                      const char ***texts, unsigned int *count, CliLine *said)
{
    const unsigned int list = cli_json_member(json, entry, field);
    *texts = NULL;
    *count = 0u;
    if (list == CLI_JSON_NONE)
    {
        return 1;
    }
    if (!cli_json_is(json, list, CFG_JSON_ARRAY))
    {
        return read_invalid(json, list, "a sequence", said);
    }
    unsigned int items = 0u;
    for (; cli_json_item(json, list, items) != CLI_JSON_NONE; items += 1u)
    {
        const unsigned int item = cli_json_item(json, list, items);
        if (!cli_json_is(json, item, CFG_JSON_STRING))
        {
            return read_invalid(json, item, "a string", said);
        }
    }
    if (items == 0u)
    {
        return 1;
    }
    const char **const kept = cli_deploy_placed(pool, (unsigned long long)items * sizeof(kept[0]));
    if (kept == NULL)
    {
        cli_line_text(said, "deploy.json: ");
        cli_line_text(said, field);
        cli_line_text(said, " names more than orior holds");
        return 0;
    }
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        kept[at] = cli_json_string(json, cli_json_item(json, list, at), NULL);
    }
    *texts = kept;
    *count = items;
    return 1;
}

// one deployment of the list read into `deployment`, its lists placed in `pool`: 1, or 0 with why said
static int read_deployment(const CliJson *json, unsigned int entry, CliDeployPool *pool, CliDeployment *deployment,
                           CliLine *said)
{
    if (!cli_json_is(json, entry, CFG_JSON_OBJECT))
    {
        return read_invalid(json, entry, "struct Deployment", said);
    }
    const unsigned int name = cli_json_member(json, entry, "name");
    const unsigned int from = cli_json_member(json, entry, "from");
    const unsigned int keep = cli_json_member(json, entry, "keep");
    const unsigned int in_place = cli_json_member(json, entry, "inPlace");
    if (name == CLI_JSON_NONE)
    {
        return read_missing("name", said);
    }
    if (cli_json_member(json, entry, "to") == CLI_JSON_NONE)
    {
        return read_missing("to", said);
    }
    if (!cli_json_is(json, name, CFG_JSON_STRING) ||
        ((from != CLI_JSON_NONE) && !cli_json_is(json, from, CFG_JSON_STRING)))
    {
        return read_invalid(json, cli_json_is(json, name, CFG_JSON_STRING) ? from : name, "a string", said);
    }
    deployment->name = cli_json_string(json, name, NULL);
    deployment->from = (from != CLI_JSON_NONE) ? cli_json_string(json, from, NULL) : ".";
    deployment->keep = 5ull;
    if (keep != CLI_JSON_NONE)
    {
        unsigned long long length = 0ull;
        const char *const digits = cli_json_is(json, keep, CFG_JSON_NUMBER) ? (json->text + json->node[keep].text) : "";
        length = cli_json_is(json, keep, CFG_JSON_NUMBER) ? json->node[keep].length : 0ull;
        unsigned long long value = 0ull;
        int whole = (length > 0u);
        for (unsigned long long at = 0ull; whole && (at < length); at += 1u)
        {
            whole = (digits[at] >= '0') && (digits[at] <= '9') &&
                    (value <= (0xffffffffffffffffull - (unsigned long long)(digits[at] - '0')) / 10u);
            value = whole ? ((value * 10u) + (unsigned long long)(digits[at] - '0')) : value;
        }
        if (!whole)
        {
            return read_invalid(json, keep, "usize", said);
        }
        deployment->keep = value;
    }
    deployment->in_place = 0;
    if (in_place != CLI_JSON_NONE)
    {
        if (!cli_json_is(json, in_place, CFG_JSON_TRUE) && !cli_json_is(json, in_place, CFG_JSON_FALSE))
        {
            return read_invalid(json, in_place, "a boolean", said);
        }
        deployment->in_place = cli_json_is(json, in_place, CFG_JSON_TRUE);
    }
    return read_texts(json, entry, "exclude", pool, &deployment->exclude, &deployment->excludes, said) &&
           read_texts(json, entry, "to", pool, &deployment->to, &deployment->tos, said);
}

void cli_deploy_tree_given(CliDeployTree *tree)
{
    cli_json_given(&tree->json);
    cli_deploy_pool_given(&tree->pool);
}

int cli_deploy_read(const char *root, CliDeployTree *tree, const CliDeployment **deployments, unsigned int *count,
                    CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, "deploy.json", path);
    cli_deploy_tree_given(tree);
    *deployments = NULL;
    *count = 0u;
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    if ((bytes == NULL) || !cli_utf8_whole((const char *)bytes, size))
    {
        if (bytes != NULL)
        {
            qry_member_file_unmapped(bytes, size);
        }
        return 1;
    }
    char why[CLI_SAID_LONGEST];
    CliLine read_said = {why, sizeof(why), 0ull};
    const unsigned int top = cli_json_taken(&tree->json, (const char *)bytes, size, 0u, 0ull, &read_said);
    qry_member_file_unmapped(bytes, size);
    const CliJson *const json = &tree->json;
    if (top == CLI_JSON_NONE)
    {
        cli_line_text(said, "deploy.json: ");
        cli_line_counted(said, why, read_said.at);
        return 0;
    }
    if (!cli_json_is(json, top, CFG_JSON_OBJECT))
    {
        return read_invalid(json, top, "struct Named", said);
    }
    const unsigned int list = cli_json_member(json, top, "deployments");
    if (list == CLI_JSON_NONE)
    {
        return read_missing("deployments", said);
    }
    if (!cli_json_is(json, list, CFG_JSON_ARRAY))
    {
        return read_invalid(json, list, "a sequence", said);
    }
    unsigned int items = 0u;
    while (cli_json_item(json, list, items) != CLI_JSON_NONE)
    {
        items += 1u;
    }
    if (items == 0u)
    {
        return 1;
    }
    CliDeployment *const read = cli_deploy_placed(&tree->pool, (unsigned long long)items * sizeof(read[0]));
    if (read == NULL)
    {
        cli_line_text(said, "deploy.json names more deployments than orior holds");
        return 0;
    }
    for (unsigned int at = 0u; at < items; at += 1u)
    {
        if (!read_deployment(json, cli_json_item(json, list, at), &tree->pool, &read[at], said))
        {
            return 0;
        }
    }
    *deployments = read;
    *count = items;
    return 1;
}

// whether a letter is white as Rust's char::is_whitespace has it
static int read_white(unsigned int letter)
{
    return ((letter >= 0x09u) && (letter <= 0x0du)) || (letter == 0x20u) || (letter == 0x85u) || (letter == 0xa0u) ||
           (letter == 0x1680u) || ((letter >= 0x2000u) && (letter <= 0x200au)) || (letter == 0x2028u) ||
           (letter == 0x2029u) || (letter == 0x202fu) || (letter == 0x205fu) || (letter == 0x3000u);
}

int cli_deploy_place(const char *text, const char *root, CliDeployPlace *place, CliLine *said)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    unsigned int letter = 0u;
    for (unsigned long long at = 0ull; at < length;)
    {
        const unsigned long long taken = cli_lossy_letter((const unsigned char *)text, length, at, &letter);
        start = ((start == at) && read_white(letter)) ? (at + taken) : start;
        at += taken;
        end = read_white(letter) ? end : at;
    }
    end = (end < start) ? start : end;
    char trimmed[CLI_PATH_LONGEST];
    const unsigned long long size = (end - start < sizeof(trimmed)) ? (end - start) : (sizeof(trimmed) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = trimmed, .src = text + start, .bytes = (size_t)size});
    trimmed[size] = '\0';
    const int letter_drive =
        ((trimmed[0] >= 'a') && (trimmed[0] <= 'z')) || ((trimmed[0] >= 'A') && (trimmed[0] <= 'Z'));
    const int drive = (size >= 2u) && letter_drive && (trimmed[1] == ':') &&
                      ((size == 2u) || (trimmed[2] == '/') || (trimmed[2] == '\\'));
    if (!drive && (trimmed[0] != '/') && (trimmed[0] != '.') && (trimmed[0] != '\\'))
    {
        place->here = 0;
        return cli_link_address_parse(trimmed, &place->address, said);
    }
    place->here = 1;
    CliLine path = {place->path, sizeof(place->path), 0ull};
#if defined(_WIN32)
    // a path with a drive stands on its own, as Rust's join takes one, as does a share's
    const int whole =
        drive || (((trimmed[0] == '/') || (trimmed[0] == '\\')) && ((trimmed[1] == '/') || (trimmed[1] == '\\')));
    const int rooted = !whole && !drive && ((trimmed[0] == '/') || (trimmed[0] == '\\'));
    if (rooted && (root[0] != '\0') && (root[1] == ':'))
    {
        cli_line_counted(&path, root, 2u);
    }
    else if (!whole && !rooted)
    {
        cli_line_text(&path, root);
        cli_line_character(&path, '\\');
    }
#else
    if (trimmed[0] != '/')
    {
        cli_line_text(&path, root);
        cli_line_character(&path, '/');
    }
#endif
    cli_line_text(&path, trimmed);
    if (cli_line_closed(&path) == 0ull)
    {
        cli_line_text(said, trimmed);
        cli_line_text(said, ": its path is too long");
        return 0;
    }
    return 1;
}

void cli_deploy_place_name(const CliDeployPlace *place, CliLine *out)
{
    if (place->here)
    {
        cli_line_text(out, place->path);
    }
    else
    {
        cli_link_address_text(&place->address, out);
    }
}

const char *cli_deploy_place_folder(const CliDeployPlace *place)
{
    return place->here ? place->path : place->address.folder;
}

int cli_deploy_ask(const CliDeployPlace *place, const char *line, const char *input, unsigned long long size,
                   CliLine *out, CliLine *said)
{
    if (place->here)
    {
        cli_line_text(said, "a folder on this machine takes no shell line");
        return 0;
    }
    char why[CLI_SAID_LONGEST];
    CliLine why_line = {why, sizeof(why) - 1u, 0ull};
    if (cli_link_ask(&place->address, line, input, size, out, &why_line))
    {
        return 1;
    }
    why[why_line.at] = '\0';
    cli_line_counted(said, why, why_line.at);
    static const char s_denied[] = "Permission denied";
    static const char s_password[] = "password";
    if (cellul.has(
            &(CatenaFinitaCfg){.src = why, .cap = why_line.at, .other = s_denied, .other_cap = sizeof(s_denied)}) ||
        cellul.has(
            &(CatenaFinitaCfg){.src = why, .cap = why_line.at, .other = s_password, .other_cap = sizeof(s_password)}))
    {
        cli_line_text(said, "; ");
        cli_link_machine(&place->address, said);
        cli_line_text(said, " asks for a password, which orior does not keep: a key the system's ssh agent holds "
                            "reaches it, and ssh-copy-id puts one there");
    }
    return 0;
}
