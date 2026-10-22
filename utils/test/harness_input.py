# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""Real input for the window's suite: each step the page's runner asks for, sent through the debugging port as the
mouse and keyboard send it, or as WebDriver's actions where the window is driven through WebDriver. The page receives
the same events, in the same order, with the same keys and codes, as from a person. The keys are those of a US
keyboard as window_keys.tsv gives them, named as the menus write them: Ctrl+Shift+P, Alt+Left, F5, Ctrl+K Ctrl+0."""

import os
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from utils.tools import findroot  # noqa: E402

KEYS_TABLE = findroot.at("utils", "test", "window_keys.tsv")
WAIT = 10
SHIFTED_DIGITS = ")!@#$%^&*("
# how a step no WebDriver window can take begins what it says; the runner reads the words and the test is said as not
# run on this window
NOT_HERE = "not run on a WebDriver window:"


def _rows():
    with open(KEYS_TABLE, encoding="utf-8") as table:
        head = table.readline().rstrip("\n").split("\t")
        for line in table:
            if line.strip():
                cells = line.rstrip("\n").split("\t")
                yield dict(zip(head, cells + [""] * (len(head) - len(cells))))


def _unescaped(text):
    return text.encode("latin-1", "backslashreplace").decode("unicode_escape") if "\\" in text else text


MARKS = {}
NAMED = {}
MODIFIERS = {}
DRIVEN = {}
for _row in _rows():
    if _row["kind"] == "mark":
        MARKS[_row["name"]] = (_row["code"], int(_row["key_code"]), _row["shifted"])
    elif _row["kind"] == "named":
        NAMED[_row["name"].lower()] = {"key": _row["key"], "code": _row["code"], "key_code": int(_row["key_code"]), "text": _unescaped(_row["text"]) or None}
    elif _row["kind"] == "modifier":
        MODIFIERS[_row["name"].lower()] = (int(_row["shifted"]), _row["key"], _row["code"], int(_row["key_code"]))
    elif _row["kind"] == "driven":
        DRIVEN[_row["name"]] = _unescaped(_row["key"])
# each modifier's bit once, in the order the keys are pressed down: Control, Shift, Alt, Meta
MODIFIER_ORDER = []
for _bit, _key, _code, _key_code in MODIFIERS.values():
    if (_bit, _key, _code, _key_code) not in MODIFIER_ORDER:
        MODIFIER_ORDER.append((_bit, _key, _code, _key_code))
MODIFIER_ORDER.sort(key=lambda one: {"Control": 0, "Shift": 1, "Alt": 2, "Meta": 3}[one[1]])
CTRL, SHIFT, ALT, META = (next(bit for bit, key, _, _ in MODIFIER_ORDER if key == name) for name in ("Control", "Shift", "Alt", "Meta"))


def typed(one, shift):
    """The key that types `one` on a US keyboard, with Shift down where `shift` or where `one` needs it; None where no
    key types it."""
    if one.isascii() and one.isalpha():
        upper = one.upper()
        shown = upper if shift or one.isupper() else one.lower()
        return {"key": shown, "code": "Key" + upper, "key_code": ord(upper), "text": shown}
    if one.isdigit() and one.isascii():
        shown = SHIFTED_DIGITS[int(one)] if shift else one
        return {"key": shown, "code": "Digit" + one, "key_code": ord(one), "text": shown}
    if one in SHIFTED_DIGITS:
        digit = str(SHIFTED_DIGITS.index(one))
        return {"key": one, "code": "Digit" + digit, "key_code": ord(digit), "text": one}
    if one == " ":
        return key("Space", shift)
    for plain, (code, key_code, shifted) in MARKS.items():
        if one in (plain, shifted):
            shown = shifted if shift or one == shifted else plain
            return {"key": shown, "code": code, "key_code": key_code, "text": shown}
    return None


def key(name, shift):
    """The key `name` names, with Shift down where `shift`: a letter, a digit, a mark, F1 to F24, a named key or a
    modifier; None where none."""
    if len(name) == 1:
        return typed(name, shift)
    if name.lower() in NAMED:
        return dict(NAMED[name.lower()])
    if name.lower() in MODIFIERS:
        _, modifier_key, code, key_code = MODIFIERS[name.lower()]
        return {"key": modifier_key, "code": code, "key_code": key_code, "text": None}
    if name[:1] in "Ff" and name[1:].isdigit() and 1 <= int(name[1:]) <= 24:
        number = int(name[1:])
        return {"key": "F%d" % number, "code": "F%d" % number, "key_code": 111 + number, "text": None}
    return None


def modifier_bits(keys):
    """The modifier bits of `keys`, as Ctrl or Ctrl+Shift."""
    bits = 0
    for part in filter(None, keys.split("+")):
        if part.lower() not in MODIFIERS:
            raise ValueError("%s is no modifier key" % part)
        bits |= MODIFIERS[part.lower()][0]
    return bits


def chord(text):
    """The modifier bits and the key of one chord, as Ctrl+Shift+P; a chord whose key is a plus ends ++."""
    if text.endswith("++"):
        held, last = text[:-2], "+"
    elif "+" in text:
        held, last = text.rsplit("+", 1)
    else:
        held, last = "", text
    bits = modifier_bits(held)
    pressed = key(last, bool(bits & SHIFT))
    if pressed is None:
        raise ValueError("%s in %s is no key" % (last, text))
    return bits, pressed


# ---- through the debugging port ----


def send_key(conn, kind, pressed, bits, repeating=False):
    params = {"type": kind, "modifiers": bits, "key": pressed["key"], "code": pressed["code"], "windowsVirtualKeyCode": pressed["key_code"], "nativeVirtualKeyCode": pressed["key_code"]}
    # Ctrl, Alt or Meta down, the key types nothing, as on the keyboard
    if kind == "keyDown" and not bits & (CTRL | ALT | META) and pressed.get("text"):
        params["text"] = params["unmodifiedText"] = pressed["text"]
    if pressed["code"].endswith("Left") and pressed["key_code"] != 37:
        params["location"] = 1
    if repeating:
        params["autoRepeat"] = True
    conn.call("Input.dispatchKeyEvent", timeout=WAIT, **params)


def hold(conn, bits):
    """The modifier keys of `bits` pressed down, in order."""
    held = 0
    for bit, modifier_key, code, key_code in MODIFIER_ORDER:
        if bits & bit:
            held |= bit
            send_key(conn, "rawKeyDown", {"key": modifier_key, "code": code, "key_code": key_code, "text": None}, held)


def release(conn, bits):
    held = bits
    for bit, modifier_key, code, key_code in reversed(MODIFIER_ORDER):
        if bits & bit:
            held &= ~bit
            send_key(conn, "keyUp", {"key": modifier_key, "code": code, "key_code": key_code, "text": None}, held)


def press(conn, keys):
    """`keys` pressed, one chord after another: Ctrl+K Ctrl+0 is two."""
    for text in filter(None, keys.split(" ")):
        bits, pressed = chord(text)
        hold(conn, bits)
        down = "keyDown" if pressed.get("text") and not bits & (CTRL | ALT | META) else "rawKeyDown"
        try:
            send_key(conn, down, pressed, bits)
            send_key(conn, "keyUp", pressed, bits)
        finally:
            release(conn, bits)


def repeat(conn, keys, count, every):
    """The chord `keys` kept down for `count` presses, one every `every` milliseconds from its first, as the system's repeat
    sends a key kept down; each press at its own time from the first, however long the page takes with the one before."""
    bits, pressed = chord(keys)
    hold(conn, bits)
    down = "keyDown" if pressed.get("text") and not bits & (CTRL | ALT | META) else "rawKeyDown"
    began = time.monotonic()
    try:
        for at in range(max(count, 1)):
            due = began + at * every / 1000.0
            if due > time.monotonic():
                time.sleep(due - time.monotonic())
            send_key(conn, down, pressed, bits, repeating=at > 0)
        send_key(conn, "keyUp", pressed, bits)
    finally:
        release(conn, bits)


def type_text(conn, text):
    """`text` typed key by key; a character no key types goes in as the system's input method puts it, and a line's end
    is Enter."""
    for one in text:
        if one == "\n":
            press(conn, "Enter")
            continue
        pressed = typed(one, False)
        if pressed is None:
            conn.call("Input.insertText", timeout=WAIT, text=one)
            continue
        shift = (one.isupper() and one.isascii()) or one in SHIFTED_DIGITS or any(one == shifted for _, _, shifted in MARKS.values())
        bits = SHIFT if shift else 0
        hold(conn, bits)
        try:
            send_key(conn, "keyDown", pressed, bits)
            send_key(conn, "keyUp", pressed, bits)
        finally:
            release(conn, bits)


BUTTON_BITS = {"left": 1, "right": 2, "middle": 4, "back": 8, "forward": 16}


def mouse(conn, kind, x, y, button, buttons, count, bits):
    conn.call("Input.dispatchMouseEvent", timeout=WAIT, type=kind, x=x, y=y, button=button, buttons=buttons, clickCount=count, modifiers=bits)


def number(step, name):
    if not isinstance(step.get(name), (int, float)):
        raise ValueError("the step has no %s" % name)
    return float(step[name])


# the programs a step left running, each stopped once the window's tests have run
STARTED = []
STARTED_LOCK = threading.Lock()


def run(step):
    """The program a step names run with its words, its environment and what its standard input reads; waited for and
    failed where it fails, or left running where `wait` is false."""
    program = step.get("program")
    if not program:
        raise ValueError("the step names no program")
    words = [word for word in step.get("args", []) if isinstance(word, str)]
    env = dict(os.environ)
    env.update({key_: str(value) for key_, value in (step.get("env") or {}).items()})
    child = subprocess.Popen([program, *words], env=env, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        child.stdin.write((step.get("input") or "").encode())
        child.stdin.close()
    except OSError:
        pass
    if step.get("wait") is False:
        with STARTED_LOCK:
            STARTED.append(child)
        return
    code = child.wait()
    if code != 0:
        raise RuntimeError("%s %s ended, exit code: %d" % (program, " ".join(words), code))


def stop_started():
    """Each program a step left running stopped."""
    with STARTED_LOCK:
        children, STARTED[:] = STARTED[:], []
    for child in children:
        child.kill()
        child.wait()


def perform(conn, step, frames):
    """One step the runner asked for, performed through the debugging port; `frames` is the browser trace's keeper."""
    op = step.get("op", "")
    if op == "click":
        x, y = number(step, "x"), number(step, "y")
        button = step.get("button") or "left"
        count = max(int(step.get("count") or 1), 1)
        bits = modifier_bits(step.get("keys") or "")
        hold(conn, bits)
        try:
            mouse(conn, "mouseMoved", x, y, "none", 0, 0, bits)
            for at in range(1, count + 1):
                mouse(conn, "mousePressed", x, y, button, BUTTON_BITS.get(button, 0), at, bits)
                mouse(conn, "mouseReleased", x, y, button, 0, at, bits)
        finally:
            release(conn, bits)
    elif op == "move":
        mouse(conn, "mouseMoved", number(step, "x"), number(step, "y"), "none", 0, 0, 0)
    elif op == "drag":
        x, y, to_x, to_y = (number(step, name) for name in ("x", "y", "toX", "toY"))
        steps = max(int(step.get("steps") or 12), 1)
        mouse(conn, "mouseMoved", x, y, "none", 0, 0, 0)
        mouse(conn, "mousePressed", x, y, "left", 1, 1, 0)
        for at in range(1, steps + 1):
            part = at / steps
            mouse(conn, "mouseMoved", x + (to_x - x) * part, y + (to_y - y) * part, "left", 1, 0, 0)
            time.sleep(0.016)
        mouse(conn, "mouseReleased", to_x, to_y, "left", 0, 1, 0)
    elif op == "wheel":
        conn.call("Input.dispatchMouseEvent", timeout=WAIT, type="mouseWheel", x=number(step, "x"), y=number(step, "y"), deltaX=float(step.get("deltaX") or 0), deltaY=float(step.get("deltaY") or 0), modifiers=0)
    elif op == "focus":
        conn.call("Emulation.setFocusEmulationEnabled", timeout=WAIT, enabled=step.get("on", True) is not False)
    elif op == "media":
        conn.call("Emulation.setEmulatedMedia", timeout=WAIT, features=[{"name": step.get("name") or "", "value": step.get("value") or ""}])
    elif op == "run":
        run(step)
    elif op == "browser_trace" and step.get("end") is True:
        frames.end(conn, step["writer"], step["qry"], step["name"])
    elif op == "browser_trace":
        frames.start(conn)
    elif op == "key":
        press(conn, step.get("keys") or "")
    elif op == "hold":
        hold(conn, modifier_bits(step.get("keys") or ""))
    elif op == "release":
        release(conn, modifier_bits(step.get("keys") or ""))
    elif op == "type":
        type_text(conn, step.get("text") or "")
    elif op == "repeat":
        repeat(conn, step.get("keys") or "", int(step.get("count") or 1), int(step.get("every") or 33))
    else:
        raise ValueError("the harness has no step %r" % op)


# ---- the same steps through WebDriver ----


def driven_value(pressed):
    """What WebDriver sends for `pressed`: a typed key's character, F1 to F12 and the named keys by WebDriver's codes."""
    if pressed["key"] in DRIVEN:
        return DRIVEN[pressed["key"]]
    if len(pressed["key"]) == 1:
        return pressed["key"]
    if pressed["key"][:1] == "F" and pressed["key"][1:].isdigit() and 1 <= int(pressed["key"][1:]) <= 12:
        return chr(0xE031 + int(pressed["key"][1:]) - 1)
    raise ValueError("WebDriver has no key %s" % pressed["key"])


def driven_modifiers(bits, up):
    """WebDriver's key actions pressing the modifier keys of `bits` down, in order, or letting them go in reverse."""
    ticks = [{"type": "keyUp" if up else "keyDown", "value": DRIVEN[modifier_key]} for bit, modifier_key, _, _ in MODIFIER_ORDER if bits & bit and modifier_key in DRIVEN]
    return list(reversed(ticks)) if up else ticks


def driven_chord(text):
    bits, pressed = chord(text)
    value = driven_value(pressed)
    return driven_modifiers(bits, False) + [{"type": "keyDown", "value": value}, {"type": "keyUp", "value": value}] + driven_modifiers(bits, True)


def keys_source(ticks):
    return {"type": "key", "id": "keyboard", "actions": ticks}


def pointer_source(ticks):
    return {"type": "pointer", "id": "mouse", "parameters": {"pointerType": "mouse"}, "actions": ticks}


DRIVEN_BUTTONS = {"left": 0, "middle": 1, "right": 2, "back": 3, "forward": 4}


def driven_move(x, y, ms):
    return {"type": "pointerMove", "origin": "viewport", "x": int(round(x)), "y": int(round(y)), "duration": ms}


WHEELS = [0]


def perform_driven(session, step):
    """One step the runner asked for, sent as WebDriver's actions; a step only the debugging port can take is said not
    to be taken."""
    op = step.get("op", "")
    if op == "click":
        x, y = number(step, "x"), number(step, "y")
        button = DRIVEN_BUTTONS.get(step.get("button") or "left", 0)
        bits = modifier_bits(step.get("keys") or "")
        session.actions([keys_source(driven_modifiers(bits, False))])
        ticks = [driven_move(x, y, 0)]
        for _ in range(max(int(step.get("count") or 1), 1)):
            ticks += [{"type": "pointerDown", "button": button}, {"type": "pointerUp", "button": button}]
        try:
            session.actions([pointer_source(ticks)])
        finally:
            session.actions([keys_source(driven_modifiers(bits, True))])
    elif op == "move":
        session.actions([pointer_source([driven_move(number(step, "x"), number(step, "y"), 0)])])
    elif op == "drag":
        x, y, to_x, to_y = (number(step, name) for name in ("x", "y", "toX", "toY"))
        steps = max(int(step.get("steps") or 12), 1)
        ticks = [driven_move(x, y, 0), {"type": "pointerDown", "button": 0}]
        ticks += [driven_move(x + (to_x - x) * at / steps, y + (to_y - y) * at / steps, 16) for at in range(1, steps + 1)]
        ticks.append({"type": "pointerUp", "button": 0})
        session.actions([pointer_source(ticks)])
    elif op == "wheel":
        # each step a wheel source of its own: WebKitWebDriver passes the first scroll of a source on and none after it
        WHEELS[0] += 1
        scroll = {"type": "scroll", "origin": "viewport", "x": int(round(number(step, "x"))), "y": int(round(number(step, "y"))), "deltaX": int(round(float(step.get("deltaX") or 0))), "deltaY": int(round(float(step.get("deltaY") or 0))), "duration": 0}
        session.actions([{"type": "wheel", "id": "wheel-%d" % WHEELS[0], "actions": [scroll]}])
    elif op == "focus":
        script = "const on = arguments[0]; window.__harnessHasFocus ??= document.hasFocus.bind(document); document.hasFocus = on ? () => true : window.__harnessHasFocus; window.dispatchEvent(new Event(on ? 'focus' : 'blur')); return true;"
        session.execute(script, [step.get("on", True) is not False])
    elif op == "media":
        raise RuntimeError("%s it is told no media feature, which the window with its debugging port is told" % NOT_HERE)
    elif op == "browser_trace":
        raise RuntimeError("%s it gives no trace of its frames, which the window with its debugging port gives" % NOT_HERE)
    elif op == "run":
        run(step)
    elif op == "key":
        ticks = []
        for text in filter(None, (step.get("keys") or "").split(" ")):
            ticks += driven_chord(text)
        session.actions([keys_source(ticks)])
    elif op == "hold":
        session.actions([keys_source(driven_modifiers(modifier_bits(step.get("keys") or ""), False))])
    elif op == "release":
        session.actions([keys_source(driven_modifiers(modifier_bits(step.get("keys") or ""), True))])
    elif op == "repeat":
        # WebDriver sends no repeat of a key kept down: the key pressed and let go `count` times, one every `every` ms
        value = step.get("keys") or ""
        ticks = []
        for at in range(max(int(step.get("count") or 1), 1)):
            if at:
                ticks.append({"type": "pause", "duration": int(step.get("every") or 33)})
            ticks += [{"type": "keyDown", "value": value}, {"type": "keyUp", "value": value}]
        session.actions([keys_source(ticks)])
    elif op == "type":
        ticks = []
        for one in step.get("text") or "":
            value = DRIVEN["Enter"] if one == "\n" else one
            ticks += [{"type": "keyDown", "value": value}, {"type": "keyUp", "value": value}]
        session.actions([keys_source(ticks)])
    else:
        raise ValueError("the harness has no step %r" % op)
