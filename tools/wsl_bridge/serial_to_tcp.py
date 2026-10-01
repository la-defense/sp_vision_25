#!/usr/bin/env python3
"""Transparent Windows COM <-> TCP bridge for the original SP serial protocol."""

import argparse
import ipaddress
import select
import socket

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM14", help="MC-02 USB CDC port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--tcp-port", type=int, default=5002)
    parser.add_argument("--allow-peer", required=True, help="Only accept this WSL client IP")
    args = parser.parse_args()
    try:
        allowed_peer = str(ipaddress.ip_address(args.allow_peer))
    except ValueError as exc:
        parser.error(f"invalid --allow-peer IP address: {exc}")

    with serial.Serial(args.port, args.baud, timeout=0, write_timeout=1) as com:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((args.host, args.tcp_port))
            server.listen(1)
            print(f"serial bridge ready: {args.port} <-> {args.host}:{args.tcp_port}")
            while True:
                client, peer = server.accept()
                if peer[0] != allowed_peer:
                    print(f"rejected client {peer[0]} (allowed: {allowed_peer})")
                    client.close()
                    continue
                print(f"WSL client connected: {peer}")
                with client:
                    client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    client.settimeout(0.005)
                    while True:
                        readable, _, _ = select.select([client], [], [], 0.002)
                        if readable:
                            data = client.recv(4096)
                            if not data:
                                break
                            com.write(data)
                        waiting = com.in_waiting
                        if waiting:
                            client.sendall(com.read(min(waiting, 4096)))
                print("WSL client disconnected")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("stopping serial bridge")
