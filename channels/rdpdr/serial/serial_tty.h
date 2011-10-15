/**
 * FreeRDP: A Remote Desktop Protocol client.
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
#include <dirent.h>

typedef struct _SERIAL_TTY SERIAL_TTY;
struct _SERIAL_TTY
{
	uint32 id;
	int fd;

	int dtr;
	int rts;
	uint32 control;
	uint32 xonoff;
	uint32 onlimit;
	uint32 offlimit;
	uint32 baud_rate;
	uint32 queue_in_size;
	uint32 queue_out_size;
	uint32 wait_mask;
	uint32 read_interval_timeout;
	uint32 read_total_timeout_multiplier;
	uint32 read_total_timeout_constant;
	uint32 write_total_timeout_multiplier;
	uint32 write_total_timeout_constant;
	uint8 stop_bits;
	uint8 parity;
	uint8 word_length;
	uint8 chars[6];
	struct termios* ptermios;
	struct termios* pold_termios;
	int event_txempty;
	int event_cts;
	int event_dsr;
	int event_rlsd;
	int event_pending;
};


SERIAL_TTY* serial_tty_new(const char* path, uint32 id);
void serial_tty_free(SERIAL_TTY* tty);

boolean serial_tty_read(SERIAL_TTY* tty, uint8* buffer, uint32* Length);
boolean serial_tty_write(SERIAL_TTY* tty, uint8* buffer, uint32 Length);
uint32 serial_tty_control(SERIAL_TTY* tty, uint32 IoControlCode, STREAM* input, STREAM* output, uint32* abort_io);

boolean serial_tty_get_event(SERIAL_TTY* tty, uint32* result);

#endif /* __SERIAL_TTY_H */
