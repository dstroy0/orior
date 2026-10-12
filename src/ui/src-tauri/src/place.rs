// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The window's place for each tree: where it stood, how large it was and whether it filled its
//! monitor, kept in places.json in orior's home as the window closes, and put back as the tree opens,
//! fitted to the monitors there are now. A place on no monitor there is goes onto the first, in its
//! middle, and a place larger than its monitor is cut to it.

use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

/// A place on the screen, in the system's own pixels.
#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct Place {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub maximized: bool,
}

/// A monitor's area, in the system's own pixels.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Area {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
}

/// How much of a window's top edge must stand on a monitor for its place there to hold: enough to
/// take it by its title and move it.
const HELD: i64 = 64;

/// The part of `place`'s top strip, HELD high, that stands on `area`, in pixels.
fn standing(place: &Place, area: &Area) -> i64 {
    let left = i64::from(place.x.max(area.x));
    let right = (i64::from(place.x) + i64::from(place.width)).min(i64::from(area.x) + i64::from(area.width));
    let top = i64::from(place.y.max(area.y));
    let bottom = (i64::from(place.y) + HELD).min(i64::from(area.y) + i64::from(area.height));
    (right - left).max(0) * (bottom - top).max(0)
}

/// `place` fitted to the monitors `areas`, the first of them the system's main one: kept on the
/// monitor its top edge stands on most, cut to that monitor's size and moved in to stand wholly on
/// it; or, standing on none, put in the middle of the first. None where there is no monitor.
pub fn fit(place: Place, areas: &[Area]) -> Option<Place> {
    let (area, stands) = areas.iter().map(|area| (*area, standing(&place, area))).max_by_key(|(_, stands)| *stands)?;
    let area = if stands >= HELD * HELD.min(i64::from(place.width)) { area } else { *areas.first()? };
    let width = place.width.min(area.width);
    let height = place.height.min(area.height);
    let inside = |at: i32, size: u32, start: i32, span: u32| at.clamp(start, start + (span - size) as i32);
    let (x, y) = if stands >= HELD * HELD.min(i64::from(place.width)) {
        (inside(place.x, width, area.x, area.width), inside(place.y, height, area.y, area.height))
    } else {
        (area.x + ((area.width - width) / 2) as i32, area.y + ((area.height - height) / 2) as i32)
    };
    Some(Place { x, y, width, height, maximized: place.maximized })
}

/// Where the places are kept: places.json in orior's home.
fn file() -> Option<PathBuf> {
    orior_cli::home::folder().map(|home| home.join("places.json"))
}

/// The key a tree's place is kept under: its folder as written.
fn key(tree: &Path) -> String {
    tree.to_string_lossy().replace('\\', "/")
}

/// The place kept for `tree`, where there is one.
pub fn kept(tree: &Path) -> Option<Place> {
    let text = std::fs::read_to_string(file()?).ok()?;
    let all: std::collections::BTreeMap<String, Place> = serde_json::from_str(&text).ok()?;
    all.get(&key(tree)).copied()
}

/// Keeps `place` for `tree`, beside the places of the other trees.
pub fn keep(tree: &Path, place: Place) -> Result<(), String> {
    let path = file().ok_or("orior has no home folder")?;
    let mut all: std::collections::BTreeMap<String, Place> = std::fs::read_to_string(&path).ok().and_then(|text| serde_json::from_str(&text).ok()).unwrap_or_default();
    all.insert(key(tree), place);
    if let Some(folder) = path.parent() {
        std::fs::create_dir_all(folder).map_err(|error| error.to_string())?;
    }
    std::fs::write(&path, serde_json::to_string_pretty(&all).map_err(|error| error.to_string())?).map_err(|error| format!("{}: {error}", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    const MAIN: Area = Area { x: 0, y: 0, width: 1920, height: 1080 };
    const BESIDE: Area = Area { x: 1920, y: 0, width: 2560, height: 1440 };

    #[test]
    fn a_place_on_a_monitor_there_is_holds() {
        let place = Place { x: 2000, y: 100, width: 1440, height: 920, maximized: false };
        assert_eq!(fit(place, &[MAIN, BESIDE]), Some(place));
    }

    #[test]
    fn a_place_on_a_monitor_gone_goes_to_the_middle_of_the_main_one() {
        let place = Place { x: 2000, y: 100, width: 1440, height: 920, maximized: true };
        assert_eq!(fit(place, &[MAIN]), Some(Place { x: 240, y: 80, width: 1440, height: 920, maximized: true }));
    }

    #[test]
    fn a_place_larger_than_its_monitor_is_cut_to_it_and_moved_in() {
        let place = Place { x: -40, y: 30, width: 2400, height: 1300, maximized: false };
        assert_eq!(fit(place, &[MAIN, BESIDE]), Some(Place { x: 0, y: 0, width: 1920, height: 1080, maximized: false }));
    }

    #[test]
    fn a_place_hanging_off_its_monitor_is_moved_wholly_onto_it() {
        let place = Place { x: 1200, y: 600, width: 1440, height: 920, maximized: false };
        assert_eq!(fit(place, &[MAIN]), Some(Place { x: 480, y: 160, width: 1440, height: 920, maximized: false }));
    }

    #[test]
    fn with_no_monitor_there_is_no_place() {
        assert_eq!(fit(Place { x: 0, y: 0, width: 800, height: 600, maximized: false }, &[]), None);
    }
}
