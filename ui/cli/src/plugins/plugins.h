// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins.h: plugins, what the window takes in from outside its own code (plugins.c finds and reads them,
// plugins_draft.c writes one from a few answers, plugins_text.c lays one out and creates it). A language plugin is a
// folder named by its id containing plugin.json:
//
//   id, name, version       the plugin's own name for itself, the name it shows, and its version
//   kind                    "language", "tool" for a tool plugin, which the validation reads, or "script" for a
//                           plugin that runs code of its own, which the window's scripts.js runs: the file `main`
//                           names beside plugin.json, plugin.js where it names none, in the form of orior's calls
//                           `api` names
//   extensions              the file extensions it opens, without the dot
//   names                   the whole names of the files it opens beside them, where it has any
//   each                    { folder, table, unless, except }: the plugin a language of its own for each folder of
//                           `folder` in the tree holding `table`, `{name}` in it the folder's name, opening the
//                           folder's name as an extension as the language `<id>:<name>`
//   comments                { line, block: [open, close] }, either left out where it has none
//   pairs, quotes           the brackets it closes and the quotes it pairs
//   indentAfter             a pattern a line ends with to indent the next deeper
//   grammar                 lists of words by name and the tokenizer, as editor/tokens.js reads one; a pattern is
//                           a string, or { pattern, flags } where it takes flags; a list may be read from a table of
//                           the tree and a pattern may be a list's words (cli_plugins_grammar_resolved)
//   hovers                  a word and the text hovering it shows, where it has any
//   snippets                { label, body } for the completion list, `${1:name}` a stop in a body
//   reads, writes           for a script plugin, the patterns of the tree's files it reads and writes, `*` any
//                           letters but `/` and `**` any folders, and `reaches`, the hosts it reaches on the
//                           network, `*.` before a host for any under it; the window restricts it to these, and runs it
//                           only once the reader lets it
//
// The plugins that come with orior are under ui/plugins and built into the program (plugins_bundled.c, written as
// the program is built by bundle/plugins_bundle.c). The reader's are under `plugins/` in orior's own folder, and one
// of the reader's stands in for one that comes with orior of the same id.
//
// The generator writes a language plugin from a few answers: a name, the extensions, how a line and a block are
// commented, the keywords, types and constants, and the quotes. Given a plugin to start from, it copies that plugin's
// grammar and changes what the answers name. Beside plugin.json it writes a sample file in the new language to open
// and try
#ifndef PLUGINS_H
#define PLUGINS_H
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliPluginsIdCfg CliPluginsIdCfg;
typedef struct CliPluginsErrMsgCfg CliPluginsErrMsgCfg;
typedef struct CliPluginsErrMsgJsonCfg CliPluginsErrMsgJsonCfg;
typedef struct CliPluginsErrMsgJsonSpecCfg CliPluginsErrMsgJsonSpecCfg;
typedef struct CliPluginsJsonCfg CliPluginsJsonCfg;
typedef struct CliPluginsErrMsgJsonSpecKindCfg CliPluginsErrMsgJsonSpecKindCfg;
typedef struct CliPluginsIdKeyCfg CliPluginsIdKeyCfg;
typedef struct CliPluginsLanguagesPathCfg CliPluginsLanguagesPathCfg;
typedef struct CliPluginsJsonOutCfg CliPluginsJsonOutCfg;
typedef struct CliPluginsEachCfg CliPluginsEachCfg;
typedef struct CliPluginsIdNameCfg CliPluginsIdNameCfg;
typedef struct CliPluginsCfg CliPluginsCfg;
typedef struct CliPluginsRootCfg CliPluginsRootCfg;
typedef struct CliPluginsTreeCfg CliPluginsTreeCfg;

// the most keys of extensions, names and paths the languages contain, and the most plugins the reader installs
#define CLI_LANGUAGE_PAIRS 4096u
#define CLI_USER_PLUGINS 1024u

// the bytes a plugin's text and its sample are written in, as a plugin is written
#define CLI_PLUGIN_TEXT_CAPACITY (1ull << 20u)

// A plugin that comes with orior: its id, the text of its plugin.json, `size` bytes, and the code it runs as a script
// plugin, `code_size` bytes, NULL for one that runs none
typedef struct
{
    const char *id;
    const unsigned char *text;
    unsigned long long size;
    const unsigned char *code;
    unsigned long long code_size;
} CliBundled;

// the plugins built into the program, in the order of their ids (plugins_bundled.c, written as the program is built)
extern const CliBundled g_cli_bundled[];
extern const unsigned int g_cli_bundled_count;

// A plugin as found: its id, where it came from, "bundled" or "user", the folder of a reader's own, NULL for one that
// comes with orior, and the text of its plugin.json
typedef struct
{
    const char *id;
    const char *source;
    const char *folder;
    const char *text;
    unsigned long long size;
} CliPlugin;

// One key an extension, whole name or pattern of paths opens by, and the id of the language it opens as
typedef struct
{
    char key[CLI_NAME_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
} CliLanguagePair;

// The language each extension, whole name and pattern of paths opens as: `/<name>` for a whole name, `?<pattern>` for
// a pattern, and the extension itself; each key with its language id, a later plugin's standing in for an earlier's,
// in a cell of `capacity`, released through persistent_buf_release
typedef struct
{
    CliLanguagePair *pair;
    unsigned int count;
    unsigned int capacity;
} CliLanguages;

typedef struct
{
    void *context;
    const CliPlugin *plugin;
} CliEachPluginCfg;

// every plugin handed to `each`: those that come with orior, then the reader's, each in the order of its id
void cli_plugins_each(const CliPluginsEachCfg *args);
// the language each extension opens as, by the language plugins, a reader's plugin standing in for one that comes with
// orior, a key whose capacity could not be taken left out; the same for a caller giving it back within its call, from
// the temporary tier; and the languages' cell given back, either way
void cli_plugins_languages(const CliPluginsCfg *args);
void cli_plugins_languages_for_call(const CliPluginsCfg *args);
// the grammar of the language plugin `id` stored for the colorer under `plugin:<id>`, its key written into `key`,
// sized for CLI_NAME_MAX_LEN: read from the plugin.json of the plugin of that id that stands, and read again only where
// that text changed since it was stored. 1, or 0 with why given (plugins_grammar.c)
int cli_plugins_grammar_store(const CliPluginsIdKeyCfg *args);
// the language the file at `path` opens as, of those `languages` gives, into `id`, sized for CLI_NAME_MAX_LEN: by
// its whole name where a plugin names it, by a plugin's pattern of paths where one takes it, and by its extension where
// neither does. 1, or 0 where none does
int cli_plugins_language_for(const CliPluginsLanguagesPathCfg *args);
// the tree a plugin's tables are read from, as the window opens it; and that tree into `tree`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 where none is open (plugins_tables.c)
void cli_plugins_set_tree(const CliPluginsRootCfg *args);
int cli_plugins_tree(const CliPluginsTreeCfg *args);
// The grammar at `grammar` in `json` made whole into `out` as JSON, before the colorer stores it: each word list a
// table of the tree `tree` fills, `{"table", "columns", "rows": {"column", "match", "skip"}, "match", "skip"}`, read
// from the table, `{name}` in its path `name`: the cell of each row `rows` saves in each of `columns`, the first group
// of `match` or its whole match, none `skip` matches, none where the table does not read; and each rule whose pattern
// is a word list, `{"words", "before", "after"}`, given the list's words, escaped, the longest first, between `before`
// and `after` as its pattern, a rule whose list has none left out. 1, or 0 with why given (plugins_tables.c)
int cli_plugins_grammar_resolved(const CliPluginsErrMsgJsonCfg *args);
// whether the folder `name` is one of the languages of the plugin whose `each`, `{"folder", "table", "unless":
// {"column", "match"}, "except"}`, is at `each` in `json`: its table read from the folder of that name in `folder` of
// the tree `tree`, no row's cell in `unless`'s column matched by its pattern, and the name not among `except`
// (plugins_tables.c)
int cli_plugins_folder_exists(const CliPluginsJsonCfg *args);
// the code the script plugin `id` runs into `out`: the file its plugin.json names as `main`, or each of a list of them
// there in order, joined by a line's end, plugin.js where it names none, in the plugin's own folder, or the code built
// into the program for one that comes with orior. A name that leaves the folder is refused. 1, or 0 with why given
// (plugins.c); and the same code read for the plugin `id` of the folder `folder` whose plugin.json is `size` bytes of
// `text` (plugins_code.c)
int cli_plugins_code_get(const CliPluginsIdCfg *args);
int cli_plugins_code_in(const CliPluginsIdCfg *args);
// a name as an id into `id`, sized for CLI_NAME_MAX_LEN: its letters and digits in lower case, every run of anything
// else one dash
void cli_plugins_slug(const CliPluginsIdNameCfg *args);
// The plugin the answers at `spec` in `json` request, `{name, id, extensions, from, lineComment, blockComment,
// keywords, types, constants, quotes}` each of which may be left out, made in `json`: its node, or CLI_JSON_NONE with
// why given; and a tree allocated into `draft`, released through persistent_buf_release, sized to the answers at `spec`
// in `json`, to the plugin they start from and to what a draft of them makes, the answers copied into it: their node
// there, or CLI_JSON_NONE with why given (plugins_draft.c)
unsigned int cli_plugins_draft(const CliPluginsErrMsgJsonSpecKindCfg *args);
unsigned int cli_plugins_draft_tree(const CliPluginsErrMsgJsonSpecCfg *args);
// a plugin as its file lays out it: each value on one line where it fits in 110 characters; and a few lines in its
// language to open and try, a comment, a keyword, a type, a constant, a string and a number, each where the plugin has
// one, each ended by "\n" (plugins_text.c)
void cli_plugins_text_get(const CliPluginsJsonOutCfg *args);
void cli_plugins_sample_get(const CliPluginsJsonOutCfg *args);
// the plugin the answers request written under `dir`, in a folder of its id, with a sample file beside it, the folder
// into `folder`, sized for CLI_PATH_MAX_LEN: a plugin of that id already there is retained unless `replace`. And the
// same under the reader's plugins folder. 1, or 0 with why given
int cli_plugins_create_in(const CliPluginsErrMsgCfg *args);
int cli_plugins_create(const CliPluginsErrMsgCfg *args);

struct CliPluginsIdCfg
{
    const char *id;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *folder_read;
    const char *text;
    unsigned long long size;
};

struct CliPluginsErrMsgCfg
{
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int spec;
    const char *dir;
    int replace;
    char *folder;
};

struct CliPluginsErrMsgJsonCfg
{
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int grammar;
    const char *tree;
    const char *name;
    VerbaLinea *out;
};

struct CliPluginsErrMsgJsonSpecCfg
{
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int spec;
    CliJson *draft;
};

struct CliPluginsJsonCfg
{
    const CliJson *json;
    const char *tree;
    const char *name;
    unsigned int each_u32;
};

struct CliPluginsErrMsgJsonSpecKindCfg
{
    VerbaLinea *err_msg;
    CliJson *json_out;
    unsigned int spec;
};

struct CliPluginsIdKeyCfg
{
    const char *id;
    char *key;
    VerbaLinea *err_msg;
};

struct CliPluginsLanguagesPathCfg
{
    const CliLanguages *languages_read;
    const char *path;
    char *id_out;
};

struct CliPluginsJsonOutCfg
{
    const CliJson *json;
    VerbaLinea *out;
    unsigned int plugin;
};

struct CliPluginsEachCfg
{
    void (*each)(const CliEachPluginCfg *args);
    void *context;
};

struct CliPluginsIdNameCfg
{
    char *id_out;
    const char *name;
};

struct CliPluginsCfg
{
    CliLanguages *languages;
};

struct CliPluginsRootCfg
{
    const char *root;
};

struct CliPluginsTreeCfg
{
    char *tree_out;
};

typedef struct
{
    void (*each)(const CliPluginsEachCfg *args);
} CliPluginsEachNs;
EMBED_TABLE_LAYOUT(CliPluginsEachNs, each);
EMBED_TABLE_STORAGE CliPluginsEachNs cli_plugins_each_table EMBED_UNUSED = {
    .each = cli_plugins_each,
};

typedef struct
{
    void (*languages)(const CliPluginsCfg *args);
    void (*languages_for_call)(const CliPluginsCfg *args);
} CliPluginsNs;
EMBED_TABLE_LAYOUT(CliPluginsNs, languages, languages_for_call);
EMBED_TABLE_STORAGE CliPluginsNs cli_plugins_table EMBED_UNUSED = {
    .languages = cli_plugins_languages,
    .languages_for_call = cli_plugins_languages_for_call,
};

typedef struct
{
    int (*grammar_store)(const CliPluginsIdKeyCfg *args);
} CliPluginsIdKeyNs;
EMBED_TABLE_LAYOUT(CliPluginsIdKeyNs, grammar_store);
EMBED_TABLE_STORAGE CliPluginsIdKeyNs cli_plugins_id_key EMBED_UNUSED = {
    .grammar_store = cli_plugins_grammar_store,
};

typedef struct
{
    int (*language_for)(const CliPluginsLanguagesPathCfg *args);
} CliPluginsLanguagesPathNs;
EMBED_TABLE_LAYOUT(CliPluginsLanguagesPathNs, language_for);
EMBED_TABLE_STORAGE CliPluginsLanguagesPathNs cli_plugins_languages_path EMBED_UNUSED = {
    .language_for = cli_plugins_language_for,
};

typedef struct
{
    void (*set_tree)(const CliPluginsRootCfg *args);
} CliPluginsRootNs;
EMBED_TABLE_LAYOUT(CliPluginsRootNs, set_tree);
EMBED_TABLE_STORAGE CliPluginsRootNs cli_plugins_root EMBED_UNUSED = {
    .set_tree = cli_plugins_set_tree,
};

typedef struct
{
    int (*tree)(const CliPluginsTreeCfg *args);
} CliPluginsTreeNs;
EMBED_TABLE_LAYOUT(CliPluginsTreeNs, tree);
EMBED_TABLE_STORAGE CliPluginsTreeNs cli_plugins_tree_table EMBED_UNUSED = {
    .tree = cli_plugins_tree,
};

typedef struct
{
    int (*grammar_resolved)(const CliPluginsErrMsgJsonCfg *args);
} CliPluginsErrMsgJsonNs;
EMBED_TABLE_LAYOUT(CliPluginsErrMsgJsonNs, grammar_resolved);
EMBED_TABLE_STORAGE CliPluginsErrMsgJsonNs cli_plugins_err_msg_json EMBED_UNUSED = {
    .grammar_resolved = cli_plugins_grammar_resolved,
};

typedef struct
{
    int (*folder_exists)(const CliPluginsJsonCfg *args);
} CliPluginsJsonNs;
EMBED_TABLE_LAYOUT(CliPluginsJsonNs, folder_exists);
EMBED_TABLE_STORAGE CliPluginsJsonNs cli_plugins_json EMBED_UNUSED = {
    .folder_exists = cli_plugins_folder_exists,
};

typedef struct
{
    int (*code_get)(const CliPluginsIdCfg *args);
    int (*code_in)(const CliPluginsIdCfg *args);
} CliPluginsIdNs;
EMBED_TABLE_LAYOUT(CliPluginsIdNs, code_get, code_in);
EMBED_TABLE_STORAGE CliPluginsIdNs cli_plugins_id EMBED_UNUSED = {
    .code_get = cli_plugins_code_get,
    .code_in = cli_plugins_code_in,
};

typedef struct
{
    void (*slug)(const CliPluginsIdNameCfg *args);
} CliPluginsIdNameNs;
EMBED_TABLE_LAYOUT(CliPluginsIdNameNs, slug);
EMBED_TABLE_STORAGE CliPluginsIdNameNs cli_plugins_id_name EMBED_UNUSED = {
    .slug = cli_plugins_slug,
};

typedef struct
{
    unsigned int (*draft)(const CliPluginsErrMsgJsonSpecKindCfg *args);
} CliPluginsErrMsgJsonSpecKindNs;
EMBED_TABLE_LAYOUT(CliPluginsErrMsgJsonSpecKindNs, draft);
EMBED_TABLE_STORAGE CliPluginsErrMsgJsonSpecKindNs cli_plugins_err_msg_json_spec_kind EMBED_UNUSED = {
    .draft = cli_plugins_draft,
};

typedef struct
{
    unsigned int (*draft_tree)(const CliPluginsErrMsgJsonSpecCfg *args);
} CliPluginsErrMsgJsonSpecNs;
EMBED_TABLE_LAYOUT(CliPluginsErrMsgJsonSpecNs, draft_tree);
EMBED_TABLE_STORAGE CliPluginsErrMsgJsonSpecNs cli_plugins_err_msg_json_spec EMBED_UNUSED = {
    .draft_tree = cli_plugins_draft_tree,
};

typedef struct
{
    void (*text_get)(const CliPluginsJsonOutCfg *args);
    void (*sample_get)(const CliPluginsJsonOutCfg *args);
} CliPluginsJsonOutNs;
EMBED_TABLE_LAYOUT(CliPluginsJsonOutNs, text_get, sample_get);
EMBED_TABLE_STORAGE CliPluginsJsonOutNs cli_plugins_json_out EMBED_UNUSED = {
    .text_get = cli_plugins_text_get,
    .sample_get = cli_plugins_sample_get,
};

typedef struct
{
    int (*create_in)(const CliPluginsErrMsgCfg *args);
    int (*create)(const CliPluginsErrMsgCfg *args);
} CliPluginsErrMsgNs;
EMBED_TABLE_LAYOUT(CliPluginsErrMsgNs, create_in, create);
EMBED_TABLE_STORAGE CliPluginsErrMsgNs cli_plugins_err_msg EMBED_UNUSED = {
    .create_in = cli_plugins_create_in,
    .create = cli_plugins_create,
};

#endif
