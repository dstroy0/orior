// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_field.h: fields and their projections (orior.h includes the parts in order)
#ifndef ORIOR_FIELD_H
#define ORIOR_FIELD_H

#include "orior_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Answers whether two positions carry the same symbol.
     *
     * @param[in] field     Whatever the caller searches, opaque to the engine [BORROWS].
     * @param[in] corpus_at Position in the field.
     * @param[in] needle_at Position in the pattern.
     * @return              Non-zero where the two positions carry the same symbol, zero otherwise.
     *
     * @note THIS IS THE WHOLE INTERFACE THAT THE ENGINE NEEDS TO A SYMBOL. Not an order, not a hash, not a size, not an
     *       enumeration of the alphabet. Equality at two positions only, the one relation that the soundness proof
     *       uses. A subset of a pattern's points is a necessary condition, and the proof reads neither order, dimension
     *       nor alphabet.
     * @note A symbol may therefore be a byte, a 32 bit sample, an exact rational, or a point in eight dimensions. It
     *       may also be a pointer compared by identity, or a value that only its owner can compare. The engine never
     *       learns which. An alphabet with no enumeration and no hash costs it nothing.
     * @note NO ENGINE HERE IS NARROWER THAN THE PROOF. bench_lattice takes a callback, and the python cascade needs no
     *       bytes. Its `survivors` indexes a dict by symbol, and `positions_by_symbol` builds that dict from any
     *       iterable of values. It requires equality and hashability only, and a crystallography example feeds it
     *       element strings. A C entry that took only a `uint8_t *` would be narrower than the proof that it
     *       implements. It would also be narrower than the python engine of the tests.
     * @note The two are not equivalent, and the difference runs this way. A dict key must be hashable, and this oracle
     *       tests only whether two positions are equal. A value with no hash, or with an expensive equality and a false
     *       hash, can go through this search and not through that one. A person who grades the two engines against each
     *       other should know which fields only one of them can accept.
     * @warning Must be a pure function of the two positions for the duration of a call. The engine reads the same
     *          position more than once, and assumes that the answer does not move under it.
     */
    typedef int (*OriorSameAt)(const void *field, size_t corpus_at, size_t needle_at);

    /**
     * @brief A field of any symbol type, reached only through equality.
     *
     * @note Carries no element size and no element pointer. The engine indexes positions and queries the oracle about
     *       them. Where the symbols live and how wide they are belong to the caller.
     * @warning `alignments` is the number of positions that can host a pattern, which for a linear field of `n` symbols
     *          is `n - needle_len + 1`. The engine cannot compute it, because it does not know the shape of the field.
     *          A caller that supplies it wrongly gets a wrong sweep, and no error.
     */
    typedef struct
    {
        OriorSameAt same;  /**< Equality oracle. Never null. */
        const void *field; /**< Passed to the oracle untouched, never dereferenced here [BORROWS]. May
                            *   be null ONLY where the ORACLE does not dereference it either: an oracle
                            *   that reaches its data some other way. The engine never uses it. "if
                            *   unused" reads as a condition always met, and the condition is on the
                            *   oracle. A null to an oracle that reads it faults inside the oracle,
                            *   where the engine cannot see it. */
        size_t alignments; /**< Positions that can host the pattern. Non-zero. */
        size_t needle_len; /**< Positions the pattern contains. Non-zero. */
    } OriorField;

    /**
     * @brief Projects a field of any symbol type onto a field of rarity ranks.
     *
     * @param[in] args What to project and where to put it [BORROWS].
     * @return         1 where the call wrote the projection, 0 where it errored.
     *
     * @warning RANKS FROM TWO CALLS ARE NOT COMPARABLE, AND A COMPARISON OF THEM LOSES TRUE OCCURRENCES. A rank is not
     *          a property of a symbol. It is a property of a symbol WITHIN THE POPULATION THAT THIS CALL SAW, and the
     *          population is part of the answer.
     *
     *          The class of a position comes from the oracle, which the CALLER supplies, and that relation is the same
     *          whatever field it applies to. The rarity place comes from a count over THIS field, and that is not the
     *          same. The rank fuses the two, and only the first half survives the trip to another field.
     *
     *          It is a hash and a nonce. The class is the digest and the population is the nonce. A digest computed
     *          under one nonce cannot go against a digest computed under another, however identical the input was.
     *
     *          THE FAILURE IS SILENT, AND IT IS IN THE UNSAFE DIRECTION. Take symbols where A occurs once, B ten times
     *          and C a hundred times, and search for the needle C C B A. Projected alone, the corpus ranks A below B,
     *          because one is rarer than ten. Projected alone, the needle holds one A and one B, they TIE, and the tie
     *          breaks on class index the other way round. Rank disagreement then reports symbol disagreement where the
     *          symbols agree. A probe refutes an alignment that truly matches, and the count comes back one short, with
     *          nothing in the return value to report it.
     *
     *          That inverts the necessary condition that this construction rests on. Everywhere else in this file, an
     *          order decision is a null: a wrong order costs reads, and leaves the count unchanged. Here a wrong order
     *          loses a true occurrence.
     *
     *          Use orior_field_pair_project to search one field for another. It numbers both in one call, which gives
     *          one population and one rarity order, and the ranks that it writes are comparable by construction. This
     *          entry stays the one to call for a description of a single field.
     *
     * @note THE COMPONENT COUNT HAS NO BOUND, AND ONLY THE OUTPUT GETS A CLAMP. The projection finds classes with no
     *       ceiling, orders them by rarity across every one of them, and clamps the byte rank at relabel time. A field
     *       with a thousand classes keeps its 255 rarest apart, and merges the commonest into rank 255. That is the
     *       direction that the steering needs. The survivor count of a probe is its class frequency, and the rarest
     *       class is the best probe available. The class buffers exist to carry those components, and the kernel
     *       allocates nothing.
     *
     * @note `same_in_field` NEED NOT BE TRANSITIVE, AND THE CLASSES ARE ITS TRANSITIVE CLOSURE. This matters, because
     *       the predicate that motivates an oracle at all is not transitive. A tolerance match over real valued
     *       coordinates is meaningful, and `a` within tolerance of `b` and `b` of `c` does not put `a` within tolerance
     *       of `c`. `examples/proteins/5_sift/protein_domain.py:66-74` is exactly that, and states why exact equality
     *       is the wrong test on a continuous domain.
     *
     *       Soundness needs agreement to imply a shared rank. It does NOT need a shared rank to imply agreement. The
     *       labels must be a superset of the relation, and the smallest superset that is an equivalence is the
     *       transitive closure. Classes are therefore connected components: a position joins every class that it
     *       matches, and merges them.
     *
     * @warning THE COST OF THE CLOSURE IS CHAINING. A loose tolerance can walk the whole field into one component
     *          through a path of near neighbors, none of which agree with the ends. One class ranks everything alike,
     *          every rank probe then refutes nothing, and the search falls back to the exact compare at every
     *          alignment. That is useless, and it is exactly sound, because a probe that rejects nothing is still a
     *          necessary condition. Tighten the predicate where it happens. The symptom is `distinct` back as one, on a
     *          field that the caller expects to vary.
     * @warning TAKES A DIFFERENT ORACLE FROM OriorField's, AND THE DISTINCTION IS NOT COSMETIC. OriorSameAt as a
     *          descent uses it answers about a CORPUS position against a NEEDLE position, which are two index spaces. A
     *          grouping of a field into classes compares two positions of the FIELD. A descent's oracle here would
     *          index the needle with a field position, and read off the end of it. This signature keeps the two apart.
     *
     * WHY PROJECT AT ALL. A probe only has to be a necessary condition of an occurrence. Within one projection, two
     * positions in one class carry one rank. Rank disagreement proves symbol disagreement, and a rank probe refutes a
     * subset of what a symbol probe refutes. Rank agreement does not prove symbol agreement, and a filter does not need
     * it to. The engine's construction is that a necessary condition may be weaker than the thing that it screens for.
     *
     * That buys the wide path. An equality oracle has no wide form, because a wide compare is a statement about a
     * representation, and the oracle deliberately hides one. Ranks are bytes. A field of any symbol type therefore
     * becomes a field that the byte engine already reads at full speed, AVX2 scan included. A real valued alphabet, a
     * point in eight dimensions and an opaque handle all project to the same shape. All of them run on the same loop.
     *
     * @note Rarity ORDERS the ranks, rarest first, the order that the steering already wants. The rank is therefore not
     *       an arbitrary label. Rank zero is the class that refutes the most alignments, and a planner that reads the
     *       projected field gets the entropy order for free.
     * @note A SEARCH OVER RANK FIELDS COUNTS RANK MATCHES, AND THAT EQUALS THE SYMBOL COUNT ONLY AT 256 CLASSES OR
     *       FEWER. Places then run from 0 to 255, and the clamp touches none. One rank names one class, and the two
     *       counts are the same integer.
     *
     *       Past 256 classes, every class at place 255 or later takes rank 255. Alignments whose symbols differ can
     *       then agree on every rank. The full compare of the byte engine cannot remove them, because on rank fields it
     *       compares ranks. The count is then an upper bound, and it never falls below the symbol count. Case 13 in
     *       utils/test/src/cu/engine/nbody/orior/test_adversarial_*.c measures it above: 44 against 1 on a 300-class
     *       field.
     *
     *       For an exact count past 256 classes, check the rank survivors against the symbols through the oracle, or
     *       give the oracle to a descent directly.
     * @warning COSTS UP TO `length` SQUARED ORACLE CALLS, AND THAT IS NOT A LOOSE BOUND. The transitive closure of a
     *          graph that only a pairwise probe reaches needs the pairs. The predicate may be one that the caller chose
     *          for its approximation. There is no correct shortcut. The only economy is a skip of a pair already in one
     *          component. That is real on a chained field, and nothing on a field of singletons.
     *
     *          A comparison of each position against one representative per class is `length` times `distinct`, and is
     *          correct ONLY for a transitive predicate. This header does not offer it as a fast path. A choice of it
     *          would assert transitivity, and the cost of a wrong assertion is a silently wrong answer.
     *
     *          A field of a few thousand positions projects in a moment, and one of a hundred thousand does not.
     *          Project a representative slice, or skip the projection and pass the oracle straight to a descent. A
     *          descent needs no closure and no projection, and costs nothing extra.
     */
    typedef struct
    {
        OriorSameAt same_in_field;       /**< Equality between two positions OF THE FIELD. Never null. */
        const void *field;               /**< Passed to the oracle untouched, never dereferenced here
                                          *   [BORROWS]. May be null ONLY where the oracle does not
                                          *   dereference it either: an oracle that reaches its data
                                          *   some other way. A null to an oracle that reads it faults
                                          *   inside the oracle, and the engine cannot see that. */
        size_t length;                   /**< Positions to project. Non-zero. */
        uint8_t *ranks;                  /**< One rarity rank per position, written whole [BORROWS]. */
        uint32_t *class_of_position;     /**< Which class each position fell in, one entry per position,
                                          *   written during the call [BORROWS]. */
        uint32_t *members_in_class;      /**< How many positions each class contains, indexed by the class,
                                          *   written during the call [BORROWS]. */
        uint32_t *rarity_place_of_class; /**< Where each class sits in the rarity order, indexed by the
                                          *   class, written during the call [BORROWS]. */
        size_t classes_length;           /**< Entries each of the three above contains. Must reach
                                          *   `length`, since every position can be its own class. */
        size_t *distinct;                /**< Where the call writes the class count, or NULL [BORROWS]. */
    } OriorFieldProjection;

    int orior_field_project(const OriorFieldProjection *args);

    /**
     * @brief Numbers a corpus and a needle in ONE population. Their ranks then compare.
     *
     * @note WHY THIS EXISTS AT ALL. orior_field_project projects one field, and its ranks mean something only inside
     *       it. Two separate calls produce two rarity orders over two populations, and a rank probe run across them
     *       refutes alignments whose symbols agree. The other order decisions in this file cost reads when they are
     *       wrong. This one loses true occurrences. A reader can skip past a warning on the single-field entry. A
     *       caller of this entry cannot reach the broken construction at all, because the entry numbers both sides in
     *       one call.
     *
     * @note THE JOINT INDEX SPACE, which the caller's oracle must answer over. Positions `0` through
     *       `corpus_length - 1` are the corpus. Positions `corpus_length` through `corpus_length + needle_length - 1`
     *       are the needle. `same_in_field` gets queries about two JOINT positions, and must reach whichever side each
     *       one names. Concatenation is the obvious way to represent that, and it is not the only one. The engine never
     *       dereferences the field and does not care.
     *
     * @note THE ORACLE HERE IS FIELD AGAINST FIELD, AND NOT CORPUS AGAINST NEEDLE. It takes two joint positions drawn
     *       from one space. `OriorField.same` in a descent takes a CORPUS position and a NEEDLE position, which are two
     *       spaces. The two have the same C type and different meanings. The compiler cannot catch the swap, and one
     *       passed where the other belongs reads off the end of something.
     */
    typedef struct
    {
        OriorSameAt same_in_field;       /**< Equality between two positions of the JOINT field. Never
                                          *   null. Queried about joint positions, never about a corpus
                                          *   position paired with a needle position. */
        const void *field;               /**< Passed to the oracle untouched, never dereferenced here
                                          *   [BORROWS]. May be null ONLY where the oracle reaches its
                                          *   data some other way. */
        size_t corpus_length;            /**< Corpus positions, at joint `0` onward. Non-zero. */
        size_t needle_length;            /**< Needle positions, at joint `corpus_length` onward.
                                          *   Non-zero, and no longer than `corpus_length`. */
        uint8_t *corpus_ranks;           /**< `corpus_length` ranks for the corpus side [BORROWS]. */
        uint8_t *needle_ranks;           /**< `needle_length` ranks for the needle side [BORROWS]. */
        uint32_t *class_of_position;     /**< Which class each joint position fell in, one entry per
                                          *   joint position, written during the call [BORROWS]. */
        uint32_t *members_in_class;      /**< How many joint positions each class contains, indexed by the
                                          *   class, written during the call [BORROWS]. */
        uint32_t *rarity_place_of_class; /**< Where each class sits in the one rarity order, indexed by
                                          *   the class, written during the call [BORROWS]. */
        size_t classes_length;           /**< Entries each of the three above contains. Must reach
                                          *   `corpus_length + needle_length`, since every joint
                                          *   position can be its own class. */
        size_t *distinct;                /**< Where the call writes the class count over the JOINT
                                          *   field, or NULL [BORROWS]. */
    } OriorFieldPairProjection;

    /**
     * @brief Projects a corpus and a needle onto one rarity order, and writes each side its own ranks.
     *
     * @param[in] args What to project and where to put it [BORROWS].
     * @return         1 where the call wrote both rank arrays, 0 where the call errored.
     *
     * @note WHAT THE CALLER DOES WITH THE RESULT. `corpus_ranks` and `needle_ranks` are byte fields numbered by one
     *       rule. They can go to orior_steer_count, or to any entry here that takes bytes. What the count means depends
     *       on how many classes the joint field contains, and `distinct` informs the caller which case applies.
     *
     * @note AT 256 CLASSES OR FEWER, THE RANK COUNT IS THE EXACT COUNT. Places run from 0 to 255, and the clamp touches
     *       none. One rank names one class, and rank agreement is class agreement. A search over the rank fields
     *       returns the integer that a search over the symbols would.
     *
     * @note PAST 256 CLASSES, THE RANK COUNT IS AN UPPER BOUND. Every class at place 255 or later takes rank 255, and
     *       two different classes can agree on rank. A search over the rank fields counts those alignments too. Its
     *       full compare cannot remove them, because on rank fields the full compare compares ranks. The direction that
     *       the filter needs still holds: one class takes one rank across the whole joint field. Symbol agreement
     *       implies rank agreement, and the search loses no true occurrence.
     *
     *       For an exact count past 256 classes, check the rank survivors against the symbols through the oracle, or
     *       run the descent on the oracle directly.
     *
     * @note Errors, and writes nothing, where `args` or any pointer but `distinct` is null, or where either length is
     *       zero. It also errors where `needle_length` exceeds `corpus_length`, or where the joint length
     *       `corpus_length + needle_length` would wrap `size_t`. It errors where that joint length exceeds
     *       `UINT32_MAX`, because class labels are 32 bit positions and a wider field would alias two of them onto one
     *       label. Last, it errors where `classes_length` does not reach the joint length. The return value is the
     *       whole report, as it is for orior_field_project.
     *
     * @warning COSTS THE SAME CLOSURE AS THE SINGLE FIELD ENTRY, over a longer field. The pairwise closure is quadratic
     *          in `corpus_length + needle_length`. This is for fields small enough to project at all. A descent takes
     *          the oracle directly, needs no closure, and costs nothing extra.
     */
    int orior_field_pair_project(const OriorFieldPairProjection *args);

#ifdef __cplusplus
}
#endif

#endif
