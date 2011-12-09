/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Interface
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

#ifndef __FREERDP_API_H
#define __FREERDP_API_H

#define FREERDP_INTERFACE_VERSION 4

#if defined _WIN32 || defined __CYGWIN__
  #ifdef FREERDP_EXPORTS
    #ifdef __GNUC__
      #define FREERDP_API __attribute__((dllexport))
    #else
      #define FREERDP_API __declspec(dllexport)
    #endif
  #else
    #ifdef __GNUC__
      #define FREERDP_API __attribute__((dllimport))
    #else
      #define FREERDP_API __declspec(dllimport)
    #endif
  #endif
#else
  #if __GNUC__ >= 4
    #define FREERDP_API   __attribute__ ((visibility("default")))
  #else
    #define FREERDP_API
  #endif
#endif

#ifdef _WIN32
#define FREERDP_CC __cdecl
#else
#define FREERDP_CC
#endif

#ifdef _WIN32
#define INLINE	__inline
#else
#define INLINE	inline
#endif

#define IFCALL(_cb, ...) do { if (_cb != NULL) { _cb( __VA_ARGS__ ); } } while (0)
#define IFCALLRET(_cb, _ret, ...) do { if (_cb != NULL) { _ret = _cb( __VA_ARGS__ ); } } while (0)

#endif
