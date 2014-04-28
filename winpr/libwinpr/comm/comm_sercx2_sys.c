/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef _WIN32

#include "comm_sercx_sys.h"

#include "comm_sercx2_sys.h"


/* specific functions only */
static REMOTE_SERIAL_DRIVER _SerCx2Sys = 
{	
	.id		= RemoteSerialDriverSerCx2Sys,
	.name		= _T("SerCx2.sys"),
	.set_baud_rate	= NULL,
	.get_baud_rate  = NULL,
	.get_properties = NULL,
};


REMOTE_SERIAL_DRIVER* SerCx2Sys_s()
{
	/* _SerCx2Sys completed with SerialSys or SerCxSys default functions */
	//REMOTE_SERIAL_DRIVER* pSerialSys = SerialSys_s();
	REMOTE_SERIAL_DRIVER* pSerCxSys = SerCxSys_s();

	_SerCx2Sys.set_baud_rate = pSerCxSys->set_baud_rate;
	_SerCx2Sys.get_baud_rate = pSerCxSys->get_baud_rate;
	_SerCx2Sys.get_properties = pSerCxSys->get_properties;

	return &_SerCx2Sys;
}

#endif /* _WIN32 */
