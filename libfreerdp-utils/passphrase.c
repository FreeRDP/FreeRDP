/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Passphrase Handling Utils
 *
 * Copyright 2011 Shea Levy <shea@shealevy.com>
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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <freerdp/utils/passphrase.h>

char* freerdp_passphrase_read(const char* prompt, char* buf, size_t bufsiz)
{
	char read_char;
	char* buf_iter;
	char term_name[L_ctermid];
	int term_id;
	size_t read_bytes = 0;

	if (bufsiz == 0)
		return NULL;

	ctermid(term_name);
	term_id = open(term_name, O_RDWR);

	write(term_id, prompt, strlen(prompt) + sizeof '\0');

	buf_iter = buf;
	while (read(term_id, &read_char, sizeof read_char) == (sizeof read_char))
	{
		read_bytes++;
		if (read_char == '\n')
			break;
		if (read_bytes < bufsiz)
		{
			*buf_iter = read_char;
			buf_iter++;
		}
	}
	*buf_iter = '\0';

	close(term_id);

	return buf;
}

