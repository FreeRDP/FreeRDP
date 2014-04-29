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


/* http://msdn.microsoft.com/en-us/library/dn265347%28v=vs.85%29.aspx
 *
 * SerCx2 does not support special characters. SerCx2 always completes
 * an IOCTL_SERIAL_SET_CHARS request with a STATUS_SUCCESS status
 * code, but does not set any special characters or perform any other
 * operation in response to this request. For an
 * IOCTL_SERIAL_GET_CHARS request, SerCx2 sets all the character
 * values in the SERIAL_CHARS structure to null, and completes the
 * request with a STATUS_SUCCESS status code.
 */

static BOOL _set_serial_chars(WINPR_COMM* pComm, const SERIAL_CHARS* pSerialChars)
{
	return TRUE;
}


static BOOL _get_serial_chars(WINPR_COMM* pComm, SERIAL_CHARS* pSerialChars)
{
	ZeroMemory(pSerialChars, sizeof(SERIAL_CHARS));
	return TRUE;
}


/* specific functions only */
static REMOTE_SERIAL_DRIVER _SerCx2Sys = 
{	
	.id		  = RemoteSerialDriverSerCx2Sys,
	.name		  = _T("SerCx2.sys"),
	.set_baud_rate	  = NULL,
	.get_baud_rate    = NULL,
	.get_properties   = NULL,
	.set_serial_chars = _set_serial_chars,
	.get_serial_chars = _get_serial_chars,
	.set_line_control = NULL,
	.get_line_control = NULL,
};


REMOTE_SERIAL_DRIVER* SerCx2Sys_s()
{
	/* _SerCx2Sys completed with inherited functions from SerialSys or SerCxSys */
	//REMOTE_SERIAL_DRIVER* pSerialSys = SerialSys_s();
	REMOTE_SERIAL_DRIVER* pSerCxSys = SerCxSys_s();

	_SerCx2Sys.set_baud_rate    = pSerCxSys->set_baud_rate;
	_SerCx2Sys.get_baud_rate    = pSerCxSys->get_baud_rate;
	_SerCx2Sys.get_properties   = pSerCxSys->get_properties;

	_SerCx2Sys.set_line_control = pSerCxSys->set_line_control;
	_SerCx2Sys.get_line_control = pSerCxSys->get_line_control;

	return &_SerCx2Sys;
}

#endif /* _WIN32 */
