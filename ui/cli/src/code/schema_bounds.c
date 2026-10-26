// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_bounds.c: a number checked against `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum` and
// `multipleOf`, draft 4's boolean exclusive bounds read beside them, the last bound it passes reported; and a string
// against `minLength`, `maxLength`, counted in letters, and `pattern`
#include "schema_internal.h"

#include <math.h>

// a bound the number passes reported: the number, `between`, the bound, and `after`
static void bounds_msg_written(VerbaLinea *msg, double number, const char *between, double bound, const char *after)
{
    msg->at = 0ull;
    cli_schema_number_shown(number, msg);
    mmgr_line_put(msg, between);
    cli_schema_number_shown(bound, msg);
    mmgr_line_put(msg, after);
}

void cli_schema_number_checked(CliSchemaCheck *check, unsigned int node, unsigned int map)
{
    const CliJson *const json = &check->store->json;
    const CliSchemaNode *const one = &check->read->node[node];
    const double number = one->number;
    const int draft4_least = cli_schema_true(json, cli_schema_get(json, map, "exclusiveMinimum"));
    const int draft4_most = cli_schema_true(json, cli_schema_get(json, map, "exclusiveMaximum"));
    VerbaLinea msg = cli_build_msg_line(check->found);
    double bound = 0.0;
    if (cli_schema_number(json, cli_schema_get(json, map, "minimum"), &bound) &&
        ((number < bound) || (draft4_least && (number == bound))))
    {
        bounds_msg_written(&msg, number, " is below the least the schema allows, ", bound,
                           draft4_least ? ", itself left out" : "");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "maximum"), &bound) &&
        ((number > bound) || (draft4_most && (number == bound))))
    {
        bounds_msg_written(&msg, number, " is above the most the schema allows, ", bound,
                           draft4_most ? ", itself left out" : "");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "exclusiveMinimum"), &bound) && (number <= bound))
    {
        bounds_msg_written(&msg, number, " is not above ", bound, ", as the schema requires");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "exclusiveMaximum"), &bound) && (number >= bound))
    {
        bounds_msg_written(&msg, number, " is not below ", bound, ", as the schema requires");
    }
    if (cli_schema_number(json, cli_schema_get(json, map, "multipleOf"), &bound) && (bound > 0.0))
    {
        const double ratio = number / bound;
        if (fabs(ratio - round(ratio)) > 1e-9)
        {
            bounds_msg_written(&msg, number, " is no multiple of ", bound, "");
        }
    }
    if (msg.at > 0u)
    {
        (void)cli_build_finding(check->found, &msg, check->lines, one->from, one->to, CLI_BUILD_WARNING,
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
        VerbaLinea msg = cli_build_msg_line(check->found);
        mmgr_line_put(&msg, "This string is ");
        mmgr_line_decimal(&msg, length, 1u);
        mmgr_line_put(&msg, " long, shorter than the ");
        mmgr_line_decimal(&msg, bound, 1u);
        mmgr_line_put(&msg, " the schema requires");
        (void)cli_build_finding(check->found, &msg, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                "schema-length");
    }
    if (cli_schema_u64(json, cli_schema_get(json, map, "maxLength"), &bound) && (length > bound))
    {
        VerbaLinea msg = cli_build_msg_line(check->found);
        mmgr_line_put(&msg, "This string is ");
        mmgr_line_decimal(&msg, length, 1u);
        mmgr_line_put(&msg, " long, longer than the ");
        mmgr_line_decimal(&msg, bound, 1u);
        mmgr_line_put(&msg, " the schema allows");
        (void)cli_build_finding(check->found, &msg, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                "schema-length");
    }
    unsigned long long size = 0ull;
    const char *const pattern = cli_schema_string(json, map, "pattern", &size);
    if ((pattern != NULL) && (cli_schema_matches(pattern, size, text, one->text_size) == 0))
    {
        VerbaLinea msg = cli_build_msg_line(check->found);
        mmgr_line_put(&msg, "This string does not match the schema's pattern `");
        mmgr_line_put_n(&msg, pattern, size);
        mmgr_line_ch(&msg, '`');
        (void)cli_build_finding(check->found, &msg, check->lines, one->from, one->to, CLI_BUILD_WARNING,
                                "schema-pattern");
    }
}
