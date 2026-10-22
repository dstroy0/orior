// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The language server: started as the first file opens, with the options SonarQube for IDE starts
// it with, and what it says of itself: its log, what it is doing, the pages it opens, and the
// questions it asks of the reader.

// The options the server starts with: who asks, the folder of the analyzers that run JavaScript and
// TypeScript, the connections and the rules as the settings hold them, and no usage statistics.
function initializationOptions() {
  const settings = sonar.settings;
  return {
    productKey: "orior",
    productName: "orior",
    productVersion: "6.1.1",
    workspaceName: "",
    firstSecretDetected: false,
    showVerboseLogs: settings.output.showVerboseLogs,
    platform: (globalThis.navigator?.userAgent ?? "").includes("Windows") ? "win32" : (globalThis.navigator?.userAgent ?? "").includes("Mac") ? "darwin" : "linux",
    architecture: "x64",
    additionalAttributes: {},
    enableNotebooks: true,
    clientNodePath: settings.pathToNodeExecutable,
    eslintBridgeServerPath: "{folder}/../../../eslint-bridge",
    connections: settings.connectedMode.connections,
    rules: settings.rules,
    focusOnNewCode: settings.focusOnNewCode,
    automaticAnalysis: settings.automaticAnalysis,
  };
}

// The server started, once, with the settings read first.
async function started() {
  if (sonar.started) {
    return true;
  }
  sonar.starting ??= (async () => {
    await settingsRead();
    try {
      await orior.server.start(initializationOptions(), machineArguments());
      sonar.started = true;
      logged("SonarQube for IDE started");
    } catch (error) {
      logged(`SonarQube for IDE did not start: ${error.message}`);
      await shown("ask", `<p class="fail">${escaped(error.message)}</p>`);
    } finally {
      sonar.starting = null;
    }
    return sonar.started;
  })();
  return sonar.starting;
}

// The server stopped and started again, to take the Java arguments the settings give.
async function restarted() {
  await orior.server.stop().catch(() => {});
  sonar.started = false;
  return started();
}

orior.on("open", ({ path }) => {
  if (path) {
    void started();
  }
});

// The kinds of message the protocol numbers, by number.
const MESSAGE_KINDS = { 1: "Error", 2: "Warning", 3: "Info", 4: "Log", 5: "Debug" };

orior.server.on("window/logMessage", ({ type, message }) => logged(`${MESSAGE_KINDS[type] ?? "Log"}: ${message}`));

// A message the server shows goes to the log, and an error opens the panel as well: the server tells
// of its connections and its status as it starts, which is the log's to hold and not the reader's to
// dismiss.
orior.server.on("window/showMessage", async ({ type, message }) => {
  logged(`${MESSAGE_KINDS[type] ?? "Info"}: ${message}`);
  if (type === 1) {
    await shown("ask", `<p class="fail">${escaped(message)}</p>`);
  }
});

// The answers waited for of questions shown, by the question's number.
const waiting = new Map();
let questions = 0;

// A question shown with an answer for each of `actions`, answered with the action pressed.
function chosen(text, actions) {
  questions += 1;
  const number = questions;
  const answered = new Promise((resolve) => waiting.set(number, resolve));
  void shown("ask", `<p>${escaped(text)}</p><div class="row">${actions.map((action, at) => button(action, "answer", `${number}:${at}`)).join("")}${button("Dismiss", "answer", `${number}:-1`)}</div>`);
  return answered;
}

onPressed("answer", async (value) => {
  const [number, at] = String(value).split(":").map(Number);
  waiting.get(number)?.(at);
  waiting.delete(number);
  await orior.panels.hide("ask");
});

orior.server.on("window/showMessageRequest", async ({ message, actions }) => {
  const titles = (actions ?? []).map((one) => one.title);
  const at = await chosen(message, titles);
  return at >= 0 ? actions[at] : null;
});

orior.server.on("window/workDoneProgress/create", () => null);

orior.server.on("sonarlint/startProgressNotification", async ({ taskId, message }) => {
  sonar.progress.set(taskId, message);
  await orior.chrome.set({ badge: `SonarQube: ${message}` });
});

orior.server.on("sonarlint/endProgressNotification", async ({ taskId }) => {
  sonar.progress.delete(taskId);
  const left = [...sonar.progress.values()].pop();
  await orior.chrome.set({ badge: left ? `SonarQube: ${left}` : "" });
});

orior.server.on("sonarlint/browseTo", (address) => orior.browse(String(address)));

orior.server.on("sonarlint/showSonarLintOutput", () => drawOutput());

orior.server.on("sonarlint/openJavaHomeSettings", () =>
  shown("ask", "<p>SonarQube for IDE runs on the Java its toolchain carries: File, Toolchains, SonarQube for IDE, names the folder it is found in.</p>"),
);

orior.server.on("sonarlint/openPathToNodeSettings", () => drawSettings());

orior.server.on("sonarlint/canShowMissingRequirementsNotification", () => (sonar.settings?.quietRequirements ? "never_again" : "full"));

orior.server.on("sonarlint/doNotShowMissingRequirementsMessageAgain", () =>
  settingsChanged((settings) => {
    settings.quietRequirements = true;
  }),
);

orior.server.on("sonarlint/showNotificationForFirstSecretsIssue", () =>
  shown("ask", "<p>SonarQube for IDE detected some secrets in one of the open files. We strongly advise you to review those secrets and ensure they are not committed into repositories.</p>"),
);

orior.server.on("sonarlint/showSoonUnsupportedVersionMessage", async ({ doNotShowAgainId, text }) => {
  const quiet = sonar.settings?.quietVersions ?? [];
  if (!quiet.includes(doNotShowAgainId) && (await chosen(text, ["Don't Show Again"])) === 0) {
    await settingsChanged((settings) => {
      settings.quietVersions = [...quiet, doNotShowAgainId];
    });
  }
});

orior.server.on("sonarlint/askSslCertificateConfirmation", async (certificate) => {
  const rows = [
    ["Issued to", certificate.issuedTo],
    ["Issued by", certificate.issuedBy],
    ["Valid from", certificate.validFrom],
    ["Valid to", certificate.validTo],
    ["SHA-1", certificate.sha1Fingerprint],
    ["SHA-256", certificate.sha256Fingerprint],
    ["Truststore", certificate.truststorePath],
  ]
    .map(([name, value]) => `${name}: ${value}`)
    .join("; ");
  return (await chosen(`SonarQube for IDE found an untrusted server certificate. ${rows}. Trust it?`, ["Trust", "Don't trust"])) === 0;
});

orior.server.on("sonarlint/getJavaConfig", () => null);

orior.server.on("sonarlint/hasJoinedIdeLabs", () => false);

orior.server.on("sonarlint/didChangePluginStatuses", ({ pluginStatuses }) => {
  sonar.statuses = pluginStatuses ?? [];
});

// The output: the server's log, its lines last first.
async function drawOutput() {
  const lines = sonar.log.slice(-1000).reverse();
  await shown("output", `<h1>SonarQube output</h1>${button("Refresh", "output-refresh")}${button("Clear", "output-clear")}<pre>${escaped(lines.join("\n"))}</pre>`);
}

onPressed("output-refresh", () => drawOutput());

onPressed("output-clear", async () => {
  sonar.log.length = 0;
  await drawOutput();
});
