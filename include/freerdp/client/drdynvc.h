/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_CLIENT_DRDYNVC_H
#define FREERDP_CHANNEL_CLIENT_DRDYNVC_H

/**
 * Client Interface
 */

typedef struct _drdynvc_client_context DrdynvcClientContext;

typedef int (*pcDrdynvcGetVersion)(DrdynvcClientContext* context);
typedef int (*pcDrdynvcOnChannelConnected)(DrdynvcClientContext* context, const char* name, void* pInterface);
typedef int (*pcDrdynvcOnChannelDisconnected)(DrdynvcClientContext* context, const char* name, void* pInterface);

struct _drdynvc_client_context
{
	void* handle;
	void* custom;

	pcDrdynvcGetVersion GetVersion;
	pcDrdynvcOnChannelConnected OnChannelConnected;
	pcDrdynvcOnChannelDisconnected OnChannelDisconnected;
};

#endif /* FREERDP_CHANNEL_CLIENT_DRDYNVC_H */
