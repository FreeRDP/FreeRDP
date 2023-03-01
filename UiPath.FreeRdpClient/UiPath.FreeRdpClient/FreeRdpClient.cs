using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Nito.Disposables;
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
        public int Port;
    }

    const string FreeRdpClientDll = "UiPath.FreeRdpWrapper.dll";
    private static string ScopeName = "RunId";

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    private delegate void LogCallback([MarshalAs(UnmanagedType.LPStr)] string category, [MarshalAs(UnmanagedType.I4)] LogLevel logLevel, [MarshalAs(UnmanagedType.LPWStr)] string message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private delegate void RegisterThreadScopeCallback([MarshalAs(UnmanagedType.LPStr)] string category);

    private static LogCallback LogCallbackDelegate = Log;
    private static RegisterThreadScopeCallback RegisterThreadScopeCallbackDelegate = RegisterThreadScope;

    private static ILoggerFactory? LoggerFactory { get; set; }

    [DllImport(FreeRdpClientDll, PreserveSig = false, CharSet = CharSet.Unicode)]
    private extern static uint InitializeLogging([MarshalAs(UnmanagedType.FunctionPtr)] LogCallback logCallback, [MarshalAs(UnmanagedType.FunctionPtr)]  RegisterThreadScopeCallback registerThreadScopeCallback);

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
            HostName = connectionSettings.HostName,
            Port = connectionSettings.Port ?? default
        };

        Activity currentActivity;

        return await Task.Run(() =>
        {
            RdpLogon(connectOptions, out var releaseObjectName);
            return new AsyncDisposable(() =>
            {
                var someLogger = LoggerFactory.CreateLogger("s");
                using var scope = BeginScope(someLogger, connectionSettings.ClientName);
                Disconnect(releaseObjectName); 
                return ValueTask.CompletedTask;
            });
        });
    }

    private static IDisposable BeginScope(ILogger logger, string scope)
    => logger.BeginScope("{RdpScope}", scope);

    private static void RegisterThreadScope(string scope)
    {
        LoggerFactory.CreateLogger(nameof(RegisterThreadScope)).BeginScope($"{{{ScopeName}}}", scope);
    }

    private static void Log(string category, LogLevel loglevel, string message)
    {
        if (LoggerFactory is null)
            return;

        var log = LoggerFactory.CreateLogger(category);
        log.Log(loglevel, message);
    }

    public static void SetupLogging(ILoggerFactory? loggerFactory)
    {
        LoggerFactory = loggerFactory;
        InitializeLogging(logCallback: LogCallbackDelegate, registerThreadScopeCallback: RegisterThreadScopeCallbackDelegate);
    }

    private static void Disconnect(string releaseObjectName)
    {
        if (releaseObjectName != default)
            RdpRelease(releaseObjectName);
    }

    public static IServiceCollection AddFreeRdp(this IServiceCollection services, string scopeName = "RunId")
    {
        ScopeName = scopeName;
        return services.AddHostedService<FreeRdpInitilizer>();
    }

    private sealed class FreeRdpInitilizer : IHostedService
    {
        private readonly ILoggerFactory _loggerFactory;

        public FreeRdpInitilizer(ILoggerFactory loggerFactory)
        {
            _loggerFactory = loggerFactory;
        }

        Task IHostedService.StartAsync(CancellationToken cancellationToken)
        {
            SetupLogging(_loggerFactory);
            return Task.CompletedTask;
        }

        Task IHostedService.StopAsync(CancellationToken cancellationToken)
        {
            SetupLogging(null);
            return Task.CompletedTask;
        }
    }
}
