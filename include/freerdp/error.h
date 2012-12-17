/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Error Codes
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_ERROR_H
#define FREERDP_ERROR_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
* This static variable holds an error code if the return value from connect is FALSE.
* This variable is always set to 0 in the beginning of the connect sequence.
* The returned code can be used to inform the user of the detailed connect error.
* The value can hold one of the defined error codes below OR an error according to errno
*/

extern int connectErrorCode;

#define ERRORSTART 10000
#define PREECONNECTERROR ERRORSTART + 1
#define UNDEFINEDCONNECTERROR ERRORSTART + 2
#define POSTCONNECTERROR ERRORSTART + 3
#define DNSERROR ERRORSTART + 4      /* general DNS ERROR */
#define DNSNAMENOTFOUND ERRORSTART + 5 /* EAI_NONAME */
#define CONNECTERROR ERRORSTART + 6  /* a connect error if errno is not define during tcp connect */
#define MCSCONNECTINITIALERROR ERRORSTART + 7
#define TLSCONNECTERROR ERRORSTART + 8
#define AUTHENTICATIONERROR ERRORSTART + 9

#ifdef	__cplusplus
}
#endif

#endif	/* FREERDP_ERROR_H */
