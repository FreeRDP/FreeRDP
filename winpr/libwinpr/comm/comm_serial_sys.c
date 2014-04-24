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

#include "comm_serial_sys.h"

static BOOL _set_baud_rate(PSERIAL_BAUD_RATE pBaudRate)
{

	return FALSE;
}


static REMOTE_SERIAL_DRIVER _SerialSys = 
{
	.id		= RemoteSerialDriverSerialSys,
	.name		= _T("Serial.sys"),
	.set_baud_rate	= _set_baud_rate,
};


PREMOTE_SERIAL_DRIVER SerialSys()
{
	return &_SerialSys;
}
