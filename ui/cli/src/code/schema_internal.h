// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_internal.h: what the schemas' reader shares: a JSON file read with the place of each of its values and what
// kept it from reading; the schemas a check reaches, each by its URL or its path, held as JSON trees; and the check of
// a file's values against them (schema.c, schema_read.c, schema_bundled.c, schema_store.c, schema_resolve.c,
// schema_check.c, schema_values.c, schema_bounds.c, schema_array.c, schema_object.c, schema_complete.c,
// schema_offer.c)
#ifndef SCHEMA_INTERNAL_H
#define SCHEMA_INTERNAL_H

#include "../builds/builds.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "schema.h"

// what a value of a JSON file is
#define CLI_SCHEMA_OBJECT 1u
#define CLI_SCHEMA_ARRAY 2u
#define CLI_SCHEMA_TEXT 3u
#define CLI_SCHEMA_NUMBER 4u
#define CLI_SCHEMA_BOOL 5u
#define CLI_SCHEMA_NULL 6u

// the most values of a file, members, things that kept it from reading, the bytes their texts take, and none
#define CLI_SCHEMA_NODES (1u << 20u)
#define CLI_SCHEMA_MEMBERS (1u << 20u)
#define CLI_SCHEMA_ERRORS 4096u
#define CLI_SCHEMA_POOL (1u << 23u)
#define CLI_SCHEMA_NONE 0xffffffffu

// A value of a JSON file and its span: its kind, a string's letters in the pool, a number's value, a boolean's truth;
// an object's first member or an array's first item, and how many; and the next item of its array
typedef struct
{
    unsigned char kind;
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

// What kept a JSON file from reading: its span and what it says, in the pool
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned int message_at;
    unsigned int message_size;
} CliSchemaError;

// A JSON file as read: its text, its values and members, what kept it from reading, its root, CLI_SCHEMA_NONE where
// none was read, the texts of all, and 1 in `full` where a part of it did not fit. The values, the members and the
// texts are cells of `nodes_most`, `members_most` and `pool_most` places that double as the file is read, to
// CLI_SCHEMA_NODES, CLI_SCHEMA_MEMBERS and CLI_SCHEMA_POOL at the most; the values' room is grown once more before
// the file is checked, by a value for each member and CLI_SCHEMA_CHECK_NODES, for the keys a check makes values of, and
// holds still while the file is checked
typedef struct
{
    const char *text;
    unsigned long long size;
    CliSchemaNode *node;
    unsigned int nodes;
    unsigned int nodes_most;
    CliSchemaMember *member;
    unsigned int members;
    unsigned int members_most;
    CliSchemaError error[CLI_SCHEMA_ERRORS];
    unsigned int errors;
    unsigned int root;
    char *pool;
    unsigned int used;
    unsigned int pool_most;
    int full;
} CliSchemaRead;

// the places a read's lists are begun with, and the values over the read's own a check may make
#define CLI_SCHEMA_FIRST 1024u
#define CLI_SCHEMA_CHECK_NODES 1024u

// a list of `each`-byte places at `cells`, `*most` of them, grown to hold `needed`, twice its room at a time from
// CLI_SCHEMA_FIRST and no more than `hard`, the places it held copied: the list, moved or not, or NULL with it left as
// it was where it cannot be; a read's lists grown to hold `nodes` values, `members` members and `pool` bytes of text,
// 1 where all do and 0 where one does not; and a read's lists given back (schema.c)
void *cli_schema_grown(void *cells, unsigned int *most, unsigned long long needed, size_t each, unsigned int hard,
                       const char *what);
int cli_schema_read_room(CliSchemaRead *read, unsigned long long nodes, unsigned long long members,
                         unsigned long long pool);
void cli_schema_read_given(CliSchemaRead *read);

// `size` bytes of a JSON file read into `read`, comments passed over (schema_read.c); a text of the pool; and the type
// a value is of as a schema names it, `integer` for a number with no fraction (schema_values.c)
void cli_schema_read(const char *text, unsigned long long size, CliSchemaRead *read);
const char *cli_schema_text(const CliSchemaRead *read, unsigned int at);
const char *cli_schema_kind(const CliSchemaNode *node);

// the meta-schemas orior_cli holds, each its URL with no scheme and no `#` and its text (schema_bundled.c)
extern const char *const g_cli_schema_bundled[5][2];

// the most documents a check reaches, those it could not find, the values and bytes their tree is begun with, doubling
// as they fill, and the deepest a value is checked
#define CLI_SCHEMA_DOCUMENTS 32u
#define CLI_SCHEMA_MISSING 64u
#define CLI_SCHEMA_STORE_NODES_FIRST 4096u
#define CLI_SCHEMA_STORE_TEXT_FIRST (1u << 16u)
#define CLI_SCHEMA_DEEPEST 64u

// The schemas a check reaches, each by its URL or its path, read into one tree that grows in a cell of its own, and
// those it could not find
typedef struct
{
    CliJson json;
    char key[CLI_SCHEMA_DOCUMENTS][CLI_PATH_LONGEST];
    unsigned int root[CLI_SCHEMA_DOCUMENTS];
    unsigned int documents;
    char missing[CLI_SCHEMA_MISSING][CLI_PATH_LONGEST];
    unsigned int missings;
} CliSchemaStore;

// the cells the store holds while it reads a document; the store made empty; the document at `key` read where it was
// not, a bundled meta-schema, a file, or a schema kept from an earlier fetch: its root, CLI_SCHEMA_NONE where none
// could be read; and the file a schema fetched from `url` is kept in, into `out`: 1, or 0 where orior_cli has no folder
// of its own (schema_store.c)
extern CliCellsTally g_cli_schema_cells;
void cli_schema_store_begun(CliSchemaStore *store);
unsigned int cli_schema_load(CliSchemaStore *store, const char *key);
int cli_schema_kept_at(const char *url, char *out);
// the schema `size` bytes of `reference` name from the document at `base`, and that document's key into `key`, which
// holds CLI_PATH_LONGEST: its node, or CLI_SCHEMA_NONE; and where a file's schema is, its `$schema` read beside it
// where it is a path, or the one its name is known by, into `key`: 1 with the span of the `$schema` value where it
// names one, `named` 0 where it is known by its name, or 0 where it has none (schema_resolve.c)
unsigned int cli_schema_resolve(CliSchemaStore *store, const char *base, const char *reference, unsigned long long size,
                                char *key);
int cli_schema_of(const char *path, const CliSchemaRead *read, char *key, int *named, unsigned long long *from,
                  unsigned long long *to);

// a member of a schema object, CLI_JSON_NONE where it has none or is no object; a schema's string member, NULL where
// it has none; whether a schema node is `true`, and `false`; a schema number's value into `value`: 1, or 0 where it is
// no number; and whether a value of the file holds the same as one of a schema, a number by its value, and two values
// of the file (schema_values.c)
unsigned int cli_schema_get(const CliJson *json, unsigned int map, const char *name);
const char *cli_schema_string(const CliJson *json, unsigned int map, const char *name, unsigned long long *size);
int cli_schema_true(const CliJson *json, unsigned int node);
int cli_schema_false(const CliJson *json, unsigned int node);
int cli_schema_number(const CliJson *json, unsigned int node, double *value);
int cli_schema_u64(const CliJson *json, unsigned int node, unsigned long long *value);
int cli_schema_same(const CliSchemaRead *read, unsigned int node, const CliJson *json, unsigned int value);
int cli_schema_same_values(const CliSchemaRead *read, unsigned int one, unsigned int other);
// a schema's value as a message shows it, between backticks, cut short where it is long; a file's value the same; a
// number as a message shows it; a type's name as a message says it; and whether `size` bytes of `text` match `pattern`
// somewhere, as ECMAScript reads it: 1, 0, or -1 where orior_cli does not read the pattern
void cli_schema_shown(const CliJson *json, unsigned int value, CliLine *out);
void cli_schema_shown_value(const CliSchemaRead *read, unsigned int node, CliLine *out);
void cli_schema_number_shown(double value, CliLine *out);
void cli_schema_kind_said(const char *kind, unsigned long long size, CliLine *out);
int cli_schema_matches(const char *pattern, unsigned long long pattern_size, const char *text, unsigned long long size);

// the file read and the schemas reached, cells taken with the reader's lock and given back with it, one check or
// completion at a time; and the lock taken with them: 1, or 0 with the lock given back where the cells cannot meet
// them; and them given back with the lock (schema.c)
extern CliSchemaRead *g_cli_schema_read;
extern CliSchemaStore *g_cli_schema_store;
extern CliLock g_cli_schema_lock;
int cli_schema_held(void);
void cli_schema_released(void);

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
    int held;
    unsigned long long from;
    unsigned long long to;
} CliSchemaKey;

// the value `node` checked against `schema`, read in the document at `base`, `key` the span of the key that holds it;
// whether it fits, its findings set aside; and its findings, those that are no note, kept from `count` on only where
// `keep`, their count returned (schema_check.c)
void cli_schema_node(CliSchemaCheck *check, unsigned int node, unsigned int schema, const char *base, CliSchemaKey key,
                     unsigned int depth);
int cli_schema_fits(CliSchemaCheck *check, unsigned int node, unsigned int schema, const char *base,
                    unsigned int depth);
unsigned int cli_schema_trial(CliSchemaCheck *check, unsigned int node, unsigned int schema, const char *base,
                              unsigned int depth, int keep);

// a number, a string and an array checked against the keywords that bound them (schema_bounds.c, schema_array.c); and
// an object (schema_object.c)
void cli_schema_number_checked(CliSchemaCheck *check, unsigned int node, unsigned int map);
void cli_schema_text_checked(CliSchemaCheck *check, unsigned int node, unsigned int map);
void cli_schema_array_checked(CliSchemaCheck *check, unsigned int node, unsigned int map, const char *base,
                              unsigned int depth);
void cli_schema_object_checked(CliSchemaCheck *check, unsigned int node, unsigned int map, const char *base,
                               CliSchemaKey key, unsigned int depth);

// the most steps to the cursor's value, the most keys open objects hold before it, and the most schemas reached
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
// the object holds before it
typedef struct
{
    CliSchemaStep step[CLI_SCHEMA_STEPS];
    unsigned int steps;
    int at_key;
    int in_string;
    unsigned int keys_first;
    unsigned int keys;
} CliSchemaSpot;

// where the cursor stands, the keys its object holds before it, and the schemas reached, each its node and the base
// it is read in, under the reader's lock; where the cursor stands at the end of `size` bytes of `text`: 1, or 0 where
// nothing stands open; the schemas that hold at the end of its steps from `schema`, read in `base`, `$ref`s followed
// and `allOf`, `anyOf`, `oneOf` and `if`'s branches taken together; and a schema's description or title, its first
// line, or its type, for a completion's detail, into `out` (schema_complete.c)
extern CliSchemaSpot g_cli_schema_spot;
extern unsigned long long g_cli_schema_key_from[CLI_SCHEMA_OPEN_KEYS];
extern unsigned long long g_cli_schema_key_to[CLI_SCHEMA_OPEN_KEYS];
extern unsigned int g_cli_schema_reached[CLI_SCHEMA_REACHED];
extern unsigned int g_cli_schema_reached_count;
int cli_schema_spot(const char *text, unsigned long long size);
void cli_schema_reached_of(const char *text, unsigned int schema, const char *base);
void cli_schema_detail(const CliJson *json, unsigned int schema, CliLine *out);

#endif
