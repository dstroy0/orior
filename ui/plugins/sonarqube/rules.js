// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The rules: every rule of every language the server holds, each turned on or off in the settings
// against its default, found by its key, and described as the server describes it.

// The style a rule's description is drawn with: its tabs and contexts chosen without a script, and the
// compliant and noncompliant code told apart.
const RULE_STYLE = `<style>
.tabs,.contexts{display:flex;flex-wrap:wrap}.tabs>input,.contexts>input{position:absolute;opacity:0}
.tabs>label,.contexts>label{padding:4px 10px;cursor:pointer;border-bottom:2px solid transparent}
.tabs>input:checked+label,.contexts>input:checked+label{border-bottom-color:currentColor;font-weight:600}
.tabs>section,.contexts>section{display:none;order:1;width:100%}
.tabs>input:checked+label+section,.contexts>input:checked+label+section{display:block}
.capsule{display:inline-block;border:1px solid;border-radius:10px;padding:0 8px;margin:2px}
pre[data-diff-type=noncompliant]{border-left:3px solid #c74e39}pre[data-diff-type=compliant]{border-left:3px solid #4e9a50}
.banner{border:1px solid;border-radius:4px;padding:6px 10px;margin:8px 0}
</style>`;

// A word as a capitalized label.
function capitalized(word) {
  const text = String(word ?? "").toLocaleLowerCase("en-us").replace(/_/g, " ");
  return text.charAt(0).toUpperCase() + text.slice(1);
}

// The rule's taxonomy: its clean code attribute and impacts, or its type and severity.
function taxonomy(rule) {
  const details = rule.severityDetails ?? rule;
  const impacts = Object.entries(details.impacts ?? {});
  if (impacts.length) {
    const attribute = `<span class="capsule" title="Coding attributes are characteristics that, when followed, ensure strong code quality and security.">${escaped(details.cleanCodeAttributeCategory)} issue | ${escaped(details.cleanCodeAttribute)}</span>`;
    const each = impacts.map(
      ([quality, severity]) =>
        `<span class="capsule" title="Issues found for this rule will have a ${escaped(severity.toLowerCase())} impact on the ${escaped(quality.toLowerCase())} of your software.">${escaped(capitalized(quality))}: ${escaped(capitalized(severity))}</span>`,
    );
    return `<div>${attribute}${each.join("")}</div>`;
  }
  return details.type ? `<div><span class="capsule">${escaped(capitalized(details.type))} | ${escaped(capitalized(details.severity))}</span></div>` : "";
}

// A tab's contexts, each a choice, the default chosen first.
function contexts(tab, at) {
  const chosenKey = tab.defaultContextKey || "others";
  const title = tab.title.endsWith("?") ? tab.title.slice(0, -1) : tab.title;
  return `<div class="contexts">${tab.ruleDescriptionTabContextual
    .map(
      (one, inner) =>
        `<input type="radio" name="contexts-${at}" id="context-${at}-${inner}"${String(one.contextKey) === chosenKey ? " checked" : ""}><label for="context-${at}-${inner}">${escaped(one.displayName)}</label><section>${one.contextKey === "others" ? "" : `<h4>${escaped(title)} in ${escaped(one.displayName)}</h4>`}<div>${one.htmlContent}</div></section>`,
    )
    .join("")}</div>`;
}

// The rule's description: its tabs, each a choice, or the one description where it has none.
function described(rule) {
  const tabs = rule.htmlDescriptionTabs ?? [];
  if (!tabs.length) {
    return `<div>${rule.htmlDescription ?? ""}</div>`;
  }
  return `<main class="tabs">${tabs
    .map(
      (tab, at) =>
        `<input type="radio" name="tabs" id="tab-${at}"${at === 0 ? " checked" : ""}><label for="tab-${at}">${escaped(tab.title)}</label><section>${tab.hasContextualInformation ? contexts(tab, at) : `<div>${tab.ruleDescriptionTabNonContextual?.htmlContent ?? ""}</div>`}</section>`,
    )
    .join("")}</main>`;
}

// The rule's parameters and what the settings set each to.
function parameters(rule) {
  if (!rule.parameters?.length) {
    return "";
  }
  const set = sonar.settings?.rules?.[rule.key]?.parameters ?? {};
  const rows = rule.parameters
    .map((one) => `<tr><td><code>${escaped(one.name)}</code><br><span class="quiet">${escaped(one.description)}</span></td><td>${escaped(set[one.name] ?? one.defaultValue ?? "")}${one.name in set ? "" : ' <span class="quiet">(default)</span>'}</td></tr>`)
    .join("");
  return `<h2>Parameters</h2><p class="quiet">Following parameter values can be set in the rules of the SonarQube settings. In connected mode, server side configuration overrides local settings.</p><table>${rows}</table>`;
}

// A rule described in its panel, as the server describes it.
async function drawRule(rule) {
  const taint = rule.isTaint
    ? '<p class="banner">This injection vulnerability was detected by the latest SonarQube (Server, Cloud) analysis. SonarQube for IDE fetches and reports it in your local code to help you investigate it and fix it, but cannot tell you whether you successfully fixed it. To verify your fix, please ensure the code containing your fix is analyzed by SonarQube (Server, Cloud).</p>'
    : "";
  const hotspot = rule.isHotspot || /hotspot/i.test(rule.severityDetails?.type ?? rule.type ?? "")
    ? '<p class="banner">A security hotspot highlights security-sensitive code that requires review: whether it is safe or must be fixed is for the reviewer to decide.</p>'
    : "";
  const level = ruleLevel(rule.key);
  await shown(
    "rule",
    `${RULE_STYLE}<h1>${escaped(rule.name)} (${escaped(rule.key)})</h1>${taxonomy(rule)}${taint}${hotspot}<div class="row">${button(level === "off" ? "Activate" : "Deactivate", level === "off" ? "rule-on" : "rule-off", rule.key)}</div>${described(rule)}${parameters(rule)}`,
  );
}

orior.server.on("sonarlint/showRuleDescription", (rule) => drawRule(rule));

// Whether the settings turn a rule on or off, null where they leave it at its default.
function ruleLevel(key) {
  return sonar.settings?.rules?.[key]?.level ?? null;
}

// A rule turned on or off: kept against its default where its default is known, and set outright where
// only its key is.
async function ruleToggled(key, level, activeByDefault) {
  await settingsChanged((settings) => {
    const rules = { ...settings.rules };
    if (activeByDefault === undefined || (level === "on" && !activeByDefault) || (level === "off" && activeByDefault)) {
      rules[key] = { ...(rules[key] ?? {}), level };
    } else {
      delete rules[key];
    }
    settings.rules = rules;
  });
}

// The default of a rule as last listed, undefined where it is not known.
function ruleDefault(key) {
  for (const list of Object.values(sonar.rules ?? {})) {
    const found = list.find((one) => one.key.toUpperCase() === String(key).toUpperCase());
    if (found) {
      return found.activeByDefault;
    }
  }
  return undefined;
}

// A rule deactivated from an issue's action, and the reader offered to undo it.
async function ruleDeactivated(key) {
  await ruleToggled(key, "off");
  const answer = await chosen(`Sonar rule ${key} is now disabled in your local environment`, ["Undo", "Show All Rules"]);
  if (answer === 0) {
    await ruleToggled(key, "on");
  } else if (answer === 1) {
    await drawRules(null, key);
  }
}

// The rules listed in their panel by language: all, or those on or off as `filter` names, the rule
// `key` among them shown first where one is named.
async function drawRules(filter, key) {
  if (!(await started())) {
    return;
  }
  sonar.ruleFilter = filter;
  sonar.rules = await orior.server.request("sonarlint/listAllRules", null).catch(() => sonar.rules ?? {});
  const wanted = key ? String(key).toUpperCase() : null;
  const sections = Object.keys(sonar.rules)
    .sort()
    .map((language) => {
      const rows = sonar.rules[language]
        .map((rule) => ({ rule, on: (ruleLevel(rule.key) ?? (rule.activeByDefault ? "on" : "off")) === "on" }))
        .filter(({ rule, on }) => (!wanted || rule.key.toUpperCase() === wanted) && (filter === null || (filter === "on") === on))
        .map(
          ({ rule, on }) =>
            `<tr><td>${linked(rule.key, "rule-describe", rule.key)}</td><td>${escaped(rule.name)}</td><td>${on ? "On" : "Off"}${ruleLevel(rule.key) ? "" : ' <span class="quiet">(default)</span>'}</td><td>${button(on ? "Deactivate" : "Activate", on ? "rule-off" : "rule-on", rule.key)}${ruleLevel(rule.key) ? button("Reset", "rule-reset", rule.key) : ""}</td></tr>`,
        );
      return rows.length ? `<h2>${escaped(language)}</h2><table>${rows.join("")}</table>` : "";
    })
    .join("");
  const filters = [button("All", "rules-filter", ""), button("Active", "rules-filter", "on"), button("Inactive", "rules-filter", "off")].join("");
  await shown(
    "rules",
    `<h1>Rules</h1><form class="row">${filters}<input name="key" placeholder="Rule key, as python:S1481" value="${escaped(key ?? "")}">${button("Find", "rules-find")}</form>${sections || '<p class="quiet">No rule matches.</p>'}`,
  );
}

onPressed("rules-filter", (value) => drawRules(value === "" ? null : value));

onPressed("rules-find", (value, fields) => drawRules(sonar.ruleFilter, String(fields.key ?? "").trim() || null));

onPressed("rule-on", async (key) => {
  await ruleToggled(key, "on", ruleDefault(key));
  await drawRules(sonar.ruleFilter);
});

onPressed("rule-off", async (key) => {
  await ruleToggled(key, "off", ruleDefault(key));
  await drawRules(sonar.ruleFilter);
});

onPressed("rule-reset", async (key) => {
  await settingsChanged((settings) => {
    const rules = { ...settings.rules };
    delete rules[key];
    settings.rules = rules;
  });
  await drawRules(sonar.ruleFilter);
});

// A rule of the list described: as its description is asked of the server through its command.
onPressed("rule-describe", async (key) => {
  await orior.server.request("workspace/executeCommand", { command: "SonarLint.OpenStandaloneRuleDesc", arguments: [key] }).catch((error) => logged(error.message));
});
