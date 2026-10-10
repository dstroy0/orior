// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Language servers, as servers.rs in the command line's crate runs them: a tab whose language has
// one in the toolchain manifest is handed to it when it opens, told of each change a moment after
// typing rests, and let go when it closes. Its hovers, completions and definitions come from the
// server, and the server's diagnostics are drawn under the text they are about.

import { invoke, listen } from "./bridge.js";

const CHANGE_REST = 300;

let tabsOf = () => [];
let painted = () => {};

// `tabs` gives the open tabs, and `paint` draws the editor again after diagnostics arrive.
export function startServers({ tabs, paint }) {
  tabsOf = tabs;
  painted = paint;
  listen("lsp-diagnostics", (event) => {
    const { path, items } = event.payload;
    for (const tab of tabsOf()) {
      if ((tab.served || tab.serving) && tab.file === path) {
        tab.session.diagnostics = items;
      }
    }
    painted();
  });
}

// Hands a tab to its language's server where there is one. A file read a window at a time, or
// as a commit left it, is not handed over.
export async function serve(tab) {
  const s = tab.session;
  if (!s || s.window || tab.commit || tab.served !== undefined) {
    return;
  }
  tab.served = false;
  const language = s.language?.id;
  if (!language) {
    return;
  }
  // The diagnostics a server already holds for the file can come before it answers.
  tab.serving = true;
  const took = await invoke("lsp_open", { path: tab.file, language, text: s.doc.text() }).catch(() => false);
  tab.serving = false;
  if (took && tabsOf().includes(tab)) {
    tab.served = true;
    s.diagnostics ??= [];
    wrap(tab);
    // A question asked now, its answer let go, has the server read the file through before the
    // reader asks one: the first Go to Definition answers as quickly as the next.
    invoke("lsp_hover", { path: tab.file, line: 0, col: 0 }).catch(() => {});
  } else if (took) {
    invoke("lsp_close", { path: tab.file }).catch(() => {});
  }
}

// Gives a served tab's language the server's hover and completion, over the language's own. A
// language set again, as a plugin read again sets it, is given them again.
export function wrap(tab) {
  const s = tab.session;
  if (!tab.served || !s?.language || s.language.served) {
    return;
  }
  const own = s.language;
  const at = (p) => ({ path: tab.file, line: s.base + p.line, col: p.col });
  s.language = Object.assign(Object.create(own), {
    served: true,
    async hover(doc, p) {
      const parts = [];
      await flush(tab);
      const said = await invoke("lsp_hover", at(p)).catch(() => null);
      if (said) {
        parts.push(said);
      } else if (own.hover) {
        const found = await own.hover(doc, p);
        parts.push(...(found?.parts ?? []));
      }
      if (!parts.length) {
        return null;
      }
      const text = doc.line(p.line);
      let from = p.col;
      let to = p.col;
      while (from > 0 && /\w/.test(text[from - 1])) {
        from -= 1;
      }
      while (to < text.length && /\w/.test(text[to])) {
        to += 1;
      }
      return { from: { line: p.line, col: from }, to: { line: p.line, col: to }, parts };
    },
    async complete(doc, p) {
      await flush(tab);
      const items = await invoke("lsp_complete", at(p)).catch(() => []);
      return items.length ? items : (own.complete?.(doc, p) ?? []);
    },
  });
}

// Tells the server of a change to a served tab, once typing has rested.
export function changed(tab) {
  if (!tab?.served) {
    return;
  }
  window.clearTimeout(tab.telling);
  tab.untold = true;
  tab.telling = window.setTimeout(() => flush(tab), CHANGE_REST);
}

// Tells the server of a change to a served tab now, where one is waiting for typing to rest, so
// that what is asked next is asked of the text as it stands.
export async function flush(tab) {
  window.clearTimeout(tab?.telling);
  if (tab?.served && tab.untold) {
    tab.untold = false;
    await invoke("lsp_change", { path: tab.file, text: tab.session.doc.text() }).catch(() => {});
  }
}

export function stopServing(tab) {
  if (tab?.served) {
    tab.served = false;
    tab.untold = false;
    window.clearTimeout(tab.telling);
    invoke("lsp_close", { path: tab.file }).catch(() => {});
  }
}

// The server's hover at a place in a served tab, as Markdown, or null. Given `text`, the server is
// asked of that text in place of the tab's, and told the tab's own again after. A server still
// reading a change, which answers that the content was modified or answers nothing, is asked again a
// moment later, up to ASKS times for the one and a few for the other.
const ASKS = 20;
export async function hoverAt(tab, p, text = null) {
  if (!tab?.served) {
    return null;
  }
  await flush(tab);
  if (text !== null) {
    await invoke("lsp_change", { path: tab.file, text }).catch(() => {});
  }
  try {
    let empty = 0;
    for (let asked = 0; asked < ASKS; asked += 1) {
      try {
        const said = await invoke("lsp_hover", { path: tab.file, line: tab.session.base + p.line, col: p.col });
        if (said) {
          return said;
        }
        empty += 1;
        if (empty > 3) {
          return null;
        }
      } catch (error) {
        if (!/modified/i.test(String(error))) {
          return null;
        }
      }
      await new Promise((resolve) => window.setTimeout(resolve, 150));
    }
    return null;
  } finally {
    if (text !== null) {
      tab.untold = true;
      await flush(tab);
    }
  }
}

// Where the symbol at a place in a served tab is defined, each { path, line, col } counted from
// the file's first: a path under the tree from its top folder, and any other whole.
export async function definition(tab, p) {
  if (!tab?.served) {
    return [];
  }
  await flush(tab);
  return invoke("lsp_definition", { path: tab.file, line: tab.session.base + p.line, col: p.col }).catch(() => []);
}
