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

/* specific functions only */
static REMOTE_SERIAL_DRIVER _SerCxSys = 
{
	.id		= RemoteSerialDriverSerCxSys,
	.name		= _T("SerCx.sys"),
	.set_baud_rate	= NULL,
};



PREMOTE_SERIAL_DRIVER SerCxSys()
{
	/* _SerCxSys completed with default SerialSys functions */
	PREMOTE_SERIAL_DRIVER serialSys = SerialSys();

	_SerCxSys.set_baud_rate = serialSys->set_baud_rate;

	return &_SerCxSys;
}
