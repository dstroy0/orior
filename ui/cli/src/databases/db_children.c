// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_children.c: the schema a connection reads on its own link, a level at a time: the schemas or databases at the top,
// a schema's tables, views and routines under it, and a table's columns, indexes, keys and triggers under it, each as
// the catalogue of its database names them
#include "../text/text.h"
#include "db_internal.h"
#include "sql.h"

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

void cli_db_quoted_name(const char *name, unsigned int dialect, VerbaLinea *out)
{
    const char quote = (dialect == CLI_SQL_MYSQL) ? '`' : '"';
    mmgr_line_ch(out, quote);
    const char quoted[2] = {quote, '\0'};
    cli_text_put_doubled(out, name, quoted);
    mmgr_line_ch(out, quote);
}

void cli_db_literal(const char *text, unsigned int dialect, VerbaLinea *out)
{
    if (dialect == CLI_SQL_MYSQL)
    {
        cli_mysql_literal(text, out);
    }
    else
    {
        cli_pg_literal(text, out);
    }
}

// a value of a row as text, empty where it is NULL or past the row's end
static void children_text(const CliDbValue *values, unsigned int count, unsigned int at, VerbaLinea *out)
{
    if ((at < count) && !values[at].null)
    {
        cli_lossy(out, (const unsigned char *)values[at].text, values[at].size);
    }
}

// whether a value of a row is there and not NULL, and whether it is exactly `text`
static int children_is(const CliDbValue *values, unsigned int count, unsigned int at)
{
    return (at < count) && !values[at].null;
}

static int children_equal(const CliDbValue *values, unsigned int count, unsigned int at, const char *text)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    const unsigned long long have = children_is(values, count, at) ? values[at].size : 0ull;
    return (have == size) &&
           ((size == 0u) ||
            (memor.cmp(&(MemoriaCfg){.src = values[at].text, .other = text, .bytes = (size_t)size}) == 0));
}

// a word of a column's detail written after those before it, where it is not empty
static void children_word(VerbaLinea *detail, const char *word, unsigned long long size)
{
    if (size > 0u)
    {
        mmgr_line_put(detail, (detail->at > 0u) ? " " : "");
        mmgr_line_put_n(detail, word, size);
    }
}

// whether objects of `kind` contain columns and the rest under them
static int children_has_parts(const char *kind)
{
    return (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "table", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "view", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "materialized view", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "foreign table", .cap = CLI_CELLS_POOL}));
}

// a node written: its label, kind, detail, whether it is a leaf, and its path, `count` names
static void children_node(CliDbChildren *children, const char *label, const char *kind, const char *detail, int leaf,
                          const char *const *at, unsigned int count)
{
    VerbaLinea *const out = children->out;
    mmgr_line_put(out, (children->written > 0u) ? ",{\"label\":" : "{\"label\":");
    cli_json_quoted_lossy(out, (const unsigned char *)label,
                          cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"kind\":");
    cli_json_quoted_lossy(out, (const unsigned char *)kind,
                          cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"detail\":");
    cli_json_quoted_lossy(out, (const unsigned char *)detail,
                          cellul.len(&(CatenaFinitaCfg){.src = detail, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, leaf ? ",\"leaf\":true,\"path\":[" : ",\"leaf\":false,\"path\":[");
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        mmgr_line_put(out, (one > 0u) ? "," : "");
        cli_json_quoted_lossy(out, (const unsigned char *)at[one],
                              cellul.len(&(CatenaFinitaCfg){.src = at[one], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(out, "]}");
    children->written += 1u;
}

// a row of a catalogue's answer written as the level's node
static int children_row(void *context, const CliDbValue *values, unsigned int count)
{
    CliDbChildren *const children = (CliDbChildren *)context;
    char first[CLI_PATH_MAX_LEN];
    char second[CLI_DB_FIELD];
    char detail[CLI_DB_FIELD * 2u];
    VerbaLinea first_line = {first, sizeof(first), 0ull};
    VerbaLinea second_line = {second, sizeof(second), 0ull};
    VerbaLinea detail_line = {detail, sizeof(detail), 0ull};
    children_text(values, count, 0u, &first_line);
    mmgr_line_finish(&first_line);
    children_text(values, count, 1u, &second_line);
    mmgr_line_finish(&second_line);
    const unsigned int how = children->how;
    if (how == CHILDREN_TOP)
    {
        const char *const at[1] = {first};
        children_node(children, first, children->kind, "", 0, at, 1u);
        return 1;
    }
    if (how == CHILDREN_OBJECT)
    {
        const char *const at[3] = {children->schema, second, first};
        children_node(children, first, second, "", !children->lite && !children_has_parts(second), at, 3u);
        return 1;
    }
    if ((how == CHILDREN_PART) || (how == CHILDREN_LITE_KEY))
    {
        if (how == CHILDREN_LITE_KEY)
        {
            char third[CLI_PATH_MAX_LEN];
            VerbaLinea third_line = {third, sizeof(third), 0ull};
            children_text(values, count, 2u, &third_line);
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
            children_text(values, count, 2u, &detail_line);
        }
        else
        {
            mmgr_line_put(&detail_line, second);
        }
        mmgr_line_finish(&detail_line);
        const char *const kind = (how == CHILDREN_LITE_KEY) ? "key"
                                 : (children->kind != NULL) ? children->kind
                                                            : second;
        children_node(children, first, kind, detail, 1, NULL, 0u);
        return 1;
    }
    // a column: its type, whether it may not be NULL, its default, and what else its database reports of it
    children_word(&detail_line, second, cellul.len(&(CatenaFinitaCfg){.src = second, .cap = CLI_CELLS_POOL}));
    const int not_null = (how == CHILDREN_PG_COLUMN)   ? children_equal(values, count, 2u, "t")
                         : (how == CHILDREN_MY_COLUMN) ? children_equal(values, count, 2u, "NO")
                                                       : children_equal(values, count, 2u, "1");
    children_word(&detail_line, "not null", not_null ? 8ull : 0ull);
    const int has_default = (how == CHILDREN_PG_COLUMN) ? (children_is(values, count, 3u) && (values[3].size > 0u))
                                                        : children_is(values, count, 3u);
    if (has_default)
    {
        char value[CLI_DB_FIELD];
        VerbaLinea value_line = {value, sizeof(value), 0ull};
        mmgr_line_put(&value_line, "default ");
        children_text(values, count, 3u, &value_line);
        children_word(&detail_line, value_line.out, value_line.at);
    }
    if (how == CHILDREN_MY_COLUMN)
    {
        char extra[CLI_DB_FIELD];
        VerbaLinea extra_line = {extra, sizeof(extra), 0ull};
        children_text(values, count, 4u, &extra_line);
        children_word(&detail_line, extra_line.out, extra_line.at);
    }
    if ((how == CHILDREN_LITE_COLUMN) && !children_equal(values, count, 4u, "0"))
    {
        children_word(&detail_line, "primary key", 11ull);
    }
    mmgr_line_finish(&detail_line);
    children_node(children, first, "column", detail, 1, NULL, 0u);
    return 1;
}

// The reading of one level: the connection's kind, the path to it, and the writing of its nodes
typedef struct
{
    const char *kind;
    const char *const *path;
    unsigned int count;
    CliDbChildren children;
} CliDbLevel;

// a question run against the catalogue, its rows written as `how`, of `kind` where each row does not name its own
static int children_query(CliDbLevel *level, CliDbLink *link, const VerbaLinea *query, unsigned int how,
                          const char *kind, CliDbFailure *failure)
{
    level->children.how = how;
    level->children.kind = kind;
    return cli_db_link_all(link, query->out, query->at, children_row, &level->children, failure);
}

// a question made of `before`, a literal, `between`, a literal, and `after`; the second literal left out where
// `between` is NULL
static void children_question(VerbaLinea *query, unsigned int dialect, const char *before, const char *first,
                              const char *between, const char *second, const char *after)
{
    query->at = 0ull;
    mmgr_line_put(query, before);
    cli_db_literal(first, dialect, query);
    if (between != NULL)
    {
        mmgr_line_put(query, between);
        cli_db_literal(second, dialect, query);
    }
    mmgr_line_put(query, after);
}

// PostgreSQL's level under `path`
static int children_postgres(CliDbLevel *level, CliDbLink *link, VerbaLinea *query, CliDbFailure *failure)
{
    const unsigned int dialect = CLI_SQL_POSTGRES;
    if (level->count == 0u)
    {
        mmgr_line_put(query, "SELECT nspname FROM pg_namespace WHERE nspname !~ '^pg_' AND nspname <> "
                             "'information_schema' ORDER BY nspname");
        return children_query(level, link, query, CHILDREN_TOP, "schema", failure);
    }
    const char *const schema = level->path[0];
    if (level->count == 1u)
    {
        children_question(
            query, dialect,
            "SELECT name, kind FROM (SELECT c.relname AS name, CASE c.relkind WHEN 'r' THEN 'table' WHEN 'p' THEN "
            "'table' WHEN 'v' THEN 'view' WHEN 'm' THEN 'materialized view' WHEN 'f' THEN 'foreign table' ELSE "
            "'sequence' END AS kind FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace WHERE n.nspname = ",
            schema,
            " AND c.relkind IN ('r','p','v','m','f','S') AND NOT c.relispartition UNION ALL SELECT p.proname || '(' || "
            "pg_get_function_identity_arguments(p.oid) || ')', CASE p.prokind WHEN 'p' THEN 'procedure' ELSE "
            "'function' END FROM pg_proc p JOIN pg_namespace n ON n.oid = p.pronamespace WHERE n.nspname = ",
            schema, ") o ORDER BY kind, name");
        return children_query(level, link, query, CHILDREN_OBJECT, NULL, failure);
    }
    if (level->count != 3u)
    {
        return 1;
    }
    // the relation as a regclass, its schema's and its own names quoted and joined, as a literal
    char named[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea name_line = {named, sizeof(named), 0ull};
    cli_db_quoted_name(schema, dialect, &name_line);
    mmgr_line_ch(&name_line, '.');
    cli_db_quoted_name(level->path[2], dialect, &name_line);
    mmgr_line_finish(&name_line);
    char relation[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea relation_line = {relation, sizeof(relation), 0ull};
    cli_db_literal(named, dialect, &relation_line);
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
        query->at = 0ull;
        mmgr_line_put(query, s_parts[part][0]);
        mmgr_line_put(query, relation);
        mmgr_line_put(query, s_parts[part][1]);
        if (!children_query(level, link, query, (part == 0u) ? CHILDREN_PG_COLUMN : CHILDREN_PART, s_parts[part][2],
                            failure))
        {
            return 0;
        }
    }
    return 1;
}

// MySQL's level under `path`
static int children_mysql(CliDbLevel *level, CliDbLink *link, VerbaLinea *query, CliDbFailure *failure)
{
    const unsigned int dialect = CLI_SQL_MYSQL;
    if (level->count == 0u)
    {
        mmgr_line_put(query, "SELECT SCHEMA_NAME FROM information_schema.SCHEMATA ORDER BY SCHEMA_NAME");
        return children_query(level, link, query, CHILDREN_TOP, "database", failure);
    }
    const char *const schema = level->path[0];
    if (level->count == 1u)
    {
        children_question(query, dialect,
                          "SELECT name, kind FROM (SELECT TABLE_NAME AS name, IF(TABLE_TYPE = 'VIEW', 'view', 'table') "
                          "AS kind FROM information_schema.TABLES WHERE TABLE_SCHEMA = ",
                          schema,
                          " UNION ALL SELECT ROUTINE_NAME, LOWER(ROUTINE_TYPE) FROM information_schema.ROUTINES WHERE "
                          "ROUTINE_SCHEMA = ",
                          schema, ") o ORDER BY kind, name");
        return children_query(level, link, query, CHILDREN_OBJECT, NULL, failure);
    }
    if (level->count != 3u)
    {
        return 1;
    }
    const char *const table = level->path[2];
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
        children_question(query, dialect, s_parts[part][0], schema, s_parts[part][1], table, s_parts[part][2]);
        if (!children_query(level, link, query, (part == 0u) ? CHILDREN_MY_COLUMN : CHILDREN_PART, s_parts[part][3],
                            failure))
        {
            return 0;
        }
    }
    return 1;
}

// SQLite's level under `path`
static int children_sqlite(CliDbLevel *level, CliDbLink *link, VerbaLinea *query, CliDbFailure *failure)
{
    const unsigned int dialect = CLI_SQL_SQLITE;
    level->children.lite = 1;
    if (level->count == 0u)
    {
        mmgr_line_put(query, "SELECT name FROM pragma_database_list ORDER BY seq");
        return children_query(level, link, query, CHILDREN_TOP, "database", failure);
    }
    const char *const schema = level->path[0];
    char master[CLI_PATH_MAX_LEN];
    VerbaLinea master_line = {master, sizeof(master), 0ull};
    cli_db_quoted_name(schema, dialect, &master_line);
    mmgr_line_put(&master_line, ".sqlite_master");
    mmgr_line_finish(&master_line);
    if (level->count == 1u)
    {
        mmgr_line_put(query, "SELECT name, type FROM ");
        mmgr_line_put(query, master);
        mmgr_line_put(query, " WHERE type IN ('table', 'view') AND name NOT LIKE 'sqlite\\_%' ESCAPE '\\' ORDER BY "
                             "type, name");
        return children_query(level, link, query, CHILDREN_OBJECT, NULL, failure);
    }
    if (level->count != 3u)
    {
        return 1;
    }
    const char *const table = level->path[2];
    children_question(query, dialect, "SELECT name, type, \"notnull\", dflt_value, pk FROM pragma_table_xinfo(", table,
                      ", ", schema, ") ORDER BY cid");
    if (!children_query(level, link, query, CHILDREN_LITE_COLUMN, "column", failure))
    {
        return 0;
    }
    query->at = 0ull;
    mmgr_line_put(query, "SELECT name, type, sql FROM ");
    mmgr_line_put(query, master);
    mmgr_line_put(query, " WHERE type IN ('index', 'trigger') AND tbl_name = ");
    cli_db_literal(table, dialect, query);
    mmgr_line_put(query, " ORDER BY type, name");
    if (!children_query(level, link, query, CHILDREN_PART, NULL, failure))
    {
        return 0;
    }
    children_question(query, dialect, "SELECT \"from\", \"table\", \"to\" FROM pragma_foreign_key_list(", table, ", ",
                      schema, ")");
    return children_query(level, link, query, CHILDREN_LITE_KEY, "key", failure);
}

static int children_work(void *context, CliDbLink *link, CliDbFailure *failure)
{
    CliDbLevel *const level = (CliDbLevel *)context;
    char question[CLI_PATH_MAX_LEN * 8u];
    VerbaLinea query = {question, sizeof(question), 0ull};
    if (cellul.eq(&(CatenaFinitaCfg){.src = level->kind, .other = "postgres", .cap = CLI_CELLS_POOL}))
    {
        return children_postgres(level, link, &query, failure);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = level->kind, .other = "mysql", .cap = CLI_CELLS_POOL}))
    {
        return children_mysql(level, link, &query, failure);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = level->kind, .other = "sqlite", .cap = CLI_CELLS_POOL}))
    {
        return children_sqlite(level, link, &query, failure);
    }
    return 1;
}

int cli_db_children(CliDbOpen *open, const char *const *path, unsigned int count, VerbaLinea *out,
                    CliDbFailure *failure)
{
    CliDbLevel level = {open->config.kind, path, count, {out, 0u, CHILDREN_TOP, NULL, (count > 0u) ? path[0] : "", 0}};
    const unsigned long long from = out->at;
    mmgr_line_ch(out, '[');
    if (!cli_db_with_meta(open, children_work, &level, failure))
    {
        out->at = from;
        return 0;
    }
    mmgr_line_ch(out, ']');
    return 1;
}
