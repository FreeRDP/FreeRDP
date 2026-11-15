# Experimental HTTP RD Gateway tunnel reuse

This patch is based on FreeRDP 3.33.0. It lets broker redirection replace the
RDP channel within an existing HTTP gateway tunnel. It is opt-in:

```sh
FREERDP_RDG_REUSE_TUNNEL=1 /path/to/patched/xfreerdp3 [your existing options]
```

The gateway must use the two-connection HTTP transport. WebSocket, RPC,
ARM, and GatewayUrl transports keep their existing behavior. Only broker
redirection uses this path; an ordinary disconnect still closes the tunnel.

## Applying the patch

The full `freerdp-3.33.0-rdg-tunnel-reuse-v2.patch` applies to a clean 3.33.0
tree. If the first tunnel-reuse patch is already applied, reverse that patch
before applying v2. Do not apply the full v2 patch over the first patch.

An alternative `freerdp-3.33.0-rdg-tunnel-reuse-v1-to-v2.patch` upgrades a tree
with the original tunnel-reuse patch already applied. Use exactly one route.

Rebuild the same targets and installation prefix used for the first version.
Do not mix these libraries with a different FreeRDP build when testing.

## Response budgets and failure behavior

`GatewayResponseTimeout` bounds each close or open phase, including sending
and draining its control packets. Its default in 3.33.0 is 15000 milliseconds.
For example, add `timeout:60000` to your existing `/gateway:` options to allow
60 seconds per phase. This setting must be positive. `TcpConnectTimeout`
continues to govern socket establishment only.

The client waits for the response to its own close request even when the server
also sends a close request. It acknowledges the server's request, drains that
acknowledgement, and then finishes waiting for its own response. Old channel
data is discarded while closing. A new channel is only accepted for `S_OK`.

Buffered control packets are drained before waiting for a response; temporary
write blocking and partial writes are retried within the same phase budget.
Cancellation is checked throughout the waits. Diagnostics distinguish sending
or draining a request from waiting for its response, and preserve gateway
result codes.

A tunnel that is ineligible for reuse uses the original connection path. Once
a reuse attempt starts, a timeout, gateway result, or protocol failure ends
that connection attempt. There is deliberately no automatic retry with a fresh
tunnel, so a failed attempt cannot silently cause a second MFA exchange or mask
the original gateway result. To test the original path, start a new connection
with `FREERDP_RDG_REUSE_TUNNEL=0`.

## Regression tests

Use the same build options and dependencies as your normal build, additionally
setting `BUILD_TESTING=ON` and `BUILD_TESTING_INTERNAL=ON`. Rebuild `TestCore`:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_TESTING_INTERNAL=ON
cmake --build build --target TestCore -j
ctest --test-dir build -R '^(TestRdg|TestDisconnect|TestRdstls)$' --output-on-failure
```

`TestRdg` simulates the gateway at the BIO boundary, using FreeRDP's actual
buffered socket BIO. Replies become available only when the client request has
actually been drained. It exercises identity and chunked HTTP responses,
fragmented reads, partial and blocked writes, old channel data, simultaneous
closes with and without an acknowledgement, service messages, keepalives,
nonzero channel results, delayed replies, phase deadlines, cancellation during
close and open, an already-closed channel, transport cleanup after failure,
and two successive detach/reset/reopen cycles.

These tests do not exercise a live gateway or negotiate real TLS. Retest the
patched client against your gateway before using it routinely.
