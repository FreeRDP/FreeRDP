/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Test host for dynamic virtual channel plugins: loads a plugin and connects it to a channel
 * without drdynvc.
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

#ifndef FREERDP_CHANNELS_TEST_DVC_TEST_HOST_H
#define FREERDP_CHANNELS_TEST_DVC_TEST_HOST_H

#include <stdio.h>

#include <freerdp/addin.h>
#include <freerdp/dvc.h>

#define TEST_CHECK(cond)                                                             \
	do                                                                               \
	{                                                                                \
		if (!(cond))                                                                 \
		{                                                                            \
			(void)fprintf(stderr, "%s:%d: failed: %s\n", __func__, __LINE__, #cond); \
			return FALSE;                                                            \
		}                                                                            \
	} while (0)

typedef struct
{
	IDRDYNVC_ENTRY_POINTS iface;
	rdpContext* context;
	const ADDIN_ARGV* args;
	IWTSPlugin* plugin;
} dvc_test_entry_points;

typedef struct
{
	IWTSVirtualChannelManager iface;
	IWTSListener listener;
	IWTSListenerCallback* listenerCallback;
} dvc_test_channel_manager;

/* Loads the plugin of channel name with the plugin arguments args (may be nullptr). */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL dvc_test_load_plugin(dvc_test_entry_points* ep, rdpContext* context,
                                        const char* name, const ADDIN_ARGV* args);

/* A channel manager that keeps the listener the plugin creates. */
FREERDP_LOCAL void dvc_test_channel_manager_init(dvc_test_channel_manager* mgr);

/* A channel that accepts and drops every write; a test can replace Write. */
FREERDP_LOCAL void dvc_test_channel_init(IWTSVirtualChannel* channel);

/* Connects channel to the listener of an initialized plugin. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL dvc_test_open_channel(dvc_test_channel_manager* mgr, IWTSVirtualChannel* channel,
                                         IWTSVirtualChannelCallback** callback);

#endif /* FREERDP_CHANNELS_TEST_DVC_TEST_HOST_H */
