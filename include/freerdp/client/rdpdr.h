/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Extension
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#ifndef FREERDP_CHANNEL_RDPDR_CLIENT_H
#define FREERDP_CHANNEL_RDPDR_CLIENT_H

#include <freerdp/channels/rdpdr.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @enum Hotplug event types.
	 *
	 *  @since version 3.16.0
	 */
	typedef enum
	{
		RDPDR_HOTPLUG_FIRST_CHECK,
		RDPDR_HOTPLUG_CHECK_FOR_CHANGES
	} RdpdrHotplugEventType;

	typedef struct s_rdpdr_client_context RdpdrClientContext;

	/** @brief register a new device and announce it to remote
	 *
	 *  @param context The \ref RdpdrClientContext to operate on.
	 *  @param device A pointer to a \ref RDPDR_DEVICE struct to register. Contents is copied.
	 *  @param pid A pointer to a variable that will be set to a unique identifier for that device.
	 *
	 *  @return \ref CHANNEL_RC_OK for success or an appropriate error code otherwise.
	 *
	 *  @since version 3.16.0
	 */
	typedef UINT (*pcRdpdrRegisterDevice)(RdpdrClientContext* context, const RDPDR_DEVICE* device,
	                                      uint32_t* pid);

	/** @brief unregister a new device and announce it to remote
	 *
	 *  @param context The \ref RdpdrClientContext to operate on.
	 *  @param count The number of uintptr_t id unique identifiers for a device (see \ref
	 * pcRdpdrRegisterDevice) following
	 *
	 *  @return \ref CHANNEL_RC_OK for success or an appropriate error code otherwise.
	 *  @since version 3.16.0
	 */
	typedef UINT (*pcRdpdrUnregisterDevice)(RdpdrClientContext* context, size_t count,
	                                        const uint32_t ids[]);

	/** @brief Check for device changes and announce it to remote
	 *
	 *  @param context The \ref RdpdrClientContext to operate on.
	 *  @param type The event type.
	 *
	 *  @return \ref CHANNEL_RC_OK for success or an appropriate error code otherwise.
	 *  @since version 3.16.0
	 */
	typedef UINT (*pcRdpdrHotplugDevice)(RdpdrClientContext* context, RdpdrHotplugEventType type);

	/** @struct rdpdr channel client context
	 *
	 *  @since version 3.16.0
	 */
	struct s_rdpdr_client_context
	{
		void* handle;
		void* custom;

		pcRdpdrRegisterDevice RdpdrRegisterDevice;
		pcRdpdrUnregisterDevice RdpdrUnregisterDevice;
		pcRdpdrHotplugDevice RdpdrHotplugDevice;
	};

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPDR_CLIENT_H */
