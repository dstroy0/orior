// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_flat.h: the schema, a file and a read ruleset arranged for the .krs reader's core (ruleset_core.h), and what
// the core read stored as the host's Ruleset, which the host's reader and the device's share
#ifndef RULESET_FLAT_H
#define RULESET_FLAT_H

#include "ruleset_core.h"
#include "ruleset_reader.h"

// A run of lists in one cell. A pass with no base counts the bytes of the lists in `used`. A pass with a base puts each
// list at its offset from the base, and the offset is the same on every pass. A cell of host memory or of device
// memory can be the base
struct RulesetCellPlan
{
    unsigned char *base;
    unsigned long long used;
};

// `count` elements of `size` bytes each, at the next offset of `plan` that 16 divides. NULL on a pass with no base
static inline void *ruleset_cell_place(RulesetCellPlan *plan, unsigned long long count, unsigned long long size)
{
    const unsigned long long at = (plan->used + 15ULL) & ~15ULL;
    plan->used = at + (count * size);
    return (plan->base != NULL) ? (void *)(plan->base + at) : NULL;
}

// the schema as the core reads it, in one cell of host memory. The cell holds the characters of each name end to end.
// It holds the span of each form, bank and fixed register among them, and the count of parameters of each form.
// `schema` is the view of the core, which points into the cell
struct RulesetFlat
{
    void *cell;
    unsigned long long bytes;
    RulesetCoreSchema schema;
};

// the memory of a read, each list sized to its capacity, in one cell of host memory of `bytes`
struct RulesetMemory
{
    void *cell;
    unsigned long long bytes;
};

// The constructs of a read ruleset as the core's scratch reads them, in one cell of host memory of `bytes`. The cell
// holds the constructs, their lines and the arguments of the lines. It also holds the placeholders of each form end
// to end, with the place of the first of each form. The counts are those of the lines, the arguments and the
// placeholders
struct RulesetFlatConstructs
{
    void *cell;
    unsigned long long bytes;
    RulesetCoreConstruct *constructs;
    RulesetCoreLine *lines;
    RulesetCoreArgument *arguments;
    unsigned int *form_placeholder_first;
    unsigned int *form_placeholders;
    unsigned int form_count;
    unsigned int line_count;
    unsigned int argument_count;
    unsigned int placeholder_count;
};

// the memory of a relation read, in one cell of host memory. It holds the characters of the files end to end, and the
// span of each file with the name that a report knows it by. It also holds the entries, the marks and stack of the
// walk, and the relations, sized from the lines of the files
struct RulesetRelationMemory
{
    void *cell;
    const char **labels;
};

// the files at `paths`, `count` of them, read into `memory`. The check ruleset_core_relations reads them against each
// other, and against `schema` where it is not NULL, into `relations`, which points into `memory`. The result is 1, or
// 0 with the path of a file that did not open on `error`
int ruleset_relations_read(const char (*paths)[PLATFORM_PATH_BYTES], unsigned int count,
                           const RulesetCoreSchema *schema, RulesetRelationMemory *memory,
                           RulesetCoreRelations *relations, VerbaLinea *error);

// the cell of `memory` given back, after the last read of the relations that point into it
void ruleset_relation_memory_ended(RulesetRelationMemory *memory);

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
int ruleset_bytes_read(const char *path, PlatformText *text);

// a text begun, empty, for the files of a ruleset: it holds RULESET_FILE_MAX characters and one past them, which the
// reader refuses
void ruleset_text_begun(PlatformText *text);

// the files of the ruleset at `path`, into `paths` in the order that ruleset_text reads them:
//   - the file.
//   - where the file names a part on its line part <part>, the .krs and .kdm of the part.
//   - the .ksc beside the file, where it opens.
//
// The result is their count
unsigned int ruleset_paths(const RulesetSchema *schema, const char *path,
                           char paths[RULESET_PATHS_MAX][PLATFORM_PATH_BYTES]);

// the ruleset at `path`, read whole onto `text`, which the caller began empty. Where the file names a part, the .krs of
// the part comes after it, without its krs 1 line. The ruleset lines of the .kdm of the part come after that. Last
// come the ruleset lines of the .ksc beside the file, where it has one.
// The result is 1 where each read succeeded. It is 0, with the reason on `error`, where one did not open or the whole
// is more than the reader supports
int ruleset_text(const RulesetSchema *schema, const char *path, PlatformText *text, VerbaLinea *error);

// `rules` begun against its schema, as the reader of the host begins it, with its text read onto `text` by
// ruleset_text. The result is 1 where the read succeeded. It is 0, with the reason in rules->error, where it failed
int ruleset_file(Ruleset *rules, const char *path, PlatformText *text);

// The schema laid out on `plan` into `placed`. The names' characters come first, then the spans of the forms, the
// banks and the fixed registers, then the forms' counts of parameters
void ruleset_flat_layout(const RulesetSchema *schema, RulesetCellPlan *plan, RulesetCoreSchema *placed);

// the schema flattened into one cell of `flat`, which ruleset_flat_ended gives back
void ruleset_flat_schema(const RulesetSchema *schema, RulesetFlat *flat);

void ruleset_flat_ended(RulesetFlat *flat);

// `read`'s capacities for a file of `text_length` characters: no read of the file passes them
void ruleset_capacities(unsigned int text_length, RulesetCoreRead *read);

// the lists of `read` laid out on `plan`, each sized from its capacities and the schema's counts
void ruleset_memory_layout(RulesetCellPlan *plan, RulesetCoreRead *read);

// `memory` sized to `read`'s capacities and the schema's counts, and `read` pointed at it. 1, or 0 where the pool
// cannot meet the bytes
int ruleset_memory_size(RulesetMemory *memory, RulesetCoreRead *read);

void ruleset_memory_ended(RulesetMemory *memory);

// what `read` read, from the file `text`, stored in `rules` as the host's reader stored it, with the reason it ended
// where it did not read the file
void ruleset_store(const RulesetCoreRead *read, const char *text, Ruleset *rules);

// `rules`' constructs arranged for the core's scratch, each argument's text left out, which the scratch does not read.
// 1, or 0 where the pool cannot meet the bytes
int ruleset_flat_constructs(const Ruleset *rules, RulesetFlatConstructs *flat);

// the lists of `flat` pointed at by `arranged` at the same offsets from `base`, a cell of `flat->bytes`
void ruleset_flat_constructs_at(const RulesetFlatConstructs *flat, unsigned char *base, RulesetCoreScratch *arranged);

void ruleset_flat_constructs_ended(RulesetFlatConstructs *flat);

#endif
