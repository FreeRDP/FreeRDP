/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (UDP multitransport)
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

#include <string.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/interlocked.h>
#include <winpr/sysinfo.h>

#include <freerdp/log.h>
#include <freerdp/settings.h>
#include "sf_udp.h"

/* The echo channel gives the tunnel something to carry once a second. */
#if defined(CHANNEL_ECHO_SERVER)
#include <freerdp/server/echo.h>
#endif

#define TAG SERVER_TAG("sample")

#if defined(CHANNEL_ECHO_SERVER)
struct sf_udp
{
	echo_server_context* echo;
	LONG echo_open; /* Updated by the echo channel thread; must be accessed using Interlocked
	                   functions */
	UINT64 echo_sent;
};

WINPR_ATTR_NODISCARD
static UINT sf_peer_echo_open_result(echo_server_context* echo, ECHO_SERVER_OPEN_RESULT result)
{
	WINPR_ASSERT(echo);
	testPeerContext* context = echo->data;
	WINPR_ASSERT(context);
	const BOOL open = (result == ECHO_SERVER_OPEN_RESULT_OK);
	/* Avoid InterlockedExchange; WinPR's version reads the old value non-atomically */
	if (open)
		InterlockedIncrement(&context->udp->echo_open);
	WLog_INFO(TAG, "echo channel status: %s", open ? "open" : "not available");
	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static UINT sf_peer_echo_response(echo_server_context* echo, WINPR_ATTR_UNUSED const BYTE* buffer,
                                  WINPR_ATTR_UNUSED UINT32 length)
{
	WINPR_ASSERT(echo);
	WLog_VRB(TAG, "received echo response");
	return CHANNEL_RC_OK;
}

static void sf_peer_echo(testPeerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->udp);

	if (!context->udp->echo)
	{
		context->udp->echo = echo_server_context_new(context->vcm);
		if (!context->udp->echo)
			return;
		context->udp->echo->data = context;
		context->udp->echo->OpenResult = sf_peer_echo_open_result;
		context->udp->echo->Response = sf_peer_echo_response;
		if (context->udp->echo->Open(context->udp->echo) != CHANNEL_RC_OK)
			WLog_WARN(TAG, "failed to open the echo channel");
		return;
	}

	const UINT64 now = GetTickCount64();
	if ((InterlockedCompareExchange(&context->udp->echo_open, 0, 0) == 0) ||
	    (now - context->udp->echo_sent < 1000))
		return;
	context->udp->echo_sent = now;
	if (!context->udp->echo->Request(context->udp->echo, (const BYTE*)&now, sizeof(now)))
		WLog_WARN(TAG, "echo request unsuccessful");
}
#endif

BOOL sf_udp_configure(rdpSettings* settings, BOOL udp)
{
	if (!udp)
		return TRUE;
	return freerdp_settings_set_uint32(settings, FreeRDP_MultitransportFlags,
	                                   (UINT32)TRANSPORT_TYPE_UDP_FECR |
	                                       (UINT32)SOFTSYNC_TCP_TO_UDP);
}

DWORD sf_udp_wait_ms(BOOL udp)
{
	return udp ? 1000 : INFINITE;
}

BOOL sf_udp_check_channels(HANDLE vcm, BOOL udp, BOOL activated)
{
	return WTSVirtualChannelManagerCheckFileDescriptorEx(vcm, !udp || activated);
}

void sf_udp_tick(testPeerContext* context, BOOL udp)
{
	WINPR_ASSERT(context);

	if (!udp)
		return;
#if defined(CHANNEL_ECHO_SERVER)
	if (!context->udp)
		context->udp = calloc(1, sizeof(struct sf_udp));
	if (context->udp)
		sf_peer_echo(context);
#endif
}

void sf_udp_free(testPeerContext* context)
{
	WINPR_ASSERT(context);

	if (!context->udp)
		return;
#if defined(CHANNEL_ECHO_SERVER)
	if (context->udp->echo)
	{
		if (context->udp->echo->Close(context->udp->echo) != CHANNEL_RC_OK)
			WLog_WARN(TAG, "failed to close the echo channel");
		echo_server_context_free(context->udp->echo);
	}
	free(context->udp);
#endif
	context->udp = nullptr;
}

void sf_udp_usage(FILE* fp)
{
	(void)fprintf(fp, "\t--udp\n");
}

BOOL sf_udp_parse_option(const char* arg, BOOL* udp)
{
	WINPR_ASSERT(arg);
	WINPR_ASSERT(udp);

	if (strcmp(arg, "--udp") != 0)
		return FALSE;
	*udp = TRUE;
	return TRUE;
}
