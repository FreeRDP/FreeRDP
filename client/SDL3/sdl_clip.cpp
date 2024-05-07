/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client keyboard helper
 *
 * Copyright 2024 Armin Novak <armin.novak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <mutex>
#include <iterator>
#include <algorithm>

#include <winpr/wlog.h>

#include "sdl_clip.hpp"
#include "sdl_freerdp.hpp"

#define TAG CLIENT_TAG("sdl.cliprdr")

#define mime_text_plain "text/plain"
#define mime_text_utf8 mime_text_plain ";charset=utf-8"

static const std::vector<const char*> mime_text = { mime_text_plain, mime_text_utf8, "UTF8_STRING",
	                                                "COMPOUND_TEXT", "TEXT",         "STRING" };

static const char mime_png[] = "image/png";
static const char mime_webp[] = "image/webp";
static const char mime_jpg[] = "image/jpeg";
static const char mime_tiff[] = "image/tiff";
static const char mime_uri_list[] = "text/uri-list";
static const char mime_html[] = "text/html";

#define BMP_MIME_LIST "image/bmp", "image/x-bmp", "image/x-MS-bmp", "image/x-win-bitmap"
static const std::vector<const char*> mime_bitmap = { BMP_MIME_LIST };
static const std::vector<const char*> mime_image = { mime_png, mime_webp, mime_jpg, mime_tiff,
	                                                 BMP_MIME_LIST };

static const char mime_gnome_copied_files[] = "x-special/gnome-copied-files";
static const char mime_mate_copied_files[] = "x-special/mate-copied-files";

static const char* type_HtmlFormat = "HTML Format";
static const char* type_FileGroupDescriptorW = "FileGroupDescriptorW";

class ClipboardLockGuard
{
  public:
	ClipboardLockGuard(wClipboard* clipboard) : _clipboard(clipboard)
	{
		ClipboardLock(_clipboard);
	}
	ClipboardLockGuard(const ClipboardLockGuard& other) = delete;
	ClipboardLockGuard(ClipboardLockGuard&& other) = delete;

	ClipboardLockGuard& operator=(const ClipboardLockGuard& rhs) = delete;
	ClipboardLockGuard& operator=(ClipboardLockGuard&& rhs) = delete;

	~ClipboardLockGuard()
	{
		ClipboardUnlock(_clipboard);
	}

  private:
	wClipboard* _clipboard;
};

static bool operator<(const CLIPRDR_FORMAT& lhs, const CLIPRDR_FORMAT& rhs)
{
	return (lhs.formatId < rhs.formatId);
}
static bool operator>(const CLIPRDR_FORMAT& lhs, const CLIPRDR_FORMAT& rhs)
{
	return (lhs.formatId > rhs.formatId);
}
static bool operator==(const CLIPRDR_FORMAT& lhs, const CLIPRDR_FORMAT& rhs)
{
	return (lhs.formatId == rhs.formatId);
}

sdlClip::sdlClip(SdlContext* sdl)
    : _sdl(sdl), _file(cliprdr_file_context_new(this)), _log(WLog_Get(TAG)),
      _system(ClipboardCreate()), _event(CreateEventA(nullptr, TRUE, FALSE, nullptr))
{
	WINPR_ASSERT(sdl);
}

sdlClip::~sdlClip()
{
	cliprdr_file_context_free(_file);
	ClipboardDestroy(_system);
	CloseHandle(_event);
}

BOOL sdlClip::init(CliprdrClientContext* clip)
{
	WINPR_ASSERT(clip);
	_ctx = clip;
	clip->custom = this;
	_ctx->MonitorReady = sdlClip::MonitorReady;
	_ctx->ServerCapabilities = sdlClip::ReceiveServerCapabilities;
	_ctx->ServerFormatList = sdlClip::ReceiveServerFormatList;
	_ctx->ServerFormatListResponse = sdlClip::ReceiveFormatListResponse;
	_ctx->ServerFormatDataRequest = sdlClip::ReceiveFormatDataRequest;
	_ctx->ServerFormatDataResponse = sdlClip::ReceiveFormatDataResponse;

	return cliprdr_file_context_init(_file, _ctx);
}

BOOL sdlClip::uninit(CliprdrClientContext* clip)
{
	WINPR_ASSERT(clip);
	if (!cliprdr_file_context_uninit(_file, _ctx))
		return FALSE;
	_ctx = nullptr;
	clip->custom = nullptr;
	return TRUE;
}

bool sdlClip::handle_update()
{
	if (!_ctx || !_sync)
		return true;

	clearServerFormats();

	std::string mime_uri_list = "text/uri-list";
	std::string mime_html = "text/html";

	std::vector<std::string> mime_bitmap = { "image/bmp", "image/x-bmp", "image/x-MS-bmp",
		                                     "image/x-win-bitmap" };
	std::string mime_webp = "image/webp";
	std::string mime_png = "image/png";
	std::string mime_jpeg = "image/jpeg";
	std::string mime_tiff = "image/tiff";
	std::vector<std::string> mime_images = { mime_webp, mime_png, mime_jpeg, mime_tiff };

	std::vector<std::string> clientFormatNames;
	std::vector<CLIPRDR_FORMAT> clientFormats;
	if (SDL_HasClipboardText())
	{
		clientFormats.push_back({ CF_TEXT, nullptr });
		clientFormats.push_back({ CF_OEMTEXT, nullptr });
		clientFormats.push_back({ CF_UNICODETEXT, nullptr });
	}
	if (SDL_HasClipboardData(mime_html.c_str()))
		clientFormatNames.push_back(type_HtmlFormat);

	for (auto& mime : mime_bitmap)
	{
		if (SDL_HasClipboardData(mime.c_str()))
		{
			clientFormats.push_back({ CF_DIB, nullptr });
			clientFormats.push_back({ CF_DIBV5, nullptr });

			for (auto& bmp : mime_bitmap)
				clientFormatNames.push_back(bmp);

			for (auto& img : mime_images)
				clientFormatNames.push_back(img);

			break;
		}
	}

	for (auto& img : mime_images)
	{
		if (SDL_HasClipboardData(img.c_str()))
			clientFormatNames.push_back(img);
	}

	for (auto& name : clientFormatNames)
	{
		clientFormats.push_back({ ClipboardRegisterFormat(_system, name.c_str()), name.data() });
	}

	std::sort(clientFormats.begin(), clientFormats.end(),
	          [](const auto& a, const auto& b) { return a < b; });
	auto u = std::unique(clientFormats.begin(), clientFormats.end());
	clientFormats.erase(u, clientFormats.end());

	const CLIPRDR_FORMAT_LIST formatList = {
		.common = { .msgType = CB_FORMAT_LIST, .msgFlags = 0 },
		.numFormats = static_cast<UINT32>(clientFormats.size()),
		.formats = clientFormats.data(),
	};

	WLog_Print(_log, WLOG_TRACE,
	           "-------------- client format list [%" PRIu32 "] ------------------",
	           formatList.numFormats);
	for (UINT32 x = 0; x < formatList.numFormats; x++)
	{
		auto format = &formatList.formats[x];
		WLog_Print(_log, WLOG_TRACE, "client announces %" PRIu32 " [%s][%s]", format->formatId,
		           ClipboardGetFormatIdString(format->formatId), format->formatName);
	}

	WINPR_ASSERT(_ctx);
	WINPR_ASSERT(_ctx->ClientFormatList);
	return _ctx->ClientFormatList(_ctx, &formatList) == CHANNEL_RC_OK;
}

UINT sdlClip::MonitorReady(CliprdrClientContext* context, const CLIPRDR_MONITOR_READY* monitorReady)
{
	WINPR_UNUSED(monitorReady);
	WINPR_ASSERT(context);
	WINPR_ASSERT(monitorReady);

	auto clipboard = static_cast<sdlClip*>(
	    cliprdr_file_context_get_context(static_cast<CliprdrFileContext*>(context->custom)));
	WINPR_ASSERT(clipboard);

	auto ret = clipboard->SendClientCapabilities();
	if (ret != CHANNEL_RC_OK)
		return ret;

	clipboard->_sync = true;
	if (!clipboard->handle_update())
		return ERROR_INTERNAL_ERROR;

	return CHANNEL_RC_OK;
}

UINT sdlClip::SendClientCapabilities()
{
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = {
		.capabilitySetType = CB_CAPSTYPE_GENERAL,
		.capabilitySetLength = 12,
		.version = CB_CAPS_VERSION_2,
		.generalFlags = CB_USE_LONG_FORMAT_NAMES | cliprdr_file_context_current_flags(_file)
	};
	CLIPRDR_CAPABILITIES capabilities = {
		.cCapabilitiesSets = 1,
		.capabilitySets = reinterpret_cast<CLIPRDR_CAPABILITY_SET*>(&generalCapabilitySet)
	};

	WINPR_ASSERT(_ctx);
	WINPR_ASSERT(_ctx->ClientCapabilities);
	return _ctx->ClientCapabilities(_ctx, &capabilities);
}

void sdlClip::clearServerFormats()
{
	_serverFormats.clear();
	cliprdr_file_context_clear(_file);
}

UINT sdlClip::SendFormatListResponse(BOOL status)
{
	const CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse = {
		.common = { .msgType = CB_FORMAT_LIST_RESPONSE,
		            .msgFlags = static_cast<UINT16>(status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL),
		            .dataLen = 0 }
	};
	WINPR_ASSERT(_ctx);
	WINPR_ASSERT(_ctx->ClientFormatListResponse);
	return _ctx->ClientFormatListResponse(_ctx, &formatListResponse);
}

UINT sdlClip::SendDataResponse(const BYTE* data, size_t size)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response = {};

	if (size > UINT32_MAX)
		return ERROR_INVALID_PARAMETER;

	response.common.msgFlags = (data) ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	response.common.dataLen = static_cast<UINT32>(size);
	response.requestedFormatData = data;

	WINPR_ASSERT(_ctx);
	WINPR_ASSERT(_ctx->ClientFormatDataResponse);
	return _ctx->ClientFormatDataResponse(_ctx, &response);
}

std::string sdlClip::getServerFormat(uint32_t id)
{
	for (auto& fmt : _serverFormats)
	{
		if (fmt.formatId() == id)
		{
			if (fmt.formatName())
				return fmt.formatName();
			break;
		}
	}

	return "";
}

uint32_t sdlClip::serverIdForMime(const std::string& mime)
{
	std::string cmp = mime;
	if (mime_is_html(mime))
		cmp = type_HtmlFormat;
	if (mime_is_file(mime))
		cmp = type_FileGroupDescriptorW;

	for (auto& format : _serverFormats)
	{
		if (!format.formatName())
			continue;
		if (cmp.compare(format.formatName()) == 0)
			return format.formatId();
	}

	if (mime_is_image(mime))
		return CF_DIB;
	if (mime_is_text(mime))
		return CF_UNICODETEXT;

	return 0;
}

UINT sdlClip::ReceiveServerCapabilities(CliprdrClientContext* context,
                                        const CLIPRDR_CAPABILITIES* capabilities)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(capabilities);

	auto capsPtr = reinterpret_cast<const BYTE*>(capabilities->capabilitySets);
	WINPR_ASSERT(capsPtr);

	auto clipboard = static_cast<sdlClip*>(
	    cliprdr_file_context_get_context(static_cast<CliprdrFileContext*>(context->custom)));
	WINPR_ASSERT(clipboard);

	if (!cliprdr_file_context_remote_set_flags(clipboard->_file, 0))
		return ERROR_INTERNAL_ERROR;

	for (UINT32 i = 0; i < capabilities->cCapabilitiesSets; i++)
	{
		auto caps = reinterpret_cast<const CLIPRDR_CAPABILITY_SET*>(capsPtr);

		if (caps->capabilitySetType == CB_CAPSTYPE_GENERAL)
		{
			auto generalCaps = reinterpret_cast<const CLIPRDR_GENERAL_CAPABILITY_SET*>(caps);

			if (!cliprdr_file_context_remote_set_flags(clipboard->_file, generalCaps->generalFlags))
				return ERROR_INTERNAL_ERROR;
		}

		capsPtr += caps->capabilitySetLength;
	}

	return CHANNEL_RC_OK;
}

UINT sdlClip::ReceiveServerFormatList(CliprdrClientContext* context,
                                      const CLIPRDR_FORMAT_LIST* formatList)
{
	BOOL html = FALSE;
	BOOL text = FALSE;
	BOOL image = FALSE;
	BOOL file = FALSE;

	if (!context || !context->custom)
		return ERROR_INVALID_PARAMETER;

	auto clipboard = static_cast<sdlClip*>(
	    cliprdr_file_context_get_context(static_cast<CliprdrFileContext*>(context->custom)));
	WINPR_ASSERT(clipboard);

	clipboard->clearServerFormats();

	for (UINT32 i = 0; i < formatList->numFormats; i++)
	{
		const CLIPRDR_FORMAT* format = &formatList->formats[i];

		clipboard->_serverFormats.push_back({ format->formatId, format->formatName });

		if (format->formatName)
		{
			if (strcmp(format->formatName, type_HtmlFormat) == 0)
			{
				text = TRUE;
				html = TRUE;
			}
			else if (strcmp(format->formatName, type_FileGroupDescriptorW) == 0)
			{
				file = TRUE;
				text = TRUE;
			}
		}
		else
		{
			switch (format->formatId)
			{
				case CF_TEXT:
				case CF_OEMTEXT:
				case CF_UNICODETEXT:
					text = TRUE;
					break;

				case CF_DIB:
					image = TRUE;
					break;

				default:
					break;
			}
		}
	}

	std::vector<const char*> mimetypes;
	if (text)
	{
		mimetypes.insert(mimetypes.end(), mime_text.begin(), mime_text.end());
	}
	if (image)
	{
		mimetypes.insert(mimetypes.end(), mime_bitmap.begin(), mime_bitmap.end());
		mimetypes.insert(mimetypes.end(), mime_image.begin(), mime_image.end());
	}
	if (html)
	{
		mimetypes.push_back(mime_html);
	}
	if (file)
	{
		mimetypes.push_back(mime_uri_list);
		mimetypes.push_back(mime_gnome_copied_files);
		mimetypes.push_back(mime_mate_copied_files);
	}

	const int rc = SDL_SetClipboardData(sdlClip::ClipDataCb, sdlClip::ClipCleanCb, clipboard,
	                                    mimetypes.data(), mimetypes.size());
	return clipboard->SendFormatListResponse(rc == 0);
}

UINT sdlClip::ReceiveFormatListResponse(CliprdrClientContext* context,
                                        const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatListResponse);

	if (formatListResponse->common.msgFlags & CB_RESPONSE_FAIL)
		WLog_WARN(TAG, "format list update failed");
	return CHANNEL_RC_OK;
}

std::shared_ptr<BYTE> sdlClip::ReceiveFormatDataRequestHandle(
    sdlClip* clipboard, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest, uint32_t& len)
{
	const char* mime = nullptr;
	UINT32 formatId = 0;

	BOOL res = FALSE;

	std::shared_ptr<BYTE> data;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(formatDataRequest);

	len = 0;
	auto localFormatId = formatId = formatDataRequest->requestedFormatId;

	ClipboardLockGuard(clipboard->_system);
	std::lock_guard<CriticalSection> lock(clipboard->_lock);

	const UINT32 fileFormatId = ClipboardGetFormatId(clipboard->_system, type_FileGroupDescriptorW);
	const UINT32 htmlFormatId = ClipboardGetFormatId(clipboard->_system, type_HtmlFormat);

	switch (formatId)
	{
		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			localFormatId = ClipboardGetFormatId(clipboard->_system, mime_text_plain);
			mime = mime_text_utf8;
			break;

		case CF_DIB:
		case CF_DIBV5:
			mime = mime_bitmap[0];
			break;

		case CF_TIFF:
			mime = mime_tiff;
			break;

		default:
			if (formatId == fileFormatId)
			{
				localFormatId = ClipboardGetFormatId(clipboard->_system, mime_uri_list);
				mime = mime_uri_list;
			}
			else if (formatId == htmlFormatId)
			{
				localFormatId = ClipboardGetFormatId(clipboard->_system, mime_html);
				mime = mime_html;
			}
			else
				return data;
	}

	{
		size_t size = 0;
		auto sdldata = std::shared_ptr<void>(SDL_GetClipboardData(mime, &size), SDL_free);
		if (!sdldata)
			return data;

		if (fileFormatId == formatId)
		{
			auto bdata = static_cast<const char*>(sdldata.get());
			if (!cliprdr_file_context_update_client_data(clipboard->_file, bdata, size))
				return data;
		}

		res = ClipboardSetData(clipboard->_system, localFormatId, sdldata.get(),
		                       static_cast<uint32_t>(size));
	}

	if (!res)
		return data;

	uint32_t ptrlen = 0;
	auto ptr = static_cast<BYTE*>(ClipboardGetData(clipboard->_system, formatId, &ptrlen));
	data = std::shared_ptr<BYTE>(ptr, free);

	if (!data)
		return data;

	if (fileFormatId == formatId)
	{
		BYTE* ddata = nullptr;
		UINT32 dsize = 0;
		const UINT32 flags = cliprdr_file_context_remote_get_flags(clipboard->_file);
		const UINT32 error = cliprdr_serialize_file_list_ex(
		    flags, reinterpret_cast<const FILEDESCRIPTORW*>(data.get()),
		    ptrlen / sizeof(FILEDESCRIPTORW), &ddata, &dsize);
		data.reset();
		auto tmp = std::shared_ptr<BYTE>(ddata, free);
		if (error)
			return data;

		data = tmp;
		len = dsize;
	}
	else
		len = ptrlen;
	return data;
}

UINT sdlClip::ReceiveFormatDataRequest(CliprdrClientContext* context,
                                       const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	auto clipboard = static_cast<sdlClip*>(
	    cliprdr_file_context_get_context(static_cast<CliprdrFileContext*>(context->custom)));
	WINPR_ASSERT(clipboard);

	uint32_t len = 0;
	auto rc = ReceiveFormatDataRequestHandle(clipboard, formatDataRequest, len);
	return clipboard->SendDataResponse(rc.get(), len);
}

UINT sdlClip::ReceiveFormatDataResponse(CliprdrClientContext* context,
                                        const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	const UINT32 size = formatDataResponse->common.dataLen;
	const BYTE* data = formatDataResponse->requestedFormatData;

	auto clipboard = static_cast<sdlClip*>(
	    cliprdr_file_context_get_context(static_cast<CliprdrFileContext*>(context->custom)));
	WINPR_ASSERT(clipboard);

	ClipboardLockGuard(clipboard->_system);
	std::lock_guard<CriticalSection> lock(clipboard->_lock);
	if (clipboard->_request_queue.empty())
		return ERROR_INTERNAL_ERROR;

	auto request = clipboard->_request_queue.front();
	if (formatDataResponse->common.msgFlags & CB_RESPONSE_FAIL)
	{
		WLog_WARN(TAG, "clipboard data request for format %" PRIu32 " [%s], mime %s failed",
		          request.format(), request.formatstr().c_str(), request.mime().c_str());
		return ERROR_INTERNAL_ERROR;
	}

	deleted_unique_ptr<void> cdata;
	UINT32 srcFormatId = 0;
	switch (request.format())
	{
		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			srcFormatId = request.format();
			break;

		case CF_DIB:
		case CF_DIBV5:
			srcFormatId = request.format();
			break;

		default:
		{

			auto name = clipboard->getServerFormat(request.format());
			if (!name.empty())
			{
				if (name.compare(type_FileGroupDescriptorW) == 0)
				{
					srcFormatId =
					    ClipboardGetFormatId(clipboard->_system, type_FileGroupDescriptorW);

					if (!cliprdr_file_context_update_server_data(clipboard->_file,
					                                             clipboard->_system, data, size))
						return ERROR_INTERNAL_ERROR;
				}
				else if (name.compare(type_HtmlFormat) == 0)
				{
					srcFormatId = ClipboardGetFormatId(clipboard->_system, type_HtmlFormat);
				}
			}
		}
		break;
	}

	const BOOL sres = ClipboardSetData(clipboard->_system, srcFormatId, data, size);
	if (!sres)
		return ERROR_INTERNAL_ERROR;

	SetEvent(clipboard->_event);
	return CHANNEL_RC_OK;
}

const void* sdlClip::ClipDataCb(void* userdata, const char* mime_type, size_t* size)
{
	auto clip = static_cast<sdlClip*>(userdata);
	WINPR_ASSERT(clip);
	WINPR_ASSERT(size);
	WINPR_ASSERT(mime_type);

	*size = 0;
	uint32_t len = 0;

	if (mime_is_text(mime_type))
		mime_type = "text/plain";

	{
		ClipboardLockGuard(clip->_system);
		std::lock_guard<CriticalSection> lock(clip->_lock);
		auto cache = clip->_cache_data.find(mime_type);
		if (cache != clip->_cache_data.end())
		{
			*size = cache->second.size;
			return cache->second.ptr.get();
		}

		uint32_t formatID = clip->serverIdForMime(mime_type);
		if (clip->SendDataRequest(formatID, mime_type))
			return nullptr;
	}

	{
		HANDLE hdl[2] = { clip->_event, freerdp_abort_event(clip->_sdl->context()) };
		auto status = WaitForMultipleObjects(ARRAYSIZE(hdl), hdl, FALSE, INFINITE);
		if (status != WAIT_OBJECT_0)
			return nullptr;
	}
	{
		ClipboardLockGuard(clip->_system);
		std::lock_guard<CriticalSection> lock(clip->_lock);
		auto request = clip->_request_queue.front();
		clip->_request_queue.pop();
		ResetEvent(clip->_event);

		auto formatID = ClipboardRegisterFormat(clip->_system, mime_type);
		auto data = ClipboardGetData(clip->_system, formatID, &len);
		if (!data)
			return nullptr;

		auto ptr = std::shared_ptr<void>(data, free);
		clip->_cache_data.insert({ mime_type, { len, ptr } });
		*size = len;
		return ptr.get();
	}
}

void sdlClip::ClipCleanCb(void* userdata)
{
	auto clip = static_cast<sdlClip*>(userdata);
	WINPR_ASSERT(clip);
	ClipboardLockGuard(clip->_system);
	std::lock_guard<CriticalSection> lock(clip->_lock);
	ClipboardEmpty(clip->_system);
	clip->_cache_data.clear();
}

bool sdlClip::mime_is_file(const std::string& mime)
{
	if (strncmp(mime_uri_list, mime.c_str(), sizeof(mime_uri_list)) == 0)
		return true;
	if (strncmp(mime_gnome_copied_files, mime.c_str(), sizeof(mime_gnome_copied_files)) == 0)
		return true;
	if (strncmp(mime_mate_copied_files, mime.c_str(), sizeof(mime_mate_copied_files)) == 0)
		return true;
	return false;
}

bool sdlClip::mime_is_text(const std::string& mime)
{
	for (size_t x = 0; x < ARRAYSIZE(mime_text); x++)
	{
		if (mime.compare(mime_text[x]) == 0)
			return true;
	}

	return false;
}

bool sdlClip::mime_is_image(const std::string& mime)
{
	for (size_t x = 0; x < ARRAYSIZE(mime_image); x++)
	{
		if (mime.compare(mime_image[x]) == 0)
			return true;
	}

	return false;
}

bool sdlClip::mime_is_html(const std::string& mime)
{
	if (mime.compare(mime_html) == 0)
		return true;

	return false;
}

ClipRequest::ClipRequest(UINT32 format, const std::string& mime) : _format(format), _mime(mime)
{
}

uint32_t ClipRequest::format() const
{
	return _format;
}

std::string ClipRequest::formatstr() const
{
	return ClipboardGetFormatIdString(_format);
}

std::string ClipRequest::mime() const
{
	return _mime;
}
