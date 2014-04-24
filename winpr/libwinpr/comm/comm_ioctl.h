/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
 * Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#ifndef WINPR_COMM_IOCTL_H_
#define WINPR_COMM_IOCTL_H_

#ifndef _WIN32

#include <winpr/io.h>
#include <winpr/tchar.h>
#include <winpr/wtypes.h>

#include "comm.h"

/* Ntddser.h http://msdn.microsoft.com/en-us/cc308432.aspx
 * Ntddpar.h http://msdn.microsoft.com/en-us/cc308431.aspx
 */

#ifdef __cplusplus 
extern "C" { 
#endif


#define IOCTL_SERIAL_SET_BAUD_RATE 0x001B0004



typedef struct _SERIAL_BAUD_RATE
{
	ULONG BaudRate;
} SERIAL_BAUD_RATE, *PSERIAL_BAUD_RATE;


/**
 * A function might be NULL if not supported by the underlying remote driver.
 *
 * FIXME: better have to use input and output buffers for all functions?
 */
typedef struct _REMOTE_SERIAL_DRIVER
{
	REMOTE_SERIAL_DRIVER_ID id;
	TCHAR *name;
	BOOL (*set_baud_rate)(PSERIAL_BAUD_RATE pBaudRate);

} REMOTE_SERIAL_DRIVER, *PREMOTE_SERIAL_DRIVER;



#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* WINPR_COMM_IOCTL_H_ */
