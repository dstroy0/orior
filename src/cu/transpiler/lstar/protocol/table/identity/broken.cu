// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// broken.cu: the broken: ours checked against the answer question by question, each place they part traced to the forms
// that wrote it
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"

// one link of ours alone in a question nvcc makes in fewer steps, a step a fold takes away. A form whose text writes
// it, and the question
struct FoldStep
{
    IdentityName form;
    IdentityName question;
};

// one form's writing over the questions nvcc makes in fewer steps: the form, the questions, and its links of ours
// alone there
struct FormFolded
{
    IdentityName form;
    unsigned long long questions;
    unsigned long long alone;
};

// what the read of ours against the answer counts
struct BrokenCounts
{
    unsigned long long ours;
    unsigned int alike;
    unsigned int same;
    unsigned int theirs_fewer;
    unsigned int ours_fewer;
    unsigned int steps_to_learn;
    unsigned int unanswered;
};

// The place of the question numbered `number` among the questions, which go in the order of their numbers, the last
// where two share it. `count` where none has it
static unsigned long long question_numbered(const Questions *questions, const char *number)
{
    const unsigned long long after =
        identity_lower(questions->question, questions->count,
                       [number](const Question &one) { return identity_order_text(one.number.text, number) <= 0; });
    return ((after != 0ULL) && identity_same(questions->question[after - 1ULL].number.text, number)) ? (after - 1ULL)
                                                                                                     : questions->count;
}

// The cells of one question's read, each of the temporary tier. The longest runs in common, the links each chain holds
// alone, the stretch of each, and which links moved, paired and fold
struct BrokenCells
{
    unsigned int *common;
    unsigned long long *mine_only;
    unsigned long long *answer_only;
    unsigned long long *mine_stretch;
    unsigned long long *answer_stretch;
    unsigned long long *mine_own;
    unsigned long long *answer_own;
    unsigned long long *answer_by_type;
    int *moved;
    int *paired;
    int *folding;
};

// a cell of the temporary tier of `count` elements of `size` bytes, every byte 0, at least one element
static void *broken_cell(unsigned long long count, unsigned long long size)
{
    return platform_cell_cleared((count != 0ULL) ? count : 1ULL, size, platform_place_g_program_owner,
                                 PLATFORM_TIER_TEMPORARY);
}

static void broken_cells_ended(BrokenCells *cells)
{
    void *const each[11] = {cells->common,         cells->mine_only, cells->answer_only, cells->mine_stretch,
                            cells->answer_stretch, cells->mine_own,  cells->answer_own,  cells->answer_by_type,
                            cells->moved,          cells->paired,    cells->folding};
    for (unsigned int at = 0u; at < 11u; at += 1u)
    {
        platform_cell_released(each[at], platform_place_g_program_owner);
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = cells, .bytes = sizeof(*cells), .val = 0);
}

// the cells of a read of `rows` links of ours against `columns` of nvcc's: 1, or 0 where the pool cannot meet one
static int broken_cells_taken(BrokenCells *cells, unsigned long long rows, unsigned long long columns)
{
    const unsigned long long stretches = rows + columns + 1ULL;
    cells->common = (unsigned int *)broken_cell((rows + 1ULL) * (columns + 1ULL), sizeof(unsigned int));
    cells->mine_only = (unsigned long long *)broken_cell(rows, sizeof(unsigned long long));
    cells->answer_only = (unsigned long long *)broken_cell(columns, sizeof(unsigned long long));
    cells->mine_stretch = (unsigned long long *)broken_cell(rows, sizeof(unsigned long long));
    cells->answer_stretch = (unsigned long long *)broken_cell(columns, sizeof(unsigned long long));
    cells->mine_own = (unsigned long long *)broken_cell(stretches, sizeof(unsigned long long));
    cells->answer_own = (unsigned long long *)broken_cell(stretches, sizeof(unsigned long long));
    cells->answer_by_type = (unsigned long long *)broken_cell(columns, sizeof(unsigned long long));
    cells->moved = (int *)broken_cell(rows, sizeof(int));
    cells->paired = (int *)broken_cell(columns, sizeof(int));
    cells->folding = (int *)broken_cell(rows, sizeof(int));
    return (cells->common != NULL) && (cells->mine_only != NULL) && (cells->answer_only != NULL) &&
           (cells->mine_stretch != NULL) && (cells->answer_stretch != NULL) && (cells->mine_own != NULL) &&
           (cells->answer_own != NULL) && (cells->answer_by_type != NULL) && (cells->moved != NULL) &&
           (cells->paired != NULL) && (cells->folding != NULL);
}

// One question of ours read against the answer onto `file`, each step a fold takes away onto `steps`, a PlatformArray
// of FoldStep, and counted. 1, or 0 where the pool cannot meet a cell
static int question_broken(const Question *mine, const Question *answer, const IdentityForms *forms, RunWritten *file,
                           PlatformArray *steps, BrokenCounts *counts)
{
    VerbaLinea *const line = &file->text.line;
    const unsigned long long rows = mine->links;
    const unsigned long long columns = answer->links;
    const unsigned long long width = columns + 1ULL;
    BrokenCells cells;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &cells, .bytes = sizeof(cells), .val = 0);
    if (!broken_cells_taken(&cells, rows, columns))
    {
        broken_cells_ended(&cells);
        return 0;
    }
    // the longest run of types the two chains contain in common, in order
    unsigned int *const common = cells.common;
    for (unsigned long long row = rows; row-- > 0ULL;)
    {
        for (unsigned long long column = columns; column-- > 0ULL;)
        {
            const unsigned int down = common[((row + 1ULL) * width) + column];
            const unsigned int across = common[(row * width) + column + 1ULL];
            common[(row * width) + column] = identity_same(mine->chain[row].type.text, answer->chain[column].type.text)
                                                 ? (common[((row + 1ULL) * width) + column + 1ULL] + 1u)
                                                 : ((down > across) ? down : across);
        }
    }
    // the links each chain contains alone, and the stretch each parts in, each stretch running from one link in common
    // to the next
    unsigned long long mine_count = 0ULL;
    unsigned long long answer_count = 0ULL;
    unsigned long long stretch = 0ULL;
    int stretch_held = 0;
    for (unsigned long long row = 0ULL, column = 0ULL; (row < rows) || (column < columns);)
    {
        if ((row < rows) && (column < columns) &&
            identity_same(mine->chain[row].type.text, answer->chain[column].type.text))
        {
            stretch += stretch_held ? 1ULL : 0ULL;
            stretch_held = 0;
            row += 1ULL;
            column += 1ULL;
        }
        else if ((column < columns) &&
                 ((row == rows) || (common[(row * width) + column + 1ULL] >= common[((row + 1ULL) * width) + column])))
        {
            cells.answer_only[answer_count] = column;
            cells.answer_stretch[answer_count] = stretch;
            answer_count += 1ULL;
            stretch_held = 1;
            column += 1ULL;
        }
        else
        {
            cells.mine_only[mine_count] = row;
            cells.mine_stretch[mine_count] = stretch;
            mine_count += 1ULL;
            stretch_held = 1;
            row += 1ULL;
        }
    }
    const unsigned int in_common = common[0];
    // A link of ours alone whose type nvcc writes alone elsewhere in the question moved, and the two are for each
    // other. Each type's links of nvcc's go in their order, the first not yet taken first
    for (unsigned long long at = 0ULL; at < answer_count; at += 1ULL)
    {
        cells.answer_by_type[at] = cells.answer_only[at];
    }
    const Link *const answer_chain = answer->chain;
    (void)identity_sorted(
        cells.answer_by_type, answer_count, [answer_chain](unsigned long long one, unsigned long long other) {
            return identity_order_text(answer_chain[one].type.text, answer_chain[other].type.text) < 0;
        });
    for (unsigned long long at = 0ULL; at < mine_count; at += 1ULL)
    {
        const unsigned long long row = cells.mine_only[at];
        const char *const type = mine->chain[row].type.text;
        unsigned long long run =
            identity_lower(cells.answer_by_type, answer_count, [answer_chain, type](unsigned long long one) {
                return identity_order_text(answer_chain[one].type.text, type) < 0;
            });
        while ((run < answer_count) && identity_same(answer_chain[cells.answer_by_type[run]].type.text, type) &&
               (cells.paired[cells.answer_by_type[run]] != 0))
        {
            run += 1ULL;
        }
        if ((run < answer_count) && identity_same(answer_chain[cells.answer_by_type[run]].type.text, type))
        {
            cells.moved[row] = 1;
            cells.paired[cells.answer_by_type[run]] = 1;
        }
    }
    // What stays of a stretch is its own. A stretch where ours contains more of its own links than nvcc's is where a
    // fold takes steps away
    for (unsigned long long at = 0ULL; at < mine_count; at += 1ULL)
    {
        cells.mine_own[cells.mine_stretch[at]] += (cells.moved[cells.mine_only[at]] == 0) ? 1ULL : 0ULL;
    }
    for (unsigned long long at = 0ULL; at < answer_count; at += 1ULL)
    {
        cells.answer_own[cells.answer_stretch[at]] += (cells.paired[cells.answer_only[at]] == 0) ? 1ULL : 0ULL;
    }
    for (unsigned long long at = 0ULL; at < mine_count; at += 1ULL)
    {
        const unsigned long long row = cells.mine_only[at];
        const unsigned long long own = cells.mine_stretch[at];
        cells.folding[row] = (cells.moved[row] == 0) && (cells.mine_own[own] > cells.answer_own[own]);
    }
    const char *verdict = "same";
    if ((mine_count == 0ULL) && (answer_count == 0ULL))
    {
        verdict = "alike";
        counts->alike += 1u;
    }
    else if (columns < rows)
    {
        verdict = "fewer theirs";
        counts->theirs_fewer += 1u;
        // a chain's links are far below 2^32
        counts->steps_to_learn += (unsigned int)(rows - columns);
    }
    else if (rows < columns)
    {
        verdict = "fewer ours";
        counts->ours_fewer += 1u;
    }
    else
    {
        counts->same += 1u;
    }
    mmgr_line_put(line, "question ");
    mmgr_line_put(line, mine->number.text);
    mmgr_line_put(line, " ");
    mmgr_line_put(line, verdict);
    mmgr_line_put(line, " ");
    mmgr_line_decimal(line, rows, 1u);
    mmgr_line_put(line, " ");
    mmgr_line_decimal(line, columns, 1u);
    mmgr_line_put(line, " ");
    mmgr_line_decimal(line, in_common, 1u);
    mmgr_line_ch(line, '\n');
    PlatformText names;
    identity_text_begun(&names);
    for (unsigned long long at = 0ULL; at < mine_count; at += 1ULL)
    {
        const unsigned long long row = cells.mine_only[at];
        const Link *const link = &mine->chain[row];
        platform_text_reset(&names);
        forms_writing(forms, link->type.text, &names);
        const char *const written = platform_text_bytes(&names);
        const char *const name_text = (written != NULL) ? written : "";
        const int is_folding = cells.folding[row];
        mmgr_line_put(line, cells.moved[row] ? "moved" : "ours");
        mmgr_line_put(line, " ");
        mmgr_line_put(line, link->position.text);
        mmgr_line_put(line, " ");
        mmgr_line_put(line, link->type.text);
        mmgr_line_put(line, " form ");
        mmgr_line_put(line, name_text);
        mmgr_line_put(line, is_folding ? " fold" : "");
        mmgr_line_ch(line, '\n');
        // only a link of a stretch where ours contains more of its own links than nvcc's is a step a fold takes away.
        // The question is one nvcc makes in fewer steps
        if (!is_folding || (columns >= rows))
        {
            continue;
        }
        const CarrierPiece split = carrier_piece_of(name_text);
        size_t word_at = 0u;
        CarrierPiece name;
        while (identity_word_next(split, &word_at, &name))
        {
            FoldStep step;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &step, .bytes = sizeof(step), .val = 0);
            (void)IDENTITY_SET(step.form, name.text, name.length);
            step.question = mine->number;
            (void)platform_array_appended(steps, &step);
        }
    }
    platform_text_ended(&names);
    for (unsigned long long at = 0ULL; at < answer_count; at += 1ULL)
    {
        const Link *const theirs = &answer->chain[cells.answer_only[at]];
        mmgr_line_put(line, "theirs ");
        mmgr_line_put(line, answer->number.text);
        mmgr_line_put(line, ":");
        mmgr_line_put(line, theirs->position.text);
        mmgr_line_put(line, " ");
        mmgr_line_put(line, theirs->type.text);
        mmgr_line_ch(line, '\n');
    }
    broken_cells_ended(&cells);
    return 1;
}

// The steps a fold takes away, each form with its questions and its links, onto `folded`, a PlatformArray of FormFolded
// that the caller begins. They go ranked by questions, the max first, and by name where two have as many
static void forms_folded(PlatformArray *steps, PlatformArray *folded)
{
    FoldStep *const step = (FoldStep *)steps->cell;
    (void)identity_sorted(step, steps->count, [](const FoldStep &one, const FoldStep &other) {
        const int form = identity_order_text(one.form.text, other.form.text);
        return (form != 0) ? (form < 0) : (identity_order_text(one.question.text, other.question.text) < 0);
    });
    for (unsigned long long at = 0ULL; at < steps->count;)
    {
        FormFolded form;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &form, .bytes = sizeof(form), .val = 0);
        form.form = step[at].form;
        unsigned long long end = at;
        while ((end < steps->count) && identity_same(step[end].form.text, step[at].form.text))
        {
            form.questions +=
                ((end == at) || !identity_same(step[end].question.text, step[end - 1ULL].question.text)) ? 1ULL : 0ULL;
            form.alone += 1ULL;
            end += 1ULL;
        }
        (void)platform_array_appended(folded, &form);
        at = end;
    }
    identity_sorted_in_place(
        (FormFolded *)folded->cell, folded->count, [](const FormFolded &one, const FormFolded &other) {
            return (one.questions != other.questions) ? (one.questions > other.questions)
                                                      : (identity_order_text(one.form.text, other.form.text) < 0);
        });
}

// The answer and ours, question by question. nvcc's chain for a question of the stick is the answer. Ours is the
// disassembly of what the transpiler wrote for it, derived from the rulesets alone, and the rulesets are what the L*
// query gave. A question is one product, and the only reason to fold is to make the same product in fewer steps. Two
// chains of as many steps are two arrangements of it and neither is broken, whatever order or pipe each takes. Where
// nvcc makes it in fewer steps there is a fold the query has not learned, and where ours takes fewer, ours folds
// further.
//
// The two chains align link by link on their types, the longest run in common kept in order. Where they part, each
// link of ours moved (nvcc writes its type elsewhere) or is ours alone (nvcc writes nothing of its type
// there). Each goes with the forms of the rulesets whose texts write it. Each link of nvcc's with nothing of ours goes
// with its address.
//
// The run's blob broken.txt contains a question a group of lines, its verdict and the steps of each. Then it contains
// each form whose links are alone in a question nvcc makes in fewer steps. Each goes with its count of questions and
// of links, the max questions first
int identity_broken(const char *listing, const char *manifest, const char *folder, const IdentityPath *ours_paths,
                    unsigned long long ours_count, int count, char **files)
{
    Questions questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    IdentityForms forms;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &forms, .bytes = sizeof(forms), .val = 0);
    if (!forms_read(count, files, &forms))
    {
        forms_ended(&forms);
        questions_ended(&questions);
        return 1;
    }
    RunWritten file;
    run_written_begun(&file, identity_path_joined(folder, "/broken.txt").text, 0);
    PlatformArray steps;
    platform_array_begun(&steps, sizeof(FoldStep), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    BrokenCounts counts;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &counts, .bytes = sizeof(counts), .val = 0);
    int whole = 1;
    // ours file by file, each file's chains in the order it writes them
    for (unsigned long long path_at = 0ULL; whole && (path_at < ours_count); path_at += 1ULL)
    {
        Questions ours;
        if (!candidates_read(ours_paths[path_at].text, &ours))
        {
            whole = 0;
            break;
        }
        for (unsigned long long mine_at = 0ULL; whole && (mine_at < ours.count); mine_at += 1ULL)
        {
            const Question *const mine = &ours.question[mine_at];
            const unsigned long long found = question_numbered(&questions, mine->number.text);
            if ((found == questions.count) || (mine->links == 0ULL))
            {
                counts.unanswered += 1u;
                continue;
            }
            whole = question_broken(mine, &questions.question[found], &forms, &file, &steps, &counts);
        }
        counts.ours += ours.count;
        questions_ended(&ours);
    }
    forms_ended(&forms);
    questions_ended(&questions);
    VerbaLinea *const line = &file.text.line;
    PlatformArray folded_forms;
    platform_array_begun(&folded_forms, sizeof(FormFolded), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    forms_folded(&steps, &folded_forms);
    platform_array_ended(&steps);
    const unsigned long long folded = folded_forms.count;
    const FormFolded *const ranked = (const FormFolded *)folded_forms.cell;
    for (unsigned long long at = 0ULL; at < folded; at += 1ULL)
    {
        mmgr_line_put(line, "fold ");
        mmgr_line_put(line, ranked[at].form.text);
        mmgr_line_put(line, " ");
        mmgr_line_decimal(line, ranked[at].questions, 1u);
        mmgr_line_put(line, " ");
        mmgr_line_decimal(line, ranked[at].alone, 1u);
        mmgr_line_ch(line, '\n');
    }
    const int written = whole && run_closed(&file);
    run_written_ended(&file);
    if (!written)
    {
        identity_path_said(file.path.text,
                           whole ? " could not be written" : ": the pool cannot meet a question's read");
        platform_array_ended(&folded_forms);
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, file.path.text);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, counts.ours, 1u);
        mmgr_line_put(output_line, " of ours, ");
        mmgr_line_decimal(output_line, counts.alike, 1u);
        mmgr_line_put(output_line, " alike link for link, ");
        mmgr_line_decimal(output_line, counts.same, 1u);
        mmgr_line_put(output_line, " in as many steps, ");
        mmgr_line_decimal(output_line, counts.theirs_fewer, 1u);
        mmgr_line_put(output_line, " nvcc makes in fewer (");
        mmgr_line_decimal(output_line, counts.steps_to_learn, 1u);
        mmgr_line_put(output_line, " steps to learn), ");
        mmgr_line_decimal(output_line, counts.ours_fewer, 1u);
        mmgr_line_put(output_line, " ours makes in fewer, ");
        mmgr_line_decimal(output_line, counts.unanswered, 1u);
        mmgr_line_put(output_line, " with no answer");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned long long at = 0ULL; (at < folded) && (at < 8ULL); at += 1ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    ");
        mmgr_line_put(output_line, ranked[at].form.text);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, ranked[at].questions, 1u);
        mmgr_line_put(output_line, " questions, ");
        mmgr_line_decimal(output_line, ranked[at].alone, 1u);
        mmgr_line_put(output_line, " links alone");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_array_ended(&folded_forms);
    return 0;
}
