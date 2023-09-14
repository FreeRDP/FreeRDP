/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Clipboard Channel
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <vector>
#include <map>
#include <atomic>

#include <freerdp/client/cliprdr.h>
#include <freerdp/client/client_cliprdr_file.h>

#include "../sdl_types.hpp"

class SdlCliprdrContext
{
  public:
    SdlCliprdrContext(SdlContext* sdl);
	virtual ~SdlCliprdrContext();

	virtual bool init(CliprdrClientContext* clip);
	virtual bool uninit(CliprdrClientContext* clip);

  private:
	virtual UINT MonitorReady(const CLIPRDR_MONITOR_READY* monitorReady);
	virtual UINT ServerCapabilities(const CLIPRDR_CAPABILITIES* capabilities);
	virtual UINT ServerFormatList(const CLIPRDR_FORMAT_LIST* formatList);
	virtual UINT ServerFormatListResponse(const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
	virtual UINT ServerFormatDataRequest(const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
	virtual UINT ServerFormatDataResponse(const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);

	virtual UINT send_client_capabilities();

	virtual UINT send_client_format_list(bool force);
	virtual UINT send_client_format_list_response(bool status);

	virtual UINT send_format_list(const std::vector<CLIPRDR_FORMAT>& formats, bool force);

	virtual bool clipboard_changed(const std::vector<CLIPRDR_FORMAT>& formats);

	virtual UINT send_data_response(const CLIPRDR_FORMAT* format, const BYTE* data, size_t size);

	void clear_cached_data();

  private:
	static UINT MonitorReady(CliprdrClientContext* context,
	                         const CLIPRDR_MONITOR_READY* monitorReady);
	static UINT ServerCapabilities(CliprdrClientContext* context,
	                               const CLIPRDR_CAPABILITIES* capabilities);
	static UINT ServerFormatList(CliprdrClientContext* context,
	                             const CLIPRDR_FORMAT_LIST* formatList);
	static UINT ServerFormatListResponse(CliprdrClientContext* context,
	                                     const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
	static UINT ServerFormatDataRequest(CliprdrClientContext* context,
	                                    const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
	static UINT ServerFormatDataResponse(CliprdrClientContext* context,
	                                     const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);

  protected:
	SdlContext* _sdl;
	CliprdrClientContext* _clip;
	CliprdrFileContext* _file;

	std::atomic<bool> _sync;
	std::atomic<int64_t> _requestedFormatId;

	std::vector<CLIPRDR_FORMAT> _current_formats;
	std::vector<CLIPRDR_FORMAT> _sent_formats;

	std::map<size_t, std::vector<BYTE>> _cache;
	std::map<size_t, std::vector<BYTE>> _raw_cache;
};
