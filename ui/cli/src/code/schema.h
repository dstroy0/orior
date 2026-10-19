// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema.h: JSON by its schema: a JSON file read with the place of each of its values, checked against the schema its
// `$schema` names, or the one its name is known by, and completed from it, each key the schema gives an object and each
// value it allows. A schema is checked against its own schema in turn: the meta-schemas of drafts 4, 6 and 7, 2019-09
// and 2020-12 come with orior.
//
// A schema is found by its path beside the file, by its URL among those orior holds, or in `schemas/` of orior's own
// folder, where a schema fetched is kept. One named by a URL and found in none of these is noted, and its note offers
// the line that fetches it with curl, run in the terminal; nothing is fetched without that press.
//
// The keywords read are those of the drafts from 4 to 2020-12 that say what a value is: `type`, `enum`, `const`, the
// bounds of numbers, strings, arrays and objects, `pattern`, `required`, `properties`, `patternProperties`,
// `additionalProperties`, `propertyNames`, `dependencies`, `dependentRequired`, `dependentSchemas`, `items`,
// `prefixItems`, `additionalItems`, `contains`, `uniqueItems`, `allOf`, `anyOf`, `oneOf`, `not`, `if`, `then`, `else`,
// `$ref` within a schema and across files, and `deprecated`. A comment, `//` or `/* */`, is passed over, as a JSON file
// with comments writes one (schema_*.c)
#ifndef SCHEMA_H
#define SCHEMA_H

#include "../text/edits.h"

// whether the file at `path` is JSON this reader reads, by its extension, `json` or `jsonc`
int cli_schema_reads(const char *path);

// the diagnostics of the JSON file at `path` of the tree at `root`, `size` bytes of its text: what kept it from
// reading, and what its schema finds, written as the protocol's array of them into `out`
void cli_schema_check(const char *root, const char *path, const char *text, unsigned long long size, CliLine *out);

// what completes the word before `line`, `col` of the JSON file at `path`, `size` bytes of its text: the keys its
// schema gives the object the cursor is in and that it does not hold yet, or the values the schema allows the value the
// cursor is at, into `items`
void cli_schema_complete(const char *path, const char *text, unsigned long long size, unsigned int line,
                         unsigned int col, CliCompletions *items);

#endif
