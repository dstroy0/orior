// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Real input for the window: each step runner.js asks for, sent through the debugging port as the
//! mouse and keyboard send it, or as WebDriver's actions where the window is driven through
//! WebDriver: the page receives the same events, in the same order, with the same keys and codes,
//! as it does from a person. Keys are those of a US keyboard, named as the
//! menus write them: Ctrl+Shift+P, Alt+Left, F5, Ctrl+K Ctrl+0.

use std::time::Duration;

use serde_json::{json, Value};

use crate::cdp::Connection;

const WAIT: Duration = Duration::from_secs(10);

pub const ALT: u32 = 1;
pub const CTRL: u32 = 2;
pub const META: u32 = 4;
pub const SHIFT: u32 = 8;

/// A key as the page receives it: its key, its code, its key code, and the text it types.
#[derive(Debug, PartialEq)]
pub struct Key {
    pub key: String,
    pub code: String,
    pub key_code: u32,
    pub text: Option<String>,
}

/// The keys of the US keyboard that type a mark, each with its code, its key code and the mark it
/// types with Shift held.
const MARKS: &[(char, &str, u32, char)] = &[
    ('`', "Backquote", 192, '~'),
    ('-', "Minus", 189, '_'),
    ('=', "Equal", 187, '+'),
    ('[', "BracketLeft", 219, '{'),
    (']', "BracketRight", 221, '}'),
    ('\\', "Backslash", 220, '|'),
    (';', "Semicolon", 186, ':'),
    ('\'', "Quote", 222, '"'),
    (',', "Comma", 188, '<'),
    ('.', "Period", 190, '>'),
    ('/', "Slash", 191, '?'),
];

const SHIFTED_DIGITS: &str = ")!@#$%^&*(";

/// The keys that type nothing, or type a control character, by the names the menus give them.
const NAMED: &[(&str, &str, &str, u32, Option<&str>)] = &[
    ("Enter", "Enter", "Enter", 13, Some("\r")),
    ("Escape", "Escape", "Escape", 27, None),
    ("Esc", "Escape", "Escape", 27, None),
    ("Tab", "Tab", "Tab", 9, None),
    ("Backspace", "Backspace", "Backspace", 8, None),
    ("Delete", "Delete", "Delete", 46, None),
    ("Insert", "Insert", "Insert", 45, None),
    ("Home", "Home", "Home", 36, None),
    ("End", "End", "End", 35, None),
    ("PageUp", "PageUp", "PageUp", 33, None),
    ("PageDown", "PageDown", "PageDown", 34, None),
    ("Up", "ArrowUp", "ArrowUp", 38, None),
    ("Down", "ArrowDown", "ArrowDown", 40, None),
    ("Left", "ArrowLeft", "ArrowLeft", 37, None),
    ("Right", "ArrowRight", "ArrowRight", 39, None),
    ("ArrowUp", "ArrowUp", "ArrowUp", 38, None),
    ("ArrowDown", "ArrowDown", "ArrowDown", 40, None),
    ("ArrowLeft", "ArrowLeft", "ArrowLeft", 37, None),
    ("ArrowRight", "ArrowRight", "ArrowRight", 39, None),
    ("Space", " ", "Space", 32, Some(" ")),
    ("ContextMenu", "ContextMenu", "ContextMenu", 93, None),
];

/// The modifier keys, each with the bit it sets, its key, its code and its key code.
const MODIFIERS: &[(&[&str], u32, &str, &str, u32)] = &[
    (&["Ctrl", "Control"], CTRL, "Control", "ControlLeft", 17),
    (&["Shift"], SHIFT, "Shift", "ShiftLeft", 16),
    (&["Alt", "Option"], ALT, "Alt", "AltLeft", 18),
    (&["Meta", "Cmd", "Command", "Win", "Super"], META, "Meta", "MetaLeft", 91),
];

/// The key `name` names, with Shift held where `shift`: a letter, a digit, a mark, F1 to F24, or a
/// key of NAMED.
pub fn key(name: &str, shift: bool) -> Option<Key> {
    let mut chars = name.chars();
    if let (Some(one), None) = (chars.next(), chars.next()) {
        return typed(one, shift);
    }
    if let Some((_, key, code, key_code, text)) = NAMED.iter().find(|(named, ..)| named.eq_ignore_ascii_case(name)) {
        return Some(Key { key: key.to_string(), code: code.to_string(), key_code: *key_code, text: text.map(str::to_string) });
    }
    if let Some((_, _, key, code, key_code)) = MODIFIERS.iter().find(|(names, ..)| names.iter().any(|named| named.eq_ignore_ascii_case(name))) {
        return Some(Key { key: key.to_string(), code: code.to_string(), key_code: *key_code, text: None });
    }
    let number: u32 = name.strip_prefix('F').or_else(|| name.strip_prefix('f'))?.parse().ok()?;
    (1..=24).contains(&number).then(|| Key { key: format!("F{number}"), code: format!("F{number}"), key_code: 111 + number, text: None })
}

/// The key that types `one` on a US keyboard, with Shift held where `shift`, or where `one` needs it.
fn typed(one: char, shift: bool) -> Option<Key> {
    if one.is_ascii_alphabetic() {
        let upper = one.to_ascii_uppercase();
        let shown = if shift || one.is_ascii_uppercase() { upper } else { one.to_ascii_lowercase() };
        return Some(Key { key: shown.to_string(), code: format!("Key{upper}"), key_code: upper as u32, text: Some(shown.to_string()) });
    }
    if let Some(digit) = one.to_digit(10) {
        let shown = if shift { SHIFTED_DIGITS.chars().nth(digit as usize)? } else { one };
        return Some(Key { key: shown.to_string(), code: format!("Digit{one}"), key_code: one as u32, text: Some(shown.to_string()) });
    }
    if let Some(at) = SHIFTED_DIGITS.find(one) {
        let digit = char::from_digit(at as u32, 10)?;
        return Some(Key { key: one.to_string(), code: format!("Digit{digit}"), key_code: digit as u32, text: Some(one.to_string()) });
    }
    if one == ' ' {
        return key("Space", shift);
    }
    let (plain, code, key_code, upper) = MARKS.iter().find(|(plain, _, _, upper)| *plain == one || *upper == one)?;
    let shown = if shift || one == *upper { *upper } else { *plain };
    Some(Key { key: shown.to_string(), code: code.to_string(), key_code: *key_code, text: Some(shown.to_string()) })
}

/// The modifier bits and the key of one chord, as Ctrl+Shift+P; a chord whose key is a plus ends ++.
pub fn chord(text: &str) -> Result<(u32, Key), String> {
    let (held, last) = if let Some(rest) = text.strip_suffix("++") { (rest, "+") } else { text.rsplit_once('+').unwrap_or(("", text)) };
    let mut bits = 0;
    for part in held.split('+').filter(|part| !part.is_empty()) {
        let (_, bit, ..) = MODIFIERS.iter().find(|(names, ..)| names.iter().any(|name| name.eq_ignore_ascii_case(part))).ok_or_else(|| format!("{part} in {text} is no modifier key"))?;
        bits |= bit;
    }
    let pressed = key(last, bits & SHIFT != 0).ok_or_else(|| format!("{last} in {text} is no key"))?;
    Ok((bits, pressed))
}

fn send_key(conn: &Connection, kind: &str, key: &Key, bits: u32) -> Result<(), String> {
    let mut params = json!({
        "type": kind,
        "modifiers": bits,
        "key": key.key,
        "code": key.code,
        "windowsVirtualKeyCode": key.key_code,
        "nativeVirtualKeyCode": key.key_code,
    });
    // Ctrl, Alt or Meta held, the key types nothing, as on the keyboard.
    if kind == "keyDown" && bits & (CTRL | ALT | META) == 0 {
        if let Some(text) = &key.text {
            params["text"] = json!(text);
            params["unmodifiedText"] = json!(text);
        }
    }
    if key.code.ends_with("Left") && key.key_code != 37 {
        params["location"] = json!(1);
    }
    conn.call("Input.dispatchKeyEvent", params, WAIT).map(|_| ())
}

/// Presses the modifier keys of `bits` down, in order, and gives what is held after each.
fn hold(conn: &Connection, bits: u32) -> Result<(), String> {
    let mut held = 0;
    for (_, bit, key, code, key_code) in MODIFIERS {
        if bits & bit != 0 {
            held |= bit;
            send_key(conn, "rawKeyDown", &Key { key: key.to_string(), code: code.to_string(), key_code: *key_code, text: None }, held)?;
        }
    }
    Ok(())
}

fn release(conn: &Connection, bits: u32) -> Result<(), String> {
    let mut held = bits;
    for (_, bit, key, code, key_code) in MODIFIERS.iter().rev() {
        if bits & bit != 0 {
            held &= !bit;
            send_key(conn, "keyUp", &Key { key: key.to_string(), code: code.to_string(), key_code: *key_code, text: None }, held)?;
        }
    }
    Ok(())
}

/// Presses `keys`, one chord after another: Ctrl+K Ctrl+0 is two.
pub fn press(conn: &Connection, keys: &str) -> Result<(), String> {
    for text in keys.split(' ').filter(|text| !text.is_empty()) {
        let (bits, pressed) = chord(text)?;
        hold(conn, bits)?;
        let down = if pressed.text.is_some() && bits & (CTRL | ALT | META) == 0 { "keyDown" } else { "rawKeyDown" };
        let sent = send_key(conn, down, &pressed, bits).and_then(|_| send_key(conn, "keyUp", &pressed, bits));
        release(conn, bits)?;
        sent?;
    }
    Ok(())
}

/// Types `text` key by key; a character no key of the keyboard types goes in as the system's input
/// method puts it, and a line's end is Enter.
pub fn type_text(conn: &Connection, text: &str) -> Result<(), String> {
    for one in text.chars() {
        if one == '\n' {
            press(conn, "Enter")?;
            continue;
        }
        match typed(one, false) {
            Some(pressed) => {
                let shift = one.is_ascii_uppercase() || SHIFTED_DIGITS.contains(one) || MARKS.iter().any(|(_, _, _, upper)| *upper == one);
                let bits = if shift { SHIFT } else { 0 };
                hold(conn, bits)?;
                send_key(conn, "keyDown", &pressed, bits)?;
                send_key(conn, "keyUp", &pressed, bits)?;
                release(conn, bits)?;
            }
            None => {
                conn.call("Input.insertText", json!({"text": one.to_string()}), WAIT)?;
            }
        }
    }
    Ok(())
}

fn button_bit(button: &str) -> u32 {
    match button {
        "left" => 1,
        "right" => 2,
        "middle" => 4,
        "back" => 8,
        "forward" => 16,
        _ => 0,
    }
}

fn mouse(conn: &Connection, kind: &str, x: f64, y: f64, button: &str, buttons: u32, count: u32, bits: u32) -> Result<(), String> {
    conn.call("Input.dispatchMouseEvent", json!({"type": kind, "x": x, "y": y, "button": button, "buttons": buttons, "clickCount": count, "modifiers": bits}), WAIT).map(|_| ())
}

/// The modifier bits of a step's `keys`, as Ctrl or Ctrl+Shift.
fn modifier_bits(keys: &str) -> Result<u32, String> {
    let mut bits = 0;
    for part in keys.split('+').filter(|part| !part.is_empty()) {
        let (_, bit, ..) = MODIFIERS.iter().find(|(names, ..)| names.iter().any(|name| name.eq_ignore_ascii_case(part))).ok_or_else(|| format!("{part} is no modifier key"))?;
        bits |= bit;
    }
    Ok(bits)
}

fn number(step: &Value, name: &str) -> Result<f64, String> {
    step[name].as_f64().ok_or_else(|| format!("the step has no {name}"))
}

/// The programs a step left running, each stopped once the window's tests have run.
static STARTED: std::sync::Mutex<Vec<std::process::Child>> = std::sync::Mutex::new(Vec::new());

/// Runs the program a step names with its words, its environment and what its standard input reads.
fn run(step: &Value) -> Result<(), String> {
    use std::io::Write as _;
    let program = step["program"].as_str().ok_or("the step names no program")?;
    let words: Vec<&str> = step["args"].as_array().into_iter().flatten().filter_map(Value::as_str).collect();
    let mut command = std::process::Command::new(program);
    command.args(&words).stdin(std::process::Stdio::piped()).stdout(std::process::Stdio::null()).stderr(std::process::Stdio::null());
    for (key, value) in step["env"].as_object().into_iter().flatten() {
        command.env(key, value.as_str().unwrap_or(""));
    }
    let mut child = command.spawn().map_err(|error| format!("{program}: {error}"))?;
    if let Some(mut input) = child.stdin.take() {
        input.write_all(step["input"].as_str().unwrap_or("").as_bytes()).map_err(|error| format!("{program}: {error}"))?;
    }
    if step["wait"].as_bool() == Some(false) {
        STARTED.lock().map_err(|error| error.to_string())?.push(child);
        return Ok(());
    }
    let ended = child.wait().map_err(|error| format!("{program}: {error}"))?;
    if ended.success() { Ok(()) } else { Err(format!("{program} {} ended {ended}", words.join(" "))) }
}

/// Stops each program a step left running.
pub fn stop_started() {
    if let Ok(mut started) = STARTED.lock() {
        for mut child in started.drain(..) {
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}

/// Performs one step runner.js asked for.
pub fn perform(conn: &Connection, step: &Value) -> Result<(), String> {
    match step["op"].as_str().unwrap_or("") {
        "click" => {
            let (x, y) = (number(step, "x")?, number(step, "y")?);
            let button = step["button"].as_str().unwrap_or("left");
            let count = step["count"].as_u64().unwrap_or(1).max(1) as u32;
            let bits = modifier_bits(step["keys"].as_str().unwrap_or(""))?;
            hold(conn, bits)?;
            let mut clicked = mouse(conn, "mouseMoved", x, y, "none", 0, 0, bits);
            for at in 1..=count {
                clicked = clicked.and_then(|_| mouse(conn, "mousePressed", x, y, button, button_bit(button), at, bits)).and_then(|_| mouse(conn, "mouseReleased", x, y, button, 0, at, bits));
            }
            release(conn, bits)?;
            clicked
        }
        "move" => mouse(conn, "mouseMoved", number(step, "x")?, number(step, "y")?, "none", 0, 0, 0),
        "drag" => {
            let (x, y, to_x, to_y) = (number(step, "x")?, number(step, "y")?, number(step, "toX")?, number(step, "toY")?);
            let steps = step["steps"].as_u64().unwrap_or(12).max(1);
            mouse(conn, "mouseMoved", x, y, "none", 0, 0, 0)?;
            mouse(conn, "mousePressed", x, y, "left", 1, 1, 0)?;
            for at in 1..=steps {
                let part = at as f64 / steps as f64;
                mouse(conn, "mouseMoved", x + (to_x - x) * part, y + (to_y - y) * part, "left", 1, 0, 0)?;
                std::thread::sleep(Duration::from_millis(16));
            }
            mouse(conn, "mouseReleased", to_x, to_y, "left", 0, 1, 0)
        }
        "wheel" => {
            let params = json!({"type": "mouseWheel", "x": number(step, "x")?, "y": number(step, "y")?, "deltaX": step["deltaX"].as_f64().unwrap_or(0.0), "deltaY": step["deltaY"].as_f64().unwrap_or(0.0), "modifiers": 0});
            conn.call("Input.dispatchMouseEvent", params, WAIT).map(|_| ())
        }
        // The page told it has the system's focus, as the harness tells it, or told what is so.
        "focus" => conn.call("Emulation.setFocusEmulationEnabled", json!({"enabled": step["on"].as_bool().unwrap_or(true)}), WAIT).map(|_| ()),
        // A media feature the system sets told to the page, as prefers-reduced-motion, or with no
        // value, what the system truly sets.
        "media" => {
            let feature = json!({"name": step["name"].as_str().unwrap_or(""), "value": step["value"].as_str().unwrap_or("")});
            conn.call("Emulation.setEmulatedMedia", json!({"features": [feature]}), WAIT).map(|_| ())
        }
        // A program started apart from the window, as a script in a terminal of the system's starts
        // one: waited for, and failed where it fails, or left running where `wait` is false.
        "run" => run(step),
        "browser_trace" if step["end"] == true => {
            let word = |key: &str| step[key].as_str().ok_or_else(|| format!("the step names no {key}"));
            crate::frames::end(conn, word("writer")?, word("qry")?, word("name")?)
        }
        "browser_trace" => crate::frames::start(conn),
        "key" => press(conn, step["keys"].as_str().unwrap_or("")),
        // Modifier keys pressed down and held, or let go, as a reader holds Ctrl to see what it does.
        "hold" => hold(conn, modifier_bits(step["keys"].as_str().unwrap_or(""))?),
        "release" => release(conn, modifier_bits(step["keys"].as_str().unwrap_or(""))?),
        "type" => type_text(conn, step["text"].as_str().unwrap_or("")),
        other => Err(format!("the harness has no step {other:?}")),
    }
}

// ---- The same steps through WebDriver ----

/// The keys WebDriver names by a code of its own, each by the key the page receives.
const DRIVEN_CODES: &[(&str, char)] = &[
    ("Enter", '\u{E007}'),
    ("Escape", '\u{E00C}'),
    ("Tab", '\u{E004}'),
    ("Backspace", '\u{E003}'),
    ("Delete", '\u{E017}'),
    ("Insert", '\u{E016}'),
    ("Home", '\u{E011}'),
    ("End", '\u{E010}'),
    ("PageUp", '\u{E00E}'),
    ("PageDown", '\u{E00F}'),
    ("ArrowUp", '\u{E013}'),
    ("ArrowDown", '\u{E015}'),
    ("ArrowLeft", '\u{E012}'),
    ("ArrowRight", '\u{E014}'),
    ("Control", '\u{E009}'),
    ("Shift", '\u{E008}'),
    ("Alt", '\u{E00A}'),
    ("Meta", '\u{E03D}'),
];

/// What WebDriver sends for `key`: the character a key that types gives, F1 to F12 and the keys of
/// DRIVEN_CODES by WebDriver's own codes.
fn driven_value(key: &Key) -> Result<String, String> {
    if let Some((_, code)) = DRIVEN_CODES.iter().find(|(name, _)| *name == key.key) {
        return Ok(code.to_string());
    }
    if key.key.chars().count() == 1 {
        return Ok(key.key.clone());
    }
    let number: u32 = key.key.strip_prefix('F').and_then(|number| number.parse().ok()).ok_or_else(|| format!("WebDriver has no key {}", key.key))?;
    if (1..=12).contains(&number) { Ok(char::from_u32(0xE031 + number - 1).unwrap_or('\u{E031}').to_string()) } else { Err(format!("WebDriver has no key F{number}")) }
}

/// WebDriver's key actions that press the modifier keys of `bits` down, in order, or let them go
/// where `up`, in the reverse order.
fn driven_modifiers(bits: u32, up: bool) -> Vec<Value> {
    let mut ticks: Vec<Value> = MODIFIERS
        .iter()
        .filter(|(_, bit, ..)| bits & bit != 0)
        .filter_map(|(_, _, key, ..)| DRIVEN_CODES.iter().find(|(name, _)| name == key))
        .map(|(_, code)| json!({"type": if up { "keyUp" } else { "keyDown" }, "value": code.to_string()}))
        .collect();
    if up {
        ticks.reverse();
    }
    ticks
}

/// The key actions of one chord: its modifiers down, its key down and up, its modifiers up.
fn driven_chord(text: &str) -> Result<Vec<Value>, String> {
    let (bits, pressed) = chord(text)?;
    let value = driven_value(&pressed)?;
    let mut ticks = driven_modifiers(bits, false);
    ticks.push(json!({"type": "keyDown", "value": value}));
    ticks.push(json!({"type": "keyUp", "value": value}));
    ticks.extend(driven_modifiers(bits, true));
    Ok(ticks)
}

fn keys_source(ticks: Vec<Value>) -> Value {
    json!({"type": "key", "id": "keyboard", "actions": ticks})
}

fn pointer_source(ticks: Vec<Value>) -> Value {
    json!({"type": "pointer", "id": "mouse", "parameters": {"pointerType": "mouse"}, "actions": ticks})
}

/// WebDriver's number for a button, as the page's MouseEvent.button gives it.
fn driven_button(button: &str) -> u32 {
    match button {
        "middle" => 1,
        "right" => 2,
        "back" => 3,
        "forward" => 4,
        _ => 0,
    }
}

/// A move of the pointer to `x`, `y` of the page, taking `ms`.
fn driven_move(x: f64, y: f64, ms: u64) -> Value {
    json!({"type": "pointerMove", "origin": "viewport", "x": x.round() as i64, "y": y.round() as i64, "duration": ms})
}

/// How a step no WebDriver window can take begins what it says: runner.js finds the words, and
/// the test is said as not run on this window, where the window with its debugging port runs it.
pub const NOT_HERE: &str = "not run on a WebDriver window:";

/// Performs one step runner.js asked for, through a WebDriver session: the same steps as `perform`,
/// sent as WebDriver's actions. A step only the debugging port can take, as a trace of the browser's
/// frames or a media feature told to the page, is said not to be taken.
pub fn perform_driven(session: &crate::webdriver::Session, step: &Value) -> Result<(), String> {
    match step["op"].as_str().unwrap_or("") {
        "click" => {
            let (x, y) = (number(step, "x")?, number(step, "y")?);
            let button = driven_button(step["button"].as_str().unwrap_or("left"));
            let count = step["count"].as_u64().unwrap_or(1).max(1);
            let bits = modifier_bits(step["keys"].as_str().unwrap_or(""))?;
            session.actions(json!([keys_source(driven_modifiers(bits, false))]))?;
            let mut ticks = vec![driven_move(x, y, 0)];
            for _ in 0..count {
                ticks.push(json!({"type": "pointerDown", "button": button}));
                ticks.push(json!({"type": "pointerUp", "button": button}));
            }
            let clicked = session.actions(json!([pointer_source(ticks)]));
            session.actions(json!([keys_source(driven_modifiers(bits, true))]))?;
            clicked
        }
        "move" => session.actions(json!([pointer_source(vec![driven_move(number(step, "x")?, number(step, "y")?, 0)])])),
        "drag" => {
            let (x, y, to_x, to_y) = (number(step, "x")?, number(step, "y")?, number(step, "toX")?, number(step, "toY")?);
            let steps = step["steps"].as_u64().unwrap_or(12).max(1);
            let mut ticks = vec![driven_move(x, y, 0), json!({"type": "pointerDown", "button": 0})];
            for at in 1..=steps {
                let part = at as f64 / steps as f64;
                ticks.push(driven_move(x + (to_x - x) * part, y + (to_y - y) * part, 16));
            }
            ticks.push(json!({"type": "pointerUp", "button": 0}));
            session.actions(json!([pointer_source(ticks)]))
        }
        "wheel" => {
            let scroll = json!({"type": "scroll", "origin": "viewport", "x": number(step, "x")?.round() as i64, "y": number(step, "y")?.round() as i64, "deltaX": step["deltaX"].as_f64().unwrap_or(0.0).round() as i64, "deltaY": step["deltaY"].as_f64().unwrap_or(0.0).round() as i64, "duration": 0});
            session.actions(json!([{"type": "wheel", "id": "wheel", "actions": [scroll]}]))
        }
        // The page told it has the system's focus, its document answering so, or what is so.
        "focus" => {
            let script = "const on = arguments[0]; window.__harnessHasFocus ??= document.hasFocus.bind(document); document.hasFocus = on ? () => true : window.__harnessHasFocus; window.dispatchEvent(new Event(on ? 'focus' : 'blur')); return true;";
            session.execute(script, json!([step["on"].as_bool().unwrap_or(true)])).map(|_| ())
        }
        "media" => Err(format!("{NOT_HERE} it is told no media feature, which the window with its debugging port is told")),
        "browser_trace" => Err(format!("{NOT_HERE} it gives no trace of its frames, which the window with its debugging port gives")),
        "run" => run(step),
        "key" => {
            let mut ticks = Vec::new();
            for text in step["keys"].as_str().unwrap_or("").split(' ').filter(|text| !text.is_empty()) {
                ticks.extend(driven_chord(text)?);
            }
            session.actions(json!([keys_source(ticks)]))
        }
        "hold" => session.actions(json!([keys_source(driven_modifiers(modifier_bits(step["keys"].as_str().unwrap_or(""))?, false))])),
        "release" => session.actions(json!([keys_source(driven_modifiers(modifier_bits(step["keys"].as_str().unwrap_or(""))?, true))])),
        "type" => {
            let mut ticks = Vec::new();
            for one in step["text"].as_str().unwrap_or("").chars() {
                let value = if one == '\n' { '\u{E007}'.to_string() } else { one.to_string() };
                ticks.push(json!({"type": "keyDown", "value": value}));
                ticks.push(json!({"type": "keyUp", "value": value}));
            }
            session.actions(json!([keys_source(ticks)]))
        }
        other => Err(format!("the harness has no step {other:?}")),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_chord_gives_its_modifiers_and_its_key() {
        let (bits, pressed) = chord("Ctrl+Shift+P").unwrap();
        assert_eq!(bits, CTRL | SHIFT);
        assert_eq!(pressed, Key { key: "P".into(), code: "KeyP".into(), key_code: 80, text: Some("P".into()) });
        let (bits, pressed) = chord("Alt+Left").unwrap();
        assert_eq!((bits, pressed.key.as_str(), pressed.code.as_str(), pressed.key_code), (ALT, "ArrowLeft", "ArrowLeft", 37));
        assert_eq!(chord("F5").unwrap().1.key_code, 116);
        assert_eq!(chord("Ctrl++").unwrap().1.code, "Equal");
        assert_eq!(chord("Ctrl+Shift+`").unwrap().1, Key { key: "~".into(), code: "Backquote".into(), key_code: 192, text: Some("~".into()) });
        assert_eq!(chord("Ctrl+Shift+0").unwrap().1.key, ")");
        assert_eq!(chord("Ctrl+Shift+Space").unwrap().1.code, "Space");
        assert_eq!(chord("Shift").unwrap(), (0, Key { key: "Shift".into(), code: "ShiftLeft".into(), key_code: 16, text: None }));
    }

    #[test]
    fn a_chord_with_no_such_key_is_said() {
        assert!(chord("Ctrl+Hyper+P").unwrap_err().contains("Hyper"));
        assert!(chord("Ctrl+Nothing").unwrap_err().contains("Nothing"));
    }

    #[test]
    fn a_typed_mark_is_the_key_that_types_it() {
        assert_eq!(typed('"', false).unwrap().code, "Quote");
        assert_eq!(typed('a', false).unwrap().text.as_deref(), Some("a"));
        assert_eq!(typed('A', false).unwrap().text.as_deref(), Some("A"));
        assert_eq!(typed('(', false).unwrap().code, "Digit9");
        assert!(typed('é', false).is_none());
    }

    #[test]
    fn a_chord_through_webdriver_holds_its_modifiers_around_its_key() {
        let ticks = driven_chord("Ctrl+Shift+P").unwrap();
        let said: Vec<(String, String)> = ticks.iter().map(|tick| (tick["type"].as_str().unwrap().to_string(), tick["value"].as_str().unwrap().to_string())).collect();
        let pairs: Vec<(&str, &str)> = said.iter().map(|(kind, value)| (kind.as_str(), value.as_str())).collect();
        assert_eq!(pairs, [("keyDown", "\u{E009}"), ("keyDown", "\u{E008}"), ("keyDown", "P"), ("keyUp", "P"), ("keyUp", "\u{E008}"), ("keyUp", "\u{E009}")]);
    }

    #[test]
    fn a_named_key_is_webdriver_s_code() {
        assert_eq!(driven_value(&key("Enter", false).unwrap()).unwrap(), "\u{E007}");
        assert_eq!(driven_value(&key("F5", false).unwrap()).unwrap(), "\u{E035}");
        assert_eq!(driven_value(&key("Space", false).unwrap()).unwrap(), " ");
        assert_eq!(driven_value(&key("f", true).unwrap()).unwrap(), "F");
        assert!(driven_value(&key("F13", false).unwrap()).unwrap_err().contains("F13"));
        assert!(driven_value(&key("ContextMenu", false).unwrap()).unwrap_err().contains("ContextMenu"));
    }

    #[test]
    fn a_button_is_numbered_as_the_page_numbers_it() {
        assert_eq!((driven_button("left"), driven_button("middle"), driven_button("right")), (0, 1, 2));
        assert_eq!(driven_move(10.4, 20.6, 16), json!({"type": "pointerMove", "origin": "viewport", "x": 10, "y": 21, "duration": 16}));
    }
}
