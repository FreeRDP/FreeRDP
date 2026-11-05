/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Result of the last call into a video decoder
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

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief What a call into a video decoder produced
	 *  @since version 3.34.0
	 */
	typedef enum
	{
		FREERDP_DECODE_NONE = 0,   /**< no decode happened yet */
		FREERDP_DECODE_FAILED,     /**< the decoder refused the input */
		FREERDP_DECODE_PICTURE,    /**< at least one picture reached the destination */
		FREERDP_DECODE_NO_PICTURE, /**< input accepted, nothing to show: the decoder waits for
		                                a key frame or buffers. Decoders can not tell which */
	} FREERDP_DECODE_STATUS;

	/** @brief The result of the last decode of a decoder context
	 *
	 *  The return values of the decompress functions do not tell a picture from none, and
	 *  they differ between the codecs, so this is the way to ask.
	 *
	 *  @since version 3.34.0
	 */
	typedef struct
	{
		FREERDP_DECODE_STATUS status;
		const char* backend; /**< valid for the lifetime of the process, \b nullptr if unknown */
	} FREERDP_DECODE_RESULT;

#ifdef __cplusplus
}
#endif
