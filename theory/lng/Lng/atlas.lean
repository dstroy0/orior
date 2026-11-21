import Lng.charts
import Lng.g_rows

/-!
# The atlas, its charts and their fibers

`charts` holds each row of each target table, as harness.py lng prove writes it. A chart is the rows of one target.
A relation of a chart is a row that is no type. Its bridge word takes it to an anchor through the anchor column of
the transpiler table, which `anchors` holds.

* `charted`: each row of each chart is a type, or a witnessed relation whose bridge word has an anchor.
* `lifts`: each relation of each chart has a row with the same bridge word in every other chart.
* `fibers`: each anchor reaches as many symbols in one chart as in every other.
* `many_to_many`: the anchor add reaches two symbols of C and two symbols of Python.
-/

namespace Lng.Atlas

open Lng.Charts Lng.GRows

/-- The anchor of the bridge word `b`. -/
def anchorOf (b : String) : Option String := (anchors.find? (fun p => p.1 == b)).map (·.2)

/-- The targets of the atlas, one for each chart. -/
def targets : List String := ["c", "python"]

/-- The relations of the chart of `target`. -/
def relations (target : String) : List ChartRow :=
  charts.filter (fun r => r.target == target && r.type != "type")

/-- The fiber of the anchor `a` in the chart of `target`: each symbol whose bridge word has the anchor `a`. -/
def fiber (target a : String) : List String :=
  ((relations target).filter (fun r => anchorOf r.bridge == some a)).map (·.symbol)

/-- Each anchor that a relation of some chart reaches. -/
def reached : List String := ((charts.filterMap (fun r => anchorOf r.bridge)).foldl
  (fun seen a => if seen.contains a then seen else seen ++ [a]) [])

theorem charted : charts.all (fun r =>
    r.type == "type" || (r.witness != "" && (anchorOf r.bridge).isSome)) = true := by decide

theorem lifts : targets.all (fun l => targets.all (fun m =>
    (relations l).all (fun r => (relations m).any (fun s => s.bridge == r.bridge)))) = true := by decide

set_option maxRecDepth 20000 in
theorem fibers : reached.all (fun a => targets.all (fun l => targets.all (fun m =>
    (fiber l a).length == (fiber m a).length))) = true := by decide

theorem many_to_many :
    fiber "c" "add" = ["+", "+="] ∧ fiber "python" "add" = ["+ & 0xffffffff", "+= &= 0xffffffff"] := by decide

end Lng.Atlas
