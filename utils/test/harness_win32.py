# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The Windows calls the harness's window commands make, through ctypes alone: the processes running and each one's
program, memory and job; a process's main window, its place and its move to a display; a picture of the screen written
as PNG. Every call here answers on Windows only; the window commands say so elsewhere."""

import ctypes
import struct
import zlib
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)

# the harness's coordinates are physical pixels, as the window's are
user32.SetProcessDPIAware()

PROCESS_TERMINATE = 0x0001
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
SYNCHRONIZE = 0x00100000
WAIT_TIMEOUT = 0x00000102
TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPTHREAD = 0x00000004
THREAD_SUSPEND_RESUME = 0x0002
CREATE_SUSPENDED = 0x00000004
INVALID_HANDLE = wintypes.HANDLE(-1).value
PAGE_BYTES = 4096
WORKING_SET_SHARED = 0x100
SW_SHOWNORMAL = 9
SW_SHOWMAXIMIZED = 3
SWP_NOSIZE = 0x0001
SWP_NOZORDER = 0x0004
GW_OWNER = 4
SRCCOPY = 0x00CC0020
BI_RGB = 0
DIB_RGB_COLORS = 0
JOB_BASIC_ACCOUNTING = 1


class ProcessEntry(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", wintypes.WCHAR * 260),
    ]


class ThreadEntry(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ThreadID", wintypes.DWORD),
        ("th32OwnerProcessID", wintypes.DWORD),
        ("tpBasePri", wintypes.LONG),
        ("tpDeltaPri", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
    ]


class MemoryCounters(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("PageFaultCount", wintypes.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
        ("PrivateUsage", ctypes.c_size_t),
    ]


class MonitorInfo(ctypes.Structure):
    _fields_ = [
        ("cbSize", wintypes.DWORD),
        ("rcMonitor", wintypes.RECT),
        ("rcWork", wintypes.RECT),
        ("dwFlags", wintypes.DWORD),
        ("szDevice", wintypes.WCHAR * 32),
    ]


class BitmapHeader(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD),
        ("biWidth", wintypes.LONG),
        ("biHeight", wintypes.LONG),
        ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD),
        ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD),
        ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG),
        ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class JobAccounting(ctypes.Structure):
    _fields_ = [
        ("TotalUserTime", ctypes.c_longlong),
        ("TotalKernelTime", ctypes.c_longlong),
        ("ThisPeriodTotalUserTime", ctypes.c_longlong),
        ("ThisPeriodTotalKernelTime", ctypes.c_longlong),
        ("TotalPageFaultCount", wintypes.DWORD),
        ("TotalProcesses", wintypes.DWORD),
        ("ActiveProcesses", wintypes.DWORD),
        ("TotalTerminatedProcesses", wintypes.DWORD),
    ]


kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenThread.restype = wintypes.HANDLE
kernel32.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.ResumeThread.restype = wintypes.DWORD
kernel32.ResumeThread.argtypes = [wintypes.HANDLE]
kernel32.CreateJobObjectW.restype = wintypes.HANDLE
kernel32.CreateJobObjectW.argtypes = [wintypes.LPVOID, wintypes.LPCWSTR]
kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
kernel32.QueryInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, wintypes.LPVOID, wintypes.DWORD,
                                               ctypes.POINTER(wintypes.DWORD)]
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
ENUM_WINDOWS = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
ENUM_MONITORS = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HANDLE, wintypes.HDC, ctypes.POINTER(wintypes.RECT), wintypes.LPARAM)


def processes():
    """Every process running: (id, parent id, program file name)."""
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE:
        return []
    entry = ProcessEntry()
    entry.dwSize = ctypes.sizeof(ProcessEntry)
    found = []
    more = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
    while more:
        found.append((entry.th32ProcessID, entry.th32ParentProcessID, entry.szExeFile))
        more = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    kernel32.CloseHandle(snapshot)
    return found


def process_tree(root_id):
    """The process and every process under it, ids, the root first."""
    every = processes()
    tree = [root_id]
    at = 0
    while at < len(tree):
        tree.extend(child for child, parent, _ in every if parent == tree[at] and child != tree[at] and child not in tree)
        at += 1
    return tree


def image_path(process_id):
    """The full path of a process's program, or an empty string where it cannot be asked."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, process_id)
    if not handle:
        return ""
    size = wintypes.DWORD(32768)
    path = ctypes.create_unicode_buffer(size.value)
    answered = kernel32.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size))
    kernel32.CloseHandle(handle)
    return path.value if answered else ""


def alive(process_id):
    handle = kernel32.OpenProcess(SYNCHRONIZE, False, process_id)
    if not handle:
        return False
    waited = kernel32.WaitForSingleObject(handle, 0)
    kernel32.CloseHandle(handle)
    return waited == WAIT_TIMEOUT


def ended(process_id, wait_ms=5000):
    """The process ended and waited for. True where it is gone."""
    handle = kernel32.OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, False, process_id)
    if not handle:
        return not alive(process_id)
    kernel32.TerminateProcess(handle, 1)
    waited = kernel32.WaitForSingleObject(handle, wait_ms)
    kernel32.CloseHandle(handle)
    return waited != WAIT_TIMEOUT


class MemoryBasicInformation(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", wintypes.DWORD),
        ("PartitionId", wintypes.WORD),
        ("RegionSize", ctypes.c_size_t),
        ("State", wintypes.DWORD),
        ("Protect", wintypes.DWORD),
        ("Type", wintypes.DWORD),
    ]


MEM_COMMIT = 0x1000
MEM_PRIVATE = 0x20000
MEM_MAPPED = 0x40000
MEM_IMAGE = 0x1000000
kernel32.VirtualQueryEx.restype = ctypes.c_size_t
kernel32.VirtualQueryEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.POINTER(MemoryBasicInformation), ctypes.c_size_t]


def regions(process_id):
    """Every region of a process's address space: (base, allocation base, bytes, state, type, protect), as
    VirtualQueryEx walks them; empty where the process cannot be asked."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, process_id)
    if not handle:
        return []
    found = []
    at = 0
    information = MemoryBasicInformation()
    while kernel32.VirtualQueryEx(handle, at, ctypes.byref(information), ctypes.sizeof(information)):
        found.append((information.BaseAddress or 0, information.AllocationBase or 0, information.RegionSize, information.State, information.Type, information.Protect))
        at = (information.BaseAddress or 0) + information.RegionSize
        if at >= 0x7FFFFFFFFFFF:
            break
    kernel32.CloseHandle(handle)
    return found


def mapped_names(process_id, addresses):
    """{address: the device path of the file mapped there}, for each of `addresses` a file backs; an address the
    paging file backs, or that cannot be asked, has none."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, process_id)
    if not handle:
        return {}
    named = {}
    text = ctypes.create_unicode_buffer(1024)
    for address in addresses:
        if kernel32.K32GetMappedFileNameW(handle, ctypes.c_void_p(address), text, 1024):
            named[address] = text.value
    kernel32.CloseHandle(handle)
    return named


def memory(process_id):
    """(private working set bytes, working set bytes, commit bytes) of a process, as the system counts them; None
    where it cannot be asked."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, process_id)
    if not handle:
        return None
    counters = MemoryCounters()
    counters.cb = ctypes.sizeof(MemoryCounters)
    kernel32.K32GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb)
    entries = 4096
    private_pages = 0
    while True:
        block = (ctypes.c_size_t * (entries + 1))()
        if kernel32.K32QueryWorkingSet(handle, ctypes.byref(block), ctypes.sizeof(block)):
            private_pages = sum(1 for page in block[1 : block[0] + 1] if not page & WORKING_SET_SHARED)
            break
        if entries > 1 << 24:
            break
        entries *= 4
    kernel32.CloseHandle(handle)
    return private_pages * PAGE_BYTES, counters.WorkingSetSize, counters.PrivateUsage


class FileTime(ctypes.Structure):
    _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]


def processor_seconds(process_id):
    """The processor time a process has used, user and kernel, in seconds; None where it cannot be asked."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, process_id)
    if not handle:
        return None
    made, gone, kernel, user = FileTime(), FileTime(), FileTime(), FileTime()
    asked = kernel32.GetProcessTimes(handle, ctypes.byref(made), ctypes.byref(gone), ctypes.byref(kernel), ctypes.byref(user))
    kernel32.CloseHandle(handle)
    if not asked:
        return None
    return sum((time.high << 32) | time.low for time in (kernel, user)) / 10_000_000


def in_job(process_id):
    """(asked, in a job) for a process: whether the question was answered and its answer."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, process_id)
    if not handle:
        return False, False
    answer = wintypes.BOOL(False)
    asked = kernel32.IsProcessInJob(handle, None, ctypes.byref(answer))
    kernel32.CloseHandle(handle)
    return bool(asked), bool(answer.value)


def threads_resumed(process_id):
    """Every thread of a process started with CREATE_SUSPENDED let run: how many were."""
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    if snapshot == INVALID_HANDLE:
        return 0
    entry = ThreadEntry()
    entry.dwSize = ctypes.sizeof(ThreadEntry)
    resumed = 0
    more = kernel32.Thread32First(snapshot, ctypes.byref(entry))
    while more:
        if entry.th32OwnerProcessID == process_id:
            thread = kernel32.OpenThread(THREAD_SUSPEND_RESUME, False, entry.th32ThreadID)
            if thread:
                resumed += 1 if kernel32.ResumeThread(thread) != 0xFFFFFFFF else 0
                kernel32.CloseHandle(thread)
        more = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
    kernel32.CloseHandle(snapshot)
    return resumed


def job_joined(process_handle):
    """A job of the harness's own with the process in it, so that every process it starts is in it too: the job's
    handle, or None where the system refused one. The process is started with CREATE_SUSPENDED and let run after it
    joins (threads_resumed); a process already running may start a child before it joins, and that child stays out."""
    job = kernel32.CreateJobObjectW(None, None)
    if not job:
        return None
    if not kernel32.AssignProcessToJobObject(job, process_handle):
        kernel32.CloseHandle(job)
        return None
    return job


def job_ended(job):
    """Every process in the job ended, whatever started it."""
    return bool(kernel32.TerminateJobObject(job, 124))


def job_processor_seconds(job):
    """The processor time every process assigned to the job has used, the ended ones with the running, in seconds; None
    where it cannot be read."""
    accounting = JobAccounting()
    if not kernel32.QueryInformationJobObject(job, JOB_BASIC_ACCOUNTING, ctypes.byref(accounting),
                                              ctypes.sizeof(JobAccounting), None):
        return None
    return (accounting.TotalUserTime + accounting.TotalKernelTime) / 10_000_000


def main_window(process_id):
    """(handle, title) of a process's main window, the visible unowned one, or (0, "") where it has none yet."""
    found = []

    def seen(handle, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(handle, ctypes.byref(owner))
        if owner.value == process_id and user32.IsWindowVisible(handle) and not user32.GetWindow(handle, GW_OWNER):
            title = ctypes.create_unicode_buffer(512)
            user32.GetWindowTextW(handle, title, 512)
            found.append((handle, title.value))
            return False
        return True

    user32.EnumWindows(ENUM_WINDOWS(seen), 0)
    return found[0] if found else (0, "")


def window_rect(handle):
    """(left, top, right, bottom) of a window."""
    rect = wintypes.RECT()
    user32.GetWindowRect(handle, ctypes.byref(rect))
    return rect.left, rect.top, rect.right, rect.bottom


class KeyInput(ctypes.Structure):
    _fields_ = [("kind", wintypes.DWORD), ("key", wintypes.WORD), ("scan", wintypes.WORD), ("flags", wintypes.DWORD),
                ("time", wintypes.DWORD), ("extra", ctypes.POINTER(ctypes.c_ulong)), ("padding", ctypes.c_ubyte * 8)]


INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 2
VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RETURN = 0x10, 0x11, 0x12, 0x5B, 0x0D


def _key_names():
    """Every key of the keyboard by the name the window gives it, or the name the system gives a key the window does
    not name: its virtual key, and 1 where it is one of the keys the keyboard marks extended."""
    names = {"Left": (0x25, 1), "Right": (0x27, 1), "Up": (0x26, 1), "Down": (0x28, 1), "Home": (0x24, 1),
             "End": (0x23, 1), "PageUp": (0x21, 1), "PageDown": (0x22, 1), "Insert": (0x2D, 1), "Delete": (0x2E, 1),
             "Backspace": (0x08, 0), "Tab": (0x09, 0), "Enter": (0x0D, 0), "Escape": (0x1B, 0), "Space": (0x20, 0),
             "Clear": (0x0C, 0), "Pause": (0x13, 0), "CapsLock": (0x14, 0), "NumLock": (0x90, 1),
             "ScrollLock": (0x91, 0), "PrintScreen": (0x2C, 1), "ContextMenu": (0x5D, 1),
             "Shift": (VK_SHIFT, 0), "Control": (VK_CONTROL, 0), "Ctrl": (VK_CONTROL, 0), "Alt": (VK_MENU, 0),
             "Win": (VK_LWIN, 1), "Meta": (VK_LWIN, 1), "Super": (VK_LWIN, 1),
             "NumpadMultiply": (0x6A, 0), "NumpadAdd": (0x6B, 0), "NumpadSeparator": (0x6C, 0),
             "NumpadSubtract": (0x6D, 0), "NumpadDecimal": (0x6E, 0), "NumpadDivide": (0x6F, 1),
             "NumpadEnter": (VK_RETURN, 1),
             "BrowserBack": (0xA6, 1), "BrowserForward": (0xA7, 1), "BrowserRefresh": (0xA8, 1),
             "BrowserStop": (0xA9, 1), "BrowserSearch": (0xAA, 1), "BrowserFavorites": (0xAB, 1),
             "BrowserHome": (0xAC, 1), "VolumeMute": (0xAD, 1), "VolumeDown": (0xAE, 1), "VolumeUp": (0xAF, 1),
             "MediaNext": (0xB0, 1), "MediaPrevious": (0xB1, 1), "MediaStop": (0xB2, 1), "MediaPlayPause": (0xB3, 1),
             "LaunchMail": (0xB4, 1), "LaunchMedia": (0xB5, 1), "LaunchApp1": (0xB6, 1), "LaunchApp2": (0xB7, 1),
             ";": (0xBA, 0), "=": (0xBB, 0), ",": (0xBC, 0), "-": (0xBD, 0), ".": (0xBE, 0), "/": (0xBF, 0),
             "`": (0xC0, 0), "[": (0xDB, 0), "\\": (0xDC, 0), "]": (0xDD, 0), "'": (0xDE, 0)}
    for number in range(1, 25):
        names["F%d" % number] = (0x6F + number, 0)
    for digit in range(10):
        names["Numpad%d" % digit] = (0x60 + digit, 0)
        names[str(digit)] = (0x30 + digit, 0)
    for letter in range(26):
        names[chr(0x41 + letter)] = (0x41 + letter, 0)
    return names


KEY_NAMES = _key_names()
user32.VkKeyScanW.argtypes = [wintypes.WCHAR]
user32.VkKeyScanW.restype = ctypes.c_short
user32.AttachThreadInput.argtypes = [wintypes.DWORD, wintypes.DWORD, wintypes.BOOL]
# the modifiers a chord names before its key, each by its virtual key
MODIFIER_OF = {"Mod": VK_CONTROL, "Ctrl": VK_CONTROL, "Control": VK_CONTROL, "Alt": VK_MENU, "Shift": VK_SHIFT,
               "Win": VK_LWIN, "Meta": VK_LWIN, "Super": VK_LWIN}

WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN, WM_SYSKEYUP = 0x0100, 0x0101, 0x0102, 0x0104, 0x0105
# a key message's lParam bits as the keyboard sets them: the key's extended mark, Alt down, the key down before it, and
# a release
KEY_EXTENDED, KEY_ALT_DOWN, KEY_WAS_DOWN, KEY_RELEASING = 1 << 24, 1 << 29, 1 << 30, 1 << 31
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEWHEEL = 0x0200, 0x0201, 0x0202, 0x020A
MK_LBUTTON = 0x0001
WHEEL_DELTA = 120


def pointer_sent(handle, x, y, down, up, moved=()):
    """A press at x, y of the window's client posted to its queue where `down`, the moves `moved` after it with the
    button down, and the release where `up`."""
    import time

    place = (y << 16) | (x & 0xFFFF)
    if down:
        user32.PostMessageW(handle, WM_LBUTTONDOWN, MK_LBUTTON, place)
    for to_x, to_y in moved:
        time.sleep(0.02)
        user32.PostMessageW(handle, WM_MOUSEMOVE, MK_LBUTTON, (to_y << 16) | (to_x & 0xFFFF))
        x, y = to_x, to_y
    if up:
        time.sleep(0.02)
        user32.PostMessageW(handle, WM_LBUTTONUP, 0, (y << 16) | (x & 0xFFFF))


def pointer_moved(handle, places, pause):
    """The pointer moved through `places`, each an x, y of the window's client, with no button down, `pause` seconds
    between one and the next."""
    import time

    for at, (x, y) in enumerate(places):
        if at:
            time.sleep(pause)
        user32.PostMessageW(handle, WM_MOUSEMOVE, 0, (y << 16) | (x & 0xFFFF))


def _key_message(key, extended, down, alt):
    """A key message and its lParam as the keyboard sends them for the virtual key `key`: down or up, the scan code
    the layout gives the key, its extended mark, and the system's messages where Alt is down or the key is F10."""
    scan = user32.MapVirtualKeyW(key, 0) & 0xFF
    system = alt or key == 0x79
    low = 1 | (scan << 16) | (KEY_EXTENDED if extended else 0) | (KEY_ALT_DOWN if alt else 0)
    if down:
        return (WM_SYSKEYDOWN if system else WM_KEYDOWN), low
    return (WM_SYSKEYUP if system else WM_KEYUP), low | KEY_WAS_DOWN | KEY_RELEASING


def _chord(name):
    """The modifiers' virtual keys and the key's, with its extended mark, a chord names: Mod+Shift+Z, F5, Numpad3,
    0x5B for a virtual key by its number, or one character, the key the layout types it on with Shift where it asks
    for Shift, Ctrl or Alt it asks for being out of a chord's reach; None where no key types the character."""
    if name == "+":
        head, key = "", "+"
    elif name.endswith("++"):
        head, key = name[:-2], "+"
    else:
        head, _, key = name.rpartition("+")
    parts = head.split("+") if head else []
    modifiers = []
    for part in parts:
        if part not in MODIFIER_OF:
            raise SystemExit("no modifier is named %s in %s" % (part, name))
        modifiers.append(MODIFIER_OF[part])
    if key in KEY_NAMES and len(key) > 1:
        return modifiers, KEY_NAMES[key]
    if key.lower().startswith("0x") and len(key) > 2:
        return modifiers, (int(key, 16) & 0xFF, 0)
    if len(key) == 1 and modifiers and key.upper() in KEY_NAMES and key.isalnum():
        # a letter or a digit in a chord is its key, the case the chord's Shift gives it
        return modifiers, KEY_NAMES[key.upper()]
    if len(key) == 1:
        if ord(key) > 0xFFFF:
            return modifiers, None
        scanned = user32.VkKeyScanW(key)
        shifts = (scanned >> 8) & 0xFF
        if scanned == -1 or shifts & 0x06:
            return modifiers, None
        if shifts & 0x01 and VK_SHIFT not in modifiers:
            modifiers.append(VK_SHIFT)
        return modifiers, (scanned & 0xFF, 0)
    raise SystemExit("no key is named %s" % name)


def _keyboard_state(thread, down):
    """The keyboard state the window's thread translates its keys by: the keys `down` pressed, each modifier's left key
    with it, and every other key up and every lock off. The harness's thread shares the window's input while it sets
    it."""
    state = (ctypes.c_ubyte * 256)()
    for key in down:
        state[key] = 0x80
        side = {VK_SHIFT: 0xA0, VK_CONTROL: 0xA2, VK_MENU: 0xA4}.get(key)
        if side:
            state[side] = 0x80
    mine = kernel32.GetCurrentThreadId()
    attached = user32.AttachThreadInput(mine, thread, True)
    user32.SetKeyboardState(state)
    return mine, attached


def _keyboard_left(mine, thread, attached):
    """Every key up again in the state the window's thread shares, and the harness's thread let go of it."""
    state = (ctypes.c_ubyte * 256)()
    user32.SetKeyboardState(state)
    if attached:
        user32.AttachThreadInput(mine, thread, False)


def keys_sent(handle, names):
    """The keys `names` pressed in the window as the keyboard presses them, each a chord the window names, Mod+Shift+Z,
    F5 or Right, a virtual key by its number, 0x5B, or one character, any the layout types: the modifiers down first, the
    key down and up as the keyboard sends it with its scan code, and the modifiers up after it, the window's thread
    reading the same keys down while it translates the key into the character the keyboard types. A
    character no key types, or one the layout types with Ctrl or Alt, goes as the character itself. Everything is
    posted to the window's own queue: no focus is needed and nothing reaches another window."""
    import time

    thread = user32.GetWindowThreadProcessId(handle, None)
    for name in names:
        modifiers, key = _chord(name)
        if key is None:
            chars_sent(handle, name if len(name) == 1 else (name.rpartition("+")[2] or "+"))
            continue
        # the keyboard sends the system's messages while Alt is down without Ctrl
        alt = (VK_MENU in modifiers) and (VK_CONTROL not in modifiers)
        mine, attached = _keyboard_state(thread, modifiers + [key[0]])
        for modifier in modifiers:
            message, low = _key_message(modifier, modifier == VK_LWIN, True, alt)
            user32.PostMessageW(handle, message, modifier, low)
        message, low = _key_message(key[0], key[1], True, alt)
        user32.PostMessageW(handle, message, key[0], low)
        # the window translates the key while its thread reads it down
        time.sleep(0.03)
        message, low = _key_message(key[0], key[1], False, alt)
        user32.PostMessageW(handle, message, key[0], low)
        for modifier in reversed(modifiers):
            message, low = _key_message(modifier, modifier == VK_LWIN, False, alt)
            user32.PostMessageW(handle, message, modifier, low)
        time.sleep(0.02)
        _keyboard_left(mine, thread, attached)
    return True


def chars_sent(handle, text):
    """Each character of `text` posted to the window as the character typed, its UTF-16 units one message each, a
    letter past the basic plane its two surrogates, with no key pressed for it."""
    import time

    for letter in text:
        point = ord(letter)
        units = [point] if point < 0x10000 else [0xD800 + ((point - 0x10000) >> 10), 0xDC00 + ((point - 0x10000) & 0x3FF)]
        for unit in units:
            user32.PostMessageW(handle, WM_CHAR, unit, 1)
        time.sleep(0.005)
    return True


def wheel_sent(handle, x, y, rows):
    """The wheel turned at x, y of the window's client by `rows` notches, toward the reader where positive, as the
    system sends it: its delta in the high word and the place on the screen."""
    point = wintypes.POINT(x, y)
    user32.ClientToScreen(handle, ctypes.byref(point))
    delta = (-rows * WHEEL_DELTA) & 0xFFFF
    user32.PostMessageW(handle, WM_MOUSEWHEEL, delta << 16, ((point.y & 0xFFFF) << 16) | (point.x & 0xFFFF))
    return True


def window_moved(handle, left, top):
    """The window shown normal and moved to (left, top), its size kept, and maximized again where it was."""
    maximized = user32.IsZoomed(handle)
    user32.ShowWindow(handle, SW_SHOWNORMAL)
    user32.SetWindowPos(handle, None, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER)
    if maximized:
        user32.ShowWindow(handle, SW_SHOWMAXIMIZED)


def displays():
    """{device name: (left, top, right, bottom) of its working area}, every display attached."""
    found = {}

    def seen(monitor, _dc, _rect, _lparam):
        info = MonitorInfo()
        info.cbSize = ctypes.sizeof(MonitorInfo)
        if user32.GetMonitorInfoW(monitor, ctypes.byref(info)):
            work = info.rcWork
            found[info.szDevice] = (work.left, work.top, work.right, work.bottom)
        return True

    user32.EnumDisplayMonitors(None, None, ENUM_MONITORS(seen), 0)
    return found


class DisplayConfigHeader(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_uint32), ("size", ctypes.c_uint32), ("adapter_low", ctypes.c_uint32), ("adapter_high", ctypes.c_int32), ("id", ctypes.c_uint32)]


class DisplayConfigSourceName(ctypes.Structure):
    _fields_ = [("header", DisplayConfigHeader), ("device", ctypes.c_wchar * 32)]


class DisplayConfigAdvancedColor(ctypes.Structure):
    _fields_ = [("header", DisplayConfigHeader), ("value", ctypes.c_uint32), ("encoding", ctypes.c_uint32), ("bits", ctypes.c_uint32)]


class DisplayConfigWhiteLevel(ctypes.Structure):
    _fields_ = [("header", DisplayConfigHeader), ("level", ctypes.c_uint32)]


# QueryDisplayConfig's active paths, and the kinds of DisplayConfigGetDeviceInfo asked: a source's GDI name, a target's
# advanced color state and its white level for SDR content. A path is 72 bytes, its source's adapter and id at 0 and 8
# and its target's at 20 and 28; a mode is 64 bytes and is not read.
QDC_ONLY_ACTIVE_PATHS = 2
GET_SOURCE_NAME = 1
GET_ADVANCED_COLOR_INFO = 9
GET_SDR_WHITE_LEVEL = 11
PATH_BYTES = 72
MODE_BYTES = 64


def sdr_white(device):
    """How a display composes SDR content, for the display named `device` as \\\\.\\DISPLAY1: (advanced color on, the
    linear scRGB value SDR white stands at, 1.0 being 80 nits). Off, or where the system cannot be asked, (False, 1.0)."""
    paths_count = ctypes.c_uint32()
    modes_count = ctypes.c_uint32()
    if user32.GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, ctypes.byref(paths_count), ctypes.byref(modes_count)) != 0:
        return False, 1.0
    paths = ctypes.create_string_buffer(PATH_BYTES * paths_count.value)
    modes = ctypes.create_string_buffer(MODE_BYTES * modes_count.value)
    if user32.QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, ctypes.byref(paths_count), paths, ctypes.byref(modes_count), modes, None) != 0:
        return False, 1.0
    for index in range(paths_count.value):
        at = index * PATH_BYTES
        source_low, source_high, source_id = struct.unpack_from("<Iii", paths, at)
        target_low, target_high, target_id = struct.unpack_from("<Iii", paths, at + 20)
        name = DisplayConfigSourceName()
        name.header = DisplayConfigHeader(GET_SOURCE_NAME, ctypes.sizeof(name), source_low, source_high, source_id)
        if user32.DisplayConfigGetDeviceInfo(ctypes.byref(name)) != 0 or name.device.lower() != device.lower():
            continue
        color = DisplayConfigAdvancedColor()
        color.header = DisplayConfigHeader(GET_ADVANCED_COLOR_INFO, ctypes.sizeof(color), target_low, target_high, target_id)
        white = DisplayConfigWhiteLevel()
        white.header = DisplayConfigHeader(GET_SDR_WHITE_LEVEL, ctypes.sizeof(white), target_low, target_high, target_id)
        if user32.DisplayConfigGetDeviceInfo(ctypes.byref(color)) != 0 or user32.DisplayConfigGetDeviceInfo(ctypes.byref(white)) != 0:
            return False, 1.0
        # advanced color enabled is the value's second bit; the white level is 1000 for 80 nits
        return bool(color.value & 2), white.level / 1000.0
    return False, 1.0


def screen_pixels(left, top, width, height):
    """The screen's pixels from (left, top), width by height, as rows of RGB bytes."""
    screen = user32.GetDC(None)
    memory_dc = gdi32.CreateCompatibleDC(screen)
    bitmap = gdi32.CreateCompatibleBitmap(screen, width, height)
    kept = gdi32.SelectObject(memory_dc, bitmap)
    gdi32.BitBlt(memory_dc, 0, 0, width, height, screen, left, top, SRCCOPY)
    header = BitmapHeader()
    header.biSize = ctypes.sizeof(BitmapHeader)
    header.biWidth = width
    header.biHeight = -height
    header.biPlanes = 1
    header.biBitCount = 32
    header.biCompression = BI_RGB
    pixels = (ctypes.c_ubyte * (width * height * 4))()
    gdi32.GetDIBits(memory_dc, bitmap, 0, height, pixels, ctypes.byref(header), DIB_RGB_COLORS)
    gdi32.SelectObject(memory_dc, kept)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(memory_dc)
    user32.ReleaseDC(None, screen)
    return rows_of_bgra(width, height, bytes(pixels))


def rows_of_bgra(width, height, raw):
    """Rows of RGB bytes from a bitmap's BGRA bytes, each channel moved by one slice."""
    rgb = bytearray(width * height * 3)
    rgb[0::3] = raw[2::4]
    rgb[1::3] = raw[1::4]
    rgb[2::3] = raw[0::4]
    stride = width * 3
    return [bytes(rgb[row * stride : (row + 1) * stride]) for row in range(height)]


def rows_scaled(rows, scale):
    """The rows drawn `scale` times larger, every pixel kept square."""
    scaled = []
    for row in rows:
        line = b"".join(row[at : at + 3] * scale for at in range(0, len(row), 3))
        scaled.extend([line] * scale)
    return scaled


def png_written(path, rows):
    """Rows of RGB bytes written as a PNG file."""
    height = len(rows)
    width = len(rows[0]) // 3 if rows else 0

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    filtered = b"".join(b"\x00" + row for row in rows)
    with open(path, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        out.write(chunk(b"IDAT", zlib.compress(filtered, 9)))
        out.write(chunk(b"IEND", b""))
