// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the pi_tower_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef PI_TOWER_INTERNAL_H
#define PI_TOWER_INTERNAL_H

// Pi turns at the boundary, and the turn builds a tower.
//
// The boundary is the circle of length 1, and the turn is x -> x + pi. On the circle the turn is the rotation by
// alpha = pi - 3. The turn never closes, and it never lands on a place a second time. Its returns near the start are
// its floors. The return map to the arc under a close return is again a rotation, by the angle of the Gauss map. The
// counts of turns on each floor are the partial quotients of pi, [3; 7, 15, 1, 292, ...].
//
// The sources are Weyl, 1916, and Sos, 1958, the three gap theorem. Also van Ravenstein, "The three gap theorem
// (Steinhaus conjecture)", Journal of the Australian Mathematical Society A 45, 1988.
//
// Cut the boundary into N = 2^k cells. The turn fills every cell at an exact step. The sim reads that step and the
// cell it fills from the floors, and it does not walk the turn.
//
// Everything is exact. Machin's formula in integers brackets pi. The sim holds the turn as the integer rotation
// y_n = n A mod 2^P, with A = floor(alpha 2^P). A first hit is the least n with y_n in a window. Euclid's descent
// through the floors finds it. A first hit also tells if the integer turn is the real one, cell for cell, and the sim
// checks it.
//
// 1. Machin's bracket gives pi: floor(pi 2^P) is one integer at both ends of the bracket. Its first 64 bits after
//    the point are the published 0x243F6A8885A308D3.
// 2. The floors are the partial quotients of pi, and the bracket certifies them. They begin 7, 15, 1, 292, ... as
//    published.
// 3. The closest returns of the turn are the floors. At a closest return the turn comes nearer its start than at
//    every step before. These steps are exactly the denominators q_j of the convergents, up to
//    2^PI_TOWER_RECORD_BITS.
// 4. The integer turn is the real turn. At every resolution, no step up to the searched one lands in a cell other
//    than the real one.
// 5. The floors give the step that fills the boundary. A walk of every step gives the same step at 2^1 to
//    2^PI_TOWER_WALK_MAX cells, and the same last cell.
// 6. At every resolution in the request, the step of the fill is the first step that touches the last cell.
//
// The request gives the resolutions. pi_tower [n] reads 2^n alone, and pi_tower [from] [to] reads 2^from to 2^to.
// With no argument the sim reads 2^1 to 2^100. The largest gives the precision: P = 3 n + 64 bits, up to a whole
// word, and at least PI_TOWER_BITS. The exact width must hold 3 (P + PI_TOWER_GUARD) + 64 bits. A build with a
// smaller width stops with the reason, and SIM_EXACT_LIMBS sets the width.
//
// The arc. Roll the boundary into a cylinder over our disk. The turn is then the helix of radius 1 / (2 pi) that
// rises 1 / pi for each turn. It goes through our disk at the marks {n pi}. Its plane of curvature is at the angle
// phi = arctan(1 / pi) to our disk. The ratio of its torsion to its curvature is tan phi (Lancret, 1806).
//
// 7. Pi is transcendental (Lindemann, 1882), and in Q(pi) pi is thus a free variable. The Frenet frame of the
//    helix comes from its derivatives at the four quarter turns. It gives curvature 2 pi^3 / (pi^2 + 1) and
//    torsion 2 pi^2 / (pi^2 + 1). Then tau / kappa = 1 / pi = tan phi. The Darboux vector tau T + kappa B lies
//    along the axis of our disk.
// 8. The bracket gives the same numbers: kappa, tau and phi in degrees. The sim prints each one only where both
//    ends agree.
//
// The balance and the boundary.
//
// 9. On the helix the pull points at our axis, and the torque about the axis is zero. At unit speed the angular
//    momentum about the axis is L_z^2 = 1 / (4 (pi^2 + 1)) at all four quarter turns, in Q(pi). The bracket
//    gives L_z to 12 places.
// 10. The billiard in the unit square starts from the corner at slope pi, unfolded. The segment in the lattice
//    cell (i, j) carries L about the center, with p = (1, pi). Here L = (-1)^(i + j) ((j + 1/2) - pi (i + 1/2)). A zero
//    L needs pi = (2j + 1) / (2i + 1), and a corner needs a whole pi (i + 1). Each tie needs a rational pi.
//
//    The sim walks the billiard from the integer turn over 2^PI_TOWER_BILLIARD_BITS columns. The certainty of the
//    turn decides every floor on the walk. No wall hit is a corner, and no segment has a zero L. The record
//    near-corners of the walk are exactly the q_j. The sim measures how often the lead changes, and the longest
//    lead. It also measures the share of each side, and the nearest tie.
// 11. The residue. Take every floor with q_j up to 2^PI_TOWER_RESIDUE_BITS. The first q_j steps of pi have the
//    whole parts of the permutation n -> n p_j mod q_j. At step q_j, pi is delta_j = q_j pi - (3 q_j + p_j) off
//    the whole.
// 12. The golden helix. The residues change sign on every floor, q_j >= F_(j+1), and |delta_j| < 1 / q_(j+1). The
//    shrink |delta_j| / |delta_(j-1)| is above 1/2 exactly where a_(j+1) = 1. The sim measures each shrink against
//    the golden 1 / phi. It measures the growth q_J^(1/J) of a floor against phi.
//
// The deepest turns of the tower, on the engine. The turn at depth n is bit n of alpha, the fraction of pi. The BBP
// formula reads it in place, without the bits before it (Bailey, Borwein and Plouffe, Mathematics of Computation 66,
// 1997). The formula is {16^d pi} = {4 S_1 - 2 S_4 - S_5 - S_6}, with S_j the sum over i of
// 16^(d - i) / (8 i + j). Each term is a lane of the record machine of the engine, and each square of its power
// goes mod 8 i + j. Keymath sizes every register, the scheduler puts them in place, and tessera admits the job.
//
// 13. On the engine, the first 64 bits of pi after the point are 0x243F6A8885A308D3. Its hex digits at the
//     published positions of Bailey are his.
// 14. At the deepest resolution, the low 64 bits of the cell of the engine are those of the exact turn.
//
// A resolution past 2^20 is (2, n), with n whole or base^exponent (10^100 for a googol). The sim prints its
// precision and widths exact. The engine sums the terms at depth n sweep by sweep, and prints the terms done and its
// rate. The sim prints the digits only after the engine sums every term.
//
// 15. A segment. pi_tower segment d [digits] reads that many hex digits from position d + 1, with d whole or
//     base^exponent. It reads them as windows at d, d + 20, d + 40, ..., each its own run on the engine. Every
//     window certifies at least 24 digits. The 4 or more past the 20 that a window gives must be the first of the
//     next window. Bailey checks a result at position d with a run at d - 1 or d + 1 ("The BBP Algorithm for Pi",
//     2006). Where the segment starts at a published position of Bailey, its head is his.

#include "sim.h"

#include "../../../../../cu/engine/analysis/cycle/cycle.h"
#include "../../../../../cu/engine/analysis/key_schedule/key_schedule.h"
#include "../../../../../cu/engine/analysis/keymath/keymath.h"

#include <signal.h>

// the walks' turn's precision, in bits after the point, and the least precision of the searched turn
#define PI_TOWER_BITS 384u

// the guard bits of Machin's bracket past the precision of the turn
#define PI_TOWER_GUARD 32u

// the resolutions read when the request names none
#define PI_TOWER_RESOLUTION_FIRST 1u

#define PI_TOWER_RESOLUTION_LAST 100u

#define PI_TOWER_WALK_MAX 24u

// the floor table and the closest-return check reach 2^PI_TOWER_RECORD_BITS steps
#define PI_TOWER_RECORD_BITS 112u

// the searched turn's records reach PI_TOWER_RECORD_MARGIN bits past the largest resolution
#define PI_TOWER_RECORD_MARGIN 12u

// the residue check walks every floor whose q_j is at max 2^PI_TOWER_RESIDUE_BITS
#define PI_TOWER_RESIDUE_BITS 21u

#define PI_TOWER_WORDS (PI_TOWER_BITS / 64u)

// the first 64 bits of pi after the point
#define PI_TOWER_PUBLISHED_BITS 0x243F6A8885A308D3ULL

#define PI_TOWER_PUBLISHED_FLOORS 21u

// the walk of the billiard goes over 2^PI_TOWER_BILLIARD_BITS columns of its unfolded lattice
#define PI_TOWER_BILLIARD_BITS 24u

// the record near-corners of the billiard at max: 4 for each bit of its columns, and 4 more
#define PI_TOWER_NEAR_CORNERS_MAX ((4u * PI_TOWER_BILLIARD_BITS) + 4u)

// the bits after the point that the sum of the engine resolves past its error, and the guard past them. The guard stops
// a carry that leaves them undecided
#define PI_TOWER_BBP_CERTIFIED 96u

#define PI_TOWER_BBP_GUARD 16u

// A segment takes this many hex digits from each window, 4 fewer than the least a window must certify. The sim checks
// the 4 or more past them against the first of the next window
#define PI_TOWER_SEGMENT_STEP ((PI_TOWER_BBP_CERTIFIED / 4u) - 4u)

// the hex digits a segment reads when the request names none, and the max it may name
#define PI_TOWER_SEGMENT_DIGITS 1000u

#define PI_TOWER_SEGMENT_MAX 65536u

// the deepest resolution of the exact turn; past it the sim reads a resolution on the engine alone
#define PI_TOWER_EXACT_MAX (1ULL << 20u)

extern int g_pi_tower_error;

extern int g_pi_tower_records_short;

typedef OriorExactInteger PiWide;

// one level of the descent: the rotation a on m, and the window's low end
typedef struct
{
    PiWide multiplier;
    PiWide modulus;
    PiWide low;
} PiTowerLevel;

// a step at which the turn sets a record, and where it is then
typedef struct
{
    PiWide step;
    PiWide place;
} PiTowerRecord;

// The searched turn: A = floor(alpha 2^precision) on 2^precision, its records to 2^range steps. The records from below
// and from above are each one cell of the persistent tier, taken at their count
typedef struct
{
    PiWide multiplier;
    PiWide modulus;
    unsigned int precision;
    unsigned int range;
    PiTowerRecord *lowest;
    unsigned int lowest_count;
    PiTowerRecord *highest;
    unsigned int highest_count;
} PiTowerTurn;

// The floors: the denominator q_j of each convergent and the partial quotient a_j that makes it, from q_0 = 1 and a_0 =
// 0. Each is one cell of the persistent tier, taken at the count of the floors that the bracket certifies
typedef struct
{
    PiWide *denominator;
    PiWide *quotient;
    unsigned int count;
} PiTowerFloors;

PiWide pi_tower_unsigned(unsigned long long number);

PiWide pi_tower_power_two(unsigned int bits);

PiWide pi_tower_sum(const PiWide &left, const PiWide &right);

PiWide pi_tower_difference(const PiWide &left, const PiWide &right);

PiWide pi_tower_product(const PiWide &left, const PiWide &right);

PiWide pi_tower_quotient(const PiWide &numerator, const PiWide &divisor);

PiWide pi_tower_remainder(const PiWide &numerator, const PiWide &divisor);

int pi_tower_compare(const PiWide &left, const PiWide &right);

unsigned long long pi_tower_word(const PiWide &value);

int pi_tower_first_hit(PiWide multiplier, PiWide modulus, PiWide low, PiWide high, PiWide *step);

int pi_tower_first_hit_from(const PiWide &multiplier, const PiWide &modulus, const PiWide &from, const PiWide &low,
                            const PiWide &high, PiWide *step);

int pi_tower_hit_between(const PiWide &multiplier, const PiWide &modulus, const PiWide &from, const PiWide &to,
                         const PiWide &low, const PiWide &high);

PiWide pi_tower_arctan(unsigned long long x, unsigned int bits, unsigned long long *terms);

void pi_tower_refused(const char *why);

void *pi_tower_taken(unsigned long long count, unsigned long long size, int ephemeral);

void pi_tower_given(void *cell);

unsigned int pi_tower_euclid_bound(unsigned int bits);

unsigned int pi_tower_records(const PiTowerTurn &turn, int lowest, PiTowerRecord **records);

int pi_tower_covered(const PiTowerTurn &turn, const PiWide &cell, const PiWide &count);

void pi_tower_print_decimal(VerbaLinea *line, PiWide value);

void pi_tower_print_exponent(VerbaLinea *line, PiWide numerator, PiWide denominator);

int pi_tower_bracket(SimResults *results, unsigned int precision, PiWide *alpha, PiWide *low, PiWide *high);

void pi_tower_floors(SimResults *results, const PiTowerTurn &turn, PiTowerFloors *floors);

unsigned long long pi_tower_walk(const PiWide &alpha, unsigned int bits, unsigned long long *last,
                                 unsigned long long *stall, unsigned long long *again, unsigned long long *home,
                                 unsigned long long *closing);

// A polynomial in pi with integer coefficients, the coefficient of pi^i at i, with no zero at the top. It is `count`
// coefficients from `first` of the stack of coefficients that the arc builds on (pi_tower_walk.cu)
typedef struct
{
    unsigned int first;
    unsigned int count;
} PiTowerPolynomial;

// a rational function of pi; pi is transcendental. Two are equal exactly where their cross products are equal as
// polynomials
typedef struct
{
    PiTowerPolynomial numerator;
    PiTowerPolynomial denominator;
} PiTowerFraction;

typedef struct
{
    PiTowerFraction axis[3];
} PiTowerVector;

PiWide pi_tower_root(const PiWide &value);

void pi_tower_arc(SimResults *results, const PiWide &low, const PiWide &high, unsigned int bits);

void pi_tower_billiard(SimResults *results, const PiWide &alpha, const PiTowerFloors *floors);

void pi_tower_residues(SimResults *results, const PiWide &walk_alpha, const PiTowerFloors *floors);

void pi_tower_golden(SimResults *results, const PiTowerTurn &turn, const PiTowerFloors *floors);

// one record program on the engine: the key keymath encodes, the layout the scheduler arranges, and the record cycle
// loads
typedef struct
{
    EngineRecordKey key;
    EngineRecordLayout layout;
    CycleRecord *record;
} PiTowerProgram;

// The hex digits of pi from position d + 1 on the engine. Term i <= d is a lane, and it takes 16^(d - i) mod 8 i + j by
// powers in base 16. Each square goes mod 8 i + j, and the lane floors the fraction of that residue to W bits. Tail
// term d + t is a lane of its own, 2^W / ((8 (d + t) + j) 16^t) floored. The pair program adds the sums of two lanes
// mod 2^W, and its rounds reduce a sweep to one. Each lane wraps its sum to W = 32 k - 1 bits, and with its sign the
// sum fills k whole limbs.
typedef struct
{
    PiWide position;
    PiWide terms;
    unsigned int position_bits;
    unsigned int fraction_bits;
    unsigned int tail_terms;
    unsigned int tail_bits;
    unsigned int in_limbs;
    unsigned int out_limbs;
    unsigned long long lanes;
    PiTowerProgram term;
    PiTowerProgram tail;
    PiTowerProgram pair;
} PiTowerBbp;

// how far a run on the engine reached: the terms it summed, the seconds it took, and the sum where it summed every term
typedef struct
{
    unsigned long long done;
    unsigned long long sweeps;
    double seconds;
    int finished;
    PiWide sum;
} PiTowerBbpRun;

extern volatile sig_atomic_t g_pi_tower_stopped;

void pi_tower_stop(int signal_number);

unsigned int pi_tower_bit_length(const PiWide &value);

// the steps of one record program at max
#define PI_TOWER_STEPS_MAX 65536u

// the steps of one record program, in a static table at a bound with their count; a program past it ends the run
typedef struct
{
    EngineRecordStep step[PI_TOWER_STEPS_MAX];
    unsigned int count;
} PiTowerSteps;

unsigned int pi_tower_step(PiTowerSteps *steps, EngineRecordOperation operation, unsigned int left, unsigned int right);

unsigned int pi_tower_field(PiTowerSteps *steps, unsigned int field, unsigned int member);

unsigned int pi_tower_constant(PiTowerSteps *steps, const PiWide &value);

void pi_tower_bbp_combine(PiTowerSteps *steps, const unsigned int *fraction, unsigned int fraction_bits);

void pi_tower_bbp_free(PiTowerBbp *bbp);

PiWide pi_tower_bbp_error(const PiTowerBbp *bbp);

int pi_tower_bbp_plan(PiTowerBbp *bbp, const PiWide &position, unsigned long long lanes, EngineError *error);

unsigned long long pi_tower_bbp_bytes(const PiTowerBbp *bbp);

int pi_tower_bbp_counted(SimResults *results, unsigned int *out, unsigned long long count, unsigned int limbs,
                         unsigned long long first);

unsigned int *pi_tower_bbp_reduce(SimResults *results, const PiTowerBbp *bbp, unsigned int *const *buffers,
                                  const unsigned int *index, unsigned long long count, EngineError *error);

int pi_tower_bbp_read(SimResults *results, const PiTowerBbp *bbp, const unsigned int *total, PiWide *sum);

void pi_tower_bbp_progress(SimResults *results, const PiTowerBbp *bbp, const PiTowerBbpRun *run);

int pi_tower_bbp_sum(SimResults *results, PiTowerBbp *bbp, int report, PiTowerBbpRun *run);

unsigned int pi_tower_bbp_certified(const PiTowerBbp *bbp, const PiWide &sum);

// The room for a sum's hex digits and their terminator. A sum fills at max the limbs of the exact width, and each limb
// holds 8 digits
#define PI_TOWER_HEX_MAX ((8u * ORIOR_EXACT_LIMBS) + 1u)

void pi_tower_bbp_hex(const PiTowerBbp *bbp, const PiWide &sum, unsigned int digits, char *hex, size_t room);

void pi_tower_bbp_print(SimResults *results, const PiTowerBbp *bbp, const PiTowerBbpRun *run, unsigned int certified);

unsigned long long pi_tower_bbp_lanes(void);

// Bailey's table of pi's hex digits, each at the position of its first digit (D. H. Bailey, "The BBP Algorithm for
// Pi", 2006, table 1), and the 23 digits his text gives from position 1,000,001
typedef struct
{
    unsigned long long position;
    const char *digits;
} PiTowerPublished;

static const PiTowerPublished s_pi_tower_published[] = {
    {1ULL, "243F6A8885A308D3"},      {1000000ULL, "26C65E52CB4593"},   {1000001ULL, "6C65E52CB459350050E4BB1"},
    {10000000ULL, "17AF5863EFED8D"}, {100000000ULL, "ECB840E21926EC"},
};

#define PI_TOWER_PUBLISHED_COUNT (sizeof(s_pi_tower_published) / sizeof(s_pi_tower_published[0]))

PiWide pi_tower_depth_position(const PiWide &depth, unsigned int *window);

unsigned long long pi_tower_bbp_cell(const PiTowerBbp *bbp, const PiWide &sum, unsigned int window);

// how a segment went: if every window ran, if the engine summed every window, and if every overlap agreed
typedef struct
{
    int ran;
    int finished;
    int certified;
    int overlapped;
    unsigned int windows;
} PiTowerSegment;

void pi_tower_segment_request(SimResults *results, int count, char **arguments, const PiWide &position,
                              unsigned int digits);

void pi_tower_engine(SimResults *results, int count, char **arguments, const PiTowerTurn &turn, unsigned int depth);

void pi_tower_deep(SimResults *results, int count, char **arguments, const PiWide &depth);

int pi_tower_request(const char *text, PiWide *value);

int pi_tower_resolution(const char *text, unsigned int *bits);

#endif
