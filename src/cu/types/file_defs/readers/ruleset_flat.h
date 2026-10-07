// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_flat.h: the schema, a file and a read ruleset laid out for the .krs reader's core (ruleset_core.h), and what
// the core read kept as the host's Ruleset, which the host's reader and the device's share
#ifndef RULESET_FLAT_H
#define RULESET_FLAT_H

#include "ruleset_core.h"
#include "ruleset_reader.h"

#include <string>
#include <vector>

// the schema as the core reads it, in host memory: every name's letters end to end, each form's, bank's and fixed
// register's name among them, each form's count of parameters, and the core's view of them, which points into them
struct RulesetFlat
{
    std::vector<unsigned char> letters;
    std::vector<RulesetCoreSpan> forms;
    std::vector<unsigned int> form_parameters;
    std::vector<RulesetCoreSpan> banks;
    std::vector<RulesetCoreSpan> fixed;
    RulesetCoreSchema schema;
};

// the memory of a read in host memory, each list sized to its capacity
struct RulesetMemory
{
    std::vector<unsigned char> letters;
    std::vector<RulesetCoreSpan> pieces;
    std::vector<unsigned int> slots;
    std::vector<RulesetCoreSpan> words;
    std::vector<RulesetCoreTemplate> banks;
    std::vector<RulesetCoreSpan> fixed;
    std::vector<RulesetCoreTemplate> forms;
    std::vector<RulesetCoreConstruct> constructs;
    std::vector<RulesetCoreLine> lines;
    std::vector<RulesetCoreArgument> arguments;
    std::vector<unsigned char> bank_given;
    std::vector<unsigned char> fixed_given;
    std::vector<unsigned char> form_given;
    std::vector<RulesetCoreSpan> building_parameters;
};

// a read ruleset's constructs as the core's scratch reads them, in host memory
struct RulesetFlatConstructs
{
    std::vector<RulesetCoreConstruct> constructs;
    std::vector<RulesetCoreLine> lines;
    std::vector<RulesetCoreArgument> arguments;
};

// the memory of a relation read in host memory: the files' letters end to end, each file's span and the name a report
// knows it by, and the entries, the walk's marks and stack and the relations, sized from the files' lines
struct RulesetRelationMemory
{
    std::vector<unsigned char> text;
    std::vector<RulesetCoreSpan> files;
    std::vector<std::string> labels;
    std::vector<RulesetCoreEntry> entries;
    std::vector<unsigned int> marks;
    std::vector<unsigned int> stack;
    std::vector<RulesetCoreRelation> relations;
};

// the files at `paths`, `count` of them, read into `memory` and held against each other, and against `schema` where it
// is not NULL, by ruleset_core_relations into `relations`, which points into `memory`: 1, or 0 where a file could not
// be opened, and its path in `error`
int ruleset_relations_read(const std::string *paths, unsigned int count, const RulesetCoreSchema *schema,
                           RulesetRelationMemory *memory, RulesetCoreRelations *relations, std::string *error);

// what a relation of kind `kind` says of its entry, as a report writes it after the entry
const char *ruleset_relation_said(unsigned int kind);

// what a relation of a map of kind `kind` says of its entry, as a report writes it after the entry
const char *ruleset_map_said(unsigned int kind);

// the most letters a ruleset's file holds, which keeps the reader's capacities in 32 bits
#define RULESET_FILE_MAX (1u << 30u)

// the most files a ruleset is read from: its own, its part's .krs and .kdm, and the .ksc beside it
#define RULESET_PATHS_MAX 4u

// the files the ruleset at `path` is read from, into `paths` in the order ruleset_text reads them: the file, where
// the file names a part on its line part <part> the part's .krs and .kdm, and the .ksc beside the file where it opens.
// Their count returned
unsigned int ruleset_paths(const RulesetSchema *schema, const std::string &path, std::string paths[RULESET_PATHS_MAX]);

// the ruleset at `path` read whole into `text`, and where the file names a part, the part's .krs after it, its krs 1
// line left out, the ruleset lines of the part's .kdm after that, and last the ruleset lines of the .ksc beside the
// file where it has one: 1 where all were read; 0, and why in `error`, where one could not be opened or the whole is
// more than the reader holds
int ruleset_text(const RulesetSchema *schema, const std::string &path, std::string *text, std::string *error);

// `rules` begun against its schema, as the host's reader begins it, and its text read into `text` by ruleset_text: 1
// where it was read; 0, and why in rules->error, where it was not
int ruleset_file(Ruleset *rules, const std::string &path, std::string *text);

void ruleset_flat_schema(const RulesetSchema *schema, RulesetFlat *flat);

// `read`'s capacities for a file of `text_length` letters: no read of the file passes them
void ruleset_capacities(unsigned int text_length, RulesetCoreRead *read);

// `memory` sized to `read`'s capacities and the schema's counts, and `read` pointed at it
void ruleset_memory_size(RulesetMemory *memory, RulesetCoreRead *read);

// what `read` read, from the file `text`, kept in `rules` as the host's reader kept it, with the reason it ended where
// it did not read the file
void ruleset_keep(const RulesetCoreRead *read, const std::string &text, Ruleset *rules);

// `rules`' constructs laid out for the core's scratch, each argument's text left out, which the scratch does not read
void ruleset_flat_constructs(const Ruleset *rules, RulesetFlatConstructs *flat);

#endif
