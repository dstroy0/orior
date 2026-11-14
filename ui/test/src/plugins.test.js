// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The language plugins as read: each a language opened for the extensions, names and paths it gives, the reader's
// own ahead of one that comes with orior, one turned off handing its extension on, and one that does not read set
// aside with what was wrong; and a drafted plugin's sample colored by its own grammar.

import { invoke } from "../../gui/src-js/bridge.js";
import { languageById, languageForExtension, languageForName, languageForPath, languageFromText, loadPlugins, onPlugins, openerOf, pluginReadings, pluginsOff, setPluginOn, toolFor } from "../../gui/src-js/plugins.js";
import { retentionStorage } from "../helpers.js";
import { putBackAsFound } from "../menus.js";

const { test, assert, calls } = window.__harness;

const found = (id, source, plugin) => ({ id, source, folder: id, text: typeof plugin === "string" ? plugin : JSON.stringify(plugin) });
const FOUND = [
  found("alpha", "orior", { id: "alpha", name: "Alpha", extensions: ["ab"], names: ["Alphafile"], paths: ["^docs/.*\\.txt$"], indentAfter: ":$" }),
  found("beta", "orior", { id: "beta", extensions: ["bb"] }),
  found("beta", "user", { id: "beta", name: "Beta of the reader's", extensions: ["AB"] }),
  found("broken", "orior", "{ not a plugin"),
  found("misplaced", "orior", { id: "other" }),
  found("lint", "orior", { id: "lint", type: "tool", languages: ["alpha"], about: "checks alpha" }),
];

// Runs `work` with the plugins read as FOUND and none off, and the tree's own read again after it with those the
// reader had off.
async function withFound(work) {
  calls.answer("plugins_read", () => FOUND);
  try {
    await retentionStorage(["orior.plugins.off"], async () => {
      localStorage.removeItem("orior.plugins.off");
      await loadPlugins();
      await work();
    });
  } finally {
    calls.answers.delete("plugins_read");
    await loadPlugins();
  }
}

test("each plugin opens what it names, the reader's own ahead of one that comes with orior and located in for it", async () => {
  await withFound(() => {
    assert.equal(openerOf("AB"), "beta", "the reader's beta takes ab from alpha");
    assert.equal(languageForExtension("ab").name, "Beta of the reader's");
    assert.equal(openerOf("bb"), null, "the beta that comes with orior is aside whole");
    assert.equal(languageForName("ALPHAFILE").id, "alpha");
    assert.equal(languageForPath("docs\\notes.txt")?.id, "alpha", "a path's slashes read either way");
    assert.equal(languageForPath("src/notes.txt"), null);
    assert.equal(languageById("alpha").name, "Alpha");
    assert.equal(languageForExtension("zz").id, "plaintext", "an extension no plugin names opens as plain text");
    const readings = new Map(pluginReadings().map((one) => [`${one.id} ${one.source}`, one]));
    assert.ok(readings.get("beta orior").replaced);
    assert.match(readings.get("broken orior").error, /JSON|Expected|Unexpected/);
    assert.equal(readings.get("misplaced orior").error, 'its id is "other", and its folder is misplaced');
    assert.equal(toolFor("alpha")?.id, "lint", "a tool plugin checks the languages it names");
  });
});

test("a plugin turned off passes its extension on, is retained off, and each change is heard", async () => {
  await withFound(() => {
    let heard = 0;
    const stop = onPlugins(() => (heard += 1));
    try {
      setPluginOn("beta", false);
      assert.ok(pluginsOff().has("beta"));
      assert.equal(openerOf("ab"), "alpha", "the next opens it");
      setPluginOn("lint", false);
      assert.equal(toolFor("alpha"), null);
      setPluginOn("beta", true);
      assert.equal(openerOf("ab"), "beta");
      assert.equal(heard, 3);
    } finally {
      stop();
    }
    assert.deepEqual([...pluginsOff()], ["lint"], "retained off where the page retains its settings");
  });
});

test("a plugin's text made a language, as the generator previews it", () => {
  const language = languageFromText(JSON.stringify({ id: "gamma", indentAfter: ":$", pairs: ["()"] }));
  assert.equal(language.name, "gamma", "named by its id where it gives no name");
  assert.ok(language.indentAfter.test("if x:"));
  assert.deepEqual(language.pairs, ["()"]);
  assert.deepEqual(language.quotes, ['"', "'"]);
});

test("a drafted plugin's sample comes colored by the plugin's own grammar, its comment and its keyword among them", async () => {
  const spec = { name: "Gamma", id: "gamma", extensions: ["gam"], from: "", lineComment: "//", blockComment: ["/*", "*/"], keywords: ["when"], types: ["Num"], constants: ["yes"], quotes: ['"'] };
  const drafted = await invoke("plugin_draft", { spec });
  assert.equal(JSON.parse(drafted.text).id, "gamma", "the plugin as its file holds it");
  const { lines, names, runs } = drafted.sample;
  // the name of the token at a line's column
  const nameAt = (line, col) => {
    let name = "";
    for (let at = 0; at < runs[line].length && runs[line][at] <= col; at += 2) {
      name = names[runs[line][at + 1]];
    }
    return name;
  };
  assert.equal(lines[0], "// Gamma opens in this plugin");
  assert.ok(nameAt(0, 0).startsWith("comment"), `the line comment colored as a comment: ${nameAt(0, 0)}`);
  const keyword = lines.findIndex((line) => line.startsWith("when "));
  assert.ok(keyword >= 0 && nameAt(keyword, 0).startsWith("keyword"), `the keyword colored as one: ${nameAt(keyword, 0)}`);
});

test("the window is put back as the file found it", putBackAsFound);
