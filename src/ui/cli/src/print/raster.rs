// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The pages laid out as pixels, for a printer that takes no PDF: each drawn on white at a printer's
//! resolution with the system's monospace font, its own bold and italic where it has them and made
//! from the regular face where it does not, and written as PWG raster, which every IPP Everywhere
//! printer takes, or Apple's raster, which AirPrint printers take. A page laid on its side is turned
//! onto the paper as it feeds. Each line of pixels is written with the count of the lines like it
//! that follow, and each run of one pixel or of different ones with its length.

use std::collections::HashMap;

use super::font::{monospace, rasterize, Font, Mask};
use super::page::{lay_out, Layout, Listing, ADVANCE, LEADING, MARGIN, NUMBER_COLOR, SIZE};

/// A page's pixels: its width and height, one byte a pixel for gray or three for color.
pub struct Pixels {
    pub width: usize,
    pub height: usize,
    pub channels: usize,
    pub data: Vec<u8>,
}

impl Pixels {
    fn white(width: usize, height: usize, channels: usize) -> Pixels {
        Pixels { width, height, channels, data: vec![255; width * height * channels] }
    }

    /// The page turned a quarter to the left: a page on its side made upright for the paper.
    pub fn turned(&self) -> Pixels {
        let mut out = Pixels::white(self.height, self.width, self.channels);
        for y in 0..self.height {
            for x in 0..self.width {
                let from = (y * self.width + x) * self.channels;
                let (nx, ny) = (y, self.width - 1 - x);
                let to = (ny * out.width + nx) * self.channels;
                out.data[to..to + self.channels].copy_from_slice(&self.data[from..from + self.channels]);
            }
        }
        out
    }
}

/// The faces drawn with: regular, bold, italic and bold italic, each where the system has it.
pub struct Faces {
    faces: [Option<Font>; 4],
}

impl Faces {
    /// The system's monospace faces, or why there are none.
    pub fn system() -> Result<Faces, String> {
        let paths = monospace();
        let faces = paths.map(|path| path.and_then(|path| Font::open(&path)));
        if faces[0].is_none() {
            return Err("the system has no monospace font orior reads: Consolas, Cascadia Mono, DejaVu Sans Mono, Liberation Mono, Menlo or Courier New".into());
        }
        Ok(Faces { faces })
    }

    /// The face for a weight and slant, and whether it must be made bolder or slanted.
    fn face(&self, bold: bool, italic: bool) -> (&Font, bool, bool) {
        let want = (bold as usize) | ((italic as usize) << 1);
        if let Some(face) = &self.faces[want] {
            return (face, false, false);
        }
        let regular = self.faces[0].as_ref().expect("a regular face");
        (regular, bold, italic)
    }
}

/// Draws the pages `listing` lays out at `dpi`, those `layout` prints, in gray or color.
pub fn render(listing: &Listing, layout: &Layout, faces: &Faces, dpi: f32) -> Vec<Pixels> {
    let laid = lay_out(listing, layout);
    let scale = dpi / 72.0;
    let (width, height) = layout.size();
    let (pixels_wide, pixels_high) = ((width * scale).round() as usize, (height * scale).round() as usize);
    let channels = if layout.gray { 1 } else { 3 };
    let em = SIZE * scale;
    let cell = ADVANCE * scale;
    let mut cache: HashMap<(usize, u16, bool, bool), Mask> = HashMap::new();
    let mut out = Vec::new();
    for (at, rows) in laid.pages.iter().enumerate() {
        if !layout.prints(at + 1) {
            continue;
        }
        let mut page = Pixels::white(pixels_wide, pixels_high, channels);
        for (index, row) in rows.iter().enumerate() {
            let baseline = (MARGIN + index as f32 * LEADING + LEADING * 0.75) * scale;
            let mut draw = |text: &str, column: usize, color: [u8; 3], bold: bool, italic: bool| {
                let (font, make_bold, slant) = faces.face(bold, italic);
                let key_face = (bold as usize) | ((italic as usize) << 1);
                let font_scale = em / font.units;
                for (offset, letter) in text.chars().enumerate() {
                    if letter == ' ' {
                        continue;
                    }
                    let glyph = font.glyph(letter);
                    let mask = cache.entry((key_face, glyph, make_bold, slant)).or_insert_with(|| {
                        let mut contours = Vec::new();
                        let shear = if slant { 0.2 * font_scale } else { 0.0 };
                        font.outline(glyph, [font_scale, 0.0, shear, font_scale, 0.0, 0.0], 0, &mut contours);
                        let mut mask = rasterize(&contours);
                        if make_bold {
                            embolden(&mut mask, (em / 28.0).max(1.0) as usize);
                        }
                        mask
                    });
                    let advance = font.advance(glyph) * font_scale;
                    let x = MARGIN * scale + (column + offset) as f32 * cell + (cell - advance) / 2.0;
                    blend(&mut page, mask, x.round() as i32 + mask.left, baseline.round() as i32 - mask.top, color);
                }
            };
            if let Some(number) = &row.number {
                let tint = if layout.gray { super::page::color("#888888", true) } else { NUMBER_COLOR };
                draw(number, 0, tint, false, false);
            }
            for piece in &row.pieces {
                draw(&piece.text, laid.number_columns + piece.column, piece.color, piece.bold, piece.italic);
            }
        }
        out.push(page);
    }
    out
}

/// Makes a glyph bolder by spreading its coverage `by` pixels to the right.
fn embolden(mask: &mut Mask, by: usize) {
    let width = mask.width + by;
    let mut cover = vec![0f32; width * mask.height];
    for y in 0..mask.height {
        for x in 0..mask.width {
            let value = mask.cover[y * mask.width + x];
            for shift in 0..=by {
                let at = y * width + x + shift;
                cover[at] = cover[at].max(value);
            }
        }
    }
    mask.width = width;
    mask.cover = cover;
}

/// Lays `mask` on `page` with its top left at x, y in `color`.
fn blend(page: &mut Pixels, mask: &Mask, left: i32, top: i32, color: [u8; 3]) {
    for my in 0..mask.height {
        let y = top + my as i32;
        if y < 0 || y as usize >= page.height {
            continue;
        }
        for mx in 0..mask.width {
            let cover = mask.cover[my * mask.width + mx];
            if cover <= 0.0 {
                continue;
            }
            let x = left + mx as i32;
            if x < 0 || x as usize >= page.width {
                continue;
            }
            let at = (y as usize * page.width + x as usize) * page.channels;
            if page.channels == 1 {
                let level = color[0] as f32;
                page.data[at] = (page.data[at] as f32 * (1.0 - cover) + level * cover).round() as u8;
            } else {
                for channel in 0..3 {
                    page.data[at + channel] = (page.data[at + channel] as f32 * (1.0 - cover) + color[channel] as f32 * cover).round() as u8;
                }
            }
        }
    }
}

/// The lines of a page in the compression PWG and Apple raster share.
fn compress(page: &Pixels, out: &mut Vec<u8>) {
    let line = page.width * page.channels;
    let pixel = page.channels;
    let mut y = 0;
    while y < page.height {
        let row = &page.data[y * line..(y + 1) * line];
        let mut repeat = 1;
        while y + repeat < page.height && repeat < 256 && &page.data[(y + repeat) * line..(y + repeat + 1) * line] == row {
            repeat += 1;
        }
        out.push((repeat - 1) as u8);
        let at = |x: usize| &row[x * pixel..(x + 1) * pixel];
        let mut x = 0;
        while x < page.width {
            if x + 1 < page.width && at(x) == at(x + 1) {
                let mut run = 2;
                while x + run < page.width && run < 128 && at(x + run) == at(x) {
                    run += 1;
                }
                out.push((run - 1) as u8);
                out.extend_from_slice(at(x));
                x += run;
            } else {
                let mut run = 1;
                while x + run < page.width && run < 128 && !(x + run + 1 < page.width && at(x + run) == at(x + run + 1)) {
                    run += 1;
                }
                if run == 1 {
                    out.push(0);
                } else {
                    out.push((257 - run) as u8);
                }
                out.extend_from_slice(&row[x * pixel..(x + run) * pixel]);
                x += run;
            }
        }
        y += repeat;
    }
}

/// The pages as PWG raster, on the paper `layout` names, at `dpi`.
pub fn pwg(pages: &[Pixels], layout: &Layout, dpi: u32) -> Vec<u8> {
    let mut out = b"RaS2".to_vec();
    let (width_points, height_points) = {
        let upright = Layout { landscape: false, ..layout.clone() };
        upright.size()
    };
    for page in pages {
        let mut head = vec![0u8; 1796];
        let put_text = |head: &mut Vec<u8>, at: usize, text: &str| head[at..at + text.len()].copy_from_slice(text.as_bytes());
        let put = |head: &mut Vec<u8>, at: usize, value: u32| head[at..at + 4].copy_from_slice(&value.to_be_bytes());
        put_text(&mut head, 0, "PwgRaster");
        put(&mut head, 276, dpi);
        put(&mut head, 280, dpi);
        put(&mut head, 340, layout.copies.max(1));
        put(&mut head, 352, width_points.round() as u32);
        put(&mut head, 356, height_points.round() as u32);
        put(&mut head, 372, page.width as u32);
        put(&mut head, 376, page.height as u32);
        put(&mut head, 384, 8);
        put(&mut head, 388, 8 * page.channels as u32);
        put(&mut head, 392, (page.width * page.channels) as u32);
        put(&mut head, 400, if page.channels == 1 { 18 } else { 19 });
        put(&mut head, 420, page.channels as u32);
        put(&mut head, 452, pages.len() as u32);
        put(&mut head, 456, 1);
        put(&mut head, 460, 1);
        put(&mut head, 480, 0x00FF_FFFF);
        put_text(&mut head, 1668, "perceptual");
        put_text(&mut head, 1732, layout.media());
        out.extend(head);
        compress(page, &mut out);
    }
    out
}

/// The pages as Apple's raster, at `dpi`.
pub fn urf(pages: &[Pixels], dpi: u32) -> Vec<u8> {
    let mut out = b"UNIRAST\0".to_vec();
    out.extend_from_slice(&(pages.len() as u32).to_be_bytes());
    for page in pages {
        out.push((8 * page.channels) as u8);
        out.push(if page.channels == 1 { 0 } else { 1 });
        out.push(1);
        out.push(4);
        out.extend_from_slice(&[0; 8]);
        out.extend_from_slice(&(page.width as u32).to_be_bytes());
        out.extend_from_slice(&(page.height as u32).to_be_bytes());
        out.extend_from_slice(&dpi.to_be_bytes());
        out.extend_from_slice(&[0; 8]);
        compress(page, &mut out);
    }
    out
}

#[cfg(test)]
mod drawing {
    use super::super::page::{Line, Run};
    use super::*;

    /// The pixels back from the compression: each line's repeat, then its runs.
    fn decompress(data: &[u8], width: usize, height: usize, channels: usize) -> Vec<u8> {
        let mut out = Vec::new();
        let mut at = 0;
        while out.len() < width * height * channels {
            let repeat = data[at] as usize + 1;
            at += 1;
            let mut line = Vec::new();
            while line.len() < width * channels {
                let control = data[at];
                at += 1;
                if control < 128 {
                    for _ in 0..control as usize + 1 {
                        line.extend_from_slice(&data[at..at + channels]);
                    }
                    at += channels;
                } else {
                    let count = 257 - control as usize;
                    line.extend_from_slice(&data[at..at + count * channels]);
                    at += count * channels;
                }
            }
            for _ in 0..repeat {
                out.extend_from_slice(&line);
            }
        }
        out
    }

    #[test]
    fn the_compression_gives_back_each_pixel() {
        let mut page = Pixels::white(300, 7, 3);
        for x in 0..300 {
            page.data[(3 * 300 + x) * 3] = (x % 7) as u8;
        }
        page.data[5 * 900 + 10] = 0;
        let mut out = Vec::new();
        compress(&page, &mut out);
        assert!(out.len() < page.data.len());
        assert_eq!(decompress(&out, 300, 7, 3), page.data);
    }

    #[test]
    fn a_listing_draws_ink_where_its_text_stands_and_writes_both_rasters() {
        let Ok(faces) = Faces::system() else { return };
        let listing = Listing { title: "t".into(), lines: vec![Line { number: Some(1), runs: vec![Run { text: "Hello, orior".into(), color: "#000000".into(), ..Run::default() }, Run { text: " bold".into(), color: "#c4314b".into(), bold: true, italic: true }] }], tab: 4 };
        let layout = Layout { gray: false, ..Layout::default() };
        let pages = render(&listing, &layout, &faces, 150.0);
        assert_eq!(pages.len(), 1);
        let page = &pages[0];
        assert_eq!((page.width, page.height), (1240, 1754));
        let dark = page.data.chunks(3).filter(|pixel| pixel[0] < 128).count();
        assert!(dark > 300, "{dark}");
        let red = page.data.chunks(3).filter(|pixel| pixel[0] > 150 && pixel[1] < 120).count();
        assert!(red > 20, "{red}");
        // The text stands within the margins, near the top.
        let top_band = (40.0 * 150.0 / 72.0) as usize..(60.0 * 150.0 / 72.0) as usize;
        assert!(top_band.clone().any(|y| page.data[y * page.width * 3..(y + 1) * page.width * 3].iter().any(|value| *value < 100)));
        let raster = pwg(&pages, &layout, 150);
        assert_eq!(&raster[..4], b"RaS2");
        assert_eq!(&raster[4..13], b"PwgRaster");
        assert_eq!(u32::from_be_bytes(raster[4 + 372..4 + 376].try_into().unwrap()), 1240);
        assert_eq!(decompress(&raster[4 + 1796..], 1240, 1754, 3), page.data);
        let apple = urf(&pages, 150);
        assert_eq!(&apple[..8], b"UNIRAST\0");
        assert_eq!(decompress(&apple[12 + 32..], 1240, 1754, 3), page.data);
        let side = render(&listing, &Layout { landscape: true, gray: true, ..Layout::default() }, &faces, 100.0);
        assert!(side[0].width > side[0].height);
        let upright = side[0].turned();
        assert_eq!((upright.width, upright.height), (side[0].height, side[0].width));
    }
}
