// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The commands: each the command palette lists for this plugin, and each command of an issue's
// action the server leaves to the editor to run.

// What each command of the command palette runs, by its id in plugin.json.
const PALETTE = {
  analyse: () => analysedNow(),
  changed: () => changedAnalysed(),
  reopen: () => reopened(),
  findings: () => drawFindings(),
  hotspots: () => hotspotsScanned(),
  duplications: () => drawDuplications(),
  rules: () => drawRules(null),
  rulesActive: () => drawRules("on"),
  rulesInactive: () => drawRules("off"),
  ruleByKey: () => drawRules(null, ""),
  connections: () => drawConnections(),
  connectServer: () => drawConnection(false, null),
  connectCloud: () => drawConnection(true, null),
  bind: () => settingsRead().then(() => drawBinding(null)),
  autobind: () => settingsRead().then(() => sonar.pressed["binding-auto"]()),
  share: () => settingsRead().then(() => sonar.pressed["binding-share"]()),
  compile: () => settingsRead().then(() => drawCompile(null)),
  newCode: () =>
    settingsChanged((settings) => {
      settings.focusOnNewCode = true;
    }),
  overallCode: () =>
    settingsChanged((settings) => {
      settings.focusOnNewCode = false;
    }),
  automaticOn: () =>
    settingsChanged((settings) => {
      settings.automaticAnalysis = true;
    }),
  automaticOff: () =>
    settingsChanged((settings) => {
      settings.automaticAnalysis = false;
    }),
  clearLocations: () => locationsCleared(),
  languages: () => drawLanguages(),
  verbose: () =>
    settingsChanged((settings) => {
      settings.output.showVerboseLogs = true;
    }).then(() => drawOutput()),
  output: () => drawOutput(),
  settings: () => drawSettings(),
};

for (const [id, run] of Object.entries(PALETTE)) {
  orior.commands.on(id, async () => {
    try {
      await run();
    } catch (error) {
      logged(`${id}: ${error.message}`);
      await shown("ask", `<p class="fail">${escaped(error.message)}</p>`);
    }
  });
}

// The Java arguments saved start the server again, as they are taken only as it starts.
onPressed("settings-restart", async () => {
  await restarted();
  await drawSettings();
});

// What each command of an issue's action the server does not run itself does, by the command.
const ACTIONS = {
  "SonarLint.DeactivateRule": (key) => ruleDeactivated(key),
  "SonarLint.ShowAllLocations": (issue) => drawLocations(issue),
  "SonarLint.ResolveIssue": (folder, issueKey, fileUri, isTaintIssue, isDependencyRisk) =>
    isDependencyRisk ? riskChanging(folder, issueKey) : issueResolving(folder, issueKey, fileUri, isTaintIssue),
  "SonarLint.OpenRuleByKey": (key) => drawRules(null, key),
};

orior.server.on("orior/command", async ({ command, arguments: args }) => {
  const run = ACTIONS[command];
  if (run) {
    await run(...(args ?? []));
  } else {
    logged(`The command ${command} is not one this plugin runs`);
  }
});
