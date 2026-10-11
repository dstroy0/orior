// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A TrueType font read and drawn by orior itself: its tables read from a .ttf or the first font of
//! a .ttc, a character found by its cmap, a glyph's outline read from glyf, simple or made of other
//! glyphs, its quadratic curves cut into short lines, and the lines drawn as coverage, a pixel's
//! share of the glyph, by the signed area each line leaves in the cells it crosses.

use std::collections::HashMap;
use std::path::{Path, PathBuf};

/// A font's tables, as read.
pub struct Font {
    data: Vec<u8>,
    tables: HashMap<[u8; 4], (usize, usize)>,
    pub units: f32,
    pub ascent: f32,
    pub descent: f32,
    long_loca: bool,
    glyphs: u16,
    metrics: u16,
    cmap: Vec<(u32, u32, u32)>,
}

fn u16_at(data: &[u8], at: usize) -> u16 {
    data.get(at..at + 2).map(|bytes| u16::from_be_bytes([bytes[0], bytes[1]])).unwrap_or(0)
}

fn i16_at(data: &[u8], at: usize) -> i16 {
    u16_at(data, at) as i16
}

fn u32_at(data: &[u8], at: usize) -> u32 {
    data.get(at..at + 4).map(|bytes| u32::from_be_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])).unwrap_or(0)
}

impl Font {
    /// The font in `data`: a TrueType font, or the first of a collection.
    pub fn read(data: Vec<u8>) -> Option<Font> {
        let base = if data.get(0..4) == Some(b"ttcf") { u32_at(&data, 12) as usize } else { 0 };
        let count = u16_at(&data, base + 4) as usize;
        let mut tables = HashMap::new();
        for index in 0..count {
            let at = base + 12 + index * 16;
            let tag: [u8; 4] = data.get(at..at + 4)?.try_into().ok()?;
            tables.insert(tag, (u32_at(&data, at + 8) as usize, u32_at(&data, at + 12) as usize));
        }
        let (head, _) = *tables.get(b"head")?;
        let (hhea, _) = *tables.get(b"hhea")?;
        let (maxp, _) = *tables.get(b"maxp")?;
        tables.get(b"glyf")?;
        tables.get(b"loca")?;
        let mut font = Font {
            units: u16_at(&data, head + 18) as f32,
            long_loca: i16_at(&data, head + 50) == 1,
            ascent: i16_at(&data, hhea + 4) as f32,
            descent: i16_at(&data, hhea + 6) as f32,
            metrics: u16_at(&data, hhea + 34),
            glyphs: u16_at(&data, maxp + 4),
            data,

            tables,
            cmap: Vec::new(),
        };
        font.cmap = font.read_cmap()?;
        Some(font)
    }

    /// The font at `path`.
    pub fn open(path: &Path) -> Option<Font> {
        Font::read(std::fs::read(path).ok()?)
    }

    fn table(&self, tag: &[u8; 4]) -> Option<usize> {
        self.tables.get(tag).map(|(at, _)| *at)
    }

    /// The character map as ranges of characters and the glyph each range starts at: format 12
    /// where the font has it, else format 4.
    fn read_cmap(&self) -> Option<Vec<(u32, u32, u32)>> {
        let cmap = self.table(b"cmap")?;
        let data = &self.data;
        let count = u16_at(data, cmap + 2) as usize;
        let mut best: Option<(u8, usize)> = None;
        for index in 0..count {
            let at = cmap + 4 + index * 8;
            let (platform, encoding) = (u16_at(data, at), u16_at(data, at + 2));
            let offset = cmap + u32_at(data, at + 4) as usize;
            let format = u16_at(data, offset);
            let rank = match (platform, encoding, format) {
                (3, 10, 12) | (0, 4, 12) | (0, 6, 12) => 3,
                (3, 1, 4) | (0, 3, 4) | (0, _, 4) => 2,
                _ => 0,
            };
            if rank > 0 && best.is_none_or(|(held, _)| rank > held) {
                best = Some((rank, offset));
            }
        }
        let (_, offset) = best?;
        let mut ranges = Vec::new();
        if u16_at(data, offset) == 12 {
            let groups = u32_at(data, offset + 12) as usize;
            for group in 0..groups {
                let at = offset + 16 + group * 12;
                ranges.push((u32_at(data, at), u32_at(data, at + 4), u32_at(data, at + 8)));
            }
            return Some(ranges);
        }
        let segments = u16_at(data, offset + 6) as usize / 2;
        let ends = offset + 14;
        let starts = ends + segments * 2 + 2;
        let deltas = starts + segments * 2;
        let offsets = deltas + segments * 2;
        for segment in 0..segments {
            let end = u16_at(data, ends + segment * 2) as u32;
            let start = u16_at(data, starts + segment * 2) as u32;
            let delta = u16_at(data, deltas + segment * 2);
            let range_offset = u16_at(data, offsets + segment * 2) as usize;
            if start == 0xFFFF {
                continue;
            }
            if range_offset == 0 {
                for code in start..=end {
                    ranges.push((code, code, (code as u16).wrapping_add(delta) as u32));
                }
            } else {
                for code in start..=end {
                    let at = offsets + segment * 2 + range_offset + (code - start) as usize * 2;
                    let glyph = u16_at(data, at);
                    let glyph = if glyph == 0 { 0 } else { glyph.wrapping_add(delta) as u32 };
                    ranges.push((code, code, glyph));
                }
            }
        }
        Some(ranges)
    }

    /// The glyph that draws `letter`, 0 where the font has none.
    pub fn glyph(&self, letter: char) -> u16 {
        let code = letter as u32;
        let found = self.cmap.partition_point(|(_, end, _)| *end < code);
        match self.cmap.get(found) {
            Some((start, _, glyph)) if *start <= code => (glyph + (code - start)) as u16,
            _ => 0,
        }
    }

    /// How far a glyph moves the pen, in font units.
    pub fn advance(&self, glyph: u16) -> f32 {
        let Some(hmtx) = self.table(b"hmtx") else { return self.units / 2.0 };
        let index = glyph.min(self.metrics.saturating_sub(1)) as usize;
        u16_at(&self.data, hmtx + index * 4) as f32
    }

    fn glyph_range(&self, glyph: u16) -> Option<(usize, usize)> {
        if glyph >= self.glyphs {
            return None;
        }
        let loca = self.table(b"loca")?;
        let glyf = self.table(b"glyf")?;
        let (start, end) = if self.long_loca {
            (u32_at(&self.data, loca + glyph as usize * 4) as usize, u32_at(&self.data, loca + glyph as usize * 4 + 4) as usize)
        } else {
            (u16_at(&self.data, loca + glyph as usize * 2) as usize * 2, u16_at(&self.data, loca + glyph as usize * 2 + 2) as usize * 2)
        };
        (end > start).then_some((glyf + start, glyf + end))
    }

    /// The outline of a glyph as closed contours of points in font units, curves cut into lines,
    /// moved by `matrix`: [a, b, c, d, e, f] for x' = a x + c y + e, y' = b x + d y + f.
    pub fn outline(&self, glyph: u16, matrix: [f32; 6], depth: u8, out: &mut Vec<Vec<(f32, f32)>>) {
        let Some((at, _)) = self.glyph_range(glyph) else { return };
        let data = &self.data;
        let contours = i16_at(data, at);
        if contours < 0 {
            if depth > 6 {
                return;
            }
            let mut part = at + 10;
            loop {
                let flags = u16_at(data, part);
                let child = u16_at(data, part + 2);
                part += 4;
                let (dx, dy) = if flags & 1 != 0 {
                    let pair = (i16_at(data, part) as f32, i16_at(data, part + 2) as f32);
                    part += 4;
                    pair
                } else {
                    let pair = (data[part] as i8 as f32, data[part + 1] as i8 as f32);
                    part += 2;
                    pair
                };
                let f2dot14 = |at: usize| i16_at(data, at) as f32 / 16384.0;
                let (mut a, mut b, mut c, mut d) = (1.0, 0.0, 0.0, 1.0);
                if flags & 0x8 != 0 {
                    a = f2dot14(part);
                    d = a;
                    part += 2;
                } else if flags & 0x40 != 0 {
                    a = f2dot14(part);
                    d = f2dot14(part + 2);
                    part += 4;
                } else if flags & 0x80 != 0 {
                    a = f2dot14(part);
                    b = f2dot14(part + 2);
                    c = f2dot14(part + 4);
                    d = f2dot14(part + 6);
                    part += 8;
                }
                let [ma, mb, mc, md, me, mf] = matrix;
                let child_matrix = [ma * a + mc * b, mb * a + md * b, ma * c + mc * d, mb * c + md * d, ma * dx + mc * dy + me, mb * dx + md * dy + mf];
                self.outline(child, child_matrix, depth + 1, out);
                if flags & 0x20 == 0 {
                    break;
                }
            }
            return;
        }
        let contours = contours as usize;
        let mut ends = Vec::with_capacity(contours);
        for index in 0..contours {
            ends.push(u16_at(data, at + 10 + index * 2) as usize);
        }
        let points = ends.last().map(|end| end + 1).unwrap_or(0);
        let instructions = u16_at(data, at + 10 + contours * 2) as usize;
        let mut cursor = at + 12 + contours * 2 + instructions;
        let mut flags = Vec::with_capacity(points);
        while flags.len() < points {
            let flag = *data.get(cursor).unwrap_or(&0);
            cursor += 1;
            flags.push(flag);
            if flag & 8 != 0 {
                let repeats = *data.get(cursor).unwrap_or(&0);
                cursor += 1;
                for _ in 0..repeats {
                    flags.push(flag);
                }
            }
        }
        flags.truncate(points);
        let mut read = |short: u8, same: u8| -> Vec<f32> {
            let mut value = 0i32;
            let mut out = Vec::with_capacity(points);
            for flag in &flags {
                if flag & short != 0 {
                    let delta = *data.get(cursor).unwrap_or(&0) as i32;
                    cursor += 1;
                    value += if flag & same != 0 { delta } else { -delta };
                } else if flag & same == 0 {
                    value += i16_at(data, cursor) as i32;
                    cursor += 2;
                }
                out.push(value as f32);
            }
            out
        };
        let xs = read(0x02, 0x10);
        let ys = read(0x04, 0x20);
        let [ma, mb, mc, md, me, mf] = matrix;
        let place = |x: f32, y: f32| (ma * x + mc * y + me, mb * x + md * y + mf);
        let mut start = 0;
        for end in ends {
            if end < start || end >= points {
                break;
            }
            let ring: Vec<(f32, f32, bool)> = (start..=end).map(|index| (xs[index], ys[index], flags[index] & 1 != 0)).collect();
            start = end + 1;
            if ring.is_empty() {
                continue;
            }
            // The contour starts at a point on the curve: the first that is, or the middle of the first two.
            let first_on = ring.iter().position(|point| point.2);
            let begin = match first_on {
                Some(index) => (ring[index].0, ring[index].1),
                None => ((ring[0].0 + ring[1 % ring.len()].0) / 2.0, (ring[0].1 + ring[1 % ring.len()].1) / 2.0),
            };
            let shift = first_on.unwrap_or(0);
            let mut line = vec![place(begin.0, begin.1)];
            let mut control: Option<(f32, f32)> = None;
            let mut pen = begin;
            for step in 1..=ring.len() {
                let point = ring[(shift + step) % ring.len()];
                let (x, y) = (point.0, point.1);
                if point.2 {
                    match control.take() {
                        Some(ctl) => curve(pen, ctl, (x, y), &mut line, &place),
                        None => line.push(place(x, y)),
                    }
                    pen = (x, y);
                } else if let Some(ctl) = control {
                    let middle = ((ctl.0 + x) / 2.0, (ctl.1 + y) / 2.0);
                    curve(pen, ctl, middle, &mut line, &place);
                    pen = middle;
                    control = Some((x, y));
                } else {
                    control = Some((x, y));
                }
            }
            if let Some(ctl) = control {
                curve(pen, ctl, begin, &mut line, &place);
            }
            out.push(line);
        }
    }
}

/// A quadratic curve cut into lines, each point placed by `place`.
fn curve(from: (f32, f32), control: (f32, f32), to: (f32, f32), line: &mut Vec<(f32, f32)>, place: &impl Fn(f32, f32) -> (f32, f32)) {
    const STEPS: usize = 8;
    for step in 1..=STEPS {
        let t = step as f32 / STEPS as f32;
        let u = 1.0 - t;
        line.push(place(u * u * from.0 + 2.0 * u * t * control.0 + t * t * to.0, u * u * from.1 + 2.0 * u * t * control.1 + t * t * to.1));
    }
}

/// A glyph drawn as coverage: its width and height in pixels, where its top left stands from the
/// pen on the baseline, and each pixel's share of it, from 0 to 1.
pub struct Mask {
    pub width: usize,
    pub height: usize,
    pub left: i32,
    pub top: i32,
    pub cover: Vec<f32>,
}

/// Draws `contours`, in pixels with y going up from the baseline, as coverage.
pub fn rasterize(contours: &[Vec<(f32, f32)>]) -> Mask {
    let (mut min_x, mut min_y, mut max_x, mut max_y) = (f32::MAX, f32::MAX, f32::MIN, f32::MIN);
    for point in contours.iter().flatten() {
        min_x = min_x.min(point.0);
        max_x = max_x.max(point.0);
        min_y = min_y.min(point.1);
        max_y = max_y.max(point.1);
    }
    if min_x > max_x {
        return Mask { width: 0, height: 0, left: 0, top: 0, cover: Vec::new() };
    }
    let left = min_x.floor() as i32;
    let top = max_y.ceil() as i32;
    let width = (max_x.ceil() as i32 - left).max(1) as usize + 1;
    let height = (top - min_y.floor() as i32).max(1) as usize;
    let mut area = vec![0f32; width * height + 4];
    for contour in contours {
        for pair in 0..contour.len() {
            let a = contour[pair];
            let b = contour[(pair + 1) % contour.len()];
            line(&mut area, width, height, (a.0 - left as f32, top as f32 - a.1), (b.0 - left as f32, top as f32 - b.1));
        }
    }
    let mut cover = Vec::with_capacity(width * height);
    for row in 0..height {
        let mut sum = 0f32;
        for column in 0..width {
            sum += area[row * width + column];
            cover.push(sum.abs().min(1.0));
        }
    }
    Mask { width, height, left, top, cover }
}

/// Adds the signed area a line from `p0` to `p1` leaves in each cell it crosses, as the sum along a
/// row of the cells' areas gives each pixel's coverage.
fn line(area: &mut [f32], width: usize, height: usize, p0: (f32, f32), p1: (f32, f32)) {
    if (p0.1 - p1.1).abs() <= f32::EPSILON {
        return;
    }
    let (dir, p0, p1) = if p0.1 < p1.1 { (1.0, p0, p1) } else { (-1.0, p1, p0) };
    let dxdy = (p1.0 - p0.0) / (p1.1 - p0.1);
    let mut x = p0.0;
    if p0.1 < 0.0 {
        x -= p0.1 * dxdy;
    }
    let first = p0.1.max(0.0) as usize;
    let last = (p1.1.ceil() as usize).min(height);
    for y in first..last {
        let start = y * width;
        let dy = ((y + 1) as f32).min(p1.1) - (y as f32).max(p0.1);
        let next = x + dxdy * dy;
        let d = dy * dir;
        let (x0, x1) = if x < next { (x, next) } else { (next, x) };
        let x0_floor = x0.floor();
        let x0i = x0_floor as isize;
        let x1_ceil = x1.ceil();
        let x1i = x1_ceil as isize;
        let at = |column: isize| (start as isize + column).max(0) as usize;
        if x1i <= x0i + 1 {
            let middle = 0.5 * (x + next) - x0_floor;
            area[at(x0i)] += d - d * middle;
            area[at(x0i + 1)] += d * middle;
        } else {
            let s = (x1 - x0).recip();
            let x0f = x0 - x0_floor;
            let a0 = 0.5 * s * (1.0 - x0f) * (1.0 - x0f);
            let x1f = x1 - x1_ceil + 1.0;
            let am = 0.5 * s * x1f * x1f;
            area[at(x0i)] += d * a0;
            if x1i == x0i + 2 {
                area[at(x0i + 1)] += d * (1.0 - a0 - am);
            } else {
                let a1 = s * (1.5 - x0f);
                area[at(x0i + 1)] += d * (a1 - a0);
                for column in x0i + 2..x1i - 1 {
                    area[at(column)] += d * s;
                }
                let a2 = a1 + (x1i - x0i - 3) as f32 * s;
                area[at(x1i - 1)] += d * (1.0 - a2 - am);
            }
            area[at(x1i)] += d * am;
        }
        x = next;
    }
}

/// The monospace fonts orior draws with, regular, bold, italic and bold italic, the first found of
/// those the editor uses and those each system has.
pub fn monospace() -> [Option<PathBuf>; 4] {
    let mut folders: Vec<PathBuf> = Vec::new();
    if cfg!(windows) {
        folders.extend(std::env::var_os("WINDIR").map(|dir| PathBuf::from(dir).join("Fonts")));
        folders.extend(std::env::var_os("LOCALAPPDATA").map(|dir| PathBuf::from(dir).join("Microsoft").join("Windows").join("Fonts")));
    } else if cfg!(target_os = "macos") {
        folders.extend(["/System/Library/Fonts", "/Library/Fonts"].map(PathBuf::from));
        folders.extend(std::env::var_os("HOME").map(|home| PathBuf::from(home).join("Library/Fonts")));
    } else {
        folders.extend(["/usr/share/fonts", "/usr/local/share/fonts"].map(PathBuf::from));
        folders.extend(std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".local/share/fonts")));
    }
    // Each family by its files: regular, bold, italic, bold italic.
    let families: [[&str; 4]; 8] = [
        ["IBMPlexMono-Regular.ttf", "IBMPlexMono-Bold.ttf", "IBMPlexMono-Italic.ttf", "IBMPlexMono-BoldItalic.ttf"],
        ["consola.ttf", "consolab.ttf", "consolai.ttf", "consolaz.ttf"],
        ["CascadiaMono.ttf", "", "", ""],
        ["DejaVuSansMono.ttf", "DejaVuSansMono-Bold.ttf", "DejaVuSansMono-Oblique.ttf", "DejaVuSansMono-BoldOblique.ttf"],
        ["LiberationMono-Regular.ttf", "LiberationMono-Bold.ttf", "LiberationMono-Italic.ttf", "LiberationMono-BoldItalic.ttf"],
        ["Menlo.ttc", "", "", ""],
        ["cour.ttf", "courbd.ttf", "couri.ttf", "courbi.ttf"],
        ["Courier New.ttf", "Courier New Bold.ttf", "Courier New Italic.ttf", "Courier New Bold Italic.ttf"],
    ];
    let find = |name: &str| -> Option<PathBuf> {
        if name.is_empty() {
            return None;
        }
        for folder in &folders {
            let direct = folder.join(name);
            if direct.is_file() {
                return Some(direct);
            }
            // Fonts of Linux distributions stand in folders of their own under the fonts folder.
            for entry in walk(folder, 4) {
                if entry.file_name().is_some_and(|file| file.eq_ignore_ascii_case(name)) {
                    return Some(entry);
                }
            }
        }
        None
    };
    for family in families {
        if let Some(regular) = find(family[0]) {
            return [Some(regular), find(family[1]), find(family[2]), find(family[3])];
        }
    }
    [None, None, None, None]
}

fn walk(folder: &Path, depth: usize) -> Vec<PathBuf> {
    let mut out = Vec::new();
    if depth == 0 || cfg!(windows) {
        return out;
    }
    for entry in std::fs::read_dir(folder).into_iter().flatten().flatten() {
        let path = entry.path();
        if path.is_dir() {
            out.extend(walk(&path, depth - 1));
        } else {
            out.push(path);
        }
    }
    out
}

#[cfg(test)]
mod drawing {
    use super::*;

    #[test]
    fn a_square_is_covered_whole_inside_and_by_half_on_its_edge() {
        let mask = rasterize(&[vec![(0.0, 0.0), (4.0, 0.0), (4.0, 4.0), (0.0, 4.0)]]);
        assert_eq!((mask.width, mask.height), (5, 4));
        assert!((mask.cover[mask.width + 1] - 1.0).abs() < 1e-4);
        assert!(mask.cover[mask.width + 4] < 1e-4);
        let half = rasterize(&[vec![(0.5, 0.0), (2.0, 0.0), (2.0, 2.0), (0.5, 2.0)]]);
        assert!((half.cover[0] - 0.5).abs() < 1e-3, "{:?}", half.cover);
    }

    #[test]
    fn a_system_monospace_font_draws_its_letters() {
        let [Some(regular), ..] = monospace() else { return };
        let font = Font::open(&regular).expect("the font reads");
        let glyph = font.glyph('H');
        assert!(glyph > 0);
        assert_eq!(font.advance(font.glyph('H')), font.advance(font.glyph('i')), "the font is monospace");
        let scale = 32.0 / font.units;
        let mut contours = Vec::new();
        font.outline(glyph, [scale, 0.0, 0.0, scale, 0.0, 0.0], 0, &mut contours);
        assert!(contours.len() >= 1);
        let mask = rasterize(&contours);
        let inked = mask.cover.iter().filter(|cover| **cover > 0.5).count();
        assert!(inked > 20 && inked < mask.cover.len(), "{inked} of {}", mask.cover.len());
        assert_eq!(font.glyph('\u{10FFFF}'), 0);
        // An accented letter is often made of other glyphs.
        let mut accented = Vec::new();
        font.outline(font.glyph('é'), [scale, 0.0, 0.0, scale, 0.0, 0.0], 0, &mut accented);
        assert!(!accented.is_empty());
    }
}
