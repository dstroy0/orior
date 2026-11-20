// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// A frame loop that reports on itself. EV.watchLoop runs the step of a page once per turn inside a guard, and asks
// EV.nextTurn for the next one. No page schedules its own frames. A throw stops the loop.
//
// The code reports a loop that stays awake for its patience and turns fewer than twice. One turn is what a dead loop
// gives, because setup draws a correct still frame first, and then the first turn throws.
// The code does not judge a hidden page. The browser holds its frames, and the count starts again when the page
// shows.
//
// A loop runs for the life of the page or on demand. A step that returns false has nothing left to draw, and
// the loop sleeps until the page wakes it. Each loop starts asleep, and a loop that sleeps is healthy.
//
//   const loop = EV.watchLoop("scope view", (now) => { draw(now); }, { vsync: () => view.vsync, settle });
//   loop.wake();
//
// The loop asks vsync, when the page gives it, before each turn. Without it, each turn waits for the refresh. settle,
// when the page gives it, returns a promise that the next uncapped turn waits on. A turn never queues behind
// unfinished work.
//
// Each access of window.__loopHealth reads it fresh, and it covers each loop on the page. Its ok is true, false, or
// null before the first verdict. Turns counts them all; why and detail come from the loop that settled the
// verdict; loops holds each loop's own record. A failure also shows on the page in a line at the top. A page
// whose loop cannot start at all, with no device to draw on, says so through EV.noLoop and shows its own reason.

EV.loops = [];

EV.noLoop = (label, why, detail) => {
  EV.loops.push({ label, ok: false, turns: 0, why, detail: detail || "", awake: false });
};

EV.loopHealth = () => {
  const loops = EV.loops.map((one) => ({ ...one }));
  const failed = loops.find((one) => one.ok === false);
  const waiting = loops.find((one) => one.ok === null);
  const deciding = failed || waiting || loops[0];
  return {
    ok: loops.length === 0 ? null : failed ? false : waiting ? null : true,
    turns: loops.reduce((sum, one) => sum + one.turns, 0),
    why: deciding ? `${deciding.label}: ${deciding.why}` : "no frame loop on this page",
    detail: failed ? failed.detail : "",
    loops,
  };
};

Object.defineProperty(window, "__loopHealth", { configurable: true, enumerable: true, get: EV.loopHealth });

EV.loopAlarm = (text) => {
  let line = document.getElementById("loopAlarm");
  if (!line) {
    line = document.createElement("div");
    line.id = "loopAlarm";
    line.setAttribute("role", "alert");
    line.style.cssText = "position: fixed; left: 50%; top: 24px; transform: translateX(-50%); z-index: 30; "
      + "padding: 6px 12px; border-radius: 6px; background: #d55e00; color: #ffffff; font: 12px/1.4 system-ui, sans-serif;";
    document.body.appendChild(line);
  }
  line.textContent = text;
  line.hidden = false;
};

EV.watchLoop = (label, step, options = {}) => {
  const patience = options.patience || 1500;
  const record = { label, ok: true, turns: 0, why: "asleep, nothing to draw", detail: "", awake: false };
  EV.loops.push(record);
  let stopped = false;
  let from = 0;
  let timer = 0;
  let unseen = false;

  const fail = (why, detail) => {
    record.ok = false;
    record.why = why;
    record.detail = detail || "";
    console.error(`[${label}] frame loop ${why}`, record.detail);
    EV.loopAlarm(`${label}: frame loop ${why}`);
  };

  const judge = () => {
    timer = 0;
    if (stopped || !record.awake || record.ok === false) {
      return;
    }
    if (document.visibilityState === "hidden") {
      unseen = true;
      return;
    }
    if (record.turns - from < 2) {
      fail(`turned ${record.turns - from} time(s) in ${patience} ms`, "a page that draws one frame and stops looks correct and is not");
    }
  };

  const arm = () => {
    from = record.turns;
    clearTimeout(timer);
    timer = setTimeout(judge, patience);
  };

  document.addEventListener("visibilitychange", () => {
    if (unseen && document.visibilityState !== "hidden") {
      unseen = false;
      arm();
    }
  });

  const schedule = () => {
    const vsync = options.vsync ? options.vsync() : true;
    const waiting = !vsync && options.settle ? options.settle() : null;
    if (waiting) {
      waiting.then(() => EV.nextTurn(options.vsync ? options.vsync() : true, turn));
    } else {
      EV.nextTurn(vsync, turn);
    }
  };

  const turn = (now) => {
    if (stopped) {
      return;
    }
    let more;
    try {
      more = step(now);
    } catch (thrown) {
      stopped = true;
      record.awake = false;
      fail(`threw on turn ${record.turns + 1}: ${thrown && thrown.message ? thrown.message : thrown}`,
           String(thrown && thrown.stack ? thrown.stack : thrown));
      return;
    }
    record.turns += 1;
    if (record.ok === null && record.turns - from >= 2) {
      record.ok = true;
      record.why = "turned twice";
    }
    if (more === false) {
      record.awake = false;
      if (record.ok === null) {
        record.ok = true;
        record.why = "asleep, nothing to draw";
      }
      return;
    }
    schedule();
  };

  return {
    record,
    // Asks for turns. A loop already awake retains running, and a stopped loop stays stopped.
    wake: () => {
      if (stopped || record.awake) {
        return;
      }
      record.awake = true;
      if (record.ok !== false) {
        record.ok = null;
        record.why = "not judged yet";
      }
      arm();
      schedule();
    },
  };
};
