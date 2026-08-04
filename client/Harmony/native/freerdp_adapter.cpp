#include "freerdp_adapter.h"

#include "freerdp_session.h"

FreeRDPHarmonyAdapter::FreeRDPHarmonyAdapter() : nextSessionId_(1) {}

int FreeRDPHarmonyAdapter::CreateSession(const HarmonySessionConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);

  const int sessionId = nextSessionId_;
  nextSessionId_ += 1;
  sessions_[sessionId] = std::make_shared<FreeRDPHarmonySession>(config);
  return sessionId;
}

bool FreeRDPHarmonyAdapter::Connect(int sessionId) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }

  if (!session) {
    return false;
  }

  return session->Connect();
}

bool FreeRDPHarmonyAdapter::Disconnect(int sessionId) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }

  if (!session) {
    return false;
  }

  return session->Disconnect();
}

HarmonySurfaceSnapshot FreeRDPHarmonyAdapter::GetSnapshot(int sessionId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::shared_ptr<FreeRDPHarmonySession> session = GetSessionLocked(sessionId);
  if (!session) {
    return HarmonySurfaceSnapshot{0, 0, 0, 0, 0, 0};
  }
  return session->GetSnapshot();
}

HarmonySessionInfo FreeRDPHarmonyAdapter::GetInfo(int sessionId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::shared_ptr<FreeRDPHarmonySession> session = GetSessionLocked(sessionId);
  if (!session) {
    return HarmonySessionInfo{HarmonySessionStage::kFailed, 1, false, "invalid session"};
  }
  return session->GetInfo();
}

std::uint32_t FreeRDPHarmonyAdapter::GetFrameStride(int sessionId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::shared_ptr<FreeRDPHarmonySession> session = GetSessionLocked(sessionId);
  if (!session) {
    return 0;
  }
  return session->GetFrameStride();
}

std::uint64_t FreeRDPHarmonyAdapter::GetFrameSequence(int sessionId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::shared_ptr<FreeRDPHarmonySession> session = GetSessionLocked(sessionId);
  if (!session) {
    return 0;
  }
  return session->GetFrameSequence();
}

std::vector<std::uint8_t> FreeRDPHarmonyAdapter::GetFrameBGRA(int sessionId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::shared_ptr<FreeRDPHarmonySession> session = GetSessionLocked(sessionId);
  if (!session) {
    return {};
  }
  return session->GetFrameBGRA();
}

std::vector<HarmonySessionEvent> FreeRDPHarmonyAdapter::DrainEvents(int sessionId) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  return session ? session->DrainEvents() : std::vector<HarmonySessionEvent>{};
}

bool FreeRDPHarmonyAdapter::SetSurfaceId(int sessionId, const std::string& surfaceIdStr) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  if (!session) {
    return false;
  }
  session->SetSurfaceId(surfaceIdStr);
  return true;
}

bool FreeRDPHarmonyAdapter::SetClipboardText(int sessionId, const std::string& text) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  return session ? session->SetClipboardText(text) : false;
}

bool FreeRDPHarmonyAdapter::SendMouseMove(int sessionId, std::uint16_t x, std::uint16_t y) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  return session ? session->SendMouseMove(x, y) : false;
}

bool FreeRDPHarmonyAdapter::SendMouseButton(int sessionId, std::uint16_t x, std::uint16_t y,
                                            std::uint16_t buttonFlags, bool down) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  return session ? session->SendMouseButton(x, y, buttonFlags, down) : false;
}

bool FreeRDPHarmonyAdapter::SendMouseWheel(int sessionId, std::uint16_t x, std::uint16_t y,
                                           std::int16_t delta) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  return session ? session->SendMouseWheel(x, y, delta) : false;
}

bool FreeRDPHarmonyAdapter::SendKeyScancode(int sessionId, std::uint32_t scancode, bool down) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  return session ? session->SendKeyScancode(scancode, down) : false;
}

bool FreeRDPHarmonyAdapter::SendUnicodeKey(int sessionId, std::uint16_t codepoint, bool down) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  return session ? session->SendUnicodeKey(codepoint, down) : false;
}

bool FreeRDPHarmonyAdapter::SubmitAuth(int sessionId, const std::string& username, const std::string& password, const std::string& domain) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  if (!session) {
    return false;
  }
  session->SubmitAuth(username, password, domain);
  return true;
}

bool FreeRDPHarmonyAdapter::SubmitCert(int sessionId, bool accept) {
  std::shared_ptr<FreeRDPHarmonySession> session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = GetSessionLocked(sessionId);
  }
  if (!session) {
    return false;
  }
  session->SubmitCert(accept);
  return true;
}

std::shared_ptr<FreeRDPHarmonySession> FreeRDPHarmonyAdapter::GetSessionLocked(int sessionId) const {
  const auto it = sessions_.find(sessionId);
  if (it == sessions_.end()) {
    return nullptr;
  }
  return it->second;
}
