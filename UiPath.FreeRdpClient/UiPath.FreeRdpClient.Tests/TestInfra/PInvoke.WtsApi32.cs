using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Windows.Win32;

internal partial class PInvoke//WtsApi32
{
    /// <summary>Connects a Remote Desktop Services session to an existing session on the local computer.</summary>
    /// <param name="LogonId">
    /// <para>The logon ID of the session to connect to. The user of that session must have permissions to connect to an existing session. The output of this session will be routed to the session identified by the <i>TargetLogonId</i> parameter. This can be <b>LOGONID_CURRENT</b> to use the current session.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsconnectsessionw#parameters">Read more on docs.microsoft.com</see>.</para>
    /// </param>
    /// <param name="TargetLogonId">
    /// <para>The logon ID of the session to receive the output of the session represented by the <i>LogonId</i> parameter. The output of the session identified by the <i>LogonId</i> parameter will be routed to this session. This can be <b>LOGONID_CURRENT</b> to use the current session.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsconnectsessionw#parameters">Read more on docs.microsoft.com</see>.</para>
    /// </param>
    /// <param name="pPassword">A pointer to the password for the user account that is specified in the <i>LogonId</i> parameter. The value of <i>pPassword</i> can be an empty string if the caller is logged on using the same domain name and user name as the logon ID. The value of <i>pPassword</i> cannot be <b>NULL</b>.</param>
    /// <param name="bWait">Indicates whether the operation is synchronous. Specify <b>TRUE</b> to wait for the operation to complete, or <b>FALSE</b> to return immediately.</param>
    /// <returns>
    /// <para>If the function succeeds, the return value is a nonzero value. If the function fails, the return value is zero. To get extended error information, call <a href="/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a>.</para>
    /// </returns>
    /// <remarks>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsconnectsessionw">Learn more about this API from docs.microsoft.com</see>.</para>
    /// </remarks>
    [DllImport("WtsApi32", ExactSpelling = true, EntryPoint = "WTSConnectSessionW", SetLastError = true)]
    [DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
    [SupportedOSPlatform("windows6.0.6000")]
    internal static extern Foundation.BOOL WTSConnectSession(uint LogonId, uint TargetLogonId, [MarshalAs(UnmanagedType.LPWStr)][In] string pPassword, bool bWait);

    /// <summary>Retrieves session information for the specified session on the specified Remote Desktop Session Host (RD Session Host) server.</summary>
    /// <param name="hServer">
    /// <para>A handle to an RD Session Host server. Specify a handle opened by the <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/nf-wtsapi32-wtsopenservera">WTSOpenServer</a> function, or specify <b>WTS_CURRENT_SERVER_HANDLE</b> to indicate the RD Session Host server on which your application is running.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsquerysessioninformationw#parameters">Read more on docs.microsoft.com</see>.</para>
    /// </param>
    /// <param name="SessionId">
    /// <para>A Remote Desktop Services session identifier. To indicate the session in which the calling application is running (or the current session) specify <b>WTS_CURRENT_SESSION</b>. Only specify <b>WTS_CURRENT_SESSION</b> when obtaining session information on the local server. If <b>WTS_CURRENT_SESSION</b> is specified when querying session information on a remote server, the returned session information will be inconsistent. Do not use the returned data. You can use the <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/nf-wtsapi32-wtsenumeratesessionsa">WTSEnumerateSessions</a> function to retrieve the identifiers of all sessions on a specified RD Session Host server. To query information for another user's session, you must have Query Information permission. For more information, see <a href="https://docs.microsoft.com/windows/desktop/TermServ/terminal-services-permissions">Remote Desktop Services Permissions</a>. To modify permissions on a session, use the Remote Desktop Services Configuration administrative tool.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsquerysessioninformationw#parameters">Read more on docs.microsoft.com</see>.</para>
    /// </param>
    /// <param name="WTSInfoClass">
    /// <para>A value of the <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ne-wtsapi32-wts_info_class">WTS_INFO_CLASS</a> enumeration that indicates the type of session information to retrieve in a call to the <b>WTSQuerySessionInformation</b> function.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsquerysessioninformationw#parameters">Read more on docs.microsoft.com</see>.</para>
    /// </param>
    /// <param name="ppBuffer">
    /// <para>A pointer to a variable that receives a pointer to the requested information. The format and contents of the data depend on the information class specified in the <i>WTSInfoClass</i> parameter. To free the returned buffer, call the <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/nf-wtsapi32-wtsfreememory">WTSFreeMemory</a> function.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsquerysessioninformationw#parameters">Read more on docs.microsoft.com</see>.</para>
    /// </param>
    /// <param name="pBytesReturned">
    /// <para>A pointer to a variable that receives the size, in bytes, of the data returned in <i>ppBuffer</i>.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsquerysessioninformationw#parameters">Read more on docs.microsoft.com</see>.</para>
    /// </param>
    /// <returns>
    /// <para>If the function succeeds, the return value is a nonzero value. If the function fails, the return value is zero. To get extended error information, call <a href="/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a>.</para>
    /// </returns>
    /// <remarks>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsquerysessioninformationw">Learn more about this API from docs.microsoft.com</see>.</para>
    /// </remarks>
    [DllImport("WtsApi32", ExactSpelling = true, EntryPoint = "WTSQuerySessionInformationW", SetLastError = true)]
    [DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
    [SupportedOSPlatform("windows6.0.6000")]
    internal static extern Foundation.BOOL WTSQuerySessionInformation(Foundation.HANDLE hServer, uint SessionId, System.RemoteDesktop.WTS_INFO_CLASS WTSInfoClass, out IntPtr ppBuffer, out uint pBytesReturned);

    /// <summary>Frees memory allocated by a Remote Desktop Services function.</summary>
    /// <param name="pMemory">Pointer to the memory to free.</param>
    /// <remarks>
    /// <para>Several Remote Desktop Services functions allocate buffers to return information. Use the <b>WTSFreeMemory</b> function to free these buffers.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/nf-wtsapi32-wtsfreememory#">Read more on docs.microsoft.com</see>.</para>
    /// </remarks>
    [DllImport("WtsApi32", ExactSpelling = true)]
    [DefaultDllImportSearchPaths(DllImportSearchPath.System32)]
    [SupportedOSPlatform("windows6.0.6000")]
    internal static extern void WTSFreeMemory(IntPtr pMemory);
}
