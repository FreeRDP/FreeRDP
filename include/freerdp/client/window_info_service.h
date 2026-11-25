/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * IWTSWindowInfoService for clients
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

#ifndef FREERDP_CLIENT_WINDOW_INFO_SERVICE_H
#define FREERDP_CLIENT_WINDOW_INFO_SERVICE_H

#include <freerdp/api.h>
#include <freerdp/wts_services.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @file
	 *  The IWTSWindowInfoService a client puts in rdpContext::windowInfoService, with
	 *  Microsoft's behaviour (one subscriber, synchronous Subscribe/Unsubscribe, OnWindowChanged
	 *  given a copy of the new WTS_WINDOW_INFO, GetProcessId the client's process). The client
	 *  only tells where its windows are.
	 *
	 *	WINDOW_INFO_SOURCE source = { me, get_window_info, get_session_type };
	 *	context->windowInfoService = freerdp_client_window_info_service_new(&source);
	 *	...  // each time a window moves, resizes or changes scale:
	 *	freerdp_client_window_info_service_changed(context->windowInfoService, 0);
	 *	...  // in ContextFree, before `me` goes:
	 *	freerdp_client_window_info_service_detach(context->windowInfoService);
	 *	WINPR_COM_RELEASE(context->windowInfoService);
	 *	context->windowInfoService = nullptr;
	 *
	 *  @since version 3.34.0
	 */

	/** @brief what the client gives the service to answer from
	 *  @since version 3.34.0 */
	typedef struct
	{
		void* custom;

		/** @brief fills `info` as Microsoft's client does (see WTS_WINDOW_INFO): for a desktop
		 *  session the window showing it (remoteHwnd unused), for a RemoteApp session the local
		 *  window of the remote window `remoteHwnd`. Called from any thread, must not block. */
		HRESULT (*GetWindowInfo)(void* custom, UINT64 remoteHwnd, WTS_WINDOW_INFO* info);

		/** @brief a WTS_RDP_SESSION_TYPE. Called from any thread. */
		UINT32 (*GetSessionType)(void* custom);
	} WINDOW_INFO_SOURCE;

	/** @brief a new service (one reference for the caller) answering from `source`, which is
	 *  copied
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API IWTSWindowInfoService*
	freerdp_client_window_info_service_new(const WINDOW_INFO_SOURCE* source);

	/** @brief a window changed (moved, resized, scaled, shown or hidden): the subscriber gets
	 *  its new WTS_WINDOW_INFO (remoteHwnd: the remote window in a RemoteApp session, 0 for the
	 *  desktop). Calls the subscriber on the calling thread.
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_client_window_info_service_changed(IWTSWindowInfoService* service,
	                                                            UINT64 remoteHwnd);

	/** @brief stops using `source`: from now on the service answers E_UNEXPECTED. To call
	 *  before what `source` uses goes, since channels and plugins may keep the service.
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_client_window_info_service_detach(IWTSWindowInfoService* service);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_WINDOW_INFO_SERVICE_H */
