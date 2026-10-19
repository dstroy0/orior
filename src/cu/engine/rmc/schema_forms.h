// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema_forms.h: every form the code generator writes, as C sees it: its name here, its name in a .krs file, how
// many parameters it takes, and its kind. Included by machine_ir_types.h, and by the writers of a part's ruleset,
// which are C (krs_write)
#ifndef SCHEMA_FORMS_H
#define SCHEMA_FORMS_H

// The kind of a form. The lane's structure is what the word web names as the kernel a lane is written into: the
// declarations, the notes, the lane's and the kernel's opening and close, the launch's and the shared memory's, the
// states and the asks, and the unit. Each names no operation and has no tree, and a part's ruleset writes it `nop`:
// the kernel the system accepted is the lane's frame there (The frame asked, Open 6.3). An operation is a form
// with a relation of its own, the ladder's, the frame's, the host's questions' or a walk of its own, and a part's
// ruleset gives it as the part answered it, or leaves it open
#define SCHEMA_FORM_OPERATION 0u
#define SCHEMA_FORM_STRUCTURE 1u

// One form a line, held from the formatter: it splits a form's name inside its words, and a search for the name
// then misses it
// clang-format off
#define OPCODES(form_) \
    form_(PROGRAM_NOTE, "program_note", 6u, SCHEMA_FORM_STRUCTURE)                               \
    form_(LANE_OPEN, "lane_open", 0u, SCHEMA_FORM_STRUCTURE)                                     \
    form_(LANE_BODY, "lane_body", 0u, SCHEMA_FORM_STRUCTURE)                                     \
    form_(LANE_CLOSE, "lane_close", 0u, SCHEMA_FORM_STRUCTURE)                                   \
    form_(DECLARE_PREDICATES, "declare_predicates", 1u, SCHEMA_FORM_STRUCTURE)                   \
    form_(DECLARE_FIXED_PREDICATES, "declare_fixed_predicates", 0u, SCHEMA_FORM_STRUCTURE)       \
    form_(DECLARE_FILE, "declare_file", 1u, SCHEMA_FORM_STRUCTURE)                               \
    form_(DECLARE_SIGNS, "declare_signs", 1u, SCHEMA_FORM_STRUCTURE)                             \
    form_(DECLARE_OUT, "declare_out", 1u, SCHEMA_FORM_STRUCTURE)                                 \
    form_(DECLARE_ATOMS, "declare_atoms", 1u, SCHEMA_FORM_STRUCTURE)                             \
    form_(DECLARE_TEMPORARIES, "declare_temporaries", 1u, SCHEMA_FORM_STRUCTURE)                 \
    form_(DECLARE_WIDES, "declare_wides", 1u, SCHEMA_FORM_STRUCTURE)                             \
    form_(DECLARE_FIXED_WORDS, "declare_fixed_words", 0u, SCHEMA_FORM_STRUCTURE)                 \
    form_(DECLARE_FIXED_WIDES, "declare_fixed_wides", 0u, SCHEMA_FORM_STRUCTURE)                 \
    form_(DECLARE_MEMBERS, "declare_members", 1u, SCHEMA_FORM_STRUCTURE)                         \
    form_(LAUNCH_OPEN, "launch_open", 0u, SCHEMA_FORM_STRUCTURE)                                 \
    form_(LAUNCH_LOAD_WIDE, "launch_load_wide", 2u, SCHEMA_FORM_OPERATION)                       \
    form_(CAST_GLOBAL, "cast_global", 1u, SCHEMA_FORM_STRUCTURE)                                 \
    form_(SHARED_OPEN, "shared_open", 0u, SCHEMA_FORM_STRUCTURE)                                 \
    form_(SHARED_CLOSE, "shared_close", 0u, SCHEMA_FORM_STRUCTURE)                               \
    form_(GLOBAL_LOAD_WORD_IF, "global_load_word_if", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(WIDE_FROM_WORD_IF, "wide_from_word_if", 3u, SCHEMA_FORM_OPERATION)                     \
    form_(ERROR_OPEN_UNLESS, "error_open_unless", 1u, SCHEMA_FORM_OPERATION)                     \
    form_(ERROR_IF, "error_if", 2u, SCHEMA_FORM_OPERATION)                                       \
    form_(LABEL_ERROR_OPEN, "label_error_open", 0u, SCHEMA_FORM_OPERATION)                       \
    form_(LABEL_ERROR, "label_error", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(GLOBAL_ADD_ATOMIC_WORD, "global_add_atomic_word", 1u, SCHEMA_FORM_OPERATION)           \
    form_(RETURN, "return", 0u, SCHEMA_FORM_OPERATION)                                           \
    form_(STEP_NOTE, "step_note", 2u, SCHEMA_FORM_STRUCTURE)                                     \
    form_(WORD_ADD, "word_add", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_ADD_FIRST, "word_add_first", 3u, SCHEMA_FORM_OPERATION)                           \
    form_(WORD_ADD_MIDDLE, "word_add_middle", 3u, SCHEMA_FORM_OPERATION)                         \
    form_(WORD_ADD_LAST, "word_add_last", 3u, SCHEMA_FORM_OPERATION)                             \
    form_(WORD_SUB, "word_sub", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_SUB_FIRST, "word_sub_first", 3u, SCHEMA_FORM_OPERATION)                           \
    form_(WORD_SUB_MIDDLE, "word_sub_middle", 3u, SCHEMA_FORM_OPERATION)                         \
    form_(WORD_SUB_LAST, "word_sub_last", 3u, SCHEMA_FORM_OPERATION)                             \
    form_(WORD_BORROW_READ, "word_borrow_read", 2u, SCHEMA_FORM_OPERATION)                       \
    form_(WORD_COPY, "word_copy", 2u, SCHEMA_FORM_OPERATION)                                     \
    form_(WORD_SET, "word_set", 2u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_BITAND, "word_bitand", 3u, SCHEMA_FORM_OPERATION)                                 \
    form_(WORD_BITOR, "word_bitor", 3u, SCHEMA_FORM_OPERATION)                                   \
    form_(WORD_BITXOR, "word_bitxor", 3u, SCHEMA_FORM_OPERATION)                                 \
    form_(WORD_NOT, "word_not", 2u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_NAND, "word_nand", 3u, SCHEMA_FORM_OPERATION)                                     \
    form_(WORD_NOR, "word_nor", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_SHL, "word_shl", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_SHR, "word_shr", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_FUNNEL_RIGHT, "word_funnel_right", 4u, SCHEMA_FORM_OPERATION)                     \
    form_(SIGNED_WORD_SHR, "signed_word_shr", 3u, SCHEMA_FORM_OPERATION)                         \
    form_(WORD_ROL, "word_rol", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_ROR, "word_ror", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_MUL, "word_mul", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_MUL_ADD, "word_mul_add", 4u, SCHEMA_FORM_OPERATION)                               \
    form_(WORD_DIV, "word_div", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WORD_REM, "word_rem", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(SIGNED_WORD_DIV, "signed_word_div", 3u, SCHEMA_FORM_OPERATION)                         \
    form_(SIGNED_WORD_REM, "signed_word_rem", 3u, SCHEMA_FORM_OPERATION)                         \
    form_(WORD_SELECT, "word_select", 4u, SCHEMA_FORM_OPERATION)                                 \
    form_(WORD_MUL_LOW, "word_mul_low", 4u, SCHEMA_FORM_OPERATION)                               \
    form_(WORD_MUL_HIGH, "word_mul_high", 3u, SCHEMA_FORM_OPERATION)                             \
    form_(SIGN_SET, "sign_set", 2u, SCHEMA_FORM_OPERATION)                                       \
    form_(SIGN_SELECT, "sign_select", 4u, SCHEMA_FORM_OPERATION)                                 \
    form_(SIGN_MUL, "sign_mul", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(SIGN_ABSOLUTE, "sign_absolute", 2u, SCHEMA_FORM_OPERATION)                             \
    form_(SIGN_NEG, "sign_neg", 2u, SCHEMA_FORM_OPERATION)                                       \
    form_(TEST_WORD_NONZERO, "test_word_nonzero", 2u, SCHEMA_FORM_OPERATION)                     \
    form_(TEST_WORD_ZERO, "test_word_zero", 2u, SCHEMA_FORM_OPERATION)                           \
    form_(TEST_SIGNED_WORD_NEGATIVE, "test_signed_word_negative", 2u, SCHEMA_FORM_OPERATION)     \
    form_(TEST_SIGN_NE, "test_sign_ne", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_SIGN_GT, "test_sign_gt", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WIDE_NONZERO, "test_wide_nonzero", 2u, SCHEMA_FORM_OPERATION)                     \
    form_(TEST_WIDE_EQ, "test_wide_eq", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WIDE_LT, "test_wide_lt", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WIDE_LT_AND, "test_wide_lt_and", 4u, SCHEMA_FORM_OPERATION)                       \
    form_(PREDICATE_BITXOR, "predicate_bitxor", 3u, SCHEMA_FORM_OPERATION)                       \
    form_(PREDICATE_BITAND, "predicate_bitand", 3u, SCHEMA_FORM_OPERATION)                       \
    form_(WIDE_FROM_WORD, "wide_from_word", 2u, SCHEMA_FORM_OPERATION)                           \
    form_(WORD_FROM_WIDE, "word_from_wide", 2u, SCHEMA_FORM_OPERATION)                           \
    form_(WIDE_PACK, "wide_pack", 3u, SCHEMA_FORM_OPERATION)                                     \
    form_(WIDE_UNPACK, "wide_unpack", 3u, SCHEMA_FORM_OPERATION)                                 \
    form_(WIDE_MUL, "wide_mul", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WIDE_MUL_WORD, "wide_mul_word", 3u, SCHEMA_FORM_OPERATION)                             \
    form_(WIDE_ADD, "wide_add", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WIDE_SHL, "wide_shl", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WIDE_SELECT, "wide_select", 4u, SCHEMA_FORM_OPERATION)                                 \
    form_(WIDE_DIV, "wide_div", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(GLOBAL_LOAD_CONSTANT_WORD, "global_load_constant_word", 3u, SCHEMA_FORM_OPERATION)     \
    form_(RECORD_STORE_WORD, "record_store_word", 2u, SCHEMA_FORM_OPERATION)                     \
    form_(DECLARE_STATES, "declare_states", 1u, SCHEMA_FORM_STRUCTURE)                           \
    form_(STATE_START, "state_start", 1u, SCHEMA_FORM_STRUCTURE)                                 \
    form_(STATE_OPEN, "state_open", 1u, SCHEMA_FORM_STRUCTURE)                                   \
    form_(STATE_NEXT, "state_next", 2u, SCHEMA_FORM_STRUCTURE)                                   \
    form_(STATE_EXIT, "state_exit", 0u, SCHEMA_FORM_STRUCTURE)                                   \
    form_(STATE_DISPATCH, "state_dispatch", 2u, SCHEMA_FORM_STRUCTURE)                           \
    form_(LAUNCH_ASK, "launch_ask", 1u, SCHEMA_FORM_STRUCTURE)                                   \
    form_(GLOBAL_ASK, "global_ask", 2u, SCHEMA_FORM_STRUCTURE)                                   \
    form_(GLOBAL_ASK_IF, "global_ask_if", 2u, SCHEMA_FORM_STRUCTURE)                             \
    form_(GLOBAL_ASK_ATOMIC, "global_ask_atomic", 1u, SCHEMA_FORM_STRUCTURE)                     \
    form_(LABEL_LOOP, "label_loop", 1u, SCHEMA_FORM_OPERATION)                                   \
    form_(LOOP_BACK_IF, "loop_back_if", 2u, SCHEMA_FORM_OPERATION)                               \
    form_(STATE_LOOP, "state_loop", 3u, SCHEMA_FORM_STRUCTURE)                                   \
    form_(PROGRAM_UNIT, "program_unit", 25u, SCHEMA_FORM_STRUCTURE)                              \
    form_(KERNEL_OPEN, "kernel_open", 0u, SCHEMA_FORM_STRUCTURE)                                 \
    form_(KERNEL_CLOSE, "kernel_close", 0u, SCHEMA_FORM_STRUCTURE)                               \
    form_(THREAD_IDX_X, "thread_idx_x", 1u, SCHEMA_FORM_OPERATION)                               \
    form_(THREAD_IDX_Y, "thread_idx_y", 1u, SCHEMA_FORM_OPERATION)                               \
    form_(THREAD_IDX_Z, "thread_idx_z", 1u, SCHEMA_FORM_OPERATION)                               \
    form_(BLOCK_IDX_X, "block_idx_x", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(BLOCK_IDX_Y, "block_idx_y", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(BLOCK_IDX_Z, "block_idx_z", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(BLOCK_DIM_X, "block_dim_x", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(BLOCK_DIM_Y, "block_dim_y", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(BLOCK_DIM_Z, "block_dim_z", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(GRID_DIM_X, "grid_dim_x", 1u, SCHEMA_FORM_OPERATION)                                   \
    form_(GRID_DIM_Y, "grid_dim_y", 1u, SCHEMA_FORM_OPERATION)                                   \
    form_(GRID_DIM_Z, "grid_dim_z", 1u, SCHEMA_FORM_OPERATION)                                   \
    form_(WARP_SIZE, "warp_size", 1u, SCHEMA_FORM_OPERATION)                                     \
    form_(PARAMETER_LOAD_WORD, "parameter_load_word", 2u, SCHEMA_FORM_OPERATION)                 \
    form_(PARAMETER_LOAD_WIDE, "parameter_load_wide", 2u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_WORD_LT, "test_word_lt", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(EXIT_UNLESS, "exit_unless", 1u, SCHEMA_FORM_OPERATION)                                 \
    form_(WIDE_MUL_WORD_ADD, "wide_mul_word_add", 4u, SCHEMA_FORM_OPERATION)                     \
    form_(GLOBAL_LOAD_WORD, "global_load_word", 3u, SCHEMA_FORM_OPERATION)                       \
    form_(GLOBAL_LOAD_WIDE, "global_load_wide", 3u, SCHEMA_FORM_OPERATION)                       \
    form_(GLOBAL_STORE_WIDE, "global_store_wide", 3u, SCHEMA_FORM_OPERATION)                     \
    form_(WIDE_FROM_SIGNED_WORD, "wide_from_signed_word", 2u, SCHEMA_FORM_OPERATION)             \
    form_(TEST_WORD_GE, "test_word_ge", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(EXIT_IF, "exit_if", 1u, SCHEMA_FORM_OPERATION)                                         \
    form_(TEST_WORD_LE, "test_word_le", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WORD_NE, "test_word_ne", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WORD_EQ, "test_word_eq", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WORD_GT, "test_word_gt", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_SIGNED_WORD_LT, "test_signed_word_lt", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_SIGNED_WORD_GE, "test_signed_word_ge", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_SIGNED_WORD_LE, "test_signed_word_le", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_SIGNED_WORD_GT, "test_signed_word_gt", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_WIDE_NE, "test_wide_ne", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WIDE_LE, "test_wide_le", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WIDE_GT, "test_wide_gt", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_WIDE_GE, "test_wide_ge", 3u, SCHEMA_FORM_OPERATION)                               \
    form_(TEST_SIGNED_WIDE_LT, "test_signed_wide_lt", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_SIGNED_WIDE_LE, "test_signed_wide_le", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_SIGNED_WIDE_GT, "test_signed_wide_gt", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_SIGNED_WIDE_GE, "test_signed_wide_ge", 3u, SCHEMA_FORM_OPERATION)                 \
    form_(TEST_WIDE_ZERO, "test_wide_zero", 2u, SCHEMA_FORM_OPERATION)                           \
    form_(WORD_FROM_BYTE, "word_from_byte", 2u, SCHEMA_FORM_OPERATION)                           \
    form_(WORD_FROM_SIGNED_BYTE, "word_from_signed_byte", 2u, SCHEMA_FORM_OPERATION)             \
    form_(WORD_FROM_HALFWORD, "word_from_halfword", 2u, SCHEMA_FORM_OPERATION)                   \
    form_(WORD_FROM_SIGNED_HALFWORD, "word_from_signed_halfword", 2u, SCHEMA_FORM_OPERATION)     \
    form_(GLOBAL_LOAD_BYTE, "global_load_byte", 3u, SCHEMA_FORM_OPERATION)                       \
    form_(GLOBAL_LOAD_SIGNED_BYTE, "global_load_signed_byte", 3u, SCHEMA_FORM_OPERATION)         \
    form_(GLOBAL_LOAD_HALFWORD, "global_load_halfword", 3u, SCHEMA_FORM_OPERATION)               \
    form_(GLOBAL_LOAD_SIGNED_HALFWORD, "global_load_signed_halfword", 3u, SCHEMA_FORM_OPERATION) \
    form_(TEST_WORD_NONZERO_AND, "test_word_nonzero_and", 3u, SCHEMA_FORM_OPERATION)             \
    form_(TEST_WORD_NONZERO_OR, "test_word_nonzero_or", 3u, SCHEMA_FORM_OPERATION)               \
    form_(TEST_WIDE_NONZERO_AND, "test_wide_nonzero_and", 3u, SCHEMA_FORM_OPERATION)             \
    form_(TEST_WIDE_NONZERO_OR, "test_wide_nonzero_or", 3u, SCHEMA_FORM_OPERATION)               \
    form_(WIDE_SUB, "wide_sub", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(WIDE_BITAND, "wide_bitand", 3u, SCHEMA_FORM_OPERATION)                                 \
    form_(WIDE_BITOR, "wide_bitor", 3u, SCHEMA_FORM_OPERATION)                                   \
    form_(WIDE_BITXOR, "wide_bitxor", 3u, SCHEMA_FORM_OPERATION)                                 \
    form_(WIDE_NOT, "wide_not", 2u, SCHEMA_FORM_OPERATION)                                       \
    form_(WIDE_SHR, "wide_shr", 3u, SCHEMA_FORM_OPERATION)                                       \
    form_(SIGNED_WIDE_SHR, "signed_wide_shr", 3u, SCHEMA_FORM_OPERATION)
// clang-format on

#endif
