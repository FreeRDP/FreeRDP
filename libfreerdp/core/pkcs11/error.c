/*
 * PKCS #11 PAM Login Module
 * Copyright (C) 2003 Mario Strasser <mast@gmx.net>,
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * $Id$
 */

#include "error.h"
#include <string.h>
#include <stdio.h>

#define __ERROR_C_

static char error_buffer[ERROR_BUFFER_SIZE] = "";

/**
* store an error message into a temporary buffer, in a similar way as sprintf does
* @param format String to be stored
* @param ... Additional parameters
*/
void set_error(const char *format, ...) {
  static char tmp[ERROR_BUFFER_SIZE];
  va_list ap;
  va_start(ap, format);
  vsnprintf(tmp, ERROR_BUFFER_SIZE, format, ap);
  va_end(ap);
  strcpy(error_buffer, tmp);
}

/**
* Retrieve error message string from buffer
*@return Error message
*/
const char *get_error(void) {
  return (const char *)error_buffer;
}
