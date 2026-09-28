# Overlay tests

`make check` builds the focused help tests and runs them on an isolated Xvfb
display. It requires the normal build dependencies and Xvfb. These tests cover
the binding catalogue, current-mode filtering, key aliases, combined actions,
query editing, input limits, and scrolling. Command stubs abort if called.

For the optional UI checks, build nsxiv and run:

```sh
sh tests/run.sh python3 tests/ui.py ./nsxiv
```

This additionally requires Python 3, xdotool, ImageMagick (`import` and
`convert`), and Openbox on PATH. It uses temporary images, configuration, and
cache directories on its own display. It checks input isolation in both modes,
clean restoration after closing, resizing, fullscreen, hidden status bars,
asynchronous redraws, and slideshow/animation pause and resume.

Set `NSXIV_TEST_ARTIFACTS` to an output directory to retain screenshots. The UI
script also accepts an address/undefined-behavior sanitizer build; disable leak
reporting for that run if font/X11 library caches produce exit-time reports.
