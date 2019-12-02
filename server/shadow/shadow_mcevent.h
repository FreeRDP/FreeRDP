/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
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

#ifndef FREERDP_SERVER_SHADOW_MCEVENT_H
#define FREERDP_SERVER_SHADOW_MCEVENT_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/collections.h>

/*
 * This file implemented a model that an event is consumed
 * by multiple clients. All clients should wait others before continue
 * Server should wait for all clients before continue
 */

#ifdef __cplusplus
extern "C"
{
#endif

	rdpShadowMultiClientEvent* shadow_multiclient_new(void);
	void shadow_multiclient_free(rdpShadowMultiClientEvent* event);
	void shadow_multiclient_publish(rdpShadowMultiClientEvent* event);
	void shadow_multiclient_wait(rdpShadowMultiClientEvent* event);
	void shadow_multiclient_publish_and_wait(rdpShadowMultiClientEvent* event);
	void* shadow_multiclient_get_subscriber(rdpShadowMultiClientEvent* event);
	void shadow_multiclient_release_subscriber(void* subscriber);
	BOOL shadow_multiclient_consume(void* subscriber);
	HANDLE shadow_multiclient_getevent(void* subscriber);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_MCEVENT_H */
