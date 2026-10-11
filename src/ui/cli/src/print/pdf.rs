// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The pages laid out as a PDF of orior's own writing: PDF 1.4, its text in the Courier faces every
//! PDF reader holds, regular, bold, oblique and bold oblique, encoded as WinAnsi, a letter that has
//! no place there written as a question mark. Only the pages the layout names are written.

use super::page::{lay_out, Laid, Layout, Listing, ADVANCE, LEADING, MARGIN, NUMBER_COLOR, SIZE};

/// A letter as WinAnsi writes it, where it has a place there.
fn win_ansi(letter: char) -> Option<u8> {
    let code = letter as u32;
    match code {
        0x20..=0x7E | 0xA0..=0xFF => Some(code as u8),
        _ => Some(match letter {
            '€' => 0x80,
            '‚' => 0x82,
            'ƒ' => 0x83,
            '„' => 0x84,
            '…' => 0x85,
            '†' => 0x86,
            '‡' => 0x87,
            'ˆ' => 0x88,
            '‰' => 0x89,
            'Š' => 0x8A,
            '‹' => 0x8B,
            'Œ' => 0x8C,
            'Ž' => 0x8E,
            '‘' => 0x91,
            '’' => 0x92,
            '“' => 0x93,
            '”' => 0x94,
            '•' => 0x95,
            '–' => 0x96,
            '—' => 0x97,
            '˜' => 0x98,
            '™' => 0x99,
            'š' => 0x9A,
            '›' => 0x9B,
            'œ' => 0x9C,
            'ž' => 0x9E,
            'Ÿ' => 0x9F,
            _ => return None,
        }),
    }
}

/// `text` as a PDF string of WinAnsi bytes, its brackets and backslashes escaped.
fn string(text: &str) -> Vec<u8> {
    let mut out = vec![b'('];
    for letter in text.chars() {
        let byte = win_ansi(letter).unwrap_or(b'?');
        if matches!(byte, b'(' | b')' | b'\\') {
            out.push(b'\\');
        }
        out.push(byte);
    }
    out.push(b')');
    out
}

/// `text` as a PDF text string in UTF-16, for the document's title.
fn text_string(text: &str) -> String {
    let mut out = String::from("<FEFF");
    for unit in text.encode_utf16() {
        out.push_str(&format!("{unit:04X}"));
    }
    out.push('>');
    out
}

fn shade([r, g, b]: [u8; 3]) -> String {
    format!("{:.3} {:.3} {:.3} rg", r as f32 / 255.0, g as f32 / 255.0, b as f32 / 255.0)
}

/// The content of one page: each row's number and pieces at their columns.
fn content(laid: &Laid, rows: &[super::page::Row], height: f32, gray: bool) -> Vec<u8> {
    let mut out = Vec::new();
    out.extend_from_slice(b"BT\n");
    let number_tint = if gray { super::page::color("#888888", true) } else { NUMBER_COLOR };
    for (index, row) in rows.iter().enumerate() {
        let baseline = height - MARGIN - index as f32 * LEADING - LEADING * 0.75;
        if let Some(number) = &row.number {
            out.extend_from_slice(format!("/F1 {SIZE} Tf {} 1 0 0 1 {MARGIN:.2} {baseline:.2} Tm ", shade(number_tint)).as_bytes());
            out.extend(string(number));
            out.extend_from_slice(b" Tj\n");
        }
        for piece in &row.pieces {
            let face = match (piece.bold, piece.italic) {
                (false, false) => "F1",
                (true, false) => "F2",
                (false, true) => "F3",
                (true, true) => "F4",
            };
            let x = MARGIN + (laid.number_columns + piece.column) as f32 * ADVANCE;
            out.extend_from_slice(format!("/{face} {SIZE} Tf {} 1 0 0 1 {x:.2} {baseline:.2} Tm ", shade(piece.color)).as_bytes());
            out.extend(string(&piece.text));
            out.extend_from_slice(b" Tj\n");
        }
    }
    out.extend_from_slice(b"ET\n");
    out
}

/// `listing` written as a PDF, as `layout` says, and the count of pages written.
pub fn write(listing: &Listing, layout: &Layout) -> (Vec<u8>, usize) {
    let laid = lay_out(listing, layout);
    let (width, height) = layout.size();
    let pages: Vec<&Vec<super::page::Row>> = laid.pages.iter().enumerate().filter(|(at, _)| layout.prints(at + 1)).map(|(_, rows)| rows).collect();
    let mut objects: Vec<Vec<u8>> = Vec::new();
    // 1 the catalog, 2 the pages, 3 to 6 the faces, 7 the document's information; then each page and
    // its content, two objects a page.
    let first_page = 8;
    let kids: Vec<String> = (0..pages.len()).map(|at| format!("{} 0 R", first_page + at * 2)).collect();
    objects.push(b"<< /Type /Catalog /Pages 2 0 R >>".to_vec());
    objects.push(format!("<< /Type /Pages /Kids [{}] /Count {} >>", kids.join(" "), pages.len()).into_bytes());
    for face in ["Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique"] {
        objects.push(format!("<< /Type /Font /Subtype /Type1 /BaseFont /{face} /Encoding /WinAnsiEncoding >>").into_bytes());
    }
    objects.push(format!("<< /Title {} /Producer (orior) >>", text_string(&listing.title)).into_bytes());
    for (at, rows) in pages.iter().enumerate() {
        let contents = first_page + at * 2 + 1;
        objects.push(format!("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {width:.2} {height:.2}] /Resources << /Font << /F1 3 0 R /F2 4 0 R /F3 5 0 R /F4 6 0 R >> >> /Contents {contents} 0 R >>").into_bytes());
        let stream = content(&laid, rows, height, layout.gray);
        let mut object = format!("<< /Length {} >>\nstream\n", stream.len()).into_bytes();
        object.extend(stream);
        object.extend_from_slice(b"endstream");
        objects.push(object);
    }
    let mut out = b"%PDF-1.4\n%\xE2\xE3\xCF\xD3\n".to_vec();
    let mut offsets = Vec::with_capacity(objects.len());
    for (index, object) in objects.iter().enumerate() {
        offsets.push(out.len());
        out.extend_from_slice(format!("{} 0 obj\n", index + 1).as_bytes());
        out.extend_from_slice(object);
        out.extend_from_slice(b"\nendobj\n");
    }
    let xref = out.len();
    out.extend_from_slice(format!("xref\n0 {}\n0000000000 65535 f \n", objects.len() + 1).as_bytes());
    for offset in offsets {
        out.extend_from_slice(format!("{offset:010} 00000 n \n").as_bytes());
    }
    out.extend_from_slice(format!("trailer\n<< /Size {} /Root 1 0 R /Info 7 0 R >>\nstartxref\n{xref}\n%%EOF\n", objects.len() + 1).as_bytes());
    (out, pages.len())
}

#[cfg(test)]
mod writing {
    use super::super::page::{Line, Run};
    use super::*;

    #[test]
    fn a_listing_is_a_pdf_whose_cross_reference_finds_each_object() {
        let listing = Listing {
            title: "main.py — orior".into(),
            lines: (1..=120).map(|number| Line { number: Some(number), runs: vec![Run { text: format!("print({number}) # (é) \\ ✓"), color: "#7a5ee0".into(), bold: number % 2 == 0, italic: number % 3 == 0 }] }).collect(),
            tab: 4,
        };
        let (pdf, pages) = write(&listing, &Layout::default());
        assert!(pages >= 2);
        let text = String::from_utf8_lossy(&pdf);
        assert!(text.starts_with("%PDF-1.4"));
        assert!(text.ends_with("%%EOF\n"));
        assert!(text.contains("(print\\(12\\) # \\(\u{e9}\\) \\\\ ?) Tj") || text.contains("print\\(12\\)"));
        assert!(text.contains(&format!("/Count {pages}")));
        // Each offset the cross-reference gives starts its object.
        let xref_at: usize = text.rsplit("startxref\n").next().unwrap().lines().next().unwrap().parse().unwrap();
        let table = &pdf[xref_at..];
        let entries: Vec<usize> = String::from_utf8_lossy(table).lines().skip(3).take_while(|line| line.ends_with(" n ")).map(|line| line[..10].parse().unwrap()).collect();
        for (index, offset) in entries.iter().enumerate() {
            assert!(pdf[*offset..].starts_with(format!("{} 0 obj", index + 1).as_bytes()), "object {}", index + 1);
        }
        let some = write(&listing, &Layout { pages: Some("2".into()), ..Layout::default() });
        assert_eq!(some.1, 1);
    }
}
