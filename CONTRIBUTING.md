# Contributing

**Code is not accepted.**

## This is a research body with code attached. Three types of change are useful:

**A measurement.** A number with its conditions attached: what corpus, what length, what it was measured against, and what the floor was. A ratio with no denominator is not a result. Every figure in `theory/workbooks/orior` names the tool that produced it, and a new one does the same.

**A correction.** The ledger retains its own corrections and it is the most valuable thing in the repository. A claim that turns out to be wrong stays on the page beside the measurement that killed it. If you find a figure that does not reproduce, the contribution is the run that shows it, added beside the figure instead of an edit that removes it.

**A precedent.** Several results here are rediscoveries of published work. This repository has been wrong about priority before, at Montemurro and Zanette, and the entry says so. If you know the prior art for something claimed here, that is worth more than a patch.

## What is not in the repository

The corpora, page renders and audio under `build/` are also absent and run to about 1.9 GB. `utils/maint/data/salishan/get_papers.py` fetches the papers and the tools rebuild the rest.

## Checks

**The kernel.**:

```sh
utils/maint/engine/build_engine.sh
```

Every arm has to agree with `orior_naive` on every row. A row printing `DIFFER` is a defect and its timing means nothing, because a measurement of an arm returning the wrong answer is a measurement of the wrong program.

**The ports.** `evidence/sims/r/departure.R` and `evidence/sims/matlab/orior_departure.m` carry the permutation null measure. The Python in `evidence/proofs/posits/proof_conservation.py` is the reference: a port is correct when it lands inside the reseeding floor of it, since each language draws its null from a different generator and none of them can agree to the last digit.

## Writing

ASD-STE100 for all technical documents and code documentation. 

## Licensing

Contributions fall under the same terms the repository carries: AGPL-3.0-or-later, or a negotiated commercial license, or an educator's license issued to a person. [Licensing](docs/licensing.md) states the scheme in full.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
