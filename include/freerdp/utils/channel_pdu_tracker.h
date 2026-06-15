/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * static channel utility functions
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_UTILS_CHANNEL_TRACKER_H_
#define FREERDP_UTILS_CHANNEL_TRACKER_H_

#include <winpr/wtypes.h>
#include <winpr/stream.h>

#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct ChannelPduTracker ChannelPduTracker;

	/**
	 * frees a ChannelPduTracker
	 * @param tracker the tracker to release
	 * @since version 3.28.0
	 */
	FREERDP_API void ChannelPduTracker_free(ChannelPduTracker* tracker);

	/**
	 * Creates a ChannelPduTracker
	 * @param vc the HANDLE of the channel
	 * @return the created ChannelPduTracker, nullptr if something failed
	 * @since version 3.28.0
	 */
	WINPR_ATTR_MALLOC(ChannelPduTracker_free, 1)
	FREERDP_API ChannelPduTracker* ChannelPduTracker_new(HANDLE vc);

	/**
	 * Polls the channel and return a PDU if a whole PDU was reassembled
	 * @param tracker the ChannelPduTracker to poll a packet from
	 * @param ok a pointer to a status BOOL
	 * @return the reassembled PDU if any
	 * @since version 3.28.0
	 */
	WINPR_ATTR_MALLOC(Stream_Release, 1)
	FREERDP_API wStream* ChannelPduTracker_poll(ChannelPduTracker* tracker, BOOL* ok);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_CHANNEL_TRACKER_H_ */
