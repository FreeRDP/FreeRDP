/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Serial Port Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#ifndef __SERIAL_TTY_H
#define __SERIAL_TTY_H

#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <dirent.h>
#endif

#include <freerdp/types.h>

#include <winpr/stream.h>

typedef struct _SERIAL_TTY SERIAL_TTY;

struct _SERIAL_TTY
{
	UINT32 id;
	int fd;

	int dtr;
	int rts;
	UINT32 control;
	UINT32 xonoff;
	UINT32 onlimit;
	UINT32 offlimit;
	UINT32 baud_rate;
	UINT32 queue_in_size;
	UINT32 queue_out_size;
	UINT32 wait_mask;
	UINT32 read_interval_timeout;
	UINT32 read_total_timeout_multiplier;
	UINT32 read_total_timeout_constant;
	UINT32 write_total_timeout_multiplier;
	UINT32 write_total_timeout_constant;
	BYTE stop_bits;
	BYTE parity;
	BYTE word_length;
	BYTE chars[6];
	struct termios* ptermios;
	struct termios* pold_termios;
	int event_txempty;
	int event_cts;
	int event_dsr;
	int event_rlsd;
	int event_pending;
	long timeout;
};

SERIAL_TTY* serial_tty_new(const char* path, UINT32 id);
void serial_tty_free(SERIAL_TTY* tty);

BOOL serial_tty_read(SERIAL_TTY* tty, BYTE* buffer, UINT32* Length);
int serial_tty_write(SERIAL_TTY* tty, BYTE* buffer, UINT32 Length);
UINT32 serial_tty_control(SERIAL_TTY* tty, UINT32 IoControlCode, wStream* input, wStream* output, UINT32* abort_io);

BOOL serial_tty_get_event(SERIAL_TTY* tty, UINT32* result);

#endif /* __SERIAL_TTY_H */
