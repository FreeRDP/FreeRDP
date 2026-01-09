/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Capture Virtual Channel Extension
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#if defined(CHANNEL_RDPECAM)
#include <stdbool.h>
#include <winpr/wtypes.h>
#include <freerdp/channels/rdpecam.h>

WINPR_ATTR_FORMAT_ARG(5, 6)
static inline void rdpecam_PrintWarning(wLog* log, const char* file, const char* fkt, size_t line,
                                        WINPR_FORMAT_ARG const char* fmt, ...)
{
	const DWORD level = WLOG_WARN;
	va_list ap;

	va_start(ap, fmt);
	if (WLog_IsLevelActive(log, level))
		WLog_PrintTextMessageVA(log, level, line, file, fkt, fmt, ap);
	va_end(ap);
}

/** @brief check input data is a valid \ref CAM_MSG_ID value
 *
 *  @param id The message id to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_messageId(id) \
	rdpecam_valid_messageId_((id), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_messageId_(UINT8 id, wLog* log, const char* file, const char* fkt,
                                            size_t line)
{
	switch (id)
	{
		case CAM_MSG_ID_SuccessResponse:
		case CAM_MSG_ID_ErrorResponse:
		case CAM_MSG_ID_SelectVersionRequest:
		case CAM_MSG_ID_SelectVersionResponse:
		case CAM_MSG_ID_DeviceAddedNotification:
		case CAM_MSG_ID_DeviceRemovedNotification:
		case CAM_MSG_ID_ActivateDeviceRequest:
		case CAM_MSG_ID_DeactivateDeviceRequest:
		case CAM_MSG_ID_StreamListRequest:
		case CAM_MSG_ID_StreamListResponse:
		case CAM_MSG_ID_MediaTypeListRequest:
		case CAM_MSG_ID_MediaTypeListResponse:
		case CAM_MSG_ID_CurrentMediaTypeRequest:
		case CAM_MSG_ID_CurrentMediaTypeResponse:
		case CAM_MSG_ID_StartStreamsRequest:
		case CAM_MSG_ID_StopStreamsRequest:
		case CAM_MSG_ID_SampleRequest:
		case CAM_MSG_ID_SampleResponse:
		case CAM_MSG_ID_SampleErrorResponse:
		case CAM_MSG_ID_PropertyListRequest:
		case CAM_MSG_ID_PropertyListResponse:
		case CAM_MSG_ID_PropertyValueRequest:
		case CAM_MSG_ID_PropertyValueResponse:
		case CAM_MSG_ID_SetPropertyValueRequest:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line, "Invalid CAM_MSG_ID %" PRIu8, id);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_ERROR_CODE value
 *
 *  @param code The code to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_CamErrorCode(code) \
	rdpecam_valid_CamErrorCode_((code), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_CamErrorCode_(UINT32 code, wLog* log, const char* file,
                                               const char* fkt, size_t line)
{
	switch (code)
	{
		case CAM_ERROR_CODE_UnexpectedError:
		case CAM_ERROR_CODE_InvalidMessage:
		case CAM_ERROR_CODE_NotInitialized:
		case CAM_ERROR_CODE_InvalidRequest:
		case CAM_ERROR_CODE_InvalidStreamNumber:
		case CAM_ERROR_CODE_InvalidMediaType:
		case CAM_ERROR_CODE_OutOfMemory:
		case CAM_ERROR_CODE_ItemNotFound:
		case CAM_ERROR_CODE_SetNotFound:
		case CAM_ERROR_CODE_OperationNotSupported:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line, "Invalid CAM_ERROR_CODE %" PRIu32, code);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_STREAM_FRAME_SOURCE_TYPES value
 *
 *  @param val The value to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_CamStreamFrameSourceType(val) \
	rdpecam_valid_CamStreamFrameSourceType_((val), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_CamStreamFrameSourceType_(UINT16 val, wLog* log, const char* file,
                                                           const char* fkt, size_t line)
{
	switch (val)
	{
		case CAM_STREAM_FRAME_SOURCE_TYPE_Color:
		case CAM_STREAM_FRAME_SOURCE_TYPE_Infrared:
		case CAM_STREAM_FRAME_SOURCE_TYPE_Custom:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line,
			                     "Invalid CAM_STREAM_FRAME_SOURCE_TYPES %" PRIu16, val);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_STREAM_CATEGORY value
 *
 *  @param val The value to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_CamStreamCategory(val) \
	rdpecam_valid_CamStreamCategory_((val), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_CamStreamCategory_(UINT8 val, wLog* log, const char* file,
                                                    const char* fkt, size_t line)
{
	switch (val)
	{
		case CAM_STREAM_CATEGORY_Capture:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line, "Invalid CAM_STREAM_CATEGORY %" PRIu8, val);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_MEDIA_FORMAT value
 *
 *  @param val The value to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_CamMediaFormat(val) \
	rdpecam_valid_CamMediaFormat_((val), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_CamMediaFormat_(UINT8 val, wLog* log, const char* file,
                                                 const char* fkt, size_t line)
{
	switch (val)
	{
		case CAM_MEDIA_FORMAT_INVALID:
		case CAM_MEDIA_FORMAT_H264:
		case CAM_MEDIA_FORMAT_MJPG:
		case CAM_MEDIA_FORMAT_YUY2:
		case CAM_MEDIA_FORMAT_NV12:
		case CAM_MEDIA_FORMAT_I420:
		case CAM_MEDIA_FORMAT_RGB24:
		case CAM_MEDIA_FORMAT_RGB32:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line, "Invalid CAM_MEDIA_FORMAT %" PRIu8, val);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_MEDIA_TYPE_DESCRIPTION_FLAGS value
 *
 *  @param val The value to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_MediaTypeDescriptionFlags(val) \
	rdpecam_valid_MediaTypeDescriptionFlags_((val), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_MediaTypeDescriptionFlags_(UINT8 val, wLog* log, const char* file,
                                                            const char* fkt, size_t line)
{
	switch (val)
	{
		case CAM_MEDIA_TYPE_DESCRIPTION_FLAG_DecodingRequired:
		case CAM_MEDIA_TYPE_DESCRIPTION_FLAG_BottomUpImage:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line,
			                     "Invalid CAM_MEDIA_TYPE_DESCRIPTION_FLAGS %" PRIu8, val);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_PROPERTY_MODE value
 *
 *  @param val The value to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_CamPropertyMode(val) \
	rdpecam_valid_CamPropertyMode_((val), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_CamPropertyMode_(UINT8 val, wLog* log, const char* file,
                                                  const char* fkt, size_t line)
{
	switch (val)
	{
		case CAM_PROPERTY_MODE_Manual:
		case CAM_PROPERTY_MODE_Auto:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line, "Invalid CAM_PROPERTY_MODE %" PRIu8, val);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_PROPERTY_SET value
 *
 *  @param val The value to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_CamPropertySet(val) \
	rdpecam_valid_CamPropertySet_((val), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_CamPropertySet_(UINT8 val, wLog* log, const char* file,
                                                 const char* fkt, size_t line)
{
	switch (val)
	{
		case CAM_PROPERTY_SET_CameraControl:
		case CAM_PROPERTY_SET_VideoProcAmp:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line, "Invalid CAM_PROPERTY_SET %" PRIu8, val);
			return false;
	}
}

/** @brief check input data is a valid \ref CAM_PROPERTY_CAPABILITIES value
 *
 *  @param val The value to check
 *  @param log The logger to use
 *  @param file The file name the function is called from
 *  @param fkt The function calling this function
 *  @param line The line number where the function is called
 *
 *  @return \b true for success, \b false if invalid
 *  @since version 3.20.1
 */
#define rdpecam_valid_CamPropertyCapabilities(val) \
	rdpecam_valid_CamPropertyCapabilities_((val), WLog_Get(TAG), __FILE__, __func__, __LINE__)
static inline bool rdpecam_valid_CamPropertyCapabilities_(UINT8 val, wLog* log, const char* file,
                                                          const char* fkt, size_t line)
{
	switch (val)
	{
		case CAM_PROPERTY_CAPABILITY_Manual:
		case CAM_PROPERTY_CAPABILITY_Auto:
			return true;
		default:
			rdpecam_PrintWarning(log, file, fkt, line, "Invalid CAM_PROPERTY_CAPABILITIES %" PRIu8,
			                     val);
			return false;
	}
}

#endif
