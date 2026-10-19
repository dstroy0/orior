// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_bounds.c: a number checked against `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum` and
// `multipleOf`, draft 4's boolean exclusive bounds read beside them, the last bound it passes said; and a string
// against `minLength`, `maxLength`, counted in letters, and `pattern`
#include "schema_internal.h"

#include <math.h>

// a bound the number passes said: the number, `between`, the bound, and `after`
static void bounds_said(CliLine *said, double number, const char *between, double bound, const char *after)
{
    said->at = 0ull;
    cli_schema_number_shown(number, said);
    cli_line_text(said, between);
    cli_schema_number_shown(bound, said);
    cli_line_text(said, after);
}

void cli_schema_number_checked(CliSchemaCheck *check, unsigned int node, unsigned int map)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaNode *const one = &check->read->node[node];
    const double number = one->number;
    const int draft4_least = cli_schema_true(json, cli_schema_get(json, map, "exclusiveMinimum"));
    const int draft4_most = cli_schema_true(json, cli_schema_get(json, map, "exclusiveMaximum"));
    CliLine said = cli_build_said(check->found);
    double bound = 0.0;
    if (cli_schema_number(json, cli_schema_get(json, map, "minimum"), &bound) &&
        ((number < bound) || (draft4_least && (number == bound))))
    {
        bounds_said(&said, number, " is below the least the schema allows, ", bound,
                    draft4_least ? ", itself left out" : "");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "maximum"), &bound) &&
        ((number > bound) || (draft4_most && (number == bound))))
    {
        bounds_said(&said, number, " is above the most the schema allows, ", bound,
                    draft4_most ? ", itself left out" : "");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "exclusiveMinimum"), &bound) && (number <= bound))
    {
        bounds_said(&said, number, " is not above ", bound, ", as the schema asks");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "exclusiveMaximum"), &bound) && (number >= bound))
    {
        bounds_said(&said, number, " is not below ", bound, ", as the schema asks");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "multipleOf"), &bound) && (bound > 0.0))
    {
        const double ratio = number / bound;
        if (fabs(ratio - round(ratio)) > 1e-9)
        {
            bounds_said(&said, number, " is no multiple of ", bound, "");
        }
    }
    if (said.at > 0u)
    {
        (void)cli_build_finding(check->found, &said, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                "schema-range");
    }
}

void cli_schema_text_checked(CliSchemaCheck *check, unsigned int node, unsigned int map)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaNode *const one = &check->read->node[node];
    const char *const text = cli_schema_text(check->read, one->text_at);
    const unsigned long long length = cli_text_letters(text, one->text_size);
    unsigned long long bound = 0ull;
    if (cli_schema_u64(json, cli_schema_get(json, map, "minLength"), &bound) && (length < bound))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "This string is ");
        cli_line_decimal(&said, length, 1u);
        cli_line_text(&said, " long, shorter than the ");
        cli_line_decimal(&said, bound, 1u);
        cli_line_text(&said, " the schema asks for");
        (void)cli_build_finding(check->found, &said, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                "schema-length");
    }
    if (cli_schema_u64(json, cli_schema_get(json, map, "maxLength"), &bound) && (length > bound))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "This string is ");
        cli_line_decimal(&said, length, 1u);
        cli_line_text(&said, " long, longer than the ");
        cli_line_decimal(&said, bound, 1u);
        cli_line_text(&said, " the schema allows");
        (void)cli_build_finding(check->found, &said, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                "schema-length");
    }
    unsigned long long size = 0ull;
    const char *const pattern = cli_schema_string(json, map, "pattern", &size);
    if ((pattern != NULL) && (cli_schema_matches(pattern, size, text, one->text_size) == 0))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "This string does not match the schema's pattern `");
        cli_line_counted(&said, pattern, size);
        cli_line_character(&said, '`');
        (void)cli_build_finding(check->found, &said, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                "schema-pattern");
    }
}
