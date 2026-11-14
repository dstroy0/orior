// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// json_tree.h: JSON values as a tree the command line reads, changes and writes (json_tree.c, json_tree_write.c). An
// object stores its members in the order they were put, one to a key: a key put again takes the place it had. Every
// node and every text of a tree is stored in the storage its caller hands it, so a tree is made, read and written with
// nothing allocated
#ifndef JSON_TREE_H
#define JSON_TREE_H
#include "../../../../includes/formats/cfg_json/cfg_json.h"
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"

typedef struct CliJsonTreeStoredCfg CliJsonTreeStoredCfg;
typedef struct CliJsonTreeTextSizeErrMsgCfg CliJsonTreeTextSizeErrMsgCfg;
typedef struct CliJsonTreeErrMsgCfg CliJsonTreeErrMsgCfg;
typedef struct CliJsonTreeTextSizeTokensCfg CliJsonTreeTextSizeTokensCfg;
typedef struct CliJsonTreeNodeNodeCfg CliJsonTreeNodeNodeCfg;
typedef struct CliJsonTreeOutCfg CliJsonTreeOutCfg;
typedef struct CliJsonTreeNodesMaxCfg CliJsonTreeNodesMaxCfg;
typedef struct CliJsonTreeNodeOutCfg CliJsonTreeNodeOutCfg;
typedef struct CliJsonTreeNodeNodesCfg CliJsonTreeNodeNodesCfg;
typedef struct CliJsonTreeOneCfg CliJsonTreeOneCfg;
typedef struct CliJsonTreeObjectNameCfg CliJsonTreeObjectNameCfg;
typedef struct CliJsonTreeLengthCfg CliJsonTreeLengthCfg;
typedef struct CliJsonTreeArrayCfg CliJsonTreeArrayCfg;
typedef struct CliJsonTreeTextCfg CliJsonTreeTextCfg;
typedef struct CliJsonTreeNodeCfg CliJsonTreeNodeCfg;
typedef struct CliJsonTreeLengthOutCfg CliJsonTreeLengthOutCfg;
typedef struct CliJsonTreeNodeTypeCfg CliJsonTreeNodeTypeCfg;
typedef struct CliJsonTreeArrayIndexCfg CliJsonTreeArrayIndexCfg;
typedef struct CliJsonTreeObjectCfg CliJsonTreeObjectCfg;
typedef struct CliJsonTreeTextLengthCfg CliJsonTreeTextLengthCfg;
typedef struct CliJsonTreeNodesCfg CliJsonTreeNodesCfg;
typedef struct CliJsonTreeNodeLengthCfg CliJsonTreeNodeLengthCfg;
typedef struct CliJsonTreeTextLengthTypeCfg CliJsonTreeTextLengthTypeCfg;
typedef struct CliJsonTreeTextSizeCfg CliJsonTreeTextSizeCfg;
typedef struct CliJsonTreeCfg CliJsonTreeCfg;
typedef struct CliJsonTreeOutValueCfg CliJsonTreeOutValueCfg;
typedef struct CliJsonTreeTypeCfg CliJsonTreeTypeCfg;
typedef struct CliJsonTreeSizeCfg CliJsonTreeSizeCfg;

// no node: a member or item not found, or a tree too full to make one
#define CLI_JSON_NONE 0xffffffffu

// One node: its type, as cfg_json names types; its key in the text where it is an object's member; its text, a
// string's letters or a number as written, `length` bytes; its children from `first` to `last`, `count` of them, and
// the node after it among its parent's
typedef struct
{
    CfgJsonKind type;
    unsigned long long key;
    unsigned long long text;
    unsigned long long length;
    unsigned int first;
    unsigned int last;
    unsigned int next;
    unsigned int count;
} CliJsonNode;

// A tree: its nodes and its texts in the storage handed it, and 1 in `full` where a node or a text did not fit; a tree
// taken stores both in one cell of its own
typedef struct
{
    CliJsonNode *node;
    unsigned int nodes;
    unsigned int nodes_max;
    char *text;
    unsigned long long used;
    unsigned long long text_max;
    int full;
    const char *what;
} CliJson;

// a tree made empty over `node`, sized for `nodes_max`, and `text`, sized for `text_max`
void cli_json_begin(const CliJsonTreeNodeNodeCfg *args);
// the JSON `size` bytes of `text` contain read into the tree, `tokens` sized for `tokens_max` of them: its root, or
// CLI_JSON_NONE with why given
unsigned int cli_json_read(const CliJsonTreeTextSizeTokensCfg *args);
// the tokens cfg_json makes of `size` bytes of `text`, counted without making them: exact where the text is JSON, and
// where it is not, a count the read then refuses (json_allocated.c)
unsigned int cli_json_tokens_count(const CliJsonTreeTextCfg *args);
// a tree made empty in a cell of the call's own, on the temporary tier, sized for `nodes_max` nodes and `text_max`
// bytes of texts: 1, or 0 with the cell refused and reported and the tree containing nothing. Every take below counts
// the tree's cell to `owner`, the tree is given back for the owner its cell counts to, and a tree that stands past the
// call is stored with cli_json_stored; capacity on the tree for `nodes` nodes and `text` bytes more: 1, or 0 with the
// tree full; and a tree emptied, its capacity retained
int cli_json_reserve(const CliJsonTreeNodesMaxCfg *args);
int cli_json_capacity(const CliJsonTreeNodesCfg *args);
void cli_json_clear(const CliJsonTreeCfg *args);
// `size` bytes of `text` read into a tree in a cell of the call's own, sized to the tokens they contain and to their
// bytes, with `nodes_more` nodes and `text_more` bytes over for what is made after: its root, or CLI_JSON_NONE with why
// given; and the tree's cell given back, the tree left containing nothing, where it contains one or none
unsigned int cli_json_alloc(const CliJsonTreeTextSizeErrMsgCfg *args);

// A tree stored past the call that made it, in one movable cell its holder reaches by the cell's token: its nodes at
// the cell's start and its texts after them, `nodes` of them and `used` bytes; `cell` NULL where none is stored
typedef struct
{
    const CarcerMovableEntry *cell;
    const CellOwner *owner;
    unsigned int nodes;
    unsigned long long used;
} CliJsonStored;

// `json_read`'s nodes and texts written into a movable cell of `owner` as `stored`, the one stored there before given
// back: 1, or 0 where the cell is refused; `stored_read` read into `json`, a tree in a cell of the call's own which
// persistent_buf_release gives back, its capacity what is stored: 1, or 0 where the cell is refused; and a stored
// tree's cell given back
int cli_json_stored(const CliJsonTreeStoredCfg *args);
int cli_json_pinned(const CliJsonTreeStoredCfg *args);
void cli_json_stored_release(CliJsonStored *stored);
// the nodes a node and all under it take, and the bytes of their texts and keys, added to `nodes` and `text`, the
// capacity a copy of it takes in another tree
void cli_json_extent(const CliJsonTreeNodeNodesCfg *args);
// the file at `path` read mapped into a tree as cli_json_alloc reads one, the file let go as the read ends: its root,
// or CLI_JSON_NONE with why given, the tree containing nothing where the file could not be read
unsigned int cli_json_file_alloc(const CliJsonTreeErrMsgCfg *args);
// a node made of `type` with no children; a string made of `length` bytes of `text`; a number made as `text` writes it
unsigned int cli_json_init(const CliJsonTreeTypeCfg *args);
unsigned int cli_json_string_init(const CliJsonTreeTextLengthTypeCfg *args);
unsigned int cli_json_number_init(const CliJsonTreeTextLengthTypeCfg *args);
// the member `name` of the object `object`, or the `index`th item of the array `array`: CLI_JSON_NONE where none is,
// or the node is not of that type
unsigned int cli_json_member(const CliJsonTreeObjectCfg *args);
unsigned int cli_json_item(const CliJsonTreeArrayIndexCfg *args);
// the letters of a string node, NULL where the node is none or no string; its length into `length` where it is given
const char *cli_json_string(const CliJsonTreeNodeLengthCfg *args);
// whether a node is of `type`, none never is
int cli_json_is(const CliJsonTreeNodeTypeCfg *args);
// `value` put as the member `name` of `object`, in the place of one it had, else after its last; the member `name`
// taken out of `object`; `value` put after the last item of `array`
void cli_json_set(const CliJsonTreeObjectNameCfg *args);
void cli_json_remove(const CliJsonTreeObjectNameCfg *args);
void cli_json_append(const CliJsonTreeArrayCfg *args);
// the members of `object` put in the order of their keys, by their bytes
void cli_json_sort(const CliJsonTreeObjectNameCfg *args);
// a node and all under it made again as a node of its own, its key not retained
unsigned int cli_json_copy(const CliJsonTreeCfg *args);
// a node of `from` and all under it made in `to`, its key not retained
unsigned int cli_json_copy_from(const CliJsonTreeNodeCfg *args);
// whether two nodes contain the same value, an object's members compared by key whatever their order
int cli_json_same(const CliJsonTreeOneCfg *args);
// the node written as JSON into `out`: on one line, or as serde_json's pretty writer lays it out with `indent` spaces
// to a level where `indent` is not 0; every object's keys in order where `sorted` is 1 (json_tree_write.c)
void cli_json_write(const CliJsonTreeNodeOutCfg *args);
// the most bytes any node of the tree writes to on one line, its 0 after them counted: a line of this capacity holds
// what cli_json_write writes of it with no indent, and what is written from its strings as they are
unsigned long long cli_json_write_capacity(const CliJsonTreeNodeOutCfg *args);
// the same with `indent` spaces to a level, for a tree no deeper than CLI_JSON_PRETTY_DEPTH
unsigned long long cli_json_pretty_capacity(const CliJsonTreeNodeOutCfg *args);
// `length` bytes of `text` written as a JSON string, quotes about them, as serde_json escapes it
void cli_json_quote(const CliJsonTreeTextLengthCfg *args);
// `length` bytes written as a JSON string as cli_json_quote writes them, each byte that starts no whole UTF-8 letter
// written as U+FFFD, as Rust's from_utf8_lossy reads it
void cli_json_quoted_lossy(const CliJsonTreeLengthOutCfg *args);
// A JSON string being written into `out` from a line that hands it its bytes as it fills, and the bytes of a letter
// the last piece ended inside of
typedef struct
{
    VerbaLinea *out;
    unsigned char pending[4];
    unsigned int count;
} CliJsonQuoting;
// the string opened in `out` and `line` made a line over `piece`, sized for `piece_size`, whose every byte goes into
// the string escaped as cli_json_quoted_lossy escapes it; and what `line` contains handed on and the string closed
// (json_quoting.c)
void cli_json_quoting_begin(const CliJsonTreeOutCfg *args);
void cli_json_quoting_end(const CliJsonTreeOutCfg *args);
// the same written with no quotes about it, and texts read apart join in one string
void cli_json_escaped_lossy(const CliJsonTreeLengthOutCfg *args);
// `length` bytes written as cli_json_quoted_lossy reads them, with no quotes and nothing escaped
void cli_lossy(const CliJsonTreeLengthOutCfg *args);
// the letter at `at` of `length` bytes as cli_lossy reads it into `letter`, U+FFFD for a byte that starts no whole
// letter: the bytes it takes
unsigned long long cli_lossy_letter(const CliJsonTreeLengthCfg *args);
// `value` written as serde_json writes a 64-bit float: the fewest digits that read back as it, `100000.0`, `-0.62`,
// `1e-7`, `1.5e300`, and `null` where it is not finite (json_number.c)
void cli_json_double(const CliJsonTreeOutValueCfg *args);
// `size` bytes of JSON written again into `out` as serde_json writes the Value it reads them as, on one line, with no
// tree made: 1, or 0 with nothing written where serde_json reads them as no Value (json_compact.c)
int cli_json_compact(const CliJsonTreeTextCfg *args);
// where the JSON value at `at` of `size` bytes of `text`, past the spaces before it, ends: past it, or `at` where no
// whole value stands there (json_compact.c)
unsigned long long cli_json_value_end(const CliJsonTreeTextSizeCfg *args);
// whether `size` bytes are UTF-8 whole, as Rust's str reads them (json_compact.c)
int cli_utf8_valid(const CliJsonTreeSizeCfg *args);

struct CliJsonTreeTextSizeErrMsgCfg
{
    CliJson *json;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
    const CellOwner *owner;
    unsigned int nodes_more;
    unsigned long long text_more;
};

struct CliJsonTreeErrMsgCfg
{
    CliJson *json;
    VerbaLinea *err_msg;
    const CellOwner *owner;
    unsigned int nodes_more;
    unsigned long long text_more;
    const char *path;
};

struct CliJsonTreeTextSizeTokensCfg
{
    CliJson *json;
    const char *text;
    unsigned long long size;
    CfgJsonToken *tokens;
    unsigned int tokens_max;
    VerbaLinea *err_msg;
};

struct CliJsonTreeNodeNodeCfg
{
    CliJson *json;
    CliJsonNode *node_json_node_pointer;
    unsigned int nodes_max;
    char *text_out;
    unsigned long long text_max;
};

struct CliJsonTreeOutCfg
{
    VerbaLinea *out;
    CliJsonQuoting *quoting;
    VerbaLinea *line;
    char *piece;
    unsigned long long piece_size;
};

struct CliJsonTreeNodesMaxCfg
{
    CliJson *json;
    unsigned int nodes_max;
    unsigned long long text_max;
    const char *what;
    const CellOwner *owner;
};

struct CliJsonTreeStoredCfg
{
    const CliJson *json_read;
    CliJson *json;
    CliJsonStored *stored;
    const CliJsonStored *stored_read;
    const CellOwner *owner;
};

struct CliJsonTreeNodeOutCfg
{
    const CliJson *json_read;
    unsigned int node;
    VerbaLinea *out;
    unsigned int indent;
    int sorted;
};

struct CliJsonTreeNodeNodesCfg
{
    const CliJson *json_read;
    unsigned int node;
    unsigned int *nodes_pointer;
    unsigned long long *text_u64s;
};

struct CliJsonTreeOneCfg
{
    const CliJson *json_read;
    unsigned int one;
    const CliJson *other_json;
    unsigned int other;
};

struct CliJsonTreeObjectNameCfg
{
    CliJson *json;
    unsigned int object;
    const char *name;
    unsigned int value;
};

struct CliJsonTreeLengthCfg
{
    unsigned long long length;
    const unsigned char *text_bytes;
    unsigned long long at;
    unsigned int *letter;
};

struct CliJsonTreeArrayCfg
{
    CliJson *json;
    unsigned int array;
    unsigned int value;
};

struct CliJsonTreeTextCfg
{
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
};

struct CliJsonTreeNodeCfg
{
    unsigned int node;
    CliJson *to;
    const CliJson *from;
};

struct CliJsonTreeLengthOutCfg
{
    unsigned long long length;
    VerbaLinea *out;
    const unsigned char *text_bytes;
};

struct CliJsonTreeNodeTypeCfg
{
    const CliJson *json_read;
    unsigned int node;
    CfgJsonKind type;
};

struct CliJsonTreeArrayIndexCfg
{
    const CliJson *json_read;
    unsigned int array;
    unsigned int index;
};

struct CliJsonTreeObjectCfg
{
    const CliJson *json_read;
    unsigned int object;
    const char *name;
};

struct CliJsonTreeTextLengthCfg
{
    const char *text;
    unsigned long long length;
    VerbaLinea *out;
};

struct CliJsonTreeNodesCfg
{
    CliJson *json;
    unsigned int nodes;
    unsigned long long text_u64;
};

struct CliJsonTreeNodeLengthCfg
{
    const CliJson *json_read;
    unsigned int node;
    unsigned long long *length_pointer;
};

struct CliJsonTreeTextLengthTypeCfg
{
    CliJson *json;
    const char *text;
    unsigned long long length;
};

struct CliJsonTreeTextSizeCfg
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
};

struct CliJsonTreeCfg
{
    CliJson *json;
    unsigned int node;
};

struct CliJsonTreeOutValueCfg
{
    VerbaLinea *out;
    double value_f64;
};

struct CliJsonTreeTypeCfg
{
    CliJson *json;
    CfgJsonKind type;
};

struct CliJsonTreeSizeCfg
{
    unsigned long long size;
    const char *bytes;
};

typedef struct
{
    void (*json_begin)(const CliJsonTreeNodeNodeCfg *args);
} CliJsonTreeNodeNodeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodeNodeNs, json_begin);
EMBED_TABLE_STORAGE CliJsonTreeNodeNodeNs cli_json_tree_node_node EMBED_UNUSED = {
    .json_begin = cli_json_begin,
};

typedef struct
{
    unsigned int (*json_read)(const CliJsonTreeTextSizeTokensCfg *args);
} CliJsonTreeTextSizeTokensNs;
EMBED_TABLE_LAYOUT(CliJsonTreeTextSizeTokensNs, json_read);
EMBED_TABLE_STORAGE CliJsonTreeTextSizeTokensNs cli_json_tree_text_size_tokens EMBED_UNUSED = {
    .json_read = cli_json_read,
};

typedef struct
{
    unsigned int (*json_tokens_count)(const CliJsonTreeTextCfg *args);
    int (*json_compact)(const CliJsonTreeTextCfg *args);
} CliJsonTreeTextNs;
EMBED_TABLE_LAYOUT(CliJsonTreeTextNs, json_tokens_count, json_compact);
EMBED_TABLE_STORAGE CliJsonTreeTextNs cli_json_tree_text EMBED_UNUSED = {
    .json_tokens_count = cli_json_tokens_count,
    .json_compact = cli_json_compact,
};

typedef struct
{
    int (*json_reserve)(const CliJsonTreeNodesMaxCfg *args);
} CliJsonTreeNodesMaxNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodesMaxNs, json_reserve);
EMBED_TABLE_STORAGE CliJsonTreeNodesMaxNs cli_json_tree_nodes_max EMBED_UNUSED = {
    .json_reserve = cli_json_reserve,
};

typedef struct
{
    int (*json_stored)(const CliJsonTreeStoredCfg *args);
    int (*json_pinned)(const CliJsonTreeStoredCfg *args);
} CliJsonTreeStoredNs;
EMBED_TABLE_LAYOUT(CliJsonTreeStoredNs, json_stored, json_pinned);
EMBED_TABLE_STORAGE CliJsonTreeStoredNs cli_json_tree_stored EMBED_UNUSED = {
    .json_stored = cli_json_stored,
    .json_pinned = cli_json_pinned,
};

typedef struct
{
    int (*json_capacity)(const CliJsonTreeNodesCfg *args);
} CliJsonTreeNodesNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodesNs, json_capacity);
EMBED_TABLE_STORAGE CliJsonTreeNodesNs cli_json_tree_nodes EMBED_UNUSED = {
    .json_capacity = cli_json_capacity,
};

typedef struct
{
    void (*json_clear)(const CliJsonTreeCfg *args);
    unsigned int (*json_copy)(const CliJsonTreeCfg *args);
} CliJsonTreeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNs, json_clear, json_copy);
EMBED_TABLE_STORAGE CliJsonTreeNs cli_json_tree EMBED_UNUSED = {
    .json_clear = cli_json_clear,
    .json_copy = cli_json_copy,
};

typedef struct
{
    unsigned int (*json_alloc)(const CliJsonTreeTextSizeErrMsgCfg *args);
} CliJsonTreeTextSizeErrMsgNs;
EMBED_TABLE_LAYOUT(CliJsonTreeTextSizeErrMsgNs, json_alloc);
EMBED_TABLE_STORAGE CliJsonTreeTextSizeErrMsgNs cli_json_tree_text_size_err_msg EMBED_UNUSED = {
    .json_alloc = cli_json_alloc,
};

typedef struct
{
    void (*json_extent)(const CliJsonTreeNodeNodesCfg *args);
} CliJsonTreeNodeNodesNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodeNodesNs, json_extent);
EMBED_TABLE_STORAGE CliJsonTreeNodeNodesNs cli_json_tree_node_nodes EMBED_UNUSED = {
    .json_extent = cli_json_extent,
};

typedef struct
{
    unsigned int (*json_file_alloc)(const CliJsonTreeErrMsgCfg *args);
} CliJsonTreeErrMsgNs;
EMBED_TABLE_LAYOUT(CliJsonTreeErrMsgNs, json_file_alloc);
EMBED_TABLE_STORAGE CliJsonTreeErrMsgNs cli_json_tree_err_msg EMBED_UNUSED = {
    .json_file_alloc = cli_json_file_alloc,
};

typedef struct
{
    unsigned int (*json_init)(const CliJsonTreeTypeCfg *args);
} CliJsonTreeTypeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeTypeNs, json_init);
EMBED_TABLE_STORAGE CliJsonTreeTypeNs cli_json_tree_type EMBED_UNUSED = {
    .json_init = cli_json_init,
};

typedef struct
{
    unsigned int (*json_string_init)(const CliJsonTreeTextLengthTypeCfg *args);
    unsigned int (*json_number_init)(const CliJsonTreeTextLengthTypeCfg *args);
} CliJsonTreeTextLengthTypeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeTextLengthTypeNs, json_string_init, json_number_init);
EMBED_TABLE_STORAGE CliJsonTreeTextLengthTypeNs cli_json_tree_text_length_type EMBED_UNUSED = {
    .json_string_init = cli_json_string_init,
    .json_number_init = cli_json_number_init,
};

typedef struct
{
    unsigned int (*json_member)(const CliJsonTreeObjectCfg *args);
} CliJsonTreeObjectNs;
EMBED_TABLE_LAYOUT(CliJsonTreeObjectNs, json_member);
EMBED_TABLE_STORAGE CliJsonTreeObjectNs cli_json_tree_object EMBED_UNUSED = {
    .json_member = cli_json_member,
};

typedef struct
{
    unsigned int (*json_item)(const CliJsonTreeArrayIndexCfg *args);
} CliJsonTreeArrayIndexNs;
EMBED_TABLE_LAYOUT(CliJsonTreeArrayIndexNs, json_item);
EMBED_TABLE_STORAGE CliJsonTreeArrayIndexNs cli_json_tree_array_index EMBED_UNUSED = {
    .json_item = cli_json_item,
};

typedef struct
{
    const char *(*json_string)(const CliJsonTreeNodeLengthCfg *args);
} CliJsonTreeNodeLengthNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodeLengthNs, json_string);
EMBED_TABLE_STORAGE CliJsonTreeNodeLengthNs cli_json_tree_node_length EMBED_UNUSED = {
    .json_string = cli_json_string,
};

typedef struct
{
    int (*json_is)(const CliJsonTreeNodeTypeCfg *args);
} CliJsonTreeNodeTypeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodeTypeNs, json_is);
EMBED_TABLE_STORAGE CliJsonTreeNodeTypeNs cli_json_tree_node_type EMBED_UNUSED = {
    .json_is = cli_json_is,
};

typedef struct
{
    void (*json_set)(const CliJsonTreeObjectNameCfg *args);
    void (*json_remove)(const CliJsonTreeObjectNameCfg *args);
    void (*json_sort)(const CliJsonTreeObjectNameCfg *args);
} CliJsonTreeObjectNameNs;
EMBED_TABLE_LAYOUT(CliJsonTreeObjectNameNs, json_set, json_remove, json_sort);
EMBED_TABLE_STORAGE CliJsonTreeObjectNameNs cli_json_tree_object_name EMBED_UNUSED = {
    .json_set = cli_json_set,
    .json_remove = cli_json_remove,
    .json_sort = cli_json_sort,
};

typedef struct
{
    void (*json_append)(const CliJsonTreeArrayCfg *args);
} CliJsonTreeArrayNs;
EMBED_TABLE_LAYOUT(CliJsonTreeArrayNs, json_append);
EMBED_TABLE_STORAGE CliJsonTreeArrayNs cli_json_tree_array EMBED_UNUSED = {
    .json_append = cli_json_append,
};

typedef struct
{
    unsigned int (*json_copy_from)(const CliJsonTreeNodeCfg *args);
} CliJsonTreeNodeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodeNs, json_copy_from);
EMBED_TABLE_STORAGE CliJsonTreeNodeNs cli_json_tree_node EMBED_UNUSED = {
    .json_copy_from = cli_json_copy_from,
};

typedef struct
{
    int (*json_same)(const CliJsonTreeOneCfg *args);
} CliJsonTreeOneNs;
EMBED_TABLE_LAYOUT(CliJsonTreeOneNs, json_same);
EMBED_TABLE_STORAGE CliJsonTreeOneNs cli_json_tree_one EMBED_UNUSED = {
    .json_same = cli_json_same,
};

typedef struct
{
    void (*json_write)(const CliJsonTreeNodeOutCfg *args);
    unsigned long long (*json_write_capacity)(const CliJsonTreeNodeOutCfg *args);
    unsigned long long (*json_pretty_capacity)(const CliJsonTreeNodeOutCfg *args);
} CliJsonTreeNodeOutNs;
EMBED_TABLE_LAYOUT(CliJsonTreeNodeOutNs, json_write, json_write_capacity, json_pretty_capacity);
EMBED_TABLE_STORAGE CliJsonTreeNodeOutNs cli_json_tree_node_out EMBED_UNUSED = {
    .json_write = cli_json_write,
    .json_write_capacity = cli_json_write_capacity,
    .json_pretty_capacity = cli_json_pretty_capacity,
};

typedef struct
{
    void (*json_quote)(const CliJsonTreeTextLengthCfg *args);
} CliJsonTreeTextLengthNs;
EMBED_TABLE_LAYOUT(CliJsonTreeTextLengthNs, json_quote);
EMBED_TABLE_STORAGE CliJsonTreeTextLengthNs cli_json_tree_text_length EMBED_UNUSED = {
    .json_quote = cli_json_quote,
};

typedef struct
{
    void (*json_quoted_lossy)(const CliJsonTreeLengthOutCfg *args);
    void (*json_escaped_lossy)(const CliJsonTreeLengthOutCfg *args);
    void (*lossy)(const CliJsonTreeLengthOutCfg *args);
} CliJsonTreeLengthOutNs;
EMBED_TABLE_LAYOUT(CliJsonTreeLengthOutNs, json_quoted_lossy, json_escaped_lossy, lossy);
EMBED_TABLE_STORAGE CliJsonTreeLengthOutNs cli_json_tree_length_out EMBED_UNUSED = {
    .json_quoted_lossy = cli_json_quoted_lossy,
    .json_escaped_lossy = cli_json_escaped_lossy,
    .lossy = cli_lossy,
};

typedef struct
{
    void (*json_quoting_begin)(const CliJsonTreeOutCfg *args);
    void (*json_quoting_end)(const CliJsonTreeOutCfg *args);
} CliJsonTreeOutNs;
EMBED_TABLE_LAYOUT(CliJsonTreeOutNs, json_quoting_begin, json_quoting_end);
EMBED_TABLE_STORAGE CliJsonTreeOutNs cli_json_tree_out EMBED_UNUSED = {
    .json_quoting_begin = cli_json_quoting_begin,
    .json_quoting_end = cli_json_quoting_end,
};

typedef struct
{
    unsigned long long (*lossy_letter)(const CliJsonTreeLengthCfg *args);
} CliJsonTreeLengthNs;
EMBED_TABLE_LAYOUT(CliJsonTreeLengthNs, lossy_letter);
EMBED_TABLE_STORAGE CliJsonTreeLengthNs cli_json_tree_length EMBED_UNUSED = {
    .lossy_letter = cli_lossy_letter,
};

typedef struct
{
    void (*json_double)(const CliJsonTreeOutValueCfg *args);
} CliJsonTreeOutValueNs;
EMBED_TABLE_LAYOUT(CliJsonTreeOutValueNs, json_double);
EMBED_TABLE_STORAGE CliJsonTreeOutValueNs cli_json_tree_out_value EMBED_UNUSED = {
    .json_double = cli_json_double,
};

typedef struct
{
    unsigned long long (*json_value_end)(const CliJsonTreeTextSizeCfg *args);
} CliJsonTreeTextSizeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeTextSizeNs, json_value_end);
EMBED_TABLE_STORAGE CliJsonTreeTextSizeNs cli_json_tree_text_size EMBED_UNUSED = {
    .json_value_end = cli_json_value_end,
};

typedef struct
{
    int (*utf8_valid)(const CliJsonTreeSizeCfg *args);
} CliJsonTreeSizeNs;
EMBED_TABLE_LAYOUT(CliJsonTreeSizeNs, utf8_valid);
EMBED_TABLE_STORAGE CliJsonTreeSizeNs cli_json_tree_size EMBED_UNUSED = {
    .utf8_valid = cli_utf8_valid,
};

#endif
