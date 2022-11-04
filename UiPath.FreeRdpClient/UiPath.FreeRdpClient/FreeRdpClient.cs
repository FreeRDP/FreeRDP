using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Nito.Disposables;
using System.ComponentModel.DataAnnotations;
using System.Diagnostics;
using System.Net.Sockets;
using System.Runtime.InteropServices;


namespace UiPath.Rdp;

public static class FreeRdpClient
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct ConnectOptions
    {
        public int Width;
        public int Height;
        public int Depth;
        public bool FontSmoothing;
        [MarshalAs(UnmanagedType.BStr)]
        public string User;
        [MarshalAs(UnmanagedType.BStr)]
        public string Domain;
        [MarshalAs(UnmanagedType.BStr)]
        public string Password;
        [MarshalAs(UnmanagedType.BStr)]
        public string ClientName;
        [MarshalAs(UnmanagedType.BStr)]
        public string HostName;
    }

    const string FreeRdpClientDll = "UiPath.FreeRdpWrapper.dll";

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private delegate void LogCallback([MarshalAs(UnmanagedType.I4)] LogLevel logLevel, [MarshalAs(UnmanagedType.LPWStr)] string message);

    private static LogCallback LogCallbackDelegate = Log;
    private static ILogger? Logger { get; set; }

    [DllImport(FreeRdpClientDll, PreserveSig = false, CharSet = CharSet.Unicode)]
    private extern static uint InitializeLogging([MarshalAs(UnmanagedType.FunctionPtr)] LogCallback logCallback);

    [DllImport(FreeRdpClientDll, PreserveSig = false, CharSet = CharSet.Unicode)]
    private extern static uint RdpLogon([In] ConnectOptions rdpOptions,  [MarshalAs(UnmanagedType.BStr)]out string releaseObjectName);

    [DllImport(FreeRdpClientDll, PreserveSig = false, CharSet = CharSet.Unicode)]
    private extern static uint RdpRelease(string releaseObjectName);

    static FreeRdpClient()
    {
        //Make sure winsock is initialized
        using var _ = new TcpClient();
    }

    public static async Task<IAsyncDisposable> Connect(RdpConnectionSettings connectionSettings)
    {
        ConnectOptions connectOptions = new()
        {
            Width = connectionSettings.DesktopWidth,
            Height = connectionSettings.DesktopHeight,
            Depth = connectionSettings.ColorDepth,
            FontSmoothing = connectionSettings.FontSmoothing,
            User = connectionSettings.Username,
            Domain = connectionSettings.Domain,
            Password = connectionSettings.Password,
            ClientName = connectionSettings.ClientName,
            HostName = connectionSettings.HostName
        };

        return await Task.Run(() =>
        {
            RdpLogon(connectOptions, out var releaseObjectName);
            return new AsyncDisposable(() => { Disconnect(releaseObjectName); return ValueTask.CompletedTask; });
        });
    }

    private static void Log(LogLevel loglevel, string message)
    => Logger?.Log(loglevel, message);

    public static void SetupLogging(ILogger? logger)
    {
        Logger = logger;
        InitializeLogging(LogCallbackDelegate);
    }

    private static void Disconnect(string releaseObjectName)
    {
        if (releaseObjectName != default)
            RdpRelease(releaseObjectName);
    }

    public static IServiceCollection AddFreeRdp(this IServiceCollection services)
        => services.AddHostedService<FreeRdpInitilizer>();

    private class FreeRdpInitilizer : IHostedService
    {
        private readonly ILogger _logger;

        public FreeRdpInitilizer(ILoggerFactory loggerFactory)
        {
            _logger = loggerFactory.CreateLogger(nameof(FreeRdpClient));
        }

        Task IHostedService.StartAsync(CancellationToken cancellationToken)
        {
            SetupLogging(_logger);
            return Task.CompletedTask;
        }

        Task IHostedService.StopAsync(CancellationToken cancellationToken)
        {
            SetupLogging(null);
            return Task.CompletedTask;
        }
    }
}

public class RdpConnectionSettings
{
    public string Username { get; init; }
    public string Domain { get; init; }
    public string Password { get; init; }

    public RdpConnectionSettings(string username, string domain, string password)
    {
        Username = username;
        Domain = domain;
        Password = password;
    }

    private static volatile int ConnectionId = 0;
    private static readonly string ClientNameBase = "RDP_" + Process.GetCurrentProcess().Id % 1e6 + "_";

    public int DesktopWidth { get; set; } = 1024;

    public int DesktopHeight { get; set; } = 768;

    public int ColorDepth { get; set; } = 32;

    [MaxLength(15, ErrorMessage = "Sometimes :) Windows returns only first 15 chars for a session ClientName")]
    public string ClientName { get; set; } = ClientNameBase + Interlocked.Increment(ref ConnectionId) % 1e4;
    public bool FontSmoothing { get; set; }
    public string HostName { get; set; } = "localhost";
}