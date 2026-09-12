#!/usr/bin/env python3
"""Prototype: invoked by the desktop environment (via a .desktop file
registered as the handler for x-scheme-handler/ms-appx-web) when a browser
hands off a ms-appx-web:// navigation to the OS.

Pulls the freerdp_id out of the *state* query parameter (Azure wants a
redirect_uri that is exactly ms-appx-web://Microsoft.AAD.BrokerPlugin/<client_id> -
but state is an app-opaque value the IdP just echoes back verbatim, so the helper prefixes
it with "<freerdp_id>." before sending the user to the IdP) and forwards the
whole URL to that session's local listener over a unix socket named
freerdp_aad_<freerdp_id>.

Expected URL shape:
  ms-appx-web://Microsoft.AAD.BrokerPlugin/<client_id>?code=...&state=<freerdp_id>.<original_state>
"""
import os
import socket
import sys
import urllib.parse


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <url>", file=sys.stderr)
        return 1

    url = sys.argv[1]
    parsed = urllib.parse.urlparse(url)

    #with open("/tmp/aad_log.txt", "a") as f:
    #    f.write(f"url={url}\n")
    #    f.write(f"parsed={parsed}\n")

    if parsed.netloc.lower() == "microsoft.aad.brokerplugin":
        state_values = urllib.parse.parse_qs(parsed.query).get("state")
        if not state_values:
            print(f"no state parameter found in {url!r}", file=sys.stderr)
            return 1

        freerdp_id = state_values[0].split(".", 1)[0]
        runtime_dir = os.environ.get("XDG_RUNTIME_DIR", "/tmp")
        sock_path = os.path.join(runtime_dir, f"freerdp_aad_{freerdp_id}.sock")

        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
                sock.connect(sock_path)
                sock.sendall(url.encode() + b"\n")
        except OSError as exc:
            print(f"could not reach {sock_path}: {exc}", file=sys.stderr)
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
