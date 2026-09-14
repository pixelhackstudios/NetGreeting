#!/usr/bin/env python3
"""Place a NetGreeting call against a listening instance and exchange chat."""
import json
import socket
import struct
import sys
import time

HELLO, HELLO_ACK, CALL_INVITE, CALL_ACCEPT, CALL_REJECT, HANGUP = 1, 2, 3, 4, 5, 6
CHAT = 10


def frame(msg_type: int, payload: bytes) -> bytes:
    body = bytes([msg_type]) + payload
    return struct.pack(">I", len(body)) + body


def read_msg(sock: socket.socket):
    hdr = recvall(sock, 4)
    if not hdr:
        return None, None
    (length,) = struct.unpack(">I", hdr)
    body = recvall(sock, length)
    return body[0], body[1:]


def recvall(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 1720
    hello = json.dumps({"name": "SmokeBot", "version": 1, "listenPort": 0}).encode()
    sock = socket.create_connection((host, port), timeout=5)
    sock.settimeout(5)
    sock.sendall(frame(HELLO, hello))
    sock.sendall(frame(CALL_INVITE, hello))

    got_accept = False
    deadline = time.time() + 5
    while time.time() < deadline:
        try:
            t, payload = read_msg(sock)
        except socket.timeout:
            break
        if t is None:
            break
        print(f"recv type={t} bytes={len(payload)}")
        if t == CALL_ACCEPT:
            got_accept = True
            break

    if not got_accept:
        print("FAIL: no CallAccept", file=sys.stderr)
        return 1

    sock.sendall(frame(CHAT, json.dumps({"from": "SmokeBot", "text": "Hello from NetGreeting smoke"}).encode()))
    time.sleep(0.2)
    sock.sendall(frame(HANGUP, b"smoke done"))
    sock.close()
    print("OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
