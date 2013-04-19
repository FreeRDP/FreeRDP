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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "request_queue.h"

TRANSFER_REQUEST* request_queue_get_next(REQUEST_QUEUE* queue)
{
	TRANSFER_REQUEST* request;

	request = queue->ireq;
	queue->ireq = (TRANSFER_REQUEST *)queue->ireq->next;

	return request;
}

int request_queue_has_next(REQUEST_QUEUE* queue)
{
	if (queue->ireq == NULL)
		return 0;
	else
		return 1;
}

TRANSFER_REQUEST* request_queue_register_request(REQUEST_QUEUE* queue, UINT32 RequestId,
	struct libusb_transfer* transfer, BYTE endpoint)
{
	TRANSFER_REQUEST* request;

	request = (TRANSFER_REQUEST*) malloc(sizeof(TRANSFER_REQUEST));

	request->prev = NULL;
	request->next = NULL;

	request->RequestId = RequestId;
	request->transfer = transfer;
	request->endpoint = endpoint;
	request->submit = 0;

	pthread_mutex_lock(&queue->request_loading);

	if (queue->head == NULL)
	{
		/* linked queue is empty */
		queue->head = request;
		queue->tail = request;
	}
	else
	{
		/* append data to the end of the linked queue */
		queue->tail->next = (void*) request;
		request->prev = (void*) queue->tail;
		queue->tail = request;
	}

	queue->request_num += 1;
	pthread_mutex_unlock(&queue->request_loading);

	return request;
}

void request_queue_rewind(REQUEST_QUEUE* queue)
{
	queue->ireq = queue->head;
}

/* Get first*/
TRANSFER_REQUEST* request_queue_get_request_by_endpoint(REQUEST_QUEUE* queue, BYTE ep)
{
	TRANSFER_REQUEST * request;
	pthread_mutex_lock(&queue->request_loading);
	queue->rewind (queue);
	while (queue->has_next (queue))
	{
		request = queue->get_next (queue);
		if (request->endpoint == ep)
		{
			pthread_mutex_unlock(&queue->request_loading);
			return request;
		}
	}
	pthread_mutex_unlock(&queue->request_loading);
	fprintf(stderr, "request_queue_get_request_by_id: ERROR!!\n");
	return NULL;
}

int request_queue_unregister_request(REQUEST_QUEUE* queue, UINT32 RequestId)
{
	TRANSFER_REQUEST *request, *request_temp;
	pthread_mutex_lock(&queue->request_loading);
	queue->rewind(queue);

	while (queue->has_next(queue) != 0)
	{
		request = queue->get_next(queue);

		if (request->RequestId == RequestId) 
		{

			if (request->prev != NULL)
			{
				request_temp = (TRANSFER_REQUEST*) request->prev;
				request_temp->next = (TRANSFER_REQUEST*) request->next;
			}
			else
			{
				queue->head = (TRANSFER_REQUEST*) request->next;
			}

			if (request->next != NULL)
			{
				request_temp = (TRANSFER_REQUEST*) request->next;
				request_temp->prev = (TRANSFER_REQUEST*) request->prev;
			}
			else
			{
				queue->tail = (TRANSFER_REQUEST*) request->prev;

			}

			queue->request_num--;
			
			if (request)
			{
				request->transfer = NULL;
				zfree(request); 
			}

			pthread_mutex_unlock(&queue->request_loading);

			return 0; 
		}
	}
	pthread_mutex_unlock(&queue->request_loading);
	/* it wasn't found */
	return 1;
}

REQUEST_QUEUE* request_queue_new()
{
	REQUEST_QUEUE* queue;

	queue = (REQUEST_QUEUE*) malloc(sizeof(REQUEST_QUEUE));
	queue->request_num = 0;
	queue->ireq = NULL;
	queue->head = NULL;
	queue->tail = NULL;   

	pthread_mutex_init(&queue->request_loading, NULL);

	/* load service */
	queue->get_next = request_queue_get_next;
	queue->has_next = request_queue_has_next;
	queue->rewind = request_queue_rewind;
	queue->register_request = request_queue_register_request;
	queue->unregister_request = request_queue_unregister_request;
	queue->get_request_by_ep = request_queue_get_request_by_endpoint;

	return queue;
}
