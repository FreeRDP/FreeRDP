/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Stream Macro Utils
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef __STREAM_UTILS_H
#define __STREAM_UTILS_H

#define GET_UINT8(_p1, _offset) *(((uint8 *) _p1) + _offset)
#define GET_UINT8A(_dest, _src, _offset, _len) ( \
	memcpy( \
	((void *) _dest), \
	((const void *) (_src + _offset)), \
	((size_t) _len) \
	))
#define GET_UINT16(_p1, _offset) ( \
	(uint16) (*(((uint8 *) _p1) + _offset)) + \
	((uint16) (*(((uint8 *) _p1) + _offset + 1)) << 8))
#define GET_UINT32(_p1, _offset) ( \
	(uint32) (*(((uint8 *) _p1) + _offset)) + \
	((uint32) (*(((uint8 *) _p1) + _offset + 1)) << 8) + \
	((uint32) (*(((uint8 *) _p1) + _offset + 2)) << 16) + \
	((uint32) (*(((uint8 *) _p1) + _offset + 3)) << 24))
#define GET_UINT64(_p1, _offset) ( \
	(uint64) (*(((uint8 *) _p1) + _offset)) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 1)) << 8) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 2)) << 16) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 3)) << 24) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 4)) << 32) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 5)) << 40) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 6)) << 48) + \
	((uint64) (*(((uint8 *) _p1) + _offset + 7)) << 56))

#define SET_UINT8(_p1, _offset, _value) *(((uint8 *) _p1) + _offset) = (uint8) (_value)
#define SET_UINT8A(_dest, _offset, _src, _len) ( \
	memcpy( \
	((void *) (_dest + _offset)), \
	((const void *) _src), \
	((size_t) _len) \
	))
#define SET_UINT8V(_dest, _offset, _value, _len) ( \
	memset( \
	((void *) (_dest + _offset)), \
	((int) _value), \
	((size_t) _len) \
	))
#define SET_UINT16(_p1, _offset, _value) \
	*(((uint8 *) _p1) + _offset) = (uint8) (((uint16) (_value)) & 0xff), \
	*(((uint8 *) _p1) + _offset + 1) = (uint8) ((((uint16) (_value)) >> 8) & 0xff)
#define SET_UINT32(_p1, _offset, _value) \
	*(((uint8 *) _p1) + _offset) = (uint8) (((uint32) (_value)) & 0xff), \
	*(((uint8 *) _p1) + _offset + 1) = (uint8) ((((uint32) (_value)) >> 8) & 0xff), \
	*(((uint8 *) _p1) + _offset + 2) = (uint8) ((((uint32) (_value)) >> 16) & 0xff), \
	*(((uint8 *) _p1) + _offset + 3) = (uint8) ((((uint32) (_value)) >> 24) & 0xff)
#define SET_UINT64(_p1, _offset, _value) \
	*(((uint8 *) _p1) + _offset) = (uint8) (((uint64) (_value)) & 0xff), \
	*(((uint8 *) _p1) + _offset + 1) = (uint8) ((((uint64) (_value)) >> 8) & 0xff), \
	*(((uint8 *) _p1) + _offset + 2) = (uint8) ((((uint64) (_value)) >> 16) & 0xff), \
	*(((uint8 *) _p1) + _offset + 3) = (uint8) ((((uint64) (_value)) >> 24) & 0xff), \
	*(((uint8 *) _p1) + _offset + 4) = (uint8) ((((uint64) (_value)) >> 32) & 0xff), \
	*(((uint8 *) _p1) + _offset + 5) = (uint8) ((((uint64) (_value)) >> 40) & 0xff), \
	*(((uint8 *) _p1) + _offset + 6) = (uint8) ((((uint64) (_value)) >> 48) & 0xff), \
	*(((uint8 *) _p1) + _offset + 7) = (uint8) ((((uint64) (_value)) >> 56) & 0xff) 

#endif /* __STREAM_UTILS_H */

