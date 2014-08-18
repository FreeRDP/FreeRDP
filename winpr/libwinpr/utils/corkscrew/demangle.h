/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* C++ symbol name demangling. */

#ifndef _CORKSCREW_DEMANGLE_H
#define _CORKSCREW_DEMANGLE_H

#include <sys/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Demangles a C++ symbol name.
 * If name is NULL or if the name cannot be demangled, returns NULL.
 * Otherwise, returns a newly allocated string that contains the demangled name.
 *
 * The caller must free the returned string using free().
 */
char* demangle_symbol_name(const char* name);

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_DEMANGLE_H
