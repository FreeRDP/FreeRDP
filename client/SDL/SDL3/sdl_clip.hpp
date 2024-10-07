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

#pragma once

#include <utility>
#include <vector>
#include <atomic>
#include <queue>
#include <map>

#include <winpr/wtypes.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/client_cliprdr_file.h>
#include <SDL3/SDL.h>

#include "sdl_types.hpp"
#include "sdl_utils.hpp"

/** @brief a clipboard format request */
class ClipRequest
{
  public:
	ClipRequest(UINT32 format, const std::string& mime);
	ClipRequest(const ClipRequest& other) = default;
	ClipRequest(ClipRequest&& other) = default;
	~ClipRequest() = default;

	ClipRequest& operator=(const ClipRequest& other) = delete;
	ClipRequest& operator=(ClipRequest&& other) = delete;

	[[nodiscard]] uint32_t format() const;
	[[nodiscard]] std::string formatstr() const;
	[[nodiscard]] std::string mime() const;
	[[nodiscard]] bool success() const;
	void setSuccess(bool status);

  private:
	uint32_t _format;
	std::string _mime;
	bool _success;
};

class CliprdrFormat
{
  public:
	CliprdrFormat(uint32_t formatID, const char* formatName) : _formatID(formatID)
	{
		if (formatName)
			_formatName = formatName;
	}

	[[nodiscard]] uint32_t formatId() const
	{
		return _formatID;
	}

	[[nodiscard]] const char* formatName() const
	{
		if (_formatName.empty())
			return nullptr;
		return _formatName.c_str();
	}

  private:
	uint32_t _formatID;
	std::string _formatName;
};

/** @brief object that handles clipboard context for the SDL3 client */
class sdlClip
{
  public:
	explicit sdlClip(SdlContext* sdl);
	virtual ~sdlClip();

	BOOL init(CliprdrClientContext* clip);
	BOOL uninit(CliprdrClientContext* clip);

	bool handle_update(const SDL_ClipboardEvent& ev);

  private:
	UINT SendClientCapabilities();
	void clearServerFormats();
	UINT SendFormatListResponse(BOOL status);
	UINT SendDataResponse(const BYTE* data, size_t size);
	UINT SendDataRequest(uint32_t formatID, const std::string& mime);

	std::string getServerFormat(uint32_t id);
	uint32_t serverIdForMime(const std::string& mime);

	static UINT MonitorReady(CliprdrClientContext* context,
	                         const CLIPRDR_MONITOR_READY* monitorReady);

	static UINT ReceiveServerCapabilities(CliprdrClientContext* context,
	                                      const CLIPRDR_CAPABILITIES* capabilities);
	static UINT ReceiveServerFormatList(CliprdrClientContext* context,
	                                    const CLIPRDR_FORMAT_LIST* formatList);
	static UINT ReceiveFormatListResponse(CliprdrClientContext* context,
	                                      const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
	static std::shared_ptr<BYTE> ReceiveFormatDataRequestHandle(
	    sdlClip* clipboard, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest, uint32_t& len);
	static UINT ReceiveFormatDataRequest(CliprdrClientContext* context,
	                                     const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
	static UINT ReceiveFormatDataResponse(CliprdrClientContext* context,
	                                      const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);

	static const void* SDLCALL ClipDataCb(void* userdata, const char* mime_type, size_t* size);
	static void SDLCALL ClipCleanCb(void* userdata);

	static bool mime_is_file(const std::string& mime);
	static bool mime_is_text(const std::string& mime);
	static bool mime_is_image(const std::string& mime);
	static bool mime_is_html(const std::string& mime);

	SdlContext* _sdl = nullptr;
	CliprdrFileContext* _file = nullptr;
	CliprdrClientContext* _ctx = nullptr;
	wLog* _log = nullptr;
	wClipboard* _system = nullptr;
	std::atomic<bool> _sync = false;
	HANDLE _event;

	std::vector<CliprdrFormat> _serverFormats;
	CriticalSection _lock;

	std::queue<ClipRequest> _request_queue;

	struct cache_entry
	{
		cache_entry(size_t len, std::shared_ptr<void> p) : size(len), ptr(std::move(p))
		{
		}

		size_t size;
		std::shared_ptr<void> ptr;
	};
	std::map<std::string, cache_entry> _cache_data;
};
