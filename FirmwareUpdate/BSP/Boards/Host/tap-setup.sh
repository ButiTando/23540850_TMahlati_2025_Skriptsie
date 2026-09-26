#!/bin/sh
# Create the TAP device the Host build talks to. Needs root; run once per boot.
#
#   sudo BSP/Boards/Host/tap-setup.sh up      # create tap0 as 192.168.7.1/24
#   sudo BSP/Boards/Host/tap-setup.sh down    # remove it
#
# The firmware then runs unprivileged:
#   PRECONFIGURED_TAPIF=tap0 ./build/Host/FirmwareUpdate
#
# PRECONFIGURED_TAPIF matters: without it tapif tries to run `ifconfig` itself,
# which needs root for the firmware process too.
set -e

DEV=${DEV:-tap0}
HOST_ADDR=${HOST_ADDR:-192.168.7.1/24}
OWNER=${SUDO_USER:-$(id -un)}

case "$1" in
  up)
    ip tuntap add dev "$DEV" mode tap user "$OWNER"
    ip addr add "$HOST_ADDR" dev "$DEV"
    ip link set "$DEV" up
    echo "$DEV is up as $HOST_ADDR, owned by $OWNER"
    echo "the firmware will appear on 192.168.7.2"
    ;;
  down)
    ip link set "$DEV" down 2>/dev/null || true
    ip tuntap del dev "$DEV" mode tap
    echo "$DEV removed"
    ;;
  *)
    echo "usage: $0 {up|down}" >&2
    exit 1
    ;;
esac
