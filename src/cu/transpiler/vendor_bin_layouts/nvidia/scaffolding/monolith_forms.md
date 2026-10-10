# The forms read off NVIDIA's compiler

Written by `monolith_forms.sh` whole on every run. Every form the lanes of the record programs decide is asked of NVIDIA's compiler once for each set of banks its arguments come from, in one program between tags (`monolith_forms.cpp`). A question in the form's text in `c.krs` is read off the PTX and held beside the form `ptx.krs` gives; one in its text in `ptx.krs`, put to `ptxas` as inline PTX, is read off the listing and held beside the form `sass.krs` gives. Each block is read back into the form it is, its registers named by the arguments they hold. A fixed register is named in angle brackets, and the ruleset names it. A form every question of which reads whole and alike is the ruleset's form, written into it by `monolith_forms.sh apply`; any other keeps the ruleset's text, and the reason stands beside it.

| tag | form | banks | read off | read | note |
|---|---|---|---|---|---|
| 1 | launch_load_wide | f2 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 2 | launch_load_wide | f2 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 3 | wide_mul | r5 f1 r8 | PTX | `mul.lo.s64 {to}, {left}, {right};` |  |
| 4 | wide_mul | r5 f1 r8 | PTX | `mul.lo.s64 {to}, {left}, {right};` |  |
| 5 | wide_add | f2 f2 r5 | PTX | `add.s64 {to}, {right}, {left};` |  |
| 6 | launch_load_wide | f3 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 7 | launch_load_wide | f3 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 8 | test_wide_nonzero | f9 f3 | PTX | `setp.ne.s64 {where}, {value}, 0;` |  |
| 9 | launch_load_wide | f5 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 10 | launch_load_wide | f5 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 11 | test_wide_eq | f10 f5 n | PTX | `setp.eq.s64 {where}, {left}, {right};` |  |
| 12 | test_wide_eq | f10 f5 n | PTX | `setp.eq.s64 {where}, {left}, {right};` |  |
| 13 | wide_select | f4 n f1 f10 | PTX | `setp.eq.s32 %p4, {where}, 0; \| selp.b64 {to}, {otherwise}, 0, %p4;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 14 | wide_select | f4 n f1 f10 | PTX | `setp.eq.s32 %p5, {where}, 0; \| selp.b64 {to}, {otherwise}, {chosen}, %p5;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 15 | wide_add | r5 r5 r8 | PTX | `` | the system folds the number put for right; the compiler writes no instruction for it |
| 16 | wide_add | r5 r5 r8 | PTX | `add.s64 {to}, {left}, {right};` |  |
| 17 | wide_shl | r5 r5 n | PTX | `shl.b64 {to}, {from}, {bits};` |  |
| 18 | wide_shl | r5 r5 n | PTX | `shl.b64 {to}, {from}, {bits};` |  |
| 19 | wide_add | r5 f3 r5 | PTX | `add.s64 {to}, {right}, {left};` |  |
| 20 | global_load_word_if | f9 r4 r5 | PTX | `setp.eq.s32 %p6, {where}, 0; \| @%p6 bra $L__BB0_2; \| ld.u32 {to}, [{address}];` | the reading holds a branch |
| 21 | wide_from_word_if | f9 f4 r4 | PTX | `setp.eq.s32 %p7, {where}, 0; \| mov.u32 %r505, 0; \| selp.b64 {to}, {to}, {from}, %p7;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 22 | test_wide_lt | f11 f4 f5 | PTX | `setp.lt.u64 {where}, {left}, {right};` |  |
| 23 | wide_select | f4 f4 n f11 | PTX | `setp.eq.s32 %p9, {where}, 0; \| selp.b64 {to}, 0, {chosen}, %p9;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 24 | wide_select | f4 f4 n f11 | PTX | `setp.eq.s32 %p10, {where}, 0; \| selp.b64 {to}, {otherwise}, {chosen}, %p10;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 25 | launch_load_wide | r7 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 26 | launch_load_wide | r7 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 27 | wide_mul | r5 f4 r8 | PTX | `shl.b64 {to}, {left}, 5;` | the system folds the number put for right; the reading does not name right |
| 28 | wide_mul | r5 f4 r8 | PTX | `mul.lo.s64 {to}, {left}, {right};` |  |
| 29 | wide_add | r7 r7 r5 | PTX | `add.s64 {to}, {right}, {left};` |  |
| 30 | word_mul_add | f8 f7 r8 f8 | PTX | `mad.lo.s32 {to}, {left}, {right}, {added};` |  |
| 31 | word_mul_add | f8 f7 r8 f8 | PTX | `mad.lo.s32 {to}, {left}, {right}, {added};` |  |
| 32 | global_load_constant_word | r3 r7 n | PTX | `ld.u32 {to}, [{address}+{offset}];` | the question names no address space |
| 33 | global_load_constant_word | r3 r7 n | PTX | `ld.u32 {to}, [{address}+{offset}];` | the question names no address space |
| 34 | word_copy | r0 r3 | PTX | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 35 | word_bitand | r4 r0 r8 | PTX | `and.b32 {to}, {left}, -{right};` |  |
| 36 | word_bitand | r4 r0 r8 | PTX | `and.b32 {to}, {left}, {right};` |  |
| 37 | test_word_nonzero | r6 r4 | PTX | `setp.ne.s32 {where}, {value}, 0;` |  |
| 38 | word_select | r0 r4 r0 r6 | PTX | `setp.eq.s32 %p12, {where}, 0; \| selp.b32 {to}, {otherwise}, {chosen}, %p12;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 39 | sign_select | r4 n n r6 | PTX | `mov.u32 {to}, {chosen};` | the system folds the number put for otherwise; the reading does not name otherwise |
| 40 | sign_select | r4 n n r6 | PTX | `mov.u32 {to}, {chosen};` | the system folds the number put for otherwise; the reading does not name otherwise |
| 41 | word_bitor | r4 r0 r0 | PTX | `or.b32 {to}, {right}, {left};` |  |
| 42 | word_bitor | r4 r4 r0 | PTX | `or.b32 {to}, {right}, {left};` |  |
| 43 | sign_select | r1 r4 n r6 | PTX | `setp.eq.s32 %p13, {where}, 0; \| cvt.s32.s8 %r540, {chosen}; \| selp.b32 {to}, 0, %r540, %p13;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 44 | sign_select | r1 r4 n r6 | PTX | `setp.eq.s32 %p14, {where}, 0; \| cvt.s32.s8 %r544, {chosen}; \| selp.b32 {to}, 4, %r544, %p14; \| mov.u32 %r546, 4;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 45 | sign_select | r1 n n r6 | PTX | `selp.u32 {to}, {chosen}, {otherwise}, {where};` |  |
| 46 | sign_select | r1 n n r6 | PTX | `setp.eq.s32 %p16, {where}, 0; \| selp.b32 {to}, {otherwise}, 2, %p16; \| mov.u32 %r551, 2;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 47 | test_word_nonzero | r6 r0 | PTX | `setp.ne.s32 {where}, {value}, 0;` |  |
| 48 | word_set | r0 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 49 | word_set | r0 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 50 | word_sub_first | r4 f0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd170, {to}, 32; \| cvt.u32.u64 %r554, %rd170; \| and.b32 <carry>, %r554, 1; \| mov.u32 %r556, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 51 | word_sub_middle | r4 f0 r0 | PTX | `sub.s64 %rd174, {left}, {right}; \| sub.s64 {to}, %rd174, <carry>; \| shr.u64 %rd176, {to}, 32; \| cvt.u32.u64 %r557, %rd176; \| and.b32 <carry>, %r557, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 52 | word_sub_last | r4 f0 r0 | PTX | `sub.s32 %r562, {left}, {right}; \| sub.s32 {to}, %r562, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 53 | word_mul_low | r0 r0 r0 r0 | PTX | `mul.lo.s32 %r566, {right}, {left}; \| cvt.u64.u32 %rd178, %r566; \| add.s64 {to}, %rd178, {added}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 54 | word_mul_high | r4 r0 r0 | PTX | `mul.wide.u32 %rd181, {right}, {left}; \| shr.u64 %rd182, %rd181, 32; \| cvt.u32.u64 %r570, %rd182; \| add.s32 {to}, <carry>, %r570;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 55 | word_add_first | r0 r0 r4 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 56 | word_add_last | r4 r4 f0 | PTX | `add.s32 %r575, {right}, {left}; \| add.s32 {to}, %r575, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 57 | word_add_first | r0 r0 r4 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 58 | word_add_middle | r0 r0 f0 | PTX | `add.s64 %rd194, {right}, {left}; \| add.s64 {to}, %rd194, <carry>; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 59 | word_add_last | r0 r0 f0 | PTX | `add.s32 %r580, {right}, {left}; \| add.s32 {to}, %r580, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 60 | word_add_first | r0 r0 r4 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 61 | word_add_last | r0 r0 f0 | PTX | `add.s32 %r585, {right}, {left}; \| add.s32 {to}, %r585, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 62 | word_add | r0 r0 r4 | PTX | `add.s32 {to}, {right}, {left};` |  |
| 63 | sign_mul | r1 r1 r1 | PTX | `mul.lo.s32 %r592, {left}, {right}; \| cvt.s32.s8 {to}, %r592;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 64 | test_signed_word_negative | r6 r1 | PTX | `and.b32 %r595, {value}, 128; \| shr.u32 {where}, %r595, 7;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 65 | word_select | r4 r4 r0 r6 | PTX | `setp.eq.s32 %p18, {where}, 0; \| selp.b32 {to}, {otherwise}, {chosen}, %p18;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 66 | word_select | r4 r4 f0 r6 | PTX | `setp.eq.s32 %p19, {where}, 0; \| selp.b32 {to}, {otherwise}, {chosen}, %p19;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 67 | word_bitand | r4 r4 r8 | PTX | `and.b32 {to}, {left}, {right};` |  |
| 68 | word_bitand | r4 r4 r8 | PTX | `and.b32 {to}, {left}, {right};` |  |
| 69 | word_set | r2 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 70 | word_set | r2 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 71 | word_bitor | r2 r2 r4 | PTX | `or.b32 {to}, {right}, {left};` |  |
| 72 | record_store_word | n r2 | PTX | `st.u32 [<record>+{offset}], {from};` | the question names no address space |
| 73 | record_store_word | n r2 | PTX | `st.u32 [<record>+{offset}], {from};` | the question names no address space |
| 74 | word_sub_first | r4 f0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd206, {to}, 32; \| cvt.u32.u64 %r614, %rd206; \| and.b32 <carry>, %r614, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 75 | word_sub_middle | r4 f0 r0 | PTX | `sub.s64 %rd210, {left}, {right}; \| sub.s64 {to}, %rd210, <carry>; \| shr.u64 %rd212, {to}, 32; \| cvt.u32.u64 %r616, %rd212; \| and.b32 <carry>, %r616, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 76 | word_sub_last | r4 f0 f0 | PTX | `sub.s32 %r621, {left}, {right}; \| sub.s32 {to}, %r621, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 77 | word_add_first | r0 r0 r0 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 78 | word_add_middle | r0 r0 f0 | PTX | `add.s64 %rd220, {right}, {left}; \| add.s64 {to}, %rd220, <carry>; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 79 | word_add_last | r0 f0 f0 | PTX | `add.s32 %r626, {right}, {left}; \| add.s32 {to}, %r626, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 80 | word_sub_first | r4 r0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd226, {to}, 32; \| cvt.u32.u64 %r628, %rd226; \| and.b32 <carry>, %r628, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 81 | word_sub_middle | r4 r0 f0 | PTX | `sub.s64 %rd230, {left}, {right}; \| sub.s64 {to}, %rd230, <carry>; \| shr.u64 %rd232, {to}, 32; \| cvt.u32.u64 %r630, %rd232; \| and.b32 <carry>, %r630, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 82 | word_sub_middle | r4 f0 f0 | PTX | `sub.s64 %rd236, {left}, {right}; \| sub.s64 {to}, %rd236, <carry>; \| shr.u64 %rd238, {to}, 32; \| cvt.u32.u64 %r632, %rd238; \| and.b32 <carry>, %r632, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 83 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 84 | sign_mul | r4 r1 r1 | PTX | `mul.lo.s32 %r639, {left}, {right}; \| cvt.s32.s8 {to}, %r639;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 85 | test_signed_word_negative | r6 r4 | PTX | `shr.u32 {where}, {value}, 31;` |  |
| 86 | word_select | r4 r4 r4 r6 | PTX | `setp.eq.s32 %p21, {where}, 0; \| selp.b32 {to}, {otherwise}, {chosen}, %p21;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 87 | sign_select | r4 r1 r1 r6 | PTX | `setp.eq.s32 %p22, {where}, 0; \| selp.b32 %r650, {otherwise}, {chosen}, %p22; \| cvt.s32.s8 {to}, %r650;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 88 | test_sign_ne | r6 r1 n | PTX | `and.b32 %r653, {left}, 255; \| setp.ne.s32 {where}, %r653, {right};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 89 | test_sign_ne | r6 r1 n | PTX | `and.b32 %r656, {left}, 255; \| setp.ne.s32 {where}, %r656, {right};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 90 | sign_select | r4 r4 r4 r6 | PTX | `setp.eq.s32 %p25, {where}, 0; \| selp.b32 %r661, {otherwise}, {chosen}, %p25; \| cvt.s32.s8 {to}, %r661;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 91 | word_sub_first | r4 f0 r4 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd242, {to}, 32; \| cvt.u32.u64 %r663, %rd242; \| and.b32 <carry>, %r663, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 92 | word_sub_middle | r4 f0 r4 | PTX | `sub.s64 %rd246, {left}, {right}; \| sub.s64 {to}, %rd246, <carry>; \| shr.u64 %rd248, {to}, 32; \| cvt.u32.u64 %r665, %rd248; \| and.b32 <carry>, %r665, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 93 | word_sub_last | r4 f0 r4 | PTX | `sub.s32 %r670, {left}, {right}; \| sub.s32 {to}, %r670, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 94 | word_shl | r4 r4 n | PTX | `shl.b32 {to}, {from}, {bits};` |  |
| 95 | word_shl | r4 r4 n | PTX | `shl.b32 {to}, {from}, {bits};` |  |
| 96 | word_shr | r4 r4 n | PTX | `shr.u32 {to}, {from}, {bits};` |  |
| 97 | word_shr | r4 r4 n | PTX | `shr.u32 {to}, {from}, {bits};` |  |
| 98 | word_add_first | r0 r0 r0 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 99 | word_add_middle | r0 r0 r0 | PTX | `add.s64 %rd256, {right}, {left}; \| add.s64 {to}, %rd256, <carry>; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 100 | word_add_middle | r0 f0 r0 | PTX | `add.s64 %rd262, {right}, {left}; \| add.s64 {to}, %rd262, <carry>; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 101 | word_add_last | r0 f0 f0 | PTX | `add.s32 %r683, {right}, {left}; \| add.s32 {to}, %r683, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 102 | word_sub_first | r4 r0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd268, {to}, 32; \| cvt.u32.u64 %r685, %rd268; \| and.b32 <carry>, %r685, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 103 | word_sub_middle | r4 r0 r0 | PTX | `sub.s64 %rd272, {left}, {right}; \| sub.s64 {to}, %rd272, <carry>; \| shr.u64 %rd274, {to}, 32; \| cvt.u32.u64 %r687, %rd274; \| and.b32 <carry>, %r687, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 104 | word_sub_middle | r4 f0 r0 | PTX | `sub.s64 %rd278, {left}, {right}; \| sub.s64 {to}, %rd278, <carry>; \| shr.u64 %rd280, {to}, 32; \| cvt.u32.u64 %r689, %rd280; \| and.b32 <carry>, %r689, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 105 | word_sub_middle | r4 f0 f0 | PTX | `sub.s64 %rd284, {left}, {right}; \| sub.s64 {to}, %rd284, <carry>; \| shr.u64 %rd286, {to}, 32; \| cvt.u32.u64 %r691, %rd286; \| and.b32 <carry>, %r691, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 106 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 107 | sign_neg | r4 r1 | PTX | `shl.b32 %r697, {from}, 24; \| neg.s32 %r698, %r697; \| shr.s32 {to}, %r698, 24;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 108 | sign_mul | r4 r1 r4 | PTX | `mul.lo.s32 %r702, {left}, {right}; \| cvt.s32.s8 {to}, %r702;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 109 | sign_select | r4 r4 r1 r6 | PTX | `setp.eq.s32 %p27, {where}, 0; \| selp.b32 %r707, {otherwise}, {chosen}, %p27; \| cvt.s32.s8 {to}, %r707;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 110 | sign_select | r4 r1 r4 r6 | PTX | `setp.eq.s32 %p28, {where}, 0; \| selp.b32 %r712, {otherwise}, {chosen}, %p28; \| cvt.s32.s8 {to}, %r712;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 111 | word_copy | r0 r0 | PTX | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 112 | sign_absolute | r1 r1 | PTX | `and.b32 %r716, {from}, 128; \| setp.eq.s32 %p29, %r716, 0; \| cvt.s32.s8 %r717, {from}; \| neg.s32 %r718, %r717; \| selp.b32 %r719, %r717, %r718, %p29; \| cvt.s32.s8 {to}, %r719;` | the compiler writes 6 instructions where the ruleset writes 1 |
| 113 | word_copy | r4 r0 | PTX | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 114 | word_bitor | r4 r4 r4 | PTX | `or.b32 {to}, {right}, {left};` |  |
| 115 | sign_select | r4 r4 n r6 | PTX | `setp.eq.s32 %p30, {where}, 0; \| cvt.s32.s8 %r727, {chosen}; \| selp.b32 {to}, 0, %r727, %p30;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 116 | sign_select | r4 r4 n r6 | PTX | `setp.eq.s32 %p31, {where}, 0; \| cvt.s32.s8 %r731, {chosen}; \| selp.b32 {to}, {otherwise}, %r731, %p31;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 117 | test_sign_gt | r6 r1 r1 | PTX | `shl.b32 %r735, {left}, 24; \| shl.b32 %r736, {right}, 24; \| setp.gt.s32 {where}, %r735, %r736;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 118 | test_sign_ne | r6 r1 r1 | PTX | `xor.b32 %r740, {right}, {left}; \| and.b32 %r741, %r740, 255; \| setp.ne.s32 {where}, %r741, 0;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 119 | sign_select | r1 r4 r4 r6 | PTX | `setp.eq.s32 %p34, {where}, 0; \| selp.b32 %r746, {otherwise}, {chosen}, %p34; \| cvt.s32.s8 {to}, %r746;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 120 | sign_absolute | r0 r1 | PTX | `and.b32 %r749, {from}, 128; \| setp.eq.s32 %p35, %r749, 0; \| cvt.s32.s8 %r750, {from}; \| neg.s32 %r751, %r750; \| selp.b32 {to}, %r750, %r751, %p35;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 121 | word_sub | r4 f0 r0 | PTX | `sub.s32 {to}, {left}, {right};` |  |
| 122 | word_set | r0 r8 | PTX | `mov.u32 {to}, {value};` |  |
| 123 | word_set | r0 r8 | PTX | `mov.u32 {to}, {value};` |  |
| 124 | sign_set | r1 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 125 | sign_set | r1 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 126 | word_sub_first | r4 r0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd290, {to}, 32; \| cvt.u32.u64 %r758, %rd290; \| and.b32 <carry>, %r758, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 127 | word_sub_middle | r4 r0 r0 | PTX | `sub.s64 %rd294, {left}, {right}; \| sub.s64 {to}, %rd294, <carry>; \| shr.u64 %rd296, {to}, 32; \| cvt.u32.u64 %r760, %rd296; \| and.b32 <carry>, %r760, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 128 | word_sub_middle | r4 r0 f0 | PTX | `sub.s64 %rd300, {left}, {right}; \| sub.s64 {to}, %rd300, <carry>; \| shr.u64 %rd302, {to}, 32; \| cvt.u32.u64 %r762, %rd302; \| and.b32 <carry>, %r762, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 129 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 130 | word_sub | r4 r1 r8 | PTX | `cvt.s32.s8 %r768, {left}; \| add.s32 {to}, %r768, -{right};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 131 | word_sub | r4 r1 r8 | PTX | `cvt.s32.s8 %r771, {left}; \| add.s32 {to}, %r771, -{right};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 132 | word_set | r4 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 133 | word_set | r4 n | PTX | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 134 | word_set | r4 r8 | PTX | `mov.u32 {to}, {value};` |  |
| 135 | word_set | r4 r8 | PTX | `mov.u32 {to}, {value};` |  |
| 136 | word_mul_low | r4 r0 r4 r4 | PTX | `mul.lo.s32 %r777, {right}, {left}; \| cvt.u64.u32 %rd304, %r777; \| add.s64 {to}, %rd304, {added}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 137 | word_mul_high | r4 r0 r4 | PTX | `mul.wide.u32 %rd307, {right}, {left}; \| shr.u64 %rd308, %rd307, 32; \| cvt.u32.u64 %r781, %rd308; \| add.s32 {to}, <carry>, %r781;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 138 | word_add | r4 r4 r4 | PTX | `add.s32 {to}, {right}, {left};` |  |
| 139 | word_add_first | r4 r4 r4 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 140 | word_add_last | r4 r4 f0 | PTX | `add.s32 %r789, {right}, {left}; \| add.s32 {to}, %r789, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 141 | word_select | r4 f0 r8 r6 | PTX | `setp.eq.s32 %p37, {where}, 0; \| selp.b32 {to}, {otherwise}, {chosen}, %p37;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 142 | word_select | r4 f0 r8 r6 | PTX | `setp.eq.s32 %p38, {where}, 0; \| selp.b32 {to}, {otherwise}, {chosen}, %p38;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 143 | word_sub_first | r4 r0 r4 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd316, {to}, 32; \| cvt.u32.u64 %r797, %rd316; \| and.b32 <carry>, %r797, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 144 | word_sub_middle | r4 f0 r4 | PTX | `sub.s64 %rd320, {left}, {right}; \| sub.s64 {to}, %rd320, <carry>; \| shr.u64 %rd322, {to}, 32; \| cvt.u32.u64 %r799, %rd322; \| and.b32 <carry>, %r799, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 145 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 146 | word_add | r4 r4 r8 | PTX | `add.s32 {to}, {left}, {right};` |  |
| 147 | word_add | r4 r4 r8 | PTX | `add.s32 {to}, {left}, {right};` |  |
| 148 | word_add_first | r4 r4 r4 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 149 | word_add_last | r4 r4 r4 | PTX | `add.s32 %r811, {right}, {left}; \| add.s32 {to}, %r811, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 150 | word_copy | r0 r4 | PTX | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 151 | sign_select | r1 r1 n r6 | PTX | `setp.eq.s32 %p40, {where}, 0; \| cvt.s32.s8 %r816, {chosen}; \| selp.b32 {to}, 0, %r816, %p40;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 152 | sign_select | r1 r1 n r6 | PTX | `setp.eq.s32 %p41, {where}, 0; \| cvt.s32.s8 %r820, {chosen}; \| selp.b32 {to}, {otherwise}, %r820, %p41;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 153 | launch_load_wide | r5 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 154 | launch_load_wide | r5 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 155 | global_add_atomic_word | r5 | PTX | `atom.add.u32 %r822, [{address}], 1;` | the question names no address space |
| 156 | record_store_word | n f0 | PTX | `st.u32 [<record>+{offset}], {from};` | the question names no address space |
| 157 | record_store_word | n f0 | PTX | `st.u32 [<record>+{offset}], {from};` | the question names no address space |
| 158 | word_sub_first | r4 r4 r8 | PTX | `add.s64 {to}, {left}, -{right}; \| shr.u64 %rd336, {to}, 32; \| cvt.u32.u64 %r825, %rd336; \| and.b32 <carry>, %r825, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 159 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 160 | word_sub_first | r4 r4 r8 | PTX | `add.s64 {to}, {left}, -{right}; \| shr.u64 %rd339, {to}, 32; \| cvt.u32.u64 %r830, %rd339; \| and.b32 <carry>, %r830, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 161 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 162 | test_word_zero | r6 r4 | PTX | `setp.eq.s32 {where}, {value}, 0;` |  |
| 163 | word_funnel_right | r4 r4 r4 n | PTX | `bfi.b64 %rd342, {high}, {low}, 32, 32; \| shr.u64 {to}, %rd342, {bits};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 164 | word_funnel_right | r4 r4 r4 n | PTX | `bfi.b64 %rd346, {high}, {low}, 32, 32; \| shr.u64 {to}, %rd346, {bits};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 165 | word_funnel_right | r4 f0 r4 n | PTX | `bfi.b64 %rd350, {high}, {low}, 32, 32; \| shr.u64 {to}, %rd350, {bits};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 166 | word_funnel_right | r4 f0 r4 n | PTX | `bfi.b64 %rd354, {high}, {low}, 32, 32; \| shr.u64 {to}, %rd354, {bits};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 167 | word_sub | r4 r4 r8 | PTX | `add.s32 {to}, {left}, -{right};` |  |
| 168 | word_sub | r4 r4 r8 | PTX | `add.s32 {to}, {left}, -{right};` |  |
| 169 | word_sub_first | r4 r4 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd359, {to}, 32; \| cvt.u32.u64 %r841, %rd359; \| and.b32 <carry>, %r841, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 170 | word_sub_middle | r4 r4 r0 | PTX | `sub.s64 %rd363, {left}, {right}; \| sub.s64 {to}, %rd363, <carry>; \| shr.u64 %rd365, {to}, 32; \| cvt.u32.u64 %r843, %rd365; \| and.b32 <carry>, %r843, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 171 | word_sub_middle | r4 r4 f0 | PTX | `sub.s64 %rd369, {left}, {right}; \| sub.s64 {to}, %rd369, <carry>; \| shr.u64 %rd371, {to}, 32; \| cvt.u32.u64 %r845, %rd371; \| and.b32 <carry>, %r845, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 172 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 173 | predicate_bitand | r6 r6 r6 | PTX | `and.pred {where}, {left}, {right};` |  |
| 174 | word_bitand | r4 r4 r4 | PTX | `and.b32 {to}, {right}, {left};` |  |
| 175 | word_sub_first | r4 r4 r4 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd375, {to}, 32; \| cvt.u32.u64 %r856, %rd375; \| and.b32 <carry>, %r856, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 176 | word_sub_middle | r4 r4 r4 | PTX | `sub.s64 %rd379, {left}, {right}; \| sub.s64 {to}, %rd379, <carry>; \| shr.u64 %rd381, {to}, 32; \| cvt.u32.u64 %r858, %rd381; \| and.b32 <carry>, %r858, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 177 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 178 | word_funnel_right | r4 r4 f0 n | PTX | `bfi.b64 %rd384, {high}, {low}, 32, 32; \| shr.u64 {to}, %rd384, {bits};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 179 | word_funnel_right | r4 r4 f0 n | PTX | `bfi.b64 %rd388, {high}, {low}, 32, 32; \| shr.u64 {to}, %rd388, {bits};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 180 | word_copy | r4 f0 | PTX | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 181 | word_add_first | r0 r0 r0 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 182 | word_add_middle | r0 r0 r0 | PTX | `add.s64 %rd397, {right}, {left}; \| add.s64 {to}, %rd397, <carry>; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 183 | word_add_middle | r0 r0 f0 | PTX | `add.s64 %rd403, {right}, {left}; \| add.s64 {to}, %rd403, <carry>; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 184 | word_add_last | r0 r0 f0 | PTX | `add.s32 %r867, {right}, {left}; \| add.s32 {to}, %r867, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 185 | word_bitand | r0 r0 r8 | PTX | `and.b32 {to}, {left}, {right};` |  |
| 186 | word_bitand | r0 r0 r8 | PTX | `and.b32 {to}, {left}, {right};` |  |
| 187 | word_bitxor | r0 r4 r4 | PTX | `xor.b32 {to}, {right}, {left};` |  |
| 188 | predicate_bitxor | r6 r6 r6 | PTX | `xor.pred {where}, {left}, {right};` |  |
| 189 | word_bitand | r0 r4 r4 | PTX | `and.b32 {to}, {right}, {left};` |  |
| 190 | word_sub_first | r4 f0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd409, {to}, 32; \| cvt.u32.u64 %r882, %rd409; \| and.b32 <carry>, %r882, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 191 | word_sub_last | r4 f0 r0 | PTX | `sub.s32 %r887, {left}, {right}; \| sub.s32 {to}, %r887, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 192 | test_wide_lt_and | f11 f4 f5 f11 | PTX | `setp.lt.u64 %p54, {left}, {right}; \| and.pred {where}, %p54, {also};` | the compiler writes 2 instructions where the ruleset writes 1 |
| 193 | word_add_first | r0 r0 r0 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 194 | word_add_last | r0 r0 r0 | PTX | `add.s32 %r894, {right}, {left}; \| add.s32 {to}, %r894, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 195 | word_sub_first | r4 r0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd419, {to}, 32; \| cvt.u32.u64 %r896, %rd419; \| and.b32 <carry>, %r896, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 196 | word_sub_middle | r4 r0 r0 | PTX | `sub.s64 %rd423, {left}, {right}; \| sub.s64 {to}, %rd423, <carry>; \| shr.u64 %rd425, {to}, 32; \| cvt.u32.u64 %r898, %rd425; \| and.b32 <carry>, %r898, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 197 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 198 | word_sub_first | r4 f0 r4 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd429, {to}, 32; \| cvt.u32.u64 %r903, %rd429; \| and.b32 <carry>, %r903, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 199 | word_sub_last | r4 f0 r4 | PTX | `sub.s32 %r908, {left}, {right}; \| sub.s32 {to}, %r908, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 200 | word_add_first | r0 r0 r0 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 201 | word_add_middle | r0 r0 r0 | PTX | `add.s64 %rd437, {right}, {left}; \| add.s64 {to}, %rd437, <carry>; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 3 instructions where the ruleset writes 1 |
| 202 | word_add_last | r0 f0 f0 | PTX | `add.s32 %r913, {right}, {left}; \| add.s32 {to}, %r913, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 203 | word_sub_first | r4 r0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd443, {to}, 32; \| cvt.u32.u64 %r915, %rd443; \| and.b32 <carry>, %r915, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 204 | word_sub_middle | r4 r0 r0 | PTX | `sub.s64 %rd447, {left}, {right}; \| sub.s64 {to}, %rd447, <carry>; \| shr.u64 %rd449, {to}, 32; \| cvt.u32.u64 %r917, %rd449; \| and.b32 <carry>, %r917, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 205 | word_sub_middle | r4 f0 f0 | PTX | `sub.s64 %rd453, {left}, {right}; \| sub.s64 {to}, %rd453, <carry>; \| shr.u64 %rd455, {to}, 32; \| cvt.u32.u64 %r919, %rd455; \| and.b32 <carry>, %r919, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 206 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 207 | launch_load_wide | f6 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 208 | launch_load_wide | f6 n | PTX | `ld.u64 {to}, [<launch>+{offset}];` | the ruleset names no fixed register launch |
| 209 | wide_unpack | r0 r0 f1 | PTX | `shr.u64 {high}, {low}, 32;` | the system folds the register put for from; the reading does not name from |
| 210 | test_wide_nonzero | r6 f1 | PTX | `setp.ne.s64 {where}, {value}, 0;` |  |
| 211 | word_mul | r4 r4 r8 | PTX | `shl.b32 {to}, {left}, 1;` | the system folds the number put for right; the reading does not name right |
| 212 | word_mul | r4 r4 r8 | PTX | `mul.lo.s32 {to}, {left}, {right};` |  |
| 213 | wide_mul_word | r5 r4 n | PTX | `mul.wide.u32 {to}, {left}, {right};` |  |
| 214 | wide_mul_word | r5 r4 n | PTX | `mul.wide.u32 {to}, {left}, {right};` |  |
| 215 | wide_add | r5 f6 r5 | PTX | `add.s64 {to}, {right}, {left};` |  |
| 216 | global_load_constant_word | r0 r5 n | PTX | `ld.u32 {to}, [{address}+{offset}];` | the question names no address space |
| 217 | global_load_constant_word | r0 r5 n | PTX | `ld.u32 {to}, [{address}+{offset}];` | the question names no address space |
| 218 | word_add_first | r0 r0 r0 | PTX | `add.s64 {to}, {right}, {left}; \| shr.u64 <carry>, {to}, 32;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 219 | word_add_last | r0 r0 f0 | PTX | `add.s32 %r936, {right}, {left}; \| add.s32 {to}, %r936, <carry>;` | the compiler writes 2 instructions where the ruleset writes 1 |
| 220 | word_sub_first | r4 r0 r0 | PTX | `sub.s64 {to}, {left}, {right}; \| shr.u64 %rd477, {to}, 32; \| cvt.u32.u64 %r938, %rd477; \| and.b32 <carry>, %r938, 1;` | the compiler writes 4 instructions where the ruleset writes 1 |
| 221 | word_sub_middle | r4 r0 f0 | PTX | `sub.s64 %rd481, {left}, {right}; \| sub.s64 {to}, %rd481, <carry>; \| shr.u64 %rd483, {to}, 32; \| cvt.u32.u64 %r940, %rd483; \| and.b32 <carry>, %r940, 1;` | the compiler writes 5 instructions where the ruleset writes 1 |
| 222 | word_borrow_read | r4 r6 | PTX | `neg.s32 {borrow}, <carry>; \| setp.ne.s32 {where}, <carry>, 0;` | the ruleset names no fixed register carry |
| 4097 | launch_load_wide | f2 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4098 | launch_load_wide | f2 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4099 | wide_mul | r5 f1 r8 | SASS | `IMAD R254, {left}.hi, 0x98, RZ; \| IMAD.WIDE.U32 R26, {left}, 0x98, RZ; \| IMAD R254, {left}, {right}.hi, R254; \| IMAD.IADD {to}.hi, R27, 0x1, R254; \| IMAD.MOV.U32 {to}, RZ, RZ, R26;` | the system folds the number put for right; more scratch words than one; the number 152 stands 2 times;  |
| 4100 | wide_mul | r5 f1 r8 | SASS | `IMAD R254, {left}.hi, 0x94, RZ; \| IMAD.WIDE.U32 R26, {left}, 0x94, RZ; \| IMAD R254, {left}, {right}.hi, R254; \| IMAD.IADD {to}.hi, R27, 0x1, R254; \| IMAD.MOV.U32 {to}, RZ, RZ, R26;` | the system folds the number put for right; more scratch words than one; the number 148 stands 2 times;  |
| 4101 | wide_mul | r5 f1 r8 | SASS, numbers loaded | `IMAD R254, {left}.hi, {right}, RZ; \| IMAD.WIDE.U32 R28, {left}, {right}, RZ; \| IMAD R254, {left}, {right}.hi, R254; \| IMAD.IADD {to}.hi, R29, 0x1, R254; \| IMAD.MOV.U32 {to}, RZ, RZ, R28;` | more scratch words than one;  |
| 4102 | wide_add | f2 f2 r5 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ; \| IMAD.X {to}.hi, {left}.hi, 0x1, {right}.hi, P6;` | the ruleset holds no form of the name |
| 4103 | launch_load_wide | f3 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4104 | launch_load_wide | f3 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4105 | test_wide_nonzero | f9 f3 | SASS | `ISETP.NE.U32.AND P6, PT, {value}, RZ, PT; \| ISETP.NE.AND.EX {where}, PT, {value}.hi, RZ, PT, P6;` | the ruleset holds no form of the name |
| 4106 | launch_load_wide | f5 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4107 | launch_load_wide | f5 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4108 | test_wide_eq | f10 f5 n | SASS | `ISETP.EQ.U32.AND P6, PT, {left}, {right}, PT; \| ISETP.EQ.AND.EX {where}, PT, {left}.hi, {right}.hi, PT, P6;` | the ruleset holds no form of the name |
| 4109 | test_wide_eq | f10 f5 n | SASS | `ISETP.EQ.U32.AND P6, PT, {left}, {right}, PT; \| ISETP.EQ.AND.EX {where}, PT, {left}.hi, {right}.hi, PT, P6;` | the ruleset holds no form of the name |
| 4110 | test_wide_eq | f10 f5 n | SASS, numbers loaded | `ISETP.EQ.U32.AND P6, PT, {left}, {right}, PT; \| ISETP.EQ.AND.EX {where}, PT, {left}.hi, {right}.hi, PT, P6;` | the ruleset holds no form of the name |
| 4111 | wide_select | f4 n f1 f10 | SASS | `SEL {to}, {otherwise}, RZ, !{where}; \| SEL {to}.hi, {otherwise}.hi, {chosen}.hi, !{where};` | the system folds the number put for chosen; the ruleset holds no form of the name |
| 4112 | wide_select | f4 n f1 f10 | SASS | `SEL {to}, {otherwise}, {chosen}, !{where}; \| SEL {to}.hi, {otherwise}.hi, {chosen}.hi, !{where};` | the ruleset holds no form of the name |
| 4113 | wide_select | f4 n f1 f10 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where}; \| SEL {to}.hi, {chosen}.hi, {otherwise}.hi, {where};` | the ruleset holds no form of the name |
| 4114 | wide_add | r5 r5 r8 | SASS | `IADD3 {to}, P6, RZ, {left}, RZ; \| IADD3.X {to}.hi, {left}.hi, {right}.hi, RZ, P6, !PT;` | the system folds the number put for right; the ruleset holds no form of the name |
| 4115 | wide_add | r5 r5 r8 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ; \| IADD3.X {to}.hi, {left}.hi, {right}.hi, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4116 | wide_add | r5 r5 r8 | SASS, numbers loaded | `IADD3 {to}, P6, {left}, {right}, RZ; \| IMAD.X {to}.hi, {left}.hi, 0x1, {right}.hi, P6;` | the ruleset holds no form of the name |
| 4117 | wide_shl | r5 r5 n | SASS | `SHF.L.U64.HI {to}.hi, {from}, {bits}, {from}.hi; \| IMAD.SHL.U32 {to}, {from}, 0x4, RZ;` | the ruleset holds no form of the name |
| 4118 | wide_shl | r5 r5 n | SASS | `SHF.L.U64.HI {to}.hi, {from}, {bits}, {from}.hi; \| IMAD.SHL.U32 {to}, {from}, 0x8, RZ;` | the ruleset holds no form of the name |
| 4119 | wide_shl | r5 r5 n | SASS, numbers loaded | `SHF.L.U64.HI {to}.hi, {from}, {bits}, {from}.hi; \| SHF.L.U32 {to}, {from}, {bits}, RZ;` | the ruleset holds no form of the name |
| 4120 | wide_add | r5 f3 r5 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ; \| IMAD.X {to}.hi, {left}.hi, 0x1, {right}.hi, P6;` | the ruleset holds no form of the name |
| 4121 | global_load_word_if | f9 r4 r5 | SASS | `@!{where} BRA 0x7b50; \| LDG.E.CONSTANT {to}, [{address}.64];` | the reading holds a branch |
| 4122 | wide_from_word_if | f9 f4 r4 | SASS | `SEL {to}, {from}, {to}, {where}; \| SEL {to}.hi, {to}.hi, RZ, !{where};` | the ruleset holds no form of the name |
| 4123 | test_wide_lt | f11 f4 f5 | SASS | `ISETP.LT.U32.AND P6, PT, {left}, {right}, PT; \| ISETP.LT.U32.AND.EX {where}, PT, {left}.hi, {right}.hi, PT, P6;` | the ruleset holds no form of the name |
| 4124 | wide_select | f4 f4 n f11 | SASS | `SEL {to}, {chosen}, RZ, {where}; \| SEL {to}.hi, {chosen}.hi, {otherwise}.hi, {where};` | the system folds the number put for otherwise; the ruleset holds no form of the name |
| 4125 | wide_select | f4 f4 n f11 | SASS | `SEL {to}, {chosen}, {otherwise}, {where}; \| SEL {to}.hi, {chosen}.hi, {otherwise}.hi, {where};` | the ruleset holds no form of the name |
| 4126 | wide_select | f4 f4 n f11 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where}; \| SEL {to}.hi, {chosen}.hi, {otherwise}.hi, {where};` | the ruleset holds no form of the name |
| 4127 | launch_load_wide | r7 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4128 | launch_load_wide | r7 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4129 | wide_mul | r5 f4 r8 | SASS | `IMAD.SHL.U32 R254, {left}.hi, 0x20, RZ; \| IMAD.WIDE.U32 R26, {left}, 0x20, RZ; \| IMAD R254, {left}, {right}.hi, R254; \| IMAD.IADD {to}.hi, R27, 0x1, R254; \| IMAD.MOV.U32 {to}, RZ, RZ, R26;` | the system folds the number put for right; more scratch words than one; the number 32 stands 2 times;  |
| 4130 | wide_mul | r5 f4 r8 | SASS | `IMAD R254, {left}.hi, 0x1c, RZ; \| IMAD.WIDE.U32 R26, {left}, 0x1c, RZ; \| IMAD R254, {left}, {right}.hi, R254; \| IMAD.IADD {to}.hi, R27, 0x1, R254; \| IMAD.MOV.U32 {to}, RZ, RZ, R26;` | the system folds the number put for right; more scratch words than one; the number 28 stands 2 times;  |
| 4131 | wide_mul | r5 f4 r8 | SASS, numbers loaded | `IMAD R254, {left}.hi, {right}, RZ; \| IMAD.WIDE.U32 R28, {left}, {right}, RZ; \| IMAD R254, {left}, {right}.hi, R254; \| IMAD.IADD {to}.hi, R29, 0x1, R254; \| IMAD.MOV.U32 {to}, RZ, RZ, R28;` | more scratch words than one;  |
| 4132 | wide_add | r7 r7 r5 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ; \| IMAD.X {to}.hi, {left}.hi, 0x1, {right}.hi, P6;` | the ruleset holds no form of the name |
| 4133 | word_mul_add | f8 f7 r8 f8 | SASS | `IMAD {to}, {left}, {right}, {added};` | the ruleset holds no form of the name |
| 4134 | word_mul_add | f8 f7 r8 f8 | SASS | `IMAD {to}, {left}, {right}, {added};` | the ruleset holds no form of the name |
| 4135 | word_mul_add | f8 f7 r8 f8 | SASS, numbers loaded | `IMAD {to}, {left}, {right}, {added};` | the ruleset holds no form of the name |
| 4136 | global_load_constant_word | r3 r7 n | SASS | `LDG.E.CONSTANT {to}, [{address}.64+{offset}];` | the ruleset holds no form of the name |
| 4137 | global_load_constant_word | r3 r7 n | SASS | `LDG.E.CONSTANT {to}, [{address}.64+{offset}];` | the ruleset holds no form of the name |
| 4138 | word_copy | r0 r3 | SASS | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 4139 | word_bitand | r4 r0 r8 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4140 | word_bitand | r4 r0 r8 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4141 | word_bitand | r4 r0 r8 | SASS, numbers loaded | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4142 | test_word_nonzero | r6 r4 | SASS | `ISETP.NE.AND {where}, PT, {value}, RZ, PT;` | the ruleset holds no form of the name |
| 4143 | word_select | r0 r4 r0 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4144 | sign_select | r4 n n r6 | SASS | `` | the system folds the number put for otherwise; the compiler writes no instruction for it |
| 4145 | sign_select | r4 n n r6 | SASS | `` | the system folds the number put for otherwise; the compiler writes no instruction for it |
| 4146 | sign_select | r4 n n r6 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4147 | word_bitor | r4 r0 r0 | SASS | `LOP3.LUT {to}, {right}, {left}, RZ, 0xfc, !PT;` | the ruleset holds no form of the name |
| 4148 | word_bitor | r4 r4 r0 | SASS | `LOP3.LUT {to}, {right}, {left}, RZ, 0xfc, !PT;` | the ruleset holds no form of the name |
| 4149 | sign_select | r1 r4 n r6 | SASS | `SEL {to}, {chosen}, RZ, {where};` | the system folds the number put for otherwise; the ruleset holds no form of the name |
| 4150 | sign_select | r1 r4 n r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4151 | sign_select | r1 r4 n r6 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4152 | sign_select | r1 n n r6 | SASS | `SEL {to}, RZ, {chosen}, !{where};` | the system folds the number put for otherwise; the ruleset holds no form of the name |
| 4153 | sign_select | r1 n n r6 | SASS | `SEL {to}, R17, {otherwise}, {where};` | the system folds the number put for chosen; the ruleset holds no form of the name |
| 4154 | sign_select | r1 n n r6 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4155 | test_word_nonzero | r6 r0 | SASS | `ISETP.NE.AND {where}, PT, {value}, RZ, PT;` | the ruleset holds no form of the name |
| 4156 | word_set | r0 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4157 | word_set | r0 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4158 | word_set | r0 n | SASS, numbers loaded | `` | the compiler writes no instruction for it |
| 4159 | word_sub_first | r4 f0 r0 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4160 | word_sub_middle | r4 f0 r0 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4161 | word_sub_last | r4 f0 r0 | SASS | `IMAD.X {to}, {left}, 0x1, ~{right}, P6;` | the ruleset holds no form of the name |
| 4162 | word_mul_low | r0 r0 r0 r0 | SASS | `IMAD R254, {left}, {right}, RZ; \| IADD3 {to}, P6, R254, {added}, RZ;` | the ruleset holds no form of the name |
| 4163 | word_mul_high | r4 r0 r0 | SASS | `IMAD.HI.U32 R254, {left}, {right}, RZ; \| IMAD.X {to}, R254, 0x1, <zero>, P6;` | the ruleset holds no form of the name |
| 4164 | word_add_first | r0 r0 r4 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4165 | word_add_last | r4 r4 f0 | SASS | `IMAD.X {to}, {left}, 0x1, {right}, P6;` | the ruleset holds no form of the name |
| 4166 | word_add_middle | r0 r0 f0 | SASS | `IADD3.X {to}, P6, {left}, {right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4167 | word_add_last | r0 r0 f0 | SASS | `IMAD.X {to}, {left}, 0x1, {right}, P6;` | the ruleset holds no form of the name |
| 4168 | word_add | r0 r0 r4 | SASS | `IMAD.IADD {to}, {left}, 0x1, {right};` | the ruleset holds no form of the name |
| 4169 | sign_mul | r1 r1 r1 | SASS | `IMAD {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4170 | test_signed_word_negative | r6 r1 | SASS | `ISETP.LT.AND {where}, PT, {value}, RZ, PT;` | the ruleset holds no form of the name |
| 4171 | word_select | r4 r4 r0 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4172 | word_select | r4 r4 f0 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4173 | word_bitand | r4 r4 r8 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4174 | word_bitand | r4 r4 r8 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4175 | word_bitand | r4 r4 r8 | SASS, numbers loaded | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4176 | word_set | r2 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4177 | word_set | r2 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4178 | word_set | r2 n | SASS, numbers loaded | `` | the compiler writes no instruction for it |
| 4179 | word_bitor | r2 r2 r4 | SASS | `LOP3.LUT {to}, {right}, {left}, RZ, 0xfc, !PT;` | the ruleset holds no form of the name |
| 4180 | record_store_word | n r2 | SASS | `STG.E [<record>.64+{offset}], {from};` | the ruleset holds no form of the name |
| 4181 | record_store_word | n r2 | SASS | `STG.E [<record>.64+{offset}], {from};` | the ruleset holds no form of the name |
| 4182 | word_sub_last | r4 f0 f0 | SASS | `IMAD.X {to}, {left}, 0x1, ~{right}, P6;` | the ruleset holds no form of the name |
| 4183 | word_add_first | r0 r0 r0 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4184 | word_add_last | r0 f0 f0 | SASS | `IMAD.X {to}, {left}, 0x1, {right}, P6;` | the ruleset holds no form of the name |
| 4185 | word_sub_first | r4 r0 r0 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4186 | word_sub_middle | r4 r0 f0 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4187 | word_sub_middle | r4 f0 f0 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4188 | word_borrow_read | r4 r6 | SASS | `IMAD.X {borrow}, <zero>, 0x1, ~<zero>, P6; \| ISETP.NE.U32.AND {where}, PT, {borrow}, RZ, PT;` | the ruleset holds no form of the name |
| 4189 | sign_mul | r4 r1 r1 | SASS | `IMAD {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4190 | test_signed_word_negative | r6 r4 | SASS | `ISETP.LT.AND {where}, PT, {value}, RZ, PT;` | the ruleset holds no form of the name |
| 4191 | word_select | r4 r4 r4 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4192 | sign_select | r4 r1 r1 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4193 | test_sign_ne | r6 r1 n | SASS | `ISETP.NE.AND {where}, PT, {left}, RZ, PT;` | the system folds the number put for right; the ruleset holds no form of the name |
| 4194 | test_sign_ne | r6 r1 n | SASS | `ISETP.NE.AND {where}, PT, {left}, {right}, PT;` | the ruleset holds no form of the name |
| 4195 | test_sign_ne | r6 r1 n | SASS, numbers loaded | `ISETP.NE.AND {where}, PT, {left}, {right}, PT;` | the ruleset holds no form of the name |
| 4196 | sign_select | r4 r4 r4 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4197 | word_sub_first | r4 f0 r4 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4198 | word_sub_middle | r4 f0 r4 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4199 | word_sub_last | r4 f0 r4 | SASS | `IMAD.X {to}, {left}, 0x1, ~{right}, P6;` | the ruleset holds no form of the name |
| 4200 | word_shl | r4 r4 n | SASS | `IMAD.SHL.U32 {to}, {from}, 0x8, RZ;` | the system folds the number put for bits; the ruleset holds no form of the name |
| 4201 | word_shl | r4 r4 n | SASS | `IMAD.SHL.U32 {to}, {from}, 0x4, RZ;` | the system folds the number put for bits; the ruleset holds no form of the name |
| 4202 | word_shl | r4 r4 n | SASS, numbers loaded | `SHF.L.U32 {to}, {from}, {bits}, RZ;` | the ruleset holds no form of the name |
| 4203 | word_shr | r4 r4 n | SASS | `SHF.R.U32.HI {to}, RZ, {bits}, {from};` | the ruleset holds no form of the name |
| 4204 | word_shr | r4 r4 n | SASS | `SHF.R.U32.HI {to}, RZ, {bits}, {from};` | the ruleset holds no form of the name |
| 4205 | word_shr | r4 r4 n | SASS, numbers loaded | `SHF.R.U32.HI {to}, RZ, {bits}, {from};` | the ruleset holds no form of the name |
| 4206 | word_add_middle | r0 r0 r0 | SASS | `IADD3.X {to}, P6, {left}, {right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4207 | word_add_middle | r0 f0 r0 | SASS | `IADD3.X {to}, P6, {left}, {right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4208 | word_sub_middle | r4 r0 r0 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4209 | sign_neg | r4 r1 | SASS | `IMAD.MOV {to}, RZ, RZ, -{from};` | the ruleset holds no form of the name |
| 4210 | sign_mul | r4 r1 r4 | SASS | `IMAD {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4211 | sign_select | r4 r4 r1 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4212 | sign_select | r4 r1 r4 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4213 | word_copy | r0 r0 | SASS | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 4214 | sign_absolute | r1 r1 | SASS | `IABS {to}, {from};` | the ruleset holds no form of the name |
| 4215 | word_copy | r4 r0 | SASS | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 4216 | word_bitor | r4 r4 r4 | SASS | `LOP3.LUT {to}, {right}, {left}, RZ, 0xfc, !PT;` | the ruleset holds no form of the name |
| 4217 | sign_select | r4 r4 n r6 | SASS | `SEL {to}, {chosen}, RZ, {where};` | the system folds the number put for otherwise; the ruleset holds no form of the name |
| 4218 | sign_select | r4 r4 n r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4219 | sign_select | r4 r4 n r6 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4220 | test_sign_gt | r6 r1 r1 | SASS | `ISETP.GT.AND {where}, PT, {left}, {right}, PT;` | the ruleset holds no form of the name |
| 4221 | test_sign_ne | r6 r1 r1 | SASS | `ISETP.NE.AND {where}, PT, {left}, {right}, PT;` | the ruleset holds no form of the name |
| 4222 | sign_select | r1 r4 r4 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4223 | sign_absolute | r0 r1 | SASS | `IABS {to}, {from};` | the ruleset holds no form of the name |
| 4224 | word_sub | r4 f0 r0 | SASS | `IMAD.IADD {to}, {left}, 0x1, -{right};` | the ruleset holds no form of the name |
| 4225 | word_set | r0 r8 | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4226 | word_set | r0 r8 | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4227 | word_set | r0 r8 | SASS, numbers loaded | `` | the compiler writes no instruction for it |
| 4228 | sign_set | r1 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4229 | sign_set | r1 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4230 | sign_set | r1 n | SASS, numbers loaded | `` | the compiler writes no instruction for it |
| 4231 | word_sub | r4 r1 r8 | SASS | `IADD3 {to}, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4232 | word_sub | r4 r1 r8 | SASS | `IADD3 {to}, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4233 | word_sub | r4 r1 r8 | SASS, numbers loaded | `IMAD.IADD {to}, {left}, 0x1, -{right};` | the ruleset holds no form of the name |
| 4234 | word_set | r4 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4235 | word_set | r4 n | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4236 | word_set | r4 n | SASS, numbers loaded | `` | the compiler writes no instruction for it |
| 4237 | word_set | r4 r8 | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4238 | word_set | r4 r8 | SASS | `` | the system folds the number put for value; the compiler writes no instruction for it |
| 4239 | word_set | r4 r8 | SASS, numbers loaded | `` | the compiler writes no instruction for it |
| 4240 | word_mul_low | r4 r0 r4 r4 | SASS | `IMAD R254, {left}, {right}, RZ; \| IADD3 {to}, P6, R254, {added}, RZ;` | the ruleset holds no form of the name |
| 4241 | word_mul_high | r4 r0 r4 | SASS | `IMAD.HI.U32 R254, {left}, {right}, RZ; \| IMAD.X {to}, R254, 0x1, <zero>, P6;` | the ruleset holds no form of the name |
| 4242 | word_add | r4 r4 r4 | SASS | `IMAD.IADD {to}, {left}, 0x1, {right};` | the ruleset holds no form of the name |
| 4243 | word_add_first | r4 r4 r4 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4244 | word_select | r4 f0 r8 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4245 | word_select | r4 f0 r8 r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4246 | word_select | r4 f0 r8 r6 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4247 | word_sub_first | r4 r0 r4 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4248 | word_add | r4 r4 r8 | SASS | `IADD3 {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4249 | word_add | r4 r4 r8 | SASS | `IADD3 {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4250 | word_add | r4 r4 r8 | SASS, numbers loaded | `IMAD.IADD {to}, {left}, 0x1, {right};` | the ruleset holds no form of the name |
| 4251 | word_add_last | r4 r4 r4 | SASS | `IMAD.X {to}, {left}, 0x1, {right}, P6;` | the ruleset holds no form of the name |
| 4252 | word_copy | r0 r4 | SASS | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 4253 | sign_select | r1 r1 n r6 | SASS | `SEL {to}, {chosen}, RZ, {where};` | the system folds the number put for otherwise; the ruleset holds no form of the name |
| 4254 | sign_select | r1 r1 n r6 | SASS | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4255 | sign_select | r1 r1 n r6 | SASS, numbers loaded | `SEL {to}, {chosen}, {otherwise}, {where};` | the ruleset holds no form of the name |
| 4256 | launch_load_wide | r5 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4257 | launch_load_wide | r5 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4258 | global_add_atomic_word | r5 | SASS | `VOTEU.ANY UR6, UPT, PT; \| UFLO.U32 UR7, UR6; \| POPC R254, UR6; \| ISETP.EQ.U32.AND P6, PT, R0, UR7, PT; \| @P6 RED.E.ADD.STRONG.GPU [{address}.64], R254;` | the ruleset holds no form of the name |
| 4259 | record_store_word | n f0 | SASS | `STG.E [<record>.64+{offset}], {from};` | the ruleset holds no form of the name |
| 4260 | record_store_word | n f0 | SASS | `STG.E [<record>.64+{offset}], {from};` | the ruleset holds no form of the name |
| 4261 | word_sub_first | r4 r4 r8 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4262 | word_sub_first | r4 r4 r8 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4263 | word_sub_first | r4 r4 r8 | SASS, numbers loaded | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4264 | test_word_zero | r6 r4 | SASS | `ISETP.EQ.AND {where}, PT, {value}, RZ, PT;` | the ruleset holds no form of the name |
| 4265 | word_funnel_right | r4 r4 r4 n | SASS | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4266 | word_funnel_right | r4 r4 r4 n | SASS | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4267 | word_funnel_right | r4 r4 r4 n | SASS, numbers loaded | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4268 | word_funnel_right | r4 f0 r4 n | SASS | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4269 | word_funnel_right | r4 f0 r4 n | SASS | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4270 | word_funnel_right | r4 f0 r4 n | SASS, numbers loaded | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4271 | word_sub | r4 r4 r8 | SASS | `IADD3 {to}, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4272 | word_sub | r4 r4 r8 | SASS | `IADD3 {to}, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4273 | word_sub | r4 r4 r8 | SASS, numbers loaded | `IMAD.IADD {to}, {left}, 0x1, -{right};` | the ruleset holds no form of the name |
| 4274 | word_sub_first | r4 r4 r0 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4275 | word_sub_middle | r4 r4 r0 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4276 | word_sub_middle | r4 r4 f0 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4277 | predicate_bitand | r6 r6 r6 | SASS | `ISETP.NE.U32.AND {where}, PT, {left}, RZ, {right};` | the ruleset holds no form of the name |
| 4278 | word_bitand | r4 r4 r4 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4279 | word_sub_first | r4 r4 r4 | SASS | `IADD3 {to}, P6, {left}, -{right}, RZ;` | the ruleset holds no form of the name |
| 4280 | word_sub_middle | r4 r4 r4 | SASS | `IADD3.X {to}, P6, {left}, ~{right}, RZ, P6, !PT;` | the ruleset holds no form of the name |
| 4281 | word_funnel_right | r4 r4 f0 n | SASS | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4282 | word_funnel_right | r4 r4 f0 n | SASS | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4283 | word_funnel_right | r4 r4 f0 n | SASS, numbers loaded | `SHF.R.U32 {to}, {low}, {bits}, {high};` | the ruleset holds no form of the name |
| 4284 | word_copy | r4 f0 | SASS | `` | the system folds the register put for to; the compiler writes no instruction for it |
| 4285 | word_bitand | r0 r0 r8 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4286 | word_bitand | r0 r0 r8 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4287 | word_bitand | r0 r0 r8 | SASS, numbers loaded | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4288 | word_bitxor | r0 r4 r4 | SASS | `LOP3.LUT {to}, {right}, {left}, RZ, 0x3c, !PT;` | the ruleset holds no form of the name |
| 4289 | predicate_bitxor | r6 r6 r6 | SASS | `ISETP.NE.U32.XOR {where}, PT, {left}, RZ, {right};` | the ruleset holds no form of the name |
| 4290 | word_bitand | r0 r4 r4 | SASS | `LOP3.LUT {to}, {left}, {right}, RZ, 0xc0, !PT;` | the ruleset holds no form of the name |
| 4291 | test_wide_lt_and | f11 f4 f5 f11 | SASS | `ISETP.LT.U32.AND P6, PT, {left}, {right}, PT; \| ISETP.LT.U32.AND.EX {where}, PT, {left}.hi, {right}.hi, {also}, P6;` | the ruleset holds no form of the name |
| 4292 | word_add_last | r0 r0 r0 | SASS | `IMAD.X {to}, {left}, 0x1, {right}, P6;` | the ruleset holds no form of the name |
| 4293 | launch_load_wide | f6 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4294 | launch_load_wide | f6 n | SASS | `LD.E.64 {to}, [<launch>.64+{offset}];` | the ruleset holds no form of the name |
| 4295 | wide_unpack | r0 r0 f1 | SASS | `` | the system folds the register put for low; the compiler writes no instruction for it |
| 4296 | test_wide_nonzero | r6 f1 | SASS | `ISETP.NE.U32.AND P6, PT, {value}, RZ, PT; \| ISETP.NE.AND.EX {where}, PT, {value}.hi, RZ, PT, P6;` | the ruleset holds no form of the name |
| 4297 | word_mul | r4 r4 r8 | SASS | `IMAD.SHL.U32 {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4298 | word_mul | r4 r4 r8 | SASS | `IMAD {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4299 | word_mul | r4 r4 r8 | SASS, numbers loaded | `IMAD {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4300 | wide_mul_word | r5 r4 n | SASS | `IMAD.WIDE.U32 {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4301 | wide_mul_word | r5 r4 n | SASS | `IMAD.WIDE.U32 {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4302 | wide_mul_word | r5 r4 n | SASS, numbers loaded | `IMAD.WIDE.U32 {to}, {left}, {right}, RZ;` | the ruleset holds no form of the name |
| 4303 | wide_add | r5 f6 r5 | SASS | `IADD3 {to}, P6, {left}, {right}, RZ; \| IMAD.X {to}.hi, {left}.hi, 0x1, {right}.hi, P6;` | the ruleset holds no form of the name |
| 4304 | global_load_constant_word | r0 r5 n | SASS | `LDG.E.CONSTANT {to}, [{address}.64+{offset}];` | the ruleset holds no form of the name |
| 4305 | global_load_constant_word | r0 r5 n | SASS | `LDG.E.CONSTANT {to}, [{address}.64+{offset}];` | the ruleset holds no form of the name |

## By form

| form | sass.krs | SASS read | | ptx.krs | PTX read | |
|---|---|---|---|---|---|---|
| global_add_atomic_word | `` | `` | kept: the ruleset holds no form of the name | `red.global.add.u32 [{address}], 1;` | `` | kept: the question names no address space |
| global_load_constant_word | `` | `` | kept: the ruleset holds no form of the name | `ld.global.nc.u32 {to}, [{address}+{offset}];` | `` | kept: the question names no address space |
| global_load_word_if | `` | `` | kept: the reading holds a branch | `@{where} ld.global.nc.u32 {to}, [{address}];` | `` | kept: the reading holds a branch |
| launch_load_wide | `` | `` | kept: the ruleset holds no form of the name | `ld.u64 {to}, [%launch+{offset}];` | `` | kept: the ruleset names no fixed register launch |
| predicate_bitand | `` | `` | kept: the ruleset holds no form of the name | `and.pred {where}, {left}, {right};` | `and.pred {where}, {left}, {right};` | same |
| predicate_bitxor | `` | `` | kept: the ruleset holds no form of the name | `xor.pred {where}, {left}, {right};` | `xor.pred {where}, {left}, {right};` | same |
| record_store_word | `` | `` | kept: the ruleset holds no form of the name | `st.global.u32 [%record+{offset}], {from};` | `` | kept: the question names no address space |
| sign_absolute | `` | `` | kept: the ruleset holds no form of the name | `abs.s32 {to}, {from};` | `` | kept: the compiler writes 6 instructions where the ruleset writes 1 |
| sign_mul | `` | `` | kept: the ruleset holds no form of the name | `mul.lo.s32 {to}, {left}, {right};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| sign_neg | `` | `` | kept: the ruleset holds no form of the name | `neg.s32 {to}, {from};` | `` | kept: the compiler writes 3 instructions where the ruleset writes 1 |
| sign_select | `` | `` | kept: the ruleset holds no form of the name | `selp.s32 {to}, {chosen}, {otherwise}, {where};` | `` | kept: the compiler writes 3 instructions where the ruleset writes 1 |
| sign_set | `` | `` | kept: the system folds the number put for value; the compiler writes no instruction for it | `mov.s32 {to}, {value};` | `` | kept: the system folds the number put for value; the compiler writes no instruction for it |
| test_sign_gt | `` | `` | kept: the ruleset holds no form of the name | `setp.gt.s32 {where}, {left}, {right};` | `` | kept: the compiler writes 3 instructions where the ruleset writes 1 |
| test_sign_ne | `` | `` | kept: the ruleset holds no form of the name | `setp.ne.s32 {where}, {left}, {right};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| test_signed_word_negative | `` | `` | kept: the ruleset holds no form of the name | `setp.lt.s32 {where}, {value}, 0;` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| test_wide_eq | `` | `` | kept: the ruleset holds no form of the name | `setp.eq.s64 {where}, {left}, {right};` | `setp.eq.s64 {where}, {left}, {right};` | same |
| test_wide_lt | `` | `` | kept: the ruleset holds no form of the name | `setp.lt.u64 {where}, {left}, {right};` | `setp.lt.u64 {where}, {left}, {right};` | same |
| test_wide_lt_and | `` | `` | kept: the ruleset holds no form of the name | `setp.lt.and.u64 {where}, {left}, {right}, {also};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| test_wide_nonzero | `` | `` | kept: the ruleset holds no form of the name | `setp.ne.s64 {where}, {value}, 0;` | `setp.ne.s64 {where}, {value}, 0;` | same |
| test_word_nonzero | `` | `` | kept: the ruleset holds no form of the name | `setp.ne.s32 {where}, {value}, 0;` | `setp.ne.s32 {where}, {value}, 0;` | same |
| test_word_zero | `` | `` | kept: the ruleset holds no form of the name | `setp.eq.s32 {where}, {value}, 0;` | `setp.eq.s32 {where}, {value}, 0;` | same |
| wide_add | `` | `` | kept: the ruleset holds no form of the name | `add.s64 {to}, {left}, {right};` | `` | kept: the questions read apart |
| wide_from_word_if | `` | `` | kept: the ruleset holds no form of the name | `@{where} cvt.u64.u32 {to}, {from};` | `` | kept: the compiler writes 3 instructions where the ruleset writes 1 |
| wide_mul | `` | `` | kept: the system folds the number put for right; more scratch words than one; the number 28 stands 2 times;  | `mul.lo.u64 {to}, {left}, {right};` | `mul.lo.s64 {to}, {left}, {right};` | read |
| wide_mul_word | `` | `` | kept: the ruleset holds no form of the name | `mul.wide.u32 {to}, {left}, {right};` | `mul.wide.u32 {to}, {left}, {right};` | same |
| wide_select | `` | `` | kept: the ruleset holds no form of the name | `selp.b64 {to}, {chosen}, {otherwise}, {where};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| wide_shl | `` | `` | kept: the ruleset holds no form of the name | `shl.b64 {to}, {from}, {bits};` | `shl.b64 {to}, {from}, {bits};` | same |
| wide_unpack | `` | `` | kept: the system folds the register put for low; the compiler writes no instruction for it | `mov.b64 {{low}, {high}}, {from};` | `` | kept: the system folds the register put for from; the reading does not name from |
| word_add | `` | `` | kept: the ruleset holds no form of the name | `add.u32 {to}, {left}, {right};` | `` | kept: the questions read apart |
| word_add_first | `` | `` | kept: the ruleset holds no form of the name | `add.cc.u32 {to}, {left}, {right};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| word_add_last | `` | `` | kept: the ruleset holds no form of the name | `addc.u32 {to}, {left}, {right};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| word_add_middle | `` | `` | kept: the ruleset holds no form of the name | `addc.cc.u32 {to}, {left}, {right};` | `` | kept: the compiler writes 3 instructions where the ruleset writes 1 |
| word_bitand | `` | `` | kept: the ruleset holds no form of the name | `and.b32 {to}, {left}, {right};` | `` | kept: the questions read apart |
| word_bitor | `` | `` | kept: the ruleset holds no form of the name | `or.b32 {to}, {right}, {left};` | `or.b32 {to}, {right}, {left};` | same |
| word_bitxor | `` | `` | kept: the ruleset holds no form of the name | `xor.b32 {to}, {right}, {left};` | `xor.b32 {to}, {right}, {left};` | same |
| word_borrow_read | `` | `` | kept: the ruleset holds no form of the name | `subc.u32 {borrow}, %zero, %zero; \| setp.ne.u32 {where}, {borrow}, 0;` | `` | kept: the ruleset names no fixed register carry |
| word_copy | `` | `` | kept: the system folds the register put for to; the compiler writes no instruction for it | `mov.b32 {to}, {from};` | `` | kept: the system folds the register put for to; the compiler writes no instruction for it |
| word_funnel_right | `` | `` | kept: the ruleset holds no form of the name | `shf.r.clamp.b32 {to}, {low}, {high}, {bits};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| word_mul | `` | `` | kept: the ruleset holds no form of the name | `mul.lo.u32 {to}, {left}, {right};` | `mul.lo.s32 {to}, {left}, {right};` | read |
| word_mul_add | `` | `` | kept: the ruleset holds no form of the name | `mad.lo.s32 {to}, {left}, {right}, {added};` | `mad.lo.s32 {to}, {left}, {right}, {added};` | same |
| word_mul_high | `` | `` | kept: the ruleset holds no form of the name | `madc.hi.u32 {to}, {left}, {right}, %zero;` | `` | kept: the compiler writes 4 instructions where the ruleset writes 1 |
| word_mul_low | `` | `` | kept: the ruleset holds no form of the name | `mad.lo.cc.u32 {to}, {left}, {right}, {added};` | `` | kept: the compiler writes 4 instructions where the ruleset writes 1 |
| word_select | `` | `` | kept: the ruleset holds no form of the name | `selp.b32 {to}, {chosen}, {otherwise}, {where};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| word_set | `` | `` | kept: the system folds the number put for value; the compiler writes no instruction for it | `mov.u32 {to}, {value};` | `mov.u32 {to}, {value};` | same |
| word_shl | `` | `` | kept: the system folds the number put for bits; the ruleset holds no form of the name | `shl.b32 {to}, {from}, {bits};` | `shl.b32 {to}, {from}, {bits};` | same |
| word_shr | `` | `` | kept: the ruleset holds no form of the name | `shr.u32 {to}, {from}, {bits};` | `shr.u32 {to}, {from}, {bits};` | same |
| word_sub | `` | `` | kept: the ruleset holds no form of the name | `sub.u32 {to}, {left}, {right};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| word_sub_first | `` | `` | kept: the ruleset holds no form of the name | `sub.cc.u32 {to}, {left}, {right};` | `` | kept: the compiler writes 5 instructions where the ruleset writes 1 |
| word_sub_last | `` | `` | kept: the ruleset holds no form of the name | `subc.u32 {to}, {left}, {right};` | `` | kept: the compiler writes 2 instructions where the ruleset writes 1 |
| word_sub_middle | `` | `` | kept: the ruleset holds no form of the name | `subc.cc.u32 {to}, {left}, {right};` | `` | kept: the compiler writes 5 instructions where the ruleset writes 1 |

431 questions over 50 forms. Of the forms, sass.krs already gives 0 as read and 0 are read otherwise; ptx.krs gives 15 as read and 2 are read otherwise.
