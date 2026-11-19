# User installation guide

[Russian translation](user-installation.ru.md)

This page is for users who want to install a ready-to-use FreeRDP client.
It does not replace the build documentation or the wiki; it summarizes the
current beginner-facing installation options and points out where maintainers
are still needed.

## Current installation options

| Platform | Current user-facing option | Notes |
| --- | --- | --- |
| Linux | [Flatpak on Flathub](https://flathub.org/en/apps/com.freerdp.FreeRDP) | Stable build maintained for Linux users. |
| Windows | [Nightly Windows builds](https://ci.freerdp.com/job/freerdp-nightly-windows/) | Intended for testing. Stable Windows installers need maintainers. |
| macOS | [Homebrew](https://brew.sh/) or [MacPorts](https://www.macports.org/) | No project-maintained stable `.dmg` installer is currently documented here. |
| Android | Google Play or APK release channel | See the Android documentation and release notes. |
| iOS | App Store | See the iOS documentation and release notes. |

If a platform-specific stable installer becomes available, please update this
page and the project download links so non-technical users can find it easily.

## macOS client to Windows computer

A common home setup is:

- the user has a macOS computer;
- the user wants to connect to a Windows computer;
- both computers are on the same network or otherwise reachable through RDP.

For this setup, FreeRDP is installed on the macOS computer. The Windows computer
usually does not need FreeRDP installed; it needs an RDP server, usually the
built-in Windows Remote Desktop feature.

Basic checklist:

1. On Windows, enable Remote Desktop in the Windows settings.
2. Make sure the Windows edition supports incoming Remote Desktop connections.
   Some editions, for example Windows Home, do not include the built-in RDP host.
3. Find the Windows computer name or IP address.
4. Install FreeRDP on macOS:

   ```sh
   brew install freerdp
   ```

   or, with MacPorts:

   ```sh
   sudo port install FreeRDP
   ```

5. Connect from macOS. Depending on the package and enabled clients, use the SDL
   client (`sdl3-freerdp` or `sdl2-freerdp`) or `xfreerdp`:

   ```sh
   sdl3-freerdp /v:windows-hostname-or-ip /u:username
   ```

   ```sh
   sdl2-freerdp /v:windows-hostname-or-ip /u:username
   ```

   ```sh
   xfreerdp /v:windows-hostname-or-ip /u:username
   ```

Prefer entering passwords interactively when possible instead of placing them in
shell history.

## Help wanted: Windows and macOS installers

Windows `.exe` or `.msi` installers and macOS `.dmg` packages are not just
one-time build artifacts. They need people who can keep them healthy over time.

Useful maintainer work includes:

- packaging release builds for the target platform;
- testing fresh releases on real Windows or macOS systems;
- handling installer updates and dependency changes;
- code signing and, on macOS, notarization;
- documenting installation and first-run behavior;
- responding to packaging-specific bug reports.

If you can help maintain a stable Windows or macOS installation path, please
open an issue or pull request and coordinate with the project maintainers.
