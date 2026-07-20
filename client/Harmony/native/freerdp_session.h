#ifndef FREERDP_HARMONY_SESSION_H
#define FREERDP_HARMONY_SESSION_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "freerdp_adapter.h"

struct NativeWindow;

class FreeRDPHarmonySession {
 public:
  explicit FreeRDPHarmonySession(HarmonySessionConfig config);
  ~FreeRDPHarmonySession();
  FreeRDPHarmonySession(const FreeRDPHarmonySession&) = delete;
  FreeRDPHarmonySession& operator=(const FreeRDPHarmonySession&) = delete;
  FreeRDPHarmonySession(FreeRDPHarmonySession&&) = delete;
  FreeRDPHarmonySession& operator=(FreeRDPHarmonySession&&) = delete;

  bool Connect();
  bool Disconnect();

  HarmonySurfaceSnapshot GetSnapshot() const;
  HarmonySessionInfo GetInfo() const;
  std::uint32_t GetFrameStride() const;
  std::uint64_t GetFrameSequence() const;
  std::vector<std::uint8_t> GetFrameBGRA() const;
  std::vector<HarmonySessionEvent> DrainEvents();
  bool ShouldIgnoreCertificate() const;
  bool SetClipboardText(const std::string& text);
  void FillAuthentication(char** username, char** password, char** domain, std::uint32_t reason);
  bool VerifyCertificate(const char* host, std::uint16_t port, const char* commonName,
                         const char* subject, const char* issuer, const char* fingerprint,
                         std::uint32_t flags);
  bool VerifyChangedCertificate(const char* host, std::uint16_t port, const char* commonName,
                                const char* subject, const char* issuer, const char* newFingerprint,
                                const char* oldSubject, const char* oldIssuer,
                                const char* oldFingerprint, std::uint32_t flags);
  bool SendMouseMove(std::uint16_t x, std::uint16_t y);
  bool SendMouseButton(std::uint16_t x, std::uint16_t y, std::uint16_t buttonFlags, bool down);
  bool SendMouseWheel(std::uint16_t x, std::uint16_t y, std::int16_t delta);
  bool SendKeyScancode(std::uint32_t scancode, bool down);
  bool SendUnicodeKey(std::uint16_t codepoint, bool down);
  void SubmitAuth(const std::string& username, const std::string& password, const std::string& domain);
  void SubmitCert(bool accept);
  void SetSurfaceId(const std::string& surfaceIdStr);
  void UpdateFrame(std::uint32_t width, std::uint32_t height, std::uint32_t left,
                   std::uint32_t top, std::uint32_t dirtyWidth, std::uint32_t dirtyHeight,
                   const std::uint8_t* data, std::size_t size, std::uint32_t stride);
  void UpdateSnapshot(std::uint32_t width, std::uint32_t height, std::uint32_t left,
                      std::uint32_t top, std::uint32_t dirtyWidth,
                      std::uint32_t dirtyHeight);
  void UpdateStage(HarmonySessionStage stage, std::uint32_t lastError,
                   const std::string& message);
  void SetCliprdrContext(void* cliprdr);
  void MarkClipboardReady(bool ready);
  std::string GetLocalClipboardText() const;
  bool SendClipboardFormatList();
  bool SendClipboardDataRequest(std::uint32_t formatId);
  bool OnRemoteClipboardData(const std::string& text);

 private:
  struct InputEvent {
    enum class Type {
      kMouse,
      kScancodeKey,
      kUnicodeKey,
    };

    Type type;
    std::uint16_t flags;
    std::uint16_t x;
    std::uint16_t y;
    std::uint32_t scancode;
    std::uint16_t codepoint;
  };

  bool EnsureContext();
  bool ApplySettings(void* settings, std::string& outError);
  std::vector<std::string> BuildCommandLineArgs() const;
  bool QueueInputEvent(const InputEvent& event);
  bool ProcessInputEvents(void* input);
  void ThreadMain();

  HarmonySessionConfig config_;
  mutable std::mutex mutex_;
  std::condition_variable stateChanged_;
  HarmonySurfaceSnapshot snapshot_;
  HarmonySessionInfo info_;
  void* context_;
  std::vector<std::uint8_t> frameBuffer_;
  std::uint32_t frameStride_;
  std::uint64_t frameSequence_;
  bool hasPendingFrameEvent_;
  std::deque<HarmonySessionEvent> pendingSessionEvents_;
  void* inputEventHandle_;
  std::deque<InputEvent> pendingInputEvents_;
  void* cliprdr_;
  bool clipboardReady_;
  std::string localClipboardText_;
  bool started_;
  bool stopRequested_;
  std::thread worker_;
  NativeWindow* nativeWindow_;

  // Authentication & Certificate Dialog State
  mutable std::mutex dialogMutex_;
  std::condition_variable dialogCV_;
  bool dialogAnswered_;
  bool certAccepted_;
  std::string dialogUsername_;
  std::string dialogPassword_;
  std::string dialogDomain_;
};

using FreeRDPHarmonySessionPtr = std::shared_ptr<FreeRDPHarmonySession>;

#endif
