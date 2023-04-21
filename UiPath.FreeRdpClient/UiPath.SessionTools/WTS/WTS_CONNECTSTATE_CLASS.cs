namespace Windows.Win32.System.RemoteDesktop;

/// <summary>Specifies the connection state of a Remote Desktop Services session.</summary>
/// <remarks>
/// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ne-wtsapi32-wts_connectstate_class">Learn more about this API from docs.microsoft.com</see>.</para>
/// </remarks>
public enum WTS_CONNECTSTATE_CLASS
{
    /// <summary>A user is logged on to the WinStation. This state occurs when a user is signed in and actively connected to the device.</summary>
    WTSActive = 0,
    /// <summary>The WinStation is connected to the client.</summary>
    WTSConnected = 1,
    /// <summary>The WinStation is in the process of connecting to the client.</summary>
    WTSConnectQuery = 2,
    /// <summary>The WinStation is shadowing another WinStation.</summary>
    WTSShadow = 3,
    /// <summary>The WinStation is active but the client is disconnected. This state occurs when a user is signed in but not actively connected to the device, such as when the user has chosen to exit to the lock screen.</summary>
    WTSDisconnected = 4,
    /// <summary>The WinStation is waiting for a client to connect.</summary>
    WTSIdle = 5,
    /// <summary>The WinStation is listening for a connection. A listener session waits for requests for new client connections. No user is logged on a listener session. A listener session cannot be reset, shadowed, or changed to a regular client session.</summary>
    WTSListen = 6,
    /// <summary>The WinStation is being reset.</summary>
    WTSReset = 7,
    /// <summary>The WinStation is down due to an error.</summary>
    WTSDown = 8,
    /// <summary>The WinStation is initializing.</summary>
    WTSInit = 9,
}
