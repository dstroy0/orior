# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py sonarqube: the SonarQube server at 127.0.0.1:9000 given the project orior-sonarqube-test, the tree's ui/
scanned onto it, and the duplications it holds listed.

    harness.py sonarqube project        the project made and a token of its admin written to build/harness/sonarqube_token.txt
    harness.py sonarqube profile [--cloud]
                                        the project's quality profiles, each language's cognitive complexity threshold
                                        at 50, on SonarCloud's project with --cloud
    harness.py sonarqube scan           ui/ scanned through the scanner's image, the log to build/sonarqube_scan.log
    harness.py sonarqube duplications   every group of blocks found the same, to build/sonarqube_duplications.md
    harness.py sonarqube issues [--under ui/cli]
                                        the project's open issues by file, to build/sonarqube_issues.md
    harness.py sonarqube cloud          every open issue of the project on SonarCloud, to build/sonarcloud_issues.tsv

The token file is where the window's sonarqube test reads the token from."""

import base64
import collections
import functools
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
BUILD = os.path.join(ROOT, "build")
TOKEN_FILE = os.path.join(BUILD, "harness", "sonarqube_token.txt")
SERVER = "http://127.0.0.1:9000"
PROJECT = "orior-sonarqube-test"
ADMIN = "admin:admin"
SCANNER_IMAGE = "sonarsource/sonar-scanner-cli"
SCANNER_CACHE = "orior-sonar-scanner-cache"
EXCLUSIONS = "**/node_modules/**,**/target/**,**/build/**,**/dist/**,**/*.min.js,**/vendor/**,**/runs/**"
PAGE = 500
CLOUD_SERVER = "https://sonarcloud.io"
CLOUD_PROJECT = "dstroy0_orior"
CLOUD_MOST = 10000
CLOUD_SEVERITIES = ["BLOCKER", "CRITICAL", "MAJOR", "MINOR", "INFO"]


def token_read():
    with open(TOKEN_FILE, encoding="utf-8") as kept:
        return kept.read().strip()


def asked(path, method="GET", auth=None, token=None, server=SERVER, **params):
    query = urllib.parse.urlencode(params)
    request = urllib.request.Request("%s/api/%s?%s" % (server, path, query), method=method)
    if auth:
        request.add_header("Authorization", "Basic " + base64.b64encode(auth.encode()).decode())
    if token:
        request.add_header("Authorization", "Bearer " + token)
    with urllib.request.urlopen(request, timeout=60) as answer:
        body = answer.read()
    return json.loads(body) if body.strip() else {}


def cmd_project(a):
    try:
        asked("projects/create", "POST", ADMIN, project=PROJECT, name=PROJECT)
    except urllib.error.HTTPError:
        pass
    made = asked("user_tokens/generate", "POST", ADMIN, name="orior-test-%d" % int(time.time()))
    os.makedirs(os.path.dirname(TOKEN_FILE), exist_ok=True)
    with open(TOKEN_FILE, "w", encoding="utf-8", newline="\n") as out:
        out.write(made["token"] + "\n")
    print("token written")
    return 0


PROFILE = "orior"
PROFILE_FROM = "Sonar way comprehensive"
REPORT_RULES = "cxx,clangtidy,cppcheck,clangsa"
COGNITIVE_THRESHOLD = 50
CLOUD_ORGANIZATION = "dstroy0"


def cognitive_rule(ask, language, **organization):
    """The language's cognitive complexity rule on the server and the key of its threshold, or None where it has none.
    A rule's limit on a property's complexity is not its threshold."""
    found = ask("rules/search", q="Cognitive Complexity of", languages=language, f="name,params", ps=100, **organization)
    for rule in found.get("rules", []):
        limits = [param["key"] for param in rule.get("params", []) if param["key"] != "propertyThreshold"]
        if rule.get("name", "").startswith("Cognitive Complexity of") and limits:
            return rule["key"], limits[0]
    return None


def cognitive_threshold_set(ask, project, **organization):
    """In each language of the project's code, its profile `orior`, created where the server has none as a child of the
    language's default profile, with the cognitive complexity rule's threshold at COGNITIVE_THRESHOLD, and set as the
    project's. The languages come from the project's last analysis."""
    measured = ask("measures/component", component=project, metricKeys="ncloc_language_distribution")
    shares = (measured.get("component", {}).get("measures") or [{"value": ""}])[0]["value"]
    languages = [share.split("=")[0] for share in shares.split(";") if share.split("=")[0].isalnum()]
    for language in languages:
        rule = cognitive_rule(ask, language, **organization)
        if rule is None:
            continue
        listed = ask("qualityprofiles/search", language=language, **organization).get("profiles", [])
        keys = {one["name"]: one["key"] for one in listed}
        if PROFILE not in keys:
            parent = next(one["name"] for one in listed if one.get("isDefault"))
            keys[PROFILE] = ask("qualityprofiles/create", "POST", language=language, name=PROFILE, **organization)["profile"]["key"]
            ask("qualityprofiles/change_parent", "POST", language=language, qualityProfile=PROFILE, parentQualityProfile=parent, **organization)
        ask("qualityprofiles/activate_rule", "POST", key=keys[PROFILE], rule=rule[0], params="%s=%d" % (rule[1], COGNITIVE_THRESHOLD))
        ask("qualityprofiles/add_project", "POST", language=language, qualityProfile=PROFILE, project=project, **organization)
        print("%s: %s %s=%d in profile %s" % (language, rule[0], rule[1], COGNITIVE_THRESHOLD, PROFILE))


def cmd_profile(a):
    """The cxx quality profile `orior` made from the plugin's comprehensive one, with every rule of the clang-tidy,
    cppcheck and clang static analyzer report readers turned on, and set as the project's; then the cognitive
    complexity threshold of every language of the project's code. With --cloud, the threshold alone, on SonarCloud's
    project, through the token in SONAR_TOKEN."""
    if a.cloud:
        token = os.environ.get("SONAR_TOKEN", "")
        if not token:
            print("SONAR_TOKEN is not set")
            return 1
        cloud = functools.partial(asked, auth=token + ":", server=CLOUD_SERVER)
        cognitive_threshold_set(cloud, CLOUD_PROJECT, organization=CLOUD_ORGANIZATION)
        return 0
    listed = asked("qualityprofiles/search", auth=ADMIN, language="cxx").get("profiles", [])
    keys = {one["name"]: one["key"] for one in listed}
    if PROFILE not in keys:
        if PROFILE_FROM not in keys:
            print("no cxx profile %r on the server: is the cxx plugin installed?" % PROFILE_FROM)
            return 1
        keys[PROFILE] = asked("qualityprofiles/copy", "POST", ADMIN, fromKey=keys[PROFILE_FROM], toName=PROFILE)["key"]
    asked("qualityprofiles/activate_rules", "POST", ADMIN, targetKey=keys[PROFILE], repositories=REPORT_RULES)
    asked("qualityprofiles/add_project", "POST", ADMIN, language="cxx", qualityProfile=PROFILE, project=PROJECT)
    print("profile %s set for %s with the %s rules on" % (PROFILE, PROJECT, REPORT_RULES))
    cognitive_threshold_set(functools.partial(asked, auth=ADMIN), PROJECT)
    return 0


def cmd_scan(a):
    log = os.path.join(BUILD, "sonarqube_scan.log")
    top = ROOT.replace("\\", "/")
    command = [
        "docker", "run", "--rm", "--add-host", "host.docker.internal:host-gateway",
        "-e", "SONAR_HOST_URL=http://host.docker.internal:9000", "-e", "SONAR_TOKEN=" + token_read(),
        "-v", top + ":/usr/src", "-v", SCANNER_CACHE + ":/opt/sonar-scanner/.sonar/cache",
        SCANNER_IMAGE,
        "-Dsonar.projectKey=" + PROJECT, "-Dsonar.projectName=" + PROJECT,
        "-Dsonar.sources=" + ",".join(a.sources), "-Dsonar.sourceEncoding=UTF-8", "-Dsonar.scm.disabled=true",
        "-Dsonar.exclusions=" + EXCLUSIONS, "-Dsonar.cpd.exclusions=",
        # sonar-project.properties has SonarCloud's settings, the coverage report among them; the folders it names as tests overlap these sources
        "-Dsonar.tests=utils/test/src", "-Dsonar.test.inclusions=utils/test/src/**",
        # the C sources, read by the sonar-cxx plugin where the server holds it, and the reports `source tidy` wrote
        "-Dsonar.cxx.file.suffixes=.c,.h,.cu,.cuh",
        "-Dsonar.cxx.includeDirectories=src/cu/includes/mmgr/include,src/cu/includes/mmgr/src,src/cu/includes/embedded_types/include,src/cu/includes,ui/cli/src,ui/query/src",
        "-Dsonar.cxx.defines=MMGR_ENABLE_DMA 0\\nMMGR_ENABLE_EXTRAM 0\\nMMGR_ENABLE_HOST 1\\nMMGR_ALIGN_BYTES 32\\nMMGR_ENABLE_HW_MEM_CAPACITY_CB 1\\nMMGR_CARCER_OWNERS 1024u",
        "-Dsonar.cxx.clangtidy.reportPaths=build/sonarqube/clang-tidy.txt",
        "-Dsonar.cxx.cppcheck.reportPaths=build/sonarqube/cppcheck.xml",
    ]
    with open(log, "wb") as out:
        ran = subprocess.run(command, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT)
    with open(log, encoding="utf-8", errors="replace") as kept:
        lines = kept.readlines()
    if ran.returncode != 0:
        print("".join(lines[-30:]), end="")
        return 1
    telling = [line.rstrip("\n") for line in lines if re.search(r"EXECUTION|ANALYSIS SUCCESSFUL|duplicat|files indexed|ERROR", line)]
    print("\n".join(telling[-12:]))
    return 0 if processed() else 1


def processed():
    """Whether the server finished the report a scan handed it, within two minutes. The scanner ends when the report
    is handed over, and until the server has read it the issues listed are the last scan's."""
    for _ in range(120):
        state = asked("ce/component", "GET", ADMIN, component=PROJECT)
        if not state.get("queue") and state.get("current", {}).get("status") != "IN_PROGRESS":
            print("the server's reading of the scan: %s" % state.get("current", {}).get("status"))
            return True
        time.sleep(1)
    print("the server had not read the scan in two minutes")
    return False


def duplications_listed(token):
    """The project's duplications as the plugin's panel reads them: each group of blocks found the same, the largest
    first, as Markdown lines."""
    files = []
    page = 1
    while True:
        listed = asked("measures/component_tree", token=token, component=PROJECT, metricKeys="duplicated_lines,duplicated_blocks", qualifiers="FIL", ps=PAGE, p=page)
        for component in listed.get("components", []):
            measures = {one["metric"]: float(one.get("value", 0)) for one in component.get("measures", [])}
            if measures.get("duplicated_blocks", 0) > 0:
                files.append((component["key"], measures.get("duplicated_lines", 0)))
        paging = listed.get("paging", {})
        if not listed.get("components") or page * paging.get("pageSize", PAGE) >= paging.get("total", 0):
            break
        page += 1
    groups = {}
    for key, _ in files:
        shown = asked("duplications/show", token=token, key=key)
        named = shown.get("files", {})
        for duplication in shown.get("duplications", []):
            blocks = sorted((named.get(block["_ref"], {}).get("name") or named.get(block["_ref"], {}).get("key") or "?", int(block["from"]), int(block["size"])) for block in duplication.get("blocks", []))
            groups.setdefault("|".join("%s:%d+%d" % block for block in blocks), blocks)
    ordered = sorted(groups.values(), key=lambda blocks: (-blocks[0][2] * len(blocks), -blocks[0][2]))
    lines = ["# Duplications of %s" % PROJECT, ""]
    lines.append("%d duplicated lines in %d files, %d groups of blocks found the same." % (int(sum(count for _, count in files)), len(files), len(ordered)))
    lines += ["", "| Lines | Places | Blocks |", "| --- | --- | --- |"]
    for blocks in ordered:
        places = "<br>".join("%s:%d (%d lines)" % block for block in blocks)
        lines.append("| %d | %d | %s |" % (blocks[0][2], len(blocks), places))
    return lines


def issues_listed(token, under=""):
    """The project's open issues, each file's together, as Markdown lines: the file, then its issues by line with
    the rule and the message. `under` keeps the issues of files under that path alone."""
    found = []
    page = 1
    while True:
        answer = asked("issues/search", token=token, componentKeys=PROJECT, resolved="false", ps=PAGE, p=page, s="FILE_LINE", asc="true")
        for issue in answer.get("issues", []):
            path = issue.get("component", "").split(":", 1)[-1]
            if path.startswith(under):
                found.append((path, issue.get("line", 0), issue.get("rule", ""), issue.get("severity", ""), issue.get("message", "")))
        total = answer.get("paging", {}).get("total", 0)
        if page * PAGE >= total or not answer.get("issues"):
            break
        page += 1
    lines = ["# Issues of %s%s" % (PROJECT, (" under " + under) if under else ""), "", "%d open issues in %d files." % (len(found), len({path for path, *_ in found})), ""]
    last = None
    for path, line, rule, severity, message in sorted(found):
        if path != last:
            lines += ["", "## " + path, ""]
            last = path
        lines.append("- %d: %s (%s) %s" % (line, message, rule, severity))
    return lines


def cmd_issues(a):
    listed = os.path.join(BUILD, "sonarqube_issues.md")
    lines = issues_listed(token_read(), a.under)
    with open(listed, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    print("\n".join(lines[:3]))
    return 0


def cmd_duplications(a):
    listed = os.path.join(BUILD, "sonarqube_duplications.md")
    lines = duplications_listed(token_read())
    with open(listed, "w", encoding="utf-8", newline="\n") as out:
        out.write("\n".join(lines) + "\n")
    print("\n".join(lines[:3]))
    return 0


def cloud_asked(path, **params):
    """A public answer of SonarCloud about the project, which takes no token."""
    query = urllib.parse.urlencode(dict(params, **{"componentKeys" if path == "issues/search" else "projects": CLOUD_PROJECT}))
    with urllib.request.urlopen("%s/api/%s?%s" % (CLOUD_SERVER, path, query), timeout=120) as answer:
        return json.loads(answer.read().decode("utf-8"))


def cloud_issues():
    """Every open issue of the project on SonarCloud, a query of the rule at a time, since one query stops at 10000 of
    them, and a rule past that a query of the severity within it: each a row of its fields."""
    found = {}
    rules = cloud_asked("issues/search", resolved="false", ps=1, facets="rules")["facets"][0]["values"]
    for rule in rules:
        page = 1
        while True:
            answer = cloud_asked("issues/search", resolved="false", ps=PAGE, p=page, rules=rule["val"], s="FILE_LINE", asc="true")
            for issue in answer.get("issues", []):
                found[issue["key"]] = (issue.get("severity", ""), issue.get("type", ""), issue.get("rule", ""),
                                       issue.get("component", "").split(":", 1)[-1], issue.get("line", 0), issue.get("message", ""))
            if (page * PAGE >= answer.get("paging", {}).get("total", 0)) or (page * PAGE >= CLOUD_MOST) or not answer.get("issues"):
                break
            page += 1
    return sorted(found.values(), key=lambda row: (CLOUD_SEVERITIES.index(row[0]) if row[0] in CLOUD_SEVERITIES else 9, row[2], row[3], row[4]))


def cmd_cloud(a):
    rows = cloud_issues()
    listed = os.path.join(BUILD, "sonarcloud_issues.tsv")
    with open(listed, "w", encoding="utf-8", newline="\n") as out:
        for row in rows:
            out.write("\t".join(str(field).replace("\t", " ").replace("\n", " ") for field in row) + "\n")
    counted = collections.Counter((row[0], row[2]) for row in rows)
    print("%d open issues, to %s" % (len(rows), listed))
    for (severity, rule), count in sorted(counted.items(), key=lambda item: (CLOUD_SEVERITIES.index(item[0][0]) if item[0][0] in CLOUD_SEVERITIES else 9, -item[1])):
        print("%-9s %6d %s" % (severity, count, rule))
    return 0


def register(sub):
    """The sonarqube group's commands added to harness.py's subparsers."""
    group = sub.add_parser("sonarqube", help="the SonarQube server given the project, ui/ scanned onto it, its duplications listed")
    sonarqube = group.add_subparsers(dest="command", required=True)
    p = sonarqube.add_parser("project", help="the project made and its token written to build/harness/sonarqube_token.txt")
    p.set_defaults(fn=cmd_project)
    p = sonarqube.add_parser("scan", help="the sources scanned through the scanner's image, ui/ where none are named, the log to build/sonarqube_scan.log")
    p.add_argument("--sources", nargs="+", default=["ui"], help="the folders scanned, from the top of the tree")
    p.set_defaults(fn=cmd_scan)
    p = sonarqube.add_parser("profile", help="the cxx profile orior, with the clang-tidy, cppcheck and clang analyzer rules on, set for the project, and each language's cognitive complexity threshold at 50")
    p.add_argument("--cloud", action="store_true", help="the cognitive complexity threshold alone, on SonarCloud's project, through the token in SONAR_TOKEN")
    p.set_defaults(fn=cmd_profile)
    p = sonarqube.add_parser("duplications", help="every group of blocks found the same, to build/sonarqube_duplications.md")
    p.set_defaults(fn=cmd_duplications)
    p = sonarqube.add_parser("cloud", help="every open issue of the project on SonarCloud, to build/sonarcloud_issues.tsv, the counts by rule printed")
    p.set_defaults(fn=cmd_cloud)
    p = sonarqube.add_parser("issues", help="the project's open issues by file, to build/sonarqube_issues.md")
    p.add_argument("--under", default="", help="the issues of files under this path alone, such as ui/cli")
    p.set_defaults(fn=cmd_issues)
