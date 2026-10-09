/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for the rdpgfx (graphics pipeline) dynamic channel client
 *
 * The built-in rdpgfx DVC plugin is loaded through its DVCPluginEntry with a
 * stub dynamic channel manager, a channel is opened (sending the client caps)
 * and server PDUs are delivered through the channel's OnDataReceived callback.
 *
 * Input: [mode:1] followed by records [length:2 LE][data].
 * If bit 0 of mode is clear each record is wrapped into an uncompressed ZGFX
 * segment so the fuzzer can focus on graphics PDUs, otherwise the record is
 * passed as-is and also goes through ZGFX decompression.
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include <freerdp/addin.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/codec/zgfx.h>
#include <freerdp/dvc.h>

#define FUZZ_MAX_RECORDS 64

typedef struct
{
	IDRDYNVC_ENTRY_POINTS iface;
	rdpContext* context;
	IWTSPlugin* plugin;
} fuzz_entry_points;

typedef struct
{
	IWTSVirtualChannelManager iface;
	IWTSListener listener;
	IWTSListenerCallback* listenerCallback;
} fuzz_channel_manager;

static UINT fuzz_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                 WINPR_ATTR_UNUSED const char* name, IWTSPlugin* pPlugin)
{
	fuzz_entry_points* ep = (fuzz_entry_points*)pEntryPoints;
	if (ep->plugin)
		return CHANNEL_RC_ALREADY_INITIALIZED;
	ep->plugin = pPlugin;
	return CHANNEL_RC_OK;
}

static IWTSPlugin* fuzz_get_plugin(WINPR_ATTR_UNUSED IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                   WINPR_ATTR_UNUSED const char* name)
{
	return nullptr;
}

static const ADDIN_ARGV* fuzz_get_plugin_data(WINPR_ATTR_UNUSED IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return nullptr;
}

static rdpSettings* fuzz_get_rdp_settings(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((fuzz_entry_points*)pEntryPoints)->context->settings;
}

static rdpContext* fuzz_get_rdp_context(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((fuzz_entry_points*)pEntryPoints)->context;
}

static UINT fuzz_create_listener(IWTSVirtualChannelManager* pChannelMgr,
                                 WINPR_ATTR_UNUSED const char* pszChannelName,
                                 WINPR_ATTR_UNUSED ULONG ulFlags,
                                 IWTSListenerCallback* pListenerCallback, IWTSListener** ppListener)
{
	fuzz_channel_manager* mgr = (fuzz_channel_manager*)pChannelMgr;
	mgr->listenerCallback = pListenerCallback;
	if (ppListener)
		*ppListener = &mgr->listener;
	return CHANNEL_RC_OK;
}

static UINT fuzz_destroy_listener(IWTSVirtualChannelManager* pChannelMgr,
                                  WINPR_ATTR_UNUSED IWTSListener* pListener)
{
	((fuzz_channel_manager*)pChannelMgr)->listenerCallback = nullptr;
	return CHANNEL_RC_OK;
}

static UINT32 fuzz_get_channel_id(WINPR_ATTR_UNUSED IWTSVirtualChannel* channel)
{
	return 1;
}

static const char* fuzz_get_channel_name(WINPR_ATTR_UNUSED IWTSVirtualChannel* channel)
{
	return RDPGFX_DVC_CHANNEL_NAME;
}

static IWTSVirtualChannel* fuzz_find_channel_by_id(WINPR_ATTR_UNUSED IWTSVirtualChannelManager* mgr,
                                                   WINPR_ATTR_UNUSED UINT32 ChannelId)
{
	return nullptr;
}

static UINT fuzz_channel_write(WINPR_ATTR_UNUSED IWTSVirtualChannel* pChannel,
                               WINPR_ATTR_UNUSED ULONG cbSize,
                               WINPR_ATTR_UNUSED const BYTE* pBuffer,
                               WINPR_ATTR_UNUSED void* pReserved)
{
	return CHANNEL_RC_OK;
}

static UINT fuzz_channel_close(WINPR_ATTR_UNUSED IWTSVirtualChannel* pChannel)
{
	return CHANNEL_RC_OK;
}

static void fuzz_feed(IWTSVirtualChannelCallback* callback, const uint8_t* data, size_t size)
{
	wStream buffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&buffer, data, size);

	if (Stream_GetRemainingLength(s) < 1)
		return;
	const BOOL raw = (Stream_Get_UINT8(s) & 0x01) != 0;

	wStream* pdu = Stream_New(nullptr, UINT16_MAX + 2);
	if (!pdu)
		return;

	for (size_t index = 0; index < FUZZ_MAX_RECORDS; index++)
	{
		if (Stream_GetRemainingLength(s) < 2)
			break;

		size_t length = Stream_Get_UINT16(s);
		if (length > Stream_GetRemainingLength(s))
			length = Stream_GetRemainingLength(s);

		Stream_ResetPosition(pdu);
		if (!raw)
		{
			Stream_Write_UINT8(pdu, ZGFX_SEGMENTED_SINGLE);
			Stream_Write_UINT8(pdu, ZGFX_PACKET_COMPR_TYPE_RDP8);
		}
		Stream_Copy(s, pdu, length);
		Stream_SealLength(pdu);
		Stream_ResetPosition(pdu);

		if (callback->OnDataReceived(callback, pdu) != CHANNEL_RC_OK)
			break;
	}

	Stream_Free(pdu, TRUE);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (1u << 20))
		return 0;

	wLog* root = WLog_GetRoot();
	(void)WLog_SetLogLevel(root, WLOG_TRACE);
	(void)WLog_SetLogAppenderType(root, WLOG_APPENDER_CALLBACK);

	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return 0;

	PDVC_PLUGIN_ENTRY pluginEntry = WINPR_FUNC_PTR_CAST(
	    freerdp_channels_load_static_addin_entry(RDPGFX_CHANNEL_NAME, nullptr, nullptr,
	                                             FREERDP_ADDIN_CHANNEL_DYNAMIC),
	    PDVC_PLUGIN_ENTRY);

	fuzz_entry_points ep = WINPR_C_ARRAY_INIT;
	ep.iface.RegisterPlugin = fuzz_register_plugin;
	ep.iface.GetPlugin = fuzz_get_plugin;
	ep.iface.GetPluginData = fuzz_get_plugin_data;
	ep.iface.GetRdpSettings = fuzz_get_rdp_settings;
	ep.iface.GetRdpContext = fuzz_get_rdp_context;
	ep.context = context;

	if (!pluginEntry || (pluginEntry(&ep.iface) != CHANNEL_RC_OK) || !ep.plugin)
		goto fail;

	{
		IWTSPlugin* plugin = ep.plugin;

		fuzz_channel_manager mgr = WINPR_C_ARRAY_INIT;
		mgr.iface.CreateListener = fuzz_create_listener;
		mgr.iface.DestroyListener = fuzz_destroy_listener;
		mgr.iface.GetChannelId = fuzz_get_channel_id;
		mgr.iface.GetChannelName = fuzz_get_channel_name;
		mgr.iface.FindChannelById = fuzz_find_channel_by_id;

		IWTSVirtualChannel channel = WINPR_C_ARRAY_INIT;
		channel.Write = fuzz_channel_write;
		channel.Close = fuzz_channel_close;

		if ((plugin->Initialize(plugin, &mgr.iface) == CHANNEL_RC_OK) && mgr.listenerCallback)
		{
			BOOL accept = TRUE;
			IWTSVirtualChannelCallback* callback = nullptr;

			if ((mgr.listenerCallback->OnNewChannelConnection(mgr.listenerCallback, &channel,
			                                                  nullptr, &accept,
			                                                  &callback) == CHANNEL_RC_OK) &&
			    callback)
			{
				if (callback->OnOpen(callback) == CHANNEL_RC_OK)
					fuzz_feed(callback, data, size);
				/* frees the channel callback */
				(void)callback->OnClose(callback);
			}
		}

		/* frees the plugin */
		(void)plugin->Terminated(plugin);
	}

fail:
	freerdp_client_context_free(context);
	return 0;
}
