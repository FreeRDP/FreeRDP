/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-json.h - JSON declarations */
/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef K5_JSON_H
#define K5_JSON_H

#include <stddef.h>

#define K5_JSON_TID_NUMBER 0
#define K5_JSON_TID_NULL 1
#define K5_JSON_TID_BOOL 2
#define K5_JSON_TID_MEMORY 128
#define K5_JSON_TID_ARRAY 129
#define K5_JSON_TID_OBJECT 130
#define K5_JSON_TID_STRING 131

/*
 * The k5_json_value C type can represent any kind of JSON value.  It has no
 * static type safety since it is represented using a void pointer, so be
 * careful with it.  Its type can be checked dynamically with k5_json_get_tid()
 * and the above constants.
 */
typedef void *k5_json_value;
typedef unsigned int k5_json_tid;

k5_json_tid k5_json_get_tid(k5_json_value val);

/*
 * k5_json_value objects are reference-counted.  These functions increment and
 * decrement the refcount, possibly freeing the value.  k5_json_retain returns
 * its argument and always succeeds.  Both functions gracefully accept NULL.
 */
k5_json_value k5_json_retain(k5_json_value val);
void k5_json_release(k5_json_value val);

/*
 * If a k5_json_* function can fail, it returns 0 on success and an errno value
 * on failure.
 */

/*
 * Null
 */

typedef struct k5_json_null_st *k5_json_null;

int k5_json_null_create(k5_json_null *null_out);

/* Create a null value as a k5_json_value, for polymorphic convenience. */
int k5_json_null_create_val(k5_json_value *val_out);

/*
 * Boolean
 */

typedef struct k5_json_bool_st *k5_json_bool;

int k5_json_bool_create(int truth, k5_json_bool *val_out);
int k5_json_bool_value(k5_json_bool bval);

/*
 * Array
 */

typedef struct k5_json_array_st *k5_json_array;

int k5_json_array_create(k5_json_array *val_out);
size_t k5_json_array_length(k5_json_array array);

/* Both of these functions increment the reference count on val. */
int k5_json_array_add(k5_json_array array, k5_json_value val);
void k5_json_array_set(k5_json_array array, size_t idx, k5_json_value val);

/* Get an alias to the idx-th element of array, without incrementing the
 * reference count.  The caller must check idx against the array length. */
k5_json_value k5_json_array_get(k5_json_array array, size_t idx);

/*
 * Create an array from a template and a variable argument list.  template
 * characters are:
 *   v: a k5_json_value argument is read, retained, and stored
 *   n: no argument is read; a null value is stored
 *   b: an int argument is read and stored as a boolean value
 *   i: an int argument is read and stored as a number value
 *   L: a long long argument is read and stored as a number value
 *   s: a const char * argument is read and stored as a null or string value
 *   B: const void * and size_t arguments are read and stored as a base64
 *      string value
 */
int
k5_json_array_fmt(k5_json_array *array_out, const char *template, ...);

/*
 * Object
 */

typedef struct k5_json_object_st *k5_json_object;
typedef void (*k5_json_object_iterator_fn)(void *arg, const char *key,
                                           k5_json_value val);

int k5_json_object_create(k5_json_object *val_out);
void k5_json_object_iterate(k5_json_object obj,
                            k5_json_object_iterator_fn func, void *arg);

/* Return the number of mappings in an object. */
size_t k5_json_object_count(k5_json_object obj);

/*
 * Store val into object at key, incrementing val's reference count and
 * releasing any previous value at key.  If val is NULL, key is removed from
 * obj if it exists, and obj remains unchanged if it does not.
 */
int k5_json_object_set(k5_json_object obj, const char *key, k5_json_value val);

/* Get an alias to the object's value for key, without incrementing the
 * reference count.  Returns NULL if there is no value for key. */
k5_json_value k5_json_object_get(k5_json_object obj, const char *key);

/*
 * String
 */

typedef struct k5_json_string_st *k5_json_string;

int k5_json_string_create(const char *cstring, k5_json_string *val_out);
int k5_json_string_create_len(const void *data, size_t len,
                              k5_json_string *val_out);
const char *k5_json_string_utf8(k5_json_string string);


/* Create a base64 string value from binary data. */
int k5_json_string_create_base64(const void *data, size_t len,
                                 k5_json_string *val_out);

/* Decode the base64 contents of string. */
int k5_json_string_unbase64(k5_json_string string, unsigned char **data_out,
                            size_t *len_out);

/*
 * Number
 */

typedef struct k5_json_number_st *k5_json_number;

int k5_json_number_create(long long number, k5_json_number *val_out);
long long k5_json_number_value(k5_json_number number);

/*
 * JSON encoding and decoding
 */

int k5_json_encode(k5_json_value val, char **json_out);
int k5_json_decode(const char *str, k5_json_value *val_out);

#endif /* K5_JSON_H */
