#ifndef FREERDP_HARMONY_ADAPTER_H
#define FREERDP_HARMONY_ADAPTER_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class FreeRDPHarmonySession;

struct HarmonySessionConfig {
  std::string name;
  std::string host;
  std::uint16_t port;
  std::string clientHostname;
  std::string username;
  std::string password;
  std::string domain;
  std::string gateway;
  std::string gatewayUsername;
  std::string gatewayPassword;
  std::string gatewayDomain;
  std::string securityMode;
  std::string appDataDir;
  bool enableClipboard;
  bool enableAudio;
  bool enableDrive;
  bool ignoreCertificate;
  std::uint32_t desktopWidth;
  std::uint32_t desktopHeight;
};

struct HarmonySurfaceSnapshot {
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t dirtyLeft;
  std::uint32_t dirtyTop;
  std::uint32_t dirtyWidth;
  std::uint32_t dirtyHeight;
};

enum class HarmonySessionStage : std::uint32_t {
  kIdle = 0,
  kConnecting = 1,
  kConnected = 2,
  kDisconnected = 3,
  kFailed = 4,
  kAuthRequired = 5,
  kCertRequired = 6
};

struct HarmonySessionInfo {
  HarmonySessionStage stage;
  std::uint32_t lastError;
  bool connected;
  std::string message;
};

enum class HarmonySessionEventType : std::uint32_t {
  kStateChanged = 1,
  kFrameUpdated = 2,
  kRemoteClipboardChanged = 3,
  kAuthRequired = 4,
  kCertRequired = 5
};

struct HarmonySessionEvent {
  HarmonySessionEventType type;
  HarmonySessionStage stage;
  std::uint32_t lastError;
  bool connected;
  std::uint64_t frameSequence;
  std::string message;
  std::string clipboardText;
  std::string certHost;
  std::string certSubject;
  std::string certIssuer;
  std::string certFingerprint;
  std::string authUsername;
  std::string authDomain;
};

class FreeRDPHarmonyAdapter {
 public:
  FreeRDPHarmonyAdapter();

  int CreateSession(const HarmonySessionConfig& config);
  bool Connect(int sessionId);
  bool Disconnect(int sessionId);
  HarmonySurfaceSnapshot GetSnapshot(int sessionId) const;
  HarmonySessionInfo GetInfo(int sessionId) const;
  std::uint32_t GetFrameStride(int sessionId) const;
  std::uint64_t GetFrameSequence(int sessionId) const;
  std::vector<std::uint8_t> GetFrameBGRA(int sessionId) const;
  std::vector<HarmonySessionEvent> DrainEvents(int sessionId);
  bool SetSurfaceId(int sessionId, const std::string& surfaceIdStr);
  bool SetClipboardText(int sessionId, const std::string& text);
  bool SendMouseMove(int sessionId, std::uint16_t x, std::uint16_t y);
  bool SendMouseButton(int sessionId, std::uint16_t x, std::uint16_t y, std::uint16_t buttonFlags,
                       bool down);
  bool SendMouseWheel(int sessionId, std::uint16_t x, std::uint16_t y, std::int16_t delta);
  bool SendKeyScancode(int sessionId, std::uint32_t scancode, bool down);
  bool SendUnicodeKey(int sessionId, std::uint16_t codepoint, bool down);
  bool SubmitAuth(int sessionId, const std::string& username, const std::string& password, const std::string& domain);
  bool SubmitCert(int sessionId, bool accept);

 private:
  std::shared_ptr<FreeRDPHarmonySession> GetSessionLocked(int sessionId) const;

  mutable std::mutex mutex_;
  int nextSessionId_;
  std::map<int, std::shared_ptr<FreeRDPHarmonySession>> sessions_;
};

#endif
