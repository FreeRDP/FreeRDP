/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Location Virtual Channel Extension
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
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

#ifndef FREERDP_CHANNEL_LOCATION_CLIENT_LOCATION_H
#define FREERDP_CHANNEL_LOCATION_CLIENT_LOCATION_H

#include <freerdp/channels/location.h>

/** @file
 *  @since version 3.4.0
 */
#ifdef __cplusplus
extern "C"
{
#endif

	/** @since version 3.4.0 */
	typedef struct s_location_client_context LocationClientContext;

	/** @since version 3.4.0 */
	typedef UINT (*pcLocationStart)(LocationClientContext* context, UINT32 version, UINT32 flags);

	/** @since version 3.4.0 */
	typedef UINT (*pcLocationStop)(LocationClientContext* context);

	/** @since version 3.4.0 */
	typedef UINT (*pcLocationSend)(LocationClientContext* context, LOCATION_PDUTYPE type,
	                               size_t count, ...);

	/** @since version 3.4.0 */
	struct s_location_client_context
	{
		void* handle;
		void* custom;

		/**! \brief initialize location services on client
		 *
		 * \param context The client context to operate on
		 * \param version The location channel version (determines which features are available.
		 * \param flags The location channel flags.
		 *
		 * \return \b CHANNEL_RC_OK for success, an appropriate error otherwise.
		 */
		pcLocationStart LocationStart;

		/**! \brief stop location services on client
		 *
		 * \param context The client context to operate on
		 *
		 * \return \b CHANNEL_RC_OK for success, an appropriate error otherwise.
		 */
		pcLocationStop LocationStop;

		/**! \brief Send a location update.
		 *
		 * This function sends location updates to a server.
		 * The following parameter formats are supported:
		 *
		 * \param type one of the following:
		 *   PDUTYPE_BASE_LOCATION3D : count = 3 | 7
		 *      latitude           : double, required
		 *      longitude          : double, required
		 *      altitude           : INT32, required
		 *      speed              : double, optional
		 *      heading            : double, optional
		 *      horizontalAccuracy : double, optional
		 *      source             : int, optional
		 *   PDUTYPE_LOCATION2D_DELTA : count = 2 | 4
		 *      latitudeDelta  : double, required
		 *      longitudeDelta : double, required
		 *      speedDelta     : double, optional
		 *      headingDelta   : double, optional
		 *   PDUTYPE_LOCATION3D_DELTA : count = 3 | 5
		 *      latitudeDelta  : double, required
		 *      longitudeDelta : double, required
		 *      altitudeDelta  : INT32, optional
		 *      speedDelta     : double, optional
		 *      headingDelta   : double, optional
		 * \param count the number of variable arguments following
		 *
		 * return \b CHANNEL_RC_OK for success, an appropriate error otherwise.
		 */
		pcLocationSend LocationSend;
	};

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_LOCATION_CLIENT_LOCATION_H */
