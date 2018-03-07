/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
 * Copyright 2011 Anthony Tong <atong@trustedcs.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/smartcard.h>
#include <winpr/environment.h>

#include <freerdp/channels/rdpdr.h>

#include "smartcard_main.h"

static DWORD WINAPI smartcard_context_thread(LPVOID arg)
{
	SMARTCARD_CONTEXT* pContext = (SMARTCARD_CONTEXT*)arg;
	DWORD nCount;
	LONG status = 0;
	DWORD waitStatus;
	HANDLE hEvents[2];
	wMessage message;
	SMARTCARD_DEVICE* smartcard;
	SMARTCARD_OPERATION* operation;
	UINT error = CHANNEL_RC_OK;
	smartcard = pContext->smartcard;
	nCount = 0;
	hEvents[nCount++] = MessageQueue_Event(pContext->IrpQueue);

	while (1)
	{
		waitStatus = WaitForMultipleObjects(nCount, hEvents, FALSE, INFINITE);

		if (waitStatus == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", error);
			break;
		}

		waitStatus = WaitForSingleObject(MessageQueue_Event(pContext->IrpQueue), 0);

		if (waitStatus == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			break;
		}

		if (waitStatus == WAIT_OBJECT_0)
		{
			if (!MessageQueue_Peek(pContext->IrpQueue, &message, TRUE))
			{
				WLog_ERR(TAG, "MessageQueue_Peek failed!");
				status = ERROR_INTERNAL_ERROR;
				break;
			}

			if (message.id == WMQ_QUIT)
				break;

			operation = (SMARTCARD_OPERATION*) message.wParam;

			if (operation)
			{
				if ((status = smartcard_irp_device_control_call(smartcard, operation)))
				{
					WLog_ERR(TAG, "smartcard_irp_device_control_call failed with error %"PRIu32"",
					         status);
					break;
				}

				if (!Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) operation->irp))
				{
					WLog_ERR(TAG, "Queue_Enqueue failed!");
					status = ERROR_INTERNAL_ERROR;
					break;
				}

				free(operation);
			}
		}
	}

	if (status && smartcard->rdpcontext)
		setChannelError(smartcard->rdpcontext, error,
		                "smartcard_context_thread reported an error");

	ExitThread(status);
	return error;
}

SMARTCARD_CONTEXT* smartcard_context_new(SMARTCARD_DEVICE* smartcard,
        SCARDCONTEXT hContext)
{
	SMARTCARD_CONTEXT* pContext;
	pContext = (SMARTCARD_CONTEXT*) calloc(1, sizeof(SMARTCARD_CONTEXT));

	if (!pContext)
	{
		WLog_ERR(TAG, "calloc failed!");
		return pContext;
	}

	pContext->smartcard = smartcard;
	pContext->hContext = hContext;
	pContext->IrpQueue = MessageQueue_New(NULL);

	if (!pContext->IrpQueue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		goto error_irpqueue;
	}

	pContext->thread = CreateThread(NULL, 0,
									smartcard_context_thread,
	                                pContext, 0, NULL);

	if (!pContext->thread)
	{
		WLog_ERR(TAG, "CreateThread failed!");
		goto error_thread;
	}

	return pContext;
error_thread:
	MessageQueue_Free(pContext->IrpQueue);
error_irpqueue:
	free(pContext);
	return NULL;
}

void smartcard_context_free(SMARTCARD_CONTEXT* pContext)
{
	if (!pContext)
		return;

	/* cancel blocking calls like SCardGetStatusChange */
	SCardCancel(pContext->hContext);

	if (MessageQueue_PostQuit(pContext->IrpQueue, 0)
	    && (WaitForSingleObject(pContext->thread, INFINITE) == WAIT_FAILED))
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", GetLastError());

	CloseHandle(pContext->thread);
	MessageQueue_Free(pContext->IrpQueue);
	free(pContext);
}


static void smartcard_release_all_contexts(SMARTCARD_DEVICE* smartcard)
{
	int index;
	int keyCount;
	ULONG_PTR* pKeys;
	SCARDCONTEXT hContext;
	SMARTCARD_CONTEXT* pContext;

	/**
	 * On protocol termination, the following actions are performed:
	 * For each context in rgSCardContextList, SCardCancel is called causing all SCardGetStatusChange calls to be processed.
	 * After that, SCardReleaseContext is called on each context and the context MUST be removed from rgSCardContextList.
	 */

	/**
	 * Call SCardCancel on existing contexts, unblocking all outstanding SCardGetStatusChange calls.
	 */

	if (ListDictionary_Count(smartcard->rgSCardContextList) > 0)
	{
		pKeys = NULL;
		keyCount = ListDictionary_GetKeys(smartcard->rgSCardContextList, &pKeys);

		for (index = 0; index < keyCount; index++)
		{
			pContext = (SMARTCARD_CONTEXT*) ListDictionary_GetItemValue(
			               smartcard->rgSCardContextList, (void*) pKeys[index]);

			if (!pContext)
				continue;

			hContext = pContext->hContext;

			if (SCardIsValidContext(hContext) == SCARD_S_SUCCESS)
			{
				SCardCancel(hContext);
			}
		}

		free(pKeys);
	}

	/**
	 * Call SCardReleaseContext on remaining contexts and remove them from rgSCardContextList.
	 */

	if (ListDictionary_Count(smartcard->rgSCardContextList) > 0)
	{
		pKeys = NULL;
		keyCount = ListDictionary_GetKeys(smartcard->rgSCardContextList, &pKeys);

		for (index = 0; index < keyCount; index++)
		{
			pContext = (SMARTCARD_CONTEXT*) ListDictionary_Remove(
			               smartcard->rgSCardContextList, (void*) pKeys[index]);

			if (!pContext)
				continue;

			hContext = pContext->hContext;

			if (SCardIsValidContext(hContext) == SCARD_S_SUCCESS)
			{
				SCardReleaseContext(hContext);

				if (MessageQueue_PostQuit(pContext->IrpQueue, 0)
				    && (WaitForSingleObject(pContext->thread, INFINITE) == WAIT_FAILED))
					WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", GetLastError());

				CloseHandle(pContext->thread);
				MessageQueue_Free(pContext->IrpQueue);
				free(pContext);
			}
		}

		free(pKeys);
	}
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT smartcard_free(DEVICE* device)
{
	UINT error;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;
	/**
	 * Calling smartcard_release_all_contexts to unblock all operations waiting for transactions
	 * to unlock.
	 */
	smartcard_release_all_contexts(smartcard);

	/* Stopping all threads and cancelling all IRPs */

	if (smartcard->IrpQueue)
	{
		if (MessageQueue_PostQuit(smartcard->IrpQueue, 0)
		    && (WaitForSingleObject(smartcard->thread, INFINITE) == WAIT_FAILED))
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			return error;
		}

		MessageQueue_Free(smartcard->IrpQueue);
		smartcard->IrpQueue = NULL;
		CloseHandle(smartcard->thread);
		smartcard->thread = NULL;
	}

	if (smartcard->device.data)
	{
		Stream_Free(smartcard->device.data, TRUE);
		smartcard->device.data = NULL;
	}

	ListDictionary_Free(smartcard->rgSCardContextList);
	ListDictionary_Free(smartcard->rgOutstandingMessages);
	Queue_Free(smartcard->CompletedIrpQueue);

	if (smartcard->StartedEvent)
	{
		SCardReleaseStartedEvent();
		smartcard->StartedEvent = NULL;
	}

	free(device);
	return CHANNEL_RC_OK;
}

/**
 * Initialization occurs when the protocol server sends a device announce message.
 * At that time, we need to cancel all outstanding IRPs.
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT smartcard_init(DEVICE* device)
{
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;
	smartcard_release_all_contexts(smartcard);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT smartcard_complete_irp(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	void* key;
	key = (void*)(size_t) irp->CompletionId;
	ListDictionary_Remove(smartcard->rgOutstandingMessages, key);
	return irp->Complete(irp);
}

/**
 * Multiple threads and SCardGetStatusChange:
 * http://musclecard.996296.n3.nabble.com/Multiple-threads-and-SCardGetStatusChange-td4430.html
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT smartcard_process_irp(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	void* key;
	LONG status;
	BOOL asyncIrp = FALSE;
	SMARTCARD_CONTEXT* pContext = NULL;
	SMARTCARD_OPERATION* operation = NULL;
	key = (void*)(size_t) irp->CompletionId;

	if (!ListDictionary_Add(smartcard->rgOutstandingMessages, key, irp))
	{
		WLog_ERR(TAG, "ListDictionary_Add failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (irp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
	{
		operation = (SMARTCARD_OPERATION*) calloc(1, sizeof(SMARTCARD_OPERATION));

		if (!operation)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		operation->irp = irp;
		status = smartcard_irp_device_control_decode(smartcard, operation);

		if (status != SCARD_S_SUCCESS)
		{
			irp->IoStatus = (UINT32)STATUS_UNSUCCESSFUL;

			if (!Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) irp))
			{
				WLog_ERR(TAG, "Queue_Enqueue failed!");
				return ERROR_INTERNAL_ERROR;
			}

			return CHANNEL_RC_OK;
		}

		asyncIrp = TRUE;

		switch (operation->ioControlCode)
		{
			case SCARD_IOCTL_ESTABLISHCONTEXT:
			case SCARD_IOCTL_RELEASECONTEXT:
			case SCARD_IOCTL_ISVALIDCONTEXT:
			case SCARD_IOCTL_CANCEL:
			case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			case SCARD_IOCTL_RELEASESTARTEDEVENT:
				asyncIrp = FALSE;
				break;

			case SCARD_IOCTL_LISTREADERGROUPSA:
			case SCARD_IOCTL_LISTREADERGROUPSW:
			case SCARD_IOCTL_LISTREADERSA:
			case SCARD_IOCTL_LISTREADERSW:
			case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			case SCARD_IOCTL_FORGETREADERGROUPA:
			case SCARD_IOCTL_FORGETREADERGROUPW:
			case SCARD_IOCTL_INTRODUCEREADERA:
			case SCARD_IOCTL_INTRODUCEREADERW:
			case SCARD_IOCTL_FORGETREADERA:
			case SCARD_IOCTL_FORGETREADERW:
			case SCARD_IOCTL_ADDREADERTOGROUPA:
			case SCARD_IOCTL_ADDREADERTOGROUPW:
			case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			case SCARD_IOCTL_LOCATECARDSA:
			case SCARD_IOCTL_LOCATECARDSW:
			case SCARD_IOCTL_LOCATECARDSBYATRA:
			case SCARD_IOCTL_LOCATECARDSBYATRW:
			case SCARD_IOCTL_READCACHEA:
			case SCARD_IOCTL_READCACHEW:
			case SCARD_IOCTL_WRITECACHEA:
			case SCARD_IOCTL_WRITECACHEW:
			case SCARD_IOCTL_GETREADERICON:
			case SCARD_IOCTL_GETDEVICETYPEID:
			case SCARD_IOCTL_GETSTATUSCHANGEA:
			case SCARD_IOCTL_GETSTATUSCHANGEW:
			case SCARD_IOCTL_CONNECTA:
			case SCARD_IOCTL_CONNECTW:
			case SCARD_IOCTL_RECONNECT:
			case SCARD_IOCTL_DISCONNECT:
			case SCARD_IOCTL_BEGINTRANSACTION:
			case SCARD_IOCTL_ENDTRANSACTION:
			case SCARD_IOCTL_STATE:
			case SCARD_IOCTL_STATUSA:
			case SCARD_IOCTL_STATUSW:
			case SCARD_IOCTL_TRANSMIT:
			case SCARD_IOCTL_CONTROL:
			case SCARD_IOCTL_GETATTRIB:
			case SCARD_IOCTL_SETATTRIB:
			case SCARD_IOCTL_GETTRANSMITCOUNT:
				asyncIrp = TRUE;
				break;
		}

		pContext = ListDictionary_GetItemValue(smartcard->rgSCardContextList,
		                                       (void*) operation->hContext);

		if (!pContext)
			asyncIrp = FALSE;

		if (!asyncIrp)
		{
			if ((status = smartcard_irp_device_control_call(smartcard, operation)))
			{
				WLog_ERR(TAG, "smartcard_irp_device_control_call failed with error %"PRId32"!",
				         status);
				return (UINT32)status;
			}

			if (!Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) irp))
			{
				WLog_ERR(TAG, "Queue_Enqueue failed!");
				return ERROR_INTERNAL_ERROR;
			}

			free(operation);
		}
		else
		{
			if (pContext)
			{
				if (!MessageQueue_Post(pContext->IrpQueue, NULL, 0, (void*) operation, NULL))
				{
					WLog_ERR(TAG, "MessageQueue_Post failed!");
					return ERROR_INTERNAL_ERROR;
				}
			}
		}
	}
	else
	{
		WLog_ERR(TAG,
		         "Unexpected SmartCard IRP: MajorFunction 0x%08"PRIX32" MinorFunction: 0x%08"PRIX32"",
		         irp->MajorFunction, irp->MinorFunction);
		irp->IoStatus = (UINT32)STATUS_NOT_SUPPORTED;

		if (!Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) irp))
		{
			WLog_ERR(TAG, "Queue_Enqueue failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static DWORD WINAPI smartcard_thread_func(LPVOID arg)
{
	IRP* irp;
	DWORD nCount;
	DWORD status;
	HANDLE hEvents[2];
	wMessage message;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) arg;
	UINT error = CHANNEL_RC_OK;
	nCount = 0;
	hEvents[nCount++] = MessageQueue_Event(smartcard->IrpQueue);
	hEvents[nCount++] = Queue_Event(smartcard->CompletedIrpQueue);

	while (1)
	{
		status = WaitForMultipleObjects(nCount, hEvents, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", error);
			break;
		}

		status = WaitForSingleObject(MessageQueue_Event(smartcard->IrpQueue), 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			if (!MessageQueue_Peek(smartcard->IrpQueue, &message, TRUE))
			{
				WLog_ERR(TAG, "MessageQueue_Peek failed!");
				error = ERROR_INTERNAL_ERROR;
				break;
			}

			if (message.id == WMQ_QUIT)
			{
				while (1)
				{
					status = WaitForSingleObject(Queue_Event(smartcard->CompletedIrpQueue), 0);

					if (status == WAIT_FAILED)
					{
						error = GetLastError();
						WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
						goto out;
					}

					if (status == WAIT_TIMEOUT)
						break;

					irp = (IRP*) Queue_Dequeue(smartcard->CompletedIrpQueue);

					if (irp)
					{
						if (irp->thread)
						{
							status = WaitForSingleObject(irp->thread, INFINITE);

							if (status == WAIT_FAILED)
							{
								error = GetLastError();
								WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
								goto out;
							}

							CloseHandle(irp->thread);
							irp->thread = NULL;
						}

						if ((error = smartcard_complete_irp(smartcard, irp)))
						{
							WLog_ERR(TAG, "smartcard_complete_irp failed with error %"PRIu32"!", error);
							goto out;
						}
					}
				}

				break;
			}

			irp = (IRP*) message.wParam;

			if (irp)
			{
				if ((error = smartcard_process_irp(smartcard, irp)))
				{
					WLog_ERR(TAG, "smartcard_process_irp failed with error %"PRIu32"!", error);
					goto out;
				}
			}
		}

		status = WaitForSingleObject(Queue_Event(smartcard->CompletedIrpQueue), 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			irp = (IRP*) Queue_Dequeue(smartcard->CompletedIrpQueue);

			if (irp)
			{
				if (irp->thread)
				{
					status = WaitForSingleObject(irp->thread, INFINITE);

					if (status == WAIT_FAILED)
					{
						error = GetLastError();
						WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
						break;
					}

					CloseHandle(irp->thread);
					irp->thread = NULL;
				}

				if ((error = smartcard_complete_irp(smartcard, irp)))
				{
					if (error == CHANNEL_RC_NOT_CONNECTED)
					{
						error = CHANNEL_RC_OK;
						goto out;
					}

					WLog_ERR(TAG, "smartcard_complete_irp failed with error %"PRIu32"!", error);
					goto out;
				}
			}
		}
	}

out:

	if (error && smartcard->rdpcontext)
		setChannelError(smartcard->rdpcontext, error,
		                "smartcard_thread_func reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT smartcard_irp_request(DEVICE* device, IRP* irp)
{
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;

	if (!MessageQueue_Post(smartcard->IrpQueue, NULL, 0, (void*) irp, NULL))
	{
		WLog_ERR(TAG, "MessageQueue_Post failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/* smartcard is always built-in */
#define DeviceServiceEntry	smartcard_DeviceServiceEntry

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	size_t length;
	SMARTCARD_DEVICE* smartcard;
	UINT error = CHANNEL_RC_NO_MEMORY;
	smartcard = (SMARTCARD_DEVICE*) calloc(1, sizeof(SMARTCARD_DEVICE));

	if (!smartcard)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	smartcard->device.type = RDPDR_DTYP_SMARTCARD;
	smartcard->device.name = "SCARD";
	smartcard->device.IRPRequest = smartcard_irp_request;
	smartcard->device.Init = smartcard_init;
	smartcard->device.Free = smartcard_free;
	smartcard->rdpcontext = pEntryPoints->rdpcontext;
	length = strlen(smartcard->device.name);
	smartcard->device.data = Stream_New(NULL, length + 1);

	if (!smartcard->device.data)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto error_device_data;
	}

	Stream_Write(smartcard->device.data, "SCARD", 6);
	smartcard->IrpQueue = MessageQueue_New(NULL);

	if (!smartcard->IrpQueue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		goto error_irp_queue;
	}

	smartcard->CompletedIrpQueue = Queue_New(TRUE, -1, -1);

	if (!smartcard->CompletedIrpQueue)
	{
		WLog_ERR(TAG, "Queue_New failed!");
		goto error_completed_irp_queue;
	}

	smartcard->rgSCardContextList = ListDictionary_New(TRUE);

	if (!smartcard->rgSCardContextList)
	{
		WLog_ERR(TAG, "ListDictionary_New failed!");
		goto error_context_list;
	}

	ListDictionary_ValueObject(smartcard->rgSCardContextList)->fnObjectFree =
	    (OBJECT_FREE_FN) smartcard_context_free;
	smartcard->rgOutstandingMessages = ListDictionary_New(TRUE);

	if (!smartcard->rgOutstandingMessages)
	{
		WLog_ERR(TAG, "ListDictionary_New failed!");
		goto error_outstanding_messages;
	}

	if ((error = pEntryPoints->RegisterDevice(pEntryPoints->devman,
	             (DEVICE*) smartcard)))
	{
		WLog_ERR(TAG, "RegisterDevice failed!");
		goto error_outstanding_messages;
	}

	smartcard->thread = CreateThread(NULL, 0,
									 smartcard_thread_func,
	                                 smartcard, CREATE_SUSPENDED, NULL);

	if (!smartcard->thread)
	{
		WLog_ERR(TAG, "ListDictionary_New failed!");
		error = ERROR_INTERNAL_ERROR;
		goto error_thread;
	}

	ResumeThread(smartcard->thread);
	return CHANNEL_RC_OK;
error_thread:
	ListDictionary_Free(smartcard->rgOutstandingMessages);
error_outstanding_messages:
	ListDictionary_Free(smartcard->rgSCardContextList);
error_context_list:
	Queue_Free(smartcard->CompletedIrpQueue);
error_completed_irp_queue:
	MessageQueue_Free(smartcard->IrpQueue);
error_irp_queue:
	Stream_Free(smartcard->device.data, TRUE);
error_device_data:
	free(smartcard);
	return error;
}

