#!/usr/bin/env python3
""" XDG "native browser" AAD auth helper.


"""
import json
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
import urllib.parse
import uuid

DEFAULT_TIMEOUT_MS = 180000
BROKER_SCHEME = "ms-appx-web://"
DESKTOP_FILE_NAME = "freerdp-aad-handler.desktop"


def locate_handler_path() -> str | None:
    """Finds the absolute path to freerdp-aad-handler, the script the OS invokes for
    ms-appx-web:// redirects: first right next to this script (where an install puts it, and
    also where it sits when both scripts are just run straight out of this source directory),
    then falls back to PATH."""
    here = os.path.dirname(os.path.abspath(__file__))
    for name in ("freerdp-aad-handler", "freerdp_aad_handler.py"):
        candidate = os.path.join(here, name)
        if os.path.isfile(candidate):
            return candidate
    return shutil.which("freerdp-aad-handler")


def build_desktop_entry(handler_path: str) -> bytes:
    return (
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=FreeRDP AAD redirect handler\n"
        f"Exec={handler_path} %u\n"
        "MimeType=x-scheme-handler/ms-appx-web;\n"
        "NoDisplay=true\n"
        "StartupNotify=false\n"
        "Terminal=false\n"
    ).encode()


def ensure_handler_installed() -> None:
    """Best-effort: makes sure freerdp-aad-handler.desktop is present in the user's own
    applications directory and registered as the default handler for
    x-scheme-handler/ms-appx-web, so the IdP's ms-appx-web:// broker redirect actually gets
    routed back to us. Generates the .desktop content itself (rather than depending on a
    CMake-installed template) so this self-heals regardless of packaging - including when run
    straight out of the build tree, with no `cmake --install` ever having happened. Failures
    here are logged but not fatal: navigate() will simply time out later for ms-appx-web://
    redirects if registration didn't take."""
    handler_path = locate_handler_path()
    if not handler_path:
        print("[xdg-aad-helper] warning: could not locate freerdp-aad-handler next to this "
              "script or on PATH - ms-appx-web:// redirects will not be caught", file=sys.stderr)
        return

    data_home = os.environ.get("XDG_DATA_HOME") or os.path.join(
        os.path.expanduser("~"), ".local", "share")
    apps_dir = os.path.join(data_home, "applications")
    dst = os.path.join(apps_dir, DESKTOP_FILE_NAME)
    desired = build_desktop_entry(handler_path)

    try:
        os.makedirs(apps_dir, exist_ok=True)
        current = None
        if os.path.isfile(dst):
            with open(dst, "rb") as f:
                current = f.read()
        if current != desired:
            with open(dst, "wb") as f:
                f.write(desired)
            subprocess.run(["update-desktop-database", apps_dir], check=False,
                            capture_output=True)
    except OSError as exc:
        print(f"[xdg-aad-helper] warning: could not install {dst!r}: {exc}", file=sys.stderr)
        return

    try:
        query = subprocess.run(
            ["xdg-mime", "query", "default", "x-scheme-handler/ms-appx-web"],
            check=False, capture_output=True, text=True)
        if query.stdout.strip() != DESKTOP_FILE_NAME:
            subprocess.run(["xdg-mime", "default", DESKTOP_FILE_NAME,
                             "x-scheme-handler/ms-appx-web"], check=False)
    except OSError as exc:
        print(f"[xdg-aad-helper] warning: could not register as the default handler for "
              f"x-scheme-handler/ms-appx-web: {exc}", file=sys.stderr)


def import_handle(arg_value: str) -> int:
    """Reverses winpr_exportHandleToString() for the POSIX pipe case: the
    value is 'P<hex-fd>' ('P' = HANDLE_TYPE_ANONYMOUS_PIPE, see
    winpr/libwinpr/handle/handle.c)."""
    if not arg_value.startswith("P"):
        raise ValueError(f"unsupported handle type tag in {arg_value!r}")
    return int(arg_value[1:], 16)


class JsonRpcChannel:
    """Newline-delimited JSON over the raw cmdIn/cmdOut pipe fds."""

    def __init__(self, fd_in: int, fd_out: int):
        self._in = os.fdopen(fd_in, "rb", buffering=0)
        self._out = os.fdopen(fd_out, "wb", buffering=0)
        self._out_lock = threading.Lock()
        self._buf = b""

    def read_message(self):
        """Returns the next parsed JSON object, {} for an unparsable line
        (silently ignored per the protocol notes), or None on EOF."""
        while b"\n" not in self._buf:
            chunk = self._in.read(4096)
            if not chunk:
                return None
            self._buf += chunk
        line, _, self._buf = self._buf.partition(b"\n")
        if not line.strip():
            return {}
        try:
            return json.loads(line)
        except ValueError:
            return {}

    def send(self, message: dict) -> None:
        data = (json.dumps(message) + "\n").encode()
        with self._out_lock:
            self._out.write(data)


def make_result(msg_id, result):
    return {"jsonrpc": "2.0", "id": msg_id, "result": result}


def make_error(msg_id, code, message):
    return {"jsonrpc": "2.0", "id": msg_id, "error": {"code": code, "message": message}}


def rewrite_state(url: str, freerdp_id: str) -> tuple[str, bool]:
    """Prefixes the /authorize url's state query parameter with
    "<freerdp_id>.", adding one if none was present. Returns the rewritten
    url and whether an original state value existed (so restore_state() can
    reverse this exactly, including the "there was no state at all" case)."""
    parsed = urllib.parse.urlparse(url)
    qs = urllib.parse.parse_qsl(parsed.query, keep_blank_values=True)
    had_state = False
    rewritten = []
    for key, value in qs:
        if key == "state":
            had_state = True
            rewritten.append((key, f"{freerdp_id}.{value}"))
        else:
            rewritten.append((key, value))
    if not had_state:
        rewritten.append(("state", freerdp_id))
    new_query = urllib.parse.urlencode(rewritten)
    return urllib.parse.urlunparse(parsed._replace(query=new_query)), had_state


def restore_state(redirect_url: str, freerdp_id: str, had_original_state: bool) -> str:
    """Reverses rewrite_state() on the redirect the IdP sent back, so FreeRDP
    only ever sees the state value (or absence of one) it originally set."""
    parsed = urllib.parse.urlparse(redirect_url)
    qs = urllib.parse.parse_qsl(parsed.query, keep_blank_values=True)
    prefix = freerdp_id + "."
    restored = []
    for key, value in qs:
        if key != "state":
            restored.append((key, value))
            continue
        if not had_original_state and value == freerdp_id:
            continue  # FreeRDP never set a state - drop the param entirely
        if value.startswith(prefix):
            restored.append((key, value[len(prefix):]))
        else:
            print(f"[xdgopen-aad-helper] warning: state={value!r} missing expected "
                  f"freerdp_id prefix {prefix!r}, passing through as-is", file=sys.stderr)
            restored.append((key, value))
    new_query = urllib.parse.urlencode(restored)
    return urllib.parse.urlunparse(parsed._replace(query=new_query))


class NavigateState:
    def __init__(self, freerdp_id: str):
        self.freerdp_id = freerdp_id
        self.had_original_state = False
        self.cancelled = threading.Event()


class Session:
    def __init__(self, channel: JsonRpcChannel):
        self.channel = channel
        self.lock = threading.Lock()
        self.current: NavigateState | None = None
        self.shutting_down = False

    def navigate(self, msg_id, params: dict) -> None:
        with self.lock:
            if self.shutting_down:
                self.channel.send(make_error(msg_id, 1, "shutting_down"))
                return
            if self.current is not None:
                self.channel.send(make_error(msg_id, 1, "navigate_already_in_progress"))
                return
            state = NavigateState(uuid.uuid4().hex[:12])
            self.current = state

        title = params.get("title", "")
        url = params.get("url", "")
        redirect_uri = params.get("redirect_uri", "")
        timeout_ms = params.get("timeout_ms") or DEFAULT_TIMEOUT_MS

        if not redirect_uri.lower().startswith(BROKER_SCHEME):
            with self.lock:
                self.current = None
            self.channel.send(make_error(msg_id, 1, "unsupported_redirect_scheme"))
            return

        effective_url, state.had_original_state = rewrite_state(url, state.freerdp_id)

        runtime_dir = os.environ.get("XDG_RUNTIME_DIR", "/tmp")
        sock_path = os.path.join(runtime_dir, f"freerdp_aad_{state.freerdp_id}.sock")
        try:
            os.unlink(sock_path)
        except FileNotFoundError:
            pass

        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        redirect_url = None
        error = None
        try:
            server.bind(sock_path)
            os.chmod(sock_path, 0o600)
            server.listen(1)
            server.settimeout(0.5)

            print(f"[xdgopen-aad-helper] navigate: opening browser for {effective_url!r} "
                  f"(title={title!r}), waiting on {sock_path}", file=sys.stderr)
            subprocess.Popen(["xdg-open", effective_url])

            deadline = time.monotonic() + timeout_ms / 1000.0
            while redirect_url is None and error is None:
                if state.cancelled.is_set():
                    error = "user_cancelled"
                    break
                with self.lock:
                    if self.shutting_down:
                        error = "shutting_down"
                        break
                if time.monotonic() >= deadline:
                    error = "timeout"
                    break
                try:
                    conn, _ = server.accept()
                except socket.timeout:
                    continue
                with conn:
                    conn.settimeout(2.0)
                    data = b""
                    try:
                        while True:
                            chunk = conn.recv(4096)
                            if not chunk:
                                break
                            data += chunk
                    except socket.timeout:
                        pass
                redirect_url = data.decode(errors="replace").strip()
        finally:
            server.close()
            try:
                os.unlink(sock_path)
            except FileNotFoundError:
                pass
            with self.lock:
                self.current = None

        if error:
            self.channel.send(make_error(msg_id, 1, error))
            return

        redirect_url = restore_state(redirect_url, state.freerdp_id, state.had_original_state)

        parsed = urllib.parse.urlparse(redirect_url)
        qs = urllib.parse.parse_qs(parsed.query)
        if "error" in qs:
            message = qs["error"][0]
            if "error_subcode" in qs:
                message += ": " + qs["error_subcode"][0]
            self.channel.send(make_error(msg_id, 1, message))
            return

        self.channel.send(make_result(msg_id, {"status": "ok", "redirect_url": redirect_url}))

    def cancel(self) -> None:
        with self.lock:
            if self.current:
                self.current.cancelled.set()

    def shutdown(self, msg_id) -> None:
        self.channel.send(make_result(msg_id, None))

    def prepare_exit(self) -> None:
        with self.lock:
            self.shutting_down = True
            if self.current:
                self.current.cancelled.set()


def reader_loop(channel: JsonRpcChannel, session: Session) -> None:
    while True:
        msg = channel.read_message()
        if msg is None:
            break
        if not msg:
            continue

        method = msg.get("method")
        msg_id = msg.get("id")

        if method == "hello":
            channel.send(make_result(
                msg_id, {"protocol_version": 1, "helper": "freerdp-xdgopen-aad-helper/1.0"}))
        elif method == "navigate":
            threading.Thread(target=session.navigate, args=(msg_id, msg.get("params") or {}),
                              daemon=True).start()
        elif method == "cancel":
            session.cancel()
        elif method == "shutdown":
            session.shutdown(msg_id)
        elif method == "exit":
            session.prepare_exit()
            break
        # any other method: silently ignored, per the protocol's implementer notes


def main() -> int:
    cmd_in_arg = None
    cmd_out_arg = None
    for arg in sys.argv[1:]:
        if arg.startswith("--cmdInFd="):
            cmd_in_arg = arg[len("--cmdInFd="):]
        elif arg.startswith("--cmdOutFd="):
            cmd_out_arg = arg[len("--cmdOutFd="):]

    if not cmd_in_arg or not cmd_out_arg:
        print(f"usage: {sys.argv[0]} --cmdInFd=<handle> --cmdOutFd=<handle>", file=sys.stderr)
        return 1

    try:
        fd_in = import_handle(cmd_in_arg)
        fd_out = import_handle(cmd_out_arg)
    except ValueError as exc:
        print(f"[xdgopen-aad-helper] {exc}", file=sys.stderr)
        return 1

    ensure_handler_installed()

    channel = JsonRpcChannel(fd_in, fd_out)
    session = Session(channel)
    reader_loop(channel, session)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
