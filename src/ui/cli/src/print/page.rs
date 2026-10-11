// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A listing as it prints: its lines, each a run of colored text, laid on pages of a paper size,
//! upright or on its side, in columns of a monospace face 9.5 points tall, each line with its number
//! and a long line going on under itself. The PDF and the raster both draw the pages laid out here,
//! the one as text and the other as pixels.

use serde::{Deserialize, Serialize};

/// A run of a line's text in one color and face.
#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct Run {
    pub text: String,
    /// The run's color as #rrggbb.
    #[serde(default)]
    pub color: String,
    #[serde(default)]
    pub bold: bool,
    #[serde(default)]
    pub italic: bool,
}

/// A line of the listing: its number, where it shows one, and its runs.
#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct Line {
    #[serde(default)]
    pub number: Option<usize>,
    pub runs: Vec<Run>,
}

/// What prints: its title and its lines, a tab as wide as `tab` spaces.
#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct Listing {
    pub title: String,
    pub lines: Vec<Line>,
    #[serde(default = "four")]
    pub tab: usize,
}

fn four() -> usize {
    4
}

/// How a page prints.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct Layout {
    /// a4 or letter.
    #[serde(default = "a4")]
    pub paper: String,
    #[serde(default)]
    pub landscape: bool,
    #[serde(default)]
    pub gray: bool,
    /// The pages printed, as 1-3,5; every page where none are named.
    #[serde(default)]
    pub pages: Option<String>,
    #[serde(default = "one")]
    pub copies: u32,
}

fn a4() -> String {
    "a4".into()
}

fn one() -> u32 {
    1
}

impl Default for Layout {
    fn default() -> Layout {
        Layout { paper: a4(), landscape: false, gray: false, pages: None, copies: 1 }
    }
}

/// The text's size, the height of a row, the width of a column and the margin, in points.
pub const SIZE: f32 = 9.5;
pub const LEADING: f32 = SIZE * 1.45;
pub const ADVANCE: f32 = SIZE * 0.6;
pub const MARGIN: f32 = 14.0 * 72.0 / 25.4;

/// The color of the line numbers.
pub const NUMBER_COLOR: [u8; 3] = [0x88, 0x88, 0x88];

impl Layout {
    /// The paper's width and height in points, as it lies.
    pub fn size(&self) -> (f32, f32) {
        let (width, height) = if self.paper.eq_ignore_ascii_case("letter") { (612.0, 792.0) } else { (595.276, 841.89) };
        if self.landscape {
            (height, width)
        } else {
            (width, height)
        }
    }

    /// The paper's name as IPP and PWG write it.
    pub fn media(&self) -> &'static str {
        if self.paper.eq_ignore_ascii_case("letter") {
            "na_letter_8.5x11in"
        } else {
            "iso_a4_210x297mm"
        }
    }

    /// Whether the page numbered `page`, counted from 1, is one to print.
    pub fn prints(&self, page: usize) -> bool {
        let Some(pages) = self.pages.as_deref().map(str::trim).filter(|pages| !pages.is_empty()) else { return true };
        pages.split(',').any(|part| {
            let part = part.trim();
            match part.split_once('-') {
                Some((from, to)) => {
                    let from = from.trim().parse::<usize>().unwrap_or(1);
                    let to = to.trim().parse::<usize>().unwrap_or(usize::MAX);
                    (from..=to).contains(&page)
                }
                None => part.parse::<usize>() == Ok(page),
            }
        })
    }

    /// The page ranges as IPP takes them: each a first and last page.
    pub fn ranges(&self) -> Vec<(i32, i32)> {
        let Some(pages) = self.pages.as_deref().map(str::trim).filter(|pages| !pages.is_empty()) else { return Vec::new() };
        pages
            .split(',')
            .filter_map(|part| {
                let part = part.trim();
                match part.split_once('-') {
                    Some((from, to)) => Some((from.trim().parse().ok()?, to.trim().parse().unwrap_or(i32::MAX))),
                    None => part.parse().ok().map(|page| (page, page)),
                }
            })
            .collect()
    }
}

/// A piece of a row: the column it starts at, its text, its color and face.
#[derive(Clone, Debug, PartialEq)]
pub struct Piece {
    pub column: usize,
    pub text: String,
    pub color: [u8; 3],
    pub bold: bool,
    pub italic: bool,
}

/// A row of a page: its number, on the first row of a line, and its pieces.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Row {
    pub number: Option<String>,
    pub pieces: Vec<Piece>,
}

/// The pages laid out, and the columns the numbers take.
pub struct Laid {
    pub pages: Vec<Vec<Row>>,
    pub number_columns: usize,
}

/// A color #rrggbb as its three bytes, made gray by its lightness where `gray` is set.
pub fn color(text: &str, gray: bool) -> [u8; 3] {
    let hex = text.trim().trim_start_matches('#');
    let value = |at: usize| u8::from_str_radix(hex.get(at..at + 2).unwrap_or("22"), 16).unwrap_or(0x22);
    let rgb = if hex.len() >= 6 { [value(0), value(2), value(4)] } else { [0x22, 0x22, 0x22] };
    if gray {
        let level = (0.2126 * rgb[0] as f32 + 0.7152 * rgb[1] as f32 + 0.0722 * rgb[2] as f32).round() as u8;
        [level; 3]
    } else {
        rgb
    }
}

/// Lays `listing` out in pages as `layout` says.
pub fn lay_out(listing: &Listing, layout: &Layout) -> Laid {
    let (width, height) = layout.size();
    let widest = listing.lines.iter().filter_map(|line| line.number).max().unwrap_or(0);
    let number_columns = if widest == 0 { 0 } else { widest.to_string().len() + 2 };
    let columns = (((width - 2.0 * MARGIN) / ADVANCE).floor() as usize).saturating_sub(number_columns).max(8);
    let rows_per_page = (((height - 2.0 * MARGIN) / LEADING).floor() as usize).max(1);
    let mut rows: Vec<Row> = Vec::new();
    for line in &listing.lines {
        let mut row = Row { number: line.number.map(|number| format!("{number:>width$}", width = number_columns.saturating_sub(2))), pieces: Vec::new() };
        let mut column = 0usize;
        for run in &line.runs {
            let tint = color(&run.color, layout.gray);
            let mut piece = String::new();
            let mut piece_start = column;
            for letter in run.text.chars() {
                let letters: Vec<char> = if letter == '\t' { vec![' '; listing.tab - column % listing.tab.max(1)] } else if letter.is_control() { vec![] } else { vec![letter] };
                for one in letters {
                    if column == columns {
                        if !piece.is_empty() {
                            row.pieces.push(Piece { column: piece_start, text: std::mem::take(&mut piece), color: tint, bold: run.bold, italic: run.italic });
                        }
                        rows.push(std::mem::take(&mut row));
                        column = 0;
                        piece_start = 0;
                    }
                    piece.push(one);
                    column += 1;
                }
            }
            if !piece.is_empty() {
                row.pieces.push(Piece { column: piece_start, text: piece, color: tint, bold: run.bold, italic: run.italic });
            }
        }
        rows.push(row);
    }
    let pages = rows.chunks(rows_per_page).map(<[Row]>::to_vec).collect();
    Laid { pages, number_columns }
}

#[cfg(test)]
mod laying {
    use super::*;

    fn listing(lines: &[&str]) -> Listing {
        Listing { title: "t".into(), lines: lines.iter().enumerate().map(|(at, text)| Line { number: Some(at + 1), runs: vec![Run { text: text.to_string(), color: "#112233".into(), ..Run::default() }] }).collect(), tab: 4 }
    }

    #[test]
    fn a_long_line_goes_on_under_itself_and_pages_break_by_rows() {
        let long = "x".repeat(200);
        let laid = lay_out(&listing(&["short", &long, "\tafter"]), &Layout::default());
        let rows = &laid.pages[0];
        assert_eq!(rows[0].number.as_deref(), Some("1"));
        assert_eq!(rows[1].number.as_deref(), Some("2"));
        assert_eq!(rows[2].number, None, "the long line's second row has no number");
        let first_width: usize = rows[1].pieces.iter().map(|piece| piece.text.chars().count()).sum();
        assert!(first_width < 200 && first_width > 60, "{first_width}");
        assert_eq!(rows.last().unwrap().pieces[0].text, "    after");
        let many: Vec<String> = (0..200).map(|n| n.to_string()).collect();
        let pages = lay_out(&listing(&many.iter().map(String::as_str).collect::<Vec<_>>()), &Layout::default()).pages;
        assert!(pages.len() >= 3, "{}", pages.len());
        let wide = lay_out(&listing(&many.iter().map(String::as_str).collect::<Vec<_>>()), &Layout { landscape: true, ..Layout::default() }).pages;
        assert!(wide.len() > pages.len());
    }

    #[test]
    fn pages_ranges_and_gray_read_as_written() {
        let layout = Layout { pages: Some("1-3, 5".into()), ..Layout::default() };
        assert!(layout.prints(2) && layout.prints(5) && !layout.prints(4));
        assert_eq!(layout.ranges(), vec![(1, 3), (5, 5)]);
        assert_eq!(color("#ff0000", true), [54, 54, 54]);
        assert_eq!(color("#0a0b0c", false), [10, 11, 12]);
    }
}
