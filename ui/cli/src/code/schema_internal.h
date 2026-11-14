// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_internal.h: what the schemas' reader shares: a JSON file read with the place of each of its values and what
// prevented it from reading; the schemas a check reaches, each by its URL or its path, stored as JSON trees; and the
// check of a file's values against them (schema.c, schema_read.c, schema_bundled.c, schema_store.c, schema_resolve.c,
// schema_check.c, schema_values.c, schema_bounds.c, schema_array.c, schema_object.c, schema_complete.c,
// schema_offer.c)
#ifndef SCHEMA_INTERNAL_H
#define SCHEMA_INTERNAL_H
#include "../builds/builds.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "embed_dispatch_layout.h"
#include "schema.h"

typedef struct CliSchemaBaseCtx CliSchemaBaseCtx;
typedef struct CliSchemaBaseMapCtx CliSchemaBaseMapCtx;
typedef struct CliSchemaReadCtx CliSchemaReadCtx;
typedef struct CliSchemaBaseNodeCtx CliSchemaBaseNodeCtx;
typedef struct CliSchemaSizeCtx CliSchemaSizeCtx;
typedef struct CliSchemaJsonCtx CliSchemaJsonCtx;
typedef struct CliSchemaTextCtx CliSchemaTextCtx;
typedef struct CliSchemaReadJsonCtx CliSchemaReadJsonCtx;
typedef struct CliSchemaJsonOutCtx CliSchemaJsonOutCtx;
typedef struct CliSchemaSizeOutCtx CliSchemaSizeOutCtx;
typedef struct CliSchemaJsonNodeCtx CliSchemaJsonNodeCtx;
typedef struct CliSchemaTextBaseCtx CliSchemaTextBaseCtx;
typedef struct CliSchemaReadTextCtx CliSchemaReadTextCtx;
typedef struct CliSchemaReadOneCtx CliSchemaReadOneCtx;
typedef struct CliSchemaJsonValueCtx CliSchemaJsonValueCtx;
typedef struct CliSchemaReadNodeCtx CliSchemaReadNodeCtx;
typedef struct CliSchemaJsonNodeValueU64sCtx CliSchemaJsonNodeValueU64sCtx;
typedef struct CliSchemaUrlCtx CliSchemaUrlCtx;
typedef struct CliSchemaStoreCtx CliSchemaStoreCtx;
typedef struct CliSchemaOutCtx CliSchemaOutCtx;
typedef struct CliSchemaReadAtCtx CliSchemaReadAtCtx;
typedef struct CliSchemaCtx CliSchemaCtx;

// what a value of a JSON file is
#define CLI_SCHEMA_OBJECT 1u
#define CLI_SCHEMA_ARRAY 2u
#define CLI_SCHEMA_TEXT 3u
#define CLI_SCHEMA_NUMBER 4u
#define CLI_SCHEMA_BOOL 5u
#define CLI_SCHEMA_NULL 6u

// the most values of a file, members, things that prevented it from reading, the bytes their texts take, and none
#define CLI_SCHEMA_NODES (1u << 20u)
#define CLI_SCHEMA_MEMBERS (1u << 20u)
#define CLI_SCHEMA_ERRORS 4096u
#define CLI_SCHEMA_POOL (1u << 23u)
#define CLI_SCHEMA_NONE 0xffffffffu

// A value of a JSON file and its span: its type, a string's letters in the pool, a number's value, a boolean's truth;
// an object's first member or an array's first item, and how many; and the next item of its array
typedef struct
{
    unsigned char type;
    int truth;
    double number;
    unsigned long long from;
    unsigned long long to;
    unsigned int text_at;
    unsigned int text_size;
    unsigned int first;
    unsigned int count;
    unsigned int next;
} CliSchemaNode;

// A key of an object, its span with its quotes, its value where one is written, and the next member
typedef struct
{
    unsigned int key_at;
    unsigned int key_size;
    unsigned long long from;
    unsigned long long to;
    unsigned int value;
    unsigned int next;
} CliSchemaMember;

// What prevented a JSON file from reading: its span and what it reports, in the pool
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned int message_at;
    unsigned int message_size;
} CliSchemaError;

// A JSON file as read: its text, its values and members, what prevented it from reading, its root, CLI_SCHEMA_NONE
// where none was read, the texts of all, and 1 in `full` where a part of it did not fit. The values, the members and
// the texts stand in one cell, `nodes_max`, `members_max` and `pool_max` places sized from the text, to
// CLI_SCHEMA_NODES, CLI_SCHEMA_MEMBERS and CLI_SCHEMA_POOL at the most; the values' capacity holds a value for each
// member and CLI_SCHEMA_CHECK_NODES past those read, for the keys a check makes values of
typedef struct
{
    const char *text;
    unsigned long long size;
    CliSchemaNode *node;
    unsigned int nodes;
    unsigned int nodes_max;
    CliSchemaMember *member;
    unsigned int members;
    unsigned int members_max;
    CliSchemaError error[CLI_SCHEMA_ERRORS];
    unsigned int errors;
    unsigned int root;
    char *pool;
    unsigned int used;
    unsigned int pool_max;
    int full;
} CliSchemaRead;

// the places a read's lists are taken with, and the values over the read's own a check may make
#define CLI_SCHEMA_CHECK_NODES 1024u

// a read's cell given back (schema.c)

// `size` bytes of a JSON file read into `read`, comments passed over (schema_read.c); a text of the pool; and the type
// a value is of as a schema names it, `integer` for a number with no fraction (schema_values.c)
void cli_schema_read(const CliSchemaReadTextCtx *args);
const char *cli_schema_text(const CliSchemaReadAtCtx *args);
const char *cli_schema_type(const CliSchemaCtx *args);

// the meta-schemas impero bundles, each its URL with no scheme and no `#` and its text (schema_bundled.c)
extern const char *const g_cli_schema_bundled[5][2];

// the most documents a check reaches, those it could not find, the values and bytes their tree is taken with, and the
// deepest a value is checked
#define CLI_SCHEMA_DOCUMENTS 32u
#define CLI_SCHEMA_MISSING 64u
#define CLI_SCHEMA_STORE_NODES_CAPACITY (1u << 18u)
#define CLI_SCHEMA_STORE_TEXT_CAPACITY (16ull << 20u)
#define CLI_SCHEMA_DEEPEST 64u

// The schemas a check reaches, each by its URL or its path, read into one tree in a cell of its own, and those it could
// not find
typedef struct
{
    CliJson json;
    char key[CLI_SCHEMA_DOCUMENTS][CLI_PATH_MAX_LEN];
    unsigned int root[CLI_SCHEMA_DOCUMENTS];
    unsigned int documents;
    char missing[CLI_SCHEMA_MISSING][CLI_PATH_MAX_LEN];
    unsigned int missings;
} CliSchemaStore;

// the cells the store occupies while it reads a document; the store made empty; the document at `key` read where it was
// not, a bundled meta-schema, a file, or a schema stored from an earlier fetch: its root, CLI_SCHEMA_NONE where none
// could be read; and the file a schema fetched from `url` is stored in, into `out`: 1, or 0 where impero has no
void cli_schema_store_begin(const CliSchemaStoreCtx *args);
unsigned int cli_schema_load(const CliSchemaStoreCtx *args);
int cli_schema_cache_path(const CliSchemaUrlCtx *args);
// the schema `size` bytes of `reference` name from the document at `base`, and that document's key into `key`, which
// is sized for CLI_PATH_MAX_LEN: its node, or CLI_SCHEMA_NONE; and where a file's schema is, its `$schema` read beside
// it where it is a path, or the one its name is known by, into `key`: 1 with the span of the `$schema` value where it
// names one, `named` 0 where it is known by its name, or 0 where it has none (schema_resolve.c)
unsigned int cli_schema_resolve(const CliSchemaSizeCtx *args);
int cli_schema_on_get(const CliSchemaReadCtx *args);

// a member of a schema object, CLI_JSON_NONE where it has none or is no object; a schema's string member, NULL where
// it has none; whether a schema node is `true`, and `false`; a schema number's value into `value`: 1, or 0 where it is
// no number; and whether a value of the file contains the same as one of a schema, a number by its value, and two
// values of the file (schema_values.c)
unsigned int cli_schema_get(const CliSchemaJsonCtx *args);
const char *cli_schema_string(const CliSchemaJsonCtx *args);
int cli_schema_true(const CliSchemaJsonNodeCtx *args);
int cli_schema_false(const CliSchemaJsonNodeCtx *args);
int cli_schema_number(const CliSchemaJsonNodeCtx *args);
int cli_schema_u64(const CliSchemaJsonNodeValueU64sCtx *args);
int cli_schema_same(const CliSchemaReadJsonCtx *args);
int cli_schema_same_values(const CliSchemaReadOneCtx *args);
// a schema's value as a message shows it, between backticks, cut short where it is long; a file's value the same; a
// number as a message shows it; a type's name as a message names it; and whether `size` bytes of `text` match `pattern`
// somewhere, as ECMAScript reads it: 1, 0, or -1 where impero does not read the pattern
void cli_schema_show(const CliSchemaJsonValueCtx *args);
void cli_schema_displayed_value(const CliSchemaReadNodeCtx *args);
void cli_schema_number_show(const CliSchemaOutCtx *args);
void cli_schema_type_write(const CliSchemaSizeOutCtx *args);
int cli_schema_matches(const CliSchemaTextCtx *args);

// the file read and the schemas reached, cells taken with the reader's lock and given back with it, one check or
// completion at a time; and the lock taken with them: 1, or 0 with the lock given back where the cells cannot meet
// them; and them given back with the lock (schema.c)
extern CliSchemaRead *g_cli_schema_read;
extern CliSchemaStore *g_cli_schema_store;
// the read's and the store's cells given back and both let go
void cli_schema_free(void);
int cli_schema_lock(void);
void cli_schema_release(void);

// What a check works with: the schemas, the file read, its lines and its text, and the findings
typedef struct
{
    CliSchemaStore *store;
    CliSchemaRead *read;
    const CliBuildLines *lines;
    const char *text;
    CliBuildFindings *found;
} CliSchemaCheck;

// A key's span, where a value is a key's: whether it is one, and where it starts and ends
typedef struct
{
    int has_key;
    unsigned long long from;
    unsigned long long to;
} CliSchemaKey;

// the value `node` checked against `schema`, read in the document at `base`, `key` the span of the key that contains
// it; whether it fits, its findings set aside; and its findings, those that are no note, retained from `count` on only
// where `record`, their count returned (schema_check.c)
void cli_schema_node(const CliSchemaBaseCtx *args);
int cli_schema_fits(const CliSchemaBaseCtx *args);
unsigned int cli_schema_trial(const CliSchemaBaseNodeCtx *args);

// a number, a string and an array checked against the keywords that bound them (schema_bounds.c, schema_array.c); and
// an object (schema_object.c)
void cli_schema_number_check(const CliSchemaBaseMapCtx *args);
void cli_schema_text_check(const CliSchemaBaseMapCtx *args);
void cli_schema_array_check(const CliSchemaBaseMapCtx *args);
void cli_schema_object_check(const CliSchemaBaseMapCtx *args);

// the most steps to the cursor's value, the most keys open objects contain before it, and the most schemas reached
#define CLI_SCHEMA_STEPS 513u
#define CLI_SCHEMA_OPEN_KEYS 65536u
#define CLI_SCHEMA_REACHED 1024u

// A step into a JSON value: an object's key's raw text or an array's index
typedef struct
{
    int key;
    unsigned long long from;
    unsigned long long to;
    unsigned int index;
} CliSchemaStep;

// Where the cursor stands: the steps to the value it is in, whether it is at a key and inside a string, and the keys
// the object contains before it
typedef struct
{
    CliSchemaStep step[CLI_SCHEMA_STEPS];
    unsigned int steps;
    int at_key;
    int in_string;
    unsigned int keys_first;
    unsigned int keys;
} CliSchemaSpot;

// where the cursor stands, the keys its object contains before it, and the schemas reached, each its node and the base
// it is read in, under the reader's lock; where the cursor stands at the end of `size` bytes of `text`: 1, or 0 where
// nothing stands open; the schemas that apply at the end of its steps from `schema`, read in `base`, `$ref`s followed
// and `allOf`, `anyOf`, `oneOf` and `if`'s branches taken together; and a schema's description or title, its first
// line, or its type, for a completion's detail, into `out` (schema_complete.c)
extern CliSchemaSpot g_cli_schema_spot;
extern unsigned long long g_cli_schema_key_from[CLI_SCHEMA_OPEN_KEYS];
extern unsigned long long g_cli_schema_key_to[CLI_SCHEMA_OPEN_KEYS];
extern unsigned int g_cli_schema_reached[CLI_SCHEMA_REACHED];
extern unsigned int g_cli_schema_reached_count;
int cli_schema_spot(const CliSchemaReadTextCtx *args);
void cli_schema_reached_get(const CliSchemaTextBaseCtx *args);
void cli_schema_detail(const CliSchemaJsonOutCtx *args);

struct CliSchemaBaseCtx
{
    const char *base;
    unsigned int node;
    CliSchemaCheck *check;
    unsigned int schema;
    CliSchemaKey key_schema_key;
    unsigned int depth;
};

struct CliSchemaBaseMapCtx
{
    const char *base;
    unsigned int map;
    unsigned int node;
    CliSchemaCheck *check;
    CliSchemaKey key_schema_key;
    unsigned int depth;
};

struct CliSchemaReadCtx
{
    const CliSchemaRead *read;
    char *key;
    const char *path;
    int *named;
    unsigned long long *from;
    unsigned long long *to;
};

struct CliSchemaBaseNodeCtx
{
    const char *base;
    unsigned int node;
    CliSchemaCheck *check;
    unsigned int schema;
    unsigned int depth;
    int record;
};

struct CliSchemaSizeCtx
{
    unsigned long long size;
    CliSchemaStore *store;
    const char *base;
    const char *reference;
    char *key;
};

struct CliSchemaJsonCtx
{
    const CliJson *json;
    unsigned int map;
    const char *name;
    unsigned long long *size_pointer;
};

struct CliSchemaTextCtx
{
    const char *text;
    unsigned long long size;
    const char *pattern;
    unsigned long long pattern_size;
};

struct CliSchemaReadJsonCtx
{
    const CliSchemaRead *read;
    const CliJson *json;
    unsigned int node;
    unsigned int value;
};

struct CliSchemaJsonOutCtx
{
    const CliJson *json;
    VerbaLinea *out;
    unsigned int schema;
};

struct CliSchemaSizeOutCtx
{
    unsigned long long size;
    VerbaLinea *out;
    const char *type;
};

struct CliSchemaJsonNodeCtx
{
    const CliJson *json;
    unsigned int node;
    double *value_f64s;
};

struct CliSchemaTextBaseCtx
{
    const char *text;
    const char *base;
    unsigned int schema;
};

struct CliSchemaReadTextCtx
{
    CliSchemaRead *read_out;
    const char *text;
    unsigned long long size;
};

struct CliSchemaReadOneCtx
{
    const CliSchemaRead *read;
    unsigned int one;
    unsigned int other;
};

struct CliSchemaJsonValueCtx
{
    const CliJson *json;
    unsigned int value;
    VerbaLinea *out;
};

struct CliSchemaReadNodeCtx
{
    const CliSchemaRead *read;
    unsigned int node;
    VerbaLinea *out;
};

struct CliSchemaJsonNodeValueU64sCtx
{
    const CliJson *json;
    unsigned int node;
    unsigned long long *value_u64s;
};

struct CliSchemaUrlCtx
{
    const char *url;
    char *out_capacity;
};

struct CliSchemaStoreCtx
{
    CliSchemaStore *store;
    const char *key_read;
};

struct CliSchemaOutCtx
{
    VerbaLinea *out;
    double value_f64;
};

struct CliSchemaReadAtCtx
{
    const CliSchemaRead *read;
    unsigned int at;
};

struct CliSchemaCtx
{
    const CliSchemaNode *node_schema_node_pointer;
};

typedef struct
{
    void (*read)(const CliSchemaReadTextCtx *args);
    int (*spot)(const CliSchemaReadTextCtx *args);
} CliSchemaReadTextNs;
EMBED_TABLE_LAYOUT(CliSchemaReadTextNs, read, spot);
EMBED_TABLE_STORAGE CliSchemaReadTextNs cli_schema_read_text_internal EMBED_UNUSED = {
    .read = cli_schema_read,
    .spot = cli_schema_spot,
};

typedef struct
{
    const char *(*text)(const CliSchemaReadAtCtx *args);
} CliSchemaReadAtNs;
EMBED_TABLE_LAYOUT(CliSchemaReadAtNs, text);
EMBED_TABLE_STORAGE CliSchemaReadAtNs cli_schema_read_at_internal EMBED_UNUSED = {
    .text = cli_schema_text,
};

typedef struct
{
    const char *(*type)(const CliSchemaCtx *args);
} CliSchemaNsTable;
EMBED_TABLE_LAYOUT(CliSchemaNsTable, type);
EMBED_TABLE_STORAGE CliSchemaNsTable cli_schema_internal EMBED_UNUSED = {
    .type = cli_schema_type,
};

typedef struct
{
    void (*store_begin)(const CliSchemaStoreCtx *args);
    unsigned int (*load)(const CliSchemaStoreCtx *args);
} CliSchemaStoreNs;
EMBED_TABLE_LAYOUT(CliSchemaStoreNs, store_begin, load);
EMBED_TABLE_STORAGE CliSchemaStoreNs cli_schema_store_internal EMBED_UNUSED = {
    .store_begin = cli_schema_store_begin,
    .load = cli_schema_load,
};

typedef struct
{
    int (*cache_path)(const CliSchemaUrlCtx *args);
} CliSchemaUrlNs;
EMBED_TABLE_LAYOUT(CliSchemaUrlNs, cache_path);
EMBED_TABLE_STORAGE CliSchemaUrlNs cli_schema_url_internal EMBED_UNUSED = {
    .cache_path = cli_schema_cache_path,
};

typedef struct
{
    unsigned int (*resolve)(const CliSchemaSizeCtx *args);
} CliSchemaSizeNs;
EMBED_TABLE_LAYOUT(CliSchemaSizeNs, resolve);
EMBED_TABLE_STORAGE CliSchemaSizeNs cli_schema_size_internal EMBED_UNUSED = {
    .resolve = cli_schema_resolve,
};

typedef struct
{
    int (*get)(const CliSchemaReadCtx *args);
} CliSchemaReadNs;
EMBED_TABLE_LAYOUT(CliSchemaReadNs, get);
EMBED_TABLE_STORAGE CliSchemaReadNs cli_schema_read_internal EMBED_UNUSED = {
    .get = cli_schema_on_get,
};

typedef struct
{
    unsigned int (*get)(const CliSchemaJsonCtx *args);
    const char *(*string)(const CliSchemaJsonCtx *args);
} CliSchemaJsonNs;
EMBED_TABLE_LAYOUT(CliSchemaJsonNs, get, string);
EMBED_TABLE_STORAGE CliSchemaJsonNs cli_schema_json_internal EMBED_UNUSED = {
    .get = cli_schema_get,
    .string = cli_schema_string,
};

typedef struct
{
    int (*schema_true)(const CliSchemaJsonNodeCtx *args);
    int (*schema_false)(const CliSchemaJsonNodeCtx *args);
    int (*number)(const CliSchemaJsonNodeCtx *args);
} CliSchemaJsonNodeNs;
EMBED_TABLE_LAYOUT(CliSchemaJsonNodeNs, schema_true, schema_false, number);
EMBED_TABLE_STORAGE CliSchemaJsonNodeNs cli_schema_json_node_internal EMBED_UNUSED = {
    .schema_true = cli_schema_true,
    .schema_false = cli_schema_false,
    .number = cli_schema_number,
};

typedef struct
{
    int (*u64)(const CliSchemaJsonNodeValueU64sCtx *args);
} CliSchemaJsonNodeValueU64sNs;
EMBED_TABLE_LAYOUT(CliSchemaJsonNodeValueU64sNs, u64);
EMBED_TABLE_STORAGE CliSchemaJsonNodeValueU64sNs cli_schema_json_node_value_u_64s_internal EMBED_UNUSED = {
    .u64 = cli_schema_u64,
};

typedef struct
{
    int (*same)(const CliSchemaReadJsonCtx *args);
} CliSchemaReadJsonNs;
EMBED_TABLE_LAYOUT(CliSchemaReadJsonNs, same);
EMBED_TABLE_STORAGE CliSchemaReadJsonNs cli_schema_read_json_internal EMBED_UNUSED = {
    .same = cli_schema_same,
};

typedef struct
{
    int (*same_values)(const CliSchemaReadOneCtx *args);
} CliSchemaReadOneNs;
EMBED_TABLE_LAYOUT(CliSchemaReadOneNs, same_values);
EMBED_TABLE_STORAGE CliSchemaReadOneNs cli_schema_read_one_internal EMBED_UNUSED = {
    .same_values = cli_schema_same_values,
};

typedef struct
{
    void (*show)(const CliSchemaJsonValueCtx *args);
} CliSchemaJsonValueNs;
EMBED_TABLE_LAYOUT(CliSchemaJsonValueNs, show);
EMBED_TABLE_STORAGE CliSchemaJsonValueNs cli_schema_json_value_internal EMBED_UNUSED = {
    .show = cli_schema_show,
};

typedef struct
{
    void (*displayed_value)(const CliSchemaReadNodeCtx *args);
} CliSchemaReadNodeNs;
EMBED_TABLE_LAYOUT(CliSchemaReadNodeNs, displayed_value);
EMBED_TABLE_STORAGE CliSchemaReadNodeNs cli_schema_read_node_internal EMBED_UNUSED = {
    .displayed_value = cli_schema_displayed_value,
};

typedef struct
{
    void (*number_show)(const CliSchemaOutCtx *args);
} CliSchemaOutNs;
EMBED_TABLE_LAYOUT(CliSchemaOutNs, number_show);
EMBED_TABLE_STORAGE CliSchemaOutNs cli_schema_out_internal EMBED_UNUSED = {
    .number_show = cli_schema_number_show,
};

typedef struct
{
    void (*type_write)(const CliSchemaSizeOutCtx *args);
} CliSchemaSizeOutNs;
EMBED_TABLE_LAYOUT(CliSchemaSizeOutNs, type_write);
EMBED_TABLE_STORAGE CliSchemaSizeOutNs cli_schema_size_out_internal EMBED_UNUSED = {
    .type_write = cli_schema_type_write,
};

typedef struct
{
    int (*matches)(const CliSchemaTextCtx *args);
} CliSchemaTextNs;
EMBED_TABLE_LAYOUT(CliSchemaTextNs, matches);
EMBED_TABLE_STORAGE CliSchemaTextNs cli_schema_text_internal EMBED_UNUSED = {
    .matches = cli_schema_matches,
};

typedef struct
{
    void (*node)(const CliSchemaBaseCtx *args);
    int (*fits)(const CliSchemaBaseCtx *args);
} CliSchemaBaseNs;
EMBED_TABLE_LAYOUT(CliSchemaBaseNs, node, fits);
EMBED_TABLE_STORAGE CliSchemaBaseNs cli_schema_base_internal EMBED_UNUSED = {
    .node = cli_schema_node,
    .fits = cli_schema_fits,
};

typedef struct
{
    unsigned int (*trial)(const CliSchemaBaseNodeCtx *args);
} CliSchemaBaseNodeNs;
EMBED_TABLE_LAYOUT(CliSchemaBaseNodeNs, trial);
EMBED_TABLE_STORAGE CliSchemaBaseNodeNs cli_schema_base_node_internal EMBED_UNUSED = {
    .trial = cli_schema_trial,
};

typedef struct
{
    void (*number_check)(const CliSchemaBaseMapCtx *args);
    void (*text_check)(const CliSchemaBaseMapCtx *args);
    void (*array_check)(const CliSchemaBaseMapCtx *args);
    void (*object_check)(const CliSchemaBaseMapCtx *args);
} CliSchemaBaseMapNs;
EMBED_TABLE_LAYOUT(CliSchemaBaseMapNs, number_check, text_check, array_check, object_check);
EMBED_TABLE_STORAGE CliSchemaBaseMapNs cli_schema_base_map_internal EMBED_UNUSED = {
    .number_check = cli_schema_number_check,
    .text_check = cli_schema_text_check,
    .array_check = cli_schema_array_check,
    .object_check = cli_schema_object_check,
};

typedef struct
{
    void (*reached_get)(const CliSchemaTextBaseCtx *args);
} CliSchemaTextBaseNs;
EMBED_TABLE_LAYOUT(CliSchemaTextBaseNs, reached_get);
EMBED_TABLE_STORAGE CliSchemaTextBaseNs cli_schema_text_base_internal EMBED_UNUSED = {
    .reached_get = cli_schema_reached_get,
};

typedef struct
{
    void (*detail)(const CliSchemaJsonOutCtx *args);
} CliSchemaJsonOutNs;
EMBED_TABLE_LAYOUT(CliSchemaJsonOutNs, detail);
EMBED_TABLE_STORAGE CliSchemaJsonOutNs cli_schema_json_out_internal EMBED_UNUSED = {
    .detail = cli_schema_detail,
};

#endif
