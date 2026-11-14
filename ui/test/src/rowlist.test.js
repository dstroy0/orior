// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A list of rows of one height, as many as a tree holds, of which only the rows in sight and those ahead are on the
// page: the height every row would take, a row brought into sight by its key, and the gap above one row.

import { RowList } from "../../gui/src-js/rowlist.js";

const { test, assert } = window.__harness;

const ROWS = Array.from({ length: 10000 }, (_, at) => ({ key: `row-${at}`, depth: at % 3, name: `name ${at}`, open: at % 2 === 0 }));

async function withList(work) {
  const list = document.createElement("div");
  list.style.cssText = "position:fixed;left:0;top:0;width:200px;height:200px;overflow:auto";
  document.body.append(list);
  const rows = new RowList(list, { height: 20, gap: 30, build: (row) => Object.assign(document.createElement("div"), { className: "row", textContent: row.name }) });
  try {
    return await work(rows, list);
  } finally {
    list.remove();
  }
}

test("ten thousand rows take the height they would and put on the page only those in sight and ahead", async () => {
  await withList((rows, list) => {
    rows.set(ROWS);
    assert.equal(rows.count, 10000);
    assert.equal(list.querySelector(".row-list-size").style.height, "200000px");
    const displayed = list.querySelectorAll(".row").length;
    assert.ok(displayed >= 10 && displayed < 200, `${displayed} rows on the page`);
    assert.deepEqual([rows.key(5), rows.depth(5), rows.name(5), rows.open(5)], ["row-5", 2, "name 5", false]);
    assert.equal(rows.indexOf("row-77"), 77);
    assert.equal(rows.indexOf("none"), -1);
  });
});

test("a row brought into sight by its key is put on the page at its place, scrolled to", async () => {
  await withList((rows, list) => {
    rows.set(ROWS);
    const far = rows.reveal("row-5000");
    assert.equal(far.textContent, "name 5000");
    assert.equal(far.style.top, "100000px");
    assert.ok(list.scrollTop + list.clientHeight >= 100020 && list.scrollTop <= 100000, "in sight");
    assert.equal(rows.displayedAt(5000), far);
    assert.equal(rows.reveal("none"), null);
  });
});

test("a gap above one row is every row past it lower", async () => {
  await withList((rows, list) => {
    rows.set(ROWS, 10);
    assert.deepEqual([rows.topOf(9), rows.topOf(10), rows.topOf(11)], [180, 230, 250]);
    assert.equal(list.querySelector(".row-list-size").style.height, "200030px");
  });
});
