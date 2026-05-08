#include "freerdp_session.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

#include <hilog/log.h>
#include <native_window/external_window.h>
#include <native_buffer/native_buffer.h>
#include <native_buffer/buffer_common.h>
#include <unistd.h>

extern "C" {
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/constants.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/input.h>
#include <freerdp/log.h>
#include <freerdp/settings.h>
#include <freerdp/utils/signal.h>
#include <winpr/crt.h>
#include <winpr/input.h>
#include <winpr/synch.h>
#include <winpr/wlog.h>
}

#define TAG CLIENT_TAG("harmony.native")

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#endif
#define LOG_DOMAIN 0x3201

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "freerdp_harmony"

namespace {

static void HiLogInfo(const std::string& message) {
  OH_LOG_INFO(LOG_APP, "%{public}s", message.c_str());
}

static void HiLogError(const std::string& message) {
  OH_LOG_ERROR(LOG_APP, "%{public}s", message.c_str());
}

constexpr DWORD kWaitTimeoutMs = 100;

struct HarmonyClientContext {
  rdpClientContext common;
  FreeRDPHarmonySession* session;
};

static FreeRDPHarmonySession* SessionFromContext(rdpContext* context) {
  if (context == nullptr) {
    return nullptr;
  }

  HarmonyClientContext* harmony = reinterpret_cast<HarmonyClientContext*>(context);
  return harmony->session;
}

static void ReplaceCString(char** target, const std::string& value) {
  if (target == nullptr) {
    return;
  }

  std::free(*target);
  *target = nullptr;

  if (!value.empty()) {
    *target = _strdup(value.c_str());
  }
}

static std::string BuildErrorMessage(const char* prefix, std::uint32_t lastError) {
  std::ostringstream stream;
  stream << prefix;
  if (lastError != 0U) {
    stream << " (0x" << std::hex << lastError << ")";
    const char* lastErrorString = freerdp_get_last_error_string(lastError);
    if ((lastErrorString != nullptr) && (lastErrorString[0] != '\0')) {
      stream << ": " << lastErrorString;
    }
  }
  return stream.str();
}

static bool IsSecurityModeEnabled(const std::string& mode, const char* expected) {
  return !mode.empty() && (mode == expected);
}

static std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

static bool IsGatewayAuthReason(rdp_auth_reason reason) {
  return (reason == GW_AUTH_HTTP) || (reason == GW_AUTH_RDG) || (reason == GW_AUTH_RPC);
}

static std::string StripTrailingNulls(const char* text, std::size_t size) {
  std::size_t actualSize = size;
  while ((actualSize > 0) && (text[actualSize - 1] == '\0')) {
    actualSize -= 1;
  }
  return std::string(text, actualSize);
}

static std::string Utf16ToUtf8(const WCHAR* text, std::size_t bytes) {
  if ((text == nullptr) || (bytes < sizeof(WCHAR))) {
    return std::string();
  }

  size_t utf8Length = 0;
  char* utf8 = ConvertWCharToUtf8Alloc(text, &utf8Length);
  if (utf8 == nullptr) {
    return std::string();
  }

  const std::string result = StripTrailingNulls(utf8, utf8Length);
  std::free(utf8);
  return result;
}

static std::vector<BYTE> Utf8ToClipboardData(const std::string& text, UINT32 formatId) {
  if (formatId == CF_UNICODETEXT) {
    size_t wideLength = 0;
    WCHAR* wideBuffer = ConvertUtf8ToWCharAlloc(text.c_str(), &wideLength);
    if (wideBuffer == nullptr) {
      return {};
    }

    const BYTE* begin = reinterpret_cast<const BYTE*>(wideBuffer);
    std::vector<BYTE> data(begin, begin + ((wideLength + 1) * sizeof(WCHAR)));
    std::free(wideBuffer);
    return data;
  }

  std::vector<BYTE> bytes(text.begin(), text.end());
  bytes.push_back('\0');
  return bytes;
}

static BOOL HarmonyBeginPaint(rdpContext* context) {
  if ((context == nullptr) || (context->gdi == nullptr) || (context->gdi->primary == nullptr) ||
      (context->gdi->primary->hdc == nullptr) ||
      (context->gdi->primary->hdc->hwnd == nullptr) ||
      (context->gdi->primary->hdc->hwnd->invalid == nullptr)) {
    return FALSE;
  }

  context->gdi->primary->hdc->hwnd->invalid->null = TRUE;
  return TRUE;
}

static BOOL HarmonyEndPaint(rdpContext* context) {
  FreeRDPHarmonySession* session = SessionFromContext(context);
  if ((session == nullptr) || (context == nullptr) || (context->gdi == nullptr) ||
      (context->gdi->primary == nullptr) || (context->gdi->primary->hdc == nullptr)) {
    return FALSE;
  }

  rdpGdi* gdi = context->gdi;
  HGDI_WND hwnd = gdi->primary->hdc->hwnd;
  if ((hwnd == nullptr) || (gdi->primary_buffer == nullptr)) {
    return FALSE;
  }

  std::uint32_t left = 0;
  std::uint32_t top = 0;
  std::uint32_t dirtyWidth = gdi->width;
  std::uint32_t dirtyHeight = gdi->height;

  const INT32 ninvalid = hwnd->ninvalid;
  const HGDI_RGN cinvalid = hwnd->cinvalid;
  if ((ninvalid > 0) && (cinvalid != nullptr)) {
    INT32 x1 = cinvalid[0].x;
    INT32 y1 = cinvalid[0].y;
    INT32 x2 = cinvalid[0].x + cinvalid[0].w;
    INT32 y2 = cinvalid[0].y + cinvalid[0].h;

    for (INT32 index = 1; index < ninvalid; index++) {
      x1 = std::min(x1, cinvalid[index].x);
      y1 = std::min(y1, cinvalid[index].y);
      x2 = std::max(x2, cinvalid[index].x + cinvalid[index].w);
      y2 = std::max(y2, cinvalid[index].y + cinvalid[index].h);
    }

    left = static_cast<std::uint32_t>(std::max<INT32>(0, x1));
    top = static_cast<std::uint32_t>(std::max<INT32>(0, y1));
    dirtyWidth = static_cast<std::uint32_t>(std::max<INT32>(0, x2 - x1));
    dirtyHeight = static_cast<std::uint32_t>(std::max<INT32>(0, y2 - y1));
  }

  const std::size_t bufferSize =
      static_cast<std::size_t>(gdi->stride) * static_cast<std::size_t>(gdi->height);
  session->UpdateFrame(gdi->width, gdi->height, left, top, dirtyWidth, dirtyHeight,
                       gdi->primary_buffer, bufferSize, gdi->stride);

  if (hwnd->invalid != nullptr) {
    hwnd->invalid->null = TRUE;
  }
  hwnd->ninvalid = 0;
  return TRUE;
}

static BOOL HarmonyDesktopResize(rdpContext* context) {
  FreeRDPHarmonySession* session = SessionFromContext(context);
  if ((session == nullptr) || (context == nullptr) || (context->settings == nullptr)) {
    return FALSE;
  }

  const std::uint32_t width =
      freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopWidth);
  const std::uint32_t height =
      freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopHeight);

  if ((context->gdi != nullptr) && !gdi_resize(context->gdi, width, height)) {
    return FALSE;
  }

  session->UpdateSnapshot(width, height, 0, 0, width, height);
  if ((context->gdi != nullptr) && (context->gdi->primary_buffer != nullptr)) {
    const std::size_t bufferSize =
        static_cast<std::size_t>(context->gdi->stride) * static_cast<std::size_t>(height);
    session->UpdateFrame(width, height, 0, 0, width, height, context->gdi->primary_buffer,
                         bufferSize, context->gdi->stride);
  }
  return TRUE;
}

static UINT HarmonyCliprdrSendClientCapabilities(CliprdrClientContext* cliprdr) {
  if ((cliprdr == nullptr) || (cliprdr->ClientCapabilities == nullptr)) {
    return CHANNEL_RC_BAD_INIT_HANDLE;
  }

  CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilities = {};
  CLIPRDR_CAPABILITIES capabilities = {};
  generalCapabilities.capabilitySetType = CB_CAPSTYPE_GENERAL;
  generalCapabilities.capabilitySetLength = 12;
  generalCapabilities.version = CB_CAPS_VERSION_2;
  generalCapabilities.generalFlags = CB_USE_LONG_FORMAT_NAMES;
  capabilities.cCapabilitiesSets = 1;
  capabilities.capabilitySets = reinterpret_cast<CLIPRDR_CAPABILITY_SET*>(&generalCapabilities);
  return cliprdr->ClientCapabilities(cliprdr, &capabilities);
}

static UINT HarmonyCliprdrMonitorReady(CliprdrClientContext* cliprdr,
                                       const CLIPRDR_MONITOR_READY* monitorReady) {
  WINPR_UNUSED(monitorReady);
  if ((cliprdr == nullptr) || (cliprdr->custom == nullptr)) {
    return ERROR_INVALID_PARAMETER;
  }

  FreeRDPHarmonySession* session =
      reinterpret_cast<FreeRDPHarmonySession*>(cliprdr->custom);
  session->MarkClipboardReady(true);
  if (HarmonyCliprdrSendClientCapabilities(cliprdr) != CHANNEL_RC_OK) {
    return ERROR_INTERNAL_ERROR;
  }
  return session->SendClipboardFormatList() ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static UINT HarmonyCliprdrServerCapabilities(CliprdrClientContext* cliprdr,
                                             const CLIPRDR_CAPABILITIES* capabilities) {
  WINPR_UNUSED(cliprdr);
  WINPR_UNUSED(capabilities);
  return CHANNEL_RC_OK;
}

static UINT HarmonyCliprdrServerFormatList(CliprdrClientContext* cliprdr,
                                           const CLIPRDR_FORMAT_LIST* formatList) {
  if ((cliprdr == nullptr) || (cliprdr->custom == nullptr) || (formatList == nullptr)) {
    return ERROR_INVALID_PARAMETER;
  }

  FreeRDPHarmonySession* session =
      reinterpret_cast<FreeRDPHarmonySession*>(cliprdr->custom);
  for (UINT32 index = 0; index < formatList->numFormats; index++) {
    const UINT32 formatId = formatList->formats[index].formatId;
    if ((formatId == CF_UNICODETEXT) || (formatId == CF_TEXT)) {
      return session->SendClipboardDataRequest(formatId) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
    }
  }
  return CHANNEL_RC_OK;
}

static UINT HarmonyCliprdrServerFormatDataRequest(
    CliprdrClientContext* cliprdr, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest) {
  if ((cliprdr == nullptr) || (cliprdr->custom == nullptr) || (formatDataRequest == nullptr) ||
      (cliprdr->ClientFormatDataResponse == nullptr)) {
    return ERROR_INVALID_PARAMETER;
  }

  FreeRDPHarmonySession* session =
      reinterpret_cast<FreeRDPHarmonySession*>(cliprdr->custom);
  const std::string clipboardText = session->GetLocalClipboardText();
  std::vector<BYTE> data =
      Utf8ToClipboardData(clipboardText, formatDataRequest->requestedFormatId);

  CLIPRDR_FORMAT_DATA_RESPONSE response = {};
  response.common.msgFlags = data.empty() ? CB_RESPONSE_FAIL : CB_RESPONSE_OK;
  response.common.dataLen = static_cast<UINT32>(data.size());
  response.requestedFormatData = data.empty() ? nullptr : data.data();
  return cliprdr->ClientFormatDataResponse(cliprdr, &response);
}

static UINT HarmonyCliprdrServerFormatDataResponse(
    CliprdrClientContext* cliprdr, const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse) {
  if ((cliprdr == nullptr) || (cliprdr->custom == nullptr) || (formatDataResponse == nullptr)) {
    return ERROR_INVALID_PARAMETER;
  }

  if ((formatDataResponse->requestedFormatData == nullptr) ||
      (formatDataResponse->common.dataLen == 0)) {
    return CHANNEL_RC_OK;
  }

  FreeRDPHarmonySession* session =
      reinterpret_cast<FreeRDPHarmonySession*>(cliprdr->custom);
  std::string clipboardText = Utf16ToUtf8(
      reinterpret_cast<const WCHAR*>(formatDataResponse->requestedFormatData),
      formatDataResponse->common.dataLen);
  if (clipboardText.empty()) {
    clipboardText = StripTrailingNulls(
        reinterpret_cast<const char*>(formatDataResponse->requestedFormatData),
        formatDataResponse->common.dataLen);
  }

  return session->OnRemoteClipboardData(clipboardText) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static UINT HarmonyCliprdrServerFormatListResponse(
    CliprdrClientContext* cliprdr, const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse) {
  WINPR_UNUSED(cliprdr);
  WINPR_UNUSED(formatListResponse);
  return CHANNEL_RC_OK;
}

static UINT HarmonyCliprdrStubLock(CliprdrClientContext* cliprdr,
                                   const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData) {
  WINPR_UNUSED(cliprdr);
  WINPR_UNUSED(lockClipboardData);
  return CHANNEL_RC_OK;
}

static UINT HarmonyCliprdrStubUnlock(
    CliprdrClientContext* cliprdr, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData) {
  WINPR_UNUSED(cliprdr);
  WINPR_UNUSED(unlockClipboardData);
  return CHANNEL_RC_OK;
}

static UINT HarmonyCliprdrStubFileContentsRequest(
    CliprdrClientContext* cliprdr, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest) {
  WINPR_UNUSED(cliprdr);
  WINPR_UNUSED(fileContentsRequest);
  return CHANNEL_RC_OK;
}

static UINT HarmonyCliprdrStubFileContentsResponse(
    CliprdrClientContext* cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse) {
  WINPR_UNUSED(cliprdr);
  WINPR_UNUSED(fileContentsResponse);
  return CHANNEL_RC_OK;
}

static BOOL HarmonyCliprdrInit(FreeRDPHarmonySession* session, CliprdrClientContext* cliprdr) {
  if ((session == nullptr) || (cliprdr == nullptr)) {
    return FALSE;
  }

  cliprdr->custom = session;
  cliprdr->MonitorReady = HarmonyCliprdrMonitorReady;
  cliprdr->ServerCapabilities = HarmonyCliprdrServerCapabilities;
  cliprdr->ServerFormatList = HarmonyCliprdrServerFormatList;
  cliprdr->ServerFormatListResponse = HarmonyCliprdrServerFormatListResponse;
  cliprdr->ServerLockClipboardData = HarmonyCliprdrStubLock;
  cliprdr->ServerUnlockClipboardData = HarmonyCliprdrStubUnlock;
  cliprdr->ServerFormatDataRequest = HarmonyCliprdrServerFormatDataRequest;
  cliprdr->ServerFormatDataResponse = HarmonyCliprdrServerFormatDataResponse;
  cliprdr->ServerFileContentsRequest = HarmonyCliprdrStubFileContentsRequest;
  cliprdr->ServerFileContentsResponse = HarmonyCliprdrStubFileContentsResponse;
  session->SetCliprdrContext(cliprdr);
  session->MarkClipboardReady(false);
  return TRUE;
}

static void HarmonyCliprdrUninit(FreeRDPHarmonySession* session, CliprdrClientContext* cliprdr) {
  if ((session == nullptr) || (cliprdr == nullptr)) {
    return;
  }

  cliprdr->custom = nullptr;
  session->SetCliprdrContext(nullptr);
  session->MarkClipboardReady(false);
}

static void HarmonyOnChannelConnectedEventHandler(void* context,
                                                  const ChannelConnectedEventArgs* eventArgs) {
  if ((context == nullptr) || (eventArgs == nullptr)) {
    return;
  }

  rdpContext* rdpContextPointer = reinterpret_cast<rdpContext*>(context);
  FreeRDPHarmonySession* session = SessionFromContext(rdpContextPointer);
  if ((session != nullptr) && (std::strcmp(eventArgs->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)) {
    HarmonyCliprdrInit(session, reinterpret_cast<CliprdrClientContext*>(eventArgs->pInterface));
    return;
  }

  freerdp_client_OnChannelConnectedEventHandler(context, eventArgs);
}

static void HarmonyOnChannelDisconnectedEventHandler(
    void* context, const ChannelDisconnectedEventArgs* eventArgs) {
  if ((context == nullptr) || (eventArgs == nullptr)) {
    return;
  }

  rdpContext* rdpContextPointer = reinterpret_cast<rdpContext*>(context);
  FreeRDPHarmonySession* session = SessionFromContext(rdpContextPointer);
  if ((session != nullptr) && (std::strcmp(eventArgs->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)) {
    HarmonyCliprdrUninit(session, reinterpret_cast<CliprdrClientContext*>(eventArgs->pInterface));
    return;
  }

  freerdp_client_OnChannelDisconnectedEventHandler(context, eventArgs);
}

static BOOL HarmonyPreConnect(freerdp* instance) {
  if ((instance == nullptr) || (instance->context == nullptr) ||
      (instance->context->settings == nullptr)) {
    return FALSE;
  }

  rdpSettings* settings = instance->context->settings;
  if (!freerdp_settings_set_bool(settings, FreeRDP_CertificateCallbackPreferPEM, TRUE) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_OsMajorType, OSMAJORTYPE_UNIX) ||
      !freerdp_settings_set_uint32(settings, FreeRDP_OsMinorType, OSMINORTYPE_NATIVE_XSERVER)) {
    return FALSE;
  }

  if (PubSub_SubscribeChannelConnected(
          instance->context->pubSub,
          HarmonyOnChannelConnectedEventHandler) < 0) {
    return FALSE;
  }

  if (PubSub_SubscribeChannelDisconnected(
          instance->context->pubSub,
          HarmonyOnChannelDisconnectedEventHandler) < 0) {
    PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
                                       HarmonyOnChannelConnectedEventHandler);
    return FALSE;
  }

  return TRUE;
}

static BOOL HarmonyPostConnect(freerdp* instance) {
  if ((instance == nullptr) || (instance->context == nullptr) ||
      (instance->context->update == nullptr)) {
    return FALSE;
  }

  if (!gdi_init(instance, PIXEL_FORMAT_BGRA32)) {
    return FALSE;
  }

  rdpContext* context = instance->context;
  context->update->BeginPaint = HarmonyBeginPaint;
  context->update->EndPaint = HarmonyEndPaint;
  context->update->DesktopResize = HarmonyDesktopResize;

  FreeRDPHarmonySession* session = SessionFromContext(context);
  if (session != nullptr) {
    const std::uint32_t width =
        freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopWidth);
    const std::uint32_t height =
        freerdp_settings_get_uint32(context->settings, FreeRDP_DesktopHeight);
    session->UpdateStage(HarmonySessionStage::kConnected, 0, "已连接到远程桌面");
    session->UpdateSnapshot(width, height, 0, 0, width, height);
    if ((context->gdi != nullptr) && (context->gdi->primary_buffer != nullptr)) {
      const std::size_t bufferSize =
          static_cast<std::size_t>(context->gdi->stride) * static_cast<std::size_t>(height);
      session->UpdateFrame(width, height, 0, 0, width, height, context->gdi->primary_buffer,
                           bufferSize, context->gdi->stride);
    }
  }

  return TRUE;
}

static void HarmonyPostDisconnect(freerdp* instance) {
  if ((instance == nullptr) || (instance->context == nullptr)) {
    return;
  }

  FreeRDPHarmonySession* session = SessionFromContext(instance->context);
  const std::uint32_t lastError = freerdp_get_last_error(instance->context);
  if (session != nullptr) {
    session->UpdateStage(HarmonySessionStage::kDisconnected, lastError, "连接已断开");
  }

  PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
                                     HarmonyOnChannelConnectedEventHandler);
  PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
                                        HarmonyOnChannelDisconnectedEventHandler);
  gdi_free(instance);
}

static BOOL HarmonyAuthenticateEx(freerdp* instance, char** username, char** password,
                                  char** domain, rdp_auth_reason reason) {
  WINPR_UNUSED(reason);

  if ((instance == nullptr) || (instance->context == nullptr)) {
    return FALSE;
  }

  FreeRDPHarmonySession* session = SessionFromContext(instance->context);
  if (session == nullptr) {
    return FALSE;
  }

  session->FillAuthentication(username, password, domain, static_cast<std::uint32_t>(reason));
  return TRUE;
}

static DWORD HarmonyVerifyCertificateEx(freerdp* instance, const char* host, UINT16 port,
                                        const char* common_name, const char* subject,
                                        const char* issuer, const char* fingerprint,
                                        DWORD flags) {
  if ((instance == nullptr) || (instance->context == nullptr)) {
    return 0;
  }

  FreeRDPHarmonySession* session = SessionFromContext(instance->context);
  if (session == nullptr) {
    return 0;
  }

  return session->VerifyCertificate(host, port, common_name, subject, issuer, fingerprint, flags) ? 2U : 0U;
}

static DWORD HarmonyVerifyChangedCertificateEx(
    freerdp* instance, const char* host, UINT16 port, const char* common_name,
    const char* subject, const char* issuer, const char* new_fingerprint,
    const char* old_subject, const char* old_issuer, const char* old_fingerprint, DWORD flags) {
  if ((instance == nullptr) || (instance->context == nullptr)) {
    return 0;
  }

  FreeRDPHarmonySession* session = SessionFromContext(instance->context);
  if (session == nullptr) {
    return 0;
  }

  return session->VerifyChangedCertificate(host, port, common_name, subject, issuer,
                                           new_fingerprint, old_subject, old_issuer,
                                           old_fingerprint, flags) ? 2U : 0U;
}

static int HarmonyLogonErrorInfo(freerdp* instance, UINT32 data, UINT32 type) {
  WINPR_UNUSED(type);

  if ((instance == nullptr) || (instance->context == nullptr)) {
    return -1;
  }

  FreeRDPHarmonySession* session = SessionFromContext(instance->context);
  if (session == nullptr) {
    return -1;
  }

  const char* description = freerdp_get_logon_error_info_data(data);
  session->UpdateStage(HarmonySessionStage::kFailed, data,
                       description != nullptr ? description : "远端登录失败");
  return 1;
}

static BOOL HarmonyLogMessage(const wLogMessage* msg) {
  if (msg == nullptr) {
    return TRUE;
  }
  const char* levelStr = "INFO";
  switch (msg->Level) {
    case WLOG_TRACE: levelStr = "TRACE"; break;
    case WLOG_DEBUG: levelStr = "DEBUG"; break;
    case WLOG_INFO: levelStr = "INFO"; break;
    case WLOG_WARN: levelStr = "WARN"; break;
    case WLOG_ERROR: levelStr = "ERROR"; break;
    case WLOG_FATAL: levelStr = "FATAL"; break;
  }
  OH_LOG_INFO(LOG_APP, "[WLog %{public}s] %{public}s%{public}s", levelStr, msg->PrefixString ? msg->PrefixString : "", msg->TextString ? msg->TextString : "");
  return TRUE;
}

static BOOL HarmonyClientGlobalInit() {
  wLog* root = WLog_GetRoot();
  WLog_SetLogAppenderType(root, WLOG_APPENDER_CALLBACK);
  wLogAppender* appender = WLog_GetLogAppender(root);
  if (appender != nullptr) {
    wLogCallbacks callbacks = { 0 };
    callbacks.message = HarmonyLogMessage;
    WLog_ConfigureAppender(appender, "callbacks", (void*)&callbacks);
    wLogLayout* layout = WLog_GetLogLayout(root);
    if (layout) {
      WLog_Layout_SetPrefixFormat(root, layout, "[%lv] ");
    }
    WLog_OpenAppender(root);
  }
  return freerdp_handle_signals() == 0;
}

static void HarmonyClientGlobalUninit() {}

static BOOL HarmonyClientNew(freerdp* instance, rdpContext* context) {
  if ((instance == nullptr) || (context == nullptr)) {
    return FALSE;
  }

  HarmonyClientContext* harmony = reinterpret_cast<HarmonyClientContext*>(context);
  harmony->session = nullptr;

  instance->PreConnect = HarmonyPreConnect;
  instance->PostConnect = HarmonyPostConnect;
  instance->PostDisconnect = HarmonyPostDisconnect;
  instance->AuthenticateEx = HarmonyAuthenticateEx;
  instance->VerifyCertificateEx = HarmonyVerifyCertificateEx;
  instance->VerifyChangedCertificateEx = HarmonyVerifyChangedCertificateEx;
  instance->LogonErrorInfo = HarmonyLogonErrorInfo;
  return TRUE;
}

static void HarmonyClientFree(freerdp* instance, rdpContext* context) {
  WINPR_UNUSED(instance);

  if (context == nullptr) {
    return;
  }

  HarmonyClientContext* harmony = reinterpret_cast<HarmonyClientContext*>(context);
  harmony->session = nullptr;
}

static int HarmonyClientStart(rdpContext* context) {
  WINPR_UNUSED(context);
  return 0;
}

static int HarmonyClientStop(rdpContext* context) {
  WINPR_UNUSED(context);
  return 0;
}

static int HarmonyRdpClientEntry(RDP_CLIENT_ENTRY_POINTS* entryPoints) {
  if (entryPoints == nullptr) {
    return -1;
  }

  ZeroMemory(entryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
  entryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
  entryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
  entryPoints->GlobalInit = HarmonyClientGlobalInit;
  entryPoints->GlobalUninit = HarmonyClientGlobalUninit;
  entryPoints->ContextSize = sizeof(HarmonyClientContext);
  entryPoints->ClientNew = HarmonyClientNew;
  entryPoints->ClientFree = HarmonyClientFree;
  entryPoints->ClientStart = HarmonyClientStart;
  entryPoints->ClientStop = HarmonyClientStop;
  return 0;
}

}  // namespace

FreeRDPHarmonySession::FreeRDPHarmonySession(HarmonySessionConfig config)
    : config_(std::move(config)),
      snapshot_{ 0, 0, 0, 0, 0, 0 },
      info_{ HarmonySessionStage::kIdle, 0, false, "尚未连接" },
      context_(nullptr),
      frameStride_(0),
      frameSequence_(0),
      hasPendingFrameEvent_(false),
      inputEventHandle_(CreateEventA(nullptr, TRUE, FALSE, nullptr)),
      cliprdr_(nullptr),
      clipboardReady_(false),
      started_(false),
      stopRequested_(false),
      nativeWindow_(nullptr),
      dialogAnswered_(false),
      certAccepted_(false) {}

FreeRDPHarmonySession::~FreeRDPHarmonySession() {
  Disconnect();

  if (nativeWindow_ != nullptr) {
    OH_NativeWindow_DestroyNativeWindow(nativeWindow_);
    nativeWindow_ = nullptr;
  }

  rdpContext* context = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    context = static_cast<rdpContext*>(context_);
    context_ = nullptr;
  }

  if (context != nullptr) {
    freerdp_client_stop(context);
    freerdp_client_context_free(context);
  }

  HANDLE inputEventHandle = static_cast<HANDLE>(inputEventHandle_);
  if (inputEventHandle != nullptr) {
    CloseHandle(inputEventHandle);
    inputEventHandle_ = nullptr;
  }
}

bool FreeRDPHarmonySession::Connect() {
  std::thread staleWorker;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) {
      HiLogInfo("ignore duplicate connect request: session is already running");
      return false;
    }
    stopRequested_ = false;
    if (worker_.joinable()) {
      staleWorker = std::move(worker_);
    }
  }

  if (staleWorker.joinable()) {
    staleWorker.join();
  }

  if (!EnsureContext()) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = true;
    info_.stage = HarmonySessionStage::kConnecting;
    info_.lastError = 0;
    info_.connected = false;
    info_.message = "正在连接远程桌面...";
  }
  stateChanged_.notify_all();

  worker_ = std::thread(&FreeRDPHarmonySession::ThreadMain, this);
  return true;
}

bool FreeRDPHarmonySession::Disconnect() {
  std::thread worker;
  rdpContext* context = nullptr;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopRequested_ = true;
    context = static_cast<rdpContext*>(context_);
    worker = std::move(worker_);
  }

  HANDLE inputEventHandle = static_cast<HANDLE>(inputEventHandle_);
  if (inputEventHandle != nullptr) {
    SetEvent(inputEventHandle);
  }

  if (context != nullptr) {
    freerdp_abort_connect_context(context);
    if (context->instance != nullptr) {
      freerdp_disconnect(context->instance);
    }
  }

  if (worker.joinable()) {
    worker.join();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
    if (info_.stage != HarmonySessionStage::kFailed) {
      info_.stage = HarmonySessionStage::kDisconnected;
      info_.connected = false;
      info_.message = "连接已断开";
    }
  }
  stateChanged_.notify_all();

  if (context != nullptr) {
    freerdp_client_stop(context);
    freerdp_client_context_free(context);

    std::lock_guard<std::mutex> lock(mutex_);
    if (context_ == context) {
      context_ = nullptr;
    }
  }

  return true;
}

HarmonySurfaceSnapshot FreeRDPHarmonySession::GetSnapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}

HarmonySessionInfo FreeRDPHarmonySession::GetInfo() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return info_;
}

std::uint32_t FreeRDPHarmonySession::GetFrameStride() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return frameStride_;
}

std::uint64_t FreeRDPHarmonySession::GetFrameSequence() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return frameSequence_;
}

std::vector<std::uint8_t> FreeRDPHarmonySession::GetFrameBGRA() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return frameBuffer_;
}

std::vector<HarmonySessionEvent> FreeRDPHarmonySession::DrainEvents() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<HarmonySessionEvent> events;
  while (!pendingSessionEvents_.empty()) {
    HarmonySessionEvent event = pendingSessionEvents_.front();
    pendingSessionEvents_.pop_front();
    if (event.type == HarmonySessionEventType::kFrameUpdated) {
      hasPendingFrameEvent_ = false;
    }
    events.push_back(std::move(event));
  }
  return events;
}

bool FreeRDPHarmonySession::ShouldIgnoreCertificate() const {
  return config_.ignoreCertificate;
}

bool FreeRDPHarmonySession::SetClipboardText(const std::string& text) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    changed = (localClipboardText_ != text);
    localClipboardText_ = text;
  }

  if (!changed) {
    return true;
  }
  return SendClipboardFormatList();
}

void FreeRDPHarmonySession::SetCliprdrContext(void* cliprdr) {
  std::lock_guard<std::mutex> lock(mutex_);
  cliprdr_ = cliprdr;
}

void FreeRDPHarmonySession::MarkClipboardReady(bool ready) {
  std::lock_guard<std::mutex> lock(mutex_);
  clipboardReady_ = ready;
}

std::string FreeRDPHarmonySession::GetLocalClipboardText() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return localClipboardText_;
}

bool FreeRDPHarmonySession::SendClipboardFormatList() {
  CliprdrClientContext* cliprdr = nullptr;
  std::string clipboardText;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!clipboardReady_ || (cliprdr_ == nullptr)) {
      return true;
    }
    cliprdr = static_cast<CliprdrClientContext*>(cliprdr_);
    clipboardText = localClipboardText_;
  }

  if (cliprdr->ClientFormatList == nullptr) {
    return false;
  }

  CLIPRDR_FORMAT format = {};
  CLIPRDR_FORMAT_LIST formatList = {};
  format.formatId = CF_UNICODETEXT;
  format.formatName = nullptr;
  formatList.common.msgFlags = 0;
  formatList.common.msgType = CB_FORMAT_LIST;
  formatList.numFormats = clipboardText.empty() ? 0 : 1;
  formatList.formats = clipboardText.empty() ? nullptr : &format;
  return cliprdr->ClientFormatList(cliprdr, &formatList) == CHANNEL_RC_OK;
}

bool FreeRDPHarmonySession::SendClipboardDataRequest(std::uint32_t formatId) {
  CliprdrClientContext* cliprdr = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cliprdr = static_cast<CliprdrClientContext*>(cliprdr_);
  }

  if ((cliprdr == nullptr) || (cliprdr->ClientFormatDataRequest == nullptr)) {
    return false;
  }

  CLIPRDR_FORMAT_DATA_REQUEST request = {};
  request.common.msgFlags = 0;
  request.common.msgType = CB_FORMAT_DATA_REQUEST;
  request.requestedFormatId = formatId;
  return cliprdr->ClientFormatDataRequest(cliprdr, &request) == CHANNEL_RC_OK;
}

bool FreeRDPHarmonySession::OnRemoteClipboardData(const std::string& text) {
  if (text.empty()) {
    return true;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  pendingSessionEvents_.push_back(HarmonySessionEvent{
      HarmonySessionEventType::kRemoteClipboardChanged, info_.stage, info_.lastError,
      info_.connected, frameSequence_, "", text });
  return true;
}

void FreeRDPHarmonySession::FillAuthentication(char** username, char** password, char** domain,
                                               std::uint32_t reason) {
  if (IsGatewayAuthReason(static_cast<rdp_auth_reason>(reason))) {
    ReplaceCString(username, config_.gatewayUsername.empty() ? config_.username : config_.gatewayUsername);
    ReplaceCString(password, config_.gatewayPassword.empty() ? config_.password : config_.gatewayPassword);
    ReplaceCString(domain, config_.gatewayDomain.empty() ? config_.domain : config_.gatewayDomain);
    return;
  }

  const auto authReason = static_cast<rdp_auth_reason>(reason);

  if (!config_.username.empty() && !config_.password.empty()) {
    HiLogInfo("FillAuthentication: using configured credentials");
    ReplaceCString(username, config_.username);
    ReplaceCString(password, config_.password);
    ReplaceCString(domain, config_.domain);
    return;
  }

  switch (authReason) {
    case AUTH_NLA:
      HiLogInfo("FillAuthentication: NLA auth required, showing dialog");
      break;

    case AUTH_TLS:
    case AUTH_RDP:
      if ((*username != nullptr && **username != '\0') &&
          (*password != nullptr && **password != '\0')) {
        HiLogInfo("FillAuthentication: TLS/RDP with existing credentials, using them");
        return;
      }
      HiLogInfo("FillAuthentication: TLS/RDP without credentials, returning empty for greeter");
      ReplaceCString(username, "");
      ReplaceCString(password, "");
      ReplaceCString(domain, "");
      return;

    default:
      HiLogInfo("FillAuthentication: unknown reason=" + std::to_string(reason) + ", showing dialog");
      break;
  }

  std::unique_lock<std::mutex> lock(dialogMutex_);
  dialogAnswered_ = false;
  dialogUsername_.clear();
  dialogPassword_.clear();
  dialogDomain_.clear();

  {
    std::lock_guard<std::mutex> lockEvent(mutex_);
    pendingSessionEvents_.push_back(HarmonySessionEvent{
        HarmonySessionEventType::kAuthRequired, info_.stage, info_.lastError, info_.connected,
        frameSequence_, "请输入登录凭据", ""});
  }
  stateChanged_.notify_all();

  dialogCV_.wait(lock, [this]() { return dialogAnswered_ || stopRequested_; });

  if (dialogAnswered_) {
    HiLogInfo("FillAuthentication: user provided credentials, username=" + dialogUsername_);
    ReplaceCString(username, dialogUsername_);
    ReplaceCString(password, dialogPassword_);
    ReplaceCString(domain, dialogDomain_);
  } else {
    HiLogInfo("FillAuthentication: dialog cancelled or stop requested");
    ReplaceCString(username, "");
    ReplaceCString(password, "");
    ReplaceCString(domain, "");
  }
}

bool FreeRDPHarmonySession::VerifyCertificate(const char* host, std::uint16_t port,
                                              const char* commonName, const char* subject,
                                              const char* issuer, const char* fingerprint,
                                              std::uint32_t flags) {
  if (config_.ignoreCertificate) {
    return true;
  }

  std::unique_lock<std::mutex> lock(dialogMutex_);
  dialogAnswered_ = false;
  certAccepted_ = false;

  {
    std::lock_guard<std::mutex> lockEvent(mutex_);
    pendingSessionEvents_.push_back(HarmonySessionEvent{
        HarmonySessionEventType::kCertRequired, info_.stage, info_.lastError, info_.connected,
        frameSequence_, "需要校验新证书", "",
        host != nullptr ? host : "",
        subject != nullptr ? subject : "",
        issuer != nullptr ? issuer : "",
        fingerprint != nullptr ? fingerprint : ""});
  }
  stateChanged_.notify_all();

  dialogCV_.wait(lock, [this]() { return dialogAnswered_ || stopRequested_; });
  return dialogAnswered_ && certAccepted_;
}

bool FreeRDPHarmonySession::VerifyChangedCertificate(const char* host, std::uint16_t port,
                                                     const char* commonName, const char* subject,
                                                     const char* issuer, const char* newFingerprint,
                                                     const char* oldSubject, const char* oldIssuer,
                                                     const char* oldFingerprint, std::uint32_t flags) {
  if (config_.ignoreCertificate) {
    return true;
  }

  std::unique_lock<std::mutex> lock(dialogMutex_);
  dialogAnswered_ = false;
  certAccepted_ = false;

  {
    std::lock_guard<std::mutex> lockEvent(mutex_);
    pendingSessionEvents_.push_back(HarmonySessionEvent{
        HarmonySessionEventType::kCertRequired, info_.stage, info_.lastError, info_.connected,
        frameSequence_, "证书已更改，需要校验", "",
        host != nullptr ? host : "",
        subject != nullptr ? subject : "",
        issuer != nullptr ? issuer : "",
        newFingerprint != nullptr ? newFingerprint : ""});
  }
  stateChanged_.notify_all();

  dialogCV_.wait(lock, [this]() { return dialogAnswered_ || stopRequested_; });
  return dialogAnswered_ && certAccepted_;
}

void FreeRDPHarmonySession::SubmitAuth(const std::string& username, const std::string& password, const std::string& domain) {
  std::lock_guard<std::mutex> lock(dialogMutex_);
  dialogUsername_ = username;
  dialogPassword_ = password;
  dialogDomain_ = domain;
  dialogAnswered_ = true;
  dialogCV_.notify_all();
}

void FreeRDPHarmonySession::SubmitCert(bool accept) {
  std::lock_guard<std::mutex> lock(dialogMutex_);
  certAccepted_ = accept;
  dialogAnswered_ = true;
  dialogCV_.notify_all();
}

bool FreeRDPHarmonySession::SendMouseMove(std::uint16_t x, std::uint16_t y) {
  return QueueInputEvent(
      { InputEvent::Type::kMouse, PTR_FLAGS_MOVE, x, y, 0U, 0U });
}

bool FreeRDPHarmonySession::SendMouseButton(std::uint16_t x, std::uint16_t y,
                                            std::uint16_t buttonFlags, bool down) {
  const std::uint16_t flags =
      static_cast<std::uint16_t>(buttonFlags | (down ? PTR_FLAGS_DOWN : 0U));
  return QueueInputEvent({ InputEvent::Type::kMouse, flags, x, y, 0U, 0U });
}

bool FreeRDPHarmonySession::SendMouseWheel(std::uint16_t x, std::uint16_t y,
                                           std::int16_t delta) {
  std::uint16_t flags = PTR_FLAGS_WHEEL;
  std::uint16_t magnitude = static_cast<std::uint16_t>(std::min<std::int32_t>(
      WheelRotationMask, std::abs(static_cast<std::int32_t>(delta))));
  if (delta < 0) {
    flags |= PTR_FLAGS_WHEEL_NEGATIVE;
  }
  flags = static_cast<std::uint16_t>(flags | magnitude);
  return QueueInputEvent({ InputEvent::Type::kMouse, flags, x, y, 0U, 0U });
}

bool FreeRDPHarmonySession::SendKeyScancode(std::uint32_t scancode, bool down) {
  return QueueInputEvent({ InputEvent::Type::kScancodeKey,
                           static_cast<std::uint16_t>(down ? 1U : 0U), 0U, 0U, scancode, 0U });
}

bool FreeRDPHarmonySession::SendUnicodeKey(std::uint16_t codepoint, bool down) {
  const std::uint16_t flags = down ? 0U : KBD_FLAGS_RELEASE;
  return QueueInputEvent(
      { InputEvent::Type::kUnicodeKey, flags, 0U, 0U, 0U, codepoint });
}

void FreeRDPHarmonySession::SetSurfaceId(const std::string& surfaceIdStr) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (nativeWindow_ != nullptr) {
    OH_NativeWindow_DestroyNativeWindow(nativeWindow_);
    nativeWindow_ = nullptr;
  }
  if (!surfaceIdStr.empty()) {
    uint64_t surfaceId = 0;
    try {
      surfaceId = std::stoull(surfaceIdStr);
    } catch (...) { return; }
    OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &nativeWindow_);
    if (nativeWindow_ != nullptr) {
      const uint64_t usage = NATIVEBUFFER_USAGE_CPU_READ | NATIVEBUFFER_USAGE_CPU_WRITE |
                             NATIVEBUFFER_USAGE_MEM_DMA | NATIVEBUFFER_USAGE_HW_RENDER;
      const int32_t usageRet =
          OH_NativeWindow_NativeWindowHandleOpt(nativeWindow_, SET_USAGE, usage);
      const int32_t formatRet = OH_NativeWindow_NativeWindowHandleOpt(
          nativeWindow_, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_BGRA_8888);
      // We must configure the window's geometry to match the RDP desktop geometry before drawing.
      // FreeRDP starts and provides snapshot_.width and snapshot_.height in UpdateSnapshot.
      // Wait, we can only set geometry if it's > 0, but during SetSurfaceId it might be 0.
      if (snapshot_.width > 0 && snapshot_.height > 0) {
        const int32_t geometryRet = OH_NativeWindow_NativeWindowHandleOpt(
            nativeWindow_, SET_BUFFER_GEOMETRY, snapshot_.width, snapshot_.height);
        HiLogInfo("SetSurfaceId geometry ret=" + std::to_string(geometryRet) + " size=" +
                  std::to_string(snapshot_.width) + "x" + std::to_string(snapshot_.height));
      }
      HiLogInfo("SetSurfaceId configured NativeWindow successfully. usageRet=" +
                std::to_string(usageRet) + " formatRet=" + std::to_string(formatRet));
    } else {
      HiLogError("SetSurfaceId failed to create NativeWindow.");
    }
  }
}

void FreeRDPHarmonySession::UpdateFrame(std::uint32_t width, std::uint32_t height,
                                        std::uint32_t left, std::uint32_t top,
                                        std::uint32_t dirtyWidth, std::uint32_t dirtyHeight,
                                        const std::uint8_t* data, std::size_t size,
                                        std::uint32_t stride) {
  const std::size_t requiredSize =
      static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
  if ((data == nullptr) || (requiredSize == 0) || (size < requiredSize)) {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.width = width;
  snapshot_.height = height;
  snapshot_.dirtyLeft = left;
  snapshot_.dirtyTop = top;
  snapshot_.dirtyWidth = dirtyWidth;
  snapshot_.dirtyHeight = dirtyHeight;
  frameStride_ = stride;
  frameBuffer_.assign(data, data + requiredSize);
  frameSequence_ += 1;
  
  if (nativeWindow_ != nullptr) {
    OHNativeWindowBuffer* buffer = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nativeWindow_, &buffer, &fenceFd);
    if (ret == 0 && buffer != nullptr) {
      BufferHandle* handle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
      OH_NativeBuffer* nativeBuffer = nullptr;
      const int32_t convertRet = OH_NativeBuffer_FromNativeWindowBuffer(buffer, &nativeBuffer);
      void* mappedAddr = nullptr;
      const int32_t mapRet =
          (convertRet == 0 && nativeBuffer != nullptr) ? OH_NativeBuffer_Map(nativeBuffer, &mappedAddr) : -1;

      if (handle != nullptr && mappedAddr != nullptr) {
         uint8_t* dst = static_cast<uint8_t*>(mappedAddr);
         const uint8_t* src = data;

         const uint32_t copyWidth = std::min<uint32_t>(width, static_cast<uint32_t>(handle->width)) * 4;
         const uint32_t copyHeight = std::min<uint32_t>(height, static_cast<uint32_t>(handle->height));
         const uint32_t srcStride = stride;
         const uint32_t dstStride = static_cast<uint32_t>(handle->stride);
         const uint32_t rowBytes = std::min<uint32_t>(copyWidth, std::min<uint32_t>(srcStride, dstStride));

         for (uint32_t y = 0; y < copyHeight; ++y) {
           memcpy(dst + y * dstStride, src + y * srcStride, rowBytes);
         }

         OH_NativeBuffer_Unmap(nativeBuffer);
         if (fenceFd >= 0) {
           close(fenceFd);
           fenceFd = -1;
         }

         struct Region region;
         struct Region::Rect dirtyRect = { static_cast<int32_t>(left), static_cast<int32_t>(top),
                                           static_cast<uint32_t>(dirtyWidth), static_cast<uint32_t>(dirtyHeight) };
         region.rects = &dirtyRect;
         region.rectNumber = 1;

         const int32_t flushRet =
             OH_NativeWindow_NativeWindowFlushBuffer(nativeWindow_, buffer, -1, region);
         if (flushRet != 0) {
           HiLogError("UpdateFrame: FlushBuffer failed ret=" + std::to_string(flushRet));
         }
      } else {
         std::ostringstream stream;
         stream << "UpdateFrame: map failed requestRet=" << ret
                << " convertRet=" << convertRet
                << " mapRet=" << mapRet;
         if (handle != nullptr) {
           stream << " handle={w=" << handle->width
                  << ",h=" << handle->height
                  << ",stride=" << handle->stride
                  << ",size=" << handle->size
                  << ",format=" << handle->format
                  << ",usage=" << handle->usage
                  << ",virAddr=" << handle->virAddr
                  << "}";
         } else {
           stream << " handle=null";
         }
         HiLogError(stream.str());
         if (fenceFd >= 0) {
           close(fenceFd);
           fenceFd = -1;
         }
         OH_NativeWindow_NativeWindowAbortBuffer(nativeWindow_, buffer);
      }
    } else {
      HiLogError("UpdateFrame: NativeWindowRequestBuffer failed with ret=" + std::to_string(ret));
    }
  } else {
    HiLogError("UpdateFrame: nativeWindow_ is null!");
  }

  if (!hasPendingFrameEvent_) {
    hasPendingFrameEvent_ = true;
    pendingSessionEvents_.push_back(HarmonySessionEvent{
        HarmonySessionEventType::kFrameUpdated, info_.stage, info_.lastError, info_.connected,
        frameSequence_, "", "" });
  }
  stateChanged_.notify_all();
}

void FreeRDPHarmonySession::UpdateSnapshot(std::uint32_t width, std::uint32_t height,
                                           std::uint32_t left, std::uint32_t top,
                                           std::uint32_t dirtyWidth,
                                           std::uint32_t dirtyHeight) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (nativeWindow_ != nullptr && (snapshot_.width != width || snapshot_.height != height)) {
    const int32_t geometryRet =
        OH_NativeWindow_NativeWindowHandleOpt(nativeWindow_, SET_BUFFER_GEOMETRY, width, height);
    HiLogInfo("UpdateSnapshot geometry ret=" + std::to_string(geometryRet) + " size=" +
              std::to_string(width) + "x" + std::to_string(height));
  }
  
  snapshot_.width = width;
  snapshot_.height = height;
  snapshot_.dirtyLeft = left;
  snapshot_.dirtyTop = top;
  snapshot_.dirtyWidth = dirtyWidth;
  snapshot_.dirtyHeight = dirtyHeight;
  stateChanged_.notify_all();
}

void FreeRDPHarmonySession::UpdateStage(HarmonySessionStage stage, std::uint32_t lastError,
                                        const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  info_.stage = stage;
  info_.lastError = lastError;
  info_.connected = stage == HarmonySessionStage::kConnected;
  info_.message = message;
  pendingSessionEvents_.push_back(HarmonySessionEvent{
      HarmonySessionEventType::kStateChanged, info_.stage, info_.lastError, info_.connected,
      frameSequence_, message, "" });
  stateChanged_.notify_all();
}

bool FreeRDPHarmonySession::QueueInputEvent(const InputEvent& event) {
  HANDLE inputEventHandle = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool contextReady = (context_ != nullptr);
    if (!contextReady || !started_ || stopRequested_) {
      return false;
    }

    pendingInputEvents_.push_back(event);
    inputEventHandle = static_cast<HANDLE>(inputEventHandle_);
  }

  if (inputEventHandle == nullptr) {
    return false;
  }

  return SetEvent(inputEventHandle) == TRUE;
}

bool FreeRDPHarmonySession::ProcessInputEvents(void* inputPointer) {
  HANDLE inputEventHandle = static_cast<HANDLE>(inputEventHandle_);
  if (inputEventHandle == nullptr) {
    return false;
  }

  if (WaitForSingleObject(inputEventHandle, 0) != WAIT_OBJECT_0) {
    return true;
  }

  if (!ResetEvent(inputEventHandle)) {
    return false;
  }

  rdpInput* input = static_cast<rdpInput*>(inputPointer);
  if (input == nullptr) {
    return false;
  }

  while (true) {
    InputEvent event = {};
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (pendingInputEvents_.empty()) {
        break;
      }

      event = pendingInputEvents_.front();
      pendingInputEvents_.pop_front();
    }

    BOOL success = FALSE;
    switch (event.type) {
      case InputEvent::Type::kMouse:
        success = freerdp_input_send_mouse_event(input, event.flags, event.x, event.y);
        break;
      case InputEvent::Type::kScancodeKey:
        success = freerdp_input_send_keyboard_event_ex(
            input, event.flags != 0 ? TRUE : FALSE, FALSE, event.scancode);
        break;
      case InputEvent::Type::kUnicodeKey:
        success = freerdp_input_send_unicode_keyboard_event(input, event.flags, event.codepoint);
        break;
    }

    if (!success) {
      return false;
    }
  }

  return true;
}

bool FreeRDPHarmonySession::EnsureContext() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (context_ != nullptr) {
    return true;
  }

  if (inputEventHandle_ == nullptr) {
    info_.stage = HarmonySessionStage::kFailed;
    info_.lastError = 1;
    info_.connected = false;
    info_.message = "创建输入事件队列失败";
    return false;
  }

  if (!config_.appDataDir.empty()) {
    if (setenv("HOME", config_.appDataDir.c_str(), 1) != 0) {
      std::ostringstream stream;
      stream << "设置 HOME 失败 errno=" << errno;
      info_.stage = HarmonySessionStage::kFailed;
      info_.lastError = 1;
      info_.connected = false;
      info_.message = stream.str();
      return false;
    }
  }

  RDP_CLIENT_ENTRY_POINTS entryPoints = {};
  if (HarmonyRdpClientEntry(&entryPoints) != 0) {
    info_.stage = HarmonySessionStage::kFailed;
    info_.lastError = 1;
    info_.connected = false;
    info_.message = "初始化 Harmony FreeRDP 客户端失败";
    return false;
  }

  rdpContext* context = freerdp_client_context_new(&entryPoints);
  if (context == nullptr) {
    info_.stage = HarmonySessionStage::kFailed;
    info_.lastError = 1;
    info_.connected = false;
    info_.message = "创建 FreeRDP 上下文失败";
    return false;
  }

  HarmonyClientContext* harmony = reinterpret_cast<HarmonyClientContext*>(context);
  harmony->session = this;

  std::string applyError;
  if (!ApplySettings(context->settings, applyError)) {
    freerdp_client_context_free(context);
    info_.stage = HarmonySessionStage::kFailed;
    info_.lastError = 1;
    info_.connected = false;
    info_.message = "应用 RDP 参数失败: " + applyError;
    return false;
  }

  context_ = context;
  return true;
}

bool FreeRDPHarmonySession::ApplySettings(void* settingsPointer, std::string& outError) {
  if (settingsPointer == nullptr) {
    outError = "settingsPointer is null";
    return false;
  }
  if (config_.host.empty()) {
    outError = "host is empty";
    return false;
  }

  rdpSettings* settings = static_cast<rdpSettings*>(settingsPointer);
  std::vector<std::string> args = BuildCommandLineArgs();
  std::vector<char*> argv;
  argv.reserve(args.size());
  std::string allArgs;
  for (const std::string& arg : args) {
    char* duplicated = _strdup(arg.c_str());
    if (duplicated == nullptr) {
      outError = "duplicate argument failed";
      for (char* allocated : argv) {
        std::free(allocated);
      }
      return false;
    }
    argv.push_back(duplicated);
    allArgs += arg + " ";
  }

  const DWORD status = freerdp_client_settings_parse_command_line(
      settings, static_cast<int>(argv.size()), argv.data(), FALSE);
  for (char* allocated : argv) {
    std::free(allocated);
  }

  if (status != 0) {
    WLog_ERR(TAG, "freerdp_client_settings_parse_command_line failed status=%" PRIu32
                      " args=%s",
             status, allArgs.c_str());
    HiLogError("freerdp_client_settings_parse_command_line failed status=" +
               std::to_string(status) + " args=" + allArgs);
    outError = "parse cmdline failed: " + allArgs;
    return false;
  }

  WLog_INFO(TAG, "ApplySettings args=%s", allArgs.c_str());
  HiLogInfo("ApplySettings args=" + allArgs);

  const std::uint32_t desktopWidth = config_.desktopWidth > 0 ? config_.desktopWidth : 1280U;
  const std::uint32_t desktopHeight = config_.desktopHeight > 0 ? config_.desktopHeight : 720U;

  if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, desktopWidth)) {
    outError = "set FreeRDP_DesktopWidth failed";
    return false;
  }
  if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, desktopHeight)) {
    outError = "set FreeRDP_DesktopHeight failed";
    return false;
  }
  if (!freerdp_settings_set_bool(settings, FreeRDP_DesktopResize, TRUE)) {
    outError = "set FreeRDP_DesktopResize failed";
    return false;
  }
  if (!freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, TRUE)) {
    outError = "set FreeRDP_SoftwareGdi failed";
    return false;
  }

  return true;
}

std::vector<std::string> FreeRDPHarmonySession::BuildCommandLineArgs() const {
  std::vector<std::string> args;
  const bool hasCredentials = !config_.username.empty() && !config_.password.empty();
  HiLogInfo("BuildCommandLineArgs: hasCredentials=" + std::string(hasCredentials ? "true" : "false") +
            " configMode=" + config_.securityMode);
  args.emplace_back("freerdp-harmony");
  args.emplace_back("/gdi:sw");
  args.emplace_back("/v:" + config_.host);
  if (config_.port > 0U) {
    args.emplace_back("/port:" + std::to_string(config_.port));
  }
  if (!config_.clientHostname.empty()) {
    args.emplace_back("/client-hostname:" + config_.clientHostname);
  }

  if (hasCredentials) {
    args.emplace_back("/u:" + config_.username);
    args.emplace_back("/p:" + config_.password);
    if (!config_.domain.empty()) {
      args.emplace_back("/d:" + config_.domain);
    }
    const std::string secMode = ToLower(config_.securityMode);
    if (!secMode.empty() && secMode != "auto") {
      args.emplace_back("/sec:" + secMode);
    }
  } else {
    args.emplace_back("/p");
    args.emplace_back("/sec:tls,rdp");
  }
  if (!config_.gateway.empty()) {
    std::string gatewayArg = "/gateway:g:" + config_.gateway;
    if (!config_.gatewayUsername.empty()) {
      gatewayArg += ",u:" + config_.gatewayUsername;
    }
    if (!config_.gatewayDomain.empty()) {
      gatewayArg += ",d:" + config_.gatewayDomain;
    }
    if (!config_.gatewayPassword.empty()) {
      gatewayArg += ",p:" + config_.gatewayPassword;
    }
    args.emplace_back(gatewayArg);
  }

  const std::uint32_t desktopWidth = config_.desktopWidth > 0 ? config_.desktopWidth : 1280U;
  const std::uint32_t desktopHeight = config_.desktopHeight > 0 ? config_.desktopHeight : 720U;
  args.emplace_back("/size:" + std::to_string(desktopWidth) + "x" +
                    std::to_string(desktopHeight));
  args.emplace_back("/bpp:32");
  args.emplace_back("/gfx");
  args.emplace_back("/network:auto");
  args.emplace_back("/dynamic-resolution");
  args.emplace_back("/log-level:TRACE");
  args.emplace_back(config_.enableClipboard ? "/clipboard" : "-clipboard");

  if (config_.enableAudio) {
    args.emplace_back("/audio-mode:0");
    args.emplace_back("/sound");
  } else {
    args.emplace_back("/audio-mode:2");
  }

  if (config_.enableDrive) {
    args.emplace_back("/home-drive");
  }

  args.emplace_back("/kbd:unicode:on");
  if (config_.ignoreCertificate) {
    args.emplace_back("/cert:ignore");
  }

  return args;
}

void FreeRDPHarmonySession::ThreadMain() {
  rdpContext* context = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    context = static_cast<rdpContext*>(context_);
  }

  if ((context == nullptr) || (context->instance == nullptr)) {
    UpdateStage(HarmonySessionStage::kFailed, 1, "RDP 上下文不可用");
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
    stateChanged_.notify_all();
    return;
  }

  freerdp* instance = context->instance;
  std::array<HANDLE, MAXIMUM_WAIT_OBJECTS> handles = {};
  HiLogInfo("connect begin host=" + config_.host + " port=" + std::to_string(config_.port) +
            " security=" + config_.securityMode + " gateway=" + config_.gateway);

  if (freerdp_client_start(context) != 0) {
    UpdateStage(HarmonySessionStage::kFailed, 1, "启动 FreeRDP 客户端失败");
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
    stateChanged_.notify_all();
    return;
  }

  const bool supportSkipChannelJoin =
      freerdp_settings_get_bool(context->settings, FreeRDP_SupportSkipChannelJoin);
  WLog_INFO(TAG, "before connect: FreeRDP_SupportSkipChannelJoin=%s",
            supportSkipChannelJoin ? "true" : "false");

  if (!freerdp_connect(instance)) {
    const std::uint32_t lastError = freerdp_get_last_error(context);
    const char* lastErrorString = freerdp_get_last_error_string(lastError);
    WLog_ERR(TAG,
             "freerdp_connect failed lastError=0x%08" PRIX32
             " lastErrorString=%s host=%s port=%" PRIu16,
             lastError, lastErrorString != nullptr ? lastErrorString : "", config_.host.c_str(),
             config_.port);
    std::ostringstream stream;
    stream << "freerdp_connect failed lastError=0x" << std::hex << std::uppercase << lastError
           << " lastErrorString="
           << (lastErrorString != nullptr ? lastErrorString : "")
           << " host=" << config_.host << " port=" << std::dec << config_.port;
    HiLogError(stream.str());
    bool stopRequested = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopRequested = stopRequested_;
    }

    UpdateStage(stopRequested ? HarmonySessionStage::kDisconnected : HarmonySessionStage::kFailed,
                lastError,
                stopRequested ? "连接已取消"
                              : BuildErrorMessage("连接远程桌面失败", lastError));

    const bool supportSkipChannelJoinAfterFail =
        freerdp_settings_get_bool(context->settings, FreeRDP_SupportSkipChannelJoin);
    WLog_INFO(TAG, "connect failed: FreeRDP_SupportSkipChannelJoin=%s",
              supportSkipChannelJoinAfterFail ? "true" : "false");
    freerdp_client_stop(context);

    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
    stateChanged_.notify_all();
    return;
  }

  while (true) {
    bool stopRequested = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopRequested = stopRequested_;
    }

    if (stopRequested || freerdp_shall_disconnect_context(context)) {
      break;
    }

    DWORD count = 0;
    HANDLE inputEventHandle = static_cast<HANDLE>(inputEventHandle_);
    if (inputEventHandle != nullptr) {
      handles[count++] = inputEventHandle;
    }

    const DWORD remoteHandleCount = freerdp_get_event_handles(
        context, &handles[count], static_cast<DWORD>(handles.size() - count));
    if (remoteHandleCount == 0) {
      const std::uint32_t lastError = freerdp_get_last_error(context);
      UpdateStage(HarmonySessionStage::kFailed, lastError,
                  BuildErrorMessage("获取远端事件句柄失败", lastError));
      break;
    }
    count += remoteHandleCount;

    const DWORD status = WaitForMultipleObjects(count, handles.data(), FALSE, INFINITE);

    if (status == WAIT_FAILED) {
      const std::uint32_t lastError = freerdp_get_last_error(context);
      UpdateStage(HarmonySessionStage::kFailed, lastError,
                  BuildErrorMessage("等待远端事件失败", lastError));
      break;
    }

    if (!freerdp_check_event_handles(context)) {
      bool stopping = false;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping = stopRequested_;
      }

      if (!stopping) {
        const std::uint32_t lastError = freerdp_get_last_error(context);
        UpdateStage(HarmonySessionStage::kFailed, lastError,
                    BuildErrorMessage("处理远端事件失败", lastError));
      }
      break;
    }

    if (!ProcessInputEvents(context->input)) {
      bool stopping = false;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping = stopRequested_;
      }

      if (!stopping) {
        const std::uint32_t lastError = freerdp_get_last_error(context);
        UpdateStage(HarmonySessionStage::kFailed, lastError,
                    BuildErrorMessage("处理本地输入事件失败", lastError));
      }
      break;
    }
  }

  freerdp_disconnect(instance);
  freerdp_client_stop(context);

  {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
  }
  stateChanged_.notify_all();
}
