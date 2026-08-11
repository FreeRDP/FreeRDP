# FreeRDP macOS Shadow Server: Sonoma and Windows 98 Plan

## Goal

Export the physical macOS Sonoma desktop through FreeRDP's shadow server to
Microsoft Remote Desktop 5.2 on Windows 98 SE, using a sharp 1024x768,
16-bit-color desktop with low interactive latency.

The initial deliverable repairs the existing `CGDisplayStream` backend because
Sonoma still provides that API. ScreenCaptureKit is a follow-on backend for
newer SDKs and macOS releases, not a prerequisite for the Sonoma experiment.

## Non-goals for the First Deliverable

- No H.264, HEVC, AV1, or other video codec path.
- No audio redirection, drive redirection, clipboard, multi-monitor, or Retina
  optimization until the basic desktop is stable.
- No direct Internet listener. The service binds to loopback and travels
  through an SSH tunnel.
- No global weakening of FreeRDP security defaults.

## Current Source Baseline

- Upstream: `FreeRDP/FreeRDP`
- Baseline commit: `9415f2d11e4cbc4e25d3d9fd0c4271e2e05d5c58`
- Development branch: `mac-shadow-sonoma`
- Primary backend: `server/shadow/Mac/mac_shadow.c`
- Backend state: `server/shadow/Mac/mac_shadow.h`
- Legacy bitmap path: `server/shadow/shadow_client.c`
- Server arguments/security negotiation: `server/shadow/shadow_server.c` and
  `server/shadow/cli/shadow.c`

At less than 32-bit client color depth, the generic shadow client already
selects `FREERDP_CODEC_INTERLEAVED`, splits updates into 64x64 rectangles, and
compresses them with the classic bitmap path. That is the principal reason to
repair this backend instead of creating a new remote-desktop protocol.

## Known Defects to Resolve

### Capture callback and dirty regions

The callback currently asks `mac_shadow_capture_get_dirty_region()` to read
`subsystem->lastUpdate` before the callback stores or merges the current
`updateRef`. On the first complete frame, `lastUpdate` is null. Later frames are
merged into persistent state that is never cleared after successful delivery.

The callback also touches `frameSurface` and dirty-region state before checking
that `status == kCGDisplayStreamFrameStatusFrameComplete` and before validating
`frameSurface` and `updateRef`.

### Locking

The surface critical section is left after calculating the invalid region, then
left again after copying pixels without a matching enter. Region extents are
also read outside the lock that protects the region. Capture, pixel copying,
frame notification, and region clearing need an explicit ownership model.

### First frame and reconnect

When there are no clients, the callback returns before copying the current
frame into the shadow surface. A newly connected client can therefore receive
an empty or stale framebuffer until a later display change occurs. A client
refresh must force a full-frame capture/update.

### Lifecycle

`mac_shadow_subsystem_start()` does not retain its worker thread handle.
`mac_shadow_subsystem_stop()` does nothing. The capture stream, worker thread,
dispatch queue, and global subsystem pointer are not shut down and released as
one ordered lifecycle. Restart and process exit can leak or race.

### Input

- Keyboard logic treats `KBD_FLAGS_DOWN` as a positive flag, although an RDP
  key-down is normally represented by the absence of the release flag.
- Unicode keyboard input and synchronize/modifier handling are stubs.
- A move event is posted in the move branch and then a second event is posted
  unconditionally, sometimes with `kCGEventNull`.
- Negative wheel movement divides by 392 while positive movement divides by
  120.
- Extended mouse input is a stub.
- A fresh `CGEventSource` is allocated for nearly every event instead of being
  retained for the subsystem lifetime.

### Sonoma permissions and diagnostics

The backend does not preflight Screen Recording or Accessibility permission.
A missing grant can look like a black screen or dead input. Startup must report
the two capabilities independently and return actionable errors.

## Patch Series

### Patch 0: Baseline and reproducible build

1. Build unmodified FreeRDP on the Sonoma iMac with the macOS shadow subsystem.
2. Record compiler, SDK, architecture, CMake options, installed dependencies,
   binary path, and exact launch command.
3. Confirm whether the stock server starts, captures, accepts a modern RDP
   client, and accepts the Windows 98 RDP 5.2 client.
4. Save debug logs for black screen, first connection, reconnect, and shutdown.

Exit criterion: a repeatable baseline matrix, even if several cells fail.

### Patch 1: Capture callback correctness

1. Reject non-complete statuses before dereferencing frame/update objects.
2. Validate subsystem, server, surface, `frameSurface`, and `updateRef`.
3. Derive dirty rectangles from the callback's current `updateRef`.
4. Remove indefinite `lastUpdate` accumulation; retain update state only if a
   bounded handoff genuinely requires it.
5. Clamp rectangles to the shadow-surface dimensions before conversion.
6. Log malformed or empty updates at an appropriate debug level.

Exit criterion: no first-frame null access; dirty state represents only pending
work.

### Patch 2: Surface locking and frame publication

1. Define the lock boundary for region mutation, extent calculation, surface
   copy, and invalid-region clearing.
2. Eliminate the unmatched `LeaveCriticalSection`.
3. Ensure the encoder never reads pixels while the callback is mutating them.
4. Avoid holding the client-list lock during expensive capture conversion where
   possible.
5. Add assertions or structured cleanup paths for every lock and IOSurface lock.

Exit criterion: Thread Sanitizer/manual stress testing shows no obvious race or
unbalanced lock across rapid screen changes and reconnects.

### Patch 3: First-frame and refresh behavior

1. Keep a valid current framebuffer even while no client is connected, or take
   a one-shot full capture when the first client arrives.
2. Turn `SHADOW_MSG_IN_REFRESH_REQUEST_ID` into a full invalidation plus frame
   publication, not publication of potentially stale pixels.
3. Test a completely static desktop followed by first connection.
4. Test disconnect, static interval, and reconnect without moving the mouse on
   the Mac.

Exit criterion: every connection receives a complete desktop immediately.

### Patch 4: Clean lifecycle

1. Store the worker thread handle and explicit running/stopping state.
2. On stop, signal the message queue, stop `CGDisplayStream`, wait for the worker,
   and prevent new callback work.
3. Release the stream, dispatch queue where required by deployment target, event
   source, retained update objects, and thread handle in one ownership path.
4. Clear `g_Subsystem` safely; preferably replace the global callback dependency
   with per-instance context if the API shape allows it.
5. Make partial initialization unwind correctly.
6. Make repeated start/stop and failed-start sequences deterministic.

Exit criterion: repeated launch/connect/disconnect/terminate cycles leave no
worker, listener, or capture stream behind.

### Patch 5: Keyboard and mouse correctness

1. Interpret key-down as not-release and preserve the extended scan-code bit.
2. Validate failed scan-code/key-code translation before posting.
3. Retain one `CGEventSource` per subsystem.
4. Implement modifier synchronization for Shift, Control, Option/Alt, Caps Lock,
   and Command mapping decisions.
5. Implement Unicode input with `CGEventKeyboardSetUnicodeString` for clients
   that send Unicode events.
6. Post exactly one motion/drag event and one button transition per RDP event.
7. Correct wheel sign and 120-unit normalization; preserve partial deltas if the
   old client sends them.
8. Implement or explicitly reject extended mouse buttons with a diagnostic.

Exit criterion: the Windows 98 client passes a written input matrix, including
typing, shortcuts, drag, right-click, wheel both directions, and stuck-modifier
recovery after disconnect.

### Patch 6: Permission preflight and operator diagnostics

1. Preflight Screen Recording permission before capture startup.
2. Preflight Accessibility permission before input injection.
3. Distinguish capture unavailable, permission denied, stream creation failed,
   and input unavailable in logs and exit status.
4. Add an optional prompt path for an interactive `.app` wrapper, but keep the
   command-line binary useful for already-granted permissions.
5. Document the stable executable identity required for macOS privacy grants.

Exit criterion: a missing grant produces an explicit diagnosis rather than a
black screen or silent input failure.

### Patch 7: Windows 98 compatibility profile

1. Verify the exact command-line switches for classic RDP security, disabled
   NLA/TLS as required by RDP 5.2, 16-bit color, port 3390, and loopback bind.
2. Add a named profile or documented wrapper only if existing switches cannot
   express the configuration clearly.
3. Confirm the client selects interleaved bitmap compression and does not enter
   NSCodec, RemoteFX, AVC, or GFX paths.
4. Keep authentication/security changes local to the explicit legacy profile.
5. Verify the SSH tunnel carries the session without exposing the listener.

Exit criterion: the VAIO connects through the tunnel without weakening the
normal server defaults.

### Patch 8: Performance tuning for 1024x768

1. Instrument capture-to-send time, bytes per update, queued frames, and dropped
   or coalesced frames.
2. Test 8, 12, 16, and 20 FPS caps at 16-bit color.
3. Compare bounding-box publication with multiple dirty rectangles; avoid
   converting a large bounding box when several small regions changed.
4. Detect and discard stale intermediate frames instead of building latency.
5. Verify cursor exclusion plus client-side cursor updates.
6. Measure idle, typing, menu use, dragging, scrolling, and full-screen motion
   independently.

Exit criterion: choose the lowest-latency stable settings for ordinary desktop
work, with native 1024x768 retained throughout.

### Patch 9: Packaging and operation

1. Produce a signed minimal `.app` wrapper or stable launcher identity for macOS
   privacy permissions.
2. Add LaunchAgent/start-stop scripts only after lifecycle behavior is proven.
3. Bind to `127.0.0.1:3390` by default in the local wrapper.
4. Document Homebrew dependencies, source build, permissions, launch, tunnel,
   Win98 client settings, logs, and complete removal.
5. Keep Homebrew's stock installation untouched; package the fork separately.

Exit criterion: one repeatable install and one start/stop command on the iMac.

### Patch 10: ScreenCaptureKit follow-on

1. Add a small Objective-C/Objective-C++ capture adapter using ScreenCaptureKit.
2. Preserve the same `rdpShadowSurface` publication contract used by the repaired
   backend.
3. Select `CGDisplayStream` for Sonoma builds where desired and
   ScreenCaptureKit for newer SDKs/releases, or retire the old backend after
   validation.
4. Keep input injection separate; ScreenCaptureKit replaces capture only.

Exit criterion: builds against current macOS SDKs without depending on removed
`CGDisplayStream` declarations, while preserving the legacy RDP client path.

## Test Matrix

| Area | Cases |
|---|---|
| Build | Intel Sonoma host; Apple Silicon build if available; warnings enabled |
| Capture | first frame, idle frame, blank status, resolution change, sleep/wake |
| Lifecycle | start, stop, failed start, reconnect, 25 repeated cycles |
| Client | modern RDP control client; Windows 98 RDP 5.2 target client |
| Display | 1024x768; 16-bit target; static desktop; scroll; full-screen change |
| Keyboard | letters, symbols, modifiers, extended keys, Unicode, disconnect while held |
| Mouse | move, left/right/middle, drag, wheel up/down, edges/corners |
| Permissions | neither grant, capture only, input only, both grants |
| Network | loopback direct; SSH tunnel; forced disconnect; high-latency link |

## Performance Measurements

For each workload, record:

- Mac capture callback rate and CPU use.
- Dirty pixels and encoded bytes per second.
- VAIO CPU use and observed update rate.
- Input-to-visible-response latency, preferably from a high-frame-rate video.
- Whether latency stays bounded during continuous scrolling.

Success is not defined as 30 FPS video. The target is pixel-sharp 1024x768
desktop work with immediate local cursor movement, responsive typing, and no
ever-growing update queue.

## Commit and Review Strategy

- Keep patches 1 through 7 independently reviewable.
- Land correctness before performance changes.
- Do not combine the ScreenCaptureKit adapter with the Sonoma repair series.
- Maintain a fork branch for deployment experiments and prepare a smaller
  upstreamable PR series after validation.
- Attach baseline and post-fix logs to the relevant commits or pull requests,
  not to unrelated source files.

## Immediate Next Actions

1. Fork `FreeRDP/FreeRDP` to `shardsofaperture/FreeRDP`.
2. Add the fork as `origin` and keep `FreeRDP/FreeRDP` as `upstream`.
3. Push `mac-shadow-sonoma` with this plan and `AGENTS.md`.
4. Create a Codex cloud environment named `freerdp-mac-shadow-sonoma` for the
   fork and default it to the development branch.
5. On the iMac, execute Patch 0 before changing functional code.

