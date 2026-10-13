// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The lists a quick open of the window's shows that are not the tree's files or symbols, as the
//! tree's tests and the machine's processes: each held here by a number as it opens, its texts split
//! into characters and in lower case beside them, and matched here as each letter is typed, the best
//! first, as files.rs matches the tree's files and scores them. A list of tens of thousands answers a
//! letter in a few milliseconds, spread over the cores, and the page's frame never waits on it.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex, OnceLock};

use serde::Serialize;

/// A list's texts, each as shown and in lower case, in characters.
struct Held {
    shown: Vec<Vec<char>>,
    lower: Vec<Vec<char>>,
}

/// A list's rows longer than this are matched over the cores, the rest on one.
const SPREAD_FROM: usize = 4096;

fn held() -> &'static Mutex<HashMap<u64, Arc<Held>>> {
    static HELD: OnceLock<Mutex<HashMap<u64, Arc<Held>>>> = OnceLock::new();
    HELD.get_or_init(Mutex::default)
}

static NEXT: AtomicU64 = AtomicU64::new(1);

/// A row of a list that answers a query: its place in the list, its score and the characters of its
/// text that matched.
#[derive(Serialize, Debug, PartialEq)]
pub struct Pick {
    pub at: usize,
    pub score: f64,
    pub hits: Vec<usize>,
}

/// Holds the texts of a list's rows, and gives the number it is held by.
pub fn hold(texts: Vec<String>) -> u64 {
    let shown: Vec<Vec<char>> = texts.iter().map(|text| text.chars().collect()).collect();
    let lower = shown.iter().map(|text| crate::files::lowered(text)).collect();
    let id = NEXT.fetch_add(1, Ordering::Relaxed);
    if let Ok(mut all) = held().lock() {
        all.insert(id, Arc::new(Held { shown, lower }));
    }
    id
}

/// Lets go of the list held by `id`.
pub fn forget(id: u64) {
    if let Ok(mut all) = held().lock() {
        all.remove(&id);
    }
}

/// The rows of the list held by `id` that answer `query`, at most `most` of them, the best first and
/// rows that score alike in the list's order; with no query, its first `most` rows.
pub fn find(id: u64, query: &str, most: usize) -> Result<Vec<Pick>, String> {
    let list = held().lock().map_err(|_| "held".to_string())?.get(&id).cloned().ok_or_else(|| format!("no list is held by {id}"))?;
    let wanted = crate::files::wanted(query);
    if wanted.is_empty() {
        return Ok((0..list.shown.len().min(most)).map(|at| Pick { at, score: 0.0, hits: Vec::new() }).collect());
    }
    let matched = |from: usize, to: usize| -> Vec<Pick> {
        (from..to)
            .filter_map(|at| crate::files::fuzzy_held(&wanted, &list.shown[at], &list.lower[at]).map(|(score, hits)| Pick { at, score, hits }))
            .collect()
    };
    let count = list.shown.len();
    let mut found: Vec<Pick> = if count < SPREAD_FROM {
        matched(0, count)
    } else {
        let cores = crate::cores::for_work().max(1);
        let share = count.div_ceil(cores);
        std::thread::scope(|scope| {
            let parts: Vec<_> = (0..cores).map(|part| scope.spawn(move || matched((part * share).min(count), ((part + 1) * share).min(count)))).collect();
            parts.into_iter().flat_map(|part| part.join().unwrap_or_default()).collect()
        })
    };
    found.sort_by(|a, b| b.score.total_cmp(&a.score).then(a.at.cmp(&b.at)));
    found.truncate(most);
    Ok(found)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_list_held_answers_its_best_rows_first_and_in_its_order_where_none_is_asked() {
        let id = hold(vec!["alpha beta".into(), "gamma".into(), "alphabet".into(), "beta".into()]);
        let none = find(id, "", 3).unwrap();
        assert_eq!(none.iter().map(|one| one.at).collect::<Vec<_>>(), vec![0, 1, 2]);
        let found = find(id, "beta", 10).unwrap();
        assert_eq!(found[0].at, 3, "the row that is the query itself first: {found:?}");
        assert!(found.iter().all(|one| one.at != 1), "a row that does not answer is left out");
        for one in &found {
            assert_eq!(crate::files::fuzzy("beta", ["alpha beta", "gamma", "alphabet", "beta"][one.at], 0).map(|(score, _)| score), Some(one.score), "scored as Go to File scores");
        }
        forget(id);
        assert!(find(id, "beta", 10).is_err(), "a list let go answers no more");
    }

    #[test]
    fn a_list_of_many_rows_matches_over_the_cores_as_it_would_on_one() {
        let texts: Vec<String> = (0..20_000).map(|at| format!("test_case_{at}_{} tests/test_part_{}.py", (at * 7919) % 20_000, at % 300)).collect();
        let id = hold(texts.clone());
        let found = find(id, "case12", 200).unwrap();
        let mut alone: Vec<(usize, f64)> = texts.iter().enumerate().filter_map(|(at, text)| crate::files::fuzzy("case12", text, 0).map(|(score, _)| (at, score))).collect();
        alone.sort_by(|a, b| b.1.total_cmp(&a.1).then(a.0.cmp(&b.0)));
        alone.truncate(200);
        assert_eq!(found.iter().map(|one| (one.at, one.score)).collect::<Vec<_>>(), alone);
        forget(id);
    }
}
