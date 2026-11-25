/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Regression test for static channel delivery after Soft-Sync tunnelling
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include <winpr/crt.h>
#include <winpr/wtsapi.h>

#include <freerdp/client.h>
#include <freerdp/channels/channels.h>

#include "../client.h"
#include "../mcs.h"
#include "../rdp.h"

#define TEST_CHANNEL_ID 1004
#define TEST_OPEN_HANDLE 7

typedef struct
{
	size_t deliveries;
	size_t bridgeFrees;
} TEST_STATE;

static void VCAPITYPE test_open_event(LPVOID userParam, WINPR_ATTR_UNUSED DWORD openHandle,
                                      UINT event, WINPR_ATTR_UNUSED LPVOID data,
                                      WINPR_ATTR_UNUSED UINT32 dataLength,
                                      WINPR_ATTR_UNUSED UINT32 totalLength,
                                      WINPR_ATTR_UNUSED UINT32 flags)
{
	TEST_STATE* state = userParam;
	WINPR_ASSERT(state);
	if (event == CHANNEL_EVENT_DATA_RECEIVED)
		state->deliveries++;
}

static BOOL test_bridge_send(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED const BYTE* data,
                             WINPR_ATTR_UNUSED size_t length)
{
	return TRUE;
}

static void test_bridge_free(void* custom)
{
	TEST_STATE* state = custom;
	WINPR_ASSERT(state);
	state->bridgeFrees++;
}

int TestClientStaticBridge(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	int rc = -1;
	TEST_STATE state = WINPR_C_ARRAY_INIT;
	const BYTE data[] = { 1, 2, 3, 4 };

	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context || !context->channels || !context->rdp || !context->rdp->mcs)
		goto out;

	rdpChannels* channels = context->channels;
	rdpMcs* mcs = context->rdp->mcs;
	CHANNEL_OPEN_DATA* openData = &channels->openDataList[0];

	channels->openDataCount = 1;
	openData->channels = channels;
	openData->lpUserParam = &state;
	openData->pChannelOpenEventProcEx = test_open_event;
	openData->stats.channelId = TEST_OPEN_HANDLE;
	(void)strncpy(openData->stats.channelName, "test", CHANNEL_NAME_LEN);

	mcs->channelCount = 1;
	mcs->channels[0].ChannelId = TEST_CHANNEL_ID;
	mcs->channels[0].joined = TRUE;
	(void)strncpy(mcs->channels[0].Name, "test", CHANNEL_NAME_LEN);

	/* Before migration, ordinary static transport data reaches the plugin. */
	if (!freerdp_channels_data(context->instance, TEST_CHANNEL_ID, data, sizeof(data),
	                           CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(data)) ||
	    (state.deliveries != 1))
		goto out;

	if (!freerdp_channels_set_static_bridge(context->instance, "test", test_bridge_send,
	                                        test_bridge_free, &state))
		goto out;

	/* Once migrated, the original static path is ignored. */
	if (!freerdp_channels_data(context->instance, TEST_CHANNEL_ID, data, sizeof(data),
	                           CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(data)) ||
	    (state.deliveries != 1))
		goto out;

	/* The DVC bridge remains the sole input path to the static plugin. */
	if (!freerdp_channels_data_bridge(context->instance, TEST_CHANNEL_ID, data, sizeof(data),
	                                  CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(data)) ||
	    (state.deliveries != 2))
		goto out;

	if (!freerdp_channels_set_static_bridge(context->instance, "test", nullptr, nullptr, &state) ||
	    (state.bridgeFrees != 1))
		goto out;

	/* Closing the DVC does not restore a transport forbidden by Soft-Sync. */
	if (!freerdp_channels_data(context->instance, TEST_CHANNEL_ID, data, sizeof(data),
	                           CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST, sizeof(data)) ||
	    (state.deliveries != 2))
		goto out;

	rc = 0;
out:
	freerdp_client_context_free(context);
	return rc;
}
