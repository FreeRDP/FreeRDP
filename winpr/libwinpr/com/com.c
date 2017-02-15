/**
 * WinPR: Windows Portable Runtime
 * Component Object Model (COM)
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/error.h>

/**
 * api-ms-win-core-com-l1-1-0.dll:
 *
 * CLSIDFromProgID
 * CLSIDFromString
 * CoAddRefServerProcess
 * CoAllowUnmarshalerCLSID
 * CoCancelCall
 * CoCopyProxy
 * CoCreateFreeThreadedMarshaler
 * CoCreateGuid
 * CoCreateInstance
 * CoCreateInstanceEx
 * CoCreateInstanceFromApp
 * CoDecodeProxy
 * CoDecrementMTAUsage
 * CoDisableCallCancellation
 * CoDisconnectContext
 * CoDisconnectObject
 * CoEnableCallCancellation
 * CoFreeUnusedLibraries
 * CoFreeUnusedLibrariesEx
 * CoGetApartmentType
 * CoGetCallContext
 * CoGetCallerTID
 * CoGetCancelObject
 * CoGetClassObject
 * CoGetContextToken
 * CoGetCurrentLogicalThreadId
 * CoGetCurrentProcess
 * CoGetDefaultContext
 * CoGetInterfaceAndReleaseStream
 * CoGetMalloc
 * CoGetMarshalSizeMax
 * CoGetObjectContext
 * CoGetPSClsid
 * CoGetStandardMarshal
 * CoGetStdMarshalEx
 * CoGetTreatAsClass
 * CoImpersonateClient
 * CoIncrementMTAUsage
 * CoInitializeEx
 * CoInitializeSecurity
 * CoInvalidateRemoteMachineBindings
 * CoIsHandlerConnected
 * CoLockObjectExternal
 * CoMarshalHresult
 * CoMarshalInterface
 * CoMarshalInterThreadInterfaceInStream
 * CoQueryAuthenticationServices
 * CoQueryClientBlanket
 * CoQueryProxyBlanket
 * CoRegisterClassObject
 * CoRegisterPSClsid
 * CoRegisterSurrogate
 * CoReleaseMarshalData
 * CoReleaseServerProcess
 * CoResumeClassObjects
 * CoRevertToSelf
 * CoRevokeClassObject
 * CoSetCancelObject
 * CoSetProxyBlanket
 * CoSuspendClassObjects
 * CoSwitchCallContext
 * CoTaskMemAlloc
 * CoTaskMemFree
 * CoTaskMemRealloc
 * CoTestCancel
 * CoUninitialize
 * CoUnmarshalHresult
 * CoUnmarshalInterface
 * CoWaitForMultipleHandles
 * CoWaitForMultipleObjects
 * CreateStreamOnHGlobal
 * FreePropVariantArray
 * GetHGlobalFromStream
 * IIDFromString
 * ProgIDFromCLSID
 * PropVariantClear
 * PropVariantCopy
 * StringFromCLSID
 * StringFromGUID2
 * StringFromIID
 */

#ifndef _WIN32

HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit)
{
	return S_OK;
}

void CoUninitialize(void)
{

}

#endif
