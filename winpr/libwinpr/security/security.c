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
#include <winpr/wlog.h>
#include "security.h"

BOOL InitializeSecurityDescriptor(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                  WINPR_ATTR_UNUSED DWORD dwRevision)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

DWORD GetSecurityDescriptorLength(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WLog_ERR("TODO", "TODO: Implement");
	return 0;
}

BOOL IsValidSecurityDescriptor(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL GetSecurityDescriptorControl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                  WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR_CONTROL pControl,
                                  WINPR_ATTR_UNUSED LPDWORD lpdwRevision)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL SetSecurityDescriptorControl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                  WINPR_ATTR_UNUSED SECURITY_DESCRIPTOR_CONTROL
                                      ControlBitsOfInterest,
                                  WINPR_ATTR_UNUSED SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL GetSecurityDescriptorDacl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                               WINPR_ATTR_UNUSED LPBOOL lpbDaclPresent,
                               WINPR_ATTR_UNUSED PACL* pDacl,
                               WINPR_ATTR_UNUSED LPBOOL lpbDaclDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL SetSecurityDescriptorDacl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                               WINPR_ATTR_UNUSED BOOL bDaclPresent, WINPR_ATTR_UNUSED PACL pDacl,
                               WINPR_ATTR_UNUSED BOOL bDaclDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL GetSecurityDescriptorGroup(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                WINPR_ATTR_UNUSED PSID* pGroup,
                                WINPR_ATTR_UNUSED LPBOOL lpbGroupDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL SetSecurityDescriptorGroup(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                WINPR_ATTR_UNUSED PSID pGroup,
                                WINPR_ATTR_UNUSED BOOL bGroupDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL GetSecurityDescriptorOwner(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                WINPR_ATTR_UNUSED PSID* pOwner,
                                WINPR_ATTR_UNUSED LPBOOL lpbOwnerDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL SetSecurityDescriptorOwner(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                WINPR_ATTR_UNUSED PSID pOwner,
                                WINPR_ATTR_UNUSED BOOL bOwnerDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

DWORD GetSecurityDescriptorRMControl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR SecurityDescriptor,
                                     WINPR_ATTR_UNUSED PUCHAR RMControl)
{
	WLog_ERR("TODO", "TODO: Implement");
	return 0;
}

DWORD SetSecurityDescriptorRMControl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR SecurityDescriptor,
                                     WINPR_ATTR_UNUSED PUCHAR RMControl)
{
	WLog_ERR("TODO", "TODO: Implement");
	return 0;
}

BOOL GetSecurityDescriptorSacl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                               WINPR_ATTR_UNUSED LPBOOL lpbSaclPresent,
                               WINPR_ATTR_UNUSED PACL* pSacl,
                               WINPR_ATTR_UNUSED LPBOOL lpbSaclDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL SetSecurityDescriptorSacl(WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                               WINPR_ATTR_UNUSED BOOL bSaclPresent, WINPR_ATTR_UNUSED PACL pSacl,
                               WINPR_ATTR_UNUSED BOOL bSaclDefaulted)
{
	WLog_ERR("TODO", "TODO: Implement");
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
