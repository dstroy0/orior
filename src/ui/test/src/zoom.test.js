// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { STEPS, setZoom, zoom, zoomBy } from "../../src/zoom.js";
import { callsTo, keepingStorage } from "../helpers.js";

const { test, assert, ui, calls } = window.__harness;

const status = () => document.getElementById("status-zoom");

// The window's zoom is the test's own: the call that sets it is answered here, and the page stays
// at its size.
const holdZoom = () => calls.answer("zoom_set", () => null);

test("the window opens at 100%, with nothing on the status bar", () => {
  assert.equal(zoom(), 1);
  assert.equal(status().hidden, true);
});

test("zooming in and out goes a step at a time and shows on the status bar", () =>
  keepingStorage(["orior.zoom"], () => {
    holdZoom();
    zoomBy(1);
    assert.equal(zoom(), 1.1);
    assert.equal(status().hidden, false);
    assert.equal(status().textContent, "110%");
    zoomBy(-1);
    zoomBy(-1);
    assert.equal(zoom(), 0.9);
    assert.equal(status().textContent, "90%");
    zoomBy(0);
    assert.equal(zoom(), 1);
    assert.equal(status().hidden, true);
    assert.deepEqual(callsTo("zoom_set").map((one) => one.args.factor), [1.1, 1, 0.9, 1]);
  }));

test("the zoom stops at its first and last steps", () =>
  keepingStorage(["orior.zoom"], () => {
    holdZoom();
    setZoom(STEPS[0]);
    zoomBy(-1);
    assert.equal(zoom(), STEPS[0]);
    setZoom(STEPS.at(-1));
    zoomBy(1);
    assert.equal(zoom(), STEPS.at(-1));
    setZoom(1);
  }));

test("a kept zoom that is no step reads as 100%", () =>
  keepingStorage(["orior.zoom"], () => {
    localStorage.setItem("orior.zoom", "1.33");
    assert.equal(zoom(), 1);
  }));

test("Ctrl+= and Ctrl+- zoom, Ctrl+0 sets it back", () =>
  keepingStorage(["orior.zoom"], async () => {
    holdZoom();
    await ui.key("Ctrl+=");
    await ui.waitFor(() => zoom() === 1.1);
    await ui.key("Ctrl+-");
    await ui.key("Ctrl+-");
    await ui.waitFor(() => zoom() === 0.9);
    await ui.key("Ctrl+0");
    await ui.waitFor(() => zoom() === 1);
  }));

test("a click on the status bar's zoom sets it back to 100%", () =>
  keepingStorage(["orior.zoom"], async () => {
    holdZoom();
    setZoom(1.25);
    assert.equal(status().title, "Reset Zoom");
    await ui.click("#status-zoom");
    await ui.waitFor(() => zoom() === 1);
    assert.equal(status().hidden, true);
  }));
