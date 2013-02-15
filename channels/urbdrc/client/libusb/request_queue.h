/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX USB Redirection
 *
 * Copyright 2012 Atrust corp.
 * Copyright 2012 Alfred Liu <alfred.liu@atruscorp.com>
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

#ifndef __REQUEST_QUEUE_H
#define __REQUEST_QUEUE_H

#include "urbdrc_types.h"

typedef struct _TRANSFER_REQUEST TRANSFER_REQUEST;
typedef struct _REQUEST_QUEUE REQUEST_QUEUE;

struct _TRANSFER_REQUEST
{
	void*	request;
	void*	prev;
	void*	next;

	UINT32	RequestId;
	BYTE	endpoint;  
	struct libusb_transfer *transfer;
	int		submit;
};


struct _REQUEST_QUEUE
{
	int request_num;
	TRANSFER_REQUEST* ireq; /* iterator request */
	TRANSFER_REQUEST* head; /* head request in linked queue */
	TRANSFER_REQUEST* tail; /* tail request in linked queue */

	pthread_mutex_t request_loading;

	/* request queue manager service */
	void (*rewind) (REQUEST_QUEUE *queue);
	int (*has_next) (REQUEST_QUEUE* queue);
	int (*unregister_request) (REQUEST_QUEUE *queue, UINT32 RequestId);
	TRANSFER_REQUEST *(*get_next) (REQUEST_QUEUE* queue);
	TRANSFER_REQUEST *(*get_request_by_ep) (REQUEST_QUEUE *queue, BYTE ep);
	TRANSFER_REQUEST *(*register_request) (REQUEST_QUEUE* queue, 
		UINT32 RequestId, struct libusb_transfer * transfer, BYTE endpoint);
};


REQUEST_QUEUE* request_queue_new(void);


#endif /* __REQUEST_QUEUE_H */
