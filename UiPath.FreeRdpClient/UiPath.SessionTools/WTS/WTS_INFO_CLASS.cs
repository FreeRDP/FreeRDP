namespace Windows.Win32.System.RemoteDesktop;

/// <summary>Contains values that indicate the type of session information to retrieve in a call to the WTSQuerySessionInformation function.</summary>
/// <remarks>
/// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class">Learn more about this API from docs.microsoft.com</see>.</para>
/// </remarks>
public enum WTS_INFO_CLASS
{
    /// <summary>
    /// <para>A null-terminated string that contains the name of the initial program that Remote Desktop Services runs when the user logs on.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSInitialProgram = 0,
    /// <summary>
    /// <para>A null-terminated string that contains the published name of the application that the session is running. <b>Windows Server 2008 R2, Windows 7, Windows Server 2008 and Windows Vista:  </b>This value is not supported</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSApplicationName = 1,
    /// <summary>A null-terminated string that contains the default directory used when launching the initial program.</summary>
    WTSWorkingDirectory = 2,
    /// <summary>This value is not used.</summary>
    WTSOEMId = 3,
    /// <summary>A <b>ULONG</b> value that contains the session identifier.</summary>
    WTSSessionId = 4,
    /// <summary>A null-terminated string that contains the name of the user associated with the session.</summary>
    WTSUserName = 5,
    /// <summary>
    /// <para>A null-terminated string that contains the name of the Remote Desktop Services session. <div class="alert"><b>Note</b>  Despite its name, specifying this type does not return the window station name. Rather, it returns the name of the Remote Desktop Services session. Each Remote Desktop Services session is associated with an interactive window station. Because the only supported window station name for an interactive window station is "WinSta0", each session is associated with its own "WinSta0" window station. For more information, see <a href="https://docs.microsoft.com/windows/desktop/winstation/window-stations">Window Stations</a>.</div> <div> </div></para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSWinStationName = 6,
    /// <summary>A null-terminated string that contains the name of the domain to which the logged-on user belongs.</summary>
    WTSDomainName = 7,
    /// <summary>
    /// <para>The session's current connection state. For more information, see <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ne-wtsapi32-wts_connectstate_class">WTS_CONNECTSTATE_CLASS</a>.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSConnectState = 8,
    /// <summary>A <b>ULONG</b> value that contains the build number of the client.</summary>
    WTSClientBuildNumber = 9,
    /// <summary>A null-terminated string that contains the name of the client.</summary>
    WTSClientName = 10,
    /// <summary>A null-terminated string that contains the directory in which the client is installed.</summary>
    WTSClientDirectory = 11,
    /// <summary>A <b>USHORT</b> client-specific product identifier.</summary>
    WTSClientProductId = 12,
    /// <summary>A <b>ULONG</b> value that contains a client-specific hardware identifier. This option is reserved for future use. <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/nf-wtsapi32-wtsquerysessioninformationa">WTSQuerySessionInformation</a> will always return a value of 0.</summary>
    WTSClientHardwareId = 13,
    /// <summary>
    /// <para>The network type and network address of the client. For more information, see <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wts_client_address">WTS_CLIENT_ADDRESS</a>. The IP address is offset by two bytes from the start of the <b>Address</b> member of the <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wts_client_address">WTS_CLIENT_ADDRESS</a> structure.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSClientAddress = 14,
    /// <summary>
    /// <para>Information about the display resolution of the client. For more information, see <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wts_client_display">WTS_CLIENT_DISPLAY</a>.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSClientDisplay = 15,
    /// <summary>A <b>USHORT</b> value that specifies information about the protocol type for the</summary>
    WTSClientProtocolType = 16,
    /// <summary>
    /// <para>This value returns <b>FALSE</b>. If you call <a href="https://docs.microsoft.com/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a> to get extended error information, <b>GetLastError</b> returns <b>ERROR_NOT_SUPPORTED</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not used.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSIdleTime = 17,
    /// <summary>
    /// <para>This value returns <b>FALSE</b>. If you call <a href="https://docs.microsoft.com/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a> to get extended error information, <b>GetLastError</b> returns <b>ERROR_NOT_SUPPORTED</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not used.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSLogonTime = 18,
    /// <summary>
    /// <para>This value returns <b>FALSE</b>. If you call <a href="https://docs.microsoft.com/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a> to get extended error information, <b>GetLastError</b> returns <b>ERROR_NOT_SUPPORTED</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not used.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSIncomingBytes = 19,
    /// <summary>
    /// <para>This value returns <b>FALSE</b>. If you call <a href="https://docs.microsoft.com/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a> to get extended error information, <b>GetLastError</b> returns <b>ERROR_NOT_SUPPORTED</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not used.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSOutgoingBytes = 20,
    /// <summary>
    /// <para>This value returns <b>FALSE</b>. If you call <a href="https://docs.microsoft.com/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a> to get extended error information, <b>GetLastError</b> returns <b>ERROR_NOT_SUPPORTED</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not used.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSIncomingFrames = 21,
    /// <summary>
    /// <para>This value returns <b>FALSE</b>. If you call <a href="https://docs.microsoft.com/windows/desktop/api/errhandlingapi/nf-errhandlingapi-getlasterror">GetLastError</a> to get extended error information, <b>GetLastError</b> returns <b>ERROR_NOT_SUPPORTED</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not used.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSOutgoingFrames = 22,
    /// <summary>Information about a Remote Desktop Connection (RDC) client. For more information, see <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wtsclienta">WTSCLIENT</a>.</summary>
    WTSClientInfo = 23,
    /// <summary>Information about a client session on a RD Session Host server. For more information, see <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wtsinfoa">WTSINFO</a>.</summary>
    WTSSessionInfo = 24,
    /// <summary>
    /// <para>Extended information about a  session on a   RD Session Host server. For more information, see <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wtsinfoexa">WTSINFOEX</a>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not supported.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSSessionInfoEx = 25,
    /// <summary>
    /// <para>A <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wtsconfiginfoa">WTSCONFIGINFO</a> structure that contains information about the configuration of a RD Session Host server. <b>Windows Server 2008 and Windows Vista:  </b>This value is not supported.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSConfigInfo = 26,
    /// <summary>This value is not supported.</summary>
    WTSValidationInfo = 27,
    /// <summary>
    /// <para>A <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ns-wtsapi32-wts_session_address">WTS_SESSION_ADDRESS</a> structure that contains the IPv4 address assigned to the session. If the session does not have a virtual IP address, the <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/nf-wtsapi32-wtsquerysessioninformationa">WTSQuerySessionInformation</a> function  returns <b>ERROR_NOT_SUPPORTED</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not supported.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSSessionAddressV4 = 28,
    /// <summary>
    /// <para>Determines whether the current session is a remote session. The <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/nf-wtsapi32-wtsquerysessioninformationa">WTSQuerySessionInformation</a> function returns a value of <b>TRUE</b> to indicate that the current session is a remote session, and <b>FALSE</b> to indicate that the current session is a local session. This value can only be used for the local machine, so the <i>hServer</i> parameter of the <b>WTSQuerySessionInformation</b> function must contain <b>WTS_CURRENT_SERVER_HANDLE</b>. <b>Windows Server 2008 and Windows Vista:  </b>This value is not supported.</para>
    /// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_info_class#members">Read more on docs.microsoft.com</see>.</para>
    /// </summary>
    WTSIsRemoteSession = 29,
}
