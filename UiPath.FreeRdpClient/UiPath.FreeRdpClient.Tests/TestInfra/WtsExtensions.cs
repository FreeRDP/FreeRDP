using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using UiPath.Rdp;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests;
using ScopeToBeforeConnectTimestamp = Dictionary<string, DateTimeOffset>;

internal static class HostExtensions
{
    public static Wts GetWts(this TestHost _) => new();
    
    public static TestHost AddBeforeConnectTimestamps(this TestHost host)
        => host.AddRegistry(s => s.AddSingleton<ScopeToBeforeConnectTimestamp>());
    public static ScopeToBeforeConnectTimestamp GetScopeTimestamps(this TestHost host)
        => host.GetRequiredService<ScopeToBeforeConnectTimestamp>();


    private static void SaveBeforeConnectTimestamp(this TestHost host, RdpConnectionSettings connectionSettings)
        => host.GetScopeTimestamps()[connectionSettings.ScopeName] = DateTimeOffset.UtcNow;
    private static DateTimeOffset GetBeforeConnectTimestamp(this TestHost host, RdpConnectionSettings connectionSettings)
        => host.GetScopeTimestamps()[connectionSettings.ScopeName];

    public static async Task<IAsyncDisposable> Connect(this TestHost host, RdpConnectionSettings connectionSettings)
    {
        var freeRdpClient = host.GetRequiredService<IFreeRdpClient>();
        var log = host.GetRequiredService<ILogger<TestHost>>();
        using var logScope = log.BeginScope($"{NativeLoggingForwarder.ScopeName}", connectionSettings.ScopeName + "_fromTest");
        host.SaveBeforeConnectTimestamp(connectionSettings);
        return await freeRdpClient.Connect(connectionSettings);
    }

    public static async Task<int> FindSession(this TestHost host, RdpConnectionSettings connectionSettings)
    {
        int? sessionId = null;
        await WaitFor.Predicate(() => (sessionId = host.FindFirstSession(connectionSettings)) is not null);
        return sessionId!.Value;
    }

    public static async Task WaitNoSession(this TestHost host, RdpConnectionSettings connectionSettings)
    {
        await WaitFor.Predicate(() => host.FindFirstSession(connectionSettings) is null);
    }

    private static int? FindFirstSession(this TestHost host, RdpConnectionSettings connectionSettings)
    {
        var wts = host.GetWts();
        var sessionIds = wts.GetSessionIdList();

        foreach (int sessionId in sessionIds)
        {
            var sessionInfo = wts.QuerySessionInformation(sessionId).SessionInfo();
            if (DateTimeOffset
                .FromFileTime(sessionInfo.Data.ConnectTime) >= host.GetBeforeConnectTimestamp(connectionSettings)
                && sessionInfo.Data.ConnectTime > sessionInfo.Data.DisconnectTime
                )
            {
                return sessionId;
            }
        }

        return null;
    }
}
