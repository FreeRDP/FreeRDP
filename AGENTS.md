# macOS Shadow Server Work

This fork's active development target is the FreeRDP macOS shadow server on
macOS Sonoma, with Microsoft Remote Desktop 5.2 on Windows 98 SE as the legacy
client.

Read `docs/mac-shadow-sonoma-plan.md` before changing the macOS shadow backend.

## Scope

- Keep the generic FreeRDP shadow server, RDP transport, legacy bitmap encoder,
  and command-line interface intact wherever possible.
- Prefer focused repairs under `server/shadow/Mac/` over cross-platform changes.
- Preserve current clients while adding an explicitly tested legacy-client
  configuration for RDP 5.2 at 1024x768 and 16-bit color.
- Target macOS 14 Sonoma first. A ScreenCaptureKit backend is a later,
  separately reviewable phase.
- Bind test servers to loopback. Remote access is expected to use an SSH tunnel.

## Change Discipline

- Make one logical fix per commit.
- Add a regression test or a deterministic diagnostic for each bug when the
  surrounding framework permits it.
- Do not mix capture, input, legacy-security, packaging, and ScreenCaptureKit
  work in one patch.
- Keep upstream compatibility in mind; avoid VAIO-specific behavior unless it
  is behind an explicit command-line option or profile.

## Required Validation

- Build with warnings enabled and the macOS shadow subsystem enabled.
- Run FreeRDP's relevant unit tests on every source change.
- On Sonoma, verify Screen Recording and Accessibility permission diagnostics,
  first-frame delivery, idle/resume, disconnect/reconnect, and clean shutdown.
- With the Windows 98 RDP 5.2 client, verify 1024x768 at 16-bit color, keyboard,
  mouse buttons, dragging, wheel direction, local cursor behavior, and repeated
  reconnects.
- Record latency and bandwidth separately for idle desktop, typing, window
  movement, scrolling, and full-screen motion.

## Security Boundary

- The intended deployment is `127.0.0.1:3390` through SSH forwarding.
- Legacy RDP security is required for the Windows 98 client, but the server
  must not expose that listener directly to the LAN or Internet.
- Do not weaken FreeRDP's default security behavior globally.

