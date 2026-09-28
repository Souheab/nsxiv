#!/bin/sh
# Run X11 tests on an isolated display, never on the user's desktop.
set -eu
tmp=$(mktemp -d)
server=
trap 'if [ -n "$server" ]; then kill "$server" 2>/dev/null || :; wait "$server" 2>/dev/null || :; fi; rm -rf "$tmp"' EXIT HUP INT TERM
Xvfb -displayfd 3 -screen 0 1280x900x24 -nolisten tcp 3>"$tmp/display" >"$tmp/server.log" 2>&1 &
server=$!
tries=0
while [ ! -s "$tmp/display" ]; do
    tries=$((tries + 1))
    if [ "$tries" -gt 50 ] || ! kill -0 "$server" 2>/dev/null; then
        tail -30 "$tmp/server.log" >&2
        exit 1
    fi
    sleep 0.1
done
DISPLAY=:$(cat "$tmp/display") "$@"
