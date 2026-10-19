// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins.h: plugins, what the window takes in from outside its own code (plugins.c finds and reads them,
// plugins_draft.c writes one from a few answers, plugins_text.c lays one out and creates it). A language plugin is a
// folder named by its id holding plugin.json:
//
//   id, name, version       the plugin's own name for itself, the name it shows, and its version
//   kind                    "language", "tool" for a tool plugin, which the validation reads, or "script" for a
//                           plugin that runs code of its own, which the window's scripts.js runs: the file `main`
//                           names beside plugin.json, plugin.js where it names none, in the form of orior's calls
//                           `api` names
//   extensions              the file extensions it opens, without the dot
//   names                   the whole names of the files it opens beside them, where it has any
//   comments                { line, block: [open, close] }, either left out where it has none
//   pairs, quotes           the brackets it closes and the quotes it pairs
//   indentAfter             a pattern a line ends with to indent the next deeper
//   grammar                 lists of words by name and the tokenizer, as editor/tokens.js reads one; a pattern is
//                           a string, or { pattern, flags } where it takes flags
//   hovers                  a word and the text hovering it shows, where it has any
//   snippets                { label, body } for the completion list, `${1:name}` a stop in a body
//   reads, writes           for a script plugin, the patterns of the tree's files it reads and writes, `*` any
//                           letters but `/` and `**` any folders, and `reaches`, the hosts it reaches on the
//                           network, `*.` before a host for any under it; the window holds it to these, and runs it
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

#include "../text/json_tree.h"

// the most keys of extensions, names and paths the languages map, and the most plugins the reader keeps
#define CLI_LANGUAGE_KEYS 8192u

#define CLI_USER_PLUGINS 1024u

// the most bytes of a plugin's text and of its sample, as a plugin is written
#define CLI_PLUGIN_FILE (1u << 22u)

// A plugin that comes with orior: its id, and the text of its plugin.json, `size` bytes
typedef struct
{
    const char *id;
    const unsigned char *text;
    unsigned long long size;
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

// The language each extension, whole name and pattern of paths opens as: `/<name>` for a whole name, `?<pattern>` for
// a pattern, and the extension itself; each key's language id, a later plugin's standing in for an earlier's
typedef struct
{
    char key[CLI_LANGUAGE_KEYS][CLI_NAME_LONGEST];
    char id[CLI_LANGUAGE_KEYS][CLI_NAME_LONGEST];
    unsigned int count;
} CliLanguages;

// every plugin handed to `each`: those that come with orior, then the reader's, each in the order of its id
void cli_plugins_each(void (*each)(void *context, const CliPlugin *plugin), void *context);
// the language each extension opens as, by the language plugins, a reader's plugin standing in for one that comes with
// orior
void cli_plugins_languages(CliLanguages *languages);
// the language the file at `path` opens as, of those `languages` gives, into `id`, which holds CLI_NAME_LONGEST: by
// its whole name where a plugin names it, by a plugin's pattern of paths where one takes it, and by its extension where
// neither does. 1, or 0 where none does
int cli_plugins_language_for(const CliLanguages *languages, const char *path, char *id);
// the code the script plugin `id` runs into `out`: the file its plugin.json names as `main`, plugin.js where it names
// none, in the plugin's own folder. A name that leaves the folder is refused. 1, or 0 with why said
int cli_plugins_code_of(const char *id, CliLine *out, CliLine *said);
// a name as an id into `id`, which holds CLI_NAME_LONGEST: its letters and digits in lower case, every run of anything
// else one dash
void cli_plugins_slug(const char *name, char *id);
// The plugin the answers at `spec` in `json` ask for, `{name, id, extensions, from, lineComment, blockComment,
// keywords, types, constants, quotes}` each of which may be left out, made in `json`: its node, or CLI_JSON_NONE with
// why said (plugins_draft.c)
unsigned int cli_plugins_draft(CliJson *json, unsigned int spec, CliLine *said);
// a plugin as its file holds it: each value on one line where it fits in 110 characters (plugins_text.c)
void cli_plugins_text_of(const CliJson *json, unsigned int plugin, CliLine *out);
// the plugin the answers ask for written under `dir`, in a folder of its id, with a sample file beside it, the folder
// into `folder`, which holds CLI_PATH_LONGEST: a plugin of that id already there is kept unless `replace`. And the same
// under the reader's plugins folder. 1, or 0 with why said
int cli_plugins_create_in(const char *dir, CliJson *json, unsigned int spec, int replace, char *folder, CliLine *said);
int cli_plugins_create(CliJson *json, unsigned int spec, int replace, char *folder, CliLine *said);

#endif
