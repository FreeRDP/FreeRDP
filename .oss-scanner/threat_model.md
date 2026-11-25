# Threat model

## What this project does and where untrusted input enters
- **What it does:** FreeRDP is an open-source implementation of the Remote Desktop Protocol (RDP) and related Microsoft protocols, providing client applications (`xfreerdp`, `wlfreerdp`, `sdl-freerdp`), server implementations (`freerdp-server`, `shadow`), protocol translation proxy services (`freerdp-proxy`), client and server abstraction libraries (`libfreerdp-client`, `libfreerdp-server`), core protocol libraries (`libfreerdp`), and **WinPR (Windows Portable Runtime)**—a fundamental abstraction sub-project that implements portable Windows API calls across Unix/Linux, POSIX, and macOS platforms.
- **RDP Layered Parser & Writer Attack Surface (First Line of Network I/O)**:
  - Multi-layered Packet Ingestion: The RDP parser/writer is the primary entry/exit point directly handling raw socket read/write operations. It parses and constructs nested protocol layers from lower-level framing up to high-level payloads, including:
    - TPKT (ISO 8073 / RFC 1006): Transport Service on top of TCP.
    - X.224 (COTP / ISO 8073): Connection-Oriented Transport Protocol class 0.
    - T.125 (MCS / ITU-T T.125): Multipoint Communication Service (Connect Initial, Connect Response, Domain PDUs, channel join/attach requests).
    - RDP Core Security Layer & Security Headers: Non-NLA / standard RDP encryption, signatures, and PDU wrapping.
  - Untrusted Multi-level Framing: Malformed length fields, corrupted segment headers, or state mismatch anywhere across TPKT, X.224, or T.125 layers can trigger memory corruption or state machine desynchronization before higher-level session code is even reached.
- **Client-side attack surface:** The primary untrusted input boundary is the network stream received from an untrusted remote RDP server. This applies across all desktop clients (xfreerdp, wlfreerdp, sdl-freerdp, macOS client), mobile platforms (Android client), and legacy/obsolete frontends (e.g., DirectFB, Windows/WinCE frontends, older mobile ports).
- **Server-side attack surface:** The server component receives untrusted network input directly from connected clients during handshake, authentication, virtual channel negotiation, and inbound device redirection packets.
- **Proxy-side attack surface (`freerdp-proxy` / intermediate MITM architecture):**
  - **Dual Untrusted Boundaries:** The proxy terminates client connections on its front-end server interface and initiates connections to destination servers on its back-end client interface. It ingests untrusted inputs simultaneously from **both** untrusted clients and untrusted upstream/downstream servers.
  - **Packet Interception & Decoding:** The proxy inspects, filters, or modifies RDP traffic, virtual channels, static and dynamic channel PDUs, and graphics pipelines in transit. Vulnerabilities in proxy packet forwarding, stream re-serialization, or custom proxy plugins expose both the proxy host and forwarded sessions to attack.
- **Bidirectional Local Device & Drive Redirection (Untrusted Input Boundary):**
  - **Client-to-Server:** When local drives, file systems, serial ports, smart cards, or USB devices are shared from client to server (directly or via `freerdp-proxy`), WinPR's path abstraction and I/O subsystems process untrusted file system paths, handles, device IOCTL queries, and stream buffers.
  - **Server-to-Client:** When a remote server accesses shared local drives or redirected devices on the client side, WinPR and virtual channel plugins process untrusted file operations, directory enumerations, path traversal attempts, smart card APDUs, and device redirection responses coming from the server.
- **File & String parsing attack surface:** In addition to live network streams, WinPR's utility modules, client configuration tools, and proxy config parsers ingest untrusted input via `.rdp` config files, TLS certificates/keys, proxy filter configurations, shadow server configurations, registry/INI emulators, BMP/image utility writers, and string conversion wrappers (e.g., Unicode/UTF-16 <-> UTF-8 conversions).
- **Applicable RDP Specifications & Extension Layers:**
  - **Core Protocols:** MS-RDPBCGR (Basic Connectivity and Graphics Redirection), MS-RDPEDYC (Extended Virtual Channels), MS-RDPEGDI (Graphics Device Interface Commands).
  - **Security & Transport:** MS-CSSP (CredSSP / NLA), MS-TSGU (Terminal Services Gateway Protocol - HTTP/HTTPS WebSocket framing).
  - **Graphics & Compression Codecs:** MS-RDPRFX (RemoteFX Codec), MS-RDPNSC (NSCodec), MS-RDPEGFX (Graphics Pipeline Extension / AVC / H.264 / ClearCodec / Progressive Codec).
  - **Input & Device Redirection:** MS-RDPINPUT (Input Push for Touch/Multi-touch), MS-RDPDR (Device
		Redirection System Protocol), MS-RDPEFS (File System Redirection Extension), MS-RDPESP (Smart Card Redirection), MS-RDPECLIP (Clipboard Virtual Channel Protocol),
		MS-RDPEA (Audio Output Protocol) / MS-RDPEAI (Audio Input Protocol), MS-RDPEUSB (RemoteFX USB Redirection), MS-RDPEWA (WebAuthn Virtual Channel),
		MS-RDPECAM (Video Capture Virtual Channel Extension)

## Components that matter most / least
- **Critical scope (High importance):**
  - **RDP Network Stack & Multi-layer Parser/Writer (libfreerdp):**
    - Lower-layer protocol framing and state machines: TPKT, X.224, and T.125 MCS parsers/encoders.
    - Primary network stream reading/writing routines (`Transport_Read`, `Transport_Write`, and `wStream` buffer interactions) that process raw, untrusted socket bytes prior to PDU dispatching.
  - **Client & Server Libraries (`libfreerdp-client`, `libfreerdp-server`):**
    - **`libfreerdp-client`:** High-level client connection state machine, display pipeline integrations, input event routing, and client-side channel orchestration.
    - **`libfreerdp-server`:** Core server session management, client peer connection handling, incoming virtual channel registration, authentication callbacks, and shadow server core engine (`shadow`).
  - **FreeRDP Proxy (`freerdp-proxy` & plugins):** Proxy packet interceptors, connection setup state machines, channel filters, module API wrappers, and credential/session handling in proxy pipelines.
  - **WinPR Runtime Core (`libwinpr`):**
    - **Stream & Memory Subsystem:** `wStream` API (`Stream_Read_*`, `Stream_Write_*`, bounds-checking macros, pool allocators), which is heavily used across *all* RDP, client, server, and proxy parsing logic.
    - **SSPI / Crypto Wrappers:** NTLM, Kerberos (`kerberos.c`), CredSSP/Schannel security provider implementations handling authentication primitives and security contexts.
    - **Path & File Abstraction (`winpr/path`, `winpr/file`):** Path sanitization, relative path resolution, and filesystem normalization used to protect against path traversal in drive redirection (**MS-RDPEFS** / **MS-RDPDR**).
    - **Utilities & Image Parsers (`winpr/utils`):** String conversion routines (UTF-8/UTF-16), ASN.1 / DER parsers, and integrated image/BMP/JPEG helpers.
  - **Core Packet & Stream Parsing:** Protocol handlers implementing **MS-RDPBCGR** and **MS-RDPEDYC** (`libfreerdp-core`), stream reading macros, and PDU decoders.
  - **Codec Decoders:** Graphics and bitmap decoding algorithms (`libfreerdp-codec`) handling **MS-RDPRFX**, **MS-RDPNSC**, and **MS-RDPEGFX** (ClearCodec, Planar, Progressive, and H.264 wrappers).
  - **Virtual Channel Plugins:** File System (**MS-RDPEFS**), Smart Card (**MS-RDPESP**), Clipboard (**MS-RDPECLIP**), Audio (**MS-RDPEA** / **MS-RDPEAI**), and USB redirection (**MS-RDPEUSB**).
  - **Authentication & Gateway Stacks:** **MS-CSSP** (CredSSP/NLA) authentication handling built on WinPR SSPI primitives, TLS wrappers, certificate validation routines, and **MS-TSGU** (TS Gateway) client wrappers.
- **Secondary scope (Moderate importance):**
  - Production Client UI binaries (`client/X11` / `xfreerdp`, `client/Wayland` / `wlfreerdp`, `client/SDL` / `sdl-freerdp`) – UI-level handling of remote display surfaces, input event mappings (**MS-RDPINPUT**), and local window buffers.
  - Client user interface & application wrappers
    - Active secondary clients: Android client (`client/Android`), macOS client (`client/Mac`), Windows client (`client/Windows`).
    - Legacy, partially obsolete, or unmaintained client implementations in the codebase (e.g., sdl2 `client/SDL/SDL2/`).
  - WinPR thread, synchronization, RPC, and logging (`wLog`) frameworks—unless misconfigured macro assertions (e.g., `WINPR_ASSERT`) introduce reachable DoS conditions.
- **Out of scope / Minimal importance:**
  - Third-party submodules bundled within WinPR/FreeRDP, or external libraries (e.g., OpenSSL, FFmpeg, LibUSB, X11) directly, unless the vulnerability resides in FreeRDP/WinPR/Proxy integration wrappers.
  - Sample/demo applications in `client/Sample` or internal test harnesses unless explicitly compiled into production binaries.
  - Administrative tooling or build system scripts (CMake configurations, CI workflows).

## How to exercise it
- **Fuzzing & Harnesses:** Integrated OSS-Fuzz targets exist within the codebase to fuzz WinPR primitives (`wStream`, path parsers, SSPI modules), individual codecs (**MS-RDPRFX**, **MS-RDPEGFX**, **MS-RDPNSC**), proxy stream handlers, client/server channel handlers, and core protocol parsers directly. Custom fuzz drivers can link directly against `libwinpr`, `libfreerdp-codec`, `libfreerdp-core`, `libfreerdp-client`, `libfreerdp-server`, or proxy module interfaces.
- **Pre-recorded traffic:** Captured RDP packet streams (`.pcap` or stream dumps) replayed into mock sockets, proxy instances, or client/server test targets.
- **Command-line utilities & Test Suite:**
  - Running unit tests via `ctest` or `cmake --build . --target test` (which extensively tests WinPR APIs alongside FreeRDP client/server core and proxy routines).
  - Manual testing using `xfreerdp /v:<target_ip>` against production hosts, launching `freerdp-server` / `shadow-cli` mock servers to test incoming client payloads, or running `freerdp-proxy <config.ini>` to route sessions through intermediate proxy pipelines.

## How you rate severity
- **Critical:**
  - Remote Code Execution (RCE) via controlled buffer overflows, heap corruptions, out-of-bounds writes, or write-what-where primitives in `libfreerdp-client`, `libfreerdp-server`, `freerdp-proxy`, WinPR memory/stream routines (`wStream`), core packet parsers (**MS-RDPBCGR**), drive redirection path handlers (**MS-RDPEFS**), or graphics decoders (**MS-RDPEGFX**) without requiring non-default flags.
  - Pre-authentication memory corruption, double-frees, or authentication/session bypass in client/server libraries, proxy pipelines, or WinPR SSPI modules (e.g., Kerberos/CredSSP/TLS verification bypass).
- **High:**
  - Arbitrary session hijacking, cross-session leakage, or privilege escalation across sessions handled by `libfreerdp-server` or `freerdp-proxy`.
  - Arbitrary file read/write outside designated shared directories (path traversal / sandbox escape) stemming from flawed WinPR path normalization in redirected drives (**MS-RDPDR** / **MS-RDPEFS**).
  - Uncontrolled or limited write-out-of-bounds, heap buffer overflows, use-after-free (UAF), or double-free vulnerabilities in client/server frameworks, proxy plugins, WinPR utility modules, core virtual channels, or codecs that do not immediately demonstrate reliable RCE.
  - Out-of-bounds reads in proxy forwarding routines, server connection peer state machines, WinPR stream buffers, or core client packet decoders that disclose sensitive memory (e.g., session credentials, TLS keys, clipboard history across **MS-RDPECLIP**) across the network boundary.
- **Medium:**
  - Pre-authentication or during-session Denial of Service (DoS) causing client, server, or proxy crashes (e.g., NULL pointer dereferences, unhandled assertion failures via `WINPR_ASSERT`, infinite loops/hangs, out-of-memory allocations triggered by oversized packet length fields in **MS-RDPEDYC** channels).
  - Vulnerabilities requiring non-standard, experimental, or obscure virtual channel extensions to be explicitly enabled in proxy, server, or client settings.
- **Low:**
  - Minor memory leaks in proxy connection pools, server client-peer contexts, or WinPR handle tables not leading to rapid resource exhaustion.
  - Crashes requiring local elevated privileges or obscure non-default command-line flags.
  - Insecure default options where clear warning documentation or compiler flags exist.

## Anything to leave alone
- **Third-party library internals:** Do not report standard vulnerabilities inherent to underlying upstream dependencies (OpenSSL, FFmpeg, LibUSB, X11 libraries) unless WinPR, FreeRDP, or `freerdp-proxy` call sites misuse the API in a dangerous manner.
- **Theoretical race conditions in client GUI shutdown:** Do not report tear-down or exit-time race conditions/leaks in GUI frontends or WinPR handle cleanup routines unless they can be reliably turned into an active exploit scenario during a session.
- **Unaligned memory access on architectures supporting it:** Avoid reporting compiler/CPU unaligned access warnings on x86/x86_64 or ARM architectures where unaligned access is handled natively by hardware or handled safely by WinPR stream macros (`Stream_Read_*`).
- **Resource exhaustion via massive server framebuffers:** Requests for extremely large screen resolutions or legitimate high-throughput graphics streams that hit hardware memory limits are treated as expected operational limits rather than security vulnerabilities unless accompanied by integer overflow issues in WinPR/proxy allocation routines.
