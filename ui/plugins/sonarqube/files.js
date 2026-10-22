// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The files the server asks about: which the editor holds open, which git ignores, which the
// settings exclude from analysis, and the files of the tree it binds a folder by.

// The names of the files a folder's binding to a project is found from.
const SCANNER_PROPERTIES = "sonar-project.properties";
const AUTOSCAN_PROPERTIES = ".sonarcloud.properties";

// The most files a scan for hotspots takes without asking, and the largest file it reads.
const SCAN_FILES_MOST = 1000;
const SCAN_FILE_BYTES = 500000;

// A glob of the settings as a pattern: `**` any folders, `*` any letters but `/`, `?` any letter.
function globPattern(glob) {
  if (/^\*\*\/\*\.[a-z0-9]{1,6}$/.test(glob)) {
    return `\\.${glob.slice(5)}$`;
  }
  const special = new Set([".", "+", "/", "|", "$", "^", "(", ")", "=", "!", ","]);
  let pattern = "";
  for (let at = 0; at < glob.length; at += 1) {
    const letter = glob[at];
    if (special.has(letter)) {
      pattern += `\\${letter}`;
    } else if (letter === "*") {
      const before = glob[at - 1];
      let stars = 1;
      while (glob[at + 1] === "*") {
        stars += 1;
        at += 1;
      }
      const after = glob[at + 1];
      if (stars > 1 && (before === "/" || before === undefined) && (after === "/" || after === undefined)) {
        pattern += "((?:[^/]*(?:/|$))*)";
        at += 1;
      } else {
        pattern += "([^/]*)";
      }
    } else if (letter === "?") {
      pattern += ".";
    } else {
      pattern += letter;
    }
  }
  return `^${pattern}$`;
}

// Whether a URI's path is matched by any of the globs, case aside; none matches where there are none.
function globbed(uri, globs) {
  const kept = globs.map((one) => one.trim()).filter(Boolean);
  if (!kept.length) {
    return false;
  }
  const path = decodeURIComponent(String(uri).replace(/^file:\/\//, ""));
  return new RegExp(kept.map(globPattern).join("|"), "i").test(path);
}

// The globs the settings exclude from analysis while a folder is bound to no project.
function excludedGlobs() {
  return String(sonar.settings?.analysisExcludesStandalone ?? "").split(",");
}

// Whether the tree is bound to a project.
function bound() {
  return Boolean(sonar.settings?.connectedMode?.project?.projectKey);
}

orior.on("open", ({ path }) => {
  if (path) {
    sonar.open.add(path);
  }
});

orior.on("close", ({ path }) => {
  sonar.open.delete(path);
});

// Whether the editor holds open the file a URI names.
async function openInEditor(uri) {
  const path = await treePathOf(uri);
  return path !== null && sonar.open.has(path);
}

orior.server.on("sonarlint/isOpenInEditor", (uri) => openInEditor(uri));

orior.server.on("sonarlint/isIgnoredByScm", async (uri) => {
  const path = await treePathOf(uri);
  return path === null ? false : orior.git.ignored(path);
});

orior.server.on("sonarlint/shouldAnalyseFile", async ({ uri }) => {
  if (!(await openInEditor(uri))) {
    return { shouldBeAnalysed: false, reason: "Skipping analysis for the file preview: " };
  }
  if (bound()) {
    return { shouldBeAnalysed: true };
  }
  return { shouldBeAnalysed: !globbed(uri, excludedGlobs()), reason: "Skipping analysis for the excluded file: " };
});

orior.server.on("sonarlint/filterOutExcludedFiles", ({ fileUris }) => ({
  fileUris: (fileUris ?? []).filter((uri) => !globbed(uri, excludedGlobs())),
}));

// A file of the tree, its name, its whole path and, where `read`, what it holds.
async function foundFile(path, read) {
  const name = path.split("/").pop();
  const content = read ? await orior.files.read(path).catch(() => null) : null;
  return { fileName: name, filePath: pathOfUri(uriInTree(path)), content };
}

orior.server.on("sonarlint/listFilesInFolder", async ({ folderUri }) => {
  sonar.folder ??= folderUri;
  const files = await orior.tree.files().catch(() => []);
  const found = [];
  for (const path of files) {
    const name = path.split("/").pop();
    if (name === SCANNER_PROPERTIES || name === AUTOSCAN_PROPERTIES) {
      found.push(await foundFile(path, true));
    } else if (/^\.sonarlint\/[^/]+\.json$/.test(path)) {
      found.push(await foundFile(path, true));
    }
  }
  return { foundFiles: found };
});

// The files of the tree a scan for hotspots analyses, as the server is handed them: those the server
// reads by the patterns it gives, git does not ignore and the settings do not exclude.
async function scannedFiles(patterns) {
  const files = await orior.tree.files().catch(() => []);
  const matching = [];
  for (const path of files) {
    const uri = uriInTree(path);
    if (globbed(uri, patterns) && !globbed(uri, excludedGlobs()) && !(await orior.git.ignored(path).catch(() => false))) {
      matching.push(path);
    }
  }
  return matching;
}

// The documents a scan for hotspots hands the server: each file's text as the editor holds it or the
// disk does, a file past the largest size left out.
async function scannedDocuments(paths) {
  const documents = [];
  for (const path of paths) {
    const text = sonar.open.has(path) ? await orior.editor.text(path).catch(() => null) : await orior.files.read(path).catch(() => null);
    if (text === null || text.length > SCAN_FILE_BYTES) {
      logged(`File will not be analysed because it's too large: ${path}`);
      continue;
    }
    documents.push({ uri: uriInTree(path), languageId: "[unknown]", version: 1, text });
  }
  return documents;
}
