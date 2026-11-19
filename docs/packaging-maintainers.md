# Packaging maintainer guide

This page describes the kind of help FreeRDP needs from people who want to
provide stable platform packages or installers. It is meant for future
maintainers, not for users who only want to install FreeRDP.

FreeRDP already has source releases, Linux packaging work, CI builds, and some
platform-specific build scripts. Stable Windows and macOS installers require
additional people who can keep the packaging healthy over time.

## Existing packaging and build entry points

| Area | Current entry point |
| --- | --- |
| Linux Flatpak | `packaging/flatpak/` |
| Debian nightly package | `packaging/deb/freerdp-nightly/` |
| RPM nightly package | `packaging/rpm/freerdp-nightly.spec` |
| Windows build notes | `docs/README.mingw` |
| Windows build script | `scripts/mingw.sh` |
| Windows CI build | `.github/workflows/mingw.yml` |
| Windows CMake preload | `packaging/windows/preload.cmake` |
| macOS build notes | `docs/README.macOS` |
| macOS build script | `scripts/bundle-mac-os.sh` |
| macOS CI build | `.github/workflows/macos.yml` |

These files are useful starting points, but a CI build is not the same thing as
a stable user-facing installer. Installers need release process, testing, and
long-term ownership.

## What a package maintainer does

A package maintainer keeps one installation path usable for real users. Useful
work includes:

- keeping the package build working when FreeRDP, compilers, SDKs, or
  dependencies change;
- testing new release candidates or release builds on real target systems;
- documenting how to install, upgrade, uninstall, and report package-specific
  issues;
- tracking platform-specific problems and deciding whether they belong in
  packaging, FreeRDP, or external dependencies;
- coordinating with FreeRDP maintainers before publishing official-looking
  artifacts;
- updating release notes or download documentation when the package status
  changes.

## Windows installer maintainership

A stable Windows installer would need maintainers who can own the Windows
packaging path. Depending on the chosen installer technology, that work can
include:

- deciding whether the project should provide `.exe`, `.msi`, `.zip`, or more
  than one format;
- defining which client binaries, libraries, plugins, resources, and licenses
  are included;
- validating static or shared runtime choices;
- testing installation and uninstallation on supported Windows versions;
- checking first-run behavior, shortcuts, PATH changes, and file associations if
  any are added;
- handling code signing if the artifact is published as an official installer;
- keeping nightly/test builds clearly separated from stable release installers.

The existing Windows-related files are a starting point, not a complete stable
installer process.

## macOS package maintainership

A stable macOS package would need maintainers who can own the macOS packaging
path. That work can include:

- deciding whether the project should provide a `.dmg`, `.pkg`, `.app`, or more
  than one format;
- defining which client binaries, libraries, plugins, resources, and licenses
  are included;
- testing on supported macOS versions and CPU architectures;
- validating Homebrew or MacPorts expectations against any project-provided
  package;
- handling code signing and notarization if the artifact is published as an
  official macOS package;
- checking first-run behavior, permissions prompts, and user-facing error
  messages.

The existing macOS build workflow is useful for build coverage, but release
packages need additional maintenance and testing.

## Release checklist for new installers

Before an installer is advertised as stable, maintainers should be able to
answer:

- Which operating system versions and CPU architectures are supported?
- Which FreeRDP clients and helper tools are included?
- Are dependencies bundled, dynamically linked, or expected from the system?
- How does a user install, upgrade, and uninstall the package?
- How are package-specific bugs reported and reproduced?
- Is the installer signed, notarized, or otherwise verified for the platform?
- Who will update the package when FreeRDP or platform dependencies change?

If any answer is missing, the artifact should be described as experimental,
nightly, or community-maintained rather than stable and official.

## How to volunteer

If you want to help maintain a package or installer:

1. Open an issue or draft pull request describing the platform and package
   format you want to maintain.
2. Link to any existing build scripts, CI workflows, or package recipes you plan
   to use.
3. Describe the systems you can test on.
4. Explain whether you can handle signing, notarization, and release updates.
5. Ask maintainers which artifact naming, hosting, and release process they
   prefer before publishing official-looking builds.

Small documentation and testing contributions are also useful. Even if you
cannot maintain a full installer, you can help by testing builds, improving
instructions, and reporting where users get stuck.
