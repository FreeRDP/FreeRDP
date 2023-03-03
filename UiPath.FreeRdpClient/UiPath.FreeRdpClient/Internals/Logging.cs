using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace UiPath.Rdp;

internal class Logging : IHostedService
{
    public static string ScopeName { get; set; } = "RunId";
    internal static Logging? Instance { get; private set; }
    private readonly NativeInterface.LogCallback _logCallbackDelegate;
    private readonly NativeInterface.RegisterThreadScopeCallback _registerThreadScopeCallbackDelegate;

    private ILoggerFactory? LoggerFactory { get; set; }

    public string[] FilterNotStartsWith { get; private set; } = new[]
    {
        // ordered by frequency
        "freerdp_check_fds() failed - 0", //24000+ in 50 sec
        "WARNING: invalid packet signature",
        "transport_check_fds: transport->ReceiveCallback() - -4",
        "fastpath_recv_update_data() fail",
        "Stream_GetRemainingLength() < size",//20000+ in 50 sec
        "Fastpath update Orders [0] failed, status 0",//??
        "Total size (", // 21234214) exceeds MultifragMaxRequestSize (65535) // 2000+ in 50 secs
        "Unexpected FASTPATH_FRAGMENT_SINGLE",//800+ in 50 sec
        "SECONDARY ORDER [0x",//<04/05/...>] Cache Bitmap V2 (Compressed) failed",
        "bulk_decompress() failed",
        "Decompression failure!",
        "Unknown bulk compression type 00000003",
        "Unsupported bulk compression type 00000003",
        "history buffer index out of range",//10+
        "history buffer overflow",
        "fastpath_recv_update() - -1",
        "order flags 03 failed",

        "order flags 01 failed", //3+
    };

    public Logging(ILoggerFactory loggerFactory)
    {
        LoggerFactory = loggerFactory;
        _logCallbackDelegate = Log;
        _registerThreadScopeCallbackDelegate = RegisterThreadScope;
        Instance = this;
    }

    private void Log(string category, LogLevel logLevel, string message)
    {
        if (LoggerFactory is null)
            return;

        if (!FilterLogs(logLevel, message))
            return;

        var log = LoggerFactory.CreateLogger(category);
        log.Log(logLevel, message);
    }


    private void RegisterThreadScope(string scope)
    {
        _ = LoggerFactory?.CreateLogger(nameof(RegisterThreadScope)).BeginScope($"{{{ScopeName}}}", scope);
    }

    private bool FilterLogs(LogLevel logLevel, string message)
    {
        if (logLevel is LogLevel.Error
            && FilterNotStartsWith.Any(message.StartsWith))
            return false;

        return true;
    }

    Task IHostedService.StartAsync(CancellationToken cancellationToken)
    {
        var forwardFreeRdpLogs = Environment.GetEnvironmentVariable("WLOG_FILEAPPENDER_OUTPUT_FILE_PATH") is null;
        NativeInterface.InitializeLogging(logCallback: _logCallbackDelegate,
                                          registerThreadScopeCallback: _registerThreadScopeCallbackDelegate,
                                          forwardFreeRdpLogs: forwardFreeRdpLogs);
        return Task.CompletedTask;
    }

    Task IHostedService.StopAsync(CancellationToken cancellationToken)
    {
        LoggerFactory = null;
        return Task.CompletedTask;
    }
}
