// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the reader asks of the analysis: an issue's, a hotspot's or a dependency risk's status changed
// on the server; a file or the files git sees changed analysed; local issues reopened; the tree
// scanned for hotspots; the compilation database chosen; and the analyzers the server holds shown.

// The labels of a dependency risk's transitions, by the transition.
const RISK_TRANSITIONS = { CONFIRM: "Confirm", REOPEN: "Reopen", SAFE: "Safe", ACCEPT: "Accept" };

// What a status change is about while its panel is shown.
let changing = null;

// The panel a status is chosen in: the statuses allowed, a comment where one is asked, and Change.
async function drawStatus(title, statuses, comment, labels = {}) {
  const options = statuses.map((one) => `<label><input type="radio" name="status" value="${escaped(one)}"> ${escaped(labels[one] ?? one)}</label>`).join("");
  await shown(
    "status",
    `<h1>${escaped(title)}</h1><form>${options}${comment ? `<label>${escaped(comment)}<br><textarea name="comment"></textarea></label>` : ""}${button("Change Status", "status-change")}</form>`,
  );
}

// An issue resolved: the statuses the server allows for it chosen from, and a comment.
async function issueResolving(folder, issueKey, fileUri, isTaintIssue) {
  if (!(await started())) {
    return;
  }
  const allowed = await orior.server.request("sonarlint/checkIssueStatusChangePermitted", { folderUri: folder, issueKey }).catch(() => null);
  if (!allowed?.permitted) {
    logged(`Status change is not permitted: ${allowed?.notPermittedReason ?? "no answer"}`);
    await shown("ask", `<p>Status change is not permitted: ${escaped(allowed?.notPermittedReason ?? "")}</p>`);
    return;
  }
  changing = { kind: "issue", folder, key: issueKey, fileUri, isTaintIssue: Boolean(isTaintIssue) };
  await drawStatus("Resolve Issue", allowed.allowedStatuses, "Consider providing a reason for muting the issue (leave empty for not adding a new comment)");
}

// A hotspot's status changed: the statuses the server allows for it chosen from.
async function hotspotChanging(hotspotKey, fileUri) {
  const allowed = await orior.server.request("sonarlint/getAllowedHotspotStatuses", { hotspotKey, folderUri: sonar.folder, fileUri }).catch(() => null);
  if (!allowed) {
    return;
  }
  if (!allowed.permitted) {
    await shown("ask", `<p>Not permitted to change hotspot status. Reason: ${escaped(allowed.notPermittedReason)}</p>`);
    return;
  }
  if (!allowed.allowedStatuses.length) {
    await shown("ask", "<p>There are no allowed statuses to set for this hotspot</p>");
    return;
  }
  changing = { kind: "hotspot", key: hotspotKey, fileUri };
  await drawStatus("Change hotspot status", allowed.allowedStatuses, null);
}

// A dependency risk's status changed: the transitions the server allows chosen from, and a reason.
async function riskChanging(folder, riskKey) {
  const answer = await orior.server.request("sonarlint/getDependencyRiskTransitions", { dependencyRiskId: riskKey }).catch(() => null);
  const transitions = answer?.transitions ?? [];
  if (!transitions.length) {
    await shown("ask", "<p>Status change is not permitted for this dependency risk.</p>");
    return;
  }
  changing = { kind: "risk", folder, key: riskKey };
  await drawStatus("Change Dependency Risk Status", transitions, "Provide a reason for changing the dependency risk status (required for 'Accept' and 'Safe' transitions)", RISK_TRANSITIONS);
}

onPressed("status-change", async (value, fields) => {
  const status = fields.status;
  const comment = String(fields.comment ?? "");
  if (!changing || !status) {
    return;
  }
  const what = { issue: "issue", hotspot: "hotspot", risk: "dependency risk" }[changing.kind];
  if ((await chosen(`Are you sure you want to change the status of this ${what}? The status will be changed on the server.`, ["Yes"])) !== 0) {
    return;
  }
  const one = changing;
  changing = null;
  if (one.kind === "issue") {
    await orior.server.notify("sonarlint/changeIssueStatus", { configurationScopeId: one.folder, issueId: one.key, newStatus: status, fileUri: one.fileUri, comment, isTaintIssue: one.isTaintIssue });
  } else if (one.kind === "hotspot") {
    await orior.server.notify("sonarlint/changeHotspotStatus", { hotspotKey: one.key, newStatus: status, fileUri: one.fileUri });
  } else {
    await orior.server.notify("sonarlint/changeDependencyRiskStatus", { configurationScopeId: one.folder, dependencyRiskKey: one.key, transition: status, comment });
  }
  await orior.panels.hide("status");
});

onPressed("finding-resolve", async (value) => {
  const one = listedFinding(value);
  if (one) {
    await issueResolving(sonar.folder, one.finding.data?.serverIssueKey, one.uri, true);
  }
});

onPressed("hotspot-status", async (value) => {
  const one = listedFinding(value);
  if (one) {
    await hotspotChanging(one.finding.data?.serverIssueKey ?? one.finding.data?.entryKey, one.uri);
  }
});

onPressed("risk-status", async (value) => {
  const one = listedFinding(value);
  if (one) {
    await riskChanging(sonar.folder, one.finding.data?.entryKey);
  }
});

// The file the editor shows, with its URI, or null where it shows none.
async function activeFile() {
  const path = await orior.editor.active().catch(() => null);
  return path ? { path, uri: uriInTree(path) } : null;
}

// The file the editor shows analysed now, the settings' exclusions aside.
async function analysedNow() {
  const file = await activeFile();
  if (!file || !(await started())) {
    return;
  }
  const text = await orior.editor.text(file.path).catch(() => null);
  if (text !== null) {
    await orior.server.notify("sonarlint/analyseOpenFileIgnoringExcludes", { triggeredByUser: true, textDocument: { uri: file.uri, languageId: "", version: 1, text } });
  }
}

// The files git sees changed analysed.
async function changedAnalysed() {
  if ((await started()) && sonar.folder) {
    await orior.server.notify("sonarlint/analyzeVCSChangedFiles", { configScopeIds: [sonar.folder] });
  }
}

// The issues resolved in the file the editor shows, while no binding holds them, reopened.
async function reopened() {
  const file = await activeFile();
  if (file && (await started())) {
    await orior.server.notify("sonarlint/reopenResolvedLocalIssues", { configurationScopeId: sonar.folder, relativePath: file.path, fileUri: file.uri });
  }
}

// The tree scanned for hotspots: where the server can find them, its files of the languages it reads
// handed to it, a scan of more than the most files asked first.
async function hotspotsScanned() {
  if (!(await started()) || !sonar.folder) {
    return;
  }
  const supported = await orior.server.request("sonarlint/checkLocalDetectionSupported", { uri: sonar.folder }).catch(() => null);
  if (!supported?.isSupported) {
    await shown("ask", `<p>The project is not compatible with local hotspot detection: ${escaped(supported?.reason ?? "")}</p>`);
    return;
  }
  const patterns = (await orior.server.request("sonarlint/listSupportedFilePatterns", { uri: sonar.folder }).catch(() => null))?.patterns ?? [];
  const paths = await scannedFiles(patterns);
  if (paths.length > SCAN_FILES_MOST && (await chosen(`There are ${paths.length} files to analyze for hotspots in the tree. This might affect performance.`, ["Analyze"])) !== 0) {
    return;
  }
  await orior.server.notify("sonarlint/scanFolderForHotspots", { folderUri: sonar.folder, documents: await scannedDocuments(paths) });
  await drawFindings();
}

// The compilation database chosen: from those of the tree, or none.
async function drawCompile(message) {
  const found = (await orior.tree.files().catch(() => [])).filter((path) => path.split("/").pop() === "compile_commands.json");
  const rows = found.map((path) => `<li>${button(path, "compile-choose", path)}</li>`).join("");
  await shown(
    "compile",
    `<h1>Compilation database</h1>${message ? `<p>${escaped(message)}</p>` : ""}<p>Now: ${escaped(sonar.settings?.pathToCompileCommands ?? "none")}</p>${rows ? `<ul>${rows}</ul>` : '<p class="quiet">No compile_commands.json is in the tree: generate one with your build to analyze C and C++.</p>'}${button("Do Not Ask Again", "compile-quiet")}`,
  );
}

orior.server.on("sonarlint/needCompilationDatabase", () =>
  sonar.settings?.quietCompile ? null : drawCompile("SonarQube for IDE is unable to analyze C and C++ file(s) because there is no configured compilation database."),
);

onPressed("compile-choose", async (path) => {
  await settingsChanged((settings) => {
    settings.pathToCompileCommands = pathOfUri(uriInTree(path));
  });
  await drawCompile(`Analysis configured. Compilation database path is set to: ${path}`);
});

onPressed("compile-quiet", async () => {
  await settingsChanged((settings) => {
    settings.quietCompile = true;
  });
  await orior.panels.hide("compile");
});

// The analyzers the server holds, each with its language, how it stands and where it came from.
async function drawLanguages() {
  if (await started()) {
    const answer = await orior.server.request("sonarlint/getPluginStatuses", { configurationScopeId: sonar.folder }).catch(() => null);
    sonar.statuses = answer?.pluginStatuses ?? sonar.statuses;
  }
  const rows = sonar.statuses
    .slice()
    .sort((one, other) => String(one.language).localeCompare(String(other.language)))
    .map(
      (one) =>
        `<tr><td>${escaped(one.language)}</td><td>${escaped(one.pluginName)}</td><td>${escaped(one.state)}</td><td>${escaped(one.source)}${one.serverVersion ? ` ${escaped(one.serverVersion)}` : ""}</td><td>${escaped(one.actualVersion ?? "")}${one.overriddenVersion ? ` (${escaped(one.overriddenVersion)})` : ""}</td></tr>`,
    )
    .join("");
  await shown("languages", `<h1>Supported languages and analyzers</h1>${rows ? `<table><tr><th>Language</th><th>Analyzer</th><th>State</th><th>Source</th><th>Version</th></tr>${rows}</table>` : '<p class="quiet">The server has not said yet.</p>'}`);
}
