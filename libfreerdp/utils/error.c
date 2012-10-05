/*
 * FreeRDP: A Remote Desktop Protocol Client
 * Error Message Handling
 *
 * Copyright 2012 Gerald Richter
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/utils/memory.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/error.h>

#include <winpr/synch.h>

static HANDLE error_mutex;
static LIST*  error_list;
static size_t error_list_msg_size ;

/**
 * Init the error store
 */

void error_init ()
{
    error_mutex = CreateMutex(NULL, FALSE, NULL);
    error_list = list_new();
}

/**
 * Cleanup the error store
 */

void error_free ()
{
    if (error_mutex)
        CloseHandle (error_mutex) ;
    if (error_list)
        list_free(error_list);
}

/**
 * Stores the given error message in the error message store and
 * prints it on stderr
 *
 * @param fmt - printf format
 * @param ... - printf parameter
 * 
 */

void error_report (const char *fmt, ...)
{
    char    msg[8192];
    va_list ap;
    
    va_start (ap, fmt);
    vsnprintf (msg,sizeof(msg), fmt, ap); 
    va_end (ap);

    fprintf(stderr, "%s", msg) ;

    if (!error_mutex)
        return ;

    if (WaitForSingleObject(error_mutex, 1000) == WAIT_OBJECT_0)
    {    
        list_enqueue (error_list, xstrdup(msg)) ;
        error_list_msg_size += strlen (msg) ;
    }
    ReleaseMutex (error_mutex) ;
}

/**
 * Shows all errors (if any) form the current message store as message box
 * must only be called from main thread
 *
 * 
 */

void error_show ()
{
    if (!error_mutex || list_size (error_list)== 0)
        return ;

    if (WaitForSingleObject(error_mutex, 1000) == WAIT_OBJECT_0)
    {
        int    n      = list_size (error_list) ;
        int    len    = error_list_msg_size + n + 1 ;
        char * allmsg = xmalloc (len) ;
        char * msg ;
        
        *allmsg = '\0' ;
        while (msg = (char *)list_dequeue (error_list))
        {
            strcat (allmsg, msg) ;
            if (--n > 0)
                strcat (allmsg, "\n") ;
        }
        MessageBoxA(NULL, allmsg, NULL, 0);
    }
    ReleaseMutex (error_mutex) ;

}