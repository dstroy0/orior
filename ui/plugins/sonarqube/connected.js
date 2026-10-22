// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The connections to SonarQube Server and SonarQube Cloud: each kept in the settings, its token in the
// keychain, checked by the server as it is saved; and the connections the server asks to be made as
// SonarQube opens an issue in the editor.

// The regions of SonarQube Cloud.
const REGIONS = ["EU", "US"];

// Every connection, each with whether it is a server's: `cloud` false for SonarQube Server.
function connections() {
  const kept = sonar.settings?.connectedMode?.connections ?? { sonarqube: [], sonarcloud: [] };
  return [...(kept.sonarqube ?? []).map((one) => ({ ...one, cloud: false })), ...(kept.sonarcloud ?? []).map((one) => ({ ...one, cloud: true }))];
}

// A connection by its id, null where none has it.
function connectionOf(id) {
  return connections().find((one) => one.connectionId === id) ?? null;
}

// What a connection is named by: its id, its address or its organization.
function connectionName(one) {
  return one.connectionId || (one.cloud ? one.organizationKey : one.serverUrl);
}

// The panel of every connection, how each last checked, and the binding of the tree.
async function drawConnections() {
  await settingsRead();
  const rows = connections()
    .map((one) => {
      const check = sonar.checks.get(one.connectionId);
      const state = check ? (check.success ? "Connected" : `<span class="fail">${escaped(check.reason ?? "Not connected")}</span>`) : '<span class="quiet">Not checked</span>';
      const where = one.cloud ? `SonarQube Cloud ${escaped(one.region ?? "EU")}: ${escaped(one.organizationKey)}` : `SonarQube Server: ${escaped(one.serverUrl)}`;
      return `<tr><td>${escaped(connectionName(one))}<br><span class="quiet">${where}</span></td><td>${state}</td><td>${button("Edit", "connection-edit", one.connectionId)}${button("Check", "connection-check", one.connectionId)}${button("Remove", "connection-remove", one.connectionId)}</td></tr>`;
    })
    .join("");
  const project = sonar.settings.connectedMode.project ?? {};
  const binding = project.projectKey
    ? `<p>The tree is bound to <strong>${escaped(project.projectKey)}</strong> through ${escaped(project.connectionId)}.</p><div class="row">${button("Edit Binding", "binding-edit")}${button("Remove Binding", "binding-remove")}${button("Share Configuration", "binding-share")}</div>`
    : `<p class="quiet">The tree is bound to no project.</p><div class="row">${button("Add Binding", "binding-edit")}${button("Bind Automatically", "binding-auto")}</div>`;
  const newCode = [...sonar.newCode.values()][0];
  await shown(
    "connections",
    `<h1>Connections</h1><div class="row">${button("Connect to SonarQube Server", "connection-new", "server")}${button("Connect to SonarQube Cloud", "connection-new", "cloud")}</div>${rows ? `<table>${rows}</table>` : '<p class="quiet">No connections.</p>'}<h2>Binding</h2>${binding}${newCode ? `<h2>New code</h2><p>${escaped(newCode.newCodeDefinitionOrMessage)}${newCode.isSupported ? "" : ' <span class="quiet">(not supported)</span>'}</p>` : ""}`,
  );
}

// The form a connection is made or edited with: a server's address, or a cloud organization and its
// region, its name, its token, and whether it notifies; filled in from `values`, a form's fields, where
// they are given.
async function drawConnection(cloud, id, message, values) {
  const one = values ? connectionFromFields(values) : id ? connectionOf(id) : null;
  const token = values ? String(values.token ?? "") : one ? (await orior.secrets.read(tokenKey(one)).catch(() => null)) ?? "" : "";
  const where = cloud
    ? `<label>Organization key<br><input name="organizationKey" value="${escaped(one?.organizationKey ?? "")}" size="40"></label><label>Region<br><select name="region">${REGIONS.map((region) => `<option${(one?.region ?? "EU") === region ? " selected" : ""}>${region}</option>`).join("")}</select></label>${button("List My Organizations", "connection-organizations")}`
    : `<label>Server URL<br><input name="serverUrl" value="${escaped(one?.serverUrl ?? "http://localhost:9000")}" size="50"></label>`;
  await shown(
    "connection",
    `<h1>${one ? "Edit" : "New"} ${cloud ? "SonarQube Cloud" : "SonarQube Server"} connection</h1>${message ? `<p class="fail">${escaped(message)}</p>` : ""}<form>
<input type="hidden" name="cloud" value="${cloud ? "1" : ""}"><input type="hidden" name="previous" value="${escaped(id ?? "")}">
${where}
<label>Token<br><input name="token" type="password" value="${escaped(token)}" size="50"></label>${button("Generate Token", "connection-token")}
<label>Connection name<br><input name="connectionId" value="${escaped(one?.connectionId ?? "")}" size="40"></label>
<label><input type="checkbox" name="notifications"${one?.disableNotifications ? "" : " checked"}> Receive notifications from ${cloud ? "SonarQube Cloud" : "SonarQube Server"}</label>
${button("Save Connection", "connection-save")}</form>`,
  );
}

// The connection a form holds, as the settings keep it.
function connectionFromFields(fields) {
  const cloud = Boolean(fields.cloud);
  const base = { connectionId: String(fields.connectionId ?? "").trim(), disableNotifications: !fields.notifications };
  if (cloud) {
    const organizationKey = String(fields.organizationKey ?? "").trim();
    return { ...base, connectionId: base.connectionId || organizationKey, organizationKey, region: REGIONS.includes(fields.region) ? fields.region : "EU" };
  }
  const serverUrl = String(fields.serverUrl ?? "").trim().replace(/\/+$/, "");
  return { ...base, connectionId: base.connectionId || serverUrl, serverUrl };
}

// A connection saved: checked by the server with its token first, kept in the settings, its token in
// the keychain, and the server told of a token changed.
async function connectionSaved(one, token, previous) {
  if (!(await started())) {
    return "The server did not start";
  }
  const cloud = one.organizationKey !== undefined;
  const params = cloud ? { token, organization: one.organizationKey, region: one.region } : { token, serverUrl: one.serverUrl };
  const check = await orior.server.request("sonarlint/checkConnection", params).catch((error) => ({ success: false, reason: error.message }));
  if (!check?.success) {
    return check?.reason ?? "The connection could not be checked";
  }
  await orior.secrets.write(tokenKey(one), token);
  await settingsChanged((settings) => {
    const kept = settings.connectedMode.connections;
    kept.sonarqube = (kept.sonarqube ?? []).filter((other) => other.connectionId !== previous && other.connectionId !== one.connectionId);
    kept.sonarcloud = (kept.sonarcloud ?? []).filter((other) => other.connectionId !== previous && other.connectionId !== one.connectionId);
    (cloud ? kept.sonarcloud : kept.sonarqube).push(one);
  });
  await orior.server.notify("sonarlint/onTokenUpdate", { connectionId: one.connectionId, token });
  sonar.checks.set(one.connectionId, { connectionId: one.connectionId, success: true });
  return null;
}

onPressed("connection-new", (value) => drawConnection(value === "cloud", null));

onPressed("connection-edit", (id) => {
  const one = connectionOf(id);
  return one ? drawConnection(one.cloud, id) : drawConnections();
});

onPressed("connection-save", async (value, fields) => {
  const one = connectionFromFields(fields);
  const token = String(fields.token ?? "");
  if (!(one.serverUrl || one.organizationKey) || !token) {
    await drawConnection(Boolean(fields.cloud), fields.previous || null, "A connection needs its address or organization, and a token", fields);
    return;
  }
  const failed = await connectionSaved(one, token, fields.previous || null);
  if (failed) {
    await drawConnection(Boolean(fields.cloud), fields.previous || null, failed, fields);
    return;
  }
  await orior.panels.hide("connection");
  // a connection the shared configuration suggested binds the tree to the project it names
  const suggested = sonar.suggested;
  if (suggested && (suggested.serverUrl === one.serverUrl || suggested.organization === one.organizationKey)) {
    sonar.suggested = null;
    await boundTo(one.connectionId, suggested.projectKey);
    return;
  }
  await drawConnections();
});

onPressed("connection-token", async (value, fields) => {
  if (!(await started())) {
    return;
  }
  const base = fields.cloud ? `https://${fields.region === "US" ? "sonarqube.us" : "sonarcloud.io"}` : String(fields.serverUrl ?? "").trim().replace(/\/+$/, "");
  const answer = await orior.server.request("sonarlint/generateToken", { baseServerUrl: base }).catch((error) => {
    logged(`Token: ${error.message}`);
    return null;
  });
  if (answer?.token) {
    await drawConnection(Boolean(fields.cloud), fields.previous || null, null, { ...fields, token: answer.token });
  }
});

onPressed("connection-organizations", async (value, fields) => {
  const token = String(fields.token ?? "");
  const found = token ? await orior.server.request("sonarlint/listUserOrganizations", { token, region: fields.region ?? "EU" }).catch(() => []) : [];
  const rows = (found ?? []).map((one) => `<li><code>${escaped(one.key)}</code> ${escaped(one.name)} <span class="quiet">${escaped(one.description ?? "")}</span></li>`).join("");
  await shown("ask", rows ? `<h2>Your organizations</h2><ul>${rows}</ul>` : "<p>No organizations were found for this token.</p>");
});

onPressed("connection-check", async (id) => {
  if (await started()) {
    const check = await orior.server.request("sonarlint/checkConnection", { connectionId: id }).catch((error) => ({ connectionId: id, success: false, reason: error.message }));
    sonar.checks.set(id, check ?? { connectionId: id, success: false });
  }
  await drawConnections();
});

onPressed("connection-remove", async (id) => {
  const one = connectionOf(id);
  if (!one || (await chosen(`Remove the connection ${connectionName(one)}?`, ["Remove"])) !== 0) {
    return;
  }
  await orior.secrets.write(tokenKey(one), null);
  await settingsChanged((settings) => {
    const kept = settings.connectedMode.connections;
    kept.sonarqube = (kept.sonarqube ?? []).filter((other) => other.connectionId !== id);
    kept.sonarcloud = (kept.sonarcloud ?? []).filter((other) => other.connectionId !== id);
    if (settings.connectedMode.project?.connectionId === id) {
      settings.connectedMode.project = {};
    }
  });
  sonar.checks.delete(id);
  await drawConnections();
});

orior.server.on("sonarlint/reportConnectionCheckResult", (check) => {
  sonar.checks.set(check.connectionId, check);
});

orior.server.on("sonarlint/notifyInvalidToken", async ({ connectionId }) => {
  sonar.checks.set(connectionId, { connectionId, success: false, reason: "The token is invalid" });
  if ((await chosen(`Connection to '${connectionId}' failed: Please verify your token.`, ["Edit Connection"])) === 0) {
    const one = connectionOf(connectionId);
    await drawConnection(one?.cloud ?? false, connectionId);
  }
});

orior.server.on("sonarlint/removeBindingsForDeletedConnections", (ids) =>
  settingsChanged((settings) => {
    if ((Array.isArray(ids) ? ids : []).includes(settings.connectedMode.project?.connectionId)) {
      settings.connectedMode.project = {};
    }
  }),
);

orior.server.on("sonarlint/openConnectionSettings", (cloud) => drawConnection(Boolean(cloud), null));

// A connection the server asks to be made, as SonarQube opens an issue in the editor: made where the
// reader trusts it, its id the answer.
orior.server.on("sonarlint/assistCreatingConnection", async ({ isSonarCloud, serverUrlOrOrganisationKey, token, region }) => {
  const kind = isSonarCloud ? "SonarQube Cloud organization" : "SonarQube Server instance";
  const warning = isSonarCloud ? "" : " Letting SonarQube for IDE connect to an untrusted SonarQube Server instance is potentially dangerous. If you don't trust this server, we recommend canceling this action and manually setting up Connected Mode.";
  const answer = await chosen(`Do you trust this ${kind}? Ensure that the requesting ${isSonarCloud ? "organization" : "server URL"} '${serverUrlOrOrganisationKey}' matches your ${kind}.${warning}`, [`Connect to this ${kind}`]);
  if (answer !== 0) {
    throw new Error("Automatic Connection setup was cancelled by the user");
  }
  const one = isSonarCloud
    ? { connectionId: serverUrlOrOrganisationKey, disableNotifications: false, organizationKey: serverUrlOrOrganisationKey, region: REGIONS[region] ?? region ?? "EU" }
    : { connectionId: serverUrlOrOrganisationKey, disableNotifications: false, serverUrl: serverUrlOrOrganisationKey };
  const kept = token || (await orior.secrets.read(tokenKey(one)).catch(() => null));
  if (!kept) {
    await drawConnection(isSonarCloud, null, "Enter a token for this connection");
    throw new Error("Automatic Connection setup cancelled; User will manually enter token");
  }
  const failed = await connectionSaved(one, kept, null);
  if (failed) {
    throw new Error(failed);
  }
  return { newConnectionId: one.connectionId };
});
