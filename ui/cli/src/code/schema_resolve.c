// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_resolve.c: where a schema's reference leads and which schema a file names: a `$ref` resolved by its document
// and its fragment, a JSON pointer with its `%XX` escapes read or an anchor, a reference read beside its base, a URL's
// or a file's; and a file's schema found by its `$schema` or by the name it is known by
#include "schema_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the schema files of these names follow where they name none, by the SchemaStore's URL
static const char *const s_known[8][2] = {
    {"package.json", "https://json.schemastore.org/package.json"},
    {"tsconfig.json", "https://json.schemastore.org/tsconfig.json"},
    {"jsconfig.json", "https://json.schemastore.org/jsconfig.json"},
    {".eslintrc.json", "https://json.schemastore.org/eslintrc.json"},
    {".prettierrc.json", "https://json.schemastore.org/prettierrc.json"},
    {"composer.json", "https://getcomposer.org/schema.json"},
    {".babelrc.json", "https://json.schemastore.org/babelrc.json"},
    {"devcontainer.json",
     "https://raw.githubusercontent.com/devcontainers/spec/main/schemas/devContainer.schema.json"}};

typedef struct
{
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
    const CliJson *json;
    unsigned int node;
    const char *pointer;
    const char *name;
    const char *base;
    const char *target;
    char *out_capacity;
} SchemaResolveCtx;
// a fragment of a reference with its `%XX` escapes read into `out`
EMBED_INLINE void resolve_percent_decode(const SchemaResolveCtx *args)
{
    for (unsigned long long at = 0ull; at < args->size;)
    {
        if ((args->text[at] == '%') && (at + 2u < args->size))
        {
            char hex[3] = {args->text[at + 1u], args->text[at + 2u], '\0'};
            char *end = NULL;
            const unsigned long value = strtoul(hex, &end, 16);
            const int spaced = (hex[0] == ' ') || ((hex[0] >= '\t') && (hex[0] <= '\r')) || (hex[0] == '-');
            if ((end == hex + 2) && !spaced)
            {
                mmgr_line_ch(args->out, (char)value);
                at += 3u;
                continue;
            }
        }
        mmgr_line_ch(args->out, args->text[at]);
        at += 1u;
    }
}

// the node at the JSON pointer `pointer` from `node`, CLI_JSON_NONE where none is there
EMBED_INLINE unsigned int resolve_pointer(const SchemaResolveCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned long long size = args->size;
    if (size == 0u)
    {
        return node;
    }
    if (args->pointer[0] != '/')
    {
        return CLI_JSON_NONE;
    }
    unsigned int at = node;
    for (unsigned long long start = 1ull; (at != CLI_JSON_NONE) && (start <= size);)
    {
        unsigned long long end = start;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->pointer, .at = end,
                         .size = size, .set = "/");
        char token[1u << 12u];
        VerbaLinea line = {token, sizeof(token), 0ull};
        for (unsigned long long one = start; one < end; one += 1u)
        {
            if ((args->pointer[one] == '~') && (one + 1u < end) &&
                ((args->pointer[one + 1u] == '0') || (args->pointer[one + 1u] == '1')))
            {
                mmgr_line_ch(&line, (args->pointer[one + 1u] == '1') ? '/' : '~');
                one += 1u;
                continue;
            }
            mmgr_line_ch(&line, args->pointer[one]);
        }
        if (mmgr_line_finish(&line) == 0u && (end > start))
        {
            return CLI_JSON_NONE;
        }
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = at,
                       .kind = CFG_JSON_OBJECT))
        {
            at = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = at,
                            .name = token);
        }
        else if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = at,
                            .kind = CFG_JSON_ARRAY))
        {
            char *stop = NULL;
            const unsigned long index = strtoul(token, &stop, 10);
            const int digits = (line.at > 0u) && (token[0] >= '0') && (token[0] <= '9') && (*stop == '\0') &&
                               !((line.at > 1u) && (token[0] == '0'));
            at = digits ? EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                     .array = at, .index = (unsigned int)index)
                        : CLI_JSON_NONE;
        }
        else
        {
            return CLI_JSON_NONE;
        }
        start = end + 1u;
    }
    return at;
}

// the schema in `node` whose `$anchor`, or whose `$id` or `id` of `#name`, is `name`, CLI_JSON_NONE where none is
static unsigned int resolve_anchor(const SchemaResolveCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const char *name = args->name;
    unsigned long long size = args->size;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = node,
                   .kind = CFG_JSON_OBJECT))
    {
        unsigned long long length = 0ull;
        const char *const anchor = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json,
                                              .map = node, .name = "$anchor", .size_pointer = &length);
        if ((anchor != NULL) && (length == size) &&
            (memor.cmp(&(MemoriaCfg){.src = anchor, .other = name, .bytes = (size_t)size}) == 0))
        {
            return node;
        }
        const char *id = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json, .map = node,
                                    .name = "$id", .size_pointer = &length);
        id = (id != NULL) ? id
                          : EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json, .map = node,
                                       .name = "id", .size_pointer = &length);
        if ((id != NULL) && (length == size + 1u) && (id[0] == '#') &&
            (memor.cmp(&(MemoriaCfg){.src = id + 1, .other = name, .bytes = (size_t)size}) == 0))
        {
            return node;
        }
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = node,
                    .kind = CFG_JSON_OBJECT) &&
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = node,
                    .kind = CFG_JSON_ARRAY))
    {
        return CLI_JSON_NONE;
    }
    for (unsigned int child = json->node[node].first; child != CLI_JSON_NONE; child = json->node[child].next)
    {
        const unsigned int found =
            EMBED_CALL(resolve_anchor, SchemaResolveCtx, .json = json, .node = child, .name = name, .size = size);
        if (found != CLI_JSON_NONE)
        {
            return found;
        }
    }
    return CLI_JSON_NONE;
}

// `target` read from `base` into `out`, sized for CLI_PATH_MAX_LEN: a URL as it stands, and otherwise beside the
// base, a URL's or a file's, its `.` and `..` read
static void resolve_join(const SchemaResolveCtx *args)
{
    unsigned long long size = args->size;
    VerbaLinea line = {args->out_capacity, CLI_PATH_MAX_LEN, 0ull};
    unsigned long long found = 0ull;
    if (EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->target, .length_value = size,
                   .required = "://", .size = 3u, .at_pointer = &found))
    {
        mmgr_line_put_n(&line, args->target, size);
        (void)mmgr_line_finish(&line);
        return;
    }
    const int url =
        (cellul.find(&(CatenaFinitaCfg){
             .src = args->base, .cap = CLI_CELLS_POOL, .other = "://", .other_cap = CLI_CELLS_POOL}) != NULL);
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->base,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}), .byte = '/');
    const char *const back =
        url ? NULL
            : EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = args->base,
                         .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}),
                         .byte = '\\');
    const char *const cut = ((back != NULL) && ((slash == NULL) || (back > slash))) ? back : slash;
    const char *rest = args->target;
    unsigned long long remainder_size = size;
    if (!url && EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = args->target, .size = size,
                           .prefix = "file://"))
    {
        rest += 7;
        remainder_size -= 7u;
    }
    const int absolute = !url && (remainder_size > 0u) &&
                         ((rest[0] == '/') || (rest[0] == '\\') || ((remainder_size > 1u) && (rest[1] == ':')));
    if (absolute)
    {
        mmgr_line_put_n(&line, rest, remainder_size);
        (void)mmgr_line_finish(&line);
        return;
    }
    mmgr_line_put_n(&line, args->base,
                    (cut != NULL)
                        ? (unsigned long long)(cut - args->base)
                        : (url ? cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}) : 0u));
    unsigned int parts = 0u;
    for (unsigned long long at = 0ull; at < line.at; at += 1u)
    {
        parts += (args->out_capacity[at] == '/') ? 1u : 0u;
    }
    for (unsigned long long start = 0ull; start <= remainder_size;)
    {
        unsigned long long end = start;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = rest, .at = start,
                         .size = remainder_size, .set = url ? "/" : "/\\");
        const unsigned long long part = end - start;
        if ((part == 2u) && (memor.cmp(&(MemoriaCfg){.src = rest + start, .other = "..", .bytes = 2u}) == 0))
        {
            if (!url || (parts + 1u > 3u))
            {
                line.at = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                     .text = args->out_capacity, .at = line.at, .set = "/\\");
                line.at -= (line.at > 0u) ? 1u : 0u;
                parts -= (parts > 0u) ? 1u : 0u;
            }
        }
        else if (!((part == 1u) && (rest[start] == '.')) && (url || (part > 0u)))
        {
            mmgr_line_put(&line, (url || (line.at > 0u)) ? "/" : "");
            mmgr_line_put_n(&line, rest + start, part);
            parts += 1u;
        }
        start = end + 1u;
    }
    (void)mmgr_line_finish(&line);
}

unsigned int cli_schema_resolve(const CliSchemaSizeCtx *args)
{
    unsigned long long hash = args->size;
    (void)EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = args->reference, .length_value = args->size,
                     .required = "#", .size = 1u, .at_pointer = &hash);
    VerbaLinea line = {args->key, CLI_PATH_MAX_LEN, 0ull};
    if (hash == 0u)
    {
        mmgr_line_put(&line, args->base);
        (void)mmgr_line_finish(&line);
    }
    else
    {
        EMBED_CALL(resolve_join, SchemaResolveCtx, .base = args->base, .target = args->reference, .size = hash,
                   .out_capacity = args->key);
    }
    const unsigned int doc =
        EMBED_CALL(cli_schema_store_internal.load, CliSchemaStoreCtx, .store = args->store, .key_read = args->key);
    if (doc == CLI_SCHEMA_NONE)
    {
        return CLI_SCHEMA_NONE;
    }
    const char *const fragment = (hash < args->size) ? args->reference + hash + 1u : "";
    const unsigned long long fragment_size = (hash < args->size) ? args->size - hash - 1u : 0u;
    if (fragment_size == 0u)
    {
        return doc;
    }
    if (fragment[0] == '/')
    {
        static char s_decoded[1u << 14u];
        VerbaLinea decoded = {s_decoded, sizeof(s_decoded), 0ull};
        EMBED_CALL(resolve_percent_decode, SchemaResolveCtx, .text = fragment, .size = fragment_size, .out = &decoded);
        const unsigned int found = EMBED_CALL(resolve_pointer, SchemaResolveCtx, .json = &args->store->json,
                                              .node = doc, .pointer = s_decoded, .size = decoded.at);
        return (found != CLI_JSON_NONE) ? found : CLI_SCHEMA_NONE;
    }
    const unsigned int found = EMBED_CALL(resolve_anchor, SchemaResolveCtx, .json = &args->store->json, .node = doc,
                                          .name = fragment, .size = fragment_size);
    return (found != CLI_JSON_NONE) ? found : CLI_SCHEMA_NONE;
}

int cli_schema_on_get(const CliSchemaReadCtx *args)
{
    int *named = args->named;
    unsigned long long *from = args->from;
    unsigned long long *to = args->to;
    *named = 0;
    const CliSchemaNode *const root =
        (args->read->root != CLI_SCHEMA_NONE) ? &args->read->node[args->read->root] : NULL;
    for (unsigned int at = ((root != NULL) && (root->kind == CLI_SCHEMA_OBJECT)) ? root->first : CLI_SCHEMA_NONE;
         at != CLI_SCHEMA_NONE; at = args->read->member[at].next)
    {
        const CliSchemaMember *const member = &args->read->member[at];
        if ((member->key_size != 7u) ||
            (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx,
                                                       .read = args->read, .at = member->key_at),
                                     .other = "$schema",
                                     .bytes = 7u}) != 0))
        {
            continue;
        }
        const CliSchemaNode *const value = (member->value != CLI_SCHEMA_NONE) ? &args->read->node[member->value] : NULL;
        if ((value == NULL) || (value->kind != CLI_SCHEMA_TEXT))
        {
            break;
        }
        const char *const text =
            EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = args->read, .at = value->text_at);
        unsigned long long found = 0ull;
        if (EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text, .length_value = value->text_size,
                       .required = "://", .size = 3u, .at_pointer = &found) &&
            !EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text, .size = value->text_size,
                        .prefix = "file://"))
        {
            VerbaLinea line = {args->key, CLI_PATH_MAX_LEN, 0ull};
            mmgr_line_put_n(&line, text, value->text_size);
            (void)mmgr_line_finish(&line);
        }
        else
        {
            EMBED_CALL(resolve_join, SchemaResolveCtx, .base = args->path, .target = text, .size = value->text_size,
                       .out_capacity = args->key);
        }
        *named = 1;
        *from = value->from;
        *to = value->to;
        return 1;
    }
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_end != 0ull) ? (args->path + name_end) : name;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        int same = (cellul.len(&(CatenaFinitaCfg){.src = s_known[at][0], .cap = CLI_CELLS_POOL}) ==
                    cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        same =
            same && cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_known[at][0], .cap = CLI_CELLS_POOL, .ci = 1});
        if (same)
        {
            VerbaLinea line = {args->key, CLI_PATH_MAX_LEN, 0ull};
            mmgr_line_put(&line, s_known[at][1]);
            (void)mmgr_line_finish(&line);
            return 1;
        }
    }
    return 0;
}
