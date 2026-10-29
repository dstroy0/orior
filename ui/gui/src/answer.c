// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer.c: what the window reads off the CLI's answers and events
#include "answer.h"

unsigned long long gui_answer_whole(const CliJson *json, unsigned int node, unsigned long long otherwise)
{
    if (!cli_json_is(json, node, CFG_JSON_NUMBER))
    {
        return otherwise;
    }
    const CliJsonNode *const number = &json->node[node];
    const char *const text = json->text + number->text;
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < number->length; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return otherwise;
        }
        // a digit's value is its distance from '0'
        value = (value * 10ull) + (unsigned long long)(text[at] - '0');
    }
    return (number->length != 0ull) ? value : otherwise;
}

unsigned long long gui_answer_member_whole(const CliJson *json, unsigned int object, const char *name,
                                           unsigned long long otherwise)
{
    return gui_answer_whole(json, cli_json_member(json, object, name), otherwise);
}

int gui_answer_member_true(const CliJson *json, unsigned int object, const char *name)
{
    return cli_json_is(json, cli_json_member(json, object, name), CFG_JSON_TRUE);
}
