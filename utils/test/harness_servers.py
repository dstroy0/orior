# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The servers the window's suite runs against, each a Docker container of the harness's own as window_servers.tsv
names them, reached on this machine's loopback alone at a port Docker chooses: the databases, the print server
ui/test/servers/cups builds, and the ssh server ui/test/servers/ssh builds, which a database's tunnel goes through with
the harness's own key and config. A run starts only the servers the tests it runs reach, read from their text, and
takes them away as it ends. What each server is and where it listens goes to the tests as ORIOR_* settings. An HTTP
server of the harness's own process answers the tests of requests."""

import http.server
import json
import os
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

ROOT = findroot.root()
SERVERS_TABLE = findroot.at("utils", "test", "window_servers.tsv")
SERVER_IMAGES = findroot.at("ui", "test", "servers")
SSH_FOLDER = findroot.at("build", "ui_test", "ssh")
# the one pixel the image path answers with, a PNG
ONE_PIXEL = bytes.fromhex(
    "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000d49444154789c63e0af0afe0f00035401dcb5b5f75e0000000049454e44ae426082"
)


def rows():
    """Each server of the table: name, image, ports (ints), env (pairs), args (words), reached (texts)."""
    found = []
    with open(SERVERS_TABLE, encoding="utf-8") as table:
        head = table.readline().rstrip("\n").split("\t")
        for line in table:
            if not line.strip():
                continue
            cells = line.rstrip("\n").split("\t")
            row = dict(zip(head, cells + [""] * (len(head) - len(cells))))
            row["ports"] = [int(port) for port in row["ports"].split(",") if port]
            row["env"] = [pair.split("=", 1) for pair in row["env"].split(",") if pair]
            row["args"] = row["args"].split() if row["args"] else []
            row["reached"] = [word for word in row["reached"].split(",") if word]
            found.append(row)
    return found


def password_of(row):
    """The tests' password for a database server, as its environment sets it."""
    return next((value for key, value in row["env"] if key.endswith("PASSWORD")), "")


def reached_in(files):
    """The servers the files reach, each named where any file's text holds a text it is reached by, in the table's
    order; a file that cannot be read reaches none."""
    texts = []
    for file in files:
        try:
            with open(file, encoding="utf-8", errors="replace") as source:
                texts.append(source.read())
        except OSError:
            continue
    return [row["name"] for row in rows() if any(word in text for text in texts for word in row["reached"])]


def names():
    return [row["name"] for row in rows()]


def docker(*args):
    """docker's standard output, trimmed; a failure raises with its standard error."""
    done = subprocess.run(["docker", *args], stdin=subprocess.DEVNULL, capture_output=True, text=True, errors="replace")
    if done.returncode != 0:
        raise RuntimeError(done.stderr.strip())
    return done.stdout.strip()


def docker_runs():
    try:
        return bool(docker("info", "--format", "{{.ServerVersion}}"))
    except (RuntimeError, OSError):
        return False


def port_of(said):
    """The first port docker port gives, of lines as 127.0.0.1:55001 or [::]:55001."""
    for line in said.splitlines():
        try:
            return int(line.rsplit(":", 1)[1].strip())
        except (IndexError, ValueError):
            continue
    return None


def published(name, port):
    try:
        return port_of(docker("port", name, "%d/tcp" % port))
    except RuntimeError:
        return None


def reachable(port):
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=0.3):
            return True
    except OSError:
        return False


def wait_for(limit, ready):
    until = time.monotonic() + limit
    while time.monotonic() < until:
        if ready():
            return True
        time.sleep(0.5)
    return False


def bring_up(row):
    """The container run, its image built or pulled where this machine lacks it; one running with a port not reached
    from this machine is made again."""
    running = bool(docker("ps", "-q", "-f", "name=^%s$" % row["name"]))
    if running and all(published(row["name"], port) is not None for port in row["ports"]):
        return
    subprocess.run(["docker", "rm", "-f", row["name"]], stdin=subprocess.DEVNULL, capture_output=True)
    try:
        docker("image", "inspect", row["image"])
    except RuntimeError:
        own = row["image"].removeprefix("orior-test-")
        if own != row["image"]:
            docker("build", "-t", row["image"], os.path.join(SERVER_IMAGES, own))
        else:
            docker("pull", row["image"])
    words = ["run", "-d", "--name", row["name"]]
    for port in row["ports"]:
        words += ["-p", "127.0.0.1::%d" % port]
    for key, value in row["env"]:
        words += ["-e", "%s=%s" % (key, value)]
    docker(*words, row["image"], *row["args"])


def exec_ok(name, *words):
    try:
        docker("exec", name, *words)
        return True
    except RuntimeError:
        return False


class Up:
    """The servers up: what the tests are told of them, what was said of each that could not come up, and the
    containers this run made, for taking them away as it ends."""

    def __init__(self):
        self.env = []
        self.said = []
        self.started = []


def start(wanted):
    """The servers `wanted` names started, as reached_in gives them, and none where it names none."""
    up = Up()
    if not wanted:
        return up
    if not docker_runs():
        up.said.append("Docker does not answer: the database and printing tests run without their servers")
        return up
    for row in rows():
        if row["name"] not in wanted:
            continue
        try:
            bring_up(row)
        except (RuntimeError, OSError) as error:
            up.said.append("%s did not come up: %s" % (row["name"], error))
            continue
        up.started.append(row["name"])
        port = published(row["name"], row["ports"][0])
        if port is None or not wait_for(60, lambda: reachable(port)):
            up.said.append("%s does not answer on its port" % row["name"])
            continue
        address = "127.0.0.1:%d" % port
        name = row["name"]
        password = password_of(row)
        if name.endswith("-postgres"):
            ready = wait_for(90, lambda: exec_ok(name, "pg_isready", "-U", "postgres"))
            # the roles the tests sign in as by md5 and in the clear, ahead of the image's lines
            lines = "host all orior_md5 all md5\\nhost all orior_plain all password"
            hba = "f=\"$(psql -U postgres -Atc 'show hba_file')\"; grep -q orior_md5 \"$f\" || sed -i '1i %s' \"$f\"; psql -U postgres -c 'select pg_reload_conf()' >/dev/null" % lines
            if not ready or not exec_ok(name, "sh", "-c", hba):
                up.said.append("PostgreSQL did not take the test's sign-in lines")
                continue
            up.env += [("ORIOR_PG_ADDRESS", address), ("ORIOR_PG_PASSWORD", password)]
        elif name.endswith("-mysql") or name.endswith("-mariadb"):
            client = "mysql" if name.endswith("-mysql") else "mariadb"
            if not wait_for(120, lambda: exec_ok(name, client, "-uroot", "-p" + password, "-e", "SELECT 1")):
                up.said.append("%s did not take sign-ins" % name)
                continue
            prefix = "ORIOR_MYSQL" if client == "mysql" else "ORIOR_MARIADB"
            up.env += [(prefix + "_ADDRESS", address), (prefix + "_PASSWORD", password)]
        elif name.endswith("-cups"):
            ready = wait_for(60, lambda: "printers ready" in logs_of(name) or exec_ok(name, "lpstat", "-p", "office"))
            printers = [published(name, port) for port in row["ports"][1:]]
            if not ready or any(one is None for one in printers):
                up.said.append("the print server's printers did not come up")
                continue
            up.env.append(("ORIOR_CUPS", address))
            for kind, printer in zip(("PDF", "PWG", "URF"), printers):
                up.env.append(("ORIOR_IPP_" + kind, "ipp://127.0.0.1:%d/ipp/print" % printer))
            up.env.append(("ORIOR_CUPS_CONTAINER", name))
        elif name.endswith("-ssh"):
            try:
                config = ssh_ready(name, port)
                up.env += [("ORIOR_TEST_SSH", 'ssh -F "%s"' % config), ("ORIOR_TEST_SSH_HOST", name), ("ORIOR_TEST_SSH_TO", "host.docker.internal")]
            except (RuntimeError, OSError) as error:
                up.said.append("the ssh server: %s" % error)
    return up


def logs_of(name):
    try:
        return docker("logs", name)
    except RuntimeError:
        return ""


def slashed(path):
    """ssh reads a path of its config with forward slashes on every system."""
    return path.replace("\\", "/")


def ssh_ready(name, port):
    """The ssh server readied: the harness's own key made where it is not, put on the server for the user orior, and a
    config of the harness's own that reaches the server at `port` with that key and a list of known hosts made new
    each run. The config's path."""
    os.makedirs(SSH_FOLDER, exist_ok=True)
    key = os.path.join(SSH_FOLDER, "id_ed25519")
    if not os.path.exists(key):
        made = subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-C", "orior-test", "-f", key], stdin=subprocess.DEVNULL, capture_output=True, text=True)
        if made.returncode != 0:
            raise RuntimeError("ssh-keygen: " + made.stderr.strip())
    # ssh takes a private key only where no one but its owner reads it; on Windows a file takes its folder's rights,
    # and the key's are cut to the user's own
    if os.name == "nt":
        user = os.environ.get("USERNAME", "")
        cut = subprocess.run(["icacls", key, "/inheritance:r", "/grant:r", user + ":F"], stdin=subprocess.DEVNULL, capture_output=True, text=True)
        if cut.returncode != 0:
            raise RuntimeError("icacls: " + cut.stdout.strip())
    with open(key + ".pub", encoding="utf-8") as public:
        public_key = public.read().strip()
    script = "printf '%s\\n' \"$1\" > /home/orior/.ssh/authorized_keys && chown orior:orior /home/orior/.ssh/authorized_keys && chmod 600 /home/orior/.ssh/authorized_keys"
    docker("exec", name, "sh", "-c", script, "sh", public_key)
    known = os.path.join(SSH_FOLDER, "known_hosts")
    if os.path.exists(known):
        os.remove(known)
    config = os.path.join(SSH_FOLDER, "config")
    with open(config, "w", encoding="utf-8") as out:
        out.write(config_text("127.0.0.1", port, key, known))
    return slashed(config)


def config_text(host, port, key, known):
    return "Host orior-test-ssh\n  HostName %s\n  Port %d\n  User orior\n  IdentityFile \"%s\"\n  IdentitiesOnly yes\n  UserKnownHostsFile \"%s\"\n  StrictHostKeyChecking accept-new\n  BatchMode yes\n" % (host, port, slashed(key), slashed(known))


def ssh_port(config_path):
    """The port the config reaches the ssh server on."""
    try:
        with open(config_path, encoding="utf-8") as config:
            for line in config:
                if line.strip().startswith("Port "):
                    return int(line.strip()[5:])
    except (OSError, ValueError):
        pass
    return None


def ssh_inside(keys, into, port):
    """The ssh server as a container reaches it: the key of the folder `keys` copied into `into` with rights for its
    owner alone, and a config there reaching the server at `port` on this machine through host.docker.internal. The
    settings the tests are told."""
    os.makedirs(into, exist_ok=True)
    key = os.path.join(into, "id_ed25519")
    with open(os.path.join(keys, "id_ed25519"), "rb") as source, open(key, "wb") as copy:
        copy.write(source.read())
    if os.name != "nt":
        os.chmod(key, 0o600)
    config = os.path.join(into, "config")
    with open(config, "w", encoding="utf-8") as out:
        out.write(config_text("host.docker.internal", port, key, os.path.join(into, "known_hosts")))
    return [("ORIOR_TEST_SSH", 'ssh -F "%s"' % config), ("ORIOR_TEST_SSH_HOST", "orior-test-ssh"), ("ORIOR_TEST_SSH_TO", "host.docker.internal")]


def seen_from_a_container(env):
    """The servers' settings as a container sees them: an address on this machine's loopback reached at Docker's name
    for this machine, and the harness's ssh, whose config names this machine's paths, left out."""
    return [(key, value.replace("127.0.0.1", "host.docker.internal")) for key, value in env if not key.startswith("ORIOR_TEST_SSH") and key != "ORIOR_CUPS_CONTAINER"]


class Echo(http.server.BaseHTTPRequestHandler):
    """The tests' HTTP server: /echo answers with the request as JSON, /count with how many times it was asked,
    /moved with a redirect to /echo, /image with a PNG of one pixel, and any other path with 404."""

    asked = 0

    def handle_any(self):
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length) if length else b""
        path = self.path.split("?")[0]
        if path == "/echo":
            headers = {name.lower(): value for name, value in self.headers.items()}
            content = json.dumps({"method": self.command, "path": self.path, "headers": headers, "body": body.decode(errors="replace")}).encode()
            self.answer(200, "application/json", content)
        elif path == "/count":
            Echo.asked += 1
            self.answer(200, "text/plain", str(Echo.asked).encode())
        elif path == "/moved":
            self.answer(302, "text/plain", b"", location="/echo")
        elif path == "/image":
            self.answer(200, "image/png", ONE_PIXEL)
        else:
            self.answer(404, "text/plain", b"no such path")

    def answer(self, status, kind, content, location=None):
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(content)))
        if location:
            self.send_header("Location", location)
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(content)

    do_GET = do_POST = do_PUT = do_DELETE = do_PATCH = do_HEAD = handle_any

    def log_message(self, *_):
        return


def echo_server():
    """The tests' HTTP server started on the loopback in a thread of its own; its address."""
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Echo)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return "127.0.0.1:%d" % server.server_address[1]


def stop(started):
    """The test servers `started` names taken away."""
    for name in started:
        subprocess.run(["docker", "rm", "-f", name], stdin=subprocess.DEVNULL, capture_output=True)
