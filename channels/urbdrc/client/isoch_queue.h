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

#ifndef __ISOCH_QUEUE_H
#define __ISOCH_QUEUE_H

#include "urbdrc_types.h"


typedef struct _ISOCH_CALLBACK_DATA ISOCH_CALLBACK_DATA;
typedef struct _ISOCH_CALLBACK_QUEUE ISOCH_CALLBACK_QUEUE;


struct _ISOCH_CALLBACK_DATA
{
	void * inode;
	void * prev;
	void * next;
	void * device;
	BYTE * out_data;
	UINT32 out_size;
	void * callback;
};



struct _ISOCH_CALLBACK_QUEUE
{
	int isoch_num;
	ISOCH_CALLBACK_DATA* curr; /* current point */
	ISOCH_CALLBACK_DATA* head; /* head point in linked list */
	ISOCH_CALLBACK_DATA* tail; /* tail point in linked list */
	
	pthread_mutex_t isoch_loading;
	
	/* Isochronous queue service */
	void (*rewind) (ISOCH_CALLBACK_QUEUE * queue);
	int (*has_next) (ISOCH_CALLBACK_QUEUE * queue);
	int (*unregister_data) (ISOCH_CALLBACK_QUEUE* queue, ISOCH_CALLBACK_DATA* isoch);
	ISOCH_CALLBACK_DATA *(*get_next) (ISOCH_CALLBACK_QUEUE * queue);
	ISOCH_CALLBACK_DATA *(*register_data) (ISOCH_CALLBACK_QUEUE* queue, 
		void * callback, void * dev);
	void (*free) (ISOCH_CALLBACK_QUEUE * queue);
	
};


ISOCH_CALLBACK_QUEUE* isoch_queue_new(void);

	

#endif /* __ISOCH_QUEUE_H */
