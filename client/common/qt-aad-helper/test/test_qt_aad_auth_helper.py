#!/usr/bin/env python3
# FreeRDP: A Remote Desktop Protocol Implementation
# Integration test for the out-of-process Qt-based AAD auth helper
#
# Copyright 2026 David Fort <contact@hardening-consulting.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""
Drives the real freerdp-qt-aad-helper binary over its JSON-RPC protocol (see
client/common/aad-auth-helper-protocol.md) against a local HTTP redirect fixture, under
QT_QPA_PLATFORM=offscreen. The protocol travels over a dedicated pair of pipes handed to the
helper via --cmdInFd=/--cmdOutFd= command line arguments (not stdin/stdout - see
client/common/aad_helper.c), so this test builds those pipes itself and passes them the same way
FreeRDP does (see HelperProcess below for the platform-specific handle encoding/inheritance).

This exists to catch, as a regression test, two bugs found while developing the helper: AAD's
native-broker redirect_uri uses the non-standard "ms-appx-web" scheme, which QtWebEngine (1)
never surfaces to navigationRequested() at all unless the scheme is registered first (Chromium
instead hands it off to the desktop environment and the redirect is lost), and (2) - even once
registered - refused with net::ERR_UNSAFE_REDIRECT to let a real https-shaped redirect into it
unless the scheme is also flagged CORS-enabled (see kBrokerScheme's comment in main.cpp for
both). A plain http(s) redirect_uri is exercised too, as a regression check that fix didn't
break the common case.

If the helper doesn't answer the initial "hello" handshake at all, the environment is assumed
unable to run Qt WebEngine (e.g. a minimal CI image missing GL/X11 libs) and the test is skipped
rather than failed.
"""
import http.server
import json
import os
import queue
import subprocess
import sys
import threading
import time

if sys.platform == "win32":
    import msvcrt

HELLO_TIMEOUT = 20
REQUEST_TIMEOUT = 20
SKIP_EXIT_CODE = 125

BROKER_TARGET = "ms-appx-web://microsoft.aad.brokerplugin/test-client-id?code=TESTCODE123"


class RedirectHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/redirect"):
            self.send_response(302)
            self.send_header("Location", BROKER_TARGET)
            self.end_headers()
        else:
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<html><body>ok</body></html>")

    def log_message(self, fmt, *args):
        pass


class HelperProcess:
    def __init__(self, helper_path, env):
        # the JSON-RPC channel: two pipes, their read/write ends handed to the helper via
        # --cmdInFd=/--cmdOutFd= - not stdin/stdout, which are left as a normal passthrough so
        # the helper's own Qt/Chromium diagnostic output doesn't collide with the protocol. The
        # value must be encoded exactly like winpr_exportHandleToString() does it (see
        # winpr/libwinpr/handle/handle.c), which differs by platform: on POSIX it's a 'P' type
        # tag followed by the fd in hex; on Windows (no type tag there) it's the native HANDLE
        # value in hex - a Windows CRT fd from os.pipe() is not that HANDLE, so it must be
        # resolved via msvcrt.get_osfhandle() first, and marked inheritable since Windows has no
        # equivalent of POSIX's pass_fds.
        cmd_in_r, cmd_in_w = os.pipe()
        cmd_out_r, cmd_out_w = os.pipe()

        if sys.platform == "win32":
            cmd_in_handle = msvcrt.get_osfhandle(cmd_in_r)
            cmd_out_handle = msvcrt.get_osfhandle(cmd_out_w)
            os.set_handle_inheritable(cmd_in_handle, True)
            os.set_handle_inheritable(cmd_out_handle, True)
            cmd_in_arg = f"--cmdInFd={cmd_in_handle:x}"
            cmd_out_arg = f"--cmdOutFd={cmd_out_handle:x}"
            # pass_fds is POSIX-only; close_fds=False lets CreateProcess(bInheritHandles=TRUE)
            # inherit every inheritable handle in this process, which - since os.pipe() creates
            # non-inheritable handles by default and only the two above were just flipped to
            # inheritable - is just these two.
            popen_kwargs = {"close_fds": False}
        else:
            cmd_in_arg = f"--cmdInFd=P{cmd_in_r:x}"
            cmd_out_arg = f"--cmdOutFd=P{cmd_out_w:x}"
            popen_kwargs = {"close_fds": True, "pass_fds": (cmd_in_r, cmd_out_w)}

        self.proc = subprocess.Popen(
            [helper_path, cmd_in_arg, cmd_out_arg],
            stdin=subprocess.DEVNULL,
            stdout=None,
            stderr=None,
            env=env,
            **popen_kwargs,
        )
        os.close(cmd_in_r)
        os.close(cmd_out_w)
        self._in = os.fdopen(cmd_in_w, "w", buffering=1)
        self._out = os.fdopen(cmd_out_r, "r")
        self._lines = queue.Queue()
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()
        self._next_id = 0

    def _read_loop(self):
        for line in self._out:
            line = line.strip()
            if line:
                self._lines.put(line)

    def request(self, method, params=None, timeout=REQUEST_TIMEOUT):
        self._next_id += 1
        req_id = self._next_id
        msg = {"jsonrpc": "2.0", "id": req_id, "method": method}
        if params is not None:
            msg["params"] = params
        self._in.write(json.dumps(msg) + "\n")
        self._in.flush()

        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                line = self._lines.get(timeout=remaining)
            except queue.Empty:
                break
            reply = json.loads(line)
            if reply.get("id") == req_id:
                return reply
            # notification or stray reply for a different id: ignore, keep waiting
        raise TimeoutError(f"no reply to {method!r} within {timeout}s")

    def notify(self, method, params=None):
        msg = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            msg["params"] = params
        self._in.write(json.dumps(msg) + "\n")
        self._in.flush()

    def wait(self, timeout=10):
        return self.proc.wait(timeout=timeout)


def skip(message):
    print(f"SKIP: {message}")
    sys.exit(SKIP_EXIT_CODE)


def fail(message):
    print(f"FAIL: {message}")
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        fail("usage: test_qt_aad_auth_helper.py <path-to-freerdp-qt-aad-helper>")
    helper_path = sys.argv[1]
    if not os.path.isfile(helper_path):
        fail(f"helper binary not found: {helper_path}")

    server = http.server.HTTPServer(("127.0.0.1", 0), RedirectHandler)
    port = server.server_port
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()

    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["QT_QUICK_BACKEND"] = "software"
    env["QTWEBENGINE_CHROMIUM_FLAGS"] = "--disable-gpu --disable-software-rasterizer"

    helper = HelperProcess(helper_path, env)
    try:
        try:
            hello = helper.request(
                "hello", {"protocol_version": 1, "client": "freerdp"}, timeout=HELLO_TIMEOUT
            )
        except TimeoutError:
            skip("helper did not answer 'hello' - environment likely can't run Qt WebEngine")
        if "error" in hello:
            fail(f"hello returned an error: {hello['error']}")
        print(f"hello ok: {hello['result']}")

        # Case 1: real HTTP redirect into AAD's ms-appx-web native-broker scheme - the exact
        # shape that broke twice during development (scheme not observed at all, then
        # ERR_UNSAFE_REDIRECT).
        nav = helper.request(
            "navigate",
            {
                "title": "test",
                "url": f"http://127.0.0.1:{port}/redirect",
                "redirect_uri": "ms-appx-web://microsoft.aad.brokerplugin/test-client-id",
                "timeout_ms": REQUEST_TIMEOUT * 1000,
            },
        )
        if "error" in nav:
            fail(f"navigate (broker redirect) failed: {nav['error']}")
        got = nav["result"].get("redirect_url")
        if got != BROKER_TARGET:
            fail(f"navigate (broker redirect): expected {BROKER_TARGET!r}, got {got!r}")
        print("broker redirect ok")

        # Case 2: plain http(s)-shaped redirect_uri - regression check, must keep working.
        plain_url = f"http://127.0.0.1:{port}/"
        nav2 = helper.request(
            "navigate",
            {
                "title": "test",
                "url": plain_url,
                "redirect_uri": plain_url,
                "timeout_ms": REQUEST_TIMEOUT * 1000,
            },
        )
        if "error" in nav2:
            fail(f"navigate (plain redirect_uri) failed: {nav2['error']}")
        got2 = nav2["result"].get("redirect_url")
        if got2 != plain_url:
            fail(f"navigate (plain redirect_uri): expected {plain_url!r}, got {got2!r}")
        print("plain redirect_uri ok")

        shut = helper.request("shutdown")
        if shut.get("result") is not None:
            fail(f"shutdown expected a null result, got {shut}")
        helper.notify("exit")

        rc = helper.wait(timeout=10)
        if rc != 0:
            fail(f"helper exited with code {rc}, expected 0")
        print("shutdown/exit ok")
    finally:
        server.shutdown()
        if helper.proc.poll() is None:
            helper.proc.kill()

    print("PASS")


if __name__ == "__main__":
    main()
