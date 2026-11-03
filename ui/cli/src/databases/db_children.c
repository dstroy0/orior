// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_children.c: the schema a connection reads on its own link, a level at a time: the schemas or databases at the top,
// a schema's tables, views and routines under it, and a table's columns, indexes, keys and triggers under it, each as
// the catalogue of its database names them
#include "../text/text.h"
#include "db_internal.h"
#include "sql.h"

// The writing of one level: where it goes, how many nodes are written, how a row is written, the kind its rows are,
// NULL where each row names its own, the schema an object stands under, and whether the database is SQLite, whose
// objects are never leaves
typedef struct
{
    VerbaLinea *out;
    unsigned int written;
    unsigned int how;
    const char *kind;
    const char *schema;
    int lite;
} CliDbChildren;
// The reading of one level: the connection's kind, the path to it, and the writing of its nodes
typedef struct
{
    const char *kind;
    const char *const *path;
    unsigned int count;
    CliDbChildren children;
} CliDbLevel;
// how a row of a catalogue's answer is written: a schema or a database at the top; an object of a schema; a column of
// PostgreSQL's, MySQL's or SQLite's; an index, a key or a trigger with its definition; and SQLite's foreign key
enum
{
    CHILDREN_TOP,
    CHILDREN_OBJECT,
    CHILDREN_PG_COLUMN,
    CHILDREN_MY_COLUMN,
    CHILDREN_LITE_COLUMN,
    CHILDREN_PART,
    CHILDREN_LITE_KEY
};

void cli_db_quoted_name(const CliDbOutNameCtx *args)
{
    const char quote = (args->dialect == CLI_SQL_MYSQL) ? '`' : '"';
    mmgr_line_ch(args->out, quote);
    const char quoted[2] = {quote, '\0'};
    EMBED_CALL(cli_text_set_table.append_double, CliTextSetCfg, .out = args->out, .text = args->name, .set = quoted);
    mmgr_line_ch(args->out, quote);
}

void cli_db_literal(const CliDbOutCtx *args)
{
    if (args->dialect == CLI_SQL_MYSQL)
    {
        EMBED_CALL(cli_mysql_text.literal, CliMysqlTextCfg, .text = args->text, .out = args->out);
    }
    else
    {
        EMBED_CALL(cli_pg_text.literal, CliPgTextCfg, .text = args->text, .out = args->out);
    }
}

typedef struct
{
    const CliDbValue *values;
    unsigned int count;
    unsigned int at;
    VerbaLinea *out;
    const char *text;
    VerbaLinea *detail;
    const char *word;
    unsigned long long size;
    const char *kind;
    CliDbChildren *children;
    const char *label;
    const char *detail_text;
    int leaf;
    const char *const *at_letters;
    CliDbLevel *level;
    CliDbLink *link;
    const VerbaLinea *query_read;
    unsigned int how;
    CliDbFailure *failure;
    VerbaLinea *query;
    unsigned int dialect;
    const char *before;
    const char *first;
    const char *between;
    const char *second;
    const char *after;
} DbChildrenCtx;
// a value of a row as text, empty where it is NULL or past the row's end
EMBED_INLINE void children_text(const DbChildrenCtx *args)
{
    VerbaLinea *out = args->out;
    if ((args->at < args->count) && !args->values[args->at].null)
    {
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->values[args->at].text,
                   .length = args->values[args->at].size);
    }
}

// whether a value of a row is there and not NULL, and whether it is exactly `text`
EMBED_INLINE int children_is(const DbChildrenCtx *args)
{
    return (args->at < args->count) && !args->values[args->at].null;
}

EMBED_INLINE int children_equal(const DbChildrenCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    const unsigned long long have = children_is(args) ? args->values[args->at].size : 0ULL;
    return (have == size) &&
           ((size == 0u) ||
            (memor.cmp(&(MemoriaCfg){.src = args->values[args->at].text, .other = args->text, .bytes = (size_t)size}) ==
             0));
}

// a word of a column's detail written after those before it, where it is not empty
EMBED_INLINE void children_word(const DbChildrenCtx *args)
{
    if (args->size > 0u)
    {
        mmgr_line_put(args->detail, (args->detail->at > 0u) ? " " : "");
        mmgr_line_put_n(args->detail, args->word, args->size);
    }
}

// whether objects of `kind` contain columns and the rest under them
EMBED_INLINE int children_has_parts(const DbChildrenCtx *args)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "table", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "view", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "materialized view", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = args->kind, .other = "foreign table", .cap = CLI_CELLS_POOL}));
}

// a node written: its label, kind, detail, whether it is a leaf, and its path, `count` names
EMBED_INLINE void children_node(const DbChildrenCtx *args)
{
    VerbaLinea *const out = args->children->out;
    mmgr_line_put(out, (args->children->written > 0u) ? ",{\"label\":" : "{\"label\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->label,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->label, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"kind\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->kind,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->kind, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"detail\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->detail_text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->detail_text, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, args->leaf ? ",\"leaf\":true,\"path\":[" : ",\"leaf\":false,\"path\":[");
    for (unsigned int one = 0u; one < args->count; one += 1u)
    {
        mmgr_line_put(out, (one > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->at_letters[one],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->at_letters[one], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(out, "]}");
    args->children->written += 1u;
}

// a row of a catalogue's answer written as the level's node
static int children_row(const CliDbRowCfg *args)
{
    const CliDbValue *values = args->values;
    unsigned int count = args->count;
    CliDbChildren *const children = (CliDbChildren *)args->context;
    char first[CLI_PATH_MAX_LEN];
    char second[CLI_DB_FIELD];
    char detail[CLI_DB_FIELD * 2u];
    VerbaLinea first_line = {first, sizeof(first), 0ULL};
    VerbaLinea second_line = {second, sizeof(second), 0ULL};
    VerbaLinea detail_line = {detail, sizeof(detail), 0ULL};
    EMBED_CALL(children_text, DbChildrenCtx, .values = values, .count = count, .at = 0u, .out = &first_line);
    mmgr_line_finish(&first_line);
    EMBED_CALL(children_text, DbChildrenCtx, .values = values, .count = count, .at = 1u, .out = &second_line);
    mmgr_line_finish(&second_line);
    const unsigned int how = children->how;
    if (how == CHILDREN_TOP)
    {
        const char *const at[1] = {first};
        EMBED_CALL(children_node, DbChildrenCtx, .children = children, .label = first, .kind = children->kind,
                   .detail_text = "", .leaf = 0, .at_letters = at, .count = 1u);
        return 1;
    }
    if (how == CHILDREN_OBJECT)
    {
        const char *const at[3] = {children->schema, second, first};
        EMBED_CALL(children_node, DbChildrenCtx, .children = children, .label = first, .kind = second,
                   .detail_text = "",
                   .leaf = !children->lite && !EMBED_CALL(children_has_parts, DbChildrenCtx, .kind = second),
                   .at_letters = at, .count = 3u);
        return 1;
    }
    if ((how == CHILDREN_PART) || (how == CHILDREN_LITE_KEY))
    {
        if (how == CHILDREN_LITE_KEY)
        {
            char third[CLI_PATH_MAX_LEN];
            VerbaLinea third_line = {third, sizeof(third), 0ULL};
            EMBED_CALL(children_text, DbChildrenCtx, .values = values, .count = count, .at = 2u, .out = &third_line);
            mmgr_line_finish(&third_line);
            mmgr_line_put(&detail_line, "references ");
            mmgr_line_put(&detail_line, second);
            mmgr_line_ch(&detail_line, '(');
            mmgr_line_put(&detail_line, third);
            mmgr_line_ch(&detail_line, ')');
        }
        else if (children->kind == NULL)
        {
            // SQLite's indexes and triggers name their kind in the row, and their definition after it
            EMBED_CALL(children_text, DbChildrenCtx, .values = values, .count = count, .at = 2u, .out = &detail_line);
        }
        else
        {
            mmgr_line_put(&detail_line, second);
        }
        mmgr_line_finish(&detail_line);
        const char *const kind = (how == CHILDREN_LITE_KEY) ? "key"
                                 : (children->kind != NULL) ? children->kind
                                                            : second;
        EMBED_CALL(children_node, DbChildrenCtx, .children = children, .label = first, .kind = kind,
                   .detail_text = detail, .leaf = 1, .at_letters = NULL, .count = 0u);
        return 1;
    }
    // a column: its type, whether it may not be NULL, its default, and what else its database reports of it
    EMBED_CALL(children_word, DbChildrenCtx, .detail = &detail_line, .word = second,
               .size = cellul.len(&(CatenaFinitaCfg){.src = second, .cap = CLI_CELLS_POOL}));
    const int not_null =
        (how == CHILDREN_PG_COLUMN)
            ? EMBED_CALL(children_equal, DbChildrenCtx, .values = values, .count = count, .at = 2u, .text = "t")
        : (how == CHILDREN_MY_COLUMN)
            ? EMBED_CALL(children_equal, DbChildrenCtx, .values = values, .count = count, .at = 2u, .text = "NO")
            : EMBED_CALL(children_equal, DbChildrenCtx, .values = values, .count = count, .at = 2u, .text = "1");
    EMBED_CALL(children_word, DbChildrenCtx, .detail = &detail_line, .word = "not null",
               .size = not_null ? 8ULL : 0ULL);
    const int has_default = (how == CHILDREN_PG_COLUMN)
                                ? (EMBED_CALL(children_is, DbChildrenCtx, .values = values, .count = count, .at = 3u) &&
                                   (values[3].size > 0u))
                                : EMBED_CALL(children_is, DbChildrenCtx, .values = values, .count = count, .at = 3u);
    if (has_default)
    {
        char value[CLI_DB_FIELD];
        VerbaLinea value_line = {value, sizeof(value), 0ULL};
        mmgr_line_put(&value_line, "default ");
        EMBED_CALL(children_text, DbChildrenCtx, .values = values, .count = count, .at = 3u, .out = &value_line);
        EMBED_CALL(children_word, DbChildrenCtx, .detail = &detail_line, .word = value_line.out, .size = value_line.at);
    }
    if (how == CHILDREN_MY_COLUMN)
    {
        char extra[CLI_DB_FIELD];
        VerbaLinea extra_line = {extra, sizeof(extra), 0ULL};
        EMBED_CALL(children_text, DbChildrenCtx, .values = values, .count = count, .at = 4u, .out = &extra_line);
        EMBED_CALL(children_word, DbChildrenCtx, .detail = &detail_line, .word = extra_line.out, .size = extra_line.at);
    }
    if ((how == CHILDREN_LITE_COLUMN) &&
        !EMBED_CALL(children_equal, DbChildrenCtx, .values = values, .count = count, .at = 4u, .text = "0"))
    {
        EMBED_CALL(children_word, DbChildrenCtx, .detail = &detail_line, .word = "primary key", .size = 11ULL);
    }
    mmgr_line_finish(&detail_line);
    EMBED_CALL(children_node, DbChildrenCtx, .children = children, .label = first, .kind = "column",
               .detail_text = detail, .leaf = 1, .at_letters = NULL, .count = 0u);
    return 1;
}

// a question run against the catalogue, its rows written as `how`, of `kind` where each row does not name its own
EMBED_INLINE int children_query(const DbChildrenCtx *args)
{
    CliDbLink *link = args->link;
    unsigned int how = args->how;
    const char *kind = args->kind;
    CliDbFailure *failure = args->failure;
    args->level->children.how = how;
    args->level->children.kind = kind;
    return EMBED_CALL(cli_db_link_failure_statement_internal.link_all, CliDbLinkFailureStatementCtx, .link = link,
                      .statement = args->query_read->out, .size = args->query_read->at, .each_row = children_row,
                      .context = &args->level->children, .failure = failure);
}

// a question made of `before`, a literal, `between`, a literal, and `after`; the second literal left out where
// `between` is NULL
EMBED_INLINE void children_question(const DbChildrenCtx *args)
{
    unsigned int dialect = args->dialect;
    args->query->at = 0ULL;
    mmgr_line_put(args->query, args->before);
    EMBED_CALL(cli_db_out_internal.literal, CliDbOutCtx, .text = args->first, .dialect = dialect, .out = args->query);
    if (args->between != NULL)
    {
        mmgr_line_put(args->query, args->between);
        EMBED_CALL(cli_db_out_internal.literal, CliDbOutCtx, .text = args->second, .dialect = dialect,
                   .out = args->query);
    }
    mmgr_line_put(args->query, args->after);
}

// PostgreSQL's level under `path`
EMBED_INLINE int children_postgres(const DbChildrenCtx *args)
{
    const unsigned int dialect = CLI_SQL_POSTGRES;
    if (args->level->count == 0u)
    {
        mmgr_line_put(args->query, "SELECT nspname FROM pg_namespace WHERE nspname !~ '^pg_' AND nspname <> "
                                   "'information_schema' ORDER BY nspname");
        return EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                          .query_read = args->query, .how = CHILDREN_TOP, .kind = "schema", .failure = args->failure);
    }
    const char *const schema = args->level->path[0];
    if (args->level->count == 1u)
    {
        EMBED_CALL(
            children_question, DbChildrenCtx, .query = args->query, .dialect = dialect,
            .before =
                "SELECT name, kind FROM (SELECT c.relname AS name, CASE c.relkind WHEN 'r' THEN 'table' WHEN 'p' THEN "
                "'table' WHEN 'v' THEN 'view' WHEN 'm' THEN 'materialized view' WHEN 'f' THEN 'foreign table' ELSE "
                "'sequence' END AS kind FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace WHERE n.nspname "
                "= ",
            .first = schema,
            .between =
                " AND c.relkind IN ('r','p','v','m','f','S') AND NOT c.relispartition UNION ALL SELECT p.proname || "
                "'(' || "
                "pg_get_function_identity_arguments(p.oid) || ')', CASE p.prokind WHEN 'p' THEN 'procedure' ELSE "
                "'function' END FROM pg_proc p JOIN pg_namespace n ON n.oid = p.pronamespace WHERE n.nspname = ",
            .second = schema, .after = ") o ORDER BY kind, name");
        return EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                          .query_read = args->query, .how = CHILDREN_OBJECT, .kind = NULL, .failure = args->failure);
    }
    if (args->level->count != 3u)
    {
        return 1;
    }
    // the relation as a regclass, its schema's and its own names quoted and joined, as a literal
    char named[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea name_line = {named, sizeof(named), 0ULL};
    EMBED_CALL(cli_db_out_name_internal.quoted_name, CliDbOutNameCtx, .name = schema, .dialect = dialect,
               .out = &name_line);
    mmgr_line_ch(&name_line, '.');
    EMBED_CALL(cli_db_out_name_internal.quoted_name, CliDbOutNameCtx, .name = args->level->path[2], .dialect = dialect,
               .out = &name_line);
    mmgr_line_finish(&name_line);
    char relation[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea relation_line = {relation, sizeof(relation), 0ULL};
    EMBED_CALL(cli_db_out_internal.literal, CliDbOutCtx, .text = named, .dialect = dialect, .out = &relation_line);
    mmgr_line_put(&relation_line, "::regclass");
    mmgr_line_finish(&relation_line);
    static const char *const s_parts[4][3] = {
        {"SELECT a.attname, format_type(a.atttypid, a.atttypmod), a.attnotnull, pg_get_expr(d.adbin, d.adrelid) FROM "
         "pg_attribute a LEFT JOIN pg_attrdef d ON d.adrelid = a.attrelid AND d.adnum = a.attnum WHERE a.attrelid = ",
         " AND a.attnum > 0 AND NOT a.attisdropped ORDER BY a.attnum", "column"},
        {"SELECT i.relname, pg_get_indexdef(i.oid) FROM pg_index x JOIN pg_class i ON i.oid = x.indexrelid WHERE "
         "x.indrelid = ",
         " ORDER BY 1", "index"},
        {"SELECT conname, pg_get_constraintdef(oid) FROM pg_constraint WHERE conrelid = ", " ORDER BY 1", "key"},
        {"SELECT tgname, pg_get_triggerdef(oid) FROM pg_trigger WHERE tgrelid = ", " AND NOT tgisinternal ORDER BY 1",
         "trigger"}};
    for (unsigned int part = 0u; part < 4u; part += 1u)
    {
        args->query->at = 0ULL;
        mmgr_line_put(args->query, s_parts[part][0]);
        mmgr_line_put(args->query, relation);
        mmgr_line_put(args->query, s_parts[part][1]);
        if (!EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                        .query_read = args->query, .how = (part == 0u) ? CHILDREN_PG_COLUMN : CHILDREN_PART,
                        .kind = s_parts[part][2], .failure = args->failure))
        {
            return 0;
        }
    }
    return 1;
}

// MySQL's level under `path`
EMBED_INLINE int children_mysql(const DbChildrenCtx *args)
{
    const unsigned int dialect = CLI_SQL_MYSQL;
    if (args->level->count == 0u)
    {
        mmgr_line_put(args->query, "SELECT SCHEMA_NAME FROM information_schema.SCHEMATA ORDER BY SCHEMA_NAME");
        return EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                          .query_read = args->query, .how = CHILDREN_TOP, .kind = "database", .failure = args->failure);
    }
    const char *const schema = args->level->path[0];
    if (args->level->count == 1u)
    {
        EMBED_CALL(
            children_question, DbChildrenCtx, .query = args->query, .dialect = dialect,
            .before = "SELECT name, kind FROM (SELECT TABLE_NAME AS name, IF(TABLE_TYPE = 'VIEW', 'view', 'table') "
                      "AS kind FROM information_schema.TABLES WHERE TABLE_SCHEMA = ",
            .first = schema,
            .between = " UNION ALL SELECT ROUTINE_NAME, LOWER(ROUTINE_TYPE) FROM information_schema.ROUTINES WHERE "
                       "ROUTINE_SCHEMA = ",
            .second = schema, .after = ") o ORDER BY kind, name");
        return EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                          .query_read = args->query, .how = CHILDREN_OBJECT, .kind = NULL, .failure = args->failure);
    }
    if (args->level->count != 3u)
    {
        return 1;
    }
    const char *const table = args->level->path[2];
    static const char *const s_parts[4][4] = {
        {"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, EXTRA FROM information_schema.COLUMNS WHERE "
         "TABLE_SCHEMA = ",
         " AND TABLE_NAME = ", " ORDER BY ORDINAL_POSITION", "column"},
        {"SELECT INDEX_NAME, CONCAT(IF(MAX(NON_UNIQUE) = 0, 'unique ', ''), '(', GROUP_CONCAT(COLUMN_NAME ORDER BY "
         "SEQ_IN_INDEX SEPARATOR ', '), ')') FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = ",
         " AND TABLE_NAME = ", " GROUP BY INDEX_NAME ORDER BY INDEX_NAME", "index"},
        {"SELECT CONSTRAINT_NAME, CONCAT('(', GROUP_CONCAT(COLUMN_NAME ORDER BY ORDINAL_POSITION SEPARATOR ', '), ') "
         "references ', REFERENCED_TABLE_NAME) FROM information_schema.KEY_COLUMN_USAGE WHERE TABLE_SCHEMA = ",
         " AND TABLE_NAME = ",
         " AND REFERENCED_TABLE_NAME IS NOT NULL GROUP BY CONSTRAINT_NAME, REFERENCED_TABLE_NAME ORDER BY "
         "CONSTRAINT_NAME",
         "key"},
        {"SELECT TRIGGER_NAME, CONCAT(ACTION_TIMING, ' ', EVENT_MANIPULATION) FROM information_schema.TRIGGERS WHERE "
         "EVENT_OBJECT_SCHEMA = ",
         " AND EVENT_OBJECT_TABLE = ", " ORDER BY TRIGGER_NAME", "trigger"}};
    for (unsigned int part = 0u; part < 4u; part += 1u)
    {
        EMBED_CALL(children_question, DbChildrenCtx, .query = args->query, .dialect = dialect,
                   .before = s_parts[part][0], .first = schema, .between = s_parts[part][1], .second = table,
                   .after = s_parts[part][2]);
        if (!EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                        .query_read = args->query, .how = (part == 0u) ? CHILDREN_MY_COLUMN : CHILDREN_PART,
                        .kind = s_parts[part][3], .failure = args->failure))
        {
            return 0;
        }
    }
    return 1;
}

// SQLite's level under `path`
EMBED_INLINE int children_sqlite(const DbChildrenCtx *args)
{
    const unsigned int dialect = CLI_SQL_SQLITE;
    args->level->children.lite = 1;
    if (args->level->count == 0u)
    {
        mmgr_line_put(args->query, "SELECT name FROM pragma_database_list ORDER BY seq");
        return EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                          .query_read = args->query, .how = CHILDREN_TOP, .kind = "database", .failure = args->failure);
    }
    const char *const schema = args->level->path[0];
    char master[CLI_PATH_MAX_LEN];
    VerbaLinea master_line = {master, sizeof(master), 0ULL};
    EMBED_CALL(cli_db_out_name_internal.quoted_name, CliDbOutNameCtx, .name = schema, .dialect = dialect,
               .out = &master_line);
    mmgr_line_put(&master_line, ".sqlite_master");
    mmgr_line_finish(&master_line);
    if (args->level->count == 1u)
    {
        mmgr_line_put(args->query, "SELECT name, type FROM ");
        mmgr_line_put(args->query, master);
        mmgr_line_put(args->query,
                      " WHERE type IN ('table', 'view') AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\' ORDER BY "
                      "type, name");
        return EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                          .query_read = args->query, .how = CHILDREN_OBJECT, .kind = NULL, .failure = args->failure);
    }
    if (args->level->count != 3u)
    {
        return 1;
    }
    const char *const table = args->level->path[2];
    EMBED_CALL(children_question, DbChildrenCtx, .query = args->query, .dialect = dialect,
               .before = "SELECT name, type, \"notnull\", dflt_value, pk FROM pragma_table_xinfo(", .first = table,
               .between = ", ", .second = schema, .after = ") ORDER BY cid");
    if (!EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link, .query_read = args->query,
                    .how = CHILDREN_LITE_COLUMN, .kind = "column", .failure = args->failure))
    {
        return 0;
    }
    args->query->at = 0ULL;
    mmgr_line_put(args->query, "SELECT name, type, sql FROM ");
    mmgr_line_put(args->query, master);
    mmgr_line_put(args->query, " WHERE type IN ('index', 'trigger') AND tbl_name = ");
    EMBED_CALL(cli_db_out_internal.literal, CliDbOutCtx, .text = table, .dialect = dialect, .out = args->query);
    mmgr_line_put(args->query, " ORDER BY type, name");
    if (!EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link, .query_read = args->query,
                    .how = CHILDREN_PART, .kind = NULL, .failure = args->failure))
    {
        return 0;
    }
    EMBED_CALL(children_question, DbChildrenCtx, .query = args->query, .dialect = dialect,
               .before = "SELECT \"from\", \"table\", \"to\" FROM pragma_foreign_key_list(", .first = table,
               .between = ", ", .second = schema, .after = ")");
    return EMBED_CALL(children_query, DbChildrenCtx, .level = args->level, .link = args->link,
                      .query_read = args->query, .how = CHILDREN_LITE_KEY, .kind = "key", .failure = args->failure);
}

static int children_work(const CliEachLinkFailureCfg *args)
{
    CliDbLink *link = args->link;
    CliDbFailure *failure = args->failure;
    CliDbLevel *const level = (CliDbLevel *)args->context;
    char question[CLI_PATH_MAX_LEN * 8u];
    VerbaLinea query = {question, sizeof(question), 0ULL};
    if (cellul.eq(&(CatenaFinitaCfg){.src = level->kind, .other = "postgres", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(children_postgres, DbChildrenCtx, .level = level, .link = link, .query = &query,
                          .failure = failure);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = level->kind, .other = "mysql", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(children_mysql, DbChildrenCtx, .level = level, .link = link, .query = &query,
                          .failure = failure);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = level->kind, .other = "sqlite", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(children_sqlite, DbChildrenCtx, .level = level, .link = link, .query = &query,
                          .failure = failure);
    }
    return 1;
}

int cli_db_children(const CliDbOutCountCfg *args)
{
    CliDbLevel level = {args->open->config.kind,
                        args->path_pointer,
                        args->count,
                        {args->out, 0u, CHILDREN_TOP, NULL, (args->count > 0u) ? args->path_pointer[0] : "", 0}};
    const unsigned long long from = args->out->at;
    mmgr_line_ch(args->out, '[');
    if (!EMBED_CALL(cli_db_failure_context_internal.with_meta, CliDbFailureContextCtx, .open = args->open,
                    .work = children_work, .context = &level, .failure = args->failure))
    {
        args->out->at = from;
        return 0;
    }
    mmgr_line_ch(args->out, ']');
    return 1;
}
