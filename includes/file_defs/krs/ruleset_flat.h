// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_flat.h: the schema, a file and a read ruleset arranged for the .krs reader's core (ruleset_core.h), and what
// the core read stored as the host's Ruleset, which the host's reader and the device's share
#ifndef RULESET_FLAT_H
#define RULESET_FLAT_H

#include "ruleset_core.h"
#include "ruleset_reader.h"

#include <string>
#include <vector>

// the schema as the core reads it, in host memory. It holds the characters of each name end to end, with the names of
// each form, bank and fixed register among them. It also holds the count of parameters of each form, and the view of
// the core, which points into them
struct RulesetFlat
{
    std::vector<unsigned char> characters;
    std::vector<RulesetCoreSpan> forms;
    std::vector<unsigned int> form_parameters;
    std::vector<RulesetCoreSpan> banks;
    std::vector<RulesetCoreSpan> fixed;
    RulesetCoreSchema schema;
};

// the memory of a read in host memory, each list sized to its capacity
struct RulesetMemory
{
    std::vector<unsigned char> characters;
    std::vector<RulesetCoreSpan> pieces;
    std::vector<unsigned int> placeholders;
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
    std::vector<unsigned int> form_placeholder_first;
    std::vector<unsigned int> form_placeholders;
};

// the memory of a relation read in host memory. It holds the characters of the files end to end, and the span of each
// file with the name that a report knows it by. It also holds the entries, the marks and stack of the walk, and the
// relations, sized from the lines of the files
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

// the files at `paths`, `count` of them, read into `memory`. The check ruleset_core_relations reads them against each
// other, and against `schema` where it is not NULL, into `relations`, which points into `memory`. The result is 1, or
// 0 with its path in `error` where a file did not open
int ruleset_relations_read(const std::string *paths, unsigned int count, const RulesetCoreSchema *schema,
                           RulesetRelationMemory *memory, RulesetCoreRelations *relations, std::string *error);

// what a relation of type `type` states of its entry, as a report writes it after the entry
const char *ruleset_relation_text(unsigned int type);

// what a relation of a map of type `type` states of its entry, as a report writes it after the entry
const char *ruleset_map_text(unsigned int type);

// the max characters a ruleset's file contains, which limits the reader's capacities in 32 bits
#define RULESET_FILE_MAX (1u << 30u)

// the most files of a ruleset: its own, the .krs and .kdm of its part, and the .ksc beside it
#define RULESET_PATHS_MAX 4u

// The file at `path`, read whole onto the end of `text`. Where the program is of a run and the run stores the file, the
// read is as the run stores it (qry_member_file). Else the read comes from the disk. The result is 1, or 0 where
// neither has it.
// A program built without the run reads each file from the disk. The text stops once it reaches RULESET_FILE_MAX
// characters, which the caller refuses
int ruleset_bytes_read(const std::string &path, std::string *text);

// the files of the ruleset at `path`, into `paths` in the order that ruleset_text reads them:
//   - the file.
//   - where the file names a part on its line part <part>, the .krs and .kdm of the part.
//   - the .ksc beside the file, where it opens.
//
// The result is their count
unsigned int ruleset_paths(const RulesetSchema *schema, const std::string &path, std::string paths[RULESET_PATHS_MAX]);

// the ruleset at `path`, read whole into `text`. Where the file names a part, the .krs of the part comes after it,
// without its krs 1 line. The ruleset lines of the .kdm of the part come after that. Last come the ruleset lines of
// the .ksc beside the file, where it has one.
// The result is 1 where each read succeeded. It is 0, with the reason in `error`, where one did not open or the whole
// is more than the reader supports
int ruleset_text(const RulesetSchema *schema, const std::string &path, std::string *text, std::string *error);

// `rules` begun against its schema, as the reader of the host begins it, with its text read into `text` by
// ruleset_text. The result is 1 where the read succeeded. It is 0, with the reason in rules->error, where it failed
int ruleset_file(Ruleset *rules, const std::string &path, std::string *text);

void ruleset_flat_schema(const RulesetSchema *schema, RulesetFlat *flat);

// `read`'s capacities for a file of `text_length` characters: no read of the file passes them
void ruleset_capacities(unsigned int text_length, RulesetCoreRead *read);

// `memory` sized to `read`'s capacities and the schema's counts, and `read` pointed at it
void ruleset_memory_size(RulesetMemory *memory, RulesetCoreRead *read);

// what `read` read, from the file `text`, stored in `rules` as the host's reader stored it, with the reason it ended
// where it did not read the file
void ruleset_store(const RulesetCoreRead *read, const std::string &text, Ruleset *rules);

// `rules`' constructs arranged for the core's scratch, each argument's text left out, which the scratch does not read
void ruleset_flat_constructs(const Ruleset *rules, RulesetFlatConstructs *flat);

#endif
