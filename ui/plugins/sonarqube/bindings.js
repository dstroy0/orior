// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The binding of the tree to a project of a connection: chosen from the connection's projects, found
// from the suggestions the server makes, and shared with the team as a file of the tree.

// The folder URI the tree is named by, as the server first names it.
async function folderUri() {
  return sonar.folder;
}

// The tree bound to the project `projectKey` of the connection `connectionId`.
async function boundTo(connectionId, projectKey) {
  await settingsChanged((settings) => {
    settings.connectedMode.project = { connectionId, projectKey };
  });
  await drawConnections();
}

// The panel a binding is chosen in: a connection, then one of its projects.
async function drawBinding(connectionId, message) {
  const all = connections();
  if (!all.length) {
    await shown("binding", `<h1>Binding</h1><p>No connection is made yet.</p>${button("Connect to SonarQube Server", "connection-new", "server")}${button("Connect to SonarQube Cloud", "connection-new", "cloud")}`);
    return;
  }
  const chosenId = connectionId ?? sonar.settings.connectedMode.project?.connectionId ?? all[0].connectionId;
  const pick = all.map((one) => button(one.connectionId === chosenId ? `${connectionName(one)} (chosen)` : connectionName(one), "binding-connection", one.connectionId)).join("");
  let projects = "";
  if (await started()) {
    const found = await orior.server.request("sonarlint/getRemoteProjectsForConnection", { connectionId: chosenId }).catch((error) => {
      message = error.message;
      return {};
    });
    projects = Object.entries(found ?? {})
      .sort(([, one], [, other]) => String(one).localeCompare(String(other)))
      .map(([key, name]) => `<tr><td>${escaped(name)}<br><span class="quiet">${escaped(key)}</span></td><td>${button("Bind", "binding-project", `${chosenId}\n${key}`)}</td></tr>`)
      .join("");
  }
  await shown(
    "binding",
    `<h1>Bind the tree to a project</h1>${message ? `<p class="fail">${escaped(message)}</p>` : ""}<div class="row">${pick}</div><form class="row"><input name="key" placeholder="Project key" size="40">${button("Bind by Key", "binding-key", chosenId)}</form>${projects ? `<table>${projects}</table>` : '<p class="quiet">No projects were found for this connection.</p>'}`,
  );
}

onPressed("binding-edit", () => drawBinding(null));

onPressed("binding-connection", (id) => drawBinding(id));

onPressed("binding-project", async (value) => {
  const [connectionId, projectKey] = String(value).split("\n");
  await boundTo(connectionId, projectKey);
  await orior.panels.hide("binding");
});

onPressed("binding-key", async (connectionId, fields) => {
  const key = String(fields.key ?? "").trim();
  if (key) {
    await boundTo(connectionId, key);
    await orior.panels.hide("binding");
  }
});

onPressed("binding-remove", async () => {
  if ((await chosen("Remove the binding of the tree?", ["Remove Binding"])) === 0) {
    await settingsChanged((settings) => {
      settings.connectedMode.project = {};
    });
    await drawConnections();
  }
});

// The bindings the server suggests offered: the one bound where the reader takes it.
async function suggestionsOffered(suggestions) {
  const all = Object.values(suggestions ?? {}).flat();
  if (!all.length) {
    await shown("ask", "<p>No binding suggestions were found for the tree.</p>");
    return;
  }
  const rows = all
    .map((one) => `<tr><td>${escaped(one.sonarProjectName || one.sonarProjectKey)}<br><span class="quiet">${escaped(one.sonarProjectKey)} | ${escaped(one.connectionId)}</span></td><td>${button("Bind", "binding-project", `${one.connectionId}\n${one.sonarProjectKey}`)}</td></tr>`)
    .join("");
  await shown("binding", `<h1>Suggested bindings</h1><table>${rows}</table>${button("Choose Manually", "binding-edit")}`);
}

orior.server.on("sonarlint/suggestBinding", ({ suggestions }) => {
  if (!bound()) {
    return suggestionsOffered(suggestions);
  }
  return null;
});

onPressed("binding-auto", async () => {
  if (!(await started())) {
    return;
  }
  const folder = await folderUri();
  const suggestions = {};
  for (const one of connections()) {
    const found = await orior.server.request("sonarlint/getBindingSuggestion", { configScopeId: folder, connectionId: one.connectionId }).catch(() => null);
    for (const [scope, list] of Object.entries(found?.suggestions ?? {})) {
      suggestions[scope] = [...(suggestions[scope] ?? []), ...list];
    }
  }
  await suggestionsOffered(suggestions);
});

// A connection the shared configuration of the tree suggests: made where the reader takes it, then
// the binding it names.
orior.server.on("sonarlint/suggestConnection", async ({ suggestionsByConfigScopeId }) => {
  const all = Object.values(suggestionsByConfigScopeId ?? {}).flat();
  for (const { connectionSuggestion } of all) {
    const cloud = connectionSuggestion.organization !== undefined && connectionSuggestion.organization !== null;
    const where = cloud ? `SonarQube Cloud organization '${connectionSuggestion.organization}'` : `SonarQube Server '${connectionSuggestion.serverUrl}'`;
    const answer = await chosen(`A Connected Mode configuration file is available to bind the tree to ${where}, project '${connectionSuggestion.projectKey}'. Do you want to use this configuration file to bind this project?`, ["Use Configuration"]);
    if (answer === 0) {
      sonar.suggested = connectionSuggestion;
      await drawConnection(cloud, null, "Enter a token to make the suggested connection; the tree is bound as it is saved", {
        cloud: cloud ? "1" : "",
        serverUrl: connectionSuggestion.serverUrl ?? "",
        organizationKey: connectionSuggestion.organization ?? "",
        region: REGIONS[connectionSuggestion.region] ?? "EU",
        notifications: true,
        token: "",
      });
    }
  }
});

// A binding the server asks for, as SonarQube opens an issue of a project in the editor: the tree bound
// to it, the tree's folder the answer.
orior.server.on("sonarlint/assistBinding", async ({ connectionId, projectKey }) => {
  const project = sonar.settings?.connectedMode?.project ?? {};
  if (project.connectionId !== connectionId || project.projectKey !== projectKey) {
    const answer = await chosen(`Bind the tree to the project '${projectKey}' of the connection '${connectionId}'?`, ["Bind"]);
    if (answer !== 0) {
      throw new Error("Binding was cancelled by the user");
    }
    await boundTo(connectionId, projectKey);
  }
  return { configurationScopeId: await folderUri() };
});

onPressed("binding-share", async () => {
  if (!bound() || !(await started())) {
    return;
  }
  const folder = await folderUri();
  const answer = await orior.server.request("sonarlint/getSharedConnectedModeFileContent", { configScopeId: folder }).catch((error) => {
    logged(`Sharing Connected Mode configuration failed: ${error.message}`);
    return null;
  });
  if (!answer?.jsonFileContent) {
    await shown("ask", '<p class="fail">Failed to create SonarQube for IDE Connected Mode configuration file.</p>');
    return;
  }
  const solutions = (await orior.tree.files().catch(() => [])).filter((path) => path.endsWith(".sln")).map((path) => path.split("/").pop().replace(/\.sln$/, ""));
  const name = solutions.length === 1 ? `${solutions[0]}.json` : "connectedMode.json";
  if ((await chosen(`Share the Connected Mode configuration as .sonarlint/${name}, to be committed with the tree?`, ["Share"])) !== 0) {
    return;
  }
  await orior.files.write(`.sonarlint/${name}`, answer.jsonFileContent);
  await shown("ask", "<p>SonarQube for IDE Connected Mode configuration file was created.</p>");
});

orior.server.on("sonarlint/submitNewCodeDefinition", (definition) => {
  sonar.newCode.set(definition.folderUri, definition);
});

orior.server.on("sonarlint/setReferenceBranchNameForFolder", ({ folderUri: folder, branchName }) => {
  sonar.branches.set(folder, branchName ?? null);
});
