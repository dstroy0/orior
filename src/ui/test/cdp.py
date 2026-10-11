# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# The window's debugging port, spoken to with nothing outside Python's own library: the targets
# the port lists, and a DevTools connection to one of them over a WebSocket of this file's own.
#
# A connection reads on a thread of its own. A command's answer goes to the command that waits for
# it, and every event to the handlers set for its method: a page's errors and its screen's frames
# arrive while a command runs.

import base64
import itertools
import json
import os
import queue
import socket
import struct
import threading
import urllib.parse
import urllib.request


def targets(port=9222, timeout=0.5):
    """The targets the port lists, or an empty list while nothing answers."""
    try:
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/json", timeout=timeout) as answer:
            return json.load(answer)
    except OSError:
        return []


def window_page(port=9222):
    """The DevTools address of orior's window: the page the port lists that is served over http."""
    for target in targets(port):
        if target.get("type") == "page" and target.get("url", "").startswith("http"):
            return target["webSocketDebuggerUrl"]
    raise ConnectionError(f"no orior window answers on port {port}: start it with WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port={port}")


class Connection:
    """One DevTools connection."""

    def __init__(self, url):
        parts = urllib.parse.urlparse(url)
        self.sock = socket.create_connection((parts.hostname, parts.port), timeout=30)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall(
            (
                f"GET {parts.path} HTTP/1.1\r\nHost: {parts.hostname}:{parts.port}\r\n"
                f"Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n"
            ).encode()
        )
        head = b""
        while b"\r\n\r\n" not in head:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("the page closed during the handshake")
            head += chunk
        if b" 101 " not in head.split(b"\r\n", 1)[0]:
            raise ConnectionError(head.split(b"\r\n", 1)[0].decode(errors="replace"))
        self.sock.settimeout(None)
        self.rest = head.split(b"\r\n\r\n", 1)[1]
        self.ids = itertools.count(1)
        self.waiting = {}
        self.handlers = {}
        self.sending = threading.Lock()
        self.closed = False
        threading.Thread(target=self._reading, daemon=True).start()

    def _read(self, count):
        while len(self.rest) < count:
            chunk = self.sock.recv(1 << 20)
            if not chunk:
                raise ConnectionError("the page closed")
            self.rest += chunk
        taken, self.rest = self.rest[:count], self.rest[count:]
        return taken

    def _message(self):
        parts = []
        while True:
            first, second = self._read(2)
            length = second & 0x7F
            if length == 126:
                length = struct.unpack(">H", self._read(2))[0]
            elif length == 127:
                length = struct.unpack(">Q", self._read(8))[0]
            payload = self._read(length)
            opcode = first & 0x0F
            if opcode == 0x8:
                raise ConnectionError("the page closed the connection")
            if opcode in (0x9, 0xA):
                continue
            parts.append(payload)
            if first & 0x80:
                return json.loads(b"".join(parts))

    def _reading(self):
        try:
            while True:
                message = self._message()
                if "id" in message:
                    waiter = self.waiting.pop(message["id"], None)
                    if waiter is not None:
                        waiter.put(message)
                else:
                    for handler in list(self.handlers.get(message.get("method"), [])):
                        try:
                            handler(message.get("params", {}))
                        except Exception as error:  # noqa: BLE001 - a handler's fault ends no reading
                            print(f"a handler of {message.get('method')} failed: {error}")
        except (ConnectionError, OSError):
            self.closed = True
            for waiter in list(self.waiting.values()):
                waiter.put({"error": {"message": "the page closed"}})

    def _send(self, text):
        data = text.encode()
        mask = os.urandom(4)
        header = bytes([0x81])
        if len(data) < 126:
            header += bytes([0x80 | len(data)])
        elif len(data) < 65536:
            header += bytes([0x80 | 126]) + struct.pack(">H", len(data))
        else:
            header += bytes([0x80 | 127]) + struct.pack(">Q", len(data))
        masked = bytes(byte ^ mask[at % 4] for at, byte in enumerate(data))
        with self.sending:
            self.sock.sendall(header + mask + masked)

    def call(self, method, timeout=60, **params):
        """Sends a command and gives its result."""
        ident = next(self.ids)
        waiter = queue.Queue()
        self.waiting[ident] = waiter
        self._send(json.dumps({"id": ident, "method": method, "params": params}))
        try:
            message = waiter.get(timeout=timeout)
        except queue.Empty:
            self.waiting.pop(ident, None)
            raise TimeoutError(f"{method} had no answer in {timeout} s") from None
        if "error" in message:
            raise RuntimeError(f"{method}: {message['error'].get('message')}")
        return message.get("result", {})

    def notify(self, method, **params):
        """Sends a command whose answer no one waits for, as a handler on the reading thread must."""
        self._send(json.dumps({"id": next(self.ids), "method": method, "params": params}))

    def on(self, method, handler):
        """Hands each event of `method` to `handler`, on the reading thread."""
        self.handlers.setdefault(method, []).append(handler)

    def evaluate(self, expression, timeout=60):
        """The value of an expression in the page, awaited where it is a promise."""
        result = self.call("Runtime.evaluate", timeout=timeout, expression=expression, awaitPromise=True, returnByValue=True)
        if "exceptionDetails" in result:
            details = result["exceptionDetails"]
            raise RuntimeError(details.get("exception", {}).get("description") or details.get("text", "the expression threw"))
        return result.get("result", {}).get("value")

    def close(self):
        self.closed = True
        try:
            self.sock.close()
        except OSError:
            pass
