# FreeRDP Harmony Client

A HarmonyOS client shell for FreeRDP.

## Goals

- Reuse `libfreerdp`, `winpr` and `client/common` as much as possible.
- Keep the HarmonyOS shell thin, clear and user friendly.
- Isolate native bridge code so protocol updates do not leak into ArkTS pages.

## Structure

- `entry`: HarmonyOS Stage model shell, pages and view models.
- `native`: N-API and FreeRDP adapter skeleton.

## Building

### Prerequisites

- DevEco Studio with HarmonyOS SDK
- HarmonyOS NDK (bundled with the SDK)

OpenSSL is downloaded and cross-compiled automatically by CMake (via `ExternalProject`)
during the native build step — no manual steps are required.

### Build the App

```bash
# Debug build
client/Harmony/scripts/build.sh debug

# Release build
client/Harmony/scripts/build.sh release
```

Or open `client/Harmony` in DevEco Studio and build from the IDE.

### Desktop Preview

For quick connectivity testing, use the existing SDL3 client:

```bash
tools/open-sdl-window.sh <host> <username> <password>
```

## License

This client is part of FreeRDP and licensed under the Apache License 2.0.
See the root `LICENSE` file for details.

This product includes software developed by the OpenSSL Project for use
in the OpenSSL Toolkit (https://www.openssl.org/). See
`native/third_party/NOTICE` for the full OpenSSL and SSLeay license texts.
