/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Unsupported RAIL launch IPC tests
 *
 * Copyright 2026 Tony Dursun <oraturk75@gmail.com>
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

#include <stdio.h>
#include "../rail_ipc.h"

int TestClientRailIpcUnsupported(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(entry);
	entry.ContextSize = sizeof(rdpClientContext);
	rdpClientContext* cctx = (rdpClientContext*)freerdp_client_context_new(&entry);
	if (!cctx)
		return -1;
	BOOL ok = cctx->context.instance->PreConnect(cctx->context.instance) &&
	          freerdp_client_rail_multi_exec_set_ready(cctx, TRUE) &&
	          freerdp_client_rail_multi_exec_set_ready(cctx, FALSE);
	ok = ok && freerdp_settings_set_bool(cctx->context.settings, FreeRDP_RemoteApplicationMultiExec,
	                                     TRUE);
	/* Unsupported enablement also fails for desktop and authentication-only settings. */
	const struct
	{
		const char* name;
		BOOL remoteApp;
		BOOL authOnly;
	} cases[] = { { "desktop", FALSE, FALSE },
		          { "RemoteApp", TRUE, FALSE },
		          { "authentication only", TRUE, TRUE } };
	for (size_t x = 0; ok && (x < ARRAYSIZE(cases)); x++)
	{
		ok = freerdp_settings_set_bool(cctx->context.settings, FreeRDP_RemoteApplicationMode,
		                               cases[x].remoteApp) &&
		     freerdp_settings_set_bool(cctx->context.settings, FreeRDP_AuthenticationOnly,
		                               cases[x].authOnly) &&
		     !cctx->context.instance->PreConnect(cctx->context.instance);
		if (!ok)
			(void)fprintf(stderr, "unsupported case: %s\n", cases[x].name);
	}
	ok = ok && freerdp_client_rail_multi_exec_set_ready(cctx, TRUE) &&
	     freerdp_client_rail_multi_exec_set_ready(cctx, FALSE);
	for (size_t cleanup = 0; cleanup < 2; cleanup++)
	{
		cctx->context.instance->PostDisconnect(cctx->context.instance);
		cctx->context.instance->PostFinalDisconnect(cctx->context.instance);
		rail_ipc_free(cctx);
	}
	freerdp_client_context_free(&cctx->context);
	return ok ? 0 : -1;
}
