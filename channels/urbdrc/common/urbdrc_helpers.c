/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server USB redirection channel - helper functions
 *
 * Copyright 2019 Armin Novak <armin.novak@thincast.com>
 * Copyright 2019 Thincast Technologies GmbH
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
#include "urbdrc_helpers.h"
#include "urbdrc_types.h"

const char* mask_to_string(UINT32 mask)
{
	switch (mask)
	{
		case STREAM_ID_NONE:
			return "STREAM_ID_NONE";

		case STREAM_ID_PROXY:
			return "STREAM_ID_PROXY";

		case STREAM_ID_STUB:
			return "STREAM_ID_STUB";

		default:
			return "UNKNOWN";
	}
}
const char* interface_to_string(UINT32 id)
{
	switch (id)
	{
		case CAPABILITIES_NEGOTIATOR:
			return "CAPABILITIES_NEGOTIATOR";

		case SERVER_CHANNEL_NOTIFICATION:
			return "SERVER_CHANNEL_NOTIFICATION";

		case CLIENT_CHANNEL_NOTIFICATION:
			return "CLIENT_CHANNEL_NOTIFICATION";

		default:
			return "DEVICE_MESSAGE";
	}
}

static const char* call_to_string_none(BOOL client, UINT32 interfaceId, UINT32 functionId)
{
	WINPR_UNUSED(interfaceId);

	if (client)
		return "RIM_EXCHANGE_CAPABILITY_RESPONSE  [none |client]";
	else
	{
		switch (functionId)
		{
			case RIM_EXCHANGE_CAPABILITY_REQUEST:
				return "RIM_EXCHANGE_CAPABILITY_REQUEST   [none |server]";

			case RIMCALL_RELEASE:
				return "RIMCALL_RELEASE                   [none |server]";

			case RIMCALL_QUERYINTERFACE:
				return "RIMCALL_QUERYINTERFACE            [none |server]";

			default:
				return "UNKNOWN                           [none |server]";
		}
	}
}

static const char* call_to_string_proxy_server(UINT32 functionId)
{
	switch (functionId)
	{
		case QUERY_DEVICE_TEXT:
			return "QUERY_DEVICE_TEXT                 [proxy|server]";

		case INTERNAL_IO_CONTROL:
			return "INTERNAL_IO_CONTROL               [proxy|server]";

		case IO_CONTROL:
			return "IO_CONTROL                        [proxy|server]";

		case REGISTER_REQUEST_CALLBACK:
			return "REGISTER_REQUEST_CALLBACK         [proxy|server]";

		case CANCEL_REQUEST:
			return "CANCEL_REQUEST                    [proxy|server]";

		case RETRACT_DEVICE:
			return "RETRACT_DEVICE                    [proxy|server]";

		case TRANSFER_IN_REQUEST:
			return "TRANSFER_IN_REQUEST               [proxy|server]";

		default:
			return "UNKNOWN                           [proxy|server]";
	}
}

static const char* call_to_string_proxy_client(UINT32 functionId)
{
	switch (functionId)
	{
		case URB_COMPLETION_NO_DATA:
			return "URB_COMPLETION_NO_DATA            [proxy|client]";

		case URB_COMPLETION:
			return "URB_COMPLETION                    [proxy|client]";

		case IOCONTROL_COMPLETION:
			return "IOCONTROL_COMPLETION              [proxy|client]";

		case TRANSFER_OUT_REQUEST:
			return "TRANSFER_OUT_REQUEST              [proxy|client]";

		default:
			return "UNKNOWN                           [proxy|client]";
	}
}

static const char* call_to_string_proxy(BOOL client, UINT32 interfaceId, UINT32 functionId)
{
	switch (interfaceId & INTERFACE_ID_MASK)
	{
		case CLIENT_DEVICE_SINK:
			switch (functionId)
			{
				case ADD_VIRTUAL_CHANNEL:
					return "ADD_VIRTUAL_CHANNEL               [proxy|sink  ]";

				case ADD_DEVICE:
					return "ADD_DEVICE                        [proxy|sink  ]";
				case RIMCALL_RELEASE:
					return "RIMCALL_RELEASE                   [proxy|sink  ]";

				case RIMCALL_QUERYINTERFACE:
					return "RIMCALL_QUERYINTERFACE            [proxy|sink  ]";
				default:
					return "UNKNOWN                           [proxy|sink  ]";
			}

		case SERVER_CHANNEL_NOTIFICATION:
			switch (functionId)
			{
				case CHANNEL_CREATED:
					return "CHANNEL_CREATED                   [proxy|server]";

				case RIMCALL_RELEASE:
					return "RIMCALL_RELEASE                   [proxy|server]";

				case RIMCALL_QUERYINTERFACE:
					return "RIMCALL_QUERYINTERFACE            [proxy|server]";

				default:
					return "UNKNOWN                           [proxy|server]";
			}

		case CLIENT_CHANNEL_NOTIFICATION:
			switch (functionId)
			{
				case CHANNEL_CREATED:
					return "CHANNEL_CREATED                   [proxy|client]";
				case RIMCALL_RELEASE:
					return "RIMCALL_RELEASE                   [proxy|client]";
				case RIMCALL_QUERYINTERFACE:
					return "RIMCALL_QUERYINTERFACE            [proxy|client]";
				default:
					return "UNKNOWN                           [proxy|client]";
			}

		default:
			if (client)
				return call_to_string_proxy_client(functionId);
			else
				return call_to_string_proxy_server(functionId);
	}
}

static const char* call_to_string_stub(BOOL client, UINT32 interfaceId, UINT32 functionId)
{
	return "QUERY_DEVICE_TEXT_RSP             [stub  |client]";
}

const char* call_to_string(BOOL client, UINT32 interface, UINT32 functionId)
{
	const UINT32 mask = (interface & STREAM_ID_MASK) >> 30;
	const UINT32 interfaceId = interface & INTERFACE_ID_MASK;

	switch (mask)
	{
		case STREAM_ID_NONE:
			return call_to_string_none(client, interfaceId, functionId);

		case STREAM_ID_PROXY:
			return call_to_string_proxy(client, interfaceId, functionId);

		case STREAM_ID_STUB:
			return call_to_string_stub(client, interfaceId, functionId);

		default:
			return "UNKNOWN[mask]";
	}
}

const char* urb_function_string(UINT16 urb)
{
	switch (urb)
	{
		case TS_URB_SELECT_CONFIGURATION:
			return "TS_URB_SELECT_CONFIGURATION";

		case TS_URB_SELECT_INTERFACE:
			return "TS_URB_SELECT_INTERFACE";

		case TS_URB_PIPE_REQUEST:
			return "TS_URB_PIPE_REQUEST";

		case TS_URB_TAKE_FRAME_LENGTH_CONTROL:
			return "TS_URB_TAKE_FRAME_LENGTH_CONTROL";

		case TS_URB_RELEASE_FRAME_LENGTH_CONTROL:
			return "TS_URB_RELEASE_FRAME_LENGTH_CONTROL";

		case TS_URB_GET_FRAME_LENGTH:
			return "TS_URB_GET_FRAME_LENGTH";

		case TS_URB_SET_FRAME_LENGTH:
			return "TS_URB_SET_FRAME_LENGTH";

		case TS_URB_GET_CURRENT_FRAME_NUMBER:
			return "TS_URB_GET_CURRENT_FRAME_NUMBER";

		case TS_URB_CONTROL_TRANSFER:
			return "TS_URB_CONTROL_TRANSFER";

		case TS_URB_BULK_OR_INTERRUPT_TRANSFER:
			return "TS_URB_BULK_OR_INTERRUPT_TRANSFER";

		case TS_URB_ISOCH_TRANSFER:
			return "TS_URB_ISOCH_TRANSFER";

		case TS_URB_GET_DESCRIPTOR_FROM_DEVICE:
			return "TS_URB_GET_DESCRIPTOR_FROM_DEVICE";

		case TS_URB_SET_DESCRIPTOR_TO_DEVICE:
			return "TS_URB_SET_DESCRIPTOR_TO_DEVICE";

		case TS_URB_SET_FEATURE_TO_DEVICE:
			return "TS_URB_SET_FEATURE_TO_DEVICE";

		case TS_URB_SET_FEATURE_TO_INTERFACE:
			return "TS_URB_SET_FEATURE_TO_INTERFACE";

		case TS_URB_SET_FEATURE_TO_ENDPOINT:
			return "TS_URB_SET_FEATURE_TO_ENDPOINT";

		case TS_URB_CLEAR_FEATURE_TO_DEVICE:
			return "TS_URB_CLEAR_FEATURE_TO_DEVICE";

		case TS_URB_CLEAR_FEATURE_TO_INTERFACE:
			return "TS_URB_CLEAR_FEATURE_TO_INTERFACE";

		case TS_URB_CLEAR_FEATURE_TO_ENDPOINT:
			return "TS_URB_CLEAR_FEATURE_TO_ENDPOINT";

		case TS_URB_GET_STATUS_FROM_DEVICE:
			return "TS_URB_GET_STATUS_FROM_DEVICE";

		case TS_URB_GET_STATUS_FROM_INTERFACE:
			return "TS_URB_GET_STATUS_FROM_INTERFACE";

		case TS_URB_GET_STATUS_FROM_ENDPOINT:
			return "TS_URB_GET_STATUS_FROM_ENDPOINT";

		case TS_URB_RESERVED_0X0016:
			return "TS_URB_RESERVED_0X0016";

		case TS_URB_VENDOR_DEVICE:
			return "TS_URB_VENDOR_DEVICE";

		case TS_URB_VENDOR_INTERFACE:
			return "TS_URB_VENDOR_INTERFACE";

		case TS_URB_VENDOR_ENDPOINT:
			return "TS_URB_VENDOR_ENDPOINT";

		case TS_URB_CLASS_DEVICE:
			return "TS_URB_CLASS_DEVICE";

		case TS_URB_CLASS_INTERFACE:
			return "TS_URB_CLASS_INTERFACE";

		case TS_URB_CLASS_ENDPOINT:
			return "TS_URB_CLASS_ENDPOINT";

		case TS_URB_RESERVE_0X001D:
			return "TS_URB_RESERVE_0X001D";

		case TS_URB_SYNC_RESET_PIPE_AND_CLEAR_STALL:
			return "TS_URB_SYNC_RESET_PIPE_AND_CLEAR_STALL";

		case TS_URB_CLASS_OTHER:
			return "TS_URB_CLASS_OTHER";

		case TS_URB_VENDOR_OTHER:
			return "TS_URB_VENDOR_OTHER";

		case TS_URB_GET_STATUS_FROM_OTHER:
			return "TS_URB_GET_STATUS_FROM_OTHER";

		case TS_URB_CLEAR_FEATURE_TO_OTHER:
			return "TS_URB_CLEAR_FEATURE_TO_OTHER";

		case TS_URB_SET_FEATURE_TO_OTHER:
			return "TS_URB_SET_FEATURE_TO_OTHER";

		case TS_URB_GET_DESCRIPTOR_FROM_ENDPOINT:
			return "TS_URB_GET_DESCRIPTOR_FROM_ENDPOINT";

		case TS_URB_SET_DESCRIPTOR_TO_ENDPOINT:
			return "TS_URB_SET_DESCRIPTOR_TO_ENDPOINT";

		case TS_URB_CONTROL_GET_CONFIGURATION_REQUEST:
			return "TS_URB_CONTROL_GET_CONFIGURATION_REQUEST";

		case TS_URB_CONTROL_GET_INTERFACE_REQUEST:
			return "TS_URB_CONTROL_GET_INTERFACE_REQUEST";

		case TS_URB_GET_DESCRIPTOR_FROM_INTERFACE:
			return "TS_URB_GET_DESCRIPTOR_FROM_INTERFACE";

		case TS_URB_SET_DESCRIPTOR_TO_INTERFACE:
			return "TS_URB_SET_DESCRIPTOR_TO_INTERFACE";

		case TS_URB_GET_OS_FEATURE_DESCRIPTOR_REQUEST:
			return "TS_URB_GET_OS_FEATURE_DESCRIPTOR_REQUEST";

		case TS_URB_RESERVE_0X002B:
			return "TS_URB_RESERVE_0X002B";

		case TS_URB_RESERVE_0X002C:
			return "TS_URB_RESERVE_0X002C";

		case TS_URB_RESERVE_0X002D:
			return "TS_URB_RESERVE_0X002D";

		case TS_URB_RESERVE_0X002E:
			return "TS_URB_RESERVE_0X002E";

		case TS_URB_RESERVE_0X002F:
			return "TS_URB_RESERVE_0X002F";

		case TS_URB_SYNC_RESET_PIPE:
			return "TS_URB_SYNC_RESET_PIPE";

		case TS_URB_SYNC_CLEAR_STALL:
			return "TS_URB_SYNC_CLEAR_STALL";

		case TS_URB_CONTROL_TRANSFER_EX:
			return "TS_URB_CONTROL_TRANSFER_EX";

		default:
			return "UNKNOWN";
	}
}

void urbdrc_dump_message(wLog* log, BOOL client, BOOL write, wStream* s)
{
	const char* type = write ? "WRITE" : "READ";
	UINT32 InterfaceId, MessageId, FunctionId;
	size_t length, pos;

	pos = Stream_GetPosition(s);
	if (write)
	{
		length = Stream_GetPosition(s);
		Stream_SetPosition(s, 0);
	}
	else
		length = Stream_GetRemainingLength(s);

	if (length < 12)
		return;

	Stream_Read_UINT32(s, InterfaceId);
	Stream_Read_UINT32(s, MessageId);
	Stream_Read_UINT32(s, FunctionId);
	Stream_SetPosition(s, pos);

	WLog_Print(log, WLOG_DEBUG,
	           "[%-5s] %s [%08" PRIx32 "] InterfaceId=%08" PRIx32 ", MessageId=%08" PRIx32
	           ", FunctionId=%08" PRIx32 ", length=%" PRIdz,
	           type, call_to_string(client, InterfaceId, FunctionId), FunctionId, InterfaceId,
	           MessageId, FunctionId, length);
}
