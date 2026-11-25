// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins.h: plugins, what the window takes in from outside its own code. plugins.c finds and reads them.
// plugins_draft.c writes one from a few answers, and plugins_text.c lays one out and creates it. A language plugin is
// a folder named by its id. The folder contains plugin.json, with these members:
//
//   id, name, version       The plugin's own name for itself, the name it shows, and its version.
//
//   type                    "language", "tool" or "script". The validation reads a tool plugin. A script plugin has
//                           code of its own: the file that `main` names beside plugin.json, or plugin.js where `main`
//                           names none. `api` names the form of orior's calls that the code uses.
//
//   extensions              The file extensions that the plugin opens, without the dot.
//
//   names                   The whole names of the files that the plugin opens, where it has any.
//
//   each                    { folder, table, unless, except }. The plugin makes a language for each folder of
//                           `folder` in the tree that holds `table`. `{name}` in it is the folder's name. The
//                           language `<id>:<name>` opens the folder's name as an extension.
//
//   comments                { line, block: [open, close] }. The plugin leaves out a member where it has none.
//
//   pairs, quotes           The brackets that the plugin closes and the quotes that it pairs.
//
//   indentAfter             A pattern at the end of a line. The next line goes one level deeper.
//
//   delimiters              { extension: characters } for each extension whose files are tables. The first character
//                           divides a line into fields. The second character, where it is present, encloses a field.
//                           The view puts each field at its column.
//
//   grammar                 Lists of words by name, and the tokenizer that the highlighter reads. A pattern is a
//                           string, or { pattern, flags } where it takes flags. A list can come from a table of the
//                           tree, and a pattern can be the words of a list (cli_plugins_grammar_resolved).
//
//   hovers                  A word and the text that a hover on it shows, where the plugin has any.
//
//   snippets                { label, body } for the completion list. `${1:name}` is a stop in a body.
//
//   reads, writes           The patterns of the tree's files that a script plugin reads and writes. `*` is any
//                           characters but `/`, and `**` is any folders. `reaches` holds the hosts that the plugin
//                           reaches on the network, and `*.` before a host means any host under it. `secrets` holds
//                           the keys of the secrets that the plugin keeps in the system's keychain, and `*` is every
//                           key. The plugin gets only these, and only after the reader permits it.
//
// The plugins that come with orior are under ui/plugins, and the build puts them into the program (plugins_bundled.c,
// from bundle/plugins_bundle.c). The reader's plugins are under `plugins/` in orior's own folder. A reader's plugin
// takes the place of the plugin of the same id that comes with orior.
//
// The generator writes a language plugin from a few answers. The answers give a name, the extensions, the comment
// marks of a line and a block, the keywords, types and constants, and the quotes. With a plugin to start from, the
// generator copies that plugin's grammar and changes what the answers name. Beside plugin.json it writes a sample file
// in the new language to open and try
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
typedef struct CliPluginsErrMsgJsonSpecTypeCfg CliPluginsErrMsgJsonSpecTypeCfg;
typedef struct CliPluginsIdKeyCfg CliPluginsIdKeyCfg;
typedef struct CliPluginsLanguagesPathCfg CliPluginsLanguagesPathCfg;
typedef struct CliPluginsGrammarOfCfg CliPluginsGrammarOfCfg;
typedef struct CliPluginsJsonOutCfg CliPluginsJsonOutCfg;
typedef struct CliPluginsJsonWriteCfg CliPluginsJsonWriteCfg;
typedef struct CliPluginsEachCfg CliPluginsEachCfg;
typedef struct CliPluginsIdEachCfg CliPluginsIdEachCfg;
typedef struct CliPluginColorCfg CliPluginColorCfg;
typedef struct CliPluginsEachIdCfg CliPluginsEachIdCfg;
typedef struct CliPluginIdCfg CliPluginIdCfg;
typedef struct CliPluginsIdNameValueCfg CliPluginsIdNameValueCfg;
typedef struct CliPluginsIdNameCfg CliPluginsIdNameCfg;
typedef struct CliPluginsRootCfg CliPluginsRootCfg;
typedef struct CliPluginsTreeCfg CliPluginsTreeCfg;
typedef struct CliPluginsSettingsOfCfg CliPluginsSettingsOfCfg;
typedef struct CliPluginsSettingCfg CliPluginsSettingCfg;
typedef struct CliPluginsSettingSetCfg CliPluginsSettingSetCfg;

// the max keys of extensions, names and paths the languages contain, and the max plugins the reader installs
#define CLI_LANGUAGE_PAIRS 4096u
#define CLI_USER_PLUGINS 1024u
// the max languages with a grammar, and the max bytes of a directive's mark
#define CLI_LANGUAGE_GRAMMARS 1024u
#define CLI_LANGUAGE_MARK_MAX 16u

// The grammar of one language as the caller reads it. The record holds the language's id, and the key of the grammar
// in the colorer. It also holds whether the tokens name a declaration. The last two members are the marks at the start
// of a directive's line and of its next line, empty for none
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    char key[CLI_NAME_MAX_LEN];
    int declares;
    char directive_mark[CLI_LANGUAGE_MARK_MAX];
    char directive_continued[CLI_LANGUAGE_MARK_MAX];
} CliLanguageGrammar;

// the bytes for a plugin's text and for its sample, as the generator writes a plugin
#define CLI_PLUGIN_TEXT_CAPACITY (1ULL << 20u)

// A plugin that comes with orior: its id, and the text of its plugin.json, `size` bytes. The record also holds the code
// of a script plugin, `code_size` bytes, NULL for a plugin with no code
typedef struct
{
    const char *id;
    const unsigned char *text;
    unsigned long long size;
    const unsigned char *code;
    unsigned long long code_size;
} CliBundled;

// the plugins in the program, in the order of their ids (plugins_bundled.c, which the build writes)
extern const CliBundled g_cli_bundled[];
extern const unsigned int g_cli_bundled_count;

// A plugin as found: its id, and where it came from, "bundled" or "user". The record holds the folder of a reader's
// own, NULL for one that comes with orior, and the text of its plugin.json. `json` is the tree of its settings and
// `root` its root, as the plugins module keeps them in memory. `json` is NULL where the text is no JSON. A reader reads
// the tree and does not read the text again (plugins_settings.c)
typedef struct
{
    const char *id;
    const char *source;
    const char *folder;
    const char *text;
    unsigned long long size;
    const CliJson *json;
    unsigned int root;
} CliPlugin;

typedef struct
{
    void *context;
    const CliPlugin *plugin;
} CliEachPluginCfg;

// every plugin passed to `each`: those that come with orior, then the reader's, each in the order of its id. Each one
// comes from the settings the plugins module keeps, with its tree
void cli_plugins_each(const CliPluginsEachCfg *args);
// every plugin passed to `each` as its file holds it, the reader's read from their folder, with no tree. Only the
// build of the settings walks the files (plugins.c)
void cli_plugins_files_each(const CliPluginsEachCfg *args);

// The settings of the plugins (plugins_settings.c). The plugins module reads each plugin's plugin.json once into a tree
// and keeps it in memory. A reader gets the tree and reaches a setting by its dotted key, as `comments.line` or
// `comments.block.0`. A key part that is a number reaches the item at that place of an array. A member name can carry
// dots, as `palette.keyword.control`, and the longest member name that fits wins.
//
// A change of a setting changes the kept tree in place. A change under the reader's plugins folder reads every plugin
// again.
//
// every plugin passed to `each` from the kept settings, as cli_plugins_each passes them
void cli_plugins_settings_each(const CliPluginsEachCfg *args);
// the last plugin of the id `id` passed to `each`. The reader's own takes the place of one that comes with orior. 1, or
// 0 where no plugin of the id is
int cli_plugins_settings_of(const CliPluginsSettingsOfCfg *args);
// the node of `json` that the dotted key `key` reaches from the node `node`: the node, or CLI_JSON_NONE where the key
// reaches none
unsigned int cli_plugins_setting(const CliPluginsSettingCfg *args);
// the member `name` made the string `value`, in place of the value it had. The member is in the object that the dotted
// key `object` reaches in the kept settings of the plugin `id`. 1, or 0 where no plugin of the id is, the key reaches
// no object, or the tree has no room
int cli_plugins_setting_set(const CliPluginsSettingSetCfg *args);
// every plugin's settings read again from its plugin.json, the kept ones given back; and every kept one given back as
// the program ends
void cli_plugins_settings_changed(void);
void cli_plugins_settings_end(void);
// The language that each extension, whole name and pattern of paths opens as. The call reads it again from the
// language plugins into the table of the plugins module. A reader's plugin takes the place of one that comes with
// orior. The next call of cli_plugins_language_for reads the new table.
//
// The watch makes the reader's plugins folder where it is absent, and keeps it open from the window's start to its
// end. A change under the folder reads the table again on the watch's thread. The last call gives back the table as
// the program ends
void cli_plugins_languages_changed(void);
void cli_plugins_watch_start(void);
void cli_plugins_languages_end(void);
// the grammar of the language plugin `id` stored for the colorer under `plugin:<id>`, with its key in `key`, sized for
// CLI_NAME_MAX_LEN. The grammar comes from the kept settings of the plugin of that id. The call stores it again only
// where the whole grammar changed since the last store. 1, or 0 with why given (plugins_grammar.c)
int cli_plugins_grammar_store(const CliPluginsIdKeyCfg *args);
// One color of a language plugin's palette: the name of the token that it colors, as the grammar names tokens. The
// value is as the plugin writes it, `#rrggbbaa`. The alpha is the token's luminosity over the editor's ground
struct CliPluginColorCfg
{
    void *context;
    const char *name;
    const char *value;
};
typedef void (*CliPluginColorEach)(const CliPluginColorCfg *args);
// each color of the palette of the language plugin `id` passed to `each` with `context`, in the order of the plugin.
// The reader's copy of a plugin that comes with orior takes its place. 1 where a plugin of the id has a palette, else
// 0 (plugins_palette.c)
int cli_plugins_palette_each(const CliPluginsIdEachCfg *args);
struct CliPluginIdCfg
{
    void *context;
    const char *id;
};
typedef void (*CliPluginIdEach)(const CliPluginIdCfg *args);
// the id of each plugin with a palette passed to `each` with `context`, in the order of the plugins. A reader's plugin
// that takes the place of one that comes with orior comes after it (plugins_palette.c)
void cli_plugins_palettes_each(const CliPluginsEachIdCfg *args);
// the color `name` of the palette of the plugin `id` made `value`. The call then writes the plugin's whole plugin.json
// into a folder of the id under the reader's plugins folder. That file takes the place of the one that comes with
// orior. 1, or 0 where no plugin of the id has a palette or the write fails (plugins_palette.c)
int cli_plugins_palette_set(const CliPluginsIdNameValueCfg *args);
// the language that the file at `path` opens as, from the table of the plugins module, into `id_out`, sized for
// CLI_NAME_MAX_LEN. The whole name comes first where a plugin names it, then a plugin's pattern of paths. The
// extension comes last. 1, or 0 where none fits. The first call reads the table from the plugins
int cli_plugins_language_for(const CliPluginsLanguagesPathCfg *args);
// the grammar of the language that the file at `path` opens as, or of the language `id`, into the record
// `grammar_out` (CliLanguageGrammar). Where `store` is 1, the grammar goes to the colorer at the first ask, and the
// plugins module records that it went. 1, or 0 where the language has no grammar or the colorer refuses it
int cli_plugins_grammar_of(const CliPluginsGrammarOfCfg *args);
// the tree that holds a plugin's tables, as the window opens it. The second call writes that tree into `tree`, sized
// for CLI_PATH_MAX_LEN: 1, or 0 where no tree is open (plugins_tables.c)
void cli_plugins_set_tree(const CliPluginsRootCfg *args);
int cli_plugins_tree(const CliPluginsTreeCfg *args);
// The grammar at `grammar` in `json` made whole into `out` as JSON, before the colorer stores it. 1, or 0 with why
// given (plugins_tables.c).
//
// A table of the tree `tree` fills a word list of this form:
// `{"table", "columns", "rows": {"column", "match", "skip"}, "match", "skip"}`. `{name}` in the table's path is `name`.
// The list gets the cell of each row that `rows` keeps, in each of `columns`. Each cell gives the first group of
// `match`, or the whole match. A cell that `skip` matches gives no word. A table that does not read gives no words.
//
// A rule whose pattern is a word list, `{"words", "before", "after"}`, gets the words of the list as its pattern. The
// call escapes the words and puts the longest first, between `before` and `after`. A rule whose list has no words goes
// out
int cli_plugins_grammar_resolved(const CliPluginsErrMsgJsonCfg *args);
// whether the folder `name` is one of the languages of a plugin. The plugin's `each` is at `each` in `json`, with the
// form `{"folder", "table", "unless": {"column", "match"}, "except"}`. The table comes from the folder `name` in
// `folder` of the tree `tree`. The pattern of `unless` matches no row's cell in its column. The name is not among
// `except` (plugins_tables.c)
int cli_plugins_folder_exists(const CliPluginsJsonCfg *args);
// the code of the script plugin `id` into `out`. The code is the file that its plugin.json names as `main`, or each
// file of a list there in order, with a line's end between. It is plugin.js where `main` names none. All the files are
// in the plugin's own folder, and the call refuses a name that leaves it. A plugin that comes with orior has its code
// in the program. 1, or 0 with why given (plugins.c).
//
// The second call reads the same code for the plugin `id` of the folder `folder`, whose plugin.json is `size` bytes of
// `text` (plugins_code.c)
int cli_plugins_code_get(const CliPluginsIdCfg *args);
int cli_plugins_code_in(const CliPluginsIdCfg *args);
// a name as an id into `id`, sized for CLI_NAME_MAX_LEN: its characters and digits in lower case, every run of anything
// else one dash
void cli_plugins_slug(const CliPluginsIdNameCfg *args);
// The plugin that the answers at `spec` in `json` request, made in `json`: its node, or CLI_JSON_NONE with why given.
// The answers are `{name, id, extensions, from, lineComment, blockComment, keywords, types, constants, quotes}`, and
// each can be absent (plugins_draft.c).
//
// The second call puts a tree into `draft`, and persistent_buf_release gives it back. The tree has room for the
// answers, for the plugin that they start from, and for what a draft of them makes. The call copies the answers into
// it: their node there, or CLI_JSON_NONE with why given
unsigned int cli_plugins_draft(const CliPluginsErrMsgJsonSpecTypeCfg *args);
unsigned int cli_plugins_draft_tree(const CliPluginsErrMsgJsonSpecCfg *args);
// a plugin as its file lays it out, each value on one line where it fits in 110 characters (plugins_text.c). The
// second call writes a few lines in the plugin's language to open and try. They hold a comment, a keyword, a type, a
// constant, a string and a number, where the plugin has each. Each line ends with "\n"
void cli_plugins_text_get(const CliPluginsJsonOutCfg *args);
void cli_plugins_sample_get(const CliPluginsJsonOutCfg *args);
// The plugin `plugin` of `json` written as plugin.json, laid out as cli_plugins_text_get lays it out, in a folder of
// its id under `dir`. The folder goes into `folder`, sized for CLI_PATH_MAX_LEN. The call keeps a plugin.json already
// there, unless `replace`. 1, or 0 with why given (plugins_text.c)
int cli_plugins_json_write(const CliPluginsJsonWriteCfg *args);
// the plugin that the answers request, written in a folder of its id under `dir`, with a sample file beside it. The
// folder goes into `folder`, sized for CLI_PATH_MAX_LEN. The call keeps a plugin of that id already there, unless
// `replace`. The second call does the same under the reader's plugins folder. 1, or 0 with why given
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

struct CliPluginsErrMsgJsonSpecTypeCfg
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

struct CliPluginsGrammarOfCfg
{
    const char *path;
    const char *id;
    int store;
    CliLanguageGrammar *grammar_out;
};

struct CliPluginsLanguagesPathCfg
{
    const char *path;
    char *id_out;
};

struct CliPluginsJsonOutCfg
{
    const CliJson *json;
    VerbaLinea *out;
    unsigned int plugin;
};

struct CliPluginsJsonWriteCfg
{
    const CliJson *json;
    unsigned int plugin;
    const char *dir;
    int replace;
    char *folder;
    VerbaLinea *err_msg;
};

struct CliPluginsEachCfg
{
    void (*each)(const CliEachPluginCfg *args);
    void *context;
};

struct CliPluginsIdEachCfg
{
    const char *id;
    CliPluginColorEach each;
    void *context;
};

struct CliPluginsEachIdCfg
{
    CliPluginIdEach each;
    void *context;
};

struct CliPluginsIdNameValueCfg
{
    const char *id;
    const char *name;
    const char *value;
};

typedef struct
{
    void (*palettes_each)(const CliPluginsEachIdCfg *args);
} CliPluginsEachIdNs;
EMBED_TABLE_LAYOUT(CliPluginsEachIdNs, palettes_each);
EMBED_TABLE_STORAGE CliPluginsEachIdNs cli_plugins_each_id EMBED_UNUSED = {
    .palettes_each = cli_plugins_palettes_each,
};

typedef struct
{
    int (*palette_set)(const CliPluginsIdNameValueCfg *args);
} CliPluginsIdNameValueNs;
EMBED_TABLE_LAYOUT(CliPluginsIdNameValueNs, palette_set);
EMBED_TABLE_STORAGE CliPluginsIdNameValueNs cli_plugins_id_name_value EMBED_UNUSED = {
    .palette_set = cli_plugins_palette_set,
};

struct CliPluginsIdNameCfg
{
    char *id_out;
    const char *name;
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

struct CliPluginsSettingsOfCfg
{
    const char *id;
    void (*each)(const CliEachPluginCfg *args);
    void *context;
};

struct CliPluginsSettingCfg
{
    const CliJson *json;
    unsigned int node;
    const char *key;
};

struct CliPluginsSettingSetCfg
{
    const char *id;
    const char *object;
    const char *name;
    const char *value;
};

typedef struct
{
    int (*of)(const CliPluginsSettingsOfCfg *args);
} CliPluginsSettingsOfNs;
EMBED_TABLE_LAYOUT(CliPluginsSettingsOfNs, of);
EMBED_TABLE_STORAGE CliPluginsSettingsOfNs cli_plugins_settings_of_table EMBED_UNUSED = {
    .of = cli_plugins_settings_of,
};

typedef struct
{
    unsigned int (*setting)(const CliPluginsSettingCfg *args);
} CliPluginsSettingNs;
EMBED_TABLE_LAYOUT(CliPluginsSettingNs, setting);
EMBED_TABLE_STORAGE CliPluginsSettingNs cli_plugins_setting_table EMBED_UNUSED = {
    .setting = cli_plugins_setting,
};

typedef struct
{
    int (*set)(const CliPluginsSettingSetCfg *args);
} CliPluginsSettingSetNs;
EMBED_TABLE_LAYOUT(CliPluginsSettingSetNs, set);
EMBED_TABLE_STORAGE CliPluginsSettingSetNs cli_plugins_setting_set_table EMBED_UNUSED = {
    .set = cli_plugins_setting_set,
};

typedef struct
{
    int (*palette_each)(const CliPluginsIdEachCfg *args);
} CliPluginsIdEachNs;
EMBED_TABLE_LAYOUT(CliPluginsIdEachNs, palette_each);
EMBED_TABLE_STORAGE CliPluginsIdEachNs cli_plugins_id_each EMBED_UNUSED = {
    .palette_each = cli_plugins_palette_each,
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
    int (*grammar_of)(const CliPluginsGrammarOfCfg *args);
} CliPluginsLanguagesPathNs;
EMBED_TABLE_LAYOUT(CliPluginsLanguagesPathNs, language_for, grammar_of);
EMBED_TABLE_STORAGE CliPluginsLanguagesPathNs cli_plugins_languages_path EMBED_UNUSED = {
    .language_for = cli_plugins_language_for,
    .grammar_of = cli_plugins_grammar_of,
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
    unsigned int (*draft)(const CliPluginsErrMsgJsonSpecTypeCfg *args);
} CliPluginsErrMsgJsonSpecTypeNs;
EMBED_TABLE_LAYOUT(CliPluginsErrMsgJsonSpecTypeNs, draft);
EMBED_TABLE_STORAGE CliPluginsErrMsgJsonSpecTypeNs cli_plugins_err_msg_json_spec_type EMBED_UNUSED = {
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
    int (*write)(const CliPluginsJsonWriteCfg *args);
} CliPluginsJsonWriteNs;
EMBED_TABLE_LAYOUT(CliPluginsJsonWriteNs, write);
EMBED_TABLE_STORAGE CliPluginsJsonWriteNs cli_plugins_json_write_table EMBED_UNUSED = {
    .write = cli_plugins_json_write,
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
