using Microsoft.Extensions.Logging;
using Nito.AsyncEx;
using Nito.Disposables;
using System.Net.Sockets;
using System.Runtime.CompilerServices;

namespace UiPath.Rdp;

internal class FreeRdpClient : IFreeRdpClient
{
    static FreeRdpClient()
    {
        //Make sure winsock is initialized
        using var _ = new TcpClient();
    }

    public FreeRdpClient(ILogger<FreeRdpClient> logger)
    {
        _log = logger;
    }

    private readonly ILogger<FreeRdpClient> _log;
    private readonly AsyncLock _initLock = new();
    private bool _initialized = false;

    public async Task<IAsyncDisposable> Connect(RdpConnectionSettings connectionSettings)
    {
        ArgumentNullException.ThrowIfNull(connectionSettings.Username);
        ArgumentNullException.ThrowIfNull(connectionSettings.Domain);
        ArgumentNullException.ThrowIfNull(connectionSettings.Password);

        NativeInterface.ConnectOptions connectOptions = new()
        {
            Width = connectionSettings.DesktopWidth,
            Height = connectionSettings.DesktopHeight,
            Depth = connectionSettings.ColorDepth,
            FontSmoothing = connectionSettings.FontSmoothing,
            User = connectionSettings.Username,
            Domain = connectionSettings.Domain,
            Password = connectionSettings.Password,
            ScopeName = connectionSettings.ScopeName,
            ClientName = connectionSettings.ClientName,
            HostName = connectionSettings.HostName,
            Port = connectionSettings.Port ?? default
        };
        
        using (await _initLock.LockAsync())
        {
            /// Make sure freerdp static initilizers are not run concurrently
            /// when they do they fail with 
            if (!_initialized)
            {
                _log.LogInformation("RdpInitLock acquired.");
                var connection = await DoConnect();
                _initialized = true;
                _log.LogInformation("RdpInitLock released.");
                return connection;
            }
        }

        return await DoConnect();

        Task<AsyncDisposable> DoConnect() => Task.Run(() =>
        {
            NativeInterface.RdpLogon(connectOptions, out var releaseObjectName);
            return new AsyncDisposable(async () =>
            {
                Disconnect(releaseObjectName);
            });
        });
    }

    private void Disconnect(string releaseObjectName)
    {
        if (releaseObjectName != default)
            NativeInterface.RdpRelease(releaseObjectName);
    }
}