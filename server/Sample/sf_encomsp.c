/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Lync Multiparty)
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include "sf_encomsp.h"

BOOL sf_peer_encomsp_init(testPeerContext* context)
{
	WINPR_ASSERT(context);

	context->encomsp = encomsp_server_context_new(context->vcm);
	if (!context->encomsp)
		return FALSE;

	context->encomsp->rdpcontext = &context->_p;

	WINPR_ASSERT(context->encomsp->Start);
	if (context->encomsp->Start(context->encomsp) != CHANNEL_RC_OK)
		return FALSE;

	return TRUE;
}
