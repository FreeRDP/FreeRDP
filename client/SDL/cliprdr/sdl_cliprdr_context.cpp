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
#include <algorithm>

#include <winpr/assert.h>

#include <freerdp/client/cliprdr.h>
#include <freerdp/client/client_cliprdr_file.h>

#include "sdl_cliprdr_context.hpp"

#include "../sdl_freerdp.hpp"

bool operator==(const CLIPRDR_FORMAT& lhs, const CLIPRDR_FORMAT& rhs)
{
	if (lhs.formatId != rhs.formatId)
		return false;
	if (!lhs.formatName && rhs.formatName)
		return false;
	if (lhs.formatName && !rhs.formatName)
		return false;
	if (!lhs.formatName && !rhs.formatName)
		return true;

	return strcmp(lhs.formatName, rhs.formatName) == 0;
}

bool operator<(const CLIPRDR_FORMAT& lhs, const CLIPRDR_FORMAT& rhs)
{
	if (lhs.formatId < rhs.formatId)
		return true;
	if (lhs.formatId > rhs.formatId)
		return false;
	if (!lhs.formatName && rhs.formatName)
		return true;
	if (lhs.formatName && !rhs.formatName)
		return false;
	if (!lhs.formatName && !rhs.formatName)
		return false;

	return strcmp(lhs.formatName, rhs.formatName) < 0;
}

bool operator>(const CLIPRDR_FORMAT& lhs, const CLIPRDR_FORMAT& rhs)
{
	if (lhs.formatId > rhs.formatId)
		return true;
	if (lhs.formatId < rhs.formatId)
		return false;
	if (!lhs.formatName && rhs.formatName)
		return false;
	if (lhs.formatName && !rhs.formatName)
		return true;
	if (!lhs.formatName && !rhs.formatName)
		return false;

	return strcmp(lhs.formatName, rhs.formatName) > 0;
}

SdlCliprdrContext::SdlCliprdrContext(SdlContext* sdl)
    : _sdl(sdl), _file(cliprdr_file_context_new(this)), _sync(false), _requestedFormatId(-1)
{
	WINPR_ASSERT(_sdl);
}

SdlCliprdrContext::~SdlCliprdrContext()
{
	cliprdr_file_context_free(_file);
}

bool SdlCliprdrContext::init(CliprdrClientContext* clip)
{
	_clip = clip;
	WINPR_ASSERT(_clip);
	_clip->MonitorReady = SdlCliprdrContext::MonitorReady;
	_clip->ServerCapabilities = SdlCliprdrContext::ServerCapabilities;
	_clip->ServerFormatList = SdlCliprdrContext::ServerFormatList;
	_clip->ServerFormatListResponse = SdlCliprdrContext::ServerFormatListResponse;
	_clip->ServerFormatDataRequest = SdlCliprdrContext::ServerFormatDataRequest;
	_clip->ServerFormatDataResponse = SdlCliprdrContext::ServerFormatDataResponse;

	if (!cliprdr_file_context_init(_file, clip))
		return false;
	return true;
}

bool SdlCliprdrContext::uninit(CliprdrClientContext* clip)
{
	if (!cliprdr_file_context_uninit(_file, clip))
		return false;
	return true;
}

UINT SdlCliprdrContext::send_client_capabilities()
{
	CLIPRDR_CAPABILITIES capabilities = {};
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = {};

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = reinterpret_cast<CLIPRDR_CAPABILITY_SET*>(&generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	generalCapabilitySet.generalFlags |= cliprdr_file_context_current_flags(_file);

	WINPR_ASSERT(_clip);
	WINPR_ASSERT(_clip->ClientCapabilities);
	return _clip->ClientCapabilities(_clip, &capabilities);
}

UINT SdlCliprdrContext::send_client_format_list(bool force)
{
	std::sort(_current_formats.begin(), _current_formats.end());
	return send_format_list(_current_formats, force);
}

UINT SdlCliprdrContext::send_client_format_list_response(bool status)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse = {};

	formatListResponse.common.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.common.msgFlags = status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	formatListResponse.common.dataLen = 0;

	WINPR_ASSERT(_clip);
	WINPR_ASSERT(_clip->ClientFormatListResponse);
	return _clip->ClientFormatListResponse(_clip, &formatListResponse);
}

UINT SdlCliprdrContext::send_format_list(const std::vector<CLIPRDR_FORMAT>& formats, bool force)
{
	if (!force && !clipboard_changed(formats))
		return CHANNEL_RC_OK;

	const CLIPRDR_HEADER header = { CB_FORMAT_LIST, CB_RESPONSE_OK, 0 };
	const CLIPRDR_FORMAT_LIST formatList = { header, static_cast<UINT32>(formats.size()),
		                                     const_cast<CLIPRDR_FORMAT*>(formats.data()) };

	_sent_formats = formats;

	/* Ensure all pending requests are answered. */
	send_data_response(nullptr, nullptr, 0);

	clear_cached_data();

	WINPR_ASSERT(_clip);
	WINPR_ASSERT(_clip->ClientFormatList);
	return _clip->ClientFormatList(_clip, &formatList);
}

bool SdlCliprdrContext::clipboard_changed(const std::vector<CLIPRDR_FORMAT>& formats)
{
	return formats == _sent_formats;
}

UINT SdlCliprdrContext::send_data_response(const CLIPRDR_FORMAT* format, const BYTE* data,
                                           size_t size)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response = {};

	/* No request currently pending, do not send a response. */
	if (_requestedFormatId.exchange(-1) < 0)
		return CHANNEL_RC_OK;

	response.common.msgFlags = (data) ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	response.common.dataLen = static_cast<UINT32>(size);
	response.requestedFormatData = data;

	WINPR_ASSERT(_clip);
	WINPR_ASSERT(_clip->ClientFormatDataResponse);
	return _clip->ClientFormatDataResponse(_clip, &response);
}

void SdlCliprdrContext::clear_cached_data()
{
	_cache.clear();
	_raw_cache.clear();
}

UINT SdlCliprdrContext::MonitorReady(CliprdrClientContext* context,
                                     const CLIPRDR_MONITOR_READY* monitorReady)
{
	WINPR_ASSERT(context);
	auto file = reinterpret_cast<CliprdrFileContext*>(context->custom);
	auto clipboard = reinterpret_cast<SdlCliprdrContext*>(cliprdr_file_context_get_context(file));
	if (!clipboard)
		return ERROR_INVALID_PARAMETER;
	return clipboard->MonitorReady(monitorReady);
}

UINT SdlCliprdrContext::ServerCapabilities(CliprdrClientContext* context,
                                           const CLIPRDR_CAPABILITIES* capabilities)
{

	WINPR_ASSERT(context);
	auto file = reinterpret_cast<CliprdrFileContext*>(context->custom);
	auto clipboard = reinterpret_cast<SdlCliprdrContext*>(cliprdr_file_context_get_context(file));
	if (!clipboard)
		return ERROR_INVALID_PARAMETER;
	return clipboard->ServerCapabilities(capabilities);
}

UINT SdlCliprdrContext::ServerFormatList(CliprdrClientContext* context,
                                         const CLIPRDR_FORMAT_LIST* formatList)
{
	WINPR_ASSERT(context);
	auto file = reinterpret_cast<CliprdrFileContext*>(context->custom);
	auto clipboard = reinterpret_cast<SdlCliprdrContext*>(cliprdr_file_context_get_context(file));
	if (!clipboard)
		return ERROR_INVALID_PARAMETER;
	return clipboard->ServerFormatList(formatList);
}

UINT SdlCliprdrContext::ServerFormatListResponse(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	WINPR_ASSERT(context);
	auto file = reinterpret_cast<CliprdrFileContext*>(context->custom);
	auto clipboard = reinterpret_cast<SdlCliprdrContext*>(cliprdr_file_context_get_context(file));
	if (!clipboard)
		return ERROR_INVALID_PARAMETER;
	return clipboard->ServerFormatListResponse(formatListResponse);
}

UINT SdlCliprdrContext::ServerFormatDataRequest(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	WINPR_ASSERT(context);
	auto file = reinterpret_cast<CliprdrFileContext*>(context->custom);
	auto clipboard = reinterpret_cast<SdlCliprdrContext*>(cliprdr_file_context_get_context(file));
	if (!clipboard)
		return ERROR_INVALID_PARAMETER;
	return clipboard->ServerFormatDataRequest(formatDataRequest);
}

UINT SdlCliprdrContext::ServerFormatDataResponse(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	WINPR_ASSERT(context);
	auto file = reinterpret_cast<CliprdrFileContext*>(context->custom);
	auto clipboard = reinterpret_cast<SdlCliprdrContext*>(cliprdr_file_context_get_context(file));
	if (!clipboard)
		return ERROR_INVALID_PARAMETER;
	return clipboard->ServerFormatDataResponse(formatDataResponse);
}

UINT SdlCliprdrContext::MonitorReady(const CLIPRDR_MONITOR_READY* monitorReady)
{
	WINPR_ASSERT(monitorReady);
	WINPR_ASSERT(!_sync);

	auto ret = send_client_capabilities();
	if (ret != CHANNEL_RC_OK)
		return ret;

	_sent_formats.clear();

	ret = send_client_format_list(true);
	if (ret != CHANNEL_RC_OK)
		return ret;

	_sync = true;
	return CHANNEL_RC_OK;
}

UINT SdlCliprdrContext::ServerCapabilities(const CLIPRDR_CAPABILITIES* capabilities)
{
	WINPR_ASSERT(capabilities);
	WINPR_ASSERT(!_sync);

	auto capsPtr = reinterpret_cast<const BYTE*>(capabilities->capabilitySets);
	WINPR_ASSERT(capsPtr);

	cliprdr_file_context_remote_set_flags(_file, 0);

	for (UINT32 i = 0; i < capabilities->cCapabilitiesSets; i++)
	{
		auto caps = reinterpret_cast<const CLIPRDR_CAPABILITY_SET*>(capsPtr);

		if (caps->capabilitySetType == CB_CAPSTYPE_GENERAL)
		{
			auto generalCaps = reinterpret_cast<const CLIPRDR_GENERAL_CAPABILITY_SET*>(caps);

			cliprdr_file_context_remote_set_flags(_file, generalCaps->generalFlags);
		}

		capsPtr += caps->capabilitySetLength;
	}

	return CHANNEL_RC_OK;
}

UINT SdlCliprdrContext::ServerFormatList(const CLIPRDR_FORMAT_LIST* formatList)
{
	WINPR_ASSERT(formatList);
	WINPR_ASSERT(_sync);
	return ERROR_INTERNAL_ERROR;
}

UINT SdlCliprdrContext::ServerFormatListResponse(
    const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	WINPR_ASSERT(formatListResponse);
	WINPR_ASSERT(_sync);

	// TODO
	return CHANNEL_RC_OK;
}

UINT SdlCliprdrContext::ServerFormatDataRequest(
    const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	WINPR_ASSERT(formatDataRequest);
	WINPR_ASSERT(_sync);

	// TODO
	return CHANNEL_RC_OK;
}

UINT SdlCliprdrContext::ServerFormatDataResponse(
    const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	WINPR_ASSERT(formatDataResponse);
	WINPR_ASSERT(_sync);

	// TODO
	return CHANNEL_RC_OK;
}
