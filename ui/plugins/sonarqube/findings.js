// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The findings: the server's issues, security hotspots, taint vulnerabilities and dependency risks,
// listed by file under the filter chosen, the new code's first where the tree focuses on it; and the
// locations of an issue or hotspot, its flows, positioned on the files they fall in.

// What each type of finding is called, by the type.
const TYPES = { issue: "Issue", hotspot: "Security Hotspot", taint: "Taint Vulnerability", risk: "Dependency Risk" };

// The filters findings are displayed by, by the filter's id.
const FILTERS = { all: "All", fixable: "Fix Available", open: "Open Files", high: "High Severity", current: "Current File" };

// The issues the server published, by file URI.
const issues = new Map();

// The filter findings are displayed by.
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

// Every finding: its type, its file's URI and the diagnostic the server gave.
function everyFinding() {
  const all = [];
  for (const [type, map] of [["issue", issues], ["hotspot", sonar.hotspots], ["taint", sonar.taints], ["risk", sonar.risks]]) {
    for (const [uri, diagnostics] of map) {
      for (const finding of diagnostics) {
        all.push({ type, uri, finding });
      }
    }
  }
  return all;
}

// Whether a finding is displayed under the filter.
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
async function findingRow({ type, uri, finding }, at) {
  const data = finding.data ?? {};
  const place = finding.range?.start ?? { line: 0, character: 0 };
  const path = (await treePathOf(uri)) ?? pathOfUri(uri);
  const status = data.status !== undefined && type === "hotspot" ? ` | ${["To review", "Acknowledged", "Fixed", "Safe"][data.status] ?? data.status}` : "";
  const actions = [button("Show", "finding-show", String(at))];
  if (type === "issue" || type === "taint") {
    actions.push(button("Actions", "finding-actions", String(at)));
  }
  if (type === "taint") {
    actions.push(button("Resolve", "finding-resolve", String(at)), button("Open on Server", "finding-browse", String(at)));
  }
  if (type === "hotspot") {
    actions.push(button("Change Status", "hotspot-status", String(at)));
    if (finding.source === "Remote_Hotspot" || data.serverIssueKey) {
      actions.push(button("Review on Server", "hotspot-browse", String(at)));
    }
  }
  if (type === "risk") {
    actions.push(button("Change Status", "risk-status", String(at)));
  }
  const impact = data.impactSeverity === undefined ? "" : " | " + (IMPACTS[data.impactSeverity] ?? "");
  return `<tr><td>${escaped(finding.message?.value ?? finding.message)}<br><span class="quiet">${escaped(TYPES[type])}${impact}${status} (${escaped(finding.code ?? "unknown")})</span></td><td>${escaped(path)}<br><span class="quiet">Ln ${place.line + 1}, Col ${place.character}</span></td><td>${actions.join("")}</td></tr>`;
}

// The findings listed, as the panel numbers them.
let listed = [];

// The findings panel: those of the filter, new code's apart from the older where the tree focuses on
// new code.
async function drawFindings() {
  const active = await orior.editor.active().catch(() => null);
  const every = everyFinding();
  const kept = await Promise.all(every.map((one) => filtered(one, active)));
  const all = every.filter((one, at) => kept[at]);
  listed = all;
  const section = async (title, part) => (part.length ? `<h2>${escaped(title)}</h2><table>${(await Promise.all(part.map((one) => findingRow(one, listed.indexOf(one))))).join("")}</table>` : "");
  let body;
  if (sonar.settings?.focusOnNewCode) {
    body = (await section("New code", all.filter((one) => one.finding.data?.isOnNewCode))) + (await section("Older code", all.filter((one) => !one.finding.data?.isOnNewCode)));
  } else {
    body = await section(`${all.length} findings`, all);
  }
  const filters = Object.entries(FILTERS)
    .map(([id, label]) => button(id === filter ? `${label} (displayed)` : label, "findings-filter", id))
    .join("");
  await displayed("findings", `<h1>Findings</h1><div class="row">${filters}${button("Refresh", "findings-filter", filter)}</div>${body || '<p class="quiet">No findings.</p>'}`);
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
  const { type, uri, finding } = one;
  const data = finding.data ?? {};
  const place = finding.range?.start ?? { line: 0, character: 0 };
  const path = await treePathOf(uri);
  if (path !== null && type !== "risk") {
    await orior.open(path, place.line, place.character);
  }
  if (type === "hotspot") {
    await orior.server.request("sonarlint/showHotspotLocations", { hotspotKey: data.entryKey, fileUri: uri }).catch(() => {});
    if (finding.source === "Remote_Hotspot" || data.serverIssueKey) {
      const details = await orior.server.request("sonarlint/getHotspotDetails", { hotspotId: data.entryKey, fileUri: uri }).catch(() => null);
      if (details) {
        await drawRule({ ...details, isHotspot: true });
      }
    } else {
      await orior.server.notify("sonarlint/showHotspotRuleDescription", { hotspotId: data.entryKey, fileUri: uri });
    }
  } else if (type === "taint") {
    await serverCommand("SonarLint.ShowTaintVulnerabilityFlows", data.serverIssueKey, connectionOfTree());
    await serverCommand("SonarLint.ShowIssueDetailsCodeAction", data.entryKey, uri);
  } else if (type === "issue") {
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
    await displayed("ask", `<p>${escaped(path)} is open at the finding: Quick Fix there lists what SonarQube offers for it.</p>`);
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
// and marks positioned on the lines they fall on.
// What the panel adds after a location: that its file is missing or that its code changed.
function locationNote(location) {
  if (location.exists === false) {
    return ' <span class="fail">(file missing)</span>';
  }
  return location.codeMatches === false ? ' <span class="quiet">(code changed)</span>' : "";
}

// The path in the tree of a location, or null where it is outside the tree.
function locationPath(location) {
  return location.uri ? treePathOf(location.uri) : orior.tree.relative(location.filePath).catch(() => null);
}

async function drawLocations(issue) {
  sonar.located = issue;
  const flows = issue.flows ?? [];
  const marks = new Map();
  const place = (location, label, path) => {
    const line = Math.max(0, (location.textRange?.startLine ?? 1) - 1);
    if (path !== null) {
      const list = marks.get(path) ?? [];
      list.push({ line, text: (label + " " + (location.message ?? "")).trim() });
      marks.set(path, list);
    }
    const where = path ?? location.filePath;
    let named = escaped(where);
    if (path !== null) {
      named = linked(where + ":" + (line + 1), "location-open", path + "\n" + line + "\n" + (location.textRange?.startLineOffset ?? 0));
    }
    return `<li>${named} ${escaped(location.message ?? "")}${locationNote(location)}</li>`;
  };
  const first = { uri: issue.fileUri, textRange: issue.textRange, message: issue.message, exists: true, codeMatches: issue.codeMatches };
  // Each flow lists its locations from the last to the first.
  const ordered = flows.map((flow) => (flow.locations ?? []).slice().reverse());
  // The paths of every location, read together, then the list built in order.
  const paths = await Promise.all([first, ...ordered.flat()].map(locationPath));
  const main = place(first, "", paths[0]);
  let next = 1;
  const sections = ordered.map((locations, at) => {
    const prefix = flows.length > 1 ? at + 1 + "." : "";
    const items = locations.map((location, inner) => {
      const item = place(location, prefix + (inner + 1), paths[next]);
      next += 1;
      return item;
    });
    const title = flows.length > 1 ? "Flow " + (at + 1) : "Locations";
    return `<h3>${title}</h3><ol>${items.join("")}</ol>`;
  });
  await Promise.all([...marks].map(([path, list]) => orior.marks.set(path, list)));
  sonar.markedPaths = [...marks.keys()];
  const created = issue.creationDate ? " | " + escaped(issue.creationDate) : "";
  await displayed(
    "locations",
    `<h1>${escaped(issue.message)}</h1><p class="quiet">${escaped(issue.ruleKey)}${created}</p><ul>${main}</ul>${sections.join("")}${button("Clear", "locations-clear")}`,
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
  await displayed(
    "rule",
    `<h1>${escaped(rule.name)} (${escaped(rule.key)})</h1><p>${escaped(hotspot.message)}</p><p class="quiet">${escaped(rule.securityCategory)} | ${escaped(["High", "Medium", "Low"][rule.vulnerabilityProbability] ?? rule.vulnerabilityProbability)} probability | ${escaped(hotspot.status)} | ${escaped(hotspot.author)}</p><h2>What's the risk?</h2>${rule.riskDescription ?? ""}<h2>Are you at risk?</h2>${rule.vulnerabilityDescription ?? ""}<h2>How can you fix it?</h2>${rule.fixRecommendations ?? ""}`,
  );
});

onPressed("location-open", async (value) => {
  const [path, line, col] = String(value).split("\n");
  await orior.open(path, Number(line), Number(col));
});

// The locations displayed cleared, and their marks taken off.
async function locationsCleared() {
  await Promise.all((sonar.markedPaths ?? []).map((path) => orior.marks.set(path, [])));
  sonar.markedPaths = [];
  sonar.located = null;
  await orior.panels.hide("locations");
}

onPressed("locations-clear", () => locationsCleared());
