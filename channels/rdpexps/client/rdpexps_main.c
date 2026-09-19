/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * XPS Print Virtual Channel Extension
 */
#include <freerdp/config.h>

#include <stdlib.h>
#include <inttypes.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>

#include "rdpexps_common.h"
#include "rdpexps_main.h"

#define TAG CHANNELS_TAG("rdpexps.client")
#define RIMCALL_RELEASE 0x00000001
#define RIMCALL_QUERYINTERFACE 0x00000002

typedef struct
{
	GENERIC_CHANNEL_CALLBACK base;
	BOOL ticket;
	UINT32 printerId;
	BOOL printerBound;
	BOOL printerInitialized;
} RDPEXPS_CHANNEL_CALLBACK;

typedef struct
{
	GENERIC_LISTENER_CALLBACK base;
	BOOL ticket;
} RDPEXPS_LISTENER_CALLBACK;

typedef struct
{
	IWTSPlugin iface;
	IWTSListener* ticketListener;
	IWTSListener* driverListener;
	GENERIC_LISTENER_CALLBACK* ticketCallback;
	GENERIC_LISTENER_CALLBACK* driverCallback;
} RDPEXPS_PLUGIN;

static UINT rdpexps_send_failure(IWTSVirtualChannel* channel,
	                               const RDPEXPS_REQUEST_HEADER* request)
{
	wStream* response = nullptr;
	UINT status = CHANNEL_RC_NO_MEMORY;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(request);
	response = Stream_New(nullptr, 8);
	if (!response)
		return CHANNEL_RC_NO_MEMORY;
	if (rdpexps_write_response_header(response, request))
		status = channel->Write(channel, (ULONG)Stream_GetPosition(response), Stream_Buffer(response),
		                        nullptr);
	Stream_Free(response, TRUE);
	return status;
}

static UINT rdpexps_send_ticket_response(IWTSVirtualChannel* channel,
	                                        const RDPEXPS_REQUEST_HEADER* request, UINT32 functionId)
{
	wStream* response = Stream_New(nullptr, 24);
	UINT status = CHANNEL_RC_NO_MEMORY;

	if (!response)
		return CHANNEL_RC_NO_MEMORY;
	WINPR_UNUSED(functionId);
	if (!rdpexps_write_ticket_response(response, request))
		goto out;
	status = channel->Write(channel, (ULONG)Stream_GetPosition(response), Stream_Buffer(response),
	                        nullptr);
out:
	Stream_Free(response, TRUE);
	return status;
}

static UINT rdpexps_send_driver_response(IWTSVirtualChannel* channel,
	                                        const RDPEXPS_REQUEST_HEADER* request, UINT32 functionId)
{
	wStream* response = Stream_New(nullptr, 16);
	UINT status = CHANNEL_RC_NO_MEMORY;

	if (!response)
		return CHANNEL_RC_NO_MEMORY;
	WINPR_UNUSED(functionId);
	if (!rdpexps_write_driver_response(response, request))
		goto out;
	status = channel->Write(channel, (ULONG)Stream_GetPosition(response), Stream_Buffer(response),
	                        nullptr);
out:
	Stream_Free(response, TRUE);
	return status;
}

static UINT rdpexps_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	RDPEXPS_CHANNEL_CALLBACK* callback = (RDPEXPS_CHANNEL_CALLBACK*)pChannelCallback;
	RDPEXPS_REQUEST_HEADER request = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(callback);
	WINPR_ASSERT(callback->base.channel);
	if (!rdpexps_read_request_header(data, &request))
		return ERROR_INVALID_DATA;

	/* MS-RDPEXPS requires a header-only failure response for an unsupported QI request. */
	if (request.FunctionId == RIMCALL_QUERYINTERFACE)
	{
		if (Stream_GetRemainingLength(data) != 16)
			return ERROR_INVALID_DATA;
		return rdpexps_send_failure(callback->base.channel, &request);
	}
	if (request.FunctionId == RIMCALL_RELEASE)
	{
		if (Stream_GetRemainingLength(data) != 0)
			return ERROR_INVALID_DATA;
		return CHANNEL_RC_OK;
	}
	if (callback->ticket && (request.InterfaceId == 0) &&
	    ((request.FunctionId >= 0x00000100) && (request.FunctionId <= 0x00000102)))
	{
		const size_t expected = (request.FunctionId == 0x00000100) ? 4 :
		                        (request.FunctionId == 0x00000101) ? 8 : 0;
		if (Stream_GetRemainingLength(data) != expected)
			return ERROR_INVALID_DATA;
		if (request.FunctionId == 0x00000100)
		{
			Stream_Read_UINT32(data, callback->printerId);
			if (callback->printerId == 0)
				return ERROR_INVALID_DATA;
			callback->printerBound = FALSE;
		}
		else if (request.FunctionId == 0x00000101)
		{
			UINT32 printerId = 0;
			UINT32 version = 0;
			Stream_Read_UINT32(data, printerId);
			Stream_Read_UINT32(data, version);
			if ((printerId != callback->printerId) || (version != 1))
				return ERROR_INVALID_DATA;
			callback->printerBound = TRUE;
		}
		else if (!callback->printerBound)
			return ERROR_INVALID_DATA;
		return rdpexps_send_ticket_response(callback->base.channel, &request, request.FunctionId);
	}
	if (!callback->ticket && (request.InterfaceId == 0) &&
	    ((request.FunctionId == 0x00000100) || (request.FunctionId == 0x00000101)))
	{
		if ((request.FunctionId == 0x00000100) && (Stream_GetRemainingLength(data) == 4))
		{
			Stream_Read_UINT32(data, callback->printerId);
			if (callback->printerId == 0)
				return ERROR_INVALID_DATA;
			callback->printerInitialized = TRUE;
		}
		else if ((request.FunctionId == 0x00000101) && callback->printerInitialized &&
		         (Stream_GetRemainingLength(data) == 0))
		{
			/* no request payload */
		}
		else
			return ERROR_INVALID_DATA;
		return rdpexps_send_driver_response(callback->base.channel, &request, request.FunctionId);
	}

	WLog_WARN(TAG, "received unsupported request %#" PRIx32 " on interface %#" PRIx32 "",
	          request.FunctionId, request.InterfaceId);
	/* An unknown function is a newer protocol version and requires a header-only failure reply. */
	return rdpexps_send_failure(callback->base.channel, &request);
}

static UINT rdpexps_on_close(IWTSVirtualChannelCallback* callback)
{
	free(callback);
	return CHANNEL_RC_OK;
}

static UINT rdpexps_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	                                              IWTSVirtualChannel* channel,
	                                              WINPR_ATTR_UNUSED BYTE* data,
	                                              WINPR_ATTR_UNUSED BOOL* accept,
	                                              IWTSVirtualChannelCallback** callback)
{
	RDPEXPS_LISTENER_CALLBACK* listener = (RDPEXPS_LISTENER_CALLBACK*)pListenerCallback;
	RDPEXPS_CHANNEL_CALLBACK* channelCallback = nullptr;

	WINPR_ASSERT(listener);
	WINPR_ASSERT(channel);
	WINPR_ASSERT(callback);
	channelCallback = calloc(1, sizeof(*channelCallback));
	if (!channelCallback)
		return CHANNEL_RC_NO_MEMORY;
	channelCallback->base.iface.OnDataReceived = rdpexps_on_data_received;
	channelCallback->base.iface.OnClose = rdpexps_on_close;
	channelCallback->base.plugin = listener->base.plugin;
	channelCallback->base.channel_mgr = listener->base.channel_mgr;
	channelCallback->base.channel = channel;
	channelCallback->ticket = listener->ticket;
	listener->base.channel_callback = &channelCallback->base;
	*callback = &channelCallback->base.iface;
	return CHANNEL_RC_OK;
}

static UINT rdpexps_create_listener(RDPEXPS_PLUGIN* plugin, IWTSVirtualChannelManager* manager,
	                                   const char* name, GENERIC_LISTENER_CALLBACK** callback,
	                                   IWTSListener** listener, BOOL ticket)
{
	UINT status = CHANNEL_RC_NO_MEMORY;

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(manager);
	WINPR_ASSERT(name);
	WINPR_ASSERT(callback);
	WINPR_ASSERT(listener);
	RDPEXPS_LISTENER_CALLBACK* rdpexpsCallback = calloc(1, sizeof(*rdpexpsCallback));
	if (!rdpexpsCallback)
		return CHANNEL_RC_NO_MEMORY;
	*callback = &rdpexpsCallback->base;
	rdpexpsCallback->base.iface.OnNewChannelConnection = rdpexps_on_new_channel_connection;
	rdpexpsCallback->base.plugin = &plugin->iface;
	rdpexpsCallback->base.channel_mgr = manager;
	rdpexpsCallback->ticket = ticket;
	status = manager->CreateListener(manager, name, 0, &(*callback)->iface, listener);
	if (status == CHANNEL_RC_OK)
		(*listener)->pInterface = plugin->iface.pInterface;
	return status;
}

static UINT rdpexps_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* manager)
{
	RDPEXPS_PLUGIN* plugin = (RDPEXPS_PLUGIN*)pPlugin;
	UINT status = CHANNEL_RC_OK;

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(manager);
	status = rdpexps_create_listener(plugin, manager, RDPEXPS_TICKET_CHANNEL_NAME,
	                                 &plugin->ticketCallback, &plugin->ticketListener, TRUE);
	if (status != CHANNEL_RC_OK)
		return status;
	return rdpexps_create_listener(plugin, manager, RDPEXPS_DRIVER_CHANNEL_NAME,
	                               &plugin->driverCallback, &plugin->driverListener, FALSE);
}

static UINT rdpexps_terminated(IWTSPlugin* pPlugin)
{
	RDPEXPS_PLUGIN* plugin = (RDPEXPS_PLUGIN*)pPlugin;
	if (!plugin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	if (plugin->ticketCallback && plugin->ticketCallback->channel_mgr && plugin->ticketListener)
		IFCALL(plugin->ticketCallback->channel_mgr->DestroyListener,
		       plugin->ticketCallback->channel_mgr, plugin->ticketListener);
	if (plugin->driverCallback && plugin->driverCallback->channel_mgr && plugin->driverListener)
		IFCALL(plugin->driverCallback->channel_mgr->DestroyListener,
		       plugin->driverCallback->channel_mgr, plugin->driverListener);
	free(plugin->ticketCallback);
	free(plugin->driverCallback);
	free(plugin->iface.pInterface);
	free(plugin);
	return CHANNEL_RC_OK;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE rdpexps_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* entryPoints))
{
	RDPEXPS_PLUGIN* plugin = nullptr;
	UINT status = CHANNEL_RC_INITIALIZATION_ERROR;

	WINPR_ASSERT(entryPoints);
	WINPR_ASSERT(entryPoints->GetPlugin);
	WINPR_ASSERT(entryPoints->RegisterPlugin);
	if (entryPoints->GetPlugin(entryPoints, "rdpexps"))
		return CHANNEL_RC_ALREADY_INITIALIZED;
	plugin = calloc(1, sizeof(*plugin));
	if (!plugin)
		return CHANNEL_RC_NO_MEMORY;
	plugin->iface.Initialize = rdpexps_initialize;
	plugin->iface.Terminated = rdpexps_terminated;
	status = entryPoints->RegisterPlugin(entryPoints, "rdpexps", &plugin->iface);
	if (status != CHANNEL_RC_OK)
		rdpexps_terminated(&plugin->iface);
	return status;
}
