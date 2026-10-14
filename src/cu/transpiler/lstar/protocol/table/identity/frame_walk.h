// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_walk.h: the halving walk over a number a relation holds at on one side of a bound and not past it, in C, for
// the frame's asks and the check that holds them (P11 Step 9)
#ifndef FRAME_WALK_H
#define FRAME_WALK_H

#ifdef __cplusplus
extern "C"
{
#endif

    // `truthy` a number the relation holds at and `falsy` one past it that it does not. Each is moved to the middle
    // the relation gives the same answer as until the two are adjacent, `holds` given `context` at every middle and
    // never the two ends themselves. The truthy end, the greatest number the relation holds at before the bound
    static inline unsigned long long frame_greatest(int (*holds)(unsigned long long, void *), void *context,
                                                    unsigned long long truthy, unsigned long long falsy)
    {
        while ((falsy - truthy) > 1ull)
        {
            const unsigned long long middle = truthy + ((falsy - truthy) / 2ull);
            if (holds(middle, context) != 0)
            {
                truthy = middle;
            }
            else
            {
                falsy = middle;
            }
        }
        return truthy;
    }

#ifdef __cplusplus
}
#endif

#endif
