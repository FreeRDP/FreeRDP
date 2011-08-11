/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Virtual Channel Manager
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#ifndef __FREERDP_CHANMAN_H
#define __FREERDP_CHANMAN_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rdp_chan_man rdpChanMan;

FREERDP_API int freerdp_chanman_global_init(void);
FREERDP_API int freerdp_chanman_global_uninit(void);
FREERDP_API rdpChanMan* freerdp_chanman_new(void);
FREERDP_API void freerdp_chanman_free(rdpChanMan* chan_man);
FREERDP_API int freerdp_chanman_load_plugin(rdpChanMan* chan_man, rdpSettings* settings,
	const char* name, void* data);
FREERDP_API int freerdp_chanman_pre_connect(rdpChanMan* chan_man, freerdp* instance);
FREERDP_API int freerdp_chanman_post_connect(rdpChanMan* chan_man, freerdp* instance);
FREERDP_API int freerdp_chanman_data(freerdp* instance, int chan_id, void* data, int data_size,
	int flags, int total_size);
FREERDP_API int freerdp_chanman_send_event(rdpChanMan* chan_man, FRDP_EVENT* event);
FREERDP_API boolean freerdp_chanman_get_fds(rdpChanMan* chan_man, freerdp* instance, void** read_fds,
	int* read_count, void** write_fds, int* write_count);
FREERDP_API boolean freerdp_chanman_check_fds(rdpChanMan* chan_man, freerdp* instance);
FREERDP_API FRDP_EVENT* freerdp_chanman_pop_event(rdpChanMan* chan_man);
FREERDP_API void freerdp_chanman_close(rdpChanMan* chan_man, freerdp* instance);

#ifdef __cplusplus
}
#endif

#endif
