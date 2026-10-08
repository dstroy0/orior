// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file mass.h
 * @brief Exact masses: an element's isotopes, a formula's monoisotopic mass and an adduct's ion.
 *
 * A mass is an integer count of femtodaltons, 10^-15 Da. Every isotope NIST lists is held at that
 * unit exactly, from the decimal NIST wrote (isotope_table.h, written by isotope_table.c from NIST's text),
 * and the electron is CODATA's, exact at the same unit or one place finer. No element is left out and
 * no set of elements is assumed. A measured m/z is never set against these here: that is the device's,
 * in the window and match programs, from the stored double's own bits.
 */
#ifndef MASS_H
#define MASS_H

#include "../casmi_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One isotope as NIST lists it. */
typedef struct
{
    unsigned int atomic_number;
    char symbol[8];
    unsigned int mass_number;
    long long mass;             /**< femtodaltons */
    long long mass_uncertainty; /**< femtodaltons, one standard uncertainty */
    int mass_estimated;         /**< NIST's '#': the mass is estimated, not measured */
    long long composition;      /**< abundance in 10^-10 parts, -1 where NIST gives none */
    long long composition_uncertainty; /**< the composition's parenthesized uncertainty in 10^-10 parts, -1 where
                                            NIST gives no composition */
    long long weight_low;       /**< the element's standard atomic weight interval, femtodaltons: its low end, -1
                                     where NIST writes no [low,high] interval */
    long long weight_high;      /**< its high end, -1 where NIST writes no interval */
} CasmiIsotope;

/** CODATA 2018 electron mass, 5.48579909065 x 10^-4 Da, in femtodaltons. */
#define CASMI_ELECTRON_MASS 548579909065ll

/**
 * CODATA 2022 electron mass, 5.485799090441(97) x 10^-4 Da, in 10^-16 Da. It is written past the
 * femtodalton; it is held one place finer, exactly.
 */
#define CASMI_ELECTRON_MASS_2022 5485799090441ll

/** Its one standard uncertainty, 97 x 10^-16 Da. */
#define CASMI_ELECTRON_UNCERTAINTY_2022 97ll

/** 10^-16 Da in one femtodalton: a NIST mass in femtodaltons, times this, is exact in 10^-16 Da. */
#define CASMI_TENTHS_PER_FEMTODALTON 10ll

/** Femtodaltons in one dalton. */
#define CASMI_FEMTODALTONS 1000000000000000ll

/** Most distinct elements one formula names. */
#define CASMI_FORMULA_ELEMENTS_MOST 32u

/** A formula as element counts, in the order the text named them. */
typedef struct
{
    unsigned int element_count;
    unsigned int isotope[CASMI_FORMULA_ELEMENTS_MOST]; /**< index into the isotope table, the monoisotope */
    long long count[CASMI_FORMULA_ELEMENTS_MOST];      /**< signed: an adduct term subtracts */
} CasmiFormula;

/** An adduct: how many molecules, the mass it adds, and the charge. */
typedef struct
{
    long long molecules; /**< the 2 of [2M+H]+ */
    long long added;     /**< femtodaltons added to molecules x M, before electrons */
    long long charge;    /**< signed: +1 for [M+H]+, -1 for [M-H]- */
    CasmiFormula terms;  /**< what added sums: each element's signed count, for a reader that sums it itself */
} CasmiAdduct;

/** What casmi_formula_read needs: the text and where the formula goes. */
typedef struct
{
    const char *text;
    unsigned long long length;
    CasmiFormula *formula;
} CasmiFormulaRead;

/** What casmi_adduct_read needs: the text and where the adduct goes. */
typedef struct
{
    const char *text;
    unsigned long long length;
    CasmiAdduct *adduct;
} CasmiAdductRead;

/**
 * The isotope a symbol names as its monoisotope: the one of greatest abundance, or the only one
 * listed where NIST gives no abundance (T, and every element with no stable isotope).
 *
 * @return the isotope index, or ENGINE_BYTES_ERROR where no isotope carries the symbol or it has
 *         several and none has an abundance
 */
long long casmi_isotope_monoisotope(const char *symbol, unsigned long long length);

/** The table itself, for a reader that wants every isotope. */
const CasmiIsotope *casmi_isotope_table(unsigned int *count);

/**
 * Read a formula such as "C10H12N2O" or "C2H4O2": element symbols, each followed by an optional
 * count. A '+' or '-' at the end, a charge some sources write, errors and is not guessed at.
 *
 * @return the element count, or ENGINE_BYTES_ERROR on any symbol NIST does not list, any other
 *         character, or a count past 2^31
 */
long long casmi_formula_read(const CasmiFormulaRead *args);

/**
 * The monoisotopic mass of a formula, in femtodaltons.
 *
 * @return the mass, or ENGINE_BYTES_ERROR where a sum would pass 2^62
 */
long long casmi_formula_mass(const CasmiFormula *formula);

/**
 * Read an adduct such as "[M+H]+", "[M-H2O+H]+", "[2M+Na]+", "[M+2H]2+" or "[M+CH2O2-H]-".
 * A term is an optional count and a formula, added or subtracted.
 *
 * @return 0, or ENGINE_BYTES_ERROR on anything outside that grammar. The charge is in the adduct;
 *         it is not returned, since a charge of -1 is the error's own value.
 */
long long casmi_adduct_read(const CasmiAdductRead *args);

/**
 * The exact ion mass numerator: molecules x M + added - charge x electron, in femtodaltons.
 * The m/z is this over |charge|.
 *
 * @return the numerator, or ENGINE_BYTES_ERROR where it is not positive or would pass 2^62
 */
long long casmi_ion_numerator(long long neutral_mass, const CasmiAdduct *adduct);

#ifdef __cplusplus
}
#endif

#endif
