/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client clipboard helper
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
#include "sdl_context.hpp"

#define TAG CLIENT_TAG("sdl.cliprdr")

#define mime_text_plain "text/plain"
// NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
const char mime_text_utf8[] = mime_text_plain ";charset=utf-8";

[[nodiscard]] static const std::vector<const char*>& s_mime_text()
{
	static std::vector<const char*> values;
	if (values.empty())
	{
		values = std::vector<const char*>(
		    { mime_text_plain, mime_text_utf8, "UTF8_STRING", "COMPOUND_TEXT", "TEXT", "STRING" });
	}
	return values;
}

static const char s_mime_jxl[] = "image/jxl";
static const char s_mime_avif[] = "image/avif";
static const char s_mime_png[] = "image/png";
static const char s_mime_webp[] = "image/webp";
static const char s_mime_jpg[] = "image/jpeg";

static const char s_mime_tiff[] = "image/tiff";
static const char s_mime_uri_list[] = "text/uri-list";
static const char s_mime_html[] = "text/html";

#define BMP_MIME_LIST "image/bmp", "image/x-bmp", "image/x-MS-bmp", "image/x-win-bitmap"

[[nodiscard]] static const std::vector<const char*>& s_mime_bitmap()
{
	static std::vector<const char*> values;
	if (values.empty())
	{
		values = std::vector<const char*>({ BMP_MIME_LIST });
	}
	return values;
}

[[nodiscard]] static const std::vector<const char*>& s_mime_image()
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

		auto bmp = std::vector<const char*>({ BMP_MIME_LIST });
		values.insert(values.end(), bmp.begin(), bmp.end());
	}
	return values;
}

/* Local image types winpr can turn into a CF_DIB, in order of preference: BMP only needs its
 * file header stripped, the others need the matching winpr image support compiled in. */
[[nodiscard]] static const std::vector<const char*>& s_mime_dib_sources()
{
	static const std::vector<const char*> values = []()
	{
		std::vector<const char*> v({ BMP_MIME_LIST });
#if defined(WINPR_UTILS_IMAGE_PNG)
		v.push_back(s_mime_png);
#endif
#if defined(WINPR_UTILS_IMAGE_WEBP)
		v.push_back(s_mime_webp);
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
		v.push_back(s_mime_jpg);
#endif
		return v;
	}();
	return values;
}

/* Local image types an "HTML Format" request may be answered from */
[[nodiscard]] static const std::vector<const char*>& s_mime_html_image_sources()
{
	static const std::vector<const char*> values({ s_mime_jpg, s_mime_png, s_mime_webp, s_mime_avif,
	                                               s_mime_jxl, s_mime_tiff, BMP_MIME_LIST });
	return values;
}

/* The mime types SDL has cached for the current clipboard owner. Unlike SDL_HasClipboardData
 * this does no clipboard I/O: on X11, SDL_HasClipboardData transfers the data, and calls our
 * own ClipDataCb when we are the owner. */
[[nodiscard]] static std::vector<std::string> localMimeTypes()
{
	std::vector<std::string> list;
	size_t count = 0;
	auto mimes = SDL_GetClipboardMimeTypes(&count);
	for (size_t x = 0; mimes && (x < count); x++)
	{
		if (mimes[x])
			list.emplace_back(mimes[x]);
	}
	SDL_free(static_cast<void*>(mimes));
	return list;
}

[[nodiscard]] static bool hasMime(const std::vector<std::string>& list, const char* mime)
{
	return std::find(list.begin(), list.end(), mime) != list.end();
}

[[nodiscard]] static const char* firstOffered(const std::vector<std::string>& offered,
                                              const std::vector<const char*>& candidates)
{
	for (const auto& m : candidates)
	{
		if (hasMime(offered, m))
			return m;
	}
	return nullptr;
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

	std::ignore = cliprdr_file_context_set_locally_available(_file, TRUE);
}

sdlClip::~sdlClip()
{
	cliprdr_file_context_free(_file);
	ClipboardDestroy(_system);
	std::ignore = CloseHandle(_event);
}

bool sdlClip::init(CliprdrClientContext* clip)
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

bool sdlClip::uninit(CliprdrClientContext* clip)
{
	WINPR_ASSERT(clip);
	if (!cliprdr_file_context_uninit(_file, _ctx))
		return false;
	ClipboardLockGuard systemlock(_system);
	std::scoped_lock lock(_lock);
	_ctx = nullptr;
	clip->custom = nullptr;
	return true;
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

bool sdlClip::ownsClipboard() const
{
	return hasMime(localMimeTypes(), _mime_uuid.c_str());
}

void sdlClip::noteInput(Uint64 timestamp)
{
	/* An event dequeued after our offer may still be older than it, and then SDL already used
	 * its serial for the offer: counting it let a replacement go to mutter with that same
	 * serial, which mutter ignores (seen with a key press queued during a server format
	 * list). */
	if (timestamp > _offer_timestamp)
		_input_since_offer = true;
}

bool sdlClip::keepCurrentOffer() const
{
	/* Only Wayland ties clipboard ownership to input serials. mutter ignores a set_selection
	 * whose serial is not newer than the current selection's, and cancels one from a window
	 * without keyboard focus; SDL destroys its previous source either way. Replacing an offer
	 * we still own in those cases leaves the clipboard without an owner, mutter restores its
	 * saved copy of an older clipboard, and we would announce that to the server. The offer we
	 * have serves the new server data as it is: ClipDataCb fetches on demand, caches cleared.
	 * This holds even when the formats differ. Windows can send an empty format list between
	 * two announcements of one copy (list, empty list, list within 250 ms, all without input).
	 * Replacing the offer for the empty list sent the older local clipboard to the server,
	 * which then owned "our" data while we waited for its own: a 10 s freeze, and pastes on
	 * the server failed. A format the server no longer has fails on request. */
	const char* driver = SDL_GetCurrentVideoDriver();
	if (!driver || (strcmp(driver, "wayland") != 0))
		return false;
	if (!ownsClipboard())
		return false;
	return !_input_since_offer || !SDL_GetKeyboardFocus();
}

bool sdlClip::handleEvent(const SDL_ClipboardEvent& ev)
{
	if (!_ctx || !_sync || ev.owner)
	{
		_last_timestamp = ev.timestamp;

		/* The channel thread replaces _current_mimetypes on every server format list, so take
		 * them under the lock instead of trusting pointers carried in the event. */
		std::vector<std::string> mimes;
		{
			ClipboardLockGuard systemlock(_system);
			std::scoped_lock lock(_lock);
			mimes.swap(_current_mimetypes);
			if (!mimes.empty())
				_cache_data.clear();
		}
		if (mimes.empty())
			return true;

		if (keepCurrentOffer())
		{
			WLog_Print(_log, WLOG_DEBUG,
			           "keeping the clipboard offer (same formats %d): it cannot be replaced now",
			           mimes == _offered_mimetypes);
			return true;
		}
		WLog_Print(_log, WLOG_DEBUG,
		           "offering server formats (owned %d, same formats %d, input since offer %d, "
		           "focused %d)",
		           ownsClipboard(), mimes == _offered_mimetypes, _input_since_offer,
		           SDL_GetKeyboardFocus() != nullptr);

		std::vector<const char*> cmimes;
		cmimes.reserve(mimes.size());
		for (const auto& m : mimes)
			cmimes.push_back(m.c_str());
		/* SDL copies the mime type strings */
		if (!SDL_SetClipboardData(sdlClip::ClipDataCb, sdlClip::ClipCleanCb, this, cmimes.data(),
		                          cmimes.size()))
			return false;
		_offered_mimetypes = std::move(mimes);
		_input_since_offer = false;
		_offer_timestamp = SDL_GetTicksNS();
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

	/* SDL holds this session's server data right now, so this update was queued before our
	 * later SDL_SetClipboardData took effect: typically the selection a compositor re-sends on
	 * focus-in, racing a server format list. Announcing it would replace the fresh server
	 * clipboard with the stale local one (and the server would then ask us for data we no
	 * longer own). A genuine local change cancels our data before its update is queued. */
	if (ownsClipboard())
	{
		WLog_Print(_log, WLOG_DEBUG, "ignoring stale local clipboard update");
		return true;
	}

	{
		ClipboardLockGuard systemlock(_system);
		std::scoped_lock lock(_lock);
		clearServerFormats();
	}

	const std::string mime_html = s_mime_html;

	const std::vector<std::string> mime_bitmap = { BMP_MIME_LIST };
	const std::string mime_webp = s_mime_webp;
	const std::string mime_png = s_mime_png;
	const std::string mime_jpeg = s_mime_jpg;
	const std::string mime_tiff = s_mime_tiff;
	const std::vector<std::string> mime_images = { mime_webp, mime_png,    mime_jpeg,
		                                           mime_tiff, s_mime_avif, s_mime_jxl };

	std::vector<std::string> clientFormatNames;
	std::vector<CLIPRDR_FORMAT> clientFormats;

	size_t nformats = WINPR_ASSERTING_INT_CAST(size_t, ev.num_mime_types);
	const char** clipboard_mime_formats = ev.mime_types;

	WLog_Print(_log, WLOG_TRACE, "SDL has %" PRIuz " formats", nformats);

	bool textPushed = false;
	bool imgPushed = false;
	bool filePushed = false;

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

				if (winpr_image_format_is_supported(WINPR_IMAGE_BITMAP))
				{
					for (auto& bmp : mime_bitmap)
						clientFormatNames.push_back(bmp);
				}

				if (winpr_image_format_is_supported(WINPR_IMAGE_JPEG))
					clientFormatNames.push_back(mime_jpeg);
				if (winpr_image_format_is_supported(WINPR_IMAGE_WEBP))
					clientFormatNames.push_back(mime_webp);
				if (winpr_image_format_is_supported(WINPR_IMAGE_PNG))
					clientFormatNames.push_back(mime_png);

				clientFormatNames.emplace_back(s_type_HtmlFormat);
				imgPushed = true;
			}
			clientFormatNames.push_back(local_mime);
		}
		else if (mime_is_file(local_mime))
		{
			if (!filePushed)
			{
				clientFormatNames.emplace_back(s_type_FileGroupDescriptorW);
				filePushed = true;
			}
		}
	}

	std::sort(clientFormatNames.begin(), clientFormatNames.end());
	clientFormatNames.erase(std::unique(clientFormatNames.begin(), clientFormatNames.end()),
	                        clientFormatNames.end());

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

	if (cliprdr_file_context_notify_new_client_format_list(_file) != CHANNEL_RC_OK)
		return false;

	{
		std::scoped_lock lock(_lock);
		_client_list.clear();
		for (const auto& format : clientFormats)
			_client_list.emplace_back(format.formatId, format.formatName ? format.formatName : "");
		_client_list_generation++;
		_client_list_resends = 0;
		_client_lists_in_flight++;
	}
	return sendFormatList(formatList);
}

bool sdlClip::sendFormatList(const CLIPRDR_FORMAT_LIST& formatList)
{
	WINPR_ASSERT(_ctx);
	WINPR_ASSERT(_ctx->ClientFormatList);
	if (_ctx->ClientFormatList(_ctx, &formatList) == CHANNEL_RC_OK)
		return true;
	std::scoped_lock lock(_lock);
	if (_client_lists_in_flight > 0)
		_client_lists_in_flight--;
	return false;
}

bool sdlClip::resendFormatList(uint32_t generation)
{
	if (!_ctx)
		return true;

	/* A server format list since then put server data on the local clipboard */
	if (ownsClipboard())
		return true;

	std::vector<std::pair<uint32_t, std::string>> list;
	{
		std::scoped_lock lock(_lock);
		if (generation != _client_list_generation)
			return true;
		list = _client_list;
		_client_lists_in_flight++;
	}

	std::vector<CLIPRDR_FORMAT> formats;
	formats.reserve(list.size());
	for (auto& entry : list)
		formats.push_back({ entry.first, entry.second.empty() ? nullptr : entry.second.data() });

	const CLIPRDR_FORMAT_LIST formatList = {
		{ CB_FORMAT_LIST, 0, 0 },
		static_cast<UINT32>(formats.size()),
		formats.data(),
	};
	WLog_Print(_log, WLOG_DEBUG, "sending the refused format list again (%" PRIu32 " formats)",
	           formatList.numFormats);
	return sendFormatList(formatList);
}

/* Timer thread: only hands the resend to the main thread */
static Uint32 SDLCALL resendFormatListTimer(void* userdata, WINPR_ATTR_UNUSED SDL_TimerID id,
                                            WINPR_ATTR_UNUSED Uint32 interval)
{
	std::ignore = sdl_push_user_event(SDL_EVENT_USER_CLIPBOARD_RESEND_LIST,
	                                  static_cast<UINT32>(reinterpret_cast<uintptr_t>(userdata)));
	return 0;
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

bool sdlClip::hasServerFormat(uint32_t id) const
{
	return std::any_of(_serverFormats.begin(), _serverFormats.end(),
	                   [id](const auto& fmt) { return fmt.formatId() == id; });
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

	auto filecontext = static_cast<CliprdrFileContext*>(context->custom);
	auto clipboard = static_cast<sdlClip*>(cliprdr_file_context_get_context(filecontext));
	WINPR_ASSERT(clipboard);

	ClipboardLockGuard systemlock(clipboard->_system);
	std::scoped_lock lock(clipboard->_lock);

	clipboard->clearServerFormats();
	/* The server clipboard changed, so a pending resend of our older list must not replace it */
	clipboard->_client_list_generation++;
	/* _system may still hold local clipboard data converted for an earlier server request.
	 * ClipDataCb converts from _system before asking the server, so left in place it would
	 * answer the next local paste with that stale local data instead of the new server data. */
	ClipboardEmpty(clipboard->_system);

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

	{
		auto res = cliprdr_file_context_notify_new_server_format_list(filecontext);
		if (res != CHANNEL_RC_OK)
			return res;
	}

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

	auto& mime = clipboard->_current_mimetypes;
	std::sort(mime.begin(), mime.end());
	mime.erase(std::unique(mime.begin(), mime.end()), mime.end());

	WLog_Print(clipboard->_log, WLOG_TRACE,
	           "-------------- server mime types [%" PRIuz "] ------------------", mime.size());
	for (const auto& m : mime)
	{
		WLog_Print(clipboard->_log, WLOG_TRACE, "server announces %s]", m.c_str());
	}

	/* handleEvent picks the mime types up from _current_mimetypes */
	SDL_Event ev = { SDL_EVENT_CLIPBOARD_UPDATE };
	ev.clipboard.owner = true;
	ev.clipboard.timestamp = SDL_GetTicksNS();

	auto rc = (SDL_PushEvent(&ev) == 1);
	return clipboard->SendFormatListResponse(rc);
}

UINT sdlClip::ReceiveFormatListResponse(CliprdrClientContext* context,
                                        const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatListResponse);

	auto clipboard = static_cast<sdlClip*>(
	    cliprdr_file_context_get_context(static_cast<CliprdrFileContext*>(context->custom)));
	WINPR_ASSERT(clipboard);

	std::scoped_lock lock(clipboard->_lock);
	if (clipboard->_client_lists_in_flight > 0)
		clipboard->_client_lists_in_flight--;

	if (!(formatListResponse->common.msgFlags & CB_RESPONSE_FAIL))
		return CHANNEL_RC_OK;

	/* Windows refuses a format list while another process has its clipboard open (seen on one
	 * host for a quarter of all copies); the server clipboard then keeps its old content and a
	 * paste there gives that. The clipboard is free again a moment later, so send the list
	 * again, unless a newer one is on its way or the local clipboard changed meanwhile. */
	static constexpr Uint32 resendDelays[] = { 200, 500, 1000 };
	if (clipboard->_client_lists_in_flight > 0)
	{
		WLog_Print(clipboard->_log, WLOG_WARN, "format list update failed, a newer one is pending");
		return CHANNEL_RC_OK;
	}
	if (clipboard->_client_list_resends >= ARRAYSIZE(resendDelays))
	{
		WLog_Print(clipboard->_log, WLOG_WARN,
		           "format list update failed, giving up after %" PRIuz " resends",
		           clipboard->_client_list_resends);
		return CHANNEL_RC_OK;
	}
	const auto delay = resendDelays[clipboard->_client_list_resends++];
	WLog_Print(clipboard->_log, WLOG_WARN,
	           "format list update failed, sending it again in %" PRIu32 " ms", delay);
	if (SDL_AddTimer(delay, resendFormatListTimer,
	                 reinterpret_cast<void*>(
	                     static_cast<uintptr_t>(clipboard->_client_list_generation))) == 0)
		WLog_Print(clipboard->_log, WLOG_WARN, "SDL_AddTimer: %s", SDL_GetError());
	return CHANNEL_RC_OK;
}

std::shared_ptr<BYTE> sdlClip::getLocalData(uint32_t formatId, uint32_t& len)
{
	len = 0;

	const auto offered = localMimeTypes();

	/* The local clipboard still holds what this session received from the server (the
	 * server asked before our format list reached it). Reading it through SDL would call
	 * ClipDataCb and ask the server for its own data, so fail the request instead. */
	if (hasMime(offered, _mime_uuid.c_str()))
	{
		WLog_Print(_log, WLOG_DEBUG, "local clipboard holds server data, not answering request");
		return {};
	}

	UINT32 fileFormatId = 0;
	UINT32 htmlFormatId = 0;
	{
		ClipboardLockGuard systemlock(_system);
		std::scoped_lock lock(_lock);
		WLog_Print(_log, WLOG_DEBUG, "Requesting format %s [0x%08" PRIx32 "] [%s]",
		           ClipboardGetFormatIdString(formatId), formatId,
		           ClipboardGetFormatName(_system, formatId));
		fileFormatId = ClipboardGetFormatId(_system, s_type_FileGroupDescriptorW);
		htmlFormatId = ClipboardGetFormatId(_system, s_type_HtmlFormat);
	}

	/* Local mime types to try, in order, and the _system format each is stored as for winpr to
	 * convert: its registered format, the requested one, or a newly registered one. */
	enum class Store
	{
		Lookup,
		Requested,
		Register
	};
	struct Candidate
	{
		const char* mime;
		Store store;
	};
	std::vector<Candidate> candidates;

	switch (formatId)
	{
		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
		{
			const char* mime = firstOffered(offered, s_mime_text());
			if (mime)
				candidates.push_back({ mime, Store::Lookup });
		}
		break;

		case CF_DIB:
		case CF_DIBV5:
			/* Every convertible type on offer (screenshot tools offer image/png only, never
			 * image/bmp), in case a decoder rejects one of them. */
			for (const auto& mime : s_mime_dib_sources())
			{
				if (hasMime(offered, mime))
					candidates.push_back({ mime, Store::Register });
			}
			break;

		case CF_TIFF:
			candidates.push_back({ s_mime_tiff, Store::Requested });
			break;

		default:
			if (formatId == fileFormatId)
				candidates.push_back({ s_mime_uri_list, Store::Lookup });
			else if (formatId == htmlFormatId)
			{
				/* In case HTML format was requested but we only have images in local clipboard */
				const char* mime = hasMime(offered, s_mime_html)
				                       ? s_mime_html
				                       : firstOffered(offered, s_mime_html_image_sources());
				if (mime)
					candidates.push_back({ mime, Store::Lookup });
			}
			else
			{
				ClipboardLockGuard systemlock(_system);
				std::scoped_lock lock(_lock);
				const char* formatName = ClipboardGetFormatName(_system, formatId);
				if (formatName && hasMime(offered, formatName))
				{
					/* the name is owned by _system and outlives this function */
					candidates.push_back({ formatName, Store::Lookup });
				}
			}
			break;
	}

	std::shared_ptr<BYTE> data;
	for (const auto& candidate : candidates)
	{
		/* Read without holding our locks: on X11 this pumps events and can take a while, and
		 * the channel thread needs the locks to deliver server data. */
		size_t size = 0;
		auto sdldata = std::shared_ptr<void>(SDL_GetClipboardData(candidate.mime, &size), SDL_free);
		if (!sdldata || (size > UINT32_MAX))
			continue;

		ClipboardLockGuard systemlock(_system);
		std::scoped_lock lock(_lock);

		if (formatId == fileFormatId)
		{
			auto bdata = static_cast<const char*>(sdldata.get());
			if (!cliprdr_file_context_update_client_data(_file, bdata, size))
				continue;
		}

		UINT32 storeId = formatId;
		if (candidate.store == Store::Lookup)
			storeId = ClipboardGetFormatId(_system, candidate.mime);
		else if (candidate.store == Store::Register)
			storeId = ClipboardRegisterFormat(_system, candidate.mime);

		uint32_t ptrlen = 0;
		BYTE* ptr = nullptr;
		if (ClipboardSetData(_system, storeId, sdldata.get(), static_cast<uint32_t>(size)))
			ptr = static_cast<BYTE*>(ClipboardGetData(_system, formatId, &ptrlen));

		/* _system is also where ClipDataCb looks for server data: leave no local data behind,
		 * or a later local paste would be answered with it. */
		ClipboardEmpty(_system);

		if (!ptr)
		{
			WLog_Print(_log, WLOG_DEBUG, "could not convert local %s", candidate.mime);
			continue;
		}

		data = std::shared_ptr<BYTE>(ptr, free);
		len = ptrlen;

		if (formatId == fileFormatId)
		{
			BYTE* ddata = nullptr;
			UINT32 dsize = 0;
			const UINT32 flags = cliprdr_file_context_remote_get_flags(_file);
			const UINT32 error = cliprdr_serialize_file_list_ex(
			    flags, reinterpret_cast<const FILEDESCRIPTORW*>(data.get()),
			    ptrlen / sizeof(FILEDESCRIPTORW), &ddata, &dsize);
			data = std::shared_ptr<BYTE>(ddata, free);
			len = dsize;
			if (error)
			{
				data.reset();
				len = 0;
			}
		}
		break;
	}
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

	{
		std::scoped_lock lock(clipboard->_lock);
		/* ClipDataCb is waiting on the main thread for the server: a local application is
		 * reading our offer of server data, so the local clipboard holds no client data to
		 * give. Answer now. Queued for the main thread, the request would wait for the end of
		 * that read while the server may hold its reply until we answer (seen with Windows:
		 * a 9 s freeze until ClipDataCb timed out). */
		if (clipboard->_reading_server)
		{
			WLog_Print(clipboard->_log, WLOG_DEBUG,
			           "local read of server data in progress, failing server request");
			return clipboard->SendDataResponse(nullptr, 0);
		}
		clipboard->_server_requests.push_back(formatDataRequest->requestedFormatId);
	}

	/* This runs on the channel thread, but the SDL clipboard API is main-thread only: on
	 * Wayland, reading the selection offer here races the main thread replacing it. */
	if (!sdl_push_user_event(SDL_EVENT_USER_CLIPBOARD_DATA_REQUEST,
	                         formatDataRequest->requestedFormatId))
	{
		std::scoped_lock lock(clipboard->_lock);
		if (!clipboard->_server_requests.empty())
			clipboard->_server_requests.pop_back();
		return clipboard->SendDataResponse(nullptr, 0);
	}
	return CHANNEL_RC_OK;
}

bool sdlClip::handleDataRequests()
{
	for (;;)
	{
		uint32_t formatId = 0;
		{
			std::scoped_lock lock(_lock);
			if (_server_requests.empty())
				return true;
			formatId = _server_requests.front();
			_server_requests.pop_front();
		}
		if (!_ctx)
			continue;

		uint32_t len = 0;
		auto data = getLocalData(formatId, len);
		if (SendDataResponse(data.get(), len) != CHANNEL_RC_OK)
			WLog_Print(_log, WLOG_WARN, "failed to send clipboard data response");
	}
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
	std::scoped_lock lock(clipboard->_lock);
	if (clipboard->_request_queue.empty())
	{
		/* The matching request already timed out (see the wait below) and was
		 * popped, so this is a late reply on a slow/high-latency link. Dropping it
		 * is harmless; returning an error here would kill the cliprdr channel thread
		 * and tear down the whole session. Warn and continue instead. */
		WLog_Print(clipboard->_log, WLOG_WARN,
		           "format data response with no pending request (late reply?), ignoring");
		return CHANNEL_RC_OK;
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

						if (cliprdr_file_context_has_local_support(clipboard->_file))
						{
							if (!cliprdr_file_context_update_server_data(
							        clipboard->_file, clipboard->_system, data, size))
							{
								WLog_Print(clipboard->_log, WLOG_WARN,
								           "File clipboard failed to update");
							}
						}
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
			/* Fail this request only: an error return here would end the clipboard channel
			 * and, without auto-reconnect, the session. */
			WLog_Print(clipboard->_log, WLOG_ERROR, "error when setting clipboard data");
			request.setSuccess(false);
			break;
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
	uint32_t formatID = 0;

	if (mime_is_text(mime_type))
		mime_type = "text/plain";

	{
		ClipboardLockGuard systemlock(clip->_system);
		std::scoped_lock lock(clip->_lock);

		if (!clip->_ctx)
			return nullptr;

		/* check if we already used this mime type */
		auto cache = clip->_cache_data.find(mime_type);
		if (cache != clip->_cache_data.end())
		{
			*size = cache->second.size;
			return cache->second.ptr.get();
		}

		formatID = clip->serverIdForMime(mime_type);

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

		/* From here until the reply, server data requests are answered on arrival (see
		 * ReceiveFormatDataRequest). Those already queued get the same answer now: a local
		 * application is reading our offer, so there is no client data to give. */
		clip->_reading_server = true;
		while (!clip->_server_requests.empty())
		{
			clip->_server_requests.pop_front();
			WLog_Print(clip->_log, WLOG_DEBUG,
			           "local read of server data in progress, failing queued server request");
			std::ignore = clip->SendDataResponse(nullptr, 0);
		}
	}

	/* A Windows server answers CB_RESPONSE_FAIL while another process has its clipboard open,
	 * and GNOME reads every new server clipboard within milliseconds of our offer, often while
	 * the copying application or a clipboard watcher on the server still holds it. Seen on one
	 * Windows host for about half of all image copies: the copy never arrived. The clipboard is
	 * free again a moment later, so ask again before giving up. */
	static constexpr DWORD retryDelays[] = { 100, 200, 400 };
	for (size_t attempt = 0;; attempt++)
	{
		{
			ClipboardLockGuard systemlock(clip->_system);
			std::scoped_lock lock(clip->_lock);
			if (attempt > 0)
			{
				/* A new server format list may have arrived meanwhile */
				formatID = clip->serverIdForMime(mime_type);
				if (!clip->hasServerFormat(formatID))
				{
					clip->_reading_server = false;
					return nullptr;
				}
			}
			if (clip->SendDataRequest(formatID, mime_type))
			{
				clip->_reading_server = false;
				return nullptr;
			}
		}
		{
			HANDLE hdl[2] = { freerdp_abort_event(clip->_sdl->context()), clip->_event };

			const UINT32 timeout =
			    freerdp_settings_get_uint32(clip->_sdl->context()->settings, FreeRDP_TcpAckTimeout);
			DWORD status = WaitForMultipleObjects(ARRAYSIZE(hdl), hdl, FALSE, timeout);

			if (status != WAIT_OBJECT_0 + 1)
			{
				std::scoped_lock lock(clip->_lock);
				clip->_reading_server = false;
				if (!clip->_request_queue.empty())
					clip->_request_queue.pop();

				if (status == WAIT_TIMEOUT)
					WLog_Print(clip->_log, WLOG_ERROR,
					           "no reply in 10 seconds, returning empty content");

				return nullptr;
			}
		}

		{
			ClipboardLockGuard systemlock(clip->_system);
			std::scoped_lock lock(clip->_lock);
			if (clip->_request_queue.empty())
			{
				clip->_reading_server = false;
				return nullptr;
			}
			auto request = clip->_request_queue.front();
			clip->_request_queue.pop();

			if (clip->_request_queue.empty())
				std::ignore = ResetEvent(clip->_event);

			if (request.success())
			{
				clip->_reading_server = false;
				auto mimeFormatID = ClipboardRegisterFormat(clip->_system, mime_type);
				auto data = ClipboardGetData(clip->_system, mimeFormatID, &len);
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

			if (attempt >= ARRAYSIZE(retryDelays))
			{
				clip->_reading_server = false;
				return nullptr;
			}
			WLog_Print(clip->_log, WLOG_DEBUG,
			           "server could not provide %s, retrying in %" PRIu32 " ms", mime_type,
			           retryDelays[attempt]);
		}

		if (WaitForSingleObject(freerdp_abort_event(clip->_sdl->context()), retryDelays[attempt]) !=
		    WAIT_TIMEOUT)
		{
			std::scoped_lock lock(clip->_lock);
			clip->_reading_server = false;
			return nullptr;
		}
	}
}

void sdlClip::ClipCleanCb(void* userdata)
{
	auto clip = static_cast<sdlClip*>(userdata);
	WINPR_ASSERT(clip);
	ClipboardLockGuard give_me_a_name(clip->_system);
	std::scoped_lock lock(clip->_lock);
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

CliprdrFormat::CliprdrFormat(uint32_t formatID, const char* formatName) : _formatID(formatID)
{
	if (formatName)
		_formatName = formatName;
}

uint32_t CliprdrFormat::formatId() const
{
	return _formatID;
}

const char* CliprdrFormat::formatName() const
{
	if (_formatName.empty())
		return nullptr;
	return _formatName.c_str();
}
