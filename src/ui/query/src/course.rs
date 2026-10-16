// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The course: the whole protocol as one job, its steps run in order, each a pass of its own with
//! its own run. The course says its steps before the first, each step as it begins, where it stops
//! and that it ended, lines the window's panel reads to draw the line and walk the fire along it.

use crate::commands::Context;

/// One step of the course: its name on the line, the loop it runs and the words it gives it.
pub struct Step {
    pub name: &'static str,
    loop_: fn(&mut Context, &[String]) -> Result<i32, String>,
    words: &'static [&'static str],
}

/// The steps in order: the part's tables are pulled off the vendor's disassembler, the widening discovers
/// forms at the slot, their turns and register runs classify them, the ruleset is written from them,
/// the engine's stick is built from the ruleset, the pairs hold the stick's chains against the part
/// and ask how far a container's code reaches, the forms are timed, and the ruleset is written again
/// with the costs.
pub const STEPS: [Step; 10] = [
    Step { name: "vendor tables", loop_: crate::tables::run, words: &["sm_86"] },
    Step { name: "discover", loop_: crate::learn::run, words: &["discover"] },
    Step { name: "turns", loop_: crate::learn::run, words: &["turns"] },
    Step { name: "registers", loop_: crate::learn::run, words: &["registers"] },
    Step { name: "write krs", loop_: crate::write::run, words: &["krs"] },
    Step { name: "stick engine", loop_: crate::stick::run, words: &["engine"] },
    Step { name: "pairs stick", loop_: crate::pairs::run, words: &["stick"] },
    Step { name: "pairs past", loop_: crate::pairs::run, words: &["past"] },
    Step { name: "costs", loop_: crate::learn::run, words: &["costs"] },
    Step { name: "write krs", loop_: crate::write::run, words: &["krs"] },
];

/// The names of the steps, in order, as the window offers them to start from.
pub fn names() -> Vec<String> {
    STEPS.iter().map(|step| step.name.to_string()).collect()
}

/// The index of the step `--from` names in `words`, the first where none is named; a name no step
/// has, or a flag with no name after it, is refused. A name shared by two steps names the first.
pub fn from(words: &[String]) -> Result<usize, String> {
    let Some(at) = words.iter().position(|word| word == "--from") else { return Ok(0) };
    let name = words.get(at + 1).ok_or("--from takes the name of a step")?;
    STEPS.iter().position(|step| step.name == name).ok_or_else(|| format!("no step of the course is {name}; the steps are {}", names().join(", ")))
}

/// The course run from the step `--from` names: each step's loop with its words, the code of the
/// first that does not end with 0 the course's, else 0. Every step is said on the sink before it runs.
pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    let first = from(words)?;
    let count = STEPS.len();
    (context.sink)(&format!("course: {count} steps: {}", names().join(", ")));
    for (index, step) in STEPS.iter().enumerate().skip(first) {
        let number = index + 1;
        (context.sink)(&format!("course: step {number} of {count}: {}", step.name));
        let given: Vec<String> = step.words.iter().map(|word| word.to_string()).collect();
        let code = (step.loop_)(context, &given)?;
        if code != 0 {
            (context.sink)(&format!("course: stopped at step {number}, {}, exit {code}", step.name));
            return Ok(code);
        }
    }
    (context.sink)(&format!("course: ended, {count} steps"));
    Ok(0)
}

#[cfg(test)]
mod steps {
    use super::*;

    #[test]
    fn the_steps_run_the_protocol_in_order_and_the_names_are_offered_to_start_from() {
        assert_eq!(names(), ["vendor tables", "discover", "turns", "registers", "write krs", "stick engine", "pairs stick", "pairs past", "costs", "write krs"]);
        assert_eq!(from(&[]).unwrap(), 0);
        assert_eq!(from(&["--from".into(), "costs".into()]).unwrap(), 8);
        assert_eq!(from(&["--from".into(), "write krs".into()]).unwrap(), 4, "a name two steps share names the first");
        assert!(from(&["--from".into()]).is_err());
        assert!(from(&["--from".into(), "fly".into()]).unwrap_err().contains("no step of the course is fly"));
    }
}
