/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Display Update Virtual Channel Extension
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

#ifndef FREERDP_CHANNEL_AINPUT_CLIENT_AINPUT_H
#define FREERDP_CHANNEL_AINPUT_CLIENT_AINPUT_H

#include <winpr/assert.h>
#include <freerdp/channels/ainput.h>

typedef UINT (*pcAInputSendInputEvent)(AInputClientContext* context, UINT64 flags, INT32 x,
                                       INT32 y);

struct ainput_client_context
{
	void* handle;
	void* custom;

	pcAInputSendInputEvent AInputSendInputEvent;
};

#endif /* FREERDP_CHANNEL_AINPUT_CLIENT_AINPUT_H */
