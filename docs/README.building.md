# FreeRDP Build and Dependency Guide

More documentation can be found on the [FreeRDP Compilation Wiki](https://github.com/FreeRDP/FreeRDP/wiki/Compilation).

## Dependencies

FreeRDP has a few dependencies required for proper operation:

### 1. SSL (Required)

RDP requires a secure tunnel and utilizes TLS for this. We do not implement this ourselves, but reuse existing libraries.

We support:
* **OpenSSL** (main development SSL library, `-DWITH_OPENSSL=ON`, default)
* **LibreSSL** (supported by community, `-DWITH_OPENSSL=ON`, drop-in replacement)
* **MBedTLS** (supported by community, `-DWITH_OPENSSL=OFF -DWITH_MBEDTLS=ON`)

Optionally, some algorithms can be shipped with FreeRDP itself if the SSL library deprecated them:
* `-DWITH_INTERNAL_MD4=ON`
* `-DWITH_INTERNAL_MD5=ON`
* `-DWITH_INTERNAL_RC4=ON`

---

### 2. Kerberos (Optional)

Authentication to many services requires Kerberos (especially if smart cards are in use). Disable with `-DWITH_KRB5=OFF`.

We support:
* **MIT**
* **Heimdal**

---

### 3. JSON (Optional)

Azure logon requires HTTP/JSON messages to be parsed. Disable with `-DWITH_JSON_DISABLED=ON`.

We support:
* **cJSON**
* **json-c**

---

### 4. H.264

RDP GFX modes (Windows 8.1 / Server 2012 and newer) support a graphics mode based on the H.264 codec.

We support:
* **OpenH264** (enable with `-DWITH_OPENH264=ON`)
* **FFmpeg** (x264 or OpenH264, enable with `-DWITH_FFMPEG=ON`)
* **Apple VideoToolbox** (decoding only, enable with `-DWITH_APPLE_VIDEOTOOLBOX=ON`)

There are other platform-specific implementations too (e.g., MediaCodec on Android).

VideoToolbox is enabled by default on macOS 10.10+ and iOS 8.0+. It uses Apple's system frameworks and default hardware/software selection. It can replace OpenH264/FFmpeg for H.264 decoding; encoding requires another backend.

---

### 5. Graphics Scaling Support (Optional)

Required for High DPI support. High DPI support and smart-sizing options require bitmaps to be scaled by the client.

We support:
* **Swscale** (enable with `-DWITH_SWSCALE=ON`)
* **Cairo** (enable with `-DWITH_CAIRO=ON`)

---

### 6. Audio Encoders/Decoders (Optional, Highly Recommended)

Sound and microphone options allow transmission of data in compressed formats. The most widely supported formats are uncompressed PCM (supported by all systems) and compressed AAC (Windows 8 or newer). Other codecs like GSM are supported as well, but do not provide the same quality as AAC or PCM.

We support:
* **FAAC / FAAD2 / soxr** (encoder/decoder/resampling)
* **GSM** (older low-bandwidth codec, `-DWITH_GSM=ON`)
* **FFmpeg** (`-DWITH_DSP_FFMPEG=ON`)
* **SOXR** (resampling library, enable with `-DWITH_SOXR=ON`)

To enable experimental codecs (mainly AAC encoding), add `-DWITH_DSP_EXPERIMENTAL=ON`.

---

### 7. Smartcard Support (Optional)

Required to utilize smart cards for authentication/redirection.

We support:
* **PCSC** (disable with `-DWITH_PCSC=OFF`)
* **PKCS#11** (disable with `-DWITH_PKCS11=OFF`)

*Note:* PCSC is required for smart card redirection; PKCS#11 is used for NLA smart card logon support.

---

### 8. Unicode (Required)

Most of the protocol uses UCS-2/UTF-16 for strings. To convert to/from UTF-8, a Unicode support library is required. (Use `-DWITH_UNICODE_BUILTIN=ON` to utilize custom `char16` $\leftrightarrow$ `utf8` conversion routines).

* **Windows:** Natively supports Unicode conversions.
* **Linux/Unix/Android:** Uses ICU.
* **Apple (iOS/macOS):** Uses native `NSString` Unicode conversion routines.

---

### 9. USB Redirection (Optional)

The protocol has an extension channel to allow low-level USB redirection. Disable with `-DCHANNEL_URBDRC=OFF`.

We support:
* **libusb 1.0**

---

### 10. Platform Support

Mainly relevant for Linux; for other systems, the platform SDK is usually sufficient.

* **SDL2 / SDL3** for the SDL client (all platforms, disable with `-DWITH_CLIENT_SDL=OFF` or `-DWITH_CLIENT_SDL3=OFF`)
* **CUPS** (Linux/macOS) for printing support (disable with `-DWITH_CUPS=OFF`)
* **libsystemd** (Linux) for journald logging support (disable with `-DWITH_SYSTEMD=OFF`)
* **PAM** headers/libraries (server-side authentication)
* **FUSE** for file clipboard support (Linux/macOS, disable with `-DWITH_FUSE=OFF`)
* **Wayland** for `wlfreerdp` (disable with `-DWITH_WAYLAND=OFF`)
* **X11** development headers for the X11 client (disable with `-DWITH_X11=OFF`)
* **ALSA** development headers/libraries (disable with `-DWITH_ALSA=OFF`)
* **PulseAudio** development headers/libraries (disable with `-DWITH_PULSE=OFF`)
* **OSS** development headers/libraries (disable with `-DWITH_OSS=OFF`)

---

### 11. WebAuthn / FIDO2 Redirection (Optional)

The `[MS-RDPEWA]` WebAuthn Virtual Channel Extension allows forwarding WebAuthn/FIDO2 authenticator requests from the RDP server to the client's local security keys. This enables passwordless authentication (e.g., passkeys) and two-factor authentication inside the remote session using hardware security keys plugged into the client machine. Disable with `-DCHANNEL_RDPEWA=OFF`.

Enabled automatically when the required libraries are found. Requires:
* **libcbor** (CBOR encoding/decoding)
