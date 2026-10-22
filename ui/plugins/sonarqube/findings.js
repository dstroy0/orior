// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The findings: the server's issues, security hotspots, taint vulnerabilities and dependency risks,
// listed by file under the filter chosen, the new code's first where the tree focuses on it; and the
// locations of an issue or hotspot, its flows, laid on the files they fall in.

// What each kind of finding is called, by the kind.
const KINDS = { issue: "Issue", hotspot: "Security Hotspot", taint: "Taint Vulnerability", risk: "Dependency Risk" };

// The filters findings are shown by, by the filter's id.
const FILTERS = { all: "All", fixable: "Fix Available", open: "Open Files", high: "High Severity", current: "Current File" };

// The issues the server published, by file URI.
const issues = new Map();

// The filter findings are shown by.
let filter = "all";

// The impact severities the protocol numbers, by number.
const IMPACTS = ["Info", "Low", "Medium", "High", "Blocker"];

orior.server.on("textDocument/publishDiagnostics", ({ uri, diagnostics }) => {
  issues.set(uri, diagnostics ?? []);
  if (!(diagnostics ?? []).length) {
    issues.delete(uri);
  }
});

orior.server.on("sonarlint/publishSecurityHotspots", ({ uri, diagnostics }) => {
  sonar.hotspots.set(uri, diagnostics ?? []);
});

orior.server.on("sonarlint/publishTaintVulnerabilities", ({ uri, diagnostics }) => {
  sonar.taints.set(uri, diagnostics ?? []);
});

orior.server.on("sonarlint/publishDependencyRisks", ({ uri, diagnostics }) => {
  sonar.risks.set(uri, diagnostics ?? []);
});

// Every finding: its kind, its file's URI and the diagnostic the server gave.
function everyFinding() {
  const all = [];
  for (const [kind, map] of [["issue", issues], ["hotspot", sonar.hotspots], ["taint", sonar.taints], ["risk", sonar.risks]]) {
    for (const [uri, diagnostics] of map) {
      for (const finding of diagnostics) {
        all.push({ kind, uri, finding });
      }
    }
  }
  return all;
}

// Whether a finding is shown under the filter.
async function filtered({ uri, finding }, active) {
  const data = finding.data ?? {};
  if (filter === "fixable") {
    return Boolean(data.hasQuickFix || data.isAiCodeFixable);
  }
  if (filter === "open") {
    return openInEditor(uri);
  }
  if (filter === "high") {
    return data.impactSeverity === 3 || data.impactSeverity === 4;
  }
  if (filter === "current") {
    return active !== null && (await treePathOf(uri)) === active;
  }
  return true;
}

// The row of a finding: what it says, its rule, its place, and what can be done with it.
async function findingRow({ kind, uri, finding }, at) {
  const data = finding.data ?? {};
  const place = finding.range?.start ?? { line: 0, character: 0 };
  const path = (await treePathOf(uri)) ?? pathOfUri(uri);
  const status = data.status !== undefined && kind === "hotspot" ? ` | ${["To review", "Acknowledged", "Fixed", "Safe"][data.status] ?? data.status}` : "";
  const actions = [button("Show", "finding-show", String(at))];
  if (kind === "issue" || kind === "taint") {
    actions.push(button("Actions", "finding-actions", String(at)));
  }
  if (kind === "taint") {
    actions.push(button("Resolve", "finding-resolve", String(at)), button("Open on Server", "finding-browse", String(at)));
  }
  if (kind === "hotspot") {
    actions.push(button("Change Status", "hotspot-status", String(at)));
    if (finding.source === "Remote_Hotspot" || data.serverIssueKey) {
      actions.push(button("Review on Server", "hotspot-browse", String(at)));
    }
  }
  if (kind === "risk") {
    actions.push(button("Change Status", "risk-status", String(at)));
  }
  return `<tr><td>${escaped(finding.message?.value ?? finding.message)}<br><span class="quiet">${escaped(KINDS[kind])}${data.impactSeverity !== undefined ? ` | ${IMPACTS[data.impactSeverity] ?? ""}` : ""}${status} (${escaped(finding.code ?? "unknown")})</span></td><td>${escaped(path)}<br><span class="quiet">Ln ${place.line + 1}, Col ${place.character}</span></td><td>${actions.join("")}</td></tr>`;
}

// The findings listed, as the panel numbers them.
let listed = [];

// The findings panel: those of the filter, new code's apart from the older where the tree focuses on
// new code.
async function drawFindings() {
  const active = await orior.editor.active().catch(() => null);
  const all = [];
  for (const one of everyFinding()) {
    if (await filtered(one, active)) {
      all.push(one);
    }
  }
  listed = all;
  const section = async (title, part) => (part.length ? `<h2>${escaped(title)}</h2><table>${(await Promise.all(part.map((one) => findingRow(one, listed.indexOf(one))))).join("")}</table>` : "");
  let body;
  if (sonar.settings?.focusOnNewCode) {
    body = (await section("New code", all.filter((one) => one.finding.data?.isOnNewCode))) + (await section("Older code", all.filter((one) => !one.finding.data?.isOnNewCode)));
  } else {
    body = await section(`${all.length} findings`, all);
  }
  const filters = Object.entries(FILTERS)
    .map(([id, label]) => button(id === filter ? `${label} (shown)` : label, "findings-filter", id))
    .join("");
  await shown("findings", `<h1>Findings</h1><div class="row">${filters}${button("Refresh", "findings-filter", filter)}</div>${body || '<p class="quiet">No findings.</p>'}`);
}

onPressed("findings-filter", async (value) => {
  filter = FILTERS[value] ? value : "all";
  await drawFindings();
});

// A finding listed in the panel, by the number it is listed by.
function listedFinding(value) {
  return listed[Number(value)] ?? null;
}

// A command of the server's run, its failure logged.
async function serverCommand(command, ...args) {
  await orior.server.request("workspace/executeCommand", { command, arguments: args }).catch((error) => logged(`${command}: ${error.message}`));
}

onPressed("finding-show", async (value) => {
  const one = listedFinding(value);
  if (!one) {
    return;
  }
  const { kind, uri, finding } = one;
  const data = finding.data ?? {};
  const place = finding.range?.start ?? { line: 0, character: 0 };
  const path = await treePathOf(uri);
  if (path !== null && kind !== "risk") {
    await orior.open(path, place.line, place.character);
  }
  if (kind === "hotspot") {
    await orior.server.request("sonarlint/showHotspotLocations", { hotspotKey: data.entryKey, fileUri: uri }).catch(() => {});
    if (finding.source === "Remote_Hotspot" || data.serverIssueKey) {
      const details = await orior.server.request("sonarlint/getHotspotDetails", { hotspotId: data.entryKey, fileUri: uri }).catch(() => null);
      if (details) {
        await drawRule({ ...details, isHotspot: true });
      }
    } else {
      await orior.server.notify("sonarlint/showHotspotRuleDescription", { hotspotId: data.entryKey, fileUri: uri });
    }
  } else if (kind === "taint") {
    await serverCommand("SonarLint.ShowTaintVulnerabilityFlows", data.serverIssueKey, connectionOfTree());
    await serverCommand("SonarLint.ShowIssueDetailsCodeAction", data.entryKey, uri);
  } else if (kind === "issue") {
    await serverCommand("SonarLint.ShowIssueFlows", data.entryKey, uri);
    await serverCommand("SonarLint.ShowIssueDetailsCodeAction", data.entryKey, uri);
  } else {
    await orior.server.notify("sonarlint/openDependencyRiskInBrowser", { folderUri: uri, issueId: data.entryKey });
  }
});

// A finding's actions: its file opened at it, where the editor's quick fixes list what SonarQube offers.
onPressed("finding-actions", async (value) => {
  const one = listedFinding(value);
  const path = one ? await treePathOf(one.uri) : null;
  if (path !== null) {
    const place = one.finding.range?.start ?? { line: 0, character: 0 };
    await orior.open(path, place.line, place.character);
    await shown("ask", `<p>${escaped(path)} is open at the finding: Quick Fix there lists what SonarQube offers for it.</p>`);
  }
});

onPressed("finding-browse", async (value) => {
  const one = listedFinding(value);
  if (one) {
    await serverCommand("SonarLint.BrowseTaintVulnerability", one.finding.data?.serverIssueKey, one.uri);
  }
});

onPressed("hotspot-browse", async (value) => {
  const one = listedFinding(value);
  if (one) {
    await orior.server.notify("sonarlint/openHotspotInBrowser", { hotspotId: one.finding.data?.entryKey, fileUri: one.uri });
  }
});

// The connection the tree is bound through, null where it is bound to none.
function connectionOfTree() {
  return sonar.settings?.connectedMode?.project?.connectionId ?? null;
}

// The places of an issue or hotspot: the file and line of each location, by flow, each a link to it,
// and marks laid on the lines they fall on.
async function drawLocations(issue) {
  sonar.located = issue;
  const flows = issue.flows ?? [];
  const marks = new Map();
  const place = async (location, label) => {
    const path = location.uri ? await treePathOf(location.uri) : await orior.tree.relative(location.filePath).catch(() => null);
    const line = Math.max(0, (location.textRange?.startLine ?? 1) - 1);
    if (path !== null) {
      const list = marks.get(path) ?? [];
      list.push({ line, text: `${label} ${location.message ?? ""}`.trim() });
      marks.set(path, list);
    }
    const where = path ?? location.filePath;
    const exists = location.exists === false ? ' <span class="fail">(file missing)</span>' : location.codeMatches === false ? ' <span class="quiet">(code changed)</span>' : "";
    return `<li>${path === null ? escaped(where) : linked(`${where}:${line + 1}`, "location-open", `${path}\n${line}\n${location.textRange?.startLineOffset ?? 0}`)} ${escaped(location.message ?? "")}${exists}</li>`;
  };
  const main = await place({ uri: issue.fileUri, textRange: issue.textRange, message: issue.message, exists: true, codeMatches: issue.codeMatches }, "");
  const sections = [];
  for (const [at, flow] of flows.entries()) {
    const items = [];
    for (const [inner, location] of (flow.locations ?? []).slice().reverse().entries()) {
      items.push(await place(location, `${flows.length > 1 ? `${at + 1}.` : ""}${inner + 1}`));
    }
    sections.push(`<h3>${flows.length > 1 ? `Flow ${at + 1}` : "Locations"}</h3><ol>${items.join("")}</ol>`);
  }
  for (const [path, list] of marks) {
    await orior.marks.set(path, list);
  }
  sonar.markedPaths = [...marks.keys()];
  await shown(
    "locations",
    `<h1>${escaped(issue.message)}</h1><p class="quiet">${escaped(issue.ruleKey)}${issue.creationDate ? ` | ${escaped(issue.creationDate)}` : ""}</p><ul>${main}</ul>${sections.join("")}${button("Clear", "locations-clear")}`,
  );
  if (issue.shouldOpenRuleDescription) {
    await serverCommand("SonarLint.OpenStandaloneRuleDesc", issue.ruleKey);
  }
}

orior.server.on("sonarlint/showIssueOrHotspot", (issue) => drawLocations(issue));

orior.server.on("sonarlint/showIssue", async (issue) => {
  const path = await treePathOf(issue.fileUri);
  if (path !== null) {
    await orior.open(path, Math.max(0, (issue.textRange?.startLine ?? 1) - 1), issue.textRange?.startLineOffset ?? 0);
  }
  await drawLocations(issue);
});

orior.server.on("sonarlint/showHotspot", async (hotspot) => {
  const path = await orior.tree.relative(hotspot.ideFilePath).catch(() => null);
  const line = Math.max(0, (hotspot.textRange?.startLine ?? 1) - 1);
  if (path !== null) {
    await orior.open(path, line, hotspot.textRange?.startLineOffset ?? 0);
  }
  const rule = hotspot.rule ?? {};
  await shown(
    "rule",
    `<h1>${escaped(rule.name)} (${escaped(rule.key)})</h1><p>${escaped(hotspot.message)}</p><p class="quiet">${escaped(rule.securityCategory)} | ${escaped(["High", "Medium", "Low"][rule.vulnerabilityProbability] ?? rule.vulnerabilityProbability)} probability | ${escaped(hotspot.status)} | ${escaped(hotspot.author)}</p><h2>What's the risk?</h2>${rule.riskDescription ?? ""}<h2>Are you at risk?</h2>${rule.vulnerabilityDescription ?? ""}<h2>How can you fix it?</h2>${rule.fixRecommendations ?? ""}`,
  );
});

onPressed("location-open", async (value) => {
  const [path, line, col] = String(value).split("\n");
  await orior.open(path, Number(line), Number(col));
});

// The locations shown cleared, and their marks taken off.
async function locationsCleared() {
  for (const path of sonar.markedPaths ?? []) {
    await orior.marks.set(path, []);
  }
  sonar.markedPaths = [];
  sonar.located = null;
  await orior.panels.hide("locations");
}

onPressed("locations-clear", () => locationsCleared());
