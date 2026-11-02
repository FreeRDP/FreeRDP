# X11 RemoteApp geometry and regression checks

An `.rdp` file can request a fixed desktop smaller than the local X11 screen.
The negotiated RDP canvas, the server window origin and the local WM placement
are distinct coordinate spaces. Selecting `/workarea` changes the requested
desktop geometry; it should not be necessary to make a fixed-size profile work.

## Geometry ownership

- Intersect the local work area with the negotiated desktop before serializing
  unsigned 16-bit RAIL edges. Keep valid panel insets and suppress empty or
  unrepresentable intersections without wrapping negative coordinates.
- Fixed-canvas size hints constrain local maximization. Existing application
  minima and stricter maxima remain authoritative. Dynamic-resolution profiles
  retain their previous size-hint path. A WM which initially ignores a maximum
  can re-evaluate the existing hints after the oversized maximize is observed;
  reapplying them must preserve minima, aspect ratios and increments.
- Ordinary windows use the root position recorded in each core X11/XInput2
  event. Combining queued event-local coordinates with later RAIL metadata or
  translating against a later X11 placement can reverse input during dragging.
- Independently placed maximized windows and non-activating popups use their
  published source origin, including the maximized-surface inset. Owned popups
  follow their owner's actual local placement but retain source input offsets.
- One window-owned pending tuple records the latest server-requested local
  position/size. Obsolete configure notifications must not echo an old
  `ClientWindowMove` while that tuple is unacknowledged. A no-op does not create
  a pending fence; after acknowledgement, genuine local WM changes can again
  send ordinary geometry feedback. There is no queue or blocking wait. A WM
  which refuses the tuple does not receive a guessed acknowledgement.

The current server-size burst handling, repaint/surface lifecycle and deferred
Alt handling remain in place. These changes do not introduce a new RAIL order,
server policy or authentication mode.

## Focused build and tests

Use an out-of-tree build and the normal platform dependencies. For example,
with X11 and the optional test dependencies installed:

```sh
cmake -S . -B ../FreeRDP-build-x11 \
  -DWITH_CLIENT=ON -DWITH_X11=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON -DBUILD_TESTING_INTERNAL=ON -DEXPORT_ALL_SYMBOLS=ON
cmake --build ../FreeRDP-build-x11 --parallel 4 --target \
  xfreerdp TestCore TestX11RailWorkArea TestX11RailSizeHints \
  TestX11RailPointer TestX11DeferredAlt TestX11CmdLine
ctest --test-dir ../FreeRDP-build-x11 --output-on-failure \
  -R '^(TestTransportLayerBIO|TestX11(Rail.*|DeferredAlt|CmdLine))$'
```

`TestX11RailSizeHints` reads real X11 properties and uses `xvfb-run` when
available; without a display it explicitly skips with code 77. The other new geometry
tests require no RDP server. The deferred-Alt test interposes client/input
symbols and should use shared core libraries; its mocks conflict with those
symbols in a fully static build.

`TestTransportLayerBIO` checks the optional OpenSSL KTLS capability queries
against the actual callback BIO. A generic callback layer reports KTLS send and
receive unavailable (zero), without treating the probes as unsupported errors;
ordinary flush/close controls must still work. Availability of kernel TLS is
not added or advertised by this compatibility correction. Unsupported commands
and FD-access controls must still fail; the regression includes those checks.

## Runtime qualification

Use the exact built client and intended `.rdp` security mode with trusted
certificate validation. Keep passwords out of argv, profiles and logs; use
interactive authentication or a protected stdin pipe with `/from-stdin:force`.

Exercise actual source/client window movement, not just a settled screenshot.
Include slow and fast large-step dragging with direction changes, maximize,
restore, menu placement and real input. A zero-reversal metric is invalid if
the source/client window did not actually move. Record source/client provenance
and preserve failed attempts. This focused coverage does not establish broad
Windows-server, WM, mixed-DPI, multimonitor or production support.
