/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012 HP Development Company, LLC
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/freerdp.h>
#include <freerdp/utils/signal.h>
#include <freerdp/client/channels.h>

#include <pthread.h>

#include "xf_interface.h"

#include "xfreerdp.h"

extern HANDLE g_sem;
extern int g_thread_count;
extern BYTE g_disconnect_reason;

int main(int argc, char* argv[])
{
	pthread_t thread;
	freerdp* instance;

	freerdp_handle_signals();

	setlocale(LC_ALL, "");

	freerdp_channels_global_init();

	g_sem = CreateSemaphore(NULL, 0, 1, NULL);

	instance = freerdp_new();
	instance->PreConnect = xf_pre_connect;
	instance->PostConnect = xf_post_connect;
	instance->Authenticate = xf_authenticate;
	instance->VerifyCertificate = xf_verify_certificate;
	instance->LogonErrorInfo = xf_logon_error_info;
	instance->ReceiveChannelData = xf_receive_channel_data;

	instance->context_size = sizeof(xfContext);
	instance->ContextNew = (pContextNew) xf_context_new;
	instance->ContextFree = (pContextFree) xf_context_free;
	freerdp_context_new(instance);

	instance->context->argc = argc;
	instance->context->argv = argv;
	instance->settings->SoftwareGdi = FALSE;

	g_thread_count++;
	pthread_create(&thread, 0, xf_thread_func, instance);

	while (g_thread_count > 0)
	{
		WaitForSingleObject(g_sem, INFINITE);
	}

	pthread_join(thread, NULL);
	pthread_detach(thread);

	freerdp_context_free(instance);
	freerdp_free(instance);

	freerdp_channels_global_uninit();

	return xf_exit_code_from_disconnect_reason(g_disconnect_reason);
}
