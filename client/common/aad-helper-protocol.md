# AAD auth helper protocol

This document describes the JSON-RPC protocol spoken between FreeRDP and an AAD auth helper
subprocess, over a dedicated pair of pipes (not stdin/stdout — see [Transport](#transport)). The
protocol is helper-implementation-agnostic — any binary that speaks it can be pointed to via
`/azure:auth-helper:<path>` (see `client/SDL/common/sdl_aad_helper.cpp`). Two implementations
exist today:

| Implementation | Directory | CMake option | Binary |
|---|---|---|---|
| Native OS webview (GTK/Cocoa/WebView2) | `client/common/webview-aad-helper/` | `WITH_WEBVIEW_AAD_AUTH_HELPER` | `freerdp-webview-aad-helper` |
| Qt (`QWebEngineView`) | `client/common/qt-aad-helper/` | `WITH_QT_AAD_AUTH_HELPER` | `freerdp-qt-aad-helper` |

- Client-side transport (shared by all implementations): `client/common/aad_helper.c` /
  `include/freerdp/client/aad_helper.h`
- Unit test covering the wire format: `client/common/test/TestClientAadAuthHelperProtocol.c`
- End-to-end integration test for the Qt implementation, run under `ctest` as
  `TestQtAadAuthHelperProtocol` (registered only when `WITH_QT_AAD_AUTH_HELPER=ON` and a Python3
  interpreter is found): `client/common/qt-aad-helper/test/test_qt_aad_auth_helper.py`.
  Spawns the real `freerdp-qt-aad-helper` binary under `QT_QPA_PLATFORM=offscreen` against a
  local HTTP redirect fixture, covering both a plain `https`-shaped `redirect_uri` and a real HTTP
  redirect into AAD's `ms-appx-web://` native-broker scheme (see `kBrokerScheme`'s comment in that
  helper's `main.cpp` for the two Chromium/QtWebEngine issues this guards against). Skips (rather
  than fails) if the helper doesn't answer the initial `hello` handshake at all, since that means
  the environment can't run Qt WebEngine rather than that the helper is broken.

A helper has no OAuth-specific knowledge. It is handed a URL to display in a browser widget and a
`redirect_uri` prefix to watch for, and it reports back whatever URI the browser eventually
navigated to (or an error). All OAuth semantics — building the authorize URL, extracting the code,
exchanging it for a token — stay client-side in `client/common/client.c`.

## Transport

- The JSON-RPC channel travels over two dedicated anonymous pipes. FreeRDP creates them and passes
  the helper's end of each as a command line argument (encoded via `winpr_exportHandleToString()`,
  decoded via `winpr_importHandleFromString()` — see `winpr/include/winpr/handle.h` for the
  encoding, which is an internal detail of that pair of functions, not part of this protocol):
  - `--cmdInFd=<value>` — the helper reads requests/notifications from this one.
  - `--cmdOutFd=<value>` — the helper writes responses/notifications to this one.

  A helper parses both switches at startup and fails immediately if either is missing.
- The helper's own stdin/stdout/stderr are left as a plain passthrough of FreeRDP's own (like any
  normally-spawned child), so a helper's own diagnostic output (Qt/Chromium warnings, WLog, native
  webview toolkit messages, ...) reaches the user's terminal normally instead of risking collision
  with the JSON-RPC stream.
- Messages are newline-delimited JSON: exactly one JSON object per line, no embedded `\n`.
- FreeRDP is always the JSON-RPC client (it assigns request ids); the helper only ever responds to
  requests or sends unsolicited notifications.
- The helper's browser/UI event loop runs on the process' main thread; message handling runs on a
  separate reader thread and is marshalled onto the UI thread internally. This is transparent to
  FreeRDP — requests can be sent and will be answered in order regardless.

Every request and response carries `"jsonrpc": "2.0"`. Requests carry an integer `id` allocated by
FreeRDP (monotonically increasing, starting at 1); notifications omit `id`.

## Requests (FreeRDP → helper)

### `hello`

Sent once immediately after spawning the helper, before anything else. Used as a handshake to
confirm the helper started correctly and speaks a compatible protocol version.

```json
{"jsonrpc":"2.0","id":1,"method":"hello","params":{"protocol_version":1,"client":"freerdp"}}
```

Response:

```json
{"jsonrpc":"2.0","id":1,"result":{"protocol_version":1,"helper":"freerdp-webview-aad-helper/1.0"}}
```

`aad_auth_helper_start()` treats any response containing a `result` object as success; the
`protocol_version`/`helper` fields are informational only today (not version-checked). `helper`
identifies the implementation, e.g. `freerdp-qt-aad-helper/1.0` for the Qt-based one.

### `navigate`

Tells the helper to point its webview at `url` and wait until the browser navigates to a URI
starting with `redirect_uri`, or until `timeout_ms` elapses. Only one `navigate` may be in flight
at a time per helper instance.

```json
{"jsonrpc":"2.0","id":2,"method":"navigate",
 "params":{"title":"Sign in","url":"https://login.microsoftonline.com/...",
           "redirect_uri":"https://login.microsoftonline.com/common/oauth2/nativeclient",
           "timeout_ms":180000}}
```

- `title` — window title for the popup (may be empty).
- `url` — initial URL to load (typically an AAD `/authorize` URL).
- `redirect_uri` — prefix match against the URL-decoded navigation target; matching is
  case-insensitive (see `RedirectWatcher::matches`).
- `timeout_ms` — 0 means "use the helper's default" (180000 ms).

Success response — `result.redirect_url` is the full, verbatim URI the browser navigated to
(FreeRDP extracts the `code`/`error` query parameters from it):

```json
{"jsonrpc":"2.0","id":2,"result":{"status":"ok","redirect_url":"https://.../nativeclient?code=..."}}
```

Failure response:

```json
{"jsonrpc":"2.0","id":2,"error":{"code":1,"message":"timeout"}}
```

`error.message` is one of:

| message | meaning |
|---|---|
| `navigate_already_in_progress` | a previous `navigate` on this helper hasn't completed yet |
| `timeout` | `timeout_ms` elapsed with no matching navigation |
| `user_cancelled` | the popup was closed by the user, or a `$/cancel` notification was received |
| `shutting_down` | the helper is exiting (see `shutdown`/`exit`) |
| `<idp error>[: <idp error_subcode>]` | the IdP redirected to `redirect_uri` with an `error` query parameter (e.g. `access_denied`); `error_subcode`, if present, is appended after `: ` |

### `shutdown`

Requests a clean shutdown acknowledgment before the helper process is torn down. Always answered
with a null result (never an error):

```json
{"jsonrpc":"2.0","id":3,"method":"shutdown"}
{"jsonrpc":"2.0","id":3,"result":null}
```

## Notifications (no `id`, no response expected)

### `exit` (FreeRDP → helper)

Tells the helper to stop reading requests and terminate its webview/event loop. Sent right after
`shutdown` completes. `aad_auth_helper_stop()` then waits up to 3 seconds for the process to exit
before forcibly terminating it.

```json
{"jsonrpc":"2.0","method":"exit"}
```

### `cancel` (FreeRDP → helper)

Cancels the in-flight `navigate` request, if any; the pending `navigate` response resolves with
`error.message = "user_cancelled"`. No-op if nothing is in flight. Not currently sent by the
FreeRDP-side transport shim, but implemented and tested for future use (e.g. a user-initiated
cancel button).

```json
{"jsonrpc":"2.0","method":"$/cancel"}
```

### `log` (helper → FreeRDP)

Forwarded to `WLog_INFO`. Recognized by the client-side transport but not currently emitted by the
helper implementation; reserved for future diagnostic use.

```json
{"jsonrpc":"2.0","method":"log","params":{"message":"..."}}
```

## Session lifecycle

1. FreeRDP spawns the helper and sends `hello`; failure to get a valid `result` tears the process
   down immediately.
2. FreeRDP sends zero or more `navigate` requests over the life of one RDP connection. The helper's
   instance (and its cookies/session) is reused across all of them, so e.g. an AVD gateway
   auth followed by a target-host auth only prompts the user once.
3. On disconnect, FreeRDP sends `shutdown`, waits for its response, then sends `exit` and waits up
   to 3 seconds for the process to exit before calling `TerminateProcess`.

## Notes for implementers

- Any line the helper cannot parse as JSON is silently ignored by the FreeRDP-side reader.
- Responses/notifications with an `id` that doesn't match the currently awaited request are
  dropped with a `WLog_WARN` (defensive against stray or out-of-order lines; in practice the
  helper answers strictly in request order).
- `id` is transported as a JSON number on the wire; FreeRDP treats it as a `UINT32`.
