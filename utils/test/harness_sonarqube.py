# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py sonarqube: the SonarQube server at 127.0.0.1:9000 given the project orior-sonarqube-test, the tree's ui/
scanned onto it, and the duplications it holds listed.

    harness.py sonarqube project        the project made and a token of its admin written to build/harness/sonarqube_token.txt
    harness.py sonarqube scan           ui/ scanned through the scanner's image, the log to build/sonarqube_scan.log
    harness.py sonarqube duplications   every group of blocks found the same, to build/sonarqube_duplications.md
    harness.py sonarqube issues [--under ui/cli]
                                        the project's open issues by file, to build/sonarqube_issues.md

The token file is where the window's sonarqube test reads the token from."""

import base64
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


def token_read():
    with open(TOKEN_FILE, encoding="utf-8") as kept:
        return kept.read().strip()


def asked(path, method="GET", auth=None, token=None, **params):
    query = urllib.parse.urlencode(params)
    request = urllib.request.Request("%s/api/%s?%s" % (SERVER, path, query), method=method)
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


def cmd_profile(a):
    """The cxx quality profile `orior` made from the plugin's comprehensive one, with every rule of the clang-tidy,
    cppcheck and clang static analyzer report readers turned on, and set as the project's."""
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
        "-Dsonar.sources=ui", "-Dsonar.sourceEncoding=UTF-8", "-Dsonar.scm.disabled=true",
        "-Dsonar.exclusions=" + EXCLUSIONS, "-Dsonar.cpd.exclusions=",
        # the C sources, read by the sonar-cxx plugin where the server holds it, and the reports `source tidy` wrote
        "-Dsonar.cxx.file.suffixes=.c,.h,.cu,.cuh",
        "-Dsonar.cxx.includeDirectories=src/cu/includes/mmgr/include,src/cu/includes/mmgr/src,src/cu/includes/embedded_types/include,src/cu/includes,ui/cli/src,ui/query/src",
        "-Dsonar.cxx.defines=MMGR_ENABLE_DMA 0\\nMMGR_ENABLE_EXTRAM 0\\nMMGR_ENABLE_HOST 1\\nMMGR_ALIGN_BYTES 32\\nMMGR_ENABLE_HW_MEM_CAPACITY_CB 1",
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
    return 0


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


def register(sub):
    """The sonarqube group's commands added to harness.py's subparsers."""
    group = sub.add_parser("sonarqube", help="the SonarQube server given the project, ui/ scanned onto it, its duplications listed")
    sonarqube = group.add_subparsers(dest="command", required=True)
    p = sonarqube.add_parser("project", help="the project made and its token written to build/harness/sonarqube_token.txt")
    p.set_defaults(fn=cmd_project)
    p = sonarqube.add_parser("scan", help="ui/ scanned through the scanner's image, the log to build/sonarqube_scan.log")
    p.set_defaults(fn=cmd_scan)
    p = sonarqube.add_parser("profile", help="the cxx profile orior, with the clang-tidy, cppcheck and clang analyzer rules on, set for the project")
    p.set_defaults(fn=cmd_profile)
    p = sonarqube.add_parser("duplications", help="every group of blocks found the same, to build/sonarqube_duplications.md")
    p.set_defaults(fn=cmd_duplications)
    p = sonarqube.add_parser("issues", help="the project's open issues by file, to build/sonarqube_issues.md")
    p.add_argument("--under", default="", help="the issues of files under this path alone, such as ui/cli")
    p.set_defaults(fn=cmd_issues)
