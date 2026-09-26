#!/usr/bin/env bash
#
# Run the simulated rig and expose its HTTP API to this machine.
#
# The Host build of the firmware talks to a TAP device, and creating one needs
# CAP_NET_ADMIN. We get that without root by running inside an unprivileged
# user + network namespace (unshare -Urn) -- but that namespace has its own
# loopback, so nothing outside it (your browser, the Vite dev server) can reach
# 192.168.7.2.
#
# A network namespace does NOT isolate the filesystem, so a Unix domain socket
# is visible on both sides of the boundary. socat carries HTTP and the data
# stream across it:
#
#   [ns] rig 192.168.7.2:80  <- socat -> /run/.../rig-http.sock  <- socat -> 127.0.0.1:8080  [host]
#   [ns] rig 192.168.7.2:5000<- socat -> /run/.../rig-stream.sock<- socat -> 127.0.0.1:8500  [host]
#
# Usage: tools/dev-rig.sh [--stop]
# Then:  curl http://127.0.0.1:8080/api/status.json

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN="${XDG_RUNTIME_DIR:-/tmp}/ilt-rig"
HTTP_SOCK="$RUN/rig-http.sock"
STREAM_SOCK="$RUN/rig-stream.sock"
HTTP_PORT=8080
STREAM_PORT=8500
LOG="$RUN/rig.log"
PIDS="$RUN/pids"

stop() {
    if [ -f "$PIDS" ]; then
        while read -r pid; do kill "$pid" 2>/dev/null || true; done < "$PIDS"
        rm -f "$PIDS"
    fi
    rm -f "$HTTP_SOCK" "$STREAM_SOCK"
    echo "rig stopped"
}

if [ "${1:-}" = "--stop" ]; then stop; exit 0; fi

command -v socat >/dev/null || { echo "socat is required: sudo apt install socat" >&2; exit 1; }
[ -x "$HERE/build/Host/FirmwareUpdate" ] || {
    echo "host build missing; run: cmake --build $HERE/build/Host" >&2; exit 1; }

stop >/dev/null 2>&1 || true
mkdir -p "$RUN"
: > "$PIDS"

# Everything network-side runs in one namespace: the rig plus the two socat
# listeners that publish it onto Unix sockets. They must share the namespace,
# so they are started from a single unshare.
unshare -Urn --map-root-user bash -c "
    set -e
    ip link set lo up
    ip tuntap add dev tap0 mode tap
    ip addr add 192.168.7.1/24 dev tap0
    ip link set tap0 up

    ILT_SIMULATE=1 PRECONFIGURED_TAPIF=tap0 '$HERE/build/Host/FirmwareUpdate' &

    # Wait for the stack to answer before publishing the sockets, so a client
    # that connects immediately does not get a closed pipe.
    for _ in \$(seq 1 80); do
        if printf 'GET /api/status.json HTTP/1.0\r\n\r\n' \
           | socat -T2 - TCP:192.168.7.2:80 >/dev/null 2>&1; then break; fi
        sleep 0.25
    done

    socat UNIX-LISTEN:'$HTTP_SOCK',fork,unlink-early TCP:192.168.7.2:80 &
    socat UNIX-LISTEN:'$STREAM_SOCK',fork,unlink-early TCP:192.168.7.2:5000 &
    wait
" > "$LOG" 2>&1 &
echo $! >> "$PIDS"

# Host side: wait for the namespace to publish the sockets, then re-expose them
# as ordinary TCP ports.
for _ in $(seq 1 120); do [ -S "$HTTP_SOCK" ] && break; sleep 0.25; done
[ -S "$HTTP_SOCK" ] || { echo "rig did not come up; see $LOG" >&2; tail -20 "$LOG" >&2; exit 1; }

socat TCP-LISTEN:$HTTP_PORT,fork,reuseaddr,bind=127.0.0.1 UNIX-CONNECT:"$HTTP_SOCK" \
    >> "$LOG" 2>&1 &
echo $! >> "$PIDS"
socat TCP-LISTEN:$STREAM_PORT,fork,reuseaddr,bind=127.0.0.1 UNIX-CONNECT:"$STREAM_SOCK" \
    >> "$LOG" 2>&1 &
echo $! >> "$PIDS"

for _ in $(seq 1 40); do
    curl -sf --max-time 1 -o /dev/null "http://127.0.0.1:$HTTP_PORT/api/status.json" && break
    sleep 0.25
done

echo "rig API   http://127.0.0.1:$HTTP_PORT"
echo "rig stream tcp 127.0.0.1:$STREAM_PORT"
echo "log        $LOG"
echo "stop       tools/dev-rig.sh --stop"
