/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <stdio.h>
#include <winpr/crt.h>
#include <freerdp/client/channels.h>

typedef struct
{
	GENERIC_DYNVC_PLUGIN base;
	BYTE extension[128];
} TEST_DYNVC_PLUGIN;

static IWTSPlugin* registeredPlugin;
static IWTSListener listener;
static IWTSListenerCallback* listenerCallback;
static BOOL poisonExtension;

static IWTSPlugin* get_plugin(IDRDYNVC_ENTRY_POINTS* entry, const char* name)
{
	WINPR_UNUSED(entry);
	WINPR_UNUSED(name);
	return registeredPlugin;
}

static UINT register_plugin(IDRDYNVC_ENTRY_POINTS* entry, const char* name, IWTSPlugin* plugin)
{
	WINPR_UNUSED(entry);
	WINPR_UNUSED(name);
	registeredPlugin = plugin;
	return CHANNEL_RC_OK;
}

static rdpSettings* get_settings(IDRDYNVC_ENTRY_POINTS* entry)
{
	WINPR_UNUSED(entry);
	return nullptr;
}

static rdpContext* get_context(IDRDYNVC_ENTRY_POINTS* entry)
{
	WINPR_UNUSED(entry);
	return nullptr;
}

static UINT create_listener(IWTSVirtualChannelManager* manager, const char* name, ULONG flags,
                            IWTSListenerCallback* callback, IWTSListener** result)
{
	WINPR_UNUSED(manager);
	WINPR_UNUSED(name);
	WINPR_UNUSED(flags);
	listenerCallback = callback;
	*result = &listener;
	return CHANNEL_RC_OK;
}

static UINT destroy_listener(IWTSVirtualChannelManager* manager, IWTSListener* value)
{
	WINPR_UNUSED(manager);
	WINPR_UNUSED(value);
	listenerCallback = nullptr;
	return CHANNEL_RC_OK;
}

static UINT init_plugin(GENERIC_DYNVC_PLUGIN* base, rdpContext* context, rdpSettings* settings)
{
	TEST_DYNVC_PLUGIN* plugin = (TEST_DYNVC_PLUGIN*)base;
	WINPR_UNUSED(context);
	WINPR_UNUSED(settings);
	if (poisonExtension)
	{
		/* A stray '*' followed by ':' reproduced the live RDPGFX hang. Also
		 * cover repeated stars and a longer prefix that differs near its end. */
		memset(plugin->extension, '*', sizeof(plugin->extension));
		plugin->extension[1] = ':';
		memcpy(&plugin->extension[32], "**GENERIC_DYNVC_PLUGIN_PRIVATX*", 29);
	}
	return CHANNEL_RC_OK;
}

static UINT close_channel(IWTSVirtualChannelCallback* callback)
{
	WINPR_UNUSED(callback);
	return CHANNEL_RC_OK;
}

#define CHECK(condition)                                                      \
	do                                                                        \
	{                                                                         \
		if (!(condition))                                                     \
		{                                                                     \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);     \
			goto out;                                                         \
		}                                                                     \
	} while (0)

static int test_channel(UINT32 flags, BOOL poison)
{
	int result = -1;
	IWTSVirtualChannel channel = WINPR_C_ARRAY_INIT;
	IWTSVirtualChannelCallback* first = nullptr;
	IWTSVirtualChannelCallback* second = nullptr;
	IWTSVirtualChannelCallback* reopened = nullptr;
	BOOL accept = TRUE;
	const IWTSVirtualChannelCallback callbacks = { .OnClose = close_channel };
	IWTSVirtualChannelManager manager = { .CreateListener = create_listener,
	                                     .DestroyListener = destroy_listener };
	IDRDYNVC_ENTRY_POINTS entry = { .GetPlugin = get_plugin,
	                               .RegisterPlugin = register_plugin,
	                               .GetRdpSettings = get_settings,
	                               .GetRdpContext = get_context };
	poisonExtension = poison;
	CHECK(freerdp_generic_DVCPluginEntryWithOptions(
	          &entry, "com.freerdp.test.genericdynvc", "test-channel", sizeof(TEST_DYNVC_PLUGIN),
	          sizeof(GENERIC_CHANNEL_CALLBACK), &callbacks, init_plugin, nullptr, flags) == CHANNEL_RC_OK);
	CHECK(registeredPlugin->Initialize(registeredPlugin, &manager) == CHANNEL_RC_OK);
	CHECK(listenerCallback->OnNewChannelConnection(listenerCallback, &channel, nullptr,
	                                               &accept, &first) == CHANNEL_RC_OK);
	CHECK(first);
	const UINT secondStatus = listenerCallback->OnNewChannelConnection(
	    listenerCallback, &channel, nullptr, &accept, &second);
	if (flags & GENERIC_CHANNEL_ENTRY_OPTION_MULTI_INSTANCE)
		CHECK(secondStatus == CHANNEL_RC_OK && second);
	else
		CHECK(secondStatus == ERROR_DEVICE_ALREADY_ATTACHED && !second);
	CHECK(first->OnClose(first) == CHANNEL_RC_OK);
	free(first);
	first = nullptr;
	CHECK(listenerCallback->OnNewChannelConnection(listenerCallback, &channel, nullptr,
	                                               &accept, &reopened) == CHANNEL_RC_OK);
	CHECK(reopened && reopened->OnClose(reopened) == CHANNEL_RC_OK);
	free(reopened);
	reopened = nullptr;
	if (second)
		CHECK(second->OnClose(second) == CHANNEL_RC_OK);
	result = 0;
out:
	free(first);
	free(second);
	free(reopened);
	if (registeredPlugin && registeredPlugin->Terminated(registeredPlugin) != CHANNEL_RC_OK)
		result = -1;
	registeredPlugin = nullptr;
	return result;
}

int TestClientGenericDynvc(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	for (size_t i = 0; i < 2; i++)
	{
		if (test_channel(GENERIC_CHANNEL_ENTRY_OPTION_NONE, i != 0) != 0 ||
		    test_channel(GENERIC_CHANNEL_ENTRY_OPTION_MULTI_INSTANCE, i != 0) != 0)
			return -1;
	}
	return 0;
}
