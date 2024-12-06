/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Channel Addins
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/path.h>
#include <winpr/string.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/collections.h>

#include <freerdp/freerdp.h>
#include <freerdp/addin.h>
#include <freerdp/build-config.h>
#include <freerdp/client/channels.h>

#include "tables.h"

#include "addin.h"

#include <freerdp/channels/log.h>
#define TAG CHANNELS_TAG("addin")

extern const STATIC_ENTRY_TABLE CLIENT_STATIC_ENTRY_TABLES[];

static void* freerdp_channels_find_static_entry_in_table(const STATIC_ENTRY_TABLE* table,
                                                         const char* identifier)
{
	size_t index = 0;
	const STATIC_ENTRY* pEntry = &table->table.cse[index++];

	while (pEntry->entry != NULL)
	{
		static_entry_fn_t fkt = pEntry->entry;
		if (strcmp(pEntry->name, identifier) == 0)
			return WINPR_FUNC_PTR_CAST(fkt, void*);

		pEntry = &table->table.cse[index++];
	}

	return NULL;
}

void* freerdp_channels_client_find_static_entry(const char* name, const char* identifier)
{
	size_t index = 0;
	const STATIC_ENTRY_TABLE* pEntry = &CLIENT_STATIC_ENTRY_TABLES[index++];

	while (pEntry->table.cse != NULL)
	{
		if (strcmp(pEntry->name, name) == 0)
		{
			return freerdp_channels_find_static_entry_in_table(pEntry, identifier);
		}

		pEntry = &CLIENT_STATIC_ENTRY_TABLES[index++];
	}

	return NULL;
}

extern const STATIC_ADDIN_TABLE CLIENT_STATIC_ADDIN_TABLE[];

static FREERDP_ADDIN** freerdp_channels_list_client_static_addins(LPCSTR pszName,
                                                                  LPCSTR pszSubsystem,
                                                                  LPCSTR pszType, DWORD dwFlags)
{
	DWORD nAddins = 0;
	FREERDP_ADDIN** ppAddins = NULL;
	const STATIC_SUBSYSTEM_ENTRY* subsystems = NULL;
	nAddins = 0;
	ppAddins = (FREERDP_ADDIN**)calloc(128, sizeof(FREERDP_ADDIN*));

	if (!ppAddins)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	ppAddins[nAddins] = NULL;

	for (size_t i = 0; CLIENT_STATIC_ADDIN_TABLE[i].name != NULL; i++)
	{
		FREERDP_ADDIN* pAddin = (FREERDP_ADDIN*)calloc(1, sizeof(FREERDP_ADDIN));
		const STATIC_ADDIN_TABLE* table = &CLIENT_STATIC_ADDIN_TABLE[i];
		if (!pAddin)
		{
			WLog_ERR(TAG, "calloc failed!");
			goto error_out;
		}

		(void)sprintf_s(pAddin->cName, ARRAYSIZE(pAddin->cName), "%s", table->name);
		pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
		pAddin->dwFlags |= FREERDP_ADDIN_STATIC;
		pAddin->dwFlags |= FREERDP_ADDIN_NAME;
		ppAddins[nAddins++] = pAddin;
		subsystems = table->table;

		for (size_t j = 0; subsystems[j].name != NULL; j++)
		{
			pAddin = (FREERDP_ADDIN*)calloc(1, sizeof(FREERDP_ADDIN));

			if (!pAddin)
			{
				WLog_ERR(TAG, "calloc failed!");
				goto error_out;
			}

			(void)sprintf_s(pAddin->cName, ARRAYSIZE(pAddin->cName), "%s", table->name);
			(void)sprintf_s(pAddin->cSubsystem, ARRAYSIZE(pAddin->cSubsystem), "%s",
			                subsystems[j].name);
			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_STATIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;
			ppAddins[nAddins++] = pAddin;
		}
	}

	return ppAddins;
error_out:
	freerdp_channels_addin_list_free(ppAddins);
	return NULL;
}

static HANDLE FindFirstFileUTF8(LPCSTR pszSearchPath, WIN32_FIND_DATAW* FindData)
{
	HANDLE hdl = INVALID_HANDLE_VALUE;
	if (!pszSearchPath)
		return hdl;
	WCHAR* wpath = ConvertUtf8ToWCharAlloc(pszSearchPath, NULL);
	if (!wpath)
		return hdl;

	hdl = FindFirstFileW(wpath, FindData);
	free(wpath);

	return hdl;
}

static FREERDP_ADDIN** freerdp_channels_list_dynamic_addins(LPCSTR pszName, LPCSTR pszSubsystem,
                                                            LPCSTR pszType, DWORD dwFlags)
{
	int nDashes = 0;
	HANDLE hFind = NULL;
	DWORD nAddins = 0;
	LPSTR pszPattern = NULL;
	size_t cchPattern = 0;
	LPCSTR pszAddinPath = FREERDP_ADDIN_PATH;
	LPCSTR pszInstallPrefix = FREERDP_INSTALL_PREFIX;
	LPCSTR pszExtension = NULL;
	LPSTR pszSearchPath = NULL;
	size_t cchSearchPath = 0;
	size_t cchAddinPath = 0;
	size_t cchInstallPrefix = 0;
	FREERDP_ADDIN** ppAddins = NULL;
	WIN32_FIND_DATAW FindData = { 0 };
	cchAddinPath = strnlen(pszAddinPath, sizeof(FREERDP_ADDIN_PATH));
	cchInstallPrefix = strnlen(pszInstallPrefix, sizeof(FREERDP_INSTALL_PREFIX));
	pszExtension = PathGetSharedLibraryExtensionA(0);
	cchPattern = 128 + strnlen(pszExtension, MAX_PATH) + 2;
	pszPattern = (LPSTR)malloc(cchPattern + 1);

	if (!pszPattern)
	{
		WLog_ERR(TAG, "malloc failed!");
		return NULL;
	}

	if (pszName && pszSubsystem && pszType)
	{
		(void)sprintf_s(pszPattern, cchPattern, FREERDP_SHARED_LIBRARY_PREFIX "%s-client-%s-%s.%s",
		                pszName, pszSubsystem, pszType, pszExtension);
	}
	else if (pszName && pszType)
	{
		(void)sprintf_s(pszPattern, cchPattern, FREERDP_SHARED_LIBRARY_PREFIX "%s-client-?-%s.%s",
		                pszName, pszType, pszExtension);
	}
	else if (pszName)
	{
		(void)sprintf_s(pszPattern, cchPattern, FREERDP_SHARED_LIBRARY_PREFIX "%s-client*.%s",
		                pszName, pszExtension);
	}
	else
	{
		(void)sprintf_s(pszPattern, cchPattern, FREERDP_SHARED_LIBRARY_PREFIX "?-client*.%s",
		                pszExtension);
	}

	cchPattern = strnlen(pszPattern, cchPattern);
	cchSearchPath = cchInstallPrefix + cchAddinPath + cchPattern + 3;
	pszSearchPath = (LPSTR)calloc(cchSearchPath + 1, sizeof(char));

	if (!pszSearchPath)
	{
		WLog_ERR(TAG, "malloc failed!");
		free(pszPattern);
		return NULL;
	}

	CopyMemory(pszSearchPath, pszInstallPrefix, cchInstallPrefix);
	pszSearchPath[cchInstallPrefix] = '\0';
	const HRESULT hr1 = NativePathCchAppendA(pszSearchPath, cchSearchPath + 1, pszAddinPath);
	const HRESULT hr2 = NativePathCchAppendA(pszSearchPath, cchSearchPath + 1, pszPattern);
	free(pszPattern);

	if (FAILED(hr1) || FAILED(hr2))
	{
		free(pszSearchPath);
		return NULL;
	}

	hFind = FindFirstFileUTF8(pszSearchPath, &FindData);

	free(pszSearchPath);
	nAddins = 0;
	ppAddins = (FREERDP_ADDIN**)calloc(128, sizeof(FREERDP_ADDIN*));

	if (!ppAddins)
	{
		FindClose(hFind);
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	if (hFind == INVALID_HANDLE_VALUE)
		return ppAddins;

	do
	{
		char* cFileName = NULL;
		BOOL used = FALSE;
		FREERDP_ADDIN* pAddin = (FREERDP_ADDIN*)calloc(1, sizeof(FREERDP_ADDIN));

		if (!pAddin)
		{
			WLog_ERR(TAG, "calloc failed!");
			goto error_out;
		}

		cFileName =
		    ConvertWCharNToUtf8Alloc(FindData.cFileName, ARRAYSIZE(FindData.cFileName), NULL);
		if (!cFileName)
			goto skip;

		nDashes = 0;
		for (size_t index = 0; cFileName[index]; index++)
			nDashes += (cFileName[index] == '-') ? 1 : 0;

		if (nDashes == 1)
		{
			size_t len = 0;
			char* p[2] = { 0 };
			/* <name>-client.<extension> */
			p[0] = cFileName;
			p[1] = strchr(p[0], '-');
			if (!p[1])
				goto skip;
			p[1] += 1;

			len = (size_t)(p[1] - p[0]);
			if (len < 1)
			{
				WLog_WARN(TAG, "Skipping file '%s', invalid format", cFileName);
				goto skip;
			}
			strncpy(pAddin->cName, p[0], MIN(ARRAYSIZE(pAddin->cName), len - 1));

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			ppAddins[nAddins++] = pAddin;

			used = TRUE;
		}
		else if (nDashes == 2)
		{
			size_t len = 0;
			char* p[4] = { 0 };
			/* <name>-client-<subsystem>.<extension> */
			p[0] = cFileName;
			p[1] = strchr(p[0], '-');
			if (!p[1])
				goto skip;
			p[1] += 1;
			p[2] = strchr(p[1], '-');
			if (!p[2])
				goto skip;
			p[2] += 1;
			p[3] = strchr(p[2], '.');
			if (!p[3])
				goto skip;
			p[3] += 1;

			len = (size_t)(p[1] - p[0]);
			if (len < 1)
			{
				WLog_WARN(TAG, "Skipping file '%s', invalid format", cFileName);
				goto skip;
			}
			strncpy(pAddin->cName, p[0], MIN(ARRAYSIZE(pAddin->cName), len - 1));

			len = (size_t)(p[3] - p[2]);
			if (len < 1)
			{
				WLog_WARN(TAG, "Skipping file '%s', invalid format", cFileName);
				goto skip;
			}
			strncpy(pAddin->cSubsystem, p[2], MIN(ARRAYSIZE(pAddin->cSubsystem), len - 1));

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;
			ppAddins[nAddins++] = pAddin;

			used = TRUE;
		}
		else if (nDashes == 3)
		{
			size_t len = 0;
			char* p[5] = { 0 };
			/* <name>-client-<subsystem>-<type>.<extension> */
			p[0] = cFileName;
			p[1] = strchr(p[0], '-');
			if (!p[1])
				goto skip;
			p[1] += 1;
			p[2] = strchr(p[1], '-');
			if (!p[2])
				goto skip;
			p[2] += 1;
			p[3] = strchr(p[2], '-');
			if (!p[3])
				goto skip;
			p[3] += 1;
			p[4] = strchr(p[3], '.');
			if (!p[4])
				goto skip;
			p[4] += 1;

			len = (size_t)(p[1] - p[0]);
			if (len < 1)
			{
				WLog_WARN(TAG, "Skipping file '%s', invalid format", cFileName);
				goto skip;
			}
			strncpy(pAddin->cName, p[0], MIN(ARRAYSIZE(pAddin->cName), len - 1));

			len = (size_t)(p[3] - p[2]);
			if (len < 1)
			{
				WLog_WARN(TAG, "Skipping file '%s', invalid format", cFileName);
				goto skip;
			}
			strncpy(pAddin->cSubsystem, p[2], MIN(ARRAYSIZE(pAddin->cSubsystem), len - 1));

			len = (size_t)(p[4] - p[3]);
			if (len < 1)
			{
				WLog_WARN(TAG, "Skipping file '%s', invalid format", cFileName);
				goto skip;
			}
			strncpy(pAddin->cType, p[3], MIN(ARRAYSIZE(pAddin->cType), len - 1));

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;
			pAddin->dwFlags |= FREERDP_ADDIN_TYPE;
			ppAddins[nAddins++] = pAddin;

			used = TRUE;
		}

	skip:
		free(cFileName);
		if (!used)
			free(pAddin);

	} while (FindNextFileW(hFind, &FindData));

	FindClose(hFind);
	ppAddins[nAddins] = NULL;
	return ppAddins;
error_out:
	FindClose(hFind);
	freerdp_channels_addin_list_free(ppAddins);
	return NULL;
}

FREERDP_ADDIN** freerdp_channels_list_addins(LPCSTR pszName, LPCSTR pszSubsystem, LPCSTR pszType,
                                             DWORD dwFlags)
{
	if (dwFlags & FREERDP_ADDIN_STATIC)
		return freerdp_channels_list_client_static_addins(pszName, pszSubsystem, pszType, dwFlags);
	else if (dwFlags & FREERDP_ADDIN_DYNAMIC)
		return freerdp_channels_list_dynamic_addins(pszName, pszSubsystem, pszType, dwFlags);

	return NULL;
}

void freerdp_channels_addin_list_free(FREERDP_ADDIN** ppAddins)
{
	if (!ppAddins)
		return;

	for (size_t index = 0; ppAddins[index] != NULL; index++)
		free(ppAddins[index]);

	free((void*)ppAddins);
}

extern const STATIC_ENTRY CLIENT_VirtualChannelEntryEx_TABLE[];

static BOOL freerdp_channels_is_virtual_channel_entry_ex(LPCSTR pszName)
{
	for (size_t i = 0; CLIENT_VirtualChannelEntryEx_TABLE[i].name != NULL; i++)
	{
		const STATIC_ENTRY* entry = &CLIENT_VirtualChannelEntryEx_TABLE[i];

		if (!strncmp(entry->name, pszName, MAX_PATH))
			return TRUE;
	}

	return FALSE;
}

PVIRTUALCHANNELENTRY freerdp_channels_load_static_addin_entry(LPCSTR pszName, LPCSTR pszSubsystem,
                                                              LPCSTR pszType, DWORD dwFlags)
{
	const STATIC_ADDIN_TABLE* table = CLIENT_STATIC_ADDIN_TABLE;
	const char* type = NULL;

	if (!pszName)
		return NULL;

	if (dwFlags & FREERDP_ADDIN_CHANNEL_DYNAMIC)
		type = "DVCPluginEntry";
	else if (dwFlags & FREERDP_ADDIN_CHANNEL_DEVICE)
		type = "DeviceServiceEntry";
	else if (dwFlags & FREERDP_ADDIN_CHANNEL_STATIC)
	{
		if (dwFlags & FREERDP_ADDIN_CHANNEL_ENTRYEX)
			type = "VirtualChannelEntryEx";
		else
			type = "VirtualChannelEntry";
	}

	for (; table->name != NULL; table++)
	{
		if (strncmp(table->name, pszName, MAX_PATH) == 0)
		{
			if (type && (strncmp(table->type, type, MAX_PATH) != 0))
				continue;

			if (pszSubsystem != NULL)
			{
				const STATIC_SUBSYSTEM_ENTRY* subsystems = table->table;

				for (; subsystems->name != NULL; subsystems++)
				{
					/* If the pszSubsystem is an empty string use the default backend. */
					if ((strnlen(pszSubsystem, 1) ==
					     0) || /* we only want to know if strnlen is > 0 */
					    (strncmp(subsystems->name, pszSubsystem, MAX_PATH) == 0))
					{
						static_subsystem_entry_fn_t fkt = subsystems->entry;

						if (pszType)
						{
							if (strncmp(subsystems->type, pszType, MAX_PATH) == 0)
								return WINPR_FUNC_PTR_CAST(fkt, PVIRTUALCHANNELENTRY);
						}
						else
							return WINPR_FUNC_PTR_CAST(fkt, PVIRTUALCHANNELENTRY);
					}
				}
			}
			else
			{
				if (dwFlags & FREERDP_ADDIN_CHANNEL_ENTRYEX)
				{
					if (!freerdp_channels_is_virtual_channel_entry_ex(pszName))
						return NULL;
				}

				return table->entry.csevc;
			}
		}
	}

	return NULL;
}

typedef struct
{
	wMessageQueue* queue;
	wStream* data_in;
	HANDLE thread;
	char* channel_name;
	rdpContext* ctx;
	LPVOID userdata;
	MsgHandler msg_handler;
} msg_proc_internals;

static DWORD WINAPI channel_client_thread_proc(LPVOID userdata)
{
	UINT error = CHANNEL_RC_OK;
	wStream* data = NULL;
	wMessage message = { 0 };
	msg_proc_internals* internals = userdata;

	WINPR_ASSERT(internals);

	while (1)
	{
		if (!MessageQueue_Wait(internals->queue))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}
		if (!MessageQueue_Peek(internals->queue, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*)message.wParam;

			if ((error = internals->msg_handler(internals->userdata, data)))
			{
				WLog_ERR(TAG, "msg_handler failed with error %" PRIu32 "!", error);
				break;
			}
		}
	}
	if (error && internals->ctx)
	{
		char msg[128];
		(void)_snprintf(msg, 127,
		                "%s_virtual_channel_client_thread reported an"
		                " error",
		                internals->channel_name);
		setChannelError(internals->ctx, error, msg);
	}
	ExitThread(error);
	return error;
}

static void free_msg(void* obj)
{
	wMessage* msg = (wMessage*)obj;

	if (msg && (msg->id == 0))
	{
		wStream* s = (wStream*)msg->wParam;
		Stream_Free(s, TRUE);
	}
}

static void channel_client_handler_free(msg_proc_internals* internals)
{
	if (!internals)
		return;

	if (internals->thread)
		(void)CloseHandle(internals->thread);
	MessageQueue_Free(internals->queue);
	Stream_Free(internals->data_in, TRUE);
	free(internals->channel_name);
	free(internals);
}

/*  Create message queue and thread or not, depending on settings */
void* channel_client_create_handler(rdpContext* ctx, LPVOID userdata, MsgHandler msg_handler,
                                    const char* channel_name)
{
	msg_proc_internals* internals = calloc(1, sizeof(msg_proc_internals));
	if (!internals)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}
	internals->msg_handler = msg_handler;
	internals->userdata = userdata;
	if (channel_name)
	{
		internals->channel_name = _strdup(channel_name);
		if (!internals->channel_name)
			goto fail;
	}
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(ctx->settings);
	internals->ctx = ctx;
	if ((freerdp_settings_get_uint32(ctx->settings, FreeRDP_ThreadingFlags) &
	     THREADING_FLAGS_DISABLE_THREADS) == 0)
	{
		wObject obj = { 0 };
		obj.fnObjectFree = free_msg;
		internals->queue = MessageQueue_New(&obj);
		if (!internals->queue)
		{
			WLog_ERR(TAG, "MessageQueue_New failed!");
			goto fail;
		}

		if (!(internals->thread =
		          CreateThread(NULL, 0, channel_client_thread_proc, (void*)internals, 0, NULL)))
		{
			WLog_ERR(TAG, "CreateThread failed!");
			goto fail;
		}
	}
	return internals;

fail:
	channel_client_handler_free(internals);
	return NULL;
}
/* post a message in the queue or directly call the processing handler */
UINT channel_client_post_message(void* MsgsHandle, LPVOID pData, UINT32 dataLength,
                                 UINT32 totalLength, UINT32 dataFlags)
{
	msg_proc_internals* internals = MsgsHandle;
	wStream* data_in = NULL;

	if (!internals)
	{
		/* TODO: return some error here */
		return CHANNEL_RC_OK;
	}

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (internals->data_in)
		{
			if (!Stream_EnsureCapacity(internals->data_in, totalLength))
				return CHANNEL_RC_NO_MEMORY;
		}
		else
			internals->data_in = Stream_New(NULL, totalLength);
	}

	if (!(data_in = internals->data_in))
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!Stream_EnsureRemainingCapacity(data_in, dataLength))
	{
		Stream_Free(internals->data_in, TRUE);
		internals->data_in = NULL;
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			char msg[128];
			(void)_snprintf(msg, 127, "%s_plugin_process_received: read error",
			                internals->channel_name);
			WLog_ERR(TAG, msg);
			return ERROR_INTERNAL_ERROR;
		}

		internals->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if ((freerdp_settings_get_uint32(internals->ctx->settings, FreeRDP_ThreadingFlags) &
		     THREADING_FLAGS_DISABLE_THREADS) != 0)
		{
			UINT error = CHANNEL_RC_OK;
			if ((error = internals->msg_handler(internals->userdata, data_in)))
			{
				WLog_ERR(TAG,
				         "msg_handler failed with error"
				         " %" PRIu32 "!",
				         error);
				return ERROR_INTERNAL_ERROR;
			}
		}
		else if (!MessageQueue_Post(internals->queue, NULL, 0, (void*)data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}
	return CHANNEL_RC_OK;
}
/* Tear down queue and thread */
UINT channel_client_quit_handler(void* MsgsHandle)
{
	msg_proc_internals* internals = MsgsHandle;
	UINT rc = 0;
	if (!internals)
	{
		/* TODO: return some error here */
		return CHANNEL_RC_OK;
	}

	WINPR_ASSERT(internals->ctx);
	WINPR_ASSERT(internals->ctx->settings);

	if ((freerdp_settings_get_uint32(internals->ctx->settings, FreeRDP_ThreadingFlags) &
	     THREADING_FLAGS_DISABLE_THREADS) == 0)
	{
		if (internals->queue && internals->thread)
		{
			if (MessageQueue_PostQuit(internals->queue, 0) &&
			    (WaitForSingleObject(internals->thread, INFINITE) == WAIT_FAILED))
			{
				rc = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", rc);
				return rc;
			}
		}
	}

	channel_client_handler_free(internals);
	return CHANNEL_RC_OK;
}
