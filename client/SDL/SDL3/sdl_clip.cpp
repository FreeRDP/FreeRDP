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
#include <sstream>
#include <mutex>
#include <iterator>
#include <algorithm>
#include <utility>

#include <winpr/wlog.h>
#include <winpr/image.h>

#include "sdl_clip.hpp"
#include "sdl_freerdp.hpp"

#define TAG CLIENT_TAG("sdl.cliprdr")

#define mime_text_plain "text/plain"
// NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
const char mime_text_utf8[] = mime_text_plain ";charset=utf-8";

static const std::vector<const char*>& s_mime_text()
{
	static std::vector<const char*> values;
	if (values.empty())
	{
		values = std::vector<const char*>(
		    { mime_text_plain, mime_text_utf8, "UTF8_STRING", "COMPOUND_TEXT", "TEXT", "STRING" });
	}
	return values;
}

static const char s_mime_png[] = "image/png";
static const char s_mime_webp[] = "image/webp";
static const char s_mime_jpg[] = "image/jpeg";

static const char s_mime_tiff[] = "image/tiff";
static const char s_mime_uri_list[] = "text/uri-list";
static const char s_mime_html[] = "text/html";

#define BMP_MIME_LIST "image/bmp", "image/x-bmp", "image/x-MS-bmp", "image/x-win-bitmap"

static const std::vector<const char*>& s_mime_bitmap()
{
	static std::vector<const char*> values;
	if (values.empty())
	{
		values = std::vector<const char*>({ BMP_MIME_LIST });
	}
	return values;
}

static const std::vector<const char*>& s_mime_image()
{
	static std::vector<const char*> values;
	if (values.empty())
	{
		if (winpr_image_format_is_supported(WINPR_IMAGE_WEBP))
			values.push_back(s_mime_webp);

		if (winpr_image_format_is_supported(WINPR_IMAGE_PNG))
			values.push_back(s_mime_png);

		if (winpr_image_format_is_supported(WINPR_IMAGE_JPEG))
			values.push_back(s_mime_jpg);

		auto bmp = std::vector<const char*>({ s_mime_tiff, BMP_MIME_LIST });
		values.insert(values.end(), bmp.begin(), bmp.end());
	}
	return values;
}

static const char s_mime_gnome_copied_files[] = "x-special/gnome-copied-files";
static const char s_mime_mate_copied_files[] = "x-special/mate-copied-files";

static const char s_mime_freerdp_update[] = "x-special/freerdp-clipboard-update";

static const char* s_type_HtmlFormat = "HTML Format";
static const char* s_type_FileGroupDescriptorW = "FileGroupDescriptorW";

class ClipboardLockGuard
{
  public:
	explicit ClipboardLockGuard(wClipboard* clipboard) : _clipboard(clipboard)
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

static bool operator==(const CLIPRDR_FORMAT& lhs, const CLIPRDR_FORMAT& rhs)
{
	return (lhs.formatId == rhs.formatId);
}

sdlClip::sdlClip(SdlContext* sdl)
    : _sdl(sdl), _file(cliprdr_file_context_new(this)), _log(WLog_Get(TAG)),
      _system(ClipboardCreate()), _event(CreateEventA(nullptr, TRUE, FALSE, nullptr)),
      _uuid(sdl::utils::generate_uuid_v4())
{
	WINPR_ASSERT(sdl);

	std::stringstream ss;
	ss << s_mime_freerdp_update << "-" << _uuid;
	_mime_uuid = ss.str();
}

sdlClip::~sdlClip()
{
	cliprdr_file_context_free(_file);
	ClipboardDestroy(_system);
	(void)CloseHandle(_event);
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

bool sdlClip::contains(const char** mime_types, Sint32 count)
{
	for (Sint32 x = 0; x < count; x++)
	{
		const auto mime = mime_types[x];
		if (mime && (strcmp(_mime_uuid.c_str(), mime) == 0))
			return true;
	}
	return false;
}

bool sdlClip::handle_update(const SDL_ClipboardEvent& ev)
{
	if (!_ctx || !_sync || ev.owner)
	{
		_last_timestamp = ev.timestamp;
		if (!_current_mimetypes.empty())
		{
			_cache_data.clear();
			auto rc =
			    SDL_SetClipboardData(sdlClip::ClipDataCb, sdlClip::ClipCleanCb, this, ev.mime_types,
			                         WINPR_ASSERTING_INT_CAST(size_t, ev.num_mime_types));
			_current_mimetypes.clear();
			return rc;
		}
		return true;
	}

	if (ev.timestamp == _last_timestamp)
	{
		return true;
	}

	if (contains(ev.mime_types, ev.num_mime_types))
	{
		return true;
	}

	clearServerFormats();

	std::string mime_html = s_mime_html;

	std::vector<std::string> mime_bitmap = { BMP_MIME_LIST };
	std::string mime_webp = s_mime_webp;
	std::string mime_png = s_mime_png;
	std::string mime_jpeg = s_mime_jpg;
	std::string mime_tiff = s_mime_tiff;
	std::vector<std::string> mime_images = { mime_webp, mime_png, mime_jpeg, mime_tiff };

	std::vector<std::string> clientFormatNames;
	std::vector<CLIPRDR_FORMAT> clientFormats;

	size_t nformats = WINPR_ASSERTING_INT_CAST(size_t, ev.num_mime_types);
	const char** clipboard_mime_formats = ev.mime_types;

	WLog_Print(_log, WLOG_TRACE, "SDL has %" PRIuz " formats", nformats);

	bool textPushed = false;
	bool imgPushed = false;

	for (size_t i = 0; i < nformats; i++)
	{
		std::string local_mime = clipboard_mime_formats[i];
		WLog_Print(_log, WLOG_TRACE, " - %s", local_mime.c_str());

		if (std::find(s_mime_text().begin(), s_mime_text().end(), local_mime) !=
		    s_mime_text().end())
		{
			/* text formats */
			if (!textPushed)
			{
				clientFormats.push_back({ CF_TEXT, nullptr });
				clientFormats.push_back({ CF_OEMTEXT, nullptr });
				clientFormats.push_back({ CF_UNICODETEXT, nullptr });
				textPushed = true;
			}
		}
		else if (local_mime == mime_html)
			/* html */
			clientFormatNames.emplace_back(s_type_HtmlFormat);
		else if ((std::find(mime_bitmap.begin(), mime_bitmap.end(), local_mime) !=
		          mime_bitmap.end()) ||
		         (std::find(mime_images.begin(), mime_images.end(), local_mime) !=
		          mime_images.end()))
		{
			/* image formats */
			if (!imgPushed)
			{
				clientFormats.push_back({ CF_DIB, nullptr });
#if defined(WINPR_UTILS_IMAGE_DIBv5)
				clientFormats.push_back({ CF_DIBV5, nullptr });
#endif

				for (auto& bmp : mime_bitmap)
					clientFormatNames.push_back(bmp);

				for (auto& img : mime_images)
					clientFormatNames.push_back(img);

				clientFormatNames.emplace_back(s_type_HtmlFormat);
				imgPushed = true;
			}
		}
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
		{ CB_FORMAT_LIST, 0, 0 },
		static_cast<UINT32>(clientFormats.size()),
		clientFormats.data(),
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
	if (!sdl_push_user_event(SDL_EVENT_CLIPBOARD_UPDATE))
		return ERROR_INTERNAL_ERROR;

	return CHANNEL_RC_OK;
}

UINT sdlClip::SendClientCapabilities()
{
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = {
		CB_CAPSTYPE_GENERAL, 12, CB_CAPS_VERSION_2,
		CB_USE_LONG_FORMAT_NAMES | cliprdr_file_context_current_flags(_file)
	};
	const CLIPRDR_CAPABILITIES capabilities = {
		{ CB_TYPE_NONE, 0, 0 }, 1, reinterpret_cast<CLIPRDR_CAPABILITY_SET*>(&generalCapabilitySet)
	};

	WINPR_ASSERT(_ctx);
	WINPR_ASSERT(_ctx->ClientCapabilities);
	return _ctx->ClientCapabilities(_ctx, &capabilities);
}

void sdlClip::clearServerFormats()
{
	_serverFormats.clear();
	_cache_data.clear();
	cliprdr_file_context_clear(_file);
}

UINT sdlClip::SendFormatListResponse(BOOL status)
{
	const CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse = {
		{ CB_FORMAT_LIST_RESPONSE, static_cast<UINT16>(status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL),
		  0 }
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

UINT sdlClip::SendDataRequest(uint32_t formatID, const std::string& mime)
{
	const CLIPRDR_FORMAT_DATA_REQUEST request = { { CB_TYPE_NONE, 0, 0 }, formatID };

	_request_queue.emplace(formatID, mime);

	WINPR_ASSERT(_ctx);
	WINPR_ASSERT(_ctx->ClientFormatDataRequest);
	UINT ret = _ctx->ClientFormatDataRequest(_ctx, &request);
	if (ret != CHANNEL_RC_OK)
	{
		WLog_Print(_log, WLOG_ERROR, "error sending ClientFormatDataRequest, cancelling request");
		_request_queue.pop();
	}

	return ret;
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
		cmp = s_type_HtmlFormat;
	if (mime_is_file(mime))
		cmp = s_type_FileGroupDescriptorW;

	for (auto& format : _serverFormats)
	{
		if (!format.formatName())
			continue;
		if (cmp == format.formatName())
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

		clipboard->_serverFormats.emplace_back(format->formatId, format->formatName);

		if (format->formatName)
		{
			if (strcmp(format->formatName, s_type_HtmlFormat) == 0)
			{
				text = TRUE;
				html = TRUE;
			}
			else if (strcmp(format->formatName, s_type_FileGroupDescriptorW) == 0)
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

	clipboard->_current_mimetypes.clear();
	if (text)
	{
		clipboard->_current_mimetypes.insert(clipboard->_current_mimetypes.end(),
		                                     s_mime_text().begin(), s_mime_text().end());
	}
	if (image)
	{
		clipboard->_current_mimetypes.insert(clipboard->_current_mimetypes.end(),
		                                     s_mime_bitmap().begin(), s_mime_bitmap().end());
		clipboard->_current_mimetypes.insert(clipboard->_current_mimetypes.end(),
		                                     s_mime_image().begin(), s_mime_image().end());
	}
	if (html)
	{
		clipboard->_current_mimetypes.push_back(s_mime_html);
	}
	if (file)
	{
		clipboard->_current_mimetypes.push_back(s_mime_uri_list);
		clipboard->_current_mimetypes.push_back(s_mime_gnome_copied_files);
		clipboard->_current_mimetypes.push_back(s_mime_mate_copied_files);
	}
	clipboard->_current_mimetypes.push_back(clipboard->_mime_uuid.c_str());

	auto s = clipboard->_current_mimetypes.size();
	SDL_Event ev = { SDL_EVENT_CLIPBOARD_UPDATE };
	ev.clipboard.owner = true;
	ev.clipboard.timestamp = SDL_GetTicksNS();
	ev.clipboard.num_mime_types = WINPR_ASSERTING_INT_CAST(Sint32, s);
	ev.clipboard.mime_types = clipboard->_current_mimetypes.data();

	auto rc = (SDL_PushEvent(&ev) == 1);
	return clipboard->SendFormatListResponse(rc);
}

UINT sdlClip::ReceiveFormatListResponse(WINPR_ATTR_UNUSED CliprdrClientContext* context,
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
	WLog_Print(clipboard->_log, WLOG_DEBUG, "Requesting format %s [0x%08" PRIx32 "] [%s]",
	           ClipboardGetFormatIdString(localFormatId), localFormatId,
	           ClipboardGetFormatName(clipboard->_system, localFormatId));

	ClipboardLockGuard systemlock(clipboard->_system);
	std::lock_guard<CriticalSection> lock(clipboard->_lock);

	const UINT32 fileFormatId =
	    ClipboardGetFormatId(clipboard->_system, s_type_FileGroupDescriptorW);
	const UINT32 htmlFormatId = ClipboardGetFormatId(clipboard->_system, s_type_HtmlFormat);

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
			mime = s_mime_bitmap()[0];
			localFormatId = ClipboardGetFormatId(clipboard->_system, mime);
			break;

		case CF_TIFF:
			mime = s_mime_tiff;
			break;

		default:
			if (formatId == fileFormatId)
			{
				localFormatId = ClipboardGetFormatId(clipboard->_system, s_mime_uri_list);
				mime = s_mime_uri_list;
			}
			else if (formatId == htmlFormatId)
			{
				/* In case HTML format was requested but we only have images in local clipboard */
				if (!SDL_HasClipboardData(s_mime_html))
				{
					for (const auto& cmime : s_mime_image())
					{
						if (SDL_HasClipboardData(cmime))
						{
							localFormatId = ClipboardGetFormatId(clipboard->_system, cmime);
							mime = cmime;
							break;
						}
					}
				}
				else
				{
					localFormatId = ClipboardGetFormatId(clipboard->_system, s_mime_html);
					mime = s_mime_html;
				}
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

	ClipboardLockGuard systemlock(clipboard->_system);
	std::lock_guard<CriticalSection> lock(clipboard->_lock);
	if (clipboard->_request_queue.empty())
	{
		WLog_Print(clipboard->_log, WLOG_ERROR, "no pending format request");
		return ERROR_INTERNAL_ERROR;
	}

	do
	{
		UINT32 srcFormatId = 0;
		auto& request = clipboard->_request_queue.front();
		bool success = (formatDataResponse->common.msgFlags & CB_RESPONSE_OK) &&
		               !(formatDataResponse->common.msgFlags & CB_RESPONSE_FAIL);
		request.setSuccess(success);

		if (!success)
		{
			WLog_Print(clipboard->_log, WLOG_WARN,
			           "clipboard data request for format %" PRIu32 " [%s], mime %s failed",
			           request.format(), request.formatstr().c_str(), request.mime().c_str());
			break;
		}

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
					if (name == s_type_FileGroupDescriptorW)
					{
						srcFormatId =
						    ClipboardGetFormatId(clipboard->_system, s_type_FileGroupDescriptorW);

						if (!cliprdr_file_context_update_server_data(
						        clipboard->_file, clipboard->_system, data, size))
							return ERROR_INTERNAL_ERROR;
					}
					else if (name == s_type_HtmlFormat)
					{
						srcFormatId = ClipboardGetFormatId(clipboard->_system, s_type_HtmlFormat);
					}
				}
			}
			break;
		}

		if (!ClipboardSetData(clipboard->_system, srcFormatId, data, size))
		{
			WLog_Print(clipboard->_log, WLOG_ERROR, "error when setting clipboard data");
			return ERROR_INTERNAL_ERROR;
		}
		WLog_Print(clipboard->_log, WLOG_DEBUG, "updated clipboard data %s [0x%08" PRIx32 "]",
		           ClipboardGetFormatName(clipboard->_system, srcFormatId), srcFormatId);
	} while (false);

	if (!SetEvent(clipboard->_event))
	{
		WLog_Print(clipboard->_log, WLOG_ERROR, "error when setting clipboard event");
		return ERROR_INTERNAL_ERROR;
	}

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
		ClipboardLockGuard systemlock(clip->_system);
		std::lock_guard<CriticalSection> lock(clip->_lock);

		/* check if we already used this mime type */
		auto cache = clip->_cache_data.find(mime_type);
		if (cache != clip->_cache_data.end())
		{
			*size = cache->second.size;
			return cache->second.ptr.get();
		}

		auto formatID = clip->serverIdForMime(mime_type);

		/* Can we convert the data from existing formats in the clibpard? */
		uint32_t fsize = 0;
		auto mimeFormatID = ClipboardRegisterFormat(clip->_system, mime_type);
		auto fptr = ClipboardGetData(clip->_system, mimeFormatID, &fsize);
		if (fptr)
		{
			auto ptr = std::shared_ptr<void>(fptr, free);
			clip->_cache_data.insert({ mime_type, { fsize, ptr } });

			auto fcache = clip->_cache_data.find(mime_type);
			if (fcache != clip->_cache_data.end())
			{
				*size = fcache->second.size;
				return fcache->second.ptr.get();
			}
		}

		WLog_Print(clip->_log, WLOG_DEBUG, "requesting format %s [%s 0x%08" PRIx32 "]", mime_type,
		           ClipboardGetFormatName(clip->_system, formatID), formatID);
		if (clip->SendDataRequest(formatID, mime_type))
			return nullptr;
	}
	{
		HANDLE hdl[2] = { freerdp_abort_event(clip->_sdl->context()), clip->_event };

		DWORD status = WaitForMultipleObjects(ARRAYSIZE(hdl), hdl, FALSE, 10 * 1000);

		if (status != WAIT_OBJECT_0 + 1)
		{
			std::lock_guard<CriticalSection> lock(clip->_lock);
			clip->_request_queue.pop();

			if (status == WAIT_TIMEOUT)
				WLog_Print(clip->_log, WLOG_ERROR,
				           "no reply in 10 seconds, returning empty content");

			return nullptr;
		}
	}

	{
		ClipboardLockGuard systemlock(clip->_system);
		std::lock_guard<CriticalSection> lock(clip->_lock);
		auto request = clip->_request_queue.front();
		clip->_request_queue.pop();

		if (clip->_request_queue.empty())
			(void)ResetEvent(clip->_event);

		if (request.success())
		{
			auto formatID = ClipboardRegisterFormat(clip->_system, mime_type);
			auto data = ClipboardGetData(clip->_system, formatID, &len);
			if (!data)
			{
				WLog_Print(clip->_log, WLOG_ERROR, "error retrieving clipboard data");
				return nullptr;
			}

			auto ptr = std::shared_ptr<void>(data, free);
			clip->_cache_data.insert({ mime_type, { len, ptr } });
			*size = len;
			return ptr.get();
		}

		return nullptr;
	}
}

void sdlClip::ClipCleanCb(void* userdata)
{
	auto clip = static_cast<sdlClip*>(userdata);
	WINPR_ASSERT(clip);
	ClipboardLockGuard give_me_a_name(clip->_system);
	std::lock_guard<CriticalSection> lock(clip->_lock);
	ClipboardEmpty(clip->_system);
}

bool sdlClip::mime_is_file(const std::string& mime)
{
	if (strncmp(s_mime_uri_list, mime.c_str(), sizeof(s_mime_uri_list)) == 0)
		return true;
	if (strncmp(s_mime_gnome_copied_files, mime.c_str(), sizeof(s_mime_gnome_copied_files)) == 0)
		return true;
	if (strncmp(s_mime_mate_copied_files, mime.c_str(), sizeof(s_mime_mate_copied_files)) == 0)
		return true;
	return false;
}

bool sdlClip::mime_is_text(const std::string& mime)
{
	for (const auto& tmime : s_mime_text())
	{
		assert(tmime != nullptr);
		if (mime == tmime)
			return true;
	}

	return false;
}

bool sdlClip::mime_is_image(const std::string& mime)
{
	for (const auto& imime : s_mime_image())
	{
		assert(imime != nullptr);
		if (mime == imime)
			return true;
	}

	return false;
}

bool sdlClip::mime_is_bmp(const std::string& mime)
{
	for (const auto& imime : s_mime_bitmap())
	{
		assert(imime != nullptr);
		if (mime == imime)
			return true;
	}

	return false;
}

bool sdlClip::mime_is_html(const std::string& mime)
{
	return mime.compare(s_mime_html) == 0;
}

ClipRequest::ClipRequest(UINT32 format, const std::string& mime)
    : _format(format), _mime(mime), _success(false)
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

bool ClipRequest::success() const
{
	return _success;
}

void ClipRequest::setSuccess(bool status)
{
	_success = status;
}
