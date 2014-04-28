/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
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

#ifndef COMM_SERIAL_SYS_H
#define COMM_SERIAL_SYS_H

#ifndef _WIN32

#include "comm_ioctl.h"

#ifdef __cplusplus 
extern "C" { 
#endif

/* Ntddser.h: http://msdn.microsoft.com/en-us/cc308432.aspx */
#define SERIAL_BAUD_075          ((ULONG)0x00000001) 
#define SERIAL_BAUD_110          ((ULONG)0x00000002) 
#define SERIAL_BAUD_134_5        ((ULONG)0x00000004) 
#define SERIAL_BAUD_150          ((ULONG)0x00000008) 
#define SERIAL_BAUD_300          ((ULONG)0x00000010) 
#define SERIAL_BAUD_600          ((ULONG)0x00000020) 
#define SERIAL_BAUD_1200         ((ULONG)0x00000040) 
#define SERIAL_BAUD_1800         ((ULONG)0x00000080) 
#define SERIAL_BAUD_2400         ((ULONG)0x00000100) 
#define SERIAL_BAUD_4800         ((ULONG)0x00000200) 
#define SERIAL_BAUD_7200         ((ULONG)0x00000400) 
#define SERIAL_BAUD_9600         ((ULONG)0x00000800) 
#define SERIAL_BAUD_14400        ((ULONG)0x00001000) 
#define SERIAL_BAUD_19200        ((ULONG)0x00002000) 
#define SERIAL_BAUD_38400        ((ULONG)0x00004000) 
#define SERIAL_BAUD_56K          ((ULONG)0x00008000) 
#define SERIAL_BAUD_128K         ((ULONG)0x00010000) 
#define SERIAL_BAUD_115200       ((ULONG)0x00020000) 
#define SERIAL_BAUD_57600        ((ULONG)0x00040000) 
#define SERIAL_BAUD_USER         ((ULONG)0x10000000) 


REMOTE_SERIAL_DRIVER* SerialSys_s();

#ifdef __cplusplus 
}
#endif


#endif /* _WIN32 */

#endif /* COMM_SERIAL_SYS_H */
