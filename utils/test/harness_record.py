# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""harness.py window record and window slide: the window recorded as a video, every frame the screen shows of it, and
the recording slid through a frame at a time.

    harness.py window record                           the window window.pid names, for --for milliseconds
    harness.py window record --port P --trigger JS     the expression run on the page once the recording runs
    harness.py window record --port P --drag 60        the editor's scrollbar dragged by its thumb once the recording runs
    harness.py window record --start --for 4000        the suite's build started on the fixture tree and recorded
                                                       from before its first frame
    harness.py window slide                            the last recording, every frame, each against the one before
    harness.py window slide REC --from 0.4 --to 1.2 --crop 0,0,400,200 --scale 3
    harness.py window frames [VIDEO] [--text L,T,R,B] [--thumb X]  each frame's text blank, plain or colored, and where
                                                       it stood still as the scrollbar's thumb moved

record grabs the screen through the Desktop Duplication API, ffmpeg's ddagrab, at --rate frames a second: the window's
rectangle of the display it stands on, or with --start the whole display the window is placed on, the window's
rectangle on it kept beside the recording. It is written lossless to build/harness/recordings/<stamp>.mkv.

slide writes each frame from --from to --to seconds under build/harness/slide as <number>_<ms>ms.png, cut to the
window's rectangle where the recording keeps one, says how far each differs from the frame before it so the frames
where something moves or flashes stand out, lays the frames that differ from the one before side by side, each under
its time, in strip.png, and writes the range as clip.mp4, --slower times slower, for any player to play and scrub."""

import glob
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

RECORDINGS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "build", "harness", "recordings")
SLIDE = os.path.join(os.path.dirname(RECORDINGS), "slide")
FRAMES = os.path.join(os.path.dirname(RECORDINGS), "frames.tsv")
# How long the recording may take to give its first frame, in seconds.
FIRST_FRAME_S = 15
# The width a frame is read at to measure how far it differs from the one before, and the difference, of 255, a frame
# must pass to stand in the strip; the most frames the strip holds, and the width of each there.
MEASURE_WIDTH = 240
STRIP_DIFFERS = 0.5
STRIP_MOST = 48
STRIP_WIDTH = 360


def ffmpeg():
    """The ffmpeg program: the PATH's, else the newest winget installed for the user, else None."""
    found = shutil.which("ffmpeg")
    if found:
        return found
    packages = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Microsoft", "WinGet", "Packages")
    installed = sorted(glob.glob(os.path.join(packages, "Gyan.FFmpeg*", "*", "bin", "ffmpeg.exe")), reverse=True)
    return installed[0] if installed else None


def dxgi_outputs():
    """Each output of the first graphics adapter, as ddagrab numbers them: (index, device name, (left, top, right,
    bottom) on the desktop)."""
    import ctypes
    from ctypes import wintypes

    class Guid(ctypes.Structure):
        _fields_ = [("data1", ctypes.c_ulong), ("data2", ctypes.c_ushort), ("data3", ctypes.c_ushort), ("data4", ctypes.c_ubyte * 8)]

    class OutputDesc(ctypes.Structure):
        _fields_ = [("device", ctypes.c_wchar * 32), ("desktop", wintypes.RECT), ("attached", wintypes.BOOL), ("rotation", ctypes.c_int), ("monitor", wintypes.HMONITOR)]

    def method(target, index, *argtypes):
        table = ctypes.cast(target, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        return ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, *argtypes)(table[index])

    def released(target):
        if target.value:
            method(target, 2)(target)

    factory_iid = Guid(0x770AAE78, 0xF26F, 0x4DBA, (ctypes.c_ubyte * 8)(0xA8, 0x29, 0x25, 0x3C, 0x83, 0xD1, 0xB3, 0x87))
    factory = ctypes.c_void_p()
    if ctypes.WinDLL("dxgi").CreateDXGIFactory1(ctypes.byref(factory_iid), ctypes.byref(factory)) != 0:
        return []
    adapter = ctypes.c_void_p()
    found = []
    try:
        # IDXGIFactory::EnumAdapters, IDXGIAdapter::EnumOutputs and IDXGIOutput::GetDesc each stand 7th in their table
        if method(factory, 7, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p))(factory, 0, ctypes.byref(adapter)) != 0:
            return []
        index = 0
        while True:
            output = ctypes.c_void_p()
            if method(adapter, 7, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p))(adapter, index, ctypes.byref(output)) != 0:
                break
            desc = OutputDesc()
            method(output, 7, ctypes.POINTER(OutputDesc))(output, ctypes.byref(desc))
            released(output)
            found.append((index, desc.device, (desc.desktop.left, desc.desktop.top, desc.desktop.right, desc.desktop.bottom)))
            index += 1
    finally:
        released(adapter)
        released(factory)
    return found


def shown_bounds(handle):
    """(left, top, right, bottom) of the window as it is drawn, its invisible resizing border left out."""
    import ctypes
    from ctypes import wintypes

    rect = wintypes.RECT()
    if ctypes.WinDLL("dwmapi").DwmGetWindowAttribute(wintypes.HWND(handle), 9, ctypes.byref(rect), ctypes.sizeof(rect)) != 0:
        import harness_win32 as win

        return win.window_rect(handle)
    return rect.left, rect.top, rect.right, rect.bottom


def output_holding(left, top, right, bottom):
    """The output that holds the most of the rectangle, or None."""
    best = None
    best_area = 0
    for one in dxgi_outputs():
        o_left, o_top, o_right, o_bottom = one[2]
        area = max(0, min(right, o_right) - max(left, o_left)) * max(0, min(bottom, o_bottom) - max(top, o_top))
        if area > best_area:
            best, best_area = one, area
    return best


def even(number):
    return number - number % 2


def color_grabbed(device):
    """ddagrab's format and the filters after it: each frame holds the colors the display's SDR content has, as a
    picture of the screen does: where the display composes in advanced color, the frames are taken in linear scRGB, SDR
    white at the level the system sets, brought back to 1.0 and written in sRGB; the 8 bits ddagrab gives there would be
    the linear values written as sRGB, every color lightened and washed toward white. Elsewhere its 8 bits are the
    screen's."""
    import harness_win32 as win

    advanced, white = win.sdr_white(device)
    if not advanced:
        return "", "hwdownload,format=bgra"
    scale = 1.0 / white
    srgb = "zscale=transferin=linear:transfer=iec61966-2-1:primariesin=709:primaries=709:rangein=full:range=full"
    return ":output_fmt=rgbaf16", "hwdownload,format=rgbaf16le,format=gbrpf32le,colorchannelmixer=rr=%.6f:gg=%.6f:bb=%.6f,%s,format=bgr0" % (scale, scale, scale, srgb)


def recording_started(command):
    """ffmpeg started on `command`, and the process once its first frame is written, or None where none came."""
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    first = threading.Event()
    said = []

    def read():
        line = b""
        while True:
            letter = process.stderr.read(1)
            if not letter:
                break
            if letter in b"\r\n":
                text = line.decode("utf-8", "replace")
                said.append(text)
                if re.search(r"frame=\s*[1-9]", text):
                    first.set()
                line = b""
            else:
                line += letter

    reader = threading.Thread(target=read, daemon=True)
    reader.start()
    process.said = said
    process.reader = reader
    if not first.wait(FIRST_FRAME_S):
        process.kill()
        process.wait()
        print("the recording gave no frame in %d s: %s" % (FIRST_FRAME_S, " / ".join(said[-4:])))
        return None
    return process


def frames_said(said):
    """The frames ffmpeg last said it wrote."""
    for text in reversed(said):
        found = re.search(r"frame=\s*(\d+)", text)
        if found:
            return int(found.group(1))
    return 0


def cmd_window_record(a):
    """The window recorded for --for milliseconds, lossless, to build/harness/recordings/<stamp>.mkv."""
    import harness_window as hw
    import harness_win32 as win

    if not hw.windows_only():
        return 1
    program = ffmpeg()
    if not program:
        print("no ffmpeg on the PATH or among winget's packages: winget install Gyan.FFmpeg puts one where this finds it")
        return 1
    os.makedirs(RECORDINGS, exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S")
    path = os.path.join(RECORDINGS, stamp + ".mkv")
    kept = {"rate": a.rate}
    if a.start or a.display:
        display = (r"\\.\DISPLAY" + a.display if a.display.isdigit() else a.display) or hw.DISPLAY
        areas = win.displays()
        if display not in areas:
            print("no display %s (attached: %s)" % (display, " ".join(sorted(areas))))
            return 1
        output = next((one for one in dxgi_outputs() if one[1] == display), None)
        grabbed = (0, 0, even(output[2][2] - output[2][0]), even(output[2][3] - output[2][1])) if output else None
    else:
        found = hw.window_of(a.id)
        if not found:
            return 1
        left, top, right, bottom = shown_bounds(found[1])
        output = output_holding(left, top, right, bottom)
        if output:
            o_left, o_top, o_right, o_bottom = output[2]
            x, y = max(0, left - o_left), max(0, top - o_top)
            grabbed = (x, y, even(min(right, o_right) - o_left - x), even(min(bottom, o_bottom) - o_top - y))
    if not output:
        print("no display output holds the window")
        return 1
    x, y, width, height = grabbed
    grab_format, filters = color_grabbed(output[1])
    source = "ddagrab=output_idx=%d:framerate=%d:offset_x=%d:offset_y=%d:video_size=%dx%d:draw_mouse=%d%s" % (output[0], a.rate, x, y, width, height, 0 if a.no_pointer else 1, grab_format)
    command = [program, "-hide_banner", "-loglevel", "error", "-stats", "-stats_period", "0.05", "-f", "lavfi", "-i", source, "-vf", filters, "-c:v", "libx264rgb", "-qp", "0", "-preset", "ultrafast", "-t", "%.3f" % (a.seconds_for / 1000.0), "-y", path]
    process = recording_started(command)
    if not process:
        return 1
    started = None
    try:
        if a.start:
            started = hw.frames_window_started()
            found = hw.window_of(started.pid)
            if found:
                left, top, right, bottom = shown_bounds(found[1])
                kept["window"] = [left - output[2][0], top - output[2][1], right - left, bottom - top]
        if a.port and a.trigger:
            import harness_cdp

            conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
            conn.call("Runtime.enable", timeout=10)
            conn.evaluate(a.trigger, timeout=10)
        if a.port and a.drag:
            import harness_cdp
            import harness_input

            conn = harness_cdp.Connection(harness_cdp.window_page(a.port))
            conn.call("Runtime.enable", timeout=10)
            where = hw.thumb_of(conn)
            if where:
                x, y = where["x"], where["y"]
                for to in (where["bottom"], where["top"], where["y"]):
                    harness_input.perform(conn, {"op": "drag", "x": x, "y": y, "toX": x, "toY": to, "steps": a.drag}, None)
                    y = to
        process.wait(timeout=a.seconds_for / 1000.0 + 30)
    finally:
        if process.poll() is None:
            process.stdin.write(b"q")
            process.stdin.flush()
            process.wait(timeout=30)
        process.reader.join(timeout=5)
        if started:
            started.kill()
            started.wait()
    kept.update({"display": output[1], "grabbed": [x, y, width, height]})
    with open(os.path.join(RECORDINGS, stamp + ".json"), "w", encoding="utf-8") as out:
        json.dump(kept, out, indent=1)
    print("%s: %d frames of %s at %d,%d, %d x %d, %d a second%s" % (path, frames_said(process.said), output[1], x, y, width, height, a.rate, ", the window at %d,%d %d x %d of it" % tuple(kept["window"]) if "window" in kept else ""))
    return 0


def latest_recording():
    found = sorted(glob.glob(os.path.join(RECORDINGS, "*.mkv")))
    return found[-1] if found else None


def frames_extracted(program, recording, folder, a, crop):
    """Each frame of the recording from --from to --to, every --every, cut to `crop` and drawn --scale times larger,
    written into `folder`: their paths and their times in milliseconds, in order."""
    chosen = "between(t,%.4f,%.4f)" % (a.seconds_from, a.seconds_to if a.seconds_to > 0 else 1e9)
    if a.every > 1:
        chosen += "*not(mod(n,%d))" % a.every
    filters = ["select='%s'" % chosen, "showinfo"]
    if crop:
        filters.append("crop=%d:%d:%d:%d" % (crop[2], crop[3], crop[0], crop[1]))
    if a.scale > 1:
        filters.append("scale=iw*%d:ih*%d:flags=neighbor" % (a.scale, a.scale))
    made = subprocess.run([program, "-hide_banner", "-loglevel", "info", "-i", recording, "-vf", ",".join(filters), "-fps_mode", "passthrough", "-y", os.path.join(folder, "frame_%06d.png")], stdin=subprocess.DEVNULL, capture_output=True, text=True, errors="replace")
    if made.returncode != 0:
        print(made.stderr[-2000:])
        return []
    times = [round(float(found) * 1000.0) for found in re.findall(r"pts_time:\s*([\d.]+)", made.stderr)]
    frames = []
    for count, at in enumerate(times):
        given = os.path.join(folder, "frame_%06d.png" % (count + 1))
        if os.path.isfile(given):
            path = os.path.join(folder, "%04d_%05dms.png" % (count, at))
            os.replace(given, path)
            frames.append((path, at))
    return frames


def differences(frames):
    """How far each frame differs from the one before, the mean of its pixels' differences out of 255; the first 0."""
    import numpy
    from PIL import Image

    measured = []
    before = None
    for path, _ in frames:
        with Image.open(path) as image:
            height = max(1, round(image.height * MEASURE_WIDTH / image.width))
            now = numpy.asarray(image.convert("L").resize((MEASURE_WIDTH, height)), dtype=numpy.float32)
        measured.append(0.0 if before is None or before.shape != now.shape else float(numpy.abs(now - before).mean()))
        before = now
    return measured


def strip_written(path, frames, measured):
    """The frames that differ from the one before, the first among them, side by side under their times."""
    from PIL import Image, ImageDraw

    chosen = [frame for at, frame in enumerate(frames) if at == 0 or measured[at] >= STRIP_DIFFERS][:STRIP_MOST]
    if not chosen:
        return 0
    thumbs = []
    for frame_path, at in chosen:
        with Image.open(frame_path) as image:
            height = max(1, round(image.height * STRIP_WIDTH / image.width))
            thumbs.append((image.convert("RGB").resize((STRIP_WIDTH, height)), at))
    columns = min(6, len(thumbs))
    rows = (len(thumbs) + columns - 1) // columns
    label = 18
    cell_height = max(thumb.height for thumb, _ in thumbs) + label
    sheet = Image.new("RGB", (columns * (STRIP_WIDTH + 4), rows * cell_height), (32, 32, 32))
    pen = ImageDraw.Draw(sheet)
    for index, (thumb, at) in enumerate(thumbs):
        left = (index % columns) * (STRIP_WIDTH + 4)
        top = (index // columns) * cell_height
        pen.text((left + 4, top + 2), "%d ms" % at, fill=(230, 230, 230))
        sheet.paste(thumb, (left, top + label))
    sheet.save(path)
    return len(thumbs)


def cmd_window_slide(a):
    """A recording slid through: each frame from --from to --to written, how far each differs from the one before,
    and the strip of those that do."""
    program = ffmpeg()
    if not program:
        print("no ffmpeg on the PATH or among winget's packages: winget install Gyan.FFmpeg puts one where this finds it")
        return 1
    recording = a.recording or latest_recording()
    if not recording or not os.path.isfile(recording):
        print("no recording under %s" % RECORDINGS)
        return 1
    crop = [int(one) for one in a.crop.split(",")] if a.crop else None
    beside = os.path.splitext(recording)[0] + ".json"
    if not crop and not a.whole and os.path.isfile(beside):
        with open(beside, encoding="utf-8") as kept:
            window = json.load(kept).get("window")
        crop = [even(one) for one in window] if window else None
    shutil.rmtree(SLIDE, ignore_errors=True)
    os.makedirs(SLIDE)
    frames = frames_extracted(program, recording, SLIDE, a, crop)
    if not frames:
        print("no frame of %s from %.3f s to %s" % (recording, a.seconds_from, "%.3f s" % a.seconds_to if a.seconds_to > 0 else "its end"))
        return 1
    measured = differences(frames)
    for (path, at), differs in zip(frames, measured):
        if a.all or differs >= STRIP_DIFFERS:
            print("%7d ms  %6.2f  %s" % (at, differs, os.path.basename(path)))
    shown = strip_written(os.path.join(SLIDE, "strip.png"), frames, measured)
    clip = clip_written(program, recording, os.path.join(SLIDE, "clip.mp4"), a, crop, frames[0][1], frames[-1][1])
    most = sorted(range(len(frames)), key=lambda at: -measured[at])[:5]
    print("%d frames from %d to %d ms of %s in %s; %d in strip.png%s; the most change at %s" % (len(frames), frames[0][1], frames[-1][1], os.path.basename(recording), SLIDE, shown, ", clip.mp4 at a %gth of the speed" % a.slower if clip and a.slower > 1 else ", clip.mp4" if clip else "", ", ".join("%d ms (%.2f)" % (frames[at][1], measured[at]) for at in most)))
    return 0


def clip_written(program, recording, path, a, crop, first_ms, last_ms):
    """The frames slid through as a video any player plays and scrubs, --slower times slower: whether it was written."""
    filters = []
    if crop:
        filters.append("crop=%d:%d:%d:%d" % (crop[2], crop[3], crop[0], crop[1]))
    if a.scale > 1:
        filters.append("scale=iw*%d:ih*%d:flags=neighbor" % (a.scale, a.scale))
    if a.slower > 1:
        filters.append("setpts=%d*PTS" % a.slower)
    filters.append("format=yuv420p")
    made = subprocess.run([program, "-hide_banner", "-loglevel", "error", "-ss", "%.3f" % (first_ms / 1000.0), "-to", "%.3f" % (last_ms / 1000.0 + 0.02), "-i", recording, "-vf", ",".join(filters), "-c:v", "libx264", "-crf", "12", "-preset", "fast", "-an", "-y", path], stdin=subprocess.DEVNULL, capture_output=True, text=True, errors="replace")
    return made.returncode == 0


def video_size(program, video):
    """A video's width and height in pixels, as ffmpeg reads its first video stream, or None."""
    said = subprocess.run([program, "-hide_banner", "-i", video], stdin=subprocess.DEVNULL, capture_output=True, text=True, errors="replace").stderr
    found = re.search(r"Video:.*?(\d{2,5})x(\d{2,5})", said)
    return (int(found.group(1)), int(found.group(2))) if found else None


def thumb_runs(bright):
    """The bright runs down a column a thumb's length long, (first, last) each: the scrollbar's arrows and the dots of
    the ground behind it are shorter."""
    import numpy

    lit = numpy.flatnonzero(bright)
    if not lit.size:
        return []
    breaks = numpy.flatnonzero(numpy.diff(lit) > 1)
    firsts = numpy.concatenate(([lit[0]], lit[breaks + 1]))
    lasts = numpy.concatenate((lit[breaks], [lit[-1]]))
    return [(int(first), int(last)) for first, last in zip(firsts, lasts) if 10 <= last - first + 1 <= 150]


def thumb_column(profiles, width):
    """The column of the scrollbar's thumb among `profiles`, each frame's brightness down every column of its right
    fifth: the column whose one bright run of a thumb's length moves most often from one frame looked at to the next,
    the leftmost of those, a part of the window that stands still holding a run that never moves; -1 where none
    moves."""
    import numpy

    left = width - profiles[0].shape[1]
    moved = numpy.zeros(profiles[0].shape[1], dtype=numpy.int32)
    before = [None] * profiles[0].shape[1]
    for profile in profiles[:: max(1, len(profiles) // 120)]:
        bright = profile > 120
        for column in range(bright.shape[1]):
            runs = thumb_runs(bright[:, column])
            now = runs[0] if len(runs) == 1 else None
            moved[column] += (now is not None) and (before[column] is not None) and (now != before[column])
            before[column] = now
    if moved.max() == 0:
        return -1
    return left + int(numpy.flatnonzero(moved == moved.max())[0])


def cmd_window_frames(a):
    """A recording, or any video of the window, read a frame at a time with the scrollbar's thumb as the compass: where
    the thumb stands, whether the text moved from the frame before, and whether the text is blank, plain or colored;
    the frames the thumb moved in and the text did not, and those the text moved in and the thumb did not, counted. Each
    frame to build/harness/frames.tsv."""
    import numpy

    program = ffmpeg()
    if not program:
        print("no ffmpeg on the PATH or among winget's packages: winget install Gyan.FFmpeg puts one where this finds it")
        return 1
    video = a.recording or latest_recording()
    size = video_size(program, video) if video and os.path.isfile(video) else None
    if not size:
        print("no video at %s" % (video or RECORDINGS))
        return 1
    width, height = size
    left, top, right, bottom = [int(one) for one in a.text.split(",")] if a.text else (width // 10, height * 9 // 100, width * 88 // 100, height * 94 // 100)
    reader = subprocess.Popen([program, "-hide_banner", "-loglevel", "error", "-i", video, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE)
    frame_bytes = width * height * 3
    side = width - width // 5
    rows = []
    profiles = []
    before = None
    while True:
        raw = reader.stdout.read(frame_bytes)
        if len(raw) < frame_bytes:
            break
        whole = numpy.frombuffer(raw, numpy.uint8).reshape(height, width, 3)
        profiles.append(whole[:, side:].max(axis=2))
        text = whole[top:bottom, left:right].astype(numpy.int16)
        high = text.max(axis=2)
        ink = high > 110
        hued = ink & ((high - text.min(axis=2)) > 40)
        inked = int(ink.sum())
        share = float(hued.sum()) / max(1, inked)
        kind = "blank" if inked < ink.size // 500 else ("colored" if share > 0.12 else "plain")
        moved = float(numpy.abs(text - before).mean()) if before is not None else 0.0
        before = text
        rows.append([kind, share, moved])
    reader.wait()
    if not rows:
        print("no frame read of %s" % video)
        return 1
    column = a.thumb if a.thumb else thumb_column(profiles, width)
    rate = a.rate
    thumbs = []
    for profile in profiles:
        runs = thumb_runs(profile[:, column - side] > 120) if column >= side else []
        thumbs.append((runs[0][0] + runs[0][1]) / 2.0 if len(runs) == 1 else -1.0)
    stalls = bursts = slid = 0
    marks = []
    with open(FRAMES, "w", encoding="utf-8") as out:
        out.write("frame\tms\tthumb\tthumb_moved\ttext\thued\ttext_moved\n")
        for at, (kind, share, moved) in enumerate(rows):
            thumb_moved = thumbs[at] - thumbs[at - 1] if at and thumbs[at] >= 0 and thumbs[at - 1] >= 0 else 0.0
            sliding = abs(thumb_moved) >= 0.5
            text_moved = moved > 1.0
            slid += sliding
            stalls += sliding and not text_moved
            bursts += text_moved and not sliding and at > 0
            marks.append("." if kind == "blank" else kind[0])
            out.write("%d\t%.0f\t%.1f\t%.1f\t%s\t%.2f\t%.2f\n" % (at, at * 1000.0 / rate, thumbs[at], thumb_moved, kind, share, moved))
    kinds = {kind: sum(1 for row in rows if row[0] == kind) for kind in ("blank", "plain", "colored")}
    print("%s: %d frames, the thumb in column %d" % (os.path.basename(video), len(rows), column))
    print("  text blank in %d, plain in %d, colored in %d" % (kinds["blank"], kinds["plain"], kinds["colored"]))
    print("  the thumb moved in %d; of those the text stood still in %d; the text moved with the thumb still in %d" % (slid, stalls, bursts))
    print("  each frame, . blank, p plain, c colored:")
    for at in range(0, len(marks), 60):
        print("    " + "".join(marks[at : at + 60]))
    print("  each frame: build/harness/frames.tsv")
    return 0


def register(window):
    """The record, slide and frames commands, added to the window group."""
    p = window.add_parser("record", help="the window recorded as a video at the display's rate, lossless, to build/harness/recordings")
    p.add_argument("--id", type=int, default=0, help="the process; window.pid's where none")
    p.add_argument("--for", dest="seconds_for", type=int, default=3000, help="milliseconds")
    p.add_argument("--rate", type=int, default=60, help="frames a second")
    p.add_argument("--port", type=int, default=0, help="the window's debugging port, for --trigger")
    p.add_argument("--trigger", default="", help="an expression run on the page once the recording runs")
    p.add_argument("--drag", type=int, default=0, help="the editor's scrollbar dragged by its thumb to the bottom, the top and back once the recording "
                                                       "runs, this many moves each way, through --port")
    p.add_argument("--start", action="store_true", help="the suite's build started on the fixture tree and recorded from before its first frame")
    p.add_argument("--display", default="", help="a display recorded whole, by its number or its device name, with no window of its own")
    p.add_argument("--no-pointer", action="store_true", help="the pointer left out of the frames")
    p.set_defaults(fn=cmd_window_record)

    p = window.add_parser("slide", help="a recording slid through a frame at a time, each frame against the one before, to build/harness/slide")
    p.add_argument("recording", nargs="?", default="", help="the .mkv; the last recorded where none")
    p.add_argument("--from", dest="seconds_from", type=float, default=0.0, help="seconds")
    p.add_argument("--to", dest="seconds_to", type=float, default=0.0, help="seconds; the recording's end where 0")
    p.add_argument("--every", type=int, default=1, help="every this many frames")
    p.add_argument("--crop", default="", help="LEFT,TOP,WIDTH,HEIGHT of each frame kept; the window's rectangle where the recording keeps one")
    p.add_argument("--whole", action="store_true", help="each frame whole, the window's rectangle set aside")
    p.add_argument("--scale", type=int, default=1, help="each frame drawn this many times larger")
    p.add_argument("--all", action="store_true", help="every frame listed, and not only those that differ from the one before")
    p.add_argument("--slower", type=int, default=1, help="clip.mp4 played this many times slower")
    p.set_defaults(fn=cmd_window_slide)

    p = window.add_parser("frames", help="a recording or any video of the window read a frame at a time, the scrollbar's thumb as the compass: "
                                         "the text blank, plain or colored, and where it stood still as the thumb moved, to build/harness/frames.tsv")
    p.add_argument("recording", nargs="?", default="", help="the video; the last recorded where none")
    p.add_argument("--text", default="", help="LEFT,TOP,RIGHT,BOTTOM of the text in each frame; the frame's middle where none")
    p.add_argument("--thumb", type=int, default=0, help="the column the scrollbar's thumb runs down; found where none")
    p.add_argument("--rate", type=float, default=60.0, help="the video's frames a second, for each frame's time")
    p.set_defaults(fn=cmd_window_frames)
