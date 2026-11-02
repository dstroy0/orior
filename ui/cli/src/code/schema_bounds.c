// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_bounds.c: a number checked against `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum` and
// `multipleOf`, draft 4's boolean exclusive bounds read beside them, the last bound it passes reported; and a string
// against `minLength`, `maxLength`, counted in letters, and `pattern`
#include "schema_internal.h"

#include <math.h>

typedef struct
{
    VerbaLinea *msg;
    double number;
    const char *between;
    double bound;
    const char *after;
} SchemaBoundsCtx;

// a bound the number passes reported: the number, `between`, the bound, and `after`
EMBED_INLINE void bounds_msg_write(const SchemaBoundsCtx *args)
{
    args->msg->at = 0ull;
    EMBED_CALL(cli_schema_out_internal.number_show, CliSchemaOutCtx, .value_f64 = args->number, .out = args->msg);
    mmgr_line_put(args->msg, args->between);
    EMBED_CALL(cli_schema_out_internal.number_show, CliSchemaOutCtx, .value_f64 = args->bound, .out = args->msg);
    mmgr_line_put(args->msg, args->after);
}

void cli_schema_number_check(const CliSchemaBaseMapCtx *args)
{
    const CliJson *const json = &args->check->store->json;
    const CliSchemaNode *const one = &args->check->read->node[args->node];
    const double number = one->number;
    const int draft4_min = EMBED_CALL(cli_schema_json_node_internal.schema_true, CliSchemaJsonNodeCtx, .json = json,
                                      .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                         .map = args->map, .name = "exclusiveMinimum"));
    const int draft4_max = EMBED_CALL(cli_schema_json_node_internal.schema_true, CliSchemaJsonNodeCtx, .json = json,
                                      .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json,
                                                         .map = args->map, .name = "exclusiveMaximum"));
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    double bound = 0.0;
    if (EMBED_CALL(cli_schema_json_node_internal.number, CliSchemaJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "minimum"),
                   .value_f64s = &bound) &&
        ((number < bound) || (draft4_min && (number == bound))))
    {
        EMBED_CALL(bounds_msg_write, SchemaBoundsCtx, .msg = &msg, .number = number,
                   .between = " is below the least the schema allows, ", .bound = bound,
                   .after = draft4_min ? ", itself left out" : "");
    }
    if (EMBED_CALL(cli_schema_json_node_internal.number, CliSchemaJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "maximum"),
                   .value_f64s = &bound) &&
        ((number > bound) || (draft4_max && (number == bound))))
    {
        EMBED_CALL(bounds_msg_write, SchemaBoundsCtx, .msg = &msg, .number = number,
                   .between = " is above the most the schema allows, ", .bound = bound,
                   .after = draft4_max ? ", itself left out" : "");
    }
    if (EMBED_CALL(cli_schema_json_node_internal.number, CliSchemaJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "exclusiveMinimum"),
                   .value_f64s = &bound) &&
        (number <= bound))
    {
        EMBED_CALL(bounds_msg_write, SchemaBoundsCtx, .msg = &msg, .number = number, .between = " is not above ",
                   .bound = bound, .after = ", as the schema requires");
    }
    if (EMBED_CALL(cli_schema_json_node_internal.number, CliSchemaJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "exclusiveMaximum"),
                   .value_f64s = &bound) &&
        (number >= bound))
    {
        EMBED_CALL(bounds_msg_write, SchemaBoundsCtx, .msg = &msg, .number = number, .between = " is not below ",
                   .bound = bound, .after = ", as the schema requires");
    }
    if (EMBED_CALL(cli_schema_json_node_internal.number, CliSchemaJsonNodeCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "multipleOf"),
                   .value_f64s = &bound) &&
        (bound > 0.0))
    {
        const double ratio = number / bound;
        if (fabs(ratio - round(ratio)) > 1e-9)
        {
            EMBED_CALL(bounds_msg_write, SchemaBoundsCtx, .msg = &msg, .number = number,
                       .between = " is no multiple of ", .bound = bound, .after = "");
        }
    }
    if (msg.at > 0u)
    {
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = one->from, .to = one->to,
                         .severity = CLI_BUILD_WARNING, .code = "schema-range");
    }
}

void cli_schema_text_check(const CliSchemaBaseMapCtx *args)
{
    const CliJson *const json = &args->check->store->json;
    const CliSchemaNode *const one = &args->check->read->node[args->node];
    const char *const text =
        EMBED_CALL(cli_schema_read_at_internal.text, CliSchemaReadAtCtx, .read = args->check->read, .at = one->text_at);
    const unsigned long long length =
        EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = text, .size = one->text_size);
    unsigned long long bound = 0ull;
    if (EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "minLength"),
                   .value_u64s = &bound) &&
        (length < bound))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "This string is ");
        mmgr_line_decimal(&msg, length, 1u);
        mmgr_line_put(&msg, " long, shorter than the ");
        mmgr_line_decimal(&msg, bound, 1u);
        mmgr_line_put(&msg, " the schema requires");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = one->from, .to = one->to,
                         .severity = CLI_BUILD_WARNING, .code = "schema-length");
    }
    if (EMBED_CALL(cli_schema_json_node_value_u_64s_internal.u64, CliSchemaJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_schema_json_internal.get, CliSchemaJsonCtx, .json = json, .map = args->map,
                                      .name = "maxLength"),
                   .value_u64s = &bound) &&
        (length > bound))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "This string is ");
        mmgr_line_decimal(&msg, length, 1u);
        mmgr_line_put(&msg, " long, longer than the ");
        mmgr_line_decimal(&msg, bound, 1u);
        mmgr_line_put(&msg, " the schema allows");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = one->from, .to = one->to,
                         .severity = CLI_BUILD_WARNING, .code = "schema-length");
    }
    unsigned long long size = 0ull;
    const char *const pattern = EMBED_CALL(cli_schema_json_internal.string, CliSchemaJsonCtx, .json = json,
                                           .map = args->map, .name = "pattern", .size_pointer = &size);
    if ((pattern != NULL) && (EMBED_CALL(cli_schema_text_internal.matches, CliSchemaTextCtx, .pattern = pattern,
                                         .pattern_size = size, .text = text, .size = one->text_size) == 0))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "This string does not match the schema's pattern `");
        mmgr_line_put_n(&msg, pattern, size);
        mmgr_line_ch(&msg, '`');
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                         .message = &msg, .lines = args->check->lines, .from = one->from, .to = one->to,
                         .severity = CLI_BUILD_WARNING, .code = "schema-pattern");
    }
}
