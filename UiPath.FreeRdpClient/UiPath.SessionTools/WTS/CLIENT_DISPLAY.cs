namespace Windows.Win32.System.RemoteDesktop;

/// <summary>Contains information about the display of a Remote Desktop Connection (RDC) client.</summary>
/// <remarks>
/// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ns-wtsapi32-wts_client_display">Learn more about this API from docs.microsoft.com</see>.</para>
/// </remarks>
public struct WTS_CLIENT_DISPLAY
{
    /// <summary>Horizontal dimension, in pixels, of the client's display.</summary>
    public uint HorizontalResolution;
    /// <summary>Vertical dimension, in pixels, of the client's display.</summary>
    public uint VerticalResolution;
    /// <summary></summary>
    public uint ColorDepth;
}
