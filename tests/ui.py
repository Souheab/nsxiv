#!/usr/bin/env python3
"""Optional end-to-end checks; run inside tests/run.sh.

Requires Python 3, xdotool, ImageMagick (import/convert), and Openbox on PATH.
Usage: sh tests/run.sh python3 tests/ui.py ./nsxiv
Set NSXIV_TEST_ARTIFACTS to retain screenshots in a chosen directory.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def run(*args):
    return subprocess.check_output(args, stderr=subprocess.PIPE, timeout=10)


def wait_for(check, message):
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if check():
            return
        time.sleep(0.05)
    raise AssertionError(message)


binary = str(Path(sys.argv[1]).resolve())
artifacts = os.environ.get("NSXIV_TEST_ARTIFACTS")
if artifacts:
    Path(artifacts).mkdir(parents=True, exist_ok=True)

with tempfile.TemporaryDirectory(prefix="nsxiv-ui-") as directory:
    root = Path(directory)
    config = root / "config/nsxiv/exec"
    config.mkdir(parents=True)
    title_script = config / "win-title"
    title_script.write_text('#!/bin/sh\nprintf "index=%s size=%sx%s zoom=%s" "$2" "$4" "$5" "$6"\n')
    title_script.chmod(0o755)
    info_script = config / "image-info"
    info_script.write_text('#!/bin/sh\nsleep 0.2\nprintf "async image info"\n')
    info_script.chmod(0o755)
    env = dict(os.environ, XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"))
    files = []
    for index in range(12):
        path = root / f"image-{index}.ppm"
        pixels = bytes((index * 17, 70, 190)) * (120 * 80)
        path.write_bytes(b"P6\n120 80\n255\n" + pixels)
        files.append(str(path))

    wm_log = (root / "wm.log").open("wb")
    wm = subprocess.Popen(["openbox"], stdout=wm_log, stderr=wm_log)
    time.sleep(0.4)

    def launch(args, images):
        log = tempfile.TemporaryFile()
        process = subprocess.Popen([binary, "-o", "-g", "900x640", *args, *images],
                                   env=env, stdout=subprocess.PIPE, stderr=log)
        window = None

        def find_window():
            nonlocal window
            assert process.poll() is None, "nsxiv exited during startup"
            found = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(process.pid)],
                                   stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            if found.returncode == 0:
                window = found.stdout.splitlines()[0].decode()
            return window is not None

        wait_for(find_window, "nsxiv window did not appear")
        run("xdotool", "windowactivate", "--sync", window)
        run("xdotool", "mousemove", "0", "0")
        time.sleep(0.3)
        return process, window, log

    def key(window, *keys):
        run("xdotool", "key", "--window", window, "--clearmodifiers", *keys)
        time.sleep(0.08)

    def title(window):
        return run("xdotool", "getwindowname", window).decode()

    def snapshot(window, name=None):
        if name and artifacts:
            run("import", "-window", window, str(Path(artifacts) / (name + ".png")))
        return run("import", "-window", window, "rgb:-")

    def finish(process, window, log):
        # KeyRelease can race destruction of the window after a quit command.
        run("xdotool", "keydown", "--window", window, "q")
        output = process.communicate(timeout=5)[0]
        run("xdotool", "keyup", "q")
        assert process.returncode == 0 and output == b"", "search input marked images or changed exit behavior"
        log.seek(0)
        errors = log.read().decode()
        assert "X Error" not in errors and "Sanitizer" not in errors, errors
        log.close()

    process = None
    try:
        process, window, log = launch([], files)
        wait_for(lambda: "index=1 " in title(window), "initial image title missing")
        baseline = snapshot(window)
        key(window, "7", "question")
        opened = snapshot(window, "image-help")
        assert opened != baseline, "help did not open"
        run("xdotool", "type", "--window", window, "--delay", "0", "qmn123?<>")
        key(window, "Return", "Tab")
        run("xdotool", "mousemove", "--window", window, "850", "500", "click", "1", "click", "3")
        assert process.poll() is None and "index=1 " in title(window)
        key(window, "ctrl+u")
        assert snapshot(window) == opened, "clearing query did not restore initial results"
        key(window, "Down", "Next")
        assert snapshot(window) != opened, "results did not scroll"
        key(window, "ctrl+u")
        run("xdotool", "type", "--window", window, "--delay", "0", "rotate 90")
        snapshot(window, "rotation-search")
        key(window, "Escape")
        time.sleep(0.3)
        assert snapshot(window) == baseline, "closing help changed viewer state or left artifacts"
        key(window, "n")
        wait_for(lambda: "index=2 " in title(window), "numeric prefix leaked out of help")
        key(window, "Return")
        wait_for(lambda: "size=x " in title(window), "thumbnail mode did not open")
        time.sleep(0.5)
        baseline = snapshot(window)
        key(window, "question")
        run("xdotool", "type", "--window", window, "--delay", "0", "qmn123")
        key(window, "Return", "Escape")
        assert snapshot(window) == baseline, "thumbnail state changed during search"
        key(window, "question")
        snapshot(window, "thumbnail-help")
        for width, height in [(80, 60), (320, 180), (1000, 800), (900, 640)]:
            run("xdotool", "windowsize", "--sync", window, str(width), str(height))
            time.sleep(0.2)
            assert process.poll() is None, "resize crashed nsxiv"
        key(window, "Escape", "Return", "b", "f", "question")
        time.sleep(0.3)
        snapshot(window, "fullscreen-hidden-bar")
        for _ in range(5):
            key(window, "Escape", "question")
        run("xdotool", "type", "--window", window, "--delay", "0", "z" * 300)
        snapshot(window, "long-query")
        key(window, "ctrl+u", "Escape")
        finish(process, window, log)

        process, window, log = launch(["-S", "1"], files)
        key(window, "question")
        frozen_title = title(window)
        frozen = snapshot(window)
        time.sleep(2.2)
        assert title(window) == frozen_title and snapshot(window) == frozen, "slideshow advanced during help"
        # A file reload and the asynchronous info callback must leave help visible.
        first = Path(files[0])
        first.write_bytes(b"P6\n120 80\n255\n" + bytes((170, 30, 20)) * (120 * 80))
        time.sleep(0.6)
        assert snapshot(window) == frozen, "background redraw erased the opaque help panel"
        key(window, "Escape")
        wait_for(lambda: title(window) != frozen_title, "slideshow failed to resume")
        key(window, "s")
        finish(process, window, log)

        gif = root / "animation.gif"
        run("convert", "-delay", "20", "-size", "1000x800", "xc:red", "xc:blue", "-loop", "0", str(gif))
        process, window, log = launch(["-a"], [str(gif)])
        samples = []
        for _ in range(4):
            samples.append(snapshot(window))
            time.sleep(0.15)
        assert len(set(samples)) > 1, "animation fixture is not playing"
        key(window, "question")
        frozen = snapshot(window)
        time.sleep(0.6)
        assert snapshot(window) == frozen, "animation advanced during help"
        key(window, "Escape")
        samples = []
        for _ in range(4):
            samples.append(snapshot(window))
            time.sleep(0.15)
        assert len(set(samples)) > 1, "animation failed to resume"
        finish(process, window, log)
        print("UI tests passed: input isolation, rendering, resize, fullscreen, reload, playback")
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        wm.terminate()
        wm.wait(timeout=5)
        wm_log.close()
