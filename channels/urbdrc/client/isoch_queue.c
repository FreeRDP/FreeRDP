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
 *	 http://www.apache.org/licenses/LICENSE-2.0
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
#include "isoch_queue.h"

static void isoch_queue_rewind(ISOCH_CALLBACK_QUEUE* queue)
{
	queue->curr = queue->head;
}

static int isoch_queue_has_next(ISOCH_CALLBACK_QUEUE* queue)
{
	if (queue->curr == NULL)
		return 0;
	else
		return 1;
}

static ISOCH_CALLBACK_DATA* isoch_queue_get_next(ISOCH_CALLBACK_QUEUE* queue)
{
	ISOCH_CALLBACK_DATA* isoch;

	isoch = queue->curr;
	queue->curr = (ISOCH_CALLBACK_DATA*)queue->curr->next;

	return isoch;
}

static ISOCH_CALLBACK_DATA* isoch_queue_register_data(ISOCH_CALLBACK_QUEUE* queue, void* callback, void* dev)
{
	ISOCH_CALLBACK_DATA* isoch;
	
	isoch = (ISOCH_CALLBACK_DATA*) malloc(sizeof(ISOCH_CALLBACK_DATA));
	
	isoch->prev = NULL;
	isoch->next = NULL;
	
	isoch->out_data = NULL;
	isoch->out_size = 0;
	isoch->device = dev;
	isoch->callback = callback;
	
	pthread_mutex_lock(&queue->isoch_loading);

	if (queue->head == NULL)
	{
		/* linked queue is empty */
		queue->head = isoch;
		queue->tail = isoch;
	}
	else
	{
		/* append data to the end of the linked queue */
		queue->tail->next = (void*)isoch;
		isoch->prev = (void*)queue->tail;
		queue->tail = isoch;
	}
	queue->isoch_num += 1;

	pthread_mutex_unlock(&queue->isoch_loading);

	return isoch;
}

static int isoch_queue_unregister_data(ISOCH_CALLBACK_QUEUE* queue, ISOCH_CALLBACK_DATA* isoch)
{
	ISOCH_CALLBACK_DATA* p;
		
	queue->rewind(queue);

	while (queue->has_next(queue) != 0)
	{
		p = queue->get_next(queue);

		if (p == isoch) /* data exists */
		{
			/* set previous data to point to next data */

			if (isoch->prev != NULL)
			{
				/* unregistered data is not the head */
				p = (ISOCH_CALLBACK_DATA*)isoch->prev;
				p->next = isoch->next;
			}
			else
			{
				/* unregistered data is the head, update head */
				queue->head = (ISOCH_CALLBACK_DATA*)isoch->next;
			}

			/* set next data to point to previous data */

			if (isoch->next != NULL)
			{
				/* unregistered data is not the tail */
				p = (ISOCH_CALLBACK_DATA*)isoch->next;
				p->prev = isoch->prev;
			}
			else
			{
				/* unregistered data is the tail, update tail */
				queue->tail = (ISOCH_CALLBACK_DATA*)isoch->prev;
			}
			queue->isoch_num--;
			
			/* free data info */
			isoch->out_data = NULL;
			
			if (isoch) zfree(isoch); 
	
			return 1; /* unregistration successful */
		}
	}

	/* if we reach this point, the isoch wasn't found */
	return 0;
}

void isoch_queue_free(ISOCH_CALLBACK_QUEUE* queue)
{
	ISOCH_CALLBACK_DATA* isoch;

	pthread_mutex_lock(&queue->isoch_loading);

	/** unregister all isochronous data*/
	queue->rewind(queue);

	while (queue->has_next(queue))
	{
		isoch = queue->get_next(queue);

		if (isoch != NULL)
			queue->unregister_data(queue, isoch);
	}

	pthread_mutex_unlock(&queue->isoch_loading);

	pthread_mutex_destroy(&queue->isoch_loading);

	/* free queue */
	if (queue) 
		zfree(queue);
}

ISOCH_CALLBACK_QUEUE* isoch_queue_new()
{
	ISOCH_CALLBACK_QUEUE* queue;
	
	queue = (ISOCH_CALLBACK_QUEUE*) malloc(sizeof(ISOCH_CALLBACK_QUEUE));
	queue->isoch_num = 0;
	queue->curr = NULL;
	queue->head = NULL;
	queue->tail = NULL;   
	
	pthread_mutex_init(&queue->isoch_loading, NULL);
	
	/* load service */
	queue->get_next = isoch_queue_get_next;
	queue->has_next = isoch_queue_has_next;
	queue->rewind = isoch_queue_rewind;
	queue->register_data = isoch_queue_register_data;
	queue->unregister_data = isoch_queue_unregister_data;
	queue->free = isoch_queue_free;
	
	return queue;
}
