/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol
 *
 * Copyright 2015 ANSSI, Author Thomas Calderon
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
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

#ifndef WINPR_SSPI_KERBEROS_PRIVATE_H
#define WINPR_SSPI_KERBEROS_PRIVATE_H

#include <winpr/sspi.h>
#include <winpr/windows.h>

#include "../sspi.h"
#include "../../log.h"

#ifdef WITH_KRB5_MIT
#include <krb5/krb5.h>
#elif defined(WITH_KRB5_HEIMDAL)
#include <krb5.h>
#endif

typedef struct s_KRB_CONTEXT KRB_CONTEXT;

extern const SecPkgInfoA KERBEROS_SecPkgInfoA;
extern const SecPkgInfoW KERBEROS_SecPkgInfoW;
extern const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA;
extern const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW;

#endif /* WINPR_SSPI_KERBEROS_PRIVATE_H */
