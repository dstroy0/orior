// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The duplications: the blocks of code the server found the same in two or more places of the bound
// project, as its last analysis of the project left them, each group of blocks a row, the largest
// first, each block a link to its lines in the editor. The server measures duplication only in an
// analysis of the whole project, which the scanner runs; the plugin reads what the last one found.

// The most files whose duplications are read, and the files a page of the server's listing holds.
const DUPLICATED_FILES = 400;
const PAGE = 500;

// The connection the tree is bound through, with the base address its API answers at, or null with
// why not where the tree is bound to no project of a connection.
async function boundServer() {
  await settingsRead();
  const project = sonar.settings.connectedMode.project ?? {};
  const one = project.projectKey ? connectionOf(project.connectionId) : null;
  if (!one) {
    return { failed: "The tree is bound to no project: bind it through SonarQube: Connections and Bindings first" };
  }
  const base = one.cloud ? (one.region === "US" ? "https://sonarqube.us" : "https://sonarcloud.io") : one.serverUrl;
  const token = (await orior.secrets.read(tokenKey(one)).catch(() => null)) ?? "";
  return { one, base, token, projectKey: project.projectKey, organization: one.cloud ? one.organizationKey : null };
}

// The server's answer to the API call `path` with `params`, as JSON.
async function asked(server, path, params) {
  const query = Object.entries({ ...(server.organization ? { organization: server.organization } : {}), ...params })
    .map(([name, value]) => `${encodeURIComponent(name)}=${encodeURIComponent(String(value))}`)
    .join("&");
  const answer = await orior.net.request({ url: `${server.base}/api/${path}?${query}`, method: "GET", headers: server.token ? { Authorization: `Bearer ${server.token}` } : {} });
  if (answer.status < 200 || answer.status >= 300) {
    const said = (() => {
      try {
        return (JSON.parse(answer.body).errors ?? []).map((error) => error.msg).join("; ");
      } catch {
        return "";
      }
    })();
    throw new Error(`${path}: the server answered ${answer.status}${said ? `: ${said}` : ""}`);
  }
  return JSON.parse(answer.body);
}

// Every file of the project the server holds duplicated lines in: its key, its path and its counts,
// the most duplicated first.
async function duplicatedFiles(server) {
  const files = [];
  for (let page = 1; ; page += 1) {
    const listed = await asked(server, "measures/component_tree", { component: server.projectKey, metricKeys: "duplicated_lines,duplicated_blocks", qualifiers: "FIL", ps: PAGE, p: page });
    for (const component of listed.components ?? []) {
      const measure = (key) => Number((component.measures ?? []).find((one) => one.metric === key)?.value ?? 0);
      if (measure("duplicated_blocks") > 0) {
        files.push({ key: component.key, path: component.path ?? component.name, lines: measure("duplicated_lines"), blocks: measure("duplicated_blocks") });
      }
    }
    const paging = listed.paging ?? {};
    if (!listed.components?.length || page * (paging.pageSize ?? PAGE) >= (paging.total ?? 0)) {
      break;
    }
  }
  return files.sort((one, other) => other.lines - one.lines);
}

// The groups of blocks found the same, each once however many of its files were read: each group's
// blocks, each a file's path, its first line and its count of lines.
async function duplicationGroups(server, files) {
  const groups = new Map();
  for (const file of files.slice(0, DUPLICATED_FILES)) {
    const shown = await asked(server, "duplications/show", { key: file.key });
    const named = shown.files ?? {};
    for (const duplication of shown.duplications ?? []) {
      const blocks = (duplication.blocks ?? [])
        .map((block) => ({ path: named[block._ref]?.name ?? named[block._ref]?.key ?? "?", from: Number(block.from), size: Number(block.size) }))
        .sort((one, other) => one.path.localeCompare(other.path) || one.from - other.from);
      const key = blocks.map((block) => `${block.path}:${block.from}+${block.size}`).join("|");
      if (!groups.has(key)) {
        groups.set(key, blocks);
      }
    }
  }
  return [...groups.values()].sort((one, other) => other[0].size * other.length - one[0].size * one.length || other[0].size - one[0].size);
}

// The panel of the duplications: what the server found, or why nothing was read.
async function drawDuplications() {
  const server = await boundServer();
  if (server.failed) {
    await shown("duplications", `<h1>Duplications</h1><p class="fail">${escaped(server.failed)}</p>`);
    return;
  }
  await shown("duplications", `<h1>Duplications</h1><p class="quiet">Reading the duplications of ${escaped(server.projectKey)} from ${escaped(server.base)}...</p>`);
  const files = await duplicatedFiles(server);
  const groups = await duplicationGroups(server, files);
  const lines = files.reduce((sum, file) => sum + file.lines, 0);
  const rows = groups
    .map((blocks, at) => {
      const places = blocks.map((block, which) => `${linked(`${block.path}:${block.from}`, "duplication-open", `${at}:${which}`)} <span class="quiet">${block.size} lines</span>`).join("<br>");
      return `<tr><td>${blocks[0].size}</td><td>${blocks.length}</td><td>${places}</td></tr>`;
    })
    .join("");
  sonar.duplications = groups;
  const summary = files.length
    ? `<p>${lines} duplicated lines in ${files.length} files, in ${groups.length} groups of blocks found the same, from the server's last analysis of the project.</p>`
    : `<p class="quiet">The server holds no duplicated lines for ${escaped(server.projectKey)}: the project has no analysis of its whole code yet, or none is duplicated. The scanner runs such an analysis; this plugin reads what it found.</p>`;
  await shown("duplications", `<h1>Duplications</h1><div class="row">${button("Read Again", "duplication-refresh")}</div>${summary}${rows ? `<table><tr><th>Lines</th><th>Places</th><th>Blocks</th></tr>${rows}</table>` : ""}`);
}

onPressed("duplication-refresh", () => drawDuplications());

onPressed("duplication-open", async (value) => {
  const [group, which] = String(value).split(":").map(Number);
  const block = sonar.duplications?.[group]?.[which];
  if (block) {
    await orior.open(block.path, block.from, 1);
  }
});
