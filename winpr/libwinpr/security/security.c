/**
 * WinPR: Windows Portable Runtime
 * Base Security Functions
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

#include <winpr/config.h>

#include <winpr/crt.h>

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/security.h>

#include "../handle/handle.h"

/**
 * api-ms-win-security-base-l1-2-0.dll:
 *
 * AccessCheck
 * AccessCheckAndAuditAlarmW
 * AccessCheckByType
 * AccessCheckByTypeAndAuditAlarmW
 * AccessCheckByTypeResultList
 * AccessCheckByTypeResultListAndAuditAlarmByHandleW
 * AccessCheckByTypeResultListAndAuditAlarmW
 * AddAccessAllowedAce
 * AddAccessAllowedAceEx
 * AddAccessAllowedObjectAce
 * AddAccessDeniedAce
 * AddAccessDeniedAceEx
 * AddAccessDeniedObjectAce
 * AddAce
 * AddAuditAccessAce
 * AddAuditAccessAceEx
 * AddAuditAccessObjectAce
 * AddMandatoryAce
 * AddResourceAttributeAce
 * AddScopedPolicyIDAce
 * AdjustTokenGroups
 * AdjustTokenPrivileges
 * AllocateAndInitializeSid
 * AllocateLocallyUniqueId
 * AreAllAccessesGranted
 * AreAnyAccessesGranted
 * CheckTokenCapability
 * CheckTokenMembership
 * CheckTokenMembershipEx
 * ConvertToAutoInheritPrivateObjectSecurity
 * CopySid
 * CreatePrivateObjectSecurity
 * CreatePrivateObjectSecurityEx
 * CreatePrivateObjectSecurityWithMultipleInheritance
 * CreateRestrictedToken
 * CreateWellKnownSid
 * DeleteAce
 * DestroyPrivateObjectSecurity
 * DuplicateToken
 * DuplicateTokenEx
 * EqualDomainSid
 * EqualPrefixSid
 * EqualSid
 * FindFirstFreeAce
 * FreeSid
 * GetAce
 * GetAclInformation
 * GetAppContainerAce
 * GetCachedSigningLevel
 * GetFileSecurityW
 * GetKernelObjectSecurity
 * GetLengthSid
 * GetPrivateObjectSecurity
 * GetSidIdentifierAuthority
 * GetSidLengthRequired
 * GetSidSubAuthority
 * GetSidSubAuthorityCount
 * GetTokenInformation
 * GetWindowsAccountDomainSid
 * ImpersonateAnonymousToken
 * ImpersonateLoggedOnUser
 * ImpersonateSelf
 * InitializeAcl
 * InitializeSid
 * IsTokenRestricted
 * IsValidAcl
 * IsValidSid
 * IsWellKnownSid
 * MakeAbsoluteSD
 * MakeSelfRelativeSD
 * MapGenericMask
 * ObjectCloseAuditAlarmW
 * ObjectDeleteAuditAlarmW
 * ObjectOpenAuditAlarmW
 * ObjectPrivilegeAuditAlarmW
 * PrivilegeCheck
 * PrivilegedServiceAuditAlarmW
 * QuerySecurityAccessMask
 * RevertToSelf
 * SetAclInformation
 * SetCachedSigningLevel
 * SetFileSecurityW
 * SetKernelObjectSecurity
 * SetPrivateObjectSecurity
 * SetPrivateObjectSecurityEx
 * SetSecurityAccessMask
 * SetTokenInformation
 */

#ifndef _WIN32

#include "security.h"

BOOL InitializeSecurityDescriptor(PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD dwRevision)
{
	return TRUE;
}

DWORD GetSecurityDescriptorLength(PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return 0;
}

BOOL IsValidSecurityDescriptor(PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return TRUE;
}

BOOL GetSecurityDescriptorControl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                  PSECURITY_DESCRIPTOR_CONTROL pControl, LPDWORD lpdwRevision)
{
	return TRUE;
}

BOOL SetSecurityDescriptorControl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                  SECURITY_DESCRIPTOR_CONTROL ControlBitsOfInterest,
                                  SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet)
{
	return TRUE;
}

BOOL GetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSecurityDescriptor, LPBOOL lpbDaclPresent,
                               PACL* pDacl, LPBOOL lpbDaclDefaulted)
{
	return TRUE;
}

BOOL SetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSecurityDescriptor, BOOL bDaclPresent,
                               PACL pDacl, BOOL bDaclDefaulted)
{
	return TRUE;
}

BOOL GetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR pSecurityDescriptor, PSID* pGroup,
                                LPBOOL lpbGroupDefaulted)
{
	return TRUE;
}

BOOL SetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR pSecurityDescriptor, PSID pGroup,
                                BOOL bGroupDefaulted)
{
	return TRUE;
}

BOOL GetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR pSecurityDescriptor, PSID* pOwner,
                                LPBOOL lpbOwnerDefaulted)
{
	return TRUE;
}

BOOL SetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR pSecurityDescriptor, PSID pOwner,
                                BOOL bOwnerDefaulted)
{
	return TRUE;
}

DWORD GetSecurityDescriptorRMControl(PSECURITY_DESCRIPTOR SecurityDescriptor, PUCHAR RMControl)
{
	return 0;
}

DWORD SetSecurityDescriptorRMControl(PSECURITY_DESCRIPTOR SecurityDescriptor, PUCHAR RMControl)
{
	return 0;
}

BOOL GetSecurityDescriptorSacl(PSECURITY_DESCRIPTOR pSecurityDescriptor, LPBOOL lpbSaclPresent,
                               PACL* pSacl, LPBOOL lpbSaclDefaulted)
{
	return TRUE;
}

BOOL SetSecurityDescriptorSacl(PSECURITY_DESCRIPTOR pSecurityDescriptor, BOOL bSaclPresent,
                               PACL pSacl, BOOL bSaclDefaulted)
{
	return TRUE;
}

#endif

BOOL AccessTokenIsValid(HANDLE handle)
{
	WINPR_HANDLE* h = (WINPR_HANDLE*)handle;

	if (!h || (h->Type != HANDLE_TYPE_ACCESS_TOKEN))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	return TRUE;
}
