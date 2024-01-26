using System.ComponentModel.DataAnnotations;
using System.Globalization;

namespace UiPath.Rdp;

public class RdpConnectionSettings
{
    public string Username { get; }
    public string Domain { get; }
    public string Password { get; }

    public int DesktopWidth { get; set; } = 1024;
    public int DesktopHeight { get; set; } = 768;
    public int ColorDepth { get; set; } = 32;
    public bool FontSmoothing { get; set; }

    public string HostName { get; set; } = "localhost";
    public int? Port { get; set; }

    public string ScopeName { get; set; }
    [MaxLength(15, ErrorMessage = "Sometimes :) Windows returns only first 15 chars for a session ClientName")]
    public string? ClientName { get; set; }

    public RdpConnectionSettings(string username, string domain, string password)
    {
        Username = username;
        Domain = domain;
        Password = password;
        ScopeName = GetUniqueScopeName();
    }

    private static volatile int ConnectionId = 0;
    private static readonly string ScopeNameBase = "RDP_" + Environment.ProcessId.ToString(CultureInfo.InvariantCulture) + "_";
    private static string GetUniqueScopeName() => ScopeNameBase
        + Interlocked.Increment(ref ConnectionId).ToString(CultureInfo.InvariantCulture);
}
