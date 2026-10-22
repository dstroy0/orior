// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The browser's own trace of the window's frames, for what the page's own trace cannot see: the
//! style, layout, paint, raster and compositing each frame takes after its script, and the collector's
//! pauses. A test starts it, and ends it handing it to a query run, each thread's events summed by
//! name a line each, the most time first: `<thread> <name> <count> <total ms> <most ms>`.

use std::collections::HashMap;
use std::io::Write as _;
use std::process::{Command, Stdio};
use std::sync::Mutex;
use std::time::{Duration, Instant};

use serde_json::{json, Value};

use crate::cdp::Connection;

/// What the browser's trace has given so far, and whether it has ended.
struct Collected {
    on: bool,
    events: Vec<Value>,
    ended: bool,
}

static COLLECTED: Mutex<Collected> = Mutex::new(Collected { on: false, events: Vec::new(), ended: false });

/// The categories traced: the timeline of each frame, its frames, and the collector.
const CATEGORIES: &str = "devtools.timeline,disabled-by-default-devtools.timeline.frame,disabled-by-default-devtools.timeline.stack,v8.gc,cc,viz";

/// The lines of the summary handed: the threads' busiest events.
const LINES: usize = 40;

pub fn start(conn: &Connection) -> Result<(), String> {
    {
        let mut collected = COLLECTED.lock().map_err(|error| error.to_string())?;
        *collected = Collected { on: true, events: Vec::new(), ended: false };
    }
    conn.call("Tracing.start", json!({ "categories": CATEGORIES, "transferMode": "ReportEvents" }), Duration::from_secs(10)).map(|_| ())
}

/// An event of the page's connection the trace takes: its events as they come, and its end.
pub fn heard(method: &str, params: &Value) {
    let Ok(mut collected) = COLLECTED.lock() else { return };
    if !collected.on {
        return;
    }
    match method {
        "Tracing.dataCollected" => collected.events.extend(params["value"].as_array().into_iter().flatten().cloned()),
        "Tracing.tracingComplete" => collected.ended = true,
        _ => {}
    }
}

/// Ends the trace and hands its summary to the run at `qry` as the file `name`, a path under the run's top, with `writer`.
pub fn end(conn: &Connection, writer: &str, qry: &str, name: &str) -> Result<(), String> {
    conn.call("Tracing.end", json!({}), Duration::from_secs(10))?;
    let until = Instant::now() + Duration::from_secs(60);
    let events = loop {
        {
            let mut collected = COLLECTED.lock().map_err(|error| error.to_string())?;
            if collected.ended {
                collected.on = false;
                break std::mem::take(&mut collected.events);
            }
        }
        if Instant::now() > until {
            return Err("the browser's trace did not end in 60 s".into());
        }
        std::thread::sleep(Duration::from_millis(50));
    };
    let summary = summary(&events);
    // the summary handed to the run as the file `name`, the writer's member word for a program that is no member
    let mut handing = Command::new(writer).args(["--filed", qry, name]).stdin(Stdio::piped()).stdout(Stdio::null()).stderr(Stdio::null()).spawn().map_err(|error| format!("{writer}: {error}"))?;
    handing.stdin.take().ok_or("the writer took no input")?.write_all(summary.as_bytes()).map_err(|error| error.to_string())?;
    let ended = handing.wait().map_err(|error| error.to_string())?;
    if ended.success() { Ok(()) } else { Err(format!("{writer} --filed {qry} {name} ended {ended}")) }
}

/// Each thread's complete events summed by name, the most time first.
fn summary(events: &[Value]) -> String {
    let mut threads: HashMap<(u64, u64), String> = HashMap::new();
    for event in events.iter().filter(|event| event["ph"] == "M" && event["name"] == "thread_name") {
        threads.insert((event["pid"].as_u64().unwrap_or(0), event["tid"].as_u64().unwrap_or(0)), event["args"]["name"].as_str().unwrap_or("").replace(' ', "_"));
    }
    // Each name's count, its time in all and at most, and the elements its passes touched in all: the
    // elements a style pass recalculated, and the objects a layout found to lay out again.
    let mut sums: HashMap<(String, String), (u64, f64, f64, u64)> = HashMap::new();
    for event in events.iter().filter(|event| event["ph"] == "X") {
        let thread = threads.get(&(event["pid"].as_u64().unwrap_or(0), event["tid"].as_u64().unwrap_or(0))).cloned().unwrap_or_else(|| "thread".into());
        let name = event["name"].as_str().unwrap_or("").replace(' ', "_");
        let ms = event["dur"].as_f64().unwrap_or(0.0) / 1000.0;
        let touched = event["args"]["elementCount"].as_u64().or_else(|| event["args"]["beginData"]["dirtyObjects"].as_u64()).unwrap_or(0);
        let sum = sums.entry((thread, name)).or_insert((0, 0.0, 0.0, 0));
        *sum = (sum.0 + 1, sum.1 + ms, sum.2.max(ms), sum.3 + touched);
    }
    let mut lines: Vec<_> = sums.into_iter().collect();
    lines.sort_by(|a, b| b.1 .1.total_cmp(&a.1 .1));
    let mut said: String = lines
        .into_iter()
        .take(LINES)
        .map(|((thread, name), (count, total, most, touched))| if touched > 0 { format!("{thread} {name} {count} {total:.1} {most:.1} touched {touched}\n") } else { format!("{thread} {name} {count} {total:.1} {most:.1}\n") })
        .collect();
    // A style pass or a layout a script forced, out of the frame's own: summed by the call that forced
    // it, the script's function and its place, the most time first.
    let mut forced: HashMap<String, (u64, f64)> = HashMap::new();
    for event in events.iter().filter(|event| event["ph"] == "X" && matches!(event["name"].as_str(), Some("UpdateLayoutTree" | "Layout"))) {
        let Some(top) = event["args"]["beginData"]["stackTrace"].as_array().and_then(|stack| stack.first()) else { continue };
        let at = format!("{} {} {}:{}", event["name"].as_str().unwrap_or(""), top["functionName"].as_str().filter(|name| !name.is_empty()).unwrap_or("(anonymous)"), top["url"].as_str().unwrap_or("").rsplit('/').next().unwrap_or(""), top["lineNumber"].as_u64().unwrap_or(0) + 1);
        let sum = forced.entry(at).or_insert((0, 0.0));
        *sum = (sum.0 + 1, sum.1 + event["dur"].as_f64().unwrap_or(0.0) / 1000.0);
    }
    let mut forced: Vec<_> = forced.into_iter().collect();
    forced.sort_by(|a, b| b.1 .1.total_cmp(&a.1 .1));
    for (at, (count, total)) in forced.into_iter().take(10) {
        said.push_str(&format!("forced {at} {count} {total:.1}\n"));
    }
    said
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn each_thread_s_events_are_summed_by_name_the_most_time_first() {
        let events = vec![
            json!({"ph": "M", "name": "thread_name", "pid": 1, "tid": 2, "args": {"name": "CrRendererMain"}}),
            json!({"ph": "X", "name": "Layout", "pid": 1, "tid": 2, "dur": 3000}),
            json!({"ph": "X", "name": "Layout", "pid": 1, "tid": 2, "dur": 1000}),
            json!({"ph": "X", "name": "Paint", "pid": 1, "tid": 2, "dur": 5000}),
            json!({"ph": "B", "name": "Open", "pid": 1, "tid": 2}),
        ];
        assert_eq!(summary(&events), "CrRendererMain Paint 1 5.0 5.0\nCrRendererMain Layout 2 4.0 3.0\n");
    }
}
