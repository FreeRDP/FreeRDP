#include "freerdp_bridge_c.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include <freerdp/freerdp.h>
}

#include "freerdp_adapter.h"

#if __has_include(<napi/native_api.h>)
#include <napi/native_api.h>
#define FREERDP_HARMONY_HAS_NAPI 1
#else
#define FREERDP_HARMONY_HAS_NAPI 0
#endif

namespace {

constexpr std::uint32_t kDefaultDesktopWidth = 1280;
constexpr std::uint32_t kDefaultDesktopHeight = 720;
constexpr std::size_t kMessageBufferSize = 512;
constexpr std::uint32_t kScancodeLeftControl = 0x001D;
constexpr std::uint32_t kScancodeLeftAlt = 0x0038;
constexpr std::uint32_t kScancodeDelete = 0x0153;
constexpr std::uint32_t kScancodeLeftWin = 0x015B;
constexpr std::uint32_t kScancodeEscape = 0x0001;

FreeRDPHarmonyAdapter& Adapter() {
  static FreeRDPHarmonyAdapter adapter;
  return adapter;
}

std::string ToString(const char* value) {
  return value != nullptr ? std::string(value) : std::string();
}

bool SendKeyCombo(int sessionId, const std::initializer_list<std::uint32_t>& downScancodes,
                  const std::initializer_list<std::uint32_t>& upScancodes) {
  for (const std::uint32_t scancode : downScancodes) {
    if (!Adapter().SendKeyScancode(sessionId, scancode, true)) {
      return false;
    }
  }

  for (const std::uint32_t scancode : upScancodes) {
    if (!Adapter().SendKeyScancode(sessionId, scancode, false)) {
      return false;
    }
  }

  return true;
}

int harmony_session_submit_auth(int sessionId, const char* username, const char* password, const char* domain) {
  return Adapter().SubmitAuth(sessionId, username != nullptr ? username : "",
                              password != nullptr ? password : "",
                              domain != nullptr ? domain : "") ? 0 : -1;
}

int harmony_session_submit_cert(int sessionId, uint8_t accept) {
  return Adapter().SubmitCert(sessionId, accept != 0) ? 0 : -1;
}

std::string BuildLastErrorString(int sessionId) {
  const HarmonySessionInfo info = Adapter().GetInfo(sessionId);
  if (info.lastError == 0) {
    return std::string();
  }

  const char* lastError = freerdp_get_last_error_string(info.lastError);
  return (lastError != nullptr) ? std::string(lastError) : std::string();
}

#if FREERDP_HARMONY_HAS_NAPI

constexpr napi_property_attributes kDefaultPropertyAttributes = napi_default;

HarmonySessionConfig GetDefaultConfig() {
  HarmonySessionConfig config{};
  config.port = 3389;
  config.clientHostname.clear();
  config.gatewayUsername.clear();
  config.gatewayPassword.clear();
  config.gatewayDomain.clear();
  config.enableClipboard = true;
  config.enableAudio = false;
  config.enableDrive = false;
  config.ignoreCertificate = true;
  config.desktopWidth = kDefaultDesktopWidth;
  config.desktopHeight = kDefaultDesktopHeight;
  config.securityMode = "auto";
  config.appDataDir.clear();
  return config;
}

bool SetNamedString(napi_env env, napi_value object, const char* name, const std::string& value) {
  napi_value napiString = nullptr;
  return (napi_create_string_utf8(env, value.c_str(), value.size(), &napiString) == napi_ok) &&
         (napi_set_named_property(env, object, name, napiString) == napi_ok);
}

bool SetNamedUint32(napi_env env, napi_value object, const char* name, std::uint32_t value) {
  napi_value napiNumber = nullptr;
  return (napi_create_uint32(env, value, &napiNumber) == napi_ok) &&
         (napi_set_named_property(env, object, name, napiNumber) == napi_ok);
}

bool SetNamedUint64(napi_env env, napi_value object, const char* name, std::uint64_t value) {
  napi_value napiNumber = nullptr;
  return (napi_create_double(env, static_cast<double>(value), &napiNumber) == napi_ok) &&
         (napi_set_named_property(env, object, name, napiNumber) == napi_ok);
}

bool SetNamedBool(napi_env env, napi_value object, const char* name, bool value) {
  napi_value napiBool = nullptr;
  return (napi_get_boolean(env, value, &napiBool) == napi_ok) &&
         (napi_set_named_property(env, object, name, napiBool) == napi_ok);
}

bool GetNamedProperty(napi_env env, napi_value object, const char* name, napi_value* result) {
  bool hasProperty = false;
  if ((napi_has_named_property(env, object, name, &hasProperty) != napi_ok) || !hasProperty) {
    return false;
  }
  return napi_get_named_property(env, object, name, result) == napi_ok;
}

bool GetNamedString(napi_env env, napi_value object, const char* name, std::string* out) {
  napi_value property = nullptr;
  if (!GetNamedProperty(env, object, name, &property)) {
    return false;
  }

  size_t length = 0;
  if (napi_get_value_string_utf8(env, property, nullptr, 0, &length) != napi_ok) {
    return false;
  }

  std::string buffer(length, '\0');
  size_t written = 0;
  if (napi_get_value_string_utf8(env, property, buffer.data(), buffer.size() + 1, &written) !=
      napi_ok) {
    return false;
  }
  buffer.resize(written);
  *out = buffer;
  return true;
}

bool GetNamedUint16(napi_env env, napi_value object, const char* name, std::uint16_t* out) {
  napi_value property = nullptr;
  if (!GetNamedProperty(env, object, name, &property)) {
    return false;
  }

  std::uint32_t value = 0;
  if (napi_get_value_uint32(env, property, &value) != napi_ok) {
    return false;
  }

  *out = static_cast<std::uint16_t>(value);
  return true;
}

bool GetNamedUint32(napi_env env, napi_value object, const char* name, std::uint32_t* out) {
  napi_value property = nullptr;
  if (!GetNamedProperty(env, object, name, &property)) {
    return false;
  }
  return napi_get_value_uint32(env, property, out) == napi_ok;
}

bool GetNamedBool(napi_env env, napi_value object, const char* name, bool* out) {
  napi_value property = nullptr;
  if (!GetNamedProperty(env, object, name, &property)) {
    return false;
  }
  return napi_get_value_bool(env, property, out) == napi_ok;
}

bool GetInt32Arg(napi_env env, napi_callback_info info, std::int32_t* out) {
  size_t argc = 1;
  napi_value argv[1] = { nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 1) {
    napi_throw_error(env, nullptr, "exactly one numeric argument is required");
    return false;
  }
  return napi_get_value_int32(env, argv[0], out) == napi_ok;
}

napi_value CreateBoolean(napi_env env, bool value) {
  napi_value result = nullptr;
  napi_get_boolean(env, value, &result);
  return result;
}

bool GetTwoInt32Args(napi_env env, napi_callback_info info, std::int32_t* first, std::int32_t* second) {
  size_t argc = 2;
  napi_value argv[2] = { nullptr, nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 2) {
    napi_throw_error(env, nullptr, "exactly two numeric arguments are required");
    return false;
  }
  return (napi_get_value_int32(env, argv[0], first) == napi_ok) &&
         (napi_get_value_int32(env, argv[1], second) == napi_ok);
}

bool GetThreeInt32Args(napi_env env, napi_callback_info info, std::int32_t* first, std::int32_t* second,
                       std::int32_t* third) {
  size_t argc = 3;
  napi_value argv[3] = { nullptr, nullptr, nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 3) {
    napi_throw_error(env, nullptr, "exactly three numeric arguments are required");
    return false;
  }
  return (napi_get_value_int32(env, argv[0], first) == napi_ok) &&
         (napi_get_value_int32(env, argv[1], second) == napi_ok) &&
         (napi_get_value_int32(env, argv[2], third) == napi_ok);
}

bool GetFourInt32Args(napi_env env, napi_callback_info info, std::int32_t* first, std::int32_t* second,
                      std::int32_t* third, std::int32_t* fourth) {
  size_t argc = 4;
  napi_value argv[4] = { nullptr, nullptr, nullptr, nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 4) {
    napi_throw_error(env, nullptr, "exactly four numeric arguments are required");
    return false;
  }
  return (napi_get_value_int32(env, argv[0], first) == napi_ok) &&
         (napi_get_value_int32(env, argv[1], second) == napi_ok) &&
         (napi_get_value_int32(env, argv[2], third) == napi_ok) &&
         (napi_get_value_int32(env, argv[3], fourth) == napi_ok);
}

bool GetFourArgsMouseButton(napi_env env, napi_callback_info info, std::int32_t* sessionId,
                            std::int32_t* x, std::int32_t* y, std::int32_t* buttonFlags,
                            bool* down) {
  size_t argc = 5;
  napi_value argv[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 5) {
    napi_throw_error(env, nullptr, "mouse button expects five arguments");
    return false;
  }
  return (napi_get_value_int32(env, argv[0], sessionId) == napi_ok) &&
         (napi_get_value_int32(env, argv[1], x) == napi_ok) &&
         (napi_get_value_int32(env, argv[2], y) == napi_ok) &&
         (napi_get_value_int32(env, argv[3], buttonFlags) == napi_ok) &&
         (napi_get_value_bool(env, argv[4], down) == napi_ok);
}

napi_value CreateUndefined(napi_env env) {
  napi_value value = nullptr;
  napi_get_undefined(env, &value);
  return value;
}

napi_value BuildInfoObject(napi_env env, int sessionId) {
  HarmonySessionInfoC info{};
  std::array<char, kMessageBufferSize> message{};
  napi_value result = nullptr;
  napi_create_object(env, &result);

  if (harmony_session_get_info(sessionId, &info, message.data(), message.size()) != 0) {
    SetNamedUint32(env, result, "stage", static_cast<std::uint32_t>(HarmonySessionStage::kFailed));
    SetNamedUint32(env, result, "lastError", 1);
    SetNamedBool(env, result, "connected", false);
    SetNamedString(env, result, "message", "error.query_info");
    return result;
  }

  SetNamedUint32(env, result, "stage", info.stage);
  SetNamedUint32(env, result, "lastError", info.lastError);
  SetNamedBool(env, result, "connected", info.connected != 0);
  SetNamedString(env, result, "message", message.data());
  return result;
}

napi_value BuildSnapshotObject(napi_env env, int sessionId) {
  HarmonySurfaceSnapshotC snapshot{};
  napi_value result = nullptr;
  napi_create_object(env, &result);

  if (harmony_session_get_snapshot(sessionId, &snapshot) != 0) {
    SetNamedUint32(env, result, "width", 0);
    SetNamedUint32(env, result, "height", 0);
    SetNamedUint32(env, result, "dirtyLeft", 0);
    SetNamedUint32(env, result, "dirtyTop", 0);
    SetNamedUint32(env, result, "dirtyWidth", 0);
    SetNamedUint32(env, result, "dirtyHeight", 0);
    SetNamedUint32(env, result, "frameStride", 0);
    SetNamedUint64(env, result, "frameSequence", 0);
    return result;
  }

  SetNamedUint32(env, result, "width", snapshot.width);
  SetNamedUint32(env, result, "height", snapshot.height);
  SetNamedUint32(env, result, "dirtyLeft", snapshot.dirtyLeft);
  SetNamedUint32(env, result, "dirtyTop", snapshot.dirtyTop);
  SetNamedUint32(env, result, "dirtyWidth", snapshot.dirtyWidth);
  SetNamedUint32(env, result, "dirtyHeight", snapshot.dirtyHeight);
  SetNamedUint32(env, result, "frameStride", harmony_session_get_frame_stride(sessionId));
  SetNamedUint64(env, result, "frameSequence", harmony_session_get_frame_sequence(sessionId));
  return result;
}

napi_value BuildSessionEventsArray(napi_env env, int sessionId) {
  napi_value result = nullptr;
  napi_create_array(env, &result);

  const std::vector<HarmonySessionEvent> events = Adapter().DrainEvents(sessionId);
  for (std::size_t index = 0; index < events.size(); index++) {
    napi_value eventObject = nullptr;
    napi_create_object(env, &eventObject);
    SetNamedUint32(env, eventObject, "type", static_cast<std::uint32_t>(events[index].type));
    SetNamedUint32(env, eventObject, "stage", static_cast<std::uint32_t>(events[index].stage));
    SetNamedUint32(env, eventObject, "lastError", events[index].lastError);
    SetNamedBool(env, eventObject, "connected", events[index].connected);
    SetNamedUint64(env, eventObject, "frameSequence", events[index].frameSequence);
    SetNamedString(env, eventObject, "message", events[index].message);
    SetNamedString(env, eventObject, "clipboardText", events[index].clipboardText);
    SetNamedString(env, eventObject, "certHost", events[index].certHost);
    SetNamedString(env, eventObject, "certSubject", events[index].certSubject);
    SetNamedString(env, eventObject, "certIssuer", events[index].certIssuer);
    SetNamedString(env, eventObject, "certFingerprint", events[index].certFingerprint);
    SetNamedString(env, eventObject, "authUsername", events[index].authUsername);
    SetNamedString(env, eventObject, "authDomain", events[index].authDomain);
    napi_set_element(env, result, static_cast<uint32_t>(index), eventObject);
  }

  return result;
}

napi_value CreateSessionNapi(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = { nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 1) {
    napi_throw_error(env, nullptr, "createSession expects one config object");
    return nullptr;
  }

  HarmonySessionConfig config = GetDefaultConfig();
  GetNamedString(env, argv[0], "name", &config.name);
  GetNamedString(env, argv[0], "host", &config.host);
  GetNamedUint16(env, argv[0], "port", &config.port);
  GetNamedString(env, argv[0], "clientHostname", &config.clientHostname);
  GetNamedString(env, argv[0], "username", &config.username);
  GetNamedString(env, argv[0], "password", &config.password);
  GetNamedString(env, argv[0], "domain", &config.domain);
  GetNamedString(env, argv[0], "gateway", &config.gateway);
  GetNamedString(env, argv[0], "gatewayUsername", &config.gatewayUsername);
  GetNamedString(env, argv[0], "gatewayPassword", &config.gatewayPassword);
  GetNamedString(env, argv[0], "gatewayDomain", &config.gatewayDomain);
  GetNamedString(env, argv[0], "securityMode", &config.securityMode);
  GetNamedString(env, argv[0], "appDataDir", &config.appDataDir);
  GetNamedBool(env, argv[0], "enableClipboard", &config.enableClipboard);
  GetNamedBool(env, argv[0], "enableAudio", &config.enableAudio);
  GetNamedBool(env, argv[0], "enableDrive", &config.enableDrive);
  GetNamedBool(env, argv[0], "ignoreCertificate", &config.ignoreCertificate);
  GetNamedUint32(env, argv[0], "desktopWidth", &config.desktopWidth);
  GetNamedUint32(env, argv[0], "desktopHeight", &config.desktopHeight);

  const HarmonySessionConfigC configC = {
    config.name.c_str(),
    config.host.c_str(),
    config.port,
    config.clientHostname.c_str(),
    config.username.c_str(),
    config.password.c_str(),
    config.domain.c_str(),
    config.gateway.c_str(),
    config.gatewayUsername.c_str(),
    config.gatewayPassword.c_str(),
    config.gatewayDomain.c_str(),
    config.securityMode.c_str(),
    config.appDataDir.c_str(),
    static_cast<std::uint8_t>(config.enableClipboard ? 1U : 0U),
    static_cast<std::uint8_t>(config.enableAudio ? 1U : 0U),
    static_cast<std::uint8_t>(config.enableDrive ? 1U : 0U),
    static_cast<std::uint8_t>(config.ignoreCertificate ? 1U : 0U),
    config.desktopWidth,
    config.desktopHeight
  };

  napi_value result = nullptr;
  napi_create_int32(env, harmony_session_create(&configC), &result);
  return result;
}

napi_value ConnectNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  harmony_session_connect(sessionId);
  return BuildInfoObject(env, sessionId);
}

napi_value DisconnectNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  harmony_session_disconnect(sessionId);
  return CreateUndefined(env);
}

napi_value GetSessionInfoNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  return BuildInfoObject(env, sessionId);
}

napi_value GetSnapshotNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  return BuildSnapshotObject(env, sessionId);
}

napi_value GetFrameBufferNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  const size_t size = harmony_session_copy_frame_bgra(sessionId, nullptr, 0);
  void* data = nullptr;
  napi_value result = nullptr;
  if (napi_create_arraybuffer(env, size, &data, &result) != napi_ok) {
    return nullptr;
  }

  if ((size > 0) && (data != nullptr)) {
    harmony_session_copy_frame_bgra(sessionId, static_cast<std::uint8_t*>(data), size);
  }

  return result;
}

napi_value SetSurfaceIdNapi(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok) {
    return nullptr;
  }

  if (argc < 2) {
    return nullptr;
  }

  std::int32_t sessionId = 0;
  if (napi_get_value_int32(env, argv[0], &sessionId) != napi_ok) {
    return nullptr;
  }

  char surfaceId[256] = {0};
  size_t length = 0;
  if (napi_get_value_string_utf8(env, argv[1], surfaceId, sizeof(surfaceId), &length) != napi_ok) {
    return nullptr;
  }

  harmony_session_set_surface_id(sessionId, surfaceId);

  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value SubmitAuthNapi(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4] = {nullptr, nullptr, nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok) {
    return nullptr;
  }
  if (argc < 4) { return nullptr; }

  std::int32_t sessionId = 0;
  if (napi_get_value_int32(env, argv[0], &sessionId) != napi_ok) {
    return nullptr;
  }

  char username[256] = {0};
  size_t length = 0;
  napi_get_value_string_utf8(env, argv[1], username, sizeof(username), &length);

  char password[256] = {0};
  napi_get_value_string_utf8(env, argv[2], password, sizeof(password), &length);

  char domain[256] = {0};
  napi_get_value_string_utf8(env, argv[3], domain, sizeof(domain), &length);

  harmony_session_submit_auth(sessionId, username, password, domain);
  
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value SubmitCertNapi(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok) {
    return nullptr;
  }
  if (argc < 2) { return nullptr; }

  std::int32_t sessionId = 0;
  if (napi_get_value_int32(env, argv[0], &sessionId) != napi_ok) {
    return nullptr;
  }

  bool accept = false;
  if (napi_get_value_bool(env, argv[1], &accept) != napi_ok) {
    return nullptr;
  }

  harmony_session_submit_cert(sessionId, accept ? 1 : 0);
  
  napi_value result = nullptr;
  napi_get_undefined(env, &result);
  return result;
}

napi_value GetLastErrorStringNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  napi_value result = nullptr;
  const std::string lastError = BuildLastErrorString(sessionId);
  if (napi_create_string_utf8(env, lastError.c_str(), lastError.size(), &result) != napi_ok) {
    return nullptr;
  }
  return result;
}

napi_value DrainEventsNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }
  return BuildSessionEventsArray(env, sessionId);
}

napi_value SetClipboardTextNapi(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = { nullptr, nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 2) {
    napi_throw_error(env, nullptr, "setClipboardText expects sessionId and text");
    return nullptr;
  }

  std::int32_t sessionId = 0;
  if (napi_get_value_int32(env, argv[0], &sessionId) != napi_ok) {
    return nullptr;
  }

  size_t length = 0;
  if (napi_get_value_string_utf8(env, argv[1], nullptr, 0, &length) != napi_ok) {
    return nullptr;
  }
  std::string text(length, '\0');
  size_t written = 0;
  if (napi_get_value_string_utf8(env, argv[1], text.data(), text.size() + 1, &written) != napi_ok) {
    return nullptr;
  }
  text.resize(written);
  return CreateBoolean(env, Adapter().SetClipboardText(sessionId, text));
}

napi_value SendMouseMoveNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  if (!GetThreeInt32Args(env, info, &sessionId, &x, &y)) {
    return nullptr;
  }

  return CreateBoolean(env, harmony_session_send_mouse_move(sessionId, static_cast<std::uint16_t>(x),
                                                            static_cast<std::uint16_t>(y)) == 0);
}

napi_value SendMouseButtonNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t buttonFlags = 0;
  bool down = false;
  if (!GetFourArgsMouseButton(env, info, &sessionId, &x, &y, &buttonFlags, &down)) {
    return nullptr;
  }

  return CreateBoolean(
      env, harmony_session_send_mouse_button(sessionId, static_cast<std::uint16_t>(x),
                                             static_cast<std::uint16_t>(y),
                                             static_cast<std::uint16_t>(buttonFlags),
                                             down ? 1U : 0U) == 0);
}

napi_value SendMouseWheelNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t delta = 0;
  if (!GetFourInt32Args(env, info, &sessionId, &x, &y, &delta)) {
    return nullptr;
  }

  return CreateBoolean(
      env, harmony_session_send_mouse_wheel(sessionId, static_cast<std::uint16_t>(x),
                                            static_cast<std::uint16_t>(y),
                                            static_cast<std::int16_t>(delta)) == 0);
}

napi_value SendKeyScancodeNapi(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3] = { nullptr, nullptr, nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 3) {
    napi_throw_error(env, nullptr, "sendKeyScancode expects three arguments");
    return nullptr;
  }

  std::int32_t sessionId = 0;
  std::uint32_t scancode = 0;
  bool down = false;
  if ((napi_get_value_int32(env, argv[0], &sessionId) != napi_ok) ||
      (napi_get_value_uint32(env, argv[1], &scancode) != napi_ok) ||
      (napi_get_value_bool(env, argv[2], &down) != napi_ok)) {
    return nullptr;
  }

  return CreateBoolean(env, harmony_session_send_key_scancode(sessionId, scancode,
                                                              down ? 1U : 0U) == 0);
}

napi_value SendUnicodeKeyNapi(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3] = { nullptr, nullptr, nullptr };
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc != 3) {
    napi_throw_error(env, nullptr, "sendUnicodeKey expects three arguments");
    return nullptr;
  }

  std::int32_t sessionId = 0;
  std::uint32_t codepoint = 0;
  bool down = false;
  if ((napi_get_value_int32(env, argv[0], &sessionId) != napi_ok) ||
      (napi_get_value_uint32(env, argv[1], &codepoint) != napi_ok) ||
      (napi_get_value_bool(env, argv[2], &down) != napi_ok)) {
    return nullptr;
  }

  return CreateBoolean(env, harmony_session_send_unicode_key(sessionId,
                                                             static_cast<std::uint16_t>(codepoint),
                                                             down ? 1U : 0U) == 0);
}

napi_value SendCtrlAltDelNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  const bool ok = SendKeyCombo(sessionId,
                               { kScancodeLeftControl, kScancodeLeftAlt,
                                 kScancodeDelete },
                               { kScancodeDelete, kScancodeLeftAlt,
                                 kScancodeLeftControl });
  napi_value result = nullptr;
  napi_get_boolean(env, ok, &result);
  return result;
}

napi_value SendWindowsKeyNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  const bool ok = SendKeyCombo(sessionId, { kScancodeLeftWin }, { kScancodeLeftWin });
  napi_value result = nullptr;
  napi_get_boolean(env, ok, &result);
  return result;
}

napi_value SendEscapeNapi(napi_env env, napi_callback_info info) {
  std::int32_t sessionId = 0;
  if (!GetInt32Arg(env, info, &sessionId)) {
    return nullptr;
  }

  const bool ok = SendKeyCombo(sessionId, { kScancodeEscape }, { kScancodeEscape });
  napi_value result = nullptr;
  napi_get_boolean(env, ok, &result);
  return result;
}

static napi_value Init(napi_env env, napi_value exports) {
  const napi_property_descriptor descriptors[] = {
    { "createSession", nullptr, CreateSessionNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "connect", nullptr, ConnectNapi, nullptr, nullptr, nullptr, kDefaultPropertyAttributes,
      nullptr },
    { "disconnect", nullptr, DisconnectNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "getSessionInfo", nullptr, GetSessionInfoNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "getSnapshot", nullptr, GetSnapshotNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "getFrameBuffer", nullptr, GetFrameBufferNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "setSurfaceId", nullptr, SetSurfaceIdNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "submitAuth", nullptr, SubmitAuthNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "submitCert", nullptr, SubmitCertNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "getLastErrorString", nullptr, GetLastErrorStringNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "drainEvents", nullptr, DrainEventsNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "setClipboardText", nullptr, SetClipboardTextNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendMouseMove", nullptr, SendMouseMoveNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendMouseButton", nullptr, SendMouseButtonNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendMouseWheel", nullptr, SendMouseWheelNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendKeyScancode", nullptr, SendKeyScancodeNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendUnicodeKey", nullptr, SendUnicodeKeyNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendCtrlAltDel", nullptr, SendCtrlAltDelNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendWindowsKey", nullptr, SendWindowsKeyNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr },
    { "sendEscape", nullptr, SendEscapeNapi, nullptr, nullptr, nullptr,
      kDefaultPropertyAttributes, nullptr }
  };
  napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
  return exports;
}

static napi_module harmonyModule = {
  .nm_version = 1,
  .nm_flags = 0,
  .nm_filename = nullptr,
  .nm_register_func = Init,
  .nm_modname = "freerdp_harmony_bridge",
  .nm_priv = nullptr,
  .reserved = { 0 }
};

#endif

}  // namespace

extern "C" {

int harmony_session_create(const HarmonySessionConfigC* config) {
  if (config == nullptr) {
    return -1;
  }

  HarmonySessionConfig cppConfig{};
  cppConfig.name = ToString(config->name);
  cppConfig.host = ToString(config->host);
  cppConfig.port = config->port;
  cppConfig.clientHostname = ToString(config->clientHostname);
  cppConfig.username = ToString(config->username);
  cppConfig.password = ToString(config->password);
  cppConfig.domain = ToString(config->domain);
  cppConfig.gateway = ToString(config->gateway);
  cppConfig.gatewayUsername = ToString(config->gatewayUsername);
  cppConfig.gatewayPassword = ToString(config->gatewayPassword);
  cppConfig.gatewayDomain = ToString(config->gatewayDomain);
  cppConfig.securityMode = ToString(config->securityMode);
  cppConfig.appDataDir = ToString(config->appDataDir);
  cppConfig.enableClipboard = config->enableClipboard != 0;
  cppConfig.enableAudio = config->enableAudio != 0;
  cppConfig.enableDrive = config->enableDrive != 0;
  cppConfig.ignoreCertificate = config->ignoreCertificate != 0;
  cppConfig.desktopWidth = config->desktopWidth;
  cppConfig.desktopHeight = config->desktopHeight;
  return Adapter().CreateSession(cppConfig);
}

int harmony_session_connect(int sessionId) {
  return Adapter().Connect(sessionId) ? 0 : -1;
}

int harmony_session_disconnect(int sessionId) {
  return Adapter().Disconnect(sessionId) ? 0 : -1;
}

int harmony_session_get_snapshot(int sessionId, HarmonySurfaceSnapshotC* snapshot) {
  if (snapshot == nullptr) {
    return -1;
  }

  const HarmonySurfaceSnapshot value = Adapter().GetSnapshot(sessionId);
  snapshot->width = value.width;
  snapshot->height = value.height;
  snapshot->dirtyLeft = value.dirtyLeft;
  snapshot->dirtyTop = value.dirtyTop;
  snapshot->dirtyWidth = value.dirtyWidth;
  snapshot->dirtyHeight = value.dirtyHeight;
  return 0;
}

int harmony_session_get_info(int sessionId, HarmonySessionInfoC* info, char* messageBuffer,
                             size_t messageBufferSize) {
  if (info == nullptr) {
    return -1;
  }

  const HarmonySessionInfo value = Adapter().GetInfo(sessionId);
  info->stage = static_cast<uint32_t>(value.stage);
  info->lastError = value.lastError;
  info->connected = value.connected ? 1 : 0;

  if ((messageBuffer != nullptr) && (messageBufferSize > 0)) {
    const size_t toCopy = std::min(messageBufferSize - 1, value.message.size());
    std::memcpy(messageBuffer, value.message.data(), toCopy);
    messageBuffer[toCopy] = '\0';
  }

  return 0;
}

uint32_t harmony_session_get_frame_stride(int sessionId) {
  return Adapter().GetFrameStride(sessionId);
}

uint64_t harmony_session_get_frame_sequence(int sessionId) {
  return Adapter().GetFrameSequence(sessionId);
}

size_t harmony_session_copy_frame_bgra(int sessionId, uint8_t* buffer, size_t capacity) {
  const std::vector<uint8_t> frame = Adapter().GetFrameBGRA(sessionId);
  if (buffer == nullptr) {
    return frame.size();
  }

  const size_t toCopy = std::min(capacity, frame.size());
  if (toCopy > 0) {
    std::memcpy(buffer, frame.data(), toCopy);
  }
  return toCopy;
}

size_t harmony_session_get_last_error_string(int sessionId, char* buffer, size_t capacity) {
  const std::string lastError = BuildLastErrorString(sessionId);
  if (buffer == nullptr) {
    return lastError.size();
  }

  if (capacity == 0) {
    return 0;
  }

  const size_t toCopy = std::min(capacity - 1, lastError.size());
  if (toCopy > 0) {
    std::memcpy(buffer, lastError.data(), toCopy);
  }
  buffer[toCopy] = '\0';
  return toCopy;
}

int harmony_session_send_mouse_move(int sessionId, uint16_t x, uint16_t y) {
  return Adapter().SendMouseMove(sessionId, x, y) ? 0 : -1;
}

int harmony_session_send_mouse_button(int sessionId, uint16_t x, uint16_t y, uint16_t buttonFlags,
                                      uint8_t down) {
  return Adapter().SendMouseButton(sessionId, x, y, buttonFlags, down != 0) ? 0 : -1;
}

int harmony_session_send_mouse_wheel(int sessionId, uint16_t x, uint16_t y, int16_t delta) {
  return Adapter().SendMouseWheel(sessionId, x, y, delta) ? 0 : -1;
}

int harmony_session_send_key_scancode(int sessionId, uint32_t scancode, uint8_t down) {
  return Adapter().SendKeyScancode(sessionId, scancode, down != 0) ? 0 : -1;
}

int harmony_session_send_unicode_key(int sessionId, uint16_t codepoint, uint8_t down) {
  return Adapter().SendUnicodeKey(sessionId, codepoint, down != 0) ? 0 : -1;
}

int harmony_session_set_surface_id(int sessionId, const char* surfaceId) {
  return Adapter().SetSurfaceId(sessionId, surfaceId != nullptr ? surfaceId : "") ? 0 : -1;
}

#if FREERDP_HARMONY_HAS_NAPI
__attribute__((constructor)) void RegisterHarmonyModule(void) {
  napi_module_register(&harmonyModule);
}
#endif

}  // extern "C"
