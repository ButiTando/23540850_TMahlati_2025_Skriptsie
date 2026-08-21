"""Cross-platform socket helpers: address reuse, LAN address, keepalive."""

import socket
import sys

# Detect a dead peer after roughly 15 s instead of the OS default (hours).
KEEPALIVE_IDLE = 5      # seconds of silence before the first probe
KEEPALIVE_INTVL = 5     # seconds between probes
KEEPALIVE_CNT = 2       # failed probes before the link is declared dead


def local_lan_ip(fallback="127.0.0.1"):
    """Return the LAN address, via the routing table.

    gethostbyname(gethostname()) returns 127.0.1.1 on Debian/Ubuntu.
    Nothing is actually sent.
    """
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("192.168.7.1", 9))
        return probe.getsockname()[0]
    except OSError:
        try:
            probe.connect(("8.8.8.8", 80))
            return probe.getsockname()[0]
        except OSError:
            return fallback
    finally:
        probe.close()


def _allow_rebind(sock):
    """Allow rebinding a port still in TIME_WAIT.

    Skipped on Windows, where SO_REUSEADDR lets a stale process steal traffic.
    """
    if sys.platform != "win32":
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)


def enable_keepalive(sock):
    """Turn on TCP keepalive with a short, portable timeout."""
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    for name, value in (("TCP_KEEPIDLE", KEEPALIVE_IDLE),
                        ("TCP_KEEPINTVL", KEEPALIVE_INTVL),
                        ("TCP_KEEPCNT", KEEPALIVE_CNT)):
        option = getattr(socket, name, None)
        if option is not None:
            try:
                sock.setsockopt(socket.IPPROTO_TCP, option, value)
            except OSError:
                pass
    # Windows exposes the same tuning only through this ioctl.
    if sys.platform == "win32" and hasattr(sock, "ioctl"):
        try:
            sock.ioctl(socket.SIO_KEEPALIVE_VALS,
                       (1, KEEPALIVE_IDLE * 1000, KEEPALIVE_INTVL * 1000))
        except OSError:
            pass


def make_tcp_listener(host, port, backlog=5):
    """Create a bound, listening TCP socket that survives a restart."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        _allow_rebind(sock)
        sock.bind((host, port))
        sock.listen(backlog)
    except OSError:
        sock.close()
        raise
    return sock


def make_udp_socket(host, port):
    """Create a bound UDP socket that survives a restart."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        _allow_rebind(sock)
        sock.bind((host, port))
    except OSError:
        sock.close()
        raise
    return sock


def close_quietly(sock):
    """Close a socket, waking any thread blocked on it, ignoring errors."""
    if sock is None:
        return
    try:
        sock.shutdown(socket.SHUT_RDWR)
    except OSError:
        pass
    try:
        sock.close()
    except OSError:
        pass
