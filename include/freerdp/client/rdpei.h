/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_RDPEI_CLIENT_RDPEI_H
#define FREERDP_CHANNEL_RDPEI_CLIENT_RDPEI_H

#include <freerdp/channels/rdpei.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * Client Interface
	 */

	typedef struct s_rdpei_client_context RdpeiClientContext;

	typedef UINT32 (*pcRdpeiGetVersion)(RdpeiClientContext* context);
	typedef UINT32 (*pcRdpeiGetFeatures)(RdpeiClientContext* context);

	typedef UINT (*pcRdpeiAddContact)(RdpeiClientContext* context,
	                                  const RDPINPUT_CONTACT_DATA* contact);

	typedef UINT (*pcRdpeiTouchEvent)(RdpeiClientContext* context, INT32 externalId, INT32 x,
	                                  INT32 y, INT32* contactId);
	typedef UINT (*pcRdpeiTouchRawEvent)(RdpeiClientContext* context, INT32 externalId, INT32 x,
	                                     INT32 y, INT32* contactId, UINT32 contactFlags,
	                                     UINT32 fieldFlags, ...);
	typedef UINT (*pcRdpeiTouchRawEventVA)(RdpeiClientContext* context, INT32 externalId, INT32 x,
	                                       INT32 y, INT32* contactId, UINT32 contactFlags,
	                                       UINT32 fieldFlags, va_list args);

	typedef UINT (*pcRdpeiAddPen)(RdpeiClientContext* context, INT32 externalId,
	                              const RDPINPUT_PEN_CONTACT* contact);

	typedef UINT (*pcRdpeiPen)(RdpeiClientContext* context, INT32 externalId, UINT32 fieldFlags,
	                           INT32 x, INT32 y, ...);

	typedef UINT (*pcRdpeiPenRawEvent)(RdpeiClientContext* context, INT32 externalId,
	                                   UINT32 contactFlags, UINT32 fieldFlags, INT32 x, INT32 y,
	                                   ...);
	typedef UINT (*pcRdpeiPenRawEventVA)(RdpeiClientContext* context, INT32 externalId,
	                                     UINT32 contactFlags, UINT32 fieldFlags, INT32 x, INT32 y,
	                                     va_list args);

	typedef UINT (*pcRdpeiSuspendTouch)(RdpeiClientContext* context);
	typedef UINT (*pcRdpeiResumeTouch)(RdpeiClientContext* context);

	struct s_rdpei_client_context
	{
		void* handle;
		void* custom;

		WINPR_ATTR_NODISCARD pcRdpeiGetVersion GetVersion;
		WINPR_ATTR_NODISCARD pcRdpeiGetFeatures GetFeatures;

		WINPR_ATTR_NODISCARD pcRdpeiAddContact AddContact;

		WINPR_ATTR_NODISCARD pcRdpeiTouchEvent TouchBegin;
		WINPR_ATTR_NODISCARD pcRdpeiTouchEvent TouchUpdate;
		WINPR_ATTR_NODISCARD pcRdpeiTouchEvent TouchEnd;

		WINPR_ATTR_NODISCARD pcRdpeiAddPen AddPen;

		WINPR_ATTR_NODISCARD pcRdpeiPen PenBegin;
		WINPR_ATTR_NODISCARD pcRdpeiPen PenUpdate;
		WINPR_ATTR_NODISCARD pcRdpeiPen PenEnd;
		WINPR_ATTR_NODISCARD pcRdpeiPen PenHoverBegin;
		WINPR_ATTR_NODISCARD pcRdpeiPen PenHoverUpdate;
		WINPR_ATTR_NODISCARD pcRdpeiPen PenHoverCancel;

		WINPR_ATTR_NODISCARD pcRdpeiSuspendTouch SuspendTouch;
		WINPR_ATTR_NODISCARD pcRdpeiResumeTouch ResumeTouch;

		WINPR_ATTR_NODISCARD pcRdpeiTouchEvent TouchCancel;
		WINPR_ATTR_NODISCARD pcRdpeiTouchRawEvent TouchRawEvent;
		WINPR_ATTR_NODISCARD pcRdpeiTouchRawEventVA TouchRawEventVA;

		WINPR_ATTR_NODISCARD pcRdpeiPen PenCancel;
		WINPR_ATTR_NODISCARD pcRdpeiPenRawEvent PenRawEvent;
		WINPR_ATTR_NODISCARD pcRdpeiPenRawEventVA PenRawEventVA;

		UINT32 clientFeaturesMask;
	};

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPEI_CLIENT_RDPEI_H */
