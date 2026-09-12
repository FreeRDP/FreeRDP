#ifndef FREERDP_HARMONY_BRIDGE_C_H
#define FREERDP_HARMONY_BRIDGE_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HarmonySessionConfigC {
  const char* name;
  const char* host;
  uint16_t port;
  const char* clientHostname;
  const char* username;
  const char* password;
  const char* domain;
  const char* gateway;
  const char* gatewayUsername;
  const char* gatewayPassword;
  const char* gatewayDomain;
  const char* securityMode;
  const char* appDataDir;
  uint8_t enableClipboard;
  uint8_t enableAudio;
  uint8_t enableDrive;
  uint8_t ignoreCertificate;
  uint32_t desktopWidth;
  uint32_t desktopHeight;
} HarmonySessionConfigC;

typedef struct HarmonySurfaceSnapshotC {
  uint32_t width;
  uint32_t height;
  uint32_t dirtyLeft;
  uint32_t dirtyTop;
  uint32_t dirtyWidth;
  uint32_t dirtyHeight;
} HarmonySurfaceSnapshotC;

typedef struct HarmonySessionInfoC {
  uint32_t stage;
  uint32_t lastError;
  uint8_t connected;
} HarmonySessionInfoC;

int harmony_session_create(const HarmonySessionConfigC* config);
int harmony_session_connect(int sessionId);
int harmony_session_disconnect(int sessionId);
int harmony_session_get_snapshot(int sessionId, HarmonySurfaceSnapshotC* snapshot);
int harmony_session_get_info(int sessionId, HarmonySessionInfoC* info, char* messageBuffer,
                             size_t messageBufferSize);
uint32_t harmony_session_get_frame_stride(int sessionId);
uint64_t harmony_session_get_frame_sequence(int sessionId);
size_t harmony_session_copy_frame_bgra(int sessionId, uint8_t* buffer, size_t capacity);
size_t harmony_session_get_last_error_string(int sessionId, char* buffer, size_t capacity);
int harmony_session_send_mouse_move(int sessionId, uint16_t x, uint16_t y);
int harmony_session_send_mouse_button(int sessionId, uint16_t x, uint16_t y, uint16_t buttonFlags,
                                      uint8_t down);
int harmony_session_send_mouse_wheel(int sessionId, uint16_t x, uint16_t y, int16_t delta);
int harmony_session_send_key_scancode(int sessionId, uint32_t scancode, uint8_t down);
int harmony_session_send_unicode_key(int sessionId, uint16_t codepoint, uint8_t down);
int harmony_session_set_surface_id(int sessionId, const char* surfaceId);
int harmony_session_submit_auth(int sessionId, const char* username, const char* password, const char* domain);
int harmony_session_submit_cert(int sessionId, uint8_t accept);

#ifdef __cplusplus
}
#endif

#endif
