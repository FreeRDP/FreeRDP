/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance Virtual Channel - common components
 *
 * Copyright 2024 Armin Novak <armin.novak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "remdesk_common.h"

#include <freerdp/channels/log.h>
#define TAG CHANNELS_TAG("remdesk.common")

UINT remdesk_write_channel_header(wStream* s, const REMDESK_CHANNEL_HEADER* header)
{
	WCHAR ChannelNameW[32] = { 0 };

	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	for (size_t index = 0; index < 32; index++)
	{
		ChannelNameW[index] = (WCHAR)header->ChannelName[index];
	}

	const size_t ChannelNameLen =
	    (strnlen(header->ChannelName, sizeof(header->ChannelName)) + 1) * 2;
	WINPR_ASSERT(ChannelNameLen <= ARRAYSIZE(header->ChannelName));

	Stream_Write_UINT32(s, (UINT32)ChannelNameLen); /* ChannelNameLen (4 bytes) */
	Stream_Write_UINT32(s, header->DataLength);     /* DataLen (4 bytes) */
	Stream_Write(s, ChannelNameW, ChannelNameLen);  /* ChannelName (variable) */
	return CHANNEL_RC_OK;
}

UINT remdesk_write_ctl_header(wStream* s, const REMDESK_CTL_HEADER* ctlHeader)
{
	WINPR_ASSERT(ctlHeader);
	const UINT error = remdesk_write_channel_header(s, &ctlHeader->ch);

	if (error != 0)
	{
		WLog_ERR(TAG, "remdesk_write_channel_header failed with error %" PRIu32 "!", error);
		return error;
	}

	Stream_Write_UINT32(s, ctlHeader->msgType); /* msgType (4 bytes) */
	return CHANNEL_RC_OK;
}

UINT remdesk_read_channel_header(wStream* s, REMDESK_CHANNEL_HEADER* header)
{
	UINT32 ChannelNameLen = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Read_UINT32(s, ChannelNameLen);     /* ChannelNameLen (4 bytes) */
	Stream_Read_UINT32(s, header->DataLength); /* DataLen (4 bytes) */

	if (ChannelNameLen > 64)
	{
		WLog_ERR(TAG, "ChannelNameLen > 64!");
		return ERROR_INVALID_DATA;
	}

	if ((ChannelNameLen % 2) != 0)
	{
		WLog_ERR(TAG, "(ChannelNameLen %% 2) != 0!");
		return ERROR_INVALID_DATA;
	}

	if (Stream_Read_UTF16_String_As_UTF8_Buffer(s, ChannelNameLen / sizeof(WCHAR),
	                                            header->ChannelName,
	                                            ARRAYSIZE(header->ChannelName)) < 0)
		return ERROR_INVALID_DATA;

	return CHANNEL_RC_OK;
}

UINT remdesk_prepare_ctl_header(REMDESK_CTL_HEADER* ctlHeader, UINT32 msgType, size_t msgSize)
{
	WINPR_ASSERT(ctlHeader);

	if (msgSize > UINT32_MAX - 4)
		return ERROR_INVALID_PARAMETER;

	ctlHeader->msgType = msgType;
	(void)sprintf_s(ctlHeader->ch.ChannelName, ARRAYSIZE(ctlHeader->ch.ChannelName),
	                REMDESK_CHANNEL_CTL_NAME);
	ctlHeader->ch.DataLength = (UINT32)(4UL + msgSize);
	return CHANNEL_RC_OK;
}
