/*
   Android RAIL/RemoteApp Channel

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

#include <freerdp/config.h>

#include <stdlib.h>

#include <jni.h>

#include <freerdp/client/rail.h>
#include <freerdp/settings.h>
#include <freerdp/window.h>

#include "android_jni_callback.h"
#include "android_rail.h"

#define TAG CLIENT_TAG("android.rail")

static BOOL android_rail_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                           const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	if (!context || !orderInfo)
		return FALSE;

	const UINT32 flags = orderInfo->fieldFlags;

	if (flags & WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED)
	{
		androidContext* afc = (androidContext*)context;
		if (afc->rail && !afc->railExecSent)
		{
			const char* app =
			    freerdp_settings_get_string(context->settings, FreeRDP_RemoteApplicationProgram);
			if (app && *app)
			{
				afc->railExecSent = TRUE;
				WLog_DBG(TAG, "RAIL ARC_COMPLETED -> sending exec");
				UINT rc = client_rail_server_start_cmd(afc->rail);
				if (rc != CHANNEL_RC_OK)
				{
					afc->railExecSent = FALSE;
					WLog_ERR(TAG, "RAIL start cmd failed after ARC_COMPLETED: %u", rc);
				}
			}
		}
	}

	/* Forward z-order and active window together (windowIds NULL / activeWindowId 0 if absent). */
	const BOOL hasZOrder = (flags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER) && monitoredDesktop &&
	                       monitoredDesktop->numWindowIds > 0 && monitoredDesktop->windowIds;
	const BOOL hasActive = (flags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND) && monitoredDesktop;

	if (hasZOrder || hasActive)
	{
		JNIEnv* env = NULL;
		jboolean attached = jni_attach_thread(&env);
		if (env)
		{
			jlongArray arr = NULL;
			if (hasZOrder)
			{
				const jsize n = (jsize)monitoredDesktop->numWindowIds;
				arr = (*env)->NewLongArray(env, n);
				if (arr)
				{
					jlong* tmp = (*env)->GetLongArrayElements(env, arr, NULL);
					if (tmp)
					{
						for (jsize i = 0; i < n; i++)
							tmp[i] = (jlong)monitoredDesktop->windowIds[i];
						(*env)->ReleaseLongArrayElements(env, arr, tmp, 0);
					}
				}
			}
			const jlong activeWindowId = hasActive ? (jlong)monitoredDesktop->activeWindowId : 0;
			freerdp_callback("OnRailMonitoredDesktop", "(J[JJ)V", (jlong)context->instance, arr,
			                 activeWindowId);
			if (arr)
				(*env)->DeleteLocalRef(env, arr);
		}
		if (attached)
			jni_detach_thread();
	}

	return TRUE;
}

static UINT android_rail_server_system_param(RailClientContext* rail,
                                             const RAIL_SYSPARAM_ORDER* sysparam)
{
	if (!rail || !sysparam)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

static UINT android_rail_server_execute_result(RailClientContext* rail,
                                               const RAIL_EXEC_RESULT_ORDER* execResult)
{
	if (!rail || !execResult)
		return ERROR_INVALID_PARAMETER;

	if (execResult->execResult != 0)
		WLog_ERR(TAG, "RAIL execute failed: execResult=0x%04X rawResult=0x%08X",
		         execResult->execResult, execResult->rawResult);
	else
		WLog_DBG(TAG, "RAIL execute success");

	return CHANNEL_RC_OK;
}

static UINT android_rail_server_local_move_size(RailClientContext* rail,
                                                const RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	if (!rail || !localMoveSize)
		return ERROR_INVALID_PARAMETER;

	WLog_DBG(TAG, "RAIL window 0x%08X %s pos=(%d,%d)", localMoveSize->windowId,
	         localMoveSize->isMoveSizeStart ? "move/size start" : "move/size end",
	         localMoveSize->posX, localMoveSize->posY);

	if (!localMoveSize->isMoveSizeStart)
	{
		androidContext* afc = (androidContext*)rail->custom;
		rdpContext* context = (rdpContext*)afc;
		freerdp_callback("OnRailWindowMove", "(JJIIII)V", (jlong)context->instance,
		                 (jlong)localMoveSize->windowId, (jint)localMoveSize->posX,
		                 (jint)localMoveSize->posY, (jint)-1, (jint)-1);
	}
	return CHANNEL_RC_OK;
}

static UINT android_rail_server_min_max_info(RailClientContext* rail,
                                             const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	if (!rail || !minMaxInfo)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

static UINT android_rail_server_z_order_sync(RailClientContext* rail,
                                             const RAIL_ZORDER_SYNC* zOrderSync)
{
	if (!rail || !zOrderSync)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

static UINT android_rail_server_cloak(RailClientContext* rail, const RAIL_CLOAK* cloak)
{
	if (!rail || !cloak)
		return ERROR_INVALID_PARAMETER;

	WLog_DBG(TAG, "RAIL window 0x%08X %s", cloak->windowId, cloak->cloak ? "cloaked" : "uncloaked");
	return CHANNEL_RC_OK;
}

static UINT android_rail_server_language_bar_info(RailClientContext* rail,
                                                  const RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	if (!rail || !langBarInfo)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

static UINT android_rail_server_get_appid_response(RailClientContext* rail,
                                                   const RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	if (!rail || !getAppIdResp)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

static BOOL android_rail_non_monitored_desktop(rdpContext* context,
                                               const WINDOW_ORDER_INFO* orderInfo)
{
	if (!context)
		return FALSE;

	freerdp_callback("OnRailSessionEnd", "(J)V", (jlong)context->instance);
	return TRUE;
}

static BOOL android_rail_window_state(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                      const WINDOW_STATE_ORDER* windowState)
{
	if (!context || !orderInfo || !windowState)
		return FALSE;

	const UINT32 flags = orderInfo->fieldFlags;
	const BOOL isNew = (flags & WINDOW_ORDER_STATE_NEW) != 0;
	const BOOL isHidden =
	    (flags & WINDOW_ORDER_FIELD_SHOW) && windowState->showState == WINDOW_HIDE;

	if (isHidden)
	{
		freerdp_callback("OnRailWindowHide", "(JJ)V", (jlong)context->instance,
		                 (jlong)orderInfo->windowId);
		return TRUE;
	}

	const BOOL isShownAgain = !isNew && (flags & WINDOW_ORDER_FIELD_SHOW) && !isHidden &&
	                          !(flags & WINDOW_ORDER_FIELD_WND_OFFSET);
	if (isShownAgain)
	{
		freerdp_callback("OnRailWindowMove", "(JJIIII)V", (jlong)context->instance,
		                 (jlong)orderInfo->windowId, (jint)-1, (jint)-1, (jint)-1, (jint)-1);
		return TRUE;
	}

	if (isNew)
	{
		const BOOL hasOffset = (flags & WINDOW_ORDER_FIELD_WND_OFFSET) != 0;
		const BOOL hasOffsetAndSize = hasOffset && (flags & WINDOW_ORDER_FIELD_WND_SIZE) != 0;
		const INT32 x = hasOffset ? windowState->windowOffsetX : 0;
		const INT32 y = hasOffset ? windowState->windowOffsetY : 0;
		const INT32 w = hasOffsetAndSize ? (INT32)windowState->windowWidth : -1;
		const INT32 h = hasOffsetAndSize ? (INT32)windowState->windowHeight : -1;
		freerdp_callback("OnRailWindowMove", "(JJIIII)V", (jlong)context->instance,
		                 (jlong)orderInfo->windowId, (jint)x, (jint)y, (jint)w, (jint)h);
	}
	else if (flags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		const BOOL hasSize = (flags & WINDOW_ORDER_FIELD_WND_SIZE) != 0;
		freerdp_callback("OnRailWindowMove", "(JJIIII)V", (jlong)context->instance,
		                 (jlong)orderInfo->windowId, (jint)windowState->windowOffsetX,
		                 (jint)windowState->windowOffsetY,
		                 hasSize ? (jint)windowState->windowWidth : (jint)-1,
		                 hasSize ? (jint)windowState->windowHeight : (jint)-1);
	}
	return TRUE;
}

static BOOL android_rail_window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	if (!context || !orderInfo)
		return FALSE;

	freerdp_callback("OnRailWindowDestroy", "(JJ)V", (jlong)context->instance,
	                 (jlong)orderInfo->windowId);
	return TRUE;
}

BOOL android_rail_init(androidContext* afc, RailClientContext* rail)
{
	if (!afc || !rail)
		return FALSE;

	afc->rail = rail;
	afc->railExecSent = FALSE;
	rail->custom = (void*)afc;
	rail->ServerSystemParam = android_rail_server_system_param;
	rail->ServerExecuteResult = android_rail_server_execute_result;
	rail->ServerLocalMoveSize = android_rail_server_local_move_size;
	rail->ServerMinMaxInfo = android_rail_server_min_max_info;
	rail->ServerZOrderSync = android_rail_server_z_order_sync;
	rail->ServerCloak = android_rail_server_cloak;
	rail->ServerLanguageBarInfo = android_rail_server_language_bar_info;
	rail->ServerGetAppIdResponse = android_rail_server_get_appid_response;

	rdpContext* context = (rdpContext*)afc;
	context->update->window->MonitoredDesktop = android_rail_monitored_desktop;
	context->update->window->NonMonitoredDesktop = android_rail_non_monitored_desktop;
	context->update->window->WindowCreate = android_rail_window_state;
	context->update->window->WindowUpdate = android_rail_window_state;
	context->update->window->WindowDelete = android_rail_window_delete;
	return TRUE;
}

BOOL android_rail_uninit(androidContext* afc, RailClientContext* rail)
{
	if (!afc || !rail)
		return FALSE;

	rail->custom = NULL;
	afc->rail = NULL;
	afc->railExecSent = FALSE;
	return TRUE;
}
