/**
 * FreeRDP: A Remote Desktop Protocol Client
 * ASN.1 Basic Encoding Rules (DER)
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/crypto/der.h>

int der_write_length(STREAM* s, int length)
{
	if (length > 0x7F && length <= 0xFF)
	{
		stream_write_uint8(s, 0x81);
		stream_write_uint8(s, length);
		return 2;
	}
	else if (length > 0xFF)
	{
		stream_write_uint8(s, 0x82);
		stream_write_uint16_be(s, length);
		return 3;
	}
	else
	{
		stream_write_uint8(s, length);
		return 1;
	}
}

boolean der_write_general_string(STREAM* s, char* str)
{
	STREAM* tmp_s;

	tmp_s = stream_new(0);
	stream_attach(tmp_s, (uint8*)str, strlen(str));

	der_write_universal_tag(s, ER_TAG_GENERAL_STRING, false);
	der_write_length(s, strlen(str));

	stream_copy(s, tmp_s, strlen(str));
	stream_detach(tmp_s);

	return true;
}

char* der_read_general_string(STREAM* s, int *length)
{
	char* str;
	int len;

	if(der_read_universal_tag(s, ER_TAG_GENERAL_STRING, false))
	{
		der_read_length(s, &len);
		str = (char*)xzalloc((len + 1) * sizeof(char));
		memcpy(str, s->p, len);
		stream_seek(s, len);
		*length = len + 2;
		return str;
	}

	stream_rewind(s, 1);
	*length = 0;

	return NULL;
}

int der_write_principal_name(STREAM* s, uint8 ntype, char** name)
{
	uint8 len;
	char** p;

	len = 0;
	p = name;

	while (*p != NULL)
	{	
		len += strlen(*p) + 2;
		p++;
	}

	p = name;
	der_write_sequence_tag(s, len+9);
	der_write_contextual_tag(s, 0, 3, true);
	der_write_integer(s, ntype);
	der_write_contextual_tag(s, 1, len + 2, true);
	der_write_sequence_tag(s, len);

	while (*p != NULL)
	{
		der_write_general_string(s, *p);
		p++;
	}

	return len + 11;
}

int der_write_generalized_time(STREAM* s, char* tstr)
{
	uint8 len;
	STREAM* tmp_s;

	len = strlen(tstr);
	tmp_s = stream_new(0);

	stream_attach(tmp_s, (uint8*) tstr, strlen(tstr));
	der_write_universal_tag(s, ER_TAG_GENERALIZED_TIME, false);
	der_write_length(s, len);

	stream_copy(s, tmp_s, len);
	stream_detach(tmp_s);

	return len + 2;
}

boolean der_read_generalized_time(STREAM* s, char** tstr)
{
	int length;
	uint8* bm;
	stream_get_mark(s, bm);

	if (!der_read_universal_tag(s, ER_TAG_GENERALIZED_TIME, false))
		goto err;

	der_read_length(s, &length);

	if (length != 15)
		goto err;

	*tstr = xzalloc(length + 1);
	stream_read(s, *tstr, length);

	return true;

	err:
		stream_set_mark(s, bm);
		return false;
}

