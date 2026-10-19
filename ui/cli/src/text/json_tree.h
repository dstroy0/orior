// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_tree.h: JSON values as a tree the command line reads, changes and writes (json_tree.c, json_tree_write.c). An
// object keeps its members in the order they were put, one to a key: a key put again takes the place it had. Every
// node and every text of a tree is kept in the storage its caller hands it, so a tree is made, read and written with
// nothing allocated
#ifndef JSON_TREE_H
#define JSON_TREE_H

#include "../../../../src/cu/includes/formats/cfg_json/cfg_json.h"
#include "../command_line/cli_internal.h"

// no node: a member or item not found, or a tree too full to make one
#define CLI_JSON_NONE 0xffffffffu

// One node: its kind, as cfg_json names kinds; its key in the text where it is an object's member; its text, a
// string's letters or a number as written, `length` bytes; its children from `first` to `last`, `count` of them, and
// the node after it among its parent's
typedef struct
{
    CfgJsonKind kind;
    unsigned long long key;
    unsigned long long text;
    unsigned long long length;
    unsigned int first;
    unsigned int last;
    unsigned int next;
    unsigned int count;
} CliJsonNode;

// A tree: its nodes and its texts in the storage handed it, and 1 in `full` where a node or a text did not fit
typedef struct
{
    CliJsonNode *node;
    unsigned int nodes;
    unsigned int nodes_most;
    char *text;
    unsigned long long used;
    unsigned long long text_most;
    int full;
} CliJson;

// a tree made empty over `node`, which holds `nodes_most`, and `text`, which holds `text_most`
void cli_json_begun(CliJson *json, CliJsonNode *node, unsigned int nodes_most, char *text,
                    unsigned long long text_most);
// the JSON `size` bytes of `text` say read into the tree, `tokens` holding `tokens_most` of them: its root, or
// CLI_JSON_NONE with why said
unsigned int cli_json_read(CliJson *json, const char *text, unsigned long long size, CfgJsonToken *tokens,
                           unsigned int tokens_most, CliLine *said);
// the tokens cfg_json makes of `size` bytes of `text`, counted without making them: exact where the text is JSON, and
// where it is not, a count the read then refuses (json_taken.c)
unsigned int cli_json_tokens_counted(const char *text, unsigned long long size);
// a tree made empty in a cell of its own holding `nodes_most` nodes and `text_most` bytes of texts: 1, or 0 with the
// cell refused and said and the tree holding nothing
int cli_json_held(CliJson *json, unsigned int nodes_most, unsigned long long text_most, const char *what);
// `size` bytes of `text` read into a tree in a cell of its own, sized to the tokens they hold and to their bytes, with
// `nodes_more` nodes and `text_more` bytes over for what is made after: its root, or CLI_JSON_NONE with why said; and
// the tree's cell given back, the tree left holding nothing, where it holds one or none
unsigned int cli_json_taken(CliJson *json, const char *text, unsigned long long size, unsigned int nodes_more,
                            unsigned long long text_more, CliLine *said);
void cli_json_given(CliJson *json);
// the nodes a node and all under it take, and the bytes of their texts and keys, added to `nodes` and `text`, the room
// a copy of it takes in another tree
void cli_json_extent(const CliJson *json, unsigned int node, unsigned int *nodes, unsigned long long *text);
// the file at `path` read mapped into a tree as cli_json_taken reads one, the file let go as the read ends: its root,
// or CLI_JSON_NONE with why said, the tree holding nothing where the file could not be read
unsigned int cli_json_file_taken(CliJson *json, const char *path, unsigned int nodes_more, unsigned long long text_more,
                                 CliLine *said);
// a node made of `kind` with no children; a string made of `length` bytes of `text`; a number made as `text` writes it
unsigned int cli_json_made(CliJson *json, CfgJsonKind kind);
unsigned int cli_json_string_made(CliJson *json, const char *text, unsigned long long length);
unsigned int cli_json_number_made(CliJson *json, const char *text);
// the member `name` of the object `object`, or the `index`th item of the array `array`: CLI_JSON_NONE where none is,
// or the node is not of that kind
unsigned int cli_json_member(const CliJson *json, unsigned int object, const char *name);
unsigned int cli_json_item(const CliJson *json, unsigned int array, unsigned int index);
// the letters of a string node, NULL where the node is none or no string; its length into `length` where it is given
const char *cli_json_string(const CliJson *json, unsigned int node, unsigned long long *length);
// whether a node is of `kind`, none never is
int cli_json_is(const CliJson *json, unsigned int node, CfgJsonKind kind);
// `value` put as the member `name` of `object`, in the place of one it had, else after its last; the member `name`
// taken out of `object`; `value` put after the last item of `array`
void cli_json_set(CliJson *json, unsigned int object, const char *name, unsigned int value);
void cli_json_remove(CliJson *json, unsigned int object, const char *name);
void cli_json_append(CliJson *json, unsigned int array, unsigned int value);
// the members of `object` put in the order of their keys, by their bytes
void cli_json_sorted(CliJson *json, unsigned int object);
// a node and all under it made again as a node of its own, its key not kept
unsigned int cli_json_copy(CliJson *json, unsigned int node);
// a node of `from` and all under it made in `to`, its key not kept
unsigned int cli_json_copy_from(CliJson *to, const CliJson *from, unsigned int node);
// whether two nodes hold the same value, an object's members compared by key whatever their order
int cli_json_same(const CliJson *json, unsigned int one, const CliJson *other_json, unsigned int other);
// the node written as JSON into `out`: on one line, or as serde_json's pretty writer lays it out with `indent` spaces
// to a level where `indent` is not 0; every object's keys in order where `sorted` is 1 (json_tree_write.c)
void cli_json_write(const CliJson *json, unsigned int node, CliLine *out, unsigned int indent, int sorted);
// `length` bytes of `text` written as a JSON string, quotes about them, as serde_json escapes it
void cli_json_quoted(CliLine *out, const char *text, unsigned long long length);
// `length` bytes written as a JSON string as cli_json_quoted writes them, each byte that starts no whole UTF-8 letter
// written as U+FFFD, as Rust's from_utf8_lossy reads it
void cli_json_quoted_lossy(CliLine *out, const unsigned char *text, unsigned long long length);
// A JSON string being written into `out` from a line that hands it its bytes as it fills, and the bytes of a letter
// the last piece ended inside of
typedef struct
{
    CliLine *out;
    unsigned char held[4];
    unsigned int count;
} CliJsonQuoting;
// the string opened in `out` and `line` made a line over `piece`, which holds `piece_size`, whose every byte goes into
// the string escaped as cli_json_quoted_lossy escapes it; and what `line` holds handed on and the string closed
// (json_quoting.c)
void cli_json_quoting_begun(CliJsonQuoting *quoting, CliLine *out, CliLine *line, char *piece,
                            unsigned long long piece_size);
void cli_json_quoting_ended(CliJsonQuoting *quoting, CliLine *line);
// the same written with no quotes about it, so texts read apart join in one string
void cli_json_escaped_lossy(CliLine *out, const unsigned char *text, unsigned long long length);
// `length` bytes written as cli_json_quoted_lossy reads them, with no quotes and nothing escaped
void cli_lossy(CliLine *out, const unsigned char *text, unsigned long long length);
// the letter at `at` of `length` bytes as cli_lossy reads it into `letter`, U+FFFD for a byte that starts no whole
// letter: the bytes it takes
unsigned long long cli_lossy_letter(const unsigned char *text, unsigned long long length, unsigned long long at,
                                    unsigned int *letter);
// `value` written as serde_json writes a 64-bit float: the fewest digits that read back as it, `100000.0`, `-0.62`,
// `1e-7`, `1.5e300`, and `null` where it is not finite (json_number.c)
void cli_json_double(CliLine *out, double value);
// `size` bytes of JSON written again into `out` as serde_json writes the Value it reads them as, on one line, with no
// tree made: 1, or 0 with nothing written where serde_json reads them as no Value (json_compact.c)
int cli_json_compacted(const char *text, unsigned long long size, CliLine *out);
// where the JSON value at `at` of `size` bytes of `text`, past the spaces before it, ends: past it, or `at` where no
// whole value stands there (json_compact.c)
unsigned long long cli_json_value_end(const char *text, unsigned long long size, unsigned long long at);
// whether `size` bytes are UTF-8 whole, as Rust's str reads them (json_compact.c)
int cli_utf8_whole(const char *bytes, unsigned long long size);

#endif
