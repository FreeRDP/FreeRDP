/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Regression coverage for receiving desktop updates during clipboard writes.
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

#include <winpr/crt.h>
#include <winpr/interlocked.h>
#include <winpr/stream.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <freerdp/client.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/transport_io.h>

#include "../client.h"
#include "../mcs.h"
#include "../rdp.h"
#include "../transport.h"

#define TEST_CHANNEL_ID 1004
#define TEST_CHUNK_SIZE 1600

typedef struct
{
	rdpClientContext common;
	BYTE* data;
	size_t length;
	size_t offset;
	size_t reads;
	size_t writes;
	size_t readsAtLastWrite;
	size_t completed;
	size_t cancelled;
	size_t customCalls;
	size_t failAtWrite;
	size_t controlWrites;
	size_t expectedMessages;
	pSendChannelData originalSend;
	BOOL showProtocol;
	BOOL requireInterleaving;
	BOOL clearOnComplete;
	BOOL invalid;
} TEST_CONTEXT;

static int test_read(rdpTransport* transport, WINPR_ATTR_UNUSED wStream* stream)
{
	TEST_CONTEXT* test = (TEST_CONTEXT*)transport_get_context(transport);
	test->reads++;
	return 0;
}

static int test_write(rdpTransport* transport, wStream* stream)
{
	TEST_CONTEXT* test = (TEST_CONTEXT*)transport_get_context(transport);
	const size_t length = Stream_GetPosition(stream);
	const BYTE* packet = Stream_Buffer(stream);
	/* TPKT (4), X.224 (3), MCS opcode (1), initiator (2), channel id (2). */
	if (length > 12 && (((UINT16)packet[10] << 8) | packet[11]) == TEST_CHANNEL_ID + 1)
	{
		test->controlWrites++;
		if (test->offset == 0 || test->offset >= test->length)
			test->invalid = TRUE;
		return (int)length;
	}
	const size_t remaining = test->length - test->offset;
	const UINT32 limit =
	    freerdp_settings_get_uint32(test->common.context.settings, FreeRDP_VCChunkSize);
	const size_t chunk = (remaining < limit) ? remaining : limit;
	UINT32 total = 0;
	UINT32 flags = 0;
	test->writes++;
	if (test->writes == test->failAtWrite)
		return -1;
	if (!chunk || (length < chunk + 8) ||
	    (test->requireInterleaving && test->writes > 1 && test->reads <= test->readsAtLastWrite))
		goto fail;
	Stream_SetPosition(stream, length - chunk - 8);
	Stream_Read_UINT32(stream, total);
	Stream_Read_UINT32(stream, flags);
	UINT32 expected = test->showProtocol ? CHANNEL_FLAG_SHOW_PROTOCOL : 0;
	if (test->offset == 0)
		expected |= CHANNEL_FLAG_FIRST;
	if (chunk == remaining)
		expected |= CHANNEL_FLAG_LAST;
	if ((total != test->length) || (flags != expected) ||
	    memcmp(Stream_ConstPointer(stream), test->data + test->offset, chunk) != 0)
		goto fail;
	test->offset += chunk;
	test->readsAtLastWrite = test->reads;
	Stream_SetPosition(stream, length);
	return (int)length;
fail:
	test->invalid = TRUE;
	return -1;
}

static void VCAPITYPE test_open_event(LPVOID user, WINPR_ATTR_UNUSED DWORD handle, UINT event,
                                      WINPR_ATTR_UNUSED LPVOID data, UINT32 length,
                                      UINT32 totalLength, WINPR_ATTR_UNUSED UINT32 flags)
{
	TEST_CONTEXT* test = user;
	if ((length != test->length) || (totalLength != test->length))
		test->invalid = TRUE;
	if (event == CHANNEL_EVENT_WRITE_COMPLETE)
	{
		test->completed++;
		if (test->completed < test->expectedMessages)
			test->offset = 0;
		if (test->clearOnComplete)
			MessageQueue_Clear(test->common.context.channels->clipboardQueue);
	}
	else if (event == CHANNEL_EVENT_WRITE_CANCELLED)
		test->cancelled++;
}

static void test_free_message(wMessage* message)
{
	free(message->wParam);
}

static BOOL test_custom_data(freerdp* instance, UINT16 channelId, const BYTE* data, size_t length)
{
	TEST_CONTEXT* test = (TEST_CONTEXT*)instance->context;
	test->customCalls++;
	if ((channelId != TEST_CHANNEL_ID) || (length != test->length) ||
	    memcmp(data, test->data, length) != 0)
		test->invalid = TRUE;
	return TRUE;
}

static BOOL test_legacy_data(freerdp* instance, UINT16 channelId, const BYTE* data, size_t length)
{
	TEST_CONTEXT* test = (TEST_CONTEXT*)instance->context;
	return test->originalSend(instance, channelId, data, length);
}

static BOOL test_custom_packet(WINPR_ATTR_UNUSED freerdp* instance,
                               WINPR_ATTR_UNUSED UINT16 channelId,
                               WINPR_ATTR_UNUSED size_t totalSize, WINPR_ATTR_UNUSED UINT32 flags,
                               WINPR_ATTR_UNUSED const BYTE* data,
                               WINPR_ATTR_UNUSED size_t chunkSize)
{
	return FALSE;
}

static TEST_CONTEXT* test_new(size_t length, const char* channelName, BOOL showProtocol)
{
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(TEST_CONTEXT);
	TEST_CONTEXT* test = (TEST_CONTEXT*)freerdp_client_context_new(&entry);
	if (!test)
		return nullptr;
	rdpContext* context = &test->common.context;
	test->length = length;
	test->showProtocol = showProtocol;
	test->data = malloc(length);
	if (!test->data)
		goto fail;
	for (size_t x = 0; x < length; x++)
		test->data[x] = (BYTE)(x * 31);
	if (!freerdp_settings_set_uint32(context->settings, FreeRDP_VCChunkSize, TEST_CHUNK_SIZE))
		goto fail;
	rdpChannels* channels = context->channels;
	CHANNEL_OPEN_DATA* open = &channels->openDataList[0];
	channels->openDataCount = 1;
	open->channels = channels;
	open->flags = 2;
	open->lpUserParam = test;
	open->pChannelOpenEventProcEx = test_open_event;
	(void)strncpy(open->stats.channelName, channelName, CHANNEL_NAME_LEN);
	rdpMcs* mcs = context->rdp->mcs;
	mcs->userId = TEST_CHANNEL_ID;
	mcs->channelCount = 1;
	mcs->channels[0].ChannelId = TEST_CHANNEL_ID;
	mcs->channels[0].options = showProtocol ? CHANNEL_OPTION_SHOW_PROTOCOL : 0;
	(void)strncpy(mcs->channels[0].Name, channelName, CHANNEL_NAME_LEN);
	rdpTransportIo io = *transport_get_io_callbacks(context->rdp->transport);
	io.ReadPdu = test_read;
	io.WritePdu = test_write;
	if (!transport_set_io_callbacks(context->rdp->transport, &io))
		goto fail;
	return test;
fail:
	free(test->data);
	freerdp_client_context_free(context);
	return nullptr;
}

static BOOL test_enqueue(TEST_CONTEXT* test)
{
	CHANNEL_OPEN_EVENT* item = calloc(1, sizeof(CHANNEL_OPEN_EVENT));
	if (!item)
		return FALSE;
	item->Data = test->data;
	item->DataLength = (UINT32)test->length;
	item->pChannelOpenData = &test->common.context.channels->openDataList[0];
	const wMessage msg = { .id = 0, .wParam = item, .Free = test_free_message };
	if (MessageQueue_Dispatch(test->common.context.channels->clipboardQueue, &msg))
		return SetEvent(test->common.context.channels->workEvent);
	free(item);
	return FALSE;
}

static void test_free(TEST_CONTEXT* test)
{
	if (!test)
		return;
	/* Cancellation callbacks still need the test state and data length. */
	MessageQueue_Clear(test->common.context.channels->clipboardQueue);
	free(test->data);
	freerdp_client_context_free(&test->common.context);
}

static BOOL test_transfer(size_t length, const char* name, BOOL showProtocol, int override)
{
	BOOL rc = FALSE;
	TEST_CONTEXT* test = test_new(length, name, showProtocol);
	if (!test)
		return FALSE;
	test->requireInterleaving = (length > TEST_CHUNK_SIZE) && (override == 0 || override == 3) &&
	                            (strcmp(name, CLIPRDR_SVC_CHANNEL_NAME) == 0);
#if defined(WITH_RDPEUDP)
	/* Experimental static-to-DVC migration retains the original whole-message path. */
	test->requireInterleaving = (override == 3);
#endif
	if (override == 1)
		test->common.context.instance->SendChannelData = test_custom_data;
	if (override == 2)
		test->common.context.instance->SendChannelPacket = test_custom_packet;
	if (override == 3)
	{
		test->originalSend = test->common.context.instance->SendChannelData;
		test->common.context.instance->SendChannelData = test_legacy_data;
	}
	if (!test_enqueue(test))
		goto out;
	size_t passes = 0;
	while (MessageQueue_Size(test->common.context.channels->clipboardQueue) > 0)
	{
		if (++passes > length / TEST_CHUNK_SIZE + 2 ||
		    !freerdp_check_event_handles(&test->common.context))
			goto out;
		if (MessageQueue_Size(test->common.context.channels->clipboardQueue) > 0 &&
		    (WaitForSingleObject(freerdp_channels_get_event_handle(test->common.context.instance),
		                         0) != WAIT_OBJECT_0 ||
		     test->completed != 0))
			goto out;
	}
	const StaticClientChannelStat* stats = &test->common.context.channels->openDataList[0].stats;
	if (test->invalid || test->completed != 1 || test->cancelled != 0 ||
	    stats->bytesOut != length || stats->packetsOut != 1 ||
	    stats->fragmentsOut != (length + TEST_CHUNK_SIZE - 1) / TEST_CHUNK_SIZE)
		goto out;
	if (override == 1)
	{
		if (test->customCalls != 1 || test->writes != 0)
			goto out;
	}
	else if (test->offset != length)
		goto out;
	if (!test->requireInterleaving && passes != 1)
		goto out;
	if (WaitForSingleObject(freerdp_channels_get_event_handle(test->common.context.instance), 0) !=
	    WAIT_TIMEOUT)
		goto out;
	rc = TRUE;
out:
	if (!rc)
		fprintf(stderr, "transfer failed: length=%zu channel=%s override=%d writes=%zu reads=%zu\n",
		        length, name, override, test->writes, test->reads);
	test_free(test);
	return rc;
}

static BOOL test_cancel_or_fail(BOOL failWrite)
{
	BOOL rc = FALSE;
	TEST_CONTEXT* test = test_new(TEST_CHUNK_SIZE * 3 + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE);
	if (!test || !test_enqueue(test))
		goto out;
	if (!freerdp_check_event_handles(&test->common.context) || test->completed != 0)
		goto out;
	if (failWrite)
	{
		test->failAtWrite = 2;
		if (freerdp_check_event_handles(&test->common.context) || test->writes != 2 ||
		    test->completed != 1 || test->cancelled != 0)
			goto out;
	}
	else
	{
		MessageQueue_Clear(test->common.context.channels->clipboardQueue);
		if (test->cancelled != 1 || test->completed != 0 || test->writes != 1)
			goto out;
	}
	if (MessageQueue_Size(test->common.context.channels->clipboardQueue) != 0)
		goto out;
	rc = TRUE;
out:
	if (!rc)
		fprintf(stderr, "partial-write %s failed\n", failWrite ? "failure" : "cancellation");
	test_free(test);
	return rc;
}

static BOOL test_control_and_order(void)
{
	BOOL rc = FALSE;
	TEST_CONTEXT* test = test_new(TEST_CHUNK_SIZE * 3 + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE);
	if (!test)
		return FALSE;
	test->requireInterleaving = TRUE;
	test->expectedMessages = 2;
	if (!test_enqueue(test) || !test_enqueue(test) ||
	    !freerdp_check_event_handles(&test->common.context))
		goto out;

	/* A graphics acknowledgement must not wait for either complete clipboard image. */
	rdpChannels* channels = test->common.context.channels;
	CHANNEL_OPEN_DATA* control = &channels->openDataList[1];
	channels->openDataCount = 2;
	control->channels = channels;
	control->flags = 2;
	strcpy(control->stats.channelName, "drdynvc");
	rdpMcs* mcs = test->common.context.rdp->mcs;
	mcs->channelCount = 2;
	mcs->channels[1].ChannelId = TEST_CHANNEL_ID + 1;
	strcpy(mcs->channels[1].Name, "drdynvc");
	static const BYTE acknowledgement[] = { 1, 2, 3, 4 };
	CHANNEL_OPEN_EVENT* item = calloc(1, sizeof(CHANNEL_OPEN_EVENT));
	if (!item)
		goto out;
	item->Data = (void*)acknowledgement;
	item->DataLength = sizeof(acknowledgement);
	item->pChannelOpenData = control;
	const wMessage msg = { .id = 0, .wParam = item, .Free = test_free_message };
	if (!MessageQueue_Dispatch(channels->queue, &msg))
	{
		free(item);
		goto out;
	}
	if (!freerdp_check_event_handles(&test->common.context) || test->controlWrites != 1 ||
	    test->completed != 0)
		goto out;
	for (size_t pass = 0; pass < 10 && MessageQueue_Size(channels->clipboardQueue); pass++)
	{
		if (!freerdp_check_event_handles(&test->common.context))
			goto out;
	}
	if (test->invalid || test->completed != 2 || test->cancelled != 0 ||
	    test->offset != test->length || test->writes != 8)
		goto out;
	rc = TRUE;
out:
	if (!rc)
		fprintf(stderr, "control traffic or clipboard ordering failed\n");
	test_free(test);
	return rc;
}

static BOOL test_completion_clears_queue(void)
{
	BOOL rc = FALSE;
	TEST_CONTEXT* test = test_new(TEST_CHUNK_SIZE + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE);
	if (!test)
		return FALSE;
	test->clearOnComplete = TRUE;
	if (!test_enqueue(test) || !test_enqueue(test) ||
	    !freerdp_check_event_handles(&test->common.context) ||
	    !freerdp_check_event_handles(&test->common.context))
		goto out;
	if (test->completed != 1 || test->cancelled != 1 || test->writes != 2 || test->invalid)
		goto out;
	rc = TRUE;
out:
	test_free(test);
	return rc;
}

static BOOL test_disconnect(void)
{
	BOOL rc = FALSE;
	TEST_CONTEXT* test = test_new(TEST_CHUNK_SIZE * 4 + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE);
	if (!test)
		return FALSE;
	if (!test_enqueue(test) || !freerdp_check_event_handles(&test->common.context))
		goto out;
	test->common.context.channels->connected = TRUE;
	if (freerdp_channels_disconnect(test->common.context.channels, test->common.context.instance) !=
	    CHANNEL_RC_OK)
		goto out;
	if (test->cancelled != 1 || test->completed != 0 || test->writes >= 5 || test->invalid)
		goto out;
	rc = TRUE;
out:
	test_free(test);
	return rc;
}

static BOOL test_change_during_write(BOOL replaceCallback)
{
	BOOL rc = FALSE;
	TEST_CONTEXT* test = test_new(TEST_CHUNK_SIZE * 3 + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE);
	if (!test || !test_enqueue(test) || !freerdp_check_event_handles(&test->common.context))
		goto out;
	if (replaceCallback)
	{
		test->common.context.instance->SendChannelData = test_custom_data;
		if (freerdp_check_event_handles(&test->common.context) || test->customCalls != 0 ||
		    test->writes != 1 || test->completed != 1)
			goto out;
	}
	else
	{
		/* Renegotiating a larger chunk must not restart the image from its first byte. */
		if (!freerdp_settings_set_uint32(test->common.context.settings, FreeRDP_VCChunkSize,
		                                 (UINT32)test->length * 2) ||
		    !freerdp_check_event_handles(&test->common.context) || test->offset != test->length ||
		    test->writes != 2 || test->completed != 1)
			goto out;
	}
	if (test->invalid || MessageQueue_Size(test->common.context.channels->clipboardQueue))
		goto out;
	rc = TRUE;
out:
	if (!rc)
		fprintf(stderr, "mid-write %s change failed\n",
		        replaceCallback ? "callback" : "chunk-size");
	test_free(test);
	return rc;
}

typedef struct
{
	TEST_CONTEXT* test;
	CHANNEL_ENTRY_POINTS_FREERDP legacy;
	CHANNEL_ENTRY_POINTS_FREERDP_EX extended;
	LPVOID initHandle;
	DWORD openHandle;
	BOOL extendedApi;
	volatile LONG error;
} TEST_PLUGIN;

/* The legacy open callback has no user parameter. These cases run sequentially. */
static TEST_PLUGIN* g_TestPlugin = nullptr;

static void VCAPITYPE test_plugin_open(DWORD handle, UINT event, LPVOID data, UINT32 length,
                                       UINT32 totalLength, UINT32 flags)
{
	test_open_event(g_TestPlugin->test, handle, event, data, length, totalLength, flags);
}

static void VCAPITYPE test_plugin_init(LPVOID handle, UINT event, WINPR_ATTR_UNUSED LPVOID data,
                                       WINPR_ATTR_UNUSED UINT length)
{
	TEST_PLUGIN* plugin = g_TestPlugin;
	if (event == CHANNEL_EVENT_CONNECTED &&
	    plugin->legacy.pVirtualChannelOpen(handle, &plugin->openHandle, CLIPRDR_SVC_CHANNEL_NAME,
	                                       test_plugin_open) != CHANNEL_RC_OK)
		InterlockedExchange(&plugin->error, 1);
}

static void VCAPITYPE test_plugin_init_ex(WINPR_ATTR_UNUSED LPVOID user, LPVOID handle, UINT event,
                                          WINPR_ATTR_UNUSED LPVOID data,
                                          WINPR_ATTR_UNUSED UINT length)
{
	TEST_PLUGIN* plugin = g_TestPlugin;
	if (event == CHANNEL_EVENT_CONNECTED &&
	    plugin->extended.pVirtualChannelOpenEx(handle, &plugin->openHandle,
	                                           CLIPRDR_SVC_CHANNEL_NAME,
	                                           test_open_event) != CHANNEL_RC_OK)
		InterlockedExchange(&plugin->error, 1);
}

static BOOL VCAPITYPE test_plugin_entry(PCHANNEL_ENTRY_POINTS entry)
{
	TEST_PLUGIN* plugin = g_TestPlugin;
	plugin->legacy = *(PCHANNEL_ENTRY_POINTS_FREERDP)entry;
	CHANNEL_DEF definition = WINPR_C_ARRAY_INIT;
	strcpy(definition.name, CLIPRDR_SVC_CHANNEL_NAME);
	definition.options = CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_SHOW_PROTOCOL;
	return plugin->legacy.pVirtualChannelInit(&plugin->initHandle, &definition, 1,
	                                          VIRTUAL_CHANNEL_VERSION_WIN2000,
	                                          test_plugin_init) == CHANNEL_RC_OK;
}

static BOOL VCAPITYPE test_plugin_entry_ex(PCHANNEL_ENTRY_POINTS_EX entry, LPVOID handle)
{
	TEST_PLUGIN* plugin = g_TestPlugin;
	plugin->extended = *(PCHANNEL_ENTRY_POINTS_FREERDP_EX)entry;
	plugin->initHandle = handle;
	CHANNEL_DEF definition = WINPR_C_ARRAY_INIT;
	strcpy(definition.name, CLIPRDR_SVC_CHANNEL_NAME);
	definition.options = CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_SHOW_PROTOCOL;
	return plugin->extended.pVirtualChannelInitEx(plugin->test, plugin->test, handle, &definition,
	                                              1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	                                              test_plugin_init_ex) == CHANNEL_RC_OK;
}

#define TEST_PRODUCERS 4
#define TEST_MESSAGES_PER_PRODUCER 250

static DWORD WINAPI test_plugin_producer(LPVOID arg)
{
	TEST_PLUGIN* plugin = arg;
	TEST_CONTEXT* test = plugin->test;
	for (size_t x = 0; x < TEST_MESSAGES_PER_PRODUCER; x++)
	{
		const UINT rc =
		    plugin->extendedApi
		        ? plugin->extended.pVirtualChannelWriteEx(plugin->initHandle, plugin->openHandle,
		                                                  test->data, (UINT32)test->length, nullptr)
		        : plugin->legacy.pVirtualChannelWrite(plugin->openHandle, test->data,
		                                              (ULONG)test->length, nullptr);
		if (rc != CHANNEL_RC_OK)
		{
			InterlockedExchange(&plugin->error, 1);
			return 1;
		}
		if (x % 19 == 0)
			Sleep(0);
	}
	return 0;
}

static BOOL test_plugin_writes(BOOL extendedApi, size_t length)
{
	BOOL rc = FALSE;
	HANDLE threads[TEST_PRODUCERS] = WINPR_C_ARRAY_INIT;
	size_t started = 0;
	TEST_PLUGIN plugin = { .extendedApi = extendedApi };
	plugin.test = test_new(length, CLIPRDR_SVC_CHANNEL_NAME, TRUE);
	if (!plugin.test)
		return FALSE;
	TEST_CONTEXT* test = plugin.test;
	test->expectedMessages = TEST_PRODUCERS * TEST_MESSAGES_PER_PRODUCER;
#if !defined(WITH_RDPEUDP)
	test->requireInterleaving = (length > TEST_CHUNK_SIZE);
#endif
	rdpContext* context = &test->common.context;
	rdpChannels* channels = context->channels;
	/* Replace the direct fixture with registration through the actual plugin API. */
	channels->openDataCount = 0;
	memset(&channels->openDataList[0], 0, sizeof(CHANNEL_OPEN_DATA));
	g_TestPlugin = &plugin;
	freerdp_channels_register_instance(channels, context->instance);
	if (!freerdp_settings_set_string(context->settings, FreeRDP_ServerHostname, "test.invalid"))
		goto out;
	const int loaded = extendedApi ? freerdp_channels_client_load_ex(channels, context->settings,
	                                                                 test_plugin_entry_ex, test)
	                               : freerdp_channels_client_load(channels, context->settings,
	                                                              test_plugin_entry, test);
	if (loaded || freerdp_channels_post_connect(channels, context->instance) != CHANNEL_RC_OK ||
	    !plugin.openHandle || InterlockedCompareExchange(&plugin.error, 0, 0))
		goto out;
	for (; started < TEST_PRODUCERS; started++)
	{
		threads[started] = CreateThread(nullptr, 0, test_plugin_producer, &plugin, 0, nullptr);
		if (!threads[started])
			goto out;
	}
	const UINT64 deadline = GetTickCount64() + 10000;
	while (test->completed < test->expectedMessages && GetTickCount64() < deadline &&
	       !InterlockedCompareExchange(&plugin.error, 0, 0))
	{
		const DWORD status =
		    WaitForSingleObject(freerdp_channels_get_event_handle(context->instance), 100);
		if (status == WAIT_TIMEOUT)
			continue;
		if (status != WAIT_OBJECT_0 || !freerdp_check_event_handles(context))
			goto out;
	}
	rc = !InterlockedCompareExchange(&plugin.error, 0, 0) && !test->invalid &&
	     test->completed == test->expectedMessages && test->cancelled == 0 &&
	     !MessageQueue_Size(channels->queue) && !MessageQueue_Size(channels->clipboardQueue);
out:
	for (size_t x = 0; x < started; x++)
	{
		if (WaitForSingleObject(threads[x], INFINITE) != WAIT_OBJECT_0)
			rc = FALSE;
		CloseHandle(threads[x]);
	}
	if (!rc)
		fprintf(stderr, "concurrent VirtualChannelWrite%s failed: size=%zu completed=%zu/%zu\n",
		        extendedApi ? "Ex" : "", length, test->completed, test->expectedMessages);
	test_free(test);
	g_TestPlugin = nullptr;
	return rc;
}

int TestClientClipboardWrite(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	/* A forwarding override exercises the unchanged, whole-message sender as a negative
	 * control. It must fail the same interleaving assertion that protects the new path. */
	if (argc > 1 && strcmp(argv[1], "--legacy") == 0)
		return test_transfer(TEST_CHUNK_SIZE + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE, 3) ? 0 : -1;
	if (!test_transfer(TEST_CHUNK_SIZE - 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE, 0) ||
	    !test_transfer(TEST_CHUNK_SIZE, CLIPRDR_SVC_CHANNEL_NAME, FALSE, 0) ||
	    !test_transfer(TEST_CHUNK_SIZE + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE, 0) ||
	    !test_transfer(TEST_CHUNK_SIZE * 3, CLIPRDR_SVC_CHANNEL_NAME, FALSE, 0) ||
	    !test_transfer(11541760, CLIPRDR_SVC_CHANNEL_NAME, TRUE, 0) ||
	    !test_transfer(TEST_CHUNK_SIZE * 3 + 1, "rdpdr", TRUE, 0) ||
	    !test_transfer(TEST_CHUNK_SIZE * 3 + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE, 1) ||
	    !test_transfer(TEST_CHUNK_SIZE * 3 + 1, CLIPRDR_SVC_CHANNEL_NAME, TRUE, 2))
		return -1;
#if !defined(WITH_RDPEUDP)
	if (!test_cancel_or_fail(FALSE) || !test_cancel_or_fail(TRUE) || !test_control_and_order() ||
	    !test_completion_clears_queue() || !test_disconnect() || !test_change_during_write(FALSE) ||
	    !test_change_during_write(TRUE))
		return -1;
#endif
	for (size_t round = 0; round < 2; round++)
	{
		if (!test_plugin_writes(FALSE, 1) || !test_plugin_writes(FALSE, TEST_CHUNK_SIZE + 1) ||
		    !test_plugin_writes(TRUE, 1) || !test_plugin_writes(TRUE, TEST_CHUNK_SIZE + 1))
			return -1;
	}
	return 0;
}
