/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNELS_CLIENT_H
#define FREERDP_CHANNELS_CLIENT_H

#include <winpr/wtypes.h>

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/config.h>
#include <freerdp/addin.h>
#include <freerdp/channels/channels.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief the supported options for \ref freerdp_generic_DVCPluginEntryWithOptions
	 *  @since version 3.33.0
	 */
	typedef enum WINPR_C23_ENUM_TYPE(uint32_t)
	{
		GENERIC_CHANNEL_ENTRY_OPTION_NONE = 0,
		GENERIC_CHANNEL_ENTRY_OPTION_MULTI_INSTANCE = 1
	} GENERIC_CHANNEL_ENTRY_OPTIONS;

	typedef struct
	{
		IWTSVirtualChannelCallback iface;
		IWTSPlugin* plugin;
		IWTSVirtualChannelManager* channel_mgr;
		IWTSVirtualChannel* channel;
	} GENERIC_CHANNEL_CALLBACK;

	typedef struct
	{
		IWTSListenerCallback iface;
		IWTSPlugin* plugin;
		IWTSVirtualChannelManager* channel_mgr;
		IWTSVirtualChannel* channel;
		GENERIC_CHANNEL_CALLBACK* channel_callback;
	} GENERIC_LISTENER_CALLBACK;

	typedef struct GENERIC_DYNVC_PLUGIN GENERIC_DYNVC_PLUGIN;
	typedef UINT (*DYNVC_PLUGIN_INIT_FN)(GENERIC_DYNVC_PLUGIN* plugin, rdpContext* context,
	                                     rdpSettings* settings);
	typedef void (*DYNVC_PLUGIN_TERMINATE_FN)(GENERIC_DYNVC_PLUGIN* plugin);

	struct GENERIC_DYNVC_PLUGIN
	{
		IWTSPlugin iface;
		GENERIC_LISTENER_CALLBACK* listener_callback;
		IWTSListener* listener;
		BOOL attached;
		BOOL initialized;
		wLog* log;
		char* dynvc_name;
		size_t channelCallbackSize;
		const IWTSVirtualChannelCallback* channel_callbacks;
		DYNVC_PLUGIN_TERMINATE_FN terminatePluginFn;
	};

#if defined(WITH_CHANNELS)
	WINPR_ATTR_NODISCARD
	FREERDP_API void* freerdp_channels_client_find_static_entry(const char* name,
	                                                            const char* identifier);

	WINPR_ATTR_NODISCARD
	FREERDP_API PVIRTUALCHANNELENTRY freerdp_channels_load_static_addin_entry(LPCSTR pszName,
	                                                                          LPCSTR pszSubsystem,
	                                                                          LPCSTR pszType,
	                                                                          DWORD dwFlags);

	WINPR_ATTR_NODISCARD
	FREERDP_API FREERDP_ADDIN** freerdp_channels_list_addins(LPCSTR pszName, LPCSTR pszSubsystem,
	                                                         LPCSTR pszType, DWORD dwFlags);
	FREERDP_API void freerdp_channels_addin_list_free(FREERDP_ADDIN** ppAddins);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_initialize_generic_dynvc_plugin(GENERIC_DYNVC_PLUGIN* plugin);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT freerdp_generic_DVCPluginEntry(
	    IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* logTag, const char* name,
	    size_t pluginSize, size_t channelCallbackSize,
	    const IWTSVirtualChannelCallback* channel_callbacks, DYNVC_PLUGIN_INIT_FN initPluginFn,
	    DYNVC_PLUGIN_TERMINATE_FN terminatePluginFn);

	/** @brief Same as \ref freerdp_generic_DVCPluginEntry but allowing more fine grained
	 * configuration.
	 *
	 *  @param pEntryPoints A pointer to the entry points to call, must not be NULL
	 *  @param logTag A string to use to identify log messages, must not be NULL
	 *  @param name The name of the channel, must not be NULL. Must not be reused for a second call.
	 *  @param pluginSize The size of the plugin struct in bytes
	 *  @param channelCallbackSize The size of the callback struct in bytes
	 *  @param channel_callbacks A pointer to the channel callbacks to use, must not be NULL
	 *  @param initPluginFn A function pointer called to initialize the channel, must not be NULL
	 *  @param terminatePluginFn A function pointer called on channel termination, must not be NULL
	 *  @param flags Configuration flags that can be passed. See \ref GENERIC_CHANNEL_ENTRY_OPTIONS
	 * for flags available.
	 *
	 *  @return \ref CHANNEL_RC_OK for success, a specific failure code otherwise.
	 *
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API UINT freerdp_generic_DVCPluginEntryWithOptions(
	    IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* logTag, const char* name,
	    size_t pluginSize, size_t channelCallbackSize,
	    const IWTSVirtualChannelCallback* channel_callbacks, DYNVC_PLUGIN_INIT_FN initPluginFn,
	    DYNVC_PLUGIN_TERMINATE_FN terminatePluginFn, UINT32 flags);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNELS_CLIENT_H */
