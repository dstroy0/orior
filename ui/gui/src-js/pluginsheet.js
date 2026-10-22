// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// File, Plugins and File, New Language Plugin. The first lists every plugin as plugins.js read it,
// each with what it opens, where it comes from and a box that turns it on or off, what was wrong
// with one that does not read, and for a script plugin what it reads, writes and reaches, with Let It
// Run where it waits for the reader's leave (scripts.js). Under them stands the catalog the reader
// names (plugin_catalog.rs): the plugins it lists that the search finds, each with what it asks for,
// installed by Install and let run as it asks, and updated only by Update, an update that asks for
// more waiting to be let again. The second asks what a language plugin needs, shows a sample colored
// by the plugin those answers make and the plugin's file as it would be written, both changing as the
// answers do, and writes it to the reader's plugins folder, as `orior_gui file new-plugin` does.

import { invoke } from "./bridge.js";
import { tokenize } from "./editor/tokens.js";
import { languageFromText, loadPlugins, onPlugins, openerOf, pluginReadings, setPluginOn } from "./plugins.js";
import { allowPlugin, askedMore, asksText } from "./scripts.js";

const CATALOG = "orior.plugins.catalog";

// Whether the version `a` comes after `b`, each read as numbers between dots.
function later(a, b) {
  const [one, two] = [a, b].map((text) => String(text).split(".").map((part) => Number.parseInt(part, 10) || 0));
  for (let at = 0; at < Math.max(one.length, two.length); at += 1) {
    if ((one[at] ?? 0) !== (two[at] ?? 0)) {
      return (one[at] ?? 0) > (two[at] ?? 0);
    }
  }
  return false;
}

// Whether each of what `asks` asks for is among what `shown` named.
const within = (asks, shown) => ["reads", "writes", "reaches"].every((kind) => (asks?.[kind] ?? []).every((one) => (shown?.[kind] ?? []).includes(one)));

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

function button(text, act, props = {}) {
  const made = element("button", { type: "button", className: "prefs-button", textContent: text, ...props });
  made.addEventListener("click", act);
  return made;
}

const SOURCES = { bundled: "comes with orior", user: "yours" };

export function showPlugins(sheet) {
  const body = element("div", { className: "sheet-plugins" });
  const said = element("p", { className: "prefs-said", ariaLive: "polite" });
  const filter = element("input", { className: "filter", type: "search", placeholder: "Search", spellcheck: false, ariaLabel: "Search plugins" });
  const list = element("div", { className: "plugins-list", role: "list" });

  const reveal = async (what) => {
    said.textContent = "";
    try {
      await invoke("home_reveal", { what });
    } catch (error) {
      said.textContent = String(error);
    }
  };

  function row(reading) {
    const box = element("input", { type: "checkbox", checked: reading.on, disabled: reading.replaced, ariaLabel: `${reading.name} on` });
    box.addEventListener("change", () => setPluginOn(reading.id, box.checked));
    const source = reading.replaced ? "comes with orior, replaced by yours" : SOURCES[reading.source] ?? reading.source;
    const opens = element(
      "span",
      { className: "plugin-opens" },
      ...(reading.kind === "tool" ? [element("span", { textContent: `checks ${reading.checks.join(", ")}`, title: reading.about })] : []),
      ...(reading.kind === "script" ? [element("span", { className: "plugin-asks", textContent: asksText(reading.asks), title: reading.about })] : []),
      ...reading.extensions.map((ext) => {
        const opener = openerOf(ext);
        const other = !reading.error && opener && opener !== reading.id;
        return element("code", { textContent: `.${ext}`, className: other ? "taken" : "", title: other ? `.${ext} opens in ${opener}` : "" });
      }),
    );
    const made = element(
      "div",
      { className: `plugin-row${reading.error ? " broken" : ""}${reading.on && !reading.replaced ? "" : " off"}`, role: "listitem" },
      box,
      element("span", { className: "plugin-name" }, element("strong", { textContent: reading.name }), element("span", { textContent: ` ${reading.id} ${reading.version}`.trimEnd() })),
      opens,
      element("span", { className: "plugin-source", textContent: source }),
      reading.folder ? button("Open Folder", () => reveal(reading.folder)) : element("span"),
    );
    if (reading.error) {
      made.append(element("p", { className: "plugin-error", textContent: reading.error }));
    }
    const more = reading.kind === "script" && !reading.error && reading.on ? askedMore(reading) : null;
    if (more) {
      made.append(
        element(
          "p",
          { className: "plugin-waits" },
          `It waits for you to let it run: it ${asksText(more)}. `,
          button("Let It Run", async () => {
            await allowPlugin(reading.id);
            said.textContent = `${reading.name} runs.`;
            draw();
          }),
        ),
      );
    }
    made.dataset.id = reading.id;
    return made;
  }

  // The catalog: its address, kept as plugins.catalog, and the plugins it lists.
  const address = element("input", { className: "report-field plugins-catalog-address", type: "text", value: localStorage.getItem(CATALOG) ?? "", placeholder: "A catalog's address, on the web or on this machine", ariaLabel: "Catalog", spellcheck: false });
  const catalog = element("div", { className: "plugins-catalog", role: "list" });
  let listed = [];
  let listedFrom = "";

  function catalogRow(entry) {
    const here = pluginReadings().find((reading) => reading.id === entry.id && !reading.replaced);
    const act = !here ? "Install" : here.source === "user" && later(entry.version, here.version) ? `Update to ${entry.version}` : null;
    const made = element(
      "div",
      { className: "plugin-row plugin-listed", role: "listitem" },
      element("span"),
      element("span", { className: "plugin-name" }, element("strong", { textContent: entry.name || entry.id }), element("span", { textContent: ` ${entry.id} ${entry.version}` })),
      element("span", { className: "plugin-asks", textContent: asksText(entry), title: entry.about }),
      element("span", { className: "plugin-source", textContent: here ? `installed, ${here.version}` : "" }),
      act
        ? button(act, async (event) => {
            event.target.disabled = true;
            try {
              await invoke("plugins_catalog_install", { address: listedFrom, entry });
              await loadPlugins();
              const now = pluginReadings().find((reading) => reading.id === entry.id);
              // An install the reader pressed seeing what it asks for lets it run, where it asks for no
              // more than it showed; an update runs as it was let, and one that asks for more waits.
              if (!here && now?.kind === "script" && within(now.asks, entry)) {
                await allowPlugin(entry.id);
              }
              said.textContent = now && askedMore(now) ? `${entry.name || entry.id} ${entry.version} is installed, and waits for you to let it run.` : `${entry.name || entry.id} ${entry.version} is installed.`;
            } catch (error) {
              said.textContent = String(error);
            }
            draw();
          })
        : element("span"),
    );
    if (entry.about) {
      made.append(element("p", { className: "plugin-about", textContent: entry.about }));
    }
    made.dataset.id = entry.id;
    return made;
  }

  function drawCatalog() {
    const word = filter.value.trim().toLowerCase();
    const found = listed.filter((entry) => !word || [entry.id, entry.name, entry.about].some((text) => String(text ?? "").toLowerCase().includes(word)));
    catalog.replaceChildren(...found.map(catalogRow));
    if (listedFrom && !found.length) {
      catalog.append(element("p", { className: "prefs-said", textContent: word ? "The catalog lists no plugin that matches." : "The catalog lists no plugins." }));
    }
  }

  const readCatalog = button("Read the Catalog", async () => {
    const from = address.value.trim();
    localStorage.setItem(CATALOG, from);
    if (!from) {
      said.textContent = "Name a catalog first.";
      return;
    }
    try {
      listed = await invoke("plugins_catalog_read", { address: from });
      listedFrom = from;
      said.textContent = `The catalog lists ${listed.length} plugin${listed.length === 1 ? "" : "s"}.`;
    } catch (error) {
      listed = [];
      listedFrom = "";
      said.textContent = String(error);
    }
    drawCatalog();
  });

  function draw() {
    const word = filter.value.trim().toLowerCase();
    const shown = pluginReadings().filter((reading) => !word || [reading.id, reading.name, ...reading.extensions].some((text) => text.toLowerCase().includes(word)));
    list.replaceChildren(...shown.map(row));
    if (!shown.length) {
      list.append(element("p", { className: "prefs-said", textContent: word ? "No plugin matches." : "No plugins were found." }));
    }
    drawCatalog();
  }

  filter.addEventListener("input", draw);
  body.append(
    element("h2", { textContent: "Plugins" }),
    element(
      "div",
      { className: "prefs-actions" },
      button("New Language Plugin…", () => {
        dialog.close();
        showGenerator(sheet);
      }),
      button("Open Plugins Folder", () => reveal("plugins")),
      button("Reload", async () => {
        await loadPlugins();
        said.textContent = `Read ${pluginReadings().length} plugins.`;
      }),
    ),
    said,
    filter,
    list,
    element("h3", { textContent: "Catalog" }),
    element("div", { className: "prefs-line plugins-catalog-line" }, address, readCatalog),
    catalog,
  );
  draw();
  const dialog = sheet(body);
  const off = onPlugins(draw);
  dialog.addEventListener("close", off);
  filter.focus();
  return dialog;
}

// The answers the command line's words give, `<name> --ext <ext,...> ...`, as plugins.rs reads them.
function answersOf(args) {
  const answers = { name: [], replace: false };
  const takes = { "--ext": "extensions", "--id": "id", "--from": "from", "--line-comment": "lineComment", "--keywords": "keywords", "--types": "types", "--constants": "constants", "--quotes": "quotes" };
  for (let at = 0; at < args.length; at += 1) {
    const word = args[at];
    if (word === "--replace") {
      answers.replace = true;
    } else if (word === "--block-comment") {
      answers.blockOpen = args[at + 1] ?? "";
      answers.blockClose = args[at + 2] ?? "";
      at += 2;
    } else if (takes[word]) {
      answers[takes[word]] = args[at + 1] ?? "";
      at += 1;
    } else {
      answers.name.push(word);
    }
  }
  answers.name = answers.name.join(" ");
  return answers;
}

const listOf = (text) =>
  text
    .split(",")
    .map((word) => word.trim())
    .filter(Boolean);

// Lines that show each kind of thing a plugin colors, as plugins.rs writes beside a new one.
function sampleOf(plugin) {
  const lines = [];
  const { line, block } = plugin.comments ?? {};
  if (line) {
    lines.push(`${line} ${plugin.name ?? "this file"} opens in this plugin`);
  }
  if (Array.isArray(block) && block.length === 2) {
    lines.push(`${block[0]} a block comment ${block[1]}`);
  }
  const first = (name) => (Array.isArray(plugin.grammar?.[name]) ? plugin.grammar[name][0] : undefined);
  const keyword = first("keywords") ?? first("words") ?? "word";
  const quote = plugin.quotes?.[0] ?? '"';
  lines.push(`${keyword} ${first("types") ?? "Type"} name = ${first("constants") ?? "42"};`);
  lines.push(`${keyword} text = ${quote}a string${quote}, count = 0x2a + 1.5e3;`);
  return lines;
}

// `lines` colored by `grammar`, a line a row.
function colored(grammar, lines) {
  let state = "root";
  return lines.map((text) => {
    const row = element("div");
    if (!grammar) {
      row.textContent = text || " ";
      return row;
    }
    const read = tokenize(grammar, text, state);
    state = read.state;
    read.runs.forEach(([start, name], index) => {
      const end = read.runs[index + 1]?.[0] ?? text.length;
      if (end > start) {
        row.append(element("span", { className: name, textContent: text.slice(start, end) }));
      }
    });
    return row;
  });
}

export function showGenerator(sheet, args = []) {
  const given = answersOf(args);
  const body = element("div", { className: "sheet-generator" });
  const said = element("p", { className: "prefs-said", ariaLive: "polite" });

  const field = (label, key, props = {}) => {
    const input = element("input", { className: "report-field", type: "text", spellcheck: false, value: given[key] ?? "", ...props });
    input.addEventListener("input", soon);
    return { input, label: element("label", { className: "report-row" }, element("span", { textContent: label }), input) };
  };
  const name = field("Name", "name", { placeholder: "My Language" });
  const id = field("Id", "id", { placeholder: "made from the name" });
  const extensions = field("Extensions", "extensions", { placeholder: "mylang, myl" });
  const from = element("select", { className: "report-field", ariaLabel: "Start From" });
  const fromIds = [...new Set(pluginReadings().filter((reading) => !reading.error).map((reading) => reading.id))];
  from.append(element("option", { value: "", textContent: "Nothing: a grammar of its own" }), ...fromIds.map((one) => element("option", { value: one, textContent: one, selected: one === given.from })));
  from.addEventListener("change", soon);
  const lineComment = field("Line Comment", "lineComment", { placeholder: "//" });
  const blockOpen = field("Block Comment Opens", "blockOpen", { placeholder: "/*" });
  const blockClose = field("Block Comment Closes", "blockClose", { placeholder: "*/" });
  const keywords = field("Keywords", "keywords", { placeholder: "if, else, while, return" });
  const types = field("Types", "types", { placeholder: "int, bool" });
  const constants = field("Constants", "constants", { placeholder: "true, false, null" });
  const quotes = field("Quotes", "quotes", { placeholder: "\", '" });
  const replace = element("input", { type: "checkbox", checked: given.replace });

  const sample = element("pre", { className: "generator-sample", ariaLabel: "Sample" });
  const text = element("pre", { className: "generator-text", ariaLabel: "plugin.json" });
  const create = button("Create", make, { className: "prefs-button generator-create", disabled: true });

  const spec = () => ({
    name: name.input.value,
    id: id.input.value,
    extensions: listOf(extensions.input.value),
    from: from.value,
    lineComment: lineComment.input.value.trim(),
    blockComment: blockOpen.input.value.trim() && blockClose.input.value.trim() ? [blockOpen.input.value.trim(), blockClose.input.value.trim()] : [],
    keywords: listOf(keywords.input.value),
    types: listOf(types.input.value),
    constants: listOf(constants.input.value),
    quotes: listOf(quotes.input.value),
  });

  let asked = 0;
  async function preview() {
    const mine = (asked += 1);
    let written;
    try {
      written = await invoke("plugin_draft", { spec: spec() });
    } catch (error) {
      if (mine === asked) {
        said.textContent = String(error);
        create.disabled = true;
      }
      return;
    }
    if (mine !== asked) {
      return;
    }
    text.textContent = written;
    try {
      const plugin = JSON.parse(written);
      const language = languageFromText(written);
      sample.replaceChildren(...colored(language.grammar, sampleOf(plugin)));
      said.textContent = "";
      create.disabled = false;
    } catch (error) {
      said.textContent = String(error.message ?? error);
      create.disabled = true;
    }
  }

  let timer = 0;
  function soon() {
    clearTimeout(timer);
    timer = setTimeout(preview, 120);
  }

  async function make() {
    create.disabled = true;
    try {
      const folder = await invoke("plugin_create", { spec: spec(), replace: replace.checked });
      await loadPlugins();
      said.replaceChildren(`Made ${folder}. `, button("Open Folder", () => invoke("home_reveal", { what: folder }).catch((error) => (said.textContent = String(error)))));
    } catch (error) {
      said.textContent = String(error);
    }
    create.disabled = false;
  }

  body.append(
    element("h2", { textContent: "New Language Plugin" }),
    element(
      "div",
      { className: "generator-panes" },
      element(
        "div",
        { className: "generator-form" },
        name.label,
        id.label,
        extensions.label,
        element("label", { className: "report-row" }, element("span", { textContent: "Start From" }), from),
        lineComment.label,
        element("div", { className: "generator-pair" }, blockOpen.label, blockClose.label),
        keywords.label,
        types.label,
        constants.label,
        quotes.label,
      ),
      element("div", { className: "generator-preview" }, element("h3", { textContent: "Sample" }), sample, element("h3", { textContent: "plugin.json" }), text),
    ),
    element("div", { className: "report-foot" }, said, element("span", { className: "generator-go" }, element("label", { className: "report-check" }, replace, element("span", { textContent: "Replace one of the same id" })), create)),
  );
  const dialog = sheet(body);
  dialog.addEventListener("close", () => clearTimeout(timer));
  name.input.focus();
  preview();
  return dialog;
}
