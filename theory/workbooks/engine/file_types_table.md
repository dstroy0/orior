# Engine file types

Every file extension the engine and its projects write or read. A new extension is Doug's to name. Before one is
proposed, it is checked against this table and against the whole biohub tree, orior_python included.

## In use

| Extension | What it is | Named by |
|---|---|---|
| `.kcr` | Kolmogorov information crystal: its own compression format. It reads every source format, takes each format's own compression out, and can rebuild the data into any format (Doug). krep kind "KCR\0", version 1. The only crystal format. | Doug |
| `.krs` | Kolmogorov information ruleset: one language's forms (`ptx.krs`, `c.krs`, `vhdl.krs`). | Doug |
| `.kcs` | Kolmogorov information construction set: what reconstructs information. A target's forms and their costs are its construction set. krep kind "KCS\0". | Doug |
| `.knf` | Kolmogorov noise floor. A sample's floor is its entropy history. krep kind "KNF\0". | Doug |
| `.kdm` | Kolmogorov device map: the hardware map. | Doug |
| `.ksc` | Kolmogorov system classification: the language map. | Doug |

## Retired

| Extension | What it was |
|---|---|
| `.iapx` | The crystal before `.kcr`. Not read; sets are re-ingested. |

## Offered, not adopted

| Extension | Offered for |
|---|---|
| `.kfc` `.kst` `.kgr` `.ksd` | The other apx files (flattened, OAPX history, BAPX bodies, IMP key); still open with Doug (build_plan.md). |
| `.khw` | A separate hardware constraints file. Withdrawn (Doug: file creep); the constraints are entries of the language's `.krs`. |
