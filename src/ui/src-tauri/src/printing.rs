// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Printing from orior's own print sheet, with no dialog of the system's: the printers the system
//! has, and a page printed on one of them, or written as a PDF, as the sheet sets it.
//!
//! On Windows a page is laid in a hidden window of its own and printed by WebView2's Print, or
//! PrintToPdf, with the printer, the copies, the pages, the way the paper lies and color or gray the
//! sheet gives, and the hidden window goes once it has printed. Elsewhere the printers are those
//! CUPS's lpstat lists, and a page goes to the system's own printing.

use serde::{Deserialize, Serialize};

/// A printer the system has.
#[derive(Serialize)]
pub struct Printer {
    pub name: String,
    pub default: bool,
}

/// How a page prints, as the print sheet sets it.
#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Settings {
    pub printer: Option<String>,
    pub copies: Option<i32>,
    /// The pages, as `1-3,5`, or every page where none are named.
    pub pages: Option<String>,
    pub landscape: bool,
    pub gray: bool,
    /// The file to write the page to as a PDF, in place of a printer.
    pub pdf: Option<String>,
}

/// The printers the system has, the one it prints on by default marked.
#[cfg(windows)]
pub fn printers() -> Vec<Printer> {
    use windows_sys::Win32::Graphics::Printing::{EnumPrintersW, GetDefaultPrinterW, PRINTER_ENUM_CONNECTIONS, PRINTER_ENUM_LOCAL, PRINTER_INFO_4W};
    let wide = |pointer: *const u16| -> String {
        if pointer.is_null() {
            return String::new();
        }
        // SAFETY: the system gives a string that ends in zero.
        let length = (0..).take_while(|&at| unsafe { *pointer.add(at) } != 0).count();
        String::from_utf16_lossy(unsafe { std::slice::from_raw_parts(pointer, length) })
    };
    let mut default_name = vec![0u16; 512];
    let mut length = default_name.len() as u32;
    // SAFETY: the buffer is as long as the length given.
    let default = if unsafe { GetDefaultPrinterW(default_name.as_mut_ptr(), &mut length) } != 0 { wide(default_name.as_ptr()) } else { String::new() };
    let flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    let (mut needed, mut count) = (0u32, 0u32);
    // SAFETY: the first call asks only how many bytes the list takes.
    unsafe { EnumPrintersW(flags, std::ptr::null(), 4, std::ptr::null_mut(), 0, &mut needed, &mut count) };
    if needed == 0 {
        return Vec::new();
    }
    let mut buffer = vec![0u64; (needed as usize).div_ceil(8)];
    // SAFETY: the buffer holds the bytes the first call asked for, aligned for the records in it.
    let read = unsafe { EnumPrintersW(flags, std::ptr::null(), 4, buffer.as_mut_ptr().cast(), needed, &mut needed, &mut count) };
    if read == 0 {
        return Vec::new();
    }
    // SAFETY: the buffer holds `count` records of PRINTER_INFO_4W, as the call wrote them.
    let records = unsafe { std::slice::from_raw_parts(buffer.as_ptr().cast::<PRINTER_INFO_4W>(), count as usize) };
    records
        .iter()
        .map(|record| {
            let name = wide(record.pPrinterName);
            Printer { default: name == default, name }
        })
        .collect()
}

#[cfg(not(windows))]
pub fn printers() -> Vec<Printer> {
    let run = |args: &[&str]| std::process::Command::new("lpstat").args(args).output().ok().filter(|done| done.status.success()).map(|done| String::from_utf8_lossy(&done.stdout).into_owned()).unwrap_or_default();
    let default = run(&["-d"]).rsplit(':').next().unwrap_or("").trim().to_string();
    run(&["-e"]).lines().map(str::trim).filter(|line| !line.is_empty()).map(|name| Printer { default: name == default, name: name.to_string() }).collect()
}

/// Whether a page prints here with no dialog of the system's.
pub const SILENT: bool = cfg!(windows);

/// Prints `html` as `settings` say, or writes it as a PDF, from a hidden window of its own, and
/// says what was done.
#[cfg(windows)]
pub fn print(app: &tauri::AppHandle, html: String, settings: Settings) -> Result<String, String> {
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::sync::mpsc;
    use webview2_com::Microsoft::Web::WebView2::Win32::*;
    use webview2_com::{NavigationCompletedEventHandler, PrintCompletedHandler, PrintToPdfCompletedHandler};
    use windows::core::{Interface, HSTRING};

    static NEXT: AtomicU64 = AtomicU64::new(1);
    let label = format!("print-{}", NEXT.fetch_add(1, Ordering::SeqCst));
    let window = tauri::WebviewWindowBuilder::new(app, &label, tauri::WebviewUrl::External("about:blank".parse().map_err(|error| format!("{error}"))?))
        .visible(false)
        .skip_taskbar(true)
        .build()
        .map_err(|error| error.to_string())?;
    let (tell, hear) = mpsc::channel::<Result<String, String>>();
    let target = settings.printer.clone().unwrap_or_default();
    let laid = window.with_webview(move |platform| {
        let failed = tell.clone();
        let started = (|| -> windows::core::Result<()> {
            // SAFETY: each call is to the webview's own interfaces, on the thread that owns them.
            unsafe {
                let core = platform.controller().CoreWebView2()?;
                let mut token = 0i64;
                let tell = tell.clone();
                let handler = NavigationCompletedEventHandler::create(Box::new(move |sender: Option<ICoreWebView2>, _args| {
                    let Some(core) = sender else { return Ok(()) };
                    let environment = core.cast::<ICoreWebView2_2>()?.Environment()?.cast::<ICoreWebView2Environment6>()?;
                    let print_settings = environment.CreatePrintSettings()?;
                    print_settings.SetOrientation(if settings.landscape { COREWEBVIEW2_PRINT_ORIENTATION_LANDSCAPE } else { COREWEBVIEW2_PRINT_ORIENTATION_PORTRAIT })?;
                    print_settings.SetShouldPrintHeaderAndFooter(false.into())?;
                    print_settings.SetShouldPrintBackgrounds(true.into())?;
                    let more = print_settings.cast::<ICoreWebView2PrintSettings2>()?;
                    more.SetColorMode(if settings.gray { COREWEBVIEW2_PRINT_COLOR_MODE_GRAYSCALE } else { COREWEBVIEW2_PRINT_COLOR_MODE_COLOR })?;
                    if let Some(pages) = settings.pages.as_deref().filter(|pages| !pages.trim().is_empty()) {
                        more.SetPageRanges(&HSTRING::from(pages.trim()))?;
                    }
                    let tell = tell.clone();
                    if let Some(pdf) = settings.pdf.clone() {
                        let written = pdf.clone();
                        let done = PrintToPdfCompletedHandler::create(Box::new(move |result: windows::core::Result<()>, ok: bool| {
                            let _ = tell.send(match (result, ok) {
                                (Ok(()), true) => Ok(format!("written to {written}")),
                                (Err(error), _) => Err(format!("the PDF was not written: {error}")),
                                _ => Err(format!("the PDF was not written to {written}")),
                            });
                            Ok(())
                        }));
                        core.cast::<ICoreWebView2_7>()?.PrintToPdf(&HSTRING::from(pdf), &print_settings, &done)?;
                    } else {
                        if let Some(printer) = settings.printer.as_deref().filter(|printer| !printer.is_empty()) {
                            more.SetPrinterName(&HSTRING::from(printer))?;
                        }
                        more.SetCopies(settings.copies.unwrap_or(1).max(1))?;
                        let printer = settings.printer.clone().unwrap_or_else(|| "the default printer".to_string());
                        let done = PrintCompletedHandler::create(Box::new(move |result: windows::core::Result<()>, status: COREWEBVIEW2_PRINT_STATUS| {
                            let _ = tell.send(match (result, status) {
                                (Err(error), _) => Err(format!("{printer} did not print: {error}")),
                                (_, COREWEBVIEW2_PRINT_STATUS_SUCCEEDED) => Ok(format!("sent to {printer}")),
                                (_, COREWEBVIEW2_PRINT_STATUS_PRINTER_UNAVAILABLE) => Err(format!("{printer} is not there to print on: it is off, away or taken out")),
                                _ => Err(format!("{printer} did not print the page")),
                            });
                            Ok(())
                        }));
                        core.cast::<ICoreWebView2_16>()?.Print(&print_settings, &done)?;
                    }
                    Ok(())
                }));
                core.add_NavigationCompleted(&handler, &mut token)?;
                core.NavigateToString(&HSTRING::from(html.as_str()))?;
            }
            Ok(())
        })();
        if let Err(error) = started {
            let _ = failed.send(Err(format!("the page could not be laid out to print: {error}")));
        }
    });
    let outcome = match laid {
        Ok(()) => hear.recv_timeout(std::time::Duration::from_secs(180)).unwrap_or_else(|_| Err(format!("{} gave no answer in three minutes", if target.is_empty() { "the printer" } else { &target }))),
        Err(error) => Err(error.to_string()),
    };
    let _ = window.close();
    outcome
}

#[cfg(not(windows))]
pub fn print(_app: &tauri::AppHandle, _html: String, _settings: Settings) -> Result<String, String> {
    Err("this system prints through its own dialog".into())
}
