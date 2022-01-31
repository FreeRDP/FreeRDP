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

#if defined __linux__ && !defined ANDROID

#include <winpr/assert.h>
#include <termios.h>

#include <winpr/wlog.h>

#include "comm_serial_sys.h"
#include "comm_sercx_sys.h"

static BOOL _set_handflow(WINPR_COMM* pComm, const SERIAL_HANDFLOW* pHandflow)
{
	SERIAL_HANDFLOW SerCxHandflow;
	BOOL result = TRUE;
	SERIAL_DRIVER* pSerialSys = SerialSys_s();

	memcpy(&SerCxHandflow, pHandflow, sizeof(SERIAL_HANDFLOW));

	/* filter out unsupported bits by SerCx.sys
	 *
	 * http://msdn.microsoft.com/en-us/library/windows/hardware/jj680685%28v=vs.85%29.aspx
	 */

	SerCxHandflow.ControlHandShake =
	    pHandflow->ControlHandShake &
	    (SERIAL_DTR_CONTROL | SERIAL_DTR_HANDSHAKE | SERIAL_CTS_HANDSHAKE | SERIAL_DSR_HANDSHAKE);
	SerCxHandflow.FlowReplace =
	    pHandflow->FlowReplace & (SERIAL_RTS_CONTROL | SERIAL_RTS_HANDSHAKE);

	if (SerCxHandflow.ControlHandShake != pHandflow->ControlHandShake)
	{
		if (pHandflow->ControlHandShake & SERIAL_DCD_HANDSHAKE)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_DCD_HANDSHAKE not supposed to be implemented by SerCx.sys");
		}

		if (pHandflow->ControlHandShake & SERIAL_DSR_SENSITIVITY)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_DSR_SENSITIVITY not supposed to be implemented by SerCx.sys");
		}

		if (pHandflow->ControlHandShake & SERIAL_ERROR_ABORT)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_ERROR_ABORT not supposed to be implemented by SerCx.sys");
		}

		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		result = FALSE;
	}

	if (SerCxHandflow.FlowReplace != pHandflow->FlowReplace)
	{
		if (pHandflow->ControlHandShake & SERIAL_AUTO_TRANSMIT)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_AUTO_TRANSMIT not supposed to be implemented by SerCx.sys");
		}

		if (pHandflow->ControlHandShake & SERIAL_AUTO_RECEIVE)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_AUTO_RECEIVE not supposed to be implemented by SerCx.sys");
		}

		if (pHandflow->ControlHandShake & SERIAL_ERROR_CHAR)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_ERROR_CHAR not supposed to be implemented by SerCx.sys");
		}

		if (pHandflow->ControlHandShake & SERIAL_NULL_STRIPPING)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_NULL_STRIPPING not supposed to be implemented by SerCx.sys");
		}

		if (pHandflow->ControlHandShake & SERIAL_BREAK_CHAR)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_BREAK_CHAR not supposed to be implemented by SerCx.sys");
		}

		if (pHandflow->ControlHandShake & SERIAL_XOFF_CONTINUE)
		{
			CommLog_Print(WLOG_WARN,
			              "SERIAL_XOFF_CONTINUE not supposed to be implemented by SerCx.sys");
		}

		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		result = FALSE;
	}

	if (!pSerialSys->set_handflow(pComm, &SerCxHandflow))
		return FALSE;

	return result;
}

static BOOL _get_handflow(WINPR_COMM* pComm, SERIAL_HANDFLOW* pHandflow)
{
	BOOL result;
	SERIAL_DRIVER* pSerialSys = SerialSys_s();

	result = pSerialSys->get_handflow(pComm, pHandflow);

	/* filter out unsupported bits by SerCx.sys
	 *
	 * http://msdn.microsoft.com/en-us/library/windows/hardware/jj680685%28v=vs.85%29.aspx
	 */

	pHandflow->ControlHandShake =
	    pHandflow->ControlHandShake &
	    (SERIAL_DTR_CONTROL | SERIAL_DTR_HANDSHAKE | SERIAL_CTS_HANDSHAKE | SERIAL_DSR_HANDSHAKE);
	pHandflow->FlowReplace = pHandflow->FlowReplace & (SERIAL_RTS_CONTROL | SERIAL_RTS_HANDSHAKE);

	return result;
}

/* http://msdn.microsoft.com/en-us/library/windows/hardware/hh439605%28v=vs.85%29.aspx */
static const ULONG _SERCX_SYS_SUPPORTED_EV_MASK = SERIAL_EV_RXCHAR |
                                                  /* SERIAL_EV_RXFLAG   | */
                                                  SERIAL_EV_TXEMPTY | SERIAL_EV_CTS |
                                                  SERIAL_EV_DSR | SERIAL_EV_RLSD | SERIAL_EV_BREAK |
                                                  SERIAL_EV_ERR | SERIAL_EV_RING /* |
                                                               SERIAL_EV_PERR     |
                                                               SERIAL_EV_RX80FULL |
                                                               SERIAL_EV_EVENT1   |
                                                               SERIAL_EV_EVENT2*/
    ;

static BOOL _set_wait_mask(WINPR_COMM* pComm, const ULONG* pWaitMask)
{
	ULONG possibleMask;
	SERIAL_DRIVER* pSerialSys = SerialSys_s();

	possibleMask = *pWaitMask & _SERCX_SYS_SUPPORTED_EV_MASK;

	if (possibleMask != *pWaitMask)
	{
		CommLog_Print(WLOG_WARN,
		              "Not all wait events supported (SerCx.sys), requested events= 0x%08" PRIX32
		              ", possible events= 0x%08" PRIX32 "",
		              *pWaitMask, possibleMask);

		/* FIXME: shall we really set the possibleMask and return FALSE? */
		pComm->WaitEventMask = possibleMask;
		return FALSE;
	}

	/* NB: All events that are supported by SerCx.sys are supported by Serial.sys*/
	return pSerialSys->set_wait_mask(pComm, pWaitMask);
}

/* specific functions only */
static SERIAL_DRIVER _SerCxSys = {
	.id = SerialDriverSerCxSys,
	.name = _T("SerCx.sys"),
	.set_baud_rate = NULL,
	.get_baud_rate = NULL,
	.get_properties = NULL,
	.set_serial_chars = NULL,
	.get_serial_chars = NULL,
	.set_line_control = NULL,
	.get_line_control = NULL,
	.set_handflow = _set_handflow,
	.get_handflow = _get_handflow,
	.set_timeouts = NULL,
	.get_timeouts = NULL,
	.set_dtr = NULL,
	.clear_dtr = NULL,
	.set_rts = NULL,
	.clear_rts = NULL,
	.get_modemstatus = NULL,
	.set_wait_mask = _set_wait_mask,
	.get_wait_mask = NULL,
	.wait_on_mask = NULL,
	.set_queue_size = NULL,
	.purge = NULL,
	.get_commstatus = NULL,
	.set_break_on = NULL,
	.set_break_off = NULL,
	.set_xoff = NULL,
	.set_xon = NULL,
	.get_dtrrts = NULL,
	.config_size = NULL, /* not supported by SerCx.sys */
	.immediate_char = NULL,
	.reset_device = NULL, /* not supported by SerCx.sys */
};

SERIAL_DRIVER* SerCxSys_s(void)
{
	/* _SerCxSys completed with inherited functions from SerialSys */
	SERIAL_DRIVER* pSerialSys = SerialSys_s();
	if (!pSerialSys)
		return NULL;

	_SerCxSys.set_baud_rate = pSerialSys->set_baud_rate;
	_SerCxSys.get_baud_rate = pSerialSys->get_baud_rate;

	_SerCxSys.get_properties = pSerialSys->get_properties;

	_SerCxSys.set_serial_chars = pSerialSys->set_serial_chars;
	_SerCxSys.get_serial_chars = pSerialSys->get_serial_chars;
	_SerCxSys.set_line_control = pSerialSys->set_line_control;
	_SerCxSys.get_line_control = pSerialSys->get_line_control;

	_SerCxSys.set_timeouts = pSerialSys->set_timeouts;
	_SerCxSys.get_timeouts = pSerialSys->get_timeouts;

	_SerCxSys.set_dtr = pSerialSys->set_dtr;
	_SerCxSys.clear_dtr = pSerialSys->clear_dtr;

	_SerCxSys.set_rts = pSerialSys->set_rts;
	_SerCxSys.clear_rts = pSerialSys->clear_rts;

	_SerCxSys.get_modemstatus = pSerialSys->get_modemstatus;

	_SerCxSys.set_wait_mask = pSerialSys->set_wait_mask;
	_SerCxSys.get_wait_mask = pSerialSys->get_wait_mask;
	_SerCxSys.wait_on_mask = pSerialSys->wait_on_mask;

	_SerCxSys.set_queue_size = pSerialSys->set_queue_size;

	_SerCxSys.purge = pSerialSys->purge;

	_SerCxSys.get_commstatus = pSerialSys->get_commstatus;

	_SerCxSys.set_break_on = pSerialSys->set_break_on;
	_SerCxSys.set_break_off = pSerialSys->set_break_off;

	_SerCxSys.set_xoff = pSerialSys->set_xoff;
	_SerCxSys.set_xon = pSerialSys->set_xon;

	_SerCxSys.get_dtrrts = pSerialSys->get_dtrrts;

	_SerCxSys.immediate_char = pSerialSys->immediate_char;

	return &_SerCxSys;
}

#endif /* __linux__ */
