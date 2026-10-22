// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The settings of SonarQube for IDE, kept as this plugin's own and given to the server as the
// `sonarlint` section it asks for; a connection's token kept in the system's keychain.

// Every setting and what it is where none is set.
const DEFAULTS = {
  connectedMode: { connections: { sonarqube: [], sonarcloud: [] }, project: {} },
  automaticAnalysis: true,
  rules: {},
  pathToCompileCommands: null,
  testFilePattern: "",
  analysisExcludesStandalone: "",
  pathToNodeExecutable: null,
  focusOnNewCode: false,
  output: { showVerboseLogs: false },
  analyzerProperties: null,
  ls: { vmargs: null },
};

// `value` filled in from `fallback` where it leaves a member of it out.
function filled(value, fallback) {
  if (fallback === null || typeof fallback !== "object" || Array.isArray(fallback)) {
    return value === undefined ? fallback : value;
  }
  const own = value !== null && typeof value === "object" && !Array.isArray(value) ? value : {};
  const whole = { ...own };
  for (const [name, inner] of Object.entries(fallback)) {
    whole[name] = filled(own[name], inner);
  }
  return whole;
}

// The settings read, the defaults filled in.
async function settingsRead() {
  const kept = await orior.settings.read().catch(() => null);
  sonar.settings = filled(kept ?? {}, DEFAULTS);
  return sonar.settings;
}

// The settings changed by `change`, written, and the server told to ask for them again.
async function settingsChanged(change) {
  const settings = sonar.settings ?? (await settingsRead());
  change(settings);
  await orior.settings.write(settings);
  if (sonar.started) {
    await orior.server.notify("workspace/didChangeConfiguration", { settings: {} });
  }
}

// The `sonarlint` section as the server reads it: the settings, its usage statistics off, and a setting
// set to nothing left out, as the server takes no null.
function configuration() {
  const settings = sonar.settings ?? filled({}, DEFAULTS);
  const section = {
    connectedMode: settings.connectedMode,
    automaticAnalysis: settings.automaticAnalysis,
    rules: settings.rules,
    pathToCompileCommands: settings.pathToCompileCommands,
    testFilePattern: settings.testFilePattern,
    analysisExcludesStandalone: settings.analysisExcludesStandalone,
    pathToNodeExecutable: settings.pathToNodeExecutable,
    focusOnNewCode: settings.focusOnNewCode,
    output: settings.output,
    analyzerProperties: settings.analyzerProperties ?? {},
    disableTelemetry: true,
  };
  return Object.fromEntries(Object.entries(section).filter(([, value]) => value !== null && value !== undefined));
}

orior.server.on("workspace/configuration", async ({ items }) => {
  await (sonar.settings ? null : settingsRead());
  return (items ?? []).map((item) => {
    sonar.folder ??= item?.scopeUri ?? null;
    const whole = configuration();
    if (!item?.section || item.section === "sonarlint") {
      return whole;
    }
    // a part of the section, as `sonarlint.output` names it
    return item.section.replace(/^sonarlint\.?/, "").split(".").reduce((at, name) => at?.[name] ?? null, whole);
  });
});

// The key a connection's token is kept by: a server's address, or an organization, its region before
// it where one is named.
function tokenKey(connection) {
  if (connection.serverUrl !== undefined) {
    return connection.serverUrl;
  }
  return `${connection.region ? `${connection.region}_` : ""}${connection.organizationKey}`;
}

orior.server.on("sonarlint/getTokenForServer", async (serverOrOrganization) => orior.secrets.read(String(serverOrOrganization)));

// The JVM's own arguments the settings give, each as the server is started with it.
function machineArguments() {
  const line = String(sonar.settings?.ls?.vmargs ?? "");
  return (line.match(/(?:[^\s"]+|"[^"]*")+/g) ?? [])
    .map((one) => one.replace(/(\\)?"/g, (whole, slash) => (slash ? whole : "")).replace(/\\"/g, '"'))
    .filter((one, at, all) => all.indexOf(one) === at);
}

// The settings panel: each setting a field, written as it is saved.
async function drawSettings() {
  const settings = sonar.settings ?? (await settingsRead());
  const text = (name, label, value, about) =>
    `<label>${escaped(label)}<br><input name="${name}" value="${escaped(value ?? "")}" size="60"><br><span class="quiet">${escaped(about)}</span></label>`;
  const check = (name, label, value) => `<label><input type="checkbox" name="${name}"${value ? " checked" : ""}> ${escaped(label)}</label>`;
  await shown(
    "settings",
    `<h1>SonarQube settings</h1><form>
${check("automaticAnalysis", "Analyze files as they open and change", settings.automaticAnalysis)}
${check("focusOnNewCode", "Focus on new code", settings.focusOnNewCode)}
${check("verbose", "Verbose logging in the output", settings.output.showVerboseLogs)}
${text("analysisExcludesStandalone", "Files excluded from analysis", settings.analysisExcludesStandalone, "Globs, separated by commas, of files left out of analysis while the tree is bound to no project.")}
${text("testFilePattern", "Test files", settings.testFilePattern, "A glob of the files analysed as tests.")}
${text("pathToCompileCommands", "Compilation database", settings.pathToCompileCommands, "The path to compile_commands.json for C and C++ analysis.")}
${text("pathToNodeExecutable", "Node.js", settings.pathToNodeExecutable, "The Node.js program JavaScript and TypeScript are analysed with; the one on the path where none is named.")}
${text("vmargs", "Java arguments", settings.ls.vmargs, "Arguments the language server's Java starts with, as -Xmx1024m; taken as the server starts again.")}
<label>Analyzer properties<br><textarea name="analyzerProperties">${escaped(settings.analyzerProperties ? JSON.stringify(settings.analyzerProperties, null, 2) : "")}</textarea><br><span class="quiet">A JSON object of properties handed to the analyzers.</span></label>
<label>Rules<br><textarea name="rules">${escaped(JSON.stringify(settings.rules, null, 2))}</textarea><br><span class="quiet">Each rule's key and its <code>level</code>, on or off, and its <code>parameters</code>.</span></label>
${button("Save", "settings-save")}${button("Start the Server Again", "settings-restart")}</form>`,
  );
}

// A field of the settings panel read as JSON, `fallback` where it is empty or no JSON.
function jsonField(text, fallback) {
  try {
    return String(text ?? "").trim() ? JSON.parse(text) : fallback;
  } catch {
    return fallback;
  }
}

onPressed("settings-save", async (value, fields) => {
  await settingsChanged((settings) => {
    settings.automaticAnalysis = Boolean(fields.automaticAnalysis);
    settings.focusOnNewCode = Boolean(fields.focusOnNewCode);
    settings.output.showVerboseLogs = Boolean(fields.verbose);
    settings.analysisExcludesStandalone = String(fields.analysisExcludesStandalone ?? "");
    settings.testFilePattern = String(fields.testFilePattern ?? "");
    settings.pathToCompileCommands = String(fields.pathToCompileCommands ?? "") || null;
    settings.pathToNodeExecutable = String(fields.pathToNodeExecutable ?? "") || null;
    settings.ls.vmargs = String(fields.vmargs ?? "") || null;
    settings.analyzerProperties = jsonField(fields.analyzerProperties, null);
    settings.rules = jsonField(fields.rules, settings.rules);
  });
  await drawSettings();
});
