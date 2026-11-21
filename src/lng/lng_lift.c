// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lng_lift.c: the lift of a bridge word from one target table to another, the code of LngLift.lean in theory/lng
// that Lean compiles to C (lng.h)
//
// This file holds no rule of the lift. It starts the Lean runtime once, gives the Lean code the columns of the three
// tables, and reads the answer back. Lng/lift_code.lean proves what the Lean code computes.

#include "lng.h"

#include <lean/lean.h>
#include <stdlib.h>

extern void lean_initialize_runtime_module(void);
extern lean_object *initialize_Lng_LngLift(uint8_t builtin);
extern lean_object *lng_lean_lift(lean_object *words, lean_object *types, lean_object *anchors, lean_object *targets,
                                  lean_object *source_bridges, lean_object *source_words, lean_object *source_witnesses,
                                  lean_object *source_name, lean_object *target_bridges, lean_object *target_words,
                                  lean_object *target_witnesses, lean_object *target_name, lean_object *bridge);

// 1 where the Lean runtime and the module LngLift run, and 0 where the module did not start
static int lng_lift_started(void)
{
    static int s_state = 0;
    if (s_state == 0)
    {
        lean_initialize_runtime_module();
        lean_object *const started = initialize_Lng_LngLift(1);
        if (lean_io_result_is_error(started))
        {
            lean_dec_ref(started);
            s_state = -1;
            return 0;
        }
        lean_dec_ref(started);
        lean_io_mark_end_initialization();
        s_state = 1;
    }
    return s_state == 1;
}

// the cells of `table` under `column`, one Lean string for each row, in the order of the rows
static lean_object *lng_lift_column(const LngTable *table, const char *column)
{
    lean_object *cells = lean_mk_empty_array();
    for (unsigned int row = 0u; row < table->rows; row += 1u)
    {
        cells = lean_array_push(cells, lean_mk_string(lng_cell_or_none(table, row, column)));
    }
    return cells;
}

const char *lng_lift(const LngTable *transpiler, const LngTable *from, const char *from_name, const LngTable *to,
                     const char *to_name, const char *bridge, LngList *refusals)
{
    if (!lng_lift_started())
    {
        lng_list_printed(refusals, "the Lean runtime did not start, and no lift goes through %s", bridge);
        return NULL;
    }
    lean_object *const answer = lng_lean_lift(
        lng_lift_column(transpiler, "word"), lng_lift_column(transpiler, "type"), lng_lift_column(transpiler, "anchor"),
        lng_lift_column(transpiler, "target"), lng_lift_column(from, "transpiler"), lng_lift_column(from, "word"),
        lng_lift_column(from, "witness"), lean_mk_string(from_name), lng_lift_column(to, "transpiler"),
        lng_lift_column(to, "word"), lng_lift_column(to, "witness"), lean_mk_string(to_name), lean_mk_string(bridge));
    // the first cell is the place of the row of `to` in decimal, or empty where the lift refuses
    const char *const place = lean_string_cstr(lean_array_get_core(answer, 0u));
    const char *word = NULL;
    if (place[0] != '\0')
    {
        // side_ok in Lng/lift_code.lean proves that the place is below the count of rows of `to`
        const unsigned long row = strtoul(place, NULL, 10);
        word = lng_cell_or_none(to, (unsigned int)row, "word");
    }
    for (size_t at = 1u; at < lean_array_size(answer); at += 1u)
    {
        lng_list_add(refusals, lean_string_cstr(lean_array_get_core(answer, at)));
    }
    lean_dec_ref(answer);
    return word;
}
